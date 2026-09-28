#include "cacherouter/router/simple_router.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "cacherouter/node.hpp"
#include "cacherouter/utils/hash.hpp"

namespace cacherouter::router {

void SimpleRouter::add_node(const NodeId& node, int virtual_nodes) {
    std::ignore = virtual_nodes;

    if (auto it = std::ranges::find(nodes_, node); it == nodes_.end()) {
        nodes_.push_back(node);
    }
}

void SimpleRouter::remove_node(const NodeId& node) {
    if (auto it = std::ranges::find(nodes_, node); it != nodes_.end()) {
        std::swap(*it, nodes_.back());
        nodes_.pop_back();
    }
}

void SimpleRouter::clear() { nodes_.clear(); }

[[nodiscard]] NodeId SimpleRouter::route(const std::string& key) const {
    // Guarded rather than left to `% 0`: resetting the cluster makes an empty
    // router reachable, and ConsistentRouter::route reports the same way.
    if (nodes_.empty()) throw std::runtime_error("No nodes available.");
    return nodes_.at(hash(key) % nodes_.size());
}

[[nodiscard]] std::vector<NodeId> SimpleRouter::nodes() const { return nodes_; }

[[nodiscard]] std::string SimpleRouter::name() const { return "simple"; }

}  // namespace cacherouter::router
