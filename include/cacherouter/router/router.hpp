#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cacherouter::router {

using NodeId = std::string;

class Router {
public:
    virtual ~Router() = default;

    virtual void add_node(const NodeId& node) = 0;
    virtual void remove_node(const NodeId& node) = 0;

    [[nodiscard]] virtual NodeId route(const std::string& key) const = 0;
    [[nodiscard]] virtual std::vector<NodeId> nodes() const = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

}  // namespace cacherouter::router
