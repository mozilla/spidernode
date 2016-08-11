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
#include "autojsapi.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "conversions.h"
#include "v8local.h"

namespace v8 {

Local<Value> StringObject::New(Handle<String> value) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::Value* inputVal = GetValue(value);
  JS::RootedValue strVal(cx, JS::StringValue(inputVal->toString()));
  JS::Value retVal;
  retVal.setObject(*JS::ToObject(cx, strVal));
  return internal::Local<Value>::New(isolate, retVal);
}

StringObject* StringObject::Cast(Value* obj) {
  assert(obj->IsStringObject());
  return static_cast<StringObject*>(obj);
}

Local<String> StringObject::ValueOf() const {
  assert(IsStringObject());
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedValue unboxedVal(cx);
  if (!js::Unbox(cx, thisObj, &unboxedVal)) {
    MOZ_CRASH("Cannot unbox the StringObject value");
  }
  return internal::Local<String>::New(isolate, unboxedVal);
}
}
