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

namespace v8 {

MaybeLocal<UnboundScript> ScriptCompiler::CompileUnboundScript(
  Isolate* isolate, Source* source, CompileOptions options) {
  // TODO: Ignore options for now. We probably can't handle it in any
  // useful way.

  // We need to make sure that compiling the script doesn't fail with
  // a SyntaxError or some such here.  The best we can do for now is to
  // try to compile the script right now, and fail if we detect an error.
  // We unfortunately need to throw out the results here.  :(
  auto script = new UnboundScript(isolate, source);
  {
    Isolate* isolate = Isolate::GetCurrent();
    Local<Context> tmpContext = Context::New(isolate);
    Context::Scope scope(tmpContext);
    HandleScope handle_scope(isolate);
    if (script->BindToCurrentContext().IsEmpty()) {
      return Local<UnboundScript>();
    }
  }
  return Local<UnboundScript>::New(isolate, script);
}

Local<UnboundScript> ScriptCompiler::CompileUnbound(Isolate* isolate,
                                                    Source* source,
                                                    CompileOptions options) {
  return CompileUnboundScript(isolate, source, options).
           FromMaybe(Local<UnboundScript>());
}
}
