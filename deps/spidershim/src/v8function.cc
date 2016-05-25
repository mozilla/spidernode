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

#include "conversions.h"
#include "v8.h"
#include "v8conversions.h"
#include "v8local.h"
#include "v8trycatch.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"

namespace {

using namespace v8;

// For now, we implement the hidden callee data using a property with a symbol
// key. This is observable to script, so if this becomes an issue in the future
// we'd need to do something more sophisticated.
JS::Symbol* GetHiddenCalleeDataSymbol(JSContext* cx) {
  JS::RootedString name(
      cx, JS_NewStringCopyZ(cx, "__spidershim_hidden_callee_data__"));
  return JS::GetSymbolFor(cx, name);
}

Local<Value> GetHiddenCalleeData(JSContext* cx, JS::HandleObject self) {
  JS::RootedId id(cx, SYMBOL_TO_JSID(GetHiddenCalleeDataSymbol(cx)));
  JS::RootedValue data(cx);
  bool hasOwn = false;
  if (JS_HasOwnPropertyById(cx, self, id, &hasOwn) && hasOwn &&
      JS_GetPropertyById(cx, self, id, &data)) {
    return internal::Local<Value>::New(Isolate::GetCurrent(), data);
  }
  return Local<Value>();
}

bool SetHiddenCalleeData(JSContext* cx, JS::HandleObject self,
                         Local<Value> data) {
  if (data.IsEmpty()) {
    return true;
  }
  JS::RootedId id(cx, SYMBOL_TO_JSID(GetHiddenCalleeDataSymbol(cx)));
  JS::RootedValue val(cx, *GetValue(*data));
  return JS_SetPropertyById(cx, self, id, val);
}

void* EncodeCallback(void* ptr1, void* ptr2) {
  return reinterpret_cast<void*>
    (reinterpret_cast<uintptr_t>(ptr1) |
     (reinterpret_cast<uintptr_t>(ptr2) >> 1));
}

// For now, we implement the hidden callback using two properties with symbol
// keys. This is observable to script, so if this becomes an issue in the future
// we'd need to do something more sophisticated.
// The reason why we need to encode this as two privates is that the max size of
// the stored private is 63 bits, and code addresses can have their least
// significant bit set.
std::pair<JS::Symbol*, JS::Symbol*> GetHiddenCallbackSymbols(JSContext* cx) {
  JS::RootedString name1(
      cx, JS_NewStringCopyZ(cx, "__spidershim_hidden_callback1__"));
  JS::RootedString name2(
      cx, JS_NewStringCopyZ(cx, "__spidershim_hidden_callback2__"));
  return std::make_pair(JS::GetSymbolFor(cx, name1),
                        JS::GetSymbolFor(cx, name2));
}

void* GetHiddenCallback(JSContext* cx, JS::HandleObject self) {
  auto symbols = GetHiddenCallbackSymbols(cx);
  JS::RootedId id1(cx, SYMBOL_TO_JSID(symbols.first));
  JS::RootedId id2(cx, SYMBOL_TO_JSID(symbols.second));
  JS::RootedValue data1(cx);
  JS::RootedValue data2(cx);
  bool hasOwn = false;
  if (JS_HasOwnPropertyById(cx, self, id1, &hasOwn) && hasOwn &&
      JS_GetPropertyById(cx, self, id1, &data1) &&
      JS_HasOwnPropertyById(cx, self, id2, &hasOwn) && hasOwn &&
      JS_GetPropertyById(cx, self, id2, &data2)) {
    assert(data2.toPrivate() == (void*)0x0 ||
           data2.toPrivate() == (void*)(0x1 << 1));
    return EncodeCallback(data1.toPrivate(), data2.toPrivate());
  }
  return nullptr;
}

bool SetHiddenCallback(JSContext* cx, JS::HandleObject self,
                       void* callback) {
  auto symbols = GetHiddenCallbackSymbols(cx);
  JS::RootedId id1(cx, SYMBOL_TO_JSID(symbols.first));
  JS::RootedId id2(cx, SYMBOL_TO_JSID(symbols.second));
  void* ptr1 = reinterpret_cast<void*>
    (reinterpret_cast<uintptr_t>(callback) & ~uintptr_t(0x1));
  void* ptr2 = reinterpret_cast<void*>
    (((reinterpret_cast<uintptr_t>(callback) & uintptr_t(0x1)) << 1));
  assert(EncodeCallback(ptr1, ptr2) == callback);
  JS::RootedValue val1(cx, JS::PrivateValue(ptr1));
  JS::RootedValue val2(cx, JS::PrivateValue(ptr2));
  return JS_SetPropertyById(cx, self, id1, val1) &&
         JS_SetPropertyById(cx, self, id2, val2);
}

bool NativeFunctionCallback(JSContext* cx, unsigned argc, JS::Value* vp) {
  Isolate* isolate = Isolate::GetCurrent();
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.calleev().isObject()) {
    return false;
  }
  JS::RootedObject callee(cx, &args.callee());
  JS::Value calleeVal;
  calleeVal.setObject(args.callee());
  Local<Function> calleeFunction =
    internal::Local<Function>::New(isolate, calleeVal);
  Local<Object> _this =
    internal::Local<Object>::New(isolate, args.thisv());
  Local<Value> data = GetHiddenCalleeData(cx, callee);
  mozilla::UniquePtr<Value*[]> v8args(new Value*[argc]);
  for (unsigned i = 0; i < argc; ++i) {
    Local<Value> arg = internal::Local<Value>::New(isolate, args[0]);
    if (arg.IsEmpty()) {
      return false;
    }
    v8args[i] = *arg;
  }
  // TODO: Figure out what we want to do for holder.  See
  // https://groups.google.com/d/msg/v8-users/Axf4hF_RfZo/hA6Mvo78AqAJ
  FunctionCallbackInfo<Value> info(v8args.get(), argc, _this, _this,
                                   args.isConstructing(),
                                   data, calleeFunction);
  FunctionCallback callback =
    reinterpret_cast<FunctionCallback>(GetHiddenCallback(cx, callee));
  if (!callback) {
    return false;
  }
  callback(info);
  if (auto rval = info.GetReturnValue().Get()) {
    args.rval().set(*GetValue(rval));
  } else {
    args.rval().setUndefined();
  }
  return !isolate->IsExecutionTerminating();
}
}

