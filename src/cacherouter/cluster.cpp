#include "cacherouter/cluster.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "cacherouter/cache.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/node.hpp"
#include "cacherouter/policy/policy_factory.hpp"
#include "cacherouter/router/router.hpp"

namespace cacherouter {

using Clock = std::chrono::system_clock;


CacheCluster::CacheCluster(std::unique_ptr<router::Router> router) : router_(std::move(router)) {}


[[nodiscard]] std::vector<NodeSpec> CacheCluster::list_nodes() const {
    std::vector<NodeSpec> nodes;
    nodes.reserve(nodes_.size());

    for (const auto& [id, cache] : nodes_) {
        std::ignore = id;
        nodes.push_back(cache->info());
    }

    return nodes;
};

std::unique_ptr<Cache<std::string, std::string>> CacheCluster::make_cache(const NodeSpec& node) {
    auto cache_ptr =
        std::make_unique<Cache<std::string, std::string>>(node, make_policy(node.policy_name));

    // The cache stamps node_id itself, so this only forwards.
    cache_ptr->set_event_handler(
        [this](CacheEvent e) { emit({.timestamp = Clock::now(), .event = std::move(e)}); });
    return cache_ptr;
}


void CacheCluster::add_node(const cacherouter::NodeSpec& node) {
    if (nodes_.contains(node.id)) return;

    // Constructed first: make_policy throws on an unknown policy, and nothing
    // should be registered if it does.
    auto cache_ptr = make_cache(node);

    nodes_.emplace(node.id, std::move(cache_ptr));
    router_->add_node(node.id, node.virtual_nodes);
}


void CacheCluster::remove_node(const NodeId& id) {
    router_->remove_node(id);
    nodes_.erase(id);
}


void CacheCluster::clear_nodes() {
    router_->clear();
    nodes_.clear();
}


void CacheCluster::reset_to_default() {
    clear_nodes();

    // default configuration
    add_node({.id = "node-A", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-B", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-C", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-D", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-E", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-F", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-G", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-H", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-I", .capacity = 50, .policy_name = "lru"});
    add_node({.id = "node-J", .capacity = 50, .policy_name = "lru"});
}


void CacheCluster::clear_data() {
    // info() is a reference into the cache the assignment is about to destroy,
    // so the spec is copied out first.
    for (auto& [id, cache] : nodes_) {
        std::ignore = id;
        const NodeSpec spec = cache->info();
        cache = make_cache(spec);
    }
}


void CacheCluster::put(const std::string& key, std::string value) {
    NodeId id = router_->route(key);
    nodes_.at(id)->put(key, std::move(value));
    emit({.timestamp = Clock::now(),
          .event = RouterEvent{.key = key, .node = id, .router_name = router_->name()}});
}


std::optional<std::string> CacheCluster::get(const std::string& key) {
    NodeId id = router_->route(key);
    return nodes_.at(id)->get(key);
}

void CacheCluster::set_event_handler(std::function<void(Event)> handler) {
    on_event_ = std::move(handler);
}


void CacheCluster::emit(Event e) {
    if (on_event_) on_event_(std::move(e));
}

void CacheCluster::set_router(std::unique_ptr<router::Router> router) {
    router_ = std::move(router);

    // Migrating the current cache nodes into the new router
    // The nodes content however are not migrated, meaning that all cached data are wiped
    std::vector<NodeId> ids;
    ids.reserve(nodes_.size());
    for (const auto& [id, cache] : nodes_) {
        std::ignore = cache;
        ids.push_back(id);
    }
    std::ranges::sort(ids);

    for (const auto& id : ids) router_->add_node(id, nodes_.at(id)->info().virtual_nodes);
}


[[nodiscard]] const router::Router& CacheCluster::router() const { return *router_; }

}  // namespace cacherouter
