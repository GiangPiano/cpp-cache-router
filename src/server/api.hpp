#pragma once

#include <httplib.h>

#include <nlohmann/json.hpp>

#include "cacherouter/cluster.hpp"
#include "nlohmann/json_fwd.hpp"

using namespace httplib;

using json = nlohmann::json;

void handle_list_nodes(const Request& req, Response& res, cacherouter::CacheCluster& cluster) {
    res.set_content(json(cluster.list_nodes()).dump(), "application/json");
}

void register_api(Server& server, cacherouter::CacheCluster& cluster) {
    server.Get("/api/nodes", [&cluster](const Request& req, Response& res) {
        handle_list_nodes(req, res, cluster);
    });
}
