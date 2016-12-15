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
#include "autojsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "accessor.h"

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
  if (!JS_WrapValue(cx, &val)) {
    return false;
  }
  return JS_SetPropertyById(cx, self, id, val);
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

FunctionCallback GetHiddenCallback(JSContext* cx, JS::HandleObject self) {
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
    return ValuesToCallback<FunctionCallback>(data1, data2);
  }
  return nullptr;
}

bool SetHiddenCallback(JSContext* cx, JS::HandleObject self,
                       FunctionCallback callback) {
  auto symbols = GetHiddenCallbackSymbols(cx);
  JS::RootedId id1(cx, SYMBOL_TO_JSID(symbols.first));
  JS::RootedId id2(cx, SYMBOL_TO_JSID(symbols.second));
  JS::RootedValue val1(cx);
  JS::RootedValue val2(cx);
  CallbackToValues(callback, &val1, &val2);
  return JS_SetPropertyById(cx, self, id1, val1) &&
         JS_SetPropertyById(cx, self, id2, val2);
}
}

namespace v8 {
namespace internal {

class FunctionCallback {
 public:
  static bool NativeFunctionCallback(JSContext* cx, unsigned argc, JS::Value* vp) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope handleScope(isolate); // Make sure there is _a_ handlescope on the stack!
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::RootedObject callee(cx, &args.callee());
    JS::Value calleeVal;
    calleeVal.setObject(args.callee());
    v8::Local<Function> calleeFunction =
      internal::Local<Function>::New(isolate, calleeVal);
    v8::Local<Value> data = GetHiddenCalleeData(cx, callee);
    JS::RootedValue templateVal(cx, js::GetFunctionNativeReserved(callee, 0));
    v8::Local<FunctionTemplate> templ;
    if (!templateVal.isUndefined()) {
      templ = internal::Local<FunctionTemplate>::NewTemplate(isolate,
                                                             templateVal);
    }

    v8::Local<Object> _this;
    if (args.isConstructing()) {
      if (templ.IsEmpty()) {
        JS::RootedObject newObj(cx, JS_NewPlainObject(cx));
        if (!newObj) {
          return false;
        }
        _this = internal::Local<Object>::New(isolate, JS::ObjectValue(*newObj));
      } else {
        _this = templ->CreateNewInstance();
        if (_this.IsEmpty()) {
          return false;
        }
      }
    } else {
      _this = internal::Local<Object>::New(isolate, args.computeThis(cx));
    }

    ::FunctionCallback callback = GetHiddenCallback(cx, callee);
    JS::RootedValue retval(cx);
    retval.setUndefined();
    if (callback) {
      mozilla::UniquePtr<Value*[]> v8args(new Value*[argc]);
      for (unsigned i = 0; i < argc; ++i) {
        v8::Local<Value> arg = internal::Local<Value>::New(isolate, args[i]);
        if (arg.IsEmpty()) {
          return false;
        }
        v8args[i] = *arg;
      }
      v8::Local<Object> holder;
      if (templ.IsEmpty()) {
        holder = _this;
      } else {
        if (!templ->CheckSignature(_this, holder)) {
          isolate->ThrowException(
              Exception::TypeError(String::NewFromUtf8(isolate,
                                                       kIllegalInvocation)));
          return false;
        }
      }
      FunctionCallbackInfo<Value> info(v8args.get(), argc, _this, holder,
                                       args.isConstructing(),
                                       data, calleeFunction);
      {
        // Enter the context of the callee if one is available.
        v8::Local<Context> context = calleeFunction->CreationContext();
        mozilla::Maybe<Context::Scope> scope;
        if (!context.IsEmpty()) {
          scope.emplace(context);
        }
        callback(info);
      }

      if (auto rval = info.GetReturnValue().Get()) {
        JS::RootedValue retVal(cx, *GetValue(rval));
        if (args.isConstructing()) {
          // Constructors must return an Object per ES6 spec.
          retVal.setObject(*JS::ToObject(cx, retVal));
        }
        if (!JS_WrapValue(cx, &retVal)) {
          return false;
        }
        retval.set(retVal);
      }
    }

