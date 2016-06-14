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
#include "conversions.h"

namespace v8 {
namespace internal {

template <class T>
class Local {
 public:
  static v8::Local<T> New(Isolate* isolate, JS::Value val) {
    return v8::Local<T>::New(isolate, GetV8Value(&val));
  }
  static v8::Local<T> New(Isolate* isolate, JSScript* script, v8::Local<Context> context) {
    return v8::Local<T>::New(isolate, script, context);
  }
  static v8::Local<T> New(Isolate* isolate, JS::Symbol* symbol) {
    return v8::Local<T>::New(isolate, symbol);
  }
  static v8::Local<T> NewTemplate(Isolate* isolate, JS::Value val) {
    return v8::Local<T>::New(isolate, GetV8Template(&val));
  }
  static v8::Local<T> NewSignature(Isolate* isolate, JS::Value val) {
    return v8::Local<T>::New(isolate, GetV8Signature(&val));
  }
  static v8::Local<T> NewAccessorSignature(Isolate* isolate, JS::Value val) {
    return v8::Local<T>::New(isolate, GetV8AccessorSignature(&val));
  }
};
}
}
