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
#include "autojsapi.h"
#include "v8.h"
#include "v8local.h"

namespace {

const char* kIllegalInvocation = "Illegal invocation";
}

namespace v8 {
namespace internal {

inline unsigned AttrsToFlags(PropertyAttribute attributes) {
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
  return attrs;
}

enum class AccessorSlots {
  NameSlot,
  DataSlot,
  CallbackSlot1,
  CallbackSlot2,
  SignatureSlot,
  NumSlots
};

const JSClass accessorDataClass = {
  "AccessorData",
  JSCLASS_HAS_RESERVED_SLOTS(uint32_t(AccessorSlots::NumSlots))
};

template<typename> struct XerTraits;
template<typename, typename> struct CallbackTraits;

template<>
struct XerTraits<v8::Name> {
  typedef v8::AccessorNameGetterCallback Getter;
  typedef v8::AccessorNameSetterCallback Setter;
};

template<>
struct XerTraits<v8::String> {
  typedef v8::AccessorGetterCallback Getter;
  typedef v8::AccessorSetterCallback Setter;
};

template<typename N>
struct CallbackTraits<typename XerTraits<N>::Getter, N> {
  typedef v8::PropertyCallbackInfo<v8::Value> PropertyCallbackInfo;

  static void doCall(v8::Isolate* isolate,
                     typename XerTraits<N>::Getter callback,
                     v8::Local<N> name,
                     PropertyCallbackInfo& info, JS::CallArgs& args) {
    callback(name, info);
  }

  static const unsigned nargs = 0;
};

template<typename N>
struct CallbackTraits<typename XerTraits<N>::Setter, N> {
  typedef v8::PropertyCallbackInfo<void> PropertyCallbackInfo;

  static void doCall(v8::Isolate* isolate,
                     typename XerTraits<N>::Setter callback,
                     v8::Local<N> name,
                     PropertyCallbackInfo& info,
                     JS::CallArgs& args) {
    v8::Local<Value> value = internal::Local<Value>::New(isolate, args.get(0));

    callback(name, value, info);
  }

  static const unsigned nargs = 0;
};

struct SignatureChecker {
  // Performs a signature check if needed.  Returns true and a holder object
  // unless the signature check fails.  thisObj should be set to the this
  // object that the function is being called on.
  static bool CheckSignature(JS::HandleObject accessorData,
                             v8::Local<Object> thisObj,
                             v8::Local<Object>& holder) {
    Isolate* isolate = Isolate::GetCurrent();
    JSContext* cx = JSContextFromIsolate(isolate);
    AutoJSAPI jsAPI(cx, GetObject(thisObj));
    assert(JS_GetClass(accessorData) == &accessorDataClass);
    JS::Value signatureVal = js::GetReservedSlot(accessorData,
                                                 size_t(AccessorSlots::SignatureSlot));
    if (!signatureVal.isUndefined()) {
      v8::Local<FunctionTemplate> signatureAsTemplate =
        internal::Local<FunctionTemplate>::NewTemplate(isolate, signatureVal);
      if (!signatureAsTemplate->IsInstance(thisObj)) {
        return false;
      }
    }
    holder = thisObj;
    return true;
  }
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
  HandleScope handleScope(isolate); // Make sure there is _a_ handlescope on the stack!
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  JS::Value calleeVal;
  calleeVal.setObject(args.callee());
  v8::Local<Object> calleeObject =
    internal::Local<Object>::New(isolate, calleeVal);

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
    v8::Local<Object> holder;
    if (!SignatureChecker::CheckSignature(accessorData, thisObject, holder)) {
      isolate->ThrowException(
          Exception::TypeError(String::NewFromUtf8(isolate,
                                                   kIllegalInvocation)));
      return false;
    }
    typedef typename
      CallbackTraits<CallbackType, N>::PropertyCallbackInfo PropertyCallbackInfo;
    PropertyCallbackInfo info(data, thisObject, holder);

    {
      // Enter the context of the callee if one is available.
      v8::Local<Context> context = calleeObject->CreationContext();
      mozilla::Maybe<Context::Scope> scope;
      if (!context.IsEmpty()) {
        scope.emplace(context);
      }
      CallbackTraits<CallbackType, N>::doCall(isolate, callback, name,
                                              info, args);
    }

    if (auto rval = info.GetReturnValue().Get()) {
      args.rval().set(*GetValue(rval));
    }
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

template<typename CallbackType, class N>
JSObject* CreateAccessor(JSContext* cx, CallbackType callback,
                         v8::Handle<N> name,
                         v8::Handle<v8::Value> data,
                         v8::Handle<v8::AccessorSignature> signature) {
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

  JS::Value signatureVal;
  signatureVal.setUndefined();
  if (!signature.IsEmpty()) {
    signatureVal = *GetValue(signature);
  }
  js::SetReservedSlot(accessorData, size_t(AccessorSlots::SignatureSlot),
                      signatureVal);

  return funObj;
}

template<class N>
bool SetAccessor(JSContext* cx,
                 JS::HandleObject obj,
                 Handle<N> name,
                 JS::HandleObject getter,
                 JS::HandleObject setter,
                 AccessControl settings,
                 PropertyAttribute attribute) {
  JS::RootedId id(cx);
  JS::RootedValue nameVal(cx, *GetValue(name));
  if (!JS_WrapValue(cx, &nameVal) ||
      !JS_ValueToId(cx, nameVal, &id)) {
    return false;
  }

  unsigned attrs = internal::AttrsToFlags(attribute) |
                   JSPROP_SHARED | JSPROP_GETTER;
  if (setter) {
    attrs |= JSPROP_SETTER;
  }

  if (!JS_DefinePropertyById(cx, obj, id, JS::UndefinedHandleValue, attrs,
                             JS_DATA_TO_FUNC_PTR(JSNative, getter.get()),
                             JS_DATA_TO_FUNC_PTR(JSNative, setter.get()))) {
    return false;
  }

  return true;
}

template<class N, class Getter, class Setter>
bool SetAccessor(JSContext* cx,
                 JS::HandleObject obj,
                 Handle<N> name,
                 Getter getter,
                 Setter setter,
                 Handle<Value> data,
                 AccessControl settings,
                 PropertyAttribute attribute,
                 Handle<AccessorSignature> signature =
                   Handle<AccessorSignature>()) {
  // TODO: What should happen with "settings"?  See
  // https://github.com/mozilla/spidernode/issues/141

  JS::RootedObject getterObj(cx, CreateAccessor(cx, getter, name, data, signature));
  if (!getterObj) {
    return false;
  }
  JS::RootedObject setterObj(cx);
  if (setter) {
    setterObj = CreateAccessor(cx, setter, name, data, signature);
    if (!setterObj) {
      return false;
    }
  }

  return SetAccessor(cx, obj, name, getterObj, setterObj, settings,
                     attribute);
}

}
}
