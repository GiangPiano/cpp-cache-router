#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "cache/lru_cache.hpp"
#include "router/consistent_router.hpp"
#include "router/simple_router.hpp"
#include "utils/error.hpp"

constexpr std::string_view to_string(CacheError err) {
  switch (err) {
    case CacheError::CacheMiss:       return "CacheMiss";
    case CacheError::CacheNotFound:   return "CacheNotFound";
    case CacheError::ClusterEmpty:    return "ClusterEmpty";
    case CacheError::UnexpectedError: return "UnexpectedError";
  }
}

int32_t main() {
  std::vector<std::string> nodes{"node-a", "node-b", "node-c", "node-d", "node-e", "node-f"};
  ConsistentRouter<LRUCache> cache_cluster{nodes, 1, 100};
  std::cout << "Added entry to node: " << cache_cluster.set("user-a", "handsome1").value() << '\n';
  std::cout << "Added entry to node: " << cache_cluster.set("user-b", "handsome2").value() << '\n';
  std::cout << "Added entry to node: " << cache_cluster.set("user-a", "handsome3").value() << '\n';
  std::cout << "Added entry to node: " << cache_cluster.set("user-c", "handsome4").value() << '\n';
  std::cout << "Added entry to node: " << cache_cluster.set("user-d", "handsome5").value() << '\n';

  for (const auto& key : {"user-a", "user-b", "user-c", "user-d"}) {
    if (auto result = cache_cluster.get(key)) {
      std::cout << key << " -> " << *result << "\n";
    } else {
      std::cout << key << " -> " << to_string(result.error()) << "\n";
    }
  }

  return 0;
}
