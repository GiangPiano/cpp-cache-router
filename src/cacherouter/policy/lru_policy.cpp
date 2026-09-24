#include "cacherouter/policy/lru_policy.hpp"

#include <optional>
#include <string>

namespace cacherouter::policy {

void LruPolicy::on_access(const std::string& key) { touch(key); }

void LruPolicy::on_insert(const std::string& key) { touch(key); }

void LruPolicy::on_remove(const std::string& key) {
    auto it = address_.find(key);
    if (it == address_.end()) return;
    bucket_.erase(it->second);
    address_.erase(it);
}

std::optional<std::string> LruPolicy::victim() const {
    if (bucket_.empty()) return std::nullopt;
    return bucket_.back();
}

std::string LruPolicy::name() const { return "lru"; }

void LruPolicy::touch(const std::string& key) {
    if (auto it = address_.find(key); it != address_.end()) bucket_.erase(it->second);
    bucket_.push_front(key);
    address_[key] = bucket_.begin();
}

}  // namespace cacherouter::policy
