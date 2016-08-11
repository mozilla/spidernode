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
#include "autojsapi.h"
#include "v8local.h"
#include "mozilla/UniquePtr.h"

namespace v8 {

struct UnboundScript::Impl {
  Impl(Isolate* isolate, Local<String> src, Local<Value> name)
    : source(isolate, src),
      resourceName(isolate, name) {}

  Persistent<String> source;
  Persistent<Value> resourceName;
};

UnboundScript::UnboundScript(Isolate* isolate, ScriptCompiler::Source* source)
  : pimpl_(new Impl(isolate, source->source_string, source->resource_name)) {
  isolate->AddUnboundScript(this);
}

UnboundScript::~UnboundScript() {
  delete pimpl_;
}

Local<Script> UnboundScript::BindToCurrentContext() {
  ScriptOrigin origin(pimpl_->resourceName);
  return Script::Compile(Isolate::GetCurrent()->GetCurrentContext(), 
                         pimpl_->source, &origin).
           FromMaybe(Local<Script>());
}
}
