/**
 * order_book_handler_test.cc
 *
 * Unit tests for book::OrderBookHandler: each message type must update the
 * order table and the right price levels together, and books for different
 * stocks (stock_locate) must stay independent while sharing one order table.
 */

#include "book/order_book_handler.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "itch/messages.h"

namespace {

using itch::BuySellIndicator;
using Levels = std::vector<std::pair<uint32_t, uint32_t>>;  // {price, shares}

constexpr uint16_t kLocate = 13;
constexpr uint16_t kOtherLocate = 14;

itch::AddOrderMessage Add(uint64_t ref, BuySellIndicator side, uint32_t price,
                          uint32_t shares, uint16_t locate = kLocate) {
  return {.reference_number = ref,
          .timestamp = 0,
          .stock = {},
          .shares = shares,
          .price = price,
          .stock_locate = locate,
          .buy_sell_indicator = side};
}

itch::ExecuteOrderMessage Execute(uint64_t ref, uint32_t shares,
                                  uint16_t locate = kLocate) {
  return {.reference_number = ref,
          .timestamp = 0,
          .shares = shares,
          .stock_locate = locate};
}

itch::CancelOrderMessage Cancel(uint64_t ref, uint32_t shares,
                                uint16_t locate = kLocate) {
  return {.reference_number = ref,
          .timestamp = 0,
          .shares = shares,
          .stock_locate = locate};
}

itch::DeleteOrderMessage Delete(uint64_t ref, uint16_t locate = kLocate) {
  return {.reference_number = ref, .timestamp = 0, .stock_locate = locate};
}

itch::ReplaceOrderMessage Replace(uint64_t old_ref, uint64_t new_ref,
                                  uint32_t price, uint32_t shares,
                                  uint16_t locate = kLocate) {
  return {.original_reference_number = old_ref,
          .new_reference_number = new_ref,
          .timestamp = 0,
          .shares = shares,
          .price = price,
          .stock_locate = locate};
}

Levels ToPairs(const std::vector<book::PriceLevel>& levels) {
  Levels pairs;
  pairs.reserve(levels.size());
  for (const book::PriceLevel& level : levels) {
    pairs.emplace_back(level.price, level.shares);
  }
  return pairs;
}

// Top 10 levels of one stock, best first.
Levels Bids(const book::OrderBookHandler& book, uint16_t locate = kLocate) {
  return ToPairs(book.TopNBid(locate, 10));
}

Levels Asks(const book::OrderBookHandler& book, uint16_t locate = kLocate) {
  return ToPairs(book.TopNAsk(locate, 10));
}

// Empty book

TEST(OrderBookHandlerTest, NewBookIsEmpty) {
  const book::OrderBookHandler book;
  EXPECT_FALSE(book.BestBid(kLocate).has_value());
  EXPECT_FALSE(book.BestAsk(kLocate).has_value());
  EXPECT_TRUE(Bids(book).empty());
  EXPECT_TRUE(Asks(book).empty());
}

// Add (A / F)

TEST(OrderBookHandlerTest, AddBuyGoesToBidsOnly) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));

  ASSERT_TRUE(book.BestBid(kLocate).has_value());
  EXPECT_EQ(book.BestBid(kLocate)->price, 100U);
  EXPECT_EQ(book.BestBid(kLocate)->shares, 300U);
  EXPECT_FALSE(book.BestAsk(kLocate).has_value());
}

TEST(OrderBookHandlerTest, AddSellGoesToAsksOnly) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kSell, 105, 200));

  ASSERT_TRUE(book.BestAsk(kLocate).has_value());
  EXPECT_EQ(book.BestAsk(kLocate)->price, 105U);
  EXPECT_EQ(book.BestAsk(kLocate)->shares, 200U);
  EXPECT_FALSE(book.BestBid(kLocate).has_value());
}

TEST(OrderBookHandlerTest, OrdersAtSamePriceAreSummed) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kBuy, 100, 200));

  EXPECT_EQ(Bids(book), (Levels{{100, 500}}));
}

TEST(OrderBookHandlerTest, LevelsAreSortedBestFirstOnBothSides) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 99, 100));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kBuy, 101, 100));
  book.OnAddOrderMessage(Add(3, BuySellIndicator::kBuy, 100, 100));
  book.OnAddOrderMessage(Add(4, BuySellIndicator::kSell, 104, 100));
  book.OnAddOrderMessage(Add(5, BuySellIndicator::kSell, 102, 100));
  book.OnAddOrderMessage(Add(6, BuySellIndicator::kSell, 103, 100));

  EXPECT_EQ(Bids(book), (Levels{{101, 100}, {100, 100}, {99, 100}}));
  EXPECT_EQ(Asks(book), (Levels{{102, 100}, {103, 100}, {104, 100}}));
}

