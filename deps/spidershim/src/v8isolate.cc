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

#ifdef _MSC_VER
#include <windows.h>
#define pthread_self() GetCurrentThreadId()
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "v8.h"
#include "v8-profiler.h"
#include "v8isolate.h"
#include "v8local.h"
#include "instanceslots.h"
#include "autojsapi.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ThreadLocal.h"

namespace v8 {

uintptr_t Isolate::sStackSize = 128 * sizeof(size_t) * 1024;

HeapProfiler dummyHeapProfiler;
CpuProfiler dummyCpuProfiler;

struct IsolateStackItem {
  IsolateStackItem(uintptr_t id, IsolateStackItem* prev, Isolate* iso)
    : thread_id(id),
      prev_item(prev),
      isolate(iso) {}
  uintptr_t thread_id;
  IsolateStackItem* prev_item;
  Isolate* isolate;
  int count = 1;
};

static MOZ_THREAD_LOCAL(IsolateStackItem*) sIsolateStack;

namespace internal {

bool InitializeIsolate() { return sIsolateStack.init(); }
}

void Isolate::Impl::OnGC(JSContext* cx, JSGCStatus status, void* data) {
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

bool Isolate::Impl::EnqueuePromiseJobCallback(JSContext* cx, JS::HandleObject job,
                                              JS::HandleObject allocationSite,
                                              JS::HandleObject incumbentGlobal, void* data) {
  Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
  return context->pimpl_->jobQueue.append(job);
}

static JSObject* GetIncumbentGlobalCallback(JSContext* cx) {
  return JS::CurrentGlobalOrNull(cx);
}

Isolate::Isolate() : pimpl_(new Impl()) {
  const uint32_t defaultHeapSize = sizeof(void*) == 8 ? 1024 * 1024 * 1024
                                                      :    // 1GB
                                       512 * 1024 * 1024;  // 512MB
  pimpl_->cx = JS_NewContext(defaultHeapSize);
  // Assert success for now!
  if (!pimpl_->cx) {
    MOZ_CRASH("Creating the JS Runtime failed!");
  }
  JS::SetWarningReporter(pimpl_->cx, Impl::WarningReporter);
  JS_SetGCParameter(pimpl_->cx, JSGC_MODE, JSGC_MODE_INCREMENTAL);
  JS_SetGCParameter(pimpl_->cx, JSGC_MAX_BYTES, 0xffffffff);
  JS_SetNativeStackQuota(pimpl_->cx, sStackSize);
  JS_SetDefaultLocale(pimpl_->cx, "UTF-8");
  js::SetStackFormat(pimpl_->cx, js::StackFormat::V8);

#ifndef DEBUG
  JS::ContextOptionsRef(pimpl_->cx)
      .setBaseline(true)
      .setIon(true)
      .setAsmJS(true)
      .setNativeRegExp(true);
#endif

  JS::SetEnqueuePromiseJobCallback(pimpl_->cx, Isolate::Impl::EnqueuePromiseJobCallback);
  JS::SetGetIncumbentGlobalCallback(pimpl_->cx, GetIncumbentGlobalCallback);
  JS_AddInterruptCallback(pimpl_->cx, Isolate::Impl::OnInterrupt);
  JS_SetGCCallback(pimpl_->cx, Isolate::Impl::OnGC, NULL);
  if (!JS::InitSelfHostedCode(pimpl_->cx)) {
    MOZ_CRASH("InitSelfHostedCode failed");
  }

  pimpl_->EnsurePersistents(this);
  pimpl_->EnsureEternals(this);
}

Isolate::~Isolate() {
  assert(pimpl_->cx);
  assert(!sIsolateStack.get());
  JS_DisableInterruptCallback(pimpl_->cx);
  JS_DestroyContext(pimpl_->cx);
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

Isolate* Isolate::GetCurrent() { return sIsolateStack.get()->isolate; }

void Isolate::Enter() {
  auto current = sIsolateStack.get();
  if (current) {
    assert(current->isolate);
    if (current->isolate == this) {
      // Re-entering the current isolate.  Must be on the same thread!
      assert(!current->prev_item ||
             current->prev_item->thread_id == uintptr_t(pthread_self()));
      ++current->count;
      return;
    }
  }

  // Entering a new Isolate. Push a new entry on top of the stack.
  sIsolateStack.set(new IsolateStackItem(uintptr_t(pthread_self()),
                                         current,
                                         this));
}

void Isolate::Exit() {
  auto current = sIsolateStack.get();
  assert(current);
  assert(current->thread_id == uintptr_t(pthread_self()));
  assert(current->isolate == this);
  assert(current->count > 0);

  if (--current->count > 0) {
    // Leave an Isolate that has been entered multiple times.
    return;
  }

  // Pop the stack.
  sIsolateStack.set(current->prev_item);
  delete current;
}

void Isolate::Dispose() {
  if (sIsolateStack.get()) {
    MOZ_CRASH("Cannot dispose an Isolate that is entered by a thread");
  }
  Enter();
  JS_SetGCCallback(pimpl_->cx, NULL, NULL);
  for (auto frame : pimpl_->stackFrames) {
    delete frame;
  }
  for (auto trace : pimpl_->stackTraces) {
    delete trace;
  }
  for (auto script : pimpl_->unboundScripts) {
    delete script;
  }
  for (auto context : pimpl_->contexts) {
    context->Dispose();
  }
  pimpl_->hiddenGlobal.Reset();
  pimpl_->eternals.reset();
  pimpl_->persistents.reset();
  Exit();
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
  if (pimpl_->currentContexts.empty()) {
    return Local<Context>();
  }
  return Local<Context>::New(this, pimpl_->currentContexts.top());
}

JSContext* JSContextFromIsolate(v8::Isolate* isolate) {
  assert(isolate);
  assert(isolate->pimpl_);
  return isolate->pimpl_->cx;
}

void Isolate::AddUnboundScript(UnboundScript* script) {
  assert(pimpl_);
  pimpl_->unboundScripts.push_back(script);
}

int Isolate::GetMicrotaskDepth() const {
  return pimpl_->microtaskDepth;
}

void Isolate::AdjustMicrotaskDepth(int change) {
  pimpl_->microtaskDepth += change;
}

#ifdef DEBUG
int Isolate::GetMicrotaskDebugDepth() const {
  return pimpl_->debugMicrotaskDepth;
}

void Isolate::AdjustMicrotaskDebugDepth(int change) {
  pimpl_->debugMicrotaskDepth += change;
}
#endif

bool Isolate::IsRunningMicrotasks() const {
  return pimpl_->runningMicrotasks;
}

void Isolate::SetMicrotasksPolicy(MicrotasksPolicy policy) {
  pimpl_->microtaskPolicy = policy;
}

void Isolate::SetAutorunMicrotasks(bool autorun) {
  SetMicrotasksPolicy(autorun ? MicrotasksPolicy::kAuto : MicrotasksPolicy::kExplicit);
}

MicrotasksPolicy Isolate::GetMicrotasksPolicy() const {
  return pimpl_->microtaskPolicy;
}

bool Isolate::WillAutorunMicrotasks() const {
  return GetMicrotasksPolicy() == MicrotasksPolicy::kAuto;
}

void Isolate::RunMicrotasks() {
  Local<Context> context = GetCurrentContext();
  pimpl_->runningMicrotasks = true;
  bool ranAny = context->pimpl_->RunMicrotasks();
  pimpl_->runningMicrotasks = false;
  if (ranAny) {
    for (auto callback : pimpl_->microtaskCompletionCallbacks) {
      callback(this);
    }
  }
}

void Isolate::EnqueueMicrotask(Local<Function> microtask) {
  auto context = JSContextFromIsolate(this);
  AutoJSAPI jsAPI(context);
  JSContext* cx = pimpl_->cx;
  JS::RootedObject fun(cx, GetObject(microtask));
  pimpl_->EnqueuePromiseJobCallback(cx, fun, nullptr, nullptr, nullptr);
}

void Isolate::EnqueueMicrotask(MicrotaskCallback microtask, void* data) {
  Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
  return context->pimpl_->jobQueueNative.push_back(std::make_pair(microtask, data));
}

bool Isolate::IsExecutionTerminating() {
  return pimpl_->terminatingExecution;
}

void Isolate::TerminateExecution() {
  assert(pimpl_->cx);
  pimpl_->terminatingExecution = true;
  pimpl_->serviceInterrupt = true;
  JS_RequestInterruptCallback(pimpl_->cx);
}

void Isolate::CancelTerminateExecution() {
  assert(pimpl_->cx);
  pimpl_->terminatingExecution = false;
}

Local<Value> Isolate::ThrowException(Local<Value> exception) {
  auto context = JSContextFromIsolate(this);
  AutoJSAPI jsAPI(context);
  JS::RootedValue rval(context, *GetValue(exception));
  JS_SetPendingException(context, rval);
  jsAPI.BleedThroughExceptions();
  return Undefined(this);
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
  JS_GC(pimpl_->cx);
}

JSContext* Isolate::RuntimeContext() const { return pimpl_->cx; }

Value* Isolate::AddPersistent(Value* val) {
  return pimpl_->persistents->Add(val);
}

void Isolate::RemovePersistent(Value* val) {
  return pimpl_->persistents->Remove(val);
}

Template* Isolate::AddPersistent(Template* val) {
  return pimpl_->persistents->Add(val);
}

void Isolate::RemovePersistent(Template* val) {
  return pimpl_->persistents->Remove(val);
}

Signature* Isolate::AddPersistent(Signature* val) {
  return pimpl_->persistents->Add(val);
}

void Isolate::RemovePersistent(Signature* val) {
  return pimpl_->persistents->Remove(val);
}

AccessorSignature* Isolate::AddPersistent(AccessorSignature* val) {
  return pimpl_->persistents->Add(val);
}

void Isolate::RemovePersistent(AccessorSignature* val) {
  return pimpl_->persistents->Remove(val);
}

Message* Isolate::AddPersistent(Message* val) {
  return pimpl_->persistents->Add(val);
}

void Isolate::RemovePersistent(Message* val) {
  return pimpl_->persistents->Remove(val);
}

size_t Isolate::PersistentCount() const {
  if (pimpl_->persistents.isNothing()) {
    return 0;
  }
  return pimpl_->persistents->RootedCount();
}

Value* Isolate::AddEternal(Value* val) {
  return pimpl_->eternals->Add(val);
}

Private* Isolate::AddEternal(Private* val) {
  return pimpl_->eternals->Add(val->symbol_);
}

Template* Isolate::AddEternal(Template* val) {
  return pimpl_->eternals->Add(val);
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

void Isolate::SetFatalErrorHandler(FatalErrorCallback that) {
  // TODO: https://github.com/mozilla/spidernode/issues/125
}

void Isolate::SetAbortOnUncaughtExceptionCallback(
  AbortOnUncaughtExceptionCallback callback) {
  // TODO: https://github.com/mozilla/spidernode/issues/126
}

Isolate::SuppressMicrotaskExecutionScope::SuppressMicrotaskExecutionScope
  (Isolate* isolate) : isolate_(isolate) {
  isolate_->pimpl_->microtaskSuppressions += 1;
  isolate_->AdjustCallDepth(+1);
}

Isolate::SuppressMicrotaskExecutionScope::~SuppressMicrotaskExecutionScope() {
  isolate_->pimpl_->microtaskSuppressions -= 1;
  isolate_->AdjustCallDepth(-1);
}

bool Isolate::IsMicrotaskExecutionSuppressed() const {
  return !!pimpl_->microtaskSuppressions;
}

int Isolate::GetCallDepth() const {
  return pimpl_->callDepth;
}

void Isolate::AdjustCallDepth(int change) {
  pimpl_->callDepth += change;
}

void Isolate::AddMicrotasksCompletedCallback(MicrotasksCompletedCallback callback) {
  pimpl_->microtaskCompletionCallbacks.insert(callback);
}

void Isolate::RemoveMicrotasksCompletedCallback(MicrotasksCompletedCallback callback) {
  auto existing = pimpl_->microtaskCompletionCallbacks.find(callback);
  if (existing != pimpl_->microtaskCompletionCallbacks.end()) {
    pimpl_->microtaskCompletionCallbacks.erase(existing);
  }
}

Local<Object> Isolate::GetHiddenGlobal() {
  if (pimpl_->hiddenGlobal.IsEmpty()) {
    // Some V8 APIs are usable before a Context is created, but the
    // underlying SpiderMonkey machinery requires a global object set up
    // and etc.  So we need to create a hidden global object which is
    // only used in those cases.
    static const JSClassOps cOps = {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, JS_GlobalObjectTraceHook};
    static const JSClass globalClass = {
      "HiddenGlobalObject",
      // SpiderMonkey allocates JSCLASS_GLOBAL_APPLICATION_SLOTS (5) reserved slots
      // by default for global objects, so we need to allocate 5 fewer slots here.
      // This also means that any access to these slots must account for this
      // difference between the global and normal objects.
      JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(uint32_t(InstanceSlots::NumSlots) - JSCLASS_GLOBAL_APPLICATION_SLOTS),
      &cOps};

    HandleScope handleScope(this);
    JSContext* cx = pimpl_->cx;
    JSAutoRequest ar(cx);
    JS::RootedObject newGlobal(cx);
    JS::CompartmentOptions options;
    options.behaviors().setVersion(JSVERSION_LATEST);
    newGlobal = JS_NewGlobalObject(cx, &globalClass, nullptr,
                                   JS::FireOnNewGlobalHook, options);
    if (!newGlobal) {
      return Local<Object>();
    }
    JSAutoCompartment ac(cx, newGlobal);
    if (!JS_InitStandardClasses(cx, newGlobal)) {
      return Local<Object>();
    }
    SetInstanceSlot(newGlobal, uint32_t(InstanceSlots::ContextSlot),
                    JS::UndefinedHandleValue);

    Local<Object> global =
      internal::Local<Object>::New(this, JS::ObjectValue(*newGlobal));
    pimpl_->hiddenGlobal.Reset(this, global);
  }

  return Local<Object>::New(this, pimpl_->hiddenGlobal);
}

void Isolate::LowMemoryNotification() {
  // TODO: Not yet implemented.
}

}
