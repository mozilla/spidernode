// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

TEST(SpiderShim, UnboundScript) {
  // This test is based on V8's ScriptContextDependence test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());

  const char *source = "foo";
  Local<Script> dep;
  Local<UnboundScript> indep;

  {
    Local<Context> c1 = Context::New(engine.isolate());
    Context::Scope context_scope(c1);
  
    dep = Script::Compile(v8_str(source));

    HandleScope scope(c1->GetIsolate());
    ScriptCompiler::Source script_source(
        String::NewFromUtf8(c1->GetIsolate(), source,
                                NewStringType::kNormal)
            .ToLocalChecked());
    indep =
        ScriptCompiler::CompileUnboundScript(c1->GetIsolate(), &script_source)
            .ToLocalChecked();
    c1->Global()
        ->Set(c1, String::NewFromUtf8(c1->GetIsolate(), "foo",
                                                  NewStringType::kNormal)
                              .ToLocalChecked(),
              Integer::New(c1->GetIsolate(), 100))
        .FromJust();
    EXPECT_EQ(
        dep->Run(c1).ToLocalChecked()->Int32Value(c1).FromJust(),
        100);
    EXPECT_EQ(indep->BindToCurrentContext()
                 ->Run(c1)
                 .ToLocalChecked()
                 ->Int32Value(c1)
                 .FromJust(),
             100);
  }
  {
    Local<Context> c2 = Context::New(engine.isolate());
    Context::Scope context_scope(c2);
  
    c2->Global()
        ->Set(c2, String::NewFromUtf8(c2->GetIsolate(), "foo",
                                                  NewStringType::kNormal)
                              .ToLocalChecked(),
              Integer::New(c2->GetIsolate(), 101))
        .FromJust();
    EXPECT_EQ(
        dep->Run(c2).ToLocalChecked()->Int32Value(c2).FromJust(),
        100);
    EXPECT_EQ(indep->BindToCurrentContext()
                 ->Run(c2)
                 .ToLocalChecked()
                 ->Int32Value(c2)
                 .FromJust(),
             101);
  }
}

TEST(SpiderShim, StackTrace) {
  // This test is based on V8's StackTrace test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  HandleScope handle_scope(engine.isolate());
  TryCatch try_catch(engine.isolate());
  const char *source = "function foo() { FAIL.FAIL; }; foo();";
  Local<String> src = v8_str(source);
  Local<String> origin = v8_str("stack-trace-test");
  ScriptCompiler::Source script_source(src, ScriptOrigin(origin));
  EXPECT_TRUE(ScriptCompiler::CompileUnboundScript(engine.isolate(),
                                                 &script_source)
            .ToLocalChecked()
            ->BindToCurrentContext()
            ->Run(context)
            .IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  String::Utf8Value stack(
      try_catch.StackTrace(context).ToLocalChecked());
  EXPECT_STREQ("    at foo (stack-trace-test:1:18)\n    at stack-trace-test:1:41", *stack);
}

TEST(SpiderShim, InvalidSyntax) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  HandleScope handle_scope(engine.isolate());
  TryCatch try_catch(engine.isolate());
  const char *source = "function foo() {";
  Local<String> src = v8_str(source);
  ScriptCompiler::Source script_source(src);
  EXPECT_TRUE(ScriptCompiler::CompileUnboundScript(engine.isolate(),
                                                 &script_source).IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
}
