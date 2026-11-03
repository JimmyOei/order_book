/**
 * This file contains the order table class
 */

#ifndef ORDER_BOOK_SRC_BOOK_ORDER_TABLE_H_
#define ORDER_BOOK_SRC_BOOK_ORDER_TABLE_H_

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "itch/messages.h"

namespace book {
struct OrderEntry {
  uint64_t reference_number;
  uint32_t price;
  uint32_t shares;
  itch::BuySellIndicator buy_sell_indicator;
};
static_assert(sizeof(OrderEntry) == 24);

class OrderTable {
 public:
  explicit OrderTable(std::size_t capacity) {
    if (!std::has_single_bit(capacity)) {
      throw std::runtime_error(
          "Cannot construct order table with a capacity that is not a power of "
          "two.");
    }
    orders_.resize(capacity);
  }

  // TODO: jimmy - implement
  // Adds an order entry into the table. If table is 70% full, we grow it and
  // rehash. If reference_number exists already it throws
  void Insert(uint64_t reference_number, uint32_t price, uint32_t shares,
              itch::BuySellIndicator buy_sell_indicator);

  // TODO: jimmy - implement
  // Remove order and return the order entry.
  // Throw if order doesn't exist
  [[nodiscard]] OrderEntry Delete(uint64_t reference_number);

  // TODO: jimmy - implement
  // Reduce shares of order and return the order entry of after reduction.
  // If shares end up 0, order entry is deleted.
  // Throw if order doesn't exist or shares to subtract > shares in order
  [[nodiscard]] OrderEntry Reduce(uint64_t reference_number, uint32_t shares);

  // TODO: jimmy - implement
  // Finds and returns order entry with reference_number
  [[nodiscard]] std::optional<OrderEntry> Get(uint64_t reference_number) const;

 private:
  std::size_t active_slots_ = 0;
  std::vector<OrderEntry> orders_;

  // TODO: jimmy - implement
  // Finds and returns index to order entry with reference_number,
  // or returns nothing if it cant be found
  [[nodiscard]] std::optional<std::size_t> FindIndex(
      uint64_t reference_number) const;
};
}  // namespace book

#endif  // ORDER_BOOK_SRC_BOOK_ORDER_TABLE_H_