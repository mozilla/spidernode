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
#include "conversions.h"
#include "v8local.h"
#include "autojsapi.h"
#include "jsfriendapi.h"

namespace v8 {

size_t TypedArray::Length() {
  AutoJSAPI jsAPI(this);
  JSObject* view = GetObject(this);
  assert(IsTypedArray());
  return JS_GetTypedArrayLength(view);
}

TypedArray* TypedArray::Cast(Value* obj) {
  assert(obj->IsTypedArray());
  return static_cast<TypedArray*>(obj);
}

#define ES_BUILTIN(X, Y)
#define COMMON_VALUE(X)
#define TYPED_ARRAY(TYPE)                                                     \
  Local<TYPE##Array> TYPE##Array::New(Handle<ArrayBuffer> buffer,             \
                                      size_t offset, size_t length) {         \
    Isolate* isolate = buffer->GetIsolate();                                  \
    JSContext* cx = JSContextFromIsolate(isolate);                            \
    AutoJSAPI jsAPI(cx);                                                      \
    JS::RootedObject buf(cx, GetObject(buffer));                              \
    JSObject* array = JS_New##TYPE##ArrayWithBuffer(cx, buf, offset, length); \
    if (!array) {                                                             \
      return Local<TYPE##Array>();                                            \
    }                                                                         \
    JS::Value arrayVal;                                                       \
    arrayVal.setObject(*array);                                               \
    return internal::Local<TYPE##Array>::New(isolate, arrayVal);              \
  }                                                                           \
                                                                              \
  TYPE##Array* TYPE##Array::Cast(Value* v) {                                  \
    assert(v->Is##TYPE##Array());                                             \
    return static_cast<TYPE##Array*>(v);                                      \
  }

#include "valuemap.inc"
#undef TYPED_ARRAY
#undef ES_BUILTIN
#undef COMMON_VALUE
}
