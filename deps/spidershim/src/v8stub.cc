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

// class UnboundScript;

Local<AccessorSignature> AccessorSignature::New(v8::Isolate*, v8::Local<v8::FunctionTemplate>) {
  fprintf(stderr, "AccessorSignature::New is a stub\n");
}

Local<Context> Debug::GetDebugContext(v8::Isolate*) {
  fprintf(stderr, "Debug::GetDebugContext is a stub\n");
}

Local<Function> FunctionTemplate::GetFunction() {
  fprintf(stderr, "FunctionTemplate::GetFunction is a stub\n");
}

bool FunctionTemplate::HasInstance(v8::Local<v8::Value>) {
  fprintf(stderr, "FunctionTemplate::HasInstance is a stub\n");
}

Local<ObjectTemplate> FunctionTemplate::InstanceTemplate() {
  fprintf(stderr, "FunctionTemplate::InstanceTemplate is a stub\n");
}

Local<FunctionTemplate> FunctionTemplate::New(v8::Isolate*, void (*)(v8::FunctionCallbackInfo<v8::Value> const&), v8::Local<v8::Value>, v8::Local<v8::Signature>, int) {
  fprintf(stderr, "FunctionTemplate::New is a stub\n");
}

Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
  fprintf(stderr, "FunctionTemplate::PrototypeTemplate is a stub\n");
}

void FunctionTemplate::SetClassName(v8::Local<v8::String>) {
  fprintf(stderr, "FunctionTemplate::SetClassName is a stub\n");
}

void FunctionTemplate::SetHiddenPrototype(bool) {
  fprintf(stderr, "FunctionTemplate::SetHiddenPrototype is a stub\n");
}

UnboundScript* Isolate::AddPersistent(v8::UnboundScript*) {
  fprintf(stderr, "Isolate::AddPersistent is a stub\n");
}

void Isolate::RemovePersistent(v8::UnboundScript*) {
  fprintf(stderr, "Isolate::RemovePersistent is a stub\n");
}

Local<Context> Object::CreationContext() {
  fprintf(stderr, "Object::CreationContext is a stub\n");
}

void* Object::GetAlignedPointerFromInternalField(int) {
  fprintf(stderr, "Object::GetAlignedPointerFromInternalField is a stub\n");
}

MaybeLocal<Value> Object::GetRealNamedProperty(v8::Local<v8::Context>, v8::Local<v8::Name>) {
  fprintf(stderr, "Object::GetRealNamedProperty is a stub\n");
}

Maybe<PropertyAttribute> Object::GetRealNamedPropertyAttributes(v8::Local<v8::Context>, v8::Local<v8::Name>) {
  fprintf(stderr, "Object::GetRealNamedPropertyAttributes is a stub\n");
}

Maybe<bool> Object::HasOwnProperty(v8::Local<v8::Context>, v8::Local<v8::Name>) {
  fprintf(stderr, "Object::HasOwnProperty is a stub\n");
}

int Object::InternalFieldCount() {
  fprintf(stderr, "Object::InternalFieldCount is a stub\n");
}

Maybe<bool> Object::SetAccessor(v8::Local<v8::Context>, v8::Local<v8::Name>, void (*)(v8::Local<v8::Name>, v8::PropertyCallbackInfo<v8::Value> const&), void (*)(v8::Local<v8::Name>, v8::Local<v8::Value>, v8::PropertyCallbackInfo<void> const&), v8::MaybeLocal<v8::Value>, v8::AccessControl, v8::PropertyAttribute) {
  fprintf(stderr, "Object::SetAccessor is a stub\n");
}

void Object::SetAlignedPointerInInternalField(int, void*) {
  fprintf(stderr, "Object::SetAlignedPointerInInternalField is a stub\n");
}

MaybeLocal<Object> ObjectTemplate::NewInstance(v8::Local<v8::Context>) {
  fprintf(stderr, "ObjectTemplate::NewInstance is a stub\n");
}

void ObjectTemplate::SetAccessor(v8::Local<v8::String>, void (*)(v8::Local<v8::String>, v8::PropertyCallbackInfo<v8::Value> const&), void (*)(v8::Local<v8::String>, v8::Local<v8::Value>, v8::PropertyCallbackInfo<void> const&), v8::Local<v8::Value>, v8::AccessControl, v8::PropertyAttribute, v8::Local<v8::AccessorSignature>) {
  fprintf(stderr, "ObjectTemplate::SetAccessor is a stub\n");
}

void ObjectTemplate::SetHandler(v8::NamedPropertyHandlerConfiguration const&) {
  fprintf(stderr, "ObjectTemplate::SetHandler is a stub\n");
}

void ObjectTemplate::SetInternalFieldCount(int) {
  fprintf(stderr, "ObjectTemplate::SetInternalFieldCount is a stub\n");
}

void ObjectTemplate::SetNamedPropertyHandler(void (*)(v8::Local<v8::String>, v8::PropertyCallbackInfo<v8::Value> const&), void (*)(v8::Local<v8::String>, v8::Local<v8::Value>, v8::PropertyCallbackInfo<v8::Value> const&), void (*)(v8::Local<v8::String>, v8::PropertyCallbackInfo<v8::Integer> const&), void (*)(v8::Local<v8::String>, v8::PropertyCallbackInfo<v8::Boolean> const&), void (*)(v8::PropertyCallbackInfo<v8::Array> const&), v8::Local<v8::Value>) {
  fprintf(stderr, "ObjectTemplate::SetNamedPropertyHandler is a stub\n");
}

Proxy* Proxy::Cast(v8::Value*) {
  fprintf(stderr, "Proxy::Cast is a stub\n");
}

Local<Value> Proxy::GetHandler() {
  fprintf(stderr, "Proxy::GetHandler is a stub\n");
}

Local<Object> Proxy::GetTarget() {
  fprintf(stderr, "Proxy::GetTarget is a stub\n");
}

MaybeLocal<UnboundScript> ScriptCompiler::CompileUnboundScript(v8::Isolate*, v8::ScriptCompiler::Source*, v8::ScriptCompiler::CompileOptions) {
  fprintf(stderr, "ScriptCompiler::CompileUnboundScript is a stub\n");
}

Local<Signature> Signature::New(v8::Isolate*, v8::Local<v8::FunctionTemplate>, int, v8::Local<v8::FunctionTemplate>*) {
  fprintf(stderr, "Signature::New is a stub\n");
}

bool Value::IsProxy() const {
  fprintf(stderr, "Value::IsProxy is a stub\n");
}

Local<Script> UnboundScript::BindToCurrentContext() {
  fprintf(stderr, "UnboundScript::BindToCurrentContext is a stub\n");
}

} // namespace v8
