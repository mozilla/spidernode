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

#include "v8.h"
#include "v8conversions.h"
#include "jsapi.h"
#include "jsfriendapi.h"

static_assert(sizeof(v8::Value) == sizeof(JS::Value),
              "v8::Value and JS::Value must be binary compatible");

namespace v8 {

#define SIMPLE_VALUE(V8_VAL, SM_VAL)                               \
  bool Value::Is##V8_VAL() const {                                 \
    return reinterpret_cast<const JS::Value*>(this)->is##SM_VAL(); \
  }
#define COMMON_VALUE(NAME) SIMPLE_VALUE(NAME, NAME)
#define ES_BUILTIN(V8_NAME, CLASS_NAME)                            \
  bool Value::Is##V8_NAME() const {                                \
    if (!IsObject()) {                                             \
      return false;                                                \
    }                                                              \
    JSContext* cx = JSContextFromIsolate(Isolate::GetCurrent());   \
    JS::RootedObject obj(cx,                                       \
      &reinterpret_cast<const JS::Value*>(this)->toObject());      \
    js::ESClassValue cls = js::ESClass_Other;                      \
    return js::GetBuiltinClass(cx, obj, &cls) &&                   \
           cls == js::ESClass_##CLASS_NAME;                        \
  }
#include "valuemap.inc"
#undef COMMON_VALUE
#undef SIMPLE_VALUE
#undef ES_BUILTIN

bool Value::IsUint32() const {
  if (!IsNumber()) {
    return false;
  }
  double value = reinterpret_cast<const JS::Value*>(this)->toDouble();
  return !internal::IsMinusZero(value) &&
         value >= 0 &&
         value <= internal::kMaxUInt32 &&
         value == internal::FastUI2D(internal::FastD2UI(value));
}

}
