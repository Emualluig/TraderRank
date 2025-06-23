#pragma once

#include <vector>
#include <map>
#include <set>
#include <variant>
#include <optional>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <cassert>
#include <exception>
#include <memory>
#include <queue>
#include <fmt/core.h>

namespace TraderRank {
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
	template<typename T> constexpr bool always_false = false;

	template<typename T> struct Shared {
		std::shared_mutex mutex;
		T inner;
	};

	template<typename T> struct Unique {
		std::mutex mutex;
		T inner;
	};

	using UserID = uint32_t;
	using Username = std::string;
	using AssetID = uint32_t;
	using AssetTicker = std::string;
	using OrderID = uint32_t;
	using TransactionID = uint32_t;
	using Step = uint32_t;
	enum class OrderSide {
		BUY,
		SELL
	};

	struct Transaction {
		// TODO: Add reference to security, add transaction id
		Step step;
		UserID buyer;
		UserID seller;
		float transacted_price;
		float transacted_volume;
	};
	struct TransactionStub {
		// TODO: Add reference to original transaction
		OrderSide side;
		float transacted_price;
		float transacted_volume;
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
		OrderID order_id_to_cancel;
	};
	
	struct MarketOrder {
		UserID user_id;
		OrderID order_id;
		OrderSide action;
		float volume;
	};

	using VariantOrder = std::variant<LimitOrder, CancelOrder, MarketOrder>;

	template<typename T> 
	struct BidAskStruct {
		T bid;
		T ask;
	};

	// Methods on this class are not atomic
	class OrderBook {
	public:
		using BookDepth = BidAskStruct<std::map<float, float>>; // price -> cumulative quantity
		using FlatOrderBook = BidAskStruct<std::vector<LimitOrder>>;
	private:
		using BidSet = std::set<LimitOrder, LimitOrder::BidComparator>;
		using AskSet = std::set<LimitOrder, LimitOrder::AskComparator>;

		BidSet bid_orders;
		AskSet ask_orders;
		std::map<OrderID, BidSet::iterator> bid_map;
		std::map<OrderID, AskSet::iterator> ask_map;
	public:
		std::size_t bid_count() const noexcept {
			return bid_orders.size();
		}

		std::size_t ask_count() const noexcept {
			return ask_orders.size();
		}

		bool has_order(OrderID order_id) const noexcept {
			return bid_map.find(order_id) != bid_map.end() || ask_map.find(order_id) != ask_map.end();
		}

