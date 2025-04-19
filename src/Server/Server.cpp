// Server.cpp : Defines the entry point for the application.
//
#include <cstdint>
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
#include <exception>
#include <stdexcept>
#include <fmt/color.h>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>
// TODO: investigate if we should use https://github.com/johnmcfarlane/cnl instead of float/double

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

struct IDNotFoundError : std::out_of_range {
	using std::out_of_range::out_of_range;
};

using UserID = uint32_t;
using SecurityID = uint32_t;
using SecurityTicker = std::string;
using OrderID = uint32_t;
using Username = std::string;
using FloatPair = std::pair<float, float>;

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
using OrderVariant = std::variant<LimitOrder, CancelOrder>;

// (bid, ask) pair of depths
using BookDepth = std::pair<std::map<float, float>, std::map<float, float>>;
using FlatOrderBook = std::pair<std::vector<LimitOrder>, std::vector<LimitOrder>>;
class OrderBook {
	using BidSet = std::set<LimitOrder, LimitOrder::BidComparator>;
	using AskSet = std::set<LimitOrder, LimitOrder::AskComparator>;

	BidSet bid_orders;
	AskSet ask_orders;
	std::map<OrderID, BidSet::iterator> bid_map;
	std::map<OrderID, AskSet::iterator> ask_map;
public:
	std::size_t bid_size() const {
		return bid_orders.size();
	}

	std::size_t ask_size() const {
		return ask_orders.size();
	}

