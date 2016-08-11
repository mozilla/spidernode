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
#include <algorithm>
#include <vector>

#include "v8.h"
#include "conversions.h"
#include "v8local.h"
#include "autojsapi.h"
#include "jsfriendapi.h"

namespace v8 {

struct StackTrace::Impl {
  Impl() : resolved_(false) {}
  void SetStack(Isolate* isolate, JSObject* stack) {
    JS::Value stackVal;
    stackVal.setObject(*stack);
    stack_ = internal::Local<Object>::New(isolate, stackVal);
  }
  bool ResolveIfNeeded() {
    if (resolved_) {
      return true;
    }

    if (stack_.IsEmpty()) {
      return false;
    }

    struct FrameInfo {
      Local<String> source_;
      Local<String> displayName_;
      uint32_t line_ = 0, column_ = 0;
      // TODO: Extract other fields such as IsEval, IsConstructor, etc.
    };

    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> context = isolate->GetCurrentContext();
    JSContext* cx = JSContextFromIsolate(isolate);
    std::vector<FrameInfo> frames;
    JS::RootedObject current(cx, GetObject(stack_));
    AutoJSAPI jsAPI(cx, current);

    do {
      FrameInfo info;
      if (options_ &
          (StackTrace::kScriptName | StackTrace::kScriptNameOrSourceURL)) {
        JS::RootedString source(cx);
        if (JS::SavedFrameResult::Ok ==
            JS::GetSavedFrameSource(cx, current, &source,
                                    JS::SavedFrameSelfHosted::Exclude)) {
          JS::Value val;
          val.setString(source);
          info.source_ = internal::Local<String>::New(isolate, val);
        }
      }
      if (options_ & StackTrace::kFunctionName) {
        JS::RootedString name(cx);
        if (JS::SavedFrameResult::Ok ==
            JS::GetSavedFrameFunctionDisplayName(
                cx, current, &name, JS::SavedFrameSelfHosted::Exclude) &&
            name) {
          JS::Value val;
          val.setString(name);
          info.displayName_ = internal::Local<String>::New(isolate, val);
        }
      }
      if (options_ & StackTrace::kLineNumber) {
        uint32_t line = 0;
        if (JS::SavedFrameResult::Ok ==
            JS::GetSavedFrameLine(cx, current, &line,
                                  JS::SavedFrameSelfHosted::Exclude)) {
          info.line_ = line;
        }
      }
      if (options_ & StackTrace::kColumnOffset) {
        uint32_t column = 0;
        if (JS::SavedFrameResult::Ok ==
            JS::GetSavedFrameColumn(cx, current, &column,
                                    JS::SavedFrameSelfHosted::Exclude)) {
          info.column_ = column;
        }
      }
      frames.push_back(info);
    } while (JS::SavedFrameResult::Ok ==
                 JS::GetSavedFrameParent(cx, current, &current,
                                         JS::SavedFrameSelfHosted::Exclude) &&
             current);

    Local<Value> nameKey = String::NewFromUtf8(isolate, "scriptName");
    Local<Value> sourceKey =
        String::NewFromUtf8(isolate, "scriptNameOrSourceURL");
    Local<Value> functionKey = String::NewFromUtf8(isolate, "functionName");
    Local<Value> lineKey = String::NewFromUtf8(isolate, "lineNumber");
    Local<Value> columnKey = String::NewFromUtf8(isolate, "column");
    Local<Value> scriptIdKey = String::NewFromUtf8(isolate, "scriptId");
    Local<Value> isEvalKey = String::NewFromUtf8(isolate, "isEval");
    Local<Value> isConstructorKey =
        String::NewFromUtf8(isolate, "isConstructor");
    Local<Array> finalFrames = Array::New(isolate, frames.size());
    uint32_t index = 0;
    for (const auto& info : frames) {
      Local<Object> frame = Object::New(isolate);
      if (options_ & StackTrace::kScriptName) {
        Maybe<bool> result = frame->Set(context, nameKey, info.source_);
        if (!result.FromMaybe(false)) {
          return false;
        }
      }
      if (options_ & StackTrace::kScriptNameOrSourceURL) {
        Maybe<bool> result = frame->Set(context, sourceKey, info.source_);
        if (!result.FromMaybe(false)) {
          return false;
        }
      }
      if ((options_ & StackTrace::kFunctionName) &&
          !info.displayName_.IsEmpty()) {
        Maybe<bool> result =
            frame->Set(context, functionKey, info.displayName_);
        if (!result.FromMaybe(false)) {
          return false;
        }
      }
      if (options_ & StackTrace::kLineNumber) {
        Maybe<bool> result = frame->Set(
            context, lineKey, Integer::NewFromUnsigned(isolate, info.line_));
        if (!result.FromMaybe(false)) {
          return false;
        }
      }
      if (options_ & StackTrace::kColumnOffset) {
        Maybe<bool> result =
            frame->Set(context, columnKey,
                       Integer::NewFromUnsigned(isolate, info.column_));
        if (!result.FromMaybe(false)) {
          return false;
        }
      }

      // Fill in the properties that we don't yet support too!
      if (options_ & StackTrace::kIsEval) {
        Maybe<bool> result =
            frame->Set(context, isEvalKey, Boolean::New(isolate, false));
        if (!result.FromMaybe(false)) {
          return false;
        }
      }
      if (options_ & StackTrace::kIsConstructor) {
        Maybe<bool> result =
            frame->Set(context, isConstructorKey, Boolean::New(isolate, false));
        if (!result.FromMaybe(false)) {
          return false;
        }
      }
      if (options_ & StackTrace::kScriptId) {
        Maybe<bool> result = frame->Set(context, scriptIdKey,
                                        Integer::NewFromUnsigned(isolate, 0));
        if (!result.FromMaybe(false)) {
          return false;
        }
      }

      Maybe<bool> result = finalFrames->Set(context, index++, frame);
      if (!result.FromMaybe(false)) {
        return false;
      }
    }

    frames_ = finalFrames;
    return resolved_ = true;
  }

