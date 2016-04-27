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

TEST(SpiderShim, GlobalHandle) {
  // This test is based on V8's GlobalHandle test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = Isolate::GetCurrent();

  Persistent<String> global;
  {
    HandleScope scope(isolate);
    global.Reset(isolate, v8_str("str"));
  }
  {
    HandleScope scope(isolate);
    EXPECT_EQ(Local<String>::New(isolate, global)->Length(), 3);
  }
  global.Reset();
  {
    HandleScope scope(isolate);
    global.Reset(isolate, v8_str("str"));
  }
  {
    HandleScope scope(isolate);
    EXPECT_EQ(Local<String>::New(isolate, global)->Length(), 3);
  }
  global.Reset();
}

TEST(SpiderShim, ResettingGlobalHandle) {
  // This test is based on V8's ResettingGlobalHandle test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = Isolate::GetCurrent();

  Persistent<String> global;
  {
    HandleScope scope(isolate);
    global.Reset(isolate, v8_str("str"));
  }
  size_t initial_handle_count = engine.GlobalHandleCount();
  {
    HandleScope scope(isolate);
    EXPECT_EQ(Local<String>::New(isolate, global)->Length(), 3);
  }
  {
    HandleScope scope(isolate);
    global.Reset(isolate, v8_str("longer"));
  }
  EXPECT_EQ(engine.GlobalHandleCount(), initial_handle_count);
  {
    HandleScope scope(isolate);
    EXPECT_EQ(Local<String>::New(isolate, global)->Length(), 6);
  }
  global.Reset();
  EXPECT_EQ(engine.GlobalHandleCount(), initial_handle_count - 1);
}

TEST(SpiderShim, ResettingGlobalHandleToEmpty) {
  // This test is based on V8's ResettingGlobalHandleToEmpty test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = Isolate::GetCurrent();

  Persistent<String> global;
  {
    HandleScope scope(isolate);
    global.Reset(isolate, v8_str("str"));
  }
  size_t initial_handle_count = engine.GlobalHandleCount();
  {
    HandleScope scope(isolate);
    EXPECT_EQ(Local<String>::New(isolate, global)->Length(), 3);
  }
  {
    HandleScope scope(isolate);
    Local<String> empty;
    global.Reset(isolate, empty);
  }
  EXPECT_TRUE(global.IsEmpty());
  EXPECT_EQ(engine.GlobalHandleCount(), initial_handle_count - 1);
}

template <class T>
static Global<T> PassUnique(Global<T> unique) {
  return unique.Pass();
}

template <class T>
static Global<T> ReturnUnique(Isolate* isolate,
                                  const Persistent<T>& global) {
  Global<String> unique(isolate, global);
  return unique.Pass();
}

TEST(SpiderShim, Global) {
  // This test is based on V8's Global test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = Isolate::GetCurrent();

  Persistent<String> global;
  {
    HandleScope scope(isolate);
    global.Reset(isolate, v8_str("str"));
  }
  size_t initial_handle_count = engine.GlobalHandleCount();
  {
    Global<String> unique(isolate, global);
    EXPECT_EQ(initial_handle_count + 1, engine.GlobalHandleCount());
    // Test assignment via Pass
    {
      Global<String> copy = unique.Pass();
      EXPECT_TRUE(unique.IsEmpty());
      EXPECT_TRUE(copy == global);
      EXPECT_EQ(initial_handle_count + 1, engine.GlobalHandleCount());
      unique = copy.Pass();
    }
    // Test ctor via Pass
    {
      Global<String> copy(unique.Pass());
      EXPECT_TRUE(unique.IsEmpty());
      EXPECT_TRUE(copy == global);
      EXPECT_EQ(initial_handle_count + 1, engine.GlobalHandleCount());
      unique = copy.Pass();
    }
    // Test pass through function call
    {
      Global<String> copy = PassUnique(unique.Pass());
      EXPECT_TRUE(unique.IsEmpty());
      EXPECT_TRUE(copy == global);
      EXPECT_EQ(initial_handle_count + 1, engine.GlobalHandleCount());
      unique = copy.Pass();
    }
    EXPECT_EQ(initial_handle_count + 1, engine.GlobalHandleCount());
  }
  // Test pass from function call
  {
    Global<String> unique = ReturnUnique(isolate, global);
    EXPECT_TRUE(unique == global);
    EXPECT_EQ(initial_handle_count + 1, engine.GlobalHandleCount());
  }
  EXPECT_EQ(initial_handle_count, engine.GlobalHandleCount());
  global.Reset();
}

TEST(SpiderShim, Eternal) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = Isolate::GetCurrent();

  Eternal<String> eternal;
  EXPECT_TRUE(eternal.IsEmpty());
  eternal.Set(isolate, v8_str("foobar"));
  EXPECT_EQ(6, eternal.Get(isolate)->Length());
  Eternal<String> eternal2(isolate, v8_str("baz"));
  EXPECT_FALSE(eternal2.IsEmpty());
  EXPECT_EQ(3, eternal2.Get(isolate)->Length());
}
