/**
 * This file contains the parser for parsing NASDAQ TotalView-ITCH 5.0 binary
 * data and calling the given handler for each message.
 */

#ifndef ORDER_BOOK_SRC_ITCH_PARSER_H_
#define ORDER_BOOK_SRC_ITCH_PARSER_H_

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "itch/byte_io.h"
#include "itch/messages.h"

namespace itch {
template <typename Handler>
class Parser {
 public:
  explicit Parser(Handler& handler) : handler_(handler) {}

  void Parse(const unsigned char* buf, std::size_t size) {
    if (size == 0) {
      return;
    }

    if (buf_remainder_size_ > 0) {
      if (buf_remainder_size_ == 1) {
        // copy remaining byte to have value of message size in buf_remainder_
        std::memcpy(&buf_remainder_[1], buf, 1);
        buf_remainder_size_++;
        buf++;
        size--;
      }

      uint16_t message_size = ReadBe16(&buf_remainder_[0]);
      if (message_size > kMaxMessageSize || message_size == 0) {
        throw std::runtime_error("Invalid message size.");
      }

      std::size_t bytes_to_copy = message_size + 2 - buf_remainder_size_;
      if (bytes_to_copy > size) {
        if (size > 0) {
          std::memcpy(&buf_remainder_[buf_remainder_size_], buf, size);
          buf_remainder_size_ += size;
        }
        return;
      }

      std::memcpy(&buf_remainder_[buf_remainder_size_], buf, bytes_to_copy);
      buf += bytes_to_copy;
      size -= bytes_to_copy;
      ParseMessage(&buf_remainder_[2], message_size);
      buf_remainder_size_ = 0;
    }

    // TODO: parsing loop for buf
  }

 private:
  Handler& handler_;

  // Maximum remainder size is
  // max message size + bytes indicating message size (= 2) - 1
  std::array<unsigned char, kMaxMessageSize + 1> buf_remainder_;
  std::size_t buf_remainder_size_ = 0;

  void ParseMessage(const unsigned char* buf, const std::size_t size) {
    // TODO: parse a given complete message from given buf
  }
};
}  // namespace itch

#endif  // ORDER_BOOK_SRC_ITCH_PARSER_H_