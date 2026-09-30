#pragma once

#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#include "cacherouter/policy/eviction_policy.hpp"

namespace cacherouter::policy {

class LfuPolicy final : public EvictionPolicy {
public:
    void on_access(const std::string& key) override;
    void on_insert(const std::string& key) override;
    void on_remove(const std::string& key) override;

    [[nodiscard]] std::optional<std::string> victim() const override;
    [[nodiscard]] std::string name() const override;

private:
    // Lowest non-empty frequency bucket; 0 when nothing is tracked.
    int min_freq_ = 0;
    std::unordered_map<int, std::list<std::string>> buckets_;
    std::unordered_map<std::string, int> freq_;
    std::unordered_map<std::string, std::list<std::string>::iterator> address_;
};

}  // namespace cacherouter::policy
