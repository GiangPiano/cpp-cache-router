#include "cacherouter/routers/consistent_router.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include "cacherouter/routers/hash_ring.hpp"
#include "cacherouter/routers/router.hpp"

namespace cacherouter {

ConsistentRouter::ConsistentRouter(int virtual_nodes) : ring_{virtual_nodes} {}

void ConsistentRouter::add_node(const NodeId& node) { ring_.add_node(node); }
void ConsistentRouter::remove_node(const NodeId& node) { ring_.remove_node(node); }

[[nodiscard]] NodeId ConsistentRouter::route(const std::string& key) const {
    if (auto node = ring_.find_node(key)) return *node;
    throw std::runtime_error("No nodes available.");
}

[[nodiscard]] std::string ConsistentRouter::name() const { return "consistent"; }
[[nodiscard]] std::vector<NodeId> ConsistentRouter::nodes() const { return ring_.nodes(); }
[[nodiscard]] const HashRing& ConsistentRouter::ring() const { return ring_; }

}  // namespace cacherouter
