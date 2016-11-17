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

// From allocation.h.
template <typename T>
T* NewArray(size_t size) {
  T* result = new T[size];
  // if (result == NULL) FatalProcessOutOfMemory("NewArray");
  return result;
}

// From allocation.h.
template <typename T>
void DeleteArray(T* array) {
  delete[] array;
}

/**
 * Ensure that the JSString object referenced by the first Local<String>
 * is the same JSString object referenced by the second Local<String>, i.e.
 * that both Local<String>s reference the same JSString (as they should do
 * when the string in question was interned).
 *
 * This is based on the function of the same name in test-api.cc, but modified
 * to compare JSString objects in a SpiderShim-compatible way.  This invokes
 * the operator== method for Value, which compares the spidershim_padding
 * data members of the two Value objects to determine whether or not they refer
 * to the same JSString.
 *
 * Since the operator== method is protected, we access it through Persistent<T>,
 * which is a friend of Value.  See the comment for the Value class in v8.h
 * for more information about the way a Value instance encapsulates a JS::Value.
 */
static bool SameSymbol(Local<String> s1, Local<String> s2) {
  Isolate* isolate = Isolate::GetCurrent();
  // Use Persistent<T> to get access to the protected Value "equal to" operator.
  Persistent<String> p1(isolate, s1);
  Persistent<String> p2(isolate, s2);
  bool same = p1 == p2;
  p1.Reset();
  p2.Reset();
  return same;
}

// Translations from cctest assertion macros to gtest equivalents, so we can
// copy code from test-api.cc into this file with minimal modifications.
#define CHECK(expression) EXPECT_TRUE(expression)
#define CHECK_EQ(a, b) EXPECT_EQ(a, b)
#define CHECK_NE(a, b) EXPECT_NE(a, b)

// From test-api.cc.
class RandomLengthOneByteResource
    : public String::ExternalOneByteStringResource {
 public:
  explicit RandomLengthOneByteResource(int length) : length_(length) {
    string_ = NewArray<char>(length);
    memset(string_, 0x1, length * sizeof(char));
  }
  ~RandomLengthOneByteResource() {
    DeleteArray(string_);
  }
  virtual const char* data() const { return string_; }
  virtual size_t length() const { return length_; }

 private:
  char* string_;
  int length_;
};

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
  EXPECT_EQ(value ? 1u : 0u, boolean->ToUint32()->Value());
  EXPECT_EQ(value ? 1u : 0u, boolean->Uint32Value());
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

TEST(SpiderShim, Undefined) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Value> undefined = Undefined(engine.isolate());
  EXPECT_EQ(0.0, undefined->ToInteger()->Value());
  EXPECT_EQ(false, undefined->IsExternal());
}

template<class T>
void CheckIsUint32(Local<Number> number) {
  EXPECT_TRUE(number->IsUint32());
}

template<>
void CheckIsUint32<double>(Local<Number> number) {
  EXPECT_FALSE(number->IsUint32());
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
  CheckIsUint32<T>(number);
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
  EXPECT_EQ(static_cast<int>(value), intVal->ToInt32()->Value());
  EXPECT_EQ(static_cast<int>(value), intVal->Int32Value());
  EXPECT_EQ(static_cast<uint32_t>(value), intVal->ToUint32()->Value());
  EXPECT_EQ(static_cast<uint32_t>(value), intVal->Uint32Value());
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
  EXPECT_EQ(0u, object->ToUint32()->Value());
  EXPECT_EQ(0u, object->Uint32Value());
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

  EXPECT_TRUE(object->GetPrototype()->StrictEquals(newProto));
  EXPECT_TRUE(object->SetPrototype(context, Null(engine.isolate())).FromJust());
  EXPECT_TRUE(object->GetPrototype()->StrictEquals(Null(engine.isolate())));
}

static void InstallContextId(Local<Context> context, int id) {
  Context::Scope scope(context);
  EXPECT_TRUE(CompileRun("Object.prototype")
            .As<Object>()
            ->Set(context, v8_str("context_id"),
                  Integer::New(context->GetIsolate(), id))
            .FromJust());
}

static void CheckContextId(Local<Object> object, int expected) {
  Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
  EXPECT_EQ(expected, object->Get(context, v8_str("context_id"))
                         .ToLocalChecked()
                         ->Int32Value(context)
                         .FromJust());
}

TEST(SpiderShim, CreationContext) {
  // This test is adopted from the V8 CreationContext test.
  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Isolate* isolate = engine.isolate();

  Local<Context> context1 = Context::New(isolate);
  InstallContextId(context1, 1);
  Local<Context> context2 = Context::New(isolate);
  InstallContextId(context2, 2);
  Local<Context> context3 = Context::New(isolate);
  InstallContextId(context3, 3);

  Local<Object> object1;
  Local<Function> func1;
  {
    Context::Scope scope(context1);
    object1 = Object::New(isolate);
    Local<FunctionTemplate> tmpl = FunctionTemplate::New(isolate);
    func1 = tmpl->GetFunction(context1).ToLocalChecked();
  }

  Local<Object> object2;
  Local<Function> func2;
  {
    Context::Scope scope(context2);
    object2 = Object::New(isolate);
    Local<FunctionTemplate> tmpl = FunctionTemplate::New(isolate);
    func2 = tmpl->GetFunction(context2).ToLocalChecked();
  }

  Local<Object> instance1;
  Local<Object> instance2;

  {
    Context::Scope scope(context3);
    instance1 = func1->NewInstance(context3).ToLocalChecked();
    instance2 = func2->NewInstance(context3).ToLocalChecked();
  }

  {
    Local<Context> other_context = Context::New(isolate);
    Context::Scope scope(other_context);
    EXPECT_TRUE(object1->CreationContext() == context1);
    CheckContextId(object1, 1);
    EXPECT_TRUE(func1->CreationContext() == context1);
    CheckContextId(func1, 1);
    EXPECT_TRUE(instance1->CreationContext() == context1);
    CheckContextId(instance1, 1);
    EXPECT_TRUE(object2->CreationContext() == context2);
    CheckContextId(object2, 2);
    EXPECT_TRUE(func2->CreationContext() == context2);
    CheckContextId(func2, 2);
    EXPECT_TRUE(instance2->CreationContext() == context2);
    CheckContextId(instance2, 2);
  }

  {
    Context::Scope scope(context1);
    EXPECT_TRUE(object1->CreationContext() == context1);
    CheckContextId(object1, 1);
    EXPECT_TRUE(func1->CreationContext() == context1);
    CheckContextId(func1, 1);
    EXPECT_TRUE(instance1->CreationContext() == context1);
    CheckContextId(instance1, 1);
    EXPECT_TRUE(object2->CreationContext() == context2);
    CheckContextId(object2, 2);
    EXPECT_TRUE(func2->CreationContext() == context2);
    CheckContextId(func2, 2);
    EXPECT_TRUE(instance2->CreationContext() == context2);
    CheckContextId(instance2, 2);
  }

  {
    Context::Scope scope(context2);
    EXPECT_TRUE(object1->CreationContext() == context1);
    CheckContextId(object1, 1);
    EXPECT_TRUE(func1->CreationContext() == context1);
    CheckContextId(func1, 1);
    EXPECT_TRUE(instance1->CreationContext() == context1);
    CheckContextId(instance1, 1);
    EXPECT_TRUE(object2->CreationContext() == context2);
    CheckContextId(object2, 2);
    EXPECT_TRUE(func2->CreationContext() == context2);
    CheckContextId(func2, 2);
    EXPECT_TRUE(instance2->CreationContext() == context2);
    CheckContextId(instance2, 2);
  }
}

TEST(SpiderShim, CreationContextOfJsFunction) {
  // This test is adopted from the V8 CreationContextOfJsFunction test.
  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());

  Local<Context> context = Context::New(engine.isolate());
  InstallContextId(context, 1);

  Local<Object> function;
  {
    Context::Scope scope(context);
    function = CompileRun("function foo() {}; foo").As<Object>();
  }

  Local<Context> other_context = Context::New(engine.isolate());
  Context::Scope scope(other_context);
  EXPECT_TRUE(function->CreationContext() == context);
  CheckContextId(function, 1);
}


TEST(SpiderShim, CreationContextOfJsBoundFunction) {
  // This test is adopted from the V8 CreationContextOfJsBoundFunction test.
  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());

  Local<Context> context1 = Context::New(engine.isolate());
  InstallContextId(context1, 1);
  Local<Context> context2 = Context::New(engine.isolate());
  InstallContextId(context2, 2);

  Local<Function> target_function;
  {
    Context::Scope scope(context1);
    target_function = CompileRun("function foo() {}; foo").As<Function>();
  }

  Local<Function> bound_function1, bound_function2;
  {
    Context::Scope scope(context2);
    EXPECT_TRUE(context2->Global()
              ->Set(context2, v8_str("foo"), target_function)
              .FromJust());
    bound_function1 = CompileRun("foo.bind(1)").As<Function>();
    bound_function2 =
        CompileRun("Function.prototype.bind.call(foo, 2)").As<Function>();
  }

  Local<Context> other_context = Context::New(engine.isolate());
  Context::Scope scope(other_context);
  EXPECT_TRUE(bound_function1->CreationContext() == context1);
  CheckContextId(bound_function1, 1);
  EXPECT_TRUE(bound_function2->CreationContext() == context1);
  CheckContextId(bound_function2, 1);
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
  EXPECT_EQ(4u, elms->Length());
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

static void GetXValue(Local<Name> name,
                      const v8::PropertyCallbackInfo<v8::Value>& info) {
  EXPECT_TRUE(info.Data()
            ->Equals(Isolate::GetCurrent()->GetCurrentContext(), v8_str("donut"))
            .FromJust());
  EXPECT_TRUE(name->Equals(Isolate::GetCurrent()->GetCurrentContext(), v8_str("x"))
            .FromJust());
  info.GetReturnValue().Set(name);
}

TEST(SpiderShim, SimplePropertyRead) {
  // This test is adopted from the V8 SimplePropertyRead test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("x"), GetXValue, NULL, v8_str("donut"));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  Local<Script> script = v8_compile("obj.x");
  for (int i = 0; i < 10; i++) {
    Local<Value> result = script->Run(context).ToLocalChecked();
    EXPECT_TRUE(result->Equals(context, v8_str("x")).FromJust());
  }
}

