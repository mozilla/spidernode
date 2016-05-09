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
  static void FinalizeExternalString(const JSStringFinalizer* fin,
                                     char16_t* chars);
};

struct ExternalOneByteStringFinalizer
    : ExternalStringFinalizerBase<ExternalOneByteStringFinalizer> {
  ExternalOneByteStringFinalizer(String::ExternalStringResourceBase* resource);
  static void FinalizeExternalString(const JSStringFinalizer* fin,
                                     char16_t* chars);
};

template <typename CharType>
static inline int Write(const String* string, CharType* buffer, int start,
                        int length, int options);

// This is similar to the function of the same name in jsfriendapi, except that
// it accepts an extra parameter, "start", that specifies the index in the "s"
// string from which to begin copying.  And it's a template with specializations
// so that CopyStringChars can call CopyLinearStringChars for both char16_t*
// and char* "dest" arrays.
//
// TODO: upstream these enhancements into jsfriendapi.
//
template <typename CharType>
MOZ_ALWAYS_INLINE void CopyLinearStringChars(CharType* dest, JSLinearString* s,
                                             size_t len, size_t start);

MOZ_ALWAYS_INLINE void CopyLinearStringChars(char16_t* dest, JSLinearString* s,
                                             size_t len, size_t start) {
  JS::AutoCheckCannotGC nogc;
  if (js::LinearStringHasLatin1Chars(s)) {
    const JS::Latin1Char* src = js::GetLatin1LinearStringChars(nogc, s);
    for (size_t i = 0; i < len; i++) {
      dest[i] = src[start + i];
    }
  } else {
    const char16_t* src = js::GetTwoByteLinearStringChars(nogc, s);
    mozilla::PodCopy(dest, src + start, len);
  }
}

// There are two functions that do something similar, the public jsapi function
// JS_EncodeStringToBuffer and the private jsstr function DeflateStringToBuffer.
// But neither does quite what we want.  This version is mostly based on
// the char16_t* specialization of CopyLinearStringChars, with some inspiration
// from those other functions.
MOZ_ALWAYS_INLINE void CopyLinearStringChars(char* dest, JSLinearString* s,
                                             size_t len, size_t start) {
  JS::AutoCheckCannotGC nogc;
  if (js::LinearStringHasLatin1Chars(s)) {
    const JS::Latin1Char* src = js::GetLatin1LinearStringChars(nogc, s);
    for (size_t i = 0; i < len; i++) {
      dest[i] = char(src[start + i]);
    }
  } else {
    const char16_t* src = js::GetTwoByteLinearStringChars(nogc, s);
    for (size_t i = 0; i < len; i++) {
      dest[i] = char(src[start + i]);
    }
  }
}

// Based on CopyStringChars in jsfriendapi, except for the "start" parameter
// (as described above CopyLinearStringChars) and templatization (so it can take
// both char16_t* and char* "dest" arrays).
template <typename CharType>
inline bool CopyStringChars(JSContext* cx, CharType* dest, JSString* s,
                            size_t len, size_t start) {
  JSLinearString* linear = js::StringToLinearString(cx, s);
  if (!linear) {
    return false;
  }

  CopyLinearStringChars(dest, linear, len, start);
  return true;
}

}
}