TEST(OrderBookHandlerTest, AddDuplicateRefThrows) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  EXPECT_THROW(book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 1)),
               std::runtime_error);
}

// Execute (E)

TEST(OrderBookHandlerTest, PartialExecuteRemovesExecutedShares) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnExecuteOrderMessage(Execute(1, 100));

  EXPECT_EQ(Bids(book), (Levels{{100, 200}}));
}

TEST(OrderBookHandlerTest, FullExecuteRemovesLevel) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kSell, 105, 300));
  book.OnExecuteOrderMessage(Execute(1, 300));

  EXPECT_TRUE(Asks(book).empty());
}

TEST(OrderBookHandlerTest, ExecutionsUntilEmptyRemoveOrderAndLevel) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnExecuteOrderMessage(Execute(1, 100));
  book.OnExecuteOrderMessage(Execute(1, 200));

  EXPECT_TRUE(Bids(book).empty());
  // The order is gone, so another execution must fail.
  EXPECT_THROW(book.OnExecuteOrderMessage(Execute(1, 1)), std::runtime_error);
}

TEST(OrderBookHandlerTest, ExecuteOneOrderLeavesOthersAtSameLevel) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kBuy, 100, 200));
  book.OnExecuteOrderMessage(Execute(1, 300));

  EXPECT_EQ(Bids(book), (Levels{{100, 200}}));
}

TEST(OrderBookHandlerTest, ExecuteTouchesOnlyTheOrdersLevel) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kBuy, 99, 300));
  book.OnAddOrderMessage(Add(3, BuySellIndicator::kSell, 101, 300));
  book.OnExecuteOrderMessage(Execute(2, 50));

  EXPECT_EQ(Bids(book), (Levels{{100, 300}, {99, 250}}));
  EXPECT_EQ(Asks(book), (Levels{{101, 300}}));
}

TEST(OrderBookHandlerTest, ExecuteUnknownRefThrows) {
  book::OrderBookHandler book;
  EXPECT_THROW(book.OnExecuteOrderMessage(Execute(1, 100)), std::runtime_error);
}

TEST(OrderBookHandlerTest, ExecuteMoreThanHeldThrows) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  EXPECT_THROW(book.OnExecuteOrderMessage(Execute(1, 301)), std::runtime_error);
}

// Cancel (X)

TEST(OrderBookHandlerTest, PartialCancelRemovesCancelledShares) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kSell, 105, 300));
  book.OnCancelOrderMessage(Cancel(1, 120));

  EXPECT_EQ(Asks(book), (Levels{{105, 180}}));
}

TEST(OrderBookHandlerTest, FullCancelRemovesOrderAndLevel) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnCancelOrderMessage(Cancel(1, 300));

  EXPECT_TRUE(Bids(book).empty());
  EXPECT_THROW(book.OnDeleteOrderMessage(Delete(1)), std::runtime_error);
}

TEST(OrderBookHandlerTest, CancelUnknownRefThrows) {
  book::OrderBookHandler book;
  EXPECT_THROW(book.OnCancelOrderMessage(Cancel(1, 100)), std::runtime_error);
}

// Delete (D)

TEST(OrderBookHandlerTest, DeleteRemovesAllOfTheOrdersShares) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kBuy, 100, 200));
  book.OnDeleteOrderMessage(Delete(1));

  EXPECT_EQ(Bids(book), (Levels{{100, 200}}));
}

TEST(OrderBookHandlerTest, DeleteAfterPartialExecuteRemovesTheRest) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kSell, 105, 300));
  book.OnExecuteOrderMessage(Execute(1, 100));
  book.OnDeleteOrderMessage(Delete(1));

  EXPECT_TRUE(Asks(book).empty());
}

TEST(OrderBookHandlerTest, DeleteUnknownRefThrows) {
  book::OrderBookHandler book;
  EXPECT_THROW(book.OnDeleteOrderMessage(Delete(1)), std::runtime_error);
}

// Replace (U)

TEST(OrderBookHandlerTest, ReplaceMovesSharesToNewPrice) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnReplaceOrderMessage(Replace(1, 2, 101, 400));

  EXPECT_EQ(Bids(book), (Levels{{101, 400}}));
  EXPECT_TRUE(Asks(book).empty());
}

TEST(OrderBookHandlerTest, ReplaceKeepsSideOfOriginalOrder) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kSell, 105, 300));
  book.OnReplaceOrderMessage(Replace(1, 2, 106, 300));

  EXPECT_EQ(Asks(book), (Levels{{106, 300}}));
  EXPECT_TRUE(Bids(book).empty());
}

