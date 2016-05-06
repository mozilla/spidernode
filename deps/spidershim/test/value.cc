// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

// From test-api.cc.
static int StrCmp16(uint16_t* a, uint16_t* b) {
  while (true) {
    if (*a == 0 && *b == 0) return 0;
    if (*a != *b) return 0 + *a - *b;
    a++;
    b++;
  }
}

// From test-api.cc.
static int StrNCmp16(uint16_t* a, uint16_t* b, int n) {
  while (true) {
    if (n-- == 0) return 0;
    if (*a == 0 && *b == 0) return 0;
    if (*a != *b) return 0 + *a - *b;
    a++;
    b++;
  }
}

// Translations from cctest assertion macros to gtest equivalents, so we can
// copy code from test-api.cc into this file with minimal modifications.
#define CHECK_EQ(a, b) EXPECT_EQ(a, b)
#define CHECK_NE(a, b) EXPECT_NE(a, b)

void TestBoolean(Isolate* isolate, bool value) {
  Local<Boolean> boolean = Boolean::New(isolate, value);
  EXPECT_TRUE(boolean->IsBoolean());
  EXPECT_FALSE(boolean->IsNumber());
  EXPECT_FALSE(boolean->IsBooleanObject());
  EXPECT_EQ(value, boolean->IsTrue());
  EXPECT_EQ(!value, boolean->IsFalse());
  EXPECT_EQ(value, boolean->Value());
  String::Utf8Value utf8(boolean->ToString());
  EXPECT_STREQ(value ? "true" : "false", *utf8);
  EXPECT_EQ(value, boolean->ToBoolean()->Value());
  EXPECT_EQ(value, boolean->BooleanValue());
  EXPECT_EQ(value ? 1.0 : 0.0, boolean->ToNumber()->Value());
  EXPECT_EQ(value ? 1.0 : 0.0, boolean->NumberValue());
  EXPECT_EQ(value ? 1 : 0, boolean->ToInteger()->Value());
  EXPECT_EQ(value ? 1 : 0, boolean->IntegerValue());
  EXPECT_EQ(value ? 1 : 0, boolean->ToInt32()->Value());
  EXPECT_EQ(value ? 1 : 0, boolean->Int32Value());
  EXPECT_EQ(value ? 1 : 0, boolean->ToUint32()->Value());
  EXPECT_EQ(value ? 1 : 0, boolean->Uint32Value());
  EXPECT_TRUE(boolean->ToObject()->IsBooleanObject());
  EXPECT_EQ(value, BooleanObject::Cast(*boolean->ToObject())->ValueOf());
}

TEST(SpiderShim, Boolean) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TestBoolean(engine.isolate(), true);
  TestBoolean(engine.isolate(), false);
}

template<class T>
void TestNumber(Isolate* isolate, T value, const char* strValue) {
  Local<Number> number = Number::New(isolate, value);
  EXPECT_TRUE(number->IsNumber());
  EXPECT_FALSE(number->IsBoolean());
  EXPECT_EQ(double(value), number->Value());
  String::Utf8Value utf8(number->ToString());
  EXPECT_STREQ(strValue, *utf8);
  EXPECT_EQ(!!value, number->ToBoolean()->Value());
  EXPECT_EQ(!!value, number->BooleanValue());
  EXPECT_EQ(value, number->ToNumber()->Value());
  EXPECT_EQ(value, number->NumberValue());
  EXPECT_EQ(value < 0 ? ceil(value) : floor(value), number->ToInteger()->Value());
  EXPECT_EQ(value < 0 ? ceil(value) : floor(value), number->IntegerValue());
  EXPECT_EQ(value < 0 ? ceil(value) : floor(value), number->ToInt32()->Value());
  EXPECT_EQ(value < 0 ? ceil(value) : floor(value), number->Int32Value());
  EXPECT_EQ(value < 0 ? uint32_t(ceil(value)) : floor(value), number->ToUint32()->Value());
  EXPECT_EQ(value < 0 ? uint32_t(ceil(value)) : floor(value), number->Uint32Value());
  EXPECT_TRUE(number->ToObject()->IsNumberObject());
  EXPECT_EQ(value, NumberObject::Cast(*number->ToObject())->ValueOf());
}

TEST(SpiderShim, Number) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TestNumber(engine.isolate(), 0, "0");
  TestNumber(engine.isolate(), 42, "42");
  TestNumber(engine.isolate(), 42.42, "42.42");
  TestNumber(engine.isolate(), -42.42, "-42.42");
}

template<class T>
class IntegerMaker;

template<>
class IntegerMaker<int> {
public:
  static Local<Integer> New(Isolate* isolate, int value) {
    return Integer::New(isolate, value);
  }
  typedef Int32 IntType;
  static constexpr const char* formatString = "%d";
};

template<>
class IntegerMaker<uint32_t> {
public:
  static Local<Integer> New(Isolate* isolate, uint32_t value) {
    return Integer::NewFromUnsigned(isolate, value);
  }
  typedef Uint32 IntType;
  static constexpr const char* formatString = "%u";
};

template<class T>
void TestInteger(Isolate* isolate, T value) {
  Local<Integer> integer = IntegerMaker<T>::New(isolate, value);
  EXPECT_TRUE(integer->IsNumber());
  EXPECT_FALSE(integer->IsBoolean());
  EXPECT_EQ(int64_t(value), integer->Value());
  typedef typename IntegerMaker<T>::IntType IntType;
  IntType* intVal = IntType::Cast(*integer);
  EXPECT_EQ(value, intVal->Value());
  String::Utf8Value utf8(intVal->ToString());
  char strValue[1024];
  sprintf(strValue, IntegerMaker<T>::formatString, value);
  EXPECT_STREQ(strValue, *utf8);
  EXPECT_EQ(!!value, intVal->ToBoolean()->Value());
  EXPECT_EQ(!!value, intVal->BooleanValue());
  EXPECT_EQ(value, intVal->ToNumber()->Value());
  EXPECT_EQ(value, intVal->NumberValue());
  EXPECT_EQ(value, intVal->ToInteger()->Value());
  EXPECT_EQ(value, intVal->IntegerValue());
  EXPECT_EQ(value, intVal->ToInt32()->Value());
  EXPECT_EQ(value, intVal->Int32Value());
  EXPECT_EQ(value, intVal->ToUint32()->Value());
  EXPECT_EQ(value, intVal->Uint32Value());
  EXPECT_TRUE(intVal->ToObject()->IsNumberObject());
  EXPECT_EQ(value, NumberObject::Cast(*intVal->ToObject())->ValueOf());
}

TEST(SpiderShim, Integer) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TestInteger(engine.isolate(), 0);
  TestInteger(engine.isolate(), 42);
  TestInteger(engine.isolate(), INT32_MAX);
  TestInteger(engine.isolate(), UINT32_MAX);
}

