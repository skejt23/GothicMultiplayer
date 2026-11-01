#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace Net {

inline constexpr std::size_t kMachineIdentityBytes = 32;
using MachineIdentity = std::array<unsigned char, kMachineIdentityBytes>;

// Derive a GMP-specific identifier from a 128-bit OS installation identifier.
// Invalid or uninitialized identifiers have no identity; never invent a random
// replacement that would silently change between connections or launches.
std::optional<MachineIdentity> DeriveMachineIdentity(std::string_view os_identifier);
std::optional<MachineIdentity> ReadMachineIdentity();

}  // namespace Net
