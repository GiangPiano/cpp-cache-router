#pragma once

#include <cstdint>
#include <string>

namespace cacherouter {

// The hash every router places keys and nodes with. Declared here and defined in
// the core library so murmur3 stays an implementation detail: consumers get the
// function without inheriting the vendored dependency's headers or symbols.
[[nodiscard]] std::uint64_t hash(const std::string& key);

}  // namespace cacherouter
