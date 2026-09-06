#pragma once

#include <cstddef>
#include <expected>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/error.hpp"

// template <typename K, typename V>
class LRUCache {
 private:
  std::size_t capacity;
  using Container = std::list<std::pair<std::string, std::string>>;
  Container cache;
  std::unordered_map<std::string, Container::iterator> address;

 public:
  LRUCache(std::size_t n) : capacity{n} {}

  std::expected<std::string, CacheError> get(const std::string& key) {
    if (auto it = address.find(key); it != address.end()) {
      cache.splice(cache.begin(), cache, address.at(key));
      return cache.front().second;
    }
    return std::unexpected(CacheError::CacheMiss);
  }

  void set(const std::string& key, const std::string& value) {
    if (auto it = address.find(key); it != address.end()) cache.erase(it->second);
    cache.emplace_front(key, value);
    address.emplace(key, cache.begin());

    if (cache.size() > capacity) {
      address.erase(cache.back().first);
      cache.pop_back();
    }
  }
};
