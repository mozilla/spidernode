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
#include "conversions.h"
#include "v8local.h"
#include "autojsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Proxy.h"

static_assert(sizeof(v8::Value) == sizeof(JS::Value),
              "v8::Value and JS::Value must be binary compatible");

namespace v8 {

#define SIMPLE_VALUE(V8_VAL, SM_VAL) \
  bool Value::Is##V8_VAL() const { return GetValue(this)->is##SM_VAL(); }
#define COMMON_VALUE(NAME) SIMPLE_VALUE(NAME, NAME)
// Needed to get around clang preprocessor issue with pasting next to :: token.
#define CONCATENATE(X) ESClass:: X
#define ES_BUILTIN(V8_NAME, CLASS_NAME)                          \
  bool Value::Is##V8_NAME() const {                              \
    if (!IsObject()) {                                           \
      return false;                                              \
    }                                                            \
    JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent()); \
    AutoJSAPI jsAPI(cx);                                         \
    JS::RootedObject obj(cx, GetObject(this));                   \
    js::ESClass cls = js::ESClass::Other;                        \
    return js::GetBuiltinClass(cx, obj, &cls) &&                 \
           cls == js::CONCATENATE(CLASS_NAME);                   \
  }
#define TYPED_ARRAY(NAME)                       \
  bool Value::Is##NAME##Array() const {         \
    if (!IsObject()) {                          \
      return false;                             \
    }                                           \
    return JS_Is##NAME##Array(GetObject(this)); \
  }
#include "valuemap.inc"
#undef COMMON_VALUE
#undef SIMPLE_VALUE
#undef CONCATENATE
#undef ES_BUILTIN
#undef TYPED_ARRAY

bool Value::IsUint32() const {
  if (!IsNumber()) {
    return false;
  }
  double value = GetValue(this)->toNumber();
  return !internal::IsMinusZero(value) && value >= 0 &&
         value <= internal::kMaxUInt32 &&
         value == internal::FastUI2D(internal::FastD2UI(value));
}

bool Value::IsTypedArray() const {
  if (!IsObject()) {
    return false;
  }
  AutoJSAPI jsAPI(this);
  return JS_IsTypedArrayObject(GetObject(this));
}

bool Value::IsDataView() const {
  if (!IsObject()) {
    return false;
  }
  AutoJSAPI jsAPI(this);
  return JS_IsDataViewObject(GetObject(this));
}

bool Value::IsArrayBufferView() const {
  return IsDataView() || IsTypedArray();
}

bool Value::IsFunction() const {
  if (!IsObject()) {
    return false;
  }
  AutoJSAPI jsAPI(this);
  return JS::IsCallable(GetObject(this));
}

MaybeLocal<Boolean> Value::ToBoolean(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  JS::Value boolVal;
  boolVal.setBoolean(JS::ToBoolean(thisVal));
  return internal::Local<Boolean>::New(context->GetIsolate(), boolVal);
}

Local<Boolean> Value::ToBoolean(Isolate* isolate) const {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  return ToBoolean(isolate->GetCurrentContext()).ToLocalChecked();
}

Maybe<bool> Value::BooleanValue(Local<Context> context) const {
  MaybeLocal<Boolean> maybeBool = ToBoolean(context);
  if (maybeBool.IsEmpty()) {
    return Nothing<bool>();
  }
  return Just(maybeBool.ToLocalChecked()->Value());
}

