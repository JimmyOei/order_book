/**
 * order_table_test.cc
 *
 * Unit tests for book::OrderTable.
 *
 * Layout tests use a capacity-8 table (grows at 6 entries, so they stay at 5
 * or fewer). Home slots for capacity 8, i.e. Hash(ref) = (ref * 2^64/phi) >> 61:
 *
 *   slot 0: 5, 13, 18     slot 4: 1, 9, 17
 *   slot 1: 2, 10, 23     slot 5: 6, 14, 19
 *   slot 2: 7, 15, 20     slot 6: 3, 11, 24
 *   slot 3: 4, 12, 25     slot 7: 8, 16, 21
 *
 * Inserting in a fixed order then gives a known slot layout, so each test can
 * set up an exact cluster and check that delete keeps every entry findable.
 */

#include "book/order_table.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <random>
#include <stdexcept>
#include <unordered_map>

#include "itch/messages.h"

namespace {

using itch::BuySellIndicator;

constexpr std::size_t kSmallCapacity = 8;

// Inserts each ref with price = ref * 10, shares = 100, buy side.
void InsertAll(book::OrderTable& table, std::initializer_list<uint64_t> refs) {
  for (const uint64_t ref : refs) {
    table.Insert(ref, static_cast<uint32_t>(ref * 10), 100U,
                 BuySellIndicator::kBuy);
  }
}

// Checks that each ref is in the table with the fields InsertAll gave it.
void ExpectAllFound(const book::OrderTable& table,
                    std::initializer_list<uint64_t> refs) {
  for (const uint64_t ref : refs) {
    const auto entry = table.Get(ref);
    ASSERT_TRUE(entry.has_value()) << "ref " << ref << " not found";
    EXPECT_EQ(entry->reference_number, ref);
    EXPECT_EQ(entry->price, static_cast<uint32_t>(ref * 10));
  }
}

// Construction

TEST(OrderTableTest, RejectsCapacityThatIsNotPowerOfTwo) {
  EXPECT_THROW(book::OrderTable{12}, std::runtime_error);
  EXPECT_THROW(book::OrderTable{0}, std::runtime_error);
}

TEST(OrderTableTest, RejectsCapacityOne) {
  EXPECT_THROW(book::OrderTable{1}, std::runtime_error);
}

TEST(OrderTableTest, CapacityTwoWorks) {
  book::OrderTable table{2};
  InsertAll(table, {1, 2, 3});
  ExpectAllFound(table, {1, 2, 3});
}

// Insert / Get

TEST(OrderTableTest, GetOnEmptyTableReturnsNothing) {
  const book::OrderTable table{kSmallCapacity};
  EXPECT_FALSE(table.Get(42).has_value());
}

TEST(OrderTableTest, InsertThenGetReturnsAllFields) {
  book::OrderTable table{kSmallCapacity};
  table.Insert(42, 1'234'500U, 300U, BuySellIndicator::kSell);

  const auto entry = table.Get(42);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->reference_number, 42U);
  EXPECT_EQ(entry->price, 1'234'500U);
  EXPECT_EQ(entry->shares, 300U);
  EXPECT_EQ(entry->buy_sell_indicator, BuySellIndicator::kSell);
}

TEST(OrderTableTest, GetMissingRefInNonEmptyTableReturnsNothing) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4, 12});  // cluster at slots 3-4
  EXPECT_FALSE(table.Get(25).has_value());  // home 3, probes past the cluster
  EXPECT_FALSE(table.Get(1).has_value());   // home 4, lands inside it
}

TEST(OrderTableTest, InsertDuplicateRefThrows) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {42});
  EXPECT_THROW(table.Insert(42, 1U, 1U, BuySellIndicator::kBuy),
               std::runtime_error);
}

TEST(OrderTableTest, InsertDuplicateDisplacedRefThrows) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4, 12});  // 12 is displaced to slot 4
  EXPECT_THROW(table.Insert(12, 1U, 1U, BuySellIndicator::kBuy),
               std::runtime_error);
}

