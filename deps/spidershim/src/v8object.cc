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
#include "jsfriendapi.h"
#include "v8context.h"
#include "conversions.h"
#include "v8local.h"
#include "v8string.h"
#include "v8trycatch.h"
#include "instanceslots.h"

namespace {

// For now, we implement the hidden values table using a property with a symbol
// key.
// This is observable to script, so if this becomes an issue in the future we'd
// need
// to do something more sophisticated.
JS::Symbol* GetHiddenValuesTableSymbol(JSContext* cx) {
  JS::RootedString name(
      cx, JS_NewStringCopyZ(cx, "__spidershim_hidden_values_table__"));
  return JS::GetSymbolFor(cx, name);
}

JSObject* GetHiddenValuesTable(JSContext* cx, JS::HandleObject self,
                               bool create = false) {
  JS::RootedId id(cx, SYMBOL_TO_JSID(GetHiddenValuesTableSymbol(cx)));
  JS::RootedValue table(cx);
  bool hasOwn = false;
  if (JS_HasOwnPropertyById(cx, self, id, &hasOwn) && hasOwn &&
      JS_GetPropertyById(cx, self, id, &table) && table.isObject()) {
    return &table.toObject();
  }
  if (create) {
    JS::RootedObject tableObj(cx, JS_NewObject(cx, nullptr));
    if (tableObj) {
      JS::RootedValue tableVal(cx);
      tableVal.setObject(*tableObj);
      if (JS_SetPropertyById(cx, self, id, tableVal)) {
        return tableObj;
      }
    }
  }
  return nullptr;
}
}

namespace v8 {

Maybe<bool> Object::Set(Local<Context> context, Local<Value> key,
                        Local<Value> value, PropertyAttribute attributes,
                        bool force) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<bool>();
  }
  JS::RootedObject thisVal(cx, &GetValue(this)->toObject());
  JS::RootedValue valueVal(cx, *GetValue(value));
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

Maybe<bool> Object::Set(Local<Context> context, Local<Value> key,
                        Local<Value> value) {
  return Set(context, key, value, None, false);
}

bool Object::Set(Handle<Value> key, Handle<Value> value) {
  return Set(Isolate::GetCurrent()->GetCurrentContext(), key, value)
      .FromMaybe(false);
}

Maybe<bool> Object::Set(Local<Context> context, uint32_t index,
                        Local<Value> value) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisVal(cx, &GetValue(this)->toObject());
  JS::RootedValue valueVal(cx, *GetValue(value));
  return Just(JS_SetElement(cx, thisVal, index, valueVal));
}

bool Object::Set(uint32_t index, Handle<Value> value) {
  return Set(Isolate::GetCurrent()->GetCurrentContext(), index, value)
      .FromMaybe(false);
}

Maybe<bool> Object::DefineOwnProperty(Local<Context> context, Local<Name> key,
                                      Local<Value> value,
                                      PropertyAttribute attributes) {
  return Set(context, key, value, attributes, /* force = */ true);
}

Maybe<bool> Object::ForceSet(Local<Context> context, Local<Value> key,
                             Local<Value> value, PropertyAttribute attributes) {
  return Set(context, key, value, attributes, /* force = */ true);
}

bool Object::ForceSet(Handle<Value> key, Handle<Value> value,
                      PropertyAttribute attributes) {
  return ForceSet(Isolate::GetCurrent()->GetCurrentContext(), key, value,
                  attributes)
      .FromMaybe(false);
}

MaybeLocal<Value> Object::Get(Local<Context> context, Local<Value> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return MaybeLocal<Value>();
  }
  JS::RootedObject thisVal(cx, GetObject(this));
  JS::RootedValue valueVal(cx);
  if (!JS_GetPropertyById(cx, thisVal, id, &valueVal)) {
    return MaybeLocal<Value>();
  }
  return MaybeLocal<Value>(
      internal::Local<Value>::New(context->GetIsolate(), valueVal));
}

Local<Value> Object::Get(Handle<Value> key) {
  return Get(Isolate::GetCurrent()->GetCurrentContext(), key)
      .FromMaybe(Local<Value>());
}

