#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "murmur3/MurmurHash3.h"

// inline std::uint64_t hash_key(const std::string& s) { return std::hash<std::string>{}(s); }
inline std::uint64_t hash_key(const std::string& s) {
  uint64_t out[2] = {0, 0};
  MurmurHash3_x64_128(s.data(), static_cast<int>(s.size()), 67, &out);
  return out[0];
}

template <typename Cache>
concept CachePolicy =
    requires(Cache cache, std::size_t capacity, const std::string& key, const std::string& value) {
      { Cache(capacity) };
      { cache.get(key) } -> std::same_as<std::expected<std::string, CacheError>>;
      { cache.set(key, value) } -> std::same_as<void>;
    };

template <CachePolicy Cache>
class RouterBase {
  std::size_t node_capacity{};
  std::unordered_map<std::string, Cache> nodes;

 protected:
  virtual std::expected<std::string, CacheError> locate(const std::string& key) = 0;

 public:
  RouterBase() = default;

  explicit RouterBase(const std::vector<std::string>& node_list, std::size_t node_capacity)
      : node_capacity(node_capacity) {
    for (const auto& s : node_list) nodes.emplace(s, Cache(node_capacity));
  }

  virtual ~RouterBase() = default;

  virtual void add_node(const std::string& node) { nodes.emplace(node, Cache(node_capacity)); }

  virtual void delete_node(const std::string& node) { nodes.erase(node); }

  [[nodiscard]] std::size_t node_count() const noexcept { return nodes.size(); }

  std::expected<std::string, CacheError> get(const std::string& key) {
    if (nodes.empty()) return std::unexpected(CacheError::CacheNotFound);
    auto node = locate(key);
    if (node.has_value()) return nodes.at(node.value()).get(key);
    return std::unexpected(node.error());
  }

  void set(const std::string& key, const std::string& value) {
    if (nodes.empty()) return;
    auto node = locate(key);
    if (node.has_value()) return nodes.at(node.value()).set(key, value);
  }
};

template <CachePolicy Cache>
class SimpleRouter : public RouterBase<Cache> {
  std::vector<std::string> node_list;
  std::unordered_map<std::string, size_t> node_idx;

 protected:
  std::expected<std::string, CacheError> locate(const std::string& key) override {
    if (node_list.empty()) return std::unexpected(CacheError::CacheNotFound);
    return node_list[hash_key(key) % node_list.size()];
  }

 public:
  SimpleRouter() = default;
  explicit SimpleRouter(std::vector<std::string> nodes, std::size_t node_capacity)
      : RouterBase<Cache>(nodes, node_capacity), node_list(std::move(nodes)) {}

  void add_node(const std::string& node) override {
    if (node_idx.contains(node)) return;
    RouterBase<Cache>::add_node(node);
    node_list.push_back(node);
    node_idx.emplace(node, node_list.size() - 1);
  }

  void delete_node(const std::string& node) override {
    RouterBase<Cache>::delete_node(node);

    if (auto it = node_idx.find(node); it != node_idx.end()) {
      node_idx[node_list.back()] = it->second;
      std::swap(node_list[it->second], node_list.back());

      node_list.pop_back();
      node_idx.erase(it);
    }
  }
};

template <CachePolicy Cache>
class ConsistentRouter : public RouterBase<Cache> {
  std::vector<std::string> node_list;
  std::map<uint64_t, std::string> ring;
  size_t virtual_node_count;

 protected:
  std::expected<std::string, CacheError> locate(const std::string& key) override {
    if (ring.empty()) return std::unexpected(CacheError::ClusterEmpty);
    if (auto it = ring.lower_bound(hash_key(key)); it != ring.end()) return it->second;
    return ring.begin()->second;
  }

 public:
  ConsistentRouter() = default;
  explicit ConsistentRouter(std::vector<std::string> nodes, size_t node_capacity,
                            size_t virtual_node_count = 10)
      : RouterBase<Cache>(nodes, node_capacity), node_list(nodes), virtual_node_count{virtual_node_count} {
    for (const auto& node : nodes)
      for (size_t i = 1; i <= virtual_node_count; i++)
        ring.emplace(hash_key(node + ":" + std::to_string(i)), node);
  }

  void add_node(const std::string& node) override {
    RouterBase<Cache>::add_node(node);
    for (size_t i = 1; i <= virtual_node_count; i++)
      ring.emplace(hash_key(node + ":" + std::to_string(i)), node);
  }

  void delete_node(const std::string& node) override {
    RouterBase<Cache>::delete_node(node);
    for (size_t i = 1; i <= virtual_node_count; i++) ring.erase(hash_key(node + ":" + std::to_string(i)));
  }
};