TEST(SpiderShim, Object) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Object> object = Object::New(engine.isolate());
  Local<String> foo =
    String::NewFromUtf8(engine.isolate(), "foo", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> bar =
    String::NewFromUtf8(engine.isolate(), "bar", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> baz =
    String::NewFromUtf8(engine.isolate(), "baz", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> qux =
    String::NewFromUtf8(engine.isolate(), "qux", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> value =
    String::NewFromUtf8(engine.isolate(), "value", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> writable =
    String::NewFromUtf8(engine.isolate(), "writable", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> get =
    String::NewFromUtf8(engine.isolate(), "get", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> set =
    String::NewFromUtf8(engine.isolate(), "set", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> configurable =
    String::NewFromUtf8(engine.isolate(), "configurable", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> enumerable =
    String::NewFromUtf8(engine.isolate(), "enumerable", NewStringType::kNormal).
      ToLocalChecked();
  Local<Integer> zero = Integer::New(engine.isolate(), 0);
  Local<Integer> one = Integer::New(engine.isolate(), 1);
  Local<String> two =
    String::NewFromUtf8(engine.isolate(), "two", NewStringType::kNormal).
      ToLocalChecked();

  EXPECT_FALSE(object->Has(foo));
  EXPECT_FALSE(object->Has(context, foo).FromJust());
  EXPECT_FALSE(object->Has(bar));
  EXPECT_FALSE(object->Has(context, bar).FromJust());
  EXPECT_FALSE(object->Has(baz));
  EXPECT_FALSE(object->Has(context, baz).FromJust());
  EXPECT_FALSE(object->Has(1));
  EXPECT_FALSE(object->Has(context, 1).FromJust());
  EXPECT_FALSE(object->Has(0));
  EXPECT_FALSE(object->Has(context, 0).FromJust());

  EXPECT_EQ(true, object->ToBoolean()->Value());
  EXPECT_EQ(true, object->BooleanValue());

  EXPECT_TRUE(object->Set(context, foo, zero).FromJust());
  EXPECT_TRUE(object->DefineOwnProperty(context, bar, one, ReadOnly).FromJust());
  EXPECT_TRUE(object->DefineOwnProperty(context, baz, two, PropertyAttribute(DontEnum | DontDelete)).FromJust());
  EXPECT_TRUE(object->Set(context, 1, zero).FromJust());
  EXPECT_TRUE(object->Set(context, 0, two).FromJust());

  Local<String> str = object->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ("[object Object]", *utf8);
  EXPECT_EQ(true, object->ToBoolean()->Value());
  EXPECT_EQ(true, object->BooleanValue());
  EXPECT_TRUE(object->ToNumber(context).IsEmpty());
  EXPECT_TRUE(object->NumberValue(context).IsNothing());
  EXPECT_NE(object->NumberValue(), object->NumberValue()); // NaN
  EXPECT_TRUE(object->ToInteger(context).IsEmpty());
  EXPECT_TRUE(object->IntegerValue(context).IsNothing());
  EXPECT_EQ(0, object->ToInt32()->Value());
  EXPECT_EQ(0, object->Int32Value());
  EXPECT_EQ(0, object->ToUint32()->Value());
  EXPECT_EQ(0, object->Uint32Value());
  EXPECT_EQ(0, object->IntegerValue());
  EXPECT_TRUE(object->ToObject()->IsObject());
  EXPECT_TRUE(Object::Cast(*object->ToObject())->Has(foo));

  EXPECT_TRUE(object->Has(foo));
  EXPECT_TRUE(object->Has(context, foo).FromJust());
  EXPECT_TRUE(object->Has(bar));
  EXPECT_TRUE(object->Has(context, bar).FromJust());
  EXPECT_TRUE(object->Has(baz));
  EXPECT_TRUE(object->Has(context, baz).FromJust());
  EXPECT_TRUE(object->Has(1));
  EXPECT_TRUE(object->Has(context, 1).FromJust());
  EXPECT_TRUE(object->Has(0));
  EXPECT_TRUE(object->Has(context, 0).FromJust());

  {
    MaybeLocal<Value> fooVal = object->Get(context, foo);
    EXPECT_FALSE(fooVal.IsEmpty());
    Integer* intVal = Integer::Cast(*fooVal.ToLocalChecked());
    EXPECT_EQ(0, intVal->Value());
  }
  {
    Local<Value> fooVal = object->Get(foo);
    Integer* intVal = Integer::Cast(*fooVal);
    EXPECT_EQ(0, intVal->Value());
  }

  {
    MaybeLocal<Value> barVal = object->Get(context, bar);
    EXPECT_FALSE(barVal.IsEmpty());
    Integer* intVal = Integer::Cast(*barVal.ToLocalChecked());
    EXPECT_EQ(1, intVal->Value());
  }
  {
    Local<Value> barVal = object->Get(bar);
    Integer* intVal = Integer::Cast(*barVal);
    EXPECT_EQ(1, intVal->Value());
  }

  {
    MaybeLocal<Value> bazVal = object->Get(context, baz);
    EXPECT_FALSE(bazVal.IsEmpty());
    String::Utf8Value utf8(bazVal.ToLocalChecked());
    EXPECT_STREQ("two", *utf8);
  }
  {
    Local<Value> bazVal = object->Get(baz);
    String::Utf8Value utf8(bazVal);
    EXPECT_STREQ("two", *utf8);
  }

  {
    MaybeLocal<Value> oneVal = object->Get(context, 1);
    EXPECT_FALSE(oneVal.IsEmpty());
    Integer* intVal = Integer::Cast(*oneVal.ToLocalChecked());
    EXPECT_EQ(0, intVal->Value());
  }
  {
    Local<Value> oneVal = object->Get(1);
    Integer* intVal = Integer::Cast(*oneVal);
    EXPECT_EQ(0, intVal->Value());
  }

  {
    MaybeLocal<Value> zeroVal = object->Get(context, 0);
    EXPECT_FALSE(zeroVal.IsEmpty());
    String::Utf8Value utf8(zeroVal.ToLocalChecked());
    EXPECT_STREQ("two", *utf8);
  }
  {
    Local<Value> zeroVal = object->Get(0);
    String::Utf8Value utf8(zeroVal);
    EXPECT_STREQ("two", *utf8);
  }

  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, foo);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(None, attributes.FromJust());
  }
  {
    PropertyAttribute attributes = object->GetPropertyAttributes(foo);
    EXPECT_EQ(None, attributes);
  }

  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, bar);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(ReadOnly, attributes.FromJust());
  }
  {
    PropertyAttribute attributes = object->GetPropertyAttributes(bar);
    EXPECT_EQ(ReadOnly, attributes);
  }

  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, baz);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(DontEnum | DontDelete, attributes.FromJust());
  }
  {
    PropertyAttribute attributes = object->GetPropertyAttributes(baz);
    EXPECT_EQ(DontEnum | DontDelete, attributes);
  }

  auto CheckPropertyDescriptor = [&](Object* desc, bool readonly, bool enum_, bool config) {
    Local<Value> writableVal = desc->Get(writable);
    Boolean* boolVal = Boolean::Cast(*writableVal);
    EXPECT_EQ(!readonly, boolVal->Value());
    Local<Value> getVal = desc->Get(get);
    EXPECT_TRUE(getVal->IsUndefined());
    Local<Value> setVal = desc->Get(set);
    EXPECT_TRUE(setVal->IsUndefined());
    Local<Value> configurableVal = desc->Get(configurable);
    boolVal = Boolean::Cast(*configurableVal);
    EXPECT_EQ(config, boolVal->Value());
    Local<Value> enumerableVal = desc->Get(enumerable);
    boolVal = Boolean::Cast(*enumerableVal);
    EXPECT_EQ(enum_, boolVal->Value());
  };

  {
    MaybeLocal<Value> maybeDesc =
      object->GetOwnPropertyDescriptor(context, foo);
    EXPECT_FALSE(maybeDesc.IsEmpty());
    Object* desc = Object::Cast(*maybeDesc.ToLocalChecked());
    Local<Value> valueVal = desc->Get(value);
    Integer* intVal = Integer::Cast(*valueVal);
    EXPECT_EQ(0, intVal->Value());
    CheckPropertyDescriptor(desc, false, true, true);
  }
  {
    Local<Value> descVal = object->GetOwnPropertyDescriptor(foo);
    EXPECT_TRUE(*descVal);
    Object* desc = Object::Cast(*descVal);
    Local<Value> valueVal = desc->Get(value);
    Integer* intVal = Integer::Cast(*valueVal);
    EXPECT_EQ(0, intVal->Value());
    CheckPropertyDescriptor(desc, false, true, true);
  }

  {
    MaybeLocal<Value> maybeDesc =
      object->GetOwnPropertyDescriptor(context, bar);
    EXPECT_FALSE(maybeDesc.IsEmpty());
    Object* desc = Object::Cast(*maybeDesc.ToLocalChecked());
    Local<Value> valueVal = desc->Get(value);
    Integer* intVal = Integer::Cast(*valueVal);
    EXPECT_EQ(1, intVal->Value());
    CheckPropertyDescriptor(desc, true, true, true);
  }
  {
    Local<Value> descVal = object->GetOwnPropertyDescriptor(bar);
    EXPECT_TRUE(*descVal);
    Object* desc = Object::Cast(*descVal);
    Local<Value> valueVal = desc->Get(value);
    Integer* intVal = Integer::Cast(*valueVal);
    EXPECT_EQ(1, intVal->Value());
    CheckPropertyDescriptor(desc, true, true, true);
  }

  {
    MaybeLocal<Value> maybeDesc =
      object->GetOwnPropertyDescriptor(context, baz);
    EXPECT_FALSE(maybeDesc.IsEmpty());
    Object* desc = Object::Cast(*maybeDesc.ToLocalChecked());
    Local<Value> valueVal = desc->Get(value);
    String::Utf8Value utf8(valueVal);
    EXPECT_STREQ("two", *utf8);
    CheckPropertyDescriptor(desc, false, false, false);
  }
  {
    Local<Value> descVal = object->GetOwnPropertyDescriptor(baz);
    EXPECT_TRUE(*descVal);
    Object* desc = Object::Cast(*descVal);
    Local<Value> valueVal = desc->Get(value);
    String::Utf8Value utf8(valueVal);
    EXPECT_STREQ("two", *utf8);
    CheckPropertyDescriptor(desc, false, false, false);
  }

  // Test ForceSet by attempting to overwrite a readonly property.
  // Set will succeed without changing the value.
  EXPECT_TRUE(object->Set(context, bar, two).FromJust());
  {
    MaybeLocal<Value> barVal = object->Get(context, bar);
    EXPECT_FALSE(barVal.IsEmpty());
    Integer* intVal = Integer::Cast(*barVal.ToLocalChecked());
    EXPECT_EQ(1, intVal->Value());
  }
  // Now try ForceSet and verify that the value and the PropertyAttribute change.
  EXPECT_TRUE(object->ForceSet(context, bar, two, DontDelete).FromJust());
  {
    MaybeLocal<Value> barVal = object->Get(context, bar);
    EXPECT_FALSE(barVal.IsEmpty());
    String::Utf8Value utf8(barVal.ToLocalChecked());
    EXPECT_STREQ("two", *utf8);
  }
  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, bar);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(DontDelete, attributes.FromJust());
  }
  EXPECT_TRUE(object->ForceSet(bar, two, PropertyAttribute(DontDelete | ReadOnly)));
  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, bar);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(DontDelete | ReadOnly, attributes.FromJust());
  }

  EXPECT_TRUE(object->Delete(foo));
  EXPECT_TRUE(object->Delete(context, foo).FromJust());
  EXPECT_TRUE(object->Delete(context, bar).FromJust());
  EXPECT_TRUE(object->Delete(bar));
  EXPECT_TRUE(object->Delete(context, 1).FromJust());
  EXPECT_TRUE(object->Delete(1));

  EXPECT_FALSE(object->Has(foo));
  EXPECT_FALSE(object->Has(context, foo).FromJust());
  EXPECT_TRUE(object->Has(bar)); // non-configurable property can't be deleted.
  EXPECT_TRUE(object->Has(context, bar).FromJust());
  EXPECT_TRUE(object->Has(baz));
  EXPECT_TRUE(object->Has(context, baz).FromJust());
  EXPECT_FALSE(object->Has(1));
  EXPECT_FALSE(object->Has(context, 1).FromJust());
  EXPECT_TRUE(object->Has(0));
  EXPECT_TRUE(object->Has(context, 0).FromJust());

  Local<Value> protoVal = object->GetPrototype();
  Object* proto = Object::Cast(*protoVal);

  EXPECT_FALSE(object->Has(qux));
  EXPECT_FALSE(proto->Has(qux));
  EXPECT_TRUE(proto->Set(context, qux, one).FromJust());
  EXPECT_TRUE(object->Has(qux));
  {
    MaybeLocal<Value> quxVal = object->Get(context, qux);
    EXPECT_FALSE(quxVal.IsEmpty());
    Integer* intVal = Integer::Cast(*quxVal.ToLocalChecked());
    EXPECT_EQ(1, intVal->Value());
  }
  Local<Object> newProto = Object::New(engine.isolate());
  EXPECT_TRUE(newProto->Set(context, foo, one).FromJust());
  EXPECT_TRUE(object->SetPrototype(context, newProto).FromJust());
  EXPECT_TRUE(object->Has(context, bar).FromJust()); // bar is an own property!
  EXPECT_TRUE(object->Has(context, foo).FromJust());
  {
    MaybeLocal<Value> fooVal = object->Get(context, foo);
    EXPECT_FALSE(fooVal.IsEmpty());
    Integer* intVal = Integer::Cast(*fooVal.ToLocalChecked());
    EXPECT_EQ(1, intVal->Value());
  }

  Local<Object> clone = object->Clone();
  // TODO: The below line should be EXPECT_TRUE once Clone() is fully fixed.
  EXPECT_FALSE(clone->Has(context, bar).FromJust()); // bar is an own property!
  EXPECT_TRUE(clone->Has(context, foo).FromJust());
  Local<Value> cloneProtoVal = clone->GetPrototype();
  Object* cloneProto = Object::Cast(*cloneProtoVal);
  EXPECT_TRUE(cloneProto->Has(qux));
}

