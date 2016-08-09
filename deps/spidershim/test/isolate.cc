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

static void check_message(Local<Message> message,
                            Local<Value> data) {
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
  for (uint32_t slot = 0; slot < Isolate::GetNumberOfDataSlots(); ++slot) {
    EXPECT_TRUE(!isolate->GetData(slot));
  }
  for (uint32_t slot = 0; slot < Isolate::GetNumberOfDataSlots(); ++slot) {
    void* data = reinterpret_cast<void*>(0xacce55ed + slot);
    isolate->SetData(slot, data);
  }
  for (uint32_t slot = 0; slot < Isolate::GetNumberOfDataSlots(); ++slot) {
    void* data = reinterpret_cast<void*>(0xacce55ed + slot);
    EXPECT_EQ(data, isolate->GetData(slot));
  }
  for (uint32_t slot = 0; slot < Isolate::GetNumberOfDataSlots(); ++slot) {
    void* data = reinterpret_cast<void*>(0xdecea5ed + slot);
    isolate->SetData(slot, data);
  }
  for (uint32_t slot = 0; slot < Isolate::GetNumberOfDataSlots(); ++slot) {
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
  engine.isolate()->SetAutorunMicrotasks(false);

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

  size_t types = engine.isolate()->NumberOfTrackedHeapObjectTypes();
  EXPECT_TRUE(types > 0);
  for (size_t i = 0; i < types; ++i) {
    HeapObjectStatistics object_statistics;
    EXPECT_TRUE(engine.isolate()->GetHeapObjectStatisticsAtLastGC(&object_statistics, i));
    EXPECT_NE(0u, object_statistics.object_count());
    EXPECT_NE(0u, object_statistics.object_size());
    EXPECT_STRNE("", object_statistics.object_type());
  }
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

static void MicrotaskOne(const FunctionCallbackInfo<Value>& info) {
  EXPECT_TRUE(MicrotasksScope::IsRunningMicrotasks(info.GetIsolate()));
  HandleScope scope(info.GetIsolate());
  MicrotasksScope microtasks(info.GetIsolate(),
                                 MicrotasksScope::kDoNotRunMicrotasks);
  CompileRun("ext1Calls++;");
}

static void MicrotaskTwo(const FunctionCallbackInfo<Value>& info) {
  EXPECT_TRUE(MicrotasksScope::IsRunningMicrotasks(info.GetIsolate()));
  HandleScope scope(info.GetIsolate());
  MicrotasksScope microtasks(info.GetIsolate(),
                                 MicrotasksScope::kDoNotRunMicrotasks);
  CompileRun("ext2Calls++;");
}

void* g_passed_to_three = NULL;

static void MicrotaskThree(void* data) {
  g_passed_to_three = data;
}

TEST(SpiderShim, EnqueueMicrotask) {
  // This test is based on V8's EnqueueMicrotask.
  V8Engine engine;
  auto isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  EXPECT_TRUE(!MicrotasksScope::IsRunningMicrotasks(context->GetIsolate()));
  CompileRun(
      "var ext1Calls = 0;"
      "var ext2Calls = 0;");
  CompileRun("1+1;");
  EXPECT_EQ(0, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());

  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskOne).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());

  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskOne).ToLocalChecked());
  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskTwo).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(1, CompileRun("ext2Calls")->Int32Value(context).FromJust());

  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskTwo).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(2, CompileRun("ext2Calls")->Int32Value(context).FromJust());

  CompileRun("1+1;");
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(2, CompileRun("ext2Calls")->Int32Value(context).FromJust());

  g_passed_to_three = NULL;
  context->GetIsolate()->EnqueueMicrotask(MicrotaskThree);
  CompileRun("1+1;");
  EXPECT_TRUE(!g_passed_to_three);
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(2, CompileRun("ext2Calls")->Int32Value(context).FromJust());

  int dummy;
  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskOne).ToLocalChecked());
  context->GetIsolate()->EnqueueMicrotask(MicrotaskThree, &dummy);
  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskTwo).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(&dummy, g_passed_to_three);
  EXPECT_EQ(3, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(3, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  g_passed_to_three = NULL;
}

static void MicrotaskExceptionOne(
    const FunctionCallbackInfo<Value>& info) {
  HandleScope scope(info.GetIsolate());
  CompileRun("exception1Calls++;");
  info.GetIsolate()->ThrowException(
      Exception::Error(v8_str("first")));
}

static void MicrotaskExceptionTwo(
    const FunctionCallbackInfo<Value>& info) {
  HandleScope scope(info.GetIsolate());
  CompileRun("exception2Calls++;");
  info.GetIsolate()->ThrowException(
      Exception::Error(v8_str("second")));
}

TEST(SpiderShim, RunMicrotasksIgnoresThrownExceptions) {
  // This test is based on V8's RunMicrotasksIgnoresThrownExceptions.
  V8Engine engine;
  auto isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  CompileRun(
      "var exception1Calls = 0;"
      "var exception2Calls = 0;");
  isolate->EnqueueMicrotask(
      Function::New(context, MicrotaskExceptionOne).ToLocalChecked());
  isolate->EnqueueMicrotask(
      Function::New(context, MicrotaskExceptionTwo).ToLocalChecked());
  TryCatch try_catch(isolate);
  CompileRun("1+1;");
  EXPECT_TRUE(!try_catch.HasCaught());
  EXPECT_EQ(1,
           CompileRun("exception1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(1,
           CompileRun("exception2Calls")->Int32Value(context).FromJust());
}

uint8_t microtasks_completed_callback_count = 0;

static void MicrotasksCompletedCallback(Isolate* isolate) {
  ++microtasks_completed_callback_count;
}

TEST(SpiderShim, SetAutorunMicrotasks) {
  // This test is based on V8's SetAutorunMicrotasks.
  V8Engine engine;
  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  context->GetIsolate()->AddMicrotasksCompletedCallback(
      &::MicrotasksCompletedCallback);
  CompileRun(
      "var ext1Calls = 0;"
      "var ext2Calls = 0;");
  CompileRun("1+1;");
  EXPECT_EQ(0, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(0u, microtasks_completed_callback_count);

  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskOne).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(1u, microtasks_completed_callback_count);

  context->GetIsolate()->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);
  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskOne).ToLocalChecked());
  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskTwo).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(1u, microtasks_completed_callback_count);

  context->GetIsolate()->RunMicrotasks();
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(1, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(2u, microtasks_completed_callback_count);

  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskTwo).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(1, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(2u, microtasks_completed_callback_count);

  context->GetIsolate()->RunMicrotasks();
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(2, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(3u, microtasks_completed_callback_count);

  context->GetIsolate()->SetMicrotasksPolicy(MicrotasksPolicy::kAuto);
  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskTwo).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(3, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(4u, microtasks_completed_callback_count);

  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskTwo).ToLocalChecked());
  {
    Isolate::SuppressMicrotaskExecutionScope scope(context->GetIsolate());
    CompileRun("1+1;");
    EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(3, CompileRun("ext2Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(4u, microtasks_completed_callback_count);
  }

  CompileRun("1+1;");
  EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(4, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(5u, microtasks_completed_callback_count);

  context->GetIsolate()->RemoveMicrotasksCompletedCallback(
      &::MicrotasksCompletedCallback);
  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskOne).ToLocalChecked());
  CompileRun("1+1;");
  EXPECT_EQ(3, CompileRun("ext1Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(4, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  EXPECT_EQ(5u, microtasks_completed_callback_count);
}

TEST(SpiderShim, ScopedMicrotasks) {
  // This test is based on V8's RunMicrotasksIgnoresThrownExceptions.
  V8Engine engine;
  auto isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  context->GetIsolate()->SetMicrotasksPolicy(MicrotasksPolicy::kScoped);
  {
    MicrotasksScope scope1(context->GetIsolate(),
                               MicrotasksScope::kDoNotRunMicrotasks);
    context->GetIsolate()->EnqueueMicrotask(
        Function::New(context, MicrotaskOne).ToLocalChecked());
    CompileRun(
        "var ext1Calls = 0;"
        "var ext2Calls = 0;");
    CompileRun("1+1;");
    EXPECT_EQ(0, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
    {
      MicrotasksScope scope2(context->GetIsolate(),
                                 MicrotasksScope::kRunMicrotasks);
      CompileRun("1+1;");
      EXPECT_EQ(0, CompileRun("ext1Calls")->Int32Value(context).FromJust());
      EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
      {
        MicrotasksScope scope3(context->GetIsolate(),
                                   MicrotasksScope::kRunMicrotasks);
        CompileRun("1+1;");
        EXPECT_EQ(0,
                 CompileRun("ext1Calls")->Int32Value(context).FromJust());
        EXPECT_EQ(0,
                 CompileRun("ext2Calls")->Int32Value(context).FromJust());
      }
      EXPECT_EQ(0, CompileRun("ext1Calls")->Int32Value(context).FromJust());
      EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
    }
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
    context->GetIsolate()->EnqueueMicrotask(
        Function::New(context, MicrotaskTwo).ToLocalChecked());
  }

  {
    MicrotasksScope scope(context->GetIsolate(),
                              MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  }

  {
    MicrotasksScope scope1(context->GetIsolate(),
                               MicrotasksScope::kRunMicrotasks);
    CompileRun("1+1;");
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
    {
      MicrotasksScope scope2(context->GetIsolate(),
                                 MicrotasksScope::kDoNotRunMicrotasks);
    }
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(0, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  }

  {
    MicrotasksScope scope(context->GetIsolate(),
                              MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(1, CompileRun("ext2Calls")->Int32Value(context).FromJust());
    context->GetIsolate()->EnqueueMicrotask(
        Function::New(context, MicrotaskTwo).ToLocalChecked());
  }

  {
    Isolate::SuppressMicrotaskExecutionScope scope1(context->GetIsolate());
    {
      MicrotasksScope scope2(context->GetIsolate(),
                                 MicrotasksScope::kRunMicrotasks);
    }
    MicrotasksScope scope3(context->GetIsolate(),
                               MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(1, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  }

  {
    MicrotasksScope scope1(context->GetIsolate(),
                               MicrotasksScope::kRunMicrotasks);
    MicrotasksScope::PerformCheckpoint(context->GetIsolate());
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(1, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  }

  {
    MicrotasksScope scope(context->GetIsolate(),
                              MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(2, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  }

  MicrotasksScope::PerformCheckpoint(context->GetIsolate());

  {
    MicrotasksScope scope(context->GetIsolate(),
                              MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(2, CompileRun("ext2Calls")->Int32Value(context).FromJust());
    context->GetIsolate()->EnqueueMicrotask(
        Function::New(context, MicrotaskTwo).ToLocalChecked());
  }

  MicrotasksScope::PerformCheckpoint(context->GetIsolate());

  {
    MicrotasksScope scope(context->GetIsolate(),
                              MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(3, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  }

  context->GetIsolate()->EnqueueMicrotask(
      Function::New(context, MicrotaskOne).ToLocalChecked());
  {
    Isolate::SuppressMicrotaskExecutionScope scope1(context->GetIsolate());
    MicrotasksScope::PerformCheckpoint(context->GetIsolate());
    MicrotasksScope scope2(context->GetIsolate(),
                               MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_EQ(1, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(3, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  }

  MicrotasksScope::PerformCheckpoint(context->GetIsolate());

  {
    MicrotasksScope scope(context->GetIsolate(),
                              MicrotasksScope::kDoNotRunMicrotasks);
    EXPECT_EQ(2, CompileRun("ext1Calls")->Int32Value(context).FromJust());
    EXPECT_EQ(3, CompileRun("ext2Calls")->Int32Value(context).FromJust());
  }

  context->GetIsolate()->SetMicrotasksPolicy(MicrotasksPolicy::kAuto);
}

void RunGlobalObjectTemplateChecks(const char* obj, int dynamicProp) {
  char buf[2048] = {'\0'};
  snprintf(buf, 2048, "[%sx1, %sy1.x1, %sy1.x2, %sy2.x1, %sy2.x2, "
                      "%sy2.y1.x1, %sy2.y1.x2, %sy1.y2, %sdynamic, "
                      "{prop:%sdynamic}]",
           obj, obj, obj, obj, obj, obj,
           obj, obj, obj, obj, obj, obj);
  Local<Value> result = CompileRun(buf);
  Array* arr = Array::Cast(*result);

  Local<Value> expected[] = {
    v8_num(42),
    v8_num(42),
    v8_num(42),
    v8_num(42),
    v8_num(42),
    v8_num(42),
    v8_num(42),
    Undefined(Isolate::GetCurrent()),
    (dynamicProp >= 0) ?
      v8_num(dynamicProp).As<Value>() :
      Undefined(Isolate::GetCurrent()).As<Value>()
  };
  auto count = sizeof(expected)/sizeof(expected[0]);
  for (auto i = 0; i < count; ++i) {
    EXPECT_TRUE(expected[i]->Equals(arr->Get(i)));
  }
  Local<Object> last = arr->Get(count).As<Object>();
  if (dynamicProp >= 0) {
    EXPECT_EQ(24, last->Get(v8_str("prop"))->Int32Value());
  } else {
    EXPECT_TRUE(last->Get(v8_str("prop"))->IsUndefined());
  }
}

void GlobalTemplateGetter(Local<Name> property,
                          const PropertyCallbackInfo<Value>& info) {
  EXPECT_TRUE(property->IsString());
  String::Utf8Value utf8(property->ToString());
  if (!strcmp(*utf8, "dynamic")) {
    info.GetReturnValue().Set(24);
  } else if (!strcmp(*utf8, "obj")) {
    info.GetReturnValue().Set(info.Data());
  } else {
    info.GetReturnValue().Set(info.This()->Get(property));
  }
}

TEST(SpiderShim, GlobalObjectTemplate) {
  V8Engine engine;
  auto isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate);

  global_template->Set(v8_str("x1"), v8_num(42));
  global_template->Set(v8_str("x2"), v8_str("42"));
  global_template->Set(v8_str("y1"), global_template->NewInstance());
  global_template->Set(v8_str("y2"), global_template->NewInstance());
  global_template->SetHandler(
      NamedPropertyHandlerConfiguration(GlobalTemplateGetter,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        global_template->NewInstance()));

  Local<Context> context = Context::New(isolate, nullptr, global_template);
  Context::Scope context_scope(context);

  RunGlobalObjectTemplateChecks("", 24);
  RunGlobalObjectTemplateChecks("this.", 24);
  RunGlobalObjectTemplateChecks("obj.", -1);
}

TEST(SpiderShim, IsolateMultipleEnter) {
  V8Engine engine;
  auto isolate = engine.isolate();
  Isolate::Scope isolate_scope_1(isolate);
  Isolate::Scope isolate_scope_2(isolate);
  Isolate::Scope isolate_scope_3(isolate);
  Isolate::Scope isolate_scope_4(isolate);
}
