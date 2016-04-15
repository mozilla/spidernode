// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

#include "gtest/gtest.h"

using namespace v8;

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};

class V8Engine {
public:
  V8Engine() {
    // Initialize V8.
    V8::InitializeICU();
    V8::InitializeExternalStartupData("");
    platform_ = platform::CreateDefaultPlatform();
    V8::InitializePlatform(platform_);
    V8::Initialize();

    // Create a new Isolate and make it the current one.
    ArrayBufferAllocator allocator;
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = &allocator;
    isolate_ = Isolate::New(create_params);
  }
  ~V8Engine() {
    // Dispose the isolate and tear down V8.
    isolate_->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete platform_;
  }

  Isolate* isolate() const {
    return isolate_;
  }

private:
  Platform* platform_;
  Isolate* isolate_;
};

void TestBoolean(Isolate* isolate, bool value) {
  Local<Boolean> boolean = Boolean::New(isolate, value);
  EXPECT_TRUE(boolean->IsBoolean());
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
