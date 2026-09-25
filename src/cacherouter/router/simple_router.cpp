#include "cacherouter/router/simple_router.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "../utils/hash.hpp"
#include "cacherouter/node.hpp"

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

[[nodiscard]] NodeId SimpleRouter::route(const std::string& key) const {
    return nodes_.at(hash(key) % nodes_.size());
}

[[nodiscard]] std::vector<NodeId> SimpleRouter::nodes() const { return nodes_; }

[[nodiscard]] std::string SimpleRouter::name() const { return "simple"; }

}  // namespace cacherouter::router
