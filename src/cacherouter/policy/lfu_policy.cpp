#include "cacherouter/policy/lfu_policy.hpp"

#include <optional>
#include <string>
#include <utility>

namespace cacherouter::policy {

void LfuPolicy::on_insert(const std::string& key) {
    if (address_.contains(key)) {
        on_access(key);
        return;
    }

    freq_[key] = 1;
    buckets_[1].push_front(key);
    address_[key] = buckets_[1].begin();
    min_freq_ = 1;
}

void LfuPolicy::on_access(const std::string& key) {
    const auto it = address_.find(key);
    if (it == address_.end()) {
        on_insert(key);
        return;
    }

    const int old_freq = freq_.at(key);
    buckets_[old_freq].erase(it->second);

    if (old_freq == min_freq_ && buckets_[old_freq].empty()) min_freq_ = old_freq + 1;
    if (buckets_[old_freq].empty()) buckets_.erase(old_freq);

    const int new_freq = old_freq + 1;
    freq_[key] = new_freq;
    buckets_[new_freq].push_front(key);
    it->second = buckets_[new_freq].begin();
}

void LfuPolicy::on_remove(const std::string& key) {
    const auto it = address_.find(key);
    if (it == address_.end()) return;

    const int old_freq = freq_.at(key);
    buckets_[old_freq].erase(it->second);
    address_.erase(it);
    freq_.erase(key);

    if (!buckets_[old_freq].empty()) return;
    buckets_.erase(old_freq);

    if (old_freq != min_freq_) return;
    if (address_.empty()) {
        min_freq_ = 0;
        return;
    }
    while (!buckets_.contains(min_freq_)) min_freq_++;
}

std::optional<std::string> LfuPolicy::victim() const {
    if (address_.empty()) return std::nullopt;
    return buckets_.at(min_freq_).back();
}

std::string LfuPolicy::name() const { return "lfu"; }

}  // namespace cacherouter::policy
