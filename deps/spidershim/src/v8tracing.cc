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
#include <string.h>

#include "libplatform/v8-tracing.h"

namespace v8 {
namespace platform {
namespace tracing {

TraceConfig* TraceConfig::CreateDefaultTraceConfig() {
  return nullptr;
}

bool TraceConfig::IsCategoryGroupEnabled(const char* category_group) const {
  return false;
}

void TraceConfig::AddIncludedCategory(const char* included_category) {
}

void TraceConfig::AddExcludedCategory(const char* excluded_category) {
}

void TracingController::Initialize(TraceBuffer* trace_buffer) {
}

void TracingController::StartTracing(TraceConfig* trace_config) {
}

void TracingController::StopTracing() {
}

TraceObject::~TraceObject() {
}

JSONTraceWriter::JSONTraceWriter(std::ostream& stream) : stream_(stream) {
}

JSONTraceWriter::~JSONTraceWriter() {
}

void JSONTraceWriter::AppendTraceEvent(TraceObject* trace_event) {
}

void JSONTraceWriter::Flush() {}

TraceWriter* TraceWriter::CreateJSONTraceWriter(std::ostream& stream) {
  return nullptr;
}

TraceBufferChunk::TraceBufferChunk(uint32_t seq) : seq_(seq) {
}

void TraceBufferChunk::Reset(uint32_t new_seq) {}

TraceObject* TraceBufferChunk::AddTraceEvent(size_t* event_index) {
  return nullptr;
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
