// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

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

