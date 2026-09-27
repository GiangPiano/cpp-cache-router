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
#include "cacherouter/utils/hash.hpp"

namespace cacherouter {

using Clock = std::chrono::system_clock;


CacheCluster::CacheCluster(std::unique_ptr<router::Router> router) : router_(std::move(router)) {}

[[nodiscard]] std::vector<Node> CacheCluster::list_nodes() const {
    std::vector<Node> nodes;
    nodes.reserve(nodes_.size());

    for (const auto& [k, v] : nodes_) nodes.push_back(v.first);

    return nodes;
};

[[nodiscard]] std::vector<NodeStatus> CacheCluster::node_status() const {
    std::vector<NodeStatus> status;
    status.reserve(nodes_.size());

    for (const auto& [id, entry] : nodes_) {
        std::ignore = id;
        status.push_back({.node = entry.first, .used = entry.second->size()});
    }

    return status;
}


std::unique_ptr<Cache<std::string, std::string>> CacheCluster::make_cache(const Node& node) {
    auto cache_ptr = std::make_unique<Cache<std::string, std::string>>(
        node.capacity, make_policy(node.policy_name));

    cache_ptr->set_event_handler([this, node](CacheEvent e) {
        e.node_id = node.id;
        emit({.timestamp = Clock::now(), .event = std::move(e)});
    });
    return cache_ptr;
}


void CacheCluster::add_node(const cacherouter::Node& node) {
    if (nodes_.contains(node.id)) return;

    nodes_.emplace(node.id, std::make_pair(node, make_cache(node)));
    router_->add_node(node.id, node.virtual_nodes);
}


void CacheCluster::remove_node(const NodeId& id) {
    nodes_.erase(id);
    router_->remove_node(id);
}


void CacheCluster::set_router(std::unique_ptr<router::Router> router) {
    router_ = std::move(router);

    // Migrating the current cache nodes into the new router
    // The nodes content however are not migrated, meaning that all cached data are wiped
    std::vector<NodeId> ids;
    ids.reserve(nodes_.size());
    for (const auto& [id, entry] : nodes_) ids.push_back(id);
    std::ranges::sort(ids);

    for (const auto& id : ids) router_->add_node(id, nodes_.at(id).first.virtual_nodes);
}


[[nodiscard]] const router::Router& CacheCluster::router() const { return *router_; }


void CacheCluster::clear_data() {
    for (auto& [id, entry] : nodes_) {
        std::ignore = id;
        entry.second = make_cache(entry.first);
    }
}


void CacheCluster::reset() {
    for (const auto& [id, entry] : nodes_) {
        std::ignore = entry;
        router_->remove_node(id);
    }
    nodes_.clear();
}


void CacheCluster::put(const std::string& key, std::string value) {
    NodeId id = router_->route(key);
    nodes_.at(id).second->put(key, std::move(value));
    emit({.timestamp = Clock::now(),
          .event = RouterEvent{.key = key, .node = id, .router_name = router_->name()}});
}


std::optional<std::string> CacheCluster::get(const std::string& key) {
    NodeId id = router_->route(key);
    return nodes_.at(id).second->get(key);
}

void CacheCluster::set_event_handler(std::function<void(Event)> handler) {
    on_event_ = std::move(handler);
}


void CacheCluster::emit(Event e) {
    if (on_event_) on_event_(std::move(e));
}

}  // namespace cacherouter
