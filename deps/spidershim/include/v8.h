// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

// Stops windows.h from including winsock.h (conflicting with winsock2.h).
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <memory>
#include <vector>

#include "v8-version.h"
#include "v8config.h"

// TODO: Do the dllexport/import dance on Windows.
#define V8_EXPORT

#define TYPE_CHECK(T, S)                                  \
  while (false) {                                         \
    *(static_cast<T* volatile*>(0)) = static_cast<S*>(0); \
  }

// TODO: The following definitions are stolen from chakracommon.h to get this
// header to compile.  We need to replace them with SM based stuff.
typedef void* JsRef;
typedef JsRef JsValueRef;

class AutoJSAPI;
struct JSRuntime;
struct JSContext;
class JSObject;
class JSScript;
class V8Engine;
struct JSClass;

namespace JS {
class Symbol;
}

namespace v8 {

class AccessorSignature;
class Array;
class ArrayBuffer;
class Value;
class External;
class Primitive;
class Boolean;
class BooleanObject;
class Context;
class CpuProfiler;
class EscapableHandleScope;
class Function;
class FunctionTemplate;
class HeapProfiler;
class HeapSpaceStatistics;
class HeapObjectStatistics;
class HeapCodeStatistics;
class HeapStatistics;
class Int32;
class Integer;
class Isolate;
class Message;
class Name;
class Number;
class NumberObject;
class Object;
class ObjectTemplate;
class Platform;
class Private;
class ResourceConstraints;
class RegExp;
class Promise;
class PropertyDescriptor;
class Proxy;
class Script;
class ScriptCompiler;
class SharedArrayBuffer;
class Signature;
class StartupData;
class StackFrame;
class StackTrace;
class String;
class StringObject;
class Template;
class TryCatch;
class Uint32;
class UnboundScript;
template <class T>
class Eternal;
template <class T>
class Local;
template <class T>
class Maybe;
template <class T>
class MaybeLocal;
template <class T>
class NonCopyablePersistentTraits;
template <class T>
class PersistentBase;
template <class T, class U, class V>
class PersistentValueMapBase;
template <class T, class U>
class PersistentValueVector;
template <class T, class M = NonCopyablePersistentTraits<T> >
class Persistent;
template <typename T>
class FunctionCallbackInfo;
template <typename T>
class PropertyCallbackInfo;

class JitCodeEvent;
class RetainedObjectInfo;
struct ExternalArrayData;

namespace internal {
class FunctionCallback;
class RootStore;
template <class T>
class Local;
template <typename T>
struct ExternalStringFinalizerBase;
struct ExternalStringFinalizer;
struct ExternalOneByteStringFinalizer;
struct SignatureChecker;
}

enum PropertyAttribute {
  None = 0,
  ReadOnly = 1 << 0,
  DontEnum = 1 << 1,
  DontDelete = 1 << 2,
};

enum ExternalArrayType {
  kExternalInt8Array = 1,
  kExternalUint8Array,
  kExternalInt16Array,
  kExternalUint16Array,
  kExternalInt32Array,
  kExternalUint32Array,
  kExternalFloat32Array,
  kExternalFloat64Array,
  kExternalUint8ClampedArray,

  // Legacy constant names
  kExternalByteArray = kExternalInt8Array,
  kExternalUnsignedByteArray = kExternalUint8Array,
  kExternalShortArray = kExternalInt16Array,
  kExternalUnsignedShortArray = kExternalUint16Array,
  kExternalIntArray = kExternalInt32Array,
  kExternalUnsignedIntArray = kExternalUint32Array,
  kExternalFloatArray = kExternalFloat32Array,
  kExternalDoubleArray = kExternalFloat64Array,
  kExternalPixelArray = kExternalUint8ClampedArray
};

enum AccessControl {
  DEFAULT = 0,
  ALL_CAN_READ = 1,
  ALL_CAN_WRITE = 1 << 1,
  PROHIBITS_OVERWRITING = 1 << 2,
};

 /**
 * Property filter bits. They can be or'ed to build a composite filter.
 */
enum PropertyFilter {
  ALL_PROPERTIES = 0,
  ONLY_WRITABLE = 1,
  ONLY_ENUMERABLE = 2,
  ONLY_CONFIGURABLE = 4,
  SKIP_STRINGS = 8,
  SKIP_SYMBOLS = 16
};

/**
 * Keys/Properties filter enums:
 *
 * KeyCollectionMode limits the range of collected properties. kOwnOnly limits
 * the collected properties to the given Object only. kIncludesPrototypes will
 * include all keys of the objects's prototype chain as well.
 */
enum class KeyCollectionMode { kOwnOnly, kIncludePrototypes };

/**
 * kIncludesIndices allows for integer indices to be collected, while
 * kSkipIndices will exclude integer indicies from being collected.
 */
enum class IndexFilter { kIncludeIndices, kSkipIndices };

/**
 * Option flags passed to the SetRAILMode function.
 * See documentation https://developers.google.com/web/tools/chrome-devtools/
 * profile/evaluate-performance/rail
 */
enum RAILMode {
  // Response performance mode: In this mode very low virtual machine latency
  // is provided. V8 will try to avoid JavaScript execution interruptions.
  // Throughput may be throttled.
  PERFORMANCE_RESPONSE,
  // Animation performance mode: In this mode low virtual machine latency is
  // provided. V8 will try to avoid as many JavaScript execution interruptions
  // as possible. Throughput may be throttled. This is the default mode.
  PERFORMANCE_ANIMATION,
  // Idle performance mode: The embedder is idle. V8 can complete deferred work
  // in this mode.
  PERFORMANCE_IDLE,
  // Load performance mode: In this mode high throughput is provided. V8 may
  // turn off latency optimizations.
  PERFORMANCE_LOAD
};

enum JitCodeEventOptions {
  kJitCodeEventDefault = 0,
  kJitCodeEventEnumExisting = 1,
};

typedef void (*AccessorGetterCallback)(Local<String> property,
                                       const PropertyCallbackInfo<Value>& info);
typedef void (*AccessorNameGetterCallback)(
    Local<Name> property, const PropertyCallbackInfo<Value>& info);

typedef void (*AccessorSetterCallback)(Local<String> property,
                                       Local<Value> value,
                                       const PropertyCallbackInfo<void>& info);
typedef void (*AccessorNameSetterCallback)(
    Local<Name> property, Local<Value> value,
    const PropertyCallbackInfo<void>& info);

typedef void (*NamedPropertyGetterCallback)(
    Local<String> property, const PropertyCallbackInfo<Value>& info);
typedef void (*NamedPropertySetterCallback)(
    Local<String> property, Local<Value> value,
    const PropertyCallbackInfo<Value>& info);
typedef void (*NamedPropertyQueryCallback)(
    Local<String> property, const PropertyCallbackInfo<Integer>& info);
typedef void (*NamedPropertyDeleterCallback)(
    Local<String> property, const PropertyCallbackInfo<Boolean>& info);
typedef void (*NamedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);

typedef void (*GenericNamedPropertyGetterCallback)(
    Local<Name> property, const PropertyCallbackInfo<Value>& info);
typedef void (*GenericNamedPropertySetterCallback)(
    Local<Name> property, Local<Value> value,
    const PropertyCallbackInfo<Value>& info);
typedef void (*GenericNamedPropertyQueryCallback)(
    Local<Name> property, const PropertyCallbackInfo<Integer>& info);
typedef void (*GenericNamedPropertyDeleterCallback)(
    Local<Name> property, const PropertyCallbackInfo<Boolean>& info);
typedef void (*GenericNamedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);

typedef void (*IndexedPropertyGetterCallback)(
    uint32_t index, const PropertyCallbackInfo<Value>& info);
typedef void (*IndexedPropertySetterCallback)(
    uint32_t index, Local<Value> value,
    const PropertyCallbackInfo<Value>& info);
typedef void (*IndexedPropertyQueryCallback)(
    uint32_t index, const PropertyCallbackInfo<Integer>& info);
typedef void (*IndexedPropertyDeleterCallback)(
    uint32_t index, const PropertyCallbackInfo<Boolean>& info);
typedef void (*IndexedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);

typedef bool (*EntropySource)(unsigned char* buffer, size_t length);
typedef void (*FatalErrorCallback)(const char* location, const char* message);
typedef void (*OOMErrorCallback)(const char* location, bool is_heap_oom);
typedef void (*JitCodeEventHandler)(const JitCodeEvent* event);

template <class T>
class Local {
 public:
  V8_INLINE Local() : val_(0) {}

  template <class S>
  V8_INLINE Local(Local<S> that) : val_(reinterpret_cast<T*>(*that)) {
    TYPE_CHECK(T, S);
  }

  V8_INLINE bool IsEmpty() const { return val_ == 0; }
  V8_INLINE void Clear() { val_ = 0; }
  V8_INLINE T* operator->() const { return val_; }
  V8_INLINE T* operator*() const { return val_; }

  template <class S>
  V8_INLINE bool operator==(const Local<S>& that) const;

  template <class S>
  V8_INLINE bool operator==(const PersistentBase<S>& that) const;

  template <class S>
  V8_INLINE bool operator!=(const Local<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE bool operator!=(const PersistentBase<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE static Local<T> Cast(Local<S> that) {
    return Local<T>(T::Cast(*that));
  }

  template <class S>
  V8_INLINE Local<S> As() const {
    return Local<S>::Cast(*this);
  }

  V8_INLINE static Local<T> New(Isolate* isolate, Local<T> that);
  V8_INLINE static Local<T> New(Isolate* isolate,
                                const PersistentBase<T>& that);

 private:
  friend struct AcessorExternalDataType;
  friend class AccessorSignature;
  friend class Array;
  friend class ArrayBuffer;
  friend class ArrayBufferView;
  friend class Boolean;
  friend class BooleanObject;
  friend class Context;
  friend class Date;
  friend class Debug;
  friend class EscapableHandleScope;
  friend class External;
  friend class Function;
  friend struct FunctionCallbackData;
  friend class FunctionTemplate;
  friend struct FunctionTemplateData;
  friend class Integer;
  friend class Isolate;
  friend class Number;
  friend class NumberObject;
  friend class Object;
  friend struct ObjectData;
  friend class ObjectTemplate;
  friend class Private;
  friend class Proxy;
  friend class Signature;
  friend class Script;
  friend class ScriptCompiler;
  friend class StackFrame;
  friend class StackTrace;
  friend class String;
  friend class StringObject;
  friend class Utils;
  friend class TryCatch;
  friend class UnboundScript;
  friend class Value;
  template <class F>
  friend class FunctionCallbackInfo;
  template <class F>
  friend class MaybeLocal;
  template <class F>
  friend class PersistentBase;
  template <class F, class M>
  friend class Persistent;
  template <class F, class M, class F2>
  friend class PersistentValueMapBase;
  template <class F, class M>
  friend class PersistentValueVector;
  template <class F>
  friend class Eternal;
  template <class F>
  friend class Local;
  template <class F>
  friend class internal::Local;
  friend V8_EXPORT Local<Primitive> Undefined(Isolate* isolate);
  friend V8_EXPORT Local<Primitive> Null(Isolate* isolate);
  friend V8_EXPORT Local<Boolean> True(Isolate* isolate);
  friend V8_EXPORT Local<Boolean> False(Isolate* isolate);

  V8_INLINE Local(T* that) : val_(that) {}
  template <class S>
  V8_INLINE static Local<T> New(Isolate* isolate, S* that) {
    return New(that);
  }
  template <class S>
  V8_INLINE static Local<T> New(Isolate* isolate, S* that,
                                Local<Context> context) {
    return New(that, context);
  }

  V8_INLINE Local(const PersistentBase<T>& that) : val_(that.val_) {}
  template <class S>
  V8_INLINE static Local<T> New(S* that);
  template <class S>
  V8_INLINE static Local<T> New(S* that, Local<Context> context);

  T* val_;
};

// Handle is an alias for Local for historical reasons.
template <class T>
using Handle = Local<T>;

V8_EXPORT Handle<Primitive> Undefined(Isolate* isolate);
V8_EXPORT Handle<Primitive> Null(Isolate* isolate);
V8_EXPORT Handle<Boolean> True(Isolate* isolate);
V8_EXPORT Handle<Boolean> False(Isolate* isolate);
V8_EXPORT bool SetResourceConstraints(ResourceConstraints* constraints);

template <class T>
class MaybeLocal {
 public:
  MaybeLocal() : val_(nullptr) {}
  template <class S>
  MaybeLocal(Local<S> that) : val_(reinterpret_cast<T*>(*that)) {
    TYPE_CHECK(T, S);
  }

  bool IsEmpty() const { return val_ == nullptr; }

  template <class S>
  bool ToLocal(Local<S>* out) const {
    out->val_ = IsEmpty() ? nullptr : this->val_;
    return !IsEmpty();
  }

  // Will crash if the MaybeLocal<> is empty.
  V8_INLINE Local<T> ToLocalChecked();

  template <class S>
  Local<S> FromMaybe(Local<S> default_value) const {
    return IsEmpty() ? default_value : Local<S>(val_);
  }

 private:
  T* val_;
};

static const int kInternalFieldsInWeakCallback = 2;

template <typename T>
class WeakCallbackInfo {
 public:
  typedef void (*Callback)(const WeakCallbackInfo<T>& data);

  WeakCallbackInfo(Isolate* isolate, T* parameter,
                   void* internal_fields[kInternalFieldsInWeakCallback],
                   Callback* callback)
      : isolate_(isolate), parameter_(parameter), callback_(callback) {
    for (int i = 0; i < kInternalFieldsInWeakCallback; ++i) {
      internal_fields_[i] = internal_fields[i];
    }
  }

  V8_INLINE Isolate* GetIsolate() const { return isolate_; }
  V8_INLINE T* GetParameter() const { return parameter_; }
  V8_INLINE void* GetInternalField(int index) const {
    return internal_fields_[index];
  }

  V8_INLINE V8_DEPRECATE_SOON("use indexed version",
                              void* GetInternalField1()) const {
    return internal_fields_[0];
  }
  V8_INLINE V8_DEPRECATE_SOON("use indexed version",
                              void* GetInternalField2()) const {
    return internal_fields_[1];
  }

  bool IsFirstPass() const { return callback_ != nullptr; }
  void SetSecondPassCallback(Callback callback) const { *callback_ = callback; }

 private:
  Isolate* isolate_;
  T* parameter_;
  Callback* callback_;
  void* internal_fields_[kInternalFieldsInWeakCallback];
};

// kParameter will pass a void* parameter back to the callback, kInternalFields
// will pass the first two internal fields back to the callback, kFinalizer
// will pass a void* parameter back, but is invoked before the object is
// actually collected, so it can be resurrected. In the last case, it is not
// possible to request a second pass callback.
enum class WeakCallbackType { kParameter, kInternalFields, kFinalizer };

template <class T>
class PersistentBase {
 public:
  void Reset();

  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const Handle<S>& other);

  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const PersistentBase<S>& other);

  V8_INLINE bool IsEmpty() const { return val_ == NULL; }
  V8_INLINE void Empty() { Reset(); }

  V8_INLINE Local<T> Get(Isolate* isolate) const {
    return Local<T>::New(isolate, *this);
  }

  template <class S>
  V8_INLINE bool operator==(const PersistentBase<S>& that) const;

  template <class S>
  V8_INLINE bool operator==(const Handle<S>& that) const;