TEST(OrderBookHandlerTest, ReplaceAtSamePriceChangesShares) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kBuy, 100, 200));
  book.OnReplaceOrderMessage(Replace(1, 3, 100, 50));

  EXPECT_EQ(Bids(book), (Levels{{100, 250}}));
}

TEST(OrderBookHandlerTest, ReplaceUsesRemainingSharesOfPartlyFilledOrder) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kBuy, 100, 200));
  book.OnExecuteOrderMessage(Execute(1, 100));  // order 1 now holds 200
  book.OnReplaceOrderMessage(Replace(1, 3, 99, 500));

  EXPECT_EQ(Bids(book), (Levels{{100, 200}, {99, 500}}));
}

TEST(OrderBookHandlerTest, ReplacedOrderLivesOnUnderNewRefOnly) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnReplaceOrderMessage(Replace(1, 2, 101, 400));

  book.OnExecuteOrderMessage(Execute(2, 150));
  EXPECT_EQ(Bids(book), (Levels{{101, 250}}));

  // The old ref must be gone from the order table: adding it again is legal.
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kSell, 110, 10));
  EXPECT_EQ(Asks(book), (Levels{{110, 10}}));
}

TEST(OrderBookHandlerTest, ReplaceUnknownRefThrows) {
  book::OrderBookHandler book;
  EXPECT_THROW(book.OnReplaceOrderMessage(Replace(1, 2, 100, 100)),
               std::runtime_error);
}

// Several stocks

TEST(OrderBookHandlerTest, UntouchedStockIsEmpty) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));

  EXPECT_FALSE(book.BestBid(kOtherLocate).has_value());
  EXPECT_FALSE(book.BestAsk(kOtherLocate).has_value());
}

TEST(OrderBookHandlerTest, AddsGoToTheirOwnStocksBook) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(
      Add(2, BuySellIndicator::kBuy, 100, 200, kOtherLocate));
  book.OnAddOrderMessage(
      Add(3, BuySellIndicator::kSell, 105, 50, kOtherLocate));

  EXPECT_EQ(Bids(book), (Levels{{100, 300}}));
  EXPECT_TRUE(Asks(book).empty());
  EXPECT_EQ(Bids(book, kOtherLocate), (Levels{{100, 200}}));
  EXPECT_EQ(Asks(book, kOtherLocate), (Levels{{105, 50}}));
}

TEST(OrderBookHandlerTest, ExecuteChangesOnlyItsOwnStock) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(
      Add(2, BuySellIndicator::kBuy, 100, 300, kOtherLocate));
  book.OnExecuteOrderMessage(Execute(2, 100, kOtherLocate));

  EXPECT_EQ(Bids(book), (Levels{{100, 300}}));
  EXPECT_EQ(Bids(book, kOtherLocate), (Levels{{100, 200}}));
}

TEST(OrderBookHandlerTest, CancelChangesOnlyItsOwnStock) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kSell, 105, 300));
  book.OnAddOrderMessage(
      Add(2, BuySellIndicator::kSell, 105, 300, kOtherLocate));
  book.OnCancelOrderMessage(Cancel(1, 120));

  EXPECT_EQ(Asks(book), (Levels{{105, 180}}));
  EXPECT_EQ(Asks(book, kOtherLocate), (Levels{{105, 300}}));
}

TEST(OrderBookHandlerTest, DeleteChangesOnlyItsOwnStock) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(
      Add(2, BuySellIndicator::kBuy, 100, 300, kOtherLocate));
  book.OnDeleteOrderMessage(Delete(2, kOtherLocate));

  EXPECT_EQ(Bids(book), (Levels{{100, 300}}));
  EXPECT_TRUE(Bids(book, kOtherLocate).empty());
}

TEST(OrderBookHandlerTest, ReplaceStaysInItsOwnStock) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(
      Add(2, BuySellIndicator::kBuy, 100, 300, kOtherLocate));
  book.OnReplaceOrderMessage(Replace(2, 3, 101, 400, kOtherLocate));

  EXPECT_EQ(Bids(book), (Levels{{100, 300}}));
  EXPECT_EQ(Bids(book, kOtherLocate), (Levels{{101, 400}}));
}

// Reference numbers are unique for the whole day across all stocks, which is
// why one shared order table works. A repeat on another stock is a feed error.
TEST(OrderBookHandlerTest, DuplicateRefOnOtherStockThrows) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  EXPECT_THROW(book.OnAddOrderMessage(
                   Add(1, BuySellIndicator::kBuy, 100, 300, kOtherLocate)),
               std::runtime_error);
}

