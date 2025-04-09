#include <iostream>
#include <mutex>
#include <queue>
#include <optional>
#include <exception>
#include <random>
#include <map>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <uwebsockets/App.h>
// To add: https://github.com/johnmcfarlane/cnl


/*
find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG)

# pybind11 method:
pybind11_add_module(MyModule1 src1.cpp)

# Python method:
Python_add_library(MyModule2 src2.cpp)
target_link_libraries(MyModule2 pybind11::headers)
set_target_properties(MyModule2 PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION ON
    CXX_VISIBILITY_PRESET ON
    VISIBILITY_INLINES_HIDDEN ON
)

*/

float roundToTwoDecimalPlaces(float value) {
    return std::round(value * 100.0f) / 100.0f;
}

using ID = uint32_t;
using f32 = float;
using f64 = double;

struct Order {
    ID id;
};

struct LimitOrder {
    f32 price;
    uint32_t quantity;
    uint32_t timestamp;
    ID id;

    bool bid_comparator(const LimitOrder& other) const noexcept {
        if (price > other.price) return true;    // Higher price first
        if (price < other.price) return false;
        if (timestamp < other.timestamp) return true; // Earlier timestamp first
        if (timestamp > other.timestamp) return false;
        return id < other.id;  // Lower ID first
    }

    // Comparator for ask queue: ascending price, ascending timestamp, ascending id
    bool ask_comparator(const LimitOrder& other) const noexcept {
        if (price < other.price) return true;   // Lower price first
        if (price > other.price) return false;
        if (timestamp < other.timestamp) return true; // Earlier timestamp first
        if (timestamp > other.timestamp) return false;
        return id < other.id;  // Lower ID first
    }

    struct BidComparator {
        bool operator()(const NonOwningPtr<LimitOrder>& lhs, const NonOwningPtr<LimitOrder>& rhs) const {
            return lhs->bid_comparator(*rhs);
        }
    };

    // Custom comparator for ask queue (ascending price, ascending timestamp, ascending id)
    struct AskComparator {
        bool operator()(const NonOwningPtr<LimitOrder>& lhs, const NonOwningPtr<LimitOrder>& rhs) const {
            return lhs->ask_comparator(*rhs);
        }
    };
};

class SimulationException : public std::exception {
public:
    explicit SimulationException(const char const* message) : std::exception(message) {}
};

class SecuritySimulation {
protected:
    explicit SecuritySimulation() {}
public:
};

class CLOB {
    std::set<NonOwningPtr<LimitOrder>, LimitOrder::BidComparator> bid_queue = {};
    std::map<ID, std::unique_ptr<LimitOrder>> bid_dict = {};
    std::set<NonOwningPtr<LimitOrder>, LimitOrder::AskComparator> ask_queue = {};
    std::map<ID, std::unique_ptr<LimitOrder>> ask_dict = {};
public:
    std::vector<ID> get_all_bid_ids() const {
        auto bid_ids = std::vector<ID>();
        for (const auto& order : bid_dict) {
            bid_ids.push_back(order.first);
        }
        return bid_ids;
    }
    bool cancel_bid(ID id) {
        if (const auto it = bid_dict.find(id); it != bid_dict.end()) {
            bid_queue.erase(it->second.get());
            bid_dict.erase(it->first);
            return true;
        }
        return false;
    }
    std::vector<ID> get_all_ask_ids() const {
        auto ask_ids = std::vector<ID>();
        for (const auto& order : ask_dict) {
            ask_ids.push_back(order.first);
        }
        return ask_ids;
    }
    bool cancel_ask(ID id) {
        if (const auto it = ask_dict.find(id); it != ask_dict.end()) {
            ask_queue.erase(it->second.get());
            ask_dict.erase(it->first);
            return true;
        }
        return false;
    }
    bool submit_bid(NonOwningPtr<LimitOrder> order) {
        auto owner = std::unique_ptr<LimitOrder>(order);
        auto weak = NonOwningPtr<LimitOrder>(order);
        bid_queue.insert(weak);
        bid_dict.insert({ order->id, std::move(owner) });
        return true;
    }
    bool submit_ask(NonOwningPtr<LimitOrder> order) {
        auto owner = std::unique_ptr<LimitOrder>(order);
        auto weak = NonOwningPtr<LimitOrder>(order);
        ask_queue.insert(weak);
        ask_dict.insert({ order->id, std::move(owner) });
        return true;
    }
    
    const LimitOrder& top_bid() const {
        // TODO: implement
    }
    const LimitOrder& top_ask() const {
        // TODO: implement
    }

    std::vector<LimitOrder> get_bid_book() const {
        // TODO: implement
    }
    std::vector<LimitOrder> get_ask_book() const {
        // TODO: implement
    }

    std::pair<std::map<f32, uint32_t>, std::map<f32, uint32_t>> get_cumulative_depth() const {
    // TODO: implement
    }

