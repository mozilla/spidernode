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
#include "v8local.h"
#include "jsapi.h"
#include "jsfriendapi.h"

namespace v8 {

int StackFrame::GetLineNumber() const {
  return Int32::Cast(*frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(),
                                                      "lineNumber")))->Value();
}

int StackFrame::GetColumn() const {
  return Int32::Cast(*frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(),
                                                      "column")))->Value();
}

int StackFrame::GetScriptId() const {
  return Int32::Cast(*frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(),
                                                      "scriptId")))->Value();
}

Local<String> StackFrame::GetScriptName() const {
  return frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(),
                                         "scriptName"))->ToString();
}

Local<String> StackFrame::GetScriptNameOrSourceURL() const {
  return frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(),
                                         "scriptNameOrSourceURL"))->ToString();
}

Local<String> StackFrame::GetFunctionName() const {
  return frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(),
                                         "functionName"))->ToString();
}

bool StackFrame::IsEval() const {
  return Boolean::Cast(*frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(),
                                                        "isEval")))->Value();
}

bool StackFrame::IsConstructor() const {
  return Boolean::Cast(*frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(),
                                                        "isConstructor")))->Value();
}

}
