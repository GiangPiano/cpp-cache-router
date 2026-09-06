#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "core/cache.hpp"
#include "core/error.hpp"
#include "core/router.hpp"

constexpr std::string_view to_string(CacheError err) {
  switch (err) {
    case CacheError::CacheMiss:     return "CacheMiss";
    case CacheError::CacheNotFound: return "CacheNotFound";
    case CacheError::ClusterEmpty:  return "ClusterEmpty";
  }
  return "UnknownCacheError";
}

int32_t main() {
  std::vector<std::string> nodes{"node-a", "node-b", "node-c"};
  ConsistentRouter<LRUCache> cache_cluster{nodes, 100};
  cache_cluster.set("user-a", "handsome1");
  cache_cluster.set("user-b", "handsome2");
  cache_cluster.set("user-a", "handsome3");
  cache_cluster.set("user-c", "handsome4");
  cache_cluster.set("user-d", "handsome5");

  for (const auto& key : {"user-a", "user-b", "user-c", "user-d"}) {
    if (auto result = cache_cluster.get(key)) {
      std::cout << key << " -> " << *result << "\n";
    } else {
      std::cout << key << " -> " << to_string(result.error()) << "\n";
    }
  }

  return 0;
}