    struct Transaction {
        f32 price;
        uint32_t quantity;
    };
    std::vector<Transaction> process_transactions() {
        // TODO: implement
    }
};

using RngGenerator = std::mt19937;
template<typename T> using NonOwningPtr = T*;

class OrderSDE_V1 {
    NonOwningPtr<RngGenerator> rng;
    std::normal_distribution<f32> Z = std::normal_distribution<f32>(0.0, 1.0);
public:
    explicit OrderSDE_V1(NonOwningPtr<RngGenerator> rng) : rng{ rng } {}

private:
    f32 adjustment_factor(uint32_t order_count) {
        return std::sqrt((double)order_count);
    }
    f32 unadjusted_volatitlity(f32 t) {
        if (0.8 <= t) {
            return 0.025f;
        }
        else if ((0.4 <= t) && (t <= 0.6)) {
            return 1.0f;
        }
        else {
            return 0.2f;
        }
    }
    f32 unadjusted_reversion(f32 t) {
        if (0.8 <= t) {
            return 30.0f;
        }
        else if (0.5 <= t) {
            return unadjusted_volatitlity(t) * (5.0f + 30.0f * t);
        }
        else {
            return unadjusted_volatitlity(t) * 5.0f;
        }
    }
    f32 get_reversion(f32 t) {
        return adjustment_factor(get_order_count(t)) * unadjusted_reversion(t);
    }
public:
    uint32_t get_order_count(f32 t) {
        return (uint32_t)(15 * unadjusted_volatitlity(t) + 5);
    }
    f32 get_volatility(f32 t) {
        return 3.0f * adjustment_factor(get_order_count(t)) * unadjusted_volatitlity(t);
    }
    f32 get_spread() {
        return (f32)0.04f;
    }
    f32 get_stock_price(f32 t) {
        if (0.5 <= t) {
            return 110.0f;
        }
        else {
            return 100.0f;
        }
    }
    std::vector<f32> generate_price_vector(f32 price, f32 t, f32 dt, bool isBid) {
        auto order_count = get_order_count(t);
        auto half_spread = get_spread() / 2.0f;
        auto target = get_stock_price(t);
        auto volatility = get_volatility(t);
        auto reversion = get_reversion(t);

        auto price_vector = std::vector<f32>(order_count);
        std::generate(price_vector.begin(), price_vector.end(), [&]() {
            auto r = volatility * std::sqrt(price * dt) * Z(*rng);
            auto d = price + (isBid ? (-1.0f) : (1.0f)) * half_spread + reversion * (target - price) * dt;
            return d + r;
        });
        return price_vector;
    }
    void generate_book(uint32_t &action_count, CLOB &clob) {
        auto ORDERS_COUNT = get_order_count(0.0f);
        auto U = std::uniform_real_distribution<f32>(0.75, 1.0);
        auto I = std::uniform_int_distribution<uint32_t>(1, 5);

        auto price_distribution = std::vector<f32>(ORDERS_COUNT);
        std::generate(price_distribution.begin(), price_distribution.end(), [&]() {
            return U(*rng);
        });

        auto volume_distribution = std::vector<uint32_t>(ORDERS_COUNT);
        std::generate(volume_distribution.begin(), volume_distribution.end(), [&]() {
            return I(*rng);
        });

        auto t = 0.0f;

        auto target = get_stock_price(t);
        auto volatility = get_volatility(t);
        auto SPREAD = get_spread();
        auto HALF_SPREAD = SPREAD / 2.0f;

        auto bid_top_price = (target - HALF_SPREAD);
        auto bid_bottom_price = (bid_top_price - 0.5f * volatility * bid_top_price);
        auto bid_prices = std::vector<f32>(ORDERS_COUNT);
        for (std::size_t i = 0; i < ORDERS_COUNT; i++) {
            bid_prices[i] = roundToTwoDecimalPlaces(bid_top_price * price_distribution[i] + bid_bottom_price * (1.0f - price_distribution[i]));
        }

        auto ask_top_price = (target + HALF_SPREAD);
        auto ask_bottom_price = (ask_top_price + 0.5f * volatility * ask_top_price);
        auto ask_prices = std::vector<f32>(ORDERS_COUNT);
        for (std::size_t i = 0; i < ORDERS_COUNT; i++) {
            ask_prices[i] = roundToTwoDecimalPlaces(ask_bottom_price * price_distribution[i] + ask_top_price * (1.0f - price_distribution[i]));
        }

        for (std::size_t i = 0; i < ORDERS_COUNT; i++) {
            clob.submit_bid(new LimitOrder{
                .price = bid_prices[i],
                .quantity = volume_distribution[i],
                .timestamp = 0,
                .id = action_count
            });
            clob.submit_ask(new LimitOrder{
                .price = ask_prices[i],
                .quantity = volume_distribution[i],
                .timestamp = 0,
                .id = action_count + 1
            });
            action_count += 2;
        }
    }
};

