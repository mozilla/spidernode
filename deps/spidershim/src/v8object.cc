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
#include "js/Proxy.h"
#include "v8context.h"
#include "conversions.h"
#include "v8local.h"
#include "v8string.h"
#include "v8trycatch.h"
#include "instanceslots.h"
#include "accessor.h"
#include "utils.h"

namespace {

using namespace v8;

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

// These classes have twice the number of required internal slots in order to
// keep the internal slot structure compatible with objects created through
// ObjectTemplate.
JSClass ArrayBufferInternalFieldsClass = {
  "AB_InternalFields", JSCLASS_HAS_RESERVED_SLOTS(uint32_t(InstanceSlots::NumSlots) + ArrayBuffer::kInternalFieldCount * 2)
};

JSClass ArrayBufferViewInternalFieldsClass = {
  "ABV_InternalFields", JSCLASS_HAS_RESERVED_SLOTS(uint32_t(InstanceSlots::NumSlots) + ArrayBufferView::kInternalFieldCount * 2)
};

JSObject* GetHiddenTable(JSContext* cx, JS::HandleObject self,
                         bool create, JS::Symbol* name) {
  JS::RootedId id(cx, SYMBOL_TO_JSID(name));
  JS::RootedValue table(cx);
  bool hasOwn = false;
  if (JS_HasOwnPropertyById(cx, self, id, &hasOwn) && hasOwn &&
      JS_GetPropertyById(cx, self, id, &table) && table.isObject()) {
    return &table.toObject();
  }
  if (create) {
    JSClass* clazz = nullptr;
    if (JS_IsArrayBufferObject(self)) {
      clazz = &ArrayBufferInternalFieldsClass;
    } else if (JS_IsArrayBufferViewObject(self)) {
      clazz = &ArrayBufferViewInternalFieldsClass;
    }
    JS::RootedObject tableObj(cx, JS_NewObject(cx, clazz));
    if (tableObj) {
      int numInternalFields = 0;
      if (JS_IsArrayBufferObject(self)) {
        numInternalFields = ArrayBuffer::kInternalFieldCount;
      } else if (JS_IsArrayBufferViewObject(self)) {
        numInternalFields = ArrayBufferView::kInternalFieldCount;
      }
      for (int i = 0; i < numInternalFields; ++i) {
        SetInstanceSlot(tableObj,
                        uint32_t(InstanceSlots::NumSlots) + i * 2,
                        JS::Int32Value(0));
      }
      JS::RootedValue tableVal(cx);
      tableVal.setObject(*tableObj);
      if (JS_SetPropertyById(cx, self, id, tableVal)) {
        return tableObj;
      }
    }
  }
  return nullptr;
}

JSObject* GetHiddenValuesTable(JSContext* cx, JS::HandleObject self,
                               bool create = false) {
  return GetHiddenTable(cx, self, create, GetHiddenValuesTableSymbol(cx));
}

// For now, we implement the in hidden values table using a property with a
// symbol key. This is observable to script, so if this becomes an issue in the
// future we'd need to do something more sophisticated.
JS::Symbol* GetHiddenInternalFieldsSymbol(JSContext* cx) {
  JS::RootedString name(
      cx, JS_NewStringCopyZ(cx, "__spidershim_hidden_internal_fields__"));
  return JS::GetSymbolFor(cx, name);
}

// In V8, ArrayBuffers and ArrayBufferViews always have two internal fields
// which the engine magically creates internally.  In SpiderMonkey, the
// reserved slots fields are used for inline array storage and are not
// available to the embedder.  Therefore, we offload the internal fields
// of such objects to a hidden internal fields object that can be obtained
// through this function.
//
// Note that the hidden internal fields are lazily created when needed.
// See the code in GetHiddenTable for the special casing of these types.
Local<Object> GetHiddenInternalFields(Local<Object> self) {
  assert(JS_IsArrayBufferObject(GetObject(self)) ||
         JS_IsArrayBufferViewObject(GetObject(self)));
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  JS::RootedObject selfObj(cx, GetObject(self));
  JS::RootedObject obj(cx, GetHiddenTable(cx, selfObj, true, GetHiddenInternalFieldsSymbol(cx)));
  return internal::Local<Object>::New(Isolate::GetCurrent(), JS::ObjectValue(*obj));
}
}

