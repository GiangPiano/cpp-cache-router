#include "cacherouter/cluster.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cacherouter/cache.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/node.hpp"
#include "cacherouter/policy/policy_factory.hpp"
#include "cacherouter/router/router.hpp"
#include "utils/hash.hpp"

namespace cacherouter {

using Clock = std::chrono::system_clock;


CacheCluster::CacheCluster(std::unique_ptr<router::Router> router) : router_(std::move(router)) {}

[[nodiscard]] std::vector<Node> CacheCluster::list_nodes() const {
    std::vector<Node> nodes;
    nodes.reserve(nodes_.size());
    for (const auto& [k, v] : nodes_) nodes.push_back(v.first);
    return nodes;
};

void CacheCluster::add_node(const cacherouter::Node& node) {
    const auto& [id, capacity, policy_name, virtual_nodes] = node;
    if (nodes_.contains(id)) return;
    auto cache_ptr =
        std::make_unique<Cache<std::string, std::string>>(capacity, make_policy(policy_name));
    cache_ptr->set_event_handler([this, id](CacheEvent e) {
        emit({.timestamp = std::chrono::system_clock::now(), .event = e});
    });

    nodes_.emplace(id, std::make_pair(node, std::move(cache_ptr)));
    router_->add_node(id, virtual_nodes);
}


void CacheCluster::remove_node(const NodeId& id) {
    nodes_.erase(id);
    router_->remove_node(id);
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

[[nodiscard]] uint64_t CacheCluster::get_hash(const std::string& key) const { return hash(key); }

void CacheCluster::set_event_handler(std::function<void(Event)> handler) {
    on_event_ = std::move(handler);
}


void CacheCluster::emit(Event e) {
    if (on_event_) on_event_(std::move(e));
}

}  // namespace cacherouter
