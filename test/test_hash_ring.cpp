#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "cacherouter/router/hash_ring.hpp"
#include "cacherouter/utils/hash.hpp"

namespace {

using cacherouter::router::HashRing;

constexpr int kVnodes = 100;
// A node contributes one ring point per virtual node, plus one for its own name.
constexpr std::size_t kPointsPerNode = kVnodes + 1;

TEST(Hash, IsStableWithinAProcess) {
    EXPECT_EQ(cacherouter::hash("alpha"), cacherouter::hash("alpha"));
}

TEST(Hash, DistinguishesKeys) {
    EXPECT_NE(cacherouter::hash("alpha"), cacherouter::hash("beta"));
    EXPECT_NE(cacherouter::hash("a"), cacherouter::hash("A"));
}

TEST(Hash, AcceptsAnEmptyKey) {
    EXPECT_EQ(cacherouter::hash(""), cacherouter::hash(""));
}

TEST(HashRing, EmptyRingFindsNothing) {
    const HashRing ring(kVnodes);
    EXPECT_FALSE(ring.find_node("any-key").has_value());
    EXPECT_TRUE(ring.nodes().empty());
    EXPECT_TRUE(ring.get_ring().empty());
}

TEST(HashRing, AddingANodePlacesItsPrimaryPointAndReplicas) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", kVnodes);

    EXPECT_EQ(ring.get_ring().size(), kPointsPerNode);
    EXPECT_EQ(ring.nodes(), (std::vector<std::string>{"node-A"}));
}

TEST(HashRing, AddingTheSameNodeTwiceChangesNothing) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", kVnodes);
    ring.add_node("node-A", kVnodes);

    EXPECT_EQ(ring.get_ring().size(), kPointsPerNode);
    EXPECT_EQ(ring.nodes().size(), 1u);
}

TEST(HashRing, NodesMayCarryDifferentReplicaCounts) {
    HashRing ring(kVnodes);
    ring.add_node("light", 10);
    ring.add_node("heavy", 300);

    EXPECT_EQ(ring.get_ring().size(), 11u + 301u);
}

TEST(HashRing, ANonPositiveReplicaCountFallsBackToTheRingDefault) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", 0);
    EXPECT_EQ(ring.get_ring().size(), kPointsPerNode);

    HashRing negative(kVnodes);
    negative.add_node("node-A", -5);
    EXPECT_EQ(negative.get_ring().size(), kPointsPerNode);
}

TEST(HashRing, NodesAreReportedUniqueAndSorted) {
    HashRing ring(kVnodes);
    ring.add_node("node-C", 10);
    ring.add_node("node-A", 10);
    ring.add_node("node-B", 10);

    EXPECT_EQ(ring.nodes(), (std::vector<std::string>{"node-A", "node-B", "node-C"}));
}

// Regression: removal used to erase a fixed number of replicas, so a node added
// with a custom count left the difference behind — ring points still naming a
// node that no longer existed, which route() would happily hand back.
TEST(HashRing, RemovingANodeErasesEveryOneOfItsReplicas) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", kVnodes);
    const auto baseline = ring.get_ring().size();

    ring.add_node("node-B", 300);
    ASSERT_EQ(ring.get_ring().size(), baseline + 301);

    ring.remove_node("node-B");
    EXPECT_EQ(ring.get_ring().size(), baseline);
    EXPECT_EQ(ring.nodes(), (std::vector<std::string>{"node-A"}));
    for (const auto& [point, node] : ring.get_ring()) {
        EXPECT_EQ(node, "node-A") << "orphaned replica left at ring point " << point;
    }
}

TEST(HashRing, RepeatedAddRemoveCyclesDoNotAccumulateOrphans) {
    HashRing ring(kVnodes);
    for (int cycle = 0; cycle < 3; cycle++) {
        ring.add_node("node-A", 250);
        EXPECT_EQ(ring.get_ring().size(), 251u);
        ring.remove_node("node-A");
        EXPECT_TRUE(ring.get_ring().empty());
    }
}

TEST(HashRing, RemovingAnUnknownNodeIsANoop) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", kVnodes);

    ring.remove_node("node-Z");
    EXPECT_EQ(ring.get_ring().size(), kPointsPerNode);
    EXPECT_EQ(ring.nodes(), (std::vector<std::string>{"node-A"}));
}

TEST(HashRing, RemovingTheLastNodeEmptiesTheRing) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", kVnodes);
    ring.remove_node("node-A");

    EXPECT_TRUE(ring.get_ring().empty());
    EXPECT_FALSE(ring.find_node("any-key").has_value());
}

TEST(HashRing, LookupIsDeterministic) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", kVnodes);
    ring.add_node("node-B", kVnodes);

    for (int i = 0; i < 200; i++) {
        const auto key = "key-" + std::to_string(i);
        EXPECT_EQ(ring.find_node(key), ring.find_node(key));
    }
}

TEST(HashRing, AKeyLandsOnTheFirstNodeClockwiseFromItsHash) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", kVnodes);
    ring.add_node("node-B", kVnodes);

    const auto& points = ring.get_ring();
    for (int i = 0; i < 300; i++) {
        const auto key = "key-" + std::to_string(i);
        const auto it = points.lower_bound(cacherouter::hash(key));
        const auto expected = it != points.end() ? it->second : points.begin()->second;
        EXPECT_EQ(ring.find_node(key), expected) << "key " << key;
    }
}

TEST(HashRing, AKeyPastTheLastPointWrapsToTheFirst) {
    HashRing ring(50);
    ring.add_node("node-A", 50);
    ring.add_node("node-B", 50);

    const auto last_point = ring.get_ring().rbegin()->first;
    const auto& first_node = ring.get_ring().begin()->second;

    // Keys hashing above every ring point are the wrap-around case; find one
    // rather than assume, so the assertion is about a key that really wraps.
    bool exercised = false;
    for (int i = 0; i < 100000 && !exercised; i++) {
        const auto key = "wrap-" + std::to_string(i);
        if (cacherouter::hash(key) > last_point) {
            EXPECT_EQ(ring.find_node(key), first_node) << "key " << key;
            exercised = true;
        }
    }
    EXPECT_TRUE(exercised) << "no probe key hashed above the ring's last point";
}

TEST(HashRing, ReplicasSpreadKeysAcrossNodes) {
    HashRing ring(kVnodes);
    ring.add_node("node-A", kVnodes);
    ring.add_node("node-B", kVnodes);

    std::size_t on_a = 0;
    constexpr int kKeys = 2000;
    for (int i = 0; i < kKeys; i++) {
        if (*ring.find_node("key-" + std::to_string(i)) == "node-A") on_a++;
    }

    // 100 replicas each is loose enough to wander, but a 30/70 split or worse
    // means the replicas are not doing their job.
    EXPECT_GT(on_a, kKeys * 0.3);
    EXPECT_LT(on_a, kKeys * 0.7);
}

}  // namespace
