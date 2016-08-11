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

#include <assert.h>

#include "v8.h"
#include "conversions.h"
#include "v8local.h"
#include "v8isolate.h"
#include "v8trycatch.h"
#include "autojsapi.h"
#include "jsfriendapi.h"

namespace v8 {

struct TryCatch::Impl {
  Impl(class Isolate* iso, TryCatch* self)
      : isolate_(iso),
        prev_(iso->GetTopmostTryCatch()),
        verbose_(false),
        internal_(false) {
    iso->SetTopmostTryCatch(self);
    Reset();
  }
  ~Impl() {
    // Internal TryCatch should never catch exceptions unless there is an outer
    // one.
    assert(!internal_ || !prev_ || !hasExceptionSet_);

    isolate_->SetTopmostTryCatch(prev_);
    JSContext* cx = JSContextFromIsolate(isolate_);
    if (!rethrow_ && JS_IsExceptionPending(cx)) {
      JS_ClearPendingException(cx);
    }
  }
  class Isolate* Isolate() const {
    return isolate_;
  }
  bool HasException() const {
    if (!hasExceptionSet_ && !GetAndClearExceptionIfNeeded()) {
      // TODO: Report the error in a some way.
      return false;
    }
    assert(hasExceptionSet_);
    return hasException_;
  }
  bool HasExceptionSet() const { return hasExceptionSet_; }
  static bool HasExceptionPending(JSContext* cx) {
    return JS_IsExceptionPending(cx);
  }
  void SetException(JS::HandleValue exception) const {
    assert(!hasExceptionSet_ || !HasException());
    hasExceptionSet_ = true;
    exception_ = exception.get();
  }
  void ReThrow() {
    if (rethrow_) {
      // Nothing to do!
      return;
    }
    if (!GetAndClearExceptionIfNeeded()) {
      return;
    }
    JSContext* cx = JSContextFromIsolate(isolate_);
    assert(hasExceptionSet_ && HasException() && !HasExceptionPending(cx));
    JS::RootedValue exc(cx, *exception_.unsafeGet());
    if (!JS_WrapValue(cx, &exc)) {
      // TODO: signal the failure somehow.
      return;
    }
    JS_SetPendingException(cx, exc);
    rethrow_ = true;
  }
  JS::Value* Exception() {
    assert(hasExceptionSet_ && HasException());
    return exception_.unsafeGet();
  }
  void SetVerbose(bool verbose) { verbose_ = verbose; }
  bool IsVerbose() const { return verbose_; }
  void SetInternal() { internal_ = true; }
  bool IsInternal() const { return internal_; }
  TryCatch* Previous() const { return prev_; }
  TryCatch* PreviousNonInternal() const {
    TryCatch* tc = prev_;
    while (tc && tc->pimpl_->IsInternal()) {
      tc = tc->pimpl_->prev_;
    }
    return tc;
  }
  void IgnoreException() {
    assert(!hasExceptionSet_);
    rethrow_ = true;
  }

  void Reset() {
    hasException_ = false;
    hasExceptionSet_ = false;
    rethrow_ = false;
    exception_ = JS::UndefinedValue();
  }
  bool GetAndClearExceptionIfNeeded() const {
    if (HasExceptionSet()) {
      return true;
    }
    hasException_ = false;
    JSContext* cx = JSContextFromIsolate(isolate_);
    AutoJSAPI jsAPI(cx);
    if (!HasExceptionPending(cx)) {
      return false;
    }
    JS::RootedValue exc(cx);
    if (JS_GetPendingException(cx, &exc)) {
      if (verbose_) {
        // This function clears the pending exception automatically.
        v8::ReportException(cx);
      } else {
        JS_ClearPendingException(cx);
      }
      SetException(exc);
      hasException_ = true;
      return true;
    }
    return false;
  }

 private:
  class Isolate* isolate_;
  class TryCatch* prev_;
  mutable JS::Heap<JS::Value> exception_;
  mutable bool hasException_;
  mutable bool hasExceptionSet_;
  mutable bool rethrow_;
  bool verbose_;
  bool internal_;
};

TryCatch::TryCatch(Isolate* iso)
    : pimpl_(new Impl(iso ? iso : Isolate::GetCurrent(), this)) {}

internal::TryCatch::TryCatch(Isolate* iso) : v8::TryCatch(iso) {
  pimpl_->SetInternal();
}

TryCatch::~TryCatch() {
  assert(pimpl_->Isolate()->GetTopmostTryCatch() == this);
  delete pimpl_;
}

bool TryCatch::HasCaught() const { return pimpl_->HasException(); }

bool TryCatch::HasTerminated() const {
  // TODO: Expose SpiderMonkey uncatchable exceptions
  //       https://github.com/mozilla/spidernode/issues/52
  return false;
}

Local<Value> TryCatch::ReThrow() {
  pimpl_->ReThrow();
  return internal::Local<Value>::New(pimpl_->Isolate(), *pimpl_->Exception());
}

Local<Value> TryCatch::Exception() const {
  if (!pimpl_->HasException()) {
    return Local<Value>();
  }
  return internal::Local<Value>::New(pimpl_->Isolate(), *pimpl_->Exception());
}

void TryCatch::Reset() { pimpl_->Reset(); }

void TryCatch::SetVerbose(bool verbose) { pimpl_->SetVerbose(verbose); }

void TryCatch::CheckReportExternalException() {
  assert(pimpl_->IsInternal());
  // Transfer the exception to the previous non-internal TryCatch if there is
  // one.
  TryCatch* nonInternal = pimpl_->PreviousNonInternal();
  if (nonInternal) {
    nonInternal->pimpl_->Reset();
    nonInternal->pimpl_->GetAndClearExceptionIfNeeded();
  }

  if (!pimpl_->Previous() || pimpl_->Previous()->pimpl_->IsVerbose()) {
    auto exceptionHolder = pimpl_->Previous() ? nonInternal : this;
    auto isolateImpl =
        reinterpret_cast<Isolate::Impl*>(pimpl_->Isolate()->pimpl_);
    auto messageListeners = isolateImpl->messageListeners;
    for (auto i : messageListeners) {
      (*i)(exceptionHolder->Message(), exceptionHolder->Exception());
    }
  } else {
    pimpl_->IgnoreException();
  }
}

Local<Message> TryCatch::Message() const {
  if (!pimpl_->HasException()) {
    return Local<class Message>();
  }
  auto msg = new class Message(Exception());
  return Local<class Message>::New(Isolate::GetCurrent(), msg);
}

MaybeLocal<Value> TryCatch::StackTrace(Local<Context> context) const {
  if (!pimpl_->HasException()) {
    return MaybeLocal<Value>();
  }
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JS::RootedValue excValue(cx, *GetValue(Exception()));
  if (!excValue.isObject()) {
    return MaybeLocal<Value>();
  }
  JS::RootedObject exc(cx, &excValue.toObject());
  JS::RootedObject stack(cx, ExceptionStackOrNull(exc));
  if (!stack) {
    return MaybeLocal<Value>();
  }
  JS::RootedString stackString(cx);
  if (!JS::BuildStackString(cx, stack, &stackString)) {
    return MaybeLocal<Value>();
  }
  JS::Value retVal;
  retVal.setString(stackString);
  return internal::Local<Value>::New(context->GetIsolate(), retVal);
}

Local<Value> TryCatch::StackTrace() const {
  return StackTrace(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Value>());
}
}
