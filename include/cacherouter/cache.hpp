#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "cacherouter/events.hpp"
#include "cacherouter/node.hpp"
#include "cacherouter/policy/eviction_policy.hpp"

namespace cacherouter {

template <typename K, typename V>
class Cache {
public:
    Cache(NodeSpec spec, std::unique_ptr<EvictionPolicy> policy)
        : spec_{std::move(spec)}
        , policy_{std::move(policy)} {
        spec_.size = 0;  // whatever a caller passed in is not a live count
    };

    void set_event_handler(std::function<void(CacheEvent)> handler) {
        on_event_ = std::move(handler);
    };

    std::optional<V> get(const K& key) {
        std::string skey = to_string(key);

        if (auto it = data_.find(skey); it != data_.end()) {
            policy_->on_access(skey);
            emit(CacheEventType::Hit, skey);
            return it->second;
        }

        emit(CacheEventType::Miss, skey);
        return std::nullopt;
    };

    void put(const K& key, V value) {
        std::string skey = to_string(key);

        if (auto it = data_.find(skey); it == data_.end()) {
            policy_->on_insert(skey);
            emit(CacheEventType::Insert, skey);
        } else {
            policy_->on_access(skey);
            emit(CacheEventType::Update, skey);
        }

        data_[skey] = std::move(value);

        if (data_.size() > spec_.capacity) {
            if (auto victim = policy_->victim()) {
                data_.erase(*victim);
                policy_->on_remove(*victim);
                emit(CacheEventType::Evict, *victim);
            }
        }

        spec_.size = data_.size();
    };

    // The node's spec, with size kept current against data_.
    [[nodiscard]] const NodeSpec& info() const { return spec_; };
    [[nodiscard]] size_t size() const { return data_.size(); };

private:
    static std::string to_string(const K& key) {
        std::stringstream oss;
        oss << key;
        return oss.str();
    }

    void emit(CacheEventType type, const std::string& key) {
        if (on_event_) on_event_({.type = type, .key = key, .node_id = spec_.id});
    };

    NodeSpec spec_;
    std::unique_ptr<EvictionPolicy> policy_;
    std::unordered_map<std::string, V> data_;
    std::function<void(CacheEvent)> on_event_;
};

}  // namespace cacherouter