class SimulationOrderSDE_V1 : public SecuritySimulation {
    CLOB clob = CLOB();
    f32 T;
    uint32_t N;
    f32 dt;
    f32 removal_percentage = 0.1f;
    OrderSDE_V1 order_model;
    NonOwningPtr<RngGenerator> rng;
    uint32_t action_count = 0;
    uint32_t timestamp = 0;

    std::uniform_real_distribution<f64> U = std::uniform_real_distribution<f64>(0.0, 1.0);
    std::uniform_int_distribution<uint32_t> I = std::uniform_int_distribution<uint32_t>(1, 5);
public:
    explicit SimulationOrderSDE_V1(f32 T, uint32_t N, NonOwningPtr<RngGenerator> rng)
        : SecuritySimulation(), T{ T }, N{ N }, dt{ T / N }, rng{ rng }, order_model{ OrderSDE_V1(rng) } {
    }

    f32 get_N() {
        return N;
    }

private:
    void remove_orders() {
        auto bid_ids = clob.get_all_bid_ids();
        for (auto id : bid_ids) {
            auto u = U(*rng);
            if (u < removal_percentage) {
                clob.cancel_bid(id);
            }
        }
        auto ask_ids = clob.get_all_ask_ids();
        for (auto id : ask_ids) {
            auto u = U(*rng);
            if (u < removal_percentage) {
                clob.cancel_ask(id);
            }
        }
    }
public:
    struct SimulationResult {
        std::vector<CLOB::Transaction> transactions;
        std::pair<std::map<f32, uint32_t>, std::map<f32, uint32_t>> depths;
        f32 top_bid;
        f32 top_ask;
        std::vector<LimitOrder> bid_book;
        std::vector<LimitOrder> ask_book;
    };

    SimulationResult do_simulation_step() throw(SimulationException) {
        if (timestamp > N) {
            throw SimulationException("Simulation is finished");
        }
        f32 t = timestamp * dt;
        
        if (t == 0.0f) {
            order_model.generate_book(action_count, clob);
        }
        else {
            remove_orders();

            auto top_bid = clob.top_bid();
            auto top_ask = clob.top_ask();
            auto top_bid_price = top_bid.price;
            auto top_ask_price = top_ask.price;

            auto bid_prices = order_model.generate_price_vector(top_bid_price, t, dt, true);
            auto ask_prices = order_model.generate_price_vector(top_ask_price, t, dt, false);

            for (const auto bid_price : bid_prices) {
                clob.submit_bid(new LimitOrder{
                    .price = bid_price,
                    .quantity = I(*rng),
                    .timestamp = timestamp,
                    .id = action_count
                });
                action_count += 1;
            }

            for (const auto ask_price : ask_prices) {
                clob.submit_ask(new LimitOrder{
                    .price = ask_price,
                    .quantity = I(*rng),
                    .timestamp = timestamp,
                    .id = action_count
                });
                action_count += 1;
            }
        }

        auto clob_result = clob.process_transactions();
        auto new_top_bid_price = clob.top_bid().price;
        auto new_top_ask_price = clob.top_ask().price;

        timestamp += 1;
        return {
            .transactions = std::move(clob_result),
            .depths = clob.get_cumulative_depth(),
            .top_bid = new_top_bid_price,
            .top_ask = new_top_ask_price,
            .bid_book = clob.get_bid_book(),
            .ask_book = clob.get_ask_book()
        };
    }
};

class Market {

    std::queue<Order>* orderQueue = new std::queue<Order>();
    std::mutex orderQueueMutex = std::mutex();
public:
    void ExecuteQueue() {
        std::queue<Order>* currentQueue = nullptr;
        {
            std::lock_guard lock = std::lock_guard(orderQueueMutex);
            currentQueue = orderQueue;
            orderQueue = new std::queue<Order>();
        }
        if (currentQueue == nullptr) {
            throw std::exception("Failed getting `currentQueue`.");
        }


        delete currentQueue;
    }

private:
    bool IsOrderValid(const Order& order) {
        return false;
    }
public:

    std::optional<std::string> SubmitOrder(Order& order) {
        if (!IsOrderValid(order)) {
            return "Submitted order is not valid.";
        }

        {
            std::lock_guard lock = std::lock_guard(orderQueueMutex);
            orderQueue->push(order);
        }
        return std::nullopt;
    }

    bool DoesOrderExist(ID id) {
        return false;
    }

    bool CancelOrder(ID id) {
        return false;
    }

    std::optional<Order> GetOrder(ID id) {
        return std::nullopt;
    }
};


void loop_add_orders() {}

void loop_market_step() {}

int main()
{
    std::cout << "Hello World!\n";
}
