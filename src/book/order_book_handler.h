/**
 * This file contains the order book class.
 * Any exception means the feed was inconsistent,
 * the book will be invalid and program exits.
 */

#ifndef ORDER_BOOK_SRC_BOOK_ORDER_BOOK_HANDLER_H_
#define ORDER_BOOK_SRC_BOOK_ORDER_BOOK_HANDLER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "book/order_table.h"
#include "book/price_levels.h"
#include "itch/messages.h"

namespace book {
// 2^24 Based on peak of ~6.1M live orders on 12302019.NASDAQ_ITCH50
inline constexpr std::size_t kOrderTableCapacity = 16777216;

struct OrderBook {
  book::PriceLevels<itch::BuySellIndicator::kBuy> bid;
  book::PriceLevels<itch::BuySellIndicator::kSell> ask;
};

class OrderBookHandler {
 public:
  explicit OrderBookHandler(
      std::size_t order_table_capacity = kOrderTableCapacity)
      : order_table_(order_table_capacity), books_(UINT16_MAX + 1) {}

  void OnAddOrderMessage(const itch::AddOrderMessage& message) {
    order_table_.Insert(message.reference_number, message.price, message.shares,
                        message.buy_sell_indicator);
    if (message.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      books_[message.stock_locate].bid.Add(message.price, message.shares);
    } else {
      books_[message.stock_locate].ask.Add(message.price, message.shares);
    }
  }

  void OnCancelOrderMessage(const itch::CancelOrderMessage& message) {
    OrderEntry order =
        order_table_.Reduce(message.reference_number, message.shares);
    if (order.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      books_[message.stock_locate].bid.Reduce(order.price, message.shares);
    } else {
      books_[message.stock_locate].ask.Reduce(order.price, message.shares);
    }
  }

  void OnExecuteOrderMessage(const itch::ExecuteOrderMessage& message) {
    OrderEntry order =
        order_table_.Reduce(message.reference_number, message.shares);
    if (order.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      books_[message.stock_locate].bid.Reduce(order.price, message.shares);
    } else {
      books_[message.stock_locate].ask.Reduce(order.price, message.shares);
    }
  }

  void OnDeleteOrderMessage(const itch::DeleteOrderMessage& message) {
    OrderEntry order = order_table_.Delete(message.reference_number);
    if (order.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      books_[message.stock_locate].bid.Reduce(order.price, order.shares);
    } else {
      books_[message.stock_locate].ask.Reduce(order.price, order.shares);
    }
  }

  void OnReplaceOrderMessage(const itch::ReplaceOrderMessage& message) {
    OrderEntry order = order_table_.Delete(message.original_reference_number);
    order_table_.Insert(message.new_reference_number, message.price,
                        message.shares, order.buy_sell_indicator);
    if (order.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      books_[message.stock_locate].bid.Reduce(order.price, order.shares);
      books_[message.stock_locate].bid.Add(message.price, message.shares);
    } else {
      books_[message.stock_locate].ask.Reduce(order.price, order.shares);
      books_[message.stock_locate].ask.Add(message.price, message.shares);
    }
  }

  [[nodiscard]] std::optional<PriceLevel> BestBid(uint16_t stock_locate) const {
    return books_[stock_locate].bid.GetTop();
  }

  [[nodiscard]] std::optional<PriceLevel> BestAsk(uint16_t stock_locate) const {
    return books_[stock_locate].ask.GetTop();
  }

  [[nodiscard]] std::vector<PriceLevel> TopNBid(uint16_t stock_locate,
                                                std::size_t n) const {
    return books_[stock_locate].bid.GetTopN(n);
  }

  [[nodiscard]] std::vector<PriceLevel> TopNAsk(uint16_t stock_locate,
                                                std::size_t n) const {
    return books_[stock_locate].ask.GetTopN(n);
  }

 private:
  book::OrderTable order_table_;
  std::vector<OrderBook> books_;
};
}  // namespace book

#endif  // ORDER_BOOK_SRC_BOOK_ORDER_BOOK_HANDLER_H_