MaybeLocal<Value> Object::Get(Local<Context> context, uint32_t index) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisVal(cx, GetObject(this));
  JS::RootedValue valueVal(cx);
  if (!JS_GetElement(cx, thisVal, index, &valueVal)) {
    return MaybeLocal<Value>();
  }
  return MaybeLocal<Value>(
      internal::Local<Value>::New(context->GetIsolate(), valueVal));
}

Local<Value> Object::Get(uint32_t index) {
  return Get(Isolate::GetCurrent()->GetCurrentContext(), index)
      .FromMaybe(Local<Value>());
}

Maybe<PropertyAttribute> Object::GetPropertyAttributes(Local<Context> context,
                                                       Local<Value> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<PropertyAttribute>();
  }
  JS::RootedObject thisVal(cx, GetObject(this));
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
  return GetPropertyAttributes(Isolate::GetCurrent()->GetCurrentContext(), key)
      .FromMaybe(None);
}

MaybeLocal<Value> Object::GetOwnPropertyDescriptor(Local<Context> context,
                                                   Local<String> key) {
  JSContext* cx = JSContextFromContext(*context);
  auto flatKey = internal::GetFlatString(cx, key);
  JS::RootedObject thisVal(cx, GetObject(this));
  JS::Rooted<JS::PropertyDescriptor> desc(cx);
  if (!JS_GetOwnUCPropertyDescriptor(cx, thisVal, flatKey.get(), &desc)) {
    return MaybeLocal<Value>();
  }
  JS::RootedValue result(cx);
  if (!JS::FromPropertyDescriptor(cx, desc, &result)) {
    return MaybeLocal<Value>();
  }
  return MaybeLocal<Value>(
      internal::Local<Value>::New(context->GetIsolate(), result));
}

Local<Value> Object::GetOwnPropertyDescriptor(Local<String> key) {
  return GetOwnPropertyDescriptor(Isolate::GetCurrent()->GetCurrentContext(),
                                  key)
      .FromMaybe(Local<Value>());
}

Maybe<bool> Object::Has(Local<Context> context, Local<Value> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<bool>();
  }
  JS::RootedObject thisVal(cx, GetObject(this));
  bool found = false;
  if (!JS_HasPropertyById(cx, thisVal, id, &found)) {
    return Nothing<bool>();
  }
  return Just(found);
}

bool Object::Has(Local<Value> key) {
  return Has(Isolate::GetCurrent()->GetCurrentContext(), key).FromMaybe(false);
}

Maybe<bool> Object::Has(Local<Context> context, uint32_t index) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisVal(cx, GetObject(this));
  bool found = false;
  if (!JS_HasElement(cx, thisVal, index, &found)) {
    return Nothing<bool>();
  }
  return Just(found);
}

bool Object::Has(uint32_t index) {
  return Has(Isolate::GetCurrent()->GetCurrentContext(), index)
      .FromMaybe(false);
}

Maybe<bool> Object::Delete(Local<Context> context, Local<Value> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<bool>();
  }
  JS::RootedObject thisVal(cx, GetObject(this));
  JS::ObjectOpResult result;
  if (!JS_DeletePropertyById(cx, thisVal, id, result)) {
    return Nothing<bool>();
  }
  return Just(result.succeed());
}

bool Object::Delete(Local<Value> key) {
  return Delete(Isolate::GetCurrent()->GetCurrentContext(), key)
      .FromMaybe(false);
}

Maybe<bool> Object::Delete(Local<Context> context, uint32_t index) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisVal(cx, GetObject(this));
  JS::ObjectOpResult result;
  if (!JS_DeleteElement(cx, thisVal, index, result)) {
    return Nothing<bool>();
  }
  return Just(result.succeed());
}

bool Object::Delete(uint32_t index) {
  return Delete(Isolate::GetCurrent()->GetCurrentContext(), index)
      .FromMaybe(false);
}

Isolate* Object::GetIsolate() {
  // TODO: We need to return something useful here when an isolate has not been
  // entered too.
  // Since JSRuntime is a per-thread singleton, consider using a TLS entry.
  return Isolate::GetCurrent();
}

