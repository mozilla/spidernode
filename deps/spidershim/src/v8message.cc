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
#include "autojsapi.h"
#include "jsfriendapi.h"
#include <string>

namespace v8 {

struct Message::Impl {
  Impl(Value* exception) : lineNumber_(0), columnNumber_(0) {
    Isolate* isolate = Isolate::GetCurrent();
    JSContext* cx = JSContextFromIsolate(isolate);
    AutoJSAPI jsAPI(cx);
    JS::RootedValue exc(cx, *GetValue(exception));
    js::ErrorReport errorReport(cx);
    if (errorReport.init(cx, exc, js::ErrorReport::WithSideEffects)) {
      JSErrorReport* report = errorReport.report();
      assert(report);

      if (report->linebuf() && report->linebufLength()) {
        sourceLine_ = String::NewFromTwoByte(
            isolate, reinterpret_cast<const uint16_t*>(report->linebuf()),
            NewStringType::kNormal, report->linebufLength());
      }
      resourceName_ = String::NewFromOneByte(
          isolate, reinterpret_cast<const uint8_t*>(report->filename));
      lineNumber_ = report->lineno;
      columnNumber_ = report->column;
    }
    if (exc.isObject()) {
      JS::RootedObject obj(cx, &exc.toObject());
      stackTrace_ = StackTrace::ExceptionStackTrace(isolate, obj);
    }
    Local<String> message = exception->ToString();
    std::string str = "Uncaught ";
    if (message.IsEmpty()) {
      str += "exception";
    } else {
      String::Utf8Value utf8(message);
      if (!strcmp(*utf8, "InternalError: too much recursion")) {
        str += "RangeError: Maximum call stack size exceeded";
      } else {
        str += *utf8;
      }
    }
    stringMessage_ = String::NewFromUtf8(isolate, str.c_str());
  }

  MaybeLocal<String> sourceLine_;
  Local<Value> resourceName_;
  Local<StackTrace> stackTrace_;
  int lineNumber_;
  int columnNumber_;
  Local<String> stringMessage_;
};

Message::Message(Local<Value> exception)
    : pimpl_(new Impl(*exception)) {}

Message::~Message() { delete pimpl_; }

MaybeLocal<String> Message::GetSourceLine(Local<Context> context) const {
  return pimpl_->sourceLine_;
}

Local<String> Message::GetSourceLine() const {
  return GetSourceLine(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<String>());
}

Handle<Value> Message::GetScriptResourceName() const {
  return pimpl_->resourceName_;
}

Maybe<int> Message::GetLineNumber(Local<Context> context) const {
  return Just(pimpl_->lineNumber_);
}

int Message::GetLineNumber() const {
  return GetLineNumber(Isolate::GetCurrent()->GetCurrentContext()).FromMaybe(0);
}

Maybe<int> Message::GetStartColumn(Local<Context> context) const {
  return Just(pimpl_->columnNumber_);
}

int Message::GetStartColumn() const {
  return GetStartColumn(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(0);
}

Maybe<int> Message::GetEndColumn(Local<Context> context) const {
  // TODO: Support end column
  //       https://github.com/mozilla/spidernode/issues/54
  return Nothing<int>();
}

int Message::GetEndColumn() const {
  // TODO: Support end column
  //       https://github.com/mozilla/spidernode/issues/54
  return -1;
}

Local<StackTrace> Message::GetStackTrace() const {
  return pimpl_->stackTrace_;
}

Local<String> Message::Get() const {
  return pimpl_->stringMessage_;
}

ScriptOrigin Message::GetScriptOrigin() const {
  Isolate* isolate = Isolate::GetCurrent();
  return ScriptOrigin(pimpl_->resourceName_,
                      Integer::New(isolate, pimpl_->lineNumber_),
                      Integer::New(isolate, pimpl_->columnNumber_));
}
}
