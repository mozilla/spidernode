// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"

TEST(SpiderShim, ObjectTemplate) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<ObjectTemplate> templ = ObjectTemplate::New(engine.isolate());
  templ->Set(v8_str("name1"), v8_str("value1"));
  templ->Set(v8_str("name2"), v8_str("value2"), ReadOnly);
}
