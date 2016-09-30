// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

TEST(SpiderShim, ObjectTemplate) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<ObjectTemplate> templ = ObjectTemplate::New(engine.isolate());
  templ->Set(v8_str("name1"), v8_str("value1"));
  templ->Set(v8_str("name2"), v8_str("value2"), ReadOnly);
}

static void Returns42(const FunctionCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(42);
}

static void Gets42(Local<String> property,
                   const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(42);
}

TEST(SpiderShim, ObjectTemplateDetails) {
  // Loosely based on the V8 test-api.cc ObjectTemplate test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> acc = FunctionTemplate::New(isolate, Returns42);
  EXPECT_TRUE(context->Global()->Set(context, v8_str("acc"),
                                     acc->GetFunction(context).ToLocalChecked())
              .FromJust());

  Local<FunctionTemplate> fun = FunctionTemplate::New(isolate);
  Local<String> class_name = v8_str("the_class_name");
  fun->SetClassName(class_name);
  Local<ObjectTemplate> templ1 = ObjectTemplate::New(isolate, fun);
  templ1->Set(isolate, "x", v8_num(10));
  templ1->Set(isolate, "y", v8_num(13));
  templ1->Set(v8_str("foo"), acc);
  Local<Object> instance1 =
      templ1->NewInstance(context).ToLocalChecked();
  EXPECT_TRUE(class_name->StrictEquals(instance1->GetConstructorName()));
  EXPECT_TRUE(context->Global()->Set(context, v8_str("p"), instance1).FromJust());
  EXPECT_TRUE(CompileRun("(p.x == 10)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(p.y == 13)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(p.foo() == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(p.foo == acc)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(p.toString() == '[object the_class_name]')")->BooleanValue(context).FromJust());
  // Ensure that foo become a data field.
  CompileRun("p.foo = function() {}");

  Local<FunctionTemplate> fun2 = FunctionTemplate::New(isolate);
  fun2->PrototypeTemplate()->Set(isolate, "nirk", v8_num(123));
  Local<ObjectTemplate> templ2 = fun2->InstanceTemplate();
  templ2->Set(isolate, "a", v8_num(12));
     templ2->Set(isolate, "b", templ1);
     templ2->Set(v8_str("bar"), acc);
  templ2->SetAccessorProperty(v8_str("acc"), acc);
  Local<Object> instance2 =
    templ2->NewInstance(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Set(context, v8_str("q"), instance2).FromJust());
  EXPECT_TRUE(CompileRun("(q.nirk == 123)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.a == 12)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.b.x == 10)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.b.y == 13)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.b.foo() == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.b.foo === acc)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.b !== p)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.acc == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.bar() == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q.bar == acc)")->BooleanValue(context).FromJust());

  instance2 = templ2->NewInstance(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Set(context, v8_str("q2"), instance2).FromJust());
  EXPECT_TRUE(CompileRun("(q2.nirk == 123)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q2.a == 12)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q2.b.x == 10)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q2.b.y == 13)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q2.b.foo() == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q2.b.foo === acc)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q2.acc == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q2.bar() == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q2.bar === acc)")->BooleanValue(context).FromJust());

  // Additional test (not in ObjectTemplate tests) that calling our constructor
  // works correctly too.
  EXPECT_TRUE(context->Global()->Set(context, v8_str("ourCtor"),
                                     fun2->GetFunction(context).ToLocalChecked())
              .FromJust());
  instance2 = CompileRun("new ourCtor()")->ToObject(isolate);
  EXPECT_TRUE(context->Global()->Set(context, v8_str("q3"), instance2).FromJust());
  EXPECT_TRUE(CompileRun("(q3.nirk == 123)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q3.a == 12)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q3.b.x == 10)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q3.b.y == 13)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q3.b.foo() == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q3.b.foo === acc)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q3.acc == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q3.bar() == 42)")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("(q3.bar === acc)")->BooleanValue(context).FromJust());

  // TEST_TODO: It seems like V8 instantiates the template once per instance,
  //            whereas we do that in Template::Set().  We should probably keep
  //            track of such Template properties separately and instantiate them
  //            during instance creation.
  //   EXPECT_TRUE(CompileRun("(q.b !== q2.b)")->BooleanValue(context).FromJust());
  //   EXPECT_TRUE(CompileRun("q.b.x = 17; (q2.b.x == 10)")
  //             ->BooleanValue(context)
  //             .FromJust());

  EXPECT_TRUE(engine.CompileRun(context,
                      "desc1 = Object.getOwnPropertyDescriptor(q, 'acc');"
                      "(desc1.get === acc)")
                 ->BooleanValue(context)
                 .FromJust());
  EXPECT_TRUE(engine.CompileRun(context,
                          "desc2 = Object.getOwnPropertyDescriptor(q2, 'acc');"
                          "(desc2.get === acc)")
            ->BooleanValue(context)
            .FromJust());
}

static void GetNirk(Local<String> name,
                    const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(900));
}

static void GetRino(Local<String> name,
                    const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(560));
}

enum ObjectInstantiationMode {
  // Create object using ObjectTemplate::NewInstance.
  ObjectTemplate_NewInstance,
  // Create object using FunctionTemplate::NewInstance on constructor.
  Constructor_GetFunction_NewInstance,
  // Create object using new operator on constructor.
  Constructor_GetFunction_New
};

static void TestObjectTemplateInheritedWithPrototype(
    ObjectInstantiationMode mode) {
  // Loosely based on the V8 test-api.cc
  // TestObjectTemplateInheritedWithPrototype test.  Very loosely, because a lot
  // of the templating API has changed from the version Node uses to V8 tip...
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> fun_A = FunctionTemplate::New(isolate);
  fun_A->SetClassName(v8_str("A"));
  Local<ObjectTemplate> prototype_templ = fun_A->PrototypeTemplate();
  prototype_templ->Set(isolate, "a", v8_num(113));
  prototype_templ->SetAccessor(v8_str("nirk"), GetNirk);
  prototype_templ->Set(isolate, "b", v8_num(153));

  Local<FunctionTemplate> fun_B = FunctionTemplate::New(isolate);
  Local<String> class_name = v8_str("B");
  fun_B->SetClassName(class_name);
  fun_B->Inherit(fun_A);
  prototype_templ = fun_B->PrototypeTemplate();
  prototype_templ->Set(isolate, "c", v8_num(713));
  prototype_templ->SetAccessor(v8_str("rino"), GetRino);
  prototype_templ->Set(isolate, "d", v8_num(753));

  Local<ObjectTemplate> templ = fun_B->InstanceTemplate();
  templ->Set(isolate, "x", v8_num(10));
  templ->Set(isolate, "y", v8_num(13));

  // Perform several iterations to trigger creation from cached boilerplate.
  for (int i = 0; i < 3; i++) {
    Local<Object> instance;
    switch (mode) {
      case ObjectTemplate_NewInstance:
        instance = templ->NewInstance(context).ToLocalChecked();
        break;

      case Constructor_GetFunction_NewInstance: {
        Local<Function> function_B =
            fun_B->GetFunction(context).ToLocalChecked();
        instance = function_B->NewInstance(context).ToLocalChecked();
        break;
      }
      case Constructor_GetFunction_New: {
        Local<Function> function_B =
            fun_B->GetFunction(context).ToLocalChecked();
        if (i == 0) {
          EXPECT_TRUE(context->Global()
                        ->Set(context, class_name, function_B)
                        .FromJust());
        }
        instance =
            CompileRun("new B()")->ToObject(context).ToLocalChecked();
        break;
      }
      default:
        EXPECT_TRUE(false);
    }

    EXPECT_TRUE(class_name->StrictEquals(instance->GetConstructorName()));
    EXPECT_TRUE(context->Global()->Set(context, v8_str("o"), instance).FromJust());

    EXPECT_EQ(10, CompileRun("o.x")->IntegerValue(context).FromJust());
    EXPECT_EQ(13, CompileRun("o.y")->IntegerValue(context).FromJust());

    EXPECT_EQ(113, CompileRun("o.a")->IntegerValue(context).FromJust());
    EXPECT_EQ(900, CompileRun("o.nirk")->IntegerValue(context).FromJust());
    EXPECT_EQ(153, CompileRun("o.b")->IntegerValue(context).FromJust());
    EXPECT_EQ(713, CompileRun("o.c")->IntegerValue(context).FromJust());
    EXPECT_EQ(560, CompileRun("o.rino")->IntegerValue(context).FromJust());
    EXPECT_EQ(753, CompileRun("o.d")->IntegerValue(context).FromJust());
  }
}

TEST(SpiderShim, TestObjectTemplateInheritedWithAccessorsInPrototype1) {
  TestObjectTemplateInheritedWithPrototype(ObjectTemplate_NewInstance);
}

TEST(SpiderShim, TestObjectTemplateInheritedWithAccessorsInPrototype2) {
  TestObjectTemplateInheritedWithPrototype(Constructor_GetFunction_NewInstance);
}

