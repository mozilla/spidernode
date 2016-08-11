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

namespace v8 {

Local<Private> Private::New(Isolate* isolate, Local<String> name) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::RootedString description(cx);
  if (!name.IsEmpty()) {
    description = GetString(name);
  }
  JS::Symbol* symbol = JS::NewSymbol(cx, description);
  return internal::Local<Private>::New(isolate, symbol);
}

Local<Private> Private::ForApi(Isolate* isolate, Local<String> name) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::RootedString description(cx, GetString(name));
  JS::Symbol* symbol = JS::GetSymbolFor(cx, description);
  return internal::Local<Private>::New(isolate, symbol);
}

Local<Value> Private::Name() const {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::RootedSymbol self(cx, symbol_);
  JSString* description = JS::GetSymbolDescription(self);
  if (!description) {
    return Undefined(isolate);
  }
  JS::Value retVal;
  retVal.setString(description);
  return internal::Local<Value>::New(isolate, retVal);
}
}
