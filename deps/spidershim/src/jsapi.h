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

#pragma once

#include_next "jsapi.h"
#include "v8.h"
#include "conversions.h"

// All callsittes calling into SpiderMonkey MUST have a AutoJSAPI on the stack.
class AutoJSAPI : private JSAutoCompartment {
 public:
  AutoJSAPI(JSContext* cx, JSObject* obj) :
    JSAutoCompartment(cx, obj) {
  }
  AutoJSAPI(JSContext* cx, const JSObject* obj) :
    JSAutoCompartment(cx, const_cast<JSObject*>(obj)) {
  }
  template <class T>
  AutoJSAPI(JSContext* cx, T* val) :
    AutoJSAPI(cx, v8::GetObject(val)) {
  }
  template <class T>
  AutoJSAPI(JSContext* cx, v8::Local<T> val) :
    AutoJSAPI(cx, val.IsEmpty() ? v8::GetObject(v8::Isolate::GetCurrent()->GetCurrentContext()->Global()) :
                                  v8::GetObject(val)) {
  }
  AutoJSAPI(JSContext* cx, v8::Isolate* isolate) :
    AutoJSAPI(cx, v8::GetObject(isolate->GetCurrentContext()->Global())) {
  }
  AutoJSAPI(JSContext* cx) :
    AutoJSAPI(cx, v8::Isolate::GetCurrent()) {
  }
  AutoJSAPI(JSObject* obj) :
    AutoJSAPI(JSContextFromIsolate(v8::Isolate::GetCurrent()), obj) {
  }
  AutoJSAPI(v8::Value* val) :
    AutoJSAPI(JSContextFromIsolate(v8::Isolate::GetCurrent()), val) {
  }
  AutoJSAPI(const v8::Value* val) :
    AutoJSAPI(JSContextFromIsolate(v8::Isolate::GetCurrent()), val) {
  }
  AutoJSAPI() :
    AutoJSAPI(JSContextFromIsolate(v8::Isolate::GetCurrent())) {
  }
};

