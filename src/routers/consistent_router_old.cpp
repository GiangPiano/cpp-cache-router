
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <string>
#include <vector>

#include "cache/cache_policy.hpp"
#include "router/base_router.hpp"
#include "utils/error.hpp"

template <CachePolicy Cache>
class ConsistentRouter : public BaseRouter<Cache> {
  std::vector<std::string> node_list;
  std::map<uint64_t, std::string> ring;
  size_t virtual_node_count{10};

 protected:
  std::expected<std::string, CacheError> locate(const std::string& key) override {
    if (ring.empty()) return std::unexpected(CacheError::ClusterEmpty);
    if (auto it = ring.lower_bound(hash_key(key)); it != ring.end()) return it->second;
    return ring.begin()->second;
  }

 public:
  ConsistentRouter() = default;

  explicit ConsistentRouter(std::vector<std::string> nodes, size_t node_capacity,
                            size_t virtual_node_count = 10)
      : BaseRouter<Cache>(nodes, node_capacity)
      , node_list(nodes)
      , virtual_node_count{virtual_node_count} {
    for (const auto& node : nodes) {
      for (size_t i = 1; i <= virtual_node_count; i++) {
        ring.emplace(hash_key(node + ":" + std::to_string(i)), node);
      }
    }
  }

  void add_node(const std::string& node) override {
    BaseRouter<Cache>::add_node(node);
    for (size_t i = 1; i <= virtual_node_count; i++) {
      ring.emplace(hash_key(node + ":" + std::to_string(i)), node);
    }
  }

  void delete_node(const std::string& node) override {
    BaseRouter<Cache>::delete_node(node);
    for (size_t i = 1; i <= virtual_node_count; i++) {
      ring.erase(hash_key(node + ":" + std::to_string(i)));
    }
  }
};
