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
#include "js/GCVector.h"

namespace v8 {

using ValueVector = js::GCVector<JS::Value>;
using ScriptVector = js::GCVector<JSScript*>;

// TODO: This needs to be allocated in TLS.
static HandleScope* sCurrentScope = nullptr;

struct HandleScope::Impl {
  Impl(Isolate* iso) :
    values(JSContextFromIsolate(iso),
           ValueVector(JSContextFromIsolate(iso))),
    scripts(JSContextFromIsolate(iso),
            ScriptVector(JSContextFromIsolate(iso))),
    isolate(iso)
  {
  }
  JS::Rooted<ValueVector> values;
  JS::Rooted<ScriptVector> scripts;
  std::vector<Script*> scriptObjects;
  HandleScope* prev;
  Isolate* isolate;
};

HandleScope::HandleScope(Isolate* isolate)
  : pimpl_(new Impl(isolate)) {
  pimpl_->prev = sCurrentScope;
  sCurrentScope = this;
}

HandleScope::~HandleScope() {
  assert(pimpl_);
  sCurrentScope = pimpl_->prev;
  assert(pimpl_->scripts.length() == pimpl_->scriptObjects.size());
  for (auto script : pimpl_->scriptObjects) {
    delete script;
  }
  delete pimpl_;
}

int HandleScope::NumberOfHandles(Isolate* isolate) {
  HandleScope* current = sCurrentScope;
  size_t count = 0;
  while (current) {
    assert(current->pimpl_);
    if (current->pimpl_->isolate == isolate) {
      count += current->pimpl_->values.length();
      count += current->pimpl_->scripts.length();
    }
    current = current->pimpl_->prev;
  }
  return int(count);
}

Value* HandleScope::AddToScope(Value* val) {
  if (!sCurrentScope ||
      !sCurrentScope->pimpl_->values.append(reinterpret_cast<JS::Value&>(*val))) {
    return nullptr;
  }
  return reinterpret_cast<Value*>(&sCurrentScope->pimpl_->values.back());
}

Script* HandleScope::AddToScope(JSScript* script) {
  assert(sCurrentScope->pimpl_->scripts.length() ==
         sCurrentScope->pimpl_->scriptObjects.size());
  if (!sCurrentScope ||
      !sCurrentScope->pimpl_->scripts.append(script)) {
    return nullptr;
  }
  sCurrentScope->pimpl_->scriptObjects.push_back(new Script(script));
  return sCurrentScope->pimpl_->scriptObjects.back();
}

bool EscapableHandleScope::AddToParentScope(const Value* val) {
  return sCurrentScope &&
         sCurrentScope->pimpl_->prev &&
         sCurrentScope->pimpl_->prev->pimpl_->
           values.append(reinterpret_cast<const JS::Value&>(*val));
}

}
