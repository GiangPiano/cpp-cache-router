#include "cacherouter/router/hash_ring.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "cacherouter/utils/hash.hpp"

namespace cacherouter::router {

HashRing::HashRing(int virtual_nodes) : vnodes_(virtual_nodes) {}

void HashRing::add_node(const std::string& node, int virtual_nodes) {
    if (ring_.contains(hash(node))) return;
    const int replicas = virtual_nodes > 0 ? virtual_nodes : vnodes_;
    ring_.emplace(hash(node), node);
    for (int i = 0; i < replicas; i++) {
        ring_.emplace(hash(node + ":" + std::to_string(i)), node);
    }
}

void HashRing::remove_node(const std::string& node) {
    std::erase_if(ring_, [&node](const auto& entry) { return entry.second == node; });
}

void HashRing::clear() { ring_.clear(); }

[[nodiscard]] std::optional<std::string> HashRing::find_node(const std::string& key) const {
    if (ring_.empty()) return std::nullopt;
    if (auto it = ring_.lower_bound(hash(key)); it != ring_.end()) return it->second;
    return ring_.begin()->second;
};

[[nodiscard]] std::vector<std::string> HashRing::nodes() const {
    std::set<std::string> unique_nodes;
    for (const auto& [k, v] : ring_) unique_nodes.insert(v);
    return {unique_nodes.begin(), unique_nodes.end()};
};

[[nodiscard]] const Ring& HashRing::get_ring() const { return ring_; }

}  // namespace cacherouter::router
