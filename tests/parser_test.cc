/**
 * parser_test.cc
 *
 * Unit tests for itch::Parser.
 */

#include "itch/parser.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr uint16_t kLocate = 0x1234;
constexpr uint64_t kTimestamp = 0x0A0B0C0D0E0F;  // 48-bit
constexpr uint64_t kReference = 0x0102030405060708;
constexpr uint64_t kNewReference = 0x1112131415161718;
constexpr uint32_t kShares = 100;
constexpr uint32_t kPrice = 1234500;  // $123.45, 4 implied decimals
constexpr std::array<char, 8> kStock{'J', 'I', 'M', 'M', 'Y', ' ', ' ', ' '};

// Appends to out the num_bytes from value in big endian
void AppendBe(Bytes& out, uint64_t value, int num_bytes) {
  for (int i = num_bytes - 1; i >= 0; --i) {
    out.push_back(static_cast<unsigned char>(value >> (8 * i)));
  }
}

// Appends to out the characters as raw bytes
void AppendText(Bytes& out, std::string_view text) {
  for (const char c : text) {
    out.push_back(static_cast<unsigned char>(c));
  }
}

// Wraps a message body in ITCH binary file framing: [u16 BE length][body].
Bytes Frame(const Bytes& body) {
  Bytes framed;
  AppendBe(framed, body.size(), 2);
  framed.insert(framed.end(), body.begin(), body.end());
  return framed;
}

// Concatenates framed messages into one input stream.
Bytes Concat(const std::vector<Bytes>& parts) {
  Bytes out;
  for (const Bytes& part : parts) {
    out.insert(out.end(), part.begin(), part.end());
  }
  return out;
}

// Fields shared by every message: type, stock locate, tracking, timestamp.
Bytes Header(char type) {
  Bytes body;
  AppendText(body, std::string_view(&type, 1));  // 0 message type
  AppendBe(body, kLocate, 2);                    // 1 stock locate
  AppendBe(body, 0, 2);                          // 3 tracking number
  AppendBe(body, kTimestamp, 6);                 // 5 timestamp
  return body;
}

// 'A' (36 bytes), or 'F' (40 bytes) with a trailing 4-byte MPID.
Bytes AddOrderBody(char type = 'A', char side = 'B') {
  Bytes body = Header(type);
  AppendBe(body, kReference, 8);                     // 11 order reference
  AppendText(body, std::string_view(&side, 1));      // 19 buy/sell indicator
  AppendBe(body, kShares, 4);                        // 20 shares
  AppendText(body, {kStock.data(), kStock.size()});  // 24 stock
  AppendBe(body, kPrice, 4);                         // 32 price
  if (type == 'F') {
    AppendText(body, "MPID");  // 36 attribution
  }
  return body;
}

// 'E' (31 bytes), or 'C' (36 bytes) with printable flag + execution price.
Bytes OrderExecutedBody(char type = 'E') {
  Bytes body = Header(type);
  AppendBe(body, kReference, 8);  // 11 order reference
  AppendBe(body, kShares, 4);     // 19 executed shares
  AppendBe(body, 0xABCDEF, 8);    // 23 match number
  if (type == 'C') {
    AppendText(body, "Y");      // 31 printable
    AppendBe(body, kPrice, 4);  // 32 execution price
  }
  return body;
}

// 'X' (23 bytes)
Bytes OrderCancelBody() {
  Bytes body = Header('X');
  AppendBe(body, kReference, 8);  // 11 order reference
  AppendBe(body, kShares, 4);     // 19 cancelled shares
  return body;
}

// 'D' (19 bytes)
Bytes OrderDeleteBody() {
  Bytes body = Header('D');
  AppendBe(body, kReference, 8);  // 11 order reference
  return body;
}

// 'U' (35 bytes)
Bytes OrderReplaceBody() {
  Bytes body = Header('U');
  AppendBe(body, kReference, 8);     // 11 original order reference
  AppendBe(body, kNewReference, 8);  // 19 new order reference
  AppendBe(body, kShares, 4);        // 27 shares
  AppendBe(body, kPrice, 4);         // 31 price
  return body;
}