  template <class S>
  V8_INLINE bool operator!=(const PersistentBase<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE bool operator!=(const Handle<S>& that) const {
    return !operator==(that);
  }

  template <typename P>
  V8_INLINE void SetWeak(P* parameter,
                         typename WeakCallbackInfo<P>::Callback callback,
                         WeakCallbackType type);

  /**
   * Turns this handle into a weak phantom handle without finalization callback.
   * The handle will be reset automatically when the garbage collector detects
   * that the object is no longer reachable.
   * A related function Isolate::NumberOfPhantomHandleResetsSinceLastCall
   * returns how many phantom handles were reset by the garbage collector.
   */
  V8_INLINE void SetWeak();

  template <typename P>
  P* ClearWeak();

  V8_INLINE void ClearWeak() { ClearWeak<void>(); }
  V8_INLINE void MarkIndependent();
  V8_INLINE V8_DEPRECATED(
      "deprecated optimization, do not use partially dependent groups",
      void MarkPartiallyDependent());
  V8_INLINE bool IsIndependent() const;
  V8_INLINE bool IsNearDeath() const;
  bool IsWeak() const;
  V8_INLINE void SetWrapperClassId(uint16_t class_id);

 private:
  template <class F>
  friend class Global;
  template <class F>
  friend class Local;
  template <class F1, class F2>
  friend class Persistent;
  template <class F, class M, class F2>
  friend class PersistentValueMapBase;
  template <class F, class M>
  friend class PersistentValueVector;
  friend class Object;

  explicit V8_INLINE PersistentBase(T* val) : val_(val) {}
  PersistentBase(const PersistentBase& other) = delete;  // NOLINT
  void operator=(const PersistentBase&) = delete;
  V8_INLINE static T* New(Isolate* isolate, T* that);

  template <typename P, typename Callback>
  void SetWeakCommon(P* parameter, Callback callback);

  T* val_;
};

template <class T>
class NonCopyablePersistentTraits {
 public:
  typedef Persistent<T, NonCopyablePersistentTraits<T> > NonCopyablePersistent;
  static const bool kResetInDestructor = true;  // chakra: changed to true!
  template <class S, class M>
  V8_INLINE static void Copy(const Persistent<S, M>& source,
                             NonCopyablePersistent* dest) {
    Uncompilable<Object>();
  }
  template <class O>
  V8_INLINE static void Uncompilable() {
    TYPE_CHECK(O, Primitive);
  }
};

template <class T>
struct CopyablePersistentTraits {
  typedef Persistent<T, CopyablePersistentTraits<T> > CopyablePersistent;
  static const bool kResetInDestructor = true;
  template <class S, class M>
  static V8_INLINE void Copy(const Persistent<S, M>& source,
                             CopyablePersistent* dest) {
    // do nothing, just allow copy
  }
};

template <class T, class M>
class Persistent : public PersistentBase<T> {
 public:
  V8_INLINE Persistent() : PersistentBase<T>(0) {}

  template <class S>
  V8_INLINE Persistent(Isolate* isolate, Handle<S> that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    TYPE_CHECK(T, S);
  }

  template <class S, class M2>
  V8_INLINE Persistent(Isolate* isolate, const Persistent<S, M2>& that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    TYPE_CHECK(T, S);
  }

  V8_INLINE Persistent(const Persistent& that) : PersistentBase<T>(0) {
    Copy(that);
  }

  template <class S, class M2>
  V8_INLINE Persistent(const Persistent<S, M2>& that) : PersistentBase<T>(0) {
    Copy(that);
  }

  V8_INLINE Persistent& operator=(const Persistent& that) {  // NOLINT
    Copy(that);
    return *this;
  }

  template <class S, class M2>
  V8_INLINE Persistent& operator=(const Persistent<S, M2>& that) {  // NOLINT
    Copy(that);
    return *this;
  }

  V8_INLINE ~Persistent() {
    if (M::kResetInDestructor) this->Reset();
  }

  template <class S>
  V8_INLINE static Persistent<T>& Cast(const Persistent<S>& that) {  // NOLINT
    return reinterpret_cast<Persistent<T>&>(const_cast<Persistent<S>&>(that));
  }

  template <class S>
  V8_INLINE Persistent<S>& As() const {  // NOLINT
    return Persistent<S>::Cast(*this);
  }

 private:
  friend class Object;
  friend struct ObjectData;
  friend class ObjectTemplate;
  friend struct ObjectTemplateData;
  friend struct TemplateData;
  friend struct FunctionCallbackData;
  friend class FunctionTemplate;
  friend struct FunctionTemplateData;
  friend class Utils;
  template <class F>
  friend class Local;
  template <class F>
  friend class ReturnValue;

  template <class S>
  V8_INLINE Persistent(S* that) : PersistentBase<T>(that) {}

  V8_INLINE T* operator*() const { return this->val_; }
  V8_INLINE T* operator->() const { return this->val_; }

  template <class S, class M2>
  V8_INLINE void Copy(const Persistent<S, M2>& that);
};

template <class T>
class Global : public PersistentBase<T> {
 public:
  V8_INLINE Global() : PersistentBase<T>(nullptr) {}

  template <class S>
  V8_INLINE Global(Isolate* isolate, Handle<S> that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    TYPE_CHECK(T, S);
  }

  template <class S>
  V8_INLINE Global(Isolate* isolate, const PersistentBase<S>& that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, that.val_)) {
    TYPE_CHECK(T, S);
  }

  V8_INLINE Global(Global&& other) : PersistentBase<T>(other.val_) {
#if 0
    this._weakWrapper = other._weakWrapper;
#endif
    other.val_ = nullptr;
#if 0
    other._weakWrapper.reset();
#endif
  }

  V8_INLINE ~Global() { this->Reset(); }

  template <class S>
  V8_INLINE Global& operator=(Global<S>&& rhs) {
    TYPE_CHECK(T, S);
    if (this != &rhs) {
      this->Reset();
      this->val_ = rhs.val_;
#if 0
      this->_weakWrapper = rhs._weakWrapper;
#endif
      rhs.val_ = nullptr;
#if 0
      rhs._weakWrapper.reset();
#endif
    }
    return *this;
  }

  Global Pass() { return static_cast<Global&&>(*this); }

  /*
   * For compatibility with Chromium's base::Bind (base::Passed).
   */
  typedef void MoveOnlyTypeForCPP03;

 private:
  Global(const Global&) = delete;
  void operator=(const Global&) = delete;
  V8_INLINE T* operator*() const { return this->val_; }
};

// UniquePersistent is an alias for Global for historical reason.
template <class T>
using UniquePersistent = Global<T>;

template <class T>
class Eternal {
 public:
  Eternal() : val_(nullptr) {}

  template <class S>
  Eternal(Isolate* isolate, Local<S> handle) : val_(nullptr) {
    Set(isolate, handle);
  }

  V8_INLINE Local<T> Get(Isolate* isolate);
  V8_INLINE bool IsEmpty() { return !val_; }
  template <class S>
  V8_INLINE void Set(Isolate* isolate, Local<S> handle);

 private:
  T* val_;
};

// The SpiderMonkey Rooting API works by creating a bunch of Rooted objects on
// the stack and obtain Handles from them to pass around.  These Handles can
// only be obtained from Rooted objects, and are non-copyable, so they can be
// created efficiently as pointers to the Rooted object because they're
// guaranteed to have a lifetime shorter than the corresponding Rooted object.
//
// V8's Rooting API is different in that the object that contains the stack
// roots is HandleScope, from which you can obtain a Local object that can be
// implemented efficiently as a pointer to the root managed by HandleScope.
// HandleScopes are intended to be allocated on the stack and when they are
// destroyed, all of the corresponding roots can be collected.
//
// We bridge these two worlds by making each HandleScope object manage a vector
// of Rooted objects, and it hands out Local objects that are essentially
// pointers to the underlying Rooted object.  Furthermore, to simplify things,
// we represents everything as JS::Values, so for example instead of needing to
// worry about rooting things such as JSString*s, we simply rooted a JS::Value
// and store the string inside it.
class V8_EXPORT HandleScope {
 public:
  HandleScope(Isolate* isolate);
  ~HandleScope();

  static int NumberOfHandles(Isolate* isolate);

  V8_INLINE Isolate* GetIsolate() const;

 private:
  friend class EscapableHandleScope;
  template <class T>
  friend class Local;

  static Value* AddToScope(Value* val);
  static Template* AddToScope(Template* val);
  static Signature* AddToScope(Signature* val);
  static AccessorSignature* AddToScope(AccessorSignature* val);
  static Script* AddToScope(JSScript* script, Local<Context> context);
  static Private* AddToScope(JS::Symbol* priv);
  static Message* AddToScope(Message* msg);
  static Context* AddToScope(Context* context) {
    // Contexts are not currently tracked by HandleScopes.
    return context;
  }
  static StackFrame* AddToScope(StackFrame* frame) {
    // StackFrames are not currently tracked by HandleScopes.
    return frame;
  }
  static StackTrace* AddToScope(StackTrace* trace) {
    // StackTraces are not currently tracked by HandleScopes.
    return trace;
  }
  static UnboundScript* AddToScope(UnboundScript* us) {
    // UnboundScripts are not currently tracked by HandleScopes.
    return us;
  }

  struct Impl;
  Impl* pimpl_;
};

class V8_EXPORT EscapableHandleScope : public HandleScope {
 public:
  EscapableHandleScope(Isolate* isolate) : HandleScope(isolate) {}

  template <class T>
  Local<T> Escape(Handle<T> value) {
    return EscapeToParentHandleScope(value);
  }

 private:
  template <class F>
  friend class ReturnValue;

  template <class T>
  static Local<T> EscapeToParentHandleScope(Local<T> value) {
    auto result = AddToParentScope(*value);
    if (!result) {
      return Local<T>();
    }
    return Local<T>(static_cast<T*>(result));
  }

  template <class T, class U>
  static U* AddToParentScopeImpl(T* val);

  static Value* AddToParentScope(Value* val);
  static Template* AddToParentScope(Template* val);
  static Signature* AddToParentScope(Signature* val);
  static AccessorSignature* AddToParentScope(AccessorSignature* val);
  static Private* AddToParentScope(JS::Symbol* priv);
  static Message* AddToParentScope(Message* msg);
  static Context* AddToParentScope(Context* context) {
    // Contexts are not currently tracked by HandleScopes.
    return context;
  }
  static StackFrame* AddToParentScope(StackFrame* frame) {
    // StackFrames are not currently tracked by HandleScopes.
    return frame;
  }
  static StackTrace* AddToParentScope(StackTrace* trace) {
    // StackTraces are not currently tracked by HandleScopes.
    return trace;
  }
  static UnboundScript* AddToParentScope(UnboundScript* us) {
    // UnboundScripts are not currently tracked by HandleScopes.
    return us;
  }
};

typedef HandleScope SealHandleScope;

class V8_EXPORT Data {
 public:
};

class ScriptOrigin {
 public:
  explicit ScriptOrigin(
      Local<Value> resource_name,
      Local<Integer> resource_line_offset = Local<Integer>(),
      Local<Integer> resource_column_offset = Local<Integer>());
  Local<Value> ResourceName() const { return resource_name_; }
  Local<Integer> ResourceLineOffset() const { return resource_line_offset_; }
  Local<Integer> ResourceColumnOffset() const {
    return resource_column_offset_;
  }

 private:
  Local<Value> resource_name_;
  Local<Integer> resource_line_offset_;
  Local<Integer> resource_column_offset_;
};

class V8_EXPORT Script {
 public:
  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<Script> Compile(Handle<String> source,
                            ScriptOrigin* origin = nullptr));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, Handle<String> source,
      ScriptOrigin* origin = nullptr);

  static Local<Script> V8_DEPRECATE_SOON("Use maybe version",
                                         Compile(Handle<String> source,
                                                 Handle<String> file_name));

  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Run());
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Run(Local<Context> context);

  Local<UnboundScript> GetUnboundScript();

 private:
  friend class internal::RootStore;

  Script(Local<Context> context, JSScript* script);

  JSScript* script_;
  Local<Context> context_;
};

class V8_EXPORT ScriptCompiler {
 public:
  struct CachedData {
    // CHAKRA-TODO: Not implemented
    enum BufferPolicy { BufferNotOwned, BufferOwned };

    const uint8_t* data;
    int length;
    bool rejected = true;
    BufferPolicy buffer_policy;

    CachedData();
    CachedData(const uint8_t* data, int length,
               BufferPolicy buffer_policy = BufferNotOwned) {}
  };

  class Source {
   public:
    Source(Local<String> source_string, const ScriptOrigin& origin,
           CachedData* cached_data = NULL);

    Source(Local<String> source_string, CachedData* cached_data = NULL);

    const CachedData* GetCachedData() const { return nullptr; }

   private:
    friend ScriptCompiler;
    friend class UnboundScript;
    Local<String> source_string;
    Handle<Value> resource_name;
  };

  enum CompileOptions {
    kNoCompileOptions = 0,
    kProduceParserCache,
    kConsumeParserCache,
    kProduceCodeCache,
    kConsumeCodeCache
  };

  static V8_DEPRECATE_SOON("Use maybe version",
                           Local<UnboundScript> CompileUnbound(
                               Isolate* isolate, Source* source,
                               CompileOptions options = kNoCompileOptions));
  static V8_WARN_UNUSED_RESULT MaybeLocal<UnboundScript> CompileUnboundScript(
      Isolate* isolate, Source* source,
      CompileOptions options = kNoCompileOptions);

  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<Script> Compile(Isolate* isolate, Source* source,
                            CompileOptions options = kNoCompileOptions));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, Source* source,
      CompileOptions options = kNoCompileOptions);

  static uint32_t CachedDataVersionTag() { return 0; }
};

class V8_EXPORT UnboundScript {
 public:
  Local<Script> BindToCurrentContext();

 private:
  UnboundScript(Isolate* isolate, ScriptCompiler::Source* source);
  ~UnboundScript();

  friend class Isolate;
  friend class Script;
  friend class ScriptCompiler;

  struct Impl;
  Impl* pimpl_;
};

class V8_EXPORT Message {
 public:
  Local<String> Get() const;

  V8_DEPRECATE_SOON("Use maybe version", Local<String> GetSourceLine()) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<String> GetSourceLine(
      Local<Context> context) const;

  Handle<Value> GetScriptResourceName() const;

  V8_DEPRECATE_SOON("Use maybe version", int GetLineNumber()) const;
  V8_WARN_UNUSED_RESULT Maybe<int> GetLineNumber(Local<Context> context) const;

  V8_DEPRECATE_SOON("Use maybe version", int GetStartColumn()) const;
  V8_WARN_UNUSED_RESULT Maybe<int> GetStartColumn(Local<Context> context) const;

  V8_DEPRECATE_SOON("Use maybe version", int GetEndColumn()) const;
  V8_WARN_UNUSED_RESULT Maybe<int> GetEndColumn(Local<Context> context) const;

  Local<StackTrace> GetStackTrace() const;

  ScriptOrigin GetScriptOrigin() const;

  static const int kNoLineNumberInfo = 0;
  static const int kNoColumnInfo = 0;
  static const int kNoScriptIdInfo = 0;

 private:
  friend class TryCatch;
  friend class internal::RootStore;
  explicit Message(Local<Value> exception);
  ~Message();

 private:
  struct Impl;
  Impl* pimpl_;
};

typedef void (*MessageCallback)(Handle<Message> message, Handle<Value> error);

class V8_EXPORT StackTrace {
 public:
  enum StackTraceOptions {
    kLineNumber = 1,
    kColumnOffset = 1 << 1 | kLineNumber,
    kScriptName = 1 << 2,
    kFunctionName = 1 << 3,
    kIsEval = 1 << 4,
    kIsConstructor = 1 << 5,
    kScriptNameOrSourceURL = 1 << 6,
    kScriptId = 1 << 7,
    kExposeFramesAcrossSecurityOrigins = 1 << 8,
    kOverview = kLineNumber | kColumnOffset | kScriptName | kFunctionName,
    kDetailed = kOverview | kIsEval | kIsConstructor | kScriptNameOrSourceURL
  };

  Local<StackFrame> GetFrame(uint32_t index) const;
  int GetFrameCount() const;
  Local<Array> AsArray();

  static Local<StackTrace> CurrentStackTrace(
      Isolate* isolate, int frame_limit, StackTraceOptions options = kOverview);

  ~StackTrace();

 private:
  StackTrace();

  friend class Message;

  static Local<StackTrace> CreateStackTrace(Isolate* isolate,
                                            JSObject* stack,
                                            StackTraceOptions options);
  static Local<StackTrace> ExceptionStackTrace(Isolate* isolate,
                                               JSObject* exception);

