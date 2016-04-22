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

public:
  RootStore(Isolate* iso) :
    values(JSContextFromIsolate(iso)),
    scripts(JSContextFromIsolate(iso)) {}

  ~RootStore() {
    assert(scripts.size() == scriptObjects.size());
    for (auto script : scriptObjects) {
      delete script;
    }
  }

  size_t RootedCount() const {
    return values.size() + scripts.size();
  }

  Value* Add(Value* val) {
    values.push_back(reinterpret_cast<JS::Value&>(*val));
    return reinterpret_cast<Value*>(&values.back());
  }

  Script* Add(JSScript* script) {
    assert(scripts.size() == scriptObjects.size());
    scripts.push_back(script);
    scriptObjects.push_back(new Script(script));
    return scriptObjects.back();
  }

private:
  JS::Rooted<ValueVector> values;
  JS::Rooted<ScriptVector> scripts;
  std::vector<Script*> scriptObjects;
};

}

}