TEST(OrderTableTest, InsertRefZeroThrows) {
  book::OrderTable table{kSmallCapacity};
  EXPECT_THROW(table.Insert(0, 1U, 1U, BuySellIndicator::kBuy),
               std::runtime_error);
}

TEST(OrderTableTest, CollidingRefsAreAllFound) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4, 12, 25});  // all home 3 -> slots 3, 4, 5
  ExpectAllFound(table, {4, 12, 25});
}

TEST(OrderTableTest, ProbeWrapsPastLastSlot) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {8, 16, 21});  // all home 7 -> slots 7, 0, 1
  ExpectAllFound(table, {8, 16, 21});
}

// Ref 0 is the empty marker, so it must never match an empty slot.

TEST(OrderTableTest, GetRefZeroReturnsNothing) {
  const book::OrderTable table{kSmallCapacity};
  EXPECT_FALSE(table.Get(0).has_value());
}

TEST(OrderTableTest, DeleteRefZeroThrows) {
  book::OrderTable table{kSmallCapacity};
  EXPECT_THROW(static_cast<void>(table.Delete(0)), std::runtime_error);
}

TEST(OrderTableTest, ReduceRefZeroThrows) {
  book::OrderTable table{kSmallCapacity};
  EXPECT_THROW(static_cast<void>(table.Reduce(0, 0U)), std::runtime_error);
}

// Delete

TEST(OrderTableTest, DeleteReturnsEntryAndRemovesIt) {
  book::OrderTable table{kSmallCapacity};
  table.Insert(42, 500U, 300U, BuySellIndicator::kSell);

  const book::OrderEntry deleted = table.Delete(42);
  EXPECT_EQ(deleted.reference_number, 42U);
  EXPECT_EQ(deleted.price, 500U);
  EXPECT_EQ(deleted.shares, 300U);
  EXPECT_EQ(deleted.buy_sell_indicator, BuySellIndicator::kSell);
  EXPECT_FALSE(table.Get(42).has_value());
}

TEST(OrderTableTest, DeleteMissingRefThrows) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4});
  EXPECT_THROW(static_cast<void>(table.Delete(12)), std::runtime_error);
}

TEST(OrderTableTest, DeleteTwiceThrows) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {42});
  static_cast<void>(table.Delete(42));
  EXPECT_THROW(static_cast<void>(table.Delete(42)), std::runtime_error);
}

TEST(OrderTableTest, DeletedRefCanBeInsertedAgain) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4, 12});
  static_cast<void>(table.Delete(4));
  InsertAll(table, {4});
  ExpectAllFound(table, {4, 12});
}

// Backward shift

TEST(OrderTableTest, DeleteHeadOfClusterKeepsRestFindable) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {2, 10});  // both home 1 -> slots 1, 2
  static_cast<void>(table.Delete(2));
  ExpectAllFound(table, {10});
}

TEST(OrderTableTest, DeleteMiddleOfClusterKeepsRestFindable) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4, 12, 25});  // all home 3 -> slots 3, 4, 5
  static_cast<void>(table.Delete(12));
  ExpectAllFound(table, {4, 25});
  EXPECT_FALSE(table.Get(12).has_value());
}

// Slot 3: ref 4 (home 3), slot 4: ref 1 (home 4), slot 5: ref 12 (home 3).
// Deleting ref 4 must skip ref 1 (already home) and still move ref 12 back.
TEST(OrderTableTest, DeleteSkipsEntryAtHomeAndShiftsLaterOne) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4, 1, 12});
  static_cast<void>(table.Delete(4));
  ExpectAllFound(table, {1, 12});
}

// Same layout: after the shift, ref 12 must exist exactly once. A leftover
// copy in its old slot would still be found after deleting it.
TEST(OrderTableTest, ShiftedEntryIsNotDuplicated) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4, 1, 12});
  static_cast<void>(table.Delete(4));
  static_cast<void>(table.Delete(12));
  EXPECT_FALSE(table.Get(12).has_value());
  ExpectAllFound(table, {1});
}

