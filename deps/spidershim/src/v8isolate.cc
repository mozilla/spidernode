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
#include <algorithm>

#include "v8.h"
#include "v8-profiler.h"
#include "v8isolate.h"
#include "jsapi.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ThreadLocal.h"

namespace v8 {

HeapProfiler dummyHeapProfiler;
CpuProfiler dummyCpuProfiler;

static MOZ_THREAD_LOCAL(Isolate*) sCurrentIsolate;

namespace internal {

bool InitializeIsolate() { return sCurrentIsolate.init(); }
}

void Isolate::Impl::OnGC(JSRuntime* rt, JSGCStatus status, void* data) {
  auto isolate = Isolate::GetCurrent();
  switch (status) {
    case JSGC_BEGIN:
      for (auto callback : isolate->pimpl_->gcProlougeCallbacks) {
        (*callback)(isolate, kGCTypeAll, kNoGCCallbackFlags);
      }
      break;
    case JSGC_END:
      for (auto callback : isolate->pimpl_->gcEpilogueCallbacks) {
        (*callback)(isolate, kGCTypeAll, kNoGCCallbackFlags);
      }
      break;
  }

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

bool Isolate::Impl::OnInterrupt(JSContext* cx) {
  auto isolateImpl = Isolate::GetCurrent()->pimpl_;
  // Prevent re-entering while handler is running.
  if (!isolateImpl->serviceInterrupt) {
    return true;
  }
  isolateImpl->serviceInterrupt = false;

  if (isolateImpl->terminatingExecution) {
    return false;
  }
  return true;
}

bool Isolate::Impl::EnqueuePromiseJobCallback(JSContext* cx, JS::HandleObject job, void* data) {
  Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
  return context->pimpl_->jobQueue.append(job);
}

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

  JS::SetEnqueuePromiseJobCallback(pimpl_->rt, Isolate::Impl::EnqueuePromiseJobCallback);
  JS_SetInterruptCallback(pimpl_->rt, Isolate::Impl::OnInterrupt);
  JS_SetGCCallback(pimpl_->rt, Isolate::Impl::OnGC, NULL);
}

Isolate::~Isolate() {
  assert(pimpl_->rt);
  JS_SetInterruptCallback(pimpl_->rt, NULL);
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

Isolate* Isolate::GetCurrent() { return sCurrentIsolate.get(); }

void Isolate::Enter() {
  // TODO: Multiple isolates not currently supported.
  assert(!sCurrentIsolate.get());
  sCurrentIsolate.set(this);
}

void Isolate::Exit() {
  // TODO: Multiple isolates not currently supported.
  assert(sCurrentIsolate.get() == this);
  sCurrentIsolate.set(nullptr);
}

void Isolate::Dispose() {
  pimpl_->persistents.reset();
  pimpl_->eternals.reset();
  JS_SetGCCallback(pimpl_->rt, NULL, NULL);
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

void Isolate::SetAutorunMicrotasks(bool autorun) {
  // Node only sets this to false.
  // TODO: fully implement this https://github.com/mozilla/spidernode/issues/110
  if (autorun) {
    MOZ_CRASH("Only autorun false is supported.");
  }
}

void Isolate::RunMicrotasks() {
  Local<Context> context = GetCurrentContext();
  context->pimpl_->RunMicrotasks();
}

bool Isolate::IsExecutionTerminating() {
  return pimpl_->terminatingExecution;
}

void Isolate::TerminateExecution() {
  assert(pimpl_->rt);
  pimpl_->terminatingExecution = true;
  pimpl_->serviceInterrupt = true;
  JS_RequestInterruptCallback(pimpl_->rt);
}

void Isolate::CancelTerminateExecution() {
  assert(pimpl_->rt);
  pimpl_->terminatingExecution = false;
}

void Isolate::AddStackFrame(StackFrame* frame) {
  assert(pimpl_);
  pimpl_->stackFrames.push_back(frame);
}

void Isolate::AddStackTrace(StackTrace* trace) {
  assert(pimpl_);
  pimpl_->stackTraces.push_back(trace);
}

void Isolate::AddGCPrologueCallback(
  GCCallback callback, GCType gc_type_filter) {
  assert(pimpl_);
  if (gc_type_filter != kGCTypeAll) {
    MOZ_CRASH("Unimplmented type of GC callback.");
  }
  pimpl_->gcProlougeCallbacks.push_back(callback);
}

void Isolate::RemoveGCPrologueCallback(GCCallback callback) {
  auto i = std::remove(pimpl_->gcProlougeCallbacks.begin(),
                       pimpl_->gcProlougeCallbacks.end(), callback);
  pimpl_->gcProlougeCallbacks.erase(i, pimpl_->gcProlougeCallbacks.end());
}

void Isolate::AddGCEpilogueCallback(
  GCCallback callback, GCType gc_type_filter) {
  assert(pimpl_);
  if (gc_type_filter != kGCTypeAll) {
    MOZ_CRASH("Unimplmented type of GC callback.");
  }
  pimpl_->gcEpilogueCallbacks.push_back(callback);
}

void Isolate::RemoveGCEpilogueCallback(GCCallback callback) {
  auto i = std::remove(pimpl_->gcEpilogueCallbacks.begin(),
                       pimpl_->gcEpilogueCallbacks.end(), callback);
  pimpl_->gcEpilogueCallbacks.erase(i, pimpl_->gcEpilogueCallbacks.end());
}

void Isolate::RequestGarbageCollectionForTesting(GarbageCollectionType type) {
  JS_GC(Runtime());
}

JSRuntime* Isolate::Runtime() const { return pimpl_->rt; }

Value* Isolate::AddPersistent(Value* val) {
  return pimpl_->EnsurePersistents(this).Add(val);
}

void Isolate::RemovePersistent(Value* val) {
  return pimpl_->EnsurePersistents(this).Remove(val);
}

Template* Isolate::AddPersistent(Template* val) {
  return pimpl_->EnsurePersistents(this).Add(val);
}

void Isolate::RemovePersistent(Template* val) {
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

Template* Isolate::AddEternal(Template* val) {
  return pimpl_->EnsureEternals(this).Add(val);
}

bool Isolate::AddMessageListener(MessageCallback that, Handle<Value> data) {
  // Ignore data parameter.  Node doesn't use it.
  pimpl_->messageListeners.push_back(that);
  return true;
}

void Isolate::RemoveMessageListeners(MessageCallback that) {
  auto i = std::remove(pimpl_->messageListeners.begin(),
                       pimpl_->messageListeners.end(), that);
  pimpl_->messageListeners.erase(i, pimpl_->messageListeners.end());
}

uint32_t Isolate::GetNumberOfDataSlots() {
  return internal::kNumIsolateDataSlots;
}

int64_t Isolate::AdjustAmountOfExternalAllocatedMemory(
    int64_t change_in_bytes) {
  // XXX SpiderMonkey and V8 have different ways of doing memory pressure. V8's
  // value is persistent whereas SpiderMonkey's malloc counter is reset on GC's,
  // so we only report increases in memory to SpiderMonkey, but we track the
  // the persistent value in case an embedder relies on it.
  if (change_in_bytes > 0) {
    auto context = JSContextFromIsolate(this);
    JS_updateMallocCounter(context, change_in_bytes);
  }
  return pimpl_->amountOfExternallyAllocatedMemory += change_in_bytes;
}

void Isolate::SetData(uint32_t slot, void* data) {
  if (slot >= mozilla::ArrayLength(pimpl_->embeddedData)) {
    MOZ_CRASH("Invalid embedded data index");
  }
  pimpl_->embeddedData[slot] = data;
}

void Isolate::SetPromiseRejectCallback(PromiseRejectCallback callback) {
  // TODO: https://github.com/mozilla/spidernode/issues/127
}

void* Isolate::GetData(uint32_t slot) {
  return slot < mozilla::ArrayLength(pimpl_->embeddedData)
             ? pimpl_->embeddedData[slot]
             : nullptr;
}

TryCatch* Isolate::GetTopmostTryCatch() const { return pimpl_->topTryCatch; }

void Isolate::SetTopmostTryCatch(TryCatch* val) { pimpl_->topTryCatch = val; }

size_t Isolate::NumberOfHeapSpaces() {
  // Spidermonkey doesn't expose this and it's only used by node to allocate
  // the heap's name to avoid creating it multiple times.
  return 0;
}

bool Isolate::GetHeapSpaceStatistics(HeapSpaceStatistics* space_statistics,
                                     size_t index) {
  // TODO: https://github.com/mozilla/spidernode/issues/132
  return true;
}

HeapProfiler* Isolate::GetHeapProfiler() {
  // TODO: https://github.com/mozilla/spidernode/issues/131
  return &dummyHeapProfiler;
}

CpuProfiler* Isolate::GetCpuProfiler() {
  // TODO: https://github.com/mozilla/spidernode/issues/130
  return &dummyCpuProfiler;
}
}