bool Value::BooleanValue() const {
  return BooleanValue(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(false);
}

MaybeLocal<Number> Value::ToNumber(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  double num = 0.0;
  if (!thisVal.isUndefined() && !JS::ToNumber(cx, thisVal, &num) || num != num) {
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
  return ToNumber(isolate->GetCurrentContext()).FromMaybe(Local<Number>());
}

Maybe<double> Value::NumberValue(Local<Context> context) const {
  MaybeLocal<Number> maybeNum = ToNumber(context);
  if (maybeNum.IsEmpty()) {
    return Nothing<double>();
  }
  return Just(maybeNum.ToLocalChecked()->Value());
}

double Value::NumberValue() const {
  return NumberValue(Isolate::GetCurrent()->GetCurrentContext()).FromMaybe(NAN);
}

MaybeLocal<Integer> Value::ToInteger(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  double num = 0.0;
  if (!thisVal.isUndefined() && (!JS::ToNumber(cx, thisVal, &num) || num != num)) {
    return MaybeLocal<Integer>();
  }
  JS::Value numVal;
  numVal.setNumber(JS::ToInteger(num));
  return internal::Local<Integer>::New(context->GetIsolate(), numVal);
}

Local<Integer> Value::ToInteger(Isolate* isolate) const {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  return ToInteger(isolate->GetCurrentContext()).FromMaybe(Local<Integer>());
}

Maybe<int64_t> Value::IntegerValue(Local<Context> context) const {
  MaybeLocal<Integer> maybeInt = ToInteger(context);
  if (maybeInt.IsEmpty()) {
    return Nothing<int64_t>();
  }
  return Just(maybeInt.ToLocalChecked()->Value());
}

int64_t Value::IntegerValue() const {
  return IntegerValue(Isolate::GetCurrent()->GetCurrentContext()).FromMaybe(0);
}

MaybeLocal<Int32> Value::ToInt32(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  int32_t num = 0;
  if (!JS::ToInt32(cx, thisVal, &num)) {
    return MaybeLocal<Int32>();
  }
  JS::Value numVal;
  numVal.setNumber(double(num));
  return internal::Local<Int32>::New(context->GetIsolate(), numVal);
}

Local<Int32> Value::ToInt32(Isolate* isolate) const {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  return ToInt32(isolate->GetCurrentContext()).FromMaybe(Local<Int32>());
}

Maybe<int32_t> Value::Int32Value(Local<Context> context) const {
  MaybeLocal<Int32> maybeInt = ToInt32(context);
  if (maybeInt.IsEmpty()) {
    return Nothing<int32_t>();
  }
  return Just(maybeInt.ToLocalChecked()->Value());
}

int32_t Value::Int32Value() const {
  return Int32Value(Isolate::GetCurrent()->GetCurrentContext()).FromMaybe(0);
}

MaybeLocal<Uint32> Value::ToUint32(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  uint32_t num = 0;
  if (!JS::ToUint32(cx, thisVal, &num)) {
    return MaybeLocal<Uint32>();
  }
  JS::Value numVal;
  numVal.setNumber(num);
  return internal::Local<Uint32>::New(context->GetIsolate(), numVal);
}

Local<Uint32> Value::ToUint32(Isolate* isolate) const {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  return ToUint32(isolate->GetCurrentContext()).FromMaybe(Local<Uint32>());
}

Maybe<uint32_t> Value::Uint32Value(Local<Context> context) const {
  MaybeLocal<Uint32> maybeInt = ToUint32(context);
  if (maybeInt.IsEmpty()) {
    return Nothing<uint32_t>();
  }
  return Just(maybeInt.ToLocalChecked()->Value());
}

uint32_t Value::Uint32Value() const {
  return Uint32Value(Isolate::GetCurrent()->GetCurrentContext()).FromMaybe(0);
}

MaybeLocal<String> Value::ToString(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
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
  return ToString(isolate->GetCurrentContext()).FromMaybe(Local<String>());
}

MaybeLocal<Object> Value::ToObject(Local<Context> context) const {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  JSObject* obj = JS::ToObject(cx, thisVal);
  if (!obj) {
    return MaybeLocal<Object>();
  }
  JS::Value objVal;
  objVal.setObject(*obj);
  return internal::Local<Object>::New(context->GetIsolate(), objVal);
}

Local<Object> Value::ToObject(Isolate* isolate) const {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  return ToObject(isolate->GetCurrentContext()).FromMaybe(Local<Object>());
}

Maybe<bool> Value::Equals(Local<Context> context, Handle<Value> that) const {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  JS::RootedValue thatVal(cx, *GetValue(that));
  bool equal = false;
  if (!JS_WrapValue(cx, &thisVal) ||
      !JS_WrapValue(cx, &thatVal) ||
      !JS_LooselyEqual(cx, thisVal, thatVal, &equal)) {
    return Nothing<bool>();
  }
  return Just(equal);
}

bool Value::Equals(Handle<Value> that) const {
  return Equals(Isolate::GetCurrent()->GetCurrentContext(), that)
      .FromMaybe(false);
}

bool Value::StrictEquals(Handle<Value> that) const {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  JS::RootedValue thatVal(cx, *GetValue(that));
  if (!JS_WrapValue(cx, &thatVal)) {
    return false;
  }
  bool equal = false;
  JS_StrictlyEqual(cx, thisVal, thatVal, &equal);
  return equal;
}

bool Value::SameValue(Handle<Value> that) const {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  AutoJSAPI jsAPI(cx);
  JS::RootedValue thisVal(cx, *GetValue(this));
  JS::RootedValue thatVal(cx, *GetValue(that));
  if (!JS_WrapValue(cx, &thatVal)) {
    return false;
  }
  bool same = false;
  JS_SameValue(cx, thisVal, thatVal, &same);
  return same;
}

bool Value::IsNativeError() const {
  if (!IsObject()) {
    return false;
  }
  AutoJSAPI jsAPI(this);
  JSProtoKey key = JS::IdentifyStandardInstanceOrPrototype(GetObject(this));
  return key == JSProto_EvalError || key == JSProto_RangeError ||
         key == JSProto_ReferenceError || key == JSProto_SyntaxError ||
         key == JSProto_TypeError || key == JSProto_URIError;
}

bool Value::IsExternal() const { return External::IsExternal(this); }

bool
Value::IsProxy() const
{
  JSObject* obj = GetObject(this);
  if (!obj) {
    return false;
  }

  return js::IsProxy(obj);
}

bool
Value::IsSharedArrayBuffer() const
{
  JSObject* obj = GetObject(this);
  if (!obj) {
    return false;
  }

  return JS_IsSharedArrayBufferObject(obj);
}

bool
Value::IsSymbol() const
{
  return GetValue(this)->isSymbol();
}
}
