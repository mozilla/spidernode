// Copyright Mozilla Foundation. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <stdio.h>
#include <stdlib.h>

#include "v8engine.h"

#include "gtest/gtest.h"

static void CheckUncle(TryCatch* try_catch) {
  EXPECT_TRUE(try_catch->HasCaught());
  String::Utf8Value str_value(try_catch->Exception());
  EXPECT_EQ(0, strcmp(*str_value, "uncle?"));
  try_catch->Reset();
}

TEST(SpiderShim, ConversionException) {
  // This test is based on V8's ConversionException test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = Isolate::GetCurrent();

  engine.CompileRun(context,
      "function TestClass() { };"
      "TestClass.prototype.toString = function () { throw 'uncle?'; };"
      "var obj = new TestClass();");
  Local<Value> obj = engine.CompileRun(context, "obj");

  TryCatch try_catch(isolate);

  EXPECT_TRUE(obj->ToString(context).IsEmpty());
  CheckUncle(&try_catch);

  EXPECT_TRUE(obj->ToNumber(context).IsEmpty());
  CheckUncle(&try_catch);

  EXPECT_TRUE(obj->ToInteger(context).IsEmpty());
  CheckUncle(&try_catch);

  EXPECT_TRUE(obj->ToUint32(context).IsEmpty());
  CheckUncle(&try_catch);

  EXPECT_TRUE(obj->ToInt32(context).IsEmpty());
  CheckUncle(&try_catch);

  EXPECT_TRUE(Undefined(isolate)->ToObject(context).IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  try_catch.Reset();

  EXPECT_TRUE(obj->Int32Value(context).IsNothing());
  CheckUncle(&try_catch);

  EXPECT_TRUE(obj->Uint32Value(context).IsNothing());
  CheckUncle(&try_catch);

  EXPECT_TRUE(obj->NumberValue(context).IsNothing());
  CheckUncle(&try_catch);

  EXPECT_TRUE(obj->IntegerValue(context).IsNothing());
  CheckUncle(&try_catch);
}

TEST(SpiderShim, EvalInTryFinally) {
  // This test is based on V8's EvalInTryFinally test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TryCatch try_catch(context->GetIsolate());
  engine.CompileRun(context,
      "(function() {"
      "  try {"
      "    eval('asldkf (*&^&*^');"
      "  } finally {"
      "    return;"
      "  }"
      "})()");
  EXPECT_TRUE(!try_catch.HasCaught());
}

TEST(SpiderShim, CatchZero) {
  // This test is based on V8's CatchZero test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TryCatch try_catch(context->GetIsolate());
  EXPECT_TRUE(!try_catch.HasCaught());
  engine.CompileRun(context, "throw 10");
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_EQ(10, try_catch.Exception()->Int32Value(context).FromJust());
  try_catch.Reset();
  EXPECT_TRUE(!try_catch.HasCaught());
  engine.CompileRun(context, "throw 0");
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_EQ(0, try_catch.Exception()->Int32Value(context).FromJust());
}

TEST(SpiderShim, CatchExceptionFromWith) {
  // This test is based on V8's CatchExceptionFromWith test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TryCatch try_catch(context->GetIsolate());
  EXPECT_TRUE(!try_catch.HasCaught());
  engine.CompileRun(context, "var o = {}; with (o) { throw 42; }");
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST(SpiderShim, TryCatchAndFinallyHidingException) {
  // This test is based on V8's TryCatchAndFinallyHidingException test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TryCatch try_catch(context->GetIsolate());
  EXPECT_TRUE(!try_catch.HasCaught());
  engine.CompileRun(context, "function f(k) { try { this[k]; } finally { return 0; } };");
  engine.CompileRun(context, "f({toString: function() { throw 42; }});");
  EXPECT_TRUE(!try_catch.HasCaught());
}

TEST(SpiderShim, CompilationErrorUsingTryCatchHandler) {
  // This test is based on V8's CompilationErrorUsingTryCatchHandler test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TryCatch try_catch(context->GetIsolate());
  engine.CompileRun(context, "This doesn't &*&@#$&*^ compile.");
  EXPECT_TRUE(*try_catch.Exception());
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST(SpiderShim, TryCatchFinallyUsingTryCatchHandler) {
  // This test is based on V8's TryCatchFinallyUsingTryCatchHandler test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TryCatch try_catch(context->GetIsolate());
  engine.CompileRun(context, "try { throw ''; } catch (e) {}");
  EXPECT_TRUE(!try_catch.HasCaught());
  engine.CompileRun(context, "try { throw ''; } finally {}");
  EXPECT_TRUE(try_catch.HasCaught());
  try_catch.Reset();
  engine.CompileRun(context,
      "(function() {"
      "try { throw ''; } finally { return; }"
      "})()");
  EXPECT_TRUE(!try_catch.HasCaught());
  engine.CompileRun(context,
      "(function() {"
      "try { throw ''; } finally { throw 0; }"
      "})()");
  EXPECT_TRUE(try_catch.HasCaught());
}

static void CheckTryCatchSourceInfo(const V8Engine& engine,
                                    Local<Script> script,
                                    const char* resource_name,
                                    int line_offset) {
  HandleScope scope(engine.isolate());
  TryCatch try_catch;
  EXPECT_TRUE(try_catch.Message().IsEmpty());
  Local<Context> context = engine.isolate()->GetCurrentContext();
  EXPECT_TRUE(script->Run(context).IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  Local<Message> message = try_catch.Message();
  EXPECT_TRUE(!message.IsEmpty());
  EXPECT_EQ(10 + line_offset, message->GetLineNumber(context).FromJust());
  // GetStartPosition and GetEndPosition not supported yet.
  //EXPECT_EQ(91, message->GetStartPosition());
  //EXPECT_EQ(92, message->GetEndPosition());
  EXPECT_EQ(9, message->GetStartColumn(context).FromJust());
  // GetEndColumn is not supported yet.
  //EXPECT_EQ(3, message->GetEndColumn(context).FromJust());
  EXPECT_TRUE(message->GetEndColumn(context).IsNothing());
  // GetSourceLine not supported yet.
  //String::Utf8Value line(message->GetSourceLine(context).ToLocalChecked());
  //EXPECT_EQ(0, strcmp("  throw 'nirk';", *line));
  // GetScriptOrigin not supported yet.
  //String::Utf8Value name(message->GetScriptOrigin().ResourceName());
  //EXPECT_EQ(0, strcmp(resource_name, *name));
  Local<StackTrace> stackTrace = message->GetStackTrace();
  EXPECT_EQ(4, stackTrace->GetFrameCount());
  EXPECT_EQ(4, stackTrace->AsArray()->Length());
  struct Frame {
    uint32_t line, column;
    const char* func;
  } expected[] = {
    {10, 9, "Baz"},
    {6, 10, "Bar"},
    {2, 10, "Foo"},
    {13, 1, ""}
  };
  for (auto i = 0; i < 4; ++i) {
    Local<StackFrame> frame = stackTrace->GetFrame(i);
    EXPECT_EQ(expected[i].line + line_offset, frame->GetLineNumber());
    EXPECT_EQ(expected[i].column, frame->GetColumn());
    EXPECT_EQ(0, frame->GetScriptId());
    String::Utf8Value name(frame->GetScriptName());
    EXPECT_STREQ(resource_name, *name);
    String::Utf8Value nameOrURL(frame->GetScriptNameOrSourceURL());
    EXPECT_STREQ("", *nameOrURL);
    String::Utf8Value funcName(frame->GetFunctionName());
    EXPECT_STREQ(expected[i].func, *funcName);
    EXPECT_FALSE(frame->IsEval());
    EXPECT_FALSE(frame->IsConstructor());
  }
}

TEST(SpiderShim, TryCatchSourceInfo) {
  // This test is based on V8's TryCatchSourceInfo test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<String> source = v8_str(
      "function Foo() {\n"
      "  return Bar();\n"
      "}\n"
      "\n"
      "function Bar() {\n"
      "  return Baz();\n"
      "}\n"
      "\n"
      "function Baz() {\n"
      "  throw new Error('nirk');\n"
      "}\n"
      "\n"
      "Foo();\n");

  const char* resource_name;
  Local<Script> script;
  resource_name = "test.js";
  ScriptOrigin origin0(v8_str(resource_name));
  script =
      Script::Compile(context, source, &origin0).ToLocalChecked();
  CheckTryCatchSourceInfo(engine, script, resource_name, 0);

  resource_name = "test1.js";
  ScriptOrigin origin1(v8_str(resource_name));
  script =
      Script::Compile(context, source, &origin1).ToLocalChecked();
  CheckTryCatchSourceInfo(engine, script, resource_name, 0);

  resource_name = "test2.js";
  ScriptOrigin origin2(v8_str(resource_name),
                           Integer::New(context->GetIsolate(), 7));
  script =
      Script::Compile(context, source, &origin2).ToLocalChecked();
  CheckTryCatchSourceInfo(engine, script, resource_name, 7);
}

TEST(SpiderShim, TryCatchSourceInfoForEOSError) {
  // This test is based on V8's TryCatchSourceInfoForEOSError test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  TryCatch try_catch(engine.isolate());
  EXPECT_TRUE(Script::Compile(context, v8_str("!\n")).IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  Local<Message> message = try_catch.Message();
  // TODO: V8 seems to report the line number as 1 here!
  EXPECT_EQ(2, message->GetLineNumber(context).FromJust());
  EXPECT_EQ(0, message->GetStartColumn(context).FromJust());
}

TEST(SpiderShim, TryCatchStackTrace) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<String> source = v8_str(
      "function Foo() {\n"
      "  return Bar();\n"
      "}\n"
      "\n"
      "function Bar() {\n"
      "  return Baz();\n"
      "}\n"
      "\n"
      "function Baz() {\n"
      "  throw new Error('nirk');\n"
      "}\n"
      "\n"
      "Foo();\n");
  const char* expectedStack = "    at Baz (test.js:10:9)\n"
                              "    at Bar (test.js:6:10)\n"
                              "    at Foo (test.js:2:10)\n"
                              "    at test.js:13:1";

  const char* resource_name;
  Local<Script> script;
  resource_name = "test.js";
  ScriptOrigin origin0(v8_str(resource_name));
  script =
      Script::Compile(context, source, &origin0).ToLocalChecked();
  TryCatch try_catch(engine.isolate());
  EXPECT_TRUE(script->Run(context).IsEmpty());
  String::Utf8Value stackTrace(try_catch.StackTrace());
  EXPECT_STREQ(expectedStack, *stackTrace);
}

static void dont_expect_exception(Local<Message> message,
                                  Local<Value> data) {
  EXPECT_TRUE(false);
}

TEST(SpiderShim, TryCatchFunctionCall) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  engine.CompileRun(context,
      "function Foo() {\n"
      "  if (!arguments.length) {\n"
      "    throw new Error('quirk');\n"
      "  }\n"
      "}");
  Local<Function> Foo = Local<Function>::Cast(
      context->Global()->Get(context, v8_str("Foo")).ToLocalChecked());

  {
    TryCatch try_catch(context->GetIsolate());
    Local<Value>* args0 = NULL;
    MaybeLocal<Value> a0 = Foo->Call(context, Foo, 0, args0);
    EXPECT_TRUE(a0.IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
    EXPECT_TRUE(*try_catch.Exception());
    String::Utf8Value str_value(try_catch.Exception());
    EXPECT_STREQ("Error: quirk", *str_value);
    String::Utf8Value stackTrace(try_catch.StackTrace());
    EXPECT_STREQ("    at Foo (:3:11)", *stackTrace);
  }

  {
    TryCatch try_catch(context->GetIsolate());
    Local<Value> args1[] = {
      Integer::New(context->GetIsolate(), 42)
    };
    context->GetIsolate()->AddMessageListener(dont_expect_exception);
    MaybeLocal<Value> a0 = Foo->Call(context, Foo, 1, args1);
    EXPECT_TRUE(a0.ToLocalChecked()->IsUndefined());
    EXPECT_TRUE(!try_catch.HasCaught());
    EXPECT_TRUE(try_catch.Exception().IsEmpty());
    context->GetIsolate()->RemoveMessageListeners(dont_expect_exception);
  }
}

void WithTryCatch(const FunctionCallbackInfo<Value>& args) {
  TryCatch try_catch(args.GetIsolate());
}

TEST(SpiderShim, TryCatchAndFinally) {
  // This test is based on V8's TryCatchAndFinally test.
  V8Engine engine;

  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  EXPECT_TRUE(context->Global()
            ->Set(context, v8_str("native_with_try_catch"),
                  FunctionTemplate::New(isolate, WithTryCatch)
                      ->GetFunction(context)
                      .ToLocalChecked())
            .FromJust());
  TryCatch try_catch(isolate);
  EXPECT_TRUE(!try_catch.HasCaught());
  CompileRun(
      "try {\n"
      "  throw new Error('a');\n"
      "} finally {\n"
      "  native_with_try_catch();\n"
      "}\n");
  EXPECT_TRUE(try_catch.HasCaught());
}

static void TryCatchNested1Helper(int depth) {
  if (depth > 0) {
    TryCatch try_catch(Isolate::GetCurrent());
    try_catch.SetVerbose(true);
    TryCatchNested1Helper(depth - 1);
    EXPECT_TRUE(try_catch.HasCaught());
    try_catch.ReThrow();
    try_catch.ReThrow();
  } else {
    Isolate::GetCurrent()->ThrowException(v8_str("E1"));
  }
}

static void TryCatchNested2Helper(int depth) {
  if (depth > 0) {
    TryCatch try_catch(Isolate::GetCurrent());
    try_catch.SetVerbose(true);
    TryCatchNested2Helper(depth - 1);
    EXPECT_TRUE(try_catch.HasCaught());
    try_catch.ReThrow();
  } else {
    CompileRun("throw 'E2';");
  }
}

TEST(SpiderShim, TryCatchNested) {
  // This test is based on V8's TryCatchNested test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  {
    // Test nested try-catch with a native throw in the end.
    TryCatch try_catch(context->GetIsolate());
    TryCatchNested1Helper(5);
    EXPECT_TRUE(try_catch.HasCaught());
    EXPECT_EQ(0, strcmp(*String::Utf8Value(try_catch.Exception()), "E1"));
  }

  {
    // Test nested try-catch with a JavaScript throw in the end.
    TryCatch try_catch(context->GetIsolate());
    TryCatchNested2Helper(5);
    EXPECT_TRUE(try_catch.HasCaught());
    EXPECT_EQ(0, strcmp(*String::Utf8Value(try_catch.Exception()), "E2"));
  }
}

void TryCatchMixedNestingCheck(TryCatch* try_catch) {
  EXPECT_TRUE(try_catch->HasCaught());
  Local<Message> message = try_catch->Message();
  Local<Value> resource = message->GetScriptOrigin().ResourceName();
  EXPECT_EQ(0, strcmp(*String::Utf8Value(resource), "inner"));
  EXPECT_EQ(0,
           strcmp(*String::Utf8Value(message->Get()), "Uncaught Error: a"));
  EXPECT_EQ(1, message->GetLineNumber(Isolate::GetCurrent()->GetCurrentContext())
                  .FromJust());
  EXPECT_EQ(7, message->GetStartColumn(Isolate::GetCurrent()->GetCurrentContext())
                  .FromJust());
}

void TryCatchMixedNestingHelper(
    const FunctionCallbackInfo<Value>& args) {
  TryCatch try_catch(args.GetIsolate());
  CompileRunWithOrigin("throw new Error('a');\n", "inner", 0, 0);
  EXPECT_TRUE(try_catch.HasCaught());
  TryCatchMixedNestingCheck(&try_catch);
  try_catch.ReThrow();
}

// This test ensures that an outer TryCatch in the following situation:
//   C++/TryCatch -> JS -> C++/TryCatch -> JS w/ SyntaxError
// does not clobber the Message object generated for the inner TryCatch.
// This exercises the ability of TryCatch.ReThrow() to restore the
// inner pending Message before throwing the exception again.
TEST(SpiderShim, TryCatchMixedNesting) {
  // This test is based on V8's TryCatchMixedNesting test.
  V8Engine engine;

  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  TryCatch try_catch(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(v8_str("TryCatchMixedNestingHelper"),
             FunctionTemplate::New(isolate, TryCatchMixedNestingHelper));
  Local<Context> context2 = Context::New(isolate, nullptr, templ);
  Context::Scope context_scope2(context2);
  CompileRunWithOrigin("TryCatchMixedNestingHelper();\n", "outer", 1, 1);
  TryCatchMixedNestingCheck(&try_catch);
}

void TryCatchNativeHelper(const FunctionCallbackInfo<Value>& args) {
  TryCatch try_catch(args.GetIsolate());
  args.GetIsolate()->ThrowException(v8_str("boom"));
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST(SpiderShim, TryCatchNative) {
  // This test is based on V8's TryCatchNative test.
  V8Engine engine;

  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);

  TryCatch try_catch(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(v8_str("TryCatchNativeHelper"),
             FunctionTemplate::New(isolate, TryCatchNativeHelper));
  Local<Context> context = Context::New(isolate, nullptr, templ);
  Context::Scope context_scope(context);
  CompileRun("TryCatchNativeHelper();");
  EXPECT_TRUE(!try_catch.HasCaught());
}

void TryCatchNativeResetHelper(
    const FunctionCallbackInfo<Value>& args) {
  TryCatch try_catch(args.GetIsolate());
  args.GetIsolate()->ThrowException(v8_str("boom"));
  EXPECT_TRUE(try_catch.HasCaught());
  try_catch.Reset();
  EXPECT_TRUE(!try_catch.HasCaught());
}

TEST(SpiderShim, TryCatchNativeReset) {
  // This test is based on V8's TryCatchNativeReset test.
  V8Engine engine;

  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope handle_scope(isolate);

  TryCatch try_catch(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(v8_str("TryCatchNativeResetHelper"),
             FunctionTemplate::New(isolate, TryCatchNativeResetHelper));
  Local<Context> context = Context::New(isolate, nullptr, templ);
  Context::Scope context_scope(context);
  CompileRun("TryCatchNativeResetHelper();");
  EXPECT_TRUE(!try_catch.HasCaught());
}
