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
#include "jsapi.h"
#include "v8context.h"
#include "v8local.h"
#include "v8string.h"

namespace js {
// This needs to be exposed as a JS API from SpiderMonkey.
extern bool FromPropertyDescriptor(JSContext* cx, JS::Handle<JS::PropertyDescriptor> desc, JS::MutableHandleValue vp);
}

namespace v8 {

Maybe<bool> Object::Set(Local<Context> context, Local<Value> key,
                        Local<Value> value, PropertyAttribute attributes,
                        bool force) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *reinterpret_cast<const JS::Value*>(*key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<bool>();
  }
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  JS::RootedValue valueVal(cx, *reinterpret_cast<const JS::Value*>(*value));
  if (!force && attributes == None) {
    return Just(JS_SetPropertyById(cx, thisVal, id, valueVal));
  }
  unsigned attrs = 0;
  if (attributes & ReadOnly) {
    attrs |= JSPROP_READONLY;
  }
  if (!(attributes & DontEnum)) {
    attrs |= JSPROP_ENUMERATE;
  }
  if (attributes & DontDelete) {
    attrs |= JSPROP_PERMANENT;
  }
  return Just(JS_DefinePropertyById(cx, thisVal, id, valueVal, attrs));
}

Maybe<bool> Object::Set(Local<Context> context, Local<Value> key, Local<Value> value) {
  return Set(context, key, value, None, false);
}

bool Object::Set(Handle<Value> key, Handle<Value> value) {
  return Set(Isolate::GetCurrent()->GetCurrentContext(), key, value).
           FromMaybe(false);
}

Maybe<bool> Object::Set(Local<Context> context, uint32_t index, Local<Value> value) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  JS::RootedValue valueVal(cx, *reinterpret_cast<const JS::Value*>(*value));
  return Just(JS_SetElement(cx, thisVal, index, valueVal));
}

bool Object::Set(uint32_t index, Handle<Value> value) {
  return Set(Isolate::GetCurrent()->GetCurrentContext(), index, value).
           FromMaybe(false);
}

Maybe<bool> Object::DefineOwnProperty(Local<Context> context, Local<Name> key,
                                      Local<Value> value,
                                      PropertyAttribute attributes) {
  return Set(context, key, value, attributes, /* force = */ true);
}

Maybe<bool> Object::ForceSet(Local<Context> context,
                             Local<Value> key,
                             Local<Value> value,
                             PropertyAttribute attributes) {
  return Set(context, key, value, attributes, /* force = */ true);
}

bool Object::ForceSet(Handle<Value> key, Handle<Value> value,
                      PropertyAttribute attribites) {
  return Set(Isolate::GetCurrent()->GetCurrentContext(), key, value).
           FromMaybe(false);
}

MaybeLocal<Value> Object::Get(Local<Context> context, Local<Value> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *reinterpret_cast<const JS::Value*>(*key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return MaybeLocal<Value>();
  }
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  JS::RootedValue valueVal(cx);
  if (!JS_GetPropertyById(cx, thisVal, id, &valueVal)) {
    return MaybeLocal<Value>();
  }
  return MaybeLocal<Value>(internal::Local<Value>::New(context->GetIsolate(), valueVal));
}

Local<Value> Object::Get(Handle<Value> key) {
  return Get(Isolate::GetCurrent()->GetCurrentContext(), key).
           FromMaybe(Local<Value>());
}

MaybeLocal<Value> Object::Get(Local<Context> context, uint32_t index) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  JS::RootedValue valueVal(cx);
  if (!JS_GetElement(cx, thisVal, index, &valueVal)) {
    return MaybeLocal<Value>();
  }
  return MaybeLocal<Value>(internal::Local<Value>::New(context->GetIsolate(), valueVal));
}

Local<Value> Object::Get(uint32_t index) {
  return Get(Isolate::GetCurrent()->GetCurrentContext(), index).
           FromMaybe(Local<Value>());
}

