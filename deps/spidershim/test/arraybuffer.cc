// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"

#include "gtest/gtest.h"
#include "jsapi.h"

template <typename T, int N = T::kInternalFieldCount>
static void CheckInternalFieldsAreZero(Local<T> value) {
  EXPECT_EQ(N, value->InternalFieldCount());
  for (int i = 0; i < value->InternalFieldCount(); i++) {
    EXPECT_EQ(0, value->GetInternalField(i)
                    ->Int32Value(Isolate::GetCurrent()->GetCurrentContext())
                    .FromJust());
  }
}

TEST(SpiderShim, ArrayBuffer) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ArrayBuffer> arr = ArrayBuffer::New(isolate, 0);
  EXPECT_TRUE(arr->IsArrayBuffer());
  EXPECT_EQ(0u, arr->ByteLength());
  ArrayBuffer::Contents contents = arr->GetContents();
  EXPECT_EQ(0u, contents.ByteLength());
  CheckInternalFieldsAreZero(arr);
  Local<ArrayBuffer> arr2 = ArrayBuffer::New(isolate, 2);
  EXPECT_TRUE(arr2->IsArrayBuffer());
  EXPECT_EQ(2u, arr2->ByteLength());
  contents = arr2->GetContents();
  EXPECT_EQ(2u, contents.ByteLength());
  EXPECT_EQ(2u, ArrayBuffer::Cast(*arr2->ToObject())->ByteLength());
  CheckInternalFieldsAreZero(arr2);
}

TEST(SpiderShim, ArrayBuffer_Neutering) {
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());

  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  char buf[1024];
  Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, buf, 1024);
  CheckInternalFieldsAreZero(buffer);
  buffer->Neuter();
  EXPECT_EQ(0u, buffer->ByteLength());
}

class ScopedArrayBufferContents {
 public:
  explicit ScopedArrayBufferContents(const ArrayBuffer::Contents& contents)
      : contents_(contents) {}
  ~ScopedArrayBufferContents() { free(contents_.Data()); }
  void* Data() const { return contents_.Data(); }
  size_t ByteLength() const { return contents_.ByteLength(); }

 private:
  const ArrayBuffer::Contents contents_;
};

TEST(SpiderShim, ArrayBuffer_ApiInternalToExternal) {
  // This test is based on V8's ArrayBuffer_ApiInternalToExternal test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, 1024);
  CheckInternalFieldsAreZero(ab);
  EXPECT_EQ(1024, static_cast<int>(ab->ByteLength()));
  EXPECT_TRUE(!ab->IsExternal());
  isolate->RequestGarbageCollectionForTesting(kFullGarbageCollection);

  ScopedArrayBufferContents ab_contents(ab->Externalize());
  EXPECT_TRUE(ab->IsExternal());

  EXPECT_EQ(1024, static_cast<int>(ab_contents.ByteLength()));
  uint8_t* data = static_cast<uint8_t*>(ab_contents.Data());
  EXPECT_TRUE(data != NULL);
  EXPECT_TRUE(context->Global()->Set(context, v8_str("ab"), ab).FromJust());

  Local<Value> result = CompileRun("ab.byteLength");
  EXPECT_EQ(1024, result->Int32Value(context).FromJust());

  result = CompileRun(
      "var u8 = new Uint8Array(ab);"
      "u8[0] = 0xFF;"
      "u8[1] = 0xAA;"
      "u8.length");
  EXPECT_EQ(1024, result->Int32Value(context).FromJust());
  EXPECT_EQ(0xFF, data[0]);
  EXPECT_EQ(0xAA, data[1]);
  data[0] = 0xCC;
  data[1] = 0x11;
  result = CompileRun("u8[0] + u8[1]");
  EXPECT_EQ(0xDD, result->Int32Value(context).FromJust());
}

