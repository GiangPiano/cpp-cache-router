add_library(cacherouter_core
  hash_ring.cpp
  cluster.cpp
  policies/lru_policy.cpp
  policies/lfu_policy.cpp
  routers/simple_router.cpp
  routers/consistent_router.cpp
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