  struct Impl;
  Impl* pimpl_;
};

class V8_EXPORT StackFrame {
 public:
  int GetLineNumber() const;
  int GetColumn() const;
  int GetScriptId() const;
  Local<String> GetScriptName() const;
  Local<String> GetScriptNameOrSourceURL() const;
  Local<String> GetFunctionName() const;
  bool IsEval() const;
  bool IsConstructor() const;

 private:
  StackFrame(Local<Object> frame);

  friend class StackTrace;
  Local<Object> frame_;
};

/**
 * Value serialization compatible with the HTML structured clone algorithm.
 * The format is backward-compatible (i.e. safe to store to disk).
 *
 * WARNING: This API is under development, and changes (including incompatible
 * changes to the API or wire format) may occur without notice until this
 * warning is removed.
 */
class V8_EXPORT ValueSerializer {
 public:
  class V8_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    /*
     * Handles the case where a DataCloneError would be thrown in the structured
     * clone spec. Other V8 embedders may throw some other appropriate exception
     * type.
     */
    virtual void ThrowDataCloneError(Local<String> message) = 0;

    /*
     * The embedder overrides this method to write some kind of host object, if
     * possible. If not, a suitable exception should be thrown and
     * Nothing<bool>() returned.
     */
    virtual Maybe<bool> WriteHostObject(Isolate* isolate, Local<Object> object);

    /*
     * Called when the ValueSerializer is going to serialize a
     * SharedArrayBuffer object. The embedder must return an ID for the
     * object, using the same ID if this SharedArrayBuffer has already been
     * serialized in this buffer. When deserializing, this ID will be passed to
     * ValueDeserializer::TransferSharedArrayBuffer as |transfer_id|.
     *
     * If the object cannot be serialized, an
     * exception should be thrown and Nothing<uint32_t>() returned.
     */
    virtual Maybe<uint32_t> GetSharedArrayBufferId(
        Isolate* isolate, Local<SharedArrayBuffer> shared_array_buffer);

    /*
     * Allocates memory for the buffer of at least the size provided. The actual
     * size (which may be greater or equal) is written to |actual_size|. If no
     * buffer has been allocated yet, nullptr will be provided.
     *
     * If the memory cannot be allocated, nullptr should be returned.
     * |actual_size| will be ignored. It is assumed that |old_buffer| is still
     * valid in this case and has not been modified.
     */
    virtual void* ReallocateBufferMemory(void* old_buffer, size_t size,
                                         size_t* actual_size);

    /*
     * Frees a buffer allocated with |ReallocateBufferMemory|.
     */
    virtual void FreeBufferMemory(void* buffer);
  };

  explicit ValueSerializer(Isolate* isolate);
  ValueSerializer(Isolate* isolate, Delegate* delegate);
  ~ValueSerializer();

  /*
   * Writes out a header, which includes the format version.
   */
  void WriteHeader();

  /*
   * Serializes a JavaScript value into the buffer.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> WriteValue(Local<Context> context,
                                               Local<Value> value);

  /*
   * Returns the stored data. This serializer should not be used once the buffer
   * is released. The contents are undefined if a previous write has failed.
   */
  V8_DEPRECATE_SOON("Use Release()", std::vector<uint8_t> ReleaseBuffer());

  /*
   * Returns the stored data (allocated using the delegate's
   * AllocateBufferMemory) and its size. This serializer should not be used once
   * the buffer is released. The contents are undefined if a previous write has
   * failed.
   */
  V8_WARN_UNUSED_RESULT std::pair<uint8_t*, size_t> Release();

  /*
   * Marks an ArrayBuffer as havings its contents transferred out of band.
   * Pass the corresponding JSArrayBuffer in the deserializing context to
   * ValueDeserializer::TransferArrayBuffer.
   */
  void TransferArrayBuffer(uint32_t transfer_id,
                           Local<ArrayBuffer> array_buffer);

  /*
   * Similar to TransferArrayBuffer, but for SharedArrayBuffer.
   */
  V8_DEPRECATE_SOON("Use Delegate::GetSharedArrayBufferId",
                    void TransferSharedArrayBuffer(
                        uint32_t transfer_id,
                        Local<SharedArrayBuffer> shared_array_buffer));

  /*
   * Indicate whether to treat ArrayBufferView objects as host objects,
   * i.e. pass them to Delegate::WriteHostObject. This should not be
   * called when no Delegate was passed.
   *
   * The default is not to treat ArrayBufferViews as host objects.
   */
  void SetTreatArrayBufferViewsAsHostObjects(bool mode);

  /*
   * Write raw data in various common formats to the buffer.
   * Note that integer types are written in base-128 varint format, not with a
   * binary copy. For use during an override of Delegate::WriteHostObject.
   */
  void WriteUint32(uint32_t value);
  void WriteUint64(uint64_t value);
  void WriteDouble(double value);
  void WriteRawBytes(const void* source, size_t length);

 private:
  ValueSerializer(const ValueSerializer&) = delete;
  void operator=(const ValueSerializer&) = delete;

  struct PrivateData;
  PrivateData* private_;
};

/**
 * Deserializes values from data written with ValueSerializer, or a compatible
 * implementation.
 *
 * WARNING: This API is under development, and changes (including incompatible
 * changes to the API or wire format) may occur without notice until this
 * warning is removed.
 */
class V8_EXPORT ValueDeserializer {
 public:
  class V8_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    /*
     * The embedder overrides this method to read some kind of host object, if
     * possible. If not, a suitable exception should be thrown and
     * MaybeLocal<Object>() returned.
     */
    virtual MaybeLocal<Object> ReadHostObject(Isolate* isolate);
  };

  ValueDeserializer(Isolate* isolate, const uint8_t* data, size_t size);
  ValueDeserializer(Isolate* isolate, const uint8_t* data, size_t size,
                    Delegate* delegate);
  ~ValueDeserializer();

  /*
   * Reads and validates a header (including the format version).
   * May, for example, reject an invalid or unsupported wire format.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> ReadHeader(Local<Context> context);

  /*
   * Deserializes a JavaScript value from the buffer.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> ReadValue(Local<Context> context);

  /*
   * Accepts the array buffer corresponding to the one passed previously to
   * ValueSerializer::TransferArrayBuffer.
   */
  void TransferArrayBuffer(uint32_t transfer_id,
                           Local<ArrayBuffer> array_buffer);

  /*
   * Similar to TransferArrayBuffer, but for SharedArrayBuffer.
   * The id is not necessarily in the same namespace as unshared ArrayBuffer
   * objects.
   */
  void TransferSharedArrayBuffer(uint32_t id,
                                 Local<SharedArrayBuffer> shared_array_buffer);

  /*
   * Must be called before ReadHeader to enable support for reading the legacy
   * wire format (i.e., which predates this being shipped).
   *
   * Don't use this unless you need to read data written by previous versions of
   * blink::ScriptValueSerializer.
   */
  void SetSupportsLegacyWireFormat(bool supports_legacy_wire_format);

  /*
   * Reads the underlying wire format version. Likely mostly to be useful to
   * legacy code reading old wire format versions. Must be called after
   * ReadHeader.
   */
  uint32_t GetWireFormatVersion() const;

  /*
   * Reads raw data in various common formats to the buffer.
   * Note that integer types are read in base-128 varint format, not with a
   * binary copy. For use during an override of Delegate::ReadHostObject.
   */
  V8_WARN_UNUSED_RESULT bool ReadUint32(uint32_t* value);
  V8_WARN_UNUSED_RESULT bool ReadUint64(uint64_t* value);
  V8_WARN_UNUSED_RESULT bool ReadDouble(double* value);
  V8_WARN_UNUSED_RESULT bool ReadRawBytes(size_t length, const void** data);

 private:
  ValueDeserializer(const ValueDeserializer&) = delete;
  void operator=(const ValueDeserializer&) = delete;

  struct PrivateData;
  PrivateData* private_;
};

// v8::Value C++ objects in V8 are not created like normal C++ objects. Instead,
// the engine itself is in charge of creating them.  In V8, v8::Value inherits
// from Data which is an empty class of size 1 byte, and is also 1 byte long,
// but
// it's really a 32-bit value that's masked as a C++ 1-byte object.  Internally,
// V8 reinterpret_cast's Values to more useful types before it can do anything
// with them.
//
// This model is vastly incompatible with SpiderMonkey's JS::Value which is a
// normal C++ class that is 64 bits in size.  The implementation would become
// much
// simpler if we implemented v8::Value in terms of JS::Value, but there is no
// good
// way to maintain v8::Value inheriting from v8::Data.  In order to maintain the
// type hierarchy, we add an 8-byte char array to this type to expand the total
// size
// to 8 bytes, and we store the JS::Value in-place inside the object.  Note that
// this relies on the compiler implementing the empty base optimization which in
// practice all compilers do, and is mandated in C++17 so it's future proof.
class V8_EXPORT Value : public Data {
 public:
  bool IsUndefined() const;
  bool IsNull() const;
  bool IsTrue() const;
  bool IsFalse() const;
  bool IsString() const;
  bool IsSymbol() const;
  bool IsFunction() const;
  bool IsArray() const;
  bool IsObject() const;
  bool IsBoolean() const;
  bool IsNumber() const;
  bool IsInt32() const;
  bool IsUint32() const;
  bool IsDate() const;
  bool IsBooleanObject() const;
  bool IsNumberObject() const;
  bool IsStringObject() const;
  bool IsNativeError() const;
  bool IsRegExp() const;
  bool IsExternal() const;
  bool IsArrayBuffer() const;
  bool IsArrayBufferView() const;
  bool IsTypedArray() const;
  bool IsUint8Array() const;
  bool IsUint8ClampedArray() const;
  bool IsInt8Array() const;
  bool IsUint16Array() const;
  bool IsInt16Array() const;
  bool IsUint32Array() const;
  bool IsInt32Array() const;
  bool IsFloat32Array() const;
  bool IsFloat64Array() const;
  bool IsDataView() const;
  bool IsSharedArrayBuffer() const;
  bool IsMapIterator() const;
  bool IsSetIterator() const;
  bool IsMap() const;
  bool IsSet() const;
  bool IsPromise() const;
  bool IsProxy() const;
  bool IsWebAssemblyCompiledModule() const;

