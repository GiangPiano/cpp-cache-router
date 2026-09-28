#include "cacherouter/router/consistent_router.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "cacherouter/node.hpp"
#include "cacherouter/router/hash_ring.hpp"

namespace cacherouter::router {

ConsistentRouter::ConsistentRouter(int virtual_nodes) : ring_{virtual_nodes} {}

void ConsistentRouter::add_node(const NodeId& node, int virtual_nodes) {
    ring_.add_node(node, virtual_nodes);
}
void ConsistentRouter::remove_node(const NodeId& node) { ring_.remove_node(node); }
void ConsistentRouter::clear() { ring_.clear(); }

[[nodiscard]] NodeId ConsistentRouter::route(const std::string& key) const {
    if (auto node = ring_.find_node(key)) return *node;
    throw std::runtime_error("No nodes available.");
}

[[nodiscard]] std::string ConsistentRouter::name() const { return "consistent"; }
[[nodiscard]] std::vector<NodeId> ConsistentRouter::nodes() const { return ring_.nodes(); }
[[nodiscard]] const Ring& ConsistentRouter::ring() const { return ring_.get_ring(); }

}  // namespace cacherouter::router