namespace v8 {

Maybe<bool> Object::Set(Local<Context> context, Local<Value> key,
                        Local<Value> value, PropertyAttribute attributes,
                        bool force) {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx, this);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_WrapValue(cx, &keyVal) ||
      !JS_ValueToId(cx, keyVal, &id)) {
    return Nothing<bool>();
  }
  JSObject* thisObj = GetObject(this);
  JSAutoCompartment ac(cx, thisObj);
  JS::RootedObject thisVal(cx, thisObj);
  JS::RootedValue valueVal(cx, *GetValue(value));
  if (!JS_WrapValue(cx, &valueVal)) {
    return Nothing<bool>();
  }
  if (!force && attributes == None) {
    return Just(JS_SetPropertyById(cx, thisVal, id, valueVal));
  }
  unsigned attrs = internal::AttrsToFlags(attributes);
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
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject thisVal(cx, &GetValue(this)->toObject());
  JS::RootedValue valueVal(cx, *GetValue(value));
  if (!JS_WrapValue(cx, &valueVal)) {
    return Nothing<bool>();
  }
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

Maybe<bool> Object::DefineProperty(Local<Context> context, Local<Name> key,
                                   PropertyDescriptor& descriptor) {
  uint32_t attributes = None;
  if (!descriptor.writable()) {
    attributes |= ReadOnly;
  }
  if (!descriptor.enumerable()) {
    attributes |= DontEnum;
  }
  if (!descriptor.configurable()) {
    attributes |= DontDelete;
  }
  return Set(context, key, descriptor.value(), static_cast<PropertyAttribute>(attributes), /* force = */ true);
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
  AutoJSAPI jsAPI(cx, this);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_WrapValue(cx, &keyVal) ||
      !JS_ValueToId(cx, keyVal, &id)) {
    return MaybeLocal<Value>();
  }
  JSObject* thisObj = GetObject(this);
  JSAutoCompartment ac(cx, thisObj);
  JS::RootedObject thisVal(cx, thisObj);
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
  AutoJSAPI jsAPI(cx, this);
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
  AutoJSAPI jsAPI(cx, this);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_WrapValue(cx, &keyVal) ||
      !JS_ValueToId(cx, keyVal, &id)) {
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
  AutoJSAPI jsAPI(cx, this);
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
  AutoJSAPI jsAPI(cx, this);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_WrapValue(cx, &keyVal) ||
      !JS_ValueToId(cx, keyVal, &id)) {
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
  AutoJSAPI jsAPI(cx, this);
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
  AutoJSAPI jsAPI(cx, this);
  JS::Rooted<jsid> id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_WrapValue(cx, &keyVal) ||
      !JS_ValueToId(cx, keyVal, &id)) {
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
  AutoJSAPI jsAPI(cx, this);
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
  AutoJSAPI jsAPI(cx);
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
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject prototype(cx);
  JS::Value retVal;
  if (JS_GetPrototype(cx, thisObj, &prototype)) {
    if (prototype) {
      retVal.setObject(*prototype);
    } else {
      retVal.setNull();
    }
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
  AutoJSAPI jsAPI(cx, this);
  JS::RootedValue protoVal(cx, *GetValue(prototype));
  if (!JS_WrapValue(cx, &protoVal)) {
    return Nothing<bool>();
  }
  JS::RootedObject protoObj(cx);
  if (protoVal.isObject()) {
    protoObj = &protoVal.toObject();
  }
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
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject thisObj(cx, GetObject(this));
  bool isArray;
  if (!JS_IsArrayObject(cx, thisObj, &isArray)) {
    return Local<Object>();
  }

  if (isArray) {
    uint32_t length;
    if (!JS_GetArrayLength(cx, thisObj, &length)) {
      return Local<Object>();
    }

    JS::RootedObject copy(cx, JS_NewArrayObject(cx, length));
    if (!copy) {
      return Local<Object>();
    }

    JS::RootedValue val(cx);
    for (uint32_t i = 0; i < length; i++) {
      if (!JS_GetElement(cx, thisObj, i, &val)) {
        return Local<Object>();
      }

      if (!JS_DefineElement(cx, copy, i, val, JSPROP_ENUMERATE)) {
        return Local<Object>();
      }
    }

    JS::Value copyVal;
    copyVal.setObject(*copy);
    return internal::Local<Object>::New(isolate, copyVal);
  }

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
  AutoJSAPI jsAPI(cx, this);
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

Maybe<bool> Object::HasOwnProperty(Local<Context> context, Local<Name> key) {
  if (key.IsEmpty()) {
    return Nothing<bool>();
  }
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedId id(cx);
  JS::RootedValue keyVal(cx, *GetValue(key));
  if (!JS_WrapValue(cx, &keyVal)) {
    return Nothing<bool>();
  }
  assert(keyVal.isString() || keyVal.isSymbol());
  if (keyVal.isString()) {
    JS::RootedString str(cx, keyVal.toString());
    JS::RootedString interned(cx, JS_AtomizeAndPinJSString(cx, str));
    if (!interned) {
      return Nothing<bool>();
    }
    id = INTERNED_STRING_TO_JSID(cx, interned);
  } else if (keyVal.isSymbol()) {
    id = SYMBOL_TO_JSID(keyVal.toSymbol());
  } else {
    return Nothing<bool>();
  }
  bool hasOwn = false;
  JS::RootedObject thisObj(cx, GetObject(this));
  if (!JS_HasOwnPropertyById(cx, thisObj, id, &hasOwn)) {
    return Nothing<bool>();
  }
  return Just(hasOwn);
}

bool Object::HasOwnProperty(Handle<String> key) {
  return HasOwnProperty(Isolate::GetCurrent()->GetCurrentContext(), key)
      .FromMaybe(false);
}

Maybe<bool> Object::HasPrivate(Local<Context> context, Local<Private> key) {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx, this);
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
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject table(cx, GetHiddenValuesTable(cx, thisObj, true));
  if (!table) {
    return Just(false);
  }
  JS::RootedId keyId(cx, SYMBOL_TO_JSID(key->symbol_));
  JS::RootedValue valueVal(cx, *GetValue(value));
  if (!JS_WrapValue(cx, &valueVal) ||
      !JS_SetPropertyById(cx, table, keyId, valueVal)) {
    return Nothing<bool>();
  }
  return Just(true);
}

Maybe<bool> Object::DeletePrivate(Local<Context> context, Local<Private> key) {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx, this);
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
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedObject table(cx, GetHiddenValuesTable(cx, thisObj));
  if (!table) {
    return MaybeLocal<Value>(Undefined(context->GetIsolate()));
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
  AutoJSAPI jsAPI(cx, this);
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
    displayID = JS_GetEmptyString(cx);
  }
  JS::Value retVal;
  retVal.setString(displayID);
  return internal::Local<String>::New(isolate, retVal);
}

