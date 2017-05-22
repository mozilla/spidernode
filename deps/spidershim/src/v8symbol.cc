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
#include "v8conversions.h"
#include "conversions.h"
#include "v8local.h"
#include "autojsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Proxy.h"

namespace v8 {

Symbol*
Symbol::Cast(Value* v)
{
  assert(v->IsSymbol());
  return static_cast<Symbol*>(v);
}

Local<Symbol>
Symbol::New(Isolate* isolate, Local<String> name)
{
  JSContext* cx = JSContextFromIsolate(isolate);
  JS::RootedString jsName(cx, GetString(name));
  JS::Symbol* symbol = JS::NewSymbol(cx, jsName);
  JS::Value symbolVal;
  symbolVal.setSymbol(symbol);
  return internal::Local<Symbol>::New(isolate, symbolVal);
}
}
