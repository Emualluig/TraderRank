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

#include <pybind11/pybind11.h>
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

        // Returns the updated value
        float change_user_position(UserID user_id, SecurityID security_id, float change) {
            assert(user_id < user_count);
            assert(security_id < columns);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto& ref = data[user_id * columns + security_id];
            ref += change;
            return ref;
        }
        
        // Returns the updated values
        std::pair<float, float> change_user_position(
            UserID user_id, 
            SecurityID security_id_1, float change_1, 
            SecurityID security_id_2, float change_2
        ) {
            assert(user_id < user_count);
            assert(security_id_1 < columns);
            assert(security_id_2 < columns);
            assert(security_id_1 != security_id_2);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto& ref_1 = data[user_id * columns + security_id_1];
            ref_1 += change_1;
            auto& ref_2 = data[user_id * columns + security_id_2];
            ref_2 += change_2;
            return { ref_1, ref_2 };
        }

        // Takes the value in `security_id_to_decrease`, multiplies it by `scale_by` and adds it to `security_id_to_scale`
        // and then sets `security_id_to_decrease` to 0. Returns new value in `security_id_to_scale`. 
        float change_user_transfer_and_scale(
            UserID user_id, SecurityID security_id_to_decrease, SecurityID security_id_to_scale, float scale_by
        ) {
            assert(user_id < user_count);
            assert(security_id_to_decrease < columns);
            assert(security_id_to_scale < columns);
            assert(security_id_to_decrease != security_id_to_scale);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto& ref_to_decrease = data[user_id * columns + security_id_to_decrease];
            auto& ref_to_scale = data[user_id * columns + security_id_to_scale];
            ref_to_scale += ref_to_decrease * scale_by;
            ref_to_decrease = 0;
            return ref_to_scale;
        }

        // Multiplies `security_as_base` by `scale_by` and then adds it to `security_to_add_multiply`
        float change_user_add_multiply_by(
            UserID user_id, SecurityID security_as_base, SecurityID security_to_add_multiply, float scale_by
        ) {
            assert(user_id < user_count);
            assert(security_as_base < columns);
            assert(security_to_add_multiply < columns);
            assert(security_as_base != security_to_add_multiply);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto& ref_as_base = data[user_id * columns + security_as_base];
            auto& ref_to_scale = data[user_id * columns + security_to_add_multiply];
            ref_to_scale += ref_as_base * scale_by;
            return ref_to_scale;
        }

        std::vector<float> get_user_portfolio_copy(UserID user_id) const {
            assert(user_id < user_count);

            auto read_lock = std::shared_lock(data_mutex);
            auto user_lock = std::unique_lock(user_mutexes[user_id]);

            auto result = std::vector<float>(columns);
            std::memcpy(result.data(), data.get() + user_id * columns, columns * sizeof(float));
            return result;
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
        uint32_t current_step = 0;
        const std::map<SecurityTicker, std::shared_ptr<ISecurity>> ticker_to_security;
        UserPortfolioTable user_table;
        std::map<SecurityID, SecurityTicker> id_to_ticker;
        std::map<SecurityID, std::shared_ptr<ISecurity>> id_to_security;
        std::vector<std::shared_ptr<ISecurity>> securities;
        std::vector<SecurityID> security_ids;
        std::vector<SecurityTicker> security_tickers;
        std::vector<bool> security_tradability;
        std::vector<OrderBook> order_books;
    public:
        explicit Simulation(
            std::map<SecurityTicker, std::shared_ptr<ISecurity>>&& ticker_to_security,
            uint32_t N,
            float T
        ) : N{ N }, T{ T }, ticker_to_security{ std::move(ticker_to_security) }, user_table{ UserPortfolioTable(ticker_to_security.size()) } {
            SecurityID security_id = 0;
            for (const auto& [ticker, security] : ticker_to_security) {
                id_to_ticker.emplace(security_id, ticker);
                id_to_security.emplace(security_id, security);

                securities.push_back(security);
                security_ids.push_back(security_id);
                security_tickers.push_back(ticker);
                security_tradability.push_back(security->is_tradeable());
                order_books.push_back(OrderBook());

                security_id += 1;
            }
        }

        UserPortfolioTable& get_user_table() {
            return user_table;
        }

        std::vector<std::shared_ptr<ISecurity>>& get_securities() {
            return securities;
        }

        std::vector<SecurityID>& get_security_ids() {
            return security_ids;
        }

        SecurityID get_security_id(const SecurityTicker& ticker) const {
            for (uint32_t i = 0; i < security_tickers.size(); i++) {
                if (security_tickers[i] == ticker) {
                    return i;
                }
            }
            throw std::runtime_error("Could not find such a ticker.");
        }

        std::shared_ptr<ISecurity>& get_security(SecurityID security_id) {
            assert(id_to_security.find(security_id) != id_to_security.end());
            return id_to_security[security_id];
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

        bool is_security_tradable(SecurityID security_id) const {
            assert(security_id < security_tradability.size());
            return security_tradability[security_id];
        }

        uint32_t get_current_step() const {
            return current_step;
        }

        void increment_step() {
            current_step += 1;
        }

        float get_dt() const {
            return T / N;
        }

        float get_t() const {
            return current_step * T / N;
        }

        uint32_t get_N() const {
            return N;
        }
        
        // Ignore these for now
        void on_simulation_start_1() {}
        void on_simulation_start_2() {}
        void on_simulation_end_1() {}
        void on_simulation_end_2() {}

    private:
        std::map<UserID, Username> user_id_to_username = {};
    public:
        UserID add_user(const Username& username) {
            auto user_id = user_table.register_new_user();
            user_id_to_username.emplace(user_id, username);
            return user_id;
        }

        const std::map<UserID, Username>& get_user_id_to_username_map() const {
            return user_id_to_username;
        }

        std::map<SecurityID, std::queue<CommandVariant>> get_user_commands() {
            std::map<SecurityID, std::queue<CommandVariant>> retval = {};

            for (const auto& security_id : security_ids) {
                retval[security_id] = {};
            }
            
            assert(retval.size() == ticker_to_security.size());
            return retval;
        }
    };

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
        ) override {}
    };

    class Security_BOND : public Server::ISecurity {
        float rate = 0.05f;
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
            auto& user_table = simulation.get_user_table();
            for (UserID index_user = 0; index_user < user_table.get_user_count(); index_user++) {
                // The bond pays `rate * dt` per step, having more bonds increases nomial amount added to cad
                user_table.change_user_add_multiply_by(
                    index_user, bond_id, cad_id, rate * dt
                );
            }
        }
        void on_simulation_start(Simulation& simulation) override {}
        void on_simulation_end(Simulation& simulation) override {
            auto bond_id = simulation.get_security_id("BOND");
            auto cad_id = simulation.get_security_id("CAD");
            auto& user_table = simulation.get_user_table();
            for (UserID index_user = 0; index_user < user_table.get_user_count(); index_user++) {
                // Reduce the amount of bond to 0, and realize it as CAD (each bond is worth 100).
                user_table.change_user_transfer_and_scale(
                    index_user, bond_id, cad_id, 100.0f
                );
            }
        }
        void on_trade_executed(
            Simulation& simulation,
            Server::UserID buyer, Server::UserID seller, float price, float quantity
        ) override {
            auto bond_id = simulation.get_security_id("BOND");
            auto cad_id = simulation.get_security_id("CAD");
            auto& user_table = simulation.get_user_table();
            // The bond buyer gets the bond, but losses money
            user_table.change_user_position(buyer, bond_id, quantity, cad_id, -price * quantity);
            // The bond seller losses the bond, but gets money
            user_table.change_user_position(seller, bond_id, -quantity, cad_id, price * quantity);
        }      
    };

    class BindSimulation : public std::enable_shared_from_this<BindSimulation> {
        struct Private { explicit Private() = default; };

        uint32_t N = 100;
        float T = 1.0f;
        Server::Simulation simulation;
    public:
        explicit BindSimulation(Private) : simulation { 
            Server::Simulation({
                std::make_pair("CAD", std::make_shared<Security_CAD>(Security_CAD())),
                std::make_pair("BOND", std::make_shared<Security_BOND>(Security_BOND()))
            }, N, T)
        } {
            auto anon_user_id = simulation.add_user("ANON");
        }

        std::vector<SecurityTicker> get_tickers() const {
            return simulation.get_all_tickers();
        }

        std::string do_simulation_step(std::shared_ptr<std::map<SecurityID, std::queue<CommandVariant>>> queues) {
            // Perform a simulaiton step
            auto step = simulation.get_current_step(); // step ∈ [0, ..., N] inclusive
            if (step > simulation.get_N()) {
                throw std::runtime_error("Passed simulation endpoint!");
            }

            auto t = simulation.get_t(); // t ∈ [0, ..., T]
            auto dt = simulation.get_dt(); // dt = T / N

            if (step == 0) {
                simulation.on_simulation_start_1();
                for (auto& security : simulation.get_securities()) {
                    security->on_simulation_start(simulation);
                }
                simulation.on_simulation_start_2();
            }

            for (auto& security : simulation.get_securities()) {
                security->before_step(simulation);
            }

            // Keep track of market updates
            std::map<SecurityTicker, std::map<OrderID, float>> partially_transacted_orders = {}; // ticker -> order_id -> new volume
            std::map<SecurityTicker, std::set<OrderID>> fully_transacted_orders = {};
            std::map<SecurityTicker, std::set<OrderID>> cancelled_orders = {};
            std::map<SecurityTicker, std::vector<Transaction>> transactions = {};

            for (auto security_id : simulation.get_security_ids()) {
                auto& security_class = simulation.get_security(security_id);
                auto& order_book = simulation.get_order_book(security_id);
                auto& commands = queues->at(security_id);

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
                                    security_class->on_trade_executed(simulation, buyer_id, seller_id, transacted_price, transacted_volume);
                                    local_transactions.push_back(Transaction{ .price = transacted_price, .volume = transacted_volume });
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
                    commands.pop();
                }

                // Save the differences to a simulation step object
                const auto& ticker = simulation.get_security_ticker(security_id);
                partially_transacted_orders.emplace(ticker, std::move(local_partially_transacted_orders));
                fully_transacted_orders.emplace(ticker, std::move(local_fully_transacted_orders));
                cancelled_orders.emplace(ticker, std::move(local_cancelled_orders));
                transactions.emplace(ticker, std::move(local_transactions));
            }

            for (auto& security : simulation.get_securities()) {
                security->after_step(simulation);
            }

            if (step == N) {
                simulation.on_simulation_end_1();
                for (auto& security : simulation.get_securities()) {
                    security->on_simulation_end(simulation);
                }
                simulation.on_simulation_end_2();
            }

            auto order_book_depth_per_security = std::map<SecurityTicker, OrderBook::BookDepth>();
            auto order_book_per_security = std::map<SecurityTicker, std::pair<std::vector<LimitOrder>, std::vector<LimitOrder>>>();
            for (auto security_id : simulation.get_security_ids()) {
                const auto& ticker = simulation.get_security_ticker(security_id);
                const auto& order_book = simulation.get_order_book(security_id);
                order_book_depth_per_security[ticker] = std::move(order_book.get_book_depth());
                order_book_per_security[ticker] = order_book.get_limit_orders();
            }

            auto& user_table = simulation.get_user_table();
            auto portfolios = user_table.get_portfolio_table();

            auto& user_id_to_username_map = simulation.get_user_id_to_username_map();

            nlohmann::json json = {};
            json.emplace("partially_transacted_orders", partially_transacted_orders);
            json.emplace("fully_transacted_orders", fully_transacted_orders);
            json.emplace("cancelled_orders", cancelled_orders);
            json.emplace("transactions", transactions);
            json.emplace("order_book_depth_per_security", order_book_depth_per_security);
            json.emplace("order_book_per_security", order_book_per_security);
            json.emplace("portfolios", portfolios);
            json.emplace("user_id_to_username_map", user_id_to_username_map);

            simulation.increment_step();
            return json.dump();
        }

        static std::shared_ptr<BindSimulation> create()
        {
            return std::make_shared<BindSimulation>(Private());
        }

        std::shared_ptr<BindSimulation> getptr()
        {
            return shared_from_this();
        }
    };
};

