// Server.cpp : Defines the entry point for the application.
//

#include <iostream>
#include <algorithm>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <memory>
#include <variant>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <fmt/color.h>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>
// TODO: investigate if we should use https://github.com/johnmcfarlane/cnl instead of float/double

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

namespace Server {
    using Username = std::string;
    using SecurityTicker = std::string;
    using OrderID = uint32_t;
    using UserID = uint32_t;
    using SecurityID = uint32_t;

    class UserPortfolioTable {
        uint32_t user_count = 0;
        uint32_t capacity = 0;
        const uint32_t columns;

        std::unique_ptr<float[]> data = nullptr;
        mutable std::shared_mutex data_mutex = std::shared_mutex();
        mutable std::unique_ptr<std::mutex[]> user_mutexes;
    public:
        explicit UserPortfolioTable(const uint32_t columns) : columns{ columns } {}

        const uint32_t get_column_count() const {
            return columns;
        }

        const uint32_t get_user_count() const {
            return user_count;
        }

        UserID register_new_user() {
            auto write_lock = std::unique_lock(data_mutex);
            auto user_id = user_count++;
            assert(user_count > 0);

            if (user_count > capacity) {
                capacity = std::max(1u, capacity * 2);

                // Resize data buffer
                auto new_data_ptr = std::make_unique<float[]>(capacity * columns);
                std::fill(new_data_ptr.get(), new_data_ptr.get() + capacity * columns, 0.0f);

                if (data != nullptr) {
                    std::memcpy(new_data_ptr.get(), data.get(), (user_count - 1) * columns * sizeof(float));
                }
                data = std::move(new_data_ptr);

                auto new_mutexes = std::make_unique<std::mutex[]>(capacity);
                // no need to copy old ones, just replace
                user_mutexes = std::move(new_mutexes);
            }

            // Zero out just the new row
            std::fill(data.get() + user_id * columns, data.get() + (user_id + 1) * columns, 0.0f);
            return user_id;
        }

        std::vector<std::vector<float>> get_portfolio_table() const {
            auto read_lock = std::shared_lock(data_mutex);
            auto table = std::vector<std::vector<float>>(user_count, std::vector<float>(columns));

            for (UserID user_index = 0; user_index < user_count; user_index++) {
                std::unique_lock user_lock(user_mutexes[user_index]);
                std::memcpy(
                    table[user_index].data(),
                    data.get() + user_index * columns,
                    columns * sizeof(float)
                );
            }

            return table;
        }

        void reset_user_position(UserID user_id) {
            assert(user_id < user_count);
            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            std::fill(data.get() + user_id * columns, data.get() + (user_id + 1) * columns, 0.0f);
        }

        // `security_1 += addition_1`
        // returns: the new value of the security in the portfolio
        float add_to_security(UserID user_id, SecurityID security_1, float addition_1) {
            assert(user_id < user_count);
            assert(security_1 < columns);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto& ref = data[user_id * columns + security_1];
            ref += addition_1;
            return ref;
        }

        // `security_1 += addition_1`, `security_2 += addition_2`
        // returns: a pair of the new portfolio values
        std::pair<float, float> add_to_two_securities(UserID user_id, SecurityID security_1, float addition_1, SecurityID security_2, float addition_2) {
            assert(user_id < user_count);
            assert(security_1 < columns);
            assert(security_2 < columns);
            assert(security_1 != security_2);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto& ref_1 = data[user_id * columns + security_1];
            ref_1 += addition_1;
            auto& ref_2 = data[user_id * columns + security_2];
            ref_2 += addition_2;
            return { ref_1, ref_2 };
        }

        // `security_2 += security_1 * multiply`
        // returns: the new value of security_2
        float multiply_and_add_1_to_2(UserID user_id, SecurityID security_1, SecurityID security_2, float multiply) {
            assert(user_id < user_count);
            assert(security_1 < columns);
            assert(security_2 < columns);
            assert(security_1 != security_2);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto& ref_to_multiply_from = data[user_id * columns + security_1];
            auto& ref_to_mult_and_add = data[user_id * columns + security_2];
            ref_to_mult_and_add += ref_to_multiply_from * multiply;
            return ref_to_mult_and_add;
        }