  V8_WARN_UNUSED_RESULT MaybeLocal<Boolean> ToBoolean(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Number> ToNumber(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToString(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToDetailString(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> ToObject(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Integer> ToInteger(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToUint32(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Int32> ToInt32(Local<Context> context) const;

  Local<Boolean> ToBoolean(Isolate* isolate = nullptr) const;
  Local<Number> ToNumber(Isolate* isolate = nullptr) const;
  Local<String> ToString(Isolate* isolate = nullptr) const;
  Local<String> ToDetailString(Isolate* isolate = nullptr) const;
  Local<Object> ToObject(Isolate* isolate = nullptr) const;
  Local<Integer> ToInteger(Isolate* isolate = nullptr) const;
  Local<Uint32> ToUint32(Isolate* isolate = nullptr) const;
  Local<Int32> ToInt32(Isolate* isolate = nullptr) const;

  V8_DEPRECATE_SOON("Use maybe version", Local<Uint32> ToArrayIndex()) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToArrayIndex(
      Local<Context> context) const;

  V8_WARN_UNUSED_RESULT Maybe<bool> BooleanValue(Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<double> NumberValue(Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<int64_t> IntegerValue(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<uint32_t> Uint32Value(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<int32_t> Int32Value(Local<Context> context) const;

  V8_DEPRECATE_SOON("Use maybe version", bool BooleanValue()) const;
  V8_DEPRECATE_SOON("Use maybe version", double NumberValue()) const;
  V8_DEPRECATE_SOON("Use maybe version", int64_t IntegerValue()) const;
  V8_DEPRECATE_SOON("Use maybe version", uint32_t Uint32Value()) const;
  V8_DEPRECATE_SOON("Use maybe version", int32_t Int32Value()) const;

  V8_DEPRECATE_SOON("Use maybe version", bool Equals(Handle<Value> that)) const;
  V8_WARN_UNUSED_RESULT Maybe<bool> Equals(Local<Context> context,
                                           Handle<Value> that) const;

  bool StrictEquals(Handle<Value> that) const;
  bool SameValue(Local<Value> that) const;

  template <class T>
  static Value* Cast(T* value) {
    return static_cast<Value*>(value);
  }

  Local<String> TypeOf(v8::Isolate*);

 private:
  char spidershim_padding[8];  // see the comment for Value.

 protected:
  template <class F>
  friend class Local;
  template <class F>
  friend class PersistentBase;
  V8_INLINE bool operator==(const Value& that) const {
    for (size_t i = 0; i < sizeof(spidershim_padding); ++i) {
      if (spidershim_padding[i] != that.spidershim_padding[i]) {
        return false;
      }
    }
    return true;
  }
};
static_assert(sizeof(v8::Value) == 8,
              "v8::Value must be the same size as JS::Value");

class V8_EXPORT Private : public Data {
 public:
  Local<Value> Name() const;
  static Local<Private> New(Isolate* isolate,
                            Local<String> name = Local<String>());
  static Local<Private> ForApi(Isolate* isolate, Local<String> name);

 private:
  friend class internal::RootStore;
  friend class Isolate;
  friend class Object;

  Private(JS::Symbol* symbol) : symbol_(symbol) {}

  JS::Symbol* symbol_;
};

class V8_EXPORT Primitive : public Value {
 public:
};

class V8_EXPORT Boolean : public Primitive {
 public:
  bool Value() const;
  static Handle<Boolean> New(Isolate* isolate, bool value);
  static Boolean* Cast(v8::Value* obj);

 private:
  friend class BooleanObject;
  template <class F>
  friend class ReturnValue;
  static Local<Boolean> From(bool value);
};

class V8_EXPORT Name : public Primitive {
 public:
  int GetIdentityHash();
  static Name* Cast(v8::Value* obj);

 private:
  static void CheckCast(v8::Value* obj);
};

enum class NewStringType { kNormal, kInternalized };

class V8_EXPORT String : public Name {
 public:
  static const int kMaxLength = (1 << 28) - 16;

  int Length() const;
  int Utf8Length() const;
  bool IsOneByte() const { return false; }
  bool ContainsOnlyOneByte() const { return false; }

  enum WriteOptions {
    NO_OPTIONS = 0,
    HINT_MANY_WRITES_EXPECTED = 1,
    NO_NULL_TERMINATION = 2,
    PRESERVE_ONE_BYTE_NULL = 4,
    REPLACE_INVALID_UTF8 = 8
  };

  int Write(uint16_t* buffer, int start = 0, int length = -1,
            int options = NO_OPTIONS) const;
  int WriteOneByte(uint8_t* buffer, int start = 0, int length = -1,
                   int options = NO_OPTIONS) const;
  int WriteUtf8(char* buffer, int length = -1, int* nchars_ref = NULL,
                int options = NO_OPTIONS) const;

  static Local<String> Empty(Isolate* isolate);
  bool IsExternal() const { return false; }
  bool IsExternalOneByte() const { return false; }

  class V8_EXPORT ExternalStringResourceBase {  // NOLINT
   public:
    virtual ~ExternalStringResourceBase() {}

    virtual bool IsCompressible() const;

   protected:
    ExternalStringResourceBase() {}

    /**
     * Internally V8 will call this Dispose method when the external string
     * resource is no longer needed. The default implementation will use the
     * delete operator. This method can be overridden in subclasses to
     * control how allocated external string resources are disposed.
     */
    virtual void Dispose();

   private:
    // Disallow copying and assigning.
    ExternalStringResourceBase(const ExternalStringResourceBase&);
    void operator=(const ExternalStringResourceBase&);

    friend struct internal::ExternalStringFinalizerBase<
        internal::ExternalStringFinalizer>;
    friend struct internal::ExternalStringFinalizerBase<
        internal::ExternalOneByteStringFinalizer>;
  };

  /**
   * An ExternalStringResource is a wrapper around a two-byte string
   * buffer that resides outside V8's heap. Implement an
   * ExternalStringResource to manage the life cycle of the underlying
   * buffer.  Note that the string data must be immutable.
   */
  class V8_EXPORT ExternalStringResource : public ExternalStringResourceBase {
   public:
    /**
     * Override the destructor to manage the life cycle of the underlying
     * buffer.
     */
    ~ExternalStringResource() override {}

    /**
     * The string data from the underlying buffer.
     */
    virtual const uint16_t* data() const = 0;

    /**
     * The length of the string. That is, the number of two-byte characters.
     */
    virtual size_t length() const = 0;

   protected:
    ExternalStringResource() {}
  };

  /**
   * An ExternalOneByteStringResource is a wrapper around an one-byte
   * string buffer that resides outside V8's heap. Implement an
   * ExternalOneByteStringResource to manage the life cycle of the
   * underlying buffer.  Note that the string data must be immutable
   * and that the data must be Latin-1 and not UTF-8, which would require
   * special treatment internally in the engine and do not allow efficient
   * indexing.  Use String::New or convert to 16 bit data for non-Latin1.
   */

  class V8_EXPORT ExternalOneByteStringResource
      : public ExternalStringResourceBase {
   public:
    /**
     * Override the destructor to manage the life cycle of the underlying
     * buffer.
     */
    ~ExternalOneByteStringResource() override {}
    /** The string data from the underlying buffer.*/
    virtual const char* data() const = 0;
    /** The number of Latin-1 characters in the string.*/
    virtual size_t length() const = 0;

   protected:
    ExternalOneByteStringResource() {}
  };

  ExternalStringResource* GetExternalStringResource() const { return nullptr; }
  const ExternalOneByteStringResource* GetExternalOneByteStringResource()
      const {
    return nullptr;
  }

  static String* Cast(v8::Value* obj);

  enum NewStringType {
    kNormalString = static_cast<int>(v8::NewStringType::kNormal),
    kInternalizedString = static_cast<int>(v8::NewStringType::kInternalized)
  };

  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<String> NewFromUtf8(Isolate* isolate, const char* data,
                                NewStringType type = kNormalString,
                                int length = -1));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromUtf8(
      Isolate* isolate, const char* data, v8::NewStringType type,
      int length = -1);

  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<String> NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                   NewStringType type = kNormalString,
                                   int length = -1));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromOneByte(
      Isolate* isolate, const uint8_t* data, v8::NewStringType type,
      int length = -1);

  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<String> NewFromTwoByte(Isolate* isolate, const uint16_t* data,
                                   NewStringType type = kNormalString,
                                   int length = -1));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromTwoByte(
      Isolate* isolate, const uint16_t* data, v8::NewStringType type,
      int length = -1);

  static Local<String> Concat(Handle<String> left, Handle<String> right);

  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<String> NewExternal(Isolate* isolate,
                                ExternalStringResource* resource));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewExternalTwoByte(
      Isolate* isolate, ExternalStringResource* resource);

  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<String> NewExternal(Isolate* isolate,
                                ExternalOneByteStringResource* resource));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewExternalOneByte(
      Isolate* isolate, ExternalOneByteStringResource* resource);

  class V8_EXPORT Utf8Value {
   public:
    explicit Utf8Value(Handle<v8::Value> obj);
    ~Utf8Value();
    char* operator*() { return str_; }
    const char* operator*() const { return str_; }
    int length() const { return static_cast<int>(length_); }

   private:
    Utf8Value(const Utf8Value&);
    void operator=(const Utf8Value&);

    char* str_;
    size_t length_;
  };

  class V8_EXPORT Value {
   public:
    explicit Value(Handle<v8::Value> obj);
    ~Value();
    uint16_t* operator*() { return reinterpret_cast<uint16_t*>(str_); }
    const uint16_t* operator*() const {
      return reinterpret_cast<const uint16_t*>(str_);
    }
    int length() const { return static_cast<int>(length_); }

   private:
    Value(const Value&);
    void operator=(const Value&);

    static_assert(sizeof(char16_t) == sizeof(uint16_t),
                  "Sanity check for size of char16_t");
    char16_t* str_;
    size_t length_;
  };

 private:
  template <class ToWide>
  static MaybeLocal<String> New(const ToWide& toWide, const char* data,
                                int length = -1);
  static MaybeLocal<String> New(const wchar_t* data, int length = -1);
};

/**
 * A JavaScript symbol (ECMA-262 edition 6)
 */
class V8_EXPORT Symbol : public Name {
 public:
  // Returns the print name string of the symbol, or undefined if none.
  Local<Value> Name() const;

  // Create a symbol. If name is not empty, it will be used as the description.
  static Local<Symbol> New(Isolate* isolate,
                           Local<String> name = Local<String>());

  // Access global symbol registry.
  // Note that symbols created this way are never collected, so
  // they should only be used for statically fixed properties.
  // Also, there is only one global name space for the names used as keys.
  // To minimize the potential for clashes, use qualified names as keys.
  static Local<Symbol> For(Isolate *isolate, Local<String> name);

  // Retrieve a global symbol. Similar to |For|, but using a separate
  // registry that is not accessible by (and cannot clash with) JavaScript code.
  static Local<Symbol> ForApi(Isolate *isolate, Local<String> name);

  // Well-known symbols
  static Local<Symbol> GetIterator(Isolate* isolate);
  static Local<Symbol> GetUnscopables(Isolate* isolate);
  static Local<Symbol> GetToPrimitive(Isolate* isolate);
  static Local<Symbol> GetToStringTag(Isolate* isolate);
  static Local<Symbol> GetIsConcatSpreadable(Isolate* isolate);

  V8_INLINE static Symbol* Cast(Value* obj);

 private:
  Symbol();
  static void CheckCast(Value* obj);
};

class V8_EXPORT Number : public Primitive {
 public:
  double Value() const;
  static Local<Number> New(Isolate* isolate, double value);
  static Number* Cast(v8::Value* obj);

 private:
  friend class Integer;
  template <class F>
  friend class ReturnValue;
  static Local<Number> From(double value);
};

class V8_EXPORT Integer : public Number {
 public:
  static Local<Integer> New(Isolate* isolate, int32_t value);
  static Local<Integer> NewFromUnsigned(Isolate* isolate, uint32_t value);
  static Integer* Cast(v8::Value* obj);

  int64_t Value() const;

 private:
  friend class Utils;
  template <class F>
  friend class ReturnValue;
  static Local<Integer> From(int32_t value);
  static Local<Integer> From(uint32_t value);
};

class V8_EXPORT Int32 : public Integer {
 public:
  int32_t Value() const;
  static Int32* Cast(v8::Value* obj);
};

class V8_EXPORT Uint32 : public Integer {
 public:
  uint32_t Value() const;
  static Uint32* Cast(v8::Value* obj);
};

class V8_EXPORT Object : public Value {
 public:
  V8_DEPRECATE_SOON("Use maybe version",
                    bool Set(Handle<Value> key, Handle<Value> value));
  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context,
                                        Local<Value> key, Local<Value> value);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool Set(uint32_t index, Handle<Value> value));
  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context, uint32_t index,
                                        Local<Value> value);

  V8_WARN_UNUSED_RESULT Maybe<bool> DefineOwnProperty(
      Local<Context> context, Local<Name> key, Local<Value> value,
      PropertyAttribute attributes = None);

  V8_WARN_UNUSED_RESULT Maybe<bool> DefineProperty(
      Local<Context> context, Local<Name> key, PropertyDescriptor& descriptor);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool ForceSet(Handle<Value> key, Handle<Value> value,
                                  PropertyAttribute attribs = None));
  V8_WARN_UNUSED_RESULT Maybe<bool> ForceSet(Local<Context> context,
                                             Local<Value> key,
                                             Local<Value> value,
                                             PropertyAttribute attribs = None);

  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Get(Handle<Value> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Get(uint32_t index));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              uint32_t index);

  V8_DEPRECATE_SOON("Use maybe version",
                    PropertyAttribute GetPropertyAttributes(Handle<Value> key));
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetPropertyAttributes(
      Local<Context> context, Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> GetOwnPropertyDescriptor(Local<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetOwnPropertyDescriptor(
      Local<Context> context, Local<String> key);

  V8_DEPRECATE_SOON("Use maybe version", bool Has(Handle<Value> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context,
                                        Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version", bool Delete(Handle<Value> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version", bool Has(uint32_t index));
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context, uint32_t index);

  V8_DEPRECATE_SOON("Use maybe version", bool Delete(uint32_t index));
  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           uint32_t index);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool SetAccessor(Handle<String> name,
                                     AccessorGetterCallback getter,
                                     AccessorSetterCallback setter = 0,
                                     Handle<Value> data = Handle<Value>(),
                                     AccessControl settings = DEFAULT,
                                     PropertyAttribute attribute = None));
  V8_DEPRECATE_SOON("Use maybe version",
                    bool SetAccessor(Handle<Name> name,
                                     AccessorNameGetterCallback getter,
                                     AccessorNameSetterCallback setter = 0,
                                     Handle<Value> data = Handle<Value>(),
                                     AccessControl settings = DEFAULT,
                                     PropertyAttribute attribute = None));
  V8_WARN_UNUSED_RESULT
  Maybe<bool> SetAccessor(Local<Context> context, Local<Name> name,
                          AccessorNameGetterCallback getter,
                          AccessorNameSetterCallback setter = 0,
                          MaybeLocal<Value> data = MaybeLocal<Value>(),
                          AccessControl settings = DEFAULT,
                          PropertyAttribute attribute = None);

  void SetAccessorProperty(Local<Name> name, Local<Function> getter,
                           Local<Function> setter = Local<Function>(),
                           PropertyAttribute attribute = None,
                           AccessControl settings = DEFAULT);

  V8_DEPRECATE_SOON("Use maybe version", Local<Array> GetPropertyNames());
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetPropertyNames(
      Local<Context> context);
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetPropertyNames(
      Local<Context> context, KeyCollectionMode mode,
      PropertyFilter property_filter, IndexFilter index_filter);

  V8_DEPRECATE_SOON("Use maybe version", Local<Array> GetOwnPropertyNames());
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetOwnPropertyNames(
      Local<Context> context);

  /**
   * Returns an array containing the names of the filtered properties
   * of this object, including properties from prototype objects.  The
   * array returned by this method contains the same values as would
   * be enumerated by a for-in statement over this object.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetOwnPropertyNames(
      Local<Context> context, PropertyFilter filter);

  Local<Value> GetPrototype();

  V8_DEPRECATE_SOON("Use maybe version",
                    bool SetPrototype(Handle<Value> prototype));
  V8_WARN_UNUSED_RESULT Maybe<bool> SetPrototype(Local<Context> context,
                                                 Local<Value> prototype);

  V8_DEPRECATE_SOON("Use maybe version", Local<String> ObjectProtoToString());
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ObjectProtoToString(
      Local<Context> context);

  Local<String> GetConstructorName();
  int InternalFieldCount();
  /** Same as above, but works for Persistents */
  V8_INLINE static int InternalFieldCount(
      const PersistentBase<Object>& object) {
    return object.val_->InternalFieldCount();
  }
  Local<Value> GetInternalField(int index);
  void SetInternalField(int index, Handle<Value> value);
  void* GetAlignedPointerFromInternalField(int index);
  /** Same as above, but works for Persistents */
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const PersistentBase<Object>& object, int index) {
    return object.val_->GetAlignedPointerFromInternalField(index);
  }
  void SetAlignedPointerInInternalField(int index, void* value);
  void SetAlignedPointerInInternalFields(int argc, int indices[],
                                         void* values[]);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasOwnProperty(Handle<String> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasOwnProperty(Local<Context> context,
                                                   Local<Name> key);
  V8_WARN_UNUSED_RESULT Maybe<bool> HasOwnProperty(Local<Context> context,
                                                   uint32_t index);
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealNamedProperty(Handle<String> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealNamedProperty(Local<Context> context,
                                                         Local<Name> key);
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealIndexedProperty(uint32_t index));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealIndexedProperty(
      Local<Context> context, uint32_t index);
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealNamedCallbackProperty(Handle<String> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealNamedCallbackProperty(
      Local<Context> context, Local<Name> key);

  V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<Value> GetRealNamedPropertyInPrototypeChain(Handle<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedPropertyInPrototypeChain(
      Local<Context> context, Local<Name> key);

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> GetRealNamedProperty(Handle<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedProperty(
      Local<Context> context, Local<Name> key);

  V8_DEPRECATE_SOON("Use maybe version",
                    Maybe<PropertyAttribute> GetRealNamedPropertyAttributes(
                        Handle<String> key));
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetRealNamedPropertyAttributes(
      Local<Context> context, Local<Name> key);

  Maybe<bool> HasPrivate(Local<Context> context, Local<Private> key);
  Maybe<bool> SetPrivate(Local<Context> context, Local<Private> key,
                         Local<Value> value);
  Maybe<bool> DeletePrivate(Local<Context> context, Local<Private> key);
  MaybeLocal<Value> GetPrivate(Local<Context> context, Local<Private> key);

  Local<Object> Clone();
  Local<Context> CreationContext();

  /**
   * True if this object is a constructor.
   */
  bool IsConstructor();

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> CallAsFunction(Handle<Value> recv, int argc,
                                                Handle<Value> argv[]));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsFunction(Local<Context> context,
                                                         Handle<Value> recv,
                                                         int argc,
                                                         Handle<Value> argv[]);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> CallAsConstructor(int argc,
                                                   Handle<Value> argv[]));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsConstructor(
      Local<Context> context, int argc, Local<Value> argv[]);

  Isolate* GetIsolate();
  static Local<Object> New(Isolate* isolate = nullptr);
  static Object* Cast(Value* obj);

 private:
  friend struct ObjectData;
  friend class ObjectTemplate;
  friend class Utils;

  Maybe<bool> Set(Local<Context> context, Handle<Value> key,
                  Handle<Value> value, PropertyAttribute attribs, bool force);

  template <class N, class Getter, class Setter>
  Maybe<bool> SetAccessorInternal(JSContext* cx, Handle<N> name, Getter getter,
                                  Setter setter, Handle<Value> data,
                                  AccessControl settings,
                                  PropertyAttribute attribute);

  MaybeLocal<Array> GetPropertyNames(Local<Context> context, unsigned flags);

#if 0
  JsErrorCode GetObjectData(struct ObjectData** objectData);
#endif
  ObjectTemplate* GetObjectTemplate();
};

class V8_EXPORT Array : public Object {
 public:
  uint32_t Length() const;

#if 0
  // Not supported yet, see https://github.com/mozilla/spidernode/issues/33
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Object> CloneElementAt(uint32_t index));
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> CloneElementAt(
    Local<Context> context, uint32_t index);
#endif

  static Local<Array> New(Isolate* isolate = nullptr, int length = 0);
  static Array* Cast(Value* obj);
};

class V8_EXPORT BooleanObject : public Object {
 public:
  static Local<Value> New(bool value);
  bool ValueOf() const;
  static BooleanObject* Cast(Value* obj);
};

class V8_EXPORT StringObject : public Object {
 public:
  static Local<Value> New(Handle<String> value);
  Local<String> ValueOf() const;
  static StringObject* Cast(Value* obj);
};

class V8_EXPORT NumberObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, double value);
  double ValueOf() const;
  static NumberObject* Cast(Value* obj);
};

class V8_EXPORT Date : public Object {
 public:
  static V8_DEPRECATE_SOON("Use maybe version.",
                           Local<Value> New(Isolate* isolate, double time));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Value> New(Local<Context> context,
                                                     double time);

  double ValueOf() const;
  static Date* Cast(Value* obj);
};

class V8_EXPORT RegExp : public Object {
 public:
  enum Flags { kNone = 0, kGlobal = 1, kIgnoreCase = 2, kMultiline = 4 };