TEST(SpiderShim, DefinePropertyOnAPIAccessor) {
  // This test is adopted from the V8 DefinePropertyOnAPIAccessor test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("x"), GetXValue, NULL, v8_str("donut"));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());

  // Uses getOwnPropertyDescriptor to check the configurable status
  Local<Script> script_desc = v8_compile(
      "var prop = Object.getOwnPropertyDescriptor( "
      "obj, 'x');"
      "prop.configurable;");
  Local<Value> result = script_desc->Run(context).ToLocalChecked();
  EXPECT_EQ(result->BooleanValue(context).FromJust(), true);

  // Redefine get - but still configurable
  Local<Script> script_define = v8_compile(
      "var desc = { get: function(){return 42; },"
      "            configurable: true };"
      "Object.defineProperty(obj, 'x', desc);"
      "obj.x");
  result = script_define->Run(context).ToLocalChecked();
  EXPECT_TRUE(result->Equals(context, v8_num(42)).FromJust());

  // Check that the accessor is still configurable
  result = script_desc->Run(context).ToLocalChecked();
  EXPECT_EQ(result->BooleanValue(context).FromJust(), true);

  // Redefine to a non-configurable
  script_define = v8_compile(
      "var desc = { get: function(){return 43; },"
      "             configurable: false };"
      "Object.defineProperty(obj, 'x', desc);"
      "obj.x");
  result = script_define->Run(context).ToLocalChecked();
  EXPECT_TRUE(result->Equals(context, v8_num(43)).FromJust());
  result = script_desc->Run(context).ToLocalChecked();
  EXPECT_EQ(result->BooleanValue(context).FromJust(), false);

  // Make sure that it is not possible to redefine again
  v8::TryCatch try_catch(isolate);
  EXPECT_TRUE(script_define->Run(context).IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  String::Utf8Value exception_value(try_catch.Exception());
  EXPECT_STREQ("TypeError: can't redefine non-configurable property \"x\"", *exception_value);
}

TEST(SpiderShim, DefinePropertyOnDefineGetterSetter) {
  // This test is adopted from the V8 DefinePropertyOnDefineGetterSetter test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("x"), GetXValue, NULL, v8_str("donut"));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());

  Local<Script> script_desc = v8_compile(
      "var prop ="
      "Object.getOwnPropertyDescriptor( "
      "obj, 'x');"
      "prop.configurable;");
  Local<Value> result = script_desc->Run(context).ToLocalChecked();
  EXPECT_EQ(result->BooleanValue(context).FromJust(), true);

  Local<Script> script_define = v8_compile(
      "var desc = {get: function(){return 42; },"
      "            configurable: true };"
      "Object.defineProperty(obj, 'x', desc);"
      "obj.x");
  result = script_define->Run(context).ToLocalChecked();
  EXPECT_TRUE(result->Equals(context, v8_num(42)).FromJust());

  result = script_desc->Run(context).ToLocalChecked();
  EXPECT_EQ(result->BooleanValue(context).FromJust(), true);

  script_define = v8_compile(
      "var desc = {get: function(){return 43; },"
      "            configurable: false };"
      "Object.defineProperty(obj, 'x', desc);"
      "obj.x");
  result = script_define->Run(context).ToLocalChecked();
  EXPECT_TRUE(result->Equals(context, v8_num(43)).FromJust());

  result = script_desc->Run(context).ToLocalChecked();
  EXPECT_EQ(result->BooleanValue(context).FromJust(), false);

  v8::TryCatch try_catch(isolate);
  EXPECT_TRUE(script_define->Run(context).IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  String::Utf8Value exception_value(try_catch.Exception());
  EXPECT_STREQ("TypeError: can't redefine non-configurable property \"x\"", *exception_value);
}

static inline void ExpectString(const char* code, const char* expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsString());
  v8::String::Utf8Value utf8(result);
  CHECK_EQ(0, strcmp(expected, *utf8));
}

static inline void ExpectBoolean(const char* code, bool expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsBoolean());
  CHECK_EQ(expected,
           result->BooleanValue(v8::Isolate::GetCurrent()->GetCurrentContext())
               .FromJust());
}

static inline void ExpectTrue(const char* code) {
  ExpectBoolean(code, true);
}

static inline void ExpectFalse(const char* code) {
  ExpectBoolean(code, false);
}

static v8::Local<v8::Object> GetGlobalProperty(Context* context,
                                               char const* name) {
  return v8::Local<v8::Object>::Cast(
      (context)
          ->Global()
          ->Get(Isolate::GetCurrent()->GetCurrentContext(), v8_str(name))
          .ToLocalChecked());
}

TEST(SpiderShim, DefineAPIAccessorOnObject) {
  // This test is adopted from the V8 DefineAPIAccessorOnObject test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj1"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  CompileRun("var obj2 = {};");

  EXPECT_TRUE(CompileRun("obj1.x")->IsUndefined());
  EXPECT_TRUE(CompileRun("obj2.x")->IsUndefined());

  EXPECT_TRUE(GetGlobalProperty(*context, "obj1")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"))
            .FromJust());

  ExpectString("obj1.x", "x");
  EXPECT_TRUE(CompileRun("obj2.x")->IsUndefined());

  EXPECT_TRUE(GetGlobalProperty(*context, "obj2")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"))
            .FromJust());

  ExpectString("obj1.x", "x");
  ExpectString("obj2.x", "x");

  ExpectTrue("Object.getOwnPropertyDescriptor(obj1, 'x').configurable");
  ExpectTrue("Object.getOwnPropertyDescriptor(obj2, 'x').configurable");

  CompileRun(
      "Object.defineProperty(obj1, 'x',"
      "{ get: function() { return 'y'; }, configurable: true })");

  ExpectString("obj1.x", "y");
  ExpectString("obj2.x", "x");

  CompileRun(
      "Object.defineProperty(obj2, 'x',"
      "{ get: function() { return 'y'; }, configurable: true })");

  ExpectString("obj1.x", "y");
  ExpectString("obj2.x", "y");

  ExpectTrue("Object.getOwnPropertyDescriptor(obj1, 'x').configurable");
  ExpectTrue("Object.getOwnPropertyDescriptor(obj2, 'x').configurable");

  EXPECT_TRUE(GetGlobalProperty(*context, "obj1")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"))
            .FromJust());
  EXPECT_TRUE(GetGlobalProperty(*context, "obj2")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"))
            .FromJust());

  ExpectString("obj1.x", "x");
  ExpectString("obj2.x", "x");

  ExpectTrue("Object.getOwnPropertyDescriptor(obj1, 'x').configurable");
  ExpectTrue("Object.getOwnPropertyDescriptor(obj2, 'x').configurable");

  // Define getters/setters, but now make them not configurable.
  CompileRun(
      "Object.defineProperty(obj1, 'x',"
      "{ get: function() { return 'z'; }, configurable: false })");
  CompileRun(
      "Object.defineProperty(obj2, 'x',"
      "{ get: function() { return 'z'; }, configurable: false })");
  ExpectTrue("!Object.getOwnPropertyDescriptor(obj1, 'x').configurable");
  ExpectTrue("!Object.getOwnPropertyDescriptor(obj2, 'x').configurable");

  ExpectString("obj1.x", "z");
  ExpectString("obj2.x", "z");

  EXPECT_TRUE(GetGlobalProperty(*context, "obj1")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"))
            .IsNothing());
  EXPECT_TRUE(GetGlobalProperty(*context, "obj2")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"))
            .IsNothing());

  ExpectString("obj1.x", "z");
  ExpectString("obj2.x", "z");
}

TEST(SpiderShim, DontDeleteAPIAccessorsCannotBeOverriden) {
  // This test is adopted from the V8 DontDeleteAPIAccessorsCannotBeOverriden test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj1"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  CompileRun("var obj2 = {};");

  EXPECT_TRUE(GetGlobalProperty(*context, "obj1")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"), v8::DEFAULT, v8::DontDelete)
            .FromJust());
  EXPECT_TRUE(GetGlobalProperty(*context, "obj2")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"), v8::DEFAULT, v8::DontDelete)
            .FromJust());

  ExpectString("obj1.x", "x");
  ExpectString("obj2.x", "x");

  ExpectTrue("!Object.getOwnPropertyDescriptor(obj1, 'x').configurable");
  ExpectTrue("!Object.getOwnPropertyDescriptor(obj2, 'x').configurable");

  EXPECT_TRUE(GetGlobalProperty(*context, "obj1")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"))
            .IsNothing());
  EXPECT_TRUE(GetGlobalProperty(*context, "obj2")
            ->SetAccessor(context, v8_str("x"), GetXValue, NULL,
                          v8_str("donut"))
            .IsNothing());

  {
    v8::TryCatch try_catch(isolate);
    CompileRun(
        "Object.defineProperty(obj1, 'x',"
        "{get: function() { return 'func'; }})");
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value(try_catch.Exception());
    EXPECT_STREQ("TypeError: can't redefine non-configurable property \"x\"", *exception_value);
  }
  {
    v8::TryCatch try_catch(isolate);
    CompileRun(
        "Object.defineProperty(obj2, 'x',"
        "{get: function() { return 'func'; }})");
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value(try_catch.Exception());
    EXPECT_STREQ("TypeError: can't redefine non-configurable property \"x\"", *exception_value);
  }
}

static void Get239Value(Local<Name> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  EXPECT_TRUE(info.Data()
            ->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("donut"))
            .FromJust());
  EXPECT_TRUE(name->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("239"))
            .FromJust());
  info.GetReturnValue().Set(name);
}

TEST(SpiderShim, ElementAPIAccessor) {
  // This test is adopted from the V8 ElementAPIAccessor test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj1"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  CompileRun("var obj2 = {};");

  EXPECT_TRUE(GetGlobalProperty(*context, "obj1")
            ->SetAccessor(context, v8_str("239"), Get239Value, NULL,
                          v8_str("donut"))
            .FromJust());
  EXPECT_TRUE(GetGlobalProperty(*context, "obj2")
            ->SetAccessor(context, v8_str("239"), Get239Value, NULL,
                          v8_str("donut"))
            .FromJust());

  ExpectString("obj1[239]", "239");
  ExpectString("obj2[239]", "239");
  ExpectString("obj1['239']", "239");
  ExpectString("obj2['239']", "239");
}

v8::Persistent<Value> xValue;

