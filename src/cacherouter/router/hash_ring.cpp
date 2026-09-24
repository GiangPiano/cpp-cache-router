#include "cacherouter/router/hash_ring.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "../utils/hash.hpp"

namespace cacherouter::router {

HashRing::HashRing(int virtual_nodes) : vnodes_(virtual_nodes) {}

void HashRing::add_node(const std::string& node) {
    if (ring_.contains(hash(node))) return;
    ring_.emplace(hash(node), node);
    for (int i = 0; i < vnodes_; i++) {
        ring_.emplace(hash(node + ":" + std::to_string(i)), node);
    }
}

void HashRing::remove_node(const std::string& node) {
    if (!ring_.contains(hash(node))) return;
    ring_.erase(hash(node));
    for (int i = 0; i < vnodes_; i++) {
        ring_.erase(hash(node + ":" + std::to_string(i)));
    }
}

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

[[nodiscard]] const std::map<uint64_t, std::string>& HashRing::get_ring() const { return ring_; }

uint64_t HashRing::hash(const std::string& key) { return hash_key(key); }

}  // namespace cacherouter::router