void CheckProperties(Isolate* isolate, Local<Value> val,
                     unsigned elmc, const char* elmv[]) {
  Local<Context> context = isolate->GetCurrentContext();
  Object* obj = Object::Cast(*val);
  Local<Array> props = obj->GetPropertyNames(context).ToLocalChecked();
  EXPECT_EQ(elmc, props->Length());
  for (unsigned i = 0; i < elmc; i++) {
    String::Utf8Value elm(
        props->Get(context, Integer::New(isolate, i)).ToLocalChecked());
    EXPECT_STREQ(elmv[i], *elm);
  }
}

void CheckOwnProperties(Isolate* isolate, Local<Value> val,
                        unsigned elmc, const char* elmv[]) {
  Local<Context> context = isolate->GetCurrentContext();
  Object* obj = Object::Cast(*val);
  Local<Array> props =
      obj->GetOwnPropertyNames(context).ToLocalChecked();
  EXPECT_EQ(elmc, props->Length());
  for (unsigned i = 0; i < elmc; i++) {
    String::Utf8Value elm(
        props->Get(context, Integer::New(isolate, i)).ToLocalChecked());
    EXPECT_STREQ(elmv[i], *elm);
  }
}

TEST(SpiderShim, ObjectPropertyEnumeration) {
  // This test is adopted from the V8 PropertyEnumeration test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Isolate* isolate = engine.isolate();
  Local<Value> obj = engine.CompileRun(context,
      "var result = [];"
      "result[0] = {};"
      "result[1] = {a: 1, b: 2};"
      "result[2] = [1, 2, 3];"
      "var proto = {x: 1, y: 2, z: 3};"
      "var x = { __proto__: proto, w: 0, z: 1 };"
      "result[3] = x;"
      "result;");
  Array* elms = Array::Cast(*obj);
  EXPECT_EQ(4, elms->Length());
  int elmc0 = 0;
  const char** elmv0 = NULL;
  CheckProperties(
      isolate,
      elms->Get(context, Integer::New(isolate, 0)).ToLocalChecked(),
      elmc0, elmv0);
  CheckOwnProperties(
      isolate,
      elms->Get(context, Integer::New(isolate, 0)).ToLocalChecked(),
      elmc0, elmv0);
  int elmc1 = 2;
  const char* elmv1[] = {"a", "b"};
  CheckProperties(
      isolate,
      elms->Get(context, Integer::New(isolate, 1)).ToLocalChecked(),
      elmc1, elmv1);
  CheckOwnProperties(
      isolate,
      elms->Get(context, Integer::New(isolate, 1)).ToLocalChecked(),
      elmc1, elmv1);
  int elmc2 = 3;
  const char* elmv2[] = {"0", "1", "2"};
  CheckProperties(
      isolate,
      elms->Get(context, Integer::New(isolate, 2)).ToLocalChecked(),
      elmc2, elmv2);
  CheckOwnProperties(
      isolate,
      elms->Get(context, Integer::New(isolate, 2)).ToLocalChecked(),
      elmc2, elmv2);
  int elmc3 = 4;
  const char* elmv3[] = {"w", "z", "x", "y"};
  CheckProperties(
      isolate,
      elms->Get(context, Integer::New(isolate, 3)).ToLocalChecked(),
      elmc3, elmv3);
  int elmc4 = 2;
  const char* elmv4[] = {"w", "z"};
  CheckOwnProperties(
      isolate,
      elms->Get(context, Integer::New(isolate, 3)).ToLocalChecked(),
      elmc4, elmv4);
}

TEST(SpiderShim, Array) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Array> array = Array::New(engine.isolate(), 10);
  EXPECT_EQ(Array::Cast(*array), *array);
  EXPECT_EQ(10, array->Length());
  for (int i = 0; i < 10; ++i) {
    MaybeLocal<Value> val = array->Get(context, i);
    EXPECT_TRUE(val.ToLocalChecked()->IsUndefined());
    EXPECT_TRUE(array->Set(context, i, Integer::New(engine.isolate(), i * i)).FromJust());
    val = array->Get(context, i);
    EXPECT_EQ(i * i, Integer::Cast(*val.ToLocalChecked())->Value());
  }
  EXPECT_TRUE(array->Set(context, 14, Integer::New(engine.isolate(), 42)).FromJust());
  MaybeLocal<Value> val = array->Get(context, 14);
  EXPECT_EQ(42, Integer::Cast(*val.ToLocalChecked())->Value());
  EXPECT_EQ(15, array->Length());

  Local<String> str = array->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ("0,1,4,9,16,25,36,49,64,81,,,,,42", *utf8);
  EXPECT_EQ(true, array->ToBoolean()->Value());
  EXPECT_EQ(true, array->BooleanValue());
  EXPECT_TRUE(array->ToNumber(context).IsEmpty());
  EXPECT_TRUE(array->NumberValue(context).IsNothing());
  EXPECT_NE(array->NumberValue(), array->NumberValue()); // NaN
  EXPECT_TRUE(array->ToInteger(context).IsEmpty());
  EXPECT_TRUE(array->IntegerValue(context).IsNothing());
  EXPECT_EQ(0, array->ToInt32()->Value());
  EXPECT_EQ(0, array->Int32Value());
  EXPECT_EQ(0, array->ToUint32()->Value());
  EXPECT_EQ(0, array->Uint32Value());
  EXPECT_EQ(0, array->IntegerValue());
  EXPECT_TRUE(array->ToObject()->IsObject());
  EXPECT_EQ(4, Object::Cast(*array->ToObject())->Get(2)->ToInteger()->Value());
}