TEST(SpiderShim, TestObjectTemplateInheritedWithAccessorsInPrototype3) {
  TestObjectTemplateInheritedWithPrototype(Constructor_GetFunction_New);
}

// We skip test-api.cc's TestObjectTemplateInheritedWithoutInstanceTemplate
// because that uses SetNativeDataProperty, which we don't need/have so far.

static void handle_callback_1(const FunctionCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(102));
}

static void handle_callback_2(const FunctionCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(103));
}

static void handle_callback_3(const FunctionCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(104));
}

TEST(SpiderShim, FunctionTemplateBasicInit) {
  // Loosely based on the V8 test-api.cc TestFunctionTemplateInitializer test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  // Test passing a callback directly to FunctionTemplate::New.
  Local<FunctionTemplate> fun_templ =
    FunctionTemplate::New(isolate, handle_callback_1);
  Local<Function> fun = fun_templ->GetFunction(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Set(context, v8_str("obj"), fun).FromJust());
  Local<Script> script = v8_compile("obj()");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(102, v8_run_int32value(script));
  }

  // Test setting a callback after construction
  fun_templ = FunctionTemplate::New(isolate);
  fun_templ->SetCallHandler(handle_callback_2);
  fun = fun_templ->GetFunction(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Set(context, v8_str("obj"), fun).FromJust());
  script = v8_compile("obj()");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(103, v8_run_int32value(script));
  }

  // Test passing a callback and then setting a different one.
  fun_templ = FunctionTemplate::New(isolate, handle_callback_1);
  fun_templ->SetCallHandler(handle_callback_3);
  fun = fun_templ->GetFunction(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Set(context, v8_str("obj"), fun).FromJust());
  script = v8_compile("obj()");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(104, v8_run_int32value(script));
  }
}

static void construct_callback(
    const FunctionCallbackInfo<Value>& info) {
  EXPECT_TRUE(
      info.This()
          ->Set(info.GetIsolate()->GetCurrentContext(), v8_str("x"), v8_num(1))
          .FromJust());
  EXPECT_TRUE(
      info.This()
          ->Set(info.GetIsolate()->GetCurrentContext(), v8_str("y"), v8_num(2))
          .FromJust());
  info.GetReturnValue().Set(v8_str("bad value"));
  info.GetReturnValue().Set(info.This());
}

static void Return239Callback(
    Local<String> name, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_str("bad value"));
  info.GetReturnValue().Set(v8_num(239));
}

TEST(SpiderShim, FunctionTemplateBasicAccessors) {
  // Loosely based on the V8 test-api.cc TestFunctionTemplateAccessor test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> fun_templ =
    FunctionTemplate::New(isolate, construct_callback);
  fun_templ->SetClassName(v8_str("funky"));
  fun_templ->InstanceTemplate()->SetAccessor(v8_str("m"), Return239Callback);
  Local<Function> fun = fun_templ->GetFunction(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Set(context, v8_str("obj"), fun).FromJust());
  Local<Value> result =
      v8_compile("(new obj()).toString()")->Run(context).ToLocalChecked();
  EXPECT_TRUE(v8_str("[object funky]")->Equals(context, result).FromJust());
  CompileRun("var obj_instance = new obj();");
  Local<Script> script;
  script = v8_compile("obj_instance.x");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(1, v8_run_int32value(script));
  }
  script = v8_compile("obj_instance.m");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(239, v8_run_int32value(script));
  }
}


TEST(SpiderShim, FunctionTemplateSetLength) {
  // Loosely based on the V8 test-api.cc FunctionTemplateSetLength test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  {
    Local<FunctionTemplate> fun_templ =
        FunctionTemplate::New(isolate, handle_callback_1, Local<Value>(),
                              Local<Signature>(), 23);
    Local<Function> fun = fun_templ->GetFunction(context).ToLocalChecked();
    EXPECT_TRUE(context->Global()->Set(context, v8_str("obj"), fun).FromJust());
    Local<Script> script = v8_compile("obj.length");
    EXPECT_EQ(23, v8_run_int32value(script));
  }
  {
    // Without setting length it defaults to 0.
    Local<FunctionTemplate> fun_templ =
        FunctionTemplate::New(isolate, handle_callback_1);
    Local<Function> fun = fun_templ->GetFunction(context).ToLocalChecked();
    EXPECT_TRUE(context->Global()->Set(context, v8_str("obj"), fun).FromJust());
    Local<Script> script = v8_compile("obj.length");
    EXPECT_EQ(0, v8_run_int32value(script));
  }
}


static int signature_callback_count;
static Local<Value> signature_expected_receiver;
static void IncrementingSignatureCallback(
    const FunctionCallbackInfo<Value>& args) {
  signature_callback_count++;
  EXPECT_TRUE(signature_expected_receiver->Equals(
                                       args.GetIsolate()->GetCurrentContext(),
                                       args.Holder())
                .FromJust());
  EXPECT_TRUE(signature_expected_receiver->Equals(
                                       args.GetIsolate()->GetCurrentContext(),
                                       args.This())
                .FromJust());
  Local<Array> result =
      Array::New(args.GetIsolate(), args.Length());
  for (int i = 0; i < args.Length(); i++) {
    EXPECT_TRUE(result->Set(args.GetIsolate()->GetCurrentContext(),
                      Integer::New(args.GetIsolate(), i), args[i])
                  .FromJust());
  }
  args.GetReturnValue().Set(result);
}

static void TestSignature(V8Engine& engine, Local<Context> context,
                          const char* loop_js,
                          Local<Value> receiver, Isolate* isolate) {
  char source[200];
  sprintf(source,
          "for (var i = 0; i < 10; i++) {"
          "  %s"
          "}",
          loop_js);
  signature_callback_count = 0;
  signature_expected_receiver = receiver;
  bool expected_to_throw = receiver.IsEmpty();
  TryCatch try_catch(isolate);
  engine.CompileRun(context, source);
  EXPECT_EQ(expected_to_throw, try_catch.HasCaught());
  if (!expected_to_throw) {
    EXPECT_EQ(10, signature_callback_count);
  } else {
    EXPECT_TRUE(v8_str("TypeError: Illegal invocation")
                ->Equals(isolate->GetCurrentContext(),
                         try_catch.Exception()
                             ->ToString(isolate->GetCurrentContext())
                             .ToLocalChecked())
                .FromJust());
  }
}

TEST(SpiderShim, FunctionTemplateReceiverSignature) {
  // Largely stolen from the V8 test-api.cc ReceiverSignature test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  // Setup templates.
  Local<FunctionTemplate> fun = FunctionTemplate::New(isolate);
  Local<Signature> sig = Signature::New(isolate, fun);
  Local<FunctionTemplate> callback_sig = FunctionTemplate::New(
      isolate, IncrementingSignatureCallback, Local<Value>(), sig);
  Local<FunctionTemplate> callback =
      FunctionTemplate::New(isolate, IncrementingSignatureCallback);
  Local<FunctionTemplate> sub_fun = FunctionTemplate::New(isolate);
  sub_fun->Inherit(fun);
  Local<FunctionTemplate> unrel_fun =
      FunctionTemplate::New(isolate);
  // Install properties.
  Local<ObjectTemplate> fun_proto = fun->PrototypeTemplate();
  fun_proto->Set(v8_str("prop_sig"), callback_sig);
  fun_proto->Set(v8_str("prop"), callback);
  fun_proto->SetAccessorProperty(
      v8_str("accessor_sig"), callback_sig, callback_sig);
  fun_proto->SetAccessorProperty(v8_str("accessor"), callback, callback);

  // Instantiate templates.
  Local<Value> fun_instance =
      fun->InstanceTemplate()->NewInstance(context).ToLocalChecked();
  Local<Value> sub_fun_instance =
      sub_fun->InstanceTemplate()->NewInstance(context).ToLocalChecked();
  // Setup global variables.
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("Fun"),
                  fun->GetFunction(context).ToLocalChecked())
            .FromJust());
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("UnrelFun"),
                  unrel_fun->GetFunction(context).ToLocalChecked())
            .FromJust());
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("fun_instance"), fun_instance)
            .FromJust());
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("sub_fun_instance"), sub_fun_instance)
            .FromJust());
  engine.CompileRun(context,
      "var accessor_sig_key = 'accessor_sig';"
      "var accessor_key = 'accessor';"
      "var prop_sig_key = 'prop_sig';"
      "var prop_key = 'prop';"
      ""
      "function copy_props(obj) {"
      "  var keys = [accessor_sig_key, accessor_key, prop_sig_key, prop_key];"
      "  var source = Fun.prototype;"
      "  for (var i in keys) {"
      "    var key = keys[i];"
      "    var desc = Object.getOwnPropertyDescriptor(source, key);"
      "    Object.defineProperty(obj, key, desc);"
      "  }"
      "}"
      ""
      "var obj = {};"
      "copy_props(obj);"
      "var unrel = new UnrelFun();"
      "copy_props(unrel);");
  // Test with and without ICs
  const char* test_objects[] = {
      "fun_instance", "sub_fun_instance", "obj", "unrel" };
  unsigned bad_signature_start_offset = 2;
  for (unsigned i = 0; i < sizeof(test_objects)/sizeof(*test_objects); i++) {
    char source[200];
    sprintf(source, "var test_object = %s; test_object", test_objects[i]);
    Local<Value> test_object = engine.CompileRun(context, source);
    TestSignature(engine, context,
                  "test_object.prop();", test_object, isolate);
    TestSignature(engine, context,
                  "test_object.accessor;", test_object, isolate);
    TestSignature(engine, context,
                  "test_object[accessor_key];", test_object, isolate);
    TestSignature(engine, context,
                  "test_object.accessor = 1;", test_object, isolate);
    TestSignature(engine, context,
                  "test_object[accessor_key] = 1;", test_object, isolate);
    if (i >= bad_signature_start_offset) test_object = Local<Value>();
    TestSignature(engine, context,
                  "test_object.prop_sig();", test_object, isolate);
    TestSignature(engine, context,
                  "test_object.accessor_sig;", test_object, isolate);
    TestSignature(engine, context,
                  "test_object[accessor_sig_key];", test_object, isolate);
    TestSignature(engine, context,
                  "test_object.accessor_sig = 1;", test_object, isolate);
    TestSignature(engine, context,
                  "test_object[accessor_sig_key] = 1;", test_object, isolate);
  }
}