Local<Object> Object::New(Isolate* isolate) {
  if (!isolate) {
    isolate = Isolate::GetCurrent();
  }
  JSContext* cx = JSContextFromIsolate(isolate);
  JSObject* obj = JS_NewObject(cx, nullptr);
  if (!obj) {
    return Local<Object>();
  }
  JS::Value objVal;
  objVal.setObject(*obj);
  return internal::Local<Object>::New(isolate, objVal);
}

Object* Object::Cast(Value* obj) {
  assert(GetValue(obj)->isObject());
  return static_cast<Object*>(obj);
}

Local<Value> Object::GetPrototype() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject prototype(cx);
  JS::Value retVal;
  if (JS_GetPrototype(cx, thisObj, &prototype)) {
    retVal.setObject(*prototype);
  } else {
    // The V8 method is infallible, it's not clear what we should return here.
    // Return undefined for now, but that's probably wrong.
    retVal.setUndefined();
  }
  return internal::Local<Value>::New(isolate, retVal);
}

Maybe<bool> Object::SetPrototype(Local<Context> context,
                                 Local<Value> prototype) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedObject protoObj(cx, GetObject(prototype));
  JS::RootedObject thisObj(cx, GetObject(this));
  return Just(JS_SetPrototype(cx, thisObj, protoObj));
}

bool Object::SetPrototype(Handle<Value> prototype) {
  return SetPrototype(Isolate::GetCurrent()->GetCurrentContext(), prototype)
      .FromMaybe(false);
}

Local<Object> Object::Clone() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject prototype(cx);
  JS::Value cloneVal;
  if (JS_GetPrototype(cx, thisObj, &prototype)) {
    JS::RootedObject clone(cx, JS_CloneObject(cx, thisObj, prototype));
    // TODO: Clone the properties, etc.
    cloneVal.setObject(*clone);
  } else {
    // The V8 method is infallible, it's not clear what we should return here.
    // Return undefined for now, but that's probably wrong.
    cloneVal.setUndefined();
  }
  return internal::Local<Object>::New(isolate, cloneVal);
}

MaybeLocal<Array> Object::GetPropertyNames(Local<Context> context,
                                           unsigned flags) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::AutoIdVector idv(cx);
  if (!js::GetPropertyKeys(cx, thisObj, flags, &idv)) {
    return MaybeLocal<Array>();
  }
  Local<Array> array = Array::New(context->GetIsolate(), idv.length());
  for (size_t i = 0; i < idv.length(); ++i) {
    Local<Value> val;
    if (JSID_IS_STRING(idv[i])) {
      JSString* rawStr = JSID_TO_STRING(idv[i]);
      JS::Value strVal;
      strVal.setString(rawStr);
      val = internal::Local<Value>::New(context->GetIsolate(), strVal);
    } else if (JSID_IS_INT(idv[i])) {
      val = Integer::New(context->GetIsolate(), JSID_TO_INT(idv[i]));
    } else {
      continue;
    }
    if (!array->Set(context, i, val).FromMaybe(false)) {
      return MaybeLocal<Array>();
    }
  }
  return array;
}

MaybeLocal<Array> Object::GetPropertyNames(Local<Context> context) {
  return GetPropertyNames(context, 0);
}

Local<Array> Object::GetPropertyNames() {
  return GetPropertyNames(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Array>());
}

MaybeLocal<Array> Object::GetOwnPropertyNames(Local<Context> context) {
  return GetPropertyNames(context, JSITER_OWNONLY);
}

Local<Array> Object::GetOwnPropertyNames() {
  return GetOwnPropertyNames(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Array>());
}

Maybe<bool> Object::HasPrivate(Local<Context> context, Local<Private> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject table(cx, GetHiddenValuesTable(cx, thisObj));
  if (!table) {
    return Just(false);
  }
  JS::RootedId keyId(cx, SYMBOL_TO_JSID(key->symbol_));
  bool hasPrivate = false;
  if (!JS_HasPropertyById(cx, table, keyId, &hasPrivate)) {
    return Nothing<bool>();
  }
  return Just(hasPrivate);
}

Maybe<bool> Object::SetPrivate(Local<Context> context, Local<Private> key,
                               Local<Value> value) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject table(cx, GetHiddenValuesTable(cx, thisObj, true));
  if (!table) {
    return Just(false);
  }
  JS::RootedId keyId(cx, SYMBOL_TO_JSID(key->symbol_));
  JS::RootedValue valueVal(cx, *GetValue(value));
  if (!JS_SetPropertyById(cx, table, keyId, valueVal)) {
    return Nothing<bool>();
  }
  return Just(true);
}