		bool insert_order(const LimitOrder& order) noexcept {
			if (order.side == OrderSide::BUY) {
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

		bool cancel_order(const CancelOrder& cancel) noexcept {
			if (auto it = bid_map.find(cancel.order_id_to_cancel); it != bid_map.end()) {
				bid_orders.erase(it->second);
				bid_map.erase(it);
				return true;
			}
			else if (auto it = ask_map.find(cancel.order_id_to_cancel); it != ask_map.end()) {
				ask_orders.erase(it->second);
				ask_map.erase(it);
				return true;
			}
			return false;
		}

		LimitOrder& top_bid() const {
			if (bid_count() == 0) {
				throw std::runtime_error("Bid book is empty.");
			}
			return const_cast<LimitOrder&>(*bid_orders.begin());
		}

		LimitOrder& top_ask() const {
			if (ask_count() == 0) {
				throw std::runtime_error("Ask book is empty.");
			}
			return const_cast<LimitOrder&>(*ask_orders.begin());
		}

		void pop_top_bid() {
			if (bid_count() == 0) {
				throw std::runtime_error("Bid book is empty.");
			}
			auto it = bid_orders.begin();
			bid_map.erase(it->order_id);
			bid_orders.erase(it);
		}

		void pop_top_ask() {
			if (ask_count() == 0) {
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

			return BookDepth{ .bid = bid_depth, .ask = ask_depth };
		}

		FlatOrderBook get_limit_orders() const {
			std::vector<LimitOrder> bids(bid_orders.begin(), bid_orders.end());
			std::vector<LimitOrder> asks(ask_orders.begin(), ask_orders.end());
			return FlatOrderBook{ .bid = std::move(bids), .ask = std::move(asks) };
		}

		std::set<OrderID> get_all_user_orders(UserID user_id) const noexcept {
			std::set<OrderID> result = {};
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

	class IAsset {
		AssetTicker ticker;
	public:
		explicit IAsset(const AssetTicker& ticker) : ticker{ ticker } {}
		const AssetTicker& get_ticker() const noexcept {
			return ticker;
		}
		virtual bool is_tradeable() const noexcept = 0;
		virtual void on_step() = 0;
		virtual const AssetTicker& get_demoninated_asset() const noexcept = 0;
	};


	struct PortfolioHoldings {
		float holdings;
	};

	struct PortfolioInformation {
		float cost; // The cost (per unit) to build this position
		float vwap; // 
	};

	class Simulation : public std::enable_shared_from_this<Simulation> {
		const bool python_mode;
		struct Private { explicit Private() = default; };
		explicit Simulation(bool python_mode, Private) : python_mode{ python_mode } {}
	public:
		static std::shared_ptr<Simulation> create_python() {
			return std::make_shared<Simulation>(true, Private());
		}
		static std::shared_ptr<Simulation> create_cpp() {
			return std::make_shared<Simulation>(false, Private());
		}
		std::shared_ptr<Simulation> getptr() {
			return shared_from_this();
		}
	private:
		const Step max_step_count = 0;
		std::atomic<Step> step_counter = std::atomic<Step>(Step(0)); // step ∈ [0, ..., max_step_count)
		std::atomic<UserID> user_id_counter = std::atomic<UserID>(UserID(0));
		std::atomic<AssetID> asset_id_counter = std::atomic<AssetID>(AssetID(0));
		std::atomic<OrderID> order_id_counter = std::atomic<OrderID>(OrderID(0));
		std::atomic<TransactionID> transaction_id_counter = std::atomic<TransactionID>(TransactionID(0));

		Shared<std::map<AssetID, std::shared_ptr<IAsset>>> asset_id_to_asset = {};
		Shared<std::map<AssetTicker, AssetID>> asset_ticker_to_asset_id = {};
		Shared<std::map<AssetID, AssetTicker>> asset_id_to_asset_ticker = {};

		Shared<std::map<AssetID, Unique<std::vector<Transaction>>>> asset_transactions = {};
		Shared<std::map<AssetID, Unique<std::vector<VariantOrder>*>>> asset_order_queues = {};
		Shared<std::map<AssetID, Unique<OrderBook>>> asset_order_books = {};

		Shared<std::map<UserID, Shared<std::map<AssetID, std::atomic<float>>>>> user_asset_holdings = {};
		Shared<std::map<UserID, Shared<std::map<AssetID, std::deque<TransactionStub>>>>> user_fifo_transaction_queues = {};
		struct CostAndVWAP {
			float cost;
			float vwap;
		};
		void add_transaction_to_fifo_queue(const UserID user_id, const AssetID asset_id, const OrderSide side, float transacted_price, float transacted_volume) {
			auto user_asset_transactions_queues_read_lock = std::shared_lock(user_fifo_transaction_queues.mutex);
			auto& user_asset_transaction_queues_pair = user_fifo_transaction_queues.inner.at(user_id);
			auto asset_transaction_queues_read_lock = std::shared_lock(user_asset_transaction_queues_pair.mutex);
			auto& transaction_queue = user_asset_transaction_queues_pair.inner.at(asset_id);
		
			while (true) {
				if (transaction_queue.empty()) {
					// The fifo queue is empty so we just add the stub
					transaction_queue.push_back(TransactionStub{ .side = side, .transacted_price = transacted_price, .transacted_volume = transacted_volume });
					break;
				}
				else {
					auto& front = transaction_queue.front();
					if (front.side == side) {
						// The fifo queue doesn't contain any of the opposite side, so we append
						transaction_queue.push_back(TransactionStub{ .side = side, .transacted_price = transacted_price, .transacted_volume = transacted_volume });
						break;
					}
					else {
						// The fifo queue contains stubs of the opposite side, we must resolve those
						auto resolved_volume = std::min(front.transacted_volume, transacted_volume);
						front.transacted_volume -= resolved_volume;
						transacted_volume -= resolved_volume;
						// At this point, either `front.transacted_volume == 0` or `transacted_volume == 0` or both
						if (front.transacted_volume == 0.0f) {
							// `front.transacted_volume == 0`, maybe `transacted_volume == 0`
							// Pop the empty
							transaction_queue.pop_front();
							if (transacted_volume == 0) {
								break;
							}
						}
						else {
							// `transacted_volume == 0` and `front.transacted_volume != 0`
							// Nothing left to do since we ran out of volume
							break;
						}
					}
				}
			}		
		}
		CostAndVWAP calculate_cost_and_vwap(const UserID user_id, const AssetID asset_id) const {
			auto user_asset_transactions_queues_read_lock = std::shared_lock(user_fifo_transaction_queues.mutex);
			auto& user_asset_transaction_queues_pair = user_fifo_transaction_queues.inner.at(user_id);
			auto asset_transaction_queues_read_lock = std::shared_lock(user_asset_transaction_queues_pair.mutex);
			auto& transaction_queue = user_asset_transaction_queues_pair.inner.at(asset_id);

			double cumulative_volume = 0.0;
			double cumulative_price_volume = 0.0;
			for (const auto& stub : transaction_queue) {
				cumulative_volume += stub.transacted_volume;
				cumulative_price_volume += stub.transacted_price * stub.transacted_volume;
			}
			return CostAndVWAP{
				.cost = (float)(cumulative_price_volume),
				.vwap = (float)(cumulative_price_volume / cumulative_volume)
			};
		}
	public:
		AssetID get_asset_id(const AssetTicker& asset_ticker) const {
			auto read_lock = std::shared_lock(asset_ticker_to_asset_id.mutex);
			if (const auto iter = asset_ticker_to_asset_id.inner.find(asset_ticker); iter != asset_ticker_to_asset_id.inner.end()) {
				return iter->second;
			}
			throw std::runtime_error("No such asset ticker exists.");
		}
		bool has_next_step() const noexcept {
			return step_counter < max_step_count;
		}
		Step get_step() const noexcept {
			return step_counter;
		}
		Step get_max_step() const noexcept {
			return step_counter;
		}
		bool does_user_id_exist(UserID user_id) const noexcept {
			return user_id < user_id_counter;
		}
		bool does_asset_id_exist(AssetID asset_id) const noexcept {
			return asset_id < asset_id_counter;
		}
		bool does_order_id_exist(OrderID order_id) const noexcept {
			return order_id < order_id_counter;
		}
		bool does_transaction_id_exist(TransactionID transaction_id) const noexcept {
			return transaction_id < transaction_id_counter;
		}
	private:
		std::mutex process_mutex = std::mutex();
	public:
		struct StepResult {
			Step current_step;
			bool has_next_step;
			std::map<UserID, Username> user_id_to_username;
			std::map<AssetID, AssetTicker> asset_id_to_ticker;
			std::map<AssetID, std::vector<OrderID>> submitted_orders_per_asset;
			std::map<AssetID, std::map<OrderID, float>> transacted_orders_per_asset;
			std::map<AssetID, std::vector<OrderID>> cancelled_orders_per_asset;
			std::map<AssetID, std::vector<Transaction>> transactions_per_asset;
			std::map<AssetID, FlatOrderBook> limit_orders_per_asset;
			std::map<AssetID, BookDepth> book_depth_per_asset;
		};
		StepResult process_step() {
			// Prevent 
			auto process_lock = std::unique_lock(process_mutex);

			if (!has_next_step()) {
				throw std::runtime_error("Passed simulation endpoint!");
			}

			// TODO: Allow custom action for the first step

			// Increment the step
			step_counter += 1;

			// TODO: Allow custom action before each step

			auto asset_transactions_read_lock = std::shared_lock(asset_transactions.mutex);
			auto asset_order_queues_read_lock = std::shared_lock(asset_order_queues.mutex);
			auto asset_order_books_read_lock  = std::shared_lock(asset_order_books.mutex);

			auto submitted_limit_orders_per_asset  = std::map<AssetID, std::vector<OrderID>>();
			auto transacted_limit_orders_per_asset = std::map<AssetID, std::map<OrderID, float>>();
			auto cancelled_limit_orders_per_asset  = std::map<AssetID, std::vector<OrderID>>();
			auto transactions_per_asset      = std::map<AssetID, std::vector<Transaction>>();
			for (auto& [asset_id, order_book_pair] : asset_order_books.inner) {
				// TODO: Check that order book is not crossed invariant

				auto asset_read_lock = std::shared_lock(asset_id_to_asset.mutex);
				auto& asset = asset_id_to_asset.inner.at(asset_id);

				if (!asset->is_tradeable()) {
					continue;
				}

				auto asset_ticker_read_lock = std::shared_lock(asset_ticker_to_asset_id.mutex);
				auto denominated_asset_id = asset_ticker_to_asset_id.inner.at(asset->get_demoninated_asset());

				// Get and replace the order queue to minimize locking
				std::vector<VariantOrder>* order_queue_pointer = nullptr;
				{
					auto& order_queue_pair = asset_order_queues.inner[asset_id];
					auto order_queue_write_lock = std::unique_lock(order_queue_pair.mutex);
					order_queue_pointer = order_queue_pair.inner;
					assert(order_queue_pointer != nullptr);
					order_queue_pair.inner = new std::vector<VariantOrder>(order_queue_pointer->size());
				}

				// Get write lock on transactions vector
				auto& transactions_pair = asset_transactions.inner[asset_id];
				auto transactions_write_lock = std::unique_lock(transactions_pair.mutex);
				auto& transactions = transactions_pair.inner;

				// One-by-one add the orders to the order book and process after each
				auto order_book_write_lock = std::unique_lock(order_book_pair.mutex);
				auto& order_book = order_book_pair.inner;

				// Keep track of submitted/transacted/cancelled/transactions
				auto submitted_orders  = std::vector<OrderID>();
				auto transacted_limit_orders = std::map<OrderID, float>(); // OrderID -> Transaced volume
				auto cancelled_orders  = std::vector<OrderID>();
				auto transactions      = std::vector<Transaction>();

				// Local functions
				auto perform_transaction_lambda = [&](
					UserID buyer_id,
					UserID seller_id,
					float transacted_price,
					float transacted_volume
				) -> void {
					auto user_read_lock = std::shared_lock(user_asset_holdings.mutex);
					auto& buyer_asset_pair = user_asset_holdings.inner.at(buyer_id);
					auto& seller_asset_pair = user_asset_holdings.inner.at(seller_id);

					auto buyer_asset_read_lock = std::shared_lock(buyer_asset_pair.mutex);
					auto seller_asset_read_lock = std::shared_lock(seller_asset_pair.mutex);

					auto& buyer_holdings = buyer_asset_pair.inner;
					auto& seller_holdings = seller_asset_pair.inner;

					buyer_holdings[asset_id] += transacted_volume;
					buyer_holdings[denominated_asset_id] -= transacted_price * transacted_volume;

					seller_holdings[asset_id] -= transacted_volume;
					seller_holdings[denominated_asset_id] += transacted_price * transacted_volume;

					add_transaction_to_fifo_queue(buyer_id, asset_id, OrderSide::BUY, transacted_price, transacted_volume);
					add_transaction_to_fifo_queue(seller_id, asset_id, OrderSide::SELL, transacted_price, transacted_volume);

					transactions.push_back(Transaction{
						.step = step_counter,
						.buyer = buyer_id,
						.seller = seller_id,
						.transacted_price = transacted_price,
						.transacted_volume = transacted_volume
					});
				};

				for (auto& order : *order_queue_pointer) {
					std::visit(overloaded{
						[&](LimitOrder& limit_order) {
							// TODO: Check for invariant here aswell
							order_book.insert_order(limit_order);
							submitted_orders.push_back(limit_order.order_id);
							
							// The order book is now potentially crossed, we must resolve it
							// additionally, if it was crossed, it is due to the added order, by our invariants
							while (order_book.bid_count() > 0 && order_book.ask_count() > 0) {
								auto& top_bid = order_book.top_bid();
								auto& top_ask = order_book.top_ask();
								if (top_bid.price >= top_ask.price) {
									// The trades will execute on the submitted order's opposite side.
									// If the submitted order is a bid, the market is crossed because of it, 
									// the execution price will be of the ask. And vice-versa if the crossing is ask.
									auto transacted_price = limit_order.side == OrderSide::BUY ? top_ask.price : top_bid.price;
									auto transacted_volume = std::min(top_bid.volume, top_ask.volume);
									auto buyer_id = top_bid.user_id;
									auto seller_id = top_ask.user_id;
									auto bid_id = top_bid.order_id;
									auto ask_id = top_ask.order_id;

									auto remaining_bid_volume = top_bid.volume - transacted_volume;
									if (remaining_bid_volume == 0.0f) {
										order_book.pop_top_bid();
									}
									else {
										top_bid.volume = remaining_bid_volume;
									}

									auto remaining_ask_volume = top_ask.volume - transacted_volume;
									if (remaining_ask_volume == 0.0f) {
										order_book.pop_top_ask();
									}
									else {
										top_ask.volume = remaining_ask_volume;
									}

									transacted_limit_orders[bid_id] += transacted_volume;
									transacted_limit_orders[ask_id] += transacted_volume;

									// Perform the transaction
									perform_transaction_lambda(
										buyer_id, 
										seller_id, 
										transacted_price, 
										transacted_volume
									);
								}
								else {
									break;
								}
							}
						
							// TODO: Check for invariant here aswell
						},
						[&](CancelOrder& cancel_order) {
							auto was_cancelled = order_book.cancel_order(cancel_order);
							if (was_cancelled) {
								cancelled_orders.push_back(cancel_order.order_id_to_cancel);
							}
						},
						[&](MarketOrder& market_order) {
							// TODO: Check that order book is not crossed invariant

							auto action = market_order.action;
							auto order_user_id = market_order.user_id;
							assert(action == OrderSide::BUY || action == OrderSide::SELL);
							while (market_order.volume > 0) {
								if (action == OrderSide::BUY && order_book.ask_count() > 0) {
									// The market order is buying
									auto& top_ask = order_book.top_ask();
									auto top_ask_order_id = top_ask.order_id;
									auto top_ask_user_id = top_ask.user_id;
									auto transacted_price = top_ask.price;
									auto transacted_volume = std::min(market_order.volume, top_ask.volume);

									auto remaining_order_volume = market_order.volume - transacted_volume;
									auto remaining_ask_volume = top_ask.volume - transacted_price;
									assert(remaining_order_volume == 0 || remaining_ask_volume == 0);

									market_order.volume = remaining_order_volume;
									if (remaining_ask_volume == 0.0f) {
										order_book.pop_top_ask();
									}
									else {
										top_ask.volume = remaining_ask_volume;
									}

									transacted_limit_orders[top_ask_order_id] += transacted_volume;

									perform_transaction_lambda(
										market_order.user_id,
										top_ask_user_id,
										transacted_price,
										transacted_volume
									);
								}
								else if (action == OrderSide::SELL && order_book.bid_count() > 0) {
									// The market order is selling
									auto& top_bid = order_book.top_bid();
									auto top_bid_order_id = top_bid.order_id;
									auto top_bid_user_id = top_bid.user_id;
									auto transacted_price = top_bid.price;
									auto transacted_volume = std::min(market_order.volume, top_bid.volume);

									auto remaining_order_volume = market_order.volume - transacted_volume;
									auto remaining_bid_volume = top_bid.volume - transacted_price;
									assert(remaining_order_volume == 0 || remaining_bid_volume == 0);

									market_order.volume = remaining_order_volume;
									if (remaining_bid_volume == 0.0f) {
										order_book.pop_top_bid();
									}
									else {
										top_bid.volume = remaining_bid_volume;
									}

									transacted_limit_orders[top_bid_order_id] += transacted_volume;

									perform_transaction_lambda(
										top_bid_order_id,
										market_order.user_id,
										transacted_price,
										transacted_volume
									);
								}
								else {
									// The market order has exhausted the opposite side
									break;
								}
							}
							// The market order has either consumed the either other side or is out of volume

							// TODO: Check that order book is not crossed invariant
						},
						[&](auto&& arg) -> void {
							static_assert(always_false<decltype(arg)>, "Unhandled variant alternative");
						}
					}, order);
				}

				// Delete the old order queue
				delete order_queue_pointer;

				// Move tracking objects to outside scope per asset
				submitted_limit_orders_per_asset.emplace(asset_id, submitted_limit_orders_per_asset);
				transacted_limit_orders_per_asset.emplace(asset_id, transacted_limit_orders);
				cancelled_limit_orders_per_asset.emplace(asset_id, cancelled_orders);
				transactions_per_asset.emplace(asset_id, transactions);

				// TODO: Check that the order book is not crossed here aswell
			}
			
			// TODO: Allow custom action after each step

			// TODO: Allow custom action for the last step

			// Track limit orders and book depth across assets
			auto limit_orders_per_asset = std::map<AssetID, OrderBook::FlatOrderBook>();
			auto book_depth_per_asset   = std::map<AssetID, OrderBook::BookDepth>();
			for (auto& [asset_id, order_book_pair] : asset_order_books.inner) {
				auto order_book_lock = std::unique_lock(order_book_pair.mutex);
				const auto& order_book = order_book_pair.inner;

				limit_orders_per_asset.emplace(asset_id, order_book.get_limit_orders());
				book_depth_per_asset.emplace(asset_id, order_book.get_book_depth());
			}

			// Track user statistics
			for (const auto& [asset_id, asset] : asset_id_to_asset.inner) {

			}

			return StepResult{
				
			};
		}
	};
};


