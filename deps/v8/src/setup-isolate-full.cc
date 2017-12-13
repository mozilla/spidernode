// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

#include "src/base/logging.h"
#include "src/heap/heap-inl.h"
#include "src/interpreter/interpreter.h"
#include "src/interpreter/setup-interpreter.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

void SetupIsolateDelegate::SetupBuiltins(Isolate* isolate) {
  if (create_heap_objects_) {
    SetupBuiltinsInternal(isolate);
  } else {
    DCHECK(isolate->snapshot_available());
  }
}

void SetupIsolateDelegate::SetupInterpreter(
    interpreter::Interpreter* interpreter) {
  if (create_heap_objects_) {
    interpreter::SetupInterpreter::InstallBytecodeHandlers(interpreter);
  } else {
    DCHECK(interpreter->IsDispatchTableInitialized());
  }
}

bool SetupIsolateDelegate::SetupHeap(Heap* heap) {
  if (create_heap_objects_) {
    return SetupHeapInternal(heap);
  } else {
    DCHECK(heap->isolate()->snapshot_available());
    return true;
  }
}

}  // namespace internal
}  // namespace v8