        // `security_2 += security_1 * multiply`, then `security_1 = set_value`
        // returns: the new value of security_2
        float multiply_and_add_1_to_2_and_set_1(UserID user_id, SecurityID security_1, SecurityID security_2, float multiply, float set_value) {
            assert(user_id < user_count);
            assert(security_1 < columns);
            assert(security_2 < columns);
            assert(security_1 != security_2);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto& ref_to_set = data[user_id * columns + security_1];
            auto& ref_to_mult_and_add = data[user_id * columns + security_2];
            ref_to_mult_and_add += ref_to_set * multiply;
            ref_to_set = set_value;
            return ref_to_mult_and_add;
        }
    };

    enum OrderSide : uint8_t {
        BID,
        ASK
    };
    struct LimitOrder {
        UserID user_id;
        OrderID order_id;
        OrderSide side;
        float price;
        float volume;

        struct BidComparator {
            bool operator()(const LimitOrder& a, const LimitOrder& b) const {
                return (a.price != b.price) ? (a.price > b.price) : (a.order_id < b.order_id);
            }
        };
        struct AskComparator {
            bool operator()(const LimitOrder& a, const LimitOrder& b) const {
                return (a.price != b.price) ? (a.price < b.price) : (a.order_id < b.order_id);
            }
        };
    };
    struct CancelOrder {
        UserID user_id;
        OrderID order_id;
    };
    using CommandVariant = std::variant<LimitOrder, CancelOrder>;

    struct Transaction {
        float price;
        float volume;
        UserID buyer_id;
        UserID seller_id;
    };

    // Keeps track of both sides (bids and asks) of a CLOB
    class OrderBook {
        using BidSet = std::set<LimitOrder, LimitOrder::BidComparator>;
        using AskSet = std::set<LimitOrder, LimitOrder::AskComparator>;

        BidSet bid_orders;
        AskSet ask_orders;
        std::unordered_map<OrderID, BidSet::iterator> bid_map;
        std::unordered_map<OrderID, AskSet::iterator> ask_map;
    public:
        // (bid, ask) pair of depths
        using BookDepth = std::pair<std::map<float, float>, std::map<float, float>>;

        std::size_t bid_size() const {
            return bid_orders.size();
        }

        std::size_t ask_size() const {
            return ask_orders.size();
        }

        bool insert_order(const LimitOrder& order) {
            if (order.side == BID) {
                auto [it, inserted] = bid_orders.insert(order);
                if (!inserted) return false;
                bid_map[order.order_id] = it;
            }
            else {
                auto [it, inserted] = ask_orders.insert(order);
                if (!inserted) return false;
                ask_map[order.order_id] = it;
            }
            return true;
        }

        bool cancel_order(const CancelOrder& cancel) {
            if (auto it = bid_map.find(cancel.order_id); it != bid_map.end()) {
                bid_orders.erase(it->second);
                bid_map.erase(it);
                return true;
            }
            else if (auto it = ask_map.find(cancel.order_id); it != ask_map.end()) {
                ask_orders.erase(it->second);
                ask_map.erase(it);
                return true;
            }
            return false;
        }

        LimitOrder& top_bid() {
            if (bid_orders.empty()) {
                throw std::runtime_error("Bid book is empty.");
            }
            return const_cast<LimitOrder&>(*bid_orders.begin());
        }

        LimitOrder& top_ask() {
            if (ask_orders.empty()) {
                throw std::runtime_error("Ask book is empty.");
            }
            return const_cast<LimitOrder&>(*ask_orders.begin());
        }

        void pop_top_bid() {
            assert(!bid_orders.empty());
            auto it = bid_orders.begin();
            bid_map.erase(it->order_id);
            bid_orders.erase(it);
        }

        void pop_top_ask() {
            assert(!ask_orders.empty());
            auto it = ask_orders.begin();
            ask_map.erase(it->order_id);
            ask_orders.erase(it);
        }

        BookDepth get_book_depth() const {
            std::map<float, float> bid_depth;
            std::map<float, float> ask_depth;

            // For bids, accumulate by descending price
            for (const auto& order : bid_orders) {
                bid_depth[order.price] += order.volume;
            }

            // For asks, accumulate by ascending price
            for (const auto& order : ask_orders) {
                ask_depth[order.price] += order.volume;
            }

            return { bid_depth, ask_depth };
        }

