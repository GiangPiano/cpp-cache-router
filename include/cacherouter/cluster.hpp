#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "cacherouter/cache.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/router/router.hpp"

namespace cacherouter {

class CacheCluster {
public:
    explicit CacheCluster(std::unique_ptr<router::Router> router);

    void add_node(const router::NodeId& id, size_t capacity, const std::string& policy_name);
    void remove_node(const router::NodeId& id);

    void put(const std::string& key, std::string value);
    std::optional<std::string> get(const std::string& key);

    void set_event_handler(std::function<void(Event)> handler);

private:
    void emit(Event ev);

    std::unique_ptr<router::Router> router_;
    std::unordered_map<router::NodeId, std::unique_ptr<Cache<std::string, std::string>>> nodes_;
    std::function<void(Event)> on_event_;
};

}  // namespace cacherouter