MaybeLocal<Value> Object::CallAsConstructor(Local<Context> context, int argc,
                                            Local<Value> argv[]) {
  Isolate* isolate = context->GetIsolate();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  jsAPI.MarkScriptCall();
  JS::AutoValueVector args(cx);
  if (!args.reserve(argc)) {
    return Local<Value>();
  }

  for (int i = 0; i < argc; i++) {
    JS::RootedValue val(cx, *GetValue(argv[i]));
    if (!JS_WrapValue(cx, &val)) {
      return Local<Value>();
    }
    args.infallibleAppend(val);
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

MaybeLocal<Value> Object::CallAsFunction(Local<Context> context,
                                         Local<Value> recv,
                                         int argc,
                                         Handle<Value> argv[]) {
  if (!IsFunction()) {
    return Local<Value>();
  }
  Local<Object> thisObj =
    internal::Local<Object>::New(context->GetIsolate(),
                                 JS::ObjectValue(*GetObject(this)));
  if (ObjectTemplate::ObjectHasNativeCallHandler(thisObj)) {
    // If the this function has a native callback, the receiver is forced
    // to be the this object.
    recv = thisObj;
  }
  return Function::Cast(this)->Call(context, recv, argc, argv);
}

Local<Value> Object::CallAsFunction(Local<Value> recv, int argc,
                                    Local<Value> argv[]) {
  Local<Context> ctx = Isolate::GetCurrent()->GetCurrentContext();
  return CallAsFunction(ctx, recv, argc, argv).ToLocalChecked();
}

int Object::InternalFieldCount() {
  Local<Object> thisObj = UnwrapProxyIfNeeded(this);
  if (IsArrayBuffer() || IsArrayBufferView()) {
    return GetHiddenInternalFields(thisObj)->InternalFieldCount();
  }
  uint32_t globalClassAdjustment = 0;
  if (JS_IsGlobalObject(GetObject(thisObj))) {
    globalClassAdjustment = JSCLASS_GLOBAL_SLOT_COUNT -
                            JSCLASS_GLOBAL_APPLICATION_SLOTS;
  }
  return (JSCLASS_RESERVED_SLOTS(js::GetObjectClass(GetObject(thisObj))) -
          uint32_t(InstanceSlots::NumSlots) - globalClassAdjustment) / 2;
}

Local<Value> Object::GetInternalField(int index) {
  Local<Object> thisObj = UnwrapProxyIfNeeded(this);
  if (IsArrayBuffer() || IsArrayBufferView()) {
    return GetHiddenInternalFields(thisObj)->GetInternalField(index);
  }
  assert(index < InternalFieldCount());
  JS::Value retVal(GetInstanceSlot(GetObject(thisObj),
                                   uint32_t(InstanceSlots::NumSlots) + index * 2));
  return internal::Local<Value>::New(Isolate::GetCurrent(), retVal);
}

void Object::SetInternalField(int index, Local<Value> value) {
  Local<Object> thisObj = UnwrapProxyIfNeeded(this);
  if (IsArrayBuffer() || IsArrayBufferView()) {
    return GetHiddenInternalFields(thisObj)->SetInternalField(index, value);
  }
  assert(index < InternalFieldCount());
  if (!value.IsEmpty()) {
    SetInstanceSlot(GetObject(thisObj),
                    uint32_t(InstanceSlots::NumSlots) + index * 2,
                    *GetValue(value));
  }
}

void* Object::GetAlignedPointerFromInternalField(int index) {
  Local<Object> thisObj = UnwrapProxyIfNeeded(this);
  if (IsArrayBuffer() || IsArrayBufferView()) {
    return GetHiddenInternalFields(thisObj)->GetAlignedPointerFromInternalField(index);
  }
  assert(index < InternalFieldCount());
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedValue ptr1(cx,
                       GetInstanceSlot(GetObject(thisObj),
                                       uint32_t(InstanceSlots::NumSlots) + index * 2));
  JS::RootedValue ptr2(cx,
                       GetInstanceSlot(GetObject(thisObj),
                                       uint32_t(InstanceSlots::NumSlots) + index * 2 + 1));
  return ValuesToCallback<void*>(ptr1, ptr2);
}

void Object::SetAlignedPointerInInternalField(int index, void* value) {
  Local<Object> thisObj = UnwrapProxyIfNeeded(this);
  if (IsArrayBuffer() || IsArrayBufferView()) {
    return GetHiddenInternalFields(thisObj)->SetAlignedPointerInInternalField(index, value);
  }
  assert(index < InternalFieldCount());
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedValue ptr1(cx), ptr2(cx);
  CallbackToValues(value, &ptr1, &ptr2);
  SetInstanceSlot(GetObject(thisObj),
                  uint32_t(InstanceSlots::NumSlots) + index * 2,
                  ptr1);
  SetInstanceSlot(GetObject(thisObj),
                  uint32_t(InstanceSlots::NumSlots) + index * 2 + 1,
                  ptr2);
}

Local<Context> Object::CreationContext() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JSObject* thisObj = UnwrapProxyIfNeeded(GetObject(this));
  AutoJSAPI jsAPI(cx, thisObj);
  if (JS_ObjectIsFunction(cx, thisObj)) {
    JS::RootedValue thisVal(cx, JS::ObjectValue(*thisObj));
    JSFunction* fun = JS_ValueToFunction(cx, thisVal);
    assert(fun);
    if (JS_IsFunctionBound(fun)) {
      JS::Value targetVal;
      targetVal.setObject(*JS_GetBoundFunctionTarget(fun));
      Local<Object> target = internal::Local<Object>::New(isolate, targetVal);
      return target->CreationContext();
    }
  }
  JSObject* global = JS_GetGlobalForObject(cx, thisObj);
  if (!global) {
    return Local<Context>();
  }
  auto slot = GetInstanceSlot(global, uint32_t(InstanceSlots::ContextSlot));
  if (slot.isUndefined()) {
    return Local<Context>();
  }
  auto context = static_cast<Context*>(slot.toPrivate());
  return Local<Context>::New(isolate, context);
}

bool Object::SetAccessor(Handle<String> name,
                         AccessorGetterCallback getter,
                         AccessorSetterCallback setter,
                         Handle<Value> data,
                         AccessControl settings,
                         PropertyAttribute attribute) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  return SetAccessorInternal(cx, name, getter, setter, data,
                             settings, attribute).
           FromMaybe(false);
}