static void SetXValue(Local<Name> name, Local<Value> value,
                      const v8::PropertyCallbackInfo<void>& info) {
  Local<Context> context = info.GetIsolate()->GetCurrentContext();
  EXPECT_TRUE(value->Equals(context, v8_num(4)).FromJust());
  EXPECT_TRUE(info.Data()->Equals(context, v8_str("donut")).FromJust());
  EXPECT_TRUE(name->Equals(context, v8_str("x")).FromJust());
  EXPECT_TRUE(xValue.IsEmpty());
  xValue.Reset(info.GetIsolate(), value);
}

TEST(SpiderShim, SimplePropertyWrite) {
  // This test is adopted from the V8 SimplePropertyWrite test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("x"), GetXValue, SetXValue, v8_str("donut"));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  Local<Script> script = v8_compile("obj.x = 4");
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(xValue.IsEmpty());
    script->Run(context).ToLocalChecked();
    EXPECT_TRUE(v8_num(4)
              ->Equals(context,
                       Local<Value>::New(Isolate::GetCurrent(), xValue))
              .FromJust());
    xValue.Reset();
  }
}

TEST(SpiderShim, SetterOnly) {
  // This test is adopted from the V8 SetterOnly test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("x"), NULL, SetXValue, v8_str("donut"));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  Local<Script> script = v8_compile("obj.x = 4; obj.x");
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(xValue.IsEmpty());
    script->Run(context).ToLocalChecked();
    EXPECT_TRUE(v8_num(4)
              ->Equals(context,
                       Local<Value>::New(Isolate::GetCurrent(), xValue))
              .FromJust());
    xValue.Reset();
  }
}

TEST(SpiderShim, NoAccessors) {
  // This test is adopted from the V8 NoAccessors test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("x"), static_cast<v8::AccessorGetterCallback>(NULL),
                     NULL, v8_str("donut"));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  Local<Script> script = v8_compile("obj.x = 4; obj.x");
  for (int i = 0; i < 10; i++) {
    script->Run(context).ToLocalChecked();
  }
}

TEST(SpiderShim, Array) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Array> array = Array::New(engine.isolate(), 10);
  EXPECT_EQ(Array::Cast(*array), *array);
  EXPECT_EQ(10u, array->Length());
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
  EXPECT_EQ(15u, array->Length());

  Local<Object> copy = array->Clone();
  Local<Array> arrayClone = Local<Array>::Cast(copy);
  for (int i = 0; i < 10; ++i) {
    MaybeLocal<Value> val = array->Get(context, i);
    EXPECT_EQ(i * i, Integer::Cast(*val.ToLocalChecked())->Value());
  }
  val = arrayClone->Get(context, 14);
  EXPECT_EQ(42, Integer::Cast(*val.ToLocalChecked())->Value());
  EXPECT_EQ(15u, arrayClone->Length());

  Local<String> str = array->ToString();
  String::Utf8Value utf8(str);
  EXPECT_STREQ("0,1,4,9,16,25,36,49,64,81,,,,,42", *utf8);

  str = arrayClone->ToString();
  String::Utf8Value utf8Clone(str);
  EXPECT_STREQ("0,1,4,9,16,25,36,49,64,81,,,,,42", *utf8Clone);

  EXPECT_EQ(true, array->ToBoolean()->Value());
  EXPECT_EQ(true, array->BooleanValue());
  EXPECT_TRUE(array->ToNumber(context).IsEmpty());
  EXPECT_TRUE(array->NumberValue(context).IsNothing());
  EXPECT_NE(array->NumberValue(), array->NumberValue()); // NaN
  EXPECT_TRUE(array->ToInteger(context).IsEmpty());
  EXPECT_TRUE(array->IntegerValue(context).IsNothing());
  EXPECT_EQ(0, array->ToInt32()->Value());
  EXPECT_EQ(0, array->Int32Value());
  EXPECT_EQ(0u, array->ToUint32()->Value());
  EXPECT_EQ(0u, array->Uint32Value());
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
  EXPECT_EQ(1u, boolean->ToUint32()->Value());
  EXPECT_EQ(1u, boolean->Uint32Value());
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
  EXPECT_DOUBLE_EQ(42.0, num->ToNumber()->Value());
  EXPECT_DOUBLE_EQ(42.0, num->NumberValue());
  EXPECT_DOUBLE_EQ(42, num->ToInteger()->Value());
  EXPECT_DOUBLE_EQ(42, num->IntegerValue());
  EXPECT_DOUBLE_EQ(42, num->ToInt32()->Value());
  EXPECT_DOUBLE_EQ(42, num->Int32Value());
  EXPECT_DOUBLE_EQ(42, num->ToUint32()->Value());
  EXPECT_DOUBLE_EQ(42, num->Uint32Value());
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
  EXPECT_EQ(0u, str->ToUint32()->Value());
  EXPECT_EQ(0u, str->Uint32Value());
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
  EXPECT_DOUBLE_EQ(time, date.ToLocalChecked()->ToNumber()->Value());
  EXPECT_DOUBLE_EQ(time, date.ToLocalChecked()->NumberValue());
  EXPECT_EQ(time, date.ToLocalChecked()->ToInteger()->Value());
  EXPECT_EQ(time, date.ToLocalChecked()->IntegerValue());
  EXPECT_EQ(int64_t(time) & 0xffffffff, date.ToLocalChecked()->ToInt32()->Value());
  EXPECT_EQ(int64_t(time) & 0xffffffff, date.ToLocalChecked()->Int32Value());
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
  const uint16_t asciiResult[] = { 0x4F, 0x68, 0x61, 0x69 };

  {
    Local<String> asciiStr =
      String::NewFromOneByte(engine.isolate(), asciiData, NewStringType::kNormal).
        ToLocalChecked();
    EXPECT_EQ(4, asciiStr->Length());
    EXPECT_EQ(4, asciiStr->Utf8Length());
    String::Value asciiVal(asciiStr);
    EXPECT_EQ(0, memcmp(*asciiVal, asciiResult, sizeof(asciiResult)));
  }

  {
    Local<String> asciiStr =
      String::NewFromOneByte(engine.isolate(), asciiData, NewStringType::kNormal, 3).
        ToLocalChecked();
    EXPECT_EQ(3, asciiStr->Length());
    EXPECT_EQ(3, asciiStr->Utf8Length());
    String::Value asciiVal(asciiStr);
    EXPECT_EQ(0, memcmp(*asciiVal, asciiResult, 3 * sizeof(*asciiResult)));
  }

  {
    Local<String> asciiStr =
      String::NewFromOneByte(engine.isolate(), asciiData, NewStringType::kInternalized, 3).
        ToLocalChecked();
    EXPECT_EQ(3, asciiStr->Length());
    EXPECT_EQ(3, asciiStr->Utf8Length());
    String::Value asciiVal(asciiStr);
    EXPECT_EQ(0, memcmp(*asciiVal, asciiResult, 3 * sizeof(*asciiResult)));
  }

  const uint8_t latin1Data[] = { 0xD3, 0x68, 0xE3, 0xEF, 0x00 }; // "Óhãï"
  const uint16_t latin1Result[] = { 0xD3, 0x68, 0xE3, 0xEF };

  {
    Local<String> latin1Str =
      String::NewFromOneByte(engine.isolate(), latin1Data, NewStringType::kNormal).
        ToLocalChecked();
    EXPECT_EQ(4, latin1Str->Length());
    EXPECT_EQ(7, latin1Str->Utf8Length());
    String::Value latin1Val(latin1Str);
    EXPECT_EQ(0, memcmp(*latin1Val, latin1Result, sizeof(latin1Result)));
  }

  {
    Local<String> latin1Str =
      String::NewFromOneByte(engine.isolate(), latin1Data, NewStringType::kNormal, 3).
        ToLocalChecked();
    EXPECT_EQ(3, latin1Str->Length());
    EXPECT_EQ(5, latin1Str->Utf8Length());
    String::Value latin1Val(latin1Str);
    EXPECT_EQ(0, memcmp(*latin1Val, latin1Result, 3 * sizeof(*latin1Result)));
  }

  {
    Local<String> latin1Str =
      String::NewFromOneByte(engine.isolate(), latin1Data, NewStringType::kInternalized, 3).
        ToLocalChecked();
    EXPECT_EQ(3, latin1Str->Length());
    EXPECT_EQ(5, latin1Str->Utf8Length());
    String::Value latin1Val(latin1Str);
    EXPECT_EQ(0, memcmp(*latin1Val, latin1Result, 3 * sizeof(*latin1Result)));
  }

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

  {
    Local<String> fromTwoByteStr =
      String::NewFromTwoByte(engine.isolate(), utf16Data, NewStringType::kNormal).
        ToLocalChecked();
    EXPECT_EQ(5, fromTwoByteStr->Length());
    EXPECT_EQ(10, fromTwoByteStr->Utf8Length());
    String::Value fromTwoByteVal(fromTwoByteStr);
    String::Utf8Value fromTwoByteUtf8Val(fromTwoByteStr);
    EXPECT_EQ(0, memcmp(*fromTwoByteVal, utf16Data, sizeof(utf16Data)));
    EXPECT_EQ(0, memcmp(*fromTwoByteUtf8Val, utf8Data, sizeof(utf8Data)));
  }

  {
    Local<String> fromTwoByteStr =
      String::NewFromTwoByte(engine.isolate(), utf16Data, NewStringType::kNormal, 4).
        ToLocalChecked();
    EXPECT_EQ(4, fromTwoByteStr->Length());
    EXPECT_EQ(7, fromTwoByteStr->Utf8Length());
    String::Value fromTwoByteVal(fromTwoByteStr);
    String::Utf8Value fromTwoByteUtf8Val(fromTwoByteStr);
    EXPECT_EQ(0, memcmp(*fromTwoByteVal, utf16Data, 4 * sizeof(*utf16Data)));
    EXPECT_EQ(0, memcmp(*fromTwoByteUtf8Val, utf8Data, 7 * sizeof(*utf8Data)));
  }

  {
    Local<String> fromTwoByteStr =
      String::NewFromTwoByte(engine.isolate(), utf16Data, NewStringType::kInternalized, 4).
        ToLocalChecked();
    EXPECT_EQ(4, fromTwoByteStr->Length());
    EXPECT_EQ(7, fromTwoByteStr->Utf8Length());
    String::Value fromTwoByteVal(fromTwoByteStr);
    String::Utf8Value fromTwoByteUtf8Val(fromTwoByteStr);
    EXPECT_EQ(0, memcmp(*fromTwoByteVal, utf16Data, 4 * sizeof(*utf16Data)));
    EXPECT_EQ(0, memcmp(*fromTwoByteUtf8Val, utf8Data, 7 * sizeof(*utf8Data)));
  }

  {
    Local<String> fromUtf8Str =
      String::NewFromUtf8(engine.isolate(), reinterpret_cast<const char*>(utf8Data), NewStringType::kNormal).
        ToLocalChecked();
    EXPECT_EQ(5, fromUtf8Str->Length());
    EXPECT_EQ(10, fromUtf8Str->Utf8Length());
    String::Value fromUtf8Val(fromUtf8Str);
    String::Utf8Value fromUtf8Utf8Val(fromUtf8Str);
    EXPECT_EQ(0, memcmp(*fromUtf8Val, utf16Data, sizeof(utf16Data)));
    EXPECT_EQ(0, memcmp(*fromUtf8Utf8Val, utf8Data, sizeof(utf8Data)));
  }

  {
    Local<String> fromUtf8Str =
      String::NewFromUtf8(engine.isolate(), reinterpret_cast<const char*>(utf8Data), NewStringType::kNormal, 7).
        ToLocalChecked();
    EXPECT_EQ(4, fromUtf8Str->Length());
    EXPECT_EQ(7, fromUtf8Str->Utf8Length());
    String::Value fromUtf8Val(fromUtf8Str);
    String::Utf8Value fromUtf8Utf8Val(fromUtf8Str);
    EXPECT_EQ(0, memcmp(*fromUtf8Val, utf16Data, 4 * sizeof(*utf16Data)));
    EXPECT_EQ(0, memcmp(*fromUtf8Utf8Val, utf8Data, 7));
  }

  {
    Local<String> fromUtf8Str =
      String::NewFromUtf8(engine.isolate(), reinterpret_cast<const char*>(utf8Data), NewStringType::kInternalized, 7).
        ToLocalChecked();
    EXPECT_EQ(4, fromUtf8Str->Length());
    EXPECT_EQ(7, fromUtf8Str->Utf8Length());
    String::Value fromUtf8Val(fromUtf8Str);
    String::Utf8Value fromUtf8Utf8Val(fromUtf8Str);
    EXPECT_EQ(0, memcmp(*fromUtf8Val, utf16Data, 4 * sizeof(*utf16Data)));
    EXPECT_EQ(0, memcmp(*fromUtf8Utf8Val, utf8Data, 7));
  }

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

