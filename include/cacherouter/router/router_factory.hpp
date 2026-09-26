#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "cacherouter/router/consistent_router.hpp"
#include "cacherouter/router/router.hpp"
#include "cacherouter/router/simple_router.hpp"

namespace cacherouter::router {

inline std::unique_ptr<Router> make_router(const std::string& router, int virtual_nodes = 100) {
    if (router == "simple") return std::make_unique<SimpleRouter>();
    if (router == "consistent") return std::make_unique<ConsistentRouter>(virtual_nodes);
    throw std::invalid_argument("unknown router: " + router);
}

inline std::vector<std::string> available_routers() { return {"simple", "consistent"}; }

}  // namespace cacherouter::router
