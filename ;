#include "cacherouter/logging/event_log.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <format>
#include <iterator>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <variant>

#include "cacherouter/events.hpp"

namespace cacherouter::logging {

std::string action_name(CacheEventType type) {
    switch (type) {
        case CacheEventType::Hit:    return "hit";
        case CacheEventType::Miss:   return "miss";
        case CacheEventType::Insert: return "insert";
        case CacheEventType::Evict:  return "evict";
        case CacheEventType::Update: return "update";
    }
    return "unknown";
}

std::string timestamp(const Event& event) {
    return std::format("{:%FT%T}Z", std::chrono::floor<std::chrono::milliseconds>(event.timestamp));
}

std::string level_to_name(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO";
        case Level::Off:   return "OFF";
    }
    return "?";
}

std::optional<Level> name_to_level(const std::string& name) {
    std::string lowered;
    lowered.reserve(name.size());
    std::ranges::transform(name, std::back_inserter(lowered),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lowered == "trace") return Level::Trace;
    if (lowered == "debug") return Level::Debug;
    if (lowered == "info") return Level::Info;
    if (lowered == "off" || lowered == "none") return Level::Off;
    return std::nullopt;
}

Level level_of(const Event& event) {
    if (const auto* cache = std::get_if<CacheEvent>(&event.event)) {
        switch (cache->type) {
            case CacheEventType::Hit:    return Level::Trace;
            case CacheEventType::Miss:   return Level::Debug;
            case CacheEventType::Insert: return Level::Debug;
            case CacheEventType::Update: return Level::Debug;
            case CacheEventType::Evict:  return Level::Info;
        }
    }
    return Level::Trace;  // routing events
}

std::string format(const Event& event) {
    const auto time = timestamp(event);
    const auto level = level_to_name(level_of(event));

    if (const auto* cache_event = std::get_if<CacheEvent>(&event.event)) {
        // A cache that was never registered with a cluster emits no node id.
        const std::string node =
            cache_event->node_id.empty() ? std::string{"-"} : std::string{cache_event->node_id};
        return std::format(R"({}  {:<5}  {:<6}  node={:<10}  key="{}")", time, level,
                           action_name(cache_event->type), node, cache_event->key);
    }

    const auto& router_event = std::get<RouterEvent>(event.event);
    return std::format(R"({}  {:<5}  {:<6}  node={:<10}  key="{}"  router={})", time, level,
                       "route", router_event.node, router_event.key, router_event.router_name);
}

EventLog::EventLog(std::ostream& out, Level threshold) : out_(&out), threshold_(threshold) {}

void EventLog::operator()(const Event& event) {
    const auto level = level_of(event);
    const std::scoped_lock lock(mutex_);

    if (threshold_ == Level::Off || level < threshold_) {
        suppressed_++;
        return;
    }

    *out_ << format(event) << '\n' << std::flush;
    written_++;
}

void EventLog::set_threshold(Level threshold) {
    const std::scoped_lock lock(mutex_);
    threshold_ = threshold;
}

Level EventLog::threshold() const {
    const std::scoped_lock lock(mutex_);
    return threshold_;
}

std::size_t EventLog::written() const {
    const std::scoped_lock lock(mutex_);
    return written_;
}

std::size_t EventLog::suppressed() const {
    const std::scoped_lock lock(mutex_);
    return suppressed_;
}

}  // namespace cacherouter::logging
