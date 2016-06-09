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
#include "accessor.h"

namespace v8 {

/**
 * We sometimes need custom JSClasses, and we need to dynamically generate
 * them.  That means we want to keep them alive as long as any objects using
 * them are alive.  We implement this by having the relevant objects refcount
 * the class and drop the ref in the finalizer.
 */
class ObjectTemplate::InstanceClass : public JSClass {
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

  static const uint32_t nameAllocated = JSCLASS_USERBIT1;

private:
  ~InstanceClass() {
    if (flags & nameAllocated) {
      // We heap-allocated our name, so need to free it now.
      JS_free(nullptr, const_cast<char*>(name));
    }
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
  assert(instanceClass);

  instanceClass->Release();
}

enum class TemplateSlots {
  InstanceClassSlot,      // Stores the InstanceClass* for our instances.
  InternalFieldCountSlot, // Stores the internal field count for our instances.
  ConstructorSlot,        // Stores our constructor FunctionTemplate.
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

} // anonymous namespace

namespace v8 {

Local<ObjectTemplate> ObjectTemplate::New(Isolate* isolate,
                                          Local<FunctionTemplate> constructor) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);
  if (!constructor.IsEmpty() &&
      js::GetContextCompartment(cx) !=
      js::GetObjectCompartment(GetObject(*constructor))) {
    MOZ_CRASH("Attempt to create an ObjectTemplate with a constructor from a "
              "different compartment");
  }

  return ObjectTemplate::New(isolate, cx, constructor);
}

Local<ObjectTemplate> ObjectTemplate::New(Isolate* isolate,
                                          JSContext* cx,
                                          Local<FunctionTemplate> constructor) {
  assert(cx == JSContextFromIsolate(isolate));

  bool setInstance = false;
  if (constructor.IsEmpty()) {
    constructor = FunctionTemplate::New(isolate);
    setInstance = true;
    if (constructor.IsEmpty()) {
      return Local<ObjectTemplate>();
    }
  }

  Local<Template> templ = Template::New(isolate, cx, &objectTemplateClass);
  if (templ.IsEmpty()) {
    return Local<ObjectTemplate>();
  }

  Local<ObjectTemplate> objectTempl(static_cast<ObjectTemplate*>(*templ));
  JS::RootedObject obj(cx, GetObject(*templ));

  if (setInstance) {
    constructor->SetInstanceTemplate(objectTempl);
  }

  js::SetReservedSlot(obj, size_t(TemplateSlots::ConstructorSlot),
                      JS::ObjectValue(*GetObject(*constructor)));
  return objectTempl;
}

Local<Object> ObjectTemplate::NewInstance() {
  MaybeLocal<Object> maybeObj =
    NewInstance(Isolate::GetCurrent()->GetCurrentContext());
  return maybeObj.FromMaybe(Local<Object>());
}

MaybeLocal<Object> ObjectTemplate::NewInstance(Local<Context> context) {
  Local<Object> prototype = GetConstructor()->GetProtoInstance(context);
  if (prototype.IsEmpty()) {
    return MaybeLocal<Object>();
  }

  return NewInstance(prototype);
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

  assert(!prototype.IsEmpty());
  JS::RootedObject protoObj(cx, &GetValue(prototype)->toObject());

  InstanceClass* instanceClass = GetInstanceClass();
  assert(instanceClass);

  // XXXbz this needs more fleshing out to deal with the whole business of
  // indexed/named getters/setters and so forth.  That will involve creating
  // proxy objects if we've had any of the indexed/named callbacks defined on
  // us.  For now, let's just go ahead and do the simple thing, since we don't
  // implement those parts of the API yet..
  JS::RootedObject instanceObj(cx);
  instanceObj = JS_NewObjectWithGivenProto(cx, instanceClass, protoObj);
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
  GetConstructor()->SetClassName(name);
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

  internal::SetAccessor(cx, obj, name, getter, setter, data,
                        settings, attribute, signature);
}

// TODO SetNamedPropertyHandler: will need us to create proxies.

// TODO SetHandler both overloads: will need us to create proxies.

// TODO SetIndexedPropertyHandler: will need us to create proxies.

// TODO SetAccessCheckCallbacks: Can this just be a no-op?

// TODO SetCallAsFunctionHandler

Handle<String> ObjectTemplate::GetClassName() {
  return GetConstructor()->GetClassName();
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

  InstanceClass* instanceClass = new InstanceClass();
  if (!instanceClass) {
    MOZ_CRASH();
  }

  uint32_t flags = 0;
  Local<String> name = GetClassName();
  if (name.IsEmpty()) {
    instanceClass->name = "Object";
  } else {
    JS::RootedString str(cx, GetValue(name)->toString());
    instanceClass->name = JS_EncodeStringToUTF8(cx, str);
    flags |= InstanceClass::nameAllocated;
  }

  JS::Value internalFieldCountVal = js::GetReservedSlot(obj,
                                                        size_t(TemplateSlots::InternalFieldCountSlot));
  uint32_t internalFieldCount = 0;
  if (!internalFieldCountVal.isUndefined()) {
    internalFieldCount = internalFieldCountVal.toInt32();
  }

  instanceClass->flags =
    flags | JSCLASS_HAS_RESERVED_SLOTS(uint32_t(InstanceSlots::NumSlots) +
                                       internalFieldCount);
  instanceClass->cOps = &objectInstanceClassOps;
  instanceClass->reserved[0] = nullptr;
  instanceClass->reserved[1] = nullptr;
  instanceClass->reserved[2] = nullptr;

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
  return JS_GetClass(obj) == instanceClass;
}

Local<FunctionTemplate> ObjectTemplate::GetConstructor() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  JS::Value ctorVal =
    js::GetReservedSlot(obj, size_t(TemplateSlots::ConstructorSlot));
  return internal::Local<FunctionTemplate>::NewTemplate(isolate, ctorVal);
}

} // namespace v8
