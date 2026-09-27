#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>

#include "cacherouter/events.hpp"

namespace cacherouter::logging {

// Ordered by increasing importance. Off suppresses everything.
enum class Level : std::uint8_t { Trace, Debug, Info, Off };

[[nodiscard]] std::string level_to_name(Level level);

[[nodiscard]] std::optional<Level> name_to_level(const std::string& name);

[[nodiscard]] Level level_of(const Event& event);

// time  level  event_name  node  key  <router>
[[nodiscard]] std::string format(const Event& event);

// Writes events at or above `threshold` to a stream.
//
// CacheCluster::set_event_handler takes a std::function<void>
// `cluster.set_event_handler [&log](Event e) { log(e); })`.
//
// The lock is needed because httplib serves
// requests on a thread pool, so the cluster emits from several threads at once
// and unsynchronised writes would interleave mid-line.
class EventLog {
public:
    explicit EventLog(std::ostream& out, Level threshold = Level::Info);

    EventLog(const EventLog&) = delete;
    EventLog& operator=(const EventLog&) = delete;

    void operator()(const Event& event);

    void set_threshold(Level threshold);
    [[nodiscard]] Level threshold() const;

    [[nodiscard]] std::size_t written() const;
    [[nodiscard]] std::size_t suppressed() const;

private:
    std::ostream* out_;
    Level threshold_;
    mutable std::mutex mutex_;
    std::size_t written_ = 0;
    std::size_t suppressed_ = 0;
};

}  // namespace cacherouter::logging