TEST(SpiderShim, StringNullChar) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<String> str1 =
      String::NewFromUtf8(context->GetIsolate(), "abc\0def",
                              NewStringType::kNormal, 7)
          .ToLocalChecked();
  EXPECT_EQ(7, str1->Length());
  Local<String> str2 =
      String::NewFromUtf8(context->GetIsolate(), "abc\0def",
                              NewStringType::kInternalized, 7)
          .ToLocalChecked();
  EXPECT_EQ(7, str2->Length());
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
  const int kStride = 4;  // Must match stride in for loops in JS below.
  engine.CompileRun(context,
      "var left = '';"
      "for (var i = 0; i < 0xd800; i += 4) {"
      "  left = left + String.fromCharCode(i);"
      "}");
  engine.CompileRun(context,
      "var right = '';"
      "for (var i = 0; i < 0xd800; i += 4) {"
      "  right = String.fromCharCode(i) + right;"
      "}");
  Local<Object> global = context->Global();
  Local<String> left_tree = global->Get(context, v8_str("left"))
                                .ToLocalChecked()
                                .As<String>();
  Local<String> right_tree = global->Get(context, v8_str("right"))
                                 .ToLocalChecked()
                                 .As<String>();

  CHECK_EQ(5, str2->Length());
  CHECK_EQ(0xd800 / kStride, left_tree->Length());
  CHECK_EQ(0xd800 / kStride, right_tree->Length());

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
  len = str2->WriteUtf8(utf8buf, -1, &charlen);
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
  // TODO: re-enable once upstream JS::DeflateStringToUTF8Buffer supports
  // writing orphan surrogates to the output buffer.
  // memset(utf8buf, 0x1, 1000);
  // len = orphans_str->WriteUtf8(utf8buf, sizeof(utf8buf), &charlen);
  // CHECK_EQ(13, len);
  // CHECK_EQ(8, charlen);
  // CHECK_EQ(0, strcmp(utf8buf, "ab\355\240\200cd\355\260\200ef"));

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

  memset(utf8buf, 0x1, sizeof(utf8buf));
  len = left_tree->Utf8Length();
  int utf8_expected =
      (0x80 + (0x800 - 0x80) * 2 + (0xd800 - 0x800) * 3) / kStride;
  CHECK_EQ(utf8_expected, len);
  len = left_tree->WriteUtf8(utf8buf, utf8_expected, &charlen);
  CHECK_EQ(utf8_expected, len);
  CHECK_EQ(0xd800 / kStride, charlen);
  CHECK_EQ(0xed, static_cast<unsigned char>(utf8buf[utf8_expected - 3]));
  CHECK_EQ(0x9f, static_cast<unsigned char>(utf8buf[utf8_expected - 2]));
  CHECK_EQ(0xc0 - kStride,
           static_cast<unsigned char>(utf8buf[utf8_expected - 1]));
  CHECK_EQ(1, utf8buf[utf8_expected]);

  memset(utf8buf, 0x1, sizeof(utf8buf));
  len = right_tree->Utf8Length();
  CHECK_EQ(utf8_expected, len);
  len = right_tree->WriteUtf8(utf8buf, utf8_expected, &charlen);
  CHECK_EQ(utf8_expected, len);
  CHECK_EQ(0xd800 / kStride, charlen);
  CHECK_EQ(0xed, static_cast<unsigned char>(utf8buf[0]));
  CHECK_EQ(0x9f, static_cast<unsigned char>(utf8buf[1]));
  CHECK_EQ(0xc0 - kStride, static_cast<unsigned char>(utf8buf[2]));
  CHECK_EQ(1, utf8buf[utf8_expected]);

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

TEST(SpiderShim, Utf16Symbol) {
  // This test is based on V8's Utf16Symbol test.

  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  {
    Local<String> symbol1 =
        v8::String::NewFromUtf8(engine.isolate(), "abc",
                                v8::NewStringType::kInternalized)
            .ToLocalChecked();
    Local<String> symbol2 =
        v8::String::NewFromUtf8(engine.isolate(), "abc",
                                v8::NewStringType::kInternalized)
            .ToLocalChecked();
    CHECK(SameSymbol(symbol1, symbol2));
  }

  {
    Local<String> symbol1 =
        v8::String::NewFromUtf8(engine.isolate(), "abc",
                                v8::NewStringType::kNormal)
            .ToLocalChecked();
    Local<String> symbol2 =
        v8::String::NewFromUtf8(engine.isolate(), "abc",
                                v8::NewStringType::kNormal)
            .ToLocalChecked();
    CHECK(!SameSymbol(symbol1, symbol2));
  }

  {
    Local<String> symbol1 =
        v8::String::NewFromOneByte(engine.isolate(),
                                   reinterpret_cast<const uint8_t*>("abc"),
                                   v8::NewStringType::kInternalized)
            .ToLocalChecked();
    Local<String> symbol2 =
        v8::String::NewFromOneByte(engine.isolate(),
                                   reinterpret_cast<const uint8_t*>("abc"),
                                   v8::NewStringType::kInternalized)
            .ToLocalChecked();
    CHECK(SameSymbol(symbol1, symbol2));
  }

  {
    Local<String> symbol1 =
        v8::String::NewFromTwoByte(engine.isolate(),
                                   reinterpret_cast<const uint16_t*>(u"abc"),
                                   v8::NewStringType::kInternalized)
            .ToLocalChecked();
    Local<String> symbol2 =
        v8::String::NewFromTwoByte(engine.isolate(),
                                   reinterpret_cast<const uint16_t*>(u"abc"),
                                   v8::NewStringType::kInternalized)
            .ToLocalChecked();
    CHECK(SameSymbol(symbol1, symbol2));
  }

  engine.CompileRun(context,
      "var sym0 = 'benedictus';"
      "var sym0b = 'S\303\270ren';"
      "var sym1 = '\355\240\201\355\260\207';"
      "var sym2 = '\360\220\220\210';"
      "var sym3 = 'x\355\240\201\355\260\207';"
      "var sym4 = 'x\360\220\220\210';"
      "if (sym1.length != 2) throw sym1;"
      "if (sym1.charCodeAt(1) != 0xdc07) throw sym1.charCodeAt(1);"
      "if (sym2.length != 2) throw sym2;"
      "if (sym2.charCodeAt(1) != 0xdc08) throw sym2.charCodeAt(2);"
      "if (sym3.length != 3) throw sym3;"
      "if (sym3.charCodeAt(2) != 0xdc07) throw sym1.charCodeAt(2);"
      "if (sym4.length != 3) throw sym4;"
      "if (sym4.charCodeAt(2) != 0xdc08) throw sym2.charCodeAt(2);"
  );
  Local<String> sym0 =
      v8::String::NewFromUtf8(engine.isolate(), "benedictus",
                              v8::NewStringType::kInternalized)
          .ToLocalChecked();
  Local<String> sym0b =
      v8::String::NewFromUtf8(engine.isolate(), "S\303\270ren",
                              v8::NewStringType::kInternalized)
          .ToLocalChecked();
  Local<String> sym1 =
      v8::String::NewFromUtf8(engine.isolate(), "\355\240\201\355\260\207",
                              v8::NewStringType::kInternalized)
          .ToLocalChecked();
  Local<String> sym2 =
      v8::String::NewFromUtf8(engine.isolate(), "\360\220\220\210",
                              v8::NewStringType::kInternalized)
          .ToLocalChecked();
  Local<String> sym3 = v8::String::NewFromUtf8(engine.isolate(),
                                               "x\355\240\201\355\260\207",
                                               v8::NewStringType::kInternalized)
                           .ToLocalChecked();
  Local<String> sym4 =
      v8::String::NewFromUtf8(engine.isolate(), "x\360\220\220\210",
                              v8::NewStringType::kInternalized)
          .ToLocalChecked();
  v8::Local<v8::Object> global = context->Global();
  Local<Value> s0 =
      global->Get(context, v8_str("sym0")).ToLocalChecked();
  Local<Value> s0b =
      global->Get(context, v8_str("sym0b")).ToLocalChecked();
  Local<Value> s1 =
      global->Get(context, v8_str("sym1")).ToLocalChecked();
  Local<Value> s2 =
      global->Get(context, v8_str("sym2")).ToLocalChecked();
  Local<Value> s3 =
      global->Get(context, v8_str("sym3")).ToLocalChecked();
  Local<Value> s4 =
      global->Get(context, v8_str("sym4")).ToLocalChecked();
  CHECK(SameSymbol(sym0, Local<String>::Cast(s0)));
  CHECK(SameSymbol(sym0b, Local<String>::Cast(s0b)));
  CHECK(SameSymbol(sym1, Local<String>::Cast(s1)));
  CHECK(SameSymbol(sym2, Local<String>::Cast(s2)));
  CHECK(SameSymbol(sym3, Local<String>::Cast(s3)));
  CHECK(SameSymbol(sym4, Local<String>::Cast(s4)));
}

