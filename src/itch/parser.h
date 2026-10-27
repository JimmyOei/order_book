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

    if constexpr (requires(const unsigned char& t) { handler_.OnMessage(t); }) {
      handler_.OnMessage(message_type);
    }

    switch (message_type) {
      case 'A':
      case 'F':
        if constexpr (requires(const AddOrderMessage& m) {
                        handler_.OnAddOrderMessage(m);
                      }) {
          AddOrderMessage message;
          message.reference_number = ReadBe64(&buf[11]);
          message.timestamp = ReadBe48(&buf[5]);
          message.shares = ReadBe32(&buf[20]);
          message.price = ReadBe32(&buf[32]);
          message.stock_locate = ReadBe16(&buf[1]);

          if (buf[19] != 'B' && buf[19] != 'S') {
            throw std::runtime_error(
                "Invalid Buy/Sell Indicator for Add Order message.");
          }
          message.buy_sell_indicator = static_cast<BuySellIndicator>(buf[19]);

          std::memcpy(message.stock.data(), &buf[24], sizeof(message.stock));

          handler_.OnAddOrderMessage(message);
        }
        return;
      case 'E':
      case 'C':
        if constexpr (requires(const ExecuteOrderMessage& m) {
                        handler_.OnExecuteOrderMessage(m);
                      }) {
          ExecuteOrderMessage message{.reference_number = ReadBe64(&buf[11]),
                                      .timestamp = ReadBe48(&buf[5]),
                                      .shares = ReadBe32(&buf[19]),
                                      .stock_locate = ReadBe16(&buf[1])};
          handler_.OnExecuteOrderMessage(message);
        }
        return;
      case 'X':
        if constexpr (requires(const CancelOrderMessage& m) {
                        handler_.OnCancelOrderMessage(m);
                      }) {
          CancelOrderMessage message{.reference_number = ReadBe64(&buf[11]),
                                     .timestamp = ReadBe48(&buf[5]),
                                     .shares = ReadBe32(&buf[19]),
                                     .stock_locate = ReadBe16(&buf[1])};
          handler_.OnCancelOrderMessage(message);
        }
        return;
      case 'D':
        if constexpr (requires(const DeleteOrderMessage& m) {
                        handler_.OnDeleteOrderMessage(m);
                      }) {
          DeleteOrderMessage message{.reference_number = ReadBe64(&buf[11]),
                                     .timestamp = ReadBe48(&buf[5]),
                                     .stock_locate = ReadBe16(&buf[1])};
          handler_.OnDeleteOrderMessage(message);
        }
        return;
      case 'U':
        if constexpr (requires(const ReplaceOrderMessage& m) {
                        handler_.OnReplaceOrderMessage(m);
                      }) {
          ReplaceOrderMessage message{
              .original_reference_number = ReadBe64(&buf[11]),
              .new_reference_number = ReadBe64(&buf[19]),
              .timestamp = ReadBe48(&buf[5]),
              .shares = ReadBe32(&buf[27]),
              .price = ReadBe32(&buf[31]),
              .stock_locate = ReadBe16(&buf[1])};
          handler_.OnReplaceOrderMessage(message);
        }
        return;
      default:
        return;
    }
  }
};
}  // namespace itch

#endif  // ORDER_BOOK_SRC_ITCH_PARSER_H_