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

#include <stdlib.h>
#include <vector>
#include <stack>

#include "v8.h"
#include "v8context.h"
#include "jsapi.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Maybe.h"
#include "rootstore.h"

namespace {

void OnGC(JSRuntime* rt, JSGCStatus status, void* data) {
  const char* env = getenv("DUMP_GC");
  if (env && *env) {
    using mozilla::TimeStamp;
    static TimeStamp beginTime;
    switch (status) {
      case JSGC_BEGIN:
        beginTime = TimeStamp::Now();
        printf("GC: begin\n");
        break;
      case JSGC_END:
        printf("GC: end, took %lf microseconds\n",
               (TimeStamp::Now() - beginTime).ToMicroseconds());
        break;
    }
  }
}
}

namespace v8 {

struct Isolate::Impl {
  JSRuntime* rt;
  std::vector<Context*> contexts;
  std::stack<Context*> currentContexts;
  std::vector<StackFrame*> stackFrames;
  std::vector<StackTrace*> stackTraces;
  mozilla::Maybe<internal::RootStore> persistents;
  mozilla::Maybe<internal::RootStore> eternals;

  internal::RootStore& EnsurePersistents(Isolate* iso) {
    if (!persistents) {
      persistents.emplace(iso);
    }
    return *persistents;
  }

  internal::RootStore& EnsureEternals(Isolate* iso) {
    if (!eternals) {
      eternals.emplace(iso);
    }
    return *eternals;
  }

  static void ErrorReporter(JSContext* cx, const char* message,
                            JSErrorReport* report) {
    fprintf(stderr, "JS error at %s:%u: %s\n",
            report->filename ? report->filename : "<no filename>",
            (unsigned int)report->lineno, message);
  }
};

Isolate* Isolate::current_ = nullptr;

Isolate::Isolate() : pimpl_(new Impl()) {
  const uint32_t defaultHeapSize = sizeof(void*) == 8 ? 1024 * 1024 * 1024
                                                      :    // 1GB
                                       512 * 1024 * 1024;  // 512MB
  pimpl_->rt = JS_NewRuntime(defaultHeapSize, JS::DefaultNurseryBytes, nullptr);
  // Assert success for now!
  if (!pimpl_->rt) {
    MOZ_CRASH("Creating the JS Runtime failed!");
  }
  JS_SetErrorReporter(pimpl_->rt, Impl::ErrorReporter);
  const size_t defaultStackQuota = 128 * sizeof(size_t) * 1024;
  JS_SetGCParameter(pimpl_->rt, JSGC_MODE, JSGC_MODE_INCREMENTAL);
  JS_SetGCParameter(pimpl_->rt, JSGC_MAX_BYTES, 0xffffffff);
  JS_SetNativeStackQuota(pimpl_->rt, defaultStackQuota);
  JS_SetDefaultLocale(pimpl_->rt, "UTF-8");

#ifndef DEBUG
  JS::RuntimeOptionsRef(pimpl_->rt)
      .setBaseline(true)
      .setIon(true)
      .setAsmJS(true)
      .setNativeRegExp(true);
#endif

  JS_SetGCCallback(pimpl_->rt, OnGC, NULL);
}

Isolate::~Isolate() {
  assert(pimpl_->rt);
  JS_DestroyRuntime(pimpl_->rt);
  delete pimpl_;
}

Isolate* Isolate::New(const CreateParams& params) {
  Isolate* isolate = new Isolate();
  if (params.array_buffer_allocator) {
    V8::SetArrayBufferAllocator(params.array_buffer_allocator);
  }
  return isolate;
}

Isolate* Isolate::New() { return new Isolate(); }

Isolate* Isolate::GetCurrent() { return current_; }

void Isolate::Enter() {
  // TODO: Multiple isolates not currently supported.
  assert(!current_);
  current_ = this;
}

void Isolate::Exit() {
  // TODO: Multiple isolates not currently supported.
  assert(current_ == this);
  current_ = nullptr;
}

void Isolate::Dispose() {
  pimpl_->persistents.reset();
  pimpl_->eternals.reset();
  for (auto context : pimpl_->contexts) {
    context->Dispose();
  }
  for (auto frame : pimpl_->stackFrames) {
    delete frame;
  }
  for (auto trace : pimpl_->stackTraces) {
    delete trace;
  }
  delete this;
}

void Isolate::AddContext(Context* context) {
  assert(pimpl_);
  pimpl_->contexts.push_back(context);
}

void Isolate::PushCurrentContext(Context* context) {
  assert(pimpl_);
  pimpl_->currentContexts.push(context);
}

void Isolate::PopCurrentContext() {
  assert(pimpl_);
  pimpl_->currentContexts.pop();
}

Local<Context> Isolate::GetCurrentContext() {
  assert(pimpl_);
  return Local<Context>::New(this, pimpl_->currentContexts.top());
}

JSContext* JSContextFromIsolate(v8::Isolate* isolate) {
  assert(isolate);
  assert(isolate->pimpl_);
  return isolate->pimpl_->currentContexts.top()->pimpl_->cx;
}

void Isolate::AddStackFrame(StackFrame* frame) {
  assert(pimpl_);
  pimpl_->stackFrames.push_back(frame);
}

void Isolate::AddStackTrace(StackTrace* trace) {
  assert(pimpl_);
  pimpl_->stackTraces.push_back(trace);
}

JSRuntime* Isolate::Runtime() const { return pimpl_->rt; }

Value* Isolate::AddPersistent(Value* val) {
  return pimpl_->EnsurePersistents(this).Add(val);
}

void Isolate::RemovePersistent(Value* val) {
  return pimpl_->EnsurePersistents(this).Remove(val);
}

size_t Isolate::PersistentCount() const {
  if (pimpl_->persistents.isNothing()) {
    return 0;
  }
  return pimpl_->persistents->RootedCount();
}

Value* Isolate::AddEternal(Value* val) {
  return pimpl_->EnsureEternals(this).Add(val);
}

Private* Isolate::AddEternal(Private* val) {
  return pimpl_->EnsureEternals(this).Add(val->symbol_);
}
}
