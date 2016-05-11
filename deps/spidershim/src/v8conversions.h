// Copyright Mozilla Foundation. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <cstring>

namespace v8 {
namespace internal {

union DoubleRepresentation {
  double value;
  int64_t bits;
  DoubleRepresentation(double x) { value = x; }
  bool operator==(const DoubleRepresentation& other) const {
    return bits == other.bits;
  }
};

static inline bool IsMinusZero(double value) {
  static const DoubleRepresentation minus_zero(-0.0);
  return DoubleRepresentation(value) == minus_zero;
}

const uint32_t kMaxUInt32 = 0xFFFFFFFFu;

// The fast double-to-unsigned-int conversion routine does not guarantee
// rounding towards zero, or any reasonable value if the argument is larger
// than what fits in an unsigned 32-bit integer.
inline unsigned int FastD2UI(double x) {
  // There is no unsigned version of lrint, so there is no fast path
  // in this function as there is in FastD2I. Using lrint doesn't work
  // for values of 2^31 and above.

  // Convert "small enough" doubles to uint32_t by fixing the 32
  // least significant non-fractional bits in the low 32 bits of the
  // double, and reading them from there.
  const double k2Pow52 = 4503599627370496.0;
  bool negative = x < 0;
  if (negative) {
    x = -x;
  }
  if (x < k2Pow52) {
    x += k2Pow52;
    uint32_t result;
    uint8_t* mantissa_ptr = reinterpret_cast<uint8_t*>(&x);
    // Copy least significant 32 bits of mantissa.
    memcpy(&result, mantissa_ptr, sizeof(result));
    return negative ? ~result + 1 : result;
  }
  // Large number (outside uint32 range), Infinity or NaN.
  return 0x80000000u;  // Return integer indefinite.
}

inline double FastUI2D(unsigned x) {
  // There is no rounding involved in converting an unsigned integer to a
  // double, so this code should compile to a few instructions without
  // any FPU pipeline stalls.
  return static_cast<double>(x);
}
}
}