  static V8_DEPRECATE_SOON("Use maybe version",
                           Local<RegExp> New(Handle<String> pattern,
                                             Flags flags));
  static V8_WARN_UNUSED_RESULT MaybeLocal<RegExp> New(Local<Context> context,
                                                      Handle<String> pattern,
                                                      Flags flags);
  Local<String> GetSource() const;
  static RegExp* Cast(v8::Value* obj);
};

template <typename T>
class ReturnValue {
 public:
  // Handle setters
  template <typename S>
  void Set(const Persistent<S>& handle) {
    if (handle.IsEmpty()) {
      SetUndefined();
      return;
    }
    *_value = static_cast<Value*>(*handle);
  }
  template <typename S>
  void Set(const Handle<S> handle) {
    if (handle.IsEmpty()) {
      SetUndefined();
      return;
    }
    // ReturnValue needs to root the Local to the parent HandleScope.
    Local<S> parentHandle =
        EscapableHandleScope::EscapeToParentHandleScope(handle);
    if (parentHandle.IsEmpty()) {
      // That didn't work, we can't really do anything better here.
      *_value = static_cast<Value*>(*handle);
    } else {
      *_value = static_cast<Value*>(*parentHandle);
    }
  }
  // Fast primitive setters
  void Set(bool value) { Set(Boolean::From(value)); }
  void Set(double value) { Set(Number::From(value)); }
  void Set(int32_t value) { Set(Integer::From(value)); }
  void Set(uint32_t value) { Set(Integer::From(value)); }
  // Fast JS primitive setters
  void SetNull() { Set(Null(nullptr)); }
  void SetUndefined() { Set(Undefined(nullptr)); }
  void SetEmptyString() { Set(String::Empty(nullptr)); }
  // Convenience getter for Isolate
  V8_INLINE Isolate* GetIsolate();

  Value* Get() const { return *_value; }

 private:
  explicit ReturnValue(Value** value) : _value(value) {}

  Value** _value;
  template <typename F>
  friend class FunctionCallbackInfo;
  template <typename F>
  friend class PropertyCallbackInfo;
};

template <typename T>
class FunctionCallbackInfo {
 public:
  int Length() const { return _length; }
  Local<Value> operator[](int i) const {
    return (i >= 0 && i < _length) ? _args[i] : Undefined(nullptr).As<Value>();
  }
  Local<Function> Callee() const { return _callee; }
  Local<Object> This() const { return _thisPointer; }
  Local<Object> Holder() const { return _holder; }
  bool IsConstructCall() const { return _isConstructorCall; }
  Local<Value> Data() const { return _data; }
  V8_INLINE Isolate* GetIsolate() const;
  ReturnValue<T> GetReturnValue() const {
    return ReturnValue<T>(
        &(const_cast<FunctionCallbackInfo<T>*>(this)->_returnValue));
  }

  FunctionCallbackInfo(Value** args, int length, Local<Object> _this,
                       Local<Object> holder, bool isConstructorCall,
                       Local<Value> data, Local<Function> callee)
      : _args(args),
        _length(length),
        _thisPointer(_this),
        _holder(holder),
        _isConstructorCall(isConstructorCall),
        _data(data),
        _callee(callee),
        _returnValue(nullptr) {}

 private:
  Value** _args;
  int _length;
  Local<Object> _thisPointer;
  Local<Object> _holder;
  bool _isConstructorCall;
  Local<Value> _data;
  Local<Function> _callee;
  Value* _returnValue;
};

template <typename T>
class PropertyCallbackInfo {
 public:
  V8_INLINE Isolate* GetIsolate() const;
  Local<Value> Data() const { return _data; }
  Local<Object> This() const { return _thisObject; }
  Local<Object> Holder() const { return _holder; }
  ReturnValue<T> GetReturnValue() const {
    return ReturnValue<T>(
        &(const_cast<PropertyCallbackInfo<T>*>(this)->_returnValue));
  }
  bool ShouldThrowOnError() const { return true; }

  PropertyCallbackInfo(Local<Value> data, Local<Object> thisObject,
                       Local<Object> holder)
      : _data(data),
        _thisObject(thisObject),
        _holder(holder),
        _returnValue(nullptr) {}

 private:
  Local<Value> _data;
  Local<Object> _thisObject;
  Local<Object> _holder;
  Value* _returnValue;
};

typedef void (*FunctionCallback)(const FunctionCallbackInfo<Value>& info);

enum class ConstructorBehavior { kThrow, kAllow };

class V8_EXPORT Function : public Object {
 public:
  static MaybeLocal<Function> New(
      Local<Context> context, FunctionCallback callback,
      Local<Value> data = Local<Value>(), int length = 0,
      ConstructorBehavior behavior = ConstructorBehavior::kAllow);
  static Local<Function> New(Isolate* isolate, FunctionCallback callback,
                             Local<Value> data = Local<Value>(),
                             int length = 0);

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Object> NewInstance(int argc, Handle<Value> argv[]))
  const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
      Local<Context> context, int argc, Handle<Value> argv[]) const;

  V8_DEPRECATE_SOON("Use maybe version", Local<Object> NewInstance()) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
      Local<Context> context) const {
    return NewInstance(context, 0, nullptr);
  }

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> Call(Handle<Value> recv, int argc,
                                      Handle<Value> argv[]));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Call(Local<Context> context,
                                               Handle<Value> recv, int argc,
                                               Handle<Value> argv[]);

  void SetName(Handle<String> name);
  Local<Value> GetName() const;

  ScriptOrigin GetScriptOrigin() const;

  static Function* Cast(Value* obj);

 protected:
  static MaybeLocal<Function> New(Local<Context> context,
                                  FunctionCallback callback, Local<Value> data,
                                  int length, Local<FunctionTemplate> templ,
                                  Local<String> name);

 private:
  friend class FunctionTemplate;
  friend class ObjectTemplate;
};

class V8_EXPORT Promise : public Object {
 public:
  class V8_EXPORT Resolver : public Object {
   public:
    static Local<Resolver> New(Isolate* isolate);
    Local<Promise> GetPromise();
    void Resolve(Handle<Value> value);
    void Reject(Handle<Value> value);
    static Resolver* Cast(Value* obj);

   private:
    Resolver();
    static void CheckCast(Value* obj);
  };

  Local<Promise> Catch(Handle<Function> handler);
  Local<Promise> Then(Handle<Function> handler);

  bool HasHandler();
  static Promise* Cast(Value* obj);

 private:
  Promise();
};

class V8_EXPORT PropertyDescriptor {
 public:
  // GenericDescriptor
  PropertyDescriptor();

  // DataDescriptor
  PropertyDescriptor(Local<Value> value);

  // DataDescriptor with writable property
  PropertyDescriptor(Local<Value> value, bool writable);

  // AccessorDescriptor
  PropertyDescriptor(Local<Value> get, Local<Value> set);

  ~PropertyDescriptor();

  Local<Value> value() const;
  bool has_value() const;

  Local<Value> get() const;
  bool has_get() const;
  Local<Value> set() const;
  bool has_set() const;

  void set_enumerable(bool enumerable);
  bool enumerable() const;
  bool has_enumerable() const;

  void set_configurable(bool configurable);
  bool configurable() const;
  bool has_configurable() const;

  bool writable() const;
  bool has_writable() const;

  PropertyDescriptor(const PropertyDescriptor&) = delete;
  void operator=(const PropertyDescriptor&) = delete;

 private:
  struct Impl;
  Impl* pimpl_;
};

class V8_EXPORT Proxy : public Object {
 public:
  Local<Object> GetTarget();
  Local<Value> GetHandler();
  bool IsRevoked();
  void Revoke();

  static MaybeLocal<Proxy> New(Local<Context> context,
                               Local<Object> local_target,
                               Local<Object> local_handler);

  static Proxy* Cast(Value* obj);

 private:
  Proxy();
  static void CheckCast(Value* obj);
};

class V8_EXPORT WasmCompiledModule : public Object {
 public:
  typedef std::pair<std::unique_ptr<const uint8_t[]>, size_t> SerializedModule;

  SerializedModule Serialize();
  static MaybeLocal<WasmCompiledModule> Deserialize(
      Isolate* isolate, const SerializedModule& serialized_data);
  V8_INLINE static WasmCompiledModule* Cast(Value* obj);

 private:
  WasmCompiledModule();
  static void CheckCast(Value* obj);
};

WasmCompiledModule* WasmCompiledModule::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<WasmCompiledModule*>(value);
}

#ifndef V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT
// The number of required internal fields can be defined by embedder.
#define V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT 2
#endif

enum class ArrayBufferCreationMode { kInternalized, kExternalized };

class V8_EXPORT ArrayBuffer : public Object {
 public:
  class V8_EXPORT Allocator {  // NOLINT
   public:
    virtual ~Allocator() {}
    virtual void* Allocate(size_t length) = 0;
    virtual void* AllocateUninitialized(size_t length) = 0;
    virtual void Free(void* data, size_t length) = 0;

    /**
     * malloc/free based convenience allocator.
     *
     * Caller takes ownership.
     */
    static Allocator* NewDefaultAllocator();
  };

  class V8_EXPORT Contents {  // NOLINT
   public:
    Contents() : data_(NULL), byte_length_(0) {}
    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }

   private:
    void* data_;
    size_t byte_length_;
    friend class ArrayBuffer;
  };

  size_t ByteLength() const;
  static Local<ArrayBuffer> New(Isolate* isolate, size_t byte_length);
  static Local<ArrayBuffer> New(
      Isolate* isolate, void* data, size_t byte_length,
      ArrayBufferCreationMode mode = ArrayBufferCreationMode::kExternalized);

  bool IsExternal() const;
  bool IsNeuterable() const;
  void Neuter();
  Contents Externalize();
  Contents GetContents();

  static ArrayBuffer* Cast(Value* obj);

  static const int kInternalFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;

 private:
  ArrayBuffer();
};

#ifndef V8_ARRAY_BUFFER_VIEW_INTERNAL_FIELD_COUNT
// The number of required internal fields can be defined by embedder.
#define V8_ARRAY_BUFFER_VIEW_INTERNAL_FIELD_COUNT 2
#endif

class V8_EXPORT ArrayBufferView : public Object {
 public:
  Local<ArrayBuffer> Buffer();
  size_t ByteOffset();
  size_t ByteLength();
  size_t CopyContents(void* dest, size_t byte_length);
  bool HasBuffer() const;

  static ArrayBufferView* Cast(Value* obj);

  static const int kInternalFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;

 private:
  ArrayBufferView();
};

class V8_EXPORT TypedArray : public ArrayBufferView {
 public:
  size_t Length();
  static TypedArray* Cast(Value* obj);
};

class V8_EXPORT Uint8Array : public TypedArray {
 public:
  static Local<Uint8Array> New(Handle<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Uint8Array* Cast(Value* obj);

 private:
  Uint8Array();
};

class V8_EXPORT Uint8ClampedArray : public TypedArray {
 public:
  static Local<Uint8ClampedArray> New(Handle<ArrayBuffer> array_buffer,
                                      size_t byte_offset, size_t length);
  static Uint8ClampedArray* Cast(Value* obj);

 private:
  Uint8ClampedArray();
};

class V8_EXPORT Int8Array : public TypedArray {
 public:
  static Local<Int8Array> New(Handle<ArrayBuffer> array_buffer,
                              size_t byte_offset, size_t length);
  static Int8Array* Cast(Value* obj);

 private:
  Int8Array();
};

class V8_EXPORT Uint16Array : public TypedArray {
 public:
  static Local<Uint16Array> New(Handle<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
  static Uint16Array* Cast(Value* obj);

 private:
  Uint16Array();
};

class V8_EXPORT Int16Array : public TypedArray {
 public:
  static Local<Int16Array> New(Handle<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Int16Array* Cast(Value* obj);

 private:
  Int16Array();
};

class V8_EXPORT Uint32Array : public TypedArray {
 public:
  static Local<Uint32Array> New(Handle<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
  static Uint32Array* Cast(Value* obj);

 private:
  Uint32Array();
};

class V8_EXPORT Int32Array : public TypedArray {
 public:
  static Local<Int32Array> New(Handle<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Int32Array* Cast(Value* obj);

 private:
  Int32Array();
};

class V8_EXPORT Float32Array : public TypedArray {
 public:
  static Local<Float32Array> New(Handle<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length);
  static Float32Array* Cast(Value* obj);

 private:
  Float32Array();
};

class V8_EXPORT Float64Array : public TypedArray {
 public:
  static Local<Float64Array> New(Handle<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length);
  static Float64Array* Cast(Value* obj);

 private:
  Float64Array();
};

enum AccessType {
  ACCESS_GET,
  ACCESS_SET,
  ACCESS_HAS,
  ACCESS_DELETE,
  ACCESS_KEYS
};

class V8_EXPORT SharedArrayBuffer : public Object {
 public:
  /**
   * The contents of an |SharedArrayBuffer|. Externalization of
   * |SharedArrayBuffer| returns an instance of this class, populated, with a
   * pointer to data and byte length.
   *
   * The Data pointer of SharedArrayBuffer::Contents is always allocated with
   * |ArrayBuffer::Allocator::Allocate| by the allocator specified in
   * v8::Isolate::CreateParams::array_buffer_allocator.
   *
   * This API is experimental and may change significantly.
   */
  class V8_EXPORT Contents {  // NOLINT
   public:
    Contents() : data_(NULL), byte_length_(0) {}

    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }

   private:
    void* data_;
    size_t byte_length_;

    friend class SharedArrayBuffer;
  };


  /**
   * Data length in bytes.
   */
  size_t ByteLength() const;

  /**
   * Create a new SharedArrayBuffer. Allocate |byte_length| bytes.
   * Allocated memory will be owned by a created SharedArrayBuffer and
   * will be deallocated when it is garbage-collected,
   * unless the object is externalized.
   */
  static Local<SharedArrayBuffer> New(Isolate* isolate, size_t byte_length);

  /**
   * Create a new SharedArrayBuffer over an existing memory block.  The created
   * array buffer is immediately in externalized state unless otherwise
   * specified. The memory block will not be reclaimed when a created
   * SharedArrayBuffer is garbage-collected.
   */
  static Local<SharedArrayBuffer> New(
      Isolate* isolate, void* data, size_t byte_length,
      ArrayBufferCreationMode mode = ArrayBufferCreationMode::kExternalized);

  /**
   * Returns true if SharedArrayBuffer is externalized, that is, does not
   * own its memory block.
   */
  bool IsExternal() const;

  /**
   * Make this SharedArrayBuffer external. The pointer to underlying memory
   * block and byte length are returned as |Contents| structure. After
   * SharedArrayBuffer had been externalized, it does no longer own the memory
   * block. The caller should take steps to free memory when it is no longer
   * needed.
   *
   * The memory block is guaranteed to be allocated with |Allocator::Allocate|
   * by the allocator specified in
   * v8::Isolate::CreateParams::array_buffer_allocator.
   *
   */
  Contents Externalize();

  /**
   * Get a pointer to the ArrayBuffer's underlying memory block without
   * externalizing it. If the ArrayBuffer is not externalized, this pointer
   * will become invalid as soon as the ArrayBuffer became garbage collected.
   *
   * The embedder should make sure to hold a strong reference to the
   * ArrayBuffer while accessing this pointer.
   *
   * The memory block is guaranteed to be allocated with |Allocator::Allocate|
   * by the allocator specified in
   * v8::Isolate::CreateParams::array_buffer_allocator.
   */
  Contents GetContents();

  static SharedArrayBuffer* Cast(Value* obj);

  static const int kInternalFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;

 private:
  SharedArrayBuffer();
  static void CheckCast(Value* obj);
};

/**
 * Returns true if the given context should be allowed to access the given
 * object.
 */
typedef bool (*AccessCheckCallback)(Local<Context> accessing_context,
                                    Local<Object> accessed_object,
                                    Local<Value> data);

class V8_EXPORT Template : public Data {
 public:
  void Set(Handle<String> name, Handle<Data> value,
           PropertyAttribute attributes = None);
  void Set(Isolate* isolate, const char* name, Handle<Data> value) {
    Set(v8::String::NewFromUtf8(isolate, name), value);
  }