// From vector.h.
inline int StrLength(const char* string) {
  size_t length = strlen(string);
  // DCHECK(length == static_cast<size_t>(static_cast<int>(length)));
  return static_cast<int>(length);
}

// From cctest.h.
static inline uint16_t* AsciiToTwoByteString(const char* source) {
  int array_length = StrLength(source) + 1;
  uint16_t* converted = NewArray<uint16_t>(array_length);
  for (int i = 0; i < array_length; i++) converted[i] = source[i];
  return converted;
}

// From test-api.cc.
class TestResource: public String::ExternalStringResource {
 public:
  explicit TestResource(uint16_t* data, int* counter = NULL,
                        bool owning_data = true)
      : data_(data), length_(0), counter_(counter), owning_data_(owning_data) {
    while (data[length_]) ++length_;
  }

  ~TestResource() {
    if (owning_data_) DeleteArray(data_);
    if (counter_ != NULL) ++*counter_;
  }

  const uint16_t* data() const {
    return data_;
  }

  size_t length() const {
    return length_;
  }

 private:
  uint16_t* data_;
  size_t length_;
  int* counter_;
  bool owning_data_;
};

// From test-api.cc.
class TestOneByteResource : public String::ExternalOneByteStringResource {
 public:
  explicit TestOneByteResource(const char* data, int* counter = NULL,
                               size_t offset = 0)
      : orig_data_(data),
        data_(data + offset),
        length_(strlen(data) - offset),
        counter_(counter) {}

  ~TestOneByteResource() {
    free(const_cast<char*>(orig_data_));
    if (counter_ != NULL) ++*counter_;
  }

  const char* data() const {
    return data_;
  }

  size_t length() const {
    return length_;
  }

 private:
  const char* orig_data_;
  const char* data_;
  size_t length_;
  int* counter_;
};

TEST(SpiderShim, NewStringRangeError) {
  // This test is based on V8's NewStringRangeError test.

  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  const int length = String::kMaxLength + 1;
  const int buffer_size = length * sizeof(uint16_t);
  void* buffer = malloc(buffer_size);
  if (buffer == NULL) return;
  memset(buffer, 'A', buffer_size - 1);
  Isolate* isolate = engine.isolate();
  {
    TryCatch try_catch(isolate);
    char* data = reinterpret_cast<char*>(buffer);
    CHECK(String::NewFromUtf8(isolate, data, NewStringType::kNormal,length)
            .IsEmpty());
    CHECK(!try_catch.HasCaught());
  }
  {
    TryCatch try_catch(isolate);
    uint8_t* data = reinterpret_cast<uint8_t*>(buffer);
    CHECK(String::NewFromOneByte(isolate, data, NewStringType::kNormal, length)
            .IsEmpty());
    CHECK(!try_catch.HasCaught());
  }
  {
    TryCatch try_catch(isolate);
    uint16_t* data = reinterpret_cast<uint16_t*>(buffer);
    CHECK(String::NewFromTwoByte(isolate, data, NewStringType::kNormal, length)
            .IsEmpty());
    CHECK(!try_catch.HasCaught());
  }
  {
    TryCatch try_catch(isolate);
    uint16_t* data = reinterpret_cast<uint16_t*>(buffer);
    // Satisfy JSExternalString::new_ constraint that data is null-terminated.
    data[buffer_size/sizeof(uint16_t) - 1] = '\0';
    TestExternalStringResource* testResource =
      new TestExternalStringResource(data, length);
    CHECK(String::NewExternalTwoByte(isolate, testResource).IsEmpty());
    CHECK(!try_catch.HasCaught());
    delete testResource;
  }
  {
    TryCatch try_catch(isolate);
    char* data = reinterpret_cast<char*>(buffer);
    // Satisfy JSExternalString::new_ constraint that data is null-terminated.
    data[buffer_size/sizeof(char) - 1] = '\0';
    TestExternalOneByteStringResource* testResource =
      new TestExternalOneByteStringResource(data, length);
    CHECK(String::NewExternalOneByte(isolate, testResource).IsEmpty());
    CHECK(!try_catch.HasCaught());
    delete testResource;
  }
  free(buffer);
}

TEST(SpiderShim, StringConcatOverflow) {
  // This test is based on V8's StringConcatOverflow test.

  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  RandomLengthOneByteResource* r =
    new RandomLengthOneByteResource(String::kMaxLength);
  Local<String> str =
    String::NewExternalOneByte(engine.isolate(), r).ToLocalChecked();
  CHECK(!str.IsEmpty());
  TryCatch try_catch(engine.isolate());
  Local<String> result = String::Concat(str, str);
  CHECK(result.IsEmpty());
  CHECK(!try_catch.HasCaught());
}

TEST(SpiderShim, ScriptUsingStringResource) {
  // This test is based on V8's ScriptUsingStringResource test.

  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  int dispose_count = 0;
  const char* c_source = "1 + 2 * 3";
  uint16_t* two_byte_source = AsciiToTwoByteString(c_source);
  {
    TestResource* resource = new TestResource(two_byte_source, &dispose_count);
    Local<String> source =
        String::NewExternalTwoByte(engine.isolate(), resource)
            .ToLocalChecked();
    Local<Value> value = engine.CompileRun(source);
    CHECK(value->IsNumber());
    CHECK_EQ(7, value->Int32Value(context).FromJust());
    // CHECK(source->IsExternal());
    // CHECK_EQ(resource,
    //          static_cast<TestResource*>(source->GetExternalStringResource()));
    // String::Encoding encoding = String::UNKNOWN_ENCODING;
    // CHECK_EQ(static_cast<const String::ExternalStringResourceBase*>(resource),
    //          source->GetExternalStringResourceBase(&encoding));
    // CHECK_EQ(String::TWO_BYTE_ENCODING, encoding);
    // CcTest::heap()->CollectAllGarbage();
    // CHECK_EQ(0, dispose_count);
  }
  // CcTest::i_isolate()->compilation_cache()->Clear();
  // CcTest::heap()->CollectAllAvailableGarbage();
  // CHECK_EQ(1, dispose_count);
}

TEST(SpiderShim, ScriptUsingOneByteStringResource) {
  // This test is based on V8's ScriptUsingOneByteStringResource test.

  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  int dispose_count = 0;
  const char* c_source = "1 + 2 * 3";
  {
    TestOneByteResource* resource =
        new TestOneByteResource(strdup(c_source), &dispose_count);
    Local<String> source =
        String::NewExternalOneByte(engine.isolate(), resource)
            .ToLocalChecked();
    // CHECK(source->IsExternalOneByte());
    // CHECK_EQ(static_cast<const String::ExternalStringResourceBase*>(resource),
    //          source->GetExternalOneByteStringResource());
    // String::Encoding encoding = String::UNKNOWN_ENCODING;
    // CHECK_EQ(static_cast<const String::ExternalStringResourceBase*>(resource),
    //          source->GetExternalStringResourceBase(&encoding));
    // CHECK_EQ(String::ONE_BYTE_ENCODING, encoding);
    Local<Value> value = engine.CompileRun(source);
    CHECK(value->IsNumber());
    CHECK_EQ(7, value->Int32Value(context).FromJust());
    // CcTest::heap()->CollectAllGarbage();
    // CHECK_EQ(0, dispose_count);
  }
  // CcTest::i_isolate()->compilation_cache()->Clear();
  // CcTest::heap()->CollectAllAvailableGarbage();
  // CHECK_EQ(1, dispose_count);
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

  {
    Local<Value> name = Function::Cast(*normal)->GetName();
    String::Utf8Value utf8(name->ToString());
    EXPECT_STREQ("normal", *utf8);
    Function::Cast(*normal)->SetName(v8_str("newName"));
    name = Function::Cast(*normal)->GetName();
    String::Utf8Value utf8_2(name->ToString());
    EXPECT_STREQ("newName", *utf8_2);
  }

  {
    Local<Value> name = Function::Cast(*gen)->GetName();
    String::Utf8Value utf8(name->ToString());
    EXPECT_STREQ("gen", *utf8);
    Function::Cast(*gen)->SetName(v8_str("newName"));
    name = Function::Cast(*gen)->GetName();
    String::Utf8Value utf8_2(name->ToString());
    EXPECT_STREQ("newName", *utf8_2);
  }
}

static Local<Value> function_new_expected_env;
static void FunctionNewCallback(const v8::FunctionCallbackInfo<Value>& info) {
  CHECK(
      function_new_expected_env->Equals(info.GetIsolate()->GetCurrentContext(),
                                        info.Data())
          .FromJust());
  info.GetReturnValue().Set(17);
}

static void FunctionNewCallback2(const v8::FunctionCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(info.Length() + info[1]->ToInt32()->Value());
}

static void FunctionNewCallback3(const v8::FunctionCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(Local<Value>());
}

