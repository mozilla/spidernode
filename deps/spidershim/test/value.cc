// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

void TestBoolean(Isolate* isolate, bool value) {
  Local<Boolean> boolean = Boolean::New(isolate, value);
  EXPECT_TRUE(boolean->IsBoolean());
  EXPECT_FALSE(boolean->IsNumber());
  EXPECT_FALSE(boolean->IsBooleanObject());
  EXPECT_EQ(boolean->IsTrue(), value);
  EXPECT_EQ(boolean->IsFalse(), !value);
  EXPECT_EQ(boolean->Value(), value);
  String::Utf8Value utf8(boolean->ToString());
  EXPECT_STREQ(*utf8, value ? "true" : "false");
  EXPECT_EQ(boolean->ToBoolean()->Value(), value);
  EXPECT_EQ(boolean->BooleanValue(), value);
  EXPECT_EQ(boolean->ToNumber()->Value(), value ? 1.0 : 0.0);
  EXPECT_EQ(boolean->NumberValue(), value ? 1.0 : 0.0);
  EXPECT_EQ(boolean->ToInteger()->Value(), value ? 1 : 0);
  EXPECT_EQ(boolean->IntegerValue(), value ? 1 : 0);
  EXPECT_TRUE(boolean->ToObject()->IsBooleanObject());
  EXPECT_EQ(BooleanObject::Cast(*boolean->ToObject())->ValueOf(), value);
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
  EXPECT_EQ(number->Value(), double(value));
  String::Utf8Value utf8(number->ToString());
  EXPECT_STREQ(*utf8, strValue);
  EXPECT_EQ(number->ToBoolean()->Value(), !!value);
  EXPECT_EQ(number->BooleanValue(), !!value);
  EXPECT_EQ(number->ToNumber()->Value(), value);
  EXPECT_EQ(number->NumberValue(), value);
  EXPECT_EQ(number->ToInteger()->Value(), value < 0 ? ceil(value) : floor(value));
  EXPECT_EQ(number->IntegerValue(), value < 0 ? ceil(value) : floor(value));
  EXPECT_TRUE(number->ToObject()->IsNumberObject());
  EXPECT_EQ(NumberObject::Cast(*number->ToObject())->ValueOf(), value);
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
  EXPECT_EQ(integer->Value(), int64_t(value));
  typedef typename IntegerMaker<T>::IntType IntType;
  IntType* intVal = IntType::Cast(*integer);
  EXPECT_EQ(intVal->Value(), value);
  String::Utf8Value utf8(intVal->ToString());
  char strValue[1024];
  sprintf(strValue, IntegerMaker<T>::formatString, value);
  EXPECT_STREQ(*utf8, strValue);
  EXPECT_EQ(intVal->ToBoolean()->Value(), !!value);
  EXPECT_EQ(intVal->BooleanValue(), !!value);
  EXPECT_EQ(intVal->ToNumber()->Value(), value);
  EXPECT_EQ(intVal->NumberValue(), value);
  EXPECT_EQ(intVal->ToInteger()->Value(), value);
  EXPECT_EQ(intVal->IntegerValue(), value);
  EXPECT_TRUE(intVal->ToObject()->IsNumberObject());
  EXPECT_EQ(NumberObject::Cast(*intVal->ToObject())->ValueOf(), value);
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

  EXPECT_EQ(object->ToBoolean()->Value(), true);
  EXPECT_EQ(object->BooleanValue(), true);

  EXPECT_TRUE(object->Set(context, foo, zero).FromJust());
  EXPECT_TRUE(object->DefineOwnProperty(context, bar, one, ReadOnly).FromJust());
  EXPECT_TRUE(object->DefineOwnProperty(context, baz, two, PropertyAttribute(DontEnum | DontDelete)).FromJust());
  EXPECT_TRUE(object->Set(context, 1, zero).FromJust());
  EXPECT_TRUE(object->Set(context, 0, two).FromJust());

  Local<String> str = object->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ(*utf8, "[object Object]");
  EXPECT_EQ(object->ToBoolean()->Value(), true);
  EXPECT_EQ(object->BooleanValue(), true);
  EXPECT_TRUE(object->ToNumber(context).IsEmpty());
  EXPECT_TRUE(object->NumberValue(context).IsNothing());
  EXPECT_NE(object->NumberValue(), object->NumberValue()); // NaN
  EXPECT_TRUE(object->ToInteger(context).IsEmpty());
  EXPECT_TRUE(object->IntegerValue(context).IsNothing());
  EXPECT_EQ(object->IntegerValue(), 0);
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
    EXPECT_EQ(intVal->Value(), 0);
  }
  {
    Local<Value> fooVal = object->Get(foo);
    Integer* intVal = Integer::Cast(*fooVal);
    EXPECT_EQ(intVal->Value(), 0);
  }

  {
    MaybeLocal<Value> barVal = object->Get(context, bar);
    EXPECT_FALSE(barVal.IsEmpty());
    Integer* intVal = Integer::Cast(*barVal.ToLocalChecked());
    EXPECT_EQ(intVal->Value(), 1);
  }
  {
    Local<Value> barVal = object->Get(bar);
    Integer* intVal = Integer::Cast(*barVal);
    EXPECT_EQ(intVal->Value(), 1);
  }

  {
    MaybeLocal<Value> bazVal = object->Get(context, baz);
    EXPECT_FALSE(bazVal.IsEmpty());
    String::Utf8Value utf8(bazVal.ToLocalChecked());
    EXPECT_STREQ(*utf8, "two");
  }
  {
    Local<Value> bazVal = object->Get(baz);
    String::Utf8Value utf8(bazVal);
    EXPECT_STREQ(*utf8, "two");
  }

  {
    MaybeLocal<Value> oneVal = object->Get(context, 1);
    EXPECT_FALSE(oneVal.IsEmpty());
    Integer* intVal = Integer::Cast(*oneVal.ToLocalChecked());
    EXPECT_EQ(intVal->Value(), 0);
  }
  {
    Local<Value> oneVal = object->Get(1);
    Integer* intVal = Integer::Cast(*oneVal);
    EXPECT_EQ(intVal->Value(), 0);
  }

  {
    MaybeLocal<Value> zeroVal = object->Get(context, 0);
    EXPECT_FALSE(zeroVal.IsEmpty());
    String::Utf8Value utf8(zeroVal.ToLocalChecked());
    EXPECT_STREQ(*utf8, "two");
  }
  {
    Local<Value> zeroVal = object->Get(0);
    String::Utf8Value utf8(zeroVal);
    EXPECT_STREQ(*utf8, "two");
  }

  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, foo);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(attributes.FromJust(), None);
  }
  {
    PropertyAttribute attributes = object->GetPropertyAttributes(foo);
    EXPECT_EQ(attributes, None);
  }

  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, bar);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(attributes.FromJust(), ReadOnly);
  }
  {
    PropertyAttribute attributes = object->GetPropertyAttributes(bar);
    EXPECT_EQ(attributes, ReadOnly);
  }

  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, baz);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(attributes.FromJust(), DontEnum | DontDelete);
  }
  {
    PropertyAttribute attributes = object->GetPropertyAttributes(baz);
    EXPECT_EQ(attributes, DontEnum | DontDelete);
  }

  auto CheckPropertyDescriptor = [&](Object* desc, bool readonly, bool enum_, bool config) {
    Local<Value> writableVal = desc->Get(writable);
    Boolean* boolVal = Boolean::Cast(*writableVal);
    EXPECT_EQ(boolVal->Value(), !readonly);
    Local<Value> getVal = desc->Get(get);
    EXPECT_TRUE(getVal->IsUndefined());
    Local<Value> setVal = desc->Get(set);
    EXPECT_TRUE(setVal->IsUndefined());
    Local<Value> configurableVal = desc->Get(configurable);
    boolVal = Boolean::Cast(*configurableVal);
    EXPECT_EQ(boolVal->Value(), config);
    Local<Value> enumerableVal = desc->Get(enumerable);
    boolVal = Boolean::Cast(*enumerableVal);
    EXPECT_EQ(boolVal->Value(), enum_);
  };

  {
    MaybeLocal<Value> maybeDesc =
      object->GetOwnPropertyDescriptor(context, foo);
    EXPECT_FALSE(maybeDesc.IsEmpty());
    Object* desc = Object::Cast(*maybeDesc.ToLocalChecked());
    Local<Value> valueVal = desc->Get(value);
    Integer* intVal = Integer::Cast(*valueVal);
    EXPECT_EQ(intVal->Value(), 0);
    CheckPropertyDescriptor(desc, false, true, true);
  }
  {
    Local<Value> descVal = object->GetOwnPropertyDescriptor(foo);
    EXPECT_TRUE(*descVal);
    Object* desc = Object::Cast(*descVal);
    Local<Value> valueVal = desc->Get(value);
    Integer* intVal = Integer::Cast(*valueVal);
    EXPECT_EQ(intVal->Value(), 0);
    CheckPropertyDescriptor(desc, false, true, true);
  }

  {
    MaybeLocal<Value> maybeDesc =
      object->GetOwnPropertyDescriptor(context, bar);
    EXPECT_FALSE(maybeDesc.IsEmpty());
    Object* desc = Object::Cast(*maybeDesc.ToLocalChecked());
    Local<Value> valueVal = desc->Get(value);
    Integer* intVal = Integer::Cast(*valueVal);
    EXPECT_EQ(intVal->Value(), 1);
    CheckPropertyDescriptor(desc, true, true, true);
  }
  {
    Local<Value> descVal = object->GetOwnPropertyDescriptor(bar);
    EXPECT_TRUE(*descVal);
    Object* desc = Object::Cast(*descVal);
    Local<Value> valueVal = desc->Get(value);
    Integer* intVal = Integer::Cast(*valueVal);
    EXPECT_EQ(intVal->Value(), 1);
    CheckPropertyDescriptor(desc, true, true, true);
  }

  {
    MaybeLocal<Value> maybeDesc =
      object->GetOwnPropertyDescriptor(context, baz);
    EXPECT_FALSE(maybeDesc.IsEmpty());
    Object* desc = Object::Cast(*maybeDesc.ToLocalChecked());
    Local<Value> valueVal = desc->Get(value);
    String::Utf8Value utf8(valueVal);
    EXPECT_STREQ(*utf8, "two");
    CheckPropertyDescriptor(desc, false, false, false);
  }
  {
    Local<Value> descVal = object->GetOwnPropertyDescriptor(baz);
    EXPECT_TRUE(*descVal);
    Object* desc = Object::Cast(*descVal);
    Local<Value> valueVal = desc->Get(value);
    String::Utf8Value utf8(valueVal);
    EXPECT_STREQ(*utf8, "two");
    CheckPropertyDescriptor(desc, false, false, false);
  }

  // Test ForceSet by attempting to overwrite a readonly property.
  // Set will succeed without changing the value.
  EXPECT_TRUE(object->Set(context, bar, two).FromJust());
  {
    MaybeLocal<Value> barVal = object->Get(context, bar);
    EXPECT_FALSE(barVal.IsEmpty());
    Integer* intVal = Integer::Cast(*barVal.ToLocalChecked());
    EXPECT_EQ(intVal->Value(), 1);
  }
  // Now try ForceSet and verify that the value and the PropertyAttribute change.
  EXPECT_TRUE(object->ForceSet(context, bar, two, DontDelete).FromJust());
  {
    MaybeLocal<Value> barVal = object->Get(context, bar);
    EXPECT_FALSE(barVal.IsEmpty());
    String::Utf8Value utf8(barVal.ToLocalChecked());
    EXPECT_STREQ(*utf8, "two");
  }
  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, bar);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(attributes.FromJust(), DontDelete);
  }
  EXPECT_TRUE(object->ForceSet(bar, two, PropertyAttribute(DontDelete | ReadOnly)));
  {
    Maybe<PropertyAttribute> attributes =
      object->GetPropertyAttributes(context, bar);
    EXPECT_TRUE(attributes.IsJust());
    EXPECT_EQ(attributes.FromJust(), DontDelete | ReadOnly);
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
    EXPECT_EQ(intVal->Value(), 1);
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
    EXPECT_EQ(intVal->Value(), 1);
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
  EXPECT_EQ(elms->Length(), 4);
  int elmc0 = 0;
  const char** elmv0 = NULL;
  CheckProperties(
      isolate,
      elms->Get(context, v8::Integer::New(isolate, 0)).ToLocalChecked(),
      elmc0, elmv0);
  CheckOwnProperties(
      isolate,
      elms->Get(context, v8::Integer::New(isolate, 0)).ToLocalChecked(),
      elmc0, elmv0);
  int elmc1 = 2;
  const char* elmv1[] = {"a", "b"};
  CheckProperties(
      isolate,
      elms->Get(context, v8::Integer::New(isolate, 1)).ToLocalChecked(),
      elmc1, elmv1);
  CheckOwnProperties(
      isolate,
      elms->Get(context, v8::Integer::New(isolate, 1)).ToLocalChecked(),
      elmc1, elmv1);
  int elmc2 = 3;
  const char* elmv2[] = {"0", "1", "2"};
  CheckProperties(
      isolate,
      elms->Get(context, v8::Integer::New(isolate, 2)).ToLocalChecked(),
      elmc2, elmv2);
  CheckOwnProperties(
      isolate,
      elms->Get(context, v8::Integer::New(isolate, 2)).ToLocalChecked(),
      elmc2, elmv2);
  int elmc3 = 4;
  const char* elmv3[] = {"w", "z", "x", "y"};
  CheckProperties(
      isolate,
      elms->Get(context, v8::Integer::New(isolate, 3)).ToLocalChecked(),
      elmc3, elmv3);
  int elmc4 = 2;
  const char* elmv4[] = {"w", "z"};
  CheckOwnProperties(
      isolate,
      elms->Get(context, v8::Integer::New(isolate, 3)).ToLocalChecked(),
      elmc4, elmv4);
}