bool Object::SetAccessor(Handle<Name> name,
                         AccessorNameGetterCallback getter,
                         AccessorNameSetterCallback setter,
                         Handle<Value> data,
                         AccessControl settings,
                         PropertyAttribute attribute) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  return SetAccessorInternal(cx, name, getter, setter, data,
                             settings, attribute).
           FromMaybe(false);
}

Maybe<bool> Object::SetAccessor(Local<Context> context,
                                Local<Name> name,
                                AccessorNameGetterCallback getter,
                                AccessorNameSetterCallback setter,
                                MaybeLocal<Value> data,
                                AccessControl settings,
                                PropertyAttribute attribute) {
  JSContext* cx = JSContextFromContext(*context);
  return SetAccessorInternal(cx, name, getter, setter, data.FromMaybe(Local<Value>()),
                             settings, attribute);
}

template <class N, class Getter, class Setter>
Maybe<bool> Object::SetAccessorInternal(JSContext* cx,
                                        Handle<N> name,
                                        Getter getter,
                                        Setter setter,
                                        Handle<Value> data,
                                        AccessControl settings,
                                        PropertyAttribute attribute) {
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));

  if (!internal::SetAccessor(cx, obj, name, getter, setter, data,
                             settings, attribute)) {
    return Nothing<bool>();
  }
  return Just(true);
}

