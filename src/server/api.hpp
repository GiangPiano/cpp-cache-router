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
#include "cacherouter/router/router_factory.hpp"
#include "cacherouter/utils/hash.hpp"
#include "nlohmann/json_fwd.hpp"

using namespace httplib;

using json = nlohmann::json;


inline void handle_list_nodes(const Request& req, Response& res,
                              cacherouter::CacheCluster& cluster) {
    std::ignore = req;

    auto nodes = json::array();
    for (const auto& node : cluster.list_nodes()) {
        nodes.push_back({{"id", node.id},
                         {"capacity", node.capacity},
                         {"policy", node.policy_name},
                         {"virtual_nodes", node.virtual_nodes},
                         {"used", node.size}});
    }
    res.status = 200;
    res.set_content(json(nodes).dump(), "application/json");
}


inline void handle_add_node(const Request& req, Response& res, cacherouter::CacheCluster& cluster) {
    try {
        const auto body = json::parse(req.body);
        cacherouter::NodeSpec node{
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
            json{{"key", key}, {"value", *value}, {"hash", std::to_string(cacherouter::hash(key))}}
                .dump(),
            "application/json");
    } else {
        res.status = 404;
        res.set_content(
            json{{"error", "not found"}, {"hash", std::to_string(cacherouter::hash(key))}}.dump(),
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
            json{{"key", key}, {"value", value}, {"hash", std::to_string(cacherouter::hash(key))}}
                .dump(),
            "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json{{"error", e.what()}}.dump(), "application/json");
    }
}


// Each router is drawn differently, so the payload is shaped per router rather
// than forced into a shared schema: a ring of hash points for ConsistentRouter,
// an ordered bucket list (index == hash % N) for SimpleRouter.
inline json router_topology(const cacherouter::CacheCluster& cluster) {
    const auto& router = cluster.router();

    if (const auto* consistent =
            dynamic_cast<const cacherouter::router::ConsistentRouter*>(&router)) {
        auto ring = json::array();
        for (const auto& [hash, node] : consistent->ring()) {
            ring.push_back({{"hash", std::to_string(hash)}, {"node", node}});
        }
        return json{{"ring", ring}};
    }

    return json{{"buckets", router.nodes()}};
}


inline json router_state(const cacherouter::CacheCluster& cluster) {
    return json{{"name", cluster.router().name()},
                {"available", cacherouter::router::available_routers()},
                {"topology", router_topology(cluster)}};
}


inline void handle_get_router(const Request& req, Response& res,
                              cacherouter::CacheCluster& cluster) {
    std::ignore = req;

    res.status = 200;
    res.set_content(router_state(cluster).dump(), "application/json");
}


inline void handle_set_router(const Request& req, Response& res,
                              cacherouter::CacheCluster& cluster) {
    try {
        const auto body = json::parse(req.body);
        const auto name = body.at("name").get<std::string>();

        if (name == cluster.router().name()) {
            res.status = 200;
            res.set_content(router_state(cluster).dump(), "application/json");
            return;
        }

        cluster.set_router(cacherouter::router::make_router(name));
        // Placements made under the old router do not survive the swap.
        cluster.clear_data();

        res.status = 200;
        res.set_content(router_state(cluster).dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json{{"error", e.what()}}.dump(), "application/json");
    }
}


inline void handle_reset(const Request& req, Response& res, cacherouter::CacheCluster& cluster) {
    auto scope = std::string{"data"};
    if (!req.body.empty()) {
        try {
            const auto body = json::parse(req.body);
            if (auto it = body.find("scope"); it != body.end()) scope = it->get<std::string>();
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
            return;
        }
    }

    if (scope == "data") {
        cluster.clear_data();
    } else if (scope == "all") {
        cluster.clear_nodes();
    } else if (scope == "default") {
        cluster.clear_nodes();
        cluster.reset_to_default();
    } else {
        res.status = 400;
        res.set_content(json{{"error", R"(scope must be "data" or "all" or "default")"}}.dump(),
                        "application/json");
        return;
    }

    res.status = 200;
    res.set_content(json{{"scope", scope}, {"nodes", cluster.list_nodes().size()}}.dump(),
                    "application/json");
}


inline void handle_get_ring(const Request& req, Response& res, cacherouter::CacheCluster& cluster) {
    std::ignore = req;

    const auto topology = router_topology(cluster);
    if (!topology.contains("ring")) {
        res.status = 409;
        res.set_content(
            json{{"error", "active router has no hash ring"}, {"router", cluster.router().name()}}
                .dump(),
            "application/json");
        return;
    }

    res.status = 200;
    res.set_content(topology.dump(), "application/json");
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


inline void register_router(Server& server, cacherouter::CacheCluster& cluster) {
    server.Get("/api/router", [&cluster](const Request& req, Response& res) {
        handle_get_router(req, res, cluster);
    });
    server.Post("/api/router", [&cluster](const Request& req, Response& res) {
        handle_set_router(req, res, cluster);
    });
    server.Post("/api/reset",
                [&cluster](const Request& req, Response& res) { handle_reset(req, res, cluster); });
    server.Get("/api/ring", [&cluster](const Request& req, Response& res) {
        handle_get_ring(req, res, cluster);
    });
}