TEST(SpiderShim, FunctionNew) {
  // This test is adopted from the V8 FunctionNew test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<Object> data = v8::Object::New(isolate);
  function_new_expected_env = data;
  Local<Function> func =
      Function::New(context, FunctionNewCallback, data).ToLocalChecked();
  CHECK(context->Global()->Set(context, v8_str("func"), func).FromJust());
  Local<Value> result = engine.CompileRun(context, "func();");
  CHECK(v8::Integer::New(isolate, 17)->Equals(context, result).FromJust());
  // Verify that each Function::New creates a new function instance
  Local<Object> data2 = v8::Object::New(isolate);
  function_new_expected_env = data2;
  Local<Function> func2 =
      Function::New(context, FunctionNewCallback, data2).ToLocalChecked();
  CHECK(!func2->IsNull());
  CHECK(!func->Equals(context, func2).FromJust());
  CHECK(context->Global()->Set(context, v8_str("func2"), func2).FromJust());
  Local<Value> result2 = engine.CompileRun(context, "func2();");
  CHECK(v8::Integer::New(isolate, 17)->Equals(context, result2).FromJust());
  Local<Function> func3 =
    Function::New(context, FunctionNewCallback2).ToLocalChecked();
  CHECK(!func3->IsNull());
  CHECK(!func3->Equals(context, func).FromJust());
  CHECK(!func3->Equals(context, func2).FromJust());
  CHECK(context->Global()->Set(context, v8_str("func3"), func3).FromJust());
  Local<Value> result3 = engine.CompileRun(context, "func3(1, 14, 4);");
  CHECK(v8::Integer::New(isolate, 17)->Equals(context, result3).FromJust());
  Local<Function> func4 =
    Function::New(context, FunctionNewCallback3).ToLocalChecked();
  CHECK(!func4->IsNull());
  CHECK(!func4->Equals(context, func).FromJust());
  CHECK(!func4->Equals(context, func2).FromJust());
  CHECK(!func4->Equals(context, func3).FromJust());
  CHECK(context->Global()->Set(context, v8_str("func4"), func4).FromJust());
  Local<Value> result4 = engine.CompileRun(context, "func4();");
  CHECK(result4->IsUndefined());
}

static void CallbackReturnNewHandle(const v8::FunctionCallbackInfo<Value>& info) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  Local<Object> obj = Object::New(isolate);
  obj->Set(v8_str("foo"), Integer::New(isolate, 42));
  info.GetReturnValue().Set(obj);
}

