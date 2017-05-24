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

#include <errno.h>

#include "v8.h"
#include "v8context.h"
#include "v8local.h"
#include "autojsapi.h"
#include "jsfriendapi.h"
#include "instanceslots.h"
#include "spidershim_natives.h"
#include "utils.h"

namespace spidershim {

bool InitLibraries(JSContext* cx) {
#define V(id)                                                                 \
  do {                                                                        \
    JS::CompileOptions options(cx);                                           \
    options.setVersion(JSVERSION_DEFAULT)                                     \
        .setNoScriptRval(true)                                                \
        .setSourceIsLazy(false)                                               \
        .setFile(id##_name)                                                   \
        .setLine(1)                                                           \
        .setColumn(0, 0)                                                      \
        .forceAsync = true;                                                   \
    JS::RootedValue value(cx);                                                \
    if (!JS::Evaluate(cx, options,                                            \
                      id##_data,                                              \
                      mozilla::ArrayLength(id##_data), &value)) {             \
      return false;                                                           \
    }                                                                         \
  } while (0);
  NODE_NATIVES_MAP(V)
#undef V
  return true;
}
}

namespace v8 {

Context::Context(JSContext* cx) : pimpl_(new Impl(cx)) {}

Context::~Context() { delete pimpl_; }

Local<Context> Context::New(Isolate* isolate,
                            ExtensionConfiguration* extensions,
                            MaybeLocal<ObjectTemplate> global_template,
                            MaybeLocal<Value> global_object) {
  // TODO: Implement extensions and global_object.
  if (extensions) {
    fprintf(stderr, "ExtensionConfiguration is not supported yet\n");
  }
  if (!global_object.IsEmpty()) {
    fprintf(stderr, "Reusing global objects is not supported yet\n");
  }

  // This HandleScope is needed here so that creating a context without a
  // HandleScope on the stack works correctly.
  HandleScope handleScope(isolate);

  assert(isolate->RuntimeContext());
  JSContext* cx = JSContextFromIsolate(isolate);
  Context* context = new Context(cx);
  if (!context->CreateGlobal(cx, isolate, global_template)) {
    return Local<Context>();
  }
  isolate->AddContext(context);
  return Local<Context>::New(isolate, context);
}

bool Context::CreateGlobal(JSContext* cx, Isolate* isolate,
                           MaybeLocal<ObjectTemplate> maybe_global_template) {
  Local<ObjectTemplate> global_template;
  if (maybe_global_template.IsEmpty()) {
    global_template = ObjectTemplate::New(isolate);
  } else {
    global_template = maybe_global_template.ToLocalChecked();
  }

  Local<FunctionTemplate> global_constructor = global_template->GetConstructor();
  global_constructor->SetPrototypeTemplate(global_template);
  Local<Object> prototype = global_constructor->GetProtoInstance(isolate->GetCurrentContext());
  if (prototype.IsEmpty()) {
    return false;
  }
  Local<Object> global = global_template->NewInstance(prototype,
                                                      ObjectTemplate::GlobalObject);
  if (global.IsEmpty()) {
    return false;
  }

  JS::RootedObject newGlobal(cx, UnwrapProxyIfNeeded(GetObject(global)));
  AutoJSAPI jsAPI(cx, newGlobal);

  SetInstanceSlot(newGlobal, uint32_t(InstanceSlots::ContextSlot),
                  JS::PrivateValue(this));

  pimpl_->global.init(isolate->RuntimeContext());
  pimpl_->global = newGlobal;
  JS::Value globalObj;
  globalObj.setObject(*newGlobal);
  HandleScope handleScope(isolate);
  pimpl_->globalObj.Reset(isolate,
      internal::Local<Object>::New(Isolate::GetCurrent(), globalObj));

#ifdef JS_GC_ZEAL
  const char* env = getenv("JS_GC_MAX_ZEAL");
  if (env && *env) {
    // Set all of the gc zeal modes for maximum verification!
    static bool warned = false;
    if (!warned) {
      fprintf(stderr,
              "Warning: enabled max-zeal GC mode, this is super slow!\n");
      warned = true;
    }
    uint32_t frequency = JS_DEFAULT_ZEAL_FREQ;
    const char* freq = getenv("JS_GC_MAX_ZEAL_FREQ");
    if (freq && *freq) {
      errno = 0;
      uint32_t f = atoi(freq);
      if (errno == 0) {
        frequency = f;
      }
    }
    for (uint32_t i = 1; i <= 14; ++i) {
      JS_SetGCZeal(cx, i, frequency);
    }
  }
#endif

  return spidershim::InitLibraries(cx);
}

Local<Object> Context::Global() { return pimpl_->globalObj; }

void Context::Dispose() {
  delete this;
}

void Context::Enter() {
  assert(pimpl_);
  assert(pimpl_->global);
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JS_BeginRequest(cx);
  pimpl_->oldCompartment = JS_EnterCompartment(cx, pimpl_->global);
  GetIsolate()->PushCurrentContext(this);
}

void Context::Exit() {
  assert(pimpl_);
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  // pimpl_->oldCompartment can be nullptr.
  JS_LeaveCompartment(cx, pimpl_->oldCompartment);
  JS_EndRequest(cx);
  GetIsolate()->PopCurrentContext();
}

void Context::SetEmbedderData(int idx, Local<Value> data) {
  assert(idx >= 0);
  if (static_cast<unsigned int>(idx * 2) >= pimpl_->embedderData.length() &&
      !pimpl_->embedderData.resize((idx + 1) * 2)) {
    return;
  }

  pimpl_->embedderData[idx * 2].set(*GetValue(data));
}

Local<Value> Context::GetEmbedderData(int idx) {
  assert(idx >= 0);
  if (static_cast<unsigned int>(idx * 2) >= pimpl_->embedderData.length()) {
    return Local<Value>();
  }

  return Local<Value>::New(Isolate::GetCurrent(),
                           GetV8Value(pimpl_->embedderData[idx * 2]));
}

// We encode each embedder data field into two embedderData indices in order
// to be able to encode full 64-bit values.
void Context::SetAlignedPointerInEmbedderData(int idx, void* data) {
  assert((reinterpret_cast<uintptr_t>(data) & 0x1) == 0);
  assert(idx >= 0);
  if (static_cast<unsigned int>(idx * 2) >= pimpl_->embedderData.length() &&
      !pimpl_->embedderData.resize((idx + 1) * 2)) {
    return;
  }

  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedValue ptr1(cx), ptr2(cx);
  CallbackToValues(data, &ptr1, &ptr2);
  pimpl_->embedderData[idx * 2].set(ptr1);
  pimpl_->embedderData[idx * 2 + 1].set(ptr2);
}

void* Context::GetAlignedPointerFromEmbedderData(int idx) {
  assert(idx >= 0);
  if (static_cast<unsigned int>(idx * 2) >= pimpl_->embedderData.length()) {
    return nullptr;
  }

  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedValue ptr1(cx, pimpl_->embedderData[idx * 2].get());
  JS::RootedValue ptr2(cx, pimpl_->embedderData[idx * 2 + 1].get());
  return ValuesToCallback<void*>(ptr1, ptr2);
}

bool Context::Impl::RunMicrotasks() {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  AutoJSAPI jsAPI(cx);
  bool ranAny = false;
  // The following code was adapted from spidermonkey's shell.
  JS::RootedObject job(cx);
  JS::HandleValueArray args(JS::HandleValueArray::empty());
  JS::RootedValue rval(cx);
  // Execute jobs in a loop until we've reached the end of the queue.
  // Since executing a job can trigger enqueuing of additional jobs,
  // it's crucial to re-check the queue length during each iteration.
  while (jobQueue.length() || jobQueueNative.size()) {
    ranAny = true;
    for (size_t i = 0; i < jobQueue.length(); i++) {
        job = jobQueue[i];
        JSAutoCompartment ac(cx, job);
        if (!JS::Call(cx, JS::UndefinedHandleValue, job, args, &rval))
            v8::ReportException(cx);
        jobQueue[i].set(nullptr);
    }
    jobQueue.clear();
    for (size_t i = 0; i < jobQueueNative.size(); i++) {
      const auto& job = jobQueueNative[i];
      job.first(job.second);
    }
    jobQueueNative.clear();
  }
  return ranAny;
}

Isolate* Context::GetIsolate() { return Isolate::GetCurrent(); }

JSContext* JSContextFromContext(Context* context) {
  return JSContextFromIsolate(context->GetIsolate());
}

void Context::SetSecurityToken(Handle<Value> token) {
  // TODO: https://github.com/mozilla/spidernode/issues/87
}

Handle<Value> Context::GetSecurityToken() {
  // TODO: https://github.com/mozilla/spidernode/issues/87
  return Handle<Value>();
}

} // namespace v8
