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

  Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(isolate);
  Local<String> class_name = v8_str("the_class_name");
  fun->SetClassName(class_name);
  Local<ObjectTemplate> templ1 = ObjectTemplate::New(isolate, fun);
  templ1->Set(isolate, "x", v8_num(10));
  templ1->Set(isolate, "y", v8_num(13));
  templ1->Set(v8_str("foo"), acc);
  Local<v8::Object> instance1 =
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
  // TEST_TODO: accessors on ObjectTemplate are totally different in node vs tip
  // V8, so test SetAccessor instead of SetAccessorProperty
  //   templ2->SetAccessorProperty(v8_str("acc"), acc);
  templ2->SetAccessor(v8_str("acc"), Gets42);
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

  // TEST_TORO: Enable this part when we switch to SetAccessorProperty() above.
  //  EXPECT_TRUE(engine.CompileRun(context,
  //                      "desc1 = Object.getOwnPropertyDescriptor(q, 'acc');"
  //                      "(desc1.get === acc)")
  //                 ->BooleanValue(context)
  //                 .FromJust());
  //  EXPECT_TRUE(engine.CompileRun(context,
  //                          "desc2 = Object.getOwnPropertyDescriptor(q2, 'acc');"
  //                          "(desc2.get === acc)")
  //            ->BooleanValue(context)
  //            .FromJust());
}

static void GetNirk(Local<String> name,
                    const PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(v8_num(900));
}