TEST(SpiderShim, BooleanObject) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Value> boolean = BooleanObject::New(true);
  EXPECT_EQ(BooleanObject::Cast(*boolean), *boolean);
  EXPECT_TRUE(boolean->IsBooleanObject());
  EXPECT_TRUE(BooleanObject::Cast(*boolean)->ValueOf());

  Local<String> str = boolean->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ("true", *utf8);
  EXPECT_EQ(true, boolean->ToBoolean()->Value());
  EXPECT_EQ(true, boolean->BooleanValue());
  EXPECT_EQ(1.0, boolean->ToNumber()->Value());
  EXPECT_EQ(1.0, boolean->NumberValue());
  EXPECT_EQ(1, boolean->ToInteger()->Value());
  EXPECT_EQ(1, boolean->IntegerValue());
  EXPECT_EQ(1, boolean->ToInt32()->Value());
  EXPECT_EQ(1, boolean->Int32Value());
  EXPECT_EQ(1, boolean->ToUint32()->Value());
  EXPECT_EQ(1, boolean->Uint32Value());
  EXPECT_TRUE(boolean->ToObject()->IsBooleanObject());
  EXPECT_EQ(true, BooleanObject::Cast(*boolean->ToObject())->ValueOf());
}

TEST(SpiderShim, NumberObject) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Value> num = NumberObject::New(engine.isolate(), 42);
  EXPECT_EQ(NumberObject::Cast(*num), *num);
  EXPECT_TRUE(num->IsNumberObject());
  EXPECT_EQ(42, NumberObject::Cast(*num)->ValueOf());

  Local<String> str = num->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ("42", *utf8);
  EXPECT_EQ(true, num->ToBoolean()->Value());
  EXPECT_EQ(true, num->BooleanValue());
  EXPECT_DOUBLE_EQ(num->ToNumber()->Value(), 42.0);
  EXPECT_DOUBLE_EQ(num->NumberValue(), 42.0);
  EXPECT_DOUBLE_EQ(num->ToInteger()->Value(), 42);
  EXPECT_DOUBLE_EQ(num->IntegerValue(), 42);
  EXPECT_DOUBLE_EQ(num->ToInt32()->Value(), 42);
  EXPECT_DOUBLE_EQ(num->Int32Value(), 42);
  EXPECT_DOUBLE_EQ(num->ToUint32()->Value(), 42);
  EXPECT_DOUBLE_EQ(num->Uint32Value(), 42);
  EXPECT_TRUE(num->ToObject()->IsNumberObject());
  EXPECT_EQ(42.0, NumberObject::Cast(*num->ToObject())->ValueOf());
}

TEST(SpiderShim, StringObject) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<String> foobar =
    String::NewFromUtf8(engine.isolate(), "foobar", NewStringType::kNormal).
      ToLocalChecked();
  Local<Value> str = StringObject::New(foobar);
  EXPECT_EQ(StringObject::Cast(*str), *str);
  EXPECT_TRUE(str->IsStringObject());
  String::Utf8Value utf8(StringObject::Cast(*str)->ValueOf());
  EXPECT_STREQ("foobar", *utf8);

  Local<String> str_2 = str->ToString();
  String::Utf8Value utf8_2(str_2);
  EXPECT_STREQ("foobar", *utf8_2);
  EXPECT_EQ(true, str->ToBoolean()->Value());
  EXPECT_EQ(true, str->BooleanValue());
  EXPECT_TRUE(str->ToNumber(context).IsEmpty());
  EXPECT_TRUE(str->NumberValue(context).IsNothing());
  EXPECT_NE(str->NumberValue(), str->NumberValue()); // NaN
  EXPECT_TRUE(str->ToInteger(context).IsEmpty());
  EXPECT_TRUE(str->IntegerValue(context).IsNothing());
  EXPECT_EQ(0, str->IntegerValue());
  EXPECT_EQ(0, str->ToInt32()->Value());
  EXPECT_EQ(0, str->Int32Value());
  EXPECT_EQ(0, str->ToUint32()->Value());
  EXPECT_EQ(0, str->Uint32Value());
  EXPECT_TRUE(str->ToObject()->IsStringObject());
  EXPECT_EQ(6, StringObject::Cast(*str->ToObject())->ValueOf()->Length());
}

TEST(SpiderShim, Date) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  const double time = 1224744689038.0;
  MaybeLocal<Value> date = Date::New(context, time);
  EXPECT_FALSE(date.IsEmpty());
  EXPECT_EQ(Date::Cast(*date.ToLocalChecked()), *date.ToLocalChecked());
  EXPECT_TRUE(date.ToLocalChecked()->IsDate());
  EXPECT_EQ(time, Date::Cast(*date.ToLocalChecked())->ValueOf());

  Local<String> str = date.ToLocalChecked()->ToString();
  String::Utf8Value utf8(str);
  const char datePortion[] = "Thu Oct 23 2008 02:51:29 GMT-0400 (EDT)";
  //                              ^      ^          ^     ^
  //                              4     11         21    27
  // Parts of this string are timezone dependent, so only compare the rest!
  struct TimeZoneIndependentOffsets {
    size_t begin, length;
  } offsets[] = {
    {4, 4},
    {11, 5},
    {21, 6}
  };
  for (auto& o : offsets) {
    EXPECT_EQ(0, strncmp(*utf8 + o.begin, datePortion + o.begin, o.length));
  }
  EXPECT_EQ(true, date.ToLocalChecked()->ToBoolean()->Value());
  EXPECT_EQ(true, date.ToLocalChecked()->BooleanValue());
  EXPECT_DOUBLE_EQ(date.ToLocalChecked()->ToNumber()->Value(), time);
  EXPECT_DOUBLE_EQ(date.ToLocalChecked()->NumberValue(), time);
  EXPECT_EQ(time, date.ToLocalChecked()->ToInteger()->Value());
  EXPECT_EQ(time, date.ToLocalChecked()->IntegerValue());
  EXPECT_EQ(uint64_t(time) & 0xffffffff, date.ToLocalChecked()->ToInt32()->Value());
  EXPECT_EQ(uint64_t(time) & 0xffffffff, date.ToLocalChecked()->Int32Value());
  EXPECT_EQ(uint64_t(time) & 0xffffffff, date.ToLocalChecked()->ToUint32()->Value());
  EXPECT_EQ(uint64_t(time) & 0xffffffff, date.ToLocalChecked()->Uint32Value());
  EXPECT_TRUE(date.ToLocalChecked()->ToObject()->IsDate());
  EXPECT_EQ(time, Date::Cast(*date.ToLocalChecked()->ToObject())->ValueOf());
}

TEST(SpiderShim, NativeError) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

#define FOR_EACH_NATIVEERROR(_)                   \
  _(EvalError)                                    \
  _(RangeError)                                   \
  _(ReferenceError)                               \
  _(SyntaxError)                                  \
  _(TypeError)                                    \
  _(URIError)