namespace nlohmann {
    void to_json(json& j, const Server::OrderSide& side) {
        j = std::string{ magic_enum::enum_name(side) };
    }

    void to_json(nlohmann::json& j, const Server::Transaction& t) {
        j = nlohmann::json{
            {"price", t.price},
            {"volume", t.volume}
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


int meaning_of_life() {
    return 42;
}

PYBIND11_MODULE(Server, m) {
    m.doc() = "Example pybind11 module";

    m.def("meaning_of_life", &meaning_of_life, "Returns the meaning of life.");

    py::enum_<Server::OrderSide>(m, "OrderSide")
        .value("BID", Server::OrderSide::BID)
        .value("ASK", Server::OrderSide::ASK)
        .export_values();

    py::class_<Server::LimitOrder>(m, "LimitOrder")
        .def(py::init<>())
        .def_readwrite("user_id", &Server::LimitOrder::user_id)
        .def_readwrite("order_id", &Server::LimitOrder::order_id)
        .def_readwrite("side", &Server::LimitOrder::side)
        .def_readwrite("price", &Server::LimitOrder::price)
        .def_readwrite("volume", &Server::LimitOrder::volume);

    py::class_<Server::CancelOrder>(m, "CancelOrder")
        .def(py::init<>())
        .def_readwrite("user_id", &Server::CancelOrder::user_id)
        .def_readwrite("order_id", &Server::CancelOrder::order_id);

    py::class_<Server::Transaction>(m, "Transaction")
        .def(py::init<>())
        .def_readwrite("price", &Server::Transaction::price)
        .def_readwrite("volume", &Server::Transaction::volume);

    py::class_<Server::BindSimulation, std::shared_ptr<Server::BindSimulation>>(m, "BindSimulation")
        .def_static("create", &Server::BindSimulation::create)
        .def("getptr", &Server::BindSimulation::getptr)
        .def("get_tickers", &Server::BindSimulation::get_tickers)
        .def("do_simulation_step", &Server::BindSimulation::do_simulation_step,
            py::arg("queues"));

    py::class_<std::queue<Server::CommandVariant>>(m, "CommandQueue")
        .def(py::init<>())
        .def("push_limit_order", [](std::queue<Server::CommandVariant>& q, const Server::LimitOrder& lo) {
        q.push(lo);
            })
        .def("push_cancel_order", [](std::queue<Server::CommandVariant>& q, const Server::CancelOrder& co) {
        q.push(co);
            });

    py::bind_map<std::map<Server::SecurityID, std::queue<Server::CommandVariant>>>(m, "CommandMap");

    py::class_<Server::CommandVariant>(m, "CommandVariant")
        .def("__repr__", [](const Server::CommandVariant& cv) {
        return "CommandVariant()";
            });
}
