#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>

#include "cacherouter/router/router.hpp"

namespace cacherouter {

enum class CacheEventType : uint8_t { Hit, Miss, Insert, Evict, Update };
struct CacheEvent {
    CacheEventType type;
    std::string key;
    // NodeId emitter;
};

struct RouterEvent {
    std::string key;
    router::NodeId node;
    std::string router_name;
};

struct Event {
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    std::variant<CacheEvent, RouterEvent> event;
};

}  // namespace cacherouter
