#pragma once

#include <optional>
#include <string>

namespace cacherouter {

class EvictionPolicy {
 public:
  virtual ~EvictionPolicy() = default;

  virtual void on_access(const std::string& key) = 0;
  virtual void on_insert(const std::string& key) = 0;
  virtual void on_remove(const std::string& key) = 0;

  [[nodiscard]] virtual std::optional<std::string> victim() const = 0;
  [[nodiscard]] virtual std::string name() const = 0;

 private:
};

}  // namespace cacherouter