  void SetAccessorProperty(
     Local<Name> name,
     Local<FunctionTemplate> getter = Local<FunctionTemplate>(),
     Local<FunctionTemplate> setter = Local<FunctionTemplate>(),
     PropertyAttribute attribute = None,
     AccessControl settings = DEFAULT);

 protected:
  // Callers are expected to put the JSContext in the right compartment before
  // calling this function.
  static Local<Template> New(Isolate* isolate, JSContext* cx,
                             const JSClass* jsclass = nullptr);

 private:
  // We treat Templates as JS::Values containing a JSObject* that we can
  // access properties on, etc, and we root them in the same way that we
  // root v8::Value.
  // TODO: Consider lifting this up into v8::Data once we have implemented
  // all of the Data subclasses.
  char spidershim_padding[8];
};
static_assert(sizeof(v8::Template) == 8,
              "v8::Template must be the same size as JS::Value");

class V8_EXPORT FunctionTemplate : public Template {
 public:
  static Local<FunctionTemplate> New(
      Isolate* isolate, FunctionCallback callback = 0,
      Handle<Value> data = Handle<Value>(),
      Handle<Signature> signature = Handle<Signature>(), int length = 0,
      ConstructorBehavior behavior = ConstructorBehavior::kAllow);


  V8_DEPRECATE_SOON("Use maybe version", Local<Function> GetFunction());
  V8_WARN_UNUSED_RESULT MaybeLocal<Function> GetFunction(
      Local<Context> context);

  Local<ObjectTemplate> InstanceTemplate();
  Local<ObjectTemplate> PrototypeTemplate();
  void SetClassName(Handle<String> name);
  void SetHiddenPrototype(bool value);

  /**
   * Similar to Context::NewRemoteContext, this creates an instance that
   * isn't backed by an actual object.
   *
   * The InstanceTemplate of this FunctionTemplate must have access checks with
   * handlers installed.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewRemoteInstance();

  void SetCallHandler(FunctionCallback callback,
                      Handle<Value> data = Handle<Value>());
  bool HasInstance(Handle<Value> object);
  void Inherit(Handle<FunctionTemplate> parent);

 private:
  friend class internal::FunctionCallback;
  friend struct internal::SignatureChecker;
  friend class Template;
  friend class ObjectTemplate;
  friend class Context;

  // Callers are expected to put the JSContext in the right compartment before
  // making this call.
  static Local<FunctionTemplate> New(
      Isolate* isolate, JSContext* cx, FunctionCallback callback = 0,
      Handle<Value> data = Handle<Value>(),
      Handle<Signature> signature = Handle<Signature>(), int length = 0);
  Local<ObjectTemplate> FetchOrCreateTemplate(size_t slotIndex);
  Local<Object> CreateNewInstance();
  static Local<Value> MaybeConvertObjectProperty(Local<Value> value,
                                                 Local<String> name);
  void SetInstanceTemplate(Local<ObjectTemplate> instanceTemplate);
  Handle<String> GetClassName();
  // Return the object that should be used as a prototype by ObjectTemplates
  // that have this FunctionTemplate as their constructor.  Can return an empty
  // Local on failure.
  Local<Object> GetProtoInstance(Local<Context> context);
  // The FunctionTemplate we inherit from, if any.
  MaybeLocal<FunctionTemplate> GetParent();
  // Install properties from this template's InstanceTemplate, as well as the
  // InstanceTemplates of all ancestors, on the given object.  Returns false on
  // failure, true on success.
  bool InstallInstanceProperties(Local<Object> target);
  // Performs a signature check if needed.  Returns true and a holder object
  // unless the signature check fails.  thisObj should be set to the this object
  // that the function is being called on.
  bool CheckSignature(Local<Object> thisObj, Local<Object>& holder);
  // Returns true if thisObj has been instantiated from this FunctionTemplate
  // or one inherited from this one.
  bool IsInstance(Local<Object> thisObj);
  void SetPrototypeTemplate(Local<ObjectTemplate> protoTemplate);
};

enum class PropertyHandlerFlags {
  kNone = 0,
  kAllCanRead = 1,
  kNonMasking = 1 << 1,
  kOnlyInterceptStrings = 1 << 2,
};

struct NamedPropertyHandlerConfiguration {
  NamedPropertyHandlerConfiguration(
      GenericNamedPropertyGetterCallback getter = 0,
      GenericNamedPropertySetterCallback setter = 0,
      GenericNamedPropertyQueryCallback query = 0,
      GenericNamedPropertyDeleterCallback deleter = 0,
      GenericNamedPropertyEnumeratorCallback enumerator = 0,
      Handle<Value> data = Handle<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone);

  GenericNamedPropertyGetterCallback getter;
  GenericNamedPropertySetterCallback setter;
  GenericNamedPropertyQueryCallback query;
  GenericNamedPropertyDeleterCallback deleter;
  GenericNamedPropertyEnumeratorCallback enumerator;
  Handle<Value> data;
  PropertyHandlerFlags flags;
};

struct IndexedPropertyHandlerConfiguration {
  IndexedPropertyHandlerConfiguration(
      IndexedPropertyGetterCallback getter = 0,
      IndexedPropertySetterCallback setter = 0,
      IndexedPropertyQueryCallback query = 0,
      IndexedPropertyDeleterCallback deleter = 0,
      IndexedPropertyEnumeratorCallback enumerator = 0,
      Handle<Value> data = Handle<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone);

  IndexedPropertyGetterCallback getter;
  IndexedPropertySetterCallback setter;
  IndexedPropertyQueryCallback query;
  IndexedPropertyDeleterCallback deleter;
  IndexedPropertyEnumeratorCallback enumerator;
  Handle<Value> data;
  PropertyHandlerFlags flags;
};

class V8_EXPORT ObjectTemplate : public Template {
 public:
  static Local<ObjectTemplate> New(
      Isolate* isolate,
      Local<FunctionTemplate> constructor = Local<FunctionTemplate>());

  /** Get a template included in the snapshot by index. */
  static MaybeLocal<ObjectTemplate> FromSnapshot(Isolate* isolate,
                                                 size_t index);

  V8_DEPRECATE_SOON("Use maybe version", Local<Object> NewInstance());
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(Local<Context> context);

  void SetClassName(Handle<String> name);

  void SetAccessor(
      Handle<String> name, AccessorGetterCallback getter,
      AccessorSetterCallback setter = 0, Handle<Value> data = Handle<Value>(),
      AccessControl settings = DEFAULT, PropertyAttribute attribute = None,
      Handle<AccessorSignature> signature = Handle<AccessorSignature>());
  void SetAccessor(
      Handle<Name> name, AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = 0,
      Handle<Value> data = Handle<Value>(), AccessControl settings = DEFAULT,
      PropertyAttribute attribute = None,
      Handle<AccessorSignature> signature = Handle<AccessorSignature>());

  void SetNamedPropertyHandler(NamedPropertyGetterCallback getter,
                               NamedPropertySetterCallback setter = 0,
                               NamedPropertyQueryCallback query = 0,
                               NamedPropertyDeleterCallback deleter = 0,
                               NamedPropertyEnumeratorCallback enumerator = 0,
                               Handle<Value> data = Handle<Value>());
  void SetHandler(const NamedPropertyHandlerConfiguration& configuration);

  void SetIndexedPropertyHandler(
      IndexedPropertyGetterCallback getter,
      IndexedPropertySetterCallback setter = 0,
      IndexedPropertyQueryCallback query = 0,
      IndexedPropertyDeleterCallback deleter = 0,
      IndexedPropertyEnumeratorCallback enumerator = 0,
      Handle<Value> data = Handle<Value>());

  /**
   * Sets an indexed property handler on the object template.
   *
   * Whenever an indexed property is accessed on objects created from
   * this object template, the provided callback is invoked instead of
   * accessing the property directly on the JavaScript object.
   *
   * @param configuration The IndexedPropertyHandlerConfiguration that defines
   * the callbacks to invoke when accessing a property.
   */
  void SetHandler(const IndexedPropertyHandlerConfiguration& configuration);

  /**
   * Like SetAccessCheckCallback but invokes an interceptor on failed access
   * checks instead of looking up all-can-read properties. You can only use
   * either this method or SetAccessCheckCallback, but not both at the same
   * time.
   */
  void SetAccessCheckCallbackAndHandler(
      AccessCheckCallback callback,
      const NamedPropertyHandlerConfiguration& named_handler,
      const IndexedPropertyHandlerConfiguration& indexed_handler,
      Handle<Value> data = Handle<Value>());

  int InternalFieldCount();
  void SetInternalFieldCount(int value);

  /**
   * Returns true if the object will be an immutable prototype exotic object.
   */
  bool IsImmutableProto();

  /**
   * Makes the ObjectTempate for an immutable prototype exotic object, with an
   * immutable __proto__.
   */
  void SetImmutableProto();


  void SetCallAsFunctionHandler(FunctionCallback callback,
                                Handle<Value> data = Handle<Value>());

  class InstanceClass;

 private:
  friend struct FunctionCallbackData;
  friend struct FunctionTemplateData;
  friend class Utils;
  friend class FunctionTemplate;
  friend class Context;
  friend class Object;

  // Callers are expected to put the JSContext in the right compartment before
  // making this call.
  static Local<ObjectTemplate> New(
      Isolate* isolate, JSContext* cx,
      Local<FunctionTemplate> constructor = Local<FunctionTemplate>());

  template <class N, class Getter, class Setter>
  void SetAccessorInternal(Handle<N> name, Getter getter, Setter setter,
                           Handle<Value> data, AccessControl settings,
                           PropertyAttribute attribute,
                           Handle<AccessorSignature> signature);
  enum ObjectType { NormalObject, GlobalObject };
  Local<Object> NewInstance(Handle<Object> prototype,
                            ObjectType objectType);
  Handle<String> GetClassName();
  Local<FunctionTemplate> GetConstructor();

  // GetInstanceClass creates it if needed, otherwise returns the existing
  // one.  Null is returned if objects should be created with the default
  // plain-object JSClass.
  InstanceClass* GetInstanceClass(ObjectType objectType);
  static bool IsObjectFromTemplate(Local<Object> object);
  static bool ObjectHasNativeCallHandler(Local<Object> object);
  // The object argument should be an object created from an ObjectTemplate,
  // i.e.,
  // an object for which IsObjectFromTemplate() returns true.
  static Local<FunctionTemplate> GetObjectTemplateConstructor(
      Local<Object> object);
};

class V8_EXPORT External : public Value {
 public:
  static Local<Value> Wrap(void* data);
  static void* Unwrap(Handle<Value> obj);
  static bool IsExternal(const Value* obj);

  static Local<External> New(Isolate* isolate, void* value);
  static External* Cast(Value* obj);
  void* Value() const;
};

class V8_EXPORT Signature : public Data {
 public:
  static Local<Signature> New(
      Isolate* isolate,
      Handle<FunctionTemplate> receiver = Handle<FunctionTemplate>());

 private:
  // We treat signatures as JS::Values containing a FunctionTemplate, and we
  // root them in the same way that we root v8::Value.
  // TODO: Consider lifting this up into v8::Data once we have implemented
  // all of the Data subclasses.
  char spidershim_padding[8];
};
static_assert(sizeof(v8::Signature) == 8, "v8::Signature must be the same size as JS::Value");

class V8_EXPORT AccessorSignature : public Data {
 public:
  static Local<AccessorSignature> New(
      Isolate* isolate,
      Handle<FunctionTemplate> receiver = Handle<FunctionTemplate>());

 private:
  // We treat signatures as JS::Values containing a FunctionTemplate, and we
  // root them in the same way that we root v8::Value.
  // TODO: Consider lifting this up into v8::Data once we have implemented
  // all of the Data subclasses.
  char spidershim_padding[8];
};
static_assert(sizeof(v8::AccessorSignature) == 8, "v8::AccessorSignature must be the same size as JS::Value");

// --- Promise Reject Callback ---
enum PromiseRejectEvent {
  kPromiseRejectWithNoHandler = 0,
  kPromiseHandlerAddedAfterReject = 1
};

class PromiseRejectMessage {
 public:
  PromiseRejectMessage(Handle<Promise> promise, PromiseRejectEvent event,
                       Handle<Value> value, Handle<StackTrace> stack_trace);

  Handle<Promise> GetPromise() const { return promise_; }
  PromiseRejectEvent GetEvent() const { return event_; }
  Handle<Value> GetValue() const { return value_; }

  // DEPRECATED. Use v8::Exception::CreateMessage(GetValue())->GetStackTrace()
  Handle<StackTrace> GetStackTrace() const { return stack_trace_; }

 private:
  Handle<Promise> promise_;
  PromiseRejectEvent event_;
  Handle<Value> value_;
  Handle<StackTrace> stack_trace_;
};

typedef void (*FunctionEntryHook)(uintptr_t function,
                                  uintptr_t return_addr_location);
typedef int* (*CounterLookupCallback)(const char* name);
typedef void* (*CreateHistogramCallback)(const char* name, int min, int max,
                                         size_t buckets);
typedef void (*AddHistogramSampleCallback)(void* histogram, int sample);
typedef void (*PromiseRejectCallback)(PromiseRejectMessage message);
typedef void (*MicrotasksCompletedCallback)(Isolate*);
typedef void (*MicrotaskCallback)(void* data);

/**
 * Policy for running microtasks:
 *   - explicit: microtasks are invoked with Isolate::RunMicrotasks() method;
 *   - scoped: microtasks invocation is controlled by MicrotasksScope objects;
 *   - auto: microtasks are invoked when the script call depth decrements
 *           to zero.
 */
enum class MicrotasksPolicy { kExplicit, kScoped, kAuto };

/**
 * This scope is used to control microtasks when kScopeMicrotasksInvocation
 * is used on Isolate. In this mode every non-primitive call to V8 should be
 * done inside some MicrotasksScope.
 * Microtasks are executed when topmost MicrotasksScope marked as kRunMicrotasks
 * exits.
 * kDoNotRunMicrotasks should be used to annotate calls not intended to trigger
 * microtasks.
 */
class V8_EXPORT MicrotasksScope {
 public:
  enum Type { kRunMicrotasks, kDoNotRunMicrotasks };

  MicrotasksScope(Isolate* isolate, Type type);
  ~MicrotasksScope();

  /**
   * Runs microtasks if no kRunMicrotasks scope is currently active.
   */
  static void PerformCheckpoint(Isolate* isolate);

  /**
   * Returns current depth of nested kRunMicrotasks scopes.
   */
  static int GetCurrentDepth(Isolate* isolate);

  /**
   * Returns true while microtasks are being executed.
   */
  static bool IsRunningMicrotasks(Isolate* isolate);

 private:
  // Prevent copying.
  MicrotasksScope(const MicrotasksScope&);
  MicrotasksScope& operator=(const MicrotasksScope&);

  Isolate* isolate_;
  Type type_;
};

enum GarbageCollectionType { kFullGarbageCollection, kMinorGarbageCollection };

enum GCType {
  kGCTypeScavenge = 1 << 0,
  kGCTypeMarkSweepCompact = 1 << 1,
  kGCTypeAll = kGCTypeScavenge | kGCTypeMarkSweepCompact
};

enum GCCallbackFlags {
  kNoGCCallbackFlags = 0,
  kGCCallbackFlagCompacted = 1 << 0,
  kGCCallbackFlagConstructRetainedObjectInfos = 1 << 1,
  kGCCallbackFlagForced = 1 << 2,
  kGCCallbackFlagSynchronousPhantomCallbackProcessing = 1 << 3,
  kGCCallbackFlagCollectAllAvailableGarbage = 1 << 4,
  kGCCallbackFlagCollectAllExternalMemory = 1 << 5,
};

typedef void (*GCPrologueCallback)(GCType type, GCCallbackFlags flags);
typedef void (*GCEpilogueCallback)(GCType type, GCCallbackFlags flags);

class V8_EXPORT ResourceConstraints {
 public:
  void set_stack_limit(uint32_t* value) {}

