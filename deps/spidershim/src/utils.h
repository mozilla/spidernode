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

#include "v8.h"
#include "autojsapi.h"
#include "js/Proxy.h"
#include "v8local.h"

namespace v8 {

inline JSObject* UnwrapProxyIfNeeded(JSObject* obj) {
  assert(obj);
  JSObject* target = obj;
  if (js::IsProxy(obj)) {
    target = js::GetProxyTargetObject(obj);
  }
  return target;
}

inline Local<Object> UnwrapProxyIfNeeded(Object* object) {
  JSObject* target = UnwrapProxyIfNeeded(GetObject(object));
  return internal::Local<Object>::New(Isolate::GetCurrent(), JS::ObjectValue(*target));
}
}
