/**
 * price_levels_test.cc
 *
 * Unit tests for book::PriceLevels.
 */

#include "book/price_levels.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <vector>

#include "itch/messages.h"

namespace {

using Bids = book::PriceLevels<itch::BuySellIndicator::kBuy>;
using Asks = book::PriceLevels<itch::BuySellIndicator::kSell>;

// Adds one level per price, each with 100 shares.
template <typename SideLevels>
SideLevels WithLevels(std::initializer_list<uint32_t> prices) {
  SideLevels levels;
  for (const uint32_t price : prices) {
    levels.Add(price, 100U);
  }
  return levels;
}

// Returns only the prices of the given levels, keeping their order.
std::vector<uint32_t> TopNPrices(const std::vector<book::PriceLevel>& top_n) {
  std::vector<uint32_t> prices;
  prices.reserve(top_n.size());
  for (const book::PriceLevel& level : top_n) {
    prices.push_back(level.price);
  }
  return prices;
}

// Generic

TEST(PriceLevelsTest, EmptySideHasNoTop) {
  const Bids bids;
  EXPECT_FALSE(bids.GetTop().has_value());
  EXPECT_TRUE(bids.GetTopN(5U).empty());
}

TEST(PriceLevelsTest, AddsLevelToEmptySide) {
  Bids bids;
  bids.Add(10U, 200U);

  ASSERT_TRUE(bids.GetTop().has_value());
  EXPECT_EQ(bids.GetTop()->price, 10U);
  EXPECT_EQ(bids.GetTop()->shares, 200U);
  EXPECT_EQ(bids.GetTopN(5U).size(), 1U);
}

TEST(PriceLevelsTest, BidsKeepHighestPriceOnTop) {
  const Bids bids = WithLevels<Bids>({10U, 11U, 9U});
  EXPECT_EQ(TopNPrices(bids.GetTopN(5U)),
            (std::vector<uint32_t>{11U, 10U, 9U}));
}

TEST(PriceLevelsTest, AsksKeepLowestPriceOnTop) {
  const Asks asks = WithLevels<Asks>({10U, 11U, 9U});
  EXPECT_EQ(TopNPrices(asks.GetTopN(5U)),
            (std::vector<uint32_t>{9U, 10U, 11U}));
}

// Add

TEST(PriceLevelsTest, AddsNewBestBid) {
  Bids bids = WithLevels<Bids>({9U, 10U, 11U});
  bids.Add(12U, 100U);
  EXPECT_EQ(TopNPrices(bids.GetTopN(10U)),
            (std::vector<uint32_t>{12U, 11U, 10U, 9U}));
}

TEST(PriceLevelsTest, AddsNewWorstBid) {
  Bids bids = WithLevels<Bids>({9U, 10U, 11U});
  bids.Add(8U, 100U);
  EXPECT_EQ(TopNPrices(bids.GetTopN(10U)),
            (std::vector<uint32_t>{11U, 10U, 9U, 8U}));
}

TEST(PriceLevelsTest, AddsBidBetweenLevels) {
  Bids bids = WithLevels<Bids>({9U, 11U});
  bids.Add(10U, 100U);
  EXPECT_EQ(TopNPrices(bids.GetTopN(10U)),
            (std::vector<uint32_t>{11U, 10U, 9U}));
}

TEST(PriceLevelsTest, AddsAskBetweenLevels) {
  Asks asks = WithLevels<Asks>({9U, 11U});
  asks.Add(10U, 100U);
  EXPECT_EQ(TopNPrices(asks.GetTopN(10U)),
            (std::vector<uint32_t>{9U, 10U, 11U}));
}

TEST(PriceLevelsTest, AddsSharesToExistingLevel) {
  Bids bids;
  bids.Add(10U, 200U);
  bids.Add(10U, 50U);

  const std::vector<book::PriceLevel> levels = bids.GetTopN(10U);
  ASSERT_EQ(levels.size(), 1U);
  EXPECT_EQ(levels[0].price, 10U);
  EXPECT_EQ(levels[0].shares, 250U);
}

TEST(PriceLevelsTest, AddsSharesToExistingLevelThatIsNotBest) {
  Bids bids = WithLevels<Bids>({9U, 10U, 11U});
  bids.Add(10U, 50U);

  const std::vector<book::PriceLevel> levels = bids.GetTopN(10U);
  ASSERT_EQ(levels.size(), 3U);
  EXPECT_EQ(levels[1].price, 10U);
  EXPECT_EQ(levels[1].shares, 150U);
}

TEST(PriceLevelsTest, ThrowsWhenAddOverflowsShares) {
  Bids bids;
  bids.Add(10U, std::numeric_limits<uint32_t>::max());
  EXPECT_THROW(bids.Add(10U, 1U), std::runtime_error);
}

// Reduce

TEST(PriceLevelsTest, ReducesPartOfLevel) {
  Bids bids;
  bids.Add(10U, 200U);
  bids.Reduce(10U, 50U);

  ASSERT_TRUE(bids.GetTop().has_value());
  EXPECT_EQ(bids.GetTop()->price, 10U);
  EXPECT_EQ(bids.GetTop()->shares, 150U);
}

TEST(PriceLevelsTest, RemovesBestLevelWhenReducedToZero) {
  Bids bids = WithLevels<Bids>({10U, 11U});
  bids.Reduce(11U, 100U);
  EXPECT_EQ(TopNPrices(bids.GetTopN(10U)), (std::vector<uint32_t>{10U}));
}

TEST(PriceLevelsTest, RemovesMiddleLevelWhenReducedToZero) {
  Asks asks = WithLevels<Asks>({9U, 10U, 11U});
  asks.Reduce(10U, 100U);
  EXPECT_EQ(TopNPrices(asks.GetTopN(10U)), (std::vector<uint32_t>{9U, 11U}));
}

TEST(PriceLevelsTest, RemovingLastLevelLeavesSideEmpty) {
  Bids bids;
  bids.Add(10U, 100U);
  bids.Reduce(10U, 100U);
  EXPECT_FALSE(bids.GetTop().has_value());
}

TEST(PriceLevelsTest, ThrowsWhenReducingMissingLevel) {
  Bids bids = WithLevels<Bids>({10U});
  EXPECT_THROW(bids.Reduce(11U, 1U), std::runtime_error);
}

TEST(PriceLevelsTest, ThrowsWhenReducingEmptySide) {
  Bids bids;
  EXPECT_THROW(bids.Reduce(10U, 1U), std::runtime_error);
}

TEST(PriceLevelsTest, ThrowsWhenReducingTooManyShares) {
  Bids bids;
  bids.Add(10U, 200U);
  EXPECT_THROW(bids.Reduce(10U, 201U), std::runtime_error);
  // The failed reduce must leave the level unchanged.
  EXPECT_EQ(bids.GetTop()->shares, 200U);
}

// GetTopN

TEST(PriceLevelsTest, TopNReturnsAtMostN) {
  const Bids bids = WithLevels<Bids>({9U, 10U, 11U});
  EXPECT_EQ(TopNPrices(bids.GetTopN(2U)), (std::vector<uint32_t>{11U, 10U}));
}

TEST(PriceLevelsTest, TopNOfZeroIsEmpty) {
  const Bids bids = WithLevels<Bids>({9U, 10U, 11U});
  EXPECT_TRUE(bids.GetTopN(0U).empty());
}

}  // namespace