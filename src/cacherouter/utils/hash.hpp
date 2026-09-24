#include <array>
#include <cstdint>
#include <random>
#include <string>

#include "MurmurHash3.h"

static inline std::uint32_t get_random_seed() {
    static const uint32_t seed = std::random_device{}();
    return seed;
}

// inline std::uint64_t hash_key(const std::string& s) { return std::hash<std::string>{}(s); }
inline std::uint64_t hash_key(const std::string& s) {
    std::array<uint64_t, 2> out = {0, 0};
    MurmurHash3_x64_128(s.data(), static_cast<int>(s.size()), get_random_seed(), &out);
    // std::cout << s << " hashed to " << out[0] << '\n';
    return out[0];
}
