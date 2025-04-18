#pragma once
#include <cstdint>
#include <optional>
#include <map>
#include <set>
#include <vector>
#include <variant>

namespace Interfaces {
	using UserID = uint32_t;
	using SecurityID = uint32_t;
	using SecurityTicker = std::string;
	using OrderID = uint32_t;

	enum OrderSide : uint8_t {
		BID,
		ASK
	};

	struct LimitOrder {};
	struct CancelOrder {};
	using OrderVariant = std::variant<LimitOrder, CancelOrder>;

	struct SimulationStepResult {};

	class ISimulation {
	public:
		// Utility methods
		virtual const std::vector<SecurityTicker>& get_all_tickers() noexcept = 0;
		virtual const SecurityTicker& get_security_ticker(SecurityID security_id) noexcept = 0;
		virtual SecurityID get_security_id(const SecurityTicker& security_ticker) noexcept = 0;

		// User management
		virtual UserID add_user() = 0; // May throw
		virtual std::map<SecurityID, float> get_user_portfolio(UserID user_id) = 0; // May throw
		virtual uint32_t get_user_count() noexcept = 0;

		// Simulation meta information
		virtual float get_dt() noexcept = 0;
		virtual float get_t() noexcept = 0;
		virtual float get_T() noexcept = 0;
		virtual uint32_t get_tick() noexcept = 0;
		virtual uint32_t get_N() noexcept = 0;

		// Simulation market information
		virtual LimitOrder& get_top_bid(SecurityID security_id) = 0; // May throw
		virtual LimitOrder& get_top_ask(SecurityID security_id) = 0; // May throw
		virtual std::set<OrderID> get_all_open_user_orders(UserID user_id, SecurityID security_id) = 0; // May throw

		// Simulation actions
		virtual SimulationStepResult do_simulation_step() = 0; // May throw
		virtual OrderID submit_limit_order(UserID user_id, SecurityID security_id, OrderSide side, float price, float volume) = 0; // May throw
		virtual void submit_cancel_order(UserID user_id, SecurityID security_id, OrderID order_id) = 0; // May throw
		virtual void reset_simulation() noexcept = 0;
	};
};


