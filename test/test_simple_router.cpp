#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "cacherouter/router/simple_router.hpp"
#include "cacherouter/utils/hash.hpp"

namespace {

using cacherouter::NodeId;
using cacherouter::router::SimpleRouter;

TEST(SimpleRouter, ReportsItsName) {
    const SimpleRouter router;
    EXPECT_EQ(router.name(), "simple");
}

TEST(SimpleRouter, StartsEmpty) {
    const SimpleRouter router;
    EXPECT_TRUE(router.nodes().empty());
}

// Guarded rather than left to `hash % 0`: resetting the cluster makes an empty
// router reachable, and the guard keeps that a throw instead of undefined behaviour.
TEST(SimpleRouter, RoutingWithNoNodesThrows) {
    const SimpleRouter router;
    EXPECT_THROW(router.route("any-key"), std::runtime_error);
}

TEST(SimpleRouter, RoutesByTakingTheHashModuloTheNodeCount) {
    SimpleRouter router;
    router.add_node("node-A");
    router.add_node("node-B");
    router.add_node("node-C");

    const auto buckets = router.nodes();
    ASSERT_EQ(buckets.size(), 3u);

    for (int i = 0; i < 500; i++) {
        const auto key = "key-" + std::to_string(i);
        const auto expected = buckets[cacherouter::hash(key) % buckets.size()];
        EXPECT_EQ(router.route(key), expected) << "key " << key;
    }
}

TEST(SimpleRouter, RoutingIsDeterministic) {
    SimpleRouter router;
    router.add_node("node-A");
    router.add_node("node-B");

    for (int i = 0; i < 200; i++) {
        const auto key = "key-" + std::to_string(i);
        EXPECT_EQ(router.route(key), router.route(key));
    }
}

TEST(SimpleRouter, BucketOrderFollowsInsertionOrder) {
    SimpleRouter router;
    router.add_node("node-C");
    router.add_node("node-A");
    router.add_node("node-B");

    // Bucket index is positional, so the order the nodes went in is the mapping.
    EXPECT_EQ(router.nodes(), (std::vector<NodeId>{"node-C", "node-A", "node-B"}));
}

TEST(SimpleRouter, AddingTheSameNodeTwiceChangesNothing) {
    SimpleRouter router;
    router.add_node("node-A");
    router.add_node("node-A");

    EXPECT_EQ(router.nodes(), (std::vector<NodeId>{"node-A"}));
}

TEST(SimpleRouter, IgnoresTheVirtualNodeCount) {
    SimpleRouter router;
    router.add_node("node-A", 300);

    // There are no replicas in a modulo router; one node is exactly one bucket.
    EXPECT_EQ(router.nodes(), (std::vector<NodeId>{"node-A"}));
}

TEST(SimpleRouter, SingleNodeTakesEveryKey) {
    SimpleRouter router;
    router.add_node("only");

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(router.route("key-" + std::to_string(i)), "only");
    }
}

TEST(SimpleRouter, RemovingANodeDropsIt) {
    SimpleRouter router;
    router.add_node("node-A");
    router.add_node("node-B");

    router.remove_node("node-A");
    const auto remaining = router.nodes();
    EXPECT_EQ(remaining, (std::vector<NodeId>{"node-B"}));

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(router.route("key-" + std::to_string(i)), "node-B");
    }
}

TEST(SimpleRouter, RemovingAnUnknownNodeIsANoop) {
    SimpleRouter router;
    router.add_node("node-A");

    router.remove_node("node-Z");
    EXPECT_EQ(router.nodes(), (std::vector<NodeId>{"node-A"}));
}

TEST(SimpleRouter, RemovingTheLastNodeMakesRoutingThrowAgain) {
    SimpleRouter router;
    router.add_node("node-A");
    router.remove_node("node-A");

    EXPECT_TRUE(router.nodes().empty());
    EXPECT_THROW(router.route("any-key"), std::runtime_error);
}

// Documents a real consequence of the swap-and-pop removal: the surviving nodes
// can change bucket index, so removal reshuffles placement beyond the lost node.
TEST(SimpleRouter, RemovalMovesTheLastNodeIntoTheGap) {
    SimpleRouter router;
    router.add_node("node-A");
    router.add_node("node-B");
    router.add_node("node-C");

    router.remove_node("node-A");
    EXPECT_EQ(router.nodes(), (std::vector<NodeId>{"node-C", "node-B"}));
}

TEST(SimpleRouter, SpreadsKeysAcrossBuckets) {
    SimpleRouter router;
    router.add_node("node-A");
    router.add_node("node-B");

    std::size_t on_a = 0;
    constexpr int kKeys = 2000;
    for (int i = 0; i < kKeys; i++) {
        if (router.route("key-" + std::to_string(i)) == "node-A") on_a++;
    }

    // A modulo split of a good hash should be close to even.
    EXPECT_GT(on_a, kKeys * 0.4);
    EXPECT_LT(on_a, kKeys * 0.6);
}

}  // namespace
