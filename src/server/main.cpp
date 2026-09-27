#include <httplib.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
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
    cacherouter::logging::Level debug_level = cacherouter::logging::Level::Off;
    int port = 8080;
};

[[nodiscard]] Config parse_argument(const std::vector<std::string>& args) {
    Config config;
    for (const std::string& arg : args) {
        if (auto pos = arg.find('='); pos != std::string::npos) {
            if (arg.starts_with("--port"))
                config.port = std::stoi(arg.substr(pos + 1));
            else if (arg.starts_with("--debug")) {
                std::string l = arg.substr(pos + 1);
                if (auto level = cacherouter::logging::name_to_level(l))
                    config.debug_level = level.value();
                else
                    throw std::invalid_argument("Unknown debug level: " + l);
            }
        }
    }
    return config;
}

int main(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(argc - 1);
    for (int i = 1; i < argc; i++) args.emplace_back(argv[i]);

    Config config;
    try {
        config = parse_argument(args);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    auto cluster = cacherouter::CacheCluster(cacherouter::router::make_router("consistent"));

    // Logs go to stderr, leaving stdout for whatever the process itself prints.
    // Declared before the server so it outlives the handler borrowing it.
    cacherouter::logging::EventLog event_log{std::clog, config.debug_level};
    cluster.set_event_handler([&event_log](const cacherouter::Event& e) { event_log(e); });

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
            }
            res.status = 500;
            res.set_content(nlohmann::json{{"error", message}}.dump(), "application/json");
        });

    server.set_mount_point("/", "./web");

    std::cout << "Server listening on port http://localhost:" << config.port << '\n';
    std::cout << "Logging cluster events at "
              << cacherouter::logging::level_to_name(config.debug_level)
              << " --debug=trace|debug|info|off\n"
              << std::flush;
    server.listen("0.0.0.0", config.port);

    return 0;
}
