#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cacherouter/cache.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/node.hpp"
#include "cacherouter/router/router.hpp"

namespace cacherouter {

class CacheCluster {
public:
    explicit CacheCluster(std::unique_ptr<router::Router> router);

    [[nodiscard]] std::vector<NodeSpec> list_nodes() const;
    void add_node(const NodeSpec& node);
    void remove_node(const NodeId& id);

    void set_router(std::unique_ptr<router::Router> router);
    [[nodiscard]] const router::Router& router() const;

    void clear_data();
    void clear_nodes();
    void reset_to_default();

    void put(const std::string& key, std::string value);
    std::optional<std::string> get(const std::string& key);

    void set_event_handler(std::function<void(Event)> handler);

private:
    void emit(Event ev);
    [[nodiscard]] std::unique_ptr<Cache<std::string, std::string>> make_cache(const NodeSpec& node);

    std::unique_ptr<router::Router> router_;
    std::function<void(Event)> on_event_;
    std::unordered_map<NodeId, std::unique_ptr<Cache<std::string, std::string>>> nodes_;
};

}  // namespace cacherouter