#define CHECK_ERROR(ERR)                          \
  Local<Value> err_ ## ERR =                      \
    engine.CompileRun(context, "new " #ERR "()"); \
  EXPECT_TRUE(err_ ## ERR->IsNativeError());
FOR_EACH_NATIVEERROR(CHECK_ERROR)
#undef CHECK_ERROR
#undef FOR_EACH_NATIVEERROR

  Local<Value> err_InternalError =
    engine.CompileRun(context, "new InternalError()");
  EXPECT_FALSE(err_InternalError->IsNativeError());
}

namespace {

bool externalStringResourceDestructorCalled = false;

class TestExternalStringResource : public String::ExternalStringResource {
  public:
    TestExternalStringResource(const uint16_t* source, size_t length)
        : data_(source), length_(length) {}
    ~TestExternalStringResource() {
      externalStringResourceDestructorCalled = true;
    }
    const uint16_t* data() const override { return data_; }
    size_t length() const override { return length_; }
  private:
    const uint16_t* data_;
    size_t length_;
};

bool externalOneByteStringResourceDestructorCalled = false;

class TestExternalOneByteStringResource : public String::ExternalOneByteStringResource {
  public:
    TestExternalOneByteStringResource(const char* source, size_t length)
        : data_(source), length_(length) {}
    ~TestExternalOneByteStringResource() {
      externalOneByteStringResourceDestructorCalled = true;
    }
    const char* data() const override { return data_; }
    size_t length() const override { return length_; }
  private:
    const char* data_;
    size_t length_;
};

}  // namespace

TEST(SpiderShim, String) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<String> foobar =
    String::NewFromUtf8(engine.isolate(), "foobar", NewStringType::kNormal).
      ToLocalChecked();
  Local<String> baz =
    String::NewFromUtf8(engine.isolate(), "baz", NewStringType::kNormal).
      ToLocalChecked();
  EXPECT_EQ(6, foobar->Length());
  EXPECT_EQ(6, foobar->Utf8Length());
  EXPECT_EQ(0, String::Empty(engine.isolate())->Length());
  String::Utf8Value utf8(foobar);
  EXPECT_STREQ("foobar", *utf8);
  String::Value twobytes(foobar);
  EXPECT_EQ(0, memcmp(*twobytes, u"foobar", sizeof(u"foobar")));
  Local<String> concat = String::Concat(foobar, baz);
  String::Utf8Value utf8Concat(concat);
  EXPECT_STREQ("foobarbaz", *utf8Concat);
  EXPECT_TRUE(foobar->ToObject()->IsStringObject());
  EXPECT_EQ(6, StringObject::Cast(*foobar->ToObject())->ValueOf()->Length());

  const uint8_t asciiData[] = { 0x4F, 0x68, 0x61, 0x69, 0x00 }; // "Ohai"

  Local<String> asciiStr =
    String::NewFromOneByte(engine.isolate(), asciiData, NewStringType::kNormal).
      ToLocalChecked();
  EXPECT_EQ(4, asciiStr->Length());
  EXPECT_EQ(4, asciiStr->Utf8Length());
  String::Value asciiVal(asciiStr);
  EXPECT_EQ(0, memcmp(*asciiVal, asciiData, sizeof(*asciiData)));

  const uint8_t latin1Data[] = { 0xD3, 0x68, 0xE3, 0xEF, 0x00 }; // "Óhãï"

  Local<String> latin1Str =
    String::NewFromOneByte(engine.isolate(), latin1Data, NewStringType::kNormal).
      ToLocalChecked();
  EXPECT_EQ(4, latin1Str->Length());
  EXPECT_EQ(7, latin1Str->Utf8Length());
  String::Value latin1Val(latin1Str);
  EXPECT_EQ(0, memcmp(*latin1Val, latin1Data, sizeof(*latin1Data)));

  // A five character string (u"ˤdዤ0ぅ", from V8's test-strings.cc) in UTF-16
  // and UTF-8 bytes.
  // UTF-16 -> UTF-8
  // ------    -----
  // U+02E4 -> CB A4
  // U+0064 -> 64
  // U+12E4 -> E1 8B A4
  // U+0030 -> 30
  // U+3045 -> E3 81 85
  const uint16_t utf16Data[] = { 0x02E4, 0x0064, 0x12E4, 0x0030, 0x3045, 0x0000 };
  const unsigned char utf8Data[] = { 0xCB, 0xA4, 0x64, 0xE1, 0x8B, 0xA4, 0x30, 0xE3, 0x81, 0x85, 0x00 };

  Local<String> fromTwoByteStr =
    String::NewFromTwoByte(engine.isolate(), utf16Data, NewStringType::kNormal).
      ToLocalChecked();
  EXPECT_EQ(5, fromTwoByteStr->Length());
  EXPECT_EQ(10, fromTwoByteStr->Utf8Length());
  String::Value fromTwoByteVal(fromTwoByteStr);
  String::Utf8Value fromTwoByteUtf8Val(fromTwoByteStr);
  EXPECT_EQ(0, memcmp(*fromTwoByteVal, utf16Data, sizeof(*utf16Data)));
  EXPECT_EQ(0, memcmp(*fromTwoByteUtf8Val, utf8Data, sizeof(*utf8Data)));

  Local<String> fromUtf8Str =
    String::NewFromUtf8(engine.isolate(), reinterpret_cast<const char*>(utf8Data), NewStringType::kNormal).
      ToLocalChecked();
  EXPECT_EQ(5, fromUtf8Str->Length());
  EXPECT_EQ(10, fromUtf8Str->Utf8Length());
  String::Value fromUtf8Val(fromUtf8Str);
  String::Utf8Value fromUtf8Utf8Val(fromUtf8Str);
  EXPECT_EQ(0, memcmp(*fromUtf8Val, utf16Data, sizeof(*utf16Data)));
  EXPECT_EQ(0, memcmp(*fromUtf8Utf8Val, utf8Data, sizeof(*utf8Data)));

  TestExternalStringResource* testResource =
    new TestExternalStringResource(utf16Data, (sizeof(utf16Data)/sizeof(*utf16Data) - 1));
  Local<String> externalStr = String::NewExternalTwoByte(engine.isolate(), testResource).ToLocalChecked();
  EXPECT_EQ(5, externalStr->Length());
  EXPECT_EQ(10, externalStr->Utf8Length());
  String::Value externalVal(externalStr);
  String::Utf8Value externalUtf8Val(externalStr);
  EXPECT_EQ(0, memcmp(*externalVal, utf16Data, sizeof(*utf16Data)));
  EXPECT_EQ(0, memcmp(*externalUtf8Val, utf8Data, sizeof(*utf8Data)));

  TestExternalOneByteStringResource* testOneByteResource =
    new TestExternalOneByteStringResource(reinterpret_cast<const char*>(latin1Data), sizeof(latin1Data) - 1);
  Local<String> externalOneByteStr = String::NewExternalOneByte(engine.isolate(), testOneByteResource).ToLocalChecked();
  EXPECT_EQ(4, externalOneByteStr->Length());
  EXPECT_EQ(7, externalOneByteStr->Utf8Length());
  String::Value externalOneByteVal(externalOneByteStr);
  EXPECT_EQ(0, memcmp(*externalOneByteVal, latin1Data, sizeof(*latin1Data)));
}

// Other tests of external strings live in the String test function above.
// This just checks that an external string resource's destructor was called
// when the string was finalized.
TEST(SpiderShim, ExternalStringResourceDestructorCalled) {
  EXPECT_TRUE(externalStringResourceDestructorCalled);
  EXPECT_TRUE(externalOneByteStringResourceDestructorCalled);
}