// Any message type the parser validates but doesn't decode, padded with
// zeros to `length` bytes. E.g. 'S' (12) or 'I' (50, the longest).
Bytes OpaqueBody(char type, std::size_t length) {
  Bytes body;
  AppendText(body, std::string_view(&type, 1));
  body.resize(length, 0);
  return body;
}

// Records every message the parser hands over
class RecordingHandler {
 public:
  void OnMessage(unsigned char message_type) { types.push_back(message_type); }

  void OnAddOrderMessage(const itch::AddOrderMessage& message) {
    add_orders.push_back(message);
  }

  void OnCancelOrderMessage(const itch::CancelOrderMessage& message) {
    cancel_orders.push_back(message);
  }

  void OnExecuteOrderMessage(const itch::ExecuteOrderMessage& message) {
    execute_orders.push_back(message);
  }

  void OnDeleteOrderMessage(const itch::DeleteOrderMessage& message) {
    delete_orders.push_back(message);
  }

  void OnReplaceOrderMessage(const itch::ReplaceOrderMessage& message) {
    replace_orders.push_back(message);
  }

  std::vector<unsigned char> types;
  std::vector<itch::AddOrderMessage> add_orders;
  std::vector<itch::CancelOrderMessage> cancel_orders;
  std::vector<itch::ExecuteOrderMessage> execute_orders;
  std::vector<itch::DeleteOrderMessage> delete_orders;
  std::vector<itch::ReplaceOrderMessage> replace_orders;
};

// Parses `input` in one call and returns what the handler received.
RecordingHandler ParseAll(const Bytes& input) {
  RecordingHandler handler;
  itch::Parser<RecordingHandler> parser{handler};
  parser.Parse(input.data(), input.size());
  EXPECT_FALSE(parser.HasPendingBytes());
  return handler;
}

RecordingHandler ParseOne(const Bytes& body) { return ParseAll(Frame(body)); }

TEST(TestHelpersTest, AppendBeWritesBytesInBigEndian) {
  Bytes out;
  AppendBe(out, 0x0102030405060708, 8);
  EXPECT_EQ(out, (Bytes{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}));
}

TEST(TestHelpersTest, BuildersMatchSpecLengths) {
  EXPECT_EQ(AddOrderBody('A').size(), 36U);
  EXPECT_EQ(AddOrderBody('F').size(), 40U);
  EXPECT_EQ(OrderExecutedBody('E').size(), 31U);
  EXPECT_EQ(OrderExecutedBody('C').size(), 36U);
  EXPECT_EQ(OrderCancelBody().size(), 23U);
  EXPECT_EQ(OrderDeleteBody().size(), 19U);
  EXPECT_EQ(OrderReplaceBody().size(), 35U);
}

TEST(ParserTest, DecodesAddOrder) {
  const RecordingHandler handler = ParseOne(AddOrderBody('A'));

  ASSERT_EQ(handler.add_orders.size(), 1U);
  const itch::AddOrderMessage& add = handler.add_orders[0];
  EXPECT_EQ(add.stock_locate, kLocate);
  EXPECT_EQ(add.timestamp, kTimestamp);
  EXPECT_EQ(add.reference_number, kReference);
  EXPECT_EQ(add.buy_sell_indicator, itch::BuySellIndicator::kBuy);
  EXPECT_EQ(add.shares, kShares);
  EXPECT_EQ(add.stock, kStock);
  EXPECT_EQ(add.price, kPrice);
}

TEST(ParserTest, DecodesAddOrderWithMpid) {
  const RecordingHandler handler = ParseOne(AddOrderBody('F'));

  ASSERT_EQ(handler.add_orders.size(), 1U);
  const itch::AddOrderMessage& add = handler.add_orders[0];
  EXPECT_EQ(add.stock_locate, kLocate);
  EXPECT_EQ(add.timestamp, kTimestamp);
  EXPECT_EQ(add.reference_number, kReference);
  EXPECT_EQ(add.buy_sell_indicator, itch::BuySellIndicator::kBuy);
  EXPECT_EQ(add.shares, kShares);
  EXPECT_EQ(add.stock, kStock);
  EXPECT_EQ(add.price, kPrice);
}