static void GetRino(Local<String> name,
                    const PropertyCallbackInfo<v8::Value>& info) {
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
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  signature_callback_count++;
  EXPECT_TRUE(signature_expected_receiver->Equals(
                                       args.GetIsolate()->GetCurrentContext(),
                                       args.Holder())
                .FromJust());
  EXPECT_TRUE(signature_expected_receiver->Equals(
                                       args.GetIsolate()->GetCurrentContext(),
                                       args.This())
                .FromJust());
  v8::Local<v8::Array> result =
      v8::Array::New(args.GetIsolate(), args.Length());
  for (int i = 0; i < args.Length(); i++) {
    EXPECT_TRUE(result->Set(args.GetIsolate()->GetCurrentContext(),
                      v8::Integer::New(args.GetIsolate(), i), args[i])
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
  v8::TryCatch try_catch(isolate);
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

#if 0
TEST(SpiderShim, FunctionTemplateReceiverSignature) {
  // Largely stolen from the V8 test-api.cc ReceiverSignature test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  // Setup templates.
  v8::Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(isolate);
  v8::Local<v8::Signature> sig = v8::Signature::New(isolate, fun);
  v8::Local<v8::FunctionTemplate> callback_sig = v8::FunctionTemplate::New(
      isolate, IncrementingSignatureCallback, Local<Value>(), sig);
  v8::Local<v8::FunctionTemplate> callback =
      v8::FunctionTemplate::New(isolate, IncrementingSignatureCallback);
  v8::Local<v8::FunctionTemplate> sub_fun = v8::FunctionTemplate::New(isolate);
  sub_fun->Inherit(fun);
  v8::Local<v8::FunctionTemplate> unrel_fun =
      v8::FunctionTemplate::New(isolate);
  // Install properties.
  v8::Local<v8::ObjectTemplate> fun_proto = fun->PrototypeTemplate();
  fun_proto->Set(v8_str("prop_sig"), callback_sig);
  fun_proto->Set(v8_str("prop"), callback);
  // No support for SetAccessorProperty yet (and indeed, it doesn't even exist
  // in our V8 version).
  // fun_proto->SetAccessorProperty(
  //     v8_str("accessor_sig"), callback_sig, callback_sig);
  // fun_proto->SetAccessorProperty(v8_str("accessor"), callback, callback);

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
#endif // FunctionTemplateReceiverSignature

TEST(SpiderShim, InternalFields) {
  // Loosely based on the V8 test-api.cc InternalFields test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
  Local<v8::ObjectTemplate> instance_templ = templ->InstanceTemplate();
  EXPECT_EQ(0, instance_templ->InternalFieldCount());
  instance_templ->SetInternalFieldCount(1);
  EXPECT_EQ(1, instance_templ->InternalFieldCount());
  Local<v8::Object> obj = templ->GetFunction(context)
                              .ToLocalChecked()
                              ->NewInstance(context)
                              .ToLocalChecked();
  EXPECT_EQ(1, obj->InternalFieldCount());
  EXPECT_TRUE(obj->GetInternalField(0)->IsUndefined());
  obj->SetInternalField(0, v8_num(17));
  EXPECT_EQ(17, obj->GetInternalField(0)->Int32Value(context).FromJust());
}

static void CheckAlignedPointerInInternalField(Local<v8::Object> obj,
                                               void* value) {
  EXPECT_EQ(0, static_cast<int>(reinterpret_cast<uintptr_t>(value) & 0x1));
  obj->SetAlignedPointerInInternalField(0, value);
  Isolate::GetCurrent()->RequestGarbageCollectionForTesting(kFullGarbageCollection);
  EXPECT_EQ(value, obj->GetAlignedPointerFromInternalField(0));
}


TEST(SpiderShim, InternalFieldsAlignedPointers) {
  // Loosely based on the V8 test-api.cc InternalFieldsAlignedPointers test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
  Local<v8::ObjectTemplate> instance_templ = templ->InstanceTemplate();
  EXPECT_EQ(0, instance_templ->InternalFieldCount());
  instance_templ->SetInternalFieldCount(1);
  EXPECT_EQ(1, instance_templ->InternalFieldCount());
  Local<v8::Object> obj = templ->GetFunction(context)
                              .ToLocalChecked()
                              ->NewInstance(context)
                              .ToLocalChecked();
  EXPECT_EQ(1, obj->InternalFieldCount());

  CheckAlignedPointerInInternalField(obj, NULL);

  int* heap_allocated = new int[100];
  CheckAlignedPointerInInternalField(obj, heap_allocated);
  delete[] heap_allocated;

  int stack_allocated[100];
  CheckAlignedPointerInInternalField(obj, stack_allocated);

  void* huge = reinterpret_cast<void*>(~static_cast<uintptr_t>(1));
  CheckAlignedPointerInInternalField(obj, huge);

  v8::Global<v8::Object> persistent(isolate, obj);
  EXPECT_EQ(1, Object::InternalFieldCount(persistent));
  EXPECT_EQ(huge, Object::GetAlignedPointerFromInternalField(persistent, 0));
}

static int instance_checked_getter_count = 0;
static void InstanceCheckedGetter(
    Local<String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  EXPECT_TRUE(name->Equals(info.GetIsolate()->GetCurrentContext(), v8_str("foo"))
            .FromJust());
  instance_checked_getter_count++;
  info.GetReturnValue().Set(v8_num(11));
}

static int instance_checked_setter_count = 0;
static void InstanceCheckedSetter(Local<String> name,
                      Local<Value> value,
                      const v8::PropertyCallbackInfo<void>& info) {
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
                    Local<Value>(), v8::DEFAULT, v8::None,
                    v8::AccessorSignature::New(context->GetIsolate(), templ));
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
                     InstanceCheckedSetter, Local<Value>(), v8::DEFAULT,
                     v8::None,
                     v8::AccessorSignature::New(context->GetIsolate(), templ));
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

static void SimpleCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
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
  Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
  templ->SetCallHandler(IsConstructHandler);

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("f"),
                  templ->GetFunction(context).ToLocalChecked())
            .FromJust());
  Local<Value> value = v8_compile("f()")->Run(context).ToLocalChecked();
  EXPECT_TRUE(!value->BooleanValue(context).FromJust());
  // TODO: new f() hits a SpiderMonkey assert.  See
  // https://github.com/mozilla/spidernode/issues/157
  // value = v8_compile("new f()")->Run(context).ToLocalChecked();
  // EXPECT_TRUE(value->BooleanValue(context).FromJust());
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
  v8::Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  EXPECT_EQ(3, args.Length());
  EXPECT_TRUE(v8::Integer::New(isolate, 1)->Equals(context, args[0]).FromJust());
  EXPECT_TRUE(v8::Integer::New(isolate, 2)->Equals(context, args[1]).FromJust());
  EXPECT_TRUE(v8::Integer::New(isolate, 3)->Equals(context, args[2]).FromJust());
  EXPECT_TRUE(v8::Undefined(isolate)->Equals(context, args[3]).FromJust());
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
