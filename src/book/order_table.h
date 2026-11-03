/**
 * This file contains the order table class
 * orders are stored in an implementation of fibonacci
 * hash table that doubles in size at 75% full capacity
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
  explicit OrderTable(std::size_t capacity)
      : shift_(CalculateShift(capacity)),
        grow_capacity_(capacity - (capacity / 4)) {
    if (!std::has_single_bit(capacity)) {
      throw std::runtime_error(
          "Cannot construct order table with a capacity that is not a power of "
          "two.");
    }
    if (capacity < 2) {
      throw std::runtime_error(
          "Order table does not work for a capacity lower than 2.");
    }
    orders_.resize(capacity);
  }

  void Insert(uint64_t reference_number, uint32_t price, uint32_t shares,
              itch::BuySellIndicator buy_sell_indicator) {
    if (reference_number == 0) {
      throw std::runtime_error(
          "Cannot insert order with invalid reference_number of 0.");
    }

    std::size_t index = FindIndex(reference_number);
    if (orders_[index].reference_number == reference_number) {
      throw std::runtime_error(
          "Cannot insert order with reference_number that already exists.");
    }

    orders_[index] = OrderEntry{.reference_number = reference_number,
                                .price = price,
                                .shares = shares,
                                .buy_sell_indicator = buy_sell_indicator};
    active_slots_++;

    if (active_slots_ >= grow_capacity_) {
      // Reached 75% capacity, growing table by 100%
      std::size_t new_capacity = orders_.size() + orders_.size();
      std::vector<OrderEntry> orders_copy(new_capacity);
      std::swap(orders_copy, orders_);
      shift_ = CalculateShift(new_capacity);
      grow_capacity_ = new_capacity - (new_capacity / 4);

      for (const auto& order : orders_copy) {
        if (order.reference_number != 0) {
          orders_[FindIndex(order.reference_number)] = order;
        }
      }
    }
  }

  [[nodiscard]] OrderEntry Delete(uint64_t reference_number) {
    if (reference_number == 0) {
      throw std::runtime_error(
          "Cannot delete order with invalid reference_number of 0.");
    }

    std::size_t index = FindIndex(reference_number);
    if (orders_[index].reference_number != reference_number) {
      throw std::runtime_error(
          "Order not found in table when trying to delete it.");
    }

    OrderEntry order_copy{orders_[index]};
    EraseOrder(index);

    return order_copy;
  }

  [[nodiscard]] OrderEntry Reduce(uint64_t reference_number, uint32_t shares) {
    if (reference_number == 0) {
      throw std::runtime_error(
          "Cannot reduce order with invalid reference_number of 0.");
    }

    std::size_t index = FindIndex(reference_number);
    if (orders_[index].reference_number != reference_number) {
      throw std::runtime_error(
          "Order not found in table when trying to reduce its shares.");
    }

    if (orders_[index].shares < shares) {
      throw std::runtime_error(
          "Cannot reduce more shares than are in the order.");
    }

    orders_[index].shares -= shares;

    OrderEntry order_copy{orders_[index]};
    if (orders_[index].shares == 0) {
      EraseOrder(index);
    }

    return order_copy;
  }

  [[nodiscard]] std::optional<OrderEntry> Get(uint64_t reference_number) const {
    if (reference_number == 0) {
      return {};
    }

    std::size_t index = FindIndex(reference_number);
    if (orders_[index].reference_number != reference_number) {
      return {};
    }

    return orders_[index];
  }

 private:
  static constexpr uint64_t kGoldenRatio64 = 11400714819323198485ULL;
  uint32_t shift_;
  std::size_t active_slots_ = 0;
  std::size_t grow_capacity_;
  std::vector<OrderEntry> orders_;

  static uint32_t CalculateShift(std::size_t capacity) {
    return 64 - static_cast<uint32_t>(std::countr_zero(capacity));
  }

  [[nodiscard]] std::uint64_t Hash(uint64_t key) const {
    return (key * kGoldenRatio64) >> shift_;
  }

  [[nodiscard]] std::size_t FindIndex(uint64_t reference_number) const {
    std::size_t index = Hash(reference_number);

    while (orders_[index].reference_number != reference_number &&
           orders_[index].reference_number != 0) {
      index = (index + 1) & (orders_.size() - 1);
    }

    return index;
  }

  void EraseOrder(std::size_t index) {
    std::size_t gap_index = index;
    index++;
    if (index == orders_.size()) {
      index = 0;
    }

    // Backward shift
    while (orders_[index].reference_number != 0) {
      std::uint64_t hash = Hash(orders_[index].reference_number);
      std::size_t distance_hash_to_index =
          (index - hash) & (orders_.size() - 1);
      std::size_t distance_gap_to_index =
          (index - gap_index) & (orders_.size() - 1);

      if (distance_gap_to_index <= distance_hash_to_index) {
        orders_[gap_index] = orders_[index];
        gap_index = index;
      }

      index = (index + 1) & (orders_.size() - 1);
    }
    orders_[gap_index].reference_number = 0;
    active_slots_--;
  }
};
}  // namespace book

#endif  // ORDER_BOOK_SRC_BOOK_ORDER_TABLE_H_