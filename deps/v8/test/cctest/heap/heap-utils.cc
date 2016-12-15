// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/heap/heap-utils.h"

#include "src/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {
namespace heap {

void SealCurrentObjects(Heap* heap) {
  heap->CollectAllGarbage();
  heap->CollectAllGarbage();
  heap->mark_compact_collector()->EnsureSweepingCompleted();
  heap->old_space()->EmptyAllocationInfo();
  for (Page* page : *heap->old_space()) {
    page->MarkNeverAllocateForTesting();
  }
}

int FixedArrayLenFromSize(int size) {
  return (size - FixedArray::kHeaderSize) / kPointerSize;
}

std::vector<Handle<FixedArray>> CreatePadding(Heap* heap, int padding_size,
                                              PretenureFlag tenure,
                                              int object_size) {
  std::vector<Handle<FixedArray>> handles;
  Isolate* isolate = heap->isolate();
  int allocate_memory;
  int length;
  int free_memory = padding_size;
  if (tenure == i::TENURED) {
    heap->old_space()->EmptyAllocationInfo();
    int overall_free_memory = static_cast<int>(heap->old_space()->Available());
    CHECK(padding_size <= overall_free_memory || overall_free_memory == 0);
  } else {
    heap->new_space()->DisableInlineAllocationSteps();
    int overall_free_memory =
        static_cast<int>(*heap->new_space()->allocation_limit_address() -
                         *heap->new_space()->allocation_top_address());
    CHECK(padding_size <= overall_free_memory || overall_free_memory == 0);
  }
  while (free_memory > 0) {
    if (free_memory > object_size) {
      allocate_memory = object_size;
      length = FixedArrayLenFromSize(allocate_memory);
    } else {
      allocate_memory = free_memory;
      length = FixedArrayLenFromSize(allocate_memory);
      if (length <= 0) {
        // Not enough room to create another fixed array. Let's create a filler.
        if (free_memory > (2 * kPointerSize)) {
          heap->CreateFillerObjectAt(
              *heap->old_space()->allocation_top_address(), free_memory,
              ClearRecordedSlots::kNo);
        }
        break;
      }
    }
    handles.push_back(isolate->factory()->NewFixedArray(length, tenure));
    CHECK((tenure == NOT_TENURED && heap->InNewSpace(*handles.back())) ||
          (tenure == TENURED && heap->InOldSpace(*handles.back())));
    free_memory -= allocate_memory;
  }
  return handles;
}

void AllocateAllButNBytes(v8::internal::NewSpace* space, int extra_bytes,
                          std::vector<Handle<FixedArray>>* out_handles) {
  space->DisableInlineAllocationSteps();
  int space_remaining = static_cast<int>(*space->allocation_limit_address() -
                                         *space->allocation_top_address());
  CHECK(space_remaining >= extra_bytes);
  int new_linear_size = space_remaining - extra_bytes;
  if (new_linear_size == 0) return;
  std::vector<Handle<FixedArray>> handles =
      heap::CreatePadding(space->heap(), new_linear_size, i::NOT_TENURED);
  if (out_handles != nullptr)
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
}

void FillCurrentPage(v8::internal::NewSpace* space,
                     std::vector<Handle<FixedArray>>* out_handles) {
  heap::AllocateAllButNBytes(space, 0, out_handles);
}

bool FillUpOnePage(v8::internal::NewSpace* space,
                   std::vector<Handle<FixedArray>>* out_handles) {
  space->DisableInlineAllocationSteps();
  int space_remaining = static_cast<int>(*space->allocation_limit_address() -
                                         *space->allocation_top_address());
  if (space_remaining == 0) return false;
  std::vector<Handle<FixedArray>> handles =
      heap::CreatePadding(space->heap(), space_remaining, i::NOT_TENURED);
  if (out_handles != nullptr)
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
  return true;
}

void SimulateFullSpace(v8::internal::NewSpace* space,
                       std::vector<Handle<FixedArray>>* out_handles) {
  heap::FillCurrentPage(space, out_handles);
  while (heap::FillUpOnePage(space, out_handles) || space->AddFreshPage()) {
  }
}

void SimulateIncrementalMarking(i::Heap* heap, bool force_completion) {
  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking();
  }
  CHECK(marking->IsMarking());
  if (!force_completion) return;

  while (!marking->IsComplete()) {
    marking->Step(i::MB, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD);
    if (marking->IsReadyToOverApproximateWeakClosure()) {
      marking->FinalizeIncrementally();
    }
  }
  CHECK(marking->IsComplete());
}

void SimulateFullSpace(v8::internal::PagedSpace* space) {
  space->EmptyAllocationInfo();
  space->ResetFreeList();
  space->ClearStats();
}

void AbandonCurrentlyFreeMemory(PagedSpace* space) {
  space->EmptyAllocationInfo();
  for (Page* page : *space) {
    page->MarkNeverAllocateForTesting();
  }
}

void GcAndSweep(Heap* heap, AllocationSpace space) {
  heap->CollectGarbage(space);
  if (heap->mark_compact_collector()->sweeping_in_progress()) {
    heap->mark_compact_collector()->EnsureSweepingCompleted();
  }
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
