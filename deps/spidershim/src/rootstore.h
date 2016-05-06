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
#include "gclist.h"

namespace v8 {

namespace internal {

class RootStore {
private:
  using ValueVector = GCList<JS::Value>;
  using ScriptVector = GCList<JSScript*>;
  using SymbolVector = GCList<JS::Symbol*>;

public:
  RootStore(Isolate* iso) :
    values(JSContextFromIsolate(iso)),
    scripts(JSContextFromIsolate(iso)),
    symbols(JSContextFromIsolate(iso)) {}

  ~RootStore() {
    assert(scripts.size() == scriptObjects.size());
    for (auto script : scriptObjects) {
      delete script;
    }
    assert(symbols.size() == privateObjects.size());
    for (auto priv : privateObjects) {
      delete priv;
    }
  }

  size_t RootedCount() const {
    return values.size() + scripts.size() + symbols.size();
  }

  Value* Add(Value* val) {
    values.push_back(reinterpret_cast<JS::Value&>(*val));
    return reinterpret_cast<Value*>(&values.back());
  }

  void Remove(Value* val) {
    typedef ValueVector::iterator Iter;
    for (Iter begin = values.get().begin(),
              end = values.get().end();
         begin != end; ++begin) {
      if (&*begin == reinterpret_cast<const JS::Value*>(val)) {
        values.erase(begin);
        break;
      }
    }
  }

  Script* Add(JSScript* script) {
    assert(scripts.size() == scriptObjects.size());
    scripts.push_back(script);
    scriptObjects.push_back(new Script(script));
    return scriptObjects.back();
  }

  Private* Add(JS::Symbol* symbol) {
    assert(symbols.size() == privateObjects.size());
    symbols.push_back(symbol);
    privateObjects.push_back(new Private(symbol));
    return privateObjects.back();
  }

private:
  JS::Rooted<ValueVector> values;
  JS::Rooted<ScriptVector> scripts;
  std::vector<Script*> scriptObjects;
  JS::Rooted<SymbolVector> symbols;
  std::vector<Private*> privateObjects;
};

}

}
