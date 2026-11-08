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
class OrderBookHandler {
 public:
  explicit OrderBookHandler(uint16_t stock_locate)
      : stock_locate_(stock_locate), order_table_(1024) {};

  void OnAddOrderMessage(const itch::AddOrderMessage& message) {
    if (message.stock_locate != stock_locate_) {
      return;
    }

    order_table_.Insert(message.reference_number, message.price, message.shares,
                        message.buy_sell_indicator);
    if (message.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      bids_.Add(message.price, message.shares);
    } else {
      asks_.Add(message.price, message.shares);
    }
  }

  void OnCancelOrderMessage(const itch::CancelOrderMessage& message) {
    if (message.stock_locate != stock_locate_) {
      return;
    }

    OrderEntry order =
        order_table_.Reduce(message.reference_number, message.shares);
    if (order.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      bids_.Reduce(order.price, message.shares);
    } else {
      asks_.Reduce(order.price, message.shares);
    }
  }

  void OnExecuteOrderMessage(const itch::ExecuteOrderMessage& message) {
    if (message.stock_locate != stock_locate_) {
      return;
    }

    OrderEntry order =
        order_table_.Reduce(message.reference_number, message.shares);
    if (order.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      bids_.Reduce(order.price, message.shares);
    } else {
      asks_.Reduce(order.price, message.shares);
    }
  }

  void OnDeleteOrderMessage(const itch::DeleteOrderMessage& message) {
    if (message.stock_locate != stock_locate_) {
      return;
    }

    OrderEntry order = order_table_.Delete(message.reference_number);
    if (order.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      bids_.Reduce(order.price, order.shares);
    } else {
      asks_.Reduce(order.price, order.shares);
    }
  }

  void OnReplaceOrderMessage(const itch::ReplaceOrderMessage& message) {
    if (message.stock_locate != stock_locate_) {
      return;
    }

    OrderEntry order = order_table_.Delete(message.original_reference_number);
    order_table_.Insert(message.new_reference_number, message.price,
                        message.shares, order.buy_sell_indicator);
    if (order.buy_sell_indicator == itch::BuySellIndicator::kBuy) {
      bids_.Reduce(order.price, order.shares);
      bids_.Add(message.price, message.shares);
    } else {
      asks_.Reduce(order.price, order.shares);
      asks_.Add(message.price, message.shares);
    }
  }

  [[nodiscard]] std::optional<PriceLevel> BestBid() const {
    return bids_.GetTop();
  }

  [[nodiscard]] std::optional<PriceLevel> BestAsk() const {
    return asks_.GetTop();
  }

  [[nodiscard]] std::vector<PriceLevel> TopNBid(std::size_t n) const {
    return bids_.GetTopN(n);
  }

  [[nodiscard]] std::vector<PriceLevel> TopNAsk(std::size_t n) const {
    return asks_.GetTopN(n);
  }

 private:
  uint16_t stock_locate_;
  book::OrderTable order_table_;
  book::PriceLevels<itch::BuySellIndicator::kBuy> bids_{};
  book::PriceLevels<itch::BuySellIndicator::kSell> asks_{};
};
}  // namespace book

#endif  // ORDER_BOOK_SRC_BOOK_ORDER_BOOK_HANDLER_H_