#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "cacherouter/cluster.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/node.hpp"
#include "cacherouter/router/consistent_router.hpp"
#include "cacherouter/router/router_factory.hpp"

namespace {

using cacherouter::CacheCluster;
using cacherouter::CacheEvent;
using cacherouter::CacheEventType;
using cacherouter::Event;
using cacherouter::NodeId;
using cacherouter::NodeSpec;
using cacherouter::RouterEvent;

CacheCluster make_cluster(const std::string& router = "consistent") {
    return CacheCluster{cacherouter::router::make_router(router)};
}

NodeSpec node(const NodeId& id, std::size_t capacity = 100) {
    return NodeSpec{.id = id, .capacity = capacity, .policy_name = "lru"};
}

std::vector<NodeId> sorted_ids(const CacheCluster& cluster) {
    std::vector<NodeId> ids;
    for (const auto& n : cluster.list_nodes()) ids.push_back(n.id);
    std::ranges::sort(ids);
    return ids;
}

std::size_t used(const CacheCluster& cluster, const NodeId& id) {
    for (const auto& node : cluster.list_nodes()) {
        if (node.id == id) return node.size;
    }
    return 0;
}

std::size_t total_used(const CacheCluster& cluster) {
    std::size_t sum = 0;
    for (const auto& node : cluster.list_nodes()) sum += node.size;
    return sum;
}

// --- topology ---------------------------------------------------------------

TEST(CacheCluster, StartsWithNoNodes) {
    const auto cluster = make_cluster();
    EXPECT_TRUE(cluster.list_nodes().empty());
    EXPECT_TRUE(cluster.router().nodes().empty());
}

TEST(CacheCluster, AddingANodeRegistersItWithTheRouter) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));

    EXPECT_EQ(sorted_ids(cluster), (std::vector<NodeId>{"node-A"}));
    EXPECT_EQ(cluster.router().nodes(), (std::vector<NodeId>{"node-A"}));
}

TEST(CacheCluster, AddingTheSameIdTwiceIsIgnored) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A", 100));
    cluster.add_node(node("node-A", 999));

    ASSERT_EQ(cluster.list_nodes().size(), 1u);
    EXPECT_EQ(cluster.list_nodes().front().capacity, 100u);
}

TEST(CacheCluster, RemovingANodeDeregistersItFromTheRouter) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));
    cluster.add_node(node("node-B"));

    cluster.remove_node("node-A");

    EXPECT_EQ(sorted_ids(cluster), (std::vector<NodeId>{"node-B"}));
    EXPECT_EQ(cluster.router().nodes(), (std::vector<NodeId>{"node-B"}));
}

TEST(CacheCluster, AnInvalidPolicyLeavesTheClusterUntouched) {
    auto cluster = make_cluster();
    const NodeSpec bad{.id = "node-A", .capacity = 10, .policy_name = "nonsense"};

    EXPECT_THROW(cluster.add_node(bad), std::invalid_argument);
    // The cache is built before the node is recorded, so a throw must not half-add.
    EXPECT_TRUE(cluster.list_nodes().empty());
    EXPECT_TRUE(cluster.router().nodes().empty());
}

// --- reads and writes -------------------------------------------------------

TEST(CacheCluster, PutThenGetReturnsTheValue) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));

    cluster.put("alpha", "one");
    EXPECT_EQ(cluster.get("alpha"), std::optional<std::string>{"one"});
}

TEST(CacheCluster, GettingAnAbsentKeyReturnsNullopt) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));

    EXPECT_FALSE(cluster.get("never-written").has_value());
}

TEST(CacheCluster, PutOverwritesAnExistingValue) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));

    cluster.put("alpha", "one");
    cluster.put("alpha", "two");

    EXPECT_EQ(cluster.get("alpha"), std::optional<std::string>{"two"});
    EXPECT_EQ(total_used(cluster), 1u);
}

TEST(CacheCluster, RoutingWithNoNodesThrows) {
    auto cluster = make_cluster("simple");
    EXPECT_THROW(cluster.put("alpha", "one"), std::runtime_error);
    EXPECT_THROW(cluster.get("alpha"), std::runtime_error);
}

TEST(CacheCluster, KeysSpreadOverMultipleNodes) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A", 10000));
    cluster.add_node(node("node-B", 10000));

    for (int i = 0; i < 500; i++) cluster.put("key-" + std::to_string(i), "v");

    EXPECT_EQ(total_used(cluster), 500u);
    EXPECT_GT(used(cluster, "node-A"), 0u);
    EXPECT_GT(used(cluster, "node-B"), 0u);
}

// --- capacity ---------------------------------------------------------------

