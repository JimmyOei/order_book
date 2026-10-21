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

  void PrintCounts() const {
    for (std::size_t i = 0; i < 256; i++) {
      if (counts_[i] > 0) {
        std::cout << i << " (" << static_cast<char>(i) << ")" << ": "
                  << counts_[i] << '\n';
      }
    }
  }

 private:
  std::array<uint64_t, 256> counts_{};
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