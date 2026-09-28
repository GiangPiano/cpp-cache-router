#pragma once

#include <cstddef>
#include <string>

namespace cacherouter {

using NodeId = std::string;

struct NodeSpec {
    NodeId id;
    size_t capacity;
    std::string policy_name;
    int virtual_nodes = 100;
    size_t size = 0;
};

}  // namespace cacherouter
