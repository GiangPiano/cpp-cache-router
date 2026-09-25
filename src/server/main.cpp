#include <httplib.h>

#include <iostream>
#include <memory>
#include <utility>

#include "api.hpp"
#include "cacherouter/cluster.hpp"
#include "cacherouter/router/consistent_router.hpp"

int main() {
    auto router = std::make_unique<cacherouter::router::ConsistentRouter>();
    auto* router_ptr = router.get();
    auto cluster = cacherouter::CacheCluster(std::move(router));

    cluster.add_node({.id = "node-A", .capacity = 100, .policy_name = "lru"});
    cluster.add_node({.id = "node-B", .capacity = 100, .policy_name = "lru"});

    httplib::Server server;
    register_cluster(server, cluster);
    register_router(server, *router_ptr);

    server.set_mount_point("/", "./web");

    std::cout << "Server listening on port http://localhost:8082\n";
    server.listen("0.0.0.0", 8082);

    return 0;
}