TEST(ParserTest, DecodesSellSide) {
  const RecordingHandler handler = ParseOne(AddOrderBody('A', 'S'));

  ASSERT_EQ(handler.add_orders.size(), 1U);
  EXPECT_EQ(handler.add_orders[0].buy_sell_indicator,
            itch::BuySellIndicator::kSell);
}

TEST(ParserTest, DecodesOrderExecuted) {
  const RecordingHandler handler = ParseOne(OrderExecutedBody('E'));

  ASSERT_EQ(handler.execute_orders.size(), 1U);
  const itch::ExecuteOrderMessage& exec = handler.execute_orders[0];
  EXPECT_EQ(exec.stock_locate, kLocate);
  EXPECT_EQ(exec.timestamp, kTimestamp);
  EXPECT_EQ(exec.reference_number, kReference);
  EXPECT_EQ(exec.shares, kShares);
}

TEST(ParserTest, DecodesOrderExecutedWithPrice) {
  const RecordingHandler handler = ParseOne(OrderExecutedBody('C'));

  ASSERT_EQ(handler.execute_orders.size(), 1U);
  const itch::ExecuteOrderMessage& exec = handler.execute_orders[0];
  EXPECT_EQ(exec.stock_locate, kLocate);
  EXPECT_EQ(exec.timestamp, kTimestamp);
  EXPECT_EQ(exec.reference_number, kReference);
  EXPECT_EQ(exec.shares, kShares);
}

TEST(ParserTest, DecodesOrderCancel) {
  const RecordingHandler handler = ParseOne(OrderCancelBody());

  ASSERT_EQ(handler.cancel_orders.size(), 1U);
  const itch::CancelOrderMessage& cancel = handler.cancel_orders[0];
  EXPECT_EQ(cancel.stock_locate, kLocate);
  EXPECT_EQ(cancel.timestamp, kTimestamp);
  EXPECT_EQ(cancel.reference_number, kReference);
  EXPECT_EQ(cancel.shares, kShares);
}

TEST(ParserTest, DecodesOrderDelete) {
  const RecordingHandler handler = ParseOne(OrderDeleteBody());

  ASSERT_EQ(handler.delete_orders.size(), 1U);
  const itch::DeleteOrderMessage& del = handler.delete_orders[0];
  EXPECT_EQ(del.stock_locate, kLocate);
  EXPECT_EQ(del.timestamp, kTimestamp);
  EXPECT_EQ(del.reference_number, kReference);
}

TEST(ParserTest, DecodesOrderReplace) {
  const RecordingHandler handler = ParseOne(OrderReplaceBody());

  ASSERT_EQ(handler.replace_orders.size(), 1U);
  const itch::ReplaceOrderMessage& replace = handler.replace_orders[0];
  EXPECT_EQ(replace.stock_locate, kLocate);
  EXPECT_EQ(replace.timestamp, kTimestamp);
  EXPECT_EQ(replace.original_reference_number, kReference);
  EXPECT_EQ(replace.new_reference_number, kNewReference);
  EXPECT_EQ(replace.shares, kShares);
  EXPECT_EQ(replace.price, kPrice);
}

TEST(ParserTest, IgnoresMessageTypesItDoesNotDecode) {
  const RecordingHandler handler = ParseOne(OpaqueBody('S', 12));

  EXPECT_EQ(handler.types, (std::vector<unsigned char>{'S'}));
  EXPECT_TRUE(handler.add_orders.empty());
  EXPECT_TRUE(handler.execute_orders.empty());
  EXPECT_TRUE(handler.cancel_orders.empty());
  EXPECT_TRUE(handler.delete_orders.empty());
  EXPECT_TRUE(handler.replace_orders.empty());
}

