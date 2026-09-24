// Demo / smoke test executable for the cache router project.
// Not a unit test suite, just a walkthrough with printed input and output
// so the pieces can be sanity checked by eye.

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "cacherouter/cache.hpp"
#include "cacherouter/cluster.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/policy/policy_factory.hpp"
#include "cacherouter/router/consistent_router.hpp"
#include "cacherouter/router/simple_router.hpp"

namespace cr = cacherouter;

namespace {

std::string event_type_name(cr::CacheEventType type) {
    switch (type) {
        case cr::CacheEventType::Hit:    return "Hit";
        case cr::CacheEventType::Miss:   return "Miss";
        case cr::CacheEventType::Insert: return "Insert";
        case cr::CacheEventType::Update: return "Update";
        case cr::CacheEventType::Evict:  return "Evict";
    }
    return "Unknown";
}

void print_optional(const std::string& label, const std::optional<std::string>& value) {
    if (value) {
        std::cout << label << " -> \"" << *value << "\"\n";
    } else {
        std::cout << label << " -> (miss)\n";
    }
}

void demo_lru_cache() {
    std::cout << "=== Demo 1: Cache<std::string,std::string> with LRU policy, capacity 2 ===\n";

    cr::Cache<std::string, std::string> cache(2, cr::make_policy("lru"));
    cache.set_event_handler([](const cr::CacheEvent& e) {
        std::cout << "  event: " << event_type_name(e.type) << " key=\"" << e.key << "\"\n";
    });

    std::cout << "put(\"a\", \"apple\")\n";
    cache.put("a", "apple");

    std::cout << "put(\"b\", \"banana\")\n";
    cache.put("b", "banana");

    std::cout << "get(\"a\")\n";
    print_optional("  get(\"a\")", cache.get("a"));  // touches "a", making "b" the LRU

    std::cout << "put(\"c\", \"cherry\")  // capacity 2 exceeded, expect \"b\" evicted\n";
    cache.put("c", "cherry");

    print_optional("get(\"a\")", cache.get("a"));  // expect hit, "a" was recently used
    print_optional("get(\"b\")", cache.get("b"));  // expect miss, evicted
    print_optional("get(\"c\")", cache.get("c"));  // expect hit

    std::cout << "size() -> " << cache.size() << "\n";
    std::cout << "policy() -> " << cache.policy() << "\n\n";
}

void demo_routers() {
    std::cout << "=== Demo 3: SimpleRouter vs ConsistentRouter shard assignment ===\n";

    cr::router::SimpleRouter simple{};
    cr::router::ConsistentRouter consistent{};

    for (const auto& shard : {"node-A", "node-B", "node-C"}) {
        simple.add_node(shard);
        consistent.add_node(shard);
    }

    std::vector<std::string> keys = {"user:1", "user:2", "user:3", "session:42"};

    for (const auto& key : keys) {
        std::cout << "key=\"" << key << "\""
                  << "  simple -> " << simple.route(key) << "  consistent -> "
                  << consistent.route(key) << "\n";
    }

    std::cout << "\nAdding \"node-D\" and re-routing the same keys:\n";
    simple.add_node("node-D");
    consistent.add_node("node-D");

    int simple_moved = 0;
    int consistent_moved = 0;
    // NOTE: this recomputation is illustrative only; a real remap-count test
    // would compare against the routing captured before the shard was added.
    for (const auto& key : keys) {
        std::cout << "key=\"" << key << "\""
                  << "  simple -> " << simple.route(key) << "  consistent -> "
                  << consistent.route(key) << "\n";
    }
    (void)simple_moved;
    (void)consistent_moved;
    std::cout << "\n";
}

void demo_cache_cluster() {
    std::cout << "=== Demo 4: CacheCluster wiring Router + per-shard Cache ===\n";
    cr::router::ConsistentRouter consistent;
    cr::CacheCluster cluster(std::make_unique<cr::router::ConsistentRouter>(consistent));
    cluster.set_event_handler([](cr::Event e) {
        std::visit(
            [](auto&& ev) {
                using T = std::decay_t<decltype(ev)>;
                if constexpr (std::is_same_v<T, cr::CacheEvent>) {
                    std::cout << "  [cache]  " << event_type_name(ev.type) << " key=\"" << ev.key
                              << "\"\n";
                } else if constexpr (std::is_same_v<T, cr::RouterEvent>) {
                    std::cout << "  [routed] key=\"" << ev.key << "\""
                              << " -> shard=\"" << ev.node << "\""
                              << " (via " << ev.router_name << ")\n";
                }
            },
            e.event);
    });

    cluster.add_node("node-A", 2, "lru");
    cluster.add_node("node-B", 2, "lru");

    std::cout << "put(\"alpha\", \"1\")\n";
    cluster.put("alpha", "1");

    std::cout << "put(\"beta\", \"2\")\n";
    cluster.put("beta", "2");

    std::cout << "get(\"alpha\")\n";
    print_optional("  get(\"alpha\")", cluster.get("alpha"));

    std::cout << "get(\"missing-key\")\n";
    print_optional("  get(\"missing-key\")", cluster.get("missing-key"));
}

}  // namespace

int main() {
    demo_lru_cache();
    demo_routers();
    demo_cache_cluster();
    return 0;
}
