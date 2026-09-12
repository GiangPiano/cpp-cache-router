#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache/cache_policy.hpp"
#include "utils/error.hpp"
#include "utils/hash.hpp"

template <CachePolicy Cache>
class BaseRouter {
  std::size_t node_capacity{};
  std::unordered_map<std::string, Cache> nodes;

 protected:
  virtual std::expected<std::string, CacheError> locate(const std::string& key) = 0;

 public:
  BaseRouter() = default;

  BaseRouter(const BaseRouter&) = delete;
  BaseRouter(BaseRouter&&) = delete;
  BaseRouter& operator=(const BaseRouter&) = delete;
  BaseRouter& operator=(BaseRouter&&) = delete;

  explicit BaseRouter(const std::vector<std::string>& node_list, std::size_t node_capacity)
      : node_capacity(node_capacity) {
    for (const auto& s : node_list) nodes.emplace(s, Cache(node_capacity));
  }

  virtual ~BaseRouter() = default;

  virtual void add_node(const std::string& node) { nodes.emplace(node, Cache(node_capacity)); }

  virtual void delete_node(const std::string& node) { nodes.erase(node); }

  [[nodiscard]] std::size_t node_count() const noexcept { return nodes.size(); }

  std::expected<std::string, CacheError> get(const std::string& key) {
    if (nodes.empty()) return std::unexpected(CacheError::CacheNotFound);
    auto node = locate(key);
    if (node.has_value()) return nodes.at(node.value()).get(key);
    return std::unexpected(node.error());
  }

  std::expected<std::string, CacheError> set(const std::string& key, const std::string& value) {
    if (nodes.empty()) return std::unexpected(CacheError::ClusterEmpty);
    auto node = locate(key);
    if (node.has_value()) {
      nodes.at(node.value()).set(key, value);
      return node.value();
    }
    return std::unexpected(CacheError::UnexpectedError);
  }
};
