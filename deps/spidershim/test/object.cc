// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <utility>

#include "v8engine.h"

#include "gtest/gtest.h"

#include "../src/instanceslots.h"
#include "../src/conversions.h"

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

TEST(SpiderShim, InstanceSlots) {
  // Note that this is not an API test.  It is designed to test the
  // functions defined in instanceslots.h.

  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<ObjectTemplate> templ_1 = ObjectTemplate::New(isolate);
  Local<Context> context = Context::New(isolate, nullptr, templ_1);
  Context::Scope context_scope(context);

  auto CheckSlots = [](JSObject* obj) {
    std::vector<std::pair<int, JS::Value>> slots;
    // Ensure that all of an object's slots are correctly accessible.
    for (size_t slot = 0; slot < size_t(InstanceSlots::NumSlots); ++slot) {
      const int value = rand();
      JS::Value oldValue = GetInstanceSlot(obj, slot);
      SetInstanceSlot(obj, slot, JS::Int32Value(value));
      slots.push_back(std::make_pair(value, oldValue));
    }
    for (size_t slot = 0; slot < size_t(InstanceSlots::NumSlots); ++slot) {
      EXPECT_EQ(slots[slot].first, GetInstanceSlot(obj, slot).toInt32());
      SetInstanceSlot(obj, slot, slots[slot].second); // restore
    }
  };

  Local<ObjectTemplate> templ_2 = ObjectTemplate::New(isolate);

  // Check the slots of a GlobalObject.
  CheckSlots(GetObject(context->Global()));
  // Check the slots of a NormalObject.
  CheckSlots(GetObject(templ_2->NewInstance()));
}

static void call_as_function(const FunctionCallbackInfo<Value>& args) {
  if (args.IsConstructCall()) {
    if (args[0]->IsInt32()) {
      args.GetReturnValue().Set(
          v8_num(-args[0]
                      ->Int32Value(args.GetIsolate()->GetCurrentContext())
                      .FromJust()));
      return;
    }
  }

  args.GetReturnValue().Set(args[0]);
}

static void ReturnThis(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(args.This());
}

void ThrowValue(const v8::FunctionCallbackInfo<v8::Value>& args) {
  EXPECT_EQ(1, args.Length());
  args.GetIsolate()->ThrowException(args[0]);
}

