cmake_minimum_required(VERSION 4.0)

project(
  ConsistentHashing
  VERSION 0.0.1
  DESCRIPTION "Simulating a multi-instance cache router, with consistent hashing"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd to work in neovim

## EXTERNAL LIBRARY
include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.0
)

FetchContent_MakeAvailable(nlohmann_json httplib)

## INTERNAL LIBRARY

add_library(murmur3 STATIC
  src/murmur3/MurmurHash3.cpp
  include/murmur3/MurmurHash3.h
)

target_include_directories(murmur3 PUBLIC include)

add_library(core INTERFACE
  src/core/config.hpp
  src/core/router.hpp
  src/core/cache.hpp
  src/core/error.hpp
)

set_target_properties(core PROPERTIES LINKER_LANGUAGE CXX)


target_include_directories(core INTERFACE src)

target_link_libraries(core INTERFACE
  nlohmann_json::nlohmann_json
  httplib::httplib
)

## EXECUTABLE
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE core murmur3)

# TEST
option(BUILD_TESTING "Build GoogleTest unit tests" ON)
if (BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()
