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
  assert(type == v8::NewStringType::kNormal); // TODO: Add support for interned strings
  JSContext* cx = JSContextFromIsolate(isolate);

  // In V8's api.cc, this conditional block is annotated with the comment:
  //    TODO(dcarney): throw a context free exception.
  if (length > v8::String::kMaxLength) {
    return MaybeLocal<String>();
  }

  if (length < 0) {
    length = strlen(data);
  }

  size_t twoByteLen;
  char16_t* twoByteChars = JS::UTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(data, length), &twoByteLen).get();
  JS::RootedString str(cx, JS_NewUCString(cx, twoByteChars, twoByteLen));
  if (!str) {
    return MaybeLocal<String>();
  }
  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

MaybeLocal<String> String::NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                          v8::NewStringType type, int length) {
  assert(type == v8::NewStringType::kNormal); // TODO: Add support for interned strings
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedString str(cx, length >= 0 ?
    JS_NewStringCopyN(cx, reinterpret_cast<const char*>(data), length) :
    JS_NewStringCopyZ(cx, reinterpret_cast<const char*>(data)));
  if (!str) {
    return MaybeLocal<String>();
  }
  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(isolate, strVal);
}

MaybeLocal<String> String::NewFromTwoByte(Isolate* isolate, const uint16_t* data,
                                          v8::NewStringType type, int length) {
  assert(type == v8::NewStringType::kNormal); // TODO: Add support for interned strings
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedString str(cx, length >= 0 ?
    JS_NewUCStringCopyN(cx, reinterpret_cast<const char16_t*>(data), length) :
    JS_NewUCStringCopyZ(cx, reinterpret_cast<const char16_t*>(data)));
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

struct ExternalStringFinalizer : JSStringFinalizer {
  ExternalStringFinalizer(String::ExternalStringResource* resource)
    : resource_(resource) {}
  String::ExternalStringResource* resource_;

  // XXX Define finalize() here as a struct member instead of assigning it
  // after instantiation.

  void dispose() {
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
};

void FinalizeExternalString(const JSStringFinalizer* fin, char16_t* chars) {
  const_cast<ExternalStringFinalizer*>(static_cast<const ExternalStringFinalizer*>(fin))->dispose();
}

MaybeLocal<String> String::NewExternalTwoByte(Isolate* isolate,
                                              ExternalStringResource* resource) {
  JSContext* cx = JSContextFromIsolate(isolate);

  auto fin = mozilla::MakeUnique<ExternalStringFinalizer>(resource);
  if (!fin) {
    return MaybeLocal<String>();
  }

  fin->finalize = FinalizeExternalString;

  JS::RootedString str(cx,
    JS_NewExternalString(cx, reinterpret_cast<const char16_t*>(resource->data()),
                         resource->length(), fin.release()));
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

}

}
