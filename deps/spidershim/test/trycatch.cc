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
  TryCatch try_catch(engine.isolate());
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
