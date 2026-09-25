#pragma once

#include <string>
#include <vector>

#include "cacherouter/node.hpp"
#include "cacherouter/router/router.hpp"

namespace cacherouter::router {

class SimpleRouter final : public Router {
public:
    void add_node(const NodeId& node, int virtual_nodes = 100) override;
    void remove_node(const NodeId& node) override;

    [[nodiscard]] NodeId route(const std::string& key) const override;
    [[nodiscard]] std::vector<NodeId> nodes() const override;
    [[nodiscard]] std::string name() const override;

private:
    std::vector<NodeId> nodes_;
};

}  // namespace cacherouter::router
