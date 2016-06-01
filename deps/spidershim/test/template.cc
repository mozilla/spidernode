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
    Local<String> name, const v8::PropertyCallbackInfo<Value>& info) {
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
    Local<v8::FunctionTemplate> fun_templ =
        v8::FunctionTemplate::New(isolate, handle_callback_1);
    Local<Function> fun = fun_templ->GetFunction(context).ToLocalChecked();
    EXPECT_TRUE(context->Global()->Set(context, v8_str("obj"), fun).FromJust());
    Local<Script> script = v8_compile("obj.length");
    EXPECT_EQ(0, v8_run_int32value(script));
  }
}


// Need support for Signature to do FunctionTemplateReceiverSignature.  See
// https://github.com/mozilla/spidernode/issues/144
#if 0
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
