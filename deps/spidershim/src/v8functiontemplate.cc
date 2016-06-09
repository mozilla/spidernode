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

namespace {
enum FunctionTemplatePrivateSlots {
  CallbackSlot1,           // CallbackSlot1/2 store our FunctionCallback.
  CallbackSlot2,
  LengthSlot,              // Stores our length.
  DataSlot,                // Stores our data value.
  ProtoTemplateSlot,       // Stores our prototype's template, if any.
  InstanceTemplateSlot,    // Stores our instance template, if any.
  ParentSlot,              // Stores our FunctionTemplate parent, if any.
  InstanceSlot,            // Stores our instantiation (a function), if we have
                           // one.  XXXbz this doesn't seem quite right, but is
                           // probably OK if we only support one global.
  ProtoSlot,               // Stores our instantiated prototype, this is an
                           // object if and only if InstanceSlot is one.
  ClassNameSlot,           // Stores our class name (see SetClassName).
  NumSlots
};

const JSClass functionTemplateClass = {
  "FunctionTemplate", JSCLASS_HAS_RESERVED_SLOTS(NumSlots)
};

using namespace v8;

bool
SetCallbackAndData(JSContext* cx, JSObject* templateObj,
                   FunctionCallback callback, Handle<Value> data) {
  assert(JS_GetClass(templateObj) == &functionTemplateClass);

  JS::RootedValue val1(cx);
  JS::RootedValue val2(cx);
  CallbackToValues(callback, &val1, &val2);
  js::SetReservedSlot(templateObj, CallbackSlot1, val1);
  js::SetReservedSlot(templateObj, CallbackSlot2, val2);

  if (data.IsEmpty()) {
    js::SetReservedSlot(templateObj, DataSlot, JS::UndefinedValue());
  } else {
    JS::RootedValue val(cx, *GetValue(data));
    if (!JS_WrapValue(cx, &val)) {
      return false;
    }
    js::SetReservedSlot(templateObj, DataSlot, val);
  }
  return true;
}

bool
TemplateIsInstantiated(JSObject* templateObj) {
  assert(JS_GetClass(templateObj) == &functionTemplateClass);
  assert(js::GetReservedSlot(templateObj, InstanceSlot).isObject() ||
         js::GetReservedSlot(templateObj, InstanceSlot).isUndefined());
  return js::GetReservedSlot(templateObj, InstanceSlot).isObject();
}
} // anonymous namespace

namespace v8 {

// static
Local<FunctionTemplate> FunctionTemplate::New(Isolate* isolate,
                                              FunctionCallback callback,
                                              Handle<Value> data,
                                              Handle<Signature> signature,
                                              int length) {
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx);

  return New(isolate, cx, callback, data, signature, length);
}

// static
Local<FunctionTemplate> FunctionTemplate::New(Isolate* isolate,
                                              JSContext* cx,
                                              FunctionCallback callback,
                                              Handle<Value> data,
                                              Handle<Signature> signature,
                                              int length) {
  assert(cx == JSContextFromIsolate(isolate));

  Local<Template> templ = Template::New(isolate, cx, &functionTemplateClass);
  if (templ.IsEmpty()) {
    return Local<FunctionTemplate>();
  }

  JSObject* obj = GetObject(*templ);
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  if (!SetCallbackAndData(cx, obj, callback, data)) {
    return Local<FunctionTemplate>();
  }

  // TODO: figure out what should happen with signature.  What is it supposed to
  // do?  See https://github.com/mozilla/spidernode/issues/144 for some
  // discussion.
  js::SetReservedSlot(obj, LengthSlot, JS::Int32Value(length));

  return Local<FunctionTemplate>(static_cast<FunctionTemplate*>(*templ));
}

Local<Function> FunctionTemplate::GetFunction() {
  MaybeLocal<Function> maybeFunc =
    GetFunction(Isolate::GetCurrent()->GetCurrentContext());
  return maybeFunc.FromMaybe(Local<Function>());
}

