#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "cacherouter/router/consistent_router.hpp"
#include "cacherouter/router/router.hpp"
#include "cacherouter/router/router_factory.hpp"
#include "cacherouter/router/simple_router.hpp"

namespace {

using cacherouter::NodeId;
using cacherouter::router::ConsistentRouter;
using cacherouter::router::Router;
using cacherouter::router::SimpleRouter;

constexpr int kVnodes = 100;

TEST(ConsistentRouter, ReportsItsName) {
    const ConsistentRouter router;
    EXPECT_EQ(router.name(), "consistent");
}

TEST(ConsistentRouter, StartsEmpty) {
    const ConsistentRouter router;
    EXPECT_TRUE(router.nodes().empty());
    EXPECT_TRUE(router.ring().empty());
}

TEST(ConsistentRouter, RoutingWithNoNodesThrows) {
    const ConsistentRouter router;
    EXPECT_THROW(router.route("any-key"), std::runtime_error);
}

TEST(ConsistentRouter, RoutesEveryKeyToSomeAddedNode) {
    ConsistentRouter router(kVnodes);
    router.add_node("node-A", kVnodes);
    router.add_node("node-B", kVnodes);

    for (int i = 0; i < 300; i++) {
        const auto node = router.route("key-" + std::to_string(i));
        EXPECT_TRUE(node == "node-A" || node == "node-B") << "unexpected node " << node;
    }
}

TEST(ConsistentRouter, RoutingIsDeterministic) {
    ConsistentRouter router(kVnodes);
    router.add_node("node-A", kVnodes);
    router.add_node("node-B", kVnodes);

    for (int i = 0; i < 200; i++) {
        const auto key = "key-" + std::to_string(i);
        EXPECT_EQ(router.route(key), router.route(key));
    }
}

TEST(ConsistentRouter, NodesAreReportedSortedAndUnique) {
    ConsistentRouter router(kVnodes);
    router.add_node("node-C", kVnodes);
    router.add_node("node-A", kVnodes);
    router.add_node("node-A", kVnodes);

    EXPECT_EQ(router.nodes(), (std::vector<NodeId>{"node-A", "node-C"}));
}

TEST(ConsistentRouter, RingExposesOnePointPerReplicaPlusThePrimary) {
    ConsistentRouter router(kVnodes);
    router.add_node("node-A", kVnodes);
    EXPECT_EQ(router.ring().size(), static_cast<std::size_t>(kVnodes) + 1);

    router.add_node("node-B", 10);
    EXPECT_EQ(router.ring().size(), static_cast<std::size_t>(kVnodes) + 1 + 11);
}

TEST(ConsistentRouter, RemovingANodeLeavesNoTraceInTheRing) {
    ConsistentRouter router(kVnodes);
    router.add_node("node-A", kVnodes);
    router.add_node("node-B", 300);

    router.remove_node("node-B");

    EXPECT_EQ(router.nodes(), (std::vector<NodeId>{"node-A"}));
    for (const auto& [point, node] : router.ring()) {
        EXPECT_EQ(node, "node-A") << "orphaned replica at ring point " << point;
    }
    for (int i = 0; i < 200; i++) {
        EXPECT_EQ(router.route("key-" + std::to_string(i)), "node-A");
    }
}

TEST(ConsistentRouter, DistributesKeysAcrossNodes) {
    ConsistentRouter router(kVnodes);
    for (const auto* id : {"node-A", "node-B", "node-C"}) router.add_node(id, kVnodes);

    std::vector<NodeId> counted;
    constexpr int kKeys = 3000;
    std::size_t a = 0, b = 0, c = 0;
    for (int i = 0; i < kKeys; i++) {
        const auto node = router.route("key-" + std::to_string(i));
        if (node == "node-A") a++;
        else if (node == "node-B") b++;
        else c++;
    }

    // Virtual nodes exist to stop any single node owning a huge arc of the ring.
    for (const auto share : {a, b, c}) {
        EXPECT_GT(share, kKeys * 0.15) << "a node is starved of keys";
        EXPECT_LT(share, kKeys * 0.55) << "a node owns too much of the ring";
    }
}

// The property the whole design exists for. Adding a node should disturb roughly
// 1/N of the keyspace under consistent hashing, but re-derive nearly all of it
// under a modulo router, because every bucket boundary moves at once.
TEST(RouterComparison, AddingANodeRemapsFarFewerKeysUnderConsistentHashing) {
    constexpr int kKeys = 3000;
    std::vector<std::string> keys;
    keys.reserve(kKeys);
    for (int i = 0; i < kKeys; i++) keys.push_back("key-" + std::to_string(i));

    const auto churn_from_adding_a_fourth_node = [&keys](Router& router) {
        for (const auto* id : {"node-A", "node-B", "node-C"}) router.add_node(id, kVnodes);

        std::vector<NodeId> before;
        before.reserve(keys.size());
        for (const auto& key : keys) before.push_back(router.route(key));

        router.add_node("node-D", kVnodes);

        std::size_t moved = 0;
        for (std::size_t i = 0; i < keys.size(); i++) {
            if (router.route(keys[i]) != before[i]) moved++;
        }
        return static_cast<double>(moved) / static_cast<double>(keys.size());
    };

    SimpleRouter simple;
    ConsistentRouter consistent(kVnodes);
    const double simple_churn = churn_from_adding_a_fourth_node(simple);
    const double consistent_churn = churn_from_adding_a_fourth_node(consistent);

    // Ideal is 0.25 for consistent hashing and ~0.75 for modulo; the bounds are
    // loose enough to absorb hash variance but far apart enough to be meaningful.
    EXPECT_LT(consistent_churn, 0.40) << "consistent churn was " << consistent_churn;
    EXPECT_GT(simple_churn, 0.50) << "simple churn was " << simple_churn;
    EXPECT_LT(consistent_churn, simple_churn);
}

TEST(RouterFactory, BuildsBothRouters) {
    const auto simple = cacherouter::router::make_router("simple");
    ASSERT_NE(simple, nullptr);
    EXPECT_EQ(simple->name(), "simple");

    const auto consistent = cacherouter::router::make_router("consistent");
    ASSERT_NE(consistent, nullptr);
    EXPECT_EQ(consistent->name(), "consistent");
}

TEST(RouterFactory, HonoursTheVirtualNodeCount) {
    const auto router = cacherouter::router::make_router("consistent", 7);
    router->add_node("node-A", 0);  // 0 falls back to the ring's own default

    const auto* consistent = dynamic_cast<const ConsistentRouter*>(router.get());
    ASSERT_NE(consistent, nullptr);
    EXPECT_EQ(consistent->ring().size(), 8u);
}

TEST(RouterFactory, RejectsAnUnknownRouterName) {
    EXPECT_THROW(cacherouter::router::make_router("bogus"), std::invalid_argument);
}

TEST(RouterFactory, AdvertisesTheRoutersItCanBuild) {
    for (const auto& name : cacherouter::router::available_routers()) {
        EXPECT_NO_THROW(cacherouter::router::make_router(name)) << name;
    }
}

}  // namespace