namespace v8 {

MaybeLocal<Object> Function::NewInstance(Local<Context> context, int argc,
                                         Handle<Value> argv[]) const {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::AutoValueVector args(cx);
  if (!args.reserve(argc)) {
    return MaybeLocal<Object>();
  }

  for (int i = 0; i < argc; i++) {
    args.infallibleAppend(*GetValue(argv[i]));
  }

  auto obj = ::JS_New(cx, thisObj, args);
  if (!obj) {
    return MaybeLocal<Object>();
  }
  JS::Value ret;
  ret.setObject(*obj);

  return internal::Local<Object>::New(isolate, ret);
}

MaybeLocal<Value> Function::Call(Local<Context> context, Local<Value> recv,
                                 int argc, Local<Value> argv[]) {
  Isolate* isolate = context->GetIsolate();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedValue val(cx, *GetValue(recv));
  JS::AutoValueVector args(cx);
  if (!args.reserve(argc)) {
    return Local<Value>();
  }

  for (int i = 0; i < argc; i++) {
    args.infallibleAppend(*GetValue(argv[i]));
  }

  JS::RootedValue func(cx, *GetValue(this));
  JS::RootedValue ret(cx);
  internal::TryCatch tryCatch(isolate);
  if (!JS::Call(cx, val, func, args, &ret)) {
    tryCatch.CheckReportExternalException();
    return Local<Value>();
  }

  return internal::Local<Value>::New(isolate, ret);
}

Local<Value> Function::Call(Local<Value> obj, int argc, Local<Value> argv[]) {
  Local<Context> ctx = Isolate::GetCurrent()->GetCurrentContext();
  return Call(ctx, obj, argc, argv).ToLocalChecked();
}

Function* Function::Cast(Value* v) {
  assert(v->IsFunction());
  return static_cast<Function*>(v);
}

Local<Value> Function::GetName() const {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedValue nameVal(cx);
  if (!JS_GetProperty(cx, thisObj, "name", &nameVal)) {
    return Local<Value>();
  }
  JS::Value retVal;
  retVal.setString(JS::ToString(cx, nameVal));
  return internal::Local<Value>::New(isolate, retVal);
}

void Function::SetName(Local<String> name) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedString str(cx, GetString(name));
  // Ignore the return value since the V8 API returns void. :(
  JS_DefineProperty(cx, thisObj, "name", str, JSPROP_READONLY);
}

MaybeLocal<Function> Function::New(Local<Context> context,
                                   FunctionCallback callback,
                                   Local<Value> data,
                                   int length) {
  assert(callback);
  JSContext* cx = JSContextFromContext(*context);
  JSFunction* func = JS_NewFunction(cx, NativeFunctionCallback, length, 0, nullptr);
  if (!func) {
    return MaybeLocal<Function>();
  }
  JS::RootedObject funobj(cx, JS_GetFunctionObject(func));
  if (!SetHiddenCalleeData(cx, funobj, data) ||
      !SetHiddenCallback(cx, funobj, reinterpret_cast<void*>(callback))) {
    return MaybeLocal<Function>();
  }
  JS::Value retVal;
  retVal.setObject(*funobj);
  return internal::Local<Function>::New(context->GetIsolate(), retVal);
}

Local<Function> Function::New(Isolate* isolate,
                              FunctionCallback callback,
                              Local<Value> data,
                              int length) {
  Local<Context> ctx = isolate->GetCurrentContext();
  return New(ctx, callback, data, length).ToLocalChecked();
}
}