TEST(SpiderShim, InternalFields) {
  // Loosely based on the V8 test-api.cc InternalFields test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> templ = FunctionTemplate::New(isolate);
  Local<ObjectTemplate> instance_templ = templ->InstanceTemplate();
  EXPECT_EQ(0, instance_templ->InternalFieldCount());
  instance_templ->SetInternalFieldCount(1);
  EXPECT_EQ(1, instance_templ->InternalFieldCount());
  Local<Object> obj = templ->GetFunction(context)
                              .ToLocalChecked()
                              ->NewInstance(context)
                              .ToLocalChecked();
  EXPECT_EQ(1, obj->InternalFieldCount());
  EXPECT_TRUE(obj->GetInternalField(0)->IsUndefined());
  obj->SetInternalField(0, v8_num(17));
  EXPECT_EQ(17, obj->GetInternalField(0)->Int32Value(context).FromJust());

  // Also ensure that the InternalFields APIs are available when the underlying
  // object is proxied.
  Local<Context> context2 = Context::New(isolate);
  Context::Scope context_scope2(context2);
  EXPECT_TRUE(context2->Global()
            ->Set(context2, v8_str("obj"), obj)
            .FromJust());
  Local<Object> obj2 = context2->Global()->Get(v8_str("obj")).As<Object>();
  EXPECT_EQ(1, obj2->InternalFieldCount());
  EXPECT_EQ(17, obj2->GetInternalField(0)->Int32Value(context2).FromJust());
  obj2->SetInternalField(0, Undefined(isolate));
  EXPECT_TRUE(obj2->GetInternalField(0)->IsUndefined());
}

static void CheckAlignedPointerInInternalField(Local<Object> obj,
                                               void* value) {
  EXPECT_EQ(0, static_cast<int>(reinterpret_cast<uintptr_t>(value) & 0x1));
  obj->SetAlignedPointerInInternalField(0, value);
  Isolate::GetCurrent()->RequestGarbageCollectionForTesting(kFullGarbageCollection);
  EXPECT_EQ(value, obj->GetAlignedPointerFromInternalField(0));
}

void CheckInternalFieldsAlignedPointers(Isolate* isolate, Local<Object> obj) {
  EXPECT_EQ(1, obj->InternalFieldCount());

  CheckAlignedPointerInInternalField(obj, NULL);

  int* heap_allocated = new int[100];
  CheckAlignedPointerInInternalField(obj, heap_allocated);
  delete[] heap_allocated;

  int stack_allocated[100];
  CheckAlignedPointerInInternalField(obj, stack_allocated);

  void* huge = reinterpret_cast<void*>(~static_cast<uintptr_t>(1));
  CheckAlignedPointerInInternalField(obj, huge);

  Global<Object> persistent(isolate, obj);
  EXPECT_EQ(1, Object::InternalFieldCount(persistent));
  EXPECT_EQ(huge, Object::GetAlignedPointerFromInternalField(persistent, 0));
}

TEST(SpiderShim, InternalFieldsAlignedPointers) {
  // Loosely based on the V8 test-api.cc InternalFieldsAlignedPointers test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope scope(isolate);
  Local<ObjectTemplate> global_templ = ObjectTemplate::New(isolate);
  global_templ->SetInternalFieldCount(1);
  Local<Context> context = Context::New(isolate, nullptr, global_templ);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> templ = FunctionTemplate::New(isolate);
  Local<ObjectTemplate> instance_templ = templ->InstanceTemplate();
  EXPECT_EQ(0, instance_templ->InternalFieldCount());
  instance_templ->SetInternalFieldCount(1);
  EXPECT_EQ(1, instance_templ->InternalFieldCount());
  Local<Object> obj = templ->GetFunction(context)
                              .ToLocalChecked()
                              ->NewInstance(context)
                              .ToLocalChecked();
  CheckInternalFieldsAlignedPointers(isolate, obj);
  CheckInternalFieldsAlignedPointers(isolate, context->Global());
  CheckInternalFieldsAlignedPointers(isolate, context->Global()->GetPrototype()->ToObject());
}

static int instance_checked_getter_count = 0;
static void InstanceCheckedGetter(
    Local<String> name,
    const PropertyCallbackInfo<Value>& info) {
  EXPECT_TRUE(name->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("foo"))
            .FromJust());
  instance_checked_getter_count++;
  info.GetReturnValue().Set(v8_num(11));
}

static int instance_checked_setter_count = 0;
static void InstanceCheckedSetter(Local<String> name,
                      Local<Value> value,
                      const PropertyCallbackInfo<void>& info) {
  EXPECT_TRUE(name->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("foo"))
            .FromJust());
  EXPECT_TRUE(value->Equals(info.GetIsolate()->GetCurrentContext(), v8_num(23))
            .FromJust());
  instance_checked_setter_count++;
}

static void CheckInstanceCheckedResult(int getters, int setters,
                                       bool expects_callbacks,
                                       TryCatch* try_catch) {
  if (expects_callbacks) {
    EXPECT_TRUE(!try_catch->HasCaught());
    EXPECT_EQ(getters, instance_checked_getter_count);
    EXPECT_EQ(setters, instance_checked_setter_count);
  } else {
    EXPECT_TRUE(try_catch->HasCaught());
    EXPECT_EQ(0, instance_checked_getter_count);
    EXPECT_EQ(0, instance_checked_setter_count);
  }
  try_catch->Reset();
}

static void CheckInstanceCheckedAccessors(bool expects_callbacks) {
  instance_checked_getter_count = 0;
  instance_checked_setter_count = 0;
  TryCatch try_catch(Isolate::GetCurrent());

  // Test path through generic runtime code.
  CompileRun("obj.foo");
  CheckInstanceCheckedResult(1, 0, expects_callbacks, &try_catch);
  CompileRun("obj.foo = 23");
  CheckInstanceCheckedResult(1, 1, expects_callbacks, &try_catch);

  // Test path through generated LoadIC and StoredIC.
  CompileRun("function test_get(o) { o.foo; }"
             "test_get(obj);");
  CheckInstanceCheckedResult(2, 1, expects_callbacks, &try_catch);
  CompileRun("test_get(obj);");
  CheckInstanceCheckedResult(3, 1, expects_callbacks, &try_catch);
  CompileRun("test_get(obj);");
  CheckInstanceCheckedResult(4, 1, expects_callbacks, &try_catch);
  CompileRun("function test_set(o) { o.foo = 23; }"
             "test_set(obj);");
  CheckInstanceCheckedResult(4, 2, expects_callbacks, &try_catch);
  CompileRun("test_set(obj);");
  CheckInstanceCheckedResult(4, 3, expects_callbacks, &try_catch);
  CompileRun("test_set(obj);");
  CheckInstanceCheckedResult(4, 4, expects_callbacks, &try_catch);
}

TEST(SpiderShim, InstanceCheckOnInstanceAccessor) {
  // Loosely based on the V8 test-api.cc InstanceCheckOnInstanceAccessor test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> templ = FunctionTemplate::New(context->GetIsolate());
  Local<ObjectTemplate> inst = templ->InstanceTemplate();
  inst->SetAccessor(v8_str("foo"), InstanceCheckedGetter, InstanceCheckedSetter,
                    Local<Value>(), DEFAULT, None,
                    AccessorSignature::New(context->GetIsolate(), templ));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("f"),
                  templ->GetFunction(context).ToLocalChecked())
            .FromJust());

  CompileRun("var obj = new f();");
  EXPECT_TRUE(templ->HasInstance(
      context->Global()->Get(context, v8_str("obj")).ToLocalChecked()));
  CheckInstanceCheckedAccessors(true);

  CompileRun("var obj = {};"
             "obj.__proto__ = new f();");
  EXPECT_TRUE(!templ->HasInstance(
      context->Global()->Get(context, v8_str("obj")).ToLocalChecked()));
  CheckInstanceCheckedAccessors(false);
}