        std::pair<std::vector<LimitOrder>, std::vector<LimitOrder>> get_limit_orders() const {
            std::vector<LimitOrder> bids(bid_orders.begin(), bid_orders.end());
            std::vector<LimitOrder> asks(ask_orders.begin(), ask_orders.end());
            return { std::move(bids), std::move(asks) };
        }

        std::set<OrderID> get_all_user_orders(UserID user_id) const {
            std::set<OrderID> result;
            for (const auto& [order_id, it] : bid_map) {
                if (it->user_id == user_id) {
                    result.insert(order_id);
                }
            }
            for (const auto& [order_id, it] : ask_map) {
                if (it->user_id == user_id) {
                    result.insert(order_id);
                }
            }
            return result;
        }
    };

    class Simulation;

    class ISecurity {
    public:
        virtual bool is_tradeable() = 0;
        virtual void before_step(Simulation& simulation) = 0;
        virtual void after_step(Simulation& simulation) = 0;
        virtual void on_simulation_start(Simulation& simulation) = 0;
        virtual void on_simulation_end(Simulation& simulation) = 0;
        virtual void on_trade_executed(
            Simulation& simulation,
            UserID buyer_id, UserID seller_id, float transacted_price, float transacted_volume
        ) = 0;
    };

    class Simulation {
        const uint32_t N;
        const float T;
        const uint32_t security_count;
        uint32_t current_step = 0;
        const std::map<SecurityTicker, std::shared_ptr<ISecurity>> ticker_to_security;
        std::shared_ptr<UserPortfolioTable> user_table;
        std::map<SecurityID, SecurityTicker> id_to_ticker = {};
        std::map<SecurityID, std::shared_ptr<ISecurity>> id_to_security = {};
        std::vector<std::shared_ptr<ISecurity>> securities = {};
        std::vector<SecurityID> security_ids = {};
        std::vector<SecurityTicker> security_tickers = {};
        std::vector<OrderBook> order_books = {};
        std::map<UserID, Username> user_id_to_username = {};
    public:
        explicit Simulation(
            std::map<SecurityTicker, std::shared_ptr<ISecurity>>&& ticker_to_security,
            uint32_t N,
            float T
        ) : N{ N }, T{ T }, security_count{ (uint32_t)ticker_to_security.size() }, ticker_to_security{ std::move(ticker_to_security) }, user_table{ std::make_unique<UserPortfolioTable>(security_count) } {
            SecurityID security_id = 0;
            for (const auto& [ticker, security] : this->ticker_to_security) {
                id_to_ticker.emplace(security_id, ticker);
                id_to_security.emplace(security_id, security);

                securities.push_back(security);
                security_ids.push_back(security_id);
                security_tickers.push_back(ticker);
                order_books.push_back(OrderBook());

                security_id += 1;
            }
        }
    private:
        std::vector<std::shared_ptr<ISecurity>>& get_securities() {
            return securities;
        }
        std::vector<SecurityID>& get_security_ids() {
            return security_ids;
        }
        std::shared_ptr<ISecurity>& get_security(SecurityID security_id) {
            assert(id_to_security.find(security_id) != id_to_security.end());
            return id_to_security[security_id];
        }

        void increment_step() {
            current_step += 1;
        }

        // Ignore these for now
        void on_simulation_start_1() {}
        void on_simulation_start_2() {}
        void on_simulation_end_1() {}
        void on_simulation_end_2() {}
    public:
        UserID add_user(const Username& username) {
            auto user_id = user_table->register_new_user();
            user_id_to_username.emplace(user_id, username);
            return user_id;
        }

        const std::map<UserID, Username>& get_user_id_to_username_map() const {
            return user_id_to_username;
        }

        auto get_user_table() {
            return user_table;
        }

        SecurityID get_security_id(const SecurityTicker& ticker) const {
            for (uint32_t i = 0; i < security_tickers.size(); i++) {
                if (security_tickers[i] == ticker) {
                    return i;
                }
            }
            throw std::runtime_error("Could not find such a ticker.");
        }

        std::vector<SecurityTicker> get_all_tickers() const {
            return security_tickers;
        }

        const SecurityTicker& get_security_ticker(SecurityID security_id) const {
            assert(security_id < security_tickers.size());
            return security_tickers.at(security_id);
        }