TEST(SpiderShim, ReturnNewHandle) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<Object> data = v8::Object::New(isolate);
  function_new_expected_env = data;
  Local<Function> func =
      Function::New(context, CallbackReturnNewHandle, data).ToLocalChecked();
  CHECK(context->Global()->Set(context, v8_str("f"), func).FromJust());
  Local<Value> result = engine.CompileRun(context, "f().foo");
  CHECK(v8::Integer::New(isolate, 42)->Equals(context, result).FromJust());
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

  Local<Value>* args0 = NULL;
  Local<Array> a0 = Local<Array>::Cast(
      Foo->NewInstance(context, 0, args0).ToLocalChecked());
  CHECK_EQ(0u, a0->Length());

  Local<Value> args1[] = {v8_num(1.1)};
  Local<Array> a1 = Local<Array>::Cast(
      Foo->NewInstance(context, 1, args1).ToLocalChecked());
  CHECK_EQ(1u, a1->Length());
  CHECK_EQ(1.1, a1->Get(context, Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  Local<Value> args2[] = {v8_num(2.2), v8_num(3.3)};
  Local<Array> a2 = Local<Array>::Cast(
      Foo->NewInstance(context, 2, args2).ToLocalChecked());
  CHECK_EQ(2u, a2->Length());
  CHECK_EQ(2.2, a2->Get(context, Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(3.3, a2->Get(context, Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  Local<Value> args3[] = {v8_num(4.4), v8_num(5.5), v8_num(6.6)};
  Local<Array> a3 = Local<Array>::Cast(
      Foo->NewInstance(context, 3, args3).ToLocalChecked());
  CHECK_EQ(3u, a3->Length());
  CHECK_EQ(4.4, a3->Get(context, Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(5.5, a3->Get(context, Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(6.6, a3->Get(context, Integer::New(isolate, 2))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());

  Local<Value> args4[] = {v8_num(7.7), v8_num(8.8), v8_num(9.9),
                              v8_num(10.11)};
  Local<Array> a4 = Local<Array>::Cast(
      Foo->NewInstance(context, 4, args4).ToLocalChecked());
  CHECK_EQ(4u, a4->Length());
  CHECK_EQ(7.7, a4->Get(context, Integer::New(isolate, 0))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(8.8, a4->Get(context, Integer::New(isolate, 1))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(9.9, a4->Get(context, Integer::New(isolate, 2))
                    .ToLocalChecked()
                    ->NumberValue(context)
                    .FromJust());
  CHECK_EQ(10.11, a4->Get(context, Integer::New(isolate, 3))
                      .ToLocalChecked()
                      ->NumberValue(context)
                      .FromJust());
}

TEST(SpiderShim, ConstructorForObject) {
  // This is based on the V8 ConstructorForObject test, rewritten to not use ObjectTemplate.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  {
    engine.CompileRun(context,
        "function obj(arg) {"
        "  var ret;"
        "  if (new.target) {"
        "    ret = new Object();"
        "    Object.setPrototypeOf(ret, obj.prototype);"
        "  } else {"
        "    ret = this;"
        "  }"
        "  ret.a = arg;"
        "  return ret;"
        "}");
    Local<Object> instance = Local<Object>::Cast(
        context->Global()->Get(context, v8_str("obj")).ToLocalChecked());

    v8::TryCatch try_catch(isolate);
    Local<Value> value;
    CHECK(!try_catch.HasCaught());

    // Call the Object's constructor with a 32-bit signed integer.
    value = engine.CompileRun(context,
        "(function() { var o = new obj(28); return o.a; })()");
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsInt32());
    CHECK_EQ(28, value->Int32Value(context).FromJust());

    Local<Value> args1[] = {v8_num(28)};
    Local<Value> value_obj1 =
        instance->CallAsConstructor(context, 1, args1).ToLocalChecked();
    CHECK(value_obj1->IsObject());
    Local<Object> object1 = Local<Object>::Cast(value_obj1);
    value = object1->Get(context, v8_str("a")).ToLocalChecked();
    CHECK(value->IsInt32());
    CHECK(!try_catch.HasCaught());
    CHECK_EQ(28, value->Int32Value(context).FromJust());

    // Call the Object's constructor with a String.
    value = engine.CompileRun(context,
        "(function() { var o = new obj('tipli'); return o.a; })()");
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsString());
    String::Utf8Value string_value1(
        value->ToString(context).ToLocalChecked());
    CHECK_EQ(0, strcmp("tipli", *string_value1));

    Local<Value> args2[] = {v8_str("tipli")};
    Local<Value> value_obj2 =
        instance->CallAsConstructor(context, 1, args2).ToLocalChecked();
    CHECK(value_obj2->IsObject());
    Local<Object> object2 = Local<Object>::Cast(value_obj2);
    value = object2->Get(context, v8_str("a")).ToLocalChecked();
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsString());
    String::Utf8Value string_value2(
        value->ToString(context).ToLocalChecked());
    CHECK_EQ(0, strcmp("tipli", *string_value2));

    // Call the Object's constructor with a Boolean.
    value = engine.CompileRun(context,
        "(function() { var o = new obj(true); return o.a; })()");
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsBoolean());
    CHECK_EQ(true, value->BooleanValue(context).FromJust());

    Local<Value> args3[] = {v8::True(isolate)};
    Local<Value> value_obj3 =
        instance->CallAsConstructor(context, 1, args3).ToLocalChecked();
    CHECK(value_obj3->IsObject());
    Local<Object> object3 = Local<Object>::Cast(value_obj3);
    value = object3->Get(context, v8_str("a")).ToLocalChecked();
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsBoolean());
    CHECK_EQ(true, value->BooleanValue(context).FromJust());

    // Call the Object's constructor with undefined.
    Local<Value> args4[] = {v8::Undefined(isolate)};
    Local<Value> value_obj4 =
        instance->CallAsConstructor(context, 1, args4).ToLocalChecked();
    CHECK(value_obj4->IsObject());
    Local<Object> object4 = Local<Object>::Cast(value_obj4);
    value = object4->Get(context, v8_str("a")).ToLocalChecked();
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsUndefined());

    // Call the Object's constructor with null.
    Local<Value> args5[] = {v8::Null(isolate)};
    Local<Value> value_obj5 =
        instance->CallAsConstructor(context, 1, args5).ToLocalChecked();
    CHECK(value_obj5->IsObject());
    Local<Object> object5 = Local<Object>::Cast(value_obj5);
    value = object5->Get(context, v8_str("a")).ToLocalChecked();
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsNull());
  }

  // Check exception handling when there is no constructor set for the Object.
  {
    engine.CompileRun(context, "obj = new Object();");
    Local<Object> instance = Local<Object>::Cast(
        context->Global()->Get(context, v8_str("obj")).ToLocalChecked());

    CHECK(context->Global()
              ->Set(context, v8_str("obj2"), instance)
              .FromJust());
    v8::TryCatch try_catch(isolate);
    Local<Value> value;
    CHECK(!try_catch.HasCaught());

    value = engine.CompileRun(context, "new obj2(28)");
    CHECK(try_catch.HasCaught());
    String::Utf8Value exception_value1(try_catch.Exception());
    EXPECT_STREQ("TypeError: obj2 is not a constructor", *exception_value1);
    try_catch.Reset();

    Local<Value> args[] = {v8_num(29)};
    CHECK(instance->CallAsConstructor(context, 1, args).IsEmpty());
    CHECK(try_catch.HasCaught());
    String::Utf8Value exception_value2(try_catch.Exception());
    EXPECT_STREQ("TypeError: ({}) is not a constructor", *exception_value2);
    try_catch.Reset();
  }

  // Check the case when constructor throws exception.
  {
    engine.CompileRun(context,
        "function obj(arg) {"
        "  throw arg;"
        "}");
    Local<Object> instance = Local<Object>::Cast(
        context->Global()->Get(context, v8_str("obj")).ToLocalChecked());

    CHECK(context->Global()
              ->Set(context, v8_str("obj3"), instance)
              .FromJust());
    v8::TryCatch try_catch(isolate);
    Local<Value> value;
    CHECK(!try_catch.HasCaught());

    value = engine.CompileRun(context, "new obj3(22)");
    CHECK(try_catch.HasCaught());
    String::Utf8Value exception_value1(try_catch.Exception());
    CHECK_EQ(0, strcmp("22", *exception_value1));
    try_catch.Reset();

    Local<Value> args[] = {v8_num(23)};
    CHECK(instance->CallAsConstructor(context, 1, args).IsEmpty());
    CHECK(try_catch.HasCaught());
    String::Utf8Value exception_value2(try_catch.Exception());
    CHECK_EQ(0, strcmp("23", *exception_value2));
    try_catch.Reset();
  }

  // Check whether constructor returns with an object or non-object.
  {
    engine.CompileRun(context,
        "function obj(arg) {"
        "  return arg;"
        "}");
    Local<Object> instance1 = Local<Object>::Cast(
        context->Global()->Get(context, v8_str("obj")).ToLocalChecked());

    CHECK(context->Global()
              ->Set(context, v8_str("obj4"), instance1)
              .FromJust());
    v8::TryCatch try_catch(isolate);
    Local<Value> value;
    CHECK(!try_catch.HasCaught());

    CHECK(instance1->IsObject());
    CHECK(instance1->IsFunction());

    value = engine.CompileRun(context, "new obj4(28)");
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsObject());

    Local<Value> args1[] = {v8_num(28)};
    value = instance1->CallAsConstructor(context, 1, args1)
                .ToLocalChecked();
    CHECK(!try_catch.HasCaught());
    CHECK(value->IsObject());
  }
}

TEST(SpiderShim, Iterator) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Value> mapIterator = engine.CompileRun(context,
      "new Map([['key1', 'value1'], ['key2', 'value2']]).entries()");
  EXPECT_TRUE(mapIterator->IsMapIterator());
  Local<Value> setIterator = engine.CompileRun(context,
      "new Set(['value1', 'value2']).entries()");
  EXPECT_TRUE(setIterator->IsSetIterator());
}

TEST(SpiderShim, HiddenProperties) {
  // This test is adopted from the V8 HiddenProperties test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<Object> obj = Object::New(isolate);
  Local<Private> key =
      Private::ForApi(isolate, v8_str("api-test::hidden-key"));
  Local<String> empty = v8_str("");
  Local<String> prop_name = v8_str("prop_name");

  // Make sure delete of a non-existent hidden value works
  obj->DeletePrivate(context, key).FromJust();

  EXPECT_TRUE(obj->SetPrivate(context, key, Integer::New(isolate, 1503))
            .FromJust());
  EXPECT_EQ(1503, obj->GetPrivate(context, key)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_TRUE(obj->SetPrivate(context, key, Integer::New(isolate, 2002))
            .FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, key)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());

  // Make sure we do not find the hidden property.
  EXPECT_TRUE(!obj->Has(context, empty).FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, key)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_TRUE(obj->Get(context, empty).ToLocalChecked()->IsUndefined());
  EXPECT_EQ(2002, obj->GetPrivate(context, key)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_TRUE(
      obj->Set(context, empty, Integer::New(isolate, 2003)).FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, key)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_EQ(2003, obj->Get(context, empty)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());

  // Add another property and delete it afterwards to force the object in
  // slow case.
  EXPECT_TRUE(obj->Set(context, prop_name, Integer::New(isolate, 2008))
            .FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, key)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_EQ(2008, obj->Get(context, prop_name)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, key)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_TRUE(obj->Delete(context, prop_name).FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, key)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());

  EXPECT_TRUE(obj->SetPrivate(context, key, Integer::New(isolate, 2002))
            .FromJust());
  EXPECT_TRUE(obj->DeletePrivate(context, key).FromJust());
  EXPECT_TRUE(!obj->HasPrivate(context, key).FromJust());
  EXPECT_TRUE(obj->GetPrivate(context, key).ToLocalChecked()->IsUndefined());
}

TEST(SpiderShim, PrivateProperties) {
  // This test is adopted from the V8 PrivateProperties test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<Object> obj = Object::New(isolate);
  Local<Private> priv1 = Private::New(isolate);
  Local<Private> priv2 =
      Private::New(isolate, v8_str("my-private"));

  EXPECT_TRUE(priv2->Name()
            ->Equals(context,
                     String::NewFromUtf8(isolate, "my-private",
                                             NewStringType::kNormal)
                         .ToLocalChecked())
            .FromJust());

  // Make sure delete of a non-existent private symbol property works.
  obj->DeletePrivate(context, priv1).FromJust();
  EXPECT_TRUE(!obj->HasPrivate(context, priv1).FromJust());

  EXPECT_TRUE(obj->SetPrivate(context, priv1, Integer::New(isolate, 1503))
            .FromJust());
  EXPECT_TRUE(obj->HasPrivate(context, priv1).FromJust());
  EXPECT_EQ(1503, obj->GetPrivate(context, priv1)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_TRUE(obj->SetPrivate(context, priv1, Integer::New(isolate, 2002))
            .FromJust());
  EXPECT_TRUE(obj->HasPrivate(context, priv1).FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, priv1)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());

  EXPECT_EQ(0u,
           obj->GetOwnPropertyNames(context).ToLocalChecked()->Length());
  unsigned num_props =
      obj->GetPropertyNames(context).ToLocalChecked()->Length();
  EXPECT_TRUE(obj->Set(context, String::NewFromUtf8(
                                  isolate, "bla", NewStringType::kNormal)
                                  .ToLocalChecked(),
                 Integer::New(isolate, 20))
            .FromJust());
  EXPECT_EQ(1u,
           obj->GetOwnPropertyNames(context).ToLocalChecked()->Length());
  EXPECT_EQ(num_props + 1,
           obj->GetPropertyNames(context).ToLocalChecked()->Length());

  // Add another property and delete it afterwards to force the object in
  // slow case.
  EXPECT_TRUE(obj->SetPrivate(context, priv2, Integer::New(isolate, 2008))
            .FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, priv1)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_EQ(2008, obj->GetPrivate(context, priv2)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, priv1)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_EQ(1u,
           obj->GetOwnPropertyNames(context).ToLocalChecked()->Length());

  EXPECT_TRUE(obj->HasPrivate(context, priv1).FromJust());
  EXPECT_TRUE(obj->HasPrivate(context, priv2).FromJust());
  EXPECT_TRUE(obj->DeletePrivate(context, priv2).FromJust());
  EXPECT_TRUE(obj->HasPrivate(context, priv1).FromJust());
  EXPECT_TRUE(!obj->HasPrivate(context, priv2).FromJust());
  EXPECT_EQ(2002, obj->GetPrivate(context, priv1)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  EXPECT_EQ(1u,
           obj->GetOwnPropertyNames(context).ToLocalChecked()->Length());

  // Private properties are not inherited (for the time being).
  Local<Object> child = Object::New(isolate);
  EXPECT_TRUE(child->SetPrototype(context, obj).FromJust());
  EXPECT_TRUE(!child->HasPrivate(context, priv1).FromJust());
  EXPECT_EQ(0u,
           child->GetOwnPropertyNames(context).ToLocalChecked()->Length());
}

TEST(SpiderShim, ObjectGetConstructorName) {
  // This test is based on V8's ObjectGetConstructorName test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  engine.CompileRun(context,
      "function Parent() {};"
      "function Child() {};"
      "Child.prototype = new Parent();"
      "Child.prototype.constructor = Child;"
      "var outer = { inner: function() { } };"
      "var p = new Parent();"
      "var c = new Child();"
      "var x = new outer.inner();"
      "var proto = Child.prototype;");

  Local<Value> p =
      context->Global()->Get(context, v8_str("p")).ToLocalChecked();
  EXPECT_TRUE(p->IsObject() &&
        p->ToObject(context)
            .ToLocalChecked()
            ->GetConstructorName()
            ->Equals(context, v8_str("Parent"))
            .FromJust());

  Local<Value> c =
      context->Global()->Get(context, v8_str("c")).ToLocalChecked();
  EXPECT_TRUE(c->IsObject() &&
        c->ToObject(context)
            .ToLocalChecked()
            ->GetConstructorName()
            ->Equals(context, v8_str("Child"))
            .FromJust());

  Local<Value> x =
      context->Global()->Get(context, v8_str("x")).ToLocalChecked();
  EXPECT_TRUE(x->IsObject() &&
        x->ToObject(context)
            .ToLocalChecked()
            ->GetConstructorName()
            ->Equals(context, v8_str("inner"))
            .FromJust());

  Local<Value> child_prototype =
      context->Global()->Get(context, v8_str("proto")).ToLocalChecked();
  EXPECT_TRUE(child_prototype->IsObject() &&
        child_prototype->ToObject(context)
            .ToLocalChecked()
            ->GetConstructorName()
            ->Equals(context, v8_str("Parent"))
            .FromJust());
}

TEST(SpiderShim, SubclassGetConstructorName) {
  // This test is based on V8's SubclassGetConstructorName test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  engine.CompileRun(context,
      "\"use strict\";"
      "class Parent {}"
      "class Child extends Parent {}"
      "var p = new Parent();"
      "var c = new Child();");

  Local<Value> p =
      context->Global()->Get(context, v8_str("p")).ToLocalChecked();
  EXPECT_TRUE(p->IsObject() &&
        p->ToObject(context)
            .ToLocalChecked()
            ->GetConstructorName()
            ->Equals(context, v8_str("Parent"))
            .FromJust());

  Local<Value> c =
      context->Global()->Get(context, v8_str("c")).ToLocalChecked();
  EXPECT_TRUE(c->IsObject() &&
        c->ToObject(context)
            .ToLocalChecked()
            ->GetConstructorName()
            ->Equals(context, v8_str("Child"))
            .FromJust());
}

TEST(SpiderShim, External) {
  // This test is based on V8's External test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  int x = 3;
  Local<External> ext = External::New(engine.isolate(), &x);
  EXPECT_TRUE(context->Global()->Set(context, v8_str("ext"), ext).FromJust());
  Local<Value> reext_obj = engine.CompileRun(context, "this.ext");
  EXPECT_TRUE(reext_obj->IsExternal());
  EXPECT_TRUE(External::IsExternal(*reext_obj));
  Local<External> reext = reext_obj.As<External>();
  int* ptr = static_cast<int*>(reext->Value());
  EXPECT_EQ(x, 3);
  *ptr = 10;
  EXPECT_EQ(x, 10);

  // Make sure unaligned pointers are wrapped properly.
  char* data = strdup("0123456789");
  Local<Value> zero = External::New(engine.isolate(), &data[0]);
  Local<Value> one = External::New(engine.isolate(), &data[1]);
  Local<Value> two = External::New(engine.isolate(), &data[2]);
  Local<Value> three = External::Wrap(&data[3]);

  char* char_ptr = reinterpret_cast<char*>(External::Cast(*zero)->Value());
  EXPECT_EQ('0', *char_ptr);
  char_ptr = reinterpret_cast<char*>(External::Cast(*one)->Value());
  EXPECT_EQ('1', *char_ptr);
  char_ptr = reinterpret_cast<char*>(External::Unwrap(two));
  EXPECT_EQ('2', *char_ptr);
  char_ptr = reinterpret_cast<char*>(External::Cast(*three)->Value());
  EXPECT_EQ('3', *char_ptr);
  free(data);
}