TEST(ParserTest, ParsesSeveralMessagesInOneBuffer) {
  const RecordingHandler handler = ParseAll(
      Concat({Frame(OpaqueBody('S', 12)), Frame(AddOrderBody('A')),
              Frame(OrderExecutedBody('E')), Frame(OrderDeleteBody())}));

  EXPECT_EQ(handler.types, (std::vector<unsigned char>{'S', 'A', 'E', 'D'}));
  EXPECT_EQ(handler.add_orders.size(), 1U);
  EXPECT_EQ(handler.execute_orders.size(), 1U);
  EXPECT_EQ(handler.delete_orders.size(), 1U);
}

TEST(ParserTest, ParsesInputSplitAtEveryOffset) {
  const Bytes input =
      Concat({Frame(OpaqueBody('I', 50)), Frame(AddOrderBody('A')),
              Frame(OpaqueBody('I', 50))});

  for (std::size_t split = 0; split <= input.size(); ++split) {
    SCOPED_TRACE(testing::Message() << "split at byte " << split);
    RecordingHandler handler;
    itch::Parser<RecordingHandler> parser{handler};

    parser.Parse(input.data(), split);
    parser.Parse(input.data() + split, input.size() - split);

    EXPECT_EQ(handler.types, (std::vector<unsigned char>{'I', 'A', 'I'}));
    ASSERT_EQ(handler.add_orders.size(), 1U);
    EXPECT_EQ(handler.add_orders[0].reference_number, kReference);
    EXPECT_EQ(handler.add_orders[0].stock, kStock);
    EXPECT_EQ(handler.add_orders[0].price, kPrice);
    EXPECT_FALSE(parser.HasPendingBytes());
  }
}

TEST(ParserTest, ParsesInputFedOneByteAtATime) {
  const Bytes input =
      Concat({Frame(OpaqueBody('I', 50)), Frame(AddOrderBody('A')),
              Frame(OrderReplaceBody())});
  RecordingHandler handler;
  itch::Parser<RecordingHandler> parser{handler};

  for (std::size_t i = 0; i < input.size(); ++i) {
    parser.Parse(input.data() + i, 1);
  }

  EXPECT_EQ(handler.types, (std::vector<unsigned char>{'I', 'A', 'U'}));
  ASSERT_EQ(handler.add_orders.size(), 1U);
  EXPECT_EQ(handler.add_orders[0].price, kPrice);
  ASSERT_EQ(handler.replace_orders.size(), 1U);
  EXPECT_EQ(handler.replace_orders[0].new_reference_number, kNewReference);
  EXPECT_FALSE(parser.HasPendingBytes());
}

TEST(ParserTest, ReportsPendingBytesForTruncatedInput) {
  const Bytes input = Frame(AddOrderBody('A'));
  RecordingHandler handler;
  itch::Parser<RecordingHandler> parser{handler};

  parser.Parse(input.data(), input.size() - 1);

  EXPECT_TRUE(parser.HasPendingBytes());
  EXPECT_TRUE(handler.add_orders.empty());
}

TEST(ParserTest, IgnoresEmptyInput) {
  RecordingHandler handler;
  itch::Parser<RecordingHandler> parser{handler};

  parser.Parse(nullptr, 0);

  EXPECT_TRUE(handler.types.empty());
  EXPECT_FALSE(parser.HasPendingBytes());
}

TEST(ParserTest, ThrowsOnUnknownMessageType) {
  EXPECT_THROW(ParseOne(OpaqueBody('Z', 12)), std::runtime_error);
}

TEST(ParserTest, ThrowsOnWrongLengthForType) {
  Bytes body = AddOrderBody('A');
  body.pop_back();  // 35 bytes, but 'A' must be 36

  EXPECT_THROW(ParseOne(body), std::runtime_error);
}

TEST(ParserTest, ThrowsOnZeroLength) {
  EXPECT_THROW(ParseAll(Bytes{0x00, 0x00}), std::runtime_error);
}

TEST(ParserTest, ThrowsOnLengthAboveMaximum) {
  // 51 is one more than the longest message type.
  EXPECT_THROW(ParseOne(OpaqueBody('A', 51)), std::runtime_error);
}

TEST(ParserTest, ThrowsOnInvalidSide) {
  EXPECT_THROW(ParseOne(AddOrderBody('A', 'Z')), std::runtime_error);
}

}  // namespace
