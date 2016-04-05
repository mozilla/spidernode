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
#include "js/CharacterEncoding.h"
#include "v8local.h"

namespace v8 {

MaybeLocal<Script> Script::Compile(Local<Context> context,
                                   Local<String> source,
                                   ScriptOrigin* origin) {
  // TODO: Add support for origin.
  assert(!origin && "The origin argument is not supported yet");
  JSContext* cx = JSContextFromContext(*context);
  auto sourceJsVal = reinterpret_cast<JS::Value*>(*source);
  auto sourceStr = sourceJsVal->toString();
  size_t length = JS_GetStringLength(sourceStr);
  auto buffer = static_cast<char16_t*>(js_malloc(sizeof(char16_t)*(length + 1)));
  mozilla::Range<char16_t> dest(buffer, length + 1);
  if (!JS_CopyStringChars(cx, dest, sourceStr)) {
    return MaybeLocal<Script>();
  }
  JS::SourceBufferHolder sbh(buffer, length,
                             JS::SourceBufferHolder::GiveOwnership);
  JS::CompileOptions options(cx);
  options.setVersion(JSVERSION_DEFAULT)
         .setNoScriptRval(false);
  JS::RootedScript jsScript(cx);
  if (!JS::Compile(cx, options, sbh, &jsScript)) {
    return MaybeLocal<Script>();
  }
  return internal::Local<Script>::New(context->GetIsolate(), jsScript);
}

}