Maybe<bool> Object::DeletePrivate(Local<Context> context, Local<Private> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject table(cx, GetHiddenValuesTable(cx, thisObj));
  if (!table) {
    return Just(false);
  }
  JS::RootedId keyId(cx, SYMBOL_TO_JSID(key->symbol_));
  JS::ObjectOpResult result;
  if (!JS_DeletePropertyById(cx, table, keyId, result)) {
    return Nothing<bool>();
  }
  return Just(result.succeed());
}

MaybeLocal<Value> Object::GetPrivate(Local<Context> context,
                                     Local<Private> key) {
  JSContext* cx = JSContextFromContext(*context);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject table(cx, GetHiddenValuesTable(cx, thisObj));
  if (!table) {
    return MaybeLocal<Value>();
  }
  JS::RootedId keyId(cx, SYMBOL_TO_JSID(key->symbol_));
  JS::RootedValue result(cx);
  if (!JS_GetPropertyById(cx, table, keyId, &result)) {
    return MaybeLocal<Value>();
  }
  return MaybeLocal<Value>(
      internal::Local<Value>::New(context->GetIsolate(), result));
}

Local<String> Object::GetConstructorName() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject prototype(cx);
  JS::RootedValue constructorVal(cx);
  JS::RootedObject constructor(cx);
  JS::RootedFunction func(cx);
  if (!JS_GetPrototype(cx, thisObj, &prototype) ||
      !JS_GetProperty(cx, prototype, "constructor", &constructorVal) ||
      !constructorVal.isObject() ||
      !(constructor = &constructorVal.toObject()) ||
      !js::IsFunctionObject(constructor) ||
      !(func = JS_GetObjectFunction(constructor))) {
    return Local<String>();
  }
  JSString* displayID = JS_GetFunctionDisplayId(func);
  if (!displayID) {
    displayID = JS_GetEmptyString(JS_GetRuntime(cx));
  }
  JS::Value retVal;
  retVal.setString(displayID);
  return internal::Local<String>::New(isolate, retVal);
}

MaybeLocal<Value> Object::CallAsConstructor(Local<Context> context, int argc,
                                            Local<Value> argv[]) {
  Isolate* isolate = context->GetIsolate();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::AutoValueVector args(cx);
  if (!args.reserve(argc)) {
    return Local<Value>();
  }

  for (int i = 0; i < argc; i++) {
    args.infallibleAppend(*GetValue(argv[i]));
  }

  JS::RootedValue ctor(cx, *GetValue(this));
  JS::RootedObject obj(cx);
  internal::TryCatch tryCatch(isolate);
  if (!JS::Construct(cx, ctor, args, &obj)) {
    tryCatch.CheckReportExternalException();
    return Local<Value>();
  }

  JS::Value val;
  val.setObject(*obj);
  return internal::Local<Value>::New(isolate, val);
}

Local<Value> Object::CallAsConstructor(int argc, Local<Value> argv[]) {
  Local<Context> ctx = Isolate::GetCurrent()->GetCurrentContext();
  return CallAsConstructor(ctx, argc, argv).ToLocalChecked();
}

int Object::InternalFieldCount() {
  return JSCLASS_RESERVED_SLOTS(js::GetObjectClass(GetObject(this))) -
         uint32_t(InstanceSlots::NumSlots);
}

Local<Value> Object::GetInternalField(int index) {
  assert(index < InternalFieldCount());
  JS::Value retVal(js::GetReservedSlot(GetObject(this),
                                       uint32_t(InstanceSlots::NumSlots) + index));
  return internal::Local<Value>::New(Isolate::GetCurrent(), retVal);
}

void Object::SetInternalField(int index, Local<Value> value) {
  assert(index < InternalFieldCount());
  if (!value.IsEmpty()) {
    js::SetReservedSlot(GetObject(this),
                        uint32_t(InstanceSlots::NumSlots) + index,
                        *GetValue(value));
  }
}
}
