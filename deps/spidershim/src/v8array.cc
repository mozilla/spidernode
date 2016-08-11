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
#include "js/Class.h"

namespace v8 {

Local<Array> Array::New(Isolate* isolate, int length) {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, isolate);
  JS::RootedObject array(cx, JS_NewArrayObject(cx, length));
  JS::Value retVal;
  retVal.setObject(*array);
  return internal::Local<Array>::New(isolate, retVal);
}

Array* Array::Cast(Value* obj) {
  bool isArray = false;
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  AutoJSAPI jsAPI(cx, obj);
  JS::RootedObject thisObj(cx, GetObject(obj));
  JS::IsArray(cx, thisObj, &isArray);
  assert(isArray);
  return static_cast<Array*>(obj);
}

uint32_t Array::Length() const {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject thisObj(cx, GetObject(this));
  uint32_t length = 0;
  JS_GetArrayLength(cx, thisObj, &length);
  return length;
}
}