TEST(SpiderShim, StringWrite) {
  // This test is based on V8's StringWrite test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<String> str = v8_str("abcde");
  // abc<Icelandic eth><Unicode snowman>.
  Local<String> str2 = v8_str("abc\303\260\342\230\203");
  Local<String> str3 =
      String::NewFromUtf8(context->GetIsolate(), "abc\0def",
                              NewStringType::kNormal, 7)
          .ToLocalChecked();
  // "ab" + lead surrogate + "cd" + trail surrogate + "ef"
  uint16_t orphans[8] = { 0x61, 0x62, 0xd800, 0x63, 0x64, 0xdc00, 0x65, 0x66 };
  Local<String> orphans_str =
      String::NewFromTwoByte(context->GetIsolate(), orphans,
                                 NewStringType::kNormal, 8)
          .ToLocalChecked();
  // single lead surrogate
  uint16_t lead[1] = { 0xd800 };
  Local<String> lead_str =
      String::NewFromTwoByte(context->GetIsolate(), lead,
                                 NewStringType::kNormal, 1)
          .ToLocalChecked();
  // single trail surrogate
  uint16_t trail[1] = { 0xdc00 };
  Local<String> trail_str =
      String::NewFromTwoByte(context->GetIsolate(), trail,
                                 NewStringType::kNormal, 1)
          .ToLocalChecked();
  // surrogate pair
  uint16_t pair[2] = { 0xd800,  0xdc00 };
  Local<String> pair_str =
      String::NewFromTwoByte(context->GetIsolate(), pair,
                                 NewStringType::kNormal, 2)
          .ToLocalChecked();
  // const int kStride = 4;  // Must match stride in for loops in JS below.
  // CompileRun(
  //     "var left = '';"
  //     "for (var i = 0; i < 0xd800; i += 4) {"
  //     "  left = left + String.fromCharCode(i);"
  //     "}");
  // CompileRun(
  //     "var right = '';"
  //     "for (var i = 0; i < 0xd800; i += 4) {"
  //     "  right = String.fromCharCode(i) + right;"
  //     "}");
  // Local<Object> global = context->Global();
  // Local<String> left_tree = global->Get(context.local(), v8_str("left"))
  //                               .ToLocalChecked()
  //                               .As<String>();
  // Local<String> right_tree = global->Get(context.local(), v8_str("right"))
  //                                .ToLocalChecked()
  //                                .As<String>();

  // CHECK_EQ(5, str2->Length());
  // CHECK_EQ(0xd800 / kStride, left_tree->Length());
  // CHECK_EQ(0xd800 / kStride, right_tree->Length());

  char buf[100];
  char utf8buf[0xd800 * 3];
  uint16_t wbuf[100];
  int len;
  int charlen;

  memset(utf8buf, 0x1, 1000);
  len = str2->WriteUtf8(utf8buf, sizeof(utf8buf), &charlen);
  CHECK_EQ(9, len);
  CHECK_EQ(5, charlen);
  CHECK_EQ(0, strcmp(utf8buf, "abc\303\260\342\230\203"));

  memset(utf8buf, 0x1, 1000);
  len = str2->WriteUtf8(utf8buf, 8, &charlen);
  CHECK_EQ(8, len);
  CHECK_EQ(5, charlen);
  CHECK_EQ(0, strncmp(utf8buf, "abc\303\260\342\230\203\1", 9));

  memset(utf8buf, 0x1, 1000);
  len = str2->WriteUtf8(utf8buf, 7, &charlen);
  CHECK_EQ(5, len);
  CHECK_EQ(4, charlen);
  CHECK_EQ(0, strncmp(utf8buf, "abc\303\260\1", 5));

  memset(utf8buf, 0x1, 1000);
  len = str2->WriteUtf8(utf8buf, 6, &charlen);
  CHECK_EQ(5, len);
  CHECK_EQ(4, charlen);
  CHECK_EQ(0, strncmp(utf8buf, "abc\303\260\1", 5));

  memset(utf8buf, 0x1, 1000);
  len = str2->WriteUtf8(utf8buf, 5, &charlen);
  CHECK_EQ(5, len);
  CHECK_EQ(4, charlen);
  CHECK_EQ(0, strncmp(utf8buf, "abc\303\260\1", 5));

  memset(utf8buf, 0x1, 1000);
  len = str2->WriteUtf8(utf8buf, 4, &charlen);
  CHECK_EQ(3, len);
  CHECK_EQ(3, charlen);
  CHECK_EQ(0, strncmp(utf8buf, "abc\1", 4));

  memset(utf8buf, 0x1, 1000);
  len = str2->WriteUtf8(utf8buf, 3, &charlen);
  CHECK_EQ(3, len);
  CHECK_EQ(3, charlen);
  CHECK_EQ(0, strncmp(utf8buf, "abc\1", 4));

  memset(utf8buf, 0x1, 1000);
  len = str2->WriteUtf8(utf8buf, 2, &charlen);
  CHECK_EQ(2, len);
  CHECK_EQ(2, charlen);
  CHECK_EQ(0, strncmp(utf8buf, "ab\1", 3));

  // allow orphan surrogates by default
  memset(utf8buf, 0x1, 1000);
  len = orphans_str->WriteUtf8(utf8buf, sizeof(utf8buf), &charlen);
  CHECK_EQ(13, len);
  CHECK_EQ(8, charlen);
  CHECK_EQ(0, strcmp(utf8buf, "ab\355\240\200cd\355\260\200ef"));

  // replace orphan surrogates with unicode replacement character
  memset(utf8buf, 0x1, 1000);
  len = orphans_str->WriteUtf8(utf8buf,
                               sizeof(utf8buf),
                               &charlen,
                               String::REPLACE_INVALID_UTF8);
  CHECK_EQ(13, len);
  CHECK_EQ(8, charlen);
  CHECK_EQ(0, strcmp(utf8buf, "ab\357\277\275cd\357\277\275ef"));

  // replace single lead surrogate with unicode replacement character
  memset(utf8buf, 0x1, 1000);
  len = lead_str->WriteUtf8(utf8buf,
                            sizeof(utf8buf),
                            &charlen,
                            String::REPLACE_INVALID_UTF8);
  CHECK_EQ(4, len);
  CHECK_EQ(1, charlen);
  CHECK_EQ(0, strcmp(utf8buf, "\357\277\275"));

  // replace single trail surrogate with unicode replacement character
  memset(utf8buf, 0x1, 1000);
  len = trail_str->WriteUtf8(utf8buf,
                             sizeof(utf8buf),
                             &charlen,
                             String::REPLACE_INVALID_UTF8);
  CHECK_EQ(4, len);
  CHECK_EQ(1, charlen);
  CHECK_EQ(0, strcmp(utf8buf, "\357\277\275"));

  // do not replace / write anything if surrogate pair does not fit the buffer
  // space
  memset(utf8buf, 0x1, 1000);
  len = pair_str->WriteUtf8(utf8buf,
                             3,
                             &charlen,
                             String::REPLACE_INVALID_UTF8);
  CHECK_EQ(0, len);
  CHECK_EQ(0, charlen);

  // memset(utf8buf, 0x1, sizeof(utf8buf));
  // len = GetUtf8Length(left_tree);
  // int utf8_expected =
  //     (0x80 + (0x800 - 0x80) * 2 + (0xd800 - 0x800) * 3) / kStride;
  // CHECK_EQ(utf8_expected, len);
  // len = left_tree->WriteUtf8(utf8buf, utf8_expected, &charlen);
  // CHECK_EQ(utf8_expected, len);
  // CHECK_EQ(0xd800 / kStride, charlen);
  // CHECK_EQ(0xed, static_cast<unsigned char>(utf8buf[utf8_expected - 3]));
  // CHECK_EQ(0x9f, static_cast<unsigned char>(utf8buf[utf8_expected - 2]));
  // CHECK_EQ(0xc0 - kStride,
  //          static_cast<unsigned char>(utf8buf[utf8_expected - 1]));
  // CHECK_EQ(1, utf8buf[utf8_expected]);

  // memset(utf8buf, 0x1, sizeof(utf8buf));
  // len = GetUtf8Length(right_tree);
  // CHECK_EQ(utf8_expected, len);
  // len = right_tree->WriteUtf8(utf8buf, utf8_expected, &charlen);
  // CHECK_EQ(utf8_expected, len);
  // CHECK_EQ(0xd800 / kStride, charlen);
  // CHECK_EQ(0xed, static_cast<unsigned char>(utf8buf[0]));
  // CHECK_EQ(0x9f, static_cast<unsigned char>(utf8buf[1]));
  // CHECK_EQ(0xc0 - kStride, static_cast<unsigned char>(utf8buf[2]));
  // CHECK_EQ(1, utf8buf[utf8_expected]);

  memset(buf, 0x1, sizeof(buf));
  memset(wbuf, 0x1, sizeof(wbuf));
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf));
  CHECK_EQ(5, len);
  len = str->Write(wbuf);
  CHECK_EQ(5, len);
  CHECK_EQ(0, strcmp("abcde", buf));
  uint16_t answer1[] = {'a', 'b', 'c', 'd', 'e', '\0'};
  CHECK_EQ(0, StrCmp16(answer1, wbuf));

  memset(buf, 0x1, sizeof(buf));
  memset(wbuf, 0x1, sizeof(wbuf));
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf), 0, 4);
  CHECK_EQ(4, len);
  len = str->Write(wbuf, 0, 4);
  CHECK_EQ(4, len);
  CHECK_EQ(0, strncmp("abcd\1", buf, 5));
  uint16_t answer2[] = {'a', 'b', 'c', 'd', 0x101};
  CHECK_EQ(0, StrNCmp16(answer2, wbuf, 5));

  memset(buf, 0x1, sizeof(buf));
  memset(wbuf, 0x1, sizeof(wbuf));
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf), 0, 5);
  CHECK_EQ(5, len);
  len = str->Write(wbuf, 0, 5);
  CHECK_EQ(5, len);
  CHECK_EQ(0, strncmp("abcde\1", buf, 6));
  uint16_t answer3[] = {'a', 'b', 'c', 'd', 'e', 0x101};
  CHECK_EQ(0, StrNCmp16(answer3, wbuf, 6));

  memset(buf, 0x1, sizeof(buf));
  memset(wbuf, 0x1, sizeof(wbuf));
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf), 0, 6);
  CHECK_EQ(5, len);
  len = str->Write(wbuf, 0, 6);
  CHECK_EQ(5, len);
  CHECK_EQ(0, strcmp("abcde", buf));
  uint16_t answer4[] = {'a', 'b', 'c', 'd', 'e', '\0'};
  CHECK_EQ(0, StrCmp16(answer4, wbuf));

  memset(buf, 0x1, sizeof(buf));
  memset(wbuf, 0x1, sizeof(wbuf));
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf), 4, -1);
  CHECK_EQ(1, len);
  len = str->Write(wbuf, 4, -1);
  CHECK_EQ(1, len);
  CHECK_EQ(0, strcmp("e", buf));
  uint16_t answer5[] = {'e', '\0'};
  CHECK_EQ(0, StrCmp16(answer5, wbuf));

  memset(buf, 0x1, sizeof(buf));
  memset(wbuf, 0x1, sizeof(wbuf));
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf), 4, 6);
  CHECK_EQ(1, len);
  len = str->Write(wbuf, 4, 6);
  CHECK_EQ(1, len);
  CHECK_EQ(0, strcmp("e", buf));
  CHECK_EQ(0, StrCmp16(answer5, wbuf));

  memset(buf, 0x1, sizeof(buf));
  memset(wbuf, 0x1, sizeof(wbuf));
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf), 4, 1);
  CHECK_EQ(1, len);
  len = str->Write(wbuf, 4, 1);
  CHECK_EQ(1, len);
  CHECK_EQ(0, strncmp("e\1", buf, 2));
  uint16_t answer6[] = {'e', 0x101};
  CHECK_EQ(0, StrNCmp16(answer6, wbuf, 2));

  memset(buf, 0x1, sizeof(buf));
  memset(wbuf, 0x1, sizeof(wbuf));
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf), 3, 1);
  CHECK_EQ(1, len);
  len = str->Write(wbuf, 3, 1);
  CHECK_EQ(1, len);
  CHECK_EQ(0, strncmp("d\1", buf, 2));
  uint16_t answer7[] = {'d', 0x101};
  CHECK_EQ(0, StrNCmp16(answer7, wbuf, 2));

  memset(wbuf, 0x1, sizeof(wbuf));
  wbuf[5] = 'X';
  len = str->Write(wbuf, 0, 6, String::NO_NULL_TERMINATION);
  CHECK_EQ(5, len);
  CHECK_EQ('X', wbuf[5]);
  uint16_t answer8a[] = {'a', 'b', 'c', 'd', 'e'};
  uint16_t answer8b[] = {'a', 'b', 'c', 'd', 'e', '\0'};
  CHECK_EQ(0, StrNCmp16(answer8a, wbuf, 5));
  CHECK_NE(0, StrCmp16(answer8b, wbuf));
  wbuf[5] = '\0';
  CHECK_EQ(0, StrCmp16(answer8b, wbuf));

  memset(buf, 0x1, sizeof(buf));
  buf[5] = 'X';
  len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf),
                          0,
                          6,
                          String::NO_NULL_TERMINATION);
  CHECK_EQ(5, len);
  CHECK_EQ('X', buf[5]);
  CHECK_EQ(0, strncmp("abcde", buf, 5));
  CHECK_NE(0, strcmp("abcde", buf));
  buf[5] = '\0';
  CHECK_EQ(0, strcmp("abcde", buf));

  memset(utf8buf, 0x1, sizeof(utf8buf));
  utf8buf[8] = 'X';
  len = str2->WriteUtf8(utf8buf, sizeof(utf8buf), &charlen,
                        String::NO_NULL_TERMINATION);
  CHECK_EQ(8, len);
  CHECK_EQ('X', utf8buf[8]);
  CHECK_EQ(5, charlen);
  CHECK_EQ(0, strncmp(utf8buf, "abc\303\260\342\230\203", 8));
  CHECK_NE(0, strcmp(utf8buf, "abc\303\260\342\230\203"));
  utf8buf[8] = '\0';
  CHECK_EQ(0, strcmp(utf8buf, "abc\303\260\342\230\203"));

  memset(utf8buf, 0x1, sizeof(utf8buf));
  utf8buf[5] = 'X';
  len = str->WriteUtf8(utf8buf, sizeof(utf8buf), &charlen,
                        String::NO_NULL_TERMINATION);
  CHECK_EQ(5, len);
  CHECK_EQ('X', utf8buf[5]);  // Test that the sixth character is untouched.
  CHECK_EQ(5, charlen);
  utf8buf[5] = '\0';
  CHECK_EQ(0, strcmp(utf8buf, "abcde"));

  memset(buf, 0x1, sizeof(buf));
  len = str3->WriteOneByte(reinterpret_cast<uint8_t*>(buf));
  CHECK_EQ(7, len);
  CHECK_EQ(0, strcmp("abc", buf));
  CHECK_EQ(0, buf[3]);
  CHECK_EQ(0, strcmp("def", buf + 4));

  CHECK_EQ(0, str->WriteOneByte(NULL, 0, 0, String::NO_NULL_TERMINATION));
  CHECK_EQ(0, str->WriteUtf8(NULL, 0, 0, String::NO_NULL_TERMINATION));
  CHECK_EQ(0, str->Write(NULL, 0, 0, String::NO_NULL_TERMINATION));
}

