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

Isolate* gc_callbacks_isolate = NULL;
int prologue_call_count = 0;
int epilogue_call_count = 0;
int prologue_call_count_second = 0;
int epilogue_call_count_second = 0;

void PrologueCallback(Isolate* isolate,
                      GCType,
                      GCCallbackFlags flags) {
  EXPECT_EQ(flags, kNoGCCallbackFlags);
  EXPECT_EQ(gc_callbacks_isolate, isolate);
  ++prologue_call_count;
}

void EpilogueCallback(Isolate* isolate,
                      GCType,
                      GCCallbackFlags flags) {
  EXPECT_EQ(flags, kNoGCCallbackFlags);
  EXPECT_EQ(gc_callbacks_isolate, isolate);
  ++epilogue_call_count;
}

void PrologueCallbackSecond(Isolate* isolate,
                            GCType,
                            GCCallbackFlags flags) {
  EXPECT_EQ(flags, kNoGCCallbackFlags);
  EXPECT_EQ(gc_callbacks_isolate, isolate);
  ++prologue_call_count_second;
}


void EpilogueCallbackSecond(Isolate* isolate,
                            GCType,
                            GCCallbackFlags flags) {
  EXPECT_EQ(flags, kNoGCCallbackFlags);
  EXPECT_EQ(gc_callbacks_isolate, isolate);
  ++epilogue_call_count_second;
}

TEST(SpiderShim, GCCallbacks) {
  // This test is based on V8's GCCallbacks.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  auto isolate = engine.isolate();

  gc_callbacks_isolate = isolate;
  isolate->AddGCPrologueCallback(PrologueCallback);
  isolate->AddGCEpilogueCallback(EpilogueCallback);
  EXPECT_EQ(0, prologue_call_count);
  EXPECT_EQ(0, epilogue_call_count);
  isolate->RequestGarbageCollectionForTesting(kFullGarbageCollection);
  EXPECT_EQ(1, prologue_call_count);
  EXPECT_EQ(1, epilogue_call_count);
  isolate->AddGCPrologueCallback(PrologueCallbackSecond);
  isolate->AddGCEpilogueCallback(EpilogueCallbackSecond);
  isolate->RequestGarbageCollectionForTesting(kFullGarbageCollection);
  EXPECT_EQ(2, prologue_call_count);
  EXPECT_EQ(2, epilogue_call_count);
  EXPECT_EQ(1, prologue_call_count_second);
  EXPECT_EQ(1, epilogue_call_count_second);
  isolate->RemoveGCPrologueCallback(PrologueCallback);
  isolate->RemoveGCEpilogueCallback(EpilogueCallback);
  isolate->RequestGarbageCollectionForTesting(kFullGarbageCollection);
  EXPECT_EQ(2, prologue_call_count);
  EXPECT_EQ(2, epilogue_call_count);
  EXPECT_EQ(2, prologue_call_count_second);
  EXPECT_EQ(2, epilogue_call_count_second);
  isolate->RemoveGCPrologueCallback(PrologueCallbackSecond);
  isolate->RemoveGCEpilogueCallback(EpilogueCallbackSecond);
  isolate->RequestGarbageCollectionForTesting(kFullGarbageCollection);
  EXPECT_EQ(2, prologue_call_count);
  EXPECT_EQ(2, epilogue_call_count);
  EXPECT_EQ(2, prologue_call_count_second);
  EXPECT_EQ(2, epilogue_call_count_second);

  // TODO: enable these when we have a way to simulate a full heap
  // https://github.com/mozilla/spidernode/issues/107
  // EXPECT_EQ(0, prologue_call_count_alloc);
  // EXPECT_EQ(0, epilogue_call_count_alloc);
  // isolate->AddGCPrologueCallback(PrologueCallbackAlloc);
  // isolate->AddGCEpilogueCallback(EpilogueCallbackAlloc);
  // CcTest::heap()->CollectAllGarbage(
  //     i::Heap::kAbortIncrementalMarkingMask);
  // EXPECT_EQ(1, prologue_call_count_alloc);
  // EXPECT_EQ(1, epilogue_call_count_alloc);
  // isolate->RemoveGCPrologueCallback(PrologueCallbackAlloc);
  // isolate->RemoveGCEpilogueCallback(EpilogueCallbackAlloc);
}

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

TEST(SpiderShim, ExternalAllocatedMemory) {
  // This test is based on V8's ExternalAllocatedMemory.
  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  auto isolate = engine.isolate();

  const int64_t kSize = 1024*1024;
  int64_t baseline = isolate->AdjustAmountOfExternalAllocatedMemory(0);
  EXPECT_EQ(baseline + kSize,
            isolate->AdjustAmountOfExternalAllocatedMemory(kSize));
  EXPECT_EQ(baseline,
            isolate->AdjustAmountOfExternalAllocatedMemory(-kSize));
}

static void Terminate(const FunctionCallbackInfo<Value>& info) {
  auto isolate = Isolate::GetCurrent();
  EXPECT_TRUE(!isolate->IsExecutionTerminating());
  isolate->TerminateExecution();
}

static void Fail(const FunctionCallbackInfo<Value>& info) {
  EXPECT_TRUE(false);
}

