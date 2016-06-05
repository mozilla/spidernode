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
#include "v8-debug.h"

namespace v8 {

Local<AccessorSignature> AccessorSignature::New(v8::Isolate*, v8::Local<v8::FunctionTemplate>) {
  fprintf(stderr, "AccessorSignature::New is a stub\n");
}

MaybeLocal<Value> Object::GetRealNamedProperty(v8::Local<v8::Context>, v8::Local<v8::Name>) {
  fprintf(stderr, "Object::GetRealNamedProperty is a stub\n");
}

Maybe<PropertyAttribute> Object::GetRealNamedPropertyAttributes(v8::Local<v8::Context>, v8::Local<v8::Name>) {
  fprintf(stderr, "Object::GetRealNamedPropertyAttributes is a stub\n");
}

void ObjectTemplate::SetHandler(v8::NamedPropertyHandlerConfiguration const&) {
  fprintf(stderr, "ObjectTemplate::SetHandler is a stub\n");
}

void ObjectTemplate::SetNamedPropertyHandler(void (*)(v8::Local<v8::String>, v8::PropertyCallbackInfo<v8::Value> const&), void (*)(v8::Local<v8::String>, v8::Local<v8::Value>, v8::PropertyCallbackInfo<v8::Value> const&), void (*)(v8::Local<v8::String>, v8::PropertyCallbackInfo<v8::Integer> const&), void (*)(v8::Local<v8::String>, v8::PropertyCallbackInfo<v8::Boolean> const&), void (*)(v8::PropertyCallbackInfo<v8::Array> const&), v8::Local<v8::Value>) {
  fprintf(stderr, "ObjectTemplate::SetNamedPropertyHandler is a stub\n");
}

Local<Signature> Signature::New(v8::Isolate*, v8::Local<v8::FunctionTemplate>, int, v8::Local<v8::FunctionTemplate>*) {
  fprintf(stderr, "Signature::New is a stub\n");
}

} // namespace v8