// Slot 7: ref 8 (home 7), slot 0: ref 16 (home 7), slot 1: ref 5 (home 0).
// Deleting ref 8 moves both back across the wrap.
TEST(OrderTableTest, DeleteShiftsBackAcrossWrap) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {8, 16, 5});
  static_cast<void>(table.Delete(8));
  ExpectAllFound(table, {16, 5});
}

// Slot 7: ref 8 (home 7), slot 0: ref 5 (home 0), slot 1: ref 13 (home 0).
// The hole at 7 is before both homes, so neither may move.
TEST(OrderTableTest, DeleteDoesNotShiftEntryBeforeItsHomeAcrossWrap) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {8, 5, 13});
  static_cast<void>(table.Delete(8));
  ExpectAllFound(table, {5, 13});

  static_cast<void>(table.Delete(5));  // now 13 moves from slot 1 to 0
  ExpectAllFound(table, {13});
}

// Slot 6: ref 3, slot 7: ref 8, slot 0: ref 11, slot 1: ref 24.
// Homes: 3 -> 6, 8 -> 7, 11 -> 6, 24 -> 6. Deleting ref 8 (hole at 7) must
// move ref 11 and then ref 24, whose paths both start at 6.
TEST(OrderTableTest, DeleteShiftsEntriesWhoseHomeIsBeforeHoleAcrossWrap) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {3, 8, 11, 24});
  static_cast<void>(table.Delete(8));
  ExpectAllFound(table, {3, 11, 24});
}

// Reduce

TEST(OrderTableTest, ReducePartiallyReturnsEntryAfterReduction) {
  book::OrderTable table{kSmallCapacity};
  table.Insert(42, 500U, 300U, BuySellIndicator::kBuy);

  const book::OrderEntry reduced = table.Reduce(42, 100U);
  EXPECT_EQ(reduced.shares, 200U);
  EXPECT_EQ(reduced.price, 500U);
  EXPECT_EQ(reduced.buy_sell_indicator, BuySellIndicator::kBuy);

  const auto entry = table.Get(42);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->shares, 200U);
}

TEST(OrderTableTest, ReduceToZeroRemovesOrder) {
  book::OrderTable table{kSmallCapacity};
  table.Insert(42, 500U, 300U, BuySellIndicator::kSell);

  const book::OrderEntry reduced = table.Reduce(42, 300U);
  EXPECT_EQ(reduced.shares, 0U);
  EXPECT_EQ(reduced.price, 500U);
  EXPECT_EQ(reduced.buy_sell_indicator, BuySellIndicator::kSell);
  EXPECT_FALSE(table.Get(42).has_value());
}

TEST(OrderTableTest, ReduceToZeroBackwardShifts) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4, 1, 12});  // same layout as the skip test
  static_cast<void>(table.Reduce(4, 100U));
  ExpectAllFound(table, {1, 12});
}

TEST(OrderTableTest, ReduceMoreThanHeldThrowsAndKeepsOrder) {
  book::OrderTable table{kSmallCapacity};
  table.Insert(42, 500U, 300U, BuySellIndicator::kBuy);

  EXPECT_THROW(static_cast<void>(table.Reduce(42, 301U)), std::runtime_error);
  const auto entry = table.Get(42);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->shares, 300U);
}

TEST(OrderTableTest, ReduceMissingRefThrows) {
  book::OrderTable table{kSmallCapacity};
  InsertAll(table, {4});
  EXPECT_THROW(static_cast<void>(table.Reduce(12, 1U)), std::runtime_error);
}

// Growth

