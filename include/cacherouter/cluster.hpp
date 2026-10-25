#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
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

    [[nodiscard]] std::vector<NodeSpec> list_nodes() const;
    void add_node(const NodeSpec& node);
    void remove_node(const NodeId& id);

    void set_router(std::unique_ptr<router::Router> router);

    // The lock router() takes internally is released before the reference
    // reaches the caller, so it gives no protection against a concurrent
    // set_router() freeing the object out from under it — safe only for
    // single-threaded callers (tests, startup). Concurrent code must use
    // with_router() below instead, which holds the lock for the callback's
    // whole duration.
    [[nodiscard]] const router::Router& router() const;

    // Runs fn(router) while holding the topology lock, so the reference stays
    // valid for fn's whole duration.
    template <typename F>
    auto with_router(F&& fn) const -> decltype(fn(std::declval<const router::Router&>())) {
        std::shared_lock lock(topology_mu_);
        return fn(*router_);
    }

    void clear_data();
    void clear_nodes();
    void reset_to_default();

    void put(const std::string& key, std::string value);
    std::optional<std::string> get(const std::string& key);

    void set_event_handler(std::function<void(Event)> handler);

private:
    void emit(Event ev);
    [[nodiscard]] std::unique_ptr<Cache<std::string, std::string>> make_cache(const NodeSpec& node);

    // Assume topology_mu_ is already held exclusively by the caller.
    void add_node_locked(const NodeSpec& node);
    void clear_nodes_locked();

    std::unique_ptr<router::Router> router_;
    std::function<void(Event)> on_event_;
    std::unordered_map<NodeId, std::unique_ptr<Cache<std::string, std::string>>> nodes_;

    // Guards nodes_ membership and router_. Node/router mutation (add/remove
    // node, router swap, reset) takes this exclusively; routing a key and
    // listing nodes only need to read it, so they take it shared and run
    // concurrently with each other. The per-node Cache has its own mutex for
    // the actual get/put mutation, so holding this shared during a route-and-
    // dispatch doesn't serialize traffic across different nodes.
    mutable std::shared_mutex topology_mu_;
};

}  // namespace cacherouter
