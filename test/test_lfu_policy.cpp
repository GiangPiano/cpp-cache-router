#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cacherouter/cluster.hpp"
#include "cacherouter/node.hpp"
#include "cacherouter/policy/lfu_policy.hpp"
#include "cacherouter/policy/policy_factory.hpp"
#include "cacherouter/router/router_factory.hpp"

namespace {

using cacherouter::policy::LfuPolicy;

TEST(LfuPolicy, EmptyPolicyHasNoVictim) {
    const LfuPolicy policy;
    EXPECT_FALSE(policy.victim().has_value());
}

TEST(LfuPolicy, ReportsItsName) {
    const LfuPolicy policy;
    EXPECT_EQ(policy.name(), "lfu");
}

// Regression: the first insert used to erase with a default-constructed list
// iterator, which segfaulted before any of the logic below could run.
TEST(LfuPolicy, InsertingASingleKeyIsTheVictim) {
    LfuPolicy policy;
    policy.on_insert("a");

    ASSERT_TRUE(policy.victim().has_value());
    EXPECT_EQ(*policy.victim(), "a");
}

TEST(LfuPolicy, EvictsTheLeastFrequentlyUsed) {
    LfuPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");
    policy.on_insert("c");

    policy.on_access("a");  // a -> 2
    policy.on_access("a");  // a -> 3
    policy.on_access("b");  // b -> 2, c stays at 1

    EXPECT_EQ(*policy.victim(), "c");
}

TEST(LfuPolicy, FrequencyBeatsRecency) {
    LfuPolicy policy;
    policy.on_insert("hot");
    policy.on_insert("cold");

    for (int i = 0; i < 5; i++) policy.on_access("hot");

    // "hot" was touched most recently but is also the most frequent, so an LRU
    // would evict it here and an LFU must not.
    EXPECT_EQ(*policy.victim(), "cold");
}

TEST(LfuPolicy, TiesWithinAFrequencyBreakByAge) {
    LfuPolicy policy;
    policy.on_insert("first");
    policy.on_insert("second");
    policy.on_insert("third");

    // All at frequency 1, so the oldest insertion goes first.
    EXPECT_EQ(*policy.victim(), "first");
}

TEST(LfuPolicy, PromotionRetiesByAgeAtTheNewFrequency) {
    LfuPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");
    policy.on_insert("c");

    policy.on_access("a");  // a -> 2
    policy.on_access("b");  // b -> 2, so bucket 2 holds b then a by recency
    EXPECT_EQ(*policy.victim(), "c");

    policy.on_remove("c");
    // Only bucket 2 remains; "a" was promoted first, so it is the older of the two.
    EXPECT_EQ(*policy.victim(), "a");
}

TEST(LfuPolicy, MinimumFrequencyAdvancesWhenTheLowestBucketEmpties) {
    LfuPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");

    policy.on_access("a");
    policy.on_access("b");  // both now at 2, bucket 1 is empty

    ASSERT_TRUE(policy.victim().has_value());
    EXPECT_EQ(*policy.victim(), "a");

    policy.on_access("a");  // a -> 3, leaving b alone at 2
    EXPECT_EQ(*policy.victim(), "b");
}

TEST(LfuPolicy, RemoveTakesAKeyOutOfConsideration) {
    LfuPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");

    policy.on_remove("a");
    EXPECT_EQ(*policy.victim(), "b");

    policy.on_remove("b");
    EXPECT_FALSE(policy.victim().has_value());
}

TEST(LfuPolicy, RemovingAnUnknownKeyIsANoop) {
    LfuPolicy policy;
    policy.on_insert("a");

    policy.on_remove("never-inserted");
    ASSERT_TRUE(policy.victim().has_value());
    EXPECT_EQ(*policy.victim(), "a");
}

TEST(LfuPolicy, RemovingTheMinimumBucketAdvancesTheCursor) {
    LfuPolicy policy;
    policy.on_insert("low");
    policy.on_insert("high");
    policy.on_access("high");  // high -> 2, low alone at 1

    policy.on_remove("low");  // the minimum bucket empties and disappears
    ASSERT_TRUE(policy.victim().has_value());
    EXPECT_EQ(*policy.victim(), "high");
}

TEST(LfuPolicy, IsReusableAfterEmptying) {
    LfuPolicy policy;
    policy.on_insert("a");
    policy.on_access("a");
    policy.on_access("a");  // a sits at frequency 3
    policy.on_remove("a");
    ASSERT_FALSE(policy.victim().has_value());

    // A fresh key must start at frequency 1 again, not inherit the old cursor.
    policy.on_insert("b");
    policy.on_insert("c");
    EXPECT_EQ(*policy.victim(), "b");
}

TEST(LfuPolicy, AccessingAnUntrackedKeyStartsTrackingIt) {
    LfuPolicy policy;
    policy.on_access("a");  // no prior insert

    ASSERT_TRUE(policy.victim().has_value());
    EXPECT_EQ(*policy.victim(), "a");
}

TEST(LfuPolicy, VictimIsStableUntilTheCacheActs) {
    LfuPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");

    EXPECT_EQ(*policy.victim(), "a");
    EXPECT_EQ(*policy.victim(), "a");
}

TEST(PolicyFactory, BuildsAnLfuPolicy) {
    const auto policy = cacherouter::make_policy("lfu");
    ASSERT_NE(policy, nullptr);
    EXPECT_EQ(policy->name(), "lfu");
}

// End to end: a node configured with lfu must keep the hot key and drop the
// one-shot keys, which is exactly where it differs from lru.
TEST(LfuPolicy, ClusterWithLfuKeepsTheHotKey) {
    auto cluster = cacherouter::CacheCluster{cacherouter::router::make_router("consistent")};
    cluster.add_node({.id = "only", .capacity = 2, .policy_name = "lfu"});

    cluster.put("hot", "1");
    for (int i = 0; i < 5; i++) cluster.get("hot");  // drive its frequency up

    cluster.put("cold-a", "2");
    cluster.put("cold-b", "3");  // capacity 2 exceeded, something must go

    EXPECT_TRUE(cluster.get("hot").has_value()) << "the most frequent key was evicted";
}

}  // namespace
