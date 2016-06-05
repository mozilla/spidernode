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

#include <assert.h>
#include "jsapi.h"
#include "v8.h"
#include "v8local.h"

namespace v8 {
namespace internal {

enum class AccessorSlots {
  NameSlot,
  DataSlot,
  CallbackSlot1,
  CallbackSlot2,
  NumSlots
};

const JSClass accessorDataClass = {
  "AccessorData",
  JSCLASS_HAS_RESERVED_SLOTS(uint32_t(AccessorSlots::NumSlots))
};

// Our getters/setters need to pass two things along to the underlying
template<typename, typename> struct CallbackTraits;

template<>
struct CallbackTraits<v8::AccessorGetterCallback, v8::String> {
  typedef v8::PropertyCallbackInfo<v8::Value> PropertyCallbackInfo;

  static void doCall(v8::Isolate* isolate, v8::AccessorGetterCallback callback,
                     v8::Local<v8::String> name,
                     PropertyCallbackInfo& info, JS::CallArgs& args) {
    callback(name, info);
  }

  static const unsigned nargs = 0;
};

template<>
struct CallbackTraits<v8::AccessorSetterCallback, v8::String> {
  typedef v8::PropertyCallbackInfo<void> PropertyCallbackInfo;

  static void doCall(v8::Isolate* isolate, v8::AccessorSetterCallback callback,
                     v8::Local<v8::String> name, PropertyCallbackInfo& info,
                     JS::CallArgs& args) {
    using namespace v8;
    v8::Local<Value> value = internal::Local<Value>::New(isolate, args.get(0));

    callback(name, value, info);
  }

  static const unsigned nargs = 0;
};

template<>
struct CallbackTraits<v8::AccessorNameGetterCallback, v8::Name> {
  typedef v8::PropertyCallbackInfo<v8::Value> PropertyCallbackInfo;

  static void doCall(v8::Isolate* isolate, v8::AccessorNameGetterCallback callback,
                     v8::Local<v8::Name> name,
                     PropertyCallbackInfo& info, JS::CallArgs& args) {
    callback(name, info);
  }

  static const unsigned nargs = 0;
};

template<>
struct CallbackTraits<v8::AccessorNameSetterCallback, v8::Name> {
  typedef v8::PropertyCallbackInfo<void> PropertyCallbackInfo;

  static void doCall(v8::Isolate* isolate, v8::AccessorNameSetterCallback callback,
                     v8::Local<v8::Name> name, PropertyCallbackInfo& info,
                     JS::CallArgs& args) {
    using namespace v8;
    v8::Local<Value> value = internal::Local<Value>::New(isolate, args.get(0));

    callback(name, value, info);
  }

  static const unsigned nargs = 0;
};

// AccessorGetterCallback/AccessorSetterCallback are passed information that is
// not really present in SpiderMonkey: a Local<String> for the property name
// (directly) and a Value for the callback data (indirectly, via
// PropertyCallbackInfo).  We store those two things, and the actual address of
// the V8 callback function, in the reserved slots of an object that hangs off
// the accessor.
template<typename CallbackType, typename N>
bool NativeAccessorCallback(JSContext* cx, unsigned argc, JS::Value* vp) {
  using namespace v8;
  Isolate* isolate = Isolate::GetCurrent();
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());

  // TODO: Verify that computeThis() here is the right thing to do!
  v8::Local<Object> thisObject =
    internal::Local<Object>::New(isolate, args.computeThis(cx));

  JS::RootedObject accessorData(cx,
    &js::GetFunctionNativeReserved(callee, 0).toObject());
  v8::Local<Value> data =
    internal::Local<Value>::New(isolate,
      js::GetReservedSlot(accessorData, size_t(AccessorSlots::DataSlot)));
  v8::Local<N> name =
    internal::Local<N>::New(isolate,
      js::GetReservedSlot(accessorData, size_t(AccessorSlots::NameSlot)));

  JS::RootedValue callbackVal1(cx,
    js::GetReservedSlot(accessorData, size_t(AccessorSlots::CallbackSlot1)));
  JS::RootedValue callbackVal2(cx,
    js::GetReservedSlot(accessorData, size_t(AccessorSlots::CallbackSlot2)));

