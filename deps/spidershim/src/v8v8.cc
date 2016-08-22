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

#include "v8.h"
#include "v8handlescope.h"
#include "v8isolate.h"
#include "autojsapi.h"
#include "js/Initialization.h"

namespace v8 {

namespace internal {
bool gJSInitNeeded = false;
bool gDisposed = false;
}

bool V8::Initialize() {
  assert(!internal::gDisposed);
  internal::gJSInitNeeded = !JS_IsInitialized();
  if (internal::gJSInitNeeded && !JS_Init()) {
    return false;
  }
  return v8::internal::InitializeIsolate() &&
         v8::internal::InitializeHandleScope();
}

bool V8::Dispose() {
  assert(!internal::gDisposed);
  internal::gDisposed = true;
  if (internal::gJSInitNeeded) {
    JS_ShutDown();
  }
  return true;
}

void V8::FromJustIsNothing() {
  MOZ_CRASH("Maybe value in FromJust() is nothing");
}

void V8::ToLocalEmpty() {
  MOZ_CRASH("MaybeLocal value in ToLocalChecked is null");
}

bool V8::IsDead() { return internal::gDisposed; }

const char *V8::GetVersion() { return JS_GetImplementationVersion(); }

void V8::SetFlagsFromString(const char *str, int length) {
  // TODO: see SetFlagsFromCommandLine
}

void V8::SetFlagsFromCommandLine(int *argc, char **argv, bool remove_flags) {
  // TODO: command line arguments should be added on an as-needed basis
  for (int i = 1; i < *argc; i++) {
    const char kStackSize[] = "--stack-size=";
    if (!strncmp(argv[i], kStackSize, sizeof(kStackSize) - 1)) {
      const char* size = argv[i] + sizeof(kStackSize) - 1;
      uintptr_t kbytes = strtol(size, nullptr, 0);
      if (!kbytes) {
        fprintf(stderr, "--stack-size requires a size\n");
        continue;
      }

      Isolate::sStackSize = kbytes * 1024;
      if (remove_flags) {
        memmove(argv + i, argv + i + 1, sizeof(char*) * (*argc - i));
        (*argc)--;
        i--;
      }
    } else {
      fprintf(stderr, "invalid argument %s\n", argv[i]);
    }
  }
}

void V8::SetEntropySource(EntropySource entropy_source) {
  // TODO: https://github.com/mozilla/spidernode/issues/58
}
}
