/**
 * This file contains the price levels for a stock
 * for only one side (buy or sell side).
 */

#ifndef ORDER_BOOK_SRC_ITCH_PRICE_LEVELS_H_
#define ORDER_BOOK_SRC_ITCH_PRICE_LEVELS_H_

#include <cstddef>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

#include "itch/messages.h"

namespace book {
struct PriceLevel {
  uint32_t price;
  uint32_t shares;
};

template <itch::BuySellIndicator Side>
class PriceLevels {
 public:
  std::optional<PriceLevel> GetTop() const {
    // TODO: jimmy - implement
    // give nothing if prices_ is empty
  }

  std::vector<PriceLevel> GetTopN(std::size_t /*n*/) const {
    // TODO: jimmy - implement
    // give top that exist if prices_ size < n
  }

  void Add(uint32_t /*price*/, uint32_t /*shares*/) {
    // TODO: jimmy - implement
    // throw is shares sum gets too big
    // conditionally add ascending/descending based on the Side
    // if constexpr (side == itch::BuySellIndicator::kBuy) {} else {}
  }

  void Reduce(uint32_t /*price*/, uint32_t /*shares*/) {
    // TODO: jimmy - implement
    // Automatic removal
    // throw if price not in prices_,
    // or shares to reduce is too many
    // if constexpr (side == itch::BuySellIndicator::kBuy) {} else {}
  }

 private:
  std::vector<PriceLevel> prices_;
};
}  // namespace book

#endif  // ORDER_BOOK_SRC_ITCH_PRICE_LEVELS_H_