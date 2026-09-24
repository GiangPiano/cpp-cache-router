include(FetchContent)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.0
)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(httplib nlohmann_json)

add_executable(cacherouter_server
  main.cpp
)

target_link_libraries(cacherouter_server PRIVATE
  cacherouter::core
  httplib::httplib
  nlohmann_json::nlohmann_json
)

add_custom_command(TARGET cacherouter_server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/web
    $<TARGET_FILE_DIR:cacherouter_server>/web
)
