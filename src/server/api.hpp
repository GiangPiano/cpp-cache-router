#pragma once

#include <httplib.h>

#include <cstddef>
#include <exception>
#include <nlohmann/json.hpp>
#include <string>
#include <tuple>

#include "cacherouter/cluster.hpp"
#include "cacherouter/node.hpp"
#include "cacherouter/router/consistent_router.hpp"
#include "nlohmann/json_fwd.hpp"

using namespace httplib;

using json = nlohmann::json;


inline void handle_list_nodes(const Request& req, Response& res,
                              cacherouter::CacheCluster& cluster) {
    std::ignore = req;

    auto nodes = json::array();
    for (const auto& [id, capacity, policy, virtual_nodes] : cluster.list_nodes()) {
        nodes.push_back({{"id", id},
                         {"capacity", capacity},
                         {"policy", policy},
                         {"virtual_nodes", virtual_nodes}});
    }
    res.status = 200;
    res.set_content(json{{"ring", nodes}}.dump(), "application/json");
    res.set_content(json(nodes).dump(), "application/json");
}


inline void handle_add_node(const Request& req, Response& res, cacherouter::CacheCluster& cluster) {
    try {
        const auto body = json::parse(req.body);
        cacherouter::Node node{
            .id = body.at("node-id").get<std::string>(),
            .capacity = body.at("capacity").get<std::size_t>(),
            .policy_name = body.at("policy").get<std::string>(),
        };

        if (auto it = body.find("virtual_nodes"); it != body.end())
            node.virtual_nodes = it->get<int>();

        cluster.add_node(node);
        res.status = 201;
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json{{"error", e.what()}}.dump(), "application/json");
    }
}


inline void handle_remove_node(const Request& req, Response& res,
                               cacherouter::CacheCluster& cluster) {
    const auto& key = req.path_params.at("id");
    cluster.remove_node(key);
    res.status = 200;
}


inline void handle_get_key(const Request& req, Response& res, cacherouter::CacheCluster& cluster) {
    if (cluster.list_nodes().empty()) {
        res.status = 503;
        res.set_content(json{{"error", "cluster has no nodes"}}.dump(), "application/json");
        return;
    }

    const auto& key = req.path_params.at("key");

    if (auto value = cluster.get(key)) {
        res.status = 200;
        res.set_content(
            json{{"key", key}, {"value", *value}, {"hash", cluster.get_hash(key)}}.dump(),
            "application/json");
    } else {
        res.status = 404;
        res.set_content(json{{"error", "not found"}, {"hash", cluster.get_hash(key)}}.dump(),
                        "application/json");
    }
}


inline void handle_put_key(const Request& req, Response& res, cacherouter::CacheCluster& cluster) {
    if (cluster.list_nodes().empty()) {
        res.status = 503;
        res.set_content(json{{"error", "cluster has no nodes"}}.dump(), "application/json");
        return;
    }

    const auto& key = req.path_params.at("key");

    try {
        const auto body = json::parse(req.body);
        const auto value = body.at("value").get<std::string>();

        cluster.put(key, value);

        res.status = 200;
        res.set_content(
            json{{"key", key}, {"value", value}, {"hash", cluster.get_hash(key)}}.dump(),
            "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json{{"error", e.what()}}.dump(), "application/json");
    }
}


inline void handle_get_ring(const Request& req, Response& res,
                            cacherouter::router::ConsistentRouter& router) {
    std::ignore = req;

    auto nodes = json::array();
    for (const auto& [hash, node] : router.ring().get_ring()) {
        nodes.push_back({{"hash", std::to_string(hash)}, {"node", node}});
    }
    res.status = 200;
    res.set_content(json{{"ring", nodes}}.dump(), "application/json");
}


inline void register_cluster(Server& server, cacherouter::CacheCluster& cluster) {
    server.Get("/api/nodes", [&cluster](const Request& req, Response& res) {
        handle_list_nodes(req, res, cluster);
    });
    server.Post("/api/nodes", [&cluster](const Request& req, Response& res) {
        handle_add_node(req, res, cluster);
    });
    server.Delete("/api/nodes/:id", [&cluster](const Request& req, Response& res) {
        handle_remove_node(req, res, cluster);
    });
    server.Get("/api/cache/:key", [&cluster](const Request& req, Response& res) {
        handle_get_key(req, res, cluster);
    });
    server.Put("/api/cache/:key", [&cluster](const Request& req, Response& res) {
        handle_put_key(req, res, cluster);
    });
}


inline void register_router(Server& server, cacherouter::router::ConsistentRouter& router) {
    server.Get("/api/ring",
               [&router](const Request& req, Response& res) { handle_get_ring(req, res, router); });
}