  /**
   * Configures the constraints with reasonable default values based on the
   * capabilities of the current device the VM is running on.
   *
   * \param physical_memory The total amount of physical memory on the current
   *   device, in bytes.
   * \param virtual_memory_limit The amount of virtual memory on the current
   *   device, in bytes, or zero, if there is no limit.
   */
  void ConfigureDefaults(uint64_t physical_memory,
                         uint64_t virtual_memory_limit) {}
};

class V8_EXPORT Isolate {
 public:
  struct CreateParams {
    CreateParams()
        : entry_hook(NULL),
          code_event_handler(NULL),
          snapshot_blob(NULL),
          counter_lookup_callback(NULL),
          create_histogram_callback(NULL),
          add_histogram_sample_callback(NULL),
          array_buffer_allocator(NULL) {}

    FunctionEntryHook entry_hook;
    JitCodeEventHandler code_event_handler;
    ResourceConstraints constraints;
    StartupData* snapshot_blob;
    CounterLookupCallback counter_lookup_callback;
    CreateHistogramCallback create_histogram_callback;
    AddHistogramSampleCallback add_histogram_sample_callback;
    ArrayBuffer::Allocator* array_buffer_allocator;

    /**
     * Specifies an optional nullptr-terminated array of raw addresses in the
     * embedder that V8 can match against during serialization and use for
     * deserialization. This array and its content must stay valid for the
     * entire lifetime of the isolate.
     */
    intptr_t* external_references;
  };

  class V8_EXPORT Scope {
   public:
    explicit Scope(Isolate* isolate) : isolate_(isolate) { isolate->Enter(); }
    ~Scope() { isolate_->Exit(); }

   private:
    Isolate* const isolate_;
    Scope(const Scope&);
    Scope& operator=(const Scope&);
  };

  /**
   * Do not run microtasks while this scope is active, even if microtasks are
   * automatically executed otherwise.
   */
  class V8_EXPORT SuppressMicrotaskExecutionScope {
   public:
    explicit SuppressMicrotaskExecutionScope(Isolate* isolate);
    ~SuppressMicrotaskExecutionScope();

   private:
    Isolate* const isolate_;

    // Prevent copying of Scope objects.
    SuppressMicrotaskExecutionScope(const SuppressMicrotaskExecutionScope&);
    SuppressMicrotaskExecutionScope& operator=(
        const SuppressMicrotaskExecutionScope&);
  };

  static Isolate* New(const CreateParams& params);
  static Isolate* New();
  static Isolate* GetCurrent();
  typedef bool (*AbortOnUncaughtExceptionCallback)(Isolate*);
  void SetAbortOnUncaughtExceptionCallback(
      AbortOnUncaughtExceptionCallback callback);

  void Enter();
  void Exit();
  void Dispose();

  void GetHeapStatistics(HeapStatistics* heap_statistics);
  size_t NumberOfHeapSpaces();
  bool GetHeapSpaceStatistics(HeapSpaceStatistics* space_statistics,
                              size_t index);
  int64_t AdjustAmountOfExternalAllocatedMemory(int64_t change_in_bytes);
  /**
   * Returns the number of phantom handles without callbacks that were reset
   * by the garbage collector since the last call to this function.
   */
  size_t NumberOfPhantomHandleResetsSinceLastCall();

  void SetData(uint32_t slot, void* data);
  void* GetData(uint32_t slot);
  static uint32_t GetNumberOfDataSlots();
  Local<Context> GetCurrentContext();
  void SetPromiseRejectCallback(PromiseRejectCallback callback);
  void RunMicrotasks();
  void EnqueueMicrotask(Local<Function> microtask);
  void EnqueueMicrotask(MicrotaskCallback microtask, void* data = NULL);
  void SetMicrotasksPolicy(MicrotasksPolicy policy);
  V8_DEPRECATE_SOON("Use SetMicrotasksPolicy",
                    void SetAutorunMicrotasks(bool autorun));
  MicrotasksPolicy GetMicrotasksPolicy() const;
  V8_DEPRECATE_SOON("Use GetMicrotasksPolicy",
                    bool WillAutorunMicrotasks() const);
  void AddMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);
  void RemoveMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);
  void SetFatalErrorHandler(FatalErrorCallback that);
  /** Set the callback to invoke in case of OOM errors. */
  void SetOOMErrorHandler(OOMErrorCallback that);
  void SetJitCodeEventHandler(JitCodeEventOptions options,
                              JitCodeEventHandler event_handler);
  bool AddMessageListener(MessageCallback that,
                          Handle<Value> data = Handle<Value>());
  void RemoveMessageListeners(MessageCallback that);
  Local<Value> ThrowException(Local<Value> exception);
  HeapProfiler* GetHeapProfiler();
  V8_DEPRECATE_SOON("CpuProfiler should be created with CpuProfiler::New call.",
                    CpuProfiler* GetCpuProfiler());

  typedef void (*GCPrologueCallback)(Isolate* isolate, GCType type,
                                     GCCallbackFlags flags);
  typedef void (*GCEpilogueCallback)(Isolate* isolate, GCType type,
                                     GCCallbackFlags flags);
  typedef void (*GCCallback)(Isolate* isolate, GCType type,
                             GCCallbackFlags flags);
  void AddGCPrologueCallback(GCCallback callback,
                             GCType gc_type_filter = kGCTypeAll);
  void RemoveGCPrologueCallback(GCCallback callback);
  void AddGCEpilogueCallback(GCCallback callback,
                             GCType gc_type_filter = kGCTypeAll);
  void RemoveGCEpilogueCallback(GCCallback callback);
  void RequestGarbageCollectionForTesting(GarbageCollectionType type);

  bool IsExecutionTerminating();
  void CancelTerminateExecution();
  void TerminateExecution();

  void SetCounterFunction(CounterLookupCallback);
  void SetCreateHistogramFunction(CreateHistogramCallback);
  void SetAddHistogramSampleFunction(AddHistogramSampleCallback);

  bool IdleNotificationDeadline(double deadline_in_seconds);
  V8_DEPRECATE_SOON("use IdleNotificationDeadline()",
                    bool IdleNotification(int idle_time_in_ms));

  void LowMemoryNotification();
  int ContextDisposedNotification();

  /**
   * Optional notification to tell V8 the current performance requirements
   * of the embedder based on RAIL.
   * V8 uses these notifications to guide heuristics.
   * This is an unfinished experimental feature. Semantics and implementation
   * may change frequently.
   */
  void SetRAILMode(RAILMode rail_mode);

  /**
   * Returns the number of types of objects tracked in the heap at GC.
   */
  size_t NumberOfTrackedHeapObjectTypes();

  /**
   * Get statistics about objects in the heap.
   *
   * \param object_statistics The HeapObjectStatistics object to fill in
   *   statistics of objects of given type, which were live in the previous GC.
   * \param type_index The index of the type of object to fill details about,
   *   which ranges from 0 to NumberOfTrackedHeapObjectTypes() - 1.
   * \returns true on success.
   */
  bool GetHeapObjectStatisticsAtLastGC(HeapObjectStatistics* object_statistics,
                                       size_t type_index);

  /**
   * Get statistics about code and its metadata in the heap.
   *
   * \param object_statistics The HeapCodeStatistics object to fill in
   *   statistics of code, bytecode and their metadata.
   * \returns true on success.
   */
  bool GetHeapCodeAndMetadataStatistics(HeapCodeStatistics* object_statistics);

  /**
   * Check if this isolate is in use.
   * True if at least one thread Enter'ed this isolate.
   */
  bool IsInUse();

  ~Isolate();

 private:
  Isolate();

  void AddContext(Context* context);
  void PushCurrentContext(Context* context);
  void PopCurrentContext();
  void AddStackFrame(StackFrame* frame);
  void AddStackTrace(StackTrace* trace);
  void AddUnboundScript(UnboundScript* script);
  friend class ::AutoJSAPI;
  friend class Context;
  friend class MicrotasksScope;
  friend class StackFrame;
  friend class StackTrace;
  friend class SuppressMicrotaskExecutionScope;
  friend class TryCatch;
  friend class UnboundScript;
  friend class ::V8Engine;
  friend JSContext* JSContextFromIsolate(Isolate* isolate);
  template <class T>
  friend class PersistentBase;
  template <class T>
  friend class Eternal;
  friend class V8;

  Value* AddPersistent(Value* val);
  void RemovePersistent(Value* val);
  Template* AddPersistent(Template* val);
  void RemovePersistent(Template* val);
  Signature* AddPersistent(Signature* val);
  void RemovePersistent(Signature* val);
  AccessorSignature* AddPersistent(AccessorSignature* val);
  void RemovePersistent(AccessorSignature* val);
  Private* AddPersistent(Private* val);  // not supported yet
  void RemovePersistent(Private* val);   // not supported yet
  Message* AddPersistent(Message* val);
  void RemovePersistent(Message* val);
  Context* AddPersistent(Context* val) {
    // Contexts are not currently tracked by HandleScopes.
    return val;
  }
  void RemovePersistent(Context* val) {
    // Contexts are not currently tracked by HandleScopes.
  }
  StackFrame* AddPersistent(StackFrame* val) {
    // StackFrames are not currently tracked by HandleScopes.
    return val;
  }
  void RemovePersistent(StackFrame* val) {
    // StackFrames are not currently tracked by HandleScopes.
  }
  StackTrace* AddPersistent(StackTrace* val) {
    // StackTraces are not currently tracked by HandleScopes.
    return val;
  }
  void RemovePersistent(StackTrace* val) {
    // StackTraces are not currently tracked by HandleScopes.
  }
  UnboundScript* AddPersistent(UnboundScript* val) {
    // UnboundScripts are not currently tracked by HandleScopes.
    return val;
  }
  void RemovePersistent(UnboundScript* val) {
    // UnboundScripts are not currently tracked by HandleScopes.
  }

  size_t PersistentCount() const;

  Value* AddEternal(Value* val);
  Private* AddEternal(Private* val);
  Template* AddEternal(Template* val);
  Context* AddEternal(Context* val);              // not supported yet
  UnboundScript* AddEternal(UnboundScript* val);  // not supported yet

  JSContext* RuntimeContext() const;

  TryCatch* GetTopmostTryCatch() const;
  void SetTopmostTryCatch(TryCatch* val);

  int GetMicrotaskDepth() const;
  void AdjustMicrotaskDepth(int change);
#ifdef DEBUG
  int GetMicrotaskDebugDepth() const;
  void AdjustMicrotaskDebugDepth(int change);
#else
  int GetMicrotaskDebugDepth() const { return 0; }
  void AdjustMicrotaskDebugDepth(int change) {}
#endif
  bool IsRunningMicrotasks() const;
  bool IsMicrotaskExecutionSuppressed() const;
  int GetCallDepth() const;
  void AdjustCallDepth(int change);
  Local<Object> GetHiddenGlobal();

  static uintptr_t sStackSize;

  struct Impl;
  Impl* pimpl_;
};

class V8_EXPORT V8 {
 public:
  static void SetArrayBufferAllocator(ArrayBuffer::Allocator* allocator) {}
  static bool IsDead();
  static void SetFlagsFromString(const char* str, int length);
  static void SetFlagsFromCommandLine(int* argc, char** argv,
                                      bool remove_flags);
  static const char* GetVersion();
  static bool Initialize();
  static void SetEntropySource(EntropySource source);
  static void TerminateExecution(Isolate* isolate);
  static bool IsExeuctionDisabled(Isolate* isolate = nullptr);
  static void CancelTerminateExecution(Isolate* isolate);
  static bool Dispose();
  static void InitializePlatform(Platform* platform) {}
  static void ShutdownPlatform() {}
  static void FromJustIsNothing();
  static void ToLocalEmpty();
  V8_DEPRECATE_SOON(
      "Use version with default location.",
      static bool InitializeICU(const char* icu_data_file = nullptr) {
        // SpiderMonkey links against ICU statically, so there is nothing to do
        // here.
        return true;
      });
  /**
   * Initialize the ICU library bundled with V8. The embedder should only
   * invoke this method when using the bundled ICU. If V8 was compiled with
   * the ICU data in an external file and when the default location of that
   * file should be used, a path to the executable must be provided.
   * Returns true on success.
   *
   * The default is a file called icudtl.dat side-by-side with the executable.
   *
   * Optionally, the location of the data file can be provided to override the
   * default.
   */
  static bool InitializeICUDefaultLocation(const char* exec_path,
                                           const char* icu_data_file = nullptr);
  static void InitializeExternalStartupData(const char* directory_path) {
    // No data to initialize.
  }
  static void InitializeExternalStartupData(const char* natives_blob,
                                            const char* snapshot_blob) {
    // No data to initialize.
  }

  /**
   * Hand startup data to V8, in case the embedder has chosen to build
   * V8 with external startup data.
   *
   * Note:
   * - By default the startup data is linked into the V8 library, in which
   *   case this function is not meaningful.
   * - If this needs to be called, it needs to be called before V8
   *   tries to make use of its built-ins.
   * - To avoid unnecessary copies of data, V8 will point directly into the
   *   given data blob, so pretty please keep it around until V8 exit.
   * - Compression of the startup blob might be useful, but needs to
   *   handled entirely on the embedders' side.
   * - The call will abort if the data is invalid.
   */
  static void SetNativesDataBlob(StartupData* startup_blob) {}
  static void SetSnapshotDataBlob(StartupData* startup_blob) {}

 private:
  friend class Context;
  template <class K, class V, class T>
  friend class PersistentValueMapBase;
};

/**
 * Helper class to create a snapshot data blob.
 */
class SnapshotCreator {
 public:
  enum class FunctionCodeHandling { kClear, kKeep };

  /**
   * Create and enter an isolate, and set it up for serialization.
   * The isolate is either created from scratch or from an existing snapshot.
   * The caller keeps ownership of the argument snapshot.
   * \param existing_blob existing snapshot from which to create this one.
   * \param external_references a null-terminated array of external references
   *        that must be equivalent to CreateParams::external_references.
   */
  SnapshotCreator(intptr_t* external_references = nullptr,
                  StartupData* existing_blob = nullptr);

  ~SnapshotCreator();

  /**
   * \returns the isolate prepared by the snapshot creator.
   */
  Isolate* GetIsolate();

  /**
   * Add a context to be included in the snapshot blob.
   * \returns the index of the context in the snapshot blob.
   */
  size_t AddContext(Local<Context> context);

  /**
   * Add a template to be included in the snapshot blob.
   * \returns the index of the template in the snapshot blob.
   */
  size_t AddTemplate(Local<Template> template_obj);

  /**
   * Created a snapshot data blob.
   * This must not be called from within a handle scope.
   * \param function_code_handling whether to include compiled function code
   *        in the snapshot.
   * \returns { nullptr, 0 } on failure, and a startup snapshot on success. The
   *        caller acquires ownership of the data array in the return value.
   */
  StartupData CreateBlob(FunctionCodeHandling function_code_handling);

 private:
  void* data_;

  // Disallow copying and assigning.
  SnapshotCreator(const SnapshotCreator&);
  void operator=(const SnapshotCreator&);
};

class V8_EXPORT Exception {
 public:
  static Local<Value> RangeError(Handle<String> message);
  static Local<Value> ReferenceError(Handle<String> message);
  static Local<Value> SyntaxError(Handle<String> message);
  static Local<Value> TypeError(Handle<String> message);
  static Local<Value> Error(Handle<String> message);
};

class V8_EXPORT HeapStatistics {
 private:
  size_t total_heap_size_;
  size_t used_heap_size_;

 public:
  HeapStatistics() : total_heap_size_(0), used_heap_size_(0){};

  size_t total_heap_size() { return this->total_heap_size_; }
  size_t total_heap_size_executable() { return 0; }
  size_t total_physical_size() { return 0; }
  size_t total_available_size() { return 0; }
  size_t used_heap_size() { return this->used_heap_size_; }
  size_t heap_size_limit() { return 0; }
  size_t malloced_memory() { return 0; }
  size_t peak_malloced_memory() { return 0; }
  size_t does_zap_garbage() { return 0; }