MaybeLocal<Function> FunctionTemplate::GetFunction(Local<Context> context) {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx, this);
  Isolate* isolate = context->GetIsolate();
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  // XXXbz V8 uses a temporary HandleScope here, but I _think_ that's some sort
  // of performance optimization...

  if (TemplateIsInstantiated(obj)) {
    JS::RootedValue existingInstance(cx,
                                     js::GetReservedSlot(obj, InstanceSlot));
    assert(existingInstance.isObject());
    return internal::Local<Function>::New(isolate, existingInstance);
  }

  JS::RootedValue callbackVal1(cx, js::GetReservedSlot(obj, CallbackSlot1));
  JS::RootedValue callbackVal2(cx, js::GetReservedSlot(obj, CallbackSlot2));
  auto callback = ValuesToCallback<FunctionCallback>(callbackVal1, callbackVal2);

  Local<Value> dataV8Val =
    internal::Local<Value>::New(isolate, js::GetReservedSlot(obj, DataSlot));

  JS::RootedValue lengthVal(cx, js::GetReservedSlot(obj, LengthSlot));

  // Figure out our prototype's prototype object.
  JS::RootedObject protoProto(cx);
  JS::RootedValue parentVal(cx, js::GetReservedSlot(obj, ParentSlot));
  if (!parentVal.isUndefined()) {
    assert(parentVal.isObject());
    Local<FunctionTemplate> parentTemplate =
      internal::Local<FunctionTemplate>::NewTemplate(isolate,
        js::GetReservedSlot(obj, ParentSlot));

    MaybeLocal<Function> parentFunc = parentTemplate->GetFunction(context);
    if (parentFunc.IsEmpty()) {
      return MaybeLocal<Function>();
    }

    JS::RootedObject parentObj(cx, GetObject(parentFunc.ToLocalChecked()));
    assert(parentObj);

    JS::RootedValue protoProtoVal(cx);
    if (!JS_GetProperty(cx, parentObj, "prototype", &protoProtoVal)) {
      return MaybeLocal<Function>();
    }
    assert(protoProtoVal.isObject());
    protoProto = &protoProtoVal.toObject();
  } else {
    JS::RootedObject currentGlobal(cx, JS::CurrentGlobalOrNull(cx));
    protoProto = JS_GetObjectPrototype(cx, currentGlobal);
  }

  assert(protoProto);

  JS::RootedObject protoObj(cx);
  JS::RootedValue protoTemplateVal(cx, js::GetReservedSlot(obj, ProtoTemplateSlot));
  if (protoTemplateVal.isUndefined()) {
    protoObj = JS_NewObjectWithGivenProto(cx, nullptr, protoProto);
    if (!protoObj) {
      return MaybeLocal<Function>();
    }
  } else {
    assert(protoTemplateVal.isObject());
    Local<ObjectTemplate> protoTemplate =
      internal::Local<ObjectTemplate>::NewTemplate(isolate, protoTemplateVal);
    Local<Object> protoProtoObject =
      internal::Local<Object>::New(isolate, JS::ObjectValue(*protoProto));

    MaybeLocal<Object> protoInstance =
      protoTemplate->NewInstance(protoProtoObject);
    if (protoInstance.IsEmpty()) {
      return MaybeLocal<Function>();
    }

    protoObj = GetObject(protoInstance.ToLocalChecked());
    assert(protoObj);
  }

  // TODO: We should be doing something faster here in terms of setting up
  // prototypes right from the beginning or something.  Maybe we can refactor
  // Function::New's implementation a bit to allow us to pass the right things
  // into there.
  JS::Value thisVal(JS::ObjectValue(*obj));
  Local<FunctionTemplate> thisTempl =
    internal::Local<FunctionTemplate>::NewTemplate(isolate, thisVal);
  MaybeLocal<Function> func = Function::New(isolate->GetCurrentContext(),
                                            callback,
                                            dataV8Val,
                                            lengthVal.toInt32(),
                                            thisTempl,
                                            GetClassName());
  if (func.IsEmpty()) {
    return MaybeLocal<Function>();
  }

  // Copy the bits set on us via Template::Set.
  JS::RootedObject funcObj(cx, GetObject(func.ToLocalChecked()));
  assert(funcObj);
  if (!JS_CopyPropertiesFrom(cx, funcObj, obj)) {
    return MaybeLocal<Function>();
  }

  // TODO: Is the result of all this supposed to have a non-configurable
  // .prototype like builtins, or a configurable one like scripted functions?
  // Let's assume the former.  See
  // https://github.com/mozilla/spidernode/issues/140
  if (!JS_LinkConstructorAndPrototype(cx, funcObj, protoObj)) {
    return MaybeLocal<Function>();
  }

  // And now we're all instantiated.  Cache that value.
  js::SetReservedSlot(obj, InstanceSlot, JS::ObjectValue(*funcObj));
  js::SetReservedSlot(obj, ProtoSlot, JS::ObjectValue(*protoObj));

  return func;
}

