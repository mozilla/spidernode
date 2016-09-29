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

#pragma once
#include "v8.h"
#include "autojsapi.h"
#include <vector>
#include <utility>

using JobQueue = JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>;
using JobQueueNative = std::vector<std::pair<v8::MicrotaskCallback, void*>>;

namespace v8 {

struct Context::Impl {
  explicit Impl(JSContext* cx)
      : oldCompartment(nullptr),
        embedderData(cx, JS::ValueVector(cx)) {
    jobQueue.init(cx, JobQueue(js::SystemAllocPolicy()));
  }
  JS::PersistentRootedObject global;
  Persistent<Object> globalObj;
  JSCompartment* oldCompartment;
  JS::PersistentRooted<JS::ValueVector> embedderData;
  JS::PersistentRooted<JobQueue> jobQueue;
  JobQueueNative jobQueueNative;
  bool RunMicrotasks();
};

JSContext* JSContextFromContext(Context* context);
}
