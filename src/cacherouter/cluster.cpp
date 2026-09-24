#include "cacherouter/cluster.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cacherouter/cache.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/policy/policy_factory.hpp"
#include "cacherouter/router/router.hpp"

namespace cacherouter {

using Clock = std::chrono::system_clock;


CacheCluster::CacheCluster(std::unique_ptr<router::Router> router) : router_(std::move(router)) {}

[[nodiscard]] std::vector<router::NodeId> CacheCluster::list_nodes() const {
    std::vector<router::NodeId> node_ids;
    node_ids.reserve(nodes_.size());
    for (const auto& [k, v] : nodes_) node_ids.push_back(k);
    return node_ids;
};

void CacheCluster::add_node(const router::NodeId& id, size_t capacity,
                            const std::string& policy_name) {
    if (nodes_.contains(id)) return;
    auto new_node =
        std::make_unique<Cache<std::string, std::string>>(capacity, make_policy(policy_name));
    new_node->set_event_handler([this, id](CacheEvent e) {
        emit({.timestamp = std::chrono::system_clock::now(), .event = e});
    });

    nodes_.emplace(id, std::move(new_node));
    router_->add_node(id);
}


void CacheCluster::remove_node(const router::NodeId& id) {
    nodes_.erase(id);
    router_->remove_node(id);
}


void CacheCluster::put(const std::string& key, std::string value) {
    router::NodeId id = router_->route(key);
    nodes_.at(id)->put(key, std::move(value));
    emit({.timestamp = Clock::now(),
          .event = RouterEvent{.key = key, .node = id, .router_name = router_->name()}});
}


std::optional<std::string> CacheCluster::get(const std::string& key) {
    router::NodeId id = router_->route(key);
    return nodes_.at(id)->get(key);
}


void CacheCluster::set_event_handler(std::function<void(Event)> handler) {
    on_event_ = std::move(handler);
}


void CacheCluster::emit(Event e) {
    if (on_event_) on_event_(std::move(e));
}

}  // namespace cacherouter
