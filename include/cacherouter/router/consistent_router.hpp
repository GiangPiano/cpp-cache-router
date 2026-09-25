#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "cacherouter/node.hpp"
#include "cacherouter/router/hash_ring.hpp"
#include "cacherouter/router/router.hpp"

namespace cacherouter::router {

class ConsistentRouter final : public Router {
public:
    explicit ConsistentRouter(int virtual_nodes = 100);

    void add_node(const NodeId& node, int virtual_nodes = 100) override;
    void remove_node(const NodeId& node) override;

    [[nodiscard]] NodeId route(const std::string& key) const override;
    [[nodiscard]] std::vector<NodeId> nodes() const override;
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] const HashRing& ring() const;

private:
    HashRing ring_;
};

}  // namespace cacherouter::router
