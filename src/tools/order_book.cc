/**
 * order_book.cc
 *
 * Keeps an order book for all stocks from the TotalView-ITCH 5.0 binary file
 * and prints the top of the book of given stock at fixed times of day.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "book/order_book_handler.h"
#include "itch/messages.h"
#include "itch/parser.h"

namespace {

constexpr std::size_t kLevelsToPrint = 5;

// ITCH timestamps are nanoseconds since midnight (New York time).
constexpr uint64_t TimeOfDay(uint64_t hours, uint64_t minutes,
                             uint64_t seconds) {
  return ((hours * 60 + minutes) * 60 + seconds) * 1'000'000'000ULL;
}

// Prints nanoseconds since midnight as HH:MM:SS.
void PrintTime(std::ostream& out, uint64_t timestamp) {
  const uint64_t total_seconds = timestamp / 1'000'000'000ULL;
  out << std::setfill('0') << std::setw(2) << total_seconds / 3600 << ':'
      << std::setw(2) << total_seconds / 60 % 60 << ':' << std::setw(2)
      << total_seconds % 60 << std::setfill(' ');
}

void PrintPrice(std::ostream& out, uint32_t price) {
  out << std::setw(6) << price / 10'000 << '.' << std::setfill('0')
      << std::setw(4) << price % 10'000 << std::setfill(' ');
}

// Prints one "shares @ price" cell, or blanks if that level doesn't exist.
void PrintLevel(std::ostream& out, const std::vector<book::PriceLevel>& levels,
                std::size_t i) {
  if (i < levels.size()) {
    out << std::setw(8) << levels[i].shares << " @ ";
    PrintPrice(out, levels[i].price);
  } else {
    out << std::setw(22) << "";
  }
}

void PrintBook(std::ostream& out, const book::OrderBookHandler& order_book,
               uint16_t stock_locate, const char* label, uint64_t timestamp) {
  const std::vector<book::PriceLevel> bids =
      order_book.TopNBid(stock_locate, kLevelsToPrint);
  const std::vector<book::PriceLevel> asks =
      order_book.TopNAsk(stock_locate, kLevelsToPrint);

  out << "=== " << label << " (";
  PrintTime(out, timestamp);
  out << ") ===\n";
  out << std::setw(22) << "BID" << "   |" << std::setw(22) << "ASK" << '\n';
  for (std::size_t i = 0; i < kLevelsToPrint; ++i) {
    PrintLevel(out, bids, i);
    out << "   |";
    PrintLevel(out, asks, i);
    out << '\n';
  }
  out << '\n';
}

// Wrapper around order book handler, used to print snapshots
// of the order book at certain timestamps
class SnapshotPrinter {
 public:
  struct Snapshot {
    const char* label;
    uint64_t timestamp;
  };

  SnapshotPrinter(book::OrderBookHandler& order_book, std::array<char, 8> stock,
                  std::vector<Snapshot> snapshots)
      : order_book_(order_book),
        stock_(stock),
        snapshots_(std::move(snapshots)) {}

  void OnStockDirectoryMessage(const itch::StockDirectoryMessage& message) {
    BeforeMessage(message.timestamp);
    if (message.stock == stock_) {
      stock_locate_ = message.stock_locate;
    }
  }

  void OnAddOrderMessage(const itch::AddOrderMessage& message) {
    BeforeMessage(message.timestamp);
    order_book_.OnAddOrderMessage(message);
  }

  void OnExecuteOrderMessage(const itch::ExecuteOrderMessage& message) {
    BeforeMessage(message.timestamp);
    order_book_.OnExecuteOrderMessage(message);
  }

  void OnCancelOrderMessage(const itch::CancelOrderMessage& message) {
    BeforeMessage(message.timestamp);
    order_book_.OnCancelOrderMessage(message);
  }

  void OnDeleteOrderMessage(const itch::DeleteOrderMessage& message) {
    BeforeMessage(message.timestamp);
    order_book_.OnDeleteOrderMessage(message);
  }

  void OnReplaceOrderMessage(const itch::ReplaceOrderMessage& message) {
    BeforeMessage(message.timestamp);
    order_book_.OnReplaceOrderMessage(message);
  }

  [[nodiscard]] uint64_t LastTimestamp() const { return last_timestamp_; }

 private:
  book::OrderBookHandler& order_book_;
  std::array<char, 8> stock_;
  std::vector<Snapshot> snapshots_;
  std::size_t next_ = 0;
  uint64_t last_timestamp_ = 0;
  std::optional<uint16_t> stock_locate_;

  void BeforeMessage(uint64_t timestamp) {
    last_timestamp_ = timestamp;
    while (next_ < snapshots_.size() &&
           timestamp >= snapshots_[next_].timestamp) {
      if (stock_locate_.has_value()) {
        PrintBook(std::cout, order_book_, stock_locate_.value(),
                  snapshots_[next_].label, snapshots_[next_].timestamp);
      } else {
        std::cout << "Could not print order book for "
                  << std::string_view(stock_.data(), stock_.size())
                  << " since we don't know its stock_locate yet.\n";
      }
      ++next_;
    }
  }
};

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Please provide the binary ITCH data file as argument."
              << '\n';
    return 1;
  }

  if (argc < 3) {
    std::cerr << "Please provide the stock ticker to see the order book for."
              << '\n';
    return 1;
  }

  try {
    book::OrderBookHandler order_book;
    std::array<char, 8> stock{};
    std::string_view stock_arg{argv[2]};
    if (stock_arg.size() > 8) {
      throw std::runtime_error("Given stock ticket is too large");
    }
    std::ranges::copy(stock_arg, stock.begin());
    for (std::size_t i = stock_arg.size(); i < 8; i++) {
      stock[i] = ' ';
    }

    SnapshotPrinter printer{
        order_book,
        stock,
        {
            {.label = "Open", .timestamp = TimeOfDay(9, 30, 0)},
            {.label = "Midday", .timestamp = TimeOfDay(12, 0, 0)},
            {.label = "Before close", .timestamp = TimeOfDay(15, 59, 59)},
            {.label = "After close", .timestamp = TimeOfDay(16, 0, 1)},
        }};
    itch::Parser<SnapshotPrinter> parser{printer};
    std::ifstream ifile(argv[1], std::ifstream::binary);

    if (!ifile.is_open()) {
      throw std::runtime_error("Couldn't open file.");
    }

    constexpr std::size_t kBufSize = 65536;  // 64 KiB
    std::array<unsigned char, kBufSize> buf;
    // char may alias any object's bytes, so viewing buf as char* is safe.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    char* raw_buf = reinterpret_cast<char*>(buf.data());
    while (ifile) {
      ifile.read(raw_buf, kBufSize);
      parser.Parse(buf.data(), static_cast<std::size_t>(ifile.gcount()));
    }

    if (ifile.bad()) {
      throw std::runtime_error("Error occured while reading file.");
    }

    if (parser.HasPendingBytes()) {
      throw std::runtime_error("File is truncated, expected more data.");
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
