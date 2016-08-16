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
#include "js/Value.h"

namespace v8 {

static inline JS::Value* GetValue(Value* val) {
  return reinterpret_cast<JS::Value*>(val);
}

static inline const JS::Value* GetValue(const Value* val) {
  return reinterpret_cast<const JS::Value*>(val);
}

template <class T>
static inline JS::Value* GetValue(const Local<T>& val) {
  return GetValue(*val);
}

static inline JS::Value* GetValue(Template* val) {
  return reinterpret_cast<JS::Value*>(val);
}

static inline const JS::Value* GetValue(const Template* val) {
  return reinterpret_cast<const JS::Value*>(val);
}

static inline JS::Value* GetValue(Signature* val) {
  return reinterpret_cast<JS::Value*>(val);
}

static inline const JS::Value* GetValue(const Signature* val) {
  return reinterpret_cast<const JS::Value*>(val);
}

static inline JS::Value* GetValue(AccessorSignature* val) {
  return reinterpret_cast<JS::Value*>(val);
}

static inline const JS::Value* GetValue(const AccessorSignature* val) {
  return reinterpret_cast<const JS::Value*>(val);
}

static inline Value* GetV8Value(JS::Value* val) {
  return reinterpret_cast<Value*>(val);
}

static inline const Value* GetV8Value(const JS::Value* val) {
  return reinterpret_cast<const Value*>(val);
}

static inline Value* GetV8Value(JS::MutableHandleValue val) {
  return GetV8Value(val.address());
}

static inline Value* GetV8Value(Template* val) {
  return GetV8Value(GetValue(val));
}

static inline const Value* GetV8Value(const Template* val) {
  return GetV8Value(GetValue(val));
}

static inline Template* GetV8Template(JS::Value* val) {
  return reinterpret_cast<Template*>(val);
}

static inline Value* GetV8Value(Signature* val) {
  return GetV8Value(GetValue(val));
}

static inline const Value* GetV8Value(const Signature* val) {
  return GetV8Value(GetValue(val));
}

static inline Signature* GetV8Signature(JS::Value* val) {
  return reinterpret_cast<Signature*>(val);
}

static inline Value* GetV8Value(AccessorSignature* val) {
  return GetV8Value(GetValue(val));
}

static inline const Value* GetV8Value(const AccessorSignature* val) {
  return GetV8Value(GetValue(val));
}

static inline AccessorSignature* GetV8AccessorSignature(JS::Value* val) {
  return reinterpret_cast<AccessorSignature*>(val);
}

static inline JSObject* GetObject(Value* val) {
  JS::Value* v = GetValue(val);
  return v->isObject() ? &v->toObject() : nullptr;
}

static inline JSObject* GetObject(const Value* val) {
  const JS::Value* v = GetValue(val);
  return v->isObject() ? &v->toObject() : nullptr;
}

static inline JSObject* GetObject(Template* val) {
  return GetObject(GetV8Value(val));
}

static inline JSObject* GetObject(const Template* val) {
  return GetObject(GetV8Value(val));
}

template <class T>
static inline JSObject* GetObject(const Local<T>& val) {
  return GetObject(*val);
}

static inline JSString* GetString(Value* val) {
  JS::Value* v = GetValue(val);
  return v->isString() ? v->toString() : nullptr;
}

static inline JSString* GetString(const Value* val) {
  const JS::Value* v = GetValue(val);
  return v->isString() ? v->toString() : nullptr;
}

static inline JSString* GetString(const Local<Value>& val) {
  return GetString(*val);
}

// Various callbacks cannot be represented in a single JS::Value, because the
// max size of the stored private is 60 bits.  So we need to encode pointer
// values as two private values.  The first one stores the most significant 60
// bits, while the second one stores the remaining 4 bits.  Since the lowest
// significant bit of private values cannot be set, we need to shift to the right
// by 3 instead of 4.
template<typename T>
static inline T ValuesToCallback(JS::HandleValue val1,
                                 JS::HandleValue val2) {
  void* ptr1 = val1.toPrivate();
  void* ptr2 = val2.toPrivate();
  assert(((reinterpret_cast<uintptr_t>(ptr1) << 3) & 0x000000000000000f) == 0);
  assert(((reinterpret_cast<uintptr_t>(ptr2) >> 1) & 0xfffffffffffffff0) == 0);
  return reinterpret_cast<T>
    ((reinterpret_cast<uintptr_t>(ptr1) << 3) |
     (reinterpret_cast<uintptr_t>(ptr2) >> 1));
}

template<typename T>
static inline void CallbackToValues(T callback,
                                    JS::MutableHandleValue val1,
                                    JS::MutableHandleValue val2) {
  void* ptr1 = reinterpret_cast<void*>
    ((reinterpret_cast<uintptr_t>(callback) & 0xfffffffffffffff0) >> 3);
  void* ptr2 = reinterpret_cast<void*>
    ((reinterpret_cast<uintptr_t>(callback) & 0x000000000000000f) << 1);
  val1.setPrivate(ptr1);
  val2.setPrivate(ptr2);
  assert(ValuesToCallback<T>(val1, val2) == callback);
}

}
