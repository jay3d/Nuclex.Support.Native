//=== itoa.h - Fast integer to ascii conversion                   --*- C++ -*-//
//
// The MIT License (MIT)
// Copyright (c) 2016 Arturo Martin-de-Nicolas
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included
//     in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//===----------------------------------------------------------------------===//

#if !defined(_MSC_VER) // MSVC is lacking the __int128_t type

#ifndef DEC_ITOA_CONVERT_H
#define DEC_ITOA_CONVERT_H

#include "./itoa_impl.h"

// Print the ASCII representation of
//   - argument of integral type I
//   - at the location pointed by argument "buffer"

// print forward, return pointer to past-the-end
template<typename I> char* itoa_fwd (I i,char *buffer);

// print reverse, return pointer to first character
template<typename I> char* itoa_rev (I i,char *buffer);

#endif // DEC_ITOA_CONVERT_H

#endif // !defined(_MSC_VER)
