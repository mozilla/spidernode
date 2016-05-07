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

#include <assert.h>

#include "v8.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CharacterEncoding.h"
#include "v8isolate.h"
#include "v8local.h"
#include "v8string.h"

namespace v8 {

String::Utf8Value::Utf8Value(Handle<v8::Value> obj)
  : str_(nullptr), length_(0) {
  Local<String> strVal = obj->ToString();
  JSString* str = nullptr;
  if (*strVal) {
    str = reinterpret_cast<JS::Value*>(*strVal)->toString();
  }
  if (str) {
    JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
    JSFlatString* flat = JS_FlattenString(cx, str);
    if (flat) {
      length_ = JS::GetDeflatedUTF8StringLength(flat);
      str_ = new char[length_ + 1];
      JS::DeflateStringToUTF8Buffer(flat, mozilla::RangedPtr<char>(str_, length_));
      str_[length_] = '\0';
    }
  }
}

String::Utf8Value::~Utf8Value() {
  delete[] str_;
}

String::Value::Value(Handle<v8::Value> obj)
  : str_(nullptr), length_(0) {
  Local<String> strVal = obj->ToString();
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JS::UniqueTwoByteChars buffer = internal::GetFlatString(cx, strVal, &length_);
  if (buffer) {
    str_ = buffer.release();
  } else {
    length_ = 0;
  }
}

String::Value::~Value() {
  js_free(str_);
}

Local<String> String::NewFromUtf8(Isolate* isolate, const char* data,
                                  NewStringType type, int length) {
  return NewFromUtf8(isolate, data, static_cast<v8::NewStringType>(type),
                     length).FromMaybe(Local<String>());
}

MaybeLocal<String> String::NewFromUtf8(Isolate* isolate, const char* data,
                                       v8::NewStringType type, int length) {
  // In V8's api.cc, this conditional block is annotated with the comment:
  //    TODO(dcarney): throw a context free exception.
  if (length > String::kMaxLength) {
    return MaybeLocal<String>();
  }

  if (length < 0) {
    length = strlen(data);
  }

  JSContext* cx = JSContextFromIsolate(isolate);

  size_t twoByteLen;
  JS::UniqueTwoByteChars twoByteChars(
    JS::LossyUTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(data, length), &twoByteLen).get());
  if (!twoByteChars) {
    return MaybeLocal<String>();
  }

  JSString* str =
    type == v8::NewStringType::kInternalized ?
      JS_AtomizeAndPinUCString(cx, reinterpret_cast<const char16_t*>(twoByteChars.release())) :
      JS_NewUCString(cx, twoByteChars.release(), twoByteLen);

  if (!str) {
    return MaybeLocal<String>();
  }

  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

MaybeLocal<String> String::NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                          v8::NewStringType type, int length) {
  // In V8's api.cc, this conditional block is annotated with the comment:
  //    TODO(dcarney): throw a context free exception.
  if (length > String::kMaxLength) {
    return MaybeLocal<String>();
  }

  JSContext* cx = JSContextFromIsolate(isolate);

  JSString* str =
    type == v8::NewStringType::kInternalized ?
      length >= 0 ?
        JS_AtomizeAndPinStringN(cx, reinterpret_cast<const char*>(data), length) :
        JS_AtomizeAndPinString(cx, reinterpret_cast<const char*>(data)) :
      length >= 0 ?
        JS_NewStringCopyN(cx, reinterpret_cast<const char*>(data), length) :
        JS_NewStringCopyZ(cx, reinterpret_cast<const char*>(data));

  if (!str) {
    return MaybeLocal<String>();
  }

  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

Local<String> String::NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                     NewStringType type, int length) {
  return NewFromOneByte(isolate, data, static_cast<v8::NewStringType>(type),
                        length).FromMaybe(Local<String>());
}