Local<ObjectTemplate> FunctionTemplate::InstanceTemplate() {
  // XXXbz the V8 behavior here is weird: calling InstanceTemplate() will
  // modify this function template, but they don't ensure that we have not been
  // instantiated yet...  Ah, well.  Let's just do what they do.
  return FetchOrCreateTemplate(InstanceTemplateSlot);
}

Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
  // XXXbz the V8 behavior here is weird: calling PrototypeTemplate() will
  // modify this function template, but they don't ensure that we have not been
  // instantiated yet...  Ah, well.  Let's just do what they do.
  return FetchOrCreateTemplate(ProtoTemplateSlot);
}

void FunctionTemplate::SetClassName(Handle<String> name) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  if (TemplateIsInstantiated(obj)) {
    // TODO: Supposed to be a fatal error of some sort.
    return;
  }

  JS::RootedValue nameVal(cx, *GetValue(name));
  if (!JS_WrapValue(cx, &nameVal)) {
    // TODO: Signal the failure somehow.
    return;
  }
  assert(nameVal.isString());
  js::SetReservedSlot(obj, ClassNameSlot, nameVal);
}

Handle<String> FunctionTemplate::GetClassName() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  JS::Value nameVal = js::GetReservedSlot(obj, ClassNameSlot);
  if (nameVal.isUndefined()) {
    return Local<String>();
  }

  assert(nameVal.isString());
  return internal::Local<String>::New(isolate, nameVal);
}

void
FunctionTemplate::SetHiddenPrototype(bool value) {
  // TODO: Do we really need to implement this?  Let's hope not!
}

void
FunctionTemplate::SetCallHandler(FunctionCallback callback,
                                 Handle<Value> data) {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  if (TemplateIsInstantiated(obj)) {
    // TODO: Supposed to be a fatal error of some sort.
    return;
  }

  if (!SetCallbackAndData(cx, obj, callback, data)) {
    // TODO: Signal the error somehow.
    return;
  }
}

// TODO: Need to implement HasInstance.  What are the actual semantics of this?
// I can't figure out so far from the V8 API docs and their implementation.  In
// particular, does it just check whether this is precisely an object that
// constructing via the function returned by this FunctionTemplate produces?  Or
// is any subclass (including via FunctionTemplates that inherit from this one)
// OK?  Or is this a dynamic check equivalent to the instanceof operator in JS?
// See https://github.com/mozilla/spidernode/issues/143

void
FunctionTemplate::Inherit(Handle<FunctionTemplate> parent) {
  JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  if (TemplateIsInstantiated(obj)) {
    // TODO: Supposed to be a fatal error of some sort.
    return;
  }

  JS::RootedValue slotVal(cx);
  if (!parent.IsEmpty()) {
    JS::RootedObject otherTemplate(cx, GetObject(*parent));
    if (js::GetObjectCompartment(otherTemplate) !=
	js::GetContextCompartment(cx)) {
      MOZ_CRASH("Trying to set up inheritance between FunctionTemplates from "
		"different compartments");
    }
    slotVal.setObject(*otherTemplate);
  }
  // else slotVal is already undefined

  js::SetReservedSlot(obj, ParentSlot, slotVal);
}

Local<Object> FunctionTemplate::CreateNewInstance() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  Local<ObjectTemplate> instanceTemplate = InstanceTemplate();
  if (instanceTemplate.IsEmpty()) {
    return Local<Object>();
  }
  return instanceTemplate->NewInstance();
}

