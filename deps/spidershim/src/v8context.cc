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
#include "v8context.h"
#include "jsapi.h"

namespace v8 {

Context::Context()
  : pimpl_(new Impl()) {
}

Context::~Context() {
  delete pimpl_;
}

Local<Context> Context::New(Isolate* isolate,
                            ExtensionConfiguration* extensions,
                            Handle<ObjectTemplate> global_template,
                            Handle<Value> global_object) {
  //TODO: Implement extensions, global_template and global_object.
  assert(isolate->Runtime());
  JSContext* cx = JS_NewContext(isolate->Runtime(), 32 * 1024);
  if (!cx) {
    return Local<Context>();
  }
  Context* context = new Context();
  context->pimpl_->cx = cx;
  JSAutoRequest ar(cx);
  if (!context->CreateGlobal(isolate)) {
    return Local<Context>();
  }
  isolate->AddContext(context);
  return Local<Context>::New(isolate, context);
}

bool Context::CreateGlobal(Isolate* isolate) {
  static const JSClassOps cOps = {
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
  };
  static const JSClass globalClass = {
    "global", JSCLASS_GLOBAL_FLAGS, &cOps
  };

  JS::RootedObject newGlobal(pimpl_->cx);
  JS::CompartmentOptions options;
  options.behaviors().setVersion(JSVERSION_LATEST);
  newGlobal = JS_NewGlobalObject(pimpl_->cx, &globalClass, nullptr,
                                 JS::FireOnNewGlobalHook, options);
  if (!newGlobal) {
    return false;
  }

  JSAutoCompartment ac(pimpl_->cx, newGlobal);

  if (!JS_InitStandardClasses(pimpl_->cx, newGlobal)) {
    return false;
  }

  pimpl_->global.init(isolate->Runtime());
  pimpl_->global = newGlobal;
  return true;
}

void Context::Dispose() {
  assert(pimpl_);
  assert(pimpl_->cx);
  JS_DestroyContext(pimpl_->cx);
  pimpl_->cx = nullptr;
}

void Context::Enter() {
  assert(pimpl_);
  assert(pimpl_->cx);
  assert(pimpl_->global);
  assert(!pimpl_->oldCompartment);
  JS_BeginRequest(pimpl_->cx);
  pimpl_->oldCompartment =
    JS_EnterCompartment(pimpl_->cx, pimpl_->global);
  GetIsolate()->PushCurrentContext(this);
}

void Context::Exit() {
  assert(pimpl_);
  assert(pimpl_->cx);
  // pimpl_->oldCompartment can be nullptr.
  JS_LeaveCompartment(pimpl_->cx, pimpl_->oldCompartment);
  JS_EndRequest(pimpl_->cx);
  GetIsolate()->PopCurrentContext();
}

Isolate* Context::GetIsolate() {
  return Isolate::GetCurrent();
}

JSContext* JSContextFromContext(Context* context) {
  return context->pimpl_->cx;
}

}