        OrderBook& get_order_book(SecurityID security_id) {
            assert(security_id < order_books.size());
            return order_books[security_id];
        }

        float get_dt() const {
            return T / N;
        }

        float get_t() const {
            return current_step * T / N;
        }

        uint32_t get_current_step() const {
            return current_step;
        }

        uint32_t get_N() const {
            return N;
        }

        // Doesn't remove users, only resets their portfolios
        void reset_simulation() {
            auto user_table = get_user_table();
            for (UserID user_id = 0; user_id < user_table->get_user_count(); user_id++) {
                user_table->reset_user_position(user_id);
            }
            current_step = 0;
        }


        struct SimulationStepResult {
            std::map<SecurityTicker, std::map<OrderID, float>> partially_transacted_orders;
            std::map<SecurityTicker, std::set<OrderID>> fully_transacted_orders;
            std::map<SecurityTicker, std::set<OrderID>> cancelled_orders;
            std::map<SecurityTicker, std::vector<Transaction>> transactions;
            std::map<SecurityTicker, OrderBook::BookDepth> order_book_depth_per_security;
            std::map<SecurityTicker, std::pair<std::vector<LimitOrder>, std::vector<LimitOrder>>> order_book_per_security;
            std::vector<std::vector<float>> portfolios;
            std::map<UserID, Username> user_id_to_username_map;
            uint32_t current_step;
            bool has_next_step;
        };
        SimulationStepResult do_simulation_step(std::map<SecurityID, std::deque<CommandVariant>>& queues) {
            // Perform a simulaiton step
            auto step = get_current_step(); // step ∈ [0, ..., N] inclusive
            if (step > get_N()) {
                throw std::runtime_error("Passed simulation endpoint!");
            }

            auto t = get_t(); // t ∈ [0, ..., T]
            auto dt = get_dt(); // dt = T / N

            if (step == 0) {
                on_simulation_start_1();
                for (auto& security : get_securities()) {
                    security->on_simulation_start(*this);
                }
                on_simulation_start_2();
            }

            for (auto& security : get_securities()) {
                security->before_step(*this);
            }

            // Keep track of market updates
            std::map<SecurityTicker, std::map<OrderID, float>> partially_transacted_orders = {}; // ticker -> order_id -> new volume
            std::map<SecurityTicker, std::set<OrderID>> fully_transacted_orders = {};
            std::map<SecurityTicker, std::set<OrderID>> cancelled_orders = {};
            std::map<SecurityTicker, std::vector<Transaction>> transactions = {};

            // Here would the command queues normally be
            for (auto security_id : get_security_ids()) {
                if (queues.find(security_id) == queues.end()) {
                    std::cout << "This security didn't have a queue! Fix this!\n";
                    continue;
                }
                auto& security_class = get_security(security_id);
                auto& order_book = get_order_book(security_id);
                auto& commands = queues.at(security_id);

                // Keep track of market updates for a particular security
                auto local_partially_transacted_orders = std::map<OrderID, float>();
                auto local_fully_transacted_orders = std::set<OrderID>();
                auto local_cancelled_orders = std::set<OrderID>();
                auto local_transactions = std::vector<Transaction>();

                while (!commands.empty()) {
                    auto& variant_command = commands.front();
                    auto index = variant_command.index();

                    if (index == 0) {
                        {
                            // Invariant: the market must not be crossed before submitting a new order
                            const auto has_orders_on_both_sides = order_book.bid_size() > 0 && order_book.ask_size() > 0;
                            const auto is_market_crossed = has_orders_on_both_sides && (order_book.top_bid().price >= order_book.top_ask().price);
                            assert(!is_market_crossed);
                        }

                        LimitOrder& order = std::get<0>(variant_command);
                        // Insert the order
                        order_book.insert_order(order);

                        // The order book is now potentially crossed, we must resolve it
                        // additionally, if it was crossed, it is due to the added order, by our invariants
                        while (true) {
                            if (order_book.bid_size() > 0 && order_book.ask_size() > 0) {
                                auto& top_bid = order_book.top_bid();
                                auto& top_ask = order_book.top_ask();
                                if (top_bid.price >= top_ask.price) {
                                    // The trades will execute on the submitted order's opposite side.
                                    // If the submitted order is a bid, the market is crossed because of it, 
                                    // the execution price will be of the ask. And vice-versa if the crossing is ask.
                                    auto transacted_price = order.side == OrderSide::BID ? top_ask.price : top_bid.price;
                                    auto transacted_volume = std::min(top_bid.volume, top_ask.volume);

                                    auto buyer_id = top_bid.user_id;
                                    auto seller_id = top_ask.user_id;

                                    auto top_bid_id = top_bid.order_id;
                                    auto top_ask_id = top_ask.order_id;

                                    // After this `top_bid` may be invalidated
                                    auto remaining_bid_volume = top_bid.volume - transacted_volume;
                                    if (remaining_bid_volume == 0) {
                                        local_partially_transacted_orders.erase(top_bid_id);
                                        local_fully_transacted_orders.emplace(top_bid_id);
                                        order_book.pop_top_bid();
                                    }
                                    else {
                                        top_bid.volume = remaining_bid_volume;
                                        local_partially_transacted_orders[top_bid_id] = remaining_bid_volume;
                                    }

                                    // After this `top_ask` may be invalidated
                                    auto remaining_ask_volume = top_ask.volume - transacted_volume;
                                    if (remaining_ask_volume == 0) {
                                        local_partially_transacted_orders.erase(top_ask_id);
                                        local_fully_transacted_orders.emplace(top_ask_id);
                                        order_book.pop_top_ask();
                                    }
                                    else {
                                        top_ask.volume = remaining_ask_volume;
                                        local_partially_transacted_orders[top_ask_id] = remaining_ask_volume;
                                    }

                                    // Perform custom security trade resolution
                                    // Must often this is used to simply modify security and cash accounts
                                    security_class->on_trade_executed(*this, buyer_id, seller_id, transacted_price, transacted_volume);
                                    local_transactions.push_back(Transaction{ .price = transacted_price, .volume = transacted_volume, .buyer_id = buyer_id, .seller_id = seller_id });
                                }
                                else {
                                    break;
                                }
                            }
                            else {
                                break;
                            }
                        }

                        {
                            // Invariant: the market must not be crossed after submitting an order and executing it
                            const auto has_orders_on_both_sides = order_book.bid_size() > 0 && order_book.ask_size() > 0;
                            const auto is_market_crossed = has_orders_on_both_sides && (order_book.top_bid().price >= order_book.top_ask().price);
                            assert(!is_market_crossed);
                        }
                    }
                    else if (index == 1) {
                        CancelOrder& order = std::get<1>(variant_command);
                        auto was_cancelled = order_book.cancel_order(order);
                        if (was_cancelled) {
                            local_cancelled_orders.insert(order.order_id);
                        }
                    }
                    else {
                        assert(false);
                    }
                    commands.pop_front();
                }

                // Save the differences to a simulation step object
                const auto& ticker = get_security_ticker(security_id);
                partially_transacted_orders.emplace(ticker, std::move(local_partially_transacted_orders));
                fully_transacted_orders.emplace(ticker, std::move(local_fully_transacted_orders));
                cancelled_orders.emplace(ticker, std::move(local_cancelled_orders));
                transactions.emplace(ticker, std::move(local_transactions));
            }

            for (auto& security : get_securities()) {
                security->after_step(*this);
            }

            if (step == N) {
                on_simulation_end_1();
                for (auto& security : get_securities()) {
                    security->on_simulation_end(*this);
                }
                on_simulation_end_2();
            }

            auto order_book_depth_per_security = std::map<SecurityTicker, OrderBook::BookDepth>();
            auto order_book_per_security = std::map<SecurityTicker, std::pair<std::vector<LimitOrder>, std::vector<LimitOrder>>>();
            for (auto security_id : get_security_ids()) {
                const auto& ticker = get_security_ticker(security_id);
                const auto& order_book = get_order_book(security_id);
                order_book_depth_per_security[ticker] = std::move(order_book.get_book_depth());
                order_book_per_security[ticker] = order_book.get_limit_orders();
            }

            auto user_table = get_user_table();
            auto portfolios = user_table->get_portfolio_table();

            auto& user_id_to_username_map = get_user_id_to_username_map();

            increment_step();
            assert(get_current_step() > 0);
            return SimulationStepResult{
                .partially_transacted_orders = std::move(partially_transacted_orders),
                .fully_transacted_orders = std::move(fully_transacted_orders),
                .cancelled_orders = std::move(cancelled_orders),
                .transactions = std::move(transactions),
                .order_book_depth_per_security = std::move(order_book_depth_per_security),
                .order_book_per_security = std::move(order_book_per_security),
                .portfolios = std::move(portfolios),
                .user_id_to_username_map = user_id_to_username_map,
                .current_step = get_current_step() - 1,
                .has_next_step = get_current_step() <= get_N()
            };
        }
    };

    namespace DemoSecurities {
        class Security_CAD : public Server::ISecurity {
        public:
            // Inherited via ISecurity
            bool is_tradeable() override
            {
                return false;
            }
            void before_step(Simulation& simulation) override {}
            void after_step(Simulation& simulation) override {}
            void on_simulation_start(Simulation& simulation) override {}
            void on_simulation_end(Simulation& simulation) override {}
            void on_trade_executed(
                Simulation& simulation,
                Server::UserID buyer, Server::UserID seller, float price, float quantity
            ) override {
            }
        };

        class Security_BOND : public Server::ISecurity {
            float rate = 0.05f;
            float face_value = 100.0f;
        public:
            // Inherited via ISecurity
            bool is_tradeable() override
            {
                return true;
            }
            void before_step(Simulation& simulation) override {}
            void after_step(Simulation& simulation) override {
                // Bonds make interest payment
                auto dt = simulation.get_dt();
                auto bond_id = simulation.get_security_id("BOND");
                auto cad_id = simulation.get_security_id("CAD");
                auto user_table = simulation.get_user_table();
                for (UserID index_user = 0; index_user < user_table->get_user_count(); index_user++) {
                    // The bond pays `rate * dt` per step, having more bonds increases nomial amount added to cad
                    user_table->multiply_and_add_1_to_2(
                        index_user, bond_id, cad_id, rate * face_value * dt
                    );
                }
            }
            void on_simulation_start(Simulation& simulation) override {}
            void on_simulation_end(Simulation& simulation) override {
                auto bond_id = simulation.get_security_id("BOND");
                auto cad_id = simulation.get_security_id("CAD");
                auto user_table = simulation.get_user_table();
                for (UserID index_user = 0; index_user < user_table->get_user_count(); index_user++) {
                    // Reduce the amount of bond to 0, and realize it as CAD (each bond is worth 100).
                    user_table->multiply_and_add_1_to_2_and_set_1(
                        index_user, bond_id, cad_id, face_value, 0.0f
                    );
                }
            }
            void on_trade_executed(
                Simulation& simulation,
                Server::UserID buyer, Server::UserID seller, float price, float quantity
            ) override {
                auto bond_id = simulation.get_security_id("BOND");
                auto cad_id = simulation.get_security_id("CAD");
                auto user_table = simulation.get_user_table();
                // The bond buyer gets the bond, but losses money
                user_table->add_to_two_securities(buyer, bond_id, quantity, cad_id, -price * quantity);
                // The bond seller losses the bond, but gets money
                user_table->add_to_two_securities(seller, bond_id, -quantity, cad_id, price * quantity);
            }
        };

