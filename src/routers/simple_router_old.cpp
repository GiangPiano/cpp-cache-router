#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cache/cache_policy.hpp"
#include "router/base_router.hpp"
#include "utils/error.hpp"

template <CachePolicy Cache>
class SimpleRouter : public BaseRouter<Cache> {
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
      : BaseRouter<Cache>(nodes, node_capacity)
      , node_list(std::move(nodes)) {}

  void add_node(const std::string& node) override {
    if (node_idx.contains(node)) return;

    BaseRouter<Cache>::add_node(node);

    node_list.push_back(node);
    node_idx.emplace(node, node_list.size() - 1);
  }

  void delete_node(const std::string& node) override {
    BaseRouter<Cache>::delete_node(node);

    if (auto it = node_idx.find(node); it != node_idx.end()) {
      node_idx[node_list.back()] = it->second;
      std::swap(node_list[it->second], node_list.back());

      node_list.pop_back();
      node_idx.erase(it);
    }
  }
};