    if (args.isConstructing()) {
      if (!retval.isNullOrUndefined()) {
        assert(retval.isObject());
        args.rval().set(retval);
      } else {
        args.rval().set(*GetValue(_this));
      }
    } else {
      args.rval().set(retval);
    }

    return !isolate->IsExecutionTerminating() && !JS_IsExceptionPending(cx);
  }
};
}

MaybeLocal<Object> Function::NewInstance(Local<Context> context, int argc,
                                         Handle<Value> argv[]) const {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  jsAPI.MarkScriptCall();
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
  AutoJSAPI jsAPI(cx, this);
  jsAPI.MarkScriptCall();
  JS::RootedValue val(cx, *GetValue(recv));
  JS::AutoValueVector args(cx);
  if (!args.reserve(argc) || !JS_WrapValue(cx, &val)) {
    return Local<Value>();
  }

  for (int i = 0; i < argc; i++) {
    JS::RootedValue val(cx, *GetValue(argv[i]));
    if (!JS_WrapValue(cx, &val)) {
      return Local<Value>();
    }
    args.infallibleAppend(val);
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
  return Call(ctx, obj, argc, argv).FromMaybe(Local<Value>());
}

Function* Function::Cast(Value* v) {
  assert(v->IsFunction());
  return static_cast<Function*>(v);
}

Local<Value> Function::GetName() const {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
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
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject thisObj(cx, GetObject(this));
  JS::RootedString str(cx, GetString(name));
  // Ignore the return value since the V8 API returns void. :(
  JS_DefineProperty(cx, thisObj, "name", str, JSPROP_READONLY);
}

MaybeLocal<Function> Function::New(Local<Context> context,
                                   FunctionCallback callback,
                                   Local<Value> data,
                                   int length,
                                   ConstructorBehavior behavior) {
  // TODO: implement behavior == ConstructorBehavior::kThrow
  if (behavior == ConstructorBehavior::kThrow) {
    fprintf(stderr, "behavior == ConstructorBehavior::kThrow is not supported yet\n");
  }

  return New(context, callback, data, length, Local<FunctionTemplate>(),
             Local<String>());
}

MaybeLocal<Function> Function::New(Local<Context> context,
                                   FunctionCallback callback,
                                   Local<Value> data,
                                   int length,
                                   Local<FunctionTemplate> templ,
                                   Local<String> name) {
  JSContext* cx = JSContextFromContext(*context);
  AutoJSAPI jsAPI(cx);
  JSFunction* func;
  auto NativeFunctionCallback =
    internal::FunctionCallback::NativeFunctionCallback;
  // It's a bit weird to always pass JSFUN_CONSTRUCTOR, but it's not clear to me
  // when we would want things we get from here to NOT be constructible...
  // Maybe when templ.IsEmpty()?  Hard to tell what chackrashim does because
  // they don't seem to implement Function::New at all and all their
  // FunctionTemplate stuff seems constructible at first glance.
  if (name.IsEmpty()) {
    func = js::NewFunctionWithReserved(cx, NativeFunctionCallback, length,
                                       JSFUN_CONSTRUCTOR, nullptr);
  } else {
    JS::RootedValue nameVal(cx, *GetValue(name));
    JS::RootedId id(cx);
    if (!JS_ValueToId(cx, nameVal, &id)) {
      return MaybeLocal<Function>();
    }
    func = js::NewFunctionByIdWithReserved(cx, NativeFunctionCallback, length,
                                           JSFUN_CONSTRUCTOR, id);
  }
  if (!func) {
    return MaybeLocal<Function>();
  }
  JS::RootedObject funobj(cx, JS_GetFunctionObject(func));
  if (!SetHiddenCalleeData(cx, funobj, data) ||
      !SetHiddenCallback(cx, funobj, callback)) {
    return MaybeLocal<Function>();
  }
  if (!templ.IsEmpty()) {
    JS::RootedValue templVal(cx, *GetValue(templ));
    if (!JS_WrapValue(cx, &templVal)) {
      return MaybeLocal<Function>();
    }
    js::SetFunctionNativeReserved(funobj, 0, templVal);
  } else {
    js::SetFunctionNativeReserved(funobj, 0, JS::UndefinedValue());
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