        class Security_STOCK : public Server::ISecurity {
            // Inherited via ISecurity
            bool is_tradeable() override
            {
                return true;
            }
            void before_step(Simulation& simulation) override {}
            void after_step(Simulation& simulation) override {}
            void on_simulation_start(Simulation& simulation) override {}
            void on_simulation_end(Simulation& simulation) override {
                // At the end convert to CAD at midpoint price, or `100.0f`
                auto stock_id = simulation.get_security_id("STOCK");
                auto cad_id = simulation.get_security_id("CAD");


                auto& order_book = simulation.get_order_book(stock_id);
                auto close_bid_price = 100.0f;
                auto close_ask_price = 100.0f;
                if (order_book.bid_size() > 0) {
                    close_bid_price = order_book.top_bid().price;
                }
                if (order_book.ask_size() > 0) {
                    close_ask_price = order_book.top_ask().price;
                }

                auto user_table = simulation.get_user_table();
                for (UserID index_user = 0; index_user < user_table->get_user_count(); index_user++) {
                    user_table->multiply_and_add_1_to_2_and_set_1(
                        index_user, stock_id, cad_id, (close_bid_price + close_ask_price) / 2.0f, 0.0f
                    );
                }
            }
            void on_trade_executed(
                Simulation& simulation,
                Server::UserID buyer, Server::UserID seller, float price, float quantity
            ) override {
                auto stock_id = simulation.get_security_id("STOCK");
                auto cad_id = simulation.get_security_id("CAD");
                auto user_table = simulation.get_user_table();
                // The buyer gets the stock, but losses money
                user_table->add_to_two_securities(buyer, stock_id, quantity, cad_id, -price * quantity);
                // The seller losses the stock, but gets money
                user_table->add_to_two_securities(seller, stock_id, -quantity, cad_id, price * quantity);
            }
        };
    }
};

