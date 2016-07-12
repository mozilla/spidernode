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

TEST(SpiderShim, ErrorConstruction) {
  // This test is based on V8's ErrorConstruction test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<String> foo = v8_str("foo");
  Local<String> message = v8_str("message");
  Local<Value> range_error = Exception::RangeError(foo);
  EXPECT_TRUE(range_error->IsObject());
  EXPECT_TRUE(range_error.As<Object>()
                ->Get(context, message)
                .ToLocalChecked()
                ->Equals(context, foo)
                .FromJust());
  Local<Value> reference_error = Exception::ReferenceError(foo);
  EXPECT_TRUE(reference_error->IsObject());
  EXPECT_TRUE(reference_error.As<Object>()
                ->Get(context, message)
                .ToLocalChecked()
                ->Equals(context, foo)
                .FromJust());
  Local<Value> syntax_error = Exception::SyntaxError(foo);
  EXPECT_TRUE(syntax_error->IsObject());
  EXPECT_TRUE(syntax_error.As<Object>()
                ->Get(context, message)
                .ToLocalChecked()
                ->Equals(context, foo)
                .FromJust());
  Local<Value> type_error = Exception::TypeError(foo);
  EXPECT_TRUE(type_error->IsObject());
  EXPECT_TRUE(type_error.As<Object>()
                ->Get(context, message)
                .ToLocalChecked()
                ->Equals(context, foo)
                .FromJust());
  Local<Value> error = Exception::Error(foo);
  EXPECT_TRUE(error->IsObject());
  EXPECT_TRUE(error.As<Object>()
                ->Get(context, message)
                .ToLocalChecked()
                ->Equals(context, foo)
                .FromJust());
}

bool handlerCalled = false;

static void check_custom_error_tostring(Local<Message> message,
                                        Local<Value> data) {
  handlerCalled = true;
  const char* uncaught_error = "Uncaught MyError toString";
  EXPECT_TRUE(message->Get()
                  ->Equals(Isolate::GetCurrent()->GetCurrentContext(),
                           v8_str(uncaught_error))
                  .FromJust());
}

TEST(SpiderShim, CustomErrorToString) {
  // This test is based on V8's CustomErrorToString test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  handlerCalled = false;
  context->GetIsolate()->AddMessageListener(check_custom_error_tostring);
  CompileRun(
      "function MyError(name, message) {                   "
      "  this.name = name;                                 "
      "  this.message = message;                           "
      "}                                                   "
      "MyError.prototype = Object.create(Error.prototype); "
      "MyError.prototype.toString = function() {           "
      "  return 'MyError toString';                        "
      "};                                                  "
      "throw new MyError('my name', 'my message');         ");
  EXPECT_TRUE(handlerCalled);
  context->GetIsolate()->RemoveMessageListeners(check_custom_error_tostring);
}

static void check_custom_error_message(Local<Message> message,
                                       Local<Value> data) {
  handlerCalled = true;
  const char* uncaught_error = "Uncaught MyError: my message";
  EXPECT_TRUE(message->Get()
                  ->Equals(Isolate::GetCurrent()->GetCurrentContext(),
                          v8_str(uncaught_error))
                 .FromJust());
}

