// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"
#include "jsapi.h"

bool message_received;

static void check_message(v8::Local<v8::Message> message,
                            v8::Local<Value> data) {
  Isolate* isolate = Isolate::GetCurrent();
  EXPECT_TRUE(data->IsNumber());
  EXPECT_EQ(1337,
            data->Int32Value(isolate->GetCurrentContext()).FromJust());
  message_received = true;
}


TEST(SpiderShim, MessageHandler) {
  // This test is based on V8's MessageHandler1, but was re-written to emulate
  // just the functionality node needs from MessageHandlers.
  V8Engine engine;
  message_received = false;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  EXPECT_TRUE(!message_received);
  context->GetIsolate()->AddMessageListener(check_message);

  engine.CompileRun(context, "function Foo() { throw 1337; }");
  Local<Function> Foo = Local<Function>::Cast(
      context->Global()->Get(context, v8_str("Foo")).ToLocalChecked());
  Local<Value>* args0 = NULL;
  MaybeLocal<Value> result = Foo->Call(context, Foo, 0, args0);
  EXPECT_TRUE(result.IsEmpty());

  EXPECT_TRUE(message_received);

  // Now try again with a TryCatch on the stack!
  {
    message_received = false;
    TryCatch try_catch(engine.isolate());
    MaybeLocal<Value> result = Foo->Call(context, Foo, 0, args0);
    EXPECT_TRUE(result.IsEmpty());
    EXPECT_TRUE(!message_received);
  }
  EXPECT_TRUE(!message_received);

  // Now try again with a verbose TryCatch on the stack!
  {
    message_received = false;
    TryCatch try_catch(engine.isolate());
    try_catch.SetVerbose(true);
    MaybeLocal<Value> result = Foo->Call(context, Foo, 0, args0);
    EXPECT_TRUE(result.IsEmpty());
    EXPECT_TRUE(message_received);
  }

  // clear out the message listener
  context->GetIsolate()->RemoveMessageListeners(check_message);
}

TEST(SpiderShim, IsolateEmbedderData) {
  // This test is based on V8's IsolateEmbedderData.
  V8Engine engine;
  auto isolate = engine.isolate();
  for (uint32_t slot = 0; slot < v8::Isolate::GetNumberOfDataSlots(); ++slot) {
    EXPECT_TRUE(!isolate->GetData(slot));
  }
  for (uint32_t slot = 0; slot < v8::Isolate::GetNumberOfDataSlots(); ++slot) {
    void* data = reinterpret_cast<void*>(0xacce55ed + slot);
    isolate->SetData(slot, data);
  }
  for (uint32_t slot = 0; slot < v8::Isolate::GetNumberOfDataSlots(); ++slot) {
    void* data = reinterpret_cast<void*>(0xacce55ed + slot);
    EXPECT_EQ(data, isolate->GetData(slot));
  }
  for (uint32_t slot = 0; slot < v8::Isolate::GetNumberOfDataSlots(); ++slot) {
    void* data = reinterpret_cast<void*>(0xdecea5ed + slot);
    isolate->SetData(slot, data);
  }
  for (uint32_t slot = 0; slot < v8::Isolate::GetNumberOfDataSlots(); ++slot) {
    void* data = reinterpret_cast<void*>(0xdecea5ed + slot);
    EXPECT_EQ(data, isolate->GetData(slot));
  }
}

bool Terminate(JSContext *cx, unsigned argc, JS::Value *vp) {
  auto isolate = Isolate::GetCurrent();
  EXPECT_TRUE(!isolate->IsExecutionTerminating());
  isolate->TerminateExecution();

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return true;
}

bool Fail(JSContext *cx, unsigned argc, JS::Value *vp) {
  EXPECT_TRUE(false);
  return true;
}

Local<Value> CompileRun(const char* script) {
  auto scr = Script::Compile(v8_str(script));
  if (*scr) {
    return scr->Run();
  }
  return Local<Value>();
}

bool Loop(JSContext *cx, unsigned argc, JS::Value *vp) {
  auto isolate = Isolate::GetCurrent();
  EXPECT_FALSE(isolate->IsExecutionTerminating());

  MaybeLocal<Value> result =
      CompileRun("try { doloop(); fail(); } catch(e) { fail(); }");
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_TRUE(isolate->IsExecutionTerminating());

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return false;
}

bool DoLoop(JSContext *cx, unsigned argc, JS::Value *vp) {
  auto isolate = Isolate::GetCurrent();
  TryCatch try_catch(isolate);
  EXPECT_FALSE(isolate->IsExecutionTerminating());
  MaybeLocal<Value> result =
      CompileRun("function f() {"
                 "  var term = true;"
                 "  try {"
                 "    while(true) {"
                 "      if (term) terminate();"
                 "      term = false;"
                 "    }"
                 "    fail();"
                 "  } catch(e) {"
                 "    fail();"
                 "  }"
                 "}"
                 "f()");
  EXPECT_TRUE(result.IsEmpty());
  JS_IsExceptionPending(cx);
  // TODO: enable these https://github.com/mozilla/spidernode/issues/96
  // EXPECT_TRUE(try_catch.HasCaught());
  // EXPECT_TRUE(try_catch.Exception()->IsNull());
  // EXPECT_TRUE(try_catch.Message().IsEmpty());
  // EXPECT_FALSE(try_catch.CanContinue());
  EXPECT_TRUE(isolate->IsExecutionTerminating());

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return false;
}

void CreateGlobalTemplate(
    JSContext *smContext, JS::Handle<JSObject *> smGlobal, JSNative doloop) {

  EXPECT_TRUE(JS_DefineFunction(smContext, smGlobal, "terminate", &Terminate, 0, 0));
  EXPECT_TRUE(JS_DefineFunction(smContext, smGlobal, "fail", &Fail, 0, 0));
  EXPECT_TRUE(JS_DefineFunction(smContext, smGlobal, "loop", &Loop, 0, 0));
  EXPECT_TRUE(JS_DefineFunction(smContext, smGlobal, "doloop", doloop, 0, 0));
}

TEST(SpiderShim, TerminateExecution) {
  // This test is based on V8's TerminateOnlyV8ThreadFromThreadItself.
  // TODO: update this test when object/function templates work
  // https://github.com/mozilla/spidernode/issues/95
  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Object> globalObj = context->Global();
  auto smContext = JSContextFromContext(*context);
  JS::RootedObject smGlobal(smContext, &reinterpret_cast<JS::Value*>(*globalObj)->toObject());

  CreateGlobalTemplate(smContext, smGlobal, DoLoop);

  MaybeLocal<Value> result = engine.CompileRun(context, "loop();");
  EXPECT_TRUE(result.IsEmpty());
}