TEST(SpiderShim, InstanceCheckOnPrototypeAccessor) {
  // Loosely based on the V8 test-api.cc InstanceCheckOnPrototypeAccessor test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> templ = FunctionTemplate::New(context->GetIsolate());
  Local<ObjectTemplate> proto = templ->PrototypeTemplate();
  proto->SetAccessor(v8_str("foo"), InstanceCheckedGetter,
                     InstanceCheckedSetter, Local<Value>(), DEFAULT,
                     None,
                     AccessorSignature::New(context->GetIsolate(), templ));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("f"),
                  templ->GetFunction(context).ToLocalChecked())
            .FromJust());

  CompileRun("var obj = new f();");
  EXPECT_TRUE(templ->HasInstance(
      context->Global()->Get(context, v8_str("obj")).ToLocalChecked()));
  CheckInstanceCheckedAccessors(true);

  CompileRun("var obj = {};"
             "obj.__proto__ = new f();");
  EXPECT_TRUE(!templ->HasInstance(
      context->Global()->Get(context, v8_str("obj")).ToLocalChecked()));
  CheckInstanceCheckedAccessors(false);

  CompileRun("var obj = new f();"
             "var pro = {};"
             "pro.__proto__ = obj.__proto__;"
             "obj.__proto__ = pro;");
  EXPECT_TRUE(templ->HasInstance(
      context->Global()->Get(context, v8_str("obj")).ToLocalChecked()));
  CheckInstanceCheckedAccessors(true);
}

static void SimpleCallback(const FunctionCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(51423 + info.Length()));
}

TEST(SpiderShim, SimpleCallback) {
  // Loosely based on the V8 test-api.cc SimpleCallback test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<ObjectTemplate> object_template =
      ObjectTemplate::New(isolate);
  object_template->Set(isolate, "callback",
                       FunctionTemplate::New(isolate, SimpleCallback));
  Local<Object> object =
      object_template->NewInstance(context).ToLocalChecked();
  EXPECT_TRUE(context
              ->Global()
              ->Set(context, v8_str("callback_object"), object)
              .FromJust());
  Local<Script> script;
  script = v8_compile("callback_object.callback(17)");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(51424, v8_run_int32value(script));
  }
  script = v8_compile("callback_object.callback(17, 24)");
  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(51425, v8_run_int32value(script));
  }
}

static void GetFlabby(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(v8_num(17.2));
}


static void GetKnurd(Local<String> property,
                     const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(15.2));
}


TEST(SpiderShim, DescriptorInheritance) {
  // Loosely based on the V8 test-api.cc DescriptorInheritance test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> super = FunctionTemplate::New(isolate);
  super->PrototypeTemplate()->Set(isolate, "flabby",
                                  FunctionTemplate::New(isolate,
                                                            GetFlabby));
  super->PrototypeTemplate()->Set(isolate, "PI", v8_num(3.14));

  super->InstanceTemplate()->SetAccessor(v8_str("knurd"), GetKnurd);

  Local<FunctionTemplate> base1 = FunctionTemplate::New(isolate);
  base1->Inherit(super);
  base1->PrototypeTemplate()->Set(isolate, "v1", v8_num(20.1));

  Local<FunctionTemplate> base2 = FunctionTemplate::New(isolate);
  base2->Inherit(super);
  base2->PrototypeTemplate()->Set(isolate, "v2", v8_num(10.1));

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("s"),
                  super->GetFunction(context).ToLocalChecked())
            .FromJust());
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("base1"),
                  base1->GetFunction(context).ToLocalChecked())
            .FromJust());
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("base2"),
                  base2->GetFunction(context).ToLocalChecked())
            .FromJust());

  // Checks right __proto__ chain.
  EXPECT_TRUE(CompileRun("base1.prototype.__proto__ == s.prototype")
            ->BooleanValue(context)
            .FromJust());
  EXPECT_TRUE(CompileRun("base2.prototype.__proto__ == s.prototype")
            ->BooleanValue(context)
            .FromJust());

  EXPECT_TRUE(v8_compile("s.prototype.PI == 3.14")
            ->Run(context)
            .ToLocalChecked()
            ->BooleanValue(context)
            .FromJust());

  // Instance accessor should not be visible on function object or its prototype
  EXPECT_TRUE(
      CompileRun("s.knurd == undefined")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("s.prototype.knurd == undefined")
            ->BooleanValue(context)
            .FromJust());
  EXPECT_TRUE(CompileRun("base1.prototype.knurd == undefined")
            ->BooleanValue(context)
            .FromJust());

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"), base1->GetFunction(context)
                                                  .ToLocalChecked()
                                                  ->NewInstance(context)
                                                  .ToLocalChecked())
            .FromJust());
  EXPECT_EQ(17.2,
           CompileRun("obj.flabby()")->NumberValue(context).FromJust());
  EXPECT_TRUE(CompileRun("'flabby' in obj")->BooleanValue(context).FromJust());
  EXPECT_EQ(15.2, CompileRun("obj.knurd")->NumberValue(context).FromJust());
  EXPECT_TRUE(CompileRun("'knurd' in obj")->BooleanValue(context).FromJust());
  EXPECT_EQ(20.1, CompileRun("obj.v1")->NumberValue(context).FromJust());

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj2"), base2->GetFunction(context)
                                                   .ToLocalChecked()
                                                   ->NewInstance(context)
                                                   .ToLocalChecked())
            .FromJust());
  EXPECT_EQ(17.2,
           CompileRun("obj2.flabby()")->NumberValue(context).FromJust());
  EXPECT_TRUE(CompileRun("'flabby' in obj2")->BooleanValue(context).FromJust());
  EXPECT_EQ(15.2, CompileRun("obj2.knurd")->NumberValue(context).FromJust());
  EXPECT_TRUE(CompileRun("'knurd' in obj2")->BooleanValue(context).FromJust());
  EXPECT_EQ(10.1, CompileRun("obj2.v2")->NumberValue(context).FromJust());

  // base1 and base2 cannot cross reference to each's prototype
  EXPECT_TRUE(CompileRun("obj.v2")->IsUndefined());
  EXPECT_TRUE(CompileRun("obj2.v1")->IsUndefined());
}

// We skip test-api.cc's DescriptorInheritance2 test because that's testing
// SetNativeDataProperty, which we don't need/have so far.

TEST(SpiderShim, FunctionPrototype) {
  // Based on the V8 test-api.cc FunctionPrototype test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> Foo = FunctionTemplate::New(isolate);
  Foo->PrototypeTemplate()->Set(v8_str("plak"), v8_num(321));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("Foo"),
                  Foo->GetFunction(context).ToLocalChecked())
            .FromJust());
  Local<Script> script = v8_compile("Foo.prototype.plak");
  EXPECT_EQ(v8_run_int32value(script), 321);
}

static void InstanceFunctionCallback(
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(v8_num(12));
}


TEST(SpiderShim, InstanceProperties) {
  // Based on the V8 test-api.cc InstanceProperties test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> t = FunctionTemplate::New(isolate);
  Local<ObjectTemplate> instance = t->InstanceTemplate();

  instance->Set(v8_str("x"), v8_num(42));
  instance->Set(v8_str("f"),
                FunctionTemplate::New(isolate, InstanceFunctionCallback));

  Local<Value> o = t->GetFunction(context)
                       .ToLocalChecked()
                       ->NewInstance(context)
                       .ToLocalChecked();

  EXPECT_TRUE(context->Global()->Set(context, v8_str("i"), o).FromJust());
  Local<Value> value = CompileRun("i.x");
  EXPECT_EQ(42, value->Int32Value(context).FromJust());

  value = CompileRun("i.f()");
  EXPECT_EQ(12, value->Int32Value(context).FromJust());
}

TEST(SpiderShim, Constructor) {
  // Based on the V8 test-api.cc Constructor test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> templ = FunctionTemplate::New(isolate);
  templ->SetClassName(v8_str("Fun"));
  Local<Function> cons = templ->GetFunction(context).ToLocalChecked();
  EXPECT_TRUE(
      context->Global()->Set(context, v8_str("Fun"), cons).FromJust());
  Local<Object> inst = cons->NewInstance(context).ToLocalChecked();
  Local<Value> value = CompileRun("(new Fun()).constructor === Fun");
  EXPECT_TRUE(value->BooleanValue(context).FromJust());
}

static void IsConstructHandler(
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(args.IsConstructCall());
}

