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
#include "jsapi.h"
#include "js/Initialization.h"

namespace v8 {

JSRuntime* V8::rt_ = nullptr;

static void ErrorReporter(JSContext* cx, const char* message,
                          JSErrorReport* report) {
  fprintf(stderr, "JS error at %s:%u: %s\n",
          report->filename ? report->filename : "<no filename>",
          (unsigned int) report->lineno,
          message);
}

bool V8::Initialize() {
  assert(!rt_);
  if (!JS_Init()) {
    return false;
  }
  const uint32_t defaultHeapSize =
    sizeof(void*) == 8 ?
      1024 * 1024 * 1024 : // 1GB
      512 * 1024 * 1024;   // 512MB
  rt_ = JS_NewRuntime(defaultHeapSize,
                      JS::DefaultNurseryBytes,
                      nullptr);
  if (!rt_) {
    return false;
  }
  JS_SetErrorReporter(rt_, ErrorReporter);
  const size_t defaultStackQuota = 128 * sizeof(size_t) * 1024;
  JS_SetNativeStackQuota(rt_, defaultStackQuota);
  return true;
}

bool V8::Dispose() {
  assert(rt_);
  JS_DestroyRuntime(rt_);
  rt_ = nullptr;
  JS_ShutDown();
  return true;
}

void V8::FromJustIsNothing() {
  MOZ_CRASH("Maybe value in FromJust() is nothing");
}

void V8::ToLocalEmpty() {
  MOZ_CRASH("MaybeLocal value in ToLocalChecked is null");
}

}
