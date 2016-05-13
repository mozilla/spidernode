// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

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

