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
#include "v8handlescope.h"
#include "autojsapi.h"
#include "rootstore.h"
#include "mozilla/ThreadLocal.h"

namespace v8 {

static MOZ_THREAD_LOCAL(HandleScope*) sCurrentScope;

namespace internal {

bool InitializeHandleScope() { return sCurrentScope.init(); }
}

struct HandleScope::Impl : internal::RootStore {
  Impl(Isolate* iso) : RootStore(iso), isolate(iso) {}
  HandleScope* prev;
  Isolate* isolate;
};

HandleScope::HandleScope(Isolate* isolate) {
  pimpl_ = new Impl(isolate);
  pimpl_->prev = sCurrentScope.get();
  sCurrentScope.set(this);
}

HandleScope::~HandleScope() {
  assert(pimpl_);
  sCurrentScope.set(pimpl_->prev);
  delete pimpl_;
}

int HandleScope::NumberOfHandles(Isolate* isolate) {
  HandleScope* current = sCurrentScope.get();
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
  if (!sCurrentScope.get()) {
    return nullptr;
  }
  return sCurrentScope.get()->pimpl_->Add(val);
}

Template* HandleScope::AddToScope(Template* val) {
  if (!sCurrentScope.get()) {
    return nullptr;
  }
  return sCurrentScope.get()->pimpl_->Add(val);
}

Signature* HandleScope::AddToScope(Signature* val) {
  if (!sCurrentScope.get()) {
    return nullptr;
  }
  return sCurrentScope.get()->pimpl_->Add(val);
}

AccessorSignature* HandleScope::AddToScope(AccessorSignature* val) {
  if (!sCurrentScope.get()) {
    return nullptr;
  }
  return sCurrentScope.get()->pimpl_->Add(val);
}

Script* HandleScope::AddToScope(JSScript* script, Local<Context> context) {
  if (!sCurrentScope.get()) {
    return nullptr;
  }
  return sCurrentScope.get()->pimpl_->Add(script, context);
}

Private* HandleScope::AddToScope(JS::Symbol* symbol) {
  if (!sCurrentScope.get()) {
    return nullptr;
  }
  return sCurrentScope.get()->pimpl_->Add(symbol);
}

Message* HandleScope::AddToScope(Message* message) {
  if (!sCurrentScope.get()) {
    return nullptr;
  }
  return sCurrentScope.get()->pimpl_->Add(message);
}

template <class T, class U>
U* EscapableHandleScope::AddToParentScopeImpl(T* val) {
  return sCurrentScope.get() && sCurrentScope.get()->pimpl_->prev ?
         sCurrentScope.get()->pimpl_->prev->pimpl_->Add(val) : nullptr;
}

Value* EscapableHandleScope::AddToParentScope(Value* val) {
  return AddToParentScopeImpl<Value, Value>(val);
}

Template* EscapableHandleScope::AddToParentScope(Template* val) {
  return AddToParentScopeImpl<Template, Template>(val);
}

Signature* EscapableHandleScope::AddToParentScope(Signature* val) {
  return AddToParentScopeImpl<Signature, Signature>(val);
}

AccessorSignature* EscapableHandleScope::AddToParentScope(AccessorSignature* val) {
  return AddToParentScopeImpl<AccessorSignature, AccessorSignature>(val);
}

Private* EscapableHandleScope::AddToParentScope(JS::Symbol* priv) {
  return AddToParentScopeImpl<JS::Symbol, Private>(priv);
}

Message* EscapableHandleScope::AddToParentScope(Message* val) {
  return AddToParentScopeImpl<Message, Message>(val);
}
}
