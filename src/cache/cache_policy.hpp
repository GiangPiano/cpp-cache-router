#pragma once

#include <concepts>
#include <cstddef>
#include <expected>
#include <string>

#include "utils/error.hpp"

template <typename Cache>
concept CachePolicy =
    requires(Cache cache, std::size_t capacity, const std::string& key, const std::string& value) {
      { Cache(capacity) };
      { cache.get(key) } -> std::same_as<std::expected<std::string, CacheError>>;
      { cache.set(key, value) } -> std::same_as<void>;
    };
