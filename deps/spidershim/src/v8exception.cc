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

#include "v8.h"
#include "conversions.h"
#include "v8local.h"
#include "autojsapi.h"
#include "jsfriendapi.h"

namespace {

static v8::Local<v8::Value> CreateError(const char* type,
                                        v8::Handle<v8::Value> message) {
  using namespace v8;
  Isolate* isolate = Isolate::GetCurrent();
  Local<Object> globalObj = isolate->GetCurrentContext()->Global();
  Local<String> ctorName = String::NewFromUtf8(isolate, type);
  Local<Value> ctor = globalObj->Get(ctorName);
  if (!ctor->IsObject()) {
    return Local<Value>();
  }
  return Object::Cast(*ctor)->CallAsConstructor(1, &message);
}
}

namespace v8 {

Local<Value> Exception::RangeError(Handle<String> message) {
  return CreateError("RangeError", message);
}

Local<Value> Exception::ReferenceError(Handle<String> message) {
  return CreateError("ReferenceError", message);
}

Local<Value> Exception::SyntaxError(Handle<String> message) {
  return CreateError("SyntaxError", message);
}

Local<Value> Exception::TypeError(Handle<String> message) {
  return CreateError("TypeError", message);
}

Local<Value> Exception::Error(Handle<String> message) {
  return CreateError("Error", message);
}
}
