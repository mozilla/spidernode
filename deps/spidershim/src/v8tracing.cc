// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <string.h>

#include "libplatform/v8-tracing.h"

namespace v8 {

class Isolate;

namespace platform {
namespace tracing {

TraceConfig* TraceConfig::CreateDefaultTraceConfig() {
  TraceConfig* trace_config = new TraceConfig();
  trace_config->included_categories_.push_back("v8");
  return trace_config;
}

bool TraceConfig::IsCategoryGroupEnabled(const char* category_group) const {
  for (auto included_category : included_categories_) {
    if (strcmp(included_category.data(), category_group) == 0) return true;
  }
  return false;
}

void TraceConfig::AddIncludedCategory(const char* included_category) {
  assert(included_category != NULL && strlen(included_category) > 0);
  included_categories_.push_back(included_category);
}

void TraceConfig::AddExcludedCategory(const char* excluded_category) {
  assert(excluded_category != NULL && strlen(excluded_category) > 0);
  excluded_categories_.push_back(excluded_category);
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
  return new JSONTraceWriter(stream);
}

TraceBufferChunk::TraceBufferChunk(uint32_t seq) : seq_(seq) {}

void TraceBufferChunk::Reset(uint32_t new_seq) {
  next_free_ = 0;
  seq_ = new_seq;
}

TraceObject* TraceBufferChunk::AddTraceEvent(size_t* event_index) {
  *event_index = next_free_++;
  return &chunk_[*event_index];
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