Local<ObjectTemplate> FunctionTemplate::FetchOrCreateTemplate(size_t slotIndex) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  JS::RootedValue templateVal(cx, js::GetReservedSlot(obj, slotIndex));
  if (templateVal.isObject()) {
    return
      internal::Local<ObjectTemplate>::NewTemplate(isolate, templateVal);
  }

  Local<FunctionTemplate> ctor;
  if (slotIndex == InstanceTemplateSlot) {
    // We want to use ourselves as the constructor here.
    ctor =
      internal::Local<FunctionTemplate>::NewTemplate(isolate, *GetValue(this));
  }
  assert(templateVal.isUndefined());
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate, cx, ctor);
  if (templ.IsEmpty()) {
    return Local<ObjectTemplate>();
  }

  js::SetReservedSlot(obj, slotIndex, JS::ObjectValue(*GetObject(*templ)));
  return templ;
}

bool FunctionTemplate::HasInstance(Local<Value> val) {
  if (val.IsEmpty() || !val->IsObject()) {
    return false;
  }
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedValue value(cx, *GetValue(val));
  if (!JS_WrapValue(cx, &value)) {
    return false;
  }
  Local<Object> obj = internal::Local<Object>::New(isolate, value);
  if (!ObjectTemplate::IsObjectFromTemplate(obj)) {
    return false;
  }
  Local<FunctionTemplate> ctor = ObjectTemplate::GetObjectTemplateConstructor(obj);
  while (!ctor.IsEmpty()) {
    // We don't need JS_StrictlyEqual since we know both sides are objects here.
    if (*GetValue(ctor) == *GetValue(this)) {
      return true;
    }
    ctor = GetParent().FromMaybe(Local<FunctionTemplate>());
  }
  return false;
}

// static
Local<Value> FunctionTemplate::MaybeConvertObjectProperty(Local<Value> value,
							  Local<String> name) {
  if (!value.IsEmpty() && value->IsObject() &&
      JS_GetClass(GetObject(value)) == &functionTemplateClass) {
    Local<Function> func =
      reinterpret_cast<FunctionTemplate*>(GetValue(value))->GetFunction();
    func->SetName(name);
    return func;
  }
  return value;
}

void FunctionTemplate::SetInstanceTemplate(Local<ObjectTemplate> instanceTemplate) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);
  assert(!instanceTemplate.IsEmpty());

  js::SetReservedSlot(obj, InstanceTemplateSlot,
                      JS::ObjectValue(*GetObject(*instanceTemplate)));
}

Local<Object> FunctionTemplate::GetProtoInstance(Local<Context> context) {
  // First ensure we're instantiated.
  MaybeLocal<Function> func = GetFunction(context);
  if (func.IsEmpty()) {
    return Local<Object>();
  }

  Isolate* isolate = context->GetIsolate();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);
  JS::Value protoVal = js::GetReservedSlot(obj, ProtoSlot);
  assert(protoVal.isObject());
  return internal::Local<Object>::New(isolate, protoVal);
}

MaybeLocal<FunctionTemplate> FunctionTemplate::GetParent()
{
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, GetObject(this));
  assert(obj);
  assert(JS_GetClass(obj) == &functionTemplateClass);

  JS::Value parentVal = js::GetReservedSlot(obj, ParentSlot);
  if (parentVal.isUndefined()) {
    return MaybeLocal<FunctionTemplate>();
  }

  assert(parentVal.isObject());
  return internal::Local<FunctionTemplate>::NewTemplate(isolate, parentVal);
}

bool FunctionTemplate::InstallInstanceProperties(Local<Object> target) {
  assert(!target.IsEmpty());

  // Install our parent's properties first so we'll override as needed if we
  // have properties with the same name.
  MaybeLocal<FunctionTemplate> parent = GetParent();
  if (!parent.IsEmpty() &&
      !parent.ToLocalChecked()->InstallInstanceProperties(target)) {
    return false;
  }

  Local<ObjectTemplate> instance = InstanceTemplate();
  if (instance.IsEmpty()) {
    return false;
  }

  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject instanceObj(cx, GetObject(*instance));
  assert(instanceObj);

  JS::RootedObject targetObj(cx, GetObject(target));
  assert(targetObj);

  return JS_CopyPropertiesFrom(cx, targetObj, instanceObj);
}

} // namespace v8
