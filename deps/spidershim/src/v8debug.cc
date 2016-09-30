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
#include "v8-debug.h"
#include "jsapi.h"
#include "v8local.h"

namespace {

using namespace v8;

void SetBreakpoint(const FunctionCallbackInfo<Value>& info) {
  // TODO: Implement this!
}

void MakeMirror(const FunctionCallbackInfo<Value>& info) {
  // TODO: Do some real error checking here.
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedObject thisObj(cx, GetObject(info.This()));
  JS::RootedValue result(cx);
  {
    JSAutoCompartment ac(cx, thisObj);
    Local<Object> mirror = Object::New(isolate);
    mirror->Set(String::NewFromUtf8(isolate, "type"),
                String::NewFromUtf8(isolate, "undefined"));
    mirror->Set(String::NewFromUtf8(isolate, "value"), Undefined(isolate));
    result.setObject(*GetObject(mirror));
  }
  JS_WrapValue(cx, &result);
  info.GetReturnValue().Set(internal::Local<Value>::New(isolate, result));
}
}

namespace v8 {

Local<Context> Debug::GetDebugContext(Isolate* isolate) {
  Local<Context> dbgContext = Context::New(isolate);
  if (dbgContext.IsEmpty()) {
    return dbgContext;
  }
  Context::Scope scope(dbgContext);
  EscapableHandleScope handleScope(isolate);
  // Emulate the basics of the V8 debug API to the extent needed by Node.
  Local<Object> Debug = Object::New(isolate);
  if (Debug.IsEmpty()) {
    return Local<Context>();
  }
  Local<Function> setBreakpoint = Function::New(isolate, SetBreakpoint);
  if (setBreakpoint.IsEmpty()) {
    return Local<Context>();
  }
  Local<Function> makeMirror = Function::New(isolate, MakeMirror);
  if (makeMirror.IsEmpty()) {
    return Local<Context>();
  }
  // TODO: Do some real error checking here.
  Debug->Set(String::NewFromUtf8(isolate, "setBreakpoint"), setBreakpoint);
  Debug->Set(String::NewFromUtf8(isolate, "makeMirror"), makeMirror);
  dbgContext->Global()->Set(String::NewFromUtf8(isolate, "Debug"), handleScope.Escape(Debug));
  return dbgContext;
}
}
