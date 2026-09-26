cmake_minimum_required(VERSION 4.0)

project(ConsistentHashing
  VERSION 0.0.2
  DESCRIPTION
    "Simulating a multi-instance cache router, with consistent hashing."
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd

option(CACHEROUTER_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

include(FetchContent)

add_library(murmur3 STATIC
  external/murmur3/MurmurHash3.cpp
)
target_include_directories(murmur3 PUBLIC external/murmur3)

## CORE LIBRARY + SERVER
add_subdirectory(src/cacherouter)
add_subdirectory(src/server)

if (CACHEROUTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
