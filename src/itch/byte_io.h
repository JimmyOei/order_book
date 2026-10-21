/**
 * This file contains helpers for reading NASDAQ TotalView-ITCH 5.0 data from a
 * binary file.
 */

#ifndef ORDER_BOOK_SRC_ITCH_BYTE_IO_H_
#define ORDER_BOOK_SRC_ITCH_BYTE_IO_H_

#include <cstdint>

namespace itch {
constexpr uint16_t ReadBe16(const unsigned char* buf) {
  return static_cast<uint16_t>((static_cast<uint16_t>(buf[0]) << 8) |
                               static_cast<uint16_t>(buf[1]));
}

constexpr uint32_t ReadBe32(const unsigned char* buf) {
  return (static_cast<uint32_t>(buf[0]) << 24) |
         (static_cast<uint32_t>(buf[1]) << 16) |
         (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
}

constexpr uint64_t ReadBe48(const unsigned char* buf) {
  return (static_cast<uint64_t>(buf[0]) << 40) |
         (static_cast<uint64_t>(buf[1]) << 32) |
         (static_cast<uint64_t>(buf[2]) << 24) |
         (static_cast<uint64_t>(buf[3]) << 16) |
         (static_cast<uint64_t>(buf[4]) << 8) | static_cast<uint64_t>(buf[5]);
}

constexpr uint64_t ReadBe64(const unsigned char* buf) {
  return (static_cast<uint64_t>(buf[0]) << 56) |
         (static_cast<uint64_t>(buf[1]) << 48) |
         (static_cast<uint64_t>(buf[2]) << 40) |
         (static_cast<uint64_t>(buf[3]) << 32) |
         (static_cast<uint64_t>(buf[4]) << 24) |
         (static_cast<uint64_t>(buf[5]) << 16) |
         (static_cast<uint64_t>(buf[6]) << 8) | static_cast<uint64_t>(buf[7]);
}
}  // namespace itch

#endif  // ORDER_BOOK_SRC_ITCH_BYTE_IO_H_