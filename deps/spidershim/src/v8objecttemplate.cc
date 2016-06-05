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

#include "conversions.h"
#include "v8local.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "instanceslots.h"

namespace v8 {

/**
 * We sometimes need custom JSClasses, and we need to dynamically generate
 * them.  That means we want to keep them alive as long as any objects using
 * them are alive.  We implement this by having the relevant objects refcount
 * the class and drop the ref in the finalizer.
 */
class ObjectTemplate::InstanceClass : public js::Class {
public:
  void AddRef() {
    ++mRefCnt;
  }

  void Release() {
    --mRefCnt;
    if (mRefCnt == 0) {
      delete this;
    }
  }

private:
  ~InstanceClass() {
    // We heap-allocated our name, so need to free it now.
    JS_free(nullptr, const_cast<char*>(name));
  }
  uint32_t mRefCnt = 0;
};

} // namespace v8

namespace {

template<uint32_t N>
void ObjectTemplateFinalize(JSFreeOp* fop, JSObject* obj) {
  JS::Value classValue = js::GetReservedSlot(obj, N);
  if (classValue.isUndefined()) {
    // We never got around to calling GetInstanceClass().
    return;
  }

  auto instanceClass =
    static_cast<v8::ObjectTemplate::InstanceClass*>(classValue.toPrivate());
  if (!instanceClass) {
    // We're using the default plain-object class.
    return;
  }

  instanceClass->Release();
}

enum class TemplateSlots {
  ClassNameSlot,          // Stores our class name (see SetClassName).
  InstanceClassSlot,      // Stores the InstanceClass* for our instances.
  InternalFieldCountSlot, // Stores the internal field count for our instances.
  NumSlots
};

const JSClassOps objectTemplateClassOps = {
  nullptr, // addProperty
  nullptr, // delProperty
  nullptr, // getProperty
  nullptr, // setProperty
  nullptr, // enumerate
  nullptr, // resolve
  nullptr, // mayResolve
  ObjectTemplateFinalize<uint32_t(TemplateSlots::InstanceClassSlot)>,
  nullptr, // call
  nullptr, // hasInstance
  nullptr, // construct
  nullptr  // trace
};

const JSClass objectTemplateClass = {
  "ObjectTemplate",
  JSCLASS_HAS_RESERVED_SLOTS(uint32_t(TemplateSlots::NumSlots)),
  &objectTemplateClassOps
};

const JSClassOps objectInstanceClassOps = {
  nullptr, // addProperty
  nullptr, // delProperty
  nullptr, // getProperty
  nullptr, // setProperty
  nullptr, // enumerate
  nullptr, // resolve
  nullptr, // mayResolve
  ObjectTemplateFinalize<uint32_t(InstanceSlots::InstanceClassSlot)>,
  nullptr, // call
  nullptr, // hasInstance
  nullptr, // construct
  nullptr  // trace
};

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
    Local<Value> value = internal::Local<Value>::New(isolate, args.get(0));

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
    Local<Value> value = internal::Local<Value>::New(isolate, args.get(0));

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
  Local<Object> thisObject =
    internal::Local<Object>::New(isolate, args.computeThis(cx));

  JS::RootedObject accessorData(cx,
    &js::GetFunctionNativeReserved(callee, 0).toObject());
  Local<Value> data =
    internal::Local<Value>::New(isolate,
      js::GetReservedSlot(accessorData, size_t(AccessorSlots::DataSlot)));
  Local<N> name =
    internal::Local<N>::New(isolate,
      js::GetReservedSlot(accessorData, size_t(AccessorSlots::NameSlot)));

  JS::RootedValue callbackVal1(cx,
    js::GetReservedSlot(accessorData, size_t(AccessorSlots::CallbackSlot1)));
  JS::RootedValue callbackVal2(cx,
    js::GetReservedSlot(accessorData, size_t(AccessorSlots::CallbackSlot2)));

  auto callback =
    ValuesToCallback<CallbackType>(callbackVal1, callbackVal2);
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
  } else {
    args.rval().setUndefined();
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

} // anonymous namespace

namespace v8 {

Local<ObjectTemplate> ObjectTemplate::New(Isolate* isolate) {
  Local<Template> templ = Template::New(isolate, &objectTemplateClass);
  return Local<ObjectTemplate>(static_cast<ObjectTemplate*>(*templ));
}

Local<Object> ObjectTemplate::NewInstance() {
  MaybeLocal<Object> maybeObj =
    NewInstance(Isolate::GetCurrent()->GetCurrentContext());
  return maybeObj.FromMaybe(Local<Object>());
}

MaybeLocal<Object> ObjectTemplate::NewInstance(Local<Context> context) {
  return NewInstance(Local<Object>());
}

Local<Object> ObjectTemplate::NewInstance(Local<Object> prototype) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  // XXXbz V8 uses a new HandleScope here.  I _think_ that's a performance
  // optimization....

  JS::RootedObject protoObj(cx);
  if (prototype.IsEmpty()) {
    JS::RootedObject currentGlobal(cx, JS::CurrentGlobalOrNull(cx));
    protoObj = JS_GetObjectPrototype(cx, currentGlobal);
  } else {
    protoObj = &GetValue(prototype)->toObject();
  }

  InstanceClass* instanceClass = GetInstanceClass();
  assert(instanceClass);

  // XXXbz this needs more fleshing out to deal with the whole business of
  // indexed/named getters/setters and so forth.  That will involve creating
  // proxy objects if we've had any of the indexed/named callbacks defined on
  // us.  For now, let's just go ahead and do the simple thing, since we don't
  // implement those parts of the API yet..
  JS::RootedObject instanceObj(cx);
  instanceObj = JS_NewObjectWithGivenProto(cx, Jsvalify(instanceClass), protoObj);
  if (!instanceObj) {
    return Local<Object>();
  }

  // Ensure that we keep our instance class, if any, alive as long as the
  // instance is alive.
  js::SetReservedSlot(instanceObj, size_t(InstanceSlots::InstanceClassSlot),
                      JS::PrivateValue(instanceClass));
  instanceClass->AddRef();

  // Copy the bits set on us via Template::Set.
  if (!JS_CopyPropertiesFrom(cx, instanceObj, obj)) {
    return Local<Object>();
  }

  JS::Value instanceVal = JS::ObjectValue(*instanceObj);
  return internal::Local<Object>::New(isolate, instanceVal);
}

void ObjectTemplate::SetClassName(Handle<String> name) {
  // XXXbz what do we do if we've already been instantiated?  We can't just
  // change our instance class...
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  JS::RootedValue nameVal(cx, *GetValue(name));
  if (!JS_WrapValue(cx, &nameVal)) {
    // TODO: Signal the failure somehow.
    return;
  }
  assert(nameVal.isString());
  js::SetReservedSlot(obj, size_t(TemplateSlots::ClassNameSlot), nameVal);
}

void ObjectTemplate::SetAccessor(Handle<String> name,
                                 AccessorGetterCallback getter,
                                 AccessorSetterCallback setter,
                                 Handle<Value> data,
                                 AccessControl settings,
                                 PropertyAttribute attribute,
                                 Handle<AccessorSignature> signature) {
  SetAccessorInternal(name, getter, setter, data,
                      settings, attribute, signature);
}

void ObjectTemplate::SetAccessor(Handle<Name> name,
                                 AccessorNameGetterCallback getter,
                                 AccessorNameSetterCallback setter,
                                 Handle<Value> data,
                                 AccessControl settings,
                                 PropertyAttribute attribute,
                                 Handle<AccessorSignature> signature) {
  SetAccessorInternal(name, getter, setter, data,
                      settings, attribute, signature);
}

template <class N, class Getter, class Setter>
void ObjectTemplate::SetAccessorInternal(Handle<N> name,
                                         Getter getter,
                                         Setter setter,
                                         Handle<Value> data,
                                         AccessControl settings,
                                         PropertyAttribute attribute,
                                         Handle<AccessorSignature> signature) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

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

// TODO SetNamedPropertyHandler: will need us to create proxies.

// TODO SetHandler both overloads: will need us to create proxies.

// TODO SetIndexedPropertyHandler: will need us to create proxies.

// TODO SetAccessCheckCallbacks: Can this just be a no-op?

// TODO SetInternalFieldCount

// TODO SetCallAsFunctionHandler

Handle<String> ObjectTemplate::GetClassName() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  JS::Value nameVal =
    js::GetReservedSlot(obj, size_t(TemplateSlots::ClassNameSlot));
  return internal::Local<String>::New(isolate, nameVal);
}

ObjectTemplate::InstanceClass* ObjectTemplate::GetInstanceClass() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  JS::Value classValue =
    js::GetReservedSlot(obj, size_t(TemplateSlots::InstanceClassSlot));
  if (!classValue.isUndefined()) {
    return static_cast<InstanceClass*>(classValue.toPrivate());
  }

  // Check whether we need to synthesize a new JSClass.
  InstanceClass* instanceClass = new InstanceClass();
  if (!instanceClass) {
    MOZ_CRASH();
  }

  JS::Value nameVal = js::GetReservedSlot(obj,
                                          size_t(TemplateSlots::ClassNameSlot));
  if (nameVal.isUndefined()) {
    instanceClass->name = "Object";
  } else {
    JS::RootedString str(cx, nameVal.toString());
    instanceClass->name = JS_EncodeStringToUTF8(cx, str);
  }

  JS::Value internalFieldCountVal = js::GetReservedSlot(obj,
                                                        size_t(TemplateSlots::InternalFieldCountSlot));
  uint32_t internalFieldCount = 0;
  if (!internalFieldCountVal.isUndefined()) {
    internalFieldCount = internalFieldCountVal.toInt32();
  }

  instanceClass->flags =
    JSCLASS_HAS_RESERVED_SLOTS(uint32_t(InstanceSlots::NumSlots) +
                               internalFieldCount);
  instanceClass->cOps = nullptr;
  instanceClass->spec = nullptr;
  instanceClass->ext = nullptr;
  instanceClass->oOps = nullptr;

  instanceClass->AddRef(); // Will be released in obj's finalizer.

  js::SetReservedSlot(obj, size_t(TemplateSlots::InstanceClassSlot),
                      JS::PrivateValue(instanceClass));
  return instanceClass;
}

void ObjectTemplate::SetInternalFieldCount(int value) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  js::SetReservedSlot(obj, size_t(TemplateSlots::InternalFieldCountSlot),
                      JS::Int32Value(value));
}

bool ObjectTemplate::IsInstance(JSObject* obj) {
  InstanceClass* instanceClass = GetInstanceClass();
  assert(instanceClass);
  return js::GetObjectClass(obj) == instanceClass;
}

} // namespace v8
