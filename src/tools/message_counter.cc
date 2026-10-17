/**
 * message_counter.cc
 *
 * Counts occurrences of each message type in the given NASDAQ
 * TotalView-ITCH 5.0 binary file and outputs this in the terminal.
 */

#include <iostream>

#include "itch/parser.h"

class DummyHandler {
 public:
  DummyHandler() = default;
};

int main() {
  try {
    DummyHandler dummy;
    itch::Parser<DummyHandler> parser{dummy};
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}