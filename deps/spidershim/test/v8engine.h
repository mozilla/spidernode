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

#include "libplatform/libplatform.h"
#include "v8.h"

#include <memory>
#include <string.h>

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

class V8Initializer {
public:
  V8Initializer() {
    // Initialize V8.
    V8::InitializeICU();
    V8::InitializeExternalStartupData("");
    platform_ = platform::CreateDefaultPlatform();
    V8::InitializePlatform(platform_);
    V8::Initialize();
  }
  ~V8Initializer() {
    // Tear down V8.
    V8::Dispose();
    V8::ShutdownPlatform();
    delete platform_;
  }

private:
  Platform* platform_;
};

static std::auto_ptr<V8Initializer> v8Initializer;

static inline Local<String> v8_str(const char* str) {
  return String::NewFromUtf8(Isolate::GetCurrent(), str);
}

static inline Local<Number> v8_num(double num) {
  return Number::New(Isolate::GetCurrent(), num);
}

static inline Local<Script> v8_compile(const char* str) {
  return Script::Compile(v8_str(str));
}

static inline Local<Value> CompileRun(const char* str) {
  Local<Script> script = v8_compile(str);
  if (script.IsEmpty()) {
    return Local<Value>();
  }
  return script->Run();
}

static inline Local<Value> CompileRunWithOrigin(const char* str,
                                                const char* resource,
                                                int line,
                                                int column) {
  Isolate* isolate = Isolate::GetCurrent();
  ScriptOrigin origin(v8_str(resource),
                      Integer::New(isolate, line),
                      Integer::New(isolate, column));
  Local<Script> script = Script::Compile(v8_str(str), &origin);
  if (script.IsEmpty()) {
    return Local<Value>();
  }
  return script->Run();
}

static inline int32_t v8_run_int32value(Local<Script> script) {
  return script->Run()->ToInt32()->Value();
}

class V8Engine {
public:
  V8Engine() {
    if (!v8Initializer.get()) {
      v8Initializer.reset(new V8Initializer());
      atexit([]() {
          v8Initializer.reset();
      });
    }

    // Create a new Isolate and make it the current one.
    ArrayBufferAllocator allocator;
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = &allocator;
    isolate_ = Isolate::New(create_params);
  }
  ~V8Engine() {
    // Dispose the isolate.
    isolate_->Dispose();
  }

  Isolate* isolate() const {
    return isolate_;
  }

  size_t GlobalHandleCount() const {
    return isolate_->PersistentCount();
  }

  Local<Value> CompileRun(Local<String> script) {
    auto scr = Script::Compile(script);
    if (*scr) {
      return scr->Run();
    }
    return Local<Value>();
  }

  // TODO: remove unused "context" parameter.
  Local<Value> CompileRun(Local<Context> context, const char* script) {
    return CompileRun(v8_str(script));
  }

private:
  Isolate* isolate_;
};