namespace nlohmann {
    void to_json(json& j, const Server::OrderSide& side) {
        j = std::string{ magic_enum::enum_name(side) };
    }

    void to_json(nlohmann::json& j, const Server::Transaction& t) {
        j = nlohmann::json{
            {"price", t.price},
            {"volume", t.volume},
            {"buyer_id", t.buyer_id},
            {"seller_id", t.seller_id}
        };
    }

    void to_json(json& j, const Server::LimitOrder& order) {
        j = json{
            {"user_id", order.user_id},
            {"order_id", order.order_id},
            {"side", order.side},
            {"price", order.price},
            {"volume", order.volume}
        };
    }

    void to_json(json& j, const Server::CancelOrder& cancel) {
        j = json{
            {"user_id", cancel.user_id},
            {"order_id", cancel.order_id}
        };
    }
}

class PyISecurity : public Server::ISecurity {
public:
    using Server::ISecurity::ISecurity;

    bool is_tradeable() override {
        PYBIND11_OVERRIDE_PURE(
            bool,                  // Return type
            Server::ISecurity,     // Parent class
            is_tradeable           // Name of function in C++
            // No arguments
        );
    }

    void before_step(Server::Simulation& sim) override {
        PYBIND11_OVERRIDE_PURE(
            void,
            Server::ISecurity,
            before_step,
            sim
        );
    }

    void after_step(Server::Simulation& sim) override {
        PYBIND11_OVERRIDE_PURE(
            void,
            Server::ISecurity,
            after_step,
            sim
        );
    }

    void on_simulation_start(Server::Simulation& sim) override {
        PYBIND11_OVERRIDE_PURE(
            void,
            Server::ISecurity,
            on_simulation_start,
            sim
        );
    }

    void on_simulation_end(Server::Simulation& sim) override {
        PYBIND11_OVERRIDE_PURE(
            void,
            Server::ISecurity,
            on_simulation_end,
            sim
        );
    }

    void on_trade_executed(Server::Simulation& sim, Server::UserID buyer,
        Server::UserID seller, float transacted_price,
        float transacted_volume) override {
        PYBIND11_OVERRIDE_PURE(
            void,
            Server::ISecurity,
            on_trade_executed,
            sim, buyer, seller, transacted_price, transacted_volume
        );
    }
};

int test_function() {
    return 42;
}

