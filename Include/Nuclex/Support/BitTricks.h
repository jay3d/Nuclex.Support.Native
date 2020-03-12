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

#ifndef NUCLEX_SUPPORT_BITTRICKS_H
#define NUCLEX_SUPPORT_BITTRICKS_H

#include "Nuclex/Support/Config.h"

#include <cstddef>
#include <cstdint>

// Microsoft compilers need a special header to know their intrinsics
#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#include <intrin.h>
#endif

namespace Nuclex { namespace Support {

  // ------------------------------------------------------------------------------------------- //

  /// <summary>A few helper methods for bit manipulation</summary>
  class BitTricks {

    /// <summary>Counts the number of bits set in a 32 bit integer</summary>
    /// <param name="value">Value whose bits will be counted</param>
    /// <returns>The number of bits set in the 32 bit integer</returns>
    public: static inline constexpr unsigned char CountBits(std::uint32_t value) {
#if defined(_MSC_VER)
      return __popcnt(value);
#elif defined(__clang__)
      return static_cast<unsigned char>(__builtin_popcount(value));
#elif (defined(__GNUC__) || defined(__GNUG__))
      return static_cast<unsigned char>(__builtin_popcount(value));
#else
      // http://stackoverflow.com/questions/109023
      value = value - ((value >> 1) & 0x55555555U);
      value = (value & 0x33333333U) + ((value >> 2) & 0x33333333U);
      return (((value + (value >> 4)) & 0xF0F0F0FU) * 0x1010101U) >> 24;
#endif
    }

    /// <summary>Counts the number of bits set in a 64 bit integer</summary>
    /// <param name="value">Value whose bits will be counted</param>
    /// <returns>The number of bits set in the 64 bit integer</returns>
    public: static inline constexpr unsigned char CountBits(std::uint64_t value) {
#if defined(_MSC_VER)
      return __popcnt64(value);
#elif defined(__clang__)
      return static_cast<unsigned char>(__builtin_popcountll(value));
#elif (defined(__GNUC__) || defined(__GNUG__))
      return static_cast<unsigned char>(__builtin_popcountll(value));
#else
      // http://stackoverflow.com/questions/2709430
      value = value - ((value >> 1) & 0x5555555555555555ULL);
      value = (value & 0x3333333333333333ULL) + ((value >> 2) & 0x3333333333333333ULL);
      return (
        (((value + (value >> 4)) & 0xF0F0F0F0F0F0F0FULL) * 0x101010101010101ULL) >> 56
      );
#endif
    }

    /// <summary>Counts the number of leading zero bits in a value</summary>
    /// <param name="value">Value in which the leading zero bits will be couned</param>
    /// <returns>The number of leading zero bits in the value</returns>
    public: static inline constexpr unsigned char CountLeadingZeroBits(std::uint32_t value) {
#if defined(_MSC_VER)
      return __lzcnt(value);
#elif defined(__clang__)
      return static_cast<unsigned char>(__builtin_clz(value));
#elif (defined(__GNUC__) || defined(__GNUG__))
      return static_cast<unsigned char>(__builtin_clz(value));
#else
      // https://www.chessprogramming.org/BitScan#Bitscan_reverse
      // https://stackoverflow.com/questions/2589096/
      const unsigned char deBruijnBitPosition[32] = {
        31, 22, 30, 21, 18, 10, 29, 2, 20, 17, 15, 13, 9, 6, 28, 1,
        23, 19, 11, 3, 16, 14, 7, 24, 12, 4, 8, 25, 5, 26, 27, 0
      };

      value |= value >> 1; // first round down to one less than a power of 2
      value |= value >> 2;
      value |= value >> 4;
      value |= value >> 8;
      value |= value >> 16;

      return deBruijnBitPosition[(value * 0x07C4ACDDU) >> 27];
#endif
    }

    /// <summary>Counts the number of leading zero bits in a value</summary>
    /// <param name="value">Value in which the leading zero bits will be couned</param>
    /// <returns>The number of leading zero bits in the value</returns>
    public: static inline constexpr unsigned char CountLeadingZeroBits(std::uint64_t value) {
#if defined(_MSC_VER)
      return __lzcnt64(value);
#elif defined(__clang__)
      return static_cast<unsigned char>(__builtin_clzll(value));
#elif (defined(__GNUC__) || defined(__GNUG__))
      return static_cast<unsigned char>(__builtin_clzll(value));
#else
      // https://stackoverflow.com/questions/21888140/
      const unsigned char deBruijnBitPosition[64] = {
        63, 16, 62, 7, 15, 36, 61, 3, 6, 14, 22, 26, 35, 47, 60, 2,
        9, 5, 28, 11, 13, 21, 42, 19, 25, 31, 34, 40, 46, 52, 59, 1,
        17, 8, 37, 4, 23, 27, 48, 10, 29, 12, 43, 20, 32, 41, 53, 18,
        38, 24, 49, 30, 44, 33, 54, 39, 50, 45, 55, 51, 56, 57, 58, 0
      };

      value |= value >> 1;
      value |= value >> 2;
      value |= value >> 4;
      value |= value >> 8;
      value |= value >> 16;
      value |= value >> 32;

      return deBruijnBitPosition[(value * 0x03F79D71B4CB0A89ULL) >> 58];
#endif
    }

    /// <summary>
    ///   Returns the nearest power of two that is greater than or equal to the input value
    /// </summary>
    /// <param name="value">Value in which the next power of two will be returned</param>
    /// <returns>
    ///   The nearest power of two that is greater than or equal to the input value
    /// </returns>
    public: static inline constexpr std::uint32_t GetUpperPowerOfTwo(std::uint32_t value) {
#if 0 // defined(_MSC_VER)
      //::_BitScanReverse(value); ??
#elif defined(__clang__)
      std::uint32_t lowerBound = 2147483648U >> __builtin_clz(value);
      return lowerBound << (value > lowerBound);
#elif (defined(__GNUC__) || defined(__GNUG__))
      std::uint32_t lowerBound = 2147483648U >> __builtin_clz(value);
      return lowerBound << (value > lowerBound);
#else
      --value;

      value |= value >> 1;
      value |= value >> 2;
      value |= value >> 4;
      value |= value >> 8;
      value |= value >> 16;

      return (value + 1);
#endif
    }

    /// <summary>
    ///   Returns the nearest power of two that is greater than or equal to the input value
    /// </summary>
    /// <param name="value">Value in which the next power of two will be returned</param>
    /// <returns>
    ///   The nearest power of two that is greater than or equal to the input value
    /// </returns>
    public: static inline constexpr std::uint64_t GetUpperPowerOfTwo(std::uint64_t value) {
#if 0 // defined(_MSC_VER)
      //::_BitScanReverse(value); ??
#elif defined(__clang__)
      std::uint64_t lowerBound = 9223372036854775808ULL >> __builtin_clzll(value);
      return lowerBound << (value > lowerBound);
#elif (defined(__GNUC__) || defined(__GNUG__))
      std::uint64_t lowerBound = 9223372036854775808ULL >> __builtin_clzll(value);
      return lowerBound << (value > lowerBound);
#else
      --value;

      value |= value >> 1;
      value |= value >> 2;
      value |= value >> 4;
      value |= value >> 8;
      value |= value >> 16;
      value |= value >> 32;

      return (value + 1);
#endif
    }

  };

  // ------------------------------------------------------------------------------------------- //

}} // namespace Nuclex::Support

#endif // NUCLEX_SUPPORT_BITTRICKS_H
