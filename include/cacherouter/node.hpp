#pragma once

#include <cstddef>
#include <string>

namespace cacherouter {

using NodeId = std::string;

struct Node {
    NodeId id;
    size_t capacity;
    std::string policy_name;
    int virtual_nodes = 100;
};

}  // namespace cacherouter