TEST(OrderTableTest, GrowsAndKeepsAllEntries) {
  book::OrderTable table{2};
  for (uint64_t ref = 1; ref <= 1000; ++ref) {
    table.Insert(ref, static_cast<uint32_t>(ref * 10),
                 static_cast<uint32_t>(ref), BuySellIndicator::kBuy);
  }
  for (uint64_t ref = 1; ref <= 1000; ++ref) {
    const auto entry = table.Get(ref);
    ASSERT_TRUE(entry.has_value()) << "ref " << ref;
    EXPECT_EQ(entry->price, static_cast<uint32_t>(ref * 10));
    EXPECT_EQ(entry->shares, static_cast<uint32_t>(ref));
  }
}

TEST(OrderTableTest, DeleteWorksAfterGrowth) {
  book::OrderTable table{2};
  for (uint64_t ref = 1; ref <= 1000; ++ref) {
    table.Insert(ref, static_cast<uint32_t>(ref * 10), 100U,
                 BuySellIndicator::kBuy);
  }
  for (uint64_t ref = 1; ref <= 1000; ref += 2) {
    static_cast<void>(table.Delete(ref));
  }
  for (uint64_t ref = 1; ref <= 1000; ++ref) {
    EXPECT_EQ(table.Get(ref).has_value(), ref % 2 == 0) << "ref " << ref;
  }
}

// Model test: random Insert/Delete/Reduce on a small table, checked against
// std::unordered_map after every step. Few distinct refs keep clusters long.

TEST(OrderTableTest, MatchesReferenceMapUnderRandomOperations) {
  book::OrderTable table{kSmallCapacity};
  std::unordered_map<uint64_t, book::OrderEntry> model;

  std::mt19937_64 rng{12345};
  std::uniform_int_distribution<uint64_t> ref_dist{1, 64};
  std::uniform_int_distribution<int> op_dist{0, 2};
  std::uniform_int_distribution<uint32_t> shares_dist{1, 100};

  for (int step = 0; step < 50'000; ++step) {
    const uint64_t ref = ref_dist(rng);
    const bool present = model.contains(ref);

    switch (op_dist(rng)) {
      case 0: {  // Insert
        const uint32_t shares = shares_dist(rng);
        if (present) {
          ASSERT_THROW(table.Insert(ref, 1U, shares, BuySellIndicator::kBuy),
                       std::runtime_error);
        } else {
          table.Insert(ref, static_cast<uint32_t>(ref), shares,
                       BuySellIndicator::kSell);
          model[ref] = {.reference_number = ref,
                        .price = static_cast<uint32_t>(ref),
                        .shares = shares,
                        .buy_sell_indicator = BuySellIndicator::kSell};
        }
        break;
      }
      case 1: {  // Delete
        if (present) {
          const book::OrderEntry deleted = table.Delete(ref);
          ASSERT_EQ(deleted.shares, model[ref].shares) << "step " << step;
          model.erase(ref);
        } else {
          ASSERT_THROW(static_cast<void>(table.Delete(ref)),
                       std::runtime_error);
        }
        break;
      }
      default: {  // Reduce
        if (!present) {
          ASSERT_THROW(static_cast<void>(table.Reduce(ref, 1U)),
                       std::runtime_error);
          break;
        }
        const uint32_t held = model[ref].shares;
        const uint32_t amount = shares_dist(rng) % held + 1;  // 1..held
        const book::OrderEntry reduced = table.Reduce(ref, amount);
        ASSERT_EQ(reduced.shares, held - amount) << "step " << step;
        if (held == amount) {
          model.erase(ref);
        } else {
          model[ref].shares = held - amount;
        }
        break;
      }
    }

    for (uint64_t check = 1; check <= 64; ++check) {
      const auto entry = table.Get(check);
      const auto it = model.find(check);
      ASSERT_EQ(entry.has_value(), it != model.end())
          << "step " << step << ", ref " << check;
      if (entry.has_value()) {
        ASSERT_EQ(entry->shares, it->second.shares)
            << "step " << step << ", ref " << check;
      }
    }
  }
}

}  // namespace