PYBIND11_MODULE(Server, m) {
    m.doc() = "Example pybind11 module";
    m.def("test_function", &test_function, "Returns the meaning of life.");

    // --- Expose OrderSide enum ---
    py::enum_<Server::OrderSide>(m, "OrderSide")
        .value("BID", Server::OrderSide::BID)
        .value("ASK", Server::OrderSide::ASK)
        .export_values();

    // --- Expose LimitOrder ---
    py::class_<Server::LimitOrder>(m, "LimitOrder")
        .def(py::init<>())  // Keep default constructor
        .def(py::init<Server::UserID, Server::OrderID, Server::OrderSide, float, float>(),
            py::arg("user_id"), py::arg("order_id"), py::arg("side"), py::arg("price"), py::arg("volume"))
        .def_readwrite("user_id", &Server::LimitOrder::user_id)
        .def_readwrite("order_id", &Server::LimitOrder::order_id)
        .def_readwrite("side", &Server::LimitOrder::side)
        .def_readwrite("price", &Server::LimitOrder::price)
        .def_readwrite("volume", &Server::LimitOrder::volume);


    // --- Expose CancelOrder ---
    py::class_<Server::CancelOrder>(m, "CancelOrder")
        .def(py::init<>())
        .def(py::init<Server::UserID, Server::OrderID>(),
            py::arg("user_id"), py::arg("order_id"))
        .def_readwrite("user_id", &Server::CancelOrder::user_id)
        .def_readwrite("order_id", &Server::CancelOrder::order_id);


    // --- Expose Transaction ---
    py::class_<Server::Transaction>(m, "Transaction")
        .def(py::init<>())
        .def_readwrite("price", &Server::Transaction::price)
        .def_readwrite("volume", &Server::Transaction::volume)
        .def_readwrite("buyer_id", &Server::Transaction::buyer_id)
        .def_readwrite("seller_id", &Server::Transaction::seller_id);

    // --- Expose CommandVariant ---
    // With stl_variant support, pybind11 can convert between the variant types.
    // Additionally, we set an alias using a Python Union from the typing module.
    m.attr("CommandVariant") =
        py::module::import("typing").attr("Union")[
            py::type::of<Server::LimitOrder>(),
                py::type::of<Server::CancelOrder>()
        ];

    py::class_<Server::Simulation::SimulationStepResult>(m, "SimulationStepResult")
        .def(py::init<>())
        .def_readwrite("partially_transacted_orders", &Server::Simulation::SimulationStepResult::partially_transacted_orders)
        .def_readwrite("fully_transacted_orders", &Server::Simulation::SimulationStepResult::fully_transacted_orders)
        .def_readwrite("cancelled_orders", &Server::Simulation::SimulationStepResult::cancelled_orders)
        .def_readwrite("transactions", &Server::Simulation::SimulationStepResult::transactions)
        .def_readwrite("order_book_depth_per_security", &Server::Simulation::SimulationStepResult::order_book_depth_per_security)
        .def_readwrite("order_book_per_security", &Server::Simulation::SimulationStepResult::order_book_per_security)
        .def_readwrite("portfolios", &Server::Simulation::SimulationStepResult::portfolios)
        .def_readwrite("user_id_to_username_map", &Server::Simulation::SimulationStepResult::user_id_to_username_map)
        .def_readwrite("current_step", &Server::Simulation::SimulationStepResult::current_step)
        .def_readwrite("has_next_step", &Server::Simulation::SimulationStepResult::has_next_step);

    // --- Expose ISecurity (as abstract base) ---
    py::class_<Server::ISecurity, PyISecurity, std::shared_ptr<Server::ISecurity>>(m, "ISecurity")
        .def(py::init<>())
        .def("is_tradeable", &Server::ISecurity::is_tradeable)
        .def("before_step", &Server::ISecurity::before_step)
        .def("after_step", &Server::ISecurity::after_step)
        .def("on_simulation_start", &Server::ISecurity::on_simulation_start)
        .def("on_simulation_end", &Server::ISecurity::on_simulation_end)
        .def("on_trade_executed", &Server::ISecurity::on_trade_executed);

    // --- Expose Simulation ---
    // Note: We use std::shared_ptr as our holder type.
    py::class_<Server::Simulation, std::shared_ptr<Server::Simulation>>(m, "Simulation")
        .def(py::init<std::map<Server::SecurityTicker, std::shared_ptr<Server::ISecurity>>&&, uint32_t, float>(),
            py::arg("ticker_to_security"), py::arg("N"), py::arg("T"))
        .def("add_user", &Server::Simulation::add_user)
        .def("get_all_tickers", &Server::Simulation::get_all_tickers)
        .def("get_security_ticker", &Server::Simulation::get_security_ticker)
        .def("get_security_id", &Server::Simulation::get_security_id)
        // Expose other public methods as needed:
        .def("get_order_book", &Server::Simulation::get_order_book)
        .def("get_dt", &Server::Simulation::get_dt)
        .def("get_t", &Server::Simulation::get_t)
        .def("get_current_step", &Server::Simulation::get_current_step)
        .def("get_N", &Server::Simulation::get_N)
        .def("reset_simulation", &Server::Simulation::reset_simulation)
        .def("do_simulation_step", &Server::Simulation::do_simulation_step);
}
