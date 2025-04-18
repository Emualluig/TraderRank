#pragma once
#include <cstdint>
#include <optional>
#include <map>

namespace Interfaces {
	using UserID = uint32_t;
	using SecurityID = uint32_t;

	class ISimulation {



	public:

		// User management
		virtual std::optional<UserID> add_user() = 0;
		virtual std::optional<std::map<SecurityID, float>> get_user_portfolio(UserID user_id) = 0;
	};

};


