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
#include <vector>

#include "v8.h"
#include "jsapi.h"
#include "rootstore.h"

namespace v8 {

// TODO: This needs to be allocated in TLS.
static HandleScope* sCurrentScope = nullptr;

struct HandleScope::Impl : internal::RootStore {
  Impl(Isolate* iso) :
    RootStore(iso),
    isolate(iso)
  {
  }
  HandleScope* prev;
  Isolate* isolate;
};

HandleScope::HandleScope(Isolate* isolate) {
  Local<Context> context = Context::New(isolate);
  Context::Scope scope(context);
  pimpl_ = new Impl(isolate);
  pimpl_->prev = sCurrentScope;
  sCurrentScope = this;
}

HandleScope::~HandleScope() {
  assert(pimpl_);
  sCurrentScope = pimpl_->prev;
  delete pimpl_;
}

int HandleScope::NumberOfHandles(Isolate* isolate) {
  HandleScope* current = sCurrentScope;
  size_t count = 0;
  while (current) {
    assert(current->pimpl_);
    if (current->pimpl_->isolate == isolate) {
      count += current->pimpl_->RootedCount();
    }
    current = current->pimpl_->prev;
  }
  return int(count);
}

Value* HandleScope::AddToScope(Value* val) {
  if (!sCurrentScope) {
    return nullptr;
  }
  return sCurrentScope->pimpl_->Add(val);
}

Script* HandleScope::AddToScope(JSScript* script) {
  if (!sCurrentScope) {
    return nullptr;
  }
  return sCurrentScope->pimpl_->Add(script);
}

bool EscapableHandleScope::AddToParentScope(Value* val) {
  return sCurrentScope &&
         sCurrentScope->pimpl_->prev &&
         !!sCurrentScope->pimpl_->prev->pimpl_->Add(val);
}

}