TEST(SpiderShim, IsConstructCall) {
  // Based on the V8 test-api.cc IsConstructCall test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  // Function template with call handler.
  Local<FunctionTemplate> templ = FunctionTemplate::New(isolate);
  templ->SetCallHandler(IsConstructHandler);

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("f"),
                  templ->GetFunction(context).ToLocalChecked())
            .FromJust());
  Local<Value> value = v8_compile("f()")->Run(context).ToLocalChecked();
  EXPECT_TRUE(!value->BooleanValue(context).FromJust());
  value = v8_compile("new f()")->Run(context).ToLocalChecked();
  EXPECT_TRUE(value->BooleanValue(context).FromJust());
}

static void FunctionNameCallback(
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(v8_num(42));
}


TEST(SpiderShim, CallbackFunctionName) {
  // Based on the V8 test-api.cc CallbackFunctionName test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<ObjectTemplate> t = ObjectTemplate::New(isolate);
  t->Set(v8_str("asdf"),
         FunctionTemplate::New(isolate, FunctionNameCallback));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"),
                  t->NewInstance(context).ToLocalChecked())
            .FromJust());
  Local<Value> value = CompileRun("obj.asdf.name");
  EXPECT_TRUE(value->IsString());
  String::Utf8Value name(value);
  EXPECT_EQ(0, strcmp("asdf", *name));
}

static void ArgumentsTestCallback(
    const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  EXPECT_EQ(3, args.Length());
  EXPECT_TRUE(Integer::New(isolate, 1)->Equals(context, args[0]).FromJust());
  EXPECT_TRUE(Integer::New(isolate, 2)->Equals(context, args[1]).FromJust());
  EXPECT_TRUE(Integer::New(isolate, 3)->Equals(context, args[2]).FromJust());
  EXPECT_TRUE(Undefined(isolate)->Equals(context, args[3]).FromJust());
}

TEST(SpiderShim, Arguments) {
  // Based on the V8 test-api.cc Arguments test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  context->Global()->Set(v8_str("f"),
			 FunctionTemplate::New(isolate, ArgumentsTestCallback)->GetFunction());
  v8_compile("f(1, 2, 3)")->Run(context).ToLocalChecked();
}

static int p_getter_count;
static int p_getter_count2;

static void PGetter(Local<Name> name,
                    const PropertyCallbackInfo<Value>& info) {
  p_getter_count++;
  Local<Context> context = info.GetIsolate()->GetCurrentContext();
  Local<Object> global = context->Global();
  ASSERT_TRUE(
      info.Holder()
          ->Equals(context, global->Get(context, v8_str("o1")).ToLocalChecked())
          .FromJust());
  if (name->Equals(context, v8_str("p1")).FromJust()) {
    ASSERT_TRUE(info.This()
              ->Equals(context,
                       global->Get(context, v8_str("o1")).ToLocalChecked())
              .FromJust());
  } else if (name->Equals(context, v8_str("p2")).FromJust()) {
    ASSERT_TRUE(info.This()
              ->Equals(context,
                       global->Get(context, v8_str("o2")).ToLocalChecked())
              .FromJust());
  } else if (name->Equals(context, v8_str("p3")).FromJust()) {
    ASSERT_TRUE(info.This()
              ->Equals(context,
                       global->Get(context, v8_str("o3")).ToLocalChecked())
              .FromJust());
  } else if (name->Equals(context, v8_str("p4")).FromJust()) {
    ASSERT_TRUE(info.This()
              ->Equals(context,
                       global->Get(context, v8_str("o4")).ToLocalChecked())
              .FromJust());
  }
}

static void RunHolderTest(Local<Context> context, Local<ObjectTemplate> obj) {
  ASSERT_TRUE(context->Global()
            ->Set(context, v8_str("o1"),
                  obj->NewInstance(context).ToLocalChecked())
            .FromJust());
  CompileRun(
    "o1.__proto__ = { };"
    "var o2 = { __proto__: o1 };"
    "var o3 = { __proto__: o2 };"
    "var o4 = { __proto__: o3 };"
    "for (var i = 0; i < 10; i++) o4.p4;"
    "for (var i = 0; i < 10; i++) o3.p3;"
    "for (var i = 0; i < 10; i++) o2.p2;"
    "for (var i = 0; i < 10; i++) o1.p1;");
}


static void PGetter2(Local<Name> name,
                     const PropertyCallbackInfo<Value>& info) {
  p_getter_count2++;
  Local<Context> context = info.GetIsolate()->GetCurrentContext();
  Local<Object> global = context->Global();
  ASSERT_TRUE(
      info.Holder()
          ->Equals(context, global->Get(context, v8_str("o1")).ToLocalChecked())
          .FromJust());
  if (name->Equals(context, v8_str("p1")).FromJust()) {
    ASSERT_TRUE(info.This()
              ->Equals(context,
                       global->Get(context, v8_str("o1")).ToLocalChecked())
              .FromJust());
  } else if (name->Equals(context, v8_str("p2")).FromJust()) {
    ASSERT_TRUE(info.This()
              ->Equals(context,
                       global->Get(context, v8_str("o2")).ToLocalChecked())
              .FromJust());
  } else if (name->Equals(context, v8_str("p3")).FromJust()) {
    ASSERT_TRUE(info.This()
              ->Equals(context,
                       global->Get(context, v8_str("o3")).ToLocalChecked())
              .FromJust());
  } else if (name->Equals(context, v8_str("p4")).FromJust()) {
    ASSERT_TRUE(info.This()
              ->Equals(context,
                       global->Get(context, v8_str("o4")).ToLocalChecked())
              .FromJust());
  }
}


#if 0
// Skip the GetterHolders test for now, because we don't implement the holder
// stuff: we just always pass `this` for the `holder` to callbacks.
TEST(SpiderShim, GetterHolders) {
  // Based on the V8 test-api.cc GetterHolders test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("p1"), PGetter);
  obj->SetAccessor(v8_str("p2"), PGetter);
  obj->SetAccessor(v8_str("p3"), PGetter);
  obj->SetAccessor(v8_str("p4"), PGetter);
  p_getter_count = 0;
  RunHolderTest(context, obj);
  ASSERT_EQ(40, p_getter_count);
}
#endif

TEST(SpiderShim, ObjectInstantiation) {
  // Based on the V8 test-api.cc ObjectInstantiation test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("t"), PGetter2);
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("o"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  for (int i = 0; i < 100; i++) {
    HandleScope inner_scope(isolate);
    Local<Object> obj =
        templ->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(!obj->Equals(context, context->Global()
                                            ->Get(context, v8_str("o"))
                                            .ToLocalChecked())
               .FromJust());
    EXPECT_TRUE(
        context->Global()->Set(context, v8_str("o2"), obj).FromJust());
    Local<Value> value = CompileRun("o.__proto__ === o2.__proto__");
    EXPECT_TRUE(True(isolate)->Equals(context, value).FromJust());
    EXPECT_TRUE(context->Global()->Set(context, v8_str("o"), obj).FromJust());
  }
}

static void YGetter(Local<String> name,
                    const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(10));
}


static void YSetter(Local<String> name,
                    Local<Value> value,
                    const PropertyCallbackInfo<void>& info) {
  Local<Object> this_obj = Local<Object>::Cast(info.This());
  Local<Context> context = info.GetIsolate()->GetCurrentContext();
  if (this_obj->Has(context, name).FromJust())
    this_obj->Delete(context, name).FromJust();
  EXPECT_TRUE(this_obj->Set(context, name, value).FromJust());
}


TEST(SpiderShim, DeleteAccessor) {
  // Based on the V8 test-api.cc DeleteAccessor test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<ObjectTemplate> obj = ObjectTemplate::New(isolate);
  obj->SetAccessor(v8_str("y"), YGetter, YSetter);
  Local<Object> holder =
      obj->NewInstance(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("holder"), holder)
            .FromJust());
  Local<Value> result =
      CompileRun("holder.y = 11; holder.y = 12; holder.y");
  EXPECT_EQ(12u, result->Uint32Value(context).FromJust());
}

static void ParentGetter(Local<String> name,
                         const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(1));
}


static void ChildGetter(Local<String> name,
                        const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(42));
}


