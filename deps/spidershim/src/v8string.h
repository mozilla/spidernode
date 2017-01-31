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
#include "v8.h"
#include "jsfriendapi.h"

struct JSContext;

namespace v8 {
namespace internal {

JS::UniqueTwoByteChars GetFlatString(JSContext* cx, v8::Local<String> source,
                                     size_t* length = nullptr);

template <typename T>
struct ExternalStringFinalizerBase : JSStringFinalizer {
  ExternalStringFinalizerBase(String::ExternalStringResourceBase* resource);
  String::ExternalStringResourceBase* resource_;
  void dispose();
};

struct ExternalStringFinalizer
    : ExternalStringFinalizerBase<ExternalStringFinalizer> {
  ExternalStringFinalizer(String::ExternalStringResourceBase* resource);
  static void FinalizeExternalString(JS::Zone* zone,
                                     const JSStringFinalizer* fin,
                                     char16_t* chars);
};

struct ExternalOneByteStringFinalizer
    : ExternalStringFinalizerBase<ExternalOneByteStringFinalizer> {
  ExternalOneByteStringFinalizer(String::ExternalStringResourceBase* resource);
  static void FinalizeExternalString(JS::Zone* zone,
                                     const JSStringFinalizer* fin,
                                     char16_t* chars);
};

template <typename CharType>
static inline int Write(const String* string, CharType* buffer, int start,
                        int length, int options);
}
}
