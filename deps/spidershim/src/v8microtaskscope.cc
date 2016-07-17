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

namespace v8 {

MicrotasksScope::MicrotasksScope(Isolate* isolate, Type type)
  : isolate_(isolate),
    type_(type) {
  if (type_ == MicrotasksScope::kRunMicrotasks) {
    isolate_->AdjustMicrotaskDepth(+1);
  } else {
    isolate_->AdjustMicrotaskDebugDepth(+1);
  }
}

MicrotasksScope::~MicrotasksScope() {
  if (type_ == MicrotasksScope::kRunMicrotasks) {
    isolate_->AdjustMicrotaskDepth(-1);
    if (isolate_->GetMicrotasksPolicy() == MicrotasksPolicy::kScoped) {
      PerformCheckpoint(isolate_);
    }
  } else {
    isolate_->AdjustMicrotaskDebugDepth(-1);
  }
}

int MicrotasksScope::GetCurrentDepth(Isolate* isolate) {
  return isolate->GetMicrotaskDepth();
}

void MicrotasksScope::PerformCheckpoint(Isolate* isolate) {
  if (!isolate->IsExecutionTerminating() &&
      !isolate->IsMicrotaskExecutionSuppressed() &&
      !GetCurrentDepth(isolate)) {
    isolate->RunMicrotasks();
  }
}

bool MicrotasksScope::IsRunningMicrotasks(Isolate* isolate) {
  return isolate->IsRunningMicrotasks();
}
}