TEST(SpiderShim, Overriding) {
  // Based on the V8 test-api.cc Overriding test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  // Parent template.
  Local<FunctionTemplate> parent_templ = FunctionTemplate::New(isolate);
  Local<ObjectTemplate> parent_instance_templ =
      parent_templ->InstanceTemplate();
  parent_instance_templ->SetAccessor(v8_str("f"), ParentGetter);

  // Template that inherits from the parent template.
  Local<FunctionTemplate> child_templ = FunctionTemplate::New(isolate);
  Local<ObjectTemplate> child_instance_templ =
      child_templ->InstanceTemplate();
  child_templ->Inherit(parent_templ);
  // Override 'f'.  The child version of 'f' should get called for child
  // instances.
  child_instance_templ->SetAccessor(v8_str("f"), ChildGetter);
  // Add 'g' twice.  The 'g' added last should get called for instances.
  child_instance_templ->SetAccessor(v8_str("g"), ParentGetter);
  child_instance_templ->SetAccessor(v8_str("g"), ChildGetter);

  // Add 'h' as an accessor to the proto template with ReadOnly attributes
  // so 'h' can be shadowed on the instance object.
  Local<ObjectTemplate> child_proto_templ = child_templ->PrototypeTemplate();
  child_proto_templ->SetAccessor(v8_str("h"), ParentGetter, 0,
                                 Local<Value>(), DEFAULT, ReadOnly);

  // Add 'i' as an accessor to the instance template with ReadOnly attributes
  // but the attribute does not have effect because it is duplicated with
  // NULL setter.
  child_instance_templ->SetAccessor(v8_str("i"), ChildGetter, 0,
                                    Local<Value>(), DEFAULT,
                                    ReadOnly);


  // Instantiate the child template.
  Local<Object> instance = child_templ->GetFunction(context)
                                   .ToLocalChecked()
                                   ->NewInstance(context)
                                   .ToLocalChecked();

  // Check that the child function overrides the parent one.
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("o"), instance)
            .FromJust());
  Local<Value> value = v8_compile("o.f")->Run(context).ToLocalChecked();
  // Check that the 'g' that was added last is hit.
  EXPECT_EQ(42, value->Int32Value(context).FromJust());
  value = v8_compile("o.g")->Run(context).ToLocalChecked();
  EXPECT_EQ(42, value->Int32Value(context).FromJust());

  // Check that 'h' cannot be shadowed.
  value = v8_compile("o.h = 3; o.h")->Run(context).ToLocalChecked();
  EXPECT_EQ(1, value->Int32Value(context).FromJust());

  // Check that 'i' cannot be shadowed or changed.
  value = v8_compile("o.i = 3; o.i")->Run(context).ToLocalChecked();
  EXPECT_EQ(42, value->Int32Value(context).FromJust());
}

static void GetterWhichReturns42(
    Local<String> name,
    const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_num(42));
}


static void SetterWhichSetsYOnThisTo23(
    Local<String> name,
    Local<Value> value,
    const PropertyCallbackInfo<void>& info) {
  Local<Object>::Cast(info.This())
      ->Set(info.GetIsolate()->GetCurrentContext(), v8_str("y"), v8_num(23))
      .FromJust();
}

TEST(SpiderShim, SetterOnConstructorPrototype) {
  // Based on the V8 test-api.cc SetterOnConstructorPrototype test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("x"), GetterWhichReturns42,
                     SetterWhichSetsYOnThisTo23);
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("P"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  CompileRun("function C1() {"
             "  this.x = 23;"
             "};"
             "C1.prototype = P;"
             "function C2() {"
             "  this.x = 23"
             "};"
             "C2.prototype = { };"
             "C2.prototype.__proto__ = P;");

  Local<Script> script;
  script = v8_compile("new C1();");
  for (int i = 0; i < 10; i++) {
    Local<Object> c1 = Local<Object>::Cast(
        script->Run(context).ToLocalChecked());
    EXPECT_EQ(42, c1->Get(context, v8_str("x"))
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
    EXPECT_EQ(23, c1->Get(context, v8_str("y"))
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  }

  script = v8_compile("new C2();");
  for (int i = 0; i < 10; i++) {
    Local<Object> c2 = Local<Object>::Cast(
        script->Run(context).ToLocalChecked());
    EXPECT_EQ(42, c2->Get(context, v8_str("x"))
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
    EXPECT_EQ(23, c2->Get(context, v8_str("y"))
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust());
  }
}

void Getter_41(Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(41);
}

void Getter_42(Local<String> property, const PropertyCallbackInfo<Value>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
  info.GetReturnValue().Set(42);
}

void Setter_43(Local<String> property, Local<Value> val, const PropertyCallbackInfo<Value>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
  EXPECT_EQ(50, Int32::Cast(*val)->Value());
  info.GetReturnValue().Set(43);
}

void DeleterSuccess(Local<String> property, const PropertyCallbackInfo<Boolean>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
  info.GetReturnValue().Set(true);
}

void Getter_54(uint32_t property, const PropertyCallbackInfo<Value>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
  info.GetReturnValue().Set(54);
}

void Setter_55(uint32_t property, Local<Value> val, const PropertyCallbackInfo<Value>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
  EXPECT_EQ(50, Int32::Cast(*val)->Value());
  info.GetReturnValue().Set(55);
}

void DeleterFailure(uint32_t property, const PropertyCallbackInfo<Boolean>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
  info.GetReturnValue().Set(false);
}

void DeleterFailure2(uint32_t property, const PropertyCallbackInfo<Boolean>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
}

void Enum(const PropertyCallbackInfo<Array>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
  Local<Array> arr = Array::New(info.GetIsolate(), 3);
  arr->Set(0, String::NewFromUtf8(info.GetIsolate(), "foopy"));
  arr->Set(1, String::NewFromUtf8(info.GetIsolate(), "eknard"));
  arr->Set(2, String::NewFromUtf8(info.GetIsolate(), "slirp"));
  info.GetReturnValue().Set(arr);
}

void EnumInt(const PropertyCallbackInfo<Array>& info) {
  EXPECT_EQ(5, Int32::Cast(*info.Data())->Value());
  Local<Array> arr = Array::New(info.GetIsolate(), 2);
  arr->Set(0, Int32::New(info.GetIsolate(), 10));
  arr->Set(1, Int32::New(info.GetIsolate(), 12));
  info.GetReturnValue().Set(arr);
}

TEST(SpiderShim, PropertyHandlers) {
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  Local<Value> five = Int32::New(engine.isolate(), 5);
  templ->SetHandler(NamedPropertyHandlerConfiguration(Getter_41));
  templ->SetNamedPropertyHandler(Getter_42, Setter_43, nullptr, DeleterSuccess, Enum, five);
  templ->SetIndexedPropertyHandler(Getter_54, Setter_55, nullptr, DeleterFailure, EnumInt, five);
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("P"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  Local<Value> val = CompileRun("var sum = ''; for (var x in P) sum += x; sum;");
  String::Utf8Value utf8(val->ToString());
  EXPECT_STREQ("1012foopyeknardslirp", *utf8);
  val = CompileRun("P.foo;");
  EXPECT_EQ(42, Int32::Cast(*val)->Value());
  val = CompileRun("P.bar = 50;");
  EXPECT_EQ(50, Int32::Cast(*val)->Value());
  val = CompileRun("delete P.baz;");
  EXPECT_TRUE(val->IsTrue());
  val = CompileRun("P[4];");
  EXPECT_EQ(54, Int32::Cast(*val)->Value());
  val = CompileRun("P[40] = 50;");
  EXPECT_EQ(50, Int32::Cast(*val)->Value());
  val = CompileRun("delete P[400];");
  EXPECT_TRUE(val->IsFalse());

  Local<ObjectTemplate> templ2 = ObjectTemplate::New(isolate);
  templ2->SetHandler(NamedPropertyHandlerConfiguration(Getter_41, nullptr, nullptr, nullptr, Enum, five));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("Q"),
                  templ2->NewInstance(context).ToLocalChecked())
            .FromJust());
  val = CompileRun("var sum = ''; for (var x in Q) sum += x; sum;");
  String::Utf8Value utf8_2(val->ToString());
  EXPECT_STREQ("foopyeknardslirp", *utf8_2);
  val = CompileRun("Q.foo;");
  EXPECT_EQ(41, Int32::Cast(*val)->Value());
  val = CompileRun("Q.bar = 50;");
  EXPECT_EQ(50, Int32::Cast(*val)->Value());
  val = CompileRun("Q.foo;");
  EXPECT_EQ(41, Int32::Cast(*val)->Value());
}

static void ShadowFunctionCallback(
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(v8_num(42));
}

static int shadow_y;
static int shadow_y_setter_call_count;
static int shadow_y_getter_call_count;

static void ShadowYSetter(Local<String>,
                          Local<Value>,
                          const PropertyCallbackInfo<void>&) {
  shadow_y_setter_call_count++;
  shadow_y = 42;
}

static void ShadowYGetter(Local<String> name,
                          const PropertyCallbackInfo<Value>& info) {
  shadow_y_getter_call_count++;
  info.GetReturnValue().Set(v8_num(shadow_y));
}

static void ShadowIndexedGet(uint32_t index,
                             const PropertyCallbackInfo<Value>&) {
}

static void ShadowNamedGet(Local<Name> key,
                           const PropertyCallbackInfo<Value>&) {}

TEST(SpiderShim, ShadowObject) {
  // Based on the V8 test-api.cc ShadowObject test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  shadow_y = shadow_y_setter_call_count = shadow_y_getter_call_count = 0;

  Local<FunctionTemplate> t = FunctionTemplate::New(isolate);
  t->InstanceTemplate()->SetHandler(
      NamedPropertyHandlerConfiguration(ShadowNamedGet));
  t->InstanceTemplate()->SetHandler(
      IndexedPropertyHandlerConfiguration(ShadowIndexedGet));
  Local<ObjectTemplate> proto = t->PrototypeTemplate();
  Local<ObjectTemplate> instance = t->InstanceTemplate();

  proto->Set(v8_str("f"),
             FunctionTemplate::New(isolate,
                                       ShadowFunctionCallback,
                                       Local<Value>()));
  proto->Set(v8_str("x"), v8_num(12));

  instance->SetAccessor(v8_str("y"), ShadowYGetter, ShadowYSetter);

  Local<Value> o = t->GetFunction(context)
                       .ToLocalChecked()
                       ->NewInstance(context)
                       .ToLocalChecked();
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("__proto__"), o)
            .FromJust());

  Local<Value> value =
      CompileRun("this.propertyIsEnumerable(0)");
  EXPECT_TRUE(value->IsBoolean());
  EXPECT_TRUE(!value->BooleanValue(context).FromJust());

  value = CompileRun("x");
  EXPECT_EQ(12, value->Int32Value(context).FromJust());

  value = CompileRun("f()");
  EXPECT_EQ(42, value->Int32Value(context).FromJust());

  CompileRun("y = 43");
  EXPECT_EQ(1, shadow_y_setter_call_count);
  value = CompileRun("y");
  EXPECT_EQ(1, shadow_y_getter_call_count);
  EXPECT_EQ(42, value->Int32Value(context).FromJust());
}

