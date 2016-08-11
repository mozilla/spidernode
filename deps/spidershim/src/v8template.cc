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
#include "autojsapi.h"
#include "jsfriendapi.h"
#include "accessor.h"
#include "v8local.h"
#include "utils.h"

namespace v8 {

Local<Template> Template::New(Isolate* isolate, JSContext* cx,
                              const JSClass* jsclass) {
  assert(cx == JSContextFromIsolate(isolate));
  JSObject* obj = JS_NewObject(cx, jsclass);
  if (!obj) {
    return Local<Template>();
  }
  JS::Value objVal;
  objVal.setObject(*obj);
  return internal::Local<Template>::NewTemplate(isolate, objVal);
}

void Template::Set(Local<String> name, Local<Data> value,
                   PropertyAttribute attributes) {
  // We have to ignore the return value since the V8 API returns void here.
  Object* thisObj = Object::Cast(GetV8Value(this));
  Local<Value> val =
    FunctionTemplate::MaybeConvertObjectProperty(value.As<Value>(), name);
  thisObj->ForceSet(name, val, attributes);
}

void Template::SetAccessorProperty(Local<Name> name,
                                   Local<FunctionTemplate> getter,
                                   Local<FunctionTemplate> setter,
                                   PropertyAttribute attribute,
                                   AccessControl settings) {
  Isolate* isolate = Isolate::GetCurrent();
  JSContext* cx = JSContextFromIsolate(isolate);
  AutoJSAPI jsAPI(cx, this);
  JS::RootedObject obj(cx, UnwrapProxyIfNeeded(GetObject(this)));

  JS::RootedObject getterObj(cx, GetObject(getter->GetFunction()));
  JS::RootedObject setterObj(cx);
  if (!setter.IsEmpty()) {
    setterObj = GetObject(setter->GetFunction());
  }
  internal::SetAccessor(cx, obj, name, getterObj, setterObj,
                        settings, attribute);
}
}