TEST(CacheCluster, ANodeEvictsOnceItExceedsCapacity) {
    auto cluster = make_cluster();
    cluster.add_node(node("only", 2));  // single node, so every key lands here

    cluster.put("a", "1");
    cluster.put("b", "2");
    EXPECT_EQ(used(cluster, "only"), 2u);

    cluster.put("c", "3");
    EXPECT_EQ(used(cluster, "only"), 2u) << "capacity was exceeded";
    EXPECT_FALSE(cluster.get("a").has_value()) << "LRU key should have been evicted";
    EXPECT_TRUE(cluster.get("b").has_value());
    EXPECT_TRUE(cluster.get("c").has_value());
}

TEST(CacheCluster, ReadingAKeyProtectsItFromTheNextEviction) {
    auto cluster = make_cluster();
    cluster.add_node(node("only", 2));

    cluster.put("a", "1");
    cluster.put("b", "2");
    ASSERT_TRUE(cluster.get("a").has_value());  // "a" becomes most recently used

    cluster.put("c", "3");
    EXPECT_TRUE(cluster.get("a").has_value());
    EXPECT_FALSE(cluster.get("b").has_value());
}

TEST(CacheCluster, ListNodesReportsCapacityAlongsideUsage) {
    auto cluster = make_cluster();
    cluster.add_node(node("only", 50));
    cluster.put("a", "1");

    const auto listed = cluster.list_nodes();
    ASSERT_EQ(listed.size(), 1u);
    EXPECT_EQ(listed.front().id, "only");
    EXPECT_EQ(listed.front().capacity, 50u);
    EXPECT_EQ(listed.front().size, 1u);
}

// size is read back from the cache on every call, so a stored Node can never go
// stale, and whatever a caller puts in the field on the way in is discarded.
TEST(CacheCluster, ListedSizeTracksTheCacheAndIgnoresCallerInput) {
    auto cluster = make_cluster();
    cluster.add_node(NodeSpec{.id = "only", .capacity = 50, .policy_name = "lru", .size = 999});
    EXPECT_EQ(cluster.list_nodes().front().size, 0u);

    cluster.put("a", "1");
    cluster.put("b", "2");
    EXPECT_EQ(cluster.list_nodes().front().size, 2u);

    cluster.clear_data();
    EXPECT_EQ(cluster.list_nodes().front().size, 0u);
}

// --- events -----------------------------------------------------------------

TEST(CacheCluster, PutEmitsAnInsertAndARouterEvent) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));

    std::vector<Event> events;
    cluster.set_event_handler([&events](Event e) { events.push_back(std::move(e)); });

    cluster.put("alpha", "one");

    ASSERT_EQ(events.size(), 2u);
    const auto* insert = std::get_if<CacheEvent>(&events[0].event);
    ASSERT_NE(insert, nullptr);
    EXPECT_EQ(insert->type, CacheEventType::Insert);
    EXPECT_EQ(insert->key, "alpha");

    const auto* routed = std::get_if<RouterEvent>(&events[1].event);
    ASSERT_NE(routed, nullptr);
    EXPECT_EQ(routed->key, "alpha");
    EXPECT_EQ(routed->node, "node-A");
    EXPECT_EQ(routed->router_name, "consistent");
}

TEST(CacheCluster, RewritingAKeyEmitsUpdateRatherThanInsert) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));
    cluster.put("alpha", "one");

    std::vector<CacheEventType> types;
    cluster.set_event_handler([&types](Event e) {
        if (const auto* c = std::get_if<CacheEvent>(&e.event)) types.push_back(c->type);
    });

    cluster.put("alpha", "two");
    EXPECT_EQ(types, (std::vector<CacheEventType>{CacheEventType::Update}));
}

TEST(CacheCluster, GetEmitsHitOrMissAndNoRouterEvent) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));
    cluster.put("alpha", "one");

    std::vector<Event> events;
    cluster.set_event_handler([&events](Event e) { events.push_back(std::move(e)); });

    cluster.get("alpha");
    cluster.get("absent");

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(std::get<CacheEvent>(events[0].event).type, CacheEventType::Hit);
    EXPECT_EQ(std::get<CacheEvent>(events[1].event).type, CacheEventType::Miss);
    for (const auto& e : events) {
        EXPECT_FALSE(std::holds_alternative<RouterEvent>(e.event));
    }
}

TEST(CacheCluster, EvictionIsReported) {
    auto cluster = make_cluster();
    cluster.add_node(node("only", 1));

    std::vector<std::string> evicted;
    cluster.set_event_handler([&evicted](Event e) {
        if (const auto* c = std::get_if<CacheEvent>(&e.event)) {
            if (c->type == CacheEventType::Evict) evicted.push_back(c->key);
        }
    });

    cluster.put("a", "1");
    cluster.put("b", "2");

    EXPECT_EQ(evicted, (std::vector<std::string>{"a"}));
}

// --- switching routers ------------------------------------------------------

TEST(CacheCluster, SwitchingRoutersCarriesTheTopologyOver) {
    auto cluster = make_cluster("consistent");
    cluster.add_node(node("node-A"));
    cluster.add_node(node("node-B"));

    cluster.set_router(cacherouter::router::make_router("simple"));

    EXPECT_EQ(cluster.router().name(), "simple");
    EXPECT_EQ(cluster.router().nodes().size(), 2u);
    EXPECT_EQ(sorted_ids(cluster), (std::vector<NodeId>{"node-A", "node-B"}));
}