  bool resolved_;
  StackTraceOptions options_;
  Local<Object> stack_;
  Local<Array> frames_;
};

StackTrace::StackTrace() : pimpl_(new Impl()) {}

StackTrace::~StackTrace() { delete pimpl_; }

Local<StackTrace> StackTrace::CurrentStackTrace(Isolate* isolate,
                                                int frame_limit,
                                                StackTraceOptions options) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::RootedObject stack(cx);
  frame_limit = std::max(frame_limit, 0);  // no negative values
  if (!JS::CaptureCurrentStack(cx, &stack,
                               JS::StackCapture(JS::MaxFrames(frame_limit)))) {
    return Local<StackTrace>();
  }
  return CreateStackTrace(isolate, stack, options);
}

Local<StackTrace> StackTrace::ExceptionStackTrace(Isolate* isolate,
                                                  JSObject* exception) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::RootedObject exc(cx, exception);
  JS::RootedObject stack(cx, ExceptionStackOrNull(exc));
  if (!stack) {
    return Local<StackTrace>();
  }
  return CreateStackTrace(isolate, stack, kOverview);
}

Local<StackTrace> StackTrace::CreateStackTrace(Isolate* isolate,
                                               JSObject* stack,
                                               StackTraceOptions options) {
  // SpiderMonkey doesn't have the concept of options, so we ignore that here.
  StackTrace* trace = new StackTrace();
  isolate->AddStackTrace(trace);
  trace->pimpl_->options_ = options;
  trace->pimpl_->SetStack(isolate, stack);
  return Local<StackTrace>::New(isolate, trace);
}

int StackTrace::GetFrameCount() const {
  if (!pimpl_->ResolveIfNeeded()) {
    return 0;
  }
  return pimpl_->frames_->Length();
}

Local<Array> StackTrace::AsArray() {
  if (!pimpl_->ResolveIfNeeded()) {
    return Local<Array>();
  }
  return pimpl_->frames_;
}

Local<StackFrame> StackTrace::GetFrame(uint32_t index) const {
  if (!pimpl_->ResolveIfNeeded() || index >= pimpl_->frames_->Length()) {
    return Local<StackFrame>();
  }
  StackFrame* frame = new StackFrame(pimpl_->frames_->Get(index)->ToObject());
  Isolate* isolate = Isolate::GetCurrent();
  isolate->AddStackFrame(frame);
  return Local<StackFrame>::New(isolate, frame);
}
}
