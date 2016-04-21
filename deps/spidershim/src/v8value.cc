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

#include "v8.h"
#include "v8conversions.h"
#include "v8local.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"

static_assert(sizeof(v8::Value) == sizeof(JS::Value),
              "v8::Value and JS::Value must be binary compatible");

namespace v8 {

#define SIMPLE_VALUE(V8_VAL, SM_VAL)                               \
  bool Value::Is##V8_VAL() const {                                 \
    return reinterpret_cast<const JS::Value*>(this)->is##SM_VAL(); \
  }
#define COMMON_VALUE(NAME) SIMPLE_VALUE(NAME, NAME)
#define ES_BUILTIN(V8_NAME, CLASS_NAME)                            \
  bool Value::Is##V8_NAME() const {                                \
    if (!IsObject()) {                                             \
      return false;                                                \
    }                                                              \
    JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());   \
    JS::RootedObject obj(cx,                                       \
      &reinterpret_cast<const JS::Value*>(this)->toObject());      \
    js::ESClassValue cls = js::ESClass_Other;                      \
    return js::GetBuiltinClass(cx, obj, &cls) &&                   \
           cls == js::ESClass_##CLASS_NAME;                        \
  }
#define TYPED_ARRAY(NAME)                                          \
  bool Value::Is##NAME##Array() const {                            \
    if (!IsObject()) {                                             \
      return false;                                                \
    }                                                              \
    return JS_Is##NAME##Array(                                     \
             &reinterpret_cast<const JS::Value*>(this)->toObject() \
           );                                                      \
  }
#include "valuemap.inc"
#undef COMMON_VALUE
#undef SIMPLE_VALUE
#undef ES_BUILTIN
#undef TYPED_ARRAY

bool Value::IsUint32() const {
  if (!IsNumber()) {
    return false;
  }
  double value = reinterpret_cast<const JS::Value*>(this)->toDouble();
  return !internal::IsMinusZero(value) &&
         value >= 0 &&
         value <= internal::kMaxUInt32 &&
         value == internal::FastUI2D(internal::FastD2UI(value));
}

bool Value::IsTypedArray() const {
  if (!IsObject()) {
    return false;
  }
  return JS_IsTypedArrayObject(&reinterpret_cast<const JS::Value*>(this)->toObject());
}

bool Value::IsDataView() const {
  if (!IsObject()) {
    return false;
  }
  return JS_IsDataViewObject(&reinterpret_cast<const JS::Value*>(this)->toObject());
}

MaybeLocal<Boolean> Value::ToBoolean(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedValue thisVal(cx, *reinterpret_cast<const JS::Value*>(this));
  JS::Value boolVal;
  boolVal.setBoolean(JS::ToBoolean(thisVal));
  return internal::Local<Boolean>::New(context->GetIsolate(), boolVal);
}

Local<Boolean> Value::ToBoolean(Isolate* isolate) const {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  return ToBoolean(isolate->GetCurrentContext()).
           ToLocalChecked();
}

MaybeLocal<Number> Value::ToNumber(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedValue thisVal(cx, *reinterpret_cast<const JS::Value*>(this));
  double num = 0.0;
  if (!JS::ToNumber(cx, thisVal, &num)) {
    return MaybeLocal<Number>();
  }
  JS::Value numVal;
  numVal.setNumber(num);
  return internal::Local<Number>::New(context->GetIsolate(), numVal);
}

Local<Number> Value::ToNumber(Isolate* isolate) const {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  return ToNumber(isolate->GetCurrentContext()).
           FromMaybe(Local<Number>());
}

MaybeLocal<String> Value::ToString(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedValue thisVal(cx, *reinterpret_cast<const JS::Value*>(this));
  JSString* str = JS::ToString(cx, thisVal);
  if (!str) {
    return MaybeLocal<String>();
  }
  JS::Value strVal;
  strVal.setString(str);
  return internal::Local<String>::New(context->GetIsolate(), strVal);
}

Local<String> Value::ToString(Isolate* isolate) const {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  return ToString(isolate->GetCurrentContext()).
           FromMaybe(Local<String>());
}

}