TEST(SpiderShim, ArrayBuffer_JSInternalToExternal) {
  // This test is based on V8's ArrayBuffer_JSInternalToExternal test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  Local<Value> result = CompileRun(
      "var ab1 = new ArrayBuffer(2);"
      "var u8_a = new Uint8Array(ab1);"
      "u8_a[0] = 0xAA;"
      "u8_a[1] = 0xFF; u8_a.buffer");
  Local<ArrayBuffer> ab1 = Local<ArrayBuffer>::Cast(result);
  CheckInternalFieldsAreZero(ab1);
  EXPECT_EQ(2, static_cast<int>(ab1->ByteLength()));
  EXPECT_TRUE(!ab1->IsExternal());
  ScopedArrayBufferContents ab1_contents(ab1->Externalize());
  EXPECT_TRUE(ab1->IsExternal());

  result = CompileRun("ab1.byteLength");
  EXPECT_EQ(2, result->Int32Value(context).FromJust());
  result = CompileRun("u8_a[0]");
  EXPECT_EQ(0xAA, result->Int32Value(context).FromJust());
  result = CompileRun("u8_a[1]");
  EXPECT_EQ(0xFF, result->Int32Value(context).FromJust());
  result = CompileRun(
      "var u8_b = new Uint8Array(ab1);"
      "u8_b[0] = 0xBB;"
      "u8_a[0]");
  EXPECT_EQ(0xBB, result->Int32Value(context).FromJust());
  result = CompileRun("u8_b[1]");
  EXPECT_EQ(0xFF, result->Int32Value(context).FromJust());

  EXPECT_EQ(2, static_cast<int>(ab1_contents.ByteLength()));
  uint8_t* ab1_data = static_cast<uint8_t*>(ab1_contents.Data());
  EXPECT_EQ(0xBB, ab1_data[0]);
  EXPECT_EQ(0xFF, ab1_data[1]);
  ab1_data[0] = 0xCC;
  ab1_data[1] = 0x11;
  result = CompileRun("u8_a[0] + u8_a[1]");
  EXPECT_EQ(0xDD, result->Int32Value(context).FromJust());
}

#if 0
static void CheckDataViewIsNeutered(Local<DataView> dv) {
  EXPECT_EQ(0, static_cast<int>(dv->ByteLength()));
  EXPECT_EQ(0, static_cast<int>(dv->ByteOffset()));
}
#endif

static void CheckIsNeutered(Local<TypedArray> ta) {
  EXPECT_EQ(0, static_cast<int>(ta->ByteLength()));
  EXPECT_EQ(0, static_cast<int>(ta->Length()));
  EXPECT_EQ(0, static_cast<int>(ta->ByteOffset()));
}

static void CheckIsTypedArrayVarNeutered(const char* name) {
  char source[1024] = {'\0'};
  snprintf(source, 1024,
           "%s.byteLength == 0 && %s.byteOffset == 0 && %s.length == 0",
           name, name, name);
  EXPECT_TRUE(CompileRun(source)->IsTrue());
  Local<TypedArray> ta =
      Local<TypedArray>::Cast(CompileRun(name));
  CheckIsNeutered(ta);
}

template <typename TypedArray, int kElementSize>
static Local<TypedArray> CreateAndCheck(Local<ArrayBuffer> ab,
                                        int byteOffset, int length) {
  Local<TypedArray> ta = TypedArray::New(ab, byteOffset, length);
  CheckInternalFieldsAreZero<ArrayBufferView>(ta);
  EXPECT_EQ(byteOffset, static_cast<int>(ta->ByteOffset()));
  EXPECT_EQ(length, static_cast<int>(ta->Length()));
  EXPECT_EQ(length * kElementSize, static_cast<int>(ta->ByteLength()));
  return ta;
}