MaybeLocal<String> String::NewFromTwoByte(Isolate* isolate, const uint16_t* data,
                                          v8::NewStringType type, int length) {
  // In V8's api.cc, this conditional block is annotated with the comment:
  //    TODO(dcarney): throw a context free exception.
  if (length > String::kMaxLength) {
    return MaybeLocal<String>();
  }

  JSContext* cx = JSContextFromIsolate(isolate);

  JSString* str =
    type == v8::NewStringType::kInternalized ?
      length >= 0 ?
        JS_AtomizeAndPinUCStringN(cx, reinterpret_cast<const char16_t*>(data), length) :
        JS_AtomizeAndPinUCString(cx, reinterpret_cast<const char16_t*>(data)) :
      length >= 0 ?
        JS_NewUCStringCopyN(cx, reinterpret_cast<const char16_t*>(data), length) :
        JS_NewUCStringCopyZ(cx, reinterpret_cast<const char16_t*>(data));

  if (!str) {
    return MaybeLocal<String>();
  }

  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

Local<String> String::NewFromTwoByte(Isolate* isolate, const uint16_t* data,
                                     NewStringType type, int length) {
  return NewFromTwoByte(isolate, data, static_cast<v8::NewStringType>(type),
                        length).FromMaybe(Local<String>());
}

MaybeLocal<String> String::NewExternalTwoByte(Isolate* isolate,
                                              ExternalStringResource* resource) {
  // In V8's api.cc, this conditional block is annotated with the comment:
  //    TODO(dcarney): throw a context free exception.
  if (resource->length() > static_cast<size_t>(String::kMaxLength)) {
    return MaybeLocal<String>();
  }

  JSContext* cx = JSContextFromIsolate(isolate);

  auto fin = mozilla::MakeUnique<internal::ExternalStringFinalizer>(resource);
  if (!fin) {
    return MaybeLocal<String>();
  }

  JSString* str =
    JS_NewExternalString(cx, reinterpret_cast<const char16_t*>(resource->data()),
                         resource->length(), fin.release());
  if (!str) {
    return MaybeLocal<String>();
  }
  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

MaybeLocal<String> String::NewExternalOneByte(Isolate* isolate,
                                              ExternalOneByteStringResource* resource) {
  // In V8's api.cc, this conditional block is annotated with the comment:
  //    TODO(dcarney): throw a context free exception.
  if (resource->length() > static_cast<size_t>(String::kMaxLength)) {
    return MaybeLocal<String>();
  }

  JSContext* cx = JSContextFromIsolate(isolate);

  auto fin = mozilla::MakeUnique<internal::ExternalOneByteStringFinalizer>(resource);
  if (!fin) {
    return MaybeLocal<String>();
  }

  // SpiderMonkey doesn't have a one-byte variant of JS_NewExternalString, so we
  // inflate the external string data to its two-byte equivalent.  According to
  // https://github.com/mozilla/spidernode/blob/7466b2f/deps/v8/include/v8.h#L2252-L2260
  // the external string data is immutable, so there's no risk of it changing
  // after we inflate it.  But we do need to ensure that the inflated data gets
  // deleted when the string is finalized, which FinalizeExternalString does.
  size_t length = resource->length();
  JS::UniqueTwoByteChars data(js_pod_malloc<char16_t>(length + 1));
  if (!data) {
    return MaybeLocal<String>();
  }
  const char* oneByteData = resource->data();
  for (size_t i = 0; i < length; ++i)
      data[i] = (unsigned char) oneByteData[i];
  data[length] = 0;

  JSString* str =
    JS_NewExternalString(cx, data.release(), length, fin.release());
  if (!str) {
    return MaybeLocal<String>();
  }

  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

Local<String> String::NewExternal(Isolate* isolate,
                                  ExternalStringResource* resource) {
  return NewExternalTwoByte(isolate, resource).FromMaybe(Local<String>());
}

Local<String> String::NewExternal(Isolate* isolate,
                                  ExternalOneByteStringResource* resource) {
  return NewExternalOneByte(isolate, resource).FromMaybe(Local<String>());
}

String* String::Cast(v8::Value* obj) {
  assert(reinterpret_cast<JS::Value*>(obj)->isString());
  return static_cast<String*>(obj);
}

int String::Length() const {
  JSString* thisStr = reinterpret_cast<const JS::Value*>(this)->toString();
  return JS_GetStringLength(thisStr);
}

int String::Utf8Length() const {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JSString* thisStr = reinterpret_cast<const JS::Value*>(this)->toString();
  JSFlatString* flat = JS_FlattenString(cx, thisStr);
  if (!flat) {
    return 0;
  }
  return JS::GetDeflatedUTF8StringLength(flat);
}

Local<String> String::Empty(Isolate* isolate) {
  return NewFromUtf8(isolate, "");
}

Local<String> String::Concat(Handle<String> left, Handle<String> right) {
  if (left->Length() + right->Length() > String::kMaxLength) {
    return Local<String>();
  }

  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedString leftStr(cx, reinterpret_cast<JS::Value*>(*left)->toString());
  JS::RootedString rightStr(cx, reinterpret_cast<JS::Value*>(*right)->toString());
  JSString* result = JS_ConcatStrings(cx, leftStr, rightStr);
  if (!result) {
    return Empty(isolate);
  }
  JS::Value retVal;
  retVal.setString(result);
  return internal::Local<String>::New(isolate, retVal);
}

int String::Write(uint16_t* buffer, int start, int length, int options) const {
  return internal::Write(this, reinterpret_cast<char16_t*>(buffer), start, length, options);
}

int String::WriteOneByte(uint8_t* buffer, int start, int length, int options) const {
  return internal::Write(this, reinterpret_cast<char*>(buffer), start, length, options);
}

// Based on String::WriteUtf8 in V8's api.cc.
int String::WriteUtf8(char* buffer, int capacity, int* numChars, int options) const {
  bool nullTermination = !(options & NO_NULL_TERMINATION);

  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JSString* thisStr = reinterpret_cast<const JS::Value*>(this)->toString();
  JSLinearString* linearStr = js::StringToLinearString(cx, thisStr);
  if (!linearStr) {
    return 0;
  }

  // The number of bytes written to the buffer.  DeflateStringToUTF8Buffer
  // expects its initial value to be the capacity of the buffer, and it uses it
  // to determine whether/when to abort writing bytes to the buffer.
  size_t numBytes = (size_t)capacity;

  // The number of Unicode characters written to the buffer. This could be
  // less than the length of the string, if the buffer isn't big enough to hold
  // the whole string.
  if (numChars != nullptr) {
    *numChars = 0;
  }

  bool completed = internal::DeflateStringToUTF8Buffer(linearStr, buffer, &numBytes, numChars, options);

  if (completed) {
    // If the caller requested null termination, but the buffer doesn't have
    // any more space for it, then disable null termination.
    if (nullTermination && (size_t)capacity == numBytes) {
      nullTermination = false;
    }
  } else {
    // Deflation was aborted, so don't null terminate, regardless of what
    // the caller requested.  Presumably this is because deflation only aborts
    // when the buffer runs out of space (although it's possible for the buffer
    // to still have space for null termination in that case, if the abort
    // happens on a character that deflates to multiple bytes, and the buffer
    // has at least one free byte, but not enough to hold the whole character).
    nullTermination = false;
  }

  if (nullTermination) {
    buffer[numBytes++] = '\0';
  }

  return numBytes;
}

namespace internal {

JS::UniqueTwoByteChars GetFlatString(JSContext* cx, v8::Local<String> source, size_t* length) {
  auto sourceJsVal = reinterpret_cast<JS::Value*>(*source);
  auto sourceStr = sourceJsVal->toString();
  size_t len = JS_GetStringLength(sourceStr);
  if (length) {
    *length = len;
  }
  JS::UniqueTwoByteChars buffer(js_pod_malloc<char16_t>(len + 1));
  mozilla::Range<char16_t> dest(buffer.get(), len + 1);
  if (!JS_CopyStringChars(cx, dest, sourceStr)) {
    return nullptr;
  }
  buffer[len] = '\0';
  return buffer;
}

template<typename T>
ExternalStringFinalizerBase<T>::ExternalStringFinalizerBase(String::ExternalStringResourceBase* resource)
  : resource_(resource) {
    static_cast<T*>(this)->finalize = T::FinalizeExternalString;
  }

template<typename T>
void ExternalStringFinalizerBase<T>::dispose() {
  // Based on V8's Heap::FinalizeExternalString.

  // Dispose of the C++ object if it has not already been disposed.
  if (this->resource_ != NULL) {
    this->resource_->Dispose();
    this->resource_ = nullptr;
  }

  // Delete ourselves, which JSExternalString::finalize doesn't do,
  // presumably because it assumes we're static.
  delete this;
};

ExternalStringFinalizer::ExternalStringFinalizer(String::ExternalStringResourceBase* resource)
  : ExternalStringFinalizerBase(resource) {}

void ExternalStringFinalizer::FinalizeExternalString(const JSStringFinalizer* fin, char16_t* chars) {
  const_cast<internal::ExternalStringFinalizer*>(
    static_cast<const internal::ExternalStringFinalizer*>(fin))->dispose();
}

ExternalOneByteStringFinalizer::ExternalOneByteStringFinalizer(String::ExternalStringResourceBase* resource)
  : ExternalStringFinalizerBase(resource) {}

void ExternalOneByteStringFinalizer::FinalizeExternalString(const JSStringFinalizer* fin, char16_t* chars) {
  const_cast<internal::ExternalStringFinalizer*>(
    static_cast<const internal::ExternalStringFinalizer*>(fin))->dispose();

  // NewExternalOneByte made a two-byte copy of the data in the resource,
  // and this is that copy. The resource will handle deleting its original
  // data, but we have to delete this copy.
  delete chars;
}

// Based on WriteHelper in V8's api.cc.
template<typename CharType>
static inline int Write(const String* string, CharType* buffer, int start,
                        int length, int options) {
  MOZ_ASSERT(start >= 0 && length >= -1);

  JSString* str = reinterpret_cast<const JS::Value*>(string)->toString();
  int strLength = JS_GetStringLength(str);
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);

  int end = start + length;
  if ((length == -1) || (length > strLength - start)) {
    end = strLength;
  }
  if (end < 0) {
    return 0;
  }

  if (!internal::CopyStringChars(cx, buffer, str, end - start, start)) {
    return 0;
  }

  if (!(options & String::NO_NULL_TERMINATION) &&
      (length == -1 || end - start < length)) {
    buffer[end - start] = '\0';
  }

  return end - start;
}

// This is identical (modulo formatting) to the function of the same name
// in jsstr.
//
// TODO: expose that function and get rid of this one.
//
uint32_t
OneUcs4ToUtf8Char(uint8_t* utf8Buffer, uint32_t ucs4Char) {
  MOZ_ASSERT(ucs4Char <= 0x10FFFF);

  if (ucs4Char < 0x80) {
    utf8Buffer[0] = uint8_t(ucs4Char);
    return 1;
  }

  uint32_t a = ucs4Char >> 11;
  uint32_t utf8Length = 2;
  while (a) {
    a >>= 5;
    utf8Length++;
  }

  MOZ_ASSERT(utf8Length <= 4);

  uint32_t i = utf8Length;
  while (--i) {
    utf8Buffer[i] = uint8_t((ucs4Char & 0x3F) | 0x80);
    ucs4Char >>= 6;
  }

  utf8Buffer[0] = uint8_t(0x100 - (1 << (8 - utf8Length)) + ucs4Char);
  return utf8Length;
}

const char16_t UTF8_REPLACEMENT_CHAR = u'\uFFFD';

// There are two existing versions of this function: the public one
// in CharacterEncoding and a private one in CTypes.
//
// Neither of them does quite what we want, since CharacterEncoding requires
// the caller to ensure that the buffer is big enough to deflate the whole
// string, while CTypes abort on a bad surrogate.  Whereas we want to abort
// if the buffer runs out of space while continuing on a bad surrogate (which
// we either replace with the replacement character or lossily convert).
//
// So this is a third implementation, based on those other two.  In theory,
// it should be possible to refactor all three into a single implementation.
//
// TODO: that.
//
template <typename CharT>
bool DeflateStringToUTF8Buffer(const CharT* src, size_t srclen,
                               char* dst, size_t* dstlenp, int* numchrp,
                               int options)
{
  size_t i, utf8Len;
  char16_t c, c2;
  uint32_t v;
  uint8_t utf8buf[6];

  size_t dstlen = *dstlenp;
  size_t origDstlen = dstlen;
  bool replaceInvalidUtf8 = (options & String::REPLACE_INVALID_UTF8);

  while (srclen) {
    c = *src++;
    srclen--;
    if (c >= 0xDC00 && c <= 0xDFFF) {
      // bad surrogate pair (trailing surrogate at first byte of pair)
      v = replaceInvalidUtf8 ? UTF8_REPLACEMENT_CHAR : c;
    }
    else if (c < 0xD800 || c > 0xDBFF) {
      // non-surrogate
      v = c;
    } else {
      // leading surrogate
      if (srclen < 1) {
        // bad surrogate pair (leading surrogate at end of string)
        v = replaceInvalidUtf8 ? UTF8_REPLACEMENT_CHAR : c;
      } else {
        c2 = *src;
        if ((c2 < 0xDC00) || (c2 > 0xDFFF)) {
          // bad surrogate pair (second byte of pair not a trailing surrogate)
          v = replaceInvalidUtf8 ? UTF8_REPLACEMENT_CHAR : c;
        } else {
          // valid surrogate pair
          src++;
          srclen--;
          v = ((c - 0xD800) << 10) + (c2 - 0xDC00) + 0x10000;
        }
      }
    }
    if (v < 0x0080) {
      /* no encoding necessary - performance hack */
      if (dstlen == 0) {
        goto bufferTooSmall;
      }
      *dst++ = (char) v;
      utf8Len = 1;
    } else {
      utf8Len = OneUcs4ToUtf8Char(utf8buf, v);
      if (utf8Len > dstlen) {
        goto bufferTooSmall;
      }
      for (i = 0; i < utf8Len; i++) {
        *dst++ = (char) utf8buf[i];
      }
    }
    dstlen -= utf8Len;
    if (numchrp != nullptr) {
      (*numchrp)++;
    }
  }
  *dstlenp = (origDstlen - dstlen);
  return true;

  bufferTooSmall:
    *dstlenp = (origDstlen - dstlen);
    return false;
}

template
bool DeflateStringToUTF8Buffer(const JS::Latin1Char* src, size_t srclen,
                               char* dst, size_t* dstlenp, int* numchrp,
                               int options);

template
bool DeflateStringToUTF8Buffer(const char16_t* src, size_t srclen,
                               char* dst, size_t* dstlenp, int* numchrp,
                               int options);

bool DeflateStringToUTF8Buffer(JSLinearString* str, char* dst, size_t* dstlenp,
                               int* numchrp, int options)
{
  size_t length = js::GetLinearStringLength(str);

  JS::AutoCheckCannotGC nogc;
  return js::LinearStringHasLatin1Chars(str)
         ? DeflateStringToUTF8Buffer(js::GetLatin1LinearStringChars(nogc, str),
                                     length, dst, dstlenp, numchrp, options)
         : DeflateStringToUTF8Buffer(js::GetTwoByteLinearStringChars(nogc, str),
                                     length, dst, dstlenp, numchrp, options);
}

}

}
