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

      const uint16_t message_size = ReadBe16(&buf_remainder_[0]);
      if (message_size > kMaxMessageLength || message_size == 0) {
        throw std::runtime_error("Invalid message size.");
      }

      const std::size_t bytes_to_copy = message_size + 2 - buf_remainder_size_;
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

    while (size > 0) {
      if (size == 1) {
        std::memcpy(buf_remainder_.data(), buf, size);
        buf_remainder_size_ = size;
        break;
      }

      const uint16_t message_size = ReadBe16(buf);
      if (message_size > kMaxMessageLength || message_size == 0) {
        throw std::runtime_error("Invalid message size.");
      }

      if (size < static_cast<size_t>(message_size) + 2) {
        std::memcpy(buf_remainder_.data(), buf, size);
        buf_remainder_size_ = size;
        break;
      }

      buf += 2;
      size -= 2;
      ParseMessage(buf, message_size);
      buf += message_size;
      size -= message_size;
    }
  }

  [[nodiscard]] bool HasPendingBytes() const {
    return buf_remainder_size_ != 0;
  }

 private:
  Handler& handler_;

  // Maximum remainder size is
  // max message size + bytes indicating message size (= 2)
  std::array<unsigned char, kMaxMessageLength + 2> buf_remainder_;
  std::size_t buf_remainder_size_ = 0;

  void ParseMessage(const unsigned char* buf, const std::size_t size) {
    const unsigned char message_type = buf[0];

    const uint8_t expected_message_length =
        kMessageLengths[static_cast<std::size_t>(message_type)];
    if (expected_message_length == 0) {
      throw std::runtime_error("Found an unknown message type.");
    }
    if (expected_message_length != size) {
      throw std::runtime_error(
          "Found a message with incorrect message length.");
    }

    handler_.OnMessage(message_type);
  }
};
}  // namespace itch

#endif  // ORDER_BOOK_SRC_ITCH_PARSER_H_