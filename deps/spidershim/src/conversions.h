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
#include "jsapi.h"

namespace v8 {

static inline JS::Value* GetValue(Value* val) {
  return reinterpret_cast<JS::Value*>(val);
}

static inline const JS::Value* GetValue(const Value* val) {
  return reinterpret_cast<const JS::Value*>(val);
}

static inline JS::Value* GetValue(const Local<Value>& val) {
  return GetValue(*val);
}

static inline JS::Value* GetValue(Template* val) {
  return reinterpret_cast<JS::Value*>(val);
}

static inline Value* GetV8Value(JS::Value* val) {
  return reinterpret_cast<Value*>(val);
}

static inline Value* GetV8Value(JS::MutableHandleValue val) {
  return GetV8Value(val.address());
}

static inline Value* GetV8Value(Template* val) {
  return GetV8Value(GetValue(val));
}

static inline Template* GetV8Template(JS::Value* val) {
  return reinterpret_cast<Template*>(val);
}

static inline JSObject* GetObject(Value* val) {
  JS::Value* v = GetValue(val);
  return v->isObject() ? &v->toObject() : nullptr;
}

static inline JSObject* GetObject(const Value* val) {
  const JS::Value* v = GetValue(val);
  return v->isObject() ? &v->toObject() : nullptr;
}

static inline JSObject* GetObject(const Local<Value>& val) {
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
}
