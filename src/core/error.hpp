#pragma once

#include <cstdint>

enum class CacheError : uint8_t { CacheMiss, CacheNotFound, ClusterEmpty };