	bool has_order(OrderID order_id) {
		return bid_map.find(order_id) != bid_map.end() || ask_map.find(order_id) != ask_map.end();
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

	LimitOrder& top_bid() const {
		if (bid_orders.empty()) {
			throw std::runtime_error("Bid book is empty.");
		}
		return const_cast<LimitOrder&>(*bid_orders.begin());
	}

	LimitOrder& top_ask() const {
		if (ask_orders.empty()) {
			throw std::runtime_error("Ask book is empty.");
		}
		return const_cast<LimitOrder&>(*ask_orders.begin());
	}

	void pop_top_bid() {
		if (bid_orders.empty()) {
			throw std::runtime_error("Bid book is empty.");
		}
		auto it = bid_orders.begin();
		bid_map.erase(it->order_id);
		bid_orders.erase(it);
	}

	void pop_top_ask() {
		if (ask_orders.empty()) {
			throw std::runtime_error("Ask book is empty.");
		}
		auto it = ask_orders.begin();
		ask_map.erase(it->order_id);
		ask_orders.erase(it);
	}

	BookDepth get_book_depth() const {
		std::map<float, float> bid_depth;
		std::map<float, float> ask_depth;

		// For bids, accumulate by descending price
		float accumulated_bid_depth = 0.0f;
		for (const auto& order : bid_orders) {
			accumulated_bid_depth += order.volume;
			bid_depth[order.price] = accumulated_bid_depth;
		}

		// For asks, accumulate by ascending price
		float accumulated_ask_depth = 0.0f;
		for (const auto& order : ask_orders) {
			accumulated_ask_depth += order.volume;
			ask_depth[order.price] = accumulated_ask_depth;
		}

		return { bid_depth, ask_depth };
	}

	FlatOrderBook get_limit_orders() const {
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

struct Transaction {
	float price;
	float volume;
	UserID buyer_id;
	UserID seller_id;
};

struct SimulationStepResult {
	std::map<SecurityTicker, std::map<OrderID, float>> partially_transacted_orders;
	std::map<SecurityTicker, std::set<OrderID>> fully_transacted_orders;
	std::map<SecurityTicker, std::set<OrderID>> cancelled_orders;
	std::map<SecurityTicker, std::vector<Transaction>> transactions;
	std::map<SecurityTicker, BookDepth> order_book_depth_per_security;
	std::map<SecurityTicker, FlatOrderBook> order_book_per_security;
	std::vector<std::vector<float>> portfolios;
	std::map<UserID, Username> user_id_to_username_map;
	uint32_t current_step;
	bool has_next_step;
};

class ISecurity;

class ISimulation {
protected:
	std::vector<SecurityTicker> tickers = {};
	std::map<SecurityTicker, SecurityID> ticker_to_id = {};
	std::map<SecurityTicker, std::shared_ptr<ISecurity>> ticker_to_security = {};
	std::vector<std::shared_ptr<ISecurity>> securities_vector = {};
	const float T;
	const uint32_t N;
	uint32_t tick = 0;
	std::map<UserID, Username> user_id_to_username = {};
protected:
	std::vector<std::shared_ptr<ISecurity>>& get_securities() noexcept {
		return securities_vector;
	}
	void increment_tick() noexcept {
		tick += 1;
	}
	void reset_tick_to_zero() noexcept {
		tick = 0;
	}
public:
	explicit ISimulation(
		const std::map<SecurityTicker, std::shared_ptr<ISecurity>>& securities,
		float T, uint32_t N
	) : T{ T }, N{ N } {
		uint32_t security_id = 0;
		for (const auto& [ticker, security] : securities) {
			tickers.push_back(ticker);
			ticker_to_id.emplace(ticker, security_id);
			ticker_to_security.emplace(ticker, security);
			securities_vector.push_back(security);
			security_id += 1;
		}
	}
	virtual ~ISimulation() = default;

	// Utility methods
	const std::vector<SecurityTicker>& get_all_tickers() const noexcept {
		return tickers;
	};
	const SecurityTicker& get_security_ticker(SecurityID security_id) const {
		if (security_id >= tickers.size()) {
			throw IDNotFoundError(fmt::format("The id `{}` doesn't exist.", security_id));
		}
		return tickers.at(security_id);
	};
	SecurityID get_security_id(const SecurityTicker& security_ticker) const {
		const auto it = ticker_to_id.find(security_ticker);
		if (it == ticker_to_id.end()) {
			throw IDNotFoundError(fmt::format("The ticker `{}` doesn't exist.", security_ticker));
		}
		return it->second;
	};
	const std::map<UserID, Username>& get_user_id_to_username() const noexcept {
		return user_id_to_username;
	}
	uint32_t get_securities_count() const noexcept {
		return securities_vector.size();
	}

	// Simulation meta information
	float get_dt() const noexcept {
		return T / N;
	};
	float get_t() const noexcept {
		return tick * T / N;
	};
	float get_T() const noexcept {
		return T;
	};
	uint32_t get_tick() const noexcept {
		return tick;
	};
	uint32_t get_N() const noexcept {
		return N;
	};

	// User management
	virtual UserID add_user(const Username& username) = 0; // May throw
	virtual uint32_t get_user_count() const noexcept = 0;
	virtual std::vector<float> get_user_portfolio(UserID user_id) const = 0; // May throw

	// Simulation order book information
	virtual const LimitOrder& get_top_bid(SecurityID security_id) const = 0; // May throw
	virtual const LimitOrder& get_top_ask(SecurityID security_id) const = 0; // May throw
	virtual uint32_t get_bid_count(SecurityID security_id) const = 0; // May throw
	virtual uint32_t get_ask_count(SecurityID security_id) const = 0; // May throw
	virtual FlatOrderBook get_order_book(SecurityID security_id) const = 0; // May throw
	virtual std::set<OrderID> get_all_open_user_orders(UserID user_id, SecurityID security_id) const = 0; // May throw
	virtual BookDepth get_cumulative_book_depth(SecurityID security_id) const = 0; // May throw

	// Simulation actions
	virtual SimulationStepResult do_simulation_step() = 0; // May throw
	virtual OrderID submit_limit_order(UserID user_id, SecurityID security_id, OrderSide side, float price, float volume) = 0; // May throw
	virtual void submit_cancel_order(UserID user_id, SecurityID security_id, OrderID order_id) = 0; // May throw
	virtual void reset_simulation() = 0;
	// TODO: can I do something better than this?
	virtual OrderID direct_insert_limit_order(UserID user_id, SecurityID security_id, OrderSide side, float price, float volume) = 0;

	// TODO:
	// public:
	// virtual uint32_t get_bid_count(SecurityID security_id) const = 0;
	// virtual uint32_t get_ask_count(SecurityID security_id) const = 0;
	// virtual OrderID submit_market_order(UserID user_id, SecurityID security_id, OrderSide side, float volume) = 0;
	// virtual std::optional<const LimitOrder&> try_get_top_bid(SecurityID security_id) const = 0;
	// virtual std::optional<const LimitOrder&> try_get_top_ask(SecurityID security_id) const = 0;
	// replace `get_N` with `get_max_tick()`?
	// protected:
	// virtual bool does_user_id_exist() const noexcept = 0;
	// virtual bool does_security_id_exist() const noexcept = 0;

};

class IPortfolioManager {
public:
	virtual ~IPortfolioManager() = default;

	virtual uint32_t get_user_count() const noexcept = 0;
	virtual std::vector<std::vector<float>> get_portfolio_table() const noexcept = 0;
	virtual void reset_user_portfolio(UserID user_id) = 0;
	virtual float add_to_security(UserID user_id, SecurityID security_1, float addition_1) = 0; // May throw
	virtual FloatPair add_to_two_securities(UserID user_id, SecurityID security_1, float addition_1, SecurityID security_2, float addition_2) = 0; // May throw
	virtual float multiply_and_add_1_to_2(UserID user_id, SecurityID security_1, SecurityID security_2, float multiply) = 0; // May throw
	virtual float multiply_and_add_1_to_2_and_set_1(UserID user_id, SecurityID security_1, SecurityID security_2, float multiply, float set_value) = 0; // May throw
};

class ISecurity {
public:
	virtual ~ISecurity() = default;

	virtual bool is_tradeable() = 0;
	virtual void before_step(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) = 0;
	virtual void after_step(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) = 0;
	virtual void on_simulation_start(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) = 0;
	virtual void on_simulation_end(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) = 0;
	virtual void on_trade_executed(
		ISimulation& simulation,
		std::shared_ptr<IPortfolioManager> portfolio,
		UserID buyer_id, UserID seller_id, float transacted_price, float transacted_volume
	) = 0;
};

class UserAndPortfolioManager : public IPortfolioManager {
	uint32_t user_count = 0;
	uint32_t capacity = 0;
	const uint32_t columns;

	std::unique_ptr<float[]> data = nullptr;
	mutable std::shared_mutex data_mutex = std::shared_mutex();
	mutable std::unique_ptr<std::mutex[]> user_mutexes;
public:
	explicit UserAndPortfolioManager(const uint32_t columns) : columns{ columns } {}

	const uint32_t get_column_count() const noexcept {
		return columns;
	}

	uint32_t get_user_count() const noexcept override {
		return user_count;
	}

	UserID register_new_user() noexcept {
		auto write_lock = std::unique_lock(data_mutex);
		auto user_id = user_count++;

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

	// Inherited methods
	std::vector<std::vector<float>> get_portfolio_table() const noexcept override {
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

	void reset_user_portfolio(UserID user_id) override {
		if (user_id >= user_count) {
			throw IDNotFoundError(fmt::format("Could not find user with id `{}`.", user_id));
		}
		auto read_lock = std::shared_lock(data_mutex);
		auto user_lock = std::unique_lock(user_mutexes[user_id]);

		std::fill(data.get() + user_id * columns, data.get() + (user_id + 1) * columns, 0.0f);
	}

	// `security_1 += addition_1`
	// returns: the new value of the security in the portfolio
	float add_to_security(UserID user_id, SecurityID security_1, float addition_1) override {
		if (user_id >= user_count) {
			throw IDNotFoundError(fmt::format("Could not find user_id: `{}`.", user_id));
		}
		if (security_1 >= columns) {
			throw IDNotFoundError(fmt::format("Could not find security_1: `{}`.", security_1));
		}
		auto read_lock = std::shared_lock(data_mutex);
		auto user_lock = std::unique_lock(user_mutexes[user_id]);

		auto& ref = data[user_id * columns + security_1];
		ref += addition_1;
		return ref;
	}

	// `security_1 += addition_1`, `security_2 += addition_2`
	// returns: a pair of the new portfolio values
	FloatPair add_to_two_securities(UserID user_id, SecurityID security_1, float addition_1, SecurityID security_2, float addition_2) override {
		if (user_id >= user_count) {
			throw IDNotFoundError(fmt::format("Could not find user_id: `{}`.", user_id));
		}
		if (security_1 >= columns) {
			throw IDNotFoundError(fmt::format("Could not find security_1: `{}`.", security_1));
		}
		if (security_2 >= columns) {
			throw IDNotFoundError(fmt::format("Could not find security_2: `{}`.", security_1));
		}
		if (security_1 == security_2) {
			throw std::runtime_error(fmt::format("Received the same security twice: `{}`", security_1));
		}
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
	float multiply_and_add_1_to_2(UserID user_id, SecurityID security_1, SecurityID security_2, float multiply) override {
		if (user_id >= user_count) {
			throw IDNotFoundError(fmt::format("Could not find user_id: `{}`.", user_id));
		}
		if (security_1 >= columns) {
			throw IDNotFoundError(fmt::format("Could not find security_1: `{}`.", security_1));
		}
		if (security_2 >= columns) {
			throw IDNotFoundError(fmt::format("Could not find security_2: `{}`.", security_1));
		}
		if (security_1 == security_2) {
			throw std::runtime_error(fmt::format("Received the same security twice: `{}`", security_1));
		}
		auto read_lock = std::shared_lock(data_mutex);
		auto user_lock = std::unique_lock(user_mutexes[user_id]);

		auto& ref_to_multiply_from = data[user_id * columns + security_1];
		auto& ref_to_mult_and_add = data[user_id * columns + security_2];
		ref_to_mult_and_add += ref_to_multiply_from * multiply;
		return ref_to_mult_and_add;
	}

	// `security_2 += security_1 * multiply`, then `security_1 = set_value`
	// returns: the new value of security_2
	float multiply_and_add_1_to_2_and_set_1(UserID user_id, SecurityID security_1, SecurityID security_2, float multiply, float set_value) override {
		if (user_id >= user_count) {
			throw IDNotFoundError(fmt::format("Could not find user_id: `{}`.", user_id));
		}
		if (security_1 >= columns) {
			throw IDNotFoundError(fmt::format("Could not find security_1: `{}`.", security_1));
		}
		if (security_2 >= columns) {
			throw IDNotFoundError(fmt::format("Could not find security_2: `{}`.", security_1));
		}
		if (security_1 == security_2) {
			throw std::runtime_error(fmt::format("Received the same security twice: `{}`", security_1));
		}
		auto read_lock = std::shared_lock(data_mutex);
		auto user_lock = std::unique_lock(user_mutexes[user_id]);

		auto& ref_to_set = data[user_id * columns + security_1];
		auto& ref_to_mult_and_add = data[user_id * columns + security_2];
		ref_to_mult_and_add += ref_to_set * multiply;
		ref_to_set = set_value;
		return ref_to_mult_and_add;
	}
};

class GenericSimulation : public ISimulation {
	std::shared_ptr<UserAndPortfolioManager> user_portfolio_manager;
	std::vector<OrderBook> order_books = {};

	std::mutex order_queue_mutex = std::mutex();
	std::map<SecurityID, std::vector<OrderVariant>> submitted_orders = {};
	OrderID order_id_counter = 0;
public:
	explicit GenericSimulation(
		const std::map<SecurityTicker, std::shared_ptr<ISecurity>>& securities,
		float T,
		uint32_t N
	) : ISimulation(securities, T, N) {
		for (uint32_t i = 0; i < securities.size(); i++) {
			order_books.push_back(OrderBook());
			submitted_orders.emplace(i, std::vector<OrderVariant>());
		}
		user_portfolio_manager = std::make_shared<UserAndPortfolioManager>((uint32_t)securities.size());
	}

	// User management
	UserID add_user(const Username& username) override {
		auto user_id = user_portfolio_manager->register_new_user();
		user_id_to_username.emplace(user_id, username);
		return user_id;
	};
	uint32_t get_user_count() const noexcept override {
		return user_portfolio_manager->get_user_count();
	};
	std::vector<float> get_user_portfolio(UserID user_id) const override {
		if (user_id >= get_user_count()) {
			throw IDNotFoundError(fmt::format("The user_id: `{}` doesn't exist.", user_id));
		}
		return user_portfolio_manager->get_portfolio_table()[user_id];
	}

	// Simulation order book information
	const LimitOrder& get_top_bid(SecurityID security_id) const override {
		if (order_books.size() <= security_id) {
			throw IDNotFoundError(fmt::format("Could not find order book with security_id: `{}`.", security_id));
		}
		return order_books.at(security_id).top_bid();
	};
	const LimitOrder& get_top_ask(SecurityID security_id) const override {
		if (order_books.size() <= security_id) {
			throw IDNotFoundError(fmt::format("Could not find order book with security_id: `{}`.", security_id));
		}
		return order_books.at(security_id).top_ask();
	};
	uint32_t get_bid_count(SecurityID security_id) const override {
		if (order_books.size() <= security_id) {
			throw IDNotFoundError(fmt::format("Could not find order book with security_id: `{}`.", security_id));
		}
		return order_books.at(security_id).bid_size();
	};
	uint32_t get_ask_count(SecurityID security_id) const override {
		if (order_books.size() <= security_id) {
			throw IDNotFoundError(fmt::format("Could not find order book with security_id: `{}`.", security_id));
		}
		return order_books.at(security_id).ask_size();
	};
	FlatOrderBook get_order_book(SecurityID security_id) const override {
		if (tickers.size() <= security_id) {
			throw IDNotFoundError(fmt::format("The security_id: `{}` doesn't exist.", security_id));
		}
		return order_books.at(security_id).get_limit_orders();
	};
	std::set<OrderID> get_all_open_user_orders(UserID user_id, SecurityID security_id) const override {
		if (user_portfolio_manager->get_user_count() <= user_id) {
			throw IDNotFoundError(fmt::format("The user_id: `{}` doesn't exist.", user_id));
		}
		if (tickers.size() <= security_id) {
			throw IDNotFoundError(fmt::format("The security_id: `{}` doesn't exist.", security_id));
		}
		return order_books.at(security_id).get_all_user_orders(user_id);
	};
	BookDepth get_cumulative_book_depth(SecurityID security_id) const override {
		if (tickers.size() <= security_id) {
			throw IDNotFoundError(fmt::format("The security_id: `{}` doesn't exist.", security_id));
		}
		return order_books.at(security_id).get_book_depth();
	};
		
	// Simulation actions
	SimulationStepResult do_simulation_step() override {
		auto submitted_orders_lock = std::unique_lock(order_queue_mutex);
		// Perform a simulaiton step
		auto step = get_tick(); // step ∈ [0, ..., N] inclusive
		if (step > get_N()) {
			throw std::runtime_error("Passed simulation endpoint!");
		}

		auto t = get_t(); // t ∈ [0, ..., T]
		auto dt = get_dt(); // dt = T / N

		if (step == 0) {
			for (auto& security : get_securities()) {
				security->on_simulation_start(*this, user_portfolio_manager);
			}
		}

		for (auto& security : get_securities()) {
			security->before_step(*this, user_portfolio_manager);
		}

		// Keep track of market updates
		std::map<SecurityTicker, std::map<OrderID, float>> partially_transacted_orders = {}; // ticker -> order_id -> new volume
		std::map<SecurityTicker, std::set<OrderID>> fully_transacted_orders = {};
		std::map<SecurityTicker, std::set<OrderID>> cancelled_orders = {};
		std::map<SecurityTicker, std::vector<Transaction>> transactions = {};

		auto& securities = get_securities();
		// Here would the command queues normally be
		for (SecurityID security_id = 0; security_id < get_securities_count(); security_id++) {
			auto& security_class = securities.at(security_id);
			auto& order_book = order_books.at(security_id);
			auto& commands = submitted_orders.at(security_id);

			// Keep track of market updates for a particular security
			auto local_partially_transacted_orders = std::map<OrderID, float>();
			auto local_fully_transacted_orders = std::set<OrderID>();
			auto local_cancelled_orders = std::set<OrderID>();
			auto local_transactions = std::vector<Transaction>();

			for (auto& variant_command : commands) {
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
								security_class->on_trade_executed(*this, user_portfolio_manager, buyer_id, seller_id, transacted_price, transacted_volume);
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
			}

			// Save the differences to a simulation step object
			const auto& ticker = get_security_ticker(security_id);
			partially_transacted_orders.emplace(ticker, local_partially_transacted_orders);
			fully_transacted_orders.emplace(ticker, local_fully_transacted_orders);
			cancelled_orders.emplace(ticker, local_cancelled_orders);
			transactions.emplace(ticker, local_transactions);

			// Reset the submitted_order[security_id] vector
			commands.clear();
		}

		for (auto& security : get_securities()) {
			security->after_step(*this, user_portfolio_manager);
		}

		if (step == N) {
			for (auto& security : get_securities()) {
				security->on_simulation_end(*this, user_portfolio_manager);
			}
		}

		auto order_book_depth_per_security = std::map<SecurityTicker, BookDepth>();
		auto order_book_per_security = std::map<SecurityTicker, FlatOrderBook>();
		for (SecurityID security_id = 0; security_id < get_securities_count(); security_id++) {
			auto& ticker = get_security_ticker(security_id);
			order_book_depth_per_security[ticker] = get_cumulative_book_depth(security_id);
			order_book_per_security[ticker] = get_order_book(security_id);
		}

		increment_tick();
		return SimulationStepResult{
			.partially_transacted_orders = partially_transacted_orders,
			.fully_transacted_orders = fully_transacted_orders,
			.cancelled_orders = cancelled_orders,
			.transactions = transactions,
			.order_book_depth_per_security = order_book_depth_per_security,
			.order_book_per_security = order_book_per_security,
			.portfolios = user_portfolio_manager->get_portfolio_table(),
			.user_id_to_username_map = get_user_id_to_username(),
			.current_step = get_tick() - 1,
			.has_next_step = get_tick() <= get_N()
		};
	};
	OrderID submit_limit_order(UserID user_id, SecurityID security_id, OrderSide side, float price, float volume) override {
		if (user_id >= get_user_count()) {
			throw IDNotFoundError(fmt::format("The user_id: `{}` doesn't exist.", user_id));
		}
		if (security_id >= get_securities_count()) {
			throw IDNotFoundError(fmt::format("The security_id: `{}` doesn't exist.", security_id));
		}
		auto order_queue_lock = std::unique_lock(order_queue_mutex);
		auto order_id = order_id_counter++;
		submitted_orders.at(security_id).push_back(LimitOrder { .user_id = user_id, .order_id = order_id_counter, .side = side, .price = price, .volume = volume });
		return order_id;
	};
	void submit_cancel_order(UserID user_id, SecurityID security_id, OrderID order_id) override {
		if (user_id >= get_user_count()) {
			throw IDNotFoundError(fmt::format("The user_id: `{}` doesn't exist.", user_id));
		}
		if (security_id >= get_securities_count()) {
			throw IDNotFoundError(fmt::format("The security_id: `{}` doesn't exist.", security_id));
		}
		auto order_queue_lock = std::unique_lock(order_queue_mutex);
		submitted_orders.at(security_id).push_back(CancelOrder{ .user_id = user_id, .order_id = order_id });
	};
	void reset_simulation() override {
		auto order_queue_lock = std::unique_lock(order_queue_mutex);
		for (auto& [security_id, vec] : submitted_orders) {
			vec.clear();
		}
		for (UserID user_id = 0; user_id < user_portfolio_manager->get_user_count(); user_id++) {
			user_portfolio_manager->reset_user_portfolio(user_id);
		}
		reset_tick_to_zero();
	};
	OrderID direct_insert_limit_order(UserID user_id, SecurityID security_id, OrderSide side, float price, float volume) override {
		if (user_id >= get_user_count()) {
			throw IDNotFoundError(fmt::format("The user_id: `{}` doesn't exist.", user_id));
		}
		if (security_id >= get_securities_count()) {
			throw IDNotFoundError(fmt::format("The security_id: `{}` doesn't exist.", security_id));
		}
		auto order_queue_lock = std::unique_lock(order_queue_mutex);
		auto& order_book = order_books.at(security_id);
		auto order_id = order_id_counter++;
		order_book.insert_order(LimitOrder{ .user_id = user_id, .order_id = order_id, .side = side, .price = price, .volume = volume });
		return order_id;
	}
};

namespace GenericSecurities {
	class GenericCurrency : public ISecurity {
		SecurityTicker ticker;
	public:
		explicit GenericCurrency(const SecurityTicker& ticker) : ticker{ ticker } {}

		// Inherited via ISecurity
		bool is_tradeable() override
		{
			return false;
		}
		void before_step(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void after_step(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void on_simulation_start(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void on_simulation_end(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void on_trade_executed(
			ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio,
			UserID buyer, UserID seller, float price, float quantity
		) override {
		}
	};

	class GenericBond : public ISecurity {
		SecurityTicker ticker;
		SecurityTicker currency;
		float rate;
		float face_value;
	public:
		explicit GenericBond(const SecurityTicker& ticker, const SecurityTicker& currency, float rate, float face_value) : ticker{ ticker }, currency{ currency }, rate{ rate }, face_value { face_value } {}

		// Inherited via ISecurity
		bool is_tradeable() override
		{
			return true;
		}
		void before_step(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void after_step(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {
			// Bonds make interest payment
			auto dt = simulation.get_dt();
			auto bond_id = simulation.get_security_id(ticker);
			auto cad_id = simulation.get_security_id(currency);
			for (UserID index_user = 0; index_user < portfolio->get_user_count(); index_user++) {
				// The bond pays `rate * dt` per step, having more bonds increases nomial amount added to cad
				portfolio->multiply_and_add_1_to_2(
					index_user, bond_id, cad_id, rate * face_value * dt
				);
			}
		}
		void on_simulation_start(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void on_simulation_end(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {
			auto bond_id = simulation.get_security_id(ticker);
			auto cad_id = simulation.get_security_id(currency);
			for (UserID index_user = 0; index_user < portfolio->get_user_count(); index_user++) {
				// Reduce the amount of bond to 0, and realize it as CAD (each bond is worth 100).
				portfolio->multiply_and_add_1_to_2_and_set_1(
					index_user, bond_id, cad_id, face_value, 0.0f
				);
			}
		}
		void on_trade_executed(
			ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio,
			UserID buyer, UserID seller, float price, float quantity
		) override {
			auto bond_id = simulation.get_security_id(ticker);
			auto cad_id = simulation.get_security_id(currency);
			// The bond buyer gets the bond, but losses money
			portfolio->add_to_two_securities(buyer, bond_id, quantity, cad_id, -price * quantity);
			// The bond seller losses the bond, but gets money
			portfolio->add_to_two_securities(seller, bond_id, -quantity, cad_id, price * quantity);
		}
	};

	class GenericStock : public ISecurity {
		SecurityTicker ticker;
		SecurityTicker currency;
	public:
		explicit GenericStock(const SecurityTicker& ticker, const SecurityTicker& currency) : ticker{ ticker }, currency{ currency } {}

		// Inherited via ISecurity
		bool is_tradeable() override
		{
			return true;
		}
		void before_step(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void after_step(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void on_simulation_start(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {}
		void on_simulation_end(ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio) override {
			// At the end convert to CAD at midpoint price, or `100.0f`
			auto stock_id = simulation.get_security_id(ticker);
			auto cad_id = simulation.get_security_id(currency);


			auto close_bid_price = 100.0f;
			auto close_ask_price = 100.0f;
			if (simulation.get_bid_count(stock_id) > 0) {
				close_bid_price = simulation.get_top_bid(stock_id).price;
			}
			if (simulation.get_ask_count(stock_id) > 0) {
				close_ask_price = simulation.get_top_ask(stock_id).price;
			}

			for (UserID index_user = 0; index_user < portfolio->get_user_count(); index_user++) {
				portfolio->multiply_and_add_1_to_2_and_set_1(
					index_user, stock_id, cad_id, (close_bid_price + close_ask_price) / 2.0f, 0.0f
				);
			}
		}
		void on_trade_executed(
			ISimulation& simulation, std::shared_ptr<IPortfolioManager> portfolio,
			UserID buyer, UserID seller, float price, float quantity
		) override {
			auto stock_id = simulation.get_security_id(ticker);
			auto cad_id = simulation.get_security_id(currency);
			// The buyer gets the stock, but losses money
			portfolio->add_to_two_securities(buyer, stock_id, quantity, cad_id, -price * quantity);
			// The seller losses the stock, but gets money
			portfolio->add_to_two_securities(seller, stock_id, -quantity, cad_id, price * quantity);
		}
	};
};

class PyISecurity : public ISecurity {
public:
	using ISecurity::ISecurity;

	bool is_tradeable() override {
		PYBIND11_OVERRIDE_PURE(bool, ISecurity, is_tradeable);
	}
	void before_step(ISimulation& sim, std::shared_ptr<IPortfolioManager> pm) override {
		PYBIND11_OVERRIDE_PURE(void, ISecurity, before_step, sim, pm);
	}
	void after_step(ISimulation& sim, std::shared_ptr<IPortfolioManager> pm) override {
		PYBIND11_OVERRIDE_PURE(void, ISecurity, after_step, sim, pm);
	}
	void on_simulation_start(ISimulation& sim, std::shared_ptr<IPortfolioManager> pm) override {
		PYBIND11_OVERRIDE_PURE(void, ISecurity, on_simulation_start, sim, pm);
	}
	void on_simulation_end(ISimulation& sim, std::shared_ptr<IPortfolioManager> pm) override {
		PYBIND11_OVERRIDE_PURE(void, ISecurity, on_simulation_end, sim, pm);
	}
	void on_trade_executed(ISimulation& sim, std::shared_ptr<IPortfolioManager> pm, UserID b, UserID s, float p, float v) override {
		PYBIND11_OVERRIDE_PURE(void, ISecurity, on_trade_executed, sim, pm, b, s, p, v);
	}
};

class PyIPortfolioManager : public IPortfolioManager {
public:
	using IPortfolioManager::IPortfolioManager;

	// Inherited via IPortfolioManager
	uint32_t get_user_count() const noexcept override {
		PYBIND11_OVERRIDE_PURE(uint32_t, IPortfolioManager, get_user_count);
	}
	std::vector<std::vector<float>> get_portfolio_table() const noexcept override {
		PYBIND11_OVERRIDE_PURE(std::vector<std::vector<float>>, IPortfolioManager, get_portfolio_table);
	}
	void reset_user_portfolio(UserID uid) override {
		PYBIND11_OVERRIDE_PURE(void, IPortfolioManager, reset_user_portfolio, uid);
	}
	float add_to_security(UserID uid, SecurityID sid1, float add1) override {
		PYBIND11_OVERRIDE_PURE(float, IPortfolioManager, add_to_security, uid, sid1, add1);
	}
	FloatPair add_to_two_securities(UserID uid, SecurityID s1, float a1, SecurityID s2, float a2) override {
		PYBIND11_OVERRIDE_PURE(FloatPair, IPortfolioManager, add_to_two_securities, uid, s1, a1, s2, a2);
	}
	float multiply_and_add_1_to_2(UserID uid, SecurityID s1, SecurityID s2, float mult) override {
		PYBIND11_OVERRIDE_PURE(float, IPortfolioManager, multiply_and_add_1_to_2, uid, s1, s2, mult);
	}
	float multiply_and_add_1_to_2_and_set_1(UserID uid, SecurityID s1, SecurityID s2, float m, float v) override {
		PYBIND11_OVERRIDE_PURE(float, IPortfolioManager, multiply_and_add_1_to_2_and_set_1, uid, s1, s2, m, v);
	}
};

class PyISimulation : public ISimulation {
public:
	using ISimulation::ISimulation;

	UserID add_user(const Username& name) override {
		PYBIND11_OVERRIDE_PURE(UserID, ISimulation, add_user, name);
	}
	uint32_t get_user_count() const noexcept override {
		PYBIND11_OVERRIDE_PURE(uint32_t, ISimulation, get_user_count);
	}
	std::vector<float> get_user_portfolio(UserID user_id) const override {
		PYBIND11_OVERRIDE_PURE(std::vector<float>, ISimulation, get_user_portfolio, user_id);
	}
	const LimitOrder& get_top_bid(SecurityID sid) const override {
		PYBIND11_OVERRIDE_PURE(const LimitOrder&, ISimulation, get_top_bid, sid);
	}
	const LimitOrder& get_top_ask(SecurityID sid) const override {
		PYBIND11_OVERRIDE_PURE(const LimitOrder&, ISimulation, get_top_ask, sid);
	}
	uint32_t get_bid_count(SecurityID sid) const override {
		PYBIND11_OVERRIDE_PURE(uint32_t, ISimulation, get_bid_count, sid);
	}
	uint32_t get_ask_count(SecurityID sid) const override {
		PYBIND11_OVERRIDE_PURE(uint32_t, ISimulation, get_ask_count, sid);
	}
	FlatOrderBook get_order_book(SecurityID sid) const override {
		PYBIND11_OVERRIDE_PURE(FlatOrderBook, ISimulation, get_order_book, sid);
	}
	std::set<OrderID> get_all_open_user_orders(UserID uid, SecurityID sid) const override {
		PYBIND11_OVERRIDE_PURE(std::set<OrderID>, ISimulation, get_all_open_user_orders, uid, sid);
	}
	BookDepth get_cumulative_book_depth(SecurityID sid) const override {
		PYBIND11_OVERRIDE_PURE(BookDepth, ISimulation, get_cumulative_book_depth, sid);
	}
	SimulationStepResult do_simulation_step() override {
		PYBIND11_OVERRIDE_PURE(SimulationStepResult, ISimulation, do_simulation_step);
	}
	OrderID submit_limit_order(UserID uid, SecurityID sid, OrderSide s, float p, float v) override {
		PYBIND11_OVERRIDE_PURE(OrderID, ISimulation, submit_limit_order, uid, sid, s, p, v);
	}
	void submit_cancel_order(UserID uid, SecurityID sid, OrderID oid) override {
		PYBIND11_OVERRIDE_PURE(void, ISimulation, submit_cancel_order, uid, sid, oid);
	}
	void reset_simulation() override {
		PYBIND11_OVERRIDE_PURE(void, ISimulation, reset_simulation);
	}
	OrderID direct_insert_limit_order(UserID uid, SecurityID sid, OrderSide s, float p, float v) override {
		PYBIND11_OVERRIDE_PURE(OrderID, ISimulation, direct_insert_limit_order, uid, sid, s, p, v);
	}
};

PYBIND11_MODULE(Server, m) {
	py::enum_<OrderSide>(m, "OrderSide")
		.value("BID", OrderSide::BID)
		.value("ASK", OrderSide::ASK)
		.export_values();

	py::class_<LimitOrder>(m, "LimitOrder")
		.def(py::init<>())
		.def_readwrite("user_id", &LimitOrder::user_id)
		.def_readwrite("order_id", &LimitOrder::order_id)
		.def_readwrite("side", &LimitOrder::side)
		.def_readwrite("price", &LimitOrder::price)
		.def_readwrite("volume", &LimitOrder::volume);

	py::class_<CancelOrder>(m, "CancelOrder")
		.def(py::init<>())
		.def_readwrite("user_id", &CancelOrder::user_id)
		.def_readwrite("order_id", &CancelOrder::order_id);

	py::class_<Transaction>(m, "Transaction")
		.def_readwrite("price", &Transaction::price)
		.def_readwrite("volume", &Transaction::volume)
		.def_readwrite("buyer_id", &Transaction::buyer_id)
		.def_readwrite("seller_id", &Transaction::seller_id);

	py::bind_vector<std::vector<LimitOrder>>(m, "LimitOrderList");
	py::bind_map<std::map<float, float>>(m, "PriceDepthMap");

	py::class_<SimulationStepResult>(m, "SimulationStepResult")
		.def_readwrite("partially_transacted_orders", &SimulationStepResult::partially_transacted_orders)
		.def_readwrite("fully_transacted_orders", &SimulationStepResult::fully_transacted_orders)
		.def_readwrite("cancelled_orders", &SimulationStepResult::cancelled_orders)
		.def_readwrite("transactions", &SimulationStepResult::transactions)
		.def_readwrite("order_book_depth_per_security", &SimulationStepResult::order_book_depth_per_security)
		.def_readwrite("order_book_per_security", &SimulationStepResult::order_book_per_security)
		.def_readwrite("portfolios", &SimulationStepResult::portfolios)
		.def_readwrite("user_id_to_username_map", &SimulationStepResult::user_id_to_username_map)
		.def_readwrite("current_step", &SimulationStepResult::current_step)
		.def_readwrite("has_next_step", &SimulationStepResult::has_next_step);

	py::class_<ISecurity, PyISecurity, std::shared_ptr<ISecurity>>(m, "ISecurity")
		.def(py::init<>())
		.def("is_tradeable", &ISecurity::is_tradeable)
		.def("before_step", &ISecurity::before_step, py::arg("simulation"), py::arg("portfolio"))
		.def("after_step", &ISecurity::after_step, py::arg("simulation"), py::arg("portfolio"))
		.def("on_simulation_start", &ISecurity::on_simulation_start, py::arg("simulation"), py::arg("portfolio"))
		.def("on_simulation_end", &ISecurity::on_simulation_end, py::arg("simulation"), py::arg("portfolio"))
		.def("on_trade_executed", &ISecurity::on_trade_executed,
			py::arg("simulation"), py::arg("portfolio"), py::arg("buyer_id"),
			py::arg("seller_id"), py::arg("transacted_price"), py::arg("transacted_volume"));

	py::class_<IPortfolioManager, PyIPortfolioManager, std::shared_ptr<IPortfolioManager>>(m, "IPortfolioManager")
		.def(py::init<>())
		.def("get_portfolio_table", &IPortfolioManager::get_portfolio_table)
		.def("reset_user_portfolio", &IPortfolioManager::reset_user_portfolio, py::arg("user_id"))
		.def("add_to_security", &IPortfolioManager::add_to_security, py::arg("user_id"), py::arg("security_id"), py::arg("addition"))
		.def("add_to_two_securities", &IPortfolioManager::add_to_two_securities, py::arg("user_id"), py::arg("security_1"), py::arg("addition_1"), py::arg("security_2"), py::arg("addition_2"))
		.def("multiply_and_add_1_to_2", &IPortfolioManager::multiply_and_add_1_to_2, py::arg("user_id"), py::arg("security_1"), py::arg("security_2"), py::arg("multiply"))
		.def("multiply_and_add_1_to_2_and_set_1", &IPortfolioManager::multiply_and_add_1_to_2_and_set_1,
			py::arg("user_id"), py::arg("security_1"), py::arg("security_2"), py::arg("multiply"), py::arg("set_value"));

	py::class_<ISimulation, PyISimulation, std::shared_ptr<ISimulation>>(m, "ISimulation")
		.def(py::init<const std::map<SecurityTicker, std::shared_ptr<ISecurity>>&, float, uint32_t>(),
			py::arg("securities"), py::arg("T"), py::arg("N"))
		.def("get_all_tickers", &ISimulation::get_all_tickers)
		.def("get_security_ticker", &ISimulation::get_security_ticker, py::arg("security_id"))
		.def("get_security_id", &ISimulation::get_security_id, py::arg("security_ticker"))
		.def("get_user_id_to_username", &ISimulation::get_user_id_to_username)
		.def("get_securities_count", &ISimulation::get_securities_count)
		.def("get_dt", &ISimulation::get_dt)
		.def("get_t", &ISimulation::get_t)
		.def("get_T", &ISimulation::get_T)
		.def("get_tick", &ISimulation::get_tick)
		.def("get_N", &ISimulation::get_N)
		.def("add_user", &ISimulation::add_user, py::arg("username"))
		.def("get_user_count", &ISimulation::get_user_count)
		.def("get_user_portfolio", &ISimulation::get_user_portfolio)
		.def("get_top_bid", &ISimulation::get_top_bid, py::arg("security_id"))
		.def("get_top_ask", &ISimulation::get_top_ask, py::arg("security_id"))
		.def("get_bid_count", &ISimulation::get_bid_count, py::arg("security_id"))
		.def("get_ask_count", &ISimulation::get_ask_count, py::arg("security_id"))
		.def("get_order_book", &ISimulation::get_order_book, py::arg("security_id"))
		.def("get_all_open_user_orders", &ISimulation::get_all_open_user_orders, py::arg("user_id"), py::arg("security_id"))
		.def("get_cumulative_book_depth", &ISimulation::get_cumulative_book_depth, py::arg("security_id"))
		.def("do_simulation_step", &ISimulation::do_simulation_step)
		.def("submit_limit_order", &ISimulation::submit_limit_order,
			py::arg("user_id"), py::arg("security_id"), py::arg("side"), py::arg("price"), py::arg("volume"))
		.def("submit_cancel_order", &ISimulation::submit_cancel_order,
			py::arg("user_id"), py::arg("security_id"), py::arg("order_id"))
		.def("reset_simulation", &ISimulation::reset_simulation)
		.def("direct_insert_limit_order", &ISimulation::direct_insert_limit_order, py::arg("user_id"), py::arg("security_id"), py::arg("side"), py::arg("price"), py::arg("volume"));

	py::class_<GenericSimulation, ISimulation, std::shared_ptr<GenericSimulation>>(m, "GenericSimulation")
		.def(py::init<const std::map<SecurityTicker, std::shared_ptr<ISecurity>>&, float, uint32_t>());

	py::module_ generic = m.def_submodule("GenericSecurities", "Generic security types");

	py::class_<GenericSecurities::GenericCurrency, ISecurity, std::shared_ptr<GenericSecurities::GenericCurrency>>(generic, "GenericCurrency")
		.def(py::init<const SecurityTicker&>(),
			py::arg("ticker"));

	py::class_<GenericSecurities::GenericBond, ISecurity, std::shared_ptr<GenericSecurities::GenericBond>>(generic, "GenericBond")
		.def(py::init<const SecurityTicker&, const SecurityTicker&, float, float>(),
			py::arg("ticker"),
			py::arg("currency"),
			py::arg("rate"),
			py::arg("face_value"));

	py::class_<GenericSecurities::GenericStock, ISecurity, std::shared_ptr<GenericSecurities::GenericStock>>(generic, "GenericStock")
		.def(py::init<const SecurityTicker&, const SecurityTicker&>(),
			py::arg("ticker"),
			py::arg("currency"));
}