TEST(SpiderShim, CustomErrorMessage) {
  // This test is based on V8's CustomErrorMessage test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  handlerCalled = false;
  context->GetIsolate()->AddMessageListener(check_custom_error_message);

  // Handlebars.
  CompileRun(
      "function MyError(msg) {                             "
      "  this.name = 'MyError';                            "
      "  this.message = msg;                               "
      "}                                                   "
      "MyError.prototype = new Error();                    "
      "throw new MyError('my message');                    ");
  EXPECT_TRUE(handlerCalled);
  handlerCalled = false;

  // Closure.
  CompileRun(
      "function MyError(msg) {                             "
      "  this.name = 'MyError';                            "
      "  this.message = msg;                               "
      "}                                                   "
      "inherits = function(childCtor, parentCtor) {        "
      "    function tempCtor() {};                         "
      "    tempCtor.prototype = parentCtor.prototype;      "
      "    childCtor.superClass_ = parentCtor.prototype;   "
      "    childCtor.prototype = new tempCtor();           "
      "    childCtor.prototype.constructor = childCtor;    "
      "};                                                  "
      "inherits(MyError, Error);                           "
      "throw new MyError('my message');                    ");
  EXPECT_TRUE(handlerCalled);
  handlerCalled = false;

  // Object.create.
  CompileRun(
      "function MyError(msg) {                             "
      "  this.name = 'MyError';                            "
      "  this.message = msg;                               "
      "}                                                   "
      "MyError.prototype = Object.create(Error.prototype); "
      "throw new MyError('my message');                    ");
  EXPECT_TRUE(handlerCalled);

  context->GetIsolate()->RemoveMessageListeners(check_custom_error_message);
}

static void check_custom_rethrowing_message(Local<Message> message,
                                            Local<Value> data) {
  handlerCalled = true;
  const char* uncaught_error = "Uncaught exception";
  EXPECT_TRUE(message->Get()
                  ->Equals(Isolate::GetCurrent()->GetCurrentContext(),
                           v8_str(uncaught_error))
                  .FromJust());
}

TEST(SpiderShim, CustomErrorRethrowsOnToString) {
  // This test is based on V8's CustomErrorRethrowsOnToString test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  handlerCalled = false;
  context->GetIsolate()->AddMessageListener(check_custom_rethrowing_message);

  CompileRun(
      "var e = { toString: function() { throw e; } };"
      "try { throw e; } finally {}");
  EXPECT_TRUE(handlerCalled);

  context->GetIsolate()->RemoveMessageListeners(
      check_custom_rethrowing_message);
}

bool message_received;
const char* expected_message;

static void receive_message(Local<Message> message,
                            Local<Value> data) {
  message_received = true;
  EXPECT_TRUE(message->Get()
                  ->Equals(Isolate::GetCurrent()->GetCurrentContext(),
                          v8_str(expected_message))
                  .FromJust());
}

void ThrowFromC(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetIsolate()->ThrowException(v8_str("konto"));
}

TEST(SpiderShim, APIThrowMessage) {
  // This test is based on V8's APIThrowMessage test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  message_received = false;
  expected_message = "Uncaught konto";
  isolate->AddMessageListener(receive_message);
  Local<Function> fun = FunctionTemplate::New(isolate, ThrowFromC)->
    GetFunction(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Set(context, v8_str("ThrowFromC"), fun).FromJust());
  CompileRun("ThrowFromC();");
  EXPECT_TRUE(message_received);
  isolate->RemoveMessageListeners(receive_message);
}

TEST(SpiderShim, APIThrowMessageAndVerboseTryCatch) {
  // This test is based on V8's APIThrowMessageAndVerboseTryCatch test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  message_received = false;
  expected_message = "Uncaught konto";
  isolate->AddMessageListener(receive_message);
  isolate->AddMessageListener(receive_message);
  Local<Function> fun = FunctionTemplate::New(isolate, ThrowFromC)->
    GetFunction(context).ToLocalChecked();
  EXPECT_TRUE(context->Global()->Set(context, v8_str("ThrowFromC"), fun).FromJust());
  TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  Local<Value> result = CompileRun("ThrowFromC();");
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_TRUE(message_received);
  isolate->RemoveMessageListeners(receive_message);
}

TEST(SpiderShim, APIStackOverflowAndVerboseTryCatch) {
  // This test is based on V8's APIStackOverflowAndVerboseTryCatch test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  message_received = false;
  expected_message = "Uncaught RangeError: Maximum call stack size exceeded";
  context->GetIsolate()->AddMessageListener(receive_message);
  TryCatch try_catch(context->GetIsolate());
  try_catch.SetVerbose(true);
  Local<Value> result = CompileRun("function foo() { foo(); } foo();");
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_TRUE(message_received);
  context->GetIsolate()->RemoveMessageListeners(receive_message);
}
