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
#include "conversions.h"
#include "v8local.h"
#include "autojsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"

namespace {
using namespace v8;

// For now, we implement the hidden creation mode using a property with a
// symbol key. This is observable to script, so if this becomes an issue in the
// future we'd need to do something more sophisticated.
JS::Symbol* GetHiddenCreationModeSymbol(JSContext* cx) {
  JS::RootedString name(
      cx, JS_NewStringCopyZ(cx, "__spidershim_hidden_creation_mode__"));
  return JS::GetSymbolFor(cx, name);
}

ArrayBufferCreationMode GetCreationMode(JSContext* cx, JS::HandleObject self) {
  JS::RootedId id(cx, SYMBOL_TO_JSID(GetHiddenCreationModeSymbol(cx)));
  JS::RootedValue value(cx);
  if (!JS_GetPropertyById(cx, self, id, &value) ||
      value.isUndefined()) {
    return ArrayBufferCreationMode::kInternalized;
  }
  assert(value.isInt32());
  assert(value.toInt32() == int32_t(ArrayBufferCreationMode::kInternalized) ||
         value.toInt32() == int32_t(ArrayBufferCreationMode::kExternalized));
  return ArrayBufferCreationMode(value.toInt32());
}

bool SetCreationMode(JSContext* cx, JS::HandleObject self,
                     ArrayBufferCreationMode mode) {
  JS::RootedId id(cx, SYMBOL_TO_JSID(GetHiddenCreationModeSymbol(cx)));
  JS::RootedValue value(cx);
  value.setInt32(int32_t(mode));
  return JS_SetPropertyById(cx, self, id, value);
}
}

namespace v8 {

ArrayBuffer* ArrayBuffer::Cast(Value* val) {
  assert(val->IsArrayBuffer());
  return static_cast<ArrayBuffer*>(val);
}

Local<ArrayBuffer> ArrayBuffer::New(Isolate* isolate, size_t size) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::RootedObject buf(cx, JS_NewArrayBuffer(cx, size));
  if (!SetCreationMode(cx, buf, ArrayBufferCreationMode::kInternalized)) {
    return Local<ArrayBuffer>();
  }
  JS::Value val;
  val.setObject(*buf);
  return internal::Local<ArrayBuffer>::New(isolate, val);
}

Local<ArrayBuffer> ArrayBuffer::New(Isolate* isolate, void* data, size_t size,
                                    ArrayBufferCreationMode mode) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::RootedObject buf(cx);
  if (mode == ArrayBufferCreationMode::kExternalized) {
    buf = JS_NewArrayBufferWithExternalContents(cx, size, data);
  } else {
    buf = JS_NewArrayBufferWithContents(cx, size, data);
  }
  if (!SetCreationMode(cx, buf, mode)) {
    return Local<ArrayBuffer>();
  }
  JS::Value val;
  val.setObject(*buf);
  return internal::Local<ArrayBuffer>::New(isolate, val);
}

bool ArrayBuffer::IsExternal() const {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JS::RootedObject obj(cx, GetObject(this));
  AutoJSAPI jsAPI(cx, obj);
  return GetCreationMode(cx, obj) == ArrayBufferCreationMode::kExternalized;
}

size_t ArrayBuffer::ByteLength() const {
  const JS::Value* val = GetValue(this);
  JSObject& obj = val->toObject();
  AutoJSAPI jsAPI(&obj);
  return JS_GetArrayBufferByteLength(&obj);
}

ArrayBuffer::Contents ArrayBuffer::GetContents() {
  const JS::Value thisVal = *GetValue(this);
  JSObject* obj = &thisVal.toObject();
  AutoJSAPI jsAPI(obj);
  uint8_t* data;
  bool shared;
  uint32_t length;
  js::GetArrayBufferLengthAndData(obj, &length, &shared, &data);
  Contents contents;
  contents.data_ = static_cast<void*>(data);
  contents.byte_length_ = length;
  return contents;
}

void ArrayBuffer::Neuter() {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JS::RootedObject obj(cx, GetObject(this));
  AutoJSAPI jsAPI(cx, obj);
  JS_DetachArrayBuffer(cx, obj);
}

auto ArrayBuffer::Externalize() -> Contents {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  JS::RootedObject obj(cx, GetObject(this));
  AutoJSAPI jsAPI(cx, obj);
  Contents result;
  result.byte_length_ = ByteLength();
  result.data_ = JS_ExternalizeArrayBufferContents(cx, obj);
  SetCreationMode(cx, obj, ArrayBufferCreationMode::kExternalized);
  return result;
}
}
