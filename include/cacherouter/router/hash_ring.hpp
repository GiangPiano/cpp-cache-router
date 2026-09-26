#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cacherouter::router {

using Ring = std::map<uint64_t, std::string>;

class HashRing {
public:
    explicit HashRing(int virtual_nodes);

    void add_node(const std::string& node, int virtual_nodes);
    void remove_node(const std::string& node);

    [[nodiscard]] std::optional<std::string> find_node(const std::string& key) const;
    [[nodiscard]] std::vector<std::string> nodes() const;
    [[nodiscard]] const Ring& get_ring() const;

private:
    int vnodes_;
    Ring ring_;
};

}  // namespace cacherouter::router