TEST(SpiderShim, ToObject) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  // Null and undefined can't be converted into an Object
  EXPECT_TRUE(Null(engine.isolate())->ToObject().IsEmpty());
  EXPECT_TRUE(Undefined(engine.isolate())->ToObject().IsEmpty());
}

TEST(SpiderShim, Equals) {
  // This test is adopted from the V8 Equality test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  EXPECT_TRUE(v8_str("a")->Equals(context, v8_str("a")).FromJust());
  EXPECT_TRUE(!v8_str("a")->Equals(context, v8_str("b")).FromJust());

  EXPECT_TRUE(v8_str("a")->Equals(context, v8_str("a")).FromJust());
  EXPECT_TRUE(!v8_str("a")->Equals(context, v8_str("b")).FromJust());
  EXPECT_TRUE(v8_num(1)->Equals(context, v8_num(1)).FromJust());
  EXPECT_TRUE(v8_num(1.00)->Equals(context, v8_num(1)).FromJust());
  EXPECT_TRUE(!v8_num(1)->Equals(context, v8_num(2)).FromJust());

  // Assume String is not internalized.
  EXPECT_TRUE(v8_str("a")->StrictEquals(v8_str("a")));
  EXPECT_TRUE(!v8_str("a")->StrictEquals(v8_str("b")));
  EXPECT_TRUE(!v8_str("5")->StrictEquals(v8_num(5)));
  EXPECT_TRUE(v8_num(1)->StrictEquals(v8_num(1)));
  EXPECT_TRUE(!v8_num(1)->StrictEquals(v8_num(2)));
  EXPECT_TRUE(v8_num(0.0)->StrictEquals(v8_num(-0.0)));
  Local<Value> not_a_number = v8_num(std::numeric_limits<double>::quiet_NaN());
  EXPECT_TRUE(!not_a_number->StrictEquals(not_a_number));
  EXPECT_TRUE(False(isolate)->StrictEquals(False(isolate)));
  EXPECT_TRUE(!False(isolate)->StrictEquals(Undefined(isolate)));

#if 0
  // TODO: Enable this test once we support Persistent.
  Local<Object> obj = Object::New(isolate);
  Persistent<Object> alias(isolate, obj);
  EXPECT_TRUE(Local<Object>::New(isolate, alias)->StrictEquals(obj));
  alias.Reset();
#endif

  EXPECT_TRUE(v8_str("a")->SameValue(v8_str("a")));
  EXPECT_TRUE(!v8_str("a")->SameValue(v8_str("b")));
  EXPECT_TRUE(!v8_str("5")->SameValue(v8_num(5)));
  EXPECT_TRUE(v8_num(1)->SameValue(v8_num(1)));
  EXPECT_TRUE(!v8_num(1)->SameValue(v8_num(2)));
  EXPECT_TRUE(!v8_num(0.0)->SameValue(v8_num(-0.0)));
  EXPECT_TRUE(not_a_number->SameValue(not_a_number));
  EXPECT_TRUE(False(isolate)->SameValue(False(isolate)));
  EXPECT_TRUE(!False(isolate)->SameValue(Undefined(isolate)));
}

TEST(SpiderShim, ArrayBuffer) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ArrayBuffer> arr = ArrayBuffer::New(isolate, 0);
  EXPECT_TRUE(arr->IsArrayBuffer());
  EXPECT_EQ(0, arr->ByteLength());
  ArrayBuffer::Contents contents = arr->GetContents();
  EXPECT_EQ(0, contents.ByteLength());
  Local<ArrayBuffer> arr2 = ArrayBuffer::New(isolate, 2);
  EXPECT_TRUE(arr2->IsArrayBuffer());
  EXPECT_EQ(2, arr2->ByteLength());
  contents = arr2->GetContents();
  EXPECT_EQ(2, contents.ByteLength());
  EXPECT_EQ(2, ArrayBuffer::Cast(*arr2->ToObject())->ByteLength());
}

TEST(SpiderShim, Function) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  engine.CompileRun(context,
      "function normal() { return 1; }"
      "function* gen() { yield 1; }");
  Local<Value> normal = engine.CompileRun(context, "normal");
  Local<Value> integer = engine.CompileRun(context, "normal()");
  Local<Value> object = engine.CompileRun(context, "{a:42}");
  Local<Value> gen = engine.CompileRun(context, "gen");
  Local<Value> genObject = engine.CompileRun(context, "gen()");

  EXPECT_TRUE(normal->IsFunction());
  EXPECT_FALSE(integer->IsFunction());
  EXPECT_FALSE(object->IsFunction());
  EXPECT_TRUE(gen->IsFunction());
  EXPECT_FALSE(genObject->IsFunction());
}

template <typename TypedArray, int kElementSize>
static Local<TypedArray> CreateAndCheck(Local<ArrayBuffer> ab,
                                        int byteOffset, int length) {
  Local<TypedArray> ta = TypedArray::New(ab, byteOffset, length);
  EXPECT_EQ(byteOffset, static_cast<int>(ta->ByteOffset()));
  EXPECT_EQ(length * kElementSize, static_cast<int>(ta->ByteLength()));
  EXPECT_EQ(ab->GetContents().Data(), ta->Buffer()->GetContents().Data());
  EXPECT_EQ(ta->ByteOffset(), TypedArray::Cast(*ta->ToObject())->ByteOffset());
  return ta;
}

