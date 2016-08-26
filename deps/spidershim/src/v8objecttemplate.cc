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
#include "autojsapi.h"
#include "jsfriendapi.h"
#include "instanceslots.h"
#include "accessor.h"
#include "mozilla/Maybe.h"
#include "utils.h"

namespace v8 {

/**
 * We sometimes need custom JSClasses, and we need to dynamically generate
 * them.  That means we want to keep them alive as long as any objects using
 * them are alive.  We implement this by having the relevant objects refcount
 * the class and drop the ref in the finalizer.
 */
class ObjectTemplate::InstanceClass : public JSClass {
public:
  InstanceClass() {
    name = nullptr;
    flags = 0;
    memset(&instanceClassOps, 0, sizeof(JSClassOps));
    cOps = &instanceClassOps;
    reserved[0] = nullptr;
    reserved[1] = nullptr;
    reserved[2] = nullptr;
    memset(&instanceObjectOps, 0, sizeof(js::ObjectOps));
    reinterpret_cast<js::Class*>(this)->oOps = &instanceObjectOps;
  }
  void AddRef() {
    ++mRefCnt;
  }

  void Release() {
    --mRefCnt;
    if (mRefCnt == 0) {
      delete this;
    }
  }

  JSClassOps& ModifyClassOps() { return instanceClassOps; }
  js::ObjectOps& ModifyObjectOps() { return instanceObjectOps; }

  static InstanceClass* FromObject(JS::HandleObject obj) {
    const JSClass* clasp = JS_GetClass(obj);
    assert(clasp->flags & instantiatedFromTemplate);
    return const_cast<InstanceClass*>(
        static_cast<const InstanceClass*>(clasp));
  }

  static const uint32_t nameAllocated = JSCLASS_USERBIT1;
  static const uint32_t instantiatedFromTemplate = JSCLASS_USERBIT2;

private:
  ~InstanceClass() {
    if (flags & nameAllocated) {
      // We heap-allocated our name, so need to free it now.
      JS_free(nullptr, const_cast<char*>(name));
    }
  }
  uint32_t mRefCnt = 0;
  JSClassOps instanceClassOps;
  js::ObjectOps instanceObjectOps;
};

} // namespace v8

namespace {
using namespace v8;

enum class TemplateSlots {
  InstanceClassSlot,              // Stores the InstanceClass* for our instances.
  GlobalInstanceClassSlot,        // Stores the InstanceClass* for our global object instances.
  InternalFieldCountSlot,         // Stores the internal field count for our instances.
  ConstructorSlot,                // Stores our constructor FunctionTemplate.
  CallCallbackSlot,               // Stores our call as function callback function.
  NamedGetterCallbackSlot1,       // Stores our named prop getter callback.
  NamedGetterCallbackSlot2,
  NamedSetterCallbackSlot1,       // Stores our named prop setter callback.
  NamedSetterCallbackSlot2,
  NamedQueryCallbackSlot1,        // Stores our named prop query callback.
  NamedQueryCallbackSlot2,
  NamedDeleterCallbackSlot1,      // Stores our named prop deleter callback.
  NamedDeleterCallbackSlot2,
  NamedEnumeratorCallbackSlot1,   // Stores our named prop enumerator callback.
  NamedEnumeratorCallbackSlot2,
  NamedCallbackDataSlot,          // Stores our named prop callback data
  IndexedGetterCallbackSlot1,     // Stores our indexed prop getter callback.
  IndexedGetterCallbackSlot2,
  IndexedSetterCallbackSlot1,     // Stores our indexed prop setter callback.
  IndexedSetterCallbackSlot2,
  IndexedQueryCallbackSlot1,      // Stores our indexed prop query callback.
  IndexedQueryCallbackSlot2,
  IndexedDeleterCallbackSlot1,    // Stores our indexed prop deleter callback.
  IndexedDeleterCallbackSlot2,
  IndexedEnumeratorCallbackSlot1, // Stores our indexed prop enumerator callback.
  IndexedEnumeratorCallbackSlot2,
  IndexedCallbackDataSlot,        // Stores our indexed prop callback data
  GenericGetterCallbackSlot1,     // Stores our generic prop getter callback.
  GenericGetterCallbackSlot2,
  GenericSetterCallbackSlot1,     // Stores our generic prop setter callback.
  GenericSetterCallbackSlot2,
  GenericQueryCallbackSlot1,      // Stores our generic prop query callback.
  GenericQueryCallbackSlot2,
  GenericDeleterCallbackSlot1,    // Stores our generic prop deleter callback.
  GenericDeleterCallbackSlot2,
  GenericEnumeratorCallbackSlot1, // Stores our generic prop enumerator callback.
  GenericEnumeratorCallbackSlot2,
  GenericCallbackDataSlot,        // Stores our generic prop callback data
  NumSlots
};

void ReleaseInstanceClass(const JS::Value& classValue) {
  auto instanceClass =
    static_cast<ObjectTemplate::InstanceClass*>(classValue.toPrivate());
  assert(instanceClass);

  instanceClass->Release();
}

void InstanceTemplateFinalize(JSFreeOp* fop, JSObject* obj) {
  JS::Value classValue =
      GetInstanceSlot(obj, size_t(InstanceSlots::InstanceClassSlot));
  if (classValue.isUndefined()) {
    // We never got around to calling GetInstanceClass().
    return;
  }

  ReleaseInstanceClass(classValue);
}

void ObjectTemplateFinalize(JSFreeOp* fop, JSObject* obj) {
  JS::Value classValue =
    js::GetReservedSlot(obj, size_t(TemplateSlots::InstanceClassSlot));
  if (!classValue.isUndefined()) {
    ReleaseInstanceClass(classValue);
  }

  classValue =
    js::GetReservedSlot(obj, size_t(TemplateSlots::GlobalInstanceClassSlot));
  if (!classValue.isUndefined()) {
    ReleaseInstanceClass(classValue);
  }
}

const JSClassOps objectTemplateClassOps = {
  nullptr, // addProperty
  nullptr, // delProperty
  nullptr, // getProperty
  nullptr, // setProperty
  nullptr, // enumerate
  nullptr, // resolve
  nullptr, // mayResolve
  ObjectTemplateFinalize,
  nullptr, // call
  nullptr, // hasInstance
  nullptr, // construct
  nullptr  // trace
};

const JSClass objectTemplateClass = {
  "ObjectTemplate",
  JSCLASS_HAS_RESERVED_SLOTS(uint32_t(TemplateSlots::NumSlots)) |
  JSCLASS_FOREGROUND_FINALIZE,
  &objectTemplateClassOps
};

template<typename, typename> struct SlotTraits;
template<typename> struct PropXerTraits;
template<typename, typename> struct PropCallbackTraits;

template<typename T>
struct SlotTraits<T, Name> {
  static const size_t PropGetterCallback1 = size_t(T::GenericGetterCallbackSlot1);
  static const size_t PropGetterCallback2 = size_t(T::GenericGetterCallbackSlot2);

