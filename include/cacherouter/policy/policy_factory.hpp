#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "cacherouter/policy/eviction_policy.hpp"
// #include "cacherouter/policy/lfu_policy.hpp"
#include "cacherouter/policy/lru_policy.hpp"

namespace cacherouter {

inline std::unique_ptr<EvictionPolicy> make_policy(const std::string& policy) {
    if (policy == "lru") return std::make_unique<policy::LruPolicy>();
    // if (policy == "lfu") return std::make_unique<LfuPolicy>();
    throw std::invalid_argument("unknown policy: " + policy);
}

}  // namespace cacherouter