TEST(SpiderShim, ArrayBuffer_NeuteringApi) {
  // This test is adopted from the V8 ArrayBuffer_NeuteringApi test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, 1024);

  Local<Uint8Array> u8a =
      CreateAndCheck<Uint8Array, 1>(buffer, 1, 1023);
  Local<Uint8ClampedArray> u8c =
      CreateAndCheck<Uint8ClampedArray, 1>(buffer, 1, 1023);
  Local<Int8Array> i8a =
      CreateAndCheck<Int8Array, 1>(buffer, 1, 1023);

  Local<Uint16Array> u16a =
      CreateAndCheck<Uint16Array, 2>(buffer, 2, 511);
  Local<Int16Array> i16a =
      CreateAndCheck<Int16Array, 2>(buffer, 2, 511);

  Local<Uint32Array> u32a =
      CreateAndCheck<Uint32Array, 4>(buffer, 4, 255);
  Local<Int32Array> i32a =
      CreateAndCheck<Int32Array, 4>(buffer, 4, 255);

  Local<Float32Array> f32a =
      CreateAndCheck<Float32Array, 4>(buffer, 4, 255);
  Local<Float64Array> f64a =
      CreateAndCheck<Float64Array, 8>(buffer, 8, 127);
}

TEST(SpiderShim, FunctionCall) {
  // This test is adopted from the V8 FunctionCall test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  engine.CompileRun(context,
      "function Foo() {"
      "  var result = [];"
      "  for (var i = 0; i < arguments.length; i++) {"
      "    result.push(arguments[i]);"
      "  }"
      "  return result;"
      "}"
      "function ReturnThisSloppy() {"
      "  return this;"
      "}"
      "function ReturnThisStrict() {"
      "  'use strict';"
      "  return this;"
      "}");
  Local<Function> Foo = Local<Function>::Cast(
      context->Global()->Get(context, v8_str("Foo")).ToLocalChecked());
  Local<Function> ReturnThisSloppy = Local<Function>::Cast(
      context->Global()
          ->Get(context, v8_str("ReturnThisSloppy"))
          .ToLocalChecked());
  Local<Function> ReturnThisStrict = Local<Function>::Cast(
      context->Global()
          ->Get(context, v8_str("ReturnThisStrict"))
          .ToLocalChecked());

  Local<Value>* args0 = NULL;
  Local<Array> a0 = Local<Array>::Cast(
      Foo->Call(context, Foo, 0, args0).ToLocalChecked());
  EXPECT_EQ(0u, a0->Length());

  Local<Value> args1[] = {v8_num(1.1)};
  Local<Array> a1 = Local<Array>::Cast(
      Foo->Call(context, Foo, 1, args1).ToLocalChecked());
  EXPECT_EQ(1u, a1->Length());
  EXPECT_EQ(1.1, a1->Get(context, Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  Local<Value> args2[] = {v8_num(2.2), v8_num(3.3)};
  Local<Array> a2 = Local<Array>::Cast(
      Foo->Call(context, Foo, 2, args2).ToLocalChecked());
  EXPECT_EQ(2u, a2->Length());
  EXPECT_EQ(2.2, a2->Get(context, Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  EXPECT_EQ(3.3, a2->Get(context, Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  Local<Value> args3[] = {v8_num(4.4), v8_num(5.5), v8_num(6.6)};
  Local<Array> a3 = Local<Array>::Cast(
      Foo->Call(context, Foo, 3, args3).ToLocalChecked());
  EXPECT_EQ(3u, a3->Length());
  EXPECT_EQ(4.4, a3->Get(context, Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  EXPECT_EQ(5.5, a3->Get(context, Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  EXPECT_EQ(6.6, a3->Get(context, Integer::New(isolate, 2))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  Local<Value> args4[] = {v8_num(7.7), v8_num(8.8), v8_num(9.9),
                              v8_num(10.11)};
  Local<Array> a4 = Local<Array>::Cast(
      Foo->Call(context, Foo, 4, args4).ToLocalChecked());
  EXPECT_EQ(4u, a4->Length());
  EXPECT_EQ(7.7, a4->Get(context, Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  EXPECT_EQ(8.8, a4->Get(context, Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  EXPECT_EQ(9.9, a4->Get(context, Integer::New(isolate, 2))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  EXPECT_EQ(10.11, a4->Get(context, Integer::New(isolate, 3))
                      .ToLocalChecked()
                      ->NumberValue(context)
                      .FromJust());

  Local<Value> r1 =
      ReturnThisSloppy->Call(context, Undefined(isolate), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r1->StrictEquals(context->Global()));
  Local<Value> r2 =
      ReturnThisSloppy->Call(context, Null(isolate), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r2->StrictEquals(context->Global()));
  Local<Value> r3 =
      ReturnThisSloppy->Call(context, v8_num(42), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r3->IsNumberObject());
  EXPECT_EQ(42.0, r3.As<NumberObject>()->ValueOf());
  Local<Value> r4 =
      ReturnThisSloppy->Call(context, v8_str("hello"), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r4->IsStringObject());
  EXPECT_TRUE(r4.As<StringObject>()->ValueOf()->StrictEquals(v8_str("hello")));
  Local<Value> r5 =
      ReturnThisSloppy->Call(context, True(isolate), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r5->IsBooleanObject());
  EXPECT_TRUE(r5.As<BooleanObject>()->ValueOf());

  Local<Value> r6 =
      ReturnThisStrict->Call(context, Undefined(isolate), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r6->IsUndefined());
  Local<Value> r7 =
      ReturnThisStrict->Call(context, Null(isolate), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r7->IsNull());
  Local<Value> r8 =
      ReturnThisStrict->Call(context, v8_num(42), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r8->StrictEquals(v8_num(42)));
  Local<Value> r9 =
      ReturnThisStrict->Call(context, v8_str("hello"), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r9->StrictEquals(v8_str("hello")));
  Local<Value> r10 =
      ReturnThisStrict->Call(context, True(isolate), 0, NULL)
          .ToLocalChecked();
  EXPECT_TRUE(r10->StrictEquals(True(isolate)));
}

TEST(SpiderShim, ConstructCall) {
  // This test is adopted from the V8 ConstructCall test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  engine.CompileRun(context,
      "function Foo() {"
      "  var result = [];"
      "  for (var i = 0; i < arguments.length; i++) {"
      "    result.push(arguments[i]);"
      "  }"
      "  return result;"
      "}");
  Local<Function> Foo = Local<Function>::Cast(
      context->Global()->Get(context, v8_str("Foo")).ToLocalChecked());

  v8::Local<Value>* args0 = NULL;
  Local<v8::Array> a0 = Local<v8::Array>::Cast(
      Foo->NewInstance(context, 0, args0).ToLocalChecked());
  CHECK_EQ(0u, a0->Length());

  v8::Local<Value> args1[] = {v8_num(1.1)};
  Local<v8::Array> a1 = Local<v8::Array>::Cast(
      Foo->NewInstance(context, 1, args1).ToLocalChecked());
  CHECK_EQ(1u, a1->Length());
  CHECK_EQ(1.1, a1->Get(context, v8::Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  v8::Local<Value> args2[] = {v8_num(2.2), v8_num(3.3)};
  Local<v8::Array> a2 = Local<v8::Array>::Cast(
      Foo->NewInstance(context, 2, args2).ToLocalChecked());
  CHECK_EQ(2u, a2->Length());
  CHECK_EQ(2.2, a2->Get(context, v8::Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(3.3, a2->Get(context, v8::Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  v8::Local<Value> args3[] = {v8_num(4.4), v8_num(5.5), v8_num(6.6)};
  Local<v8::Array> a3 = Local<v8::Array>::Cast(
      Foo->NewInstance(context, 3, args3).ToLocalChecked());
  CHECK_EQ(3u, a3->Length());
  CHECK_EQ(4.4, a3->Get(context, v8::Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(5.5, a3->Get(context, v8::Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(6.6, a3->Get(context, v8::Integer::New(isolate, 2))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  v8::Local<Value> args4[] = {v8_num(7.7), v8_num(8.8), v8_num(9.9),
                              v8_num(10.11)};
  Local<v8::Array> a4 = Local<v8::Array>::Cast(
      Foo->NewInstance(context, 4, args4).ToLocalChecked());
  CHECK_EQ(4u, a4->Length());
  CHECK_EQ(7.7, a4->Get(context, v8::Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(8.8, a4->Get(context, v8::Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(9.9, a4->Get(context, v8::Integer::New(isolate, 2))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(10.11, a4->Get(context, v8::Integer::New(isolate, 3))
                      .ToLocalChecked()
                      ->NumberValue(context)
                      .FromJust());
}
