#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cacherouter/cache.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/node.hpp"
#include "cacherouter/router/router.hpp"

namespace cacherouter {

class CacheCluster {
public:
    explicit CacheCluster(std::unique_ptr<router::Router> router);

    [[nodiscard]] std::vector<Node> list_nodes() const;
    void add_node(const Node& node);
    void remove_node(const NodeId& id);

    void put(const std::string& key, std::string value);
    std::optional<std::string> get(const std::string& key);
    [[nodiscard]] uint64_t get_hash(const std::string& key) const;

    void set_event_handler(std::function<void(Event)> handler);

private:
    void emit(Event ev);

    std::unique_ptr<router::Router> router_;
    std::function<void(Event)> on_event_;
    std::unordered_map<NodeId, std::pair<Node, std::unique_ptr<Cache<std::string, std::string>>>>
        nodes_;
};

}  // namespace cacherouter
