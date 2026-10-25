#include <httplib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <print>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "api.hpp"
#include "cacherouter/cluster.hpp"
#include "cacherouter/events.hpp"
#include "cacherouter/logging/event_log.hpp"
#include "cacherouter/router/router_factory.hpp"
#include "nlohmann/json_fwd.hpp"

struct Config {
    cacherouter::logging::Level debug_level = cacherouter::logging::Level::Off;
    int port                                = 8080;
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
    cacherouter::logging::EventLog logger{std::clog, config.debug_level};


    auto cluster = cacherouter::CacheCluster(cacherouter::router::make_router("consistent"));

    // Logs go to stderr, leaving stdout for whatever the process itself prints.
    // Declared before the server so it outlives the handler borrowing it.
    cluster.set_event_handler([&logger](const cacherouter::Event& e) { logger(e); });
    cluster.clear_nodes();
    cluster.reset_to_default();

    httplib::Server server;

    // cpp-httplib leaves TCP_NODELAY off by default, so Nagle's algorithm holds
    // small writes (e.g. headers vs body) until the peer's ACK. On Linux loopback
    // that ACK is commonly delayed ~40ms, capping throughput at ~25 req/s per
    // connection. Mac loopback doesn't hit this as reliably, which is why the
    // same binary looks fine there and crawls on Ubuntu.
    server.set_tcp_nodelay(true);

    // A pool much larger than core count just adds scheduling overhead once
    // CacheCluster can actually run requests in parallel (it used to serialize
    // everything behind one lock, so a big pool was free insurance; it isn't
    // anymore).
    const auto pool_size = std::max(std::thread::hardware_concurrency(), 4u);
    server.new_task_queue = [pool_size] { return new httplib::ThreadPool(pool_size); };

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

    std::println("Server listening on port http://localhost:{}", config.port);
    std::println("Logging cluster events at {} --debug=trace|debug|info|off",
                 cacherouter::logging::level_to_name(config.debug_level));
    if (!server.listen("0.0.0.0", config.port)) {
        std::println(stderr, "Error: could not bind to port {}", config.port);
        return 1;
    };

    return 0;
}