  friend class Isolate;
};

class V8_EXPORT HeapSpaceStatistics {
 public:
  HeapSpaceStatistics() {}
  const char* space_name() { return ""; }
  size_t space_size() { return 0; }
  size_t space_used_size() { return 0; }
  size_t space_available_size() { return 0; }
  size_t physical_space_size() { return 0; }

 private:
  const char* space_name_;
  size_t space_size_;
  size_t space_used_size_;
  size_t space_available_size_;
  size_t physical_space_size_;

  friend class Isolate;
};

class V8_EXPORT HeapObjectStatistics {
 public:
  HeapObjectStatistics();
  const char* object_type() { return object_type_; }
  const char* object_sub_type() { return object_sub_type_; }
  size_t object_count() { return object_count_; }
  size_t object_size() { return object_size_; }

 private:
  const char* object_type_;
  const char* object_sub_type_;
  size_t object_count_;
  size_t object_size_;

  friend class Isolate;
};

class V8_EXPORT HeapCodeStatistics {
 public:
  HeapCodeStatistics();
  size_t code_and_metadata_size() { return code_and_metadata_size_; }
  size_t bytecode_and_metadata_size() { return bytecode_and_metadata_size_; }

 private:
  size_t code_and_metadata_size_;
  size_t bytecode_and_metadata_size_;

  friend class Isolate;
};

class V8_EXPORT JitCodeEvent {
 public:
  enum EventType {
    CODE_ADDED,
    CODE_MOVED,
    CODE_REMOVED,
  };

  EventType type;
  void* code_start;
  size_t code_len;
  union {
    struct {
      const char* str;
      size_t len;
    } name;
    void* new_code_start;
  };
};

class V8_EXPORT StartupData {
 public:
  const char* data;
  int raw_size;
};

template <class T>
class Maybe {
 public:
  bool IsNothing() const { return !has_value_; }
  bool IsJust() const { return has_value_; }

  // Will crash if the Maybe<> is nothing.
  V8_INLINE T ToChecked() const { return FromJust(); }

  V8_WARN_UNUSED_RESULT V8_INLINE bool To(T* out) const {
    if (V8_LIKELY(IsJust())) *out = value_;
    return IsJust();
  }

  // Will crash if the Maybe<> is nothing.
  T FromJust() const {
    if (!IsJust()) {
      V8::FromJustIsNothing();
    }
    return value_;
  }

  T FromMaybe(const T& default_value_) const {
    return has_value_ ? value_ : default_value_;
  }

  bool operator==(const Maybe& other) const {
    return (IsJust() == other.IsJust()) &&
           (!IsJust() || FromJust() == other.FromJust());
  }

  bool operator!=(const Maybe& other) const { return !operator==(other); }

 private:
  Maybe() : has_value_(false) {}
  explicit Maybe(const T& t) : has_value_(true), value_(t) {}

  bool has_value_;
  T value_;

  template <class U>
  friend Maybe<U> Nothing();
  template <class U>
  friend Maybe<U> Just(const U& u);
};

template <class T>
inline Maybe<T> Nothing() {
  return Maybe<T>();
}

template <class T>
inline Maybe<T> Just(const T& t) {
  return Maybe<T>(t);
}

class V8_EXPORT TryCatch {
 public:
  TryCatch(Isolate* isolate = nullptr);
  ~TryCatch();

  bool HasCaught() const;
  bool CanContinue() const;
  bool HasTerminated() const;
  Handle<Value> ReThrow();
  Local<Value> Exception() const;

  V8_DEPRECATE_SOON("Use maybe version.", Local<Value> StackTrace()) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> StackTrace(
      Local<Context> context) const;

  Local<v8::Message> Message() const;
  void Reset();
  void SetVerbose(bool value);
  void SetCaptureMessage(bool value);

 protected:
  friend class Function;
  friend class Object;
  friend class Script;

  void CheckReportExternalException();

  struct Impl;
  Impl* pimpl_;
};

class V8_EXPORT ExtensionConfiguration {};

class V8_EXPORT Context {
 public:
  class Scope {
   public:
    explicit V8_INLINE Scope(Local<Context> context);
    V8_INLINE ~Scope() { context_->Exit(); }

   private:
    Local<Context> context_;
  };

  Local<Object> Global();

  static Local<Context> New(
      Isolate* isolate, ExtensionConfiguration* extensions = NULL,
      MaybeLocal<ObjectTemplate> global_template = MaybeLocal<ObjectTemplate>(),
      MaybeLocal<Value> global_object = MaybeLocal<Value>());
  static Local<Context> GetCurrent();

  static MaybeLocal<Context> FromSnapshot(
      Isolate* isolate, size_t context_snapshot_index,
      ExtensionConfiguration* extensions = nullptr,
      MaybeLocal<ObjectTemplate> global_template = MaybeLocal<ObjectTemplate>(),
      MaybeLocal<Value> global_object = MaybeLocal<Value>());

  /**
   * Returns an global object that isn't backed by an actual context.
   *
   * The global template needs to have access checks with handlers installed.
   * If an existing global object is passed in, the global object is detached
   * from its context.
   *
   * Note that this is different from a detached context where all accesses to
   * the global proxy will fail. Instead, the access check handlers are invoked.
   *
   * It is also not possible to detach an object returned by this method.
   * Instead, the access check handlers need to return nothing to achieve the
   * same effect.
   *
   * It is possible, however, to create a new context from the global object
   * returned by this method.
   */
  static MaybeLocal<Object> NewRemoteContext(
      Isolate* isolate, Local<ObjectTemplate> global_template,
      MaybeLocal<Value> global_object = MaybeLocal<Value>());

  Isolate* GetIsolate();
  void* GetAlignedPointerFromEmbedderData(int index);
  void SetAlignedPointerInEmbedderData(int index, void* value);
  void SetEmbedderData(int index, Local<Value> value);
  Local<Value> GetEmbedderData(int index);
  void SetSecurityToken(Handle<Value> token);
  Handle<Value> GetSecurityToken();

  void Enter();
  void Exit();

  enum EmbedderDataFields { kDebugIdIndex = 0 };

  ~Context();

 private:
  Context(JSContext* cx);

  bool CreateGlobal(JSContext* cx, Isolate* isolate,
                    MaybeLocal<ObjectTemplate> global_template);
  void Dispose();
  friend class Isolate;
  friend JSContext* JSContextFromIsolate(Isolate* isolate);
  friend JSContext* JSContextFromContext(Context* context);

  struct Impl;
  Impl* pimpl_;
};

class V8_EXPORT Locker {
  // Don't need to implement this for Chakra
 public:
  explicit Locker(Isolate* isolate) {}
};

//
// Local<T> members
//

template <class T>
template <class S>
Local<T> Local<T>::New(S* that) {
  auto result = HandleScope::AddToScope(that);
  if (!result) {
    return Local<T>();
  }
  return Local<T>(static_cast<T*>(result));
}

template <class T>
template <class S>
Local<T> Local<T>::New(S* that, Local<Context> context) {
  auto result = HandleScope::AddToScope(that, context);
  if (!result) {
    return Local<T>();
  }
  return Local<T>(static_cast<T*>(result));
}

template <class T>
Local<T> Local<T>::New(Isolate* isolate, Local<T> that) {
  return New(isolate, *that);
}

template <class T>
Local<T> Local<T>::New(Isolate* isolate, const PersistentBase<T>& that) {
  return New(isolate, that.val_);
}

//
// Persistent<T> members
//

template <class T>
T* PersistentBase<T>::New(Isolate* isolate, T* that) {
  if (!that) {
    return nullptr;
  }
  return static_cast<T*>(isolate->AddPersistent(that));
}

template <class T, class M>
template <class S, class M2>
void Persistent<T, M>::Copy(const Persistent<S, M2>& that) {
  TYPE_CHECK(T, S);
  this->Reset();
  if (that.IsEmpty()) return;

  this->val_ = that.val_;
#if 0
  this->_weakWrapper = that._weakWrapper;
  if (val_ && !IsWeak()) {
    JsAddRef(val_, nullptr);
  }
#endif

  M::Copy(that, this);
}

template <class T>
bool PersistentBase<T>::IsNearDeath() const {
  return true;
}

template <class T>
bool PersistentBase<T>::IsWeak() const {
#if 0
  return static_cast<bool>(_weakWrapper);
#else
  return false;
#endif
}

template <class T>
void PersistentBase<T>::Reset() {
  if (this->IsEmpty() || V8::IsDead()) return;

  Isolate::GetCurrent()->RemovePersistent(this->val_);

  val_ = nullptr;
}

template <class T>
template <class S>
void PersistentBase<T>::Reset(Isolate* isolate, const Handle<S>& other) {
  TYPE_CHECK(T, S);
  Reset();
  if (other.IsEmpty()) {
    return;
  }
  this->val_ = New(isolate, other.val_);
}

template <class T>
template <class S>
void PersistentBase<T>::Reset(Isolate* isolate,
                              const PersistentBase<S>& other) {
  TYPE_CHECK(T, S);
  Reset();
  if (other.IsEmpty()) {
    return;
  }
  this->val_ = New(isolate, other.val_);
}

template <class T>
template <typename P, typename Callback>
void PersistentBase<T>::SetWeakCommon(P* parameter, Callback callback) {
  if (this->IsEmpty()) return;

#if 0
  bool wasStrong = !IsWeak();
  chakrashim::SetObjectWeakReferenceCallback(val_, callback, parameter,
                                             &_weakWrapper);
  if (wasStrong) {
    JsRelease(val_, nullptr);
  }
#endif
}

template <class T>
template <typename P>
void PersistentBase<T>::SetWeak(P* parameter,
                                typename WeakCallbackInfo<P>::Callback callback,
                                WeakCallbackType type) {
  typedef typename WeakCallbackInfo<void>::Callback Callback;
  SetWeakCommon(parameter, reinterpret_cast<Callback>(callback));
}

template <class T>
void PersistentBase<T>::SetWeak() {
  // TODO: figure out what to do here.
  return;
}

template <class T>
template <typename P>
P* PersistentBase<T>::ClearWeak() {
  if (!IsWeak()) return nullptr;

  return nullptr;
}

template <class T>
void PersistentBase<T>::MarkIndependent() {}

template <class T>
void PersistentBase<T>::SetWrapperClassId(uint16_t class_id) {}

template <class T>
template <class S>
void Eternal<T>::Set(Isolate* isolate, Local<S> handle) {
  TYPE_CHECK(T, S);
  assert(!val_);
  val_ = static_cast<T*>(isolate->AddEternal(*handle));
}

template <class T>
Local<T> Eternal<T>::Get(Isolate* isolate) {
  assert(val_);
  return Local<T>(val_);
}

template <class T>
Local<T> MaybeLocal<T>::ToLocalChecked() {
  if (V8_UNLIKELY(val_ == nullptr)) V8::ToLocalEmpty();
  return Local<T>(val_);
}

template <class T>
Isolate* FunctionCallbackInfo<T>::GetIsolate() const {
  return Isolate::GetCurrent();
}

template <class T>
Isolate* PropertyCallbackInfo<T>::GetIsolate() const {
  return Isolate::GetCurrent();
}

template <class T>
Isolate* ReturnValue<T>::GetIsolate() {
  return Isolate::GetCurrent();
}

template <class T>
template <class S>
V8_INLINE bool Local<T>::operator==(const Local<S>& that) const {
  return Value::Cast(val_)->Equals(Value::Cast(that.val_));
}

template <class T>
template <class S>
V8_INLINE bool Local<T>::operator==(const PersistentBase<S>& that) const {
  return Value::Cast(val_)->Equals(Value::Cast(that.val_));
}

template <class T>
template <class S>
V8_INLINE bool PersistentBase<T>::operator==(const Local<S>& that) const {
  return Value::Cast(val_)->Equals(Value::Cast(that.val_));
}

template <class T>
template <class S>
V8_INLINE bool PersistentBase<T>::operator==(
    const PersistentBase<S>& that) const {
  return Value::Cast(val_)->Equals(Value::Cast(that.val_));
}

template <>
template <>
V8_INLINE bool Local<Context>::operator==(const Local<Context>& that) const {
  return val_ == that.val_;
}

template <>
template <>
V8_INLINE bool Local<Context>::operator==(
    const PersistentBase<Context>& that) const {
  return val_ == that.val_;
}

template <>
template <>
V8_INLINE bool PersistentBase<Context>::operator==(
    const Local<Context>& that) const {
  return val_ == that.val_;
}

template <>
template <>
V8_INLINE bool PersistentBase<Context>::operator==(
    const PersistentBase<Context>& that) const {
  return val_ == that.val_;
}

template <>
template <>
V8_INLINE bool Local<String>::operator==(const Local<String>& that) const {
  return *val_ == *that.val_;
}

template <>
template <>
V8_INLINE bool Local<String>::operator==(
    const PersistentBase<String>& that) const {
  return *val_ == *that.val_;
}

template <>
template <>
V8_INLINE bool PersistentBase<String>::operator==(
    const Local<String>& that) const {
  return *val_ == *that.val_;
}

template <>
template <>
V8_INLINE bool PersistentBase<String>::operator==(
    const PersistentBase<String>& that) const {
  return *val_ == *that.val_;
}

V8_INLINE ScriptOrigin::ScriptOrigin(Local<Value> resource_name,
                                     Local<Integer> resource_line_offset,
                                     Local<Integer> resource_column_offset)
    : resource_name_(resource_name),
      resource_line_offset_(resource_line_offset),
      resource_column_offset_(resource_column_offset) {}

V8_INLINE Script::Script(Local<Context> context, JSScript* script)
    : script_(script), context_(context) {}

V8_INLINE ScriptCompiler::Source::Source(Local<String> source_string,
                                         const ScriptOrigin& origin,
                                         CachedData* cached_data)
    : source_string(source_string), resource_name(origin.ResourceName()) {}

V8_INLINE ScriptCompiler::Source::Source(Local<String> source_string,
                                         CachedData* cached_data)
    : source_string(source_string) {}

V8_INLINE StackFrame::StackFrame(Local<Object> frame) : frame_(frame) {}

V8_INLINE bool String::ExternalStringResourceBase::IsCompressible() const {
  return false;
}

V8_INLINE void String::ExternalStringResourceBase::Dispose() { delete this; }

V8_INLINE NamedPropertyHandlerConfiguration::NamedPropertyHandlerConfiguration(
    GenericNamedPropertyGetterCallback getter,
    GenericNamedPropertySetterCallback setter,
    GenericNamedPropertyQueryCallback query,
    GenericNamedPropertyDeleterCallback deleter,
    GenericNamedPropertyEnumeratorCallback enumerator, Handle<Value> data,
    PropertyHandlerFlags flags)
    : getter(getter),
      setter(setter),
      query(query),
      deleter(deleter),
      enumerator(enumerator),
      data(data),
      flags(flags) {}

V8_INLINE
IndexedPropertyHandlerConfiguration::IndexedPropertyHandlerConfiguration(
    IndexedPropertyGetterCallback getter, IndexedPropertySetterCallback setter,
    IndexedPropertyQueryCallback query, IndexedPropertyDeleterCallback deleter,
    IndexedPropertyEnumeratorCallback enumerator, Handle<Value> data,
    PropertyHandlerFlags flags)
    : getter(getter),
      setter(setter),
      query(query),
      deleter(deleter),
      enumerator(enumerator),
      data(data),
      flags(flags) {}

V8_INLINE PromiseRejectMessage::PromiseRejectMessage(
    Handle<Promise> promise, PromiseRejectEvent event, Handle<Value> value,
    Handle<StackTrace> stack_trace)
    : promise_(promise),
      event_(event),
      value_(value),
      stack_trace_(stack_trace) {}

V8_INLINE Context::Scope::Scope(Local<Context> context) : context_(context) {
  context_->Enter();
}
}  // namespace v8