void HasOwnPropertyIndexedPropertyGetter(
    uint32_t index,
    const PropertyCallbackInfo<Value>& info) {
  if (index == 42) info.GetReturnValue().Set(v8_str("yes"));
}

void HasOwnPropertyNamedPropertyGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  if (property->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("foo"))
          .FromJust()) {
    info.GetReturnValue().Set(v8_str("yes"));
  }
}

void HasOwnPropertyIndexedPropertyQuery(
    uint32_t index, const PropertyCallbackInfo<Integer>& info) {
  if (index == 42) info.GetReturnValue().Set(1);
}

void HasOwnPropertyNamedPropertyQuery(
    Local<Name> property, const PropertyCallbackInfo<Integer>& info) {
  if (property->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("foo"))
          .FromJust()) {
    info.GetReturnValue().Set(1);
  }
}

void HasOwnPropertyNamedPropertyQuery2(
    Local<Name> property, const PropertyCallbackInfo<Integer>& info) {
  if (property->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("bar"))
          .FromJust()) {
    info.GetReturnValue().Set(1);
  }
}

void HasOwnPropertyAccessorGetter(
    Local<String> property,
    const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(v8_str("yes"));
}

TEST(SpiderShim, HasOwnProperty) {
  // Based on the V8 test-api.cc HasOwnProperty test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  { // Check normal properties and defined getters.
    Local<Value> value = CompileRun(
        "function Foo() {"
        "    this.foo = 11;"
        "    this.__defineGetter__('baz', function() { return 1; });"
        "};"
        "function Bar() { "
        "    this.bar = 13;"
        "    this.__defineGetter__('bla', function() { return 2; });"
        "};"
        "Bar.prototype = new Foo();"
        "new Bar();");
    EXPECT_TRUE(value->IsObject());
    Local<Object> object = value->ToObject(context).ToLocalChecked();
    EXPECT_TRUE(object->Has(context, v8_str("foo")).FromJust());
    EXPECT_TRUE(!object->HasOwnProperty(context, v8_str("foo")).FromJust());
    EXPECT_TRUE(object->HasOwnProperty(context, v8_str("bar")).FromJust());
    EXPECT_TRUE(object->Has(context, v8_str("baz")).FromJust());
    EXPECT_TRUE(!object->HasOwnProperty(context, v8_str("baz")).FromJust());
    EXPECT_TRUE(object->HasOwnProperty(context, v8_str("bla")).FromJust());
  }
  { // Check named getter interceptors.
    Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
    templ->SetHandler(NamedPropertyHandlerConfiguration(
        HasOwnPropertyNamedPropertyGetter));
    Local<Object> instance = templ->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(!instance->HasOwnProperty(context, v8_str("42")).FromJust());
    EXPECT_TRUE(instance->HasOwnProperty(context, v8_str("foo")).FromJust());
    EXPECT_TRUE(!instance->HasOwnProperty(context, v8_str("bar")).FromJust());
  }
  { // Check indexed getter interceptors.
    Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
    templ->SetHandler(IndexedPropertyHandlerConfiguration(
        HasOwnPropertyIndexedPropertyGetter));
    Local<Object> instance = templ->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(instance->HasOwnProperty(context, v8_str("42")).FromJust());
    EXPECT_TRUE(!instance->HasOwnProperty(context, v8_str("43")).FromJust());
    EXPECT_TRUE(!instance->HasOwnProperty(context, v8_str("foo")).FromJust());
  }
  { // Check named query interceptors.
    Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
    templ->SetHandler(NamedPropertyHandlerConfiguration(
        0, 0, HasOwnPropertyNamedPropertyQuery));
    Local<Object> instance = templ->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(instance->HasOwnProperty(context, v8_str("foo")).FromJust());
    EXPECT_TRUE(!instance->HasOwnProperty(context, v8_str("bar")).FromJust());
  }
  { // Check indexed query interceptors.
    Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
    templ->SetHandler(IndexedPropertyHandlerConfiguration(
        0, 0, HasOwnPropertyIndexedPropertyQuery));
    Local<Object> instance = templ->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(instance->HasOwnProperty(context, v8_str("42")).FromJust());
    EXPECT_TRUE(!instance->HasOwnProperty(context, v8_str("41")).FromJust());
  }
  { // Check callbacks.
    Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
    templ->SetAccessor(v8_str("foo"), HasOwnPropertyAccessorGetter);
    Local<Object> instance = templ->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(instance->HasOwnProperty(context, v8_str("foo")).FromJust());
    EXPECT_TRUE(!instance->HasOwnProperty(context, v8_str("bar")).FromJust());
  }
  { // Check that query wins on disagreement.
    Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
    templ->SetHandler(NamedPropertyHandlerConfiguration(
        HasOwnPropertyNamedPropertyGetter, 0,
        HasOwnPropertyNamedPropertyQuery2));
    Local<Object> instance = templ->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(!instance->HasOwnProperty(context, v8_str("foo")).FromJust());
    EXPECT_TRUE(instance->HasOwnProperty(context, v8_str("bar")).FromJust());
  }
}

TEST(SpiderShim, IndexedInterceptorWithStringProto) {
  // Based on the V8 test-api.cc IndexedInterceptorWithStringProto test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->SetHandler(IndexedPropertyHandlerConfiguration(
      NULL, NULL, HasOwnPropertyIndexedPropertyQuery));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("obj"),
                  templ->NewInstance(context).ToLocalChecked())
            .FromJust());
  CompileRun("var s = new String('foobar'); obj.__proto__ = s;");
  // These should be intercepted.
  EXPECT_TRUE(CompileRun("42 in obj")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("'42' in obj")->BooleanValue(context).FromJust());
  // These should fall through to the String prototype.
  EXPECT_TRUE(CompileRun("0 in obj")->BooleanValue(context).FromJust());
  EXPECT_TRUE(CompileRun("'0' in obj")->BooleanValue(context).FromJust());
  // And these should both fail.
  EXPECT_TRUE(!CompileRun("32 in obj")->BooleanValue(context).FromJust());
  EXPECT_TRUE(!CompileRun("'32' in obj")->BooleanValue(context).FromJust());
}

static void EmptyInterceptorGetter(
    Local<String> name, const PropertyCallbackInfo<Value>& info) {}

static void EmptyInterceptorSetter(
    Local<String> name, Local<Value> value,
    const PropertyCallbackInfo<Value>& info) {}

TEST(SpiderShim, InstanceCheckOnInstanceAccessorWithInterceptor) {
  // Based on the V8 test-api.cc InstanceCheckOnInstanceAccessorWithInterceptor test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<FunctionTemplate> templ = FunctionTemplate::New(context->GetIsolate());
  Local<ObjectTemplate> inst = templ->InstanceTemplate();
  templ->InstanceTemplate()->SetNamedPropertyHandler(EmptyInterceptorGetter,
                                                     EmptyInterceptorSetter);
  inst->SetAccessor(v8_str("foo"), InstanceCheckedGetter, InstanceCheckedSetter,
                    Local<Value>(), DEFAULT, None,
                    AccessorSignature::New(context->GetIsolate(), templ));
  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("f"),
                  templ->GetFunction(context).ToLocalChecked())
            .FromJust());

  printf("Testing positive ...\n");
  CompileRun("var obj = new f();");
  EXPECT_TRUE(templ->HasInstance(
      context->Global()->Get(context, v8_str("obj")).ToLocalChecked()));
  CheckInstanceCheckedAccessors(true);

  printf("Testing negative ...\n");
  CompileRun("var obj = {};"
             "obj.__proto__ = new f();");
  EXPECT_TRUE(!templ->HasInstance(
      context->Global()->Get(context, v8_str("obj")).ToLocalChecked()));
  CheckInstanceCheckedAccessors(false);
}