TEST(OrderBookHandlerTest, ReplaceToRefUsedByOtherStockThrows) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(
      Add(2, BuySellIndicator::kBuy, 100, 300, kOtherLocate));
  EXPECT_THROW(book.OnReplaceOrderMessage(Replace(1, 2, 101, 400)),
               std::runtime_error);
}

// Every uint16_t value is a valid stock_locate, so both ends must work.
// _GLIBCXX_ASSERTIONS (debug preset) turns an out-of-range index into an abort.
TEST(OrderBookHandlerTest, LowestAndHighestLocateWork) {
  constexpr uint16_t kLowest = 0;
  constexpr uint16_t kHighest = std::numeric_limits<uint16_t>::max();
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300, kLowest));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kSell, 105, 200, kHighest));

  EXPECT_EQ(Bids(book, kLowest), (Levels{{100, 300}}));
  EXPECT_EQ(Asks(book, kHighest), (Levels{{105, 200}}));

  book.OnDeleteOrderMessage(Delete(1, kLowest));
  book.OnDeleteOrderMessage(Delete(2, kHighest));
  EXPECT_TRUE(Bids(book, kLowest).empty());
  EXPECT_TRUE(Asks(book, kHighest).empty());
}

// Messages for several stocks interleaved, as in the real feed.
TEST(OrderBookHandlerTest, InterleavedStocksStayConsistentAndEndEmpty) {
  constexpr uint16_t kThirdLocate = 5291;
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(
      Add(2, BuySellIndicator::kBuy, 100, 400, kOtherLocate));
  book.OnAddOrderMessage(
      Add(3, BuySellIndicator::kSell, 2000, 10, kThirdLocate));
  book.OnAddOrderMessage(Add(4, BuySellIndicator::kSell, 102, 200));
  book.OnExecuteOrderMessage(Execute(2, 150, kOtherLocate));
  book.OnReplaceOrderMessage(Replace(1, 5, 101, 250));
  book.OnCancelOrderMessage(Cancel(3, 4, kThirdLocate));
  book.OnAddOrderMessage(
      Add(6, BuySellIndicator::kBuy, 99, 100, kOtherLocate));

  EXPECT_EQ(Bids(book), (Levels{{101, 250}}));
  EXPECT_EQ(Asks(book), (Levels{{102, 200}}));
  EXPECT_EQ(Bids(book, kOtherLocate), (Levels{{100, 250}, {99, 100}}));
  EXPECT_TRUE(Asks(book, kOtherLocate).empty());
  EXPECT_EQ(Asks(book, kThirdLocate), (Levels{{2000, 6}}));
  EXPECT_TRUE(Bids(book, kThirdLocate).empty());

  book.OnDeleteOrderMessage(Delete(5));
  book.OnExecuteOrderMessage(Execute(4, 200));
  book.OnCancelOrderMessage(Cancel(2, 250, kOtherLocate));
  book.OnDeleteOrderMessage(Delete(6, kOtherLocate));
  book.OnExecuteOrderMessage(Execute(3, 6, kThirdLocate));

  for (const uint16_t locate : {kLocate, kOtherLocate, kThirdLocate}) {
    EXPECT_TRUE(Bids(book, locate).empty()) << "locate " << locate;
    EXPECT_TRUE(Asks(book, locate).empty()) << "locate " << locate;
  }
}

// A short day: everything added is removed again, so the book ends empty.

TEST(OrderBookHandlerTest, MixedSequenceEndsEmpty) {
  book::OrderBookHandler book;
  book.OnAddOrderMessage(Add(1, BuySellIndicator::kBuy, 100, 300));
  book.OnAddOrderMessage(Add(2, BuySellIndicator::kSell, 102, 200));
  book.OnAddOrderMessage(Add(3, BuySellIndicator::kBuy, 101, 100));
  book.OnExecuteOrderMessage(Execute(2, 50));
  book.OnReplaceOrderMessage(Replace(1, 4, 99, 250));
  book.OnCancelOrderMessage(Cancel(3, 40));

  EXPECT_EQ(Bids(book), (Levels{{101, 60}, {99, 250}}));
  EXPECT_EQ(Asks(book), (Levels{{102, 150}}));

  book.OnExecuteOrderMessage(Execute(3, 60));
  book.OnDeleteOrderMessage(Delete(4));
  book.OnCancelOrderMessage(Cancel(2, 150));

  EXPECT_TRUE(Bids(book).empty());
  EXPECT_TRUE(Asks(book).empty());
}

}  // namespace
