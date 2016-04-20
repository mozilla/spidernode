// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

TEST(SpiderShim, HelloWorld) {
  V8Engine engine;
  Isolate* isolate = engine.isolate();

  Isolate::Scope isolate_scope(isolate);

  // Create a stack-allocated handle scope.
  HandleScope handle_scope(isolate);

  // Create a new context.
  Local<Context> context = Context::New(isolate);

  // Enter the context for compiling and running the hello world script.
  Context::Scope context_scope(context);

  // Run the script to get the result.
  Local<Value> result = engine.CompileRun(context, "'Hello' + ', World!'");

  // Convert the result to an UTF8 string and print it.
  String::Utf8Value utf8(result);
  EXPECT_STREQ(*utf8, "Hello, World!");
}
