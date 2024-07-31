#pragma region Apache License 2.0
/*
Nuclex Native Framework
Copyright (C) 2002-2024 Markus Ewald / Nuclex Development Labs

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma endregion // Apache License 2.0

// If the library is compiled as a DLL, this ensures symbols are exported
#define NUCLEX_SUPPORT_SOURCE 1

#include "Nuclex/Support/Text/StringMatcher.h"
#include "Nuclex/Support/Text/UnicodeHelper.h" // UTF encoding and decoding

#include <vector> // for std::vector
#include <stdexcept> // for std::invalid_argument
#include <cassert> // for assert()

namespace {

  // ------------------------------------------------------------------------------------------- //
#if 0 // This is a memento for my old, trusty Ascii wildcard checker. It's retired now.
  /// <summary>C-style function that checks if a string matches a wild card</summary>
  /// <param name="text">Text that will be checked against the wild card</param>
  /// <param name="wildcard">Wild card against which the text will be matched</param>
  /// <returns>True if the text matches the wild card, false otherwise</returns>
  bool matchWildcardAscii(const char *text, const char *wildcard) {
    if(!text) {
      return false;
    }

    for(; '*' ^ *wildcard; ++wildcard, ++text) {
      if(!*text) {
        return (!*wildcard);
      }

      if(::toupper(*text) ^ ::toupper(*wildcard) && '?' ^ *wildcard) {
        return false;
      }
    }

    while('*' == wildcard[1]) {
      wildcard++;
    }

    do {
      if(matchWildcardAscii(text, wildcard + 1)) {
        return true;
      }
    } while(*text++);

    return false;
  }
#endif
  // ------------------------------------------------------------------------------------------- //

  /// <summary>Invidual UTF-8 character type (until C++20 introduces char8_t)</summary>
  typedef unsigned char my_char8_t;

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Throws an exception of the code point is invalid</summary>
  /// <param name="codePoint">Unicode code point that will be checked</param>
  /// <remarks>
  ///   This does a generic code point check, but since within this file the code point
  ///   must be coming from an UTF-8 encoded string, we do complain about invalid UTF-8.
  /// </remarks>
  void requireValidCodePoint(char32_t codePoint) {
    if(!Nuclex::Support::Text::UnicodeHelper::IsValidCodePoint(codePoint)) {
      throw std::invalid_argument(u8"Illegal UTF-8 character(s) encountered");
    }
  }

  // ------------------------------------------------------------------------------------------- //

  /// <summary>C-style function that checks if a string starts with another string</summary>
  /// <typeparam name="CaseSensitive">Whether the comparison will be case-sensitive</typeparam>
  /// <param name="haystack">Text whose beginning will be checked for the other string</param>
  /// <param name="haystackEnd">Address one past the end of the haystack string</param>
  /// <param name="needle">Substring that will be searched for</param>
  /// <param name="needleEnd">Address one past the end of the needle string</param>
  /// <returns>True if the 'haystack' string starts with the 'needle' string</returns>
  template<bool CaseSensitive>
  bool checkStringMatchesWithUtf8(
    const my_char8_t *haystack, const my_char8_t *haystackEnd,
    const my_char8_t *needle, const my_char8_t *needleEnd
  ) {
    using Nuclex::Support::Text::UnicodeHelper;
    assert((haystack != nullptr) && u8"Haystack must not be a NULL pointer");
    assert((needle != nullptr) && u8"Needle must not be a NULL pointer");

    for(;;) {
      if(needle >= needleEnd) {
        return (haystack >= haystackEnd); // Both must end at the same time
      }
      if(haystack >= haystackEnd) {
        return false; // If the haystack was shorter, the needle wasn't found
      }

      // Fetch the next code points from both strings so we can compare them
      char32_t haystackCodePoint = UnicodeHelper::ReadCodePoint(haystack, haystackEnd);
      requireValidCodePoint(haystackCodePoint);
      char32_t needleCodePoint = UnicodeHelper::ReadCodePoint(needle, needleEnd);
      requireValidCodePoint(needleCodePoint);

      if constexpr(!CaseSensitive) {
        needleCodePoint = UnicodeHelper::ToFoldedLowercase(needleCodePoint);
        haystackCodePoint = UnicodeHelper::ToFoldedLowercase(haystackCodePoint);
      }
      if(needleCodePoint != haystackCodePoint) {
        return false;
      }
    }
  }

  // ------------------------------------------------------------------------------------------- //

  /// <summary>C-style function that checks if a string contains another string</summary>
  /// <typeparam name="CaseSensitive">Whether the comparison will be case-sensitive</typeparam>
  /// <param name="haystack">Text that will be scanned for the other string</param>
  /// <param name="haystackEnd">Address one past the end of the haystack string</param>
  /// <param name="needle">Substring that will be searched for</param>
  /// <param name="needleEnd">Address one past the end of the needle string</param>
  /// <returns>
  ///   The address in the haystack string where the first match was found or a null pointer
  ///   if no matches were found.
  /// </returns>
  template<bool CaseSensitive>
  const my_char8_t *findSubstringUtf8(
    const my_char8_t *haystack, const my_char8_t *haystackEnd,
    const my_char8_t *needle, const my_char8_t *needleEnd
  ) {
    using Nuclex::Support::Text::UnicodeHelper;
    assert((haystack != nullptr) && u8"Haystack must not be a NULL pointer");
    assert((needle != nullptr) && u8"Needle must not be a NULL pointer");

    // We treat a zero-length needle as an immediate match to anything.
    if(needle >= needleEnd) {
      return haystack;
    }

    // Get and keep the first code point. This speeds up our search since we only
    // need to scan the haystack for appearances of this code point, then compare
    // further if and when we find a match.
    char32_t firstNeedleCodePoint = UnicodeHelper::ReadCodePoint(needle, needleEnd);
    requireValidCodePoint(firstNeedleCodePoint);
    if constexpr(!CaseSensitive) {
      firstNeedleCodePoint = UnicodeHelper::ToFoldedLowercase(firstNeedleCodePoint);
    }

    // Go through the haystack and look for code points matching the first code point
    // of the needle. Any matches are investigated further in a nested loop.
    const my_char8_t *needleFromSecondCodePoint = needle;
    while(haystack < haystackEnd) {
      const my_char8_t *haystackAtStart = haystack;

      // Fetch the next haystack code point to compare against the first needle code point
      char32_t haystackCodePoint = UnicodeHelper::ReadCodePoint(haystack, haystackEnd);
      requireValidCodePoint(haystackCodePoint);
      if constexpr(!CaseSensitive) {
        haystackCodePoint = UnicodeHelper::ToFoldedLowercase(haystackCodePoint);
      }

      // In the outer loop, scan only for the a match of the first needle codepoint.
      // Keeping this loop tight allows the compiler to optimize it into a simple scan.
      if(unlikely(haystackCodePoint == firstNeedleCodePoint)) {
        const my_char8_t *haystackInner = haystack;
        for(;;) {
          if(needle >= needleEnd) { // Needle ended? We've got a full match!
            return haystackAtStart;
          }
          if(haystackInner >= haystackEnd) {
            break;
          }

          // We've got both another needle code point and another haystack code point,
          // so see if these two are still equal
          char32_t needleCodePoint = UnicodeHelper::ReadCodePoint(needle, needleEnd);
          requireValidCodePoint(needleCodePoint);
          haystackCodePoint = UnicodeHelper::ReadCodePoint(haystackInner, haystackEnd);
          requireValidCodePoint(haystackCodePoint);
          if constexpr(!CaseSensitive) {
            needleCodePoint = UnicodeHelper::ToFoldedLowercase(needleCodePoint);
            haystackCodePoint = UnicodeHelper::ToFoldedLowercase(haystackCodePoint);
          }
          if(needleCodePoint != haystackCodePoint) {
            break;
          }
        }

        // No match found. Reset the needle for the next scan.
        needle = needleFromSecondCodePoint;
      }
    } // while not end of haystack reached

    return nullptr;
  }

  // ------------------------------------------------------------------------------------------- //

  /// <summary>C-style function that checks if a string matches a wild card</summary>
  /// <typeparam name="CaseSensitive">Whether the comparison will be case-sensitive</typeparam>
  /// <param name="text">Text that will be checked against the wild card</param>
  /// <param name="textEnd">Address one past the end of the text</param>
  /// <param name="wildcard">Wildcard against which the text will be matched</param>
  /// <param name="wildcardEnd">Address one past the end of the wildcard string</param>
  /// <returns>True if the text matches the wild card, false otherwise</returns>
  template<bool CaseSensitive>
  bool matchWildcardUtf8(
    const my_char8_t *text, const my_char8_t *textEnd,
    const my_char8_t *wildcard, const my_char8_t *wildcardEnd
  ) {
    using Nuclex::Support::Text::UnicodeHelper;
    assert((text != nullptr) && u8"Text must not be a NULL pointer");
    assert((wildcard != nullptr) && u8"Wildcard must not be a NULL pointer");

    char32_t wildcardCodePoint;
    for(;;) {

      // If the end of the wildcard is reached, all letters of the input text
      // must have been consumed (unless the wildcard ends with a star)
      if(wildcard >= wildcardEnd) {
        return (text >= textEnd); // All letters must have been consumed
      }

      // Try to obtain the next wild card letter. We do this before checking
      // for the end of the text because wildcards can match zero letters, too.
      wildcardCodePoint = UnicodeHelper::ReadCodePoint(wildcard, wildcardEnd);
      requireValidCodePoint(wildcardCodePoint);
      if(wildcardCodePoint == U'*') {
        break; // Wildcard had a star, enter skip mode
      }

      // If text ends but wildcard has more letters to match
      if(text >= textEnd) {
        return false;
      }

      // We have both a valid wildcard letter and a letter from the text to compare
      // against it, so lets compare one input letter against one wildcard letter
      char32_t textCodePoint = UnicodeHelper::ReadCodePoint(text, textEnd);
      requireValidCodePoint(textCodePoint);
      if(wildcardCodePoint != U'?') {
        if constexpr(CaseSensitive) {
          if(textCodePoint != wildcardCodePoint) {
            return false;
          }
        } else { // Case insensitive
          textCodePoint = UnicodeHelper::ToFoldedLowercase(textCodePoint);
          if(textCodePoint != UnicodeHelper::ToFoldedLowercase(wildcardCodePoint)) {
            return false;
          }
        }
      }

    } // letterwise comparison loop

    // If we encountered a star, first skip any redundant stars directly following
    const my_char8_t *wildcardAfterStar = wildcard;
    for(;;) {
      if(wildcard >= wildcardEnd) {
        return true; // If the wildcard ends with a star, any remaining text is okay!
      }

      // Read the next letter from the wildcard and see if it's a star, too
      wildcardCodePoint = UnicodeHelper::ReadCodePoint(wildcard, wildcardEnd);
      requireValidCodePoint(wildcardCodePoint);
      if(wildcardCodePoint != U'*') {
        break;
      }

      // Wildcard letter was indeed a star, so the current 'wildcard' pointer
      // (already advanced to the next letter) is the earliest possible non-star
      wildcardAfterStar = wildcard;
    }

    // Then retry the wildcard match skipping any number of characters from text
    // (the star can match anything from zero to all characters)
    while(text < textEnd) {
      if(matchWildcardUtf8<CaseSensitive>(text, textEnd, wildcardAfterStar, wildcardEnd)) {
        return true;
      }

      // This could just skip, but if we encounter an invalid character,
      // we can't be sure of its length, so have to check and bail out in case.
      char32_t textCodePoint = UnicodeHelper::ReadCodePoint(text, textEnd);
      requireValidCodePoint(textCodePoint);
    }

    // No amount of skipping helped, there's no match
    return false;

  }

  // ------------------------------------------------------------------------------------------- //

} // anonymous namespace

namespace Nuclex { namespace Support { namespace Text {

  // ------------------------------------------------------------------------------------------- //

  bool StringMatcher::AreEqual(
    const std::string &left, const std::string &right, bool caseSensitive /* = false */
  ) {
    if(caseSensitive) {
      return (left == right);
    } else {
      if(left.length() != right.length()) {
        return false;
      }

      const my_char8_t *leftStart = reinterpret_cast<const my_char8_t *>(left.c_str());
      const my_char8_t *leftEnd = leftStart + left.length();

      const my_char8_t *rightStart = reinterpret_cast<const my_char8_t *>(right.c_str());
      const my_char8_t *rightEnd = rightStart + right.length();

      return checkStringMatchesWithUtf8<false>(
        leftStart, leftEnd,
        rightStart, rightEnd
      );
    }
  }

  // ------------------------------------------------------------------------------------------- //

  bool StringMatcher::Contains(
    const std::string &haystack, const std::string &needle, bool caseSensitive /* = false */
  ) {
    const my_char8_t *haystackStart = reinterpret_cast<const my_char8_t *>(haystack.c_str());
    const my_char8_t *haystackEnd = haystackStart + haystack.length();

    const my_char8_t *needleStart = reinterpret_cast<const my_char8_t *>(needle.c_str());
    const my_char8_t *needleEnd = needleStart + needle.length();

    if(caseSensitive) {
      return findSubstringUtf8<true>(
        haystackStart, haystackEnd, needleStart, needleEnd
      ) != nullptr;
    } else {
      return findSubstringUtf8<false>(
        haystackStart, haystackEnd, needleStart, needleEnd
      ) != nullptr;
    }
  }

  // ------------------------------------------------------------------------------------------- //

  bool StringMatcher::StartsWith(
    const std::string &haystack, const std::string &needle, bool caseSensitive /* = false */
  ) {
    const my_char8_t *haystackStart = reinterpret_cast<const my_char8_t *>(haystack.c_str());
    const my_char8_t *haystackEnd = haystackStart + needle.length();

    const my_char8_t *needleStart = reinterpret_cast<const my_char8_t *>(needle.c_str());
    const my_char8_t *needleEnd = needleStart + needle.length();

    if(caseSensitive) {
      return checkStringMatchesWithUtf8<true>(
        haystackStart, haystackEnd, needleStart, needleEnd
      );
    } else {
      return checkStringMatchesWithUtf8<false>(
        haystackStart, haystackEnd, needleStart, needleEnd
      );
    }
  }

  // ------------------------------------------------------------------------------------------- //

  bool StringMatcher::EndsWith(
    const std::string &haystack, const std::string &needle, bool caseSensitive /* = false */
  ) {
    if(needle.length() > haystack.length()) {
      return false;
    }

    // Pick the start in the haystack to begin the same number of bytes from
    // its end as the 'needle' is long.
    const my_char8_t *haystackStart = reinterpret_cast<const my_char8_t *>(haystack.c_str());
    const my_char8_t *haystackEnd = haystackStart + haystack.length();
    haystackStart = haystackEnd - needle.length(); // safe, since we checked the length

    // Since we don't know the contents of the 'haystack' string, the above
    // math might have placed us in the middle of a UTF-8 sequence. Luckily,
    // UTF-8 lets us detect that. In that situation, the strings are not a match.
    //
    // Note that we /could/ blindly compare the characters. If we started at a subsequent
    // character in the 'haystack', it would be a guaranteed mismatch to the first character
    // in the 'needle'. However, since we're actually decoding UTF-8 code points (in order to
    // do case folding for case insensitive comparisons), we still have to check, otherwise
    // UnicodeHelper::ReadCodePoint() would bail out with an 'illegal UTF-8 character' error.
    bool isSubsequentCharacter = (
      (static_cast<std::uint8_t>(*haystackStart) & 0xc0) == 0x80
    );
    if(isSubsequentCharacter) {
      return false;
    }

    const my_char8_t *needleStart = reinterpret_cast<const my_char8_t *>(needle.c_str());
    const my_char8_t *needleEnd = needleStart + needle.length();

    if(caseSensitive) {
      return checkStringMatchesWithUtf8<true>(
        haystackStart, haystackEnd, needleStart, needleEnd
      );
    } else {
      return checkStringMatchesWithUtf8<false>(
        haystackStart, haystackEnd, needleStart, needleEnd
      );
    }
  }

  // ------------------------------------------------------------------------------------------- //

  bool StringMatcher::FitsWildcard(
    const std::string &text, const std::string &wildcard, bool caseSensitive /* = false */
  ) {
    const my_char8_t *textStart = reinterpret_cast<const my_char8_t *>(text.c_str());
    const my_char8_t *textEnd = textStart + text.length();

    const my_char8_t *wildcardStart = reinterpret_cast<const my_char8_t *>(wildcard.c_str());
    const my_char8_t *wildcardEnd = wildcardStart + wildcard.length();

    if(caseSensitive) {
      return matchWildcardUtf8<true>(textStart, textEnd, wildcardStart, wildcardEnd);
    } else {
      return matchWildcardUtf8<false>(textStart, textEnd, wildcardStart, wildcardEnd);
    }
  }

  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Text
