/**
 * This file contains structs of the NASDAQ TotalView-ITCH 5.0 messages.
 */

#ifndef ORDER_BOOK_SRC_ITCH_MESSAGES_H_
#define ORDER_BOOK_SRC_ITCH_MESSAGES_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace itch {
constexpr std::array<uint8_t, 256> MakeMessageLengths() {
  std::array<uint8_t, 256> a{};
  // System / stock-level
  a[static_cast<std::size_t>('S')] = 12;  // System Event
  a[static_cast<std::size_t>('R')] = 39;  // Stock Directory
  a[static_cast<std::size_t>('H')] = 25;  // Stock Trading Action
  a[static_cast<std::size_t>('Y')] = 20;  // Reg SHO Restriction
  a[static_cast<std::size_t>('L')] = 26;  // Market Participant Position
  a[static_cast<std::size_t>('V')] = 35;  // MWCB Decline Level
  a[static_cast<std::size_t>('W')] = 12;  // MWCB Status
  a[static_cast<std::size_t>('K')] = 28;  // IPO Quoting Period Update
  a[static_cast<std::size_t>('J')] = 35;  // LULD Auction Collar
  a[static_cast<std::size_t>('h')] = 21;  // Operational Halt
  // Orders
  a[static_cast<std::size_t>('A')] = 36;  // Add Order (no MPID)
  a[static_cast<std::size_t>('F')] = 40;  // Add Order (with MPID)
  a[static_cast<std::size_t>('E')] = 31;  // Order Executed
  a[static_cast<std::size_t>('C')] = 36;  // Order Executed With Price
  a[static_cast<std::size_t>('X')] = 23;  // Order Cancel
  a[static_cast<std::size_t>('D')] = 19;  // Order Delete
  a[static_cast<std::size_t>('U')] = 35;  // Order Replace
  // Trades
  a[static_cast<std::size_t>('P')] = 44;  // Trade (non-cross)
  a[static_cast<std::size_t>('Q')] = 40;  // Cross Trade
  a[static_cast<std::size_t>('B')] = 19;  // Broken Trade
  // Auctions / other
  a[static_cast<std::size_t>('I')] = 50;  // NOII
  a[static_cast<std::size_t>('N')] = 20;  // Retail Price Improvement Indicator
  a[static_cast<std::size_t>('O')] = 48;  // Direct Listing with Capital Raise
  return a;
}
inline constexpr std::array<uint8_t, 256> kMessageLengths =
    MakeMessageLengths();
inline constexpr std::size_t kMaxMessageLength =
    std::ranges::max(kMessageLengths);

enum class BuySellIndicator : uint8_t { kBuy = 'B', kSell = 'S' };

// For message types "A" and "F"
struct AddOrderMessage {
  uint64_t reference_number;
  uint64_t timestamp;
  std::array<char, 8> stock;
  uint32_t shares;
  uint32_t price;
  uint16_t stock_locate;
  BuySellIndicator buy_sell_indicator;
};

// For message types "E" and "C"
struct ExecuteOrderMessage {
  uint64_t reference_number;
  uint64_t timestamp;
  uint32_t shares;
  uint16_t stock_locate;
};

// For message types "X"
struct CancelOrderMessage {
  uint64_t reference_number;
  uint64_t timestamp;
  uint32_t shares;
  uint16_t stock_locate;
};

// For message types "D"
struct DeleteOrderMessage {
  uint64_t reference_number;
  uint64_t timestamp;
  uint16_t stock_locate;
};

// For message types "U"
struct ReplaceOrderMessage {
  uint64_t original_reference_number;
  uint64_t new_reference_number;
  uint64_t timestamp;
  uint32_t shares;
  uint32_t price;
  uint16_t stock_locate;
};
}  // namespace itch

#endif  // ORDER_BOOK_SRC_ITCH_MESSAGES_H_