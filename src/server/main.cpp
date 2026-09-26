#include <httplib.h>

#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "api.hpp"
#include "cacherouter/cluster.hpp"
#include "cacherouter/router/router_factory.hpp"
#include "nlohmann/json_fwd.hpp"

int main() {
    auto cluster = cacherouter::CacheCluster(cacherouter::router::make_router("consistent"));

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
    server.listen("0.0.0.0", 8082);

    return 0;
}