  args.rval().setUndefined();

  auto callback =
    ValuesToCallback<CallbackType>(callbackVal1, callbackVal2);
  if (callback) {
    // TODO: Figure out the story for holder.  See
    // https://groups.google.com/d/msg/v8-users/Axf4hF_RfZo/hA6Mvo78AqAJ (which is
    // kinda messed up, since they have _stopped_ doing the weird holder thing for
    // DOM stuff since then).
    typedef typename
      CallbackTraits<CallbackType, N>::PropertyCallbackInfo PropertyCallbackInfo;
    PropertyCallbackInfo info(data, thisObject, thisObject);

    CallbackTraits<CallbackType, N>::doCall(isolate, callback, name,
                                            info, args);

    if (auto rval = info.GetReturnValue().Get()) {
      args.rval().set(*GetValue(rval));
    }
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

template<typename CallbackType, class N>
JSObject* CreateAccessor(JSContext* cx, CallbackType callback,
                         v8::Handle<N> name,
                         v8::Handle<v8::Value> data) {
  // XXXbz should we pass a better name here?
  JSFunction* func =
    js::NewFunctionWithReserved(cx, NativeAccessorCallback<CallbackType, N>,
                                CallbackTraits<CallbackType, N>::nargs,
                                0, nullptr);
  if (!func) {
    return nullptr;
  }

  JS::RootedObject funObj(cx, JS_GetFunctionObject(func));
  JS::RootedObject accessorData(cx, JS_NewObject(cx, &accessorDataClass));
  if (!accessorData) {
    return nullptr;
  }

  js::SetFunctionNativeReserved(funObj, 0, JS::ObjectValue(*accessorData));

  js::SetReservedSlot(accessorData, size_t(AccessorSlots::NameSlot),
                      *GetValue(name));
  if (!data.IsEmpty()) {
    JS::RootedValue dataVal(cx, *GetValue(data));
    if (JS_WrapValue(cx, &dataVal)) {
      js::SetReservedSlot(accessorData, size_t(AccessorSlots::DataSlot),
                          *GetValue(data));
    }
  }
  JS::RootedValue callbackVal1(cx);
  JS::RootedValue callbackVal2(cx);
  CallbackToValues(callback, &callbackVal1, &callbackVal2);
  js::SetReservedSlot(accessorData, size_t(AccessorSlots::CallbackSlot1),
                      callbackVal1);
  js::SetReservedSlot(accessorData, size_t(AccessorSlots::CallbackSlot2),
                      callbackVal2);

  return funObj;
}

template<class N, class Getter, class Setter>
void SetAccessor(JSContext* cx,
                 JS::HandleObject obj,
                 Handle<N> name,
                 Getter getter,
                 Setter setter,
                 Handle<Value> data,
                 AccessControl settings,
                 PropertyAttribute attribute,
                 Handle<AccessorSignature> signature) {
  // TODO: What should happen with "settings", "attribute", "signature"?  See
  // https://github.com/mozilla/spidernode/issues/141

  JS::RootedObject getterObj(cx, CreateAccessor(cx, getter, name, data));
  if (!getterObj) {
    return;
  }
  JS::RootedObject setterObj(cx);
  if (setter) {
    setterObj = CreateAccessor(cx, setter, name, data);
    if (!setterObj) {
      return;
    }
  }

  JS::RootedId id(cx);
  JS::RootedValue nameVal(cx, *GetValue(name));
  if (!JS_WrapValue(cx, &nameVal) ||
      !JS_ValueToId(cx, nameVal, &id)) {
    return;
  }

  if (!JS_DefinePropertyById(cx, obj, id, JS::UndefinedHandleValue,
                             JSPROP_ENUMERATE | JSPROP_SHARED |
                             JSPROP_GETTER | JSPROP_SETTER,
                             JS_DATA_TO_FUNC_PTR(JSNative, getterObj.get()),
                             JS_DATA_TO_FUNC_PTR(JSNative, setterObj.get()))) {
    return;
  }
}

}
}
