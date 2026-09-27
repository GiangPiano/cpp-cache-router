include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

add_executable(cacherouter_tests
  test_lru_policy.cpp
  # test_lfu_policy.cpp   # LfuPolicy is declared but not implemented yet
  test_hash_ring.cpp
  test_simple_router.cpp
  test_consistent_router.cpp
  test_cluster.cpp
  test_event_log.cpp
)

target_link_libraries(cacherouter_tests PRIVATE
  cacherouter::core
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cacherouter_tests)