// Regression: nodes_ is unordered, and SimpleRouter derives bucket index from
// insertion order, so an unsorted replay reshuffled buckets on every swap.
TEST(CacheCluster, ReplayIntoTheNewRouterIsOrderStable) {
    auto cluster = make_cluster("consistent");
    for (const auto* id : {"node-D", "node-B", "node-A", "node-C"}) cluster.add_node(node(id));

    cluster.set_router(cacherouter::router::make_router("simple"));
    const auto first = cluster.router().nodes();
    EXPECT_EQ(first, (std::vector<NodeId>{"node-A", "node-B", "node-C", "node-D"}));

    cluster.set_router(cacherouter::router::make_router("consistent"));
    cluster.set_router(cacherouter::router::make_router("simple"));
    EXPECT_EQ(cluster.router().nodes(), first) << "bucket order drifted across swaps";
}

TEST(CacheCluster, SwitchingRoutersPreservesPerNodeVirtualNodeCounts) {
    auto cluster = make_cluster("consistent");
    cluster.add_node(
        NodeSpec{.id = "small", .capacity = 10, .policy_name = "lru", .virtual_nodes = 10});
    cluster.add_node(
        NodeSpec{.id = "big", .capacity = 10, .policy_name = "lru", .virtual_nodes = 300});

    cluster.set_router(cacherouter::router::make_router("simple"));
    cluster.set_router(cacherouter::router::make_router("consistent"));

    const auto* consistent =
        dynamic_cast<const cacherouter::router::ConsistentRouter*>(&cluster.router());
    ASSERT_NE(consistent, nullptr);
    EXPECT_EQ(consistent->ring().size(), 11u + 301u);
}

TEST(CacheCluster, RoutingUsesTheNewRouterAfterASwitch) {
    auto cluster = make_cluster("consistent");
    cluster.add_node(node("node-A"));

    cluster.set_router(cacherouter::router::make_router("simple"));

    std::vector<Event> events;
    cluster.set_event_handler([&events](Event e) { events.push_back(std::move(e)); });
    cluster.put("alpha", "one");

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(std::get<RouterEvent>(events[1].event).router_name, "simple");
}

// --- clearing and resetting -------------------------------------------------

TEST(CacheCluster, ClearDataDropsEntriesButKeepsNodes) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));
    cluster.add_node(node("node-B"));
    for (int i = 0; i < 50; i++) cluster.put("key-" + std::to_string(i), "v");
    ASSERT_EQ(total_used(cluster), 50u);

    cluster.clear_data();

    EXPECT_EQ(total_used(cluster), 0u);
    EXPECT_EQ(sorted_ids(cluster), (std::vector<NodeId>{"node-A", "node-B"}));
    EXPECT_EQ(cluster.router().nodes().size(), 2u);
    EXPECT_FALSE(cluster.get("key-0").has_value());
}

TEST(CacheCluster, ClearDataResetsTheEvictionPolicyToo) {
    auto cluster = make_cluster();
    cluster.add_node(node("only", 2));
    cluster.put("a", "1");
    cluster.put("b", "2");

    cluster.clear_data();
    cluster.put("c", "3");

    // A policy still holding a/b would nominate a victim that is no longer
    // present, and the fresh entry could be dropped or miscounted.
    EXPECT_EQ(used(cluster, "only"), 1u);
    EXPECT_TRUE(cluster.get("c").has_value());
}

TEST(CacheCluster, ClearDataKeepsTheEventHandlerWired) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));

    std::vector<Event> events;
    cluster.set_event_handler([&events](Event e) { events.push_back(std::move(e)); });

    cluster.clear_data();  // rebuilds each Cache, so handlers must be reattached
    cluster.put("alpha", "one");

    EXPECT_EQ(events.size(), 2u);
}

TEST(CacheCluster, ResetRemovesEveryNode) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));
    cluster.add_node(node("node-B"));
    cluster.put("alpha", "one");

    cluster.clear_nodes();

    EXPECT_TRUE(cluster.list_nodes().empty());
    EXPECT_TRUE(cluster.router().nodes().empty());
}

TEST(CacheCluster, AClusterIsUsableAgainAfterReset) {
    auto cluster = make_cluster();
    cluster.add_node(node("node-A"));
    cluster.put("alpha", "one");

    cluster.clear_nodes();
    cluster.add_node(node("node-B"));
    cluster.put("beta", "two");

    EXPECT_EQ(cluster.get("beta"), std::optional<std::string>{"two"});
    EXPECT_FALSE(cluster.get("alpha").has_value());
    EXPECT_EQ(sorted_ids(cluster), (std::vector<NodeId>{"node-B"}));
}

}  // namespace
