#ifndef DECOMP_SRC_DEFS_H
#define DECOMP_SRC_DEFS_H

#include <climits>
#include <cstddef>
#include <cstdint>

namespace wasm {

namespace decode {

using IntType = uint64_t;
static constexpr size_t kBitsInIntType = 64;

enum class StreamType {Bit, Byte, Int, Ast};

void fatal(const char *Message = "fatal: unable to continue");

struct Utils {
  static constexpr size_t floorByte(size_t Bit) {
    return Bit >> 3;
  }

  static size_t ceilByte(size_t Bit) {
    size_t Byte = floorByte(Bit);
    return (Byte & 0x7)
        ? Byte + 1
        : Byte;
  }
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMP_SRC_DEFS_H
