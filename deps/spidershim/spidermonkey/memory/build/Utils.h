/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Utils_h
#define Utils_h

#include "mozilla/TemplateLib.h"

// Helper for log2 of powers of 2 at compile time.
template<size_t N>
struct Log2 : mozilla::tl::CeilingLog2<N>
{
  using mozilla::tl::CeilingLog2<N>::value;
  static_assert(1ULL << value == N, "Number is not a power of 2");
};
#define LOG2(N) Log2<N>::value

// Compare two addresses. Returns whether the first address is smaller (-1),
// equal (0) or greater (1) than the second address.
template<typename T>
int
CompareAddr(T* aAddr1, T* aAddr2)
{
  uintptr_t addr1 = reinterpret_cast<uintptr_t>(aAddr1);
  uintptr_t addr2 = reinterpret_cast<uintptr_t>(aAddr2);

  return (addr1 > addr2) - (addr1 < addr2);
}

// User-defined literals to make constants more legible
constexpr unsigned long long int operator"" _KiB(unsigned long long int aNum)
{
  return aNum * 1024;
}

constexpr unsigned long long int operator"" _MiB(unsigned long long int aNum)
{
  return aNum * 1024_KiB;
}

#endif
