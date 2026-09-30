#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "cacherouter/policy/lru_policy.hpp"
#include "cacherouter/policy/policy_factory.hpp"

namespace {

using cacherouter::policy::LruPolicy;

TEST(LruPolicy, EmptyPolicyHasNoVictim) {
    LruPolicy policy;
    EXPECT_FALSE(policy.victim().has_value());
}

TEST(LruPolicy, EvictsTheLeastRecentlyInserted) {
    LruPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");
    policy.on_insert("c");

    ASSERT_TRUE(policy.victim().has_value());
    EXPECT_EQ(*policy.victim(), "a");
}

TEST(LruPolicy, AccessPromotesAKeyOutOfEvictionRange) {
    LruPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");
    policy.on_insert("c");

    policy.on_access("a");  // "a" becomes the most recently used
    EXPECT_EQ(*policy.victim(), "b");
}

TEST(LruPolicy, VictimIsStableUntilTheCacheActs) {
    LruPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");

    // victim() only reports; it must not consume.
    EXPECT_EQ(*policy.victim(), "a");
    EXPECT_EQ(*policy.victim(), "a");
}

TEST(LruPolicy, RemoveTakesAKeyOutOfConsideration) {
    LruPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");

    policy.on_remove("a");
    EXPECT_EQ(*policy.victim(), "b");

    policy.on_remove("b");
    EXPECT_FALSE(policy.victim().has_value());
}

TEST(LruPolicy, RemovingAnUnknownKeyIsANoop) {
    LruPolicy policy;
    policy.on_insert("a");

    policy.on_remove("never-inserted");
    EXPECT_EQ(*policy.victim(), "a");
}

TEST(LruPolicy, ReinsertingAKeyDoesNotLeaveADuplicate) {
    LruPolicy policy;
    policy.on_insert("a");
    policy.on_insert("b");
    policy.on_insert("a");  // touch() must relocate, not append a second entry

    EXPECT_EQ(*policy.victim(), "b");
    policy.on_remove("b");
    EXPECT_EQ(*policy.victim(), "a");

    // A stale duplicate would surface here as a lingering victim.
    policy.on_remove("a");
    EXPECT_FALSE(policy.victim().has_value());
}

TEST(LruPolicy, ReportsItsName) {
    LruPolicy policy;
    EXPECT_EQ(policy.name(), "lru");
}

TEST(PolicyFactory, BuildsAnLruPolicy) {
    const auto policy = cacherouter::make_policy("lru");
    ASSERT_NE(policy, nullptr);
    EXPECT_EQ(policy->name(), "lru");
}

TEST(PolicyFactory, RejectsAnUnknownPolicyName) {
    EXPECT_THROW(cacherouter::make_policy("mru"), std::invalid_argument);
    EXPECT_THROW(cacherouter::make_policy(""), std::invalid_argument);
}

}  // namespace
