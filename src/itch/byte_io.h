/**
 * This file contains helpers for reading NASDAQ TotalView-ITCH 5.0 data from a
 * binary file.
 */

#ifndef ORDER_BOOK_SRC_ITCH_BYTE_IO_H_
#define ORDER_BOOK_SRC_ITCH_BYTE_IO_H_

#include <cstdint>

namespace itch {
inline constexpr uint16_t ReadBe16(const unsigned char* /*buf*/) {
  // TODO: read big-endian 2 bytes from buf
  return {};
}

inline constexpr uint32_t ReadBe32(const unsigned char* /*buf*/) {
  // TODO: read big-endian 4 bytes from buf
  return {};
}

inline constexpr uint64_t ReadBe48(const unsigned char* /*buf*/) {
  // TODO: read big-endian 6 bytes from buf
  return {};
}

inline constexpr uint64_t ReadBe64(const unsigned char* /*buf*/) {
  // TODO: read big-endian 8 bytes from buf
  return {};
}
}  // namespace itch

#endif  // ORDER_BOOK_SRC_ITCH_BYTE_IO_H_