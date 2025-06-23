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

namespace SingleThreadedTraderRank {
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
	template<typename T> constexpr bool always_false = false;

	template<typename T>
	struct BidAskStruct {
		T bid;
		T ask;
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
		Step step;
		UserID buyer;
		UserID seller;
		float transacted_price;
		float transacted_volume;
	};
	struct TransactionStub {
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

	class IAsset {};

	class Simulation : public std::enable_shared_from_this<Simulation> {
		struct Private { explicit Private() = default; };
		explicit Simulation(Private) {}
	public:
		static std::shared_ptr<Simulation> create_python() {
			return std::make_shared<Simulation>(Private());
		}
		static std::shared_ptr<Simulation> create_cpp() {
			return std::make_shared<Simulation>(Private());
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
	public:
		bool has_next_step() const noexcept {
			return step_counter < max_step_count;
		}
		Step get_step() const noexcept {
			return step_counter;
		}
		Step get_max_step() const noexcept {
			return step_counter;
		}
		bool does_user_id_exist(const UserID user_id) const noexcept {
			return user_id < user_id_counter;
		}
		bool does_asset_id_exist(const AssetID asset_id) const noexcept {
			return asset_id < asset_id_counter;
		}
		bool does_order_id_exist(const OrderID order_id) const noexcept {
			return order_id < order_id_counter;
		}
		bool does_transaction_id_exist(const TransactionID transaction_id) const noexcept {
			return transaction_id < transaction_id_counter;
		}
	private:
		struct AssetBlob {
			std::shared_ptr<IAsset> ptr;
			AssetTicker self_ticker;
			AssetID self_asset_id;
			AssetID denominated_asset_id;
			OrderBook order_book;
			std::map<UserID, float> positions;
		};
		std::map<AssetID, AssetBlob> asset_id_to_asset_blob = {};
	public:
		struct TradingStatistics {
			float position;
			float cost;
			float vwap;
			float realized;
			float unrealized;
			float net_liquidation_value;
		};
		struct StepResult {
			Step current_step;
			bool has_next_step;
			// std::map<UserID, Username> user_id_to_username;
			// std::map<AssetID, AssetTicker> asset_id_to_ticker;
			std::map<AssetID, std::vector<OrderID>> submitted_limit_orders_per_asset;
			std::map<AssetID, std::map<OrderID, float>> transacted_limit_orders_per_asset;
			std::map<AssetID, std::vector<OrderID>> cancelled_limit_orders_per_asset;
			std::map<AssetID, std::vector<Transaction>> transactions_per_asset;
			std::map<AssetID, OrderBook::FlatOrderBook> limit_orders_per_asset;
			std::map<AssetID, OrderBook::BookDepth> book_depth_per_asset;
			std::map<AssetID, std::map<UserID, TradingStatistics>> statistics_per_user_per_asset;
		};
		StepResult process_step(std::map<AssetID, std::vector<VariantOrder>>& received_orders_discardable) {
			if (!has_next_step()) {
				throw std::runtime_error("Passed simulation endpoint!");
			}

			// TODO: Allow custom action for the first step

			// Increment the step
			step_counter += 1;

			// TODO: Allow custom action before each step

			auto submitted_limit_orders_per_asset = std::map<AssetID, std::vector<OrderID>>();
			auto transacted_limit_orders_per_asset = std::map<AssetID, std::map<OrderID, float>>();
			auto cancelled_limit_orders_per_asset = std::map<AssetID, std::vector<OrderID>>();
			auto transactions_per_asset = std::map<AssetID, std::vector<Transaction>>();
			for (auto& [asset_id, asset_blob] : asset_id_to_asset_blob) {
				auto& ptr = asset_blob.ptr;
				auto& order_book = asset_blob.order_book;
				auto denominated_asset_id = asset_blob.denominated_asset_id;

				// Keep track of submitted/transacted/cancelled/transactions
				auto submitted_orders = std::vector<OrderID>();
				auto transacted_limit_orders = std::map<OrderID, float>(); // OrderID -> Transaced volume
				auto cancelled_orders = std::vector<OrderID>();
				auto transactions = std::vector<Transaction>();

				// Local functions
				auto perform_transaction_lambda = [&](
					UserID buyer_id,
					UserID seller_id,
					float transacted_price,
					float transacted_volume
				) -> void {
					//


					transactions.push_back(Transaction{
						.step = step_counter,
						.buyer = buyer_id,
						.seller = seller_id,
						.transacted_price = transacted_price,
						.transacted_volume = transacted_volume
					});
				};

				auto& orders = received_orders_discardable.at(asset_id);
				for (auto& order : orders) {
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

				submitted_limit_orders_per_asset.emplace(asset_id, submitted_limit_orders_per_asset);
				transacted_limit_orders_per_asset.emplace(asset_id, transacted_limit_orders);
				cancelled_limit_orders_per_asset.emplace(asset_id, cancelled_orders);
				transactions_per_asset.emplace(asset_id, transactions);
			}
		
			// TODO: Allow custom action after each step

			// TODO: Allow custom action for the last step

			auto limit_orders_per_asset = std::map<AssetID, OrderBook::FlatOrderBook>();
			auto book_depth_per_asset = std::map<AssetID, OrderBook::BookDepth>();
			auto statistics_per_user_per_asset = std::map<AssetID, std::map<UserID, TradingStatistics>>();
			for (const auto& [asset_id, asset_blob] : asset_id_to_asset_blob) {
				limit_orders_per_asset.emplace(asset_id, asset_blob.order_book.get_limit_orders());
				book_depth_per_asset.emplace(asset_id, asset_blob.order_book.get_book_depth());


			}

			return StepResult{
				.current_step = step_counter,
				.has_next_step = has_next_step(),
				.submitted_limit_orders_per_asset = submitted_limit_orders_per_asset,
				.transacted_limit_orders_per_asset = transacted_limit_orders_per_asset,
				.cancelled_limit_orders_per_asset = cancelled_limit_orders_per_asset,
				.transactions_per_asset = transactions_per_asset,
				.limit_orders_per_asset = limit_orders_per_asset,
				.book_depth_per_asset = book_depth_per_asset,
				.statistics_per_user_per_asset = statistics_per_user_per_asset,
			};
		}
	};
};