  static const size_t PropSetterCallback1 = size_t(T::GenericSetterCallbackSlot1);
  static const size_t PropSetterCallback2 = size_t(T::GenericSetterCallbackSlot2);

  static const size_t PropQueryCallback1 = size_t(T::GenericQueryCallbackSlot1);
  static const size_t PropQueryCallback2 = size_t(T::GenericQueryCallbackSlot2);

  static const size_t PropDeleterCallback1 = size_t(T::GenericDeleterCallbackSlot1);
  static const size_t PropDeleterCallback2 = size_t(T::GenericDeleterCallbackSlot2);

  static const size_t PropEnumeratorCallback1 = size_t(T::GenericEnumeratorCallbackSlot1);
  static const size_t PropEnumeratorCallback2 = size_t(T::GenericEnumeratorCallbackSlot2);

  static const size_t PropData = size_t(T::GenericCallbackDataSlot);
};

template<typename T>
struct SlotTraits<T, String> {
  static const size_t PropGetterCallback1 = size_t(T::NamedGetterCallbackSlot1);
  static const size_t PropGetterCallback2 = size_t(T::NamedGetterCallbackSlot2);

  static const size_t PropSetterCallback1 = size_t(T::NamedSetterCallbackSlot1);
  static const size_t PropSetterCallback2 = size_t(T::NamedSetterCallbackSlot2);

  static const size_t PropQueryCallback1 = size_t(T::NamedQueryCallbackSlot1);
  static const size_t PropQueryCallback2 = size_t(T::NamedQueryCallbackSlot2);

  static const size_t PropDeleterCallback1 = size_t(T::NamedDeleterCallbackSlot1);
  static const size_t PropDeleterCallback2 = size_t(T::NamedDeleterCallbackSlot2);

  static const size_t PropEnumeratorCallback1 = size_t(T::NamedEnumeratorCallbackSlot1);
  static const size_t PropEnumeratorCallback2 = size_t(T::NamedEnumeratorCallbackSlot2);

  static const size_t PropData = size_t(T::NamedCallbackDataSlot);
};

template<typename T>
struct SlotTraits<T, uint32_t> {
  static const size_t PropGetterCallback1 = size_t(T::IndexedGetterCallbackSlot1);
  static const size_t PropGetterCallback2 = size_t(T::IndexedGetterCallbackSlot2);

  static const size_t PropSetterCallback1 = size_t(T::IndexedSetterCallbackSlot1);
  static const size_t PropSetterCallback2 = size_t(T::IndexedSetterCallbackSlot2);

  static const size_t PropQueryCallback1 = size_t(T::IndexedQueryCallbackSlot1);
  static const size_t PropQueryCallback2 = size_t(T::IndexedQueryCallbackSlot2);

  static const size_t PropDeleterCallback1 = size_t(T::IndexedDeleterCallbackSlot1);
  static const size_t PropDeleterCallback2 = size_t(T::IndexedDeleterCallbackSlot2);

  static const size_t PropEnumeratorCallback1 = size_t(T::IndexedEnumeratorCallbackSlot1);
  static const size_t PropEnumeratorCallback2 = size_t(T::IndexedEnumeratorCallbackSlot2);

  static const size_t PropData = size_t(T::IndexedCallbackDataSlot);
};

#define DEFINE_MIRROR(Xer, Mirror)                             \
  static void ClearMirror## Xer ##s(JS::HandleObject obj) {    \
    using Template = SlotTraits<TemplateSlots, Mirror>;        \
    js::SetReservedSlot(obj, Template::Prop## Xer ##Callback1, \
                        JS::UndefinedValue());                 \
    js::SetReservedSlot(obj, Template::Prop## Xer ##Callback2, \
                        JS::UndefinedValue());                 \
  }

template<>
struct PropXerTraits<Name> {
  typedef GenericNamedPropertyGetterCallback PropGetter;
  typedef GenericNamedPropertySetterCallback PropSetter;
  typedef GenericNamedPropertyQueryCallback PropQuery;
  typedef GenericNamedPropertyDeleterCallback PropDeleter;
  typedef GenericNamedPropertyEnumeratorCallback PropEnumerator;

  static Local<v8::Name> MakeName(Isolate* isolate, JS::HandleId id) {
    Local<Name> name =
      internal::Local<String>::New(isolate,
                                   JS::StringValue(JSID_TO_STRING(id)));
    return name;
  }

  DEFINE_MIRROR(Getter, String)
  DEFINE_MIRROR(Setter, String)
  DEFINE_MIRROR(Query, String)
  DEFINE_MIRROR(Deleter, String)
  DEFINE_MIRROR(Enumerator, String)

  static void ClearMirrorData(JS::HandleObject obj) {
    using Template = SlotTraits<TemplateSlots, String>;

    js::SetReservedSlot(obj, Template::PropData,
                        JS::UndefinedValue());
  }
};

template<>
struct PropXerTraits<String> {
  typedef NamedPropertyGetterCallback PropGetter;
  typedef NamedPropertySetterCallback PropSetter;
  typedef NamedPropertyQueryCallback PropQuery;
  typedef NamedPropertyDeleterCallback PropDeleter;
  typedef NamedPropertyEnumeratorCallback PropEnumerator;

  static Local<v8::String> MakeName(Isolate* isolate, JS::HandleId id) {
    Local<String> name =
      internal::Local<String>::New(isolate,
                                   JS::StringValue(JSID_TO_STRING(id)));
    return name;
  }

  DEFINE_MIRROR(Getter, Name)
  DEFINE_MIRROR(Setter, Name)
  DEFINE_MIRROR(Query, Name)
  DEFINE_MIRROR(Deleter, Name)
  DEFINE_MIRROR(Enumerator, Name)

  static void ClearMirrorData(JS::HandleObject obj) {
    using Template = SlotTraits<TemplateSlots, Name>;

    js::SetReservedSlot(obj, Template::PropData,
                        JS::UndefinedValue());
  }
};

#undef DEFINE_MIRROR

template<>
struct PropXerTraits<uint32_t> {
  typedef IndexedPropertyGetterCallback PropGetter;
  typedef IndexedPropertySetterCallback PropSetter;
  typedef IndexedPropertyQueryCallback PropQuery;
  typedef IndexedPropertyDeleterCallback PropDeleter;
  typedef IndexedPropertyEnumeratorCallback PropEnumerator;

  static uint32_t MakeName(Isolate* isolate, JS::HandleId id) {
    return JSID_TO_INT(id);
  }

  static void ClearMirrorGetters(JS::HandleObject obj) {}
  static void ClearMirrorSetters(JS::HandleObject obj) {}
  static void ClearMirrorQuerys(JS::HandleObject obj) {}
  static void ClearMirrorDeleters(JS::HandleObject obj) {}
  static void ClearMirrorEnumerators(JS::HandleObject obj) {}
  static void ClearMirrorData(JS::HandleObject obj) {}
};

template<typename N>
struct PropCallbackTraits<typename PropXerTraits<N>::PropGetter, N> :
  public PropXerTraits<N> {
  typedef v8::PropertyCallbackInfo<Value> PropertyCallbackInfo;
  using Instance = SlotTraits<InstanceSlots, N>;

  static void doCall(Isolate* isolate,
                     typename PropXerTraits<N>::PropGetter callback,
                     JS::HandleId id,
                     PropertyCallbackInfo& info,
                     JS::MutableHandleValue vp) {
    auto name = PropXerTraits<N>::MakeName(isolate, id);

    callback(name, info);

    if (auto rval = info.GetReturnValue().Get()) {
      vp.set(*GetValue(rval));
    } else {
      vp.setUndefined();
    }
  }
};

template<typename N>
struct PropCallbackTraits<typename PropXerTraits<N>::PropSetter, N> :
  public PropXerTraits<N> {
  typedef v8::PropertyCallbackInfo<Value> PropertyCallbackInfo;
  using Instance = SlotTraits<InstanceSlots, N>;

  static void doCall(Isolate* isolate,
                     typename PropXerTraits<N>::PropSetter callback,
                     JS::HandleId id,
                     PropertyCallbackInfo& info,
                     JS::MutableHandleValue vp) {
    auto name = PropXerTraits<N>::MakeName(isolate, id);
    Local<Value> value = internal::Local<Value>::New(isolate, vp);

    callback(name, value, info);
  }
};

template<typename N>
struct PropCallbackTraits<typename PropXerTraits<N>::PropQuery, N> :
  public PropXerTraits<N> {
  typedef v8::PropertyCallbackInfo<Integer> PropertyCallbackInfo;
  using Instance = SlotTraits<InstanceSlots, N>;

  static void doCall(Isolate* isolate,
                     typename PropXerTraits<N>::PropQuery callback,
                     JS::HandleId id,
                     PropertyCallbackInfo& info) {
    auto name = PropXerTraits<N>::MakeName(isolate, id);

    callback(name, info);
  }
};

template<typename N>
struct PropCallbackTraits<typename PropXerTraits<N>::PropDeleter, N> :
  public PropXerTraits<N> {
  typedef v8::PropertyCallbackInfo<Boolean> PropertyCallbackInfo;
  using Instance = SlotTraits<InstanceSlots, N>;

  static void doCall(Isolate* isolate,
                     typename PropXerTraits<N>::PropDeleter callback,
                     JS::HandleId id,
                     PropertyCallbackInfo& info) {
    auto name = PropXerTraits<N>::MakeName(isolate, id);

    callback(name, info);
  }
};

template<typename N>
struct PropCallbackTraits<typename PropXerTraits<N>::PropEnumerator, N> :
  public PropXerTraits<N> {
  typedef v8::PropertyCallbackInfo<Array> PropertyCallbackInfo;
  using Instance = SlotTraits<InstanceSlots, N>;

  static void doCall(Isolate* isolate,
                     typename PropXerTraits<N>::PropEnumerator callback,
                     PropertyCallbackInfo& info) {
    callback(info);
  }
};