static void CheckEmbedderData(Local<Context>* env, int index,
                              Local<Value> data) {
  (*env)->SetEmbedderData(index, data);
  EXPECT_TRUE((*env)->GetEmbedderData(index)->StrictEquals(data));
}

TEST(SpiderShim, EmbedderData) {
  // This test is based on V8's EmbedderData test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Isolate* isolate = context->GetIsolate();

  CheckEmbedderData(&context, 3, v8_str("The quick brown fox jumps"));
  CheckEmbedderData(&context, 2, v8_str("over the lazy dog."));
  CheckEmbedderData(&context, 1, Number::New(isolate, 1.2345));
  CheckEmbedderData(&context, 0, Boolean::New(isolate, true));
}

static void CheckAlignedPointerInEmbedderData(Local<Context>* env, int index,
                                              void* value) {
  EXPECT_EQ(0, static_cast<int>(reinterpret_cast<uintptr_t>(value) & 0x1));
  (*env)->SetAlignedPointerInEmbedderData(index, value);
  EXPECT_EQ(value, (*env)->GetAlignedPointerFromEmbedderData(index));
}

static void* AlignedTestPointer(int i) {
  return reinterpret_cast<void*>(i * 1234);
}

TEST(SpiderShim, EmbedderDataAlignedPointers) {
  // This test is based on V8's EmbedderDataAlignedPointers test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  CheckAlignedPointerInEmbedderData(&context, 0, NULL);

  int* heap_allocated = new int[100];
  CheckAlignedPointerInEmbedderData(&context, 1, heap_allocated);
  delete[] heap_allocated;

  int stack_allocated[100];
  CheckAlignedPointerInEmbedderData(&context, 2, stack_allocated);

  void* huge = reinterpret_cast<void*>(~static_cast<uintptr_t>(1));
  CheckAlignedPointerInEmbedderData(&context, 3, huge);

  // Test growing of the embedder data's backing store.
  for (int i = 0; i < 100; i++) {
    context->SetAlignedPointerInEmbedderData(i, AlignedTestPointer(i));
  }

  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(AlignedTestPointer(i), context->GetAlignedPointerFromEmbedderData(i));
  }
}

void HandleF(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::EscapableHandleScope scope(args.GetIsolate());
  Local<v8::Array> result = v8::Array::New(args.GetIsolate(), args.Length());
  for (int i = 0; i < args.Length(); i++) {
    CHECK(result->Set(Isolate::GetCurrent()->GetCurrentContext(), i, args[i])
              .FromJust());
  }
  args.GetReturnValue().Set(scope.Escape(result));
}

TEST(SpiderShim, Vector) {
  // This test is based on V8's Vector test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<Object> global = context->Global();
  Local<FunctionTemplate> templ = FunctionTemplate::New(isolate, HandleF);
  global->Set(v8_str("f"), templ->GetFunction());

  const char* fun = "f()";
  Local<v8::Array> a0 = CompileRun(fun).As<v8::Array>();
  EXPECT_EQ(0u, a0->Length());

  const char* fun2 = "f(11)";
  Local<v8::Array> a1 = CompileRun(fun2).As<v8::Array>();
  EXPECT_EQ(1u, a1->Length());
  EXPECT_EQ(11, a1->Get(context, 0)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());

  const char* fun3 = "f(12, 13)";
  Local<v8::Array> a2 = CompileRun(fun3).As<v8::Array>();
  EXPECT_EQ(2u, a2->Length());
  EXPECT_EQ(12, a2->Get(context, 0)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());
  EXPECT_EQ(13, a2->Get(context, 1)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());

  const char* fun4 = "f(14, 15, 16)";
  Local<v8::Array> a3 = CompileRun(fun4).As<v8::Array>();
  EXPECT_EQ(3u, a3->Length());
  EXPECT_EQ(14, a3->Get(context, 0)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());
  EXPECT_EQ(15, a3->Get(context, 1)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());
  EXPECT_EQ(16, a3->Get(context, 2)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());

  const char* fun5 = "f(17, 18, 19, 20)";
  Local<v8::Array> a4 = CompileRun(fun5).As<v8::Array>();
  EXPECT_EQ(4u, a4->Length());
  EXPECT_EQ(17, a4->Get(context, 0)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());
  EXPECT_EQ(18, a4->Get(context, 1)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());
  EXPECT_EQ(19, a4->Get(context, 2)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());
  EXPECT_EQ(20, a4->Get(context, 3)
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust());
}

TEST(SpiderShim, PropertyDescriptor) {
  // This test is based on V8's PropertyDescriptor test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  {  // empty descriptor
    v8::PropertyDescriptor desc;
    EXPECT_TRUE(!desc.has_value());
    EXPECT_TRUE(!desc.has_set());
    EXPECT_TRUE(!desc.has_get());
    EXPECT_TRUE(!desc.has_enumerable());
    EXPECT_TRUE(!desc.has_configurable());
    EXPECT_TRUE(!desc.has_writable());
  }
  {
    // data descriptor
    v8::PropertyDescriptor desc(v8_num(42));
    desc.set_enumerable(false);
    EXPECT_TRUE(desc.value() == v8_num(42));
    EXPECT_TRUE(desc.has_value());
    EXPECT_TRUE(!desc.has_set());
    EXPECT_TRUE(!desc.has_get());
    EXPECT_TRUE(desc.has_enumerable());
    EXPECT_TRUE(!desc.enumerable());
    EXPECT_TRUE(!desc.has_configurable());
    EXPECT_TRUE(!desc.has_writable());
  }
  {
    // data descriptor
    v8::PropertyDescriptor desc(v8_num(42));
    desc.set_configurable(true);
    EXPECT_TRUE(desc.value() == v8_num(42));
    EXPECT_TRUE(desc.has_value());
    EXPECT_TRUE(!desc.has_set());
    EXPECT_TRUE(!desc.has_get());
    EXPECT_TRUE(desc.has_configurable());
    EXPECT_TRUE(desc.configurable());
    EXPECT_TRUE(!desc.has_enumerable());
    EXPECT_TRUE(!desc.has_writable());
  }
  {
    // data descriptor
    v8::PropertyDescriptor desc(v8_num(42));
    desc.set_configurable(false);
    EXPECT_TRUE(desc.value() == v8_num(42));
    EXPECT_TRUE(desc.has_value());
    EXPECT_TRUE(!desc.has_set());
    EXPECT_TRUE(!desc.has_get());
    EXPECT_TRUE(desc.has_configurable());
    EXPECT_TRUE(!desc.configurable());
    EXPECT_TRUE(!desc.has_enumerable());
    EXPECT_TRUE(!desc.has_writable());
  }
  {
    // data descriptor
    v8::PropertyDescriptor desc(v8_num(42), false);
    EXPECT_TRUE(desc.value() == v8_num(42));
    EXPECT_TRUE(desc.has_value());
    EXPECT_TRUE(!desc.has_set());
    EXPECT_TRUE(!desc.has_get());
    EXPECT_TRUE(!desc.has_enumerable());
    EXPECT_TRUE(!desc.has_configurable());
    EXPECT_TRUE(desc.has_writable());
    EXPECT_TRUE(!desc.writable());
  }
  {
    // data descriptor
    v8::PropertyDescriptor desc(v8::Local<v8::Value>(), true);
    EXPECT_TRUE(!desc.has_value());
    EXPECT_TRUE(!desc.has_set());
    EXPECT_TRUE(!desc.has_get());
    EXPECT_TRUE(!desc.has_enumerable());
    EXPECT_TRUE(!desc.has_configurable());
    EXPECT_TRUE(desc.has_writable());
    EXPECT_TRUE(desc.writable());
  }
  {
    // accessor descriptor
    CompileRun("var set = function() {return 43;};");

    v8::Local<v8::Function> set =
        v8::Local<v8::Function>::Cast(context->Global()
                                          ->Get(context, v8_str("set"))
                                          .ToLocalChecked());
    v8::PropertyDescriptor desc(v8::Undefined(isolate), set);
    desc.set_configurable(false);
    EXPECT_TRUE(!desc.has_value());
    EXPECT_TRUE(desc.has_get());
    EXPECT_TRUE(desc.get() == v8::Undefined(isolate));
    EXPECT_TRUE(desc.has_set());
    EXPECT_TRUE(desc.set() == set);
    EXPECT_TRUE(!desc.has_enumerable());
    EXPECT_TRUE(desc.has_configurable());
    EXPECT_TRUE(!desc.configurable());
    EXPECT_TRUE(!desc.has_writable());
  }
  {
    // accessor descriptor with Proxy
    CompileRun(
        "var set = new Proxy(function() {}, {});"
        "var get = undefined;");

    v8::Local<v8::Value> get =
        v8::Local<v8::Value>::Cast(context->Global()
                                       ->Get(context, v8_str("get"))
                                       .ToLocalChecked());
    v8::Local<v8::Function> set =
        v8::Local<v8::Function>::Cast(context->Global()
                                          ->Get(context, v8_str("set"))
                                          .ToLocalChecked());
    v8::PropertyDescriptor desc(get, set);
    desc.set_configurable(false);
    EXPECT_TRUE(!desc.has_value());
    EXPECT_TRUE(desc.get() == v8::Undefined(isolate));
    EXPECT_TRUE(desc.has_get());
    EXPECT_TRUE(desc.set() == set);
    EXPECT_TRUE(desc.has_set());
    EXPECT_TRUE(!desc.has_enumerable());
    EXPECT_TRUE(desc.has_configurable());
    EXPECT_TRUE(!desc.configurable());
    EXPECT_TRUE(!desc.has_writable());
  }
  {
    // accessor descriptor with empty function handle
    v8::Local<v8::Function> get = v8::Local<v8::Function>();
    v8::PropertyDescriptor desc(get, get);
    EXPECT_TRUE(!desc.has_value());
    EXPECT_TRUE(!desc.has_get());
    EXPECT_TRUE(!desc.has_set());
    EXPECT_TRUE(!desc.has_enumerable());
    EXPECT_TRUE(!desc.has_configurable());
    EXPECT_TRUE(!desc.has_writable());
  }
}
