#include "cacherouter/utils/hash.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <string>

#include "MurmurHash3.h"

namespace cacherouter {

namespace {

// Seeded once per process. Placements are therefore consistent for the lifetime
// of a run but differ between runs; swap in a fixed constant to make the ring
// reproducible across restarts.
std::uint32_t seed() {
    static const std::uint32_t value = std::random_device{}();
    return value;
}

}  // namespace

[[nodiscard]] std::uint64_t hash(const std::string& key) {
    std::array<std::uint64_t, 2> out = {0, 0};
    MurmurHash3_x64_128(key.data(), static_cast<int>(key.size()), seed(), &out);
    return out[0];
}

}  // namespace cacherouter
