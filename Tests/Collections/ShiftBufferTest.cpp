#pragma region CPL License
/*
Nuclex Native Framework
Copyright (C) 2002-2020 Nuclex Development Labs

This library is free software; you can redistribute it and/or
modify it under the terms of the IBM Common Public License as
published by the IBM Corporation; either version 1.0 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
IBM Common Public License for more details.

You should have received a copy of the IBM Common Public
License along with this library
*/
#pragma endregion // CPL License

// If the library is compiled as a DLL, this ensures symbols are exported
#define NUCLEX_SUPPORT_SOURCE 1

#include "Nuclex/Support/Collections/ShiftBuffer.h"
#include <gtest/gtest.h>

#include <vector> // for std::vector

namespace {

  // ------------------------------------------------------------------------------------------- //

  class TestItem {

  };

  // ------------------------------------------------------------------------------------------- //

} // anonymous namespace

namespace Nuclex { namespace Support { namespace Collections {

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferTest, InstancesCanBeCreated) {
    EXPECT_NO_THROW(
      ShiftBuffer<std::uint8_t> test;
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferTest, NewInstanceContainsNoItems) {
    ShiftBuffer<std::uint8_t> test;
    EXPECT_EQ(test.Count(), 0U);
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferTest, StartsWithNonZeroDefaultCapacity) {
    ShiftBuffer<std::uint8_t> test;
    EXPECT_GT(test.GetCapacity(), 0U);
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferTest, CanStartWithCustomCapacity) {
    ShiftBuffer<std::uint8_t> test(512U);
    EXPECT_GE(test.GetCapacity(), 512U);
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferTest, HasCopyConstructor) {
    ShiftBuffer<std::uint8_t> test;

    std::uint8_t items[10] = { 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U };
    test.Write(items, 10);

    EXPECT_EQ(test.Count(), 10);

    ShiftBuffer<std::uint8_t> copy(test);

    EXPECT_EQ(copy.Count(), 10);

    std::uint8_t retrieved[10];
    copy.Read(retrieved, 10);

    EXPECT_EQ(copy.Count(), 0);
    EXPECT_EQ(test.Count(), 10);

    for(std::size_t index = 0; index < 10; ++index) {
      EXPECT_EQ(retrieved[index], items[index]);
    }
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferTest, HasMoveConstructor) {
    ShiftBuffer<std::uint8_t> test;

    std::uint8_t items[10] = { 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U };
    test.Write(items, 10);

    EXPECT_EQ(test.Count(), 10);

    ShiftBuffer<std::uint8_t> moved(std::move(test));

    EXPECT_EQ(moved.Count(), 10);

    std::uint8_t retrieved[10];
    moved.Read(retrieved, 10);

    EXPECT_EQ(moved.Count(), 0);

    for(std::size_t index = 0; index < 10; ++index) {
      EXPECT_EQ(retrieved[index], items[index]);
    }
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferTest, ItemsCanBeAppended) {
    ShiftBuffer<std::uint8_t> test;

    std::uint8_t items[128];
    test.Write(items, 128);

    EXPECT_EQ(test.Count(), 128U);
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferDeathTest, SkippingOnEmptyBufferTriggersAssertion) {
    ShiftBuffer<std::uint8_t> test;

    ASSERT_DEATH(
      test.Skip(1),
      ".*Amount of data skipped is less or equal to the amount of data in the buffer.*"
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferDeathTest, ReadingFromEmptyBufferTriggersAssertion) {
    ShiftBuffer<std::uint8_t> test;

    std::uint8_t retrieved[1];

    ASSERT_DEATH(
      test.Read(retrieved, 1),
      ".*Amount of data read is less or equal to the amount of data in the buffer.*"
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ShiftBufferTest, ItemsCanBeReadAndWritten) {
    ShiftBuffer<std::uint8_t> test;

    std::uint8_t items[128];
    for(std::size_t index = 0; index < 128; ++index) {
      items[index] = static_cast<std::uint8_t>(index);
    }
    test.Write(items, 128);

    EXPECT_EQ(test.Count(), 128U);

    std::uint8_t retrieved[128];
    test.Read(retrieved, 128);

    EXPECT_EQ(test.Count(), 0U);

    for(std::size_t index = 0; index < 128; ++index) {
      EXPECT_EQ(retrieved[index], static_cast<std::uint8_t>(index));
    }
  }

  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Collections