TEST(SpiderShim, CallAsFunction) {
  // Based on the V8 test-api.cc CallAsFunction test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  {
    Local<FunctionTemplate> t = FunctionTemplate::New(isolate);
    Local<ObjectTemplate> instance_template = t->InstanceTemplate();
    instance_template->SetCallAsFunctionHandler(call_as_function);
    Local<Object> instance = t->GetFunction(context)
                                     .ToLocalChecked()
                                     ->NewInstance(context)
                                     .ToLocalChecked();
    EXPECT_TRUE(context->Global()
              ->Set(context, v8_str("obj"), instance)
              .FromJust());
    TryCatch try_catch(isolate);
    Local<Value> value;
    EXPECT_TRUE(!try_catch.HasCaught());

    value = CompileRun("obj(42)");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(42, value->Int32Value(context).FromJust());

    value = CompileRun("(function(o){return o(49)})(obj)");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(49, value->Int32Value(context).FromJust());

    // test special case of call as function
    value = CompileRun("[obj]['0'](45)");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(45, value->Int32Value(context).FromJust());

    value = CompileRun(
        "obj.call = Function.prototype.call;"
        "obj.call(null, 87)");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(87, value->Int32Value(context).FromJust());

    // Regression tests for bug #1116356: Calling call through call/apply
    // must work for non-function receivers.
    const char* apply_99 = "Function.prototype.call.apply(obj, [this, 99])";
    value = CompileRun(apply_99);
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(99, value->Int32Value(context).FromJust());

    const char* call_17 = "Function.prototype.call.call(obj, this, 17)";
    value = CompileRun(call_17);
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(17, value->Int32Value(context).FromJust());

    // Check that the call-as-function handler can be called through
    // new.
    value = CompileRun("new obj(43)");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(-43, value->Int32Value(context).FromJust());

    // Check that the call-as-function handler can be called through
    // the API.
    Local<Value> args[] = {v8_num(28)};
    value = instance->CallAsFunction(context, instance, 1, args)
                .ToLocalChecked();
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(28, value->Int32Value(context).FromJust());
  }

  {
    Local<FunctionTemplate> t = FunctionTemplate::New(isolate);
    Local<ObjectTemplate> instance_template(t->InstanceTemplate());
    (void)(instance_template);
    Local<Object> instance = t->GetFunction(context)
                                     .ToLocalChecked()
                                     ->NewInstance(context)
                                     .ToLocalChecked();
    EXPECT_TRUE(context->Global()
              ->Set(context, v8_str("obj2"), instance)
              .FromJust());
    TryCatch try_catch(isolate);
    Local<Value> value;
    EXPECT_TRUE(!try_catch.HasCaught());

    // Call an object without call-as-function handler through the JS
    value = CompileRun("obj2(28)");
    EXPECT_TRUE(value.IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value1(try_catch.Exception());
    // TODO(verwaest): Better message
    EXPECT_EQ(0, strcmp("TypeError: obj2 is not a function", *exception_value1));
    try_catch.Reset();

    // Call an object without call-as-function handler through the API
    value = CompileRun("obj2(28)");
    Local<Value> args[] = {v8_num(28)};
    EXPECT_TRUE(
        instance->CallAsFunction(context, instance, 1, args).IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value2(try_catch.Exception());
    EXPECT_STREQ("TypeError: obj2 is not a function", *exception_value2);
    try_catch.Reset();
  }

  {
    Local<FunctionTemplate> t = FunctionTemplate::New(isolate);
    Local<ObjectTemplate> instance_template = t->InstanceTemplate();
    instance_template->SetCallAsFunctionHandler(ThrowValue);
    Local<Object> instance = t->GetFunction(context)
                                     .ToLocalChecked()
                                     ->NewInstance(context)
                                     .ToLocalChecked();
    EXPECT_TRUE(context->Global()
              ->Set(context, v8_str("obj3"), instance)
              .FromJust());
    TryCatch try_catch(isolate);
    Local<Value> value;
    EXPECT_TRUE(!try_catch.HasCaught());

    // Catch the exception which is thrown by call-as-function handler
    value = CompileRun("obj3(22)");
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value1(try_catch.Exception());
    EXPECT_EQ(0, strcmp("22", *exception_value1));
    try_catch.Reset();

    Local<Value> args[] = {v8_num(23)};
    EXPECT_TRUE(
        instance->CallAsFunction(context, instance, 1, args).IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value2(try_catch.Exception());
    EXPECT_EQ(0, strcmp("23", *exception_value2));
    try_catch.Reset();
  }

  {
    Local<FunctionTemplate> t = FunctionTemplate::New(isolate);
    Local<ObjectTemplate> instance_template = t->InstanceTemplate();
    instance_template->SetCallAsFunctionHandler(ReturnThis);
    Local<Object> instance = t->GetFunction(context)
                                     .ToLocalChecked()
                                     ->NewInstance(context)
                                     .ToLocalChecked();

    Local<Value> a1 =
        instance->CallAsFunction(context, Undefined(isolate), 0,
                                 NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a1->StrictEquals(instance));
    Local<Value> a2 =
        instance->CallAsFunction(context, Null(isolate), 0, NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a2->StrictEquals(instance));
    Local<Value> a3 =
        instance->CallAsFunction(context, v8_num(42), 0, NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a3->StrictEquals(instance));
    Local<Value> a4 =
        instance->CallAsFunction(context, v8_str("hello"), 0, NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a4->StrictEquals(instance));
    Local<Value> a5 =
        instance->CallAsFunction(context, True(isolate), 0, NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a5->StrictEquals(instance));
  }

  {
    CompileRun(
        "function ReturnThisSloppy() {"
        "  return this;"
        "}"
        "function ReturnThisStrict() {"
        "  'use strict';"
        "  return this;"
        "}");
    Local<Function> ReturnThisSloppy = Local<Function>::Cast(
        context->Global()
            ->Get(context, v8_str("ReturnThisSloppy"))
            .ToLocalChecked());
    Local<Function> ReturnThisStrict = Local<Function>::Cast(
        context->Global()
            ->Get(context, v8_str("ReturnThisStrict"))
            .ToLocalChecked());

    Local<Value> a1 =
        ReturnThisSloppy->CallAsFunction(context,
                                         Undefined(isolate), 0, NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a1->StrictEquals(context->Global()));
    Local<Value> a2 =
        ReturnThisSloppy->CallAsFunction(context, Null(isolate), 0,
                                         NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a2->StrictEquals(context->Global()));
    Local<Value> a3 =
        ReturnThisSloppy->CallAsFunction(context, v8_num(42), 0, NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a3->IsNumberObject());
    EXPECT_EQ(42.0, a3.As<NumberObject>()->ValueOf());
    Local<Value> a4 =
        ReturnThisSloppy->CallAsFunction(context, v8_str("hello"), 0,
                                         NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a4->IsStringObject());
    EXPECT_TRUE(a4.As<StringObject>()->ValueOf()->StrictEquals(v8_str("hello")));
    Local<Value> a5 =
        ReturnThisSloppy->CallAsFunction(context, True(isolate), 0,
                                         NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a5->IsBooleanObject());
    EXPECT_TRUE(a5.As<BooleanObject>()->ValueOf());

    Local<Value> a6 =
        ReturnThisStrict->CallAsFunction(context,
                                         Undefined(isolate), 0, NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a6->IsUndefined());
    Local<Value> a7 =
        ReturnThisStrict->CallAsFunction(context, Null(isolate), 0,
                                         NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a7->IsNull());
    Local<Value> a8 =
        ReturnThisStrict->CallAsFunction(context, v8_num(42), 0, NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a8->StrictEquals(v8_num(42)));
    Local<Value> a9 =
        ReturnThisStrict->CallAsFunction(context, v8_str("hello"), 0,
                                         NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a9->StrictEquals(v8_str("hello")));
    Local<Value> a10 =
        ReturnThisStrict->CallAsFunction(context, True(isolate), 0,
                                         NULL)
            .ToLocalChecked();
    EXPECT_TRUE(a10->StrictEquals(True(isolate)));
  }
}
