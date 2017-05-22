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

Maybe<uint32_t>
ValueSerializer::Delegate::GetSharedArrayBufferId(Isolate*, Local<SharedArrayBuffer>)
  {
  }

void
  ValueSerializer::Delegate::FreeBufferMemory(void*)
  {
  }

Maybe<bool> ValueSerializer::Delegate::WriteHostObject(Isolate*, Local<Object>) {}
void *ValueSerializer::Delegate::ReallocateBufferMemory(void*, size_t, size_t*) {}

ValueSerializer::ValueSerializer(Isolate*, Delegate*)
{
  printf("serialization is not supported\n");
}

ValueSerializer::ValueSerializer(Isolate*)
{
  printf("serialization is not supported\n");
}

ValueSerializer::~ValueSerializer() {}

void ValueSerializer::WriteHeader() {}
void ValueSerializer::WriteDouble(double) {}
void ValueSerializer::WriteUint32(uint32_t) {}
void ValueSerializer::WriteUint64(uint64_t) {}
Maybe<bool> ValueSerializer::WriteValue(Local<Context>, Local<Value>) {}
void ValueSerializer::TransferArrayBuffer(uint32_t, Local<ArrayBuffer>) {}
void ValueSerializer::SetTreatArrayBufferViewsAsHostObjects(bool) {}
std::pair<uint8_t*, size_t> ValueSerializer::Release() {}
void ValueSerializer::WriteRawBytes(const void *, size_t) {}

MaybeLocal<Object> ValueDeserializer::Delegate::ReadHostObject(Isolate *) {}

ValueDeserializer::ValueDeserializer(Isolate*, const uint8_t *, size_t, Delegate*)
{
  printf("serialization is not supported\n");
}

ValueDeserializer::~ValueDeserializer() {}

void ValueDeserializer::TransferArrayBuffer(uint32_t, Local<ArrayBuffer>) {}
bool ValueDeserializer::ReadRawBytes(size_t, const void **) {}
Maybe<bool> ValueDeserializer::ReadHeader(Local<Context>) {}
bool ValueDeserializer::ReadUint64(uint64_t *) {}
bool ValueDeserializer::ReadUint32(uint32_t*) {}
bool ValueDeserializer::ReadDouble(double*) {}
MaybeLocal<Value> ValueDeserializer::ReadValue(Local<Context>) {}
void ValueDeserializer::TransferSharedArrayBuffer(uint32_t, Local<SharedArrayBuffer>) {}
uint32_t ValueDeserializer::GetWireFormatVersion() const {}
}
