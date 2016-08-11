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
#include "jsfriendapi.h"
#include "conversions.h"
#include "v8local.h"

namespace {

static const JSClassOps classOps = {nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr};
static const JSClass clazz = {"External", JSCLASS_HAS_PRIVATE, &classOps};

bool IsExternal(JSObject* obj) {
  return JS_GetClass(obj) == &clazz;
}

JSObject* CreateExternal(JSContext* cx) { return JS_NewObject(cx, &clazz); }
}

namespace v8 {

Local<External> External::New(Isolate* isolate, void* value) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JSObject* obj = CreateExternal(cx);
  assert(::IsExternal(obj));
  JS_SetPrivate(obj, value);
  JS::Value retVal;
  retVal.setObject(*obj);
  return internal::Local<External>::New(isolate, retVal);
}

Local<Value> External::Wrap(void* data) {
  return New(Isolate::GetCurrent(), data);
}

bool External::IsExternal(const class Value* val) {
  JSObject* obj = GetObject(val);
  return obj ? ::IsExternal(obj) : false;
}

void* External::Unwrap(Handle<class Value> obj) {
  if (obj.IsEmpty() || !IsExternal(*obj)) {
    return nullptr;
  }
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  AutoJSAPI jsAPI(cx, obj);
  return JS_GetPrivate(GetObject(obj));
}

External* External::Cast(class Value* obj) {
  assert(obj && IsExternal(obj));
  return static_cast<External*>(obj);
}

void* External::Value() const {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  AutoJSAPI jsAPI(cx, this);
  return JS_GetPrivate(GetObject(this));
}
}