Maybe<PropertyAttribute> Object::GetPropertyAttributes(Local<Context> context,
                                                       Local<Value> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *reinterpret_cast<const JS::Value*>(*key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<PropertyAttribute>();
  }
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  JS::Rooted<JS::PropertyDescriptor> desc(cx);
  if (!JS_GetPropertyDescriptorById(cx, thisVal, id, &desc)) {
    return Nothing<PropertyAttribute>();
  }
  uint32_t attributes = None;
  if (!desc.writable()) {
    attributes |= ReadOnly;
  }
  if (!desc.enumerable()) {
    attributes |= DontEnum;
  }
  if (!desc.configurable()) {
    attributes |= DontDelete;
  }
  return Just(static_cast<PropertyAttribute>(attributes));
}

PropertyAttribute Object::GetPropertyAttributes(Local<Value> key) {
  return GetPropertyAttributes(Isolate::GetCurrent()->GetCurrentContext(), key).
           FromMaybe(None);
}

MaybeLocal<Value> Object::GetOwnPropertyDescriptor(Local<Context> context,
                                                   Local<String> key) {
  JSContext* cx = JSContextFromContext(*context);
  auto flatKey = internal::GetFlatString(cx, key);
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  JS::Rooted<JS::PropertyDescriptor> desc(cx);
  if (!JS_GetOwnUCPropertyDescriptor(cx, thisVal, flatKey, &desc)) {
    js_free(flatKey);
    return MaybeLocal<Value>();
  }
  JS::RootedValue result(cx);
  if (!js::FromPropertyDescriptor(cx, desc, &result)) {
    js_free(flatKey);
    return MaybeLocal<Value>();
  }
  js_free(flatKey);
  return MaybeLocal<Value>(internal::Local<Value>::New(context->GetIsolate(), result));
}

Local<Value> Object::GetOwnPropertyDescriptor(Local<String> key) {
  return GetOwnPropertyDescriptor(Isolate::GetCurrent()->GetCurrentContext(), key).
           FromMaybe(Local<Value>());
}

Maybe<bool> Object::Has(Local<Context> context, Local<Value> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *reinterpret_cast<const JS::Value*>(*key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<bool>();
  }
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  bool found = false;
  if (!JS_HasPropertyById(cx, thisVal, id, &found)) {
    return Nothing<bool>();
  }
  return Just(found);
}

bool Object::Has(Local<Value> key) {
  return Has(Isolate::GetCurrent()->GetCurrentContext(), key).
           FromMaybe(false);
}

Maybe<bool> Object::Has(Local<Context> context, uint32_t index) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  bool found = false;
  if (!JS_HasElement(cx, thisVal, index, &found)) {
    return Nothing<bool>();
  }
  return Just(found);
}

bool Object::Has(uint32_t index) {
  return Has(Isolate::GetCurrent()->GetCurrentContext(), index).
           FromMaybe(false);
}

Maybe<bool> Object::Delete(Local<Context> context, Local<Value> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *reinterpret_cast<const JS::Value*>(*key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<bool>();
  }
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  JS::ObjectOpResult result;
  if (!JS_DeletePropertyById(cx, thisVal, id, result)) {
    return Nothing<bool>();
  }
  return Just(result.succeed());
}

bool Object::Delete(Local<Value> key) {
  return Delete(Isolate::GetCurrent()->GetCurrentContext(), key).
           FromMaybe(false);
}

Maybe<bool> Object::Delete(Local<Context> context, uint32_t index) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisVal(cx, &reinterpret_cast<const JS::Value*>(this)->toObject());
  JS::ObjectOpResult result;
  if (!JS_DeleteElement(cx, thisVal, index, result)) {
    return Nothing<bool>();
  }
  return Just(result.succeed());
}

bool Object::Delete(uint32_t index) {
  return Delete(Isolate::GetCurrent()->GetCurrentContext(), index).
           FromMaybe(false);
}

}
