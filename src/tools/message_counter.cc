/**
 * message_counter.cc
 *
 * Counts occurrences of each message type in the given NASDAQ
 * TotalView-ITCH 5.0 binary file and outputs this in the terminal.
 */

#include <fstream>
#include <iostream>

#include "itch/parser.h"

class CounterHandler {
 public:
  CounterHandler() = default;

  void OnMessage(unsigned char message_type) { counts[message_type]++; }

  void PrintCounts() const {
    for (size_t i = 0; i < 256; i++) {
      if (counts[i] > 0) {
        std::cout << i << " (" << static_cast<char>(i) << ")" << ": "
                  << counts[i] << "\n";
      }
    }
  }

 private:
  std::array<uint64_t, 256> counts{};
};

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Please provide the binary ITCH data file as argument."
              << std::endl;
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
    while (ifile) {
      ifile.read(
          reinterpret_cast<char*>(
              buf.data()),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast:
          kBufSize);        // char may access buf's bytes
      parser.Parse(buf.data(), static_cast<std::size_t>(ifile.gcount()));
    }

    // TODO: implement truncate check if parser still has leftover bytes

    if (ifile.bad()) {
      throw std::runtime_error("Error occured while reading file.");
    }

    counter.PrintCounts();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}