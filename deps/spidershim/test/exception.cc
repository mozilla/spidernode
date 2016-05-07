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
