// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
void TestNumber(Isolate* isolate, T value) {
  Local<Number> number = Number::New(isolate, value);
  EXPECT_TRUE(number->IsNumber());
  EXPECT_FALSE(number->IsBoolean());
  EXPECT_EQ(number->Value(), double(value));
}

TEST(SpiderShim, Number) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TestNumber(engine.isolate(), 0);
  TestNumber(engine.isolate(), 42);
  TestNumber(engine.isolate(), 42.42);
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
};

template<>
class IntegerMaker<uint32_t> {
public:
  static Local<Integer> New(Isolate* isolate, uint32_t value) {
    return Integer::NewFromUnsigned(isolate, value);
  }
  typedef Uint32 IntType;
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

  EXPECT_TRUE(object->Set(context, foo, zero).FromJust());
  EXPECT_TRUE(object->DefineOwnProperty(context, bar, one, ReadOnly).FromJust());
  EXPECT_TRUE(object->DefineOwnProperty(context, baz, two, PropertyAttribute(DontEnum | DontDelete)).FromJust());
  EXPECT_TRUE(object->Set(context, 1, zero).FromJust());
  EXPECT_TRUE(object->Set(context, 0, two).FromJust());

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
    EXPECT_EQ(strcmp(*utf8, "two"), 0);
  }
  {
    Local<Value> bazVal = object->Get(baz);
    String::Utf8Value utf8(bazVal);
    EXPECT_EQ(strcmp(*utf8, "two"), 0);
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
    EXPECT_EQ(strcmp(*utf8, "two"), 0);
  }
  {
    Local<Value> zeroVal = object->Get(0);
    String::Utf8Value utf8(zeroVal);
    EXPECT_EQ(strcmp(*utf8, "two"), 0);
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
    EXPECT_EQ(strcmp(*utf8, "two"), 0);
    CheckPropertyDescriptor(desc, false, false, false);
  }
  {
    Local<Value> descVal = object->GetOwnPropertyDescriptor(baz);
    EXPECT_TRUE(*descVal);
    Object* desc = Object::Cast(*descVal);
    Local<Value> valueVal = desc->Get(value);
    String::Utf8Value utf8(valueVal);
    EXPECT_EQ(strcmp(*utf8, "two"), 0);
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
    EXPECT_EQ(strcmp(*utf8, "two"), 0);
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
