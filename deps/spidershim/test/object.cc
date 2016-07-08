// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

TEST(SpiderShim, ReplaceConstantFunction) {
  // Based on the V8 test-api.cc ReplaceConstantFunction test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<Object> obj = Object::New(isolate);
  Local<FunctionTemplate> func_templ =
      FunctionTemplate::New(isolate);
  Local<String> foo_string = v8_str("foo");
  obj->Set(context, foo_string,
           func_templ->GetFunction(context).ToLocalChecked())
      .FromJust();
  Local<Object> obj_clone = obj->Clone();
  obj_clone->Set(context, foo_string, v8_str("Hello")).FromJust();
  EXPECT_TRUE(!obj->Get(context, foo_string).ToLocalChecked()->IsUndefined());
  EXPECT_TRUE(!obj->Get(context, foo_string).ToLocalChecked()->IsString());
  EXPECT_TRUE(obj->Get(context, foo_string).ToLocalChecked()->IsFunction());
  EXPECT_TRUE(obj_clone->Get(context, foo_string).ToLocalChecked()->IsString());
  EXPECT_TRUE(!obj_clone->Get(context, foo_string).ToLocalChecked()->IsFunction());
}

TEST(SpiderShim, ObjectSetAccessorProperty) {
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  engine.CompileRun(context,
      "var setValue;"
      "function return_42() { return 42; }"
      "function setter(val) { setValue = val; }");

  Local<Object> obj = Object::New(isolate);
  Local<Function> getter = context->Global()->Get(v8_str("return_42")).As<Function>();
  Local<Function> setter = context->Global()->Get(v8_str("setter")).As<Function>();
  obj->SetAccessorProperty(v8_str("prop"), getter, setter);
  context->Global()->Set(v8_str("obj"), obj);
  Local<Value> returned = engine.CompileRun(context, "obj.prop");
  EXPECT_EQ(42, returned.As<Integer>()->Value());
  EXPECT_TRUE(engine.CompileRun(context, "setValue")->IsUndefined());
  engine.CompileRun(context, "obj.prop = 'Hello'");
  Local<Value> setValue = engine.CompileRun(context, "setValue");
  returned = engine.CompileRun(context, "obj.prop");
  EXPECT_EQ(42, returned.As<Integer>()->Value());
  String::Utf8Value utf8(setValue.As<String>());
  EXPECT_STREQ("Hello", *utf8);
}
