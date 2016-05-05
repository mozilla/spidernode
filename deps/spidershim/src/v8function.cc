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
#include "v8conversions.h"
#include "v8local.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"

namespace v8 {

MaybeLocal<Value>
Function::Call(Local<Context>, Local<Value> recv, int argc, Local<Value> argv[])
{
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedValue val(cx, *reinterpret_cast<JS::Value*>(*recv));
  JS::AutoValueVector args(cx);
  if (!args.reserve(argc)) {
    return Local<Value>();
  }

  for (int i = 0; i < argc; i++) {
	  args.infallibleAppend(*reinterpret_cast<JS::Value*>(*argv[i]));
  }

  JS::RootedValue func(cx, *reinterpret_cast<JS::Value*>(this));
  JS::RootedValue ret(cx);
  if (!JS::Call(cx, val, func, args, &ret)) {
    return Local<Value>();
  }

  return internal::Local<Value>::New(isolate, ret);
}

Local<Value>
Function::Call(Local<Value> obj, int argc, Local<Value> argv[])
{
	Local<Context> ctx = Isolate::GetCurrent()->GetCurrentContext();
  return Call(ctx, obj, argc, argv).ToLocalChecked();
}

Function*
Function::Cast(Value* v)
{
  assert(v->IsFunction());
  return static_cast<Function*>(v);
}
}
