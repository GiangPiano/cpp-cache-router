#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cacherouter::router {

class HashRing {
public:
    explicit HashRing(int virtual_nodes);

    void add_node(const std::string& node, std::optional<int> virtual_nodes = std::nullopt);
    void remove_node(const std::string& node);

    [[nodiscard]] std::optional<std::string> find_node(const std::string& key) const;
    [[nodiscard]] std::vector<std::string> nodes() const;
    [[nodiscard]] const std::map<uint64_t, std::string>& get_ring() const;

private:
    int vnodes_;
    std::map<uint64_t, std::string> ring_;
};

}  // namespace cacherouter::router
