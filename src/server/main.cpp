#include <httplib.h>

#include <iostream>
#include <memory>
#include <utility>

#include "api.hpp"
#include "cacherouter/cluster.hpp"
#include "cacherouter/router/consistent_router.hpp"

int main() {
    auto router = std::make_unique<cacherouter::router::ConsistentRouter>();

    auto cluster = cacherouter::CacheCluster(std::move(router));

    cluster.add_node("node-A", 100, "lru");
    cluster.add_node("node-B", 100, "lru");

    httplib::Server server;
    register_api(server, cluster);
    server.set_mount_point("/", "./web");

    std::cout << "Server listening on port http://localhost:8082\n";
    server.listen("0.0.0.0", 8082);

    return 0;
    ;
}
