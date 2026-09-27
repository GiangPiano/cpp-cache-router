#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "cacherouter/cluster.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/logging/event_log.hpp"
#include "cacherouter/router/router_factory.hpp"

namespace {

using cacherouter::CacheEvent;
using cacherouter::CacheEventType;
using cacherouter::Event;
using cacherouter::RouterEvent;
using cacherouter::logging::EventLog;
using cacherouter::logging::Level;

Event cache_event(CacheEventType type, std::string key, std::string node = "node-A") {
    return Event{
        .timestamp = std::chrono::system_clock::now(),
        .event = CacheEvent{.type = type, .key = std::move(key), .node_id = std::move(node)}};
}

Event router_event(std::string key, std::string node, std::string router) {
    return Event{
        .timestamp = std::chrono::system_clock::now(),
        .event = RouterEvent{
            .key = std::move(key), .node = std::move(node), .router_name = std::move(router)}};
}

std::vector<std::string> lines(const std::ostringstream& out) {
    std::vector<std::string> result;
    std::istringstream in(out.str());
    for (std::string line; std::getline(in, line);) result.push_back(line);
    return result;
}

// --- level names ------------------------------------------------------------

TEST(LogLevel, NamesRoundTrip) {
    for (const auto level : {Level::Trace, Level::Debug, Level::Info, Level::Off}) {
        const auto name = cacherouter::logging::level_to_name(level);
        const auto parsed = cacherouter::logging::name_to_level(name);
        ASSERT_TRUE(parsed.has_value()) << name;
        EXPECT_EQ(*parsed, level) << name;
    }
}

TEST(LogLevel, ParsingIsCaseInsensitive) {
    EXPECT_EQ(cacherouter::logging::name_to_level("TRACE"), Level::Trace);
    EXPECT_EQ(cacherouter::logging::name_to_level("Debug"), Level::Debug);
    EXPECT_EQ(cacherouter::logging::name_to_level("iNfO"), Level::Info);
}

TEST(LogLevel, UnknownNamesAreRejectedRatherThanDefaulted) {
    EXPECT_FALSE(cacherouter::logging::name_to_level("verbose").has_value());
    EXPECT_FALSE(cacherouter::logging::name_to_level("").has_value());
}

// --- severity mapping -------------------------------------------------------

TEST(LogLevel, SeverityFollowsEventVolume) {
    using cacherouter::logging::level_of;
    EXPECT_EQ(level_of(cache_event(CacheEventType::Hit, "k")), Level::Trace);
    EXPECT_EQ(level_of(router_event("k", "node-A", "consistent")), Level::Trace);
    EXPECT_EQ(level_of(cache_event(CacheEventType::Miss, "k")), Level::Debug);
    EXPECT_EQ(level_of(cache_event(CacheEventType::Insert, "k")), Level::Debug);
    EXPECT_EQ(level_of(cache_event(CacheEventType::Update, "k")), Level::Debug);
    // An eviction means a node ran out of room, which is worth seeing by default.
    EXPECT_EQ(level_of(cache_event(CacheEventType::Evict, "k")), Level::Info);
}

// --- formatting -------------------------------------------------------------

TEST(LogFormat, CacheEventCarriesActionNodeAndKey) {
    const auto line = cacherouter::logging::format(cache_event(CacheEventType::Evict, "item:42"));

    EXPECT_NE(line.find("INFO"), std::string::npos) << line;
    EXPECT_NE(line.find("evict"), std::string::npos) << line;
    EXPECT_NE(line.find("node=node-A"), std::string::npos) << line;
    EXPECT_NE(line.find("key=\"item:42\""), std::string::npos) << line;
}

TEST(LogFormat, RouterEventNamesTheRouter) {
    const auto line = cacherouter::logging::format(router_event("item:7", "node-B", "simple"));

    EXPECT_NE(line.find("route"), std::string::npos) << line;
    EXPECT_NE(line.find("node=node-B"), std::string::npos) << line;
    EXPECT_NE(line.find("key=\"item:7\""), std::string::npos) << line;
    EXPECT_NE(line.find("router=simple"), std::string::npos) << line;
}

TEST(LogFormat, StartsWithAUtcTimestamp) {
    const auto line = cacherouter::logging::format(cache_event(CacheEventType::Hit, "k"));
    EXPECT_TRUE(
        std::regex_search(line, std::regex(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z)")))
        << line;
}

TEST(LogFormat, ProducesASingleLine) {
    const auto line = cacherouter::logging::format(cache_event(CacheEventType::Hit, "a\nb"));
    // The key itself may contain a newline; everything the formatter adds must not.
    EXPECT_EQ(line.find('\n'), line.find("a\nb") + 1);
}

TEST(LogFormat, MarksAnEventWithNoNode) {
    const auto line = cacherouter::logging::format(cache_event(CacheEventType::Hit, "k", ""));
    EXPECT_NE(line.find("node=-"), std::string::npos) << line;
}

// --- threshold filtering ----------------------------------------------------

TEST(EventLog, WritesEventsAtOrAboveTheThreshold) {
    std::ostringstream out;
    EventLog log{out, Level::Debug};

    log(cache_event(CacheEventType::Hit, "traced"));     // Trace, below threshold
    log(cache_event(CacheEventType::Miss, "debugged"));  // Debug
    log(cache_event(CacheEventType::Evict, "evicted"));  // Info

    EXPECT_EQ(log.written(), 2u);
    EXPECT_EQ(log.suppressed(), 1u);

    const auto written = lines(out);
    ASSERT_EQ(written.size(), 2u);
    EXPECT_NE(written[0].find("debugged"), std::string::npos);
    EXPECT_NE(written[1].find("evicted"), std::string::npos);
}

TEST(EventLog, DefaultThresholdKeepsOnlyEvictions) {
    std::ostringstream out;
    EventLog log{out};
    EXPECT_EQ(log.threshold(), Level::Info);

    log(cache_event(CacheEventType::Hit, "a"));
    log(cache_event(CacheEventType::Miss, "b"));
    log(cache_event(CacheEventType::Insert, "c"));
    log(router_event("d", "node-A", "consistent"));
    log(cache_event(CacheEventType::Evict, "e"));

    EXPECT_EQ(log.written(), 1u);
    EXPECT_EQ(log.suppressed(), 4u);
    EXPECT_NE(out.str().find("evict"), std::string::npos);
}

TEST(EventLog, TraceKeepsEverything) {
    std::ostringstream out;
    EventLog log{out, Level::Trace};

    log(cache_event(CacheEventType::Hit, "a"));
    log(router_event("b", "node-A", "consistent"));
    log(cache_event(CacheEventType::Evict, "c"));

    EXPECT_EQ(log.written(), 3u);
    EXPECT_EQ(log.suppressed(), 0u);
}

TEST(EventLog, OffSuppressesEverythingIncludingEvictions) {
    std::ostringstream out;
    EventLog log{out, Level::Off};

    log(cache_event(CacheEventType::Evict, "a"));
    log(cache_event(CacheEventType::Hit, "b"));

    EXPECT_EQ(log.written(), 0u);
    EXPECT_EQ(log.suppressed(), 2u);
    EXPECT_TRUE(out.str().empty());
}

TEST(EventLog, ThresholdCanChangeAtRuntime) {
    std::ostringstream out;
    EventLog log{out, Level::Off};
    log(cache_event(CacheEventType::Evict, "dropped"));

    log.set_threshold(Level::Info);
    log(cache_event(CacheEventType::Evict, "kept"));

    EXPECT_EQ(log.written(), 1u);
    EXPECT_EQ(out.str().find("dropped"), std::string::npos);
    EXPECT_NE(out.str().find("kept"), std::string::npos);
}

// --- concurrency ------------------------------------------------------------

// httplib serves requests on a thread pool, so the cluster emits from several
// threads at once. Unsynchronised writes would splice lines into each other.
TEST(EventLog, ConcurrentWritesDoNotInterleave) {
    std::ostringstream out;
    EventLog log{out, Level::Trace};

    constexpr int kThreads = 8;
    constexpr int kPerThread = 200;

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; t++) {
        workers.emplace_back([&log, t] {
            for (int i = 0; i < kPerThread; i++) {
                log(cache_event(CacheEventType::Hit,
                                "key-" + std::to_string(t) + "-" + std::to_string(i)));
            }
        });
    }
    for (auto& worker : workers) worker.join();

    EXPECT_EQ(log.written(), static_cast<std::size_t>(kThreads * kPerThread));

    const auto written = lines(out);
    ASSERT_EQ(written.size(), static_cast<std::size_t>(kThreads * kPerThread));

    const std::regex well_formed(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z\s+TRACE\s+hit\s+node=\S+\s+key="key-\d+-\d+"$)");
    for (const auto& line : written) {
        EXPECT_TRUE(std::regex_match(line, well_formed)) << "spliced line: " << line;
    }
}

// --- wired to a cluster -----------------------------------------------------

TEST(EventLog, LogsRealClusterTraffic) {
    std::ostringstream out;
    EventLog log{out, Level::Trace};

    auto cluster = cacherouter::CacheCluster{cacherouter::router::make_router("consistent")};
    cluster.set_event_handler([&log](Event e) { log(e); });
    cluster.add_node({.id = "only", .capacity = 1, .policy_name = "lru"});

    cluster.put("alpha", "one");  // insert + route
    cluster.get("alpha");         // hit
    cluster.get("absent");        // miss
    cluster.put("beta", "two");   // insert + route + evict (capacity is 1)

    const auto text = out.str();
    for (const auto* expected : {"insert", "route", "hit", "miss", "evict"}) {
        EXPECT_NE(text.find(expected), std::string::npos) << "missing " << expected << "\n" << text;
    }
    EXPECT_NE(text.find("router=consistent"), std::string::npos) << text;
}

// The cluster stamps the node id, since a Cache does not know which node owns it.
TEST(EventLog, CacheEventsIdentifyTheirNode) {
    std::vector<CacheEvent> seen;
    auto cluster = cacherouter::CacheCluster{cacherouter::router::make_router("simple")};
    cluster.set_event_handler([&seen](Event e) {
        if (const auto* c = std::get_if<CacheEvent>(&e.event)) seen.push_back(*c);
    });
    cluster.add_node({.id = "node-A", .capacity = 10, .policy_name = "lru"});

    cluster.put("alpha", "one");

    ASSERT_FALSE(seen.empty());
    for (const auto& event : seen) EXPECT_EQ(event.node_id, "node-A");
}

}  // namespace