TEST(SpiderShim, Array) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Array> array = Array::New(engine.isolate(), 10);
  EXPECT_EQ(*array, Array::Cast(*array));
  EXPECT_EQ(array->Length(), 10);
  for (int i = 0; i < 10; ++i) {
    MaybeLocal<Value> val = array->Get(context, i);
    EXPECT_TRUE(val.ToLocalChecked()->IsUndefined());
    EXPECT_TRUE(array->Set(context, i, Integer::New(engine.isolate(), i * i)).FromJust());
    val = array->Get(context, i);
    EXPECT_EQ(Integer::Cast(*val.ToLocalChecked())->Value(), i * i);
  }
  EXPECT_TRUE(array->Set(context, 14, Integer::New(engine.isolate(), 42)).FromJust());
  MaybeLocal<Value> val = array->Get(context, 14);
  EXPECT_EQ(Integer::Cast(*val.ToLocalChecked())->Value(), 42);
  EXPECT_EQ(array->Length(), 15);

  Local<String> str = array->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ(*utf8, "0,1,4,9,16,25,36,49,64,81,,,,,42");
  EXPECT_EQ(array->ToBoolean()->Value(), true);
  EXPECT_EQ(array->BooleanValue(), true);
  EXPECT_TRUE(array->ToNumber(context).IsEmpty());
  EXPECT_TRUE(array->NumberValue(context).IsNothing());
  EXPECT_NE(array->NumberValue(), array->NumberValue()); // NaN
  EXPECT_TRUE(array->ToInteger(context).IsEmpty());
  EXPECT_TRUE(array->IntegerValue(context).IsNothing());
  EXPECT_EQ(array->IntegerValue(), 0);
  EXPECT_TRUE(array->ToObject()->IsObject());
  EXPECT_EQ(Object::Cast(*array->ToObject())->Get(2)->ToInteger()->Value(), 4);
}

