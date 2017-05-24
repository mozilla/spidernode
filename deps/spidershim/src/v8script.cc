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

#include "v8.h"
#include "autojsapi.h"
#include "js/CharacterEncoding.h"
#include "v8local.h"
#include "v8string.h"
#include "v8trycatch.h"
#include "mozilla/UniquePtr.h"

namespace v8 {

MaybeLocal<Script> Script::Compile(Local<Context> context, Local<String> source,
                                   ScriptOrigin* origin) {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  Isolate* isolate = context->GetIsolate();
  size_t length = 0;
  JS::UniqueTwoByteChars buffer;
  {
    // With UnboundScript, the source variable here may be a string allocated in the
    // context of a different global, therefore from a different zone, so if we just
    // pass it down to GetFlatString(), SpiderMonkey's zone assertion checks fail.
    // Therefore we need to use JS_WrapValue to copy it into the current zone of
    // cx before using it.
    JS::RootedValue sourceVal(cx, *GetValue(source));
    if (!JS_WrapValue(cx, &sourceVal)) {
      return MaybeLocal<Script>();
    }
    Local<String> src = internal::Local<String>::New(isolate, sourceVal);
    buffer = internal::GetFlatString(cx, src, &length);
  }
  if (!buffer) {
    return MaybeLocal<Script>();
  }
  JS::SourceBufferHolder sbh(buffer.release(), length,
                             JS::SourceBufferHolder::GiveOwnership);
  JS::CompileOptions options(cx);
  mozilla::UniquePtr<String::Utf8Value> utf8;
  options.setVersion(JSVERSION_DEFAULT)
      .setNoScriptRval(false)
      .setUTF8(true)
      .setSourceIsLazy(false)
      .setLine(1)
      .setColumn(0, 0)
      .forceAsync = true;
  ;
  if (origin) {
    if (!origin->ResourceName().IsEmpty()) {
      JS::RootedValue nameVal(cx, *GetValue(origin->ResourceName()));
      if (!JS_WrapValue(cx, &nameVal)) {
        return MaybeLocal<Script>();
      }
      MaybeLocal<String> resourceName = GetV8Value(&nameVal)->ToString(context);
      if (!resourceName.IsEmpty()) {
        utf8 =
            mozilla::MakeUnique<String::Utf8Value>(resourceName.ToLocalChecked());
        options.setFile(**utf8);
      }
    }
    if (!origin->ResourceLineOffset().IsEmpty()) {
      options.setLine(origin->ResourceLineOffset()->Value() + 1);
    }
    if (!origin->ResourceColumnOffset().IsEmpty()) {
      options.setColumn(origin->ResourceColumnOffset()->Value(), 0);
    }
  }
  JS::RootedScript jsScript(cx);
  if (!JS::Compile(cx, options, sbh, &jsScript)) {
    return MaybeLocal<Script>();
  }
  return internal::Local<Script>::New(isolate, jsScript, context);
}

Local<Script> Script::Compile(Local<String> source, ScriptOrigin* origin) {
  MaybeLocal<Script> maybe =
      Compile(Isolate::GetCurrent()->GetCurrentContext(), source, origin);
  if (!maybe.IsEmpty()) {
    return maybe.ToLocalChecked();
  }
  return Local<Script>();
}

MaybeLocal<Value> Script::Run(Local<Context> context) {
  assert(script_);
  JSContext* cx = JSContextFromContext(*context_);
  JS::RootedValue result(cx);
  JS::RootedObject global(cx, GetObject(context_->Global()));
  AutoJSAPI jsAPI(cx, global);
  jsAPI.MarkScriptCall();
  internal::TryCatch tryCatch(context_->GetIsolate());
  if (!JS::CloneAndExecuteScript(cx, JS::Handle<JSScript*>::fromMarkedLocation(&script_),
                                 &result) ||
      !JS_WrapValue(cx, &result)) {
    tryCatch.CheckReportExternalException();
    return MaybeLocal<Value>();
  }
  return internal::Local<Value>::New(context->GetIsolate(), result);
}

Local<Value> Script::Run() {
  MaybeLocal<Value> maybe = Run(Isolate::GetCurrent()->GetCurrentContext());
  if (!maybe.IsEmpty()) {
    return maybe.ToLocalChecked();
  }
  return Local<Value>();
}
}