template<typename N>
static void CopyTemplateCallbackPropsOnInstance(JSContext* cx,
                                                JS::HandleObject templateObj,
                                                JS::HandleObject instanceObj) {
  using Instance = SlotTraits<InstanceSlots, N>;
  using Template = SlotTraits<TemplateSlots, N>;

#define COPIER(Xer)                                                      \
  JS::Value callback1 ## Xer =                                           \
    js::GetReservedSlot(templateObj, Template::Prop## Xer ##Callback1);  \
  JS::Value callback2 ## Xer =                                           \
    js::GetReservedSlot(templateObj, Template::Prop## Xer ##Callback2);  \
  if (!callback1 ## Xer.isUndefined() &&                                 \
      !callback2 ## Xer.isUndefined()) {                                 \
    SetInstanceSlot(instanceObj, Instance::Prop## Xer ##Callback1,       \
                    callback1 ## Xer);                                   \
    SetInstanceSlot(instanceObj, Instance::Prop## Xer ##Callback2,       \
                    callback2 ## Xer);                                   \
  }

  COPIER(Getter)
  COPIER(Setter)
  COPIER(Query)
  COPIER(Deleter)
  COPIER(Enumerator)

#undef COPIER

  JS::RootedValue namedCallbackData(cx,
    js::GetReservedSlot(templateObj, Template::PropData));
  if (!namedCallbackData.isUndefined() &&
      JS_WrapValue(cx, &namedCallbackData)) {
    SetInstanceSlot(instanceObj, Instance::PropData, namedCallbackData);
  }
}

#define HAS_XER_PROP(Xer)                                       \
template<typename N>                                            \
static bool Has## Xer ##Prop(JS::HandleObject obj) {            \
  using Template = SlotTraits<TemplateSlots, N>;                \
  JS::Value callback1 =                                         \
    js::GetReservedSlot(obj, Template::Prop## Xer ##Callback1); \
  JS::Value callback2 =                                         \
    js::GetReservedSlot(obj, Template::Prop## Xer ##Callback2); \
  return !callback1.isUndefined() && !callback2.isUndefined();  \
}

HAS_XER_PROP(Getter)
HAS_XER_PROP(Setter)
HAS_XER_PROP(Query)
HAS_XER_PROP(Deleter)
HAS_XER_PROP(Enumerator)

#undef HAS_XER_PROP

template<typename N, typename Getter, typename Setter, typename Query,
         typename Deleter, typename Enumerator>
void SetHandler(JSContext* cx, JS::HandleObject obj, Getter getter,
                Setter setter, Query query, Deleter deleter,
                Enumerator enumerator, Local<Value> data) {
  using Template = SlotTraits<TemplateSlots, N>;

  JS::RootedValue callback1(cx);
  JS::RootedValue callback2(cx);

#define SET_XER(xer, Xer)                                      \
  if (xer) {                                                   \
    CallbackToValues(xer, &callback1, &callback2);             \
    js::SetReservedSlot(obj, Template::Prop## Xer ##Callback1, \
                        callback1);                            \
    js::SetReservedSlot(obj, Template::Prop## Xer ##Callback2, \
                        callback2);                            \
    PropXerTraits<N>::ClearMirror## Xer ##s(obj);              \
  }

  SET_XER(getter, Getter)
  SET_XER(setter, Setter)
  SET_XER(query, Query)
  SET_XER(deleter, Deleter)
  SET_XER(enumerator, Enumerator)

#undef SET_XER

  if (!data.IsEmpty()) {
    js::SetReservedSlot(obj, Template::PropData,
                        *GetValue(data));

    PropXerTraits<N>::ClearMirrorData(obj);
  }
}

#define PREPARE_CALLBACK(Xer)                                            \
  Isolate* isolate = Isolate::GetCurrent();                              \
  /* Make sure there is _a_ handlescope on the stack */                  \
  HandleScope handleScope(isolate);                                      \
  typedef PropCallbackTraits<CallbackType, N> Traits;                    \
  typedef typename Traits::PropertyCallbackInfo PropertyCallbackInfo;    \
  CallbackType callback = nullptr;                                       \
  JS::RootedValue dataVal(cx,                                            \
    GetInstanceSlot(obj, Traits::Instance::PropData));                   \
  Local<Value> data = internal::Local<Value>::New(isolate, dataVal);     \
  Local<Object> thisObj =                                                \
    internal::Local<Object>::New(isolate, JS::ObjectValue(*obj));        \
  JS::RootedValue callback1(cx,                                          \
    GetInstanceSlot(obj, Traits::Instance::Prop## Xer ##Callback1));     \
  JS::RootedValue callback2(cx,                                          \
    GetInstanceSlot(obj, Traits::Instance::Prop## Xer ##Callback2));     \
  if (!callback1.isUndefined() && !callback2.isUndefined()) {            \
    callback = ValuesToCallback<CallbackType>(callback1, callback2);     \
  }

template<typename CallbackType, typename N>
static bool GetterOpImpl(JSContext* cx, JS::HandleObject obj,
                         JS::HandleId id, JS::MutableHandleValue vp) {
  PREPARE_CALLBACK(Getter)

  if (callback) {
    JS::RootedValue value(cx);
    PropertyCallbackInfo info(data, thisObj, thisObj);
    PropCallbackTraits<CallbackType, N>::doCall(isolate, callback, id,
                                                info, &value);
    if (!JS_WrapValue(cx, &value)) {
      return false;
    }
    vp.set(value);
  } else {
    vp.set(JS::UndefinedValue());
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

static bool GetterOp(JSContext* cx, JS::HandleObject obj,
                     JS::HandleId id, JS::MutableHandleValue vp) {
  JSGetterOp impl = nullptr;
  if (JSID_IS_INT(id)) {
    impl = GetterOpImpl<IndexedPropertyGetterCallback, uint32_t>;
  } else {
    JS::Value symbolPropGetterCallback =
      GetInstanceSlot(obj,
        SlotTraits<InstanceSlots, Name>::PropGetterCallback1);
    if (JSID_IS_SYMBOL(id)) {
      // Symbols can only be intercepted with a callback accepting Names.
      if (symbolPropGetterCallback.isUndefined()) {
        return false;
      }
      impl = GetterOpImpl<GenericNamedPropertyGetterCallback, Name>;
    } else if (symbolPropGetterCallback.isUndefined()) {
      impl = GetterOpImpl<NamedPropertyGetterCallback, String>;
    } else {
      impl = GetterOpImpl<GenericNamedPropertyGetterCallback, Name>;
    }
  }
  assert(impl);
  return impl(cx, obj, id, vp);
}

template<typename CallbackType, typename N>
static bool ResolveOpImpl_Getter(JSContext* cx, JS::HandleObject obj,
                                 JS::HandleId id, bool* resolved) {
  PREPARE_CALLBACK(Getter)

  *resolved = false;

  if (callback) {
    JS::RootedValue value(cx);
    PropertyCallbackInfo info(data, thisObj, thisObj);
    PropCallbackTraits<CallbackType, N>::doCall(isolate, callback, id,
                                                info, &value);
    if (!value.isUndefined()) {
      if (!JS_WrapValue(cx, &value) ||
          !JS_DefinePropertyById(cx, obj, id, value, JSPROP_RESOLVING)) {
        return false;
      }
      *resolved = true;
    }
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

static bool ResolveOp_Getter(JSContext* cx, JS::HandleObject obj,
                             JS::HandleId id, bool* resolved) {
  JS::Value ctor = GetInstanceSlot(obj, size_t(InstanceSlots::ConstructorSlot));
  if (ctor.isUndefined()) {
    // The resolve hook is being called before NewInstance() has finished creating the
    // object, so ignore this call.
    *resolved = false;
    return true;
  }

  JSResolveOp impl = nullptr;
  if (JSID_IS_INT(id)) {
    impl = ResolveOpImpl_Getter<IndexedPropertyGetterCallback, uint32_t>;
  } else {
    JS::Value symbolPropGetterCallback =
      GetInstanceSlot(obj,
        SlotTraits<InstanceSlots, Name>::PropGetterCallback1);
    if (JSID_IS_SYMBOL(id)) {
      // Symbols can only be intercepted with a callback accepting Names.
      if (symbolPropGetterCallback.isUndefined()) {
        return false;
      }
      impl = ResolveOpImpl_Getter<GenericNamedPropertyGetterCallback, Name>;
    } else if (symbolPropGetterCallback.isUndefined()) {
      impl = ResolveOpImpl_Getter<NamedPropertyGetterCallback, String>;
    } else {
      impl = ResolveOpImpl_Getter<GenericNamedPropertyGetterCallback, Name>;
    }
  }
  assert(impl);
  return impl(cx, obj, id, resolved);
}

template<typename CallbackType, typename N>
static bool SetterOpImpl(JSContext* cx, JS::HandleObject obj,
                         JS::HandleId id, JS::MutableHandleValue vp,
                         JS::ObjectOpResult& result) {
  PREPARE_CALLBACK(Setter)

  if (callback) {
    PropertyCallbackInfo info(data, thisObj, thisObj);
    PropCallbackTraits<CallbackType, N>::doCall(isolate, callback, id,
                                                info, vp);
    if (!info.GetReturnValue().Get()) {
      result.failCantSetInterposed();
    } else {
      result.succeed();
    }
  } else {
    result.failCantSetInterposed();
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

static bool SetterOp(JSContext* cx, JS::HandleObject obj,
                     JS::HandleId id, JS::MutableHandleValue vp,
                     JS::ObjectOpResult& result) {
  JSSetterOp impl = nullptr;
  if (JSID_IS_INT(id)) {
    impl = SetterOpImpl<IndexedPropertySetterCallback, uint32_t>;
  } else {
    JS::Value symbolPropSetterCallback =
      GetInstanceSlot(obj,
        SlotTraits<InstanceSlots, Name>::PropSetterCallback1);
    if (JSID_IS_SYMBOL(id)) {
      // Symbols can only be intercepted with a callback accepting Names.
      if (symbolPropSetterCallback.isUndefined()) {
        return false;
      }
      impl = SetterOpImpl<GenericNamedPropertySetterCallback, Name>;
    } else if (symbolPropSetterCallback.isUndefined()) {
      impl = SetterOpImpl<NamedPropertySetterCallback, String>;
    } else {
      impl = SetterOpImpl<GenericNamedPropertySetterCallback, Name>;
    }
  }
  assert(impl);
  return impl(cx, obj, id, vp, result);
}

template<typename CallbackType, typename N>
static bool GetOwnPropertyOpImpl(JSContext* cx, JS::HandleObject obj,
                                 JS::HandleId id,
                                 JS::MutableHandle<JS::PropertyDescriptor> desc) {
  PREPARE_CALLBACK(Query)

  desc.value().set(JS::UndefinedValue());
  desc.setGetter(nullptr);
  desc.setSetter(nullptr);
  desc.setAttributes(JSPROP_ENUMERATE);

  if (callback) {
    PropertyCallbackInfo info(data, thisObj, thisObj);
    PropCallbackTraits<CallbackType, N>::doCall(isolate, callback, id,
                                                info);

    if (auto rval = info.GetReturnValue().Get()) {
      auto pa = static_cast<PropertyAttribute>(rval->ToInt32()->Value());
      desc.setAttributes(internal::AttrsToFlags(pa));
      // Only set the object when a callback has successfully finished, to signal
      // the success of the property lookup.
      desc.object().set(obj);
    }
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

// This function is used as a hook when a V8 query hook is being used.
static bool GetOwnPropertyOp(JSContext* cx, JS::HandleObject obj,
                             JS::HandleId id,
                             JS::MutableHandle<JS::PropertyDescriptor> desc) {
  js::GetOwnPropertyOp impl = nullptr;
  if (JSID_IS_INT(id)) {
    impl = GetOwnPropertyOpImpl<IndexedPropertyQueryCallback, uint32_t>;
  } else {
    JS::Value symbolPropQueryCallback =
      GetInstanceSlot(obj,
        SlotTraits<InstanceSlots, Name>::PropQueryCallback1);
    if (JSID_IS_SYMBOL(id)) {
      // Symbols can only be intercepted with a callback accepting Names.
      if (symbolPropQueryCallback.isUndefined()) {
        return false;
      }
      impl = GetOwnPropertyOpImpl<GenericNamedPropertyQueryCallback, Name>;
    } else if (symbolPropQueryCallback.isUndefined()) {
      impl = GetOwnPropertyOpImpl<NamedPropertyQueryCallback, String>;
    } else {
      impl = GetOwnPropertyOpImpl<GenericNamedPropertyQueryCallback, Name>;
    }
  }
  assert(impl);
  return impl(cx, obj, id, desc);
}

static bool HasPropertyOp_Query(JSContext* cx, JS::HandleObject obj,
                                JS::HandleId id, bool* found);
struct AutoResetHasPropHook {
  AutoResetHasPropHook(JS::HandleObject obj)
    : clasp_(ObjectTemplate::InstanceClass::FromObject(obj)) {
    clasp_->AddRef();
    clasp_->ModifyObjectOps().hasProperty = nullptr;
  }
  ~AutoResetHasPropHook() {
    clasp_->ModifyObjectOps().hasProperty = HasPropertyOp_Query;
    clasp_->Release();
  }
 private:
  ObjectTemplate::InstanceClass* clasp_;
};

template<typename CallbackType, typename N>
static bool HasPropertyOpImpl_Query(JSContext* cx, JS::HandleObject obj,
                                    JS::HandleId id, bool* found) {
  PREPARE_CALLBACK(Query)

  *found = false;

  if (callback) {
    PropertyCallbackInfo info(data, thisObj, thisObj);
    PropCallbackTraits<CallbackType, N>::doCall(isolate, callback, id,
                                                info);

    if (auto rval = info.GetReturnValue().Get()) {
      *found = true;
    } else {
      // If the handler doesn't respond to the callback, we need to initiate a lookup
      // ignoring this hook to search the prototype chain, etc.
      AutoResetHasPropHook ignoreHook(obj);
      return JS_HasPropertyById(cx, obj, id, found);
    }
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

static bool HasPropertyOp_Query(JSContext* cx, JS::HandleObject obj,
                                JS::HandleId id, bool* found) {
  js::HasPropertyOp impl = nullptr;
  if (JSID_IS_INT(id)) {
    impl = HasPropertyOpImpl_Query<IndexedPropertyQueryCallback, uint32_t>;
  } else {
    JS::Value symbolPropQueryCallback =
      GetInstanceSlot(obj,
        SlotTraits<InstanceSlots, Name>::PropQueryCallback1);
    if (JSID_IS_SYMBOL(id)) {
      // Symbols can only be intercepted with a callback accepting Names.
      if (symbolPropQueryCallback.isUndefined()) {
        return false;
      }
      impl = HasPropertyOpImpl_Query<GenericNamedPropertyQueryCallback, Name>;
    } else if (symbolPropQueryCallback.isUndefined()) {
      impl = HasPropertyOpImpl_Query<NamedPropertyQueryCallback, String>;
    } else {
      impl = HasPropertyOpImpl_Query<GenericNamedPropertyQueryCallback, Name>;
    }
  }
  assert(impl);
  return impl(cx, obj, id, found);
}

template<typename CallbackType, typename N>
static bool DeleterOpImpl(JSContext* cx, JS::HandleObject obj,
                          JS::HandleId id, JS::ObjectOpResult& result) {
  PREPARE_CALLBACK(Deleter)

  if (callback) {
    PropertyCallbackInfo info(data, thisObj, thisObj);
    PropCallbackTraits<CallbackType, N>::doCall(isolate, callback, id, info);
    if (!info.GetReturnValue().Get()) {
      return false;
    } else {
      assert(info.GetReturnValue().Get()->IsBoolean());
      if (info.GetReturnValue().Get()->IsTrue()) {
        result.succeed();
      } else {
        result.failCantDelete();
      }
    }
  } else {
    result.failCantDelete();
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

static bool DeleterOp(JSContext* cx, JS::HandleObject obj,
                      JS::HandleId id, JS::ObjectOpResult& result) {
  JSDeletePropertyOp impl = nullptr;
  if (JSID_IS_INT(id)) {
    impl = DeleterOpImpl<IndexedPropertyDeleterCallback, uint32_t>;
  } else {
    JS::Value symbolPropDeleterCallback =
      GetInstanceSlot(obj,
        SlotTraits<InstanceSlots, Name>::PropDeleterCallback1);
    if (JSID_IS_SYMBOL(id)) {
      // Symbols can only be intercepted with a callback accepting Names.
      if (symbolPropDeleterCallback.isUndefined()) {
        return false;
      }
      impl = DeleterOpImpl<GenericNamedPropertyDeleterCallback, Name>;
    } else if (symbolPropDeleterCallback.isUndefined()) {
      impl = DeleterOpImpl<NamedPropertyDeleterCallback, String>;
    } else {
      impl = DeleterOpImpl<GenericNamedPropertyDeleterCallback, Name>;
    }
  }
  assert(impl);
  return impl(cx, obj, id, result);
}

template<typename CallbackType, typename N>
static bool EnumeratorOpImpl(JSContext* cx, JS::HandleObject obj,
                             JS::AutoIdVector& properties, bool enumerableOnly) {
  PREPARE_CALLBACK(Enumerator)

  if (callback) {
    PropertyCallbackInfo info(data, thisObj, thisObj);
    PropCallbackTraits<CallbackType, N>::doCall(isolate, callback, info);
    if (auto rval = info.GetReturnValue().Get()) {
      auto arr = Array::Cast(rval);
      for (uint32_t i = 0; i < arr->Length(); ++i) {
        Local<Value> elem = arr->Get(i);
        if (elem.IsEmpty()) {
          // TODO: Maybe we should do something better than just skipping...
          continue;
        }
        if (elem->IsInt32()) {
          properties.append(INT_TO_JSID(elem->ToInt32()->Value()));
        } else if (elem->IsString()) {
          JS::RootedString unatomized(cx, GetString(elem));
          JS::RootedString str(cx, JS_AtomizeAndPinJSString(cx, unatomized));
          if (!str) {
            // TODO: Maybe we should do something better than just skipping...
            continue;
          }
          properties.append(INTERNED_STRING_TO_JSID(cx, str));
        }
        // TODO: Handle symbols here when we implement v8::Symbol
      }
    }
  }

  return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
}

static bool EnumeratorOp(JSContext* cx, JS::HandleObject obj,
                         JS::AutoIdVector& properties, bool enumerableOnly) {
  JS::Value indexedPropEnumeratorCallback =
    GetInstanceSlot(obj,
      SlotTraits<InstanceSlots, uint32_t>::PropEnumeratorCallback1);
  if (!indexedPropEnumeratorCallback.isUndefined()) {
    if (!EnumeratorOpImpl<IndexedPropertyEnumeratorCallback, uint32_t>
          (cx, obj, properties, enumerableOnly)) {
      return false;
    }
  }
  JS::Value namedPropEnumeratorCallback =
    GetInstanceSlot(obj,
      SlotTraits<InstanceSlots, String>::PropEnumeratorCallback1);
  if (!namedPropEnumeratorCallback.isUndefined()) {
    if (!EnumeratorOpImpl<NamedPropertyEnumeratorCallback, String>
          (cx, obj, properties, enumerableOnly)) {
      return false;
    }
  }
  JS::Value symbolPropEnumeratorCallback =
    GetInstanceSlot(obj,
      SlotTraits<InstanceSlots, Name>::PropEnumeratorCallback1);
  if (!symbolPropEnumeratorCallback.isUndefined()) {
    if (!EnumeratorOpImpl<GenericNamedPropertyEnumeratorCallback, Name>
          (cx, obj, properties, enumerableOnly)) {
      return false;
    }
  }
  return true;
}

#undef PREPARE_CALLBACK

static bool CallOp(JSContext* cx, unsigned argc, JS::Value* vp) {
  Isolate* isolate = Isolate::GetCurrent();
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  JS::RootedValue callAsFunctionHandler(cx,
    GetInstanceSlot(callee, size_t(InstanceSlots::CallCallbackSlot)));
  if (callAsFunctionHandler.isUndefined() ||
      !callAsFunctionHandler.isObject()) {
    return false;
  }

  if (args.isConstructing()) {
    JS::RootedObject rval(cx);
    auto result = JS::Construct(cx, callAsFunctionHandler,
                                JS::HandleValueArray(args), &rval);
    if (result) {
      args.rval().setObject(*rval);
    }
    return result;
  } else {
    JS::RootedValue thisv(cx, args.computeThis(cx));
    return JS::Call(cx, thisv, callAsFunctionHandler,
                    JS::HandleValueArray(args), args.rval());
  }
}
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
    constructor = FunctionTemplate::New(isolate, cx);
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
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(*templ)));

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

  return NewInstance(prototype, NormalObject);
}

Local<Object> ObjectTemplate::NewInstance(Local<Object> prototype,
                                          ObjectType objectType) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  // XXXbz V8 uses a new HandleScope here.  I _think_ that's a performance
  // optimization....

  assert(!prototype.IsEmpty());
  JS::RootedObject protoObj(cx, &GetValue(prototype)->toObject());

  InstanceClass* instanceClass = GetInstanceClass(objectType);
  assert(instanceClass);

  // XXXbz this needs more fleshing out to deal with the whole business of
  // indexed/named getters/setters and so forth.  That will involve creating
  // proxy objects if we've had any of the indexed/named callbacks defined on
  // us.  For now, let's just go ahead and do the simple thing, since we don't
  // implement those parts of the API yet..
  JS::RootedObject instanceObj(cx);
  if (objectType == NormalObject) {
    instanceObj = JS_NewObjectWithGivenProto(cx, instanceClass, protoObj);
  } else if (objectType == GlobalObject) {
    JS::CompartmentOptions options;
    options.behaviors().setVersion(JSVERSION_LATEST);
    instanceObj = JS_NewGlobalObject(cx, instanceClass, nullptr,
                                     JS::FireOnNewGlobalHook, options);
    if (!instanceObj) {
      return Local<Object>();
    }

    JSAutoCompartment ac(cx, instanceObj);

    if (!JS_InitStandardClasses(cx, instanceObj) ||
        !JS_WrapObject(cx, &protoObj) ||
        !JS_SplicePrototype(cx, instanceObj, protoObj)) {
      return Local<Object>();
    }
  } else {
    assert(false && "Unexpected object type");
  }
  if (!instanceObj) {
    return Local<Object>();
  }

  // If we are creating a global object, the newly created global will be in
  // a different compartment than all of the template information we have at
  // this point, so we need to enter the global's compartment and wrap
  // everything into that compartment before attaching them to the global.
  mozilla::Maybe<JSAutoCompartment> maybeAC;
  if (objectType == GlobalObject) {
    maybeAC.emplace(cx, instanceObj);
  }

  JS::Value instanceVal = JS::ObjectValue(*instanceObj);
  Local<Object> instanceLocal =
    internal::Local<Object>::New(isolate, instanceVal);

  // Copy the bits set on us and the things we inherit from via Template::Set.
  // We don't just GetConstructor()->InstallInstanceProperties() here because
  // it's possible we're not our constructor's instance template.
  MaybeLocal<FunctionTemplate> functionTemplate = GetConstructor()->GetParent();
  if (!functionTemplate.IsEmpty() &&
      !functionTemplate.ToLocalChecked()->InstallInstanceProperties(instanceLocal)) {
    return Local<Object>();
  }

  // JS_CopyPropertiesFrom wraps the property values into the current
  // compartment for us.
  if (!JS_CopyPropertiesFrom(cx, instanceObj, obj)) {
    return Local<Object>();
  }

  SetInstanceSlot(instanceObj, size_t(InstanceSlots::InstanceClassSlot),
                  JS::PrivateValue(instanceClass));
  JS::RootedValue ctor(cx, JS::ObjectValue(*GetObject(*GetConstructor())));
  if (!JS_WrapValue(cx, &ctor)) {
    return Local<Object>();
  }
  // Note: It is important for this to be called after the call to JS_CopyPropertiesFrom
  // above, since that call may invoke the resolve hook, and that hook checks this slot
  // for undefined to signal that it should not run yet.
  SetInstanceSlot(instanceObj, size_t(InstanceSlots::ConstructorSlot), ctor);

  JS::RootedValue callCallback(cx,
      js::GetReservedSlot(obj, size_t(TemplateSlots::CallCallbackSlot)));
  if (!callCallback.isUndefined()) {
    SetInstanceSlot(instanceObj, size_t(InstanceSlots::CallCallbackSlot),
                    callCallback);
  }

  CopyTemplateCallbackPropsOnInstance<String>(cx, obj, instanceObj);
  CopyTemplateCallbackPropsOnInstance<Name>(cx, obj, instanceObj);
  CopyTemplateCallbackPropsOnInstance<uint32_t>(cx, obj, instanceObj);

  // Ensure that we keep our instance class, if any, alive as long as the
  // instance is alive.
  instanceClass->AddRef();

  return instanceLocal;
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
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  internal::SetAccessor(cx, obj, name, getter, setter, data,
                        settings, attribute, signature);
}

// TODO SetAccessCheckCallbacks: Can this just be a no-op?

Handle<String> ObjectTemplate::GetClassName() {
  return GetConstructor()->GetClassName();
}

ObjectTemplate::InstanceClass* ObjectTemplate::GetInstanceClass(ObjectType objectType) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  size_t slotIndex = (objectType == GlobalObject) ?
                       size_t(TemplateSlots::InstanceClassSlot) :
                       size_t(TemplateSlots::GlobalInstanceClassSlot);
  JS::Value classValue = js::GetReservedSlot(obj, slotIndex);
  if (!classValue.isUndefined()) {
    return static_cast<InstanceClass*>(classValue.toPrivate());
  }

  InstanceClass* instanceClass = new InstanceClass();
  if (!instanceClass) {
    MOZ_CRASH();
  }

  uint32_t flags = InstanceClass::instantiatedFromTemplate | JSCLASS_FOREGROUND_FINALIZE;
  Local<String> name = GetClassName();
  if (name.IsEmpty()) {
    instanceClass->name = (objectType == GlobalObject) ? "GlobalObject" : "Object";
  } else {
    JS::RootedString str(cx, GetValue(name)->toString());
    instanceClass->name = JS_EncodeStringToUTF8(cx, str);
    flags |= InstanceClass::nameAllocated;
  }

  if (HasGetterProp<Name>(obj) ||
      HasGetterProp<String>(obj) ||
      HasGetterProp<uint32_t>(obj)) {
    instanceClass->ModifyClassOps().getProperty = GetterOp;
    // A getProperty hook doesn't cover things such as HasOwnProperty, so we need
    // to set this additional hook too.  This is technically not correct since the
    // way the resolve hook works is by defining the properties that it resolves on
    // the object, but it seems like we can get away with this for now...
    instanceClass->ModifyClassOps().resolve = ResolveOp_Getter;
  }

  if (HasSetterProp<Name>(obj) ||
      HasSetterProp<String>(obj) ||
      HasSetterProp<uint32_t>(obj)) {
    instanceClass->ModifyClassOps().setProperty = SetterOp;
  }

  if (HasQueryProp<Name>(obj) ||
      HasQueryProp<String>(obj) ||
      HasQueryProp<uint32_t>(obj)) {
    // This can potentially overwrite the hook set for getters above.
    instanceClass->ModifyObjectOps().getOwnPropertyDescriptor = GetOwnPropertyOp;
    instanceClass->ModifyObjectOps().hasProperty = HasPropertyOp_Query;
  }

  if (HasDeleterProp<Name>(obj) ||
      HasDeleterProp<String>(obj) ||
      HasDeleterProp<uint32_t>(obj)) {
    instanceClass->ModifyClassOps().delProperty = DeleterOp;
  }

  if (HasEnumeratorProp<Name>(obj) ||
      HasEnumeratorProp<String>(obj) ||
      HasEnumeratorProp<uint32_t>(obj)) {
    instanceClass->ModifyObjectOps().enumerate = EnumeratorOp;
  }

  JS::Value callAsFunctionHandler =
    js::GetReservedSlot(obj, size_t(TemplateSlots::CallCallbackSlot));
  if (!callAsFunctionHandler.isUndefined()) {
    instanceClass->ModifyClassOps().call = CallOp;
    instanceClass->ModifyClassOps().construct = CallOp;
  }

  // We need to allocate twice the number of internal fields since aligned
  // pointers returned through GetAlignedPointerFromInternalField require
  // two internal slots, therefore we allocate two slots for each internal
  // field for simplicity.
  uint32_t internalFieldCount = static_cast<uint32_t>(InternalFieldCount()) * 2;

  auto reservedSlots = internalFieldCount + uint32_t(InstanceSlots::NumSlots);

  if (objectType == GlobalObject) {
    // SpiderMonkey allocates JSCLASS_GLOBAL_APPLICATION_SLOTS (5) reserved slots
    // by default for global objects, so we need to allocate 5 fewer slots here.
    // This also means that any access to these slots must account for this
    // difference between the global and normal objects.
    flags |= JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(reservedSlots - JSCLASS_GLOBAL_APPLICATION_SLOTS);
    instanceClass->ModifyClassOps().trace = JS_GlobalObjectTraceHook;
  } else {
    flags |= JSCLASS_HAS_RESERVED_SLOTS(reservedSlots);
  }
  instanceClass->flags = flags;
  instanceClass->ModifyClassOps().finalize = InstanceTemplateFinalize;

  instanceClass->AddRef(); // Will be released in obj's finalizer.

  js::SetReservedSlot(obj, slotIndex, JS::PrivateValue(instanceClass));
  return instanceClass;
}

int ObjectTemplate::InternalFieldCount() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  JS::Value internalFieldCountVal = js::GetReservedSlot(obj,
                                                        size_t(TemplateSlots::InternalFieldCountSlot));
  int internalFieldCount = 0;
  if (!internalFieldCountVal.isUndefined()) {
    internalFieldCount = internalFieldCountVal.toInt32();
  }
  return internalFieldCount;
}

void ObjectTemplate::SetInternalFieldCount(int value) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  js::SetReservedSlot(obj, size_t(TemplateSlots::InternalFieldCountSlot),
                      JS::Int32Value(value));
}

Local<FunctionTemplate> ObjectTemplate::GetConstructor() {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  JS::Value ctorVal =
    js::GetReservedSlot(obj, size_t(TemplateSlots::ConstructorSlot));
  return internal::Local<FunctionTemplate>::NewTemplate(isolate, ctorVal);
}

bool ObjectTemplate::IsObjectFromTemplate(Local<Object> object) {
  assert(!object.IsEmpty());
  return !!(JS_GetClass(GetObject(object))->flags & InstanceClass::instantiatedFromTemplate);
}

bool ObjectTemplate::ObjectHasNativeCallHandler(Local<Object> object) {
  if (!IsObjectFromTemplate(object)) {
    return false;
  }
  auto clasp = JS_GetClass(GetObject(object));
  assert(clasp->cOps->call == clasp->cOps->construct);
  return !!clasp->cOps->call;
}

Local<FunctionTemplate> ObjectTemplate::GetObjectTemplateConstructor(Local<Object> object) {
  assert(IsObjectFromTemplate(object));
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, object);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(*object)));
  assert(obj);

  JS::Value ctorVal =
    GetInstanceSlot(obj, size_t(InstanceSlots::ConstructorSlot));
  assert(ctorVal.isObject());
  return internal::Local<FunctionTemplate>::NewTemplate(isolate, ctorVal);
}

void ObjectTemplate::SetNamedPropertyHandler(NamedPropertyGetterCallback getter,
                                             NamedPropertySetterCallback setter,
                                             NamedPropertyQueryCallback query,
                                             NamedPropertyDeleterCallback deleter,
                                             NamedPropertyEnumeratorCallback enumerator,
                                             Local<Value> data) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  ::SetHandler<String>(cx, obj, getter, setter, query, deleter, enumerator, data);
}

void ObjectTemplate::SetHandler(const NamedPropertyHandlerConfiguration& config) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  ::SetHandler<Name>(cx, obj, config.getter, config.setter, config.query,
                     config.deleter, config.enumerator, config.data);
}

void ObjectTemplate::SetIndexedPropertyHandler(IndexedPropertyGetterCallback getter,
                                               IndexedPropertySetterCallback setter,
                                               IndexedPropertyQueryCallback query,
                                               IndexedPropertyDeleterCallback deleter,
                                               IndexedPropertyEnumeratorCallback enumerator,
                                               Handle<Value> data) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  ::SetHandler<uint32_t>(cx, obj, getter, setter, query, deleter, enumerator, data);
}

void ObjectTemplate::SetHandler(const IndexedPropertyHandlerConfiguration& config) {
  SetIndexedPropertyHandler(config.getter,
                            config.setter,
                            config.query,
                            config.deleter,
                            config.enumerator,
                            config.data);
}

void ObjectTemplate::SetCallAsFunctionHandler(FunctionCallback callback,
                                              Local<Value> data) {
  Isolate* isolate = Isolate::GetCurrent();
  if (data.IsEmpty()) {
    data = Undefined(isolate);
  }

  MaybeLocal<Function> func = Function::New(isolate->GetCurrentContext(), callback, data, 0,
                                            Local<FunctionTemplate>(),
                                            Local<String>());
  if (func.IsEmpty()) {
    // TODO: Do something better here.
    return;
  }

  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));
  assert(obj);
  assert(JS_GetClass(obj) == &objectTemplateClass);

  JS::RootedObject funcObj(cx, GetObject(func.ToLocalChecked()));
  assert(funcObj);

  js::SetReservedSlot(obj, size_t(TemplateSlots::CallCallbackSlot),
                      JS::ObjectValue(*funcObj));
}
} // namespace v8
