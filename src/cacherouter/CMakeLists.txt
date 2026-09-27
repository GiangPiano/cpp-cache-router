add_library(cacherouter_core
  cluster.cpp
  policy/lru_policy.cpp
  # policy/lfu_policy.cpp
  router/simple_router.cpp
  router/consistent_router.cpp
  router/hash_ring.cpp
  utils/hash.cpp
  logging/event_log.cpp
)
add_library(cacherouter::core ALIAS cacherouter_core)

target_include_directories(cacherouter_core PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(cacherouter_core PUBLIC cxx_std_23)

# link murmur3 as PRIVATE if only hash_ring.cpp calls into it and no public
# header exposes murmur3 types; switch to PUBLIC if hash_ring.hpp itself
# includes MurmurHash3.h
target_link_libraries(cacherouter_core PRIVATE murmur3)