TEST(SpiderShim, ArrayBuffer_NeuteringApi) {
  // This test is based on V8's ArrayBuffer_NeuteringApi test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);
  Isolate* isolate = engine.isolate();

  Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, 1024);

  Local<Uint8Array> u8a =
      CreateAndCheck<Uint8Array, 1>(buffer, 1, 1023);
  Local<Uint8ClampedArray> u8c =
      CreateAndCheck<Uint8ClampedArray, 1>(buffer, 1, 1023);
  Local<Int8Array> i8a =
      CreateAndCheck<Int8Array, 1>(buffer, 1, 1023);

  Local<Uint16Array> u16a =
      CreateAndCheck<Uint16Array, 2>(buffer, 2, 511);
  Local<Int16Array> i16a =
      CreateAndCheck<Int16Array, 2>(buffer, 2, 511);

  Local<Uint32Array> u32a =
      CreateAndCheck<Uint32Array, 4>(buffer, 4, 255);
  Local<Int32Array> i32a =
      CreateAndCheck<Int32Array, 4>(buffer, 4, 255);

  Local<Float32Array> f32a =
      CreateAndCheck<Float32Array, 4>(buffer, 4, 255);
  Local<Float64Array> f64a =
      CreateAndCheck<Float64Array, 8>(buffer, 8, 127);

#if 0
  Local<DataView> dv = DataView::New(buffer, 1, 1023);
  CheckInternalFieldsAreZero<ArrayBufferView>(dv);
  EXPECT_EQ(1, static_cast<int>(dv->ByteOffset()));
  EXPECT_EQ(1023, static_cast<int>(dv->ByteLength()));
#endif

  ScopedArrayBufferContents contents(buffer->Externalize());
  buffer->Neuter();
  EXPECT_EQ(0, static_cast<int>(buffer->ByteLength()));
  CheckIsNeutered(u8a);
  CheckIsNeutered(u8c);
  CheckIsNeutered(i8a);
  CheckIsNeutered(u16a);
  CheckIsNeutered(i16a);
  CheckIsNeutered(u32a);
  CheckIsNeutered(i32a);
  CheckIsNeutered(f32a);
  CheckIsNeutered(f64a);
#if 0
  CheckDataViewIsNeutered(dv);
#endif
}

TEST(SpiderShim, ArrayBuffer_NeuteringScript) {
  // This test is based on V8's ArrayBuffer_NeuteringScript test.
  V8Engine engine;

  Isolate::Scope isolate_scope(engine.isolate());
  HandleScope handle_scope(engine.isolate());
  Local<Context> context = Context::New(engine.isolate());
  Context::Scope context_scope(context);

  CompileRun(
      "var ab = new ArrayBuffer(1024);"
      "var u8a = new Uint8Array(ab, 1, 1023);"
      "var u8c = new Uint8ClampedArray(ab, 1, 1023);"
      "var i8a = new Int8Array(ab, 1, 1023);"
      "var u16a = new Uint16Array(ab, 2, 511);"
      "var i16a = new Int16Array(ab, 2, 511);"
      "var u32a = new Uint32Array(ab, 4, 255);"
      "var i32a = new Int32Array(ab, 4, 255);"
      "var f32a = new Float32Array(ab, 4, 255);"
      "var f64a = new Float64Array(ab, 8, 127);"
      );//"var dv = new DataView(ab, 1, 1023);");

  Local<ArrayBuffer> ab =
      Local<ArrayBuffer>::Cast(CompileRun("ab"));

  //Local<DataView> dv = Local<DataView>::Cast(CompileRun("dv"));

  ScopedArrayBufferContents contents(ab->Externalize());
  ab->Neuter();
  EXPECT_EQ(0, static_cast<int>(ab->ByteLength()));
  EXPECT_EQ(0, v8_run_int32value(v8_compile("ab.byteLength")));

  CheckIsTypedArrayVarNeutered("u8a");
  CheckIsTypedArrayVarNeutered("u8c");
  CheckIsTypedArrayVarNeutered("i8a");
  CheckIsTypedArrayVarNeutered("u16a");
  CheckIsTypedArrayVarNeutered("i16a");
  CheckIsTypedArrayVarNeutered("u32a");
  CheckIsTypedArrayVarNeutered("i32a");
  CheckIsTypedArrayVarNeutered("f32a");
  CheckIsTypedArrayVarNeutered("f64a");

#if 0
  EXPECT_TRUE(CompileRun("dv.byteLength == 0 && dv.byteOffset == 0")->IsTrue());
  CheckDataViewIsNeutered(dv);
#endif
}
