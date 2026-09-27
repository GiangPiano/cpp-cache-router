#include <httplib.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "api.hpp"
#include "cacherouter/cluster.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/logging/event_log.hpp"
#include "cacherouter/router/router_factory.hpp"
#include "nlohmann/json_fwd.hpp"

cacherouter::logging::Level log_level_from_env() {
    const char* raw = std::getenv("CACHEROUTER_LOG_LEVEL");
    if (raw == nullptr) return cacherouter::logging::Level::Info;

    if (const auto parsed = cacherouter::logging::name_to_level(raw)) return *parsed;

    std::cerr << "Ignoring unrecognised CACHEROUTER_LOG_LEVEL=\"" << raw << "\"; using info.\n";
    return cacherouter::logging::Level::Info;
}

struct Config {
    bool run_test = false;
    bool print_tokens = false;
    bool print_parse_tree = false;
    bool has_input = false;
    std::string input;
};

[[nodiscard]] Config parse_argument(const std::vector<std::string>& args) {
    Config config;
    for (const std::string& arg : args) {
        if (arg == "--test")
            config.run_test = true;
        else if (arg == "--tokens")
            config.print_tokens = true;
        else if (arg == "--parse-tree")
            config.print_parse_tree = true;
        else {
            config.has_input = true;
            config.input = arg;
        }
    }
    return config;
}

int main(int argc, char** argv) {
    auto cluster = cacherouter::CacheCluster(cacherouter::router::make_router("consistent"));

    // Logs go to stderr, leaving stdout for whatever the process itself prints.
    // Declared before the server so it outlives the handler borrowing it.
    const auto level = log_level_from_env();
    cacherouter::logging::EventLog event_log{std::clog, level};
    cluster.set_event_handler([&event_log](cacherouter::Event e) { event_log(e); });

    cluster.add_node({.id = "node-A", .capacity = 100, .policy_name = "lru"});
    cluster.add_node({.id = "node-B", .capacity = 100, .policy_name = "lru"});

    httplib::Server server;
    register_cluster(server, cluster);
    register_router(server, cluster);

    server.set_exception_handler(
        [](const httplib::Request&, httplib::Response& res, const std::exception_ptr& ep) {
            auto message = std::string{"internal error"};
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                message = e.what();
            } catch (...) {
            }
            res.status = 500;
            res.set_content(nlohmann::json{{"error", message}}.dump(), "application/json");
        });

    server.set_mount_point("/", "./web");

    std::cout << "Server listening on port http://localhost:8082\n";
    std::cout << "Logging cluster events at " << cacherouter::logging::level_to_name(level)
              << " (set CACHEROUTER_LOG_LEVEL=trace|debug|info|off)\n"
              << std::flush;
    server.listen("0.0.0.0", 8082);

    return 0;
}
