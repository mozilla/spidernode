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

#include <vector>
#include <stack>

#include "v8.h"
#include "v8context.h"
#include "jsapi.h"

namespace v8 {

struct Isolate::Impl {
  std::vector<Context*> contexts;
  std::stack<Context*> currentContexts;
};

Isolate* Isolate::current_ = nullptr;

Isolate::Isolate()
  : pimpl_(new Impl()) {
}

Isolate::~Isolate() {
  delete pimpl_;
}

Isolate* Isolate::New(const CreateParams& params) {
  Isolate* isolate = new Isolate();
  if (params.array_buffer_allocator) {
    V8::SetArrayBufferAllocator(params.array_buffer_allocator);
  }
  return isolate;
}

Isolate* Isolate::New() {
  return new Isolate();
}

Isolate* Isolate::GetCurrent() {
  return current_;
}

void Isolate::Enter() {
  // TODO: Multiple isolates not currently supported.
  assert(!current_);
  current_ = this;
}

void Isolate::Exit() {
  // TODO: Multiple isolates not currently supported.
  assert(current_ == this);
  current_ = nullptr;
}

void Isolate::Dispose() {
  for (auto context : pimpl_->contexts) {
    context->Dispose();
  }
  delete this;
}

void Isolate::AddContext(Context* context) {
  assert(pimpl_);
  pimpl_->contexts.push_back(context);
}

void Isolate::PushCurrentContext(Context* context) {
  assert(pimpl_);
  pimpl_->currentContexts.push(context);
}

void Isolate::PopCurrentContext() {
  assert(pimpl_);
  pimpl_->currentContexts.pop();
}

Local<Context> Isolate::GetCurrentContext() {
  assert(pimpl_);
  return Local<Context>::New(this, pimpl_->currentContexts.top());
}

JSContext* JSContextFromIsolate(v8::Isolate* isolate) {
  assert(isolate);
  assert(isolate->pimpl_);
  return isolate->pimpl_->currentContexts.top()->pimpl_->cx;
}

}