static void Loop(const FunctionCallbackInfo<Value>& info) {
  auto isolate = Isolate::GetCurrent();
  EXPECT_FALSE(isolate->IsExecutionTerminating());

  MaybeLocal<Value> result =
      CompileRun("try { doloop(); fail(); } catch(e) { fail(); }");
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_TRUE(isolate->IsExecutionTerminating());
}

static void DoLoop(const FunctionCallbackInfo<Value>& info) {
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
  // TODO: enable these https://github.com/mozilla/spidernode/issues/96
  // EXPECT_TRUE(try_catch.HasCaught());
  // EXPECT_TRUE(try_catch.Exception()->IsNull());
  // EXPECT_TRUE(try_catch.Message().IsEmpty());
  // EXPECT_FALSE(try_catch.CanContinue());
  EXPECT_TRUE(isolate->IsExecutionTerminating());
}

void CreateGlobalTemplate(Local<Context> context) {
  Local<Object> global = context->Global();

  EXPECT_TRUE(global->Set(context, v8_str("terminate"),
                          Function::New(context, Terminate).ToLocalChecked())
              .FromJust());
  EXPECT_TRUE(global->Set(context, v8_str("fail"),
                          Function::New(context, Fail).ToLocalChecked())
              .FromJust());
  EXPECT_TRUE(global->Set(context, v8_str("loop"),
                          Function::New(context, Loop).ToLocalChecked())
              .FromJust());
  EXPECT_TRUE(global->Set(context, v8_str("doloop"),
                          Function::New(context, DoLoop).ToLocalChecked())
              .FromJust());
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

  CreateGlobalTemplate(context);

  MaybeLocal<Value> result = engine.CompileRun(context, "loop();");
  EXPECT_TRUE(result.IsEmpty());
}

bool on_fulfilled_called;

static void OnFulfilled(const FunctionCallbackInfo<Value>& info) {
  EXPECT_FALSE(on_fulfilled_called);
  on_fulfilled_called = true;
}

TEST(SpiderShim, Microtask) {
  on_fulfilled_called = false;
  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Object> global = context->Global();

  EXPECT_TRUE(global->Set(context, v8_str("onFulfilled"),
                          Function::New(context, OnFulfilled).ToLocalChecked())
              .FromJust());
  EXPECT_FALSE(on_fulfilled_called);
  MaybeLocal<Value> result = engine.CompileRun(context, "Promise.resolve(43).then(onFulfilled);");
  EXPECT_FALSE(result.IsEmpty());
  EXPECT_FALSE(on_fulfilled_called);
  engine.isolate()->RunMicrotasks();
  EXPECT_TRUE(on_fulfilled_called);
  // Make sure the queue of micro tasks was drained.
  on_fulfilled_called = false;
  engine.isolate()->RunMicrotasks();
  EXPECT_FALSE(on_fulfilled_called);
}

TEST(SpiderShim, GetHeapStatistics) {
  // This test is based on V8's GetHeapStatistics.
  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  HeapStatistics heap_statistics;
  EXPECT_EQ(0u, heap_statistics.total_heap_size());
  EXPECT_EQ(0u, heap_statistics.used_heap_size());
  engine.isolate()->GetHeapStatistics(&heap_statistics);
  EXPECT_NE(static_cast<int>(heap_statistics.total_heap_size()), 0);
  EXPECT_NE(static_cast<int>(heap_statistics.used_heap_size()), 0);
}

void ThrowValue(const FunctionCallbackInfo<Value>& args) {
  EXPECT_EQ(1, args.Length());
  args.GetIsolate()->ThrowException(args[0]);
}


TEST(SpiderShim, ThrowValues) {
  // This test is based on V8's ThrowValues.
  V8Engine engine;
  auto isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<Object> global = context->Global();

  EXPECT_TRUE(global->Set(context, v8_str("Throw"),
                          Function::New(context, ThrowValue).ToLocalChecked())
              .FromJust());

  Local<Array> result = Local<Array>::Cast(
      engine.CompileRun(context, "function Run(obj) {"
                 "  try {"
                 "    Throw(obj);"
                 "  } catch (e) {"
                 "    return e;"
                 "  }"
                 "  return 'no exception';"
                 "}"
                 "[Run('str'), Run(1), Run(0), Run(null), Run(void 0)];"));
  EXPECT_EQ(5u, result->Length());
  EXPECT_TRUE(result->Get(context, Integer::New(isolate, 0))
            .ToLocalChecked()
            ->IsString());
  EXPECT_TRUE(result->Get(context, Integer::New(isolate, 1))
            .ToLocalChecked()
            ->IsNumber());
  EXPECT_EQ(1, result->Get(context, Integer::New(isolate, 1))
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  EXPECT_TRUE(result->Get(context, Integer::New(isolate, 2))
            .ToLocalChecked()
            ->IsNumber());
  EXPECT_EQ(0, result->Get(context, Integer::New(isolate, 2))
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  EXPECT_TRUE(result->Get(context, Integer::New(isolate, 3))
            .ToLocalChecked()
            ->IsNull());
  EXPECT_TRUE(result->Get(context, Integer::New(isolate, 4))
            .ToLocalChecked()
            ->IsUndefined());
}

