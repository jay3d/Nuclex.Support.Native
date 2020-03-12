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

#include "Nuclex/Support/BitTricks.h"
#include <gtest/gtest.h>

namespace Nuclex { namespace Support {

  // ------------------------------------------------------------------------------------------- //

  TEST(BitTricksTest, CanCountBitsIn32BitsValue) {
    EXPECT_EQ(0, BitTricks::CountBits(std::uint32_t(0U)));

    EXPECT_EQ(1, BitTricks::CountBits(std::uint32_t(1U)));
    EXPECT_EQ(2, BitTricks::CountBits(std::uint32_t(3U)));
    EXPECT_EQ(3, BitTricks::CountBits(std::uint32_t(7U)));

    EXPECT_EQ(1, BitTricks::CountBits(std::uint32_t(2147483648U)));
    EXPECT_EQ(2, BitTricks::CountBits(std::uint32_t(3221225472U)));
    EXPECT_EQ(3, BitTricks::CountBits(std::uint32_t(3758096384U)));

    EXPECT_EQ(32, BitTricks::CountBits(std::uint32_t(4294967295U)));
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(BitTricksTest, CanCountBitsIn64BitsValue) {
    EXPECT_EQ(0, BitTricks::CountBits(std::uint64_t(0ULL)));

    EXPECT_EQ(1, BitTricks::CountBits(std::uint64_t(1ULL)));
    EXPECT_EQ(2, BitTricks::CountBits(std::uint64_t(3ULL)));
    EXPECT_EQ(3, BitTricks::CountBits(std::uint64_t(7ULL)));

    EXPECT_EQ(1, BitTricks::CountBits(std::uint64_t(9223372036854775808ULL)));
    EXPECT_EQ(2, BitTricks::CountBits(std::uint64_t(13835058055282163712ULL)));
    EXPECT_EQ(3, BitTricks::CountBits(std::uint64_t(16140901064495857664ULL)));

    EXPECT_EQ(64, BitTricks::CountBits(std::uint64_t(18446744073709551615U)));
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(BitTricksTest, CanCountLeadingZeroBitsIn32BitsValue) {
    for(std::size_t index = 0; index < 31; ++index) {
      EXPECT_EQ(
        31 - index,
        BitTricks::CountLeadingZeroBits(std::uint32_t(1U << index))
      );
    }
  }

  // ------------------------------------------------------------------------------------------- //

}} // namespace Nuclex::Support
