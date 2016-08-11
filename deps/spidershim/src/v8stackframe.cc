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
#include "autojsapi.h"
#include "jsfriendapi.h"

namespace v8 {

int StackFrame::GetLineNumber() const {
  Local<Value> lineNumber = frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(), "lineNumber"));
  if (lineNumber.IsEmpty() || !lineNumber->IsInt32()) {
    return 0;
  }
  return Int32::Cast(*lineNumber)->Value();
}

int StackFrame::GetColumn() const {
  Local<Value> column = frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(), "column"));
  if (column.IsEmpty() || !column->IsInt32()) {
    return 0;
  }
  return Int32::Cast(*column)->Value();
}

int StackFrame::GetScriptId() const {
  Local<Value> scriptId = frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(), "scriptId"));
  if (scriptId.IsEmpty() || !scriptId->IsInt32()) {
    return 0;
  }
  return Int32::Cast(*scriptId)->Value();
}

Local<String> StackFrame::GetScriptName() const {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Value> scriptName = frame_->Get(String::NewFromUtf8(isolate, "scriptName"));
  if (scriptName.IsEmpty() || !scriptName->IsString()) {
    return String::NewFromUtf8(isolate, "");
  }
  return scriptName->ToString();
}

Local<String> StackFrame::GetScriptNameOrSourceURL() const {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Value> nameOrURL = frame_->Get(String::NewFromUtf8(isolate, "scriptNameOrSourceURL"));
  if (nameOrURL.IsEmpty() || !nameOrURL->IsString()) {
    return String::NewFromUtf8(isolate, "");
  }
  return nameOrURL->ToString();
}

Local<String> StackFrame::GetFunctionName() const {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Value> functionName = frame_->Get(String::NewFromUtf8(isolate, "functionName"));
  if (functionName.IsEmpty() || !functionName->IsString()) {
    return String::NewFromUtf8(isolate, "");
  }
  return functionName->ToString();
}

bool StackFrame::IsEval() const {
  Local<Value> isEval = frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(), "isEval"));
  if (isEval.IsEmpty() || !isEval->IsBoolean()) {
    return false;
  }
  return Boolean::Cast(*isEval)->Value();
}

bool StackFrame::IsConstructor() const {
  Local<Value> isConstructor = frame_->Get(String::NewFromUtf8(Isolate::GetCurrent(), "isConstructor"));
  if (isConstructor.IsEmpty() || !isConstructor->IsBoolean()) {
    return false;
  }
  return Boolean::Cast(*isConstructor)->Value();
}
}
