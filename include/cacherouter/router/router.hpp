#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "cacherouter/node.hpp"

namespace cacherouter::router {
class Router {
public:
    virtual ~Router() = default;

    virtual void add_node(const NodeId& node, int virtual_nodes = 100) = 0;
    virtual void remove_node(const NodeId& node) = 0;
    virtual void clear() = 0;

    [[nodiscard]] virtual NodeId route(const std::string& key) const = 0;
    [[nodiscard]] virtual std::vector<NodeId> nodes() const = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

}  // namespace cacherouter::router