TEST(SpiderShim, BooleanObject) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Value> boolean = BooleanObject::New(true);
  EXPECT_EQ(*boolean, BooleanObject::Cast(*boolean));
  EXPECT_TRUE(boolean->IsBooleanObject());
  EXPECT_TRUE(BooleanObject::Cast(*boolean)->ValueOf());

  Local<String> str = boolean->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ(*utf8, "true");
  EXPECT_EQ(boolean->ToBoolean()->Value(), true);
  EXPECT_EQ(boolean->BooleanValue(), true);
  EXPECT_EQ(boolean->ToNumber()->Value(), 1.0);
  EXPECT_EQ(boolean->NumberValue(), 1.0);
  EXPECT_EQ(boolean->ToInteger()->Value(), 1);
  EXPECT_EQ(boolean->IntegerValue(), 1);
  EXPECT_TRUE(boolean->ToObject()->IsBooleanObject());
  EXPECT_EQ(BooleanObject::Cast(*boolean->ToObject())->ValueOf(), true);
}

TEST(SpiderShim, NumberObject) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Value> num = NumberObject::New(engine.isolate(), 42);
  EXPECT_EQ(*num, NumberObject::Cast(*num));
  EXPECT_TRUE(num->IsNumberObject());
  EXPECT_EQ(NumberObject::Cast(*num)->ValueOf(), 42);

  Local<String> str = num->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ(*utf8, "42");
  EXPECT_EQ(num->ToBoolean()->Value(), true);
  EXPECT_EQ(num->BooleanValue(), true);
  EXPECT_DOUBLE_EQ(num->ToNumber()->Value(), 42.0);
  EXPECT_DOUBLE_EQ(num->NumberValue(), 42.0);
  EXPECT_DOUBLE_EQ(num->ToInteger()->Value(), 42);
  EXPECT_DOUBLE_EQ(num->IntegerValue(), 42);
  EXPECT_TRUE(num->ToObject()->IsNumberObject());
  EXPECT_EQ(NumberObject::Cast(*num->ToObject())->ValueOf(), 42.0);
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
  EXPECT_EQ(*str, StringObject::Cast(*str));
  EXPECT_TRUE(str->IsStringObject());
  String::Utf8Value utf8(StringObject::Cast(*str)->ValueOf());
  EXPECT_STREQ(*utf8, "foobar");

  Local<String> str_2 = str->ToString();
  String::Utf8Value utf8_2(str_2);
  EXPECT_STREQ(*utf8_2, "foobar");
  EXPECT_EQ(str->ToBoolean()->Value(), true);
  EXPECT_EQ(str->BooleanValue(), true);
  EXPECT_TRUE(str->ToNumber(context).IsEmpty());
  EXPECT_TRUE(str->NumberValue(context).IsNothing());
  EXPECT_NE(str->NumberValue(), str->NumberValue()); // NaN
  EXPECT_TRUE(str->ToInteger(context).IsEmpty());
  EXPECT_TRUE(str->IntegerValue(context).IsNothing());
  EXPECT_EQ(str->IntegerValue(), 0);
  EXPECT_TRUE(str->ToObject()->IsStringObject());
  EXPECT_EQ(StringObject::Cast(*str->ToObject())->ValueOf()->Length(), 6);
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
  EXPECT_EQ(*date.ToLocalChecked(), Date::Cast(*date.ToLocalChecked()));
  EXPECT_TRUE(date.ToLocalChecked()->IsDate());
  EXPECT_EQ(Date::Cast(*date.ToLocalChecked())->ValueOf(), time);

  Local<String> str = date.ToLocalChecked()->ToString();
  String::Utf8Value utf8(str);
  const char datePortion[] = "Thu Oct 23 2008";
  // Only compare the date portion, as the time part will change depending on
  // the timezone!
  EXPECT_EQ(0, strncmp(*utf8, datePortion, sizeof(datePortion) - 1));
  EXPECT_EQ(date.ToLocalChecked()->ToBoolean()->Value(), true);
  EXPECT_EQ(date.ToLocalChecked()->BooleanValue(), true);
  EXPECT_DOUBLE_EQ(date.ToLocalChecked()->ToNumber()->Value(), time);
  EXPECT_DOUBLE_EQ(date.ToLocalChecked()->NumberValue(), time);
  EXPECT_EQ(date.ToLocalChecked()->ToInteger()->Value(), time);
  EXPECT_EQ(date.ToLocalChecked()->IntegerValue(), time);
  EXPECT_TRUE(date.ToLocalChecked()->ToObject()->IsDate());
  EXPECT_EQ(Date::Cast(*date.ToLocalChecked()->ToObject())->ValueOf(), time);
}

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
  EXPECT_EQ(foobar->Length(), 6);
  EXPECT_EQ(foobar->Utf8Length(), 6);
  EXPECT_EQ(String::Empty(engine.isolate())->Length(), 0);
  String::Utf8Value utf8(foobar);
  EXPECT_STREQ(*utf8, "foobar");
  String::Value twobytes(foobar);
  EXPECT_EQ(0, memcmp(*twobytes, u"foobar", sizeof(u"foobar")));
  Local<String> concat = String::Concat(foobar, baz);
  String::Utf8Value utf8Concat(concat);
  EXPECT_STREQ(*utf8Concat, "foobarbaz");
  EXPECT_TRUE(foobar->ToObject()->IsStringObject());
  EXPECT_EQ(StringObject::Cast(*foobar->ToObject())->ValueOf()->Length(), 6);
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
