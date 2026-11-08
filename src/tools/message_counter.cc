/**
 * message_counter.cc
 *
 * Counts occurrences of each message type in the given NASDAQ
 * TotalView-ITCH 5.0 binary file and outputs this in the terminal.
 */

#include <fstream>
#include <iostream>

#include "itch/parser.h"

namespace {
class CounterHandler {
 public:
  CounterHandler() = default;

  void OnMessage(unsigned char message_type) { counts_[message_type]++; }

  void OnAddOrderMessage(const itch::AddOrderMessage& message) {
    // std::cout << message.reference_number << '\n';
    min_ref_ = std::min(min_ref_, message.reference_number);
    max_ref_ = std::max(max_ref_, message.reference_number);
    add_order_counts_++;
    live_count_++;
    peak_live_count_ = std::max(peak_live_count_, live_count_);
  }

  void OnCancelOrderMessage(const itch::CancelOrderMessage& /*message*/) {
    cancel_order_counts_++;
  }

  void OnExecuteOrderMessage(const itch::ExecuteOrderMessage& /*message*/) {
    execute_order_counts_++;
  }

  void OnDeleteOrderMessage(const itch::DeleteOrderMessage& /*message*/) {
    delete_order_counts_++;
    live_count_--;
  }

  void OnReplaceOrderMessage(const itch::ReplaceOrderMessage& /*message*/) {
    replace_order_counts_++;
  }

  void PrintCounts() const {
    for (std::size_t i = 0; i < 256; i++) {
      if (counts_[i] > 0) {
        std::cout << i << " (" << static_cast<char>(i) << ")" << ": "
                  << counts_[i] << '\n';
      }
    }

    std::cout << "Add Orders: " << add_order_counts_ << '\n';
    std::cout << "Execute Orders: " << execute_order_counts_ << '\n';
    std::cout << "Cancel Orders: " << cancel_order_counts_ << '\n';
    std::cout << "Delete Orders: " << delete_order_counts_ << '\n';
    std::cout << "Replace Orders: " << replace_order_counts_ << '\n';
    std::cout << "Min Ref: " << min_ref_ << '\n';
    std::cout << "Max Ref: " << max_ref_ << '\n';
    std::cout
        << "Peak Live Count: " << peak_live_count_
        << "(Note, E/C types are not accounted for, this is an upper bound)"
        << '\n';
  }

 private:
  std::array<uint64_t, 256> counts_{};
  uint64_t add_order_counts_{};
  uint64_t execute_order_counts_{};
  uint64_t cancel_order_counts_{};
  uint64_t delete_order_counts_{};
  uint64_t replace_order_counts_{};
  uint64_t min_ref_ = UINT64_MAX;
  uint64_t max_ref_{};
  uint64_t live_count_{};
  uint64_t peak_live_count_{};
};
}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Please provide the binary ITCH data file as argument."
              << '\n';
    return 1;
  }

  try {
    CounterHandler counter;
    itch::Parser<CounterHandler> parser{counter};
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

    counter.PrintCounts();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}