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
#include "js/MemoryMetrics.h"
#include "mozilla/mozalloc.h"

// DO NOT ADD ANYTHING TO THIS FILE.
// mozalloc.h overrides operator new/delete to make them use jemalloc, which
// can cause allocator mismatch issues.  See for example
// https://github.com/mozilla/spidernode/issues/119.

namespace {

class SimpleJSRuntimeStats : public JS::RuntimeStats {
  public:
    explicit SimpleJSRuntimeStats(mozilla::MallocSizeOf mallocSizeOf)
      : JS::RuntimeStats(mallocSizeOf)
    {}

    virtual void initExtraZoneStats(JS::Zone* zone, JS::ZoneStats* zStats)
        override
    {}

    virtual void initExtraCompartmentStats(
        JSCompartment* c, JS::CompartmentStats* cStats) override
    {}
};
}

namespace v8 {

void Isolate::GetHeapStatistics(HeapStatistics *heap_statistics) {
  SimpleJSRuntimeStats rtStats(moz_malloc_size_of);
  if (!JS::CollectRuntimeStats(Runtime(), &rtStats, nullptr, false)) {
    return;
  }
  heap_statistics->total_heap_size_ = rtStats.gcHeapChunkTotal;
  heap_statistics->used_heap_size_ = rtStats.gcHeapGCThings;
}
}