void Object::SetAccessorProperty(Local<Name> name, Local<Function> getter,
                                 Local<Function> setter, PropertyAttribute attribute,
                                 AccessControl settings) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));

  JS::RootedObject getterObj(cx, GetObject(getter));
  JS::RootedObject setterObj(cx);
  if (!setter.IsEmpty()) {
    setterObj = GetObject(setter);
  }
  internal::SetAccessor(cx, obj, name, getterObj, setterObj,
                        settings, attribute);
}

MaybeLocal<Value> Object::GetRealNamedProperty(Local<Context> context,
                                               Local<Name> key) {
  // We don't support hidden prototypes...
  return Get(context, key);
}

Local<Value> Object::GetRealNamedProperty(Local<String> key) {
  return GetRealNamedProperty(Isolate::GetCurrent()->GetCurrentContext(), key)
      .FromMaybe(Local<Value>());
}

Maybe<PropertyAttribute> Object::GetRealNamedPropertyAttributes(Local<Context> context,
                                                                Local<Name> key) {
  // We don't support hidden prototypes...
  return GetPropertyAttributes(context, key);
}

Maybe<PropertyAttribute> Object::GetRealNamedPropertyAttributes(Local<String> key) {
  return GetRealNamedPropertyAttributes(Isolate::GetCurrent()->GetCurrentContext(), key);
}

Maybe<bool> Object::HasRealNamedProperty(Local<Context> context,
                                         Local<Name> key) {
  return Has(context, key);
}

bool Object::HasRealNamedProperty(Handle<String> key) {
  return HasRealNamedProperty(Local<Context>(), key).FromMaybe(false);
}
}
