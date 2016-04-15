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
