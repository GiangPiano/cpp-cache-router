#pragma once

#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#include "cacherouter/eviction_policy.hpp"

namespace cacherouter {

class LfuPolicy final : public EvictionPolicy {
 public:
  void on_access() override;
  void on_insert() override;
  void on_remove() override;

  std::optional<std::string> victim() override;
  [[nodiscard]] std::string name() const override;

 private:
  void bump(const std::string& key);

  int min_freq_ = 0;
  std::unordered_map<int, std::list<std::string>> buckets_;
  std::unordered_map<std::string, int> freq_;
  std::unordered_map<std::string, std::list<std::string>::iterator> address_;
};

}  // namespace cacherouter