void ThrowValue(const v8::FunctionCallbackInfo<v8::Value>& args) {
  EXPECT_EQ(1, args.Length());
  args.GetIsolate()->ThrowException(args[0]);
}

static void ConstructorCallback(
    const FunctionCallbackInfo<Value>& args) {
  Local<Object> This;

  Local<Context> context = args.GetIsolate()->GetCurrentContext();
  if (args.IsConstructCall()) {
    Local<Object> Holder = args.Holder();
    This = Object::New(args.GetIsolate());
    Local<Value> proto = Holder->GetPrototype();
    if (proto->IsObject()) {
      This->SetPrototype(context, proto).FromJust();
    }
  } else {
    This = args.This();
  }

  This->Set(context, v8_str("a"), args[0]).FromJust();
  args.GetReturnValue().Set(This);
}

static void FakeConstructorCallback(
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(args[0]);
}

TEST(SpiderShim, ConstructorForObject) {
  // Based on the V8 test-api.cc ConstructorForObject test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  {
    Local<ObjectTemplate> instance_template = ObjectTemplate::New(isolate);
    instance_template->SetCallAsFunctionHandler(ConstructorCallback);
    Local<Object> instance =
        instance_template->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(context->Global()
              ->Set(context, v8_str("obj"), instance)
              .FromJust());
    TryCatch try_catch(isolate);
    Local<Value> value;
    EXPECT_TRUE(!try_catch.HasCaught());

    // Call the Object's constructor with a 32-bit signed integer.
    value = CompileRun("(function() { var o = new obj(28); return o.a; })()");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsInt32());
    EXPECT_EQ(28, value->Int32Value(context).FromJust());

    Local<Value> args1[] = {v8_num(28)};
    Local<Value> value_obj1 =
        instance->CallAsConstructor(context, 1, args1).ToLocalChecked();
    EXPECT_TRUE(value_obj1->IsObject());
    Local<Object> object1 = Local<Object>::Cast(value_obj1);
    value = object1->Get(context, v8_str("a")).ToLocalChecked();
    EXPECT_TRUE(value->IsInt32());
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_EQ(28, value->Int32Value(context).FromJust());

    // Call the Object's constructor with a String.
    value =
        CompileRun("(function() { var o = new obj('tipli'); return o.a; })()");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsString());
    String::Utf8Value string_value1(
        value->ToString(context).ToLocalChecked());
    EXPECT_EQ(0, strcmp("tipli", *string_value1));

    Local<Value> args2[] = {v8_str("tipli")};
    Local<Value> value_obj2 =
        instance->CallAsConstructor(context, 1, args2).ToLocalChecked();
    EXPECT_TRUE(value_obj2->IsObject());
    Local<Object> object2 = Local<Object>::Cast(value_obj2);
    value = object2->Get(context, v8_str("a")).ToLocalChecked();
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsString());
    String::Utf8Value string_value2(
        value->ToString(context).ToLocalChecked());
    EXPECT_EQ(0, strcmp("tipli", *string_value2));

    // Call the Object's constructor with a Boolean.
    value = CompileRun("(function() { var o = new obj(true); return o.a; })()");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsBoolean());
    EXPECT_EQ(true, value->BooleanValue(context).FromJust());

    Local<Value> args3[] = {True(isolate)};
    Local<Value> value_obj3 =
        instance->CallAsConstructor(context, 1, args3).ToLocalChecked();
    EXPECT_TRUE(value_obj3->IsObject());
    Local<Object> object3 = Local<Object>::Cast(value_obj3);
    value = object3->Get(context, v8_str("a")).ToLocalChecked();
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsBoolean());
    EXPECT_EQ(true, value->BooleanValue(context).FromJust());

    // Call the Object's constructor with undefined.
    Local<Value> args4[] = {Undefined(isolate)};
    Local<Value> value_obj4 =
        instance->CallAsConstructor(context, 1, args4).ToLocalChecked();
    EXPECT_TRUE(value_obj4->IsObject());
    Local<Object> object4 = Local<Object>::Cast(value_obj4);
    value = object4->Get(context, v8_str("a")).ToLocalChecked();
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsUndefined());

    // Call the Object's constructor with null.
    Local<Value> args5[] = {Null(isolate)};
    Local<Value> value_obj5 =
        instance->CallAsConstructor(context, 1, args5).ToLocalChecked();
    EXPECT_TRUE(value_obj5->IsObject());
    Local<Object> object5 = Local<Object>::Cast(value_obj5);
    value = object5->Get(context, v8_str("a")).ToLocalChecked();
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsNull());
  }

  // Check exception handling when there is no constructor set for the Object.
  {
    Local<ObjectTemplate> instance_template = ObjectTemplate::New(isolate);
    Local<Object> instance =
        instance_template->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(context->Global()
              ->Set(context, v8_str("obj2"), instance)
              .FromJust());
    TryCatch try_catch(isolate);
    Local<Value> value;
    EXPECT_TRUE(!try_catch.HasCaught());

    value = CompileRun("new obj2(28)");
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value1(try_catch.Exception());
    EXPECT_STREQ(
        "TypeError: obj2 is not a constructor", *exception_value1);
    try_catch.Reset();

    Local<Value> args[] = {v8_num(29)};
    EXPECT_TRUE(instance->CallAsConstructor(context, 1, args).IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value2(try_catch.Exception());
    EXPECT_STREQ(
        "TypeError: ({}) is not a constructor", *exception_value2);
    try_catch.Reset();
  }

  // Check the case when constructor throws exception.
  {
    Local<ObjectTemplate> instance_template = ObjectTemplate::New(isolate);
    instance_template->SetCallAsFunctionHandler(ThrowValue);
    Local<Object> instance =
        instance_template->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(context->Global()
              ->Set(context, v8_str("obj3"), instance)
              .FromJust());
    TryCatch try_catch(isolate);
    Local<Value> value;
    EXPECT_TRUE(!try_catch.HasCaught());

    value = CompileRun("new obj3(22)");
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value1(try_catch.Exception());
    EXPECT_EQ(0, strcmp("22", *exception_value1));
    try_catch.Reset();

    Local<Value> args[] = {v8_num(23)};
    EXPECT_TRUE(instance->CallAsConstructor(context, 1, args).IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value exception_value2(try_catch.Exception());
    EXPECT_EQ(0, strcmp("23", *exception_value2));
    try_catch.Reset();
  }

  // Check whether constructor returns with an object or non-object.
  {
    Local<FunctionTemplate> function_template =
        FunctionTemplate::New(isolate, FakeConstructorCallback);
    Local<Function> function =
        function_template->GetFunction(context).ToLocalChecked();
    Local<Object> instance1 = function;
    EXPECT_TRUE(context->Global()
              ->Set(context, v8_str("obj4"), instance1)
              .FromJust());
    TryCatch try_catch(isolate);
    Local<Value> value;
    EXPECT_TRUE(!try_catch.HasCaught());

    EXPECT_TRUE(instance1->IsObject());
    EXPECT_TRUE(instance1->IsFunction());

    value = CompileRun("new obj4(28)");
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsObject());

    Local<Value> args1[] = {v8_num(28)};
    value = instance1->CallAsConstructor(context, 1, args1)
                .ToLocalChecked();
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(value->IsObject());

    Local<ObjectTemplate> instance_template = ObjectTemplate::New(isolate);
    instance_template->SetCallAsFunctionHandler(FakeConstructorCallback);
    Local<Object> instance2 =
        instance_template->NewInstance(context).ToLocalChecked();
    EXPECT_TRUE(context->Global()
              ->Set(context, v8_str("obj5"), instance2)
              .FromJust());
    EXPECT_TRUE(!try_catch.HasCaught());

    EXPECT_TRUE(instance2->IsObject());
    EXPECT_TRUE(instance2->IsFunction());

    value = CompileRun("new obj5(28)");
    EXPECT_TRUE(!try_catch.HasCaught());
    // Note that V8 would return a Primitive here, but we are intentionally
    // not adhering to it in order to comply with ES6.  See the discussion in
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1287342.
    EXPECT_TRUE(value->IsObject());

    Local<Value> args2[] = {v8_num(28)};
    value = instance2->CallAsConstructor(context, 1, args2)
                .ToLocalChecked();
    EXPECT_TRUE(!try_catch.HasCaught());
    // Note that V8 would return a Primitive here, but we are intentionally
    // not adhering to it in order to comply with ES6.  See the discussion in
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1287342.
    EXPECT_TRUE(value->IsObject());
  }
}
