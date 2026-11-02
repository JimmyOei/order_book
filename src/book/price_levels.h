/**
 * This file contains the price levels for a stock
 * for only one side (buy or sell side).
 */

#ifndef ORDER_BOOK_SRC_BOOK_PRICE_LEVELS_H_
#define ORDER_BOOK_SRC_BOOK_PRICE_LEVELS_H_

#include <cstddef>
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
  [[nodiscard]] std::optional<PriceLevel> GetTop() const {
    if (prices_.empty()) {
      return {};
    }

    return prices_.back();
  }

  // Returns the top n (or book size if n > book size) elements from the side
  // book in a vector of PriceLevel in order of best to worse
  [[nodiscard]] std::vector<PriceLevel> GetTopN(std::size_t n) const {
    if (n > prices_.size()) {
      n = prices_.size();
    }

    return {prices_.rbegin(),
            prices_.rbegin() + static_cast<std::ptrdiff_t>(n)};
  }

  void Add(uint32_t price, uint32_t shares) {
    std::size_t idx = GetInsertPosForPrice(price);
    if (idx > 0 && prices_[idx - 1].price == price) {
      idx--;  // To get the idx of the already existing price level
      if (prices_[idx].shares + shares < shares) {
        throw std::runtime_error(
            "Adding shares to side book would cause an overflow.");
      }

      prices_[idx].shares += shares;
    } else {
      PriceLevel price_level{.price = price, .shares = shares};
      prices_.insert(prices_.begin() + static_cast<std::ptrdiff_t>(idx),
                     price_level);
    }
  }

  void Reduce(uint32_t price, uint32_t shares) {
    std::size_t idx = GetInsertPosForPrice(price);
    if (idx > 0 && prices_[idx - 1].price == price) {
      idx--;  // To get the idx of the already existing price level
      if (prices_[idx].shares < shares) {
        throw std::runtime_error(
            "Cannot remove more shares than are in the level.");
      }

      if (prices_[idx].shares == shares) {
        prices_.erase(prices_.begin() + static_cast<std::ptrdiff_t>(idx));
      } else {
        prices_[idx].shares -= shares;
      }
    } else {
      throw std::runtime_error("Price level to be reduced is not in the book.");
    }
  }

 private:
  // Best price is at the back (ascending for buy side,
  // descending for sell side)
  std::vector<PriceLevel> prices_;

  // Returns the index to insert the price at, or index of price
  // level + 1 if price is already in prices_
  [[nodiscard]] std::size_t constexpr GetInsertPosForPrice(
      uint32_t price) const {
    auto is_better = [](uint32_t p1, uint32_t p2) {
      if constexpr (Side == itch::BuySellIndicator::kBuy) {
        return p1 > p2;
      } else {
        return p1 < p2;
      }
    };

    std::size_t idx = prices_.size();
    while (idx > 0 && is_better(prices_[idx - 1].price, price)) {
      idx--;
    }

    return idx;
  };
};
}  // namespace book

#endif  // ORDER_BOOK_SRC_BOOK_PRICE_LEVELS_H_