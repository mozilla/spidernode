// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"

#include "src/base/atomicops.h"
#include "src/base/bits.h"
#include "src/base/sys-info.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/gdb-jit.h"
#include "src/global-handles.h"
#include "src/heap/array-buffer-tracker.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/page-parallel-job.h"
#include "src/heap/spaces-inl.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/utils-inl.h"
#include "src/v8.h"

namespace v8 {
namespace internal {


const char* Marking::kWhiteBitPattern = "00";
const char* Marking::kBlackBitPattern = "11";
const char* Marking::kGreyBitPattern = "10";
const char* Marking::kImpossibleBitPattern = "01";

// The following has to hold in order for {ObjectMarking::MarkBitFrom} to not
// produce invalid {kImpossibleBitPattern} in the marking bitmap by overlapping.
STATIC_ASSERT(Heap::kMinObjectSizeInWords >= 2);


// -------------------------------------------------------------------------
// MarkCompactCollector

MarkCompactCollector::MarkCompactCollector(Heap* heap)
    :  // NOLINT
      heap_(heap),
      page_parallel_job_semaphore_(0),
#ifdef DEBUG
      state_(IDLE),
#endif
      marking_parity_(ODD_MARKING_PARITY),
      was_marked_incrementally_(false),
      evacuation_(false),
      compacting_(false),
      black_allocation_(false),
      have_code_to_deoptimize_(false),
      marking_deque_memory_(NULL),
      marking_deque_memory_committed_(0),
      code_flusher_(nullptr),
      embedder_heap_tracer_(nullptr),
      sweeper_(heap) {
}

#ifdef VERIFY_HEAP
class VerifyMarkingVisitor : public ObjectVisitor {
 public:
  explicit VerifyMarkingVisitor(Heap* heap) : heap_(heap) {}

  void VisitPointers(Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK(heap_->mark_compact_collector()->IsMarked(object));
      }
    }
  }

  void VisitEmbeddedPointer(RelocInfo* rinfo) override {
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    if (!rinfo->host()->IsWeakObject(rinfo->target_object())) {
      Object* p = rinfo->target_object();
      VisitPointer(&p);
    }
  }

  void VisitCell(RelocInfo* rinfo) override {
    Code* code = rinfo->host();
    DCHECK(rinfo->rmode() == RelocInfo::CELL);
    if (!code->IsWeakObject(rinfo->target_cell())) {
      ObjectVisitor::VisitCell(rinfo);
    }
  }

 private:
  Heap* heap_;
};


static void VerifyMarking(Heap* heap, Address bottom, Address top) {
  VerifyMarkingVisitor visitor(heap);
  HeapObject* object;
  Address next_object_must_be_here_or_later = bottom;
  for (Address current = bottom; current < top;) {
    object = HeapObject::FromAddress(current);
    if (MarkCompactCollector::IsMarked(object)) {
      CHECK(Marking::IsBlack(ObjectMarking::MarkBitFrom(object)));
      CHECK(current >= next_object_must_be_here_or_later);
      object->Iterate(&visitor);
      next_object_must_be_here_or_later = current + object->Size();
      // The object is either part of a black area of black allocation or a
      // regular black object
      Page* page = Page::FromAddress(current);
      CHECK(
          page->markbits()->AllBitsSetInRange(
              page->AddressToMarkbitIndex(current),
              page->AddressToMarkbitIndex(next_object_must_be_here_or_later)) ||
          page->markbits()->AllBitsClearInRange(
              page->AddressToMarkbitIndex(current + kPointerSize * 2),
              page->AddressToMarkbitIndex(next_object_must_be_here_or_later)));
      current = next_object_must_be_here_or_later;
    } else {
      current += kPointerSize;
    }
  }
}

static void VerifyMarking(NewSpace* space) {
  Address end = space->top();
  // The bottom position is at the start of its page. Allows us to use
  // page->area_start() as start of range on all pages.
  CHECK_EQ(space->bottom(), Page::FromAddress(space->bottom())->area_start());

  NewSpacePageRange range(space->bottom(), end);
  for (auto it = range.begin(); it != range.end();) {
    Page* page = *(it++);
    Address limit = it != range.end() ? page->area_end() : end;
    CHECK(limit == end || !page->Contains(end));
    VerifyMarking(space->heap(), page->area_start(), limit);
  }
}


static void VerifyMarking(PagedSpace* space) {
  for (Page* p : *space) {
    VerifyMarking(space->heap(), p->area_start(), p->area_end());
  }
}


static void VerifyMarking(Heap* heap) {
  VerifyMarking(heap->old_space());
  VerifyMarking(heap->code_space());
  VerifyMarking(heap->map_space());
  VerifyMarking(heap->new_space());

  VerifyMarkingVisitor visitor(heap);

  LargeObjectIterator it(heap->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    if (MarkCompactCollector::IsMarked(obj)) {
      obj->Iterate(&visitor);
    }
  }

  heap->IterateStrongRoots(&visitor, VISIT_ONLY_STRONG);
}


class VerifyEvacuationVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) override {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*current);
        CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(object));
      }
    }
  }
};


static void VerifyEvacuation(Page* page) {
  VerifyEvacuationVisitor visitor;
  HeapObjectIterator iterator(page);
  for (HeapObject* heap_object = iterator.Next(); heap_object != NULL;
       heap_object = iterator.Next()) {
    // We skip free space objects.
    if (!heap_object->IsFiller()) {
      heap_object->Iterate(&visitor);
    }
  }
}


static void VerifyEvacuation(NewSpace* space) {
  VerifyEvacuationVisitor visitor;
  NewSpacePageRange range(space->bottom(), space->top());
  for (auto it = range.begin(); it != range.end();) {
    Page* page = *(it++);
    Address current = page->area_start();
    Address limit = it != range.end() ? page->area_end() : space->top();
    CHECK(limit == space->top() || !page->Contains(space->top()));
    while (current < limit) {
      HeapObject* object = HeapObject::FromAddress(current);
      object->Iterate(&visitor);
      current += object->Size();
    }
  }
}


static void VerifyEvacuation(Heap* heap, PagedSpace* space) {
  if (FLAG_use_allocation_folding && (space == heap->old_space())) {
    return;
  }
  for (Page* p : *space) {
    if (p->IsEvacuationCandidate()) continue;
    VerifyEvacuation(p);
  }
}


static void VerifyEvacuation(Heap* heap) {
  VerifyEvacuation(heap, heap->old_space());
  VerifyEvacuation(heap, heap->code_space());
  VerifyEvacuation(heap, heap->map_space());
  VerifyEvacuation(heap->new_space());

  VerifyEvacuationVisitor visitor;
  heap->IterateStrongRoots(&visitor, VISIT_ALL);
}
#endif  // VERIFY_HEAP


void MarkCompactCollector::SetUp() {
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  EnsureMarkingDequeIsReserved();
  EnsureMarkingDequeIsCommitted(kMinMarkingDequeSize);

  if (FLAG_flush_code) {
    code_flusher_ = new CodeFlusher(isolate());
    if (FLAG_trace_code_flushing) {
      PrintF("[code-flushing is now on]\n");
    }
  }
}


void MarkCompactCollector::TearDown() {
  AbortCompaction();
  delete marking_deque_memory_;
  delete code_flusher_;
}


void MarkCompactCollector::AddEvacuationCandidate(Page* p) {
  DCHECK(!p->NeverEvacuate());
  p->MarkEvacuationCandidate();
  evacuation_candidates_.Add(p);
}


static void TraceFragmentation(PagedSpace* space) {
  int number_of_pages = space->CountTotalPages();
  intptr_t reserved = (number_of_pages * space->AreaSize());
  intptr_t free = reserved - space->SizeOfObjects();
  PrintF("[%s]: %d pages, %d (%.1f%%) free\n",
         AllocationSpaceName(space->identity()), number_of_pages,
         static_cast<int>(free), static_cast<double>(free) * 100 / reserved);
}


bool MarkCompactCollector::StartCompaction(CompactionMode mode) {
  if (!compacting_) {
    DCHECK(evacuation_candidates_.length() == 0);

    CollectEvacuationCandidates(heap()->old_space());

    if (FLAG_compact_code_space) {
      CollectEvacuationCandidates(heap()->code_space());
    } else if (FLAG_trace_fragmentation) {
      TraceFragmentation(heap()->code_space());
    }

    if (FLAG_trace_fragmentation) {
      TraceFragmentation(heap()->map_space());
    }

    heap()->old_space()->EvictEvacuationCandidatesFromLinearAllocationArea();
    heap()->code_space()->EvictEvacuationCandidatesFromLinearAllocationArea();

    compacting_ = evacuation_candidates_.length() > 0;
  }

  return compacting_;
}

void MarkCompactCollector::ClearInvalidRememberedSetSlots() {
  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_STORE_BUFFER);
    RememberedSet<OLD_TO_NEW>::ClearInvalidSlots(heap());
  }
// There is not need to filter the old to old set because
// it is completely cleared after the mark-compact GC.
// The slots that become invalid due to runtime transitions are
// cleared eagerly immediately after the transition.

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    RememberedSet<OLD_TO_NEW>::VerifyValidSlots(heap());
    RememberedSet<OLD_TO_OLD>::VerifyValidSlots(heap());
  }
#endif
}

void MarkCompactCollector::CollectGarbage() {
  // Make sure that Prepare() has been called. The individual steps below will
  // update the state as they proceed.
  DCHECK(state_ == PREPARE_GC);

  MarkLiveObjects();

  DCHECK(heap_->incremental_marking()->IsStopped());

  ClearNonLiveReferences();

  RecordObjectStats();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyMarking(heap_);
  }
#endif

  SweepSpaces();

  EvacuateNewSpaceAndCandidates();

  Finish();
}


#ifdef VERIFY_HEAP
void MarkCompactCollector::VerifyMarkbitsAreClean(PagedSpace* space) {
  for (Page* p : *space) {
    CHECK(p->markbits()->IsClean());
    CHECK_EQ(0, p->LiveBytes());
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean(NewSpace* space) {
  for (Page* p : NewSpacePageRange(space->bottom(), space->top())) {
    CHECK(p->markbits()->IsClean());
    CHECK_EQ(0, p->LiveBytes());
  }
}


void MarkCompactCollector::VerifyMarkbitsAreClean() {
  VerifyMarkbitsAreClean(heap_->old_space());
  VerifyMarkbitsAreClean(heap_->code_space());
  VerifyMarkbitsAreClean(heap_->map_space());
  VerifyMarkbitsAreClean(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    MarkBit mark_bit = ObjectMarking::MarkBitFrom(obj);
    CHECK(Marking::IsWhite(mark_bit));
    CHECK_EQ(0, Page::FromAddress(obj->address())->LiveBytes());
  }
}


void MarkCompactCollector::VerifyWeakEmbeddedObjectsInCode() {
  HeapObjectIterator code_iterator(heap()->code_space());
  for (HeapObject* obj = code_iterator.Next(); obj != NULL;
       obj = code_iterator.Next()) {
    Code* code = Code::cast(obj);
    if (!code->is_optimized_code()) continue;
    if (WillBeDeoptimized(code)) continue;
    code->VerifyEmbeddedObjectsDependency();
  }
}


void MarkCompactCollector::VerifyOmittedMapChecks() {
  HeapObjectIterator iterator(heap()->map_space());
  for (HeapObject* obj = iterator.Next(); obj != NULL; obj = iterator.Next()) {
    Map* map = Map::cast(obj);
    map->VerifyOmittedMapChecks();
  }
}
#endif  // VERIFY_HEAP


static void ClearMarkbitsInPagedSpace(PagedSpace* space) {
  for (Page* p : *space) {
    p->ClearLiveness();
  }
}


static void ClearMarkbitsInNewSpace(NewSpace* space) {
  for (Page* page : *space) {
    page->ClearLiveness();
  }
}


void MarkCompactCollector::ClearMarkbits() {
  ClearMarkbitsInPagedSpace(heap_->code_space());
  ClearMarkbitsInPagedSpace(heap_->map_space());
  ClearMarkbitsInPagedSpace(heap_->old_space());
  ClearMarkbitsInNewSpace(heap_->new_space());

  LargeObjectIterator it(heap_->lo_space());
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    Marking::MarkWhite(ObjectMarking::MarkBitFrom(obj));
    MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
    chunk->ResetProgressBar();
    chunk->ResetLiveBytes();
  }
}

class MarkCompactCollector::Sweeper::SweeperTask : public v8::Task {
 public:
  SweeperTask(Sweeper* sweeper, base::Semaphore* pending_sweeper_tasks,
              AllocationSpace space_to_start)
      : sweeper_(sweeper),
        pending_sweeper_tasks_(pending_sweeper_tasks),
        space_to_start_(space_to_start) {}

  virtual ~SweeperTask() {}

 private:
  // v8::Task overrides.
  void Run() override {
    DCHECK_GE(space_to_start_, FIRST_SPACE);
    DCHECK_LE(space_to_start_, LAST_PAGED_SPACE);
    const int offset = space_to_start_ - FIRST_SPACE;
    const int num_spaces = LAST_PAGED_SPACE - FIRST_SPACE + 1;
    for (int i = 0; i < num_spaces; i++) {
      const int space_id = FIRST_SPACE + ((i + offset) % num_spaces);
      DCHECK_GE(space_id, FIRST_SPACE);
      DCHECK_LE(space_id, LAST_PAGED_SPACE);
      sweeper_->ParallelSweepSpace(static_cast<AllocationSpace>(space_id), 0);
    }
    pending_sweeper_tasks_->Signal();
  }

  Sweeper* sweeper_;
  base::Semaphore* pending_sweeper_tasks_;
  AllocationSpace space_to_start_;

  DISALLOW_COPY_AND_ASSIGN(SweeperTask);
};

void MarkCompactCollector::Sweeper::StartSweeping() {
  sweeping_in_progress_ = true;
  ForAllSweepingSpaces([this](AllocationSpace space) {
    std::sort(sweeping_list_[space].begin(), sweeping_list_[space].end(),
              [](Page* a, Page* b) { return a->LiveBytes() < b->LiveBytes(); });
  });
  if (FLAG_concurrent_sweeping) {
    ForAllSweepingSpaces([this](AllocationSpace space) {
      if (space == NEW_SPACE) return;
      StartSweepingHelper(space);
    });
  }
}

void MarkCompactCollector::Sweeper::StartSweepingHelper(
    AllocationSpace space_to_start) {
  num_sweeping_tasks_.Increment(1);
  V8::GetCurrentPlatform()->CallOnBackgroundThread(
      new SweeperTask(this, &pending_sweeper_tasks_semaphore_, space_to_start),
      v8::Platform::kShortRunningTask);
}

void MarkCompactCollector::Sweeper::SweepOrWaitUntilSweepingCompleted(
    Page* page) {
  if (!page->SweepingDone()) {
    ParallelSweepPage(page, page->owner()->identity());
    if (!page->SweepingDone()) {
      // We were not able to sweep that page, i.e., a concurrent
      // sweeper thread currently owns this page. Wait for the sweeper
      // thread to be done with this page.
      page->WaitUntilSweepingCompleted();
    }
  }
}

void MarkCompactCollector::SweepAndRefill(CompactionSpace* space) {
  if (FLAG_concurrent_sweeping && !sweeper().IsSweepingCompleted()) {
    sweeper().ParallelSweepSpace(space->identity(), 0);
    space->RefillFreeList();
  }
}

Page* MarkCompactCollector::Sweeper::GetSweptPageSafe(PagedSpace* space) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  SweptList& list = swept_list_[space->identity()];
  if (list.length() > 0) {
    return list.RemoveLast();
  }
  return nullptr;
}

void MarkCompactCollector::Sweeper::EnsureCompleted() {
  if (!sweeping_in_progress_) return;

  // If sweeping is not completed or not running at all, we try to complete it
  // here.
  if (!FLAG_concurrent_sweeping || !IsSweepingCompleted()) {
    ForAllSweepingSpaces(
        [this](AllocationSpace space) { ParallelSweepSpace(space, 0); });
  }

  if (FLAG_concurrent_sweeping) {
    while (num_sweeping_tasks_.Value() > 0) {
      pending_sweeper_tasks_semaphore_.Wait();
      num_sweeping_tasks_.Increment(-1);
    }
  }

  ForAllSweepingSpaces([this](AllocationSpace space) {
    if (space == NEW_SPACE) {
      swept_list_[NEW_SPACE].Clear();
    }
    DCHECK(sweeping_list_[space].empty());
  });
  late_pages_ = false;
  sweeping_in_progress_ = false;
}

void MarkCompactCollector::Sweeper::EnsureNewSpaceCompleted() {
  if (!sweeping_in_progress_) return;
  if (!FLAG_concurrent_sweeping || !IsSweepingCompleted()) {
    for (Page* p : *heap_->new_space()) {
      SweepOrWaitUntilSweepingCompleted(p);
    }
  }
}

void MarkCompactCollector::EnsureSweepingCompleted() {
  if (!sweeper().sweeping_in_progress()) return;

  sweeper().EnsureCompleted();
  heap()->old_space()->RefillFreeList();
  heap()->code_space()->RefillFreeList();
  heap()->map_space()->RefillFreeList();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !evacuation()) {
    VerifyEvacuation(heap_);
  }
#endif
}

bool MarkCompactCollector::Sweeper::IsSweepingCompleted() {
  while (pending_sweeper_tasks_semaphore_.WaitFor(
      base::TimeDelta::FromSeconds(0))) {
    num_sweeping_tasks_.Increment(-1);
  }
  return num_sweeping_tasks_.Value() == 0;
}

const char* AllocationSpaceName(AllocationSpace space) {
  switch (space) {
    case NEW_SPACE:
      return "NEW_SPACE";
    case OLD_SPACE:
      return "OLD_SPACE";
    case CODE_SPACE:
      return "CODE_SPACE";
    case MAP_SPACE:
      return "MAP_SPACE";
    case LO_SPACE:
      return "LO_SPACE";
    default:
      UNREACHABLE();
  }

  return NULL;
}


void MarkCompactCollector::ComputeEvacuationHeuristics(
    int area_size, int* target_fragmentation_percent,
    int* max_evacuated_bytes) {
  // For memory reducing and optimize for memory mode we directly define both
  // constants.
  const int kTargetFragmentationPercentForReduceMemory = 20;
  const int kMaxEvacuatedBytesForReduceMemory = 12 * Page::kPageSize;
  const int kTargetFragmentationPercentForOptimizeMemory = 20;
  const int kMaxEvacuatedBytesForOptimizeMemory = 6 * MB;

  // For regular mode (which is latency critical) we define less aggressive
  // defaults to start and switch to a trace-based (using compaction speed)
  // approach as soon as we have enough samples.
  const int kTargetFragmentationPercent = 70;
  const int kMaxEvacuatedBytes = 4 * Page::kPageSize;
  // Time to take for a single area (=payload of page). Used as soon as there
  // exist enough compaction speed samples.
  const int kTargetMsPerArea = 1;

  if (heap()->ShouldReduceMemory()) {
    *target_fragmentation_percent = kTargetFragmentationPercentForReduceMemory;
    *max_evacuated_bytes = kMaxEvacuatedBytesForReduceMemory;
  } else if (heap()->ShouldOptimizeForMemoryUsage()) {
    *target_fragmentation_percent =
        kTargetFragmentationPercentForOptimizeMemory;
    *max_evacuated_bytes = kMaxEvacuatedBytesForOptimizeMemory;
  } else {
    const double estimated_compaction_speed =
        heap()->tracer()->CompactionSpeedInBytesPerMillisecond();
    if (estimated_compaction_speed != 0) {
      // Estimate the target fragmentation based on traced compaction speed
      // and a goal for a single page.
      const double estimated_ms_per_area =
          1 + area_size / estimated_compaction_speed;
      *target_fragmentation_percent = static_cast<int>(
          100 - 100 * kTargetMsPerArea / estimated_ms_per_area);
      if (*target_fragmentation_percent <
          kTargetFragmentationPercentForReduceMemory) {
        *target_fragmentation_percent =
            kTargetFragmentationPercentForReduceMemory;
      }
    } else {
      *target_fragmentation_percent = kTargetFragmentationPercent;
    }
    *max_evacuated_bytes = kMaxEvacuatedBytes;
  }
}


void MarkCompactCollector::CollectEvacuationCandidates(PagedSpace* space) {
  DCHECK(space->identity() == OLD_SPACE || space->identity() == CODE_SPACE);

  int number_of_pages = space->CountTotalPages();
  int area_size = space->AreaSize();

  // Pairs of (live_bytes_in_page, page).
  typedef std::pair<int, Page*> LiveBytesPagePair;
  std::vector<LiveBytesPagePair> pages;
  pages.reserve(number_of_pages);

  for (Page* p : *space) {
    if (p->NeverEvacuate()) continue;
    // Invariant: Evacuation candidates are just created when marking is
    // started. This means that sweeping has finished. Furthermore, at the end
    // of a GC all evacuation candidates are cleared and their slot buffers are
    // released.
    CHECK(!p->IsEvacuationCandidate());
    CHECK_NULL(p->old_to_old_slots());
    CHECK_NULL(p->typed_old_to_old_slots());
    CHECK(p->SweepingDone());
    DCHECK(p->area_size() == area_size);
    pages.push_back(std::make_pair(p->LiveBytesFromFreeList(), p));
  }

  int candidate_count = 0;
  int total_live_bytes = 0;

  const bool reduce_memory = heap()->ShouldReduceMemory();
  if (FLAG_manual_evacuation_candidates_selection) {
    for (size_t i = 0; i < pages.size(); i++) {
      Page* p = pages[i].second;
      if (p->IsFlagSet(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING)) {
        candidate_count++;
        total_live_bytes += pages[i].first;
        p->ClearFlag(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING);
        AddEvacuationCandidate(p);
      }
    }
  } else if (FLAG_stress_compaction) {
    for (size_t i = 0; i < pages.size(); i++) {
      Page* p = pages[i].second;
      if (i % 2 == 0) {
        candidate_count++;
        total_live_bytes += pages[i].first;
        AddEvacuationCandidate(p);
      }
    }
  } else {
    // The following approach determines the pages that should be evacuated.
    //
    // We use two conditions to decide whether a page qualifies as an evacuation
    // candidate, or not:
    // * Target fragmentation: How fragmented is a page, i.e., how is the ratio
    //   between live bytes and capacity of this page (= area).
    // * Evacuation quota: A global quota determining how much bytes should be
    //   compacted.
    //
    // The algorithm sorts all pages by live bytes and then iterates through
    // them starting with the page with the most free memory, adding them to the
    // set of evacuation candidates as long as both conditions (fragmentation
    // and quota) hold.
    int max_evacuated_bytes;
    int target_fragmentation_percent;
    ComputeEvacuationHeuristics(area_size, &target_fragmentation_percent,
                                &max_evacuated_bytes);

    const intptr_t free_bytes_threshold =
        target_fragmentation_percent * (area_size / 100);

    // Sort pages from the most free to the least free, then select
    // the first n pages for evacuation such that:
    // - the total size of evacuated objects does not exceed the specified
    // limit.
    // - fragmentation of (n+1)-th page does not exceed the specified limit.
    std::sort(pages.begin(), pages.end(),
              [](const LiveBytesPagePair& a, const LiveBytesPagePair& b) {
                return a.first < b.first;
              });
    for (size_t i = 0; i < pages.size(); i++) {
      int live_bytes = pages[i].first;
      int free_bytes = area_size - live_bytes;
      if (FLAG_always_compact ||
          ((free_bytes >= free_bytes_threshold) &&
           ((total_live_bytes + live_bytes) <= max_evacuated_bytes))) {
        candidate_count++;
        total_live_bytes += live_bytes;
      }
      if (FLAG_trace_fragmentation_verbose) {
        PrintIsolate(isolate(),
                     "compaction-selection-page: space=%s free_bytes_page=%d "
                     "fragmentation_limit_kb=%" V8PRIdPTR
                     " fragmentation_limit_percent=%d sum_compaction_kb=%d "
                     "compaction_limit_kb=%d\n",
                     AllocationSpaceName(space->identity()), free_bytes / KB,
                     free_bytes_threshold / KB, target_fragmentation_percent,
                     total_live_bytes / KB, max_evacuated_bytes / KB);
      }
    }
    // How many pages we will allocated for the evacuated objects
    // in the worst case: ceil(total_live_bytes / area_size)
    int estimated_new_pages = (total_live_bytes + area_size - 1) / area_size;
    DCHECK_LE(estimated_new_pages, candidate_count);
    int estimated_released_pages = candidate_count - estimated_new_pages;
    // Avoid (compact -> expand) cycles.
    if ((estimated_released_pages == 0) && !FLAG_always_compact) {
      candidate_count = 0;
    }
    for (int i = 0; i < candidate_count; i++) {
      AddEvacuationCandidate(pages[i].second);
    }
  }

  if (FLAG_trace_fragmentation) {
    PrintIsolate(isolate(),
                 "compaction-selection: space=%s reduce_memory=%d pages=%d "
                 "total_live_bytes=%d\n",
                 AllocationSpaceName(space->identity()), reduce_memory,
                 candidate_count, total_live_bytes / KB);
  }
}


void MarkCompactCollector::AbortCompaction() {
  if (compacting_) {
    RememberedSet<OLD_TO_OLD>::ClearAll(heap());
    for (Page* p : evacuation_candidates_) {
      p->ClearEvacuationCandidate();
    }
    compacting_ = false;
    evacuation_candidates_.Rewind(0);
  }
  DCHECK_EQ(0, evacuation_candidates_.length());
}


void MarkCompactCollector::Prepare() {
  was_marked_incrementally_ = heap()->incremental_marking()->IsMarking();

#ifdef DEBUG
  DCHECK(state_ == IDLE);
  state_ = PREPARE_GC;
#endif

  DCHECK(!FLAG_never_compact || !FLAG_always_compact);

  if (sweeping_in_progress()) {
    // Instead of waiting we could also abort the sweeper threads here.
    EnsureSweepingCompleted();
  }

  // If concurrent unmapping tasks are still running, we should wait for
  // them here.
  heap()->memory_allocator()->unmapper()->WaitUntilCompleted();

  // Clear marking bits if incremental marking is aborted.
  if (was_marked_incrementally_ && heap_->ShouldAbortIncrementalMarking()) {
    heap()->incremental_marking()->Stop();
    ClearMarkbits();
    AbortWeakCollections();
    AbortWeakCells();
    AbortTransitionArrays();
    AbortCompaction();
    if (heap_->UsingEmbedderHeapTracer()) {
      heap_->mark_compact_collector()->embedder_heap_tracer()->AbortTracing();
    }
    was_marked_incrementally_ = false;
  }

  if (!was_marked_incrementally_) {
    if (heap_->UsingEmbedderHeapTracer()) {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WRAPPER_PROLOGUE);
      heap_->mark_compact_collector()->embedder_heap_tracer()->TracePrologue();
    }
  }

  if (UsingEmbedderHeapTracer()) {
    embedder_heap_tracer()->EnterFinalPause();
  }

  // Don't start compaction if we are in the middle of incremental
  // marking cycle. We did not collect any slots.
  if (!FLAG_never_compact && !was_marked_incrementally_) {
    StartCompaction(NON_INCREMENTAL_COMPACTION);
  }

  PagedSpaces spaces(heap());
  for (PagedSpace* space = spaces.next(); space != NULL;
       space = spaces.next()) {
    space->PrepareForMarkCompact();
  }
  heap()->account_external_memory_concurrently_freed();

#ifdef VERIFY_HEAP
  if (!was_marked_incrementally_ && FLAG_verify_heap) {
    VerifyMarkbitsAreClean();
  }
#endif
}


void MarkCompactCollector::Finish() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_FINISH);

  if (sweeper().contains_late_pages() && FLAG_concurrent_sweeping) {
    // If we added some more pages during MC, we need to start at least one
    // more task as all other tasks might already be finished.
    sweeper().StartSweepingHelper(OLD_SPACE);
  }

  // The hashing of weak_object_to_code_table is no longer valid.
  heap()->weak_object_to_code_table()->Rehash(
      heap()->isolate()->factory()->undefined_value());

  // Clear the marking state of live large objects.
  heap_->lo_space()->ClearMarkingStateOfLiveObjects();

#ifdef DEBUG
  DCHECK(state_ == SWEEP_SPACES || state_ == RELOCATE_OBJECTS);
  state_ = IDLE;
#endif
  heap_->isolate()->inner_pointer_to_code_cache()->Flush();

  // The stub caches are not traversed during GC; clear them to force
  // their lazy re-initialization. This must be done after the
  // GC, because it relies on the new address of certain old space
  // objects (empty string, illegal builtin).
  isolate()->load_stub_cache()->Clear();
  isolate()->store_stub_cache()->Clear();

  if (have_code_to_deoptimize_) {
    // Some code objects were marked for deoptimization during the GC.
    Deoptimizer::DeoptimizeMarkedCode(isolate());
    have_code_to_deoptimize_ = false;
  }

  heap_->incremental_marking()->ClearIdleMarkingDelayCounter();

  if (marking_parity_ == EVEN_MARKING_PARITY) {
    marking_parity_ = ODD_MARKING_PARITY;
  } else {
    DCHECK(marking_parity_ == ODD_MARKING_PARITY);
    marking_parity_ = EVEN_MARKING_PARITY;
  }
}


// -------------------------------------------------------------------------
// Phase 1: tracing and marking live objects.
//   before: all objects are in normal state.
//   after: a live object's map pointer is marked as '00'.

// Marking all live objects in the heap as part of mark-sweep or mark-compact
// collection.  Before marking, all objects are in their normal state.  After
// marking, live objects' map pointers are marked indicating that the object
// has been found reachable.
//
// The marking algorithm is a (mostly) depth-first (because of possible stack
// overflow) traversal of the graph of objects reachable from the roots.  It
// uses an explicit stack of pointers rather than recursion.  The young
// generation's inactive ('from') space is used as a marking stack.  The
// objects in the marking stack are the ones that have been reached and marked
// but their children have not yet been visited.
//
// The marking stack can overflow during traversal.  In that case, we set an
// overflow flag.  When the overflow flag is set, we continue marking objects
// reachable from the objects on the marking stack, but no longer push them on
// the marking stack.  Instead, we mark them as both marked and overflowed.
// When the stack is in the overflowed state, objects marked as overflowed
// have been reached and marked but their children have not been visited yet.
// After emptying the marking stack, we clear the overflow flag and traverse
// the heap looking for objects marked as overflowed, push them on the stack,
// and continue with marking.  This process repeats until all reachable
// objects have been marked.

void CodeFlusher::ProcessJSFunctionCandidates() {
  Code* lazy_compile = isolate_->builtins()->builtin(Builtins::kCompileLazy);
  Object* undefined = isolate_->heap()->undefined_value();

  JSFunction* candidate = jsfunction_candidates_head_;
  JSFunction* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate, undefined);

    SharedFunctionInfo* shared = candidate->shared();

    Code* code = shared->code();
    MarkBit code_mark = ObjectMarking::MarkBitFrom(code);
    if (Marking::IsWhite(code_mark)) {
      if (FLAG_trace_code_flushing && shared->is_compiled()) {
        PrintF("[code-flushing clears: ");
        shared->ShortPrint();
        PrintF(" - age: %d]\n", code->GetAge());
      }
      // Always flush the optimized code map if there is one.
      if (!shared->OptimizedCodeMapIsCleared()) {
        shared->ClearOptimizedCodeMap();
      }
      shared->set_code(lazy_compile);
      candidate->set_code(lazy_compile);
    } else {
      DCHECK(Marking::IsBlack(code_mark));
      candidate->set_code(code);
    }

    // We are in the middle of a GC cycle so the write barrier in the code
    // setter did not record the slot update and we have to do that manually.
    Address slot = candidate->address() + JSFunction::kCodeEntryOffset;
    Code* target = Code::cast(Code::GetObjectFromEntryAddress(slot));
    isolate_->heap()->mark_compact_collector()->RecordCodeEntrySlot(
        candidate, slot, target);

    Object** shared_code_slot =
        HeapObject::RawField(shared, SharedFunctionInfo::kCodeOffset);
    isolate_->heap()->mark_compact_collector()->RecordSlot(
        shared, shared_code_slot, *shared_code_slot);

    candidate = next_candidate;
  }

  jsfunction_candidates_head_ = NULL;
}


void CodeFlusher::ProcessSharedFunctionInfoCandidates() {
  Code* lazy_compile = isolate_->builtins()->builtin(Builtins::kCompileLazy);

  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  while (candidate != NULL) {
    next_candidate = GetNextCandidate(candidate);
    ClearNextCandidate(candidate);

    Code* code = candidate->code();
    MarkBit code_mark = ObjectMarking::MarkBitFrom(code);
    if (Marking::IsWhite(code_mark)) {
      if (FLAG_trace_code_flushing && candidate->is_compiled()) {
        PrintF("[code-flushing clears: ");
        candidate->ShortPrint();
        PrintF(" - age: %d]\n", code->GetAge());
      }
      // Always flush the optimized code map if there is one.
      if (!candidate->OptimizedCodeMapIsCleared()) {
        candidate->ClearOptimizedCodeMap();
      }
      candidate->set_code(lazy_compile);
    }

    Object** code_slot =
        HeapObject::RawField(candidate, SharedFunctionInfo::kCodeOffset);
    isolate_->heap()->mark_compact_collector()->RecordSlot(candidate, code_slot,
                                                           *code_slot);

    candidate = next_candidate;
  }

  shared_function_info_candidates_head_ = NULL;
}


void CodeFlusher::EvictCandidate(SharedFunctionInfo* shared_info) {
  // Make sure previous flushing decisions are revisited.
  isolate_->heap()->incremental_marking()->IterateBlackObject(shared_info);

  if (FLAG_trace_code_flushing) {
    PrintF("[code-flushing abandons function-info: ");
    shared_info->ShortPrint();
    PrintF("]\n");
  }

  SharedFunctionInfo* candidate = shared_function_info_candidates_head_;
  SharedFunctionInfo* next_candidate;
  if (candidate == shared_info) {
    next_candidate = GetNextCandidate(shared_info);
    shared_function_info_candidates_head_ = next_candidate;
    ClearNextCandidate(shared_info);
  } else {
    while (candidate != NULL) {
      next_candidate = GetNextCandidate(candidate);

      if (next_candidate == shared_info) {
        next_candidate = GetNextCandidate(shared_info);
        SetNextCandidate(candidate, next_candidate);
        ClearNextCandidate(shared_info);
        break;
      }

      candidate = next_candidate;
    }
  }
}


void CodeFlusher::EvictCandidate(JSFunction* function) {
  DCHECK(!function->next_function_link()->IsUndefined(isolate_));
  Object* undefined = isolate_->heap()->undefined_value();

  // Make sure previous flushing decisions are revisited.
  isolate_->heap()->incremental_marking()->IterateBlackObject(function);
  isolate_->heap()->incremental_marking()->IterateBlackObject(
      function->shared());

  if (FLAG_trace_code_flushing) {
    PrintF("[code-flushing abandons closure: ");
    function->shared()->ShortPrint();
    PrintF("]\n");
  }

  JSFunction* candidate = jsfunction_candidates_head_;
  JSFunction* next_candidate;
  if (candidate == function) {
    next_candidate = GetNextCandidate(function);
    jsfunction_candidates_head_ = next_candidate;
    ClearNextCandidate(function, undefined);
  } else {
    while (candidate != NULL) {
      next_candidate = GetNextCandidate(candidate);

      if (next_candidate == function) {
        next_candidate = GetNextCandidate(function);
        SetNextCandidate(candidate, next_candidate);
        ClearNextCandidate(function, undefined);
        break;
      }

      candidate = next_candidate;
    }
  }
}


void CodeFlusher::IteratePointersToFromSpace(ObjectVisitor* v) {
  Heap* heap = isolate_->heap();

  JSFunction** slot = &jsfunction_candidates_head_;
  JSFunction* candidate = jsfunction_candidates_head_;
  while (candidate != NULL) {
    if (heap->InFromSpace(candidate)) {
      v->VisitPointer(reinterpret_cast<Object**>(slot));
    }
    candidate = GetNextCandidate(*slot);
    slot = GetNextCandidateSlot(*slot);
  }
}


class MarkCompactMarkingVisitor
    : public StaticMarkingVisitor<MarkCompactMarkingVisitor> {
 public:
  static void Initialize();

  INLINE(static void VisitPointer(Heap* heap, HeapObject* object, Object** p)) {
    MarkObjectByPointer(heap->mark_compact_collector(), object, p);
  }

  INLINE(static void VisitPointers(Heap* heap, HeapObject* object,
                                   Object** start, Object** end)) {
    // Mark all objects pointed to in [start, end).
    const int kMinRangeForMarkingRecursion = 64;
    if (end - start >= kMinRangeForMarkingRecursion) {
      if (VisitUnmarkedObjects(heap, object, start, end)) return;
      // We are close to a stack overflow, so just mark the objects.
    }
    MarkCompactCollector* collector = heap->mark_compact_collector();
    for (Object** p = start; p < end; p++) {
      MarkObjectByPointer(collector, object, p);
    }
  }

  // Marks the object black and pushes it on the marking stack.
  INLINE(static void MarkObject(Heap* heap, HeapObject* object)) {
    MarkBit mark = ObjectMarking::MarkBitFrom(object);
    heap->mark_compact_collector()->MarkObject(object, mark);
  }

  // Marks the object black without pushing it on the marking stack.
  // Returns true if object needed marking and false otherwise.
  INLINE(static bool MarkObjectWithoutPush(Heap* heap, HeapObject* object)) {
    MarkBit mark_bit = ObjectMarking::MarkBitFrom(object);
    if (Marking::IsWhite(mark_bit)) {
      heap->mark_compact_collector()->SetMark(object, mark_bit);
      return true;
    }
    return false;
  }

  // Mark object pointed to by p.
  INLINE(static void MarkObjectByPointer(MarkCompactCollector* collector,
                                         HeapObject* object, Object** p)) {
    if (!(*p)->IsHeapObject()) return;
    HeapObject* target_object = HeapObject::cast(*p);
    collector->RecordSlot(object, p, target_object);
    MarkBit mark = ObjectMarking::MarkBitFrom(target_object);
    collector->MarkObject(target_object, mark);
  }


  // Visit an unmarked object.
  INLINE(static void VisitUnmarkedObject(MarkCompactCollector* collector,
                                         HeapObject* obj)) {
#ifdef DEBUG
    DCHECK(collector->heap()->Contains(obj));
    DCHECK(!collector->heap()->mark_compact_collector()->IsMarked(obj));
#endif
    Map* map = obj->map();
    Heap* heap = obj->GetHeap();
    MarkBit mark = ObjectMarking::MarkBitFrom(obj);
    heap->mark_compact_collector()->SetMark(obj, mark);
    // Mark the map pointer and the body.
    MarkBit map_mark = ObjectMarking::MarkBitFrom(map);
    heap->mark_compact_collector()->MarkObject(map, map_mark);
    IterateBody(map, obj);
  }

  // Visit all unmarked objects pointed to by [start, end).
  // Returns false if the operation fails (lack of stack space).
  INLINE(static bool VisitUnmarkedObjects(Heap* heap, HeapObject* object,
                                          Object** start, Object** end)) {
    // Return false is we are close to the stack limit.
    StackLimitCheck check(heap->isolate());
    if (check.HasOverflowed()) return false;

    MarkCompactCollector* collector = heap->mark_compact_collector();
    // Visit the unmarked objects.
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (!o->IsHeapObject()) continue;
      collector->RecordSlot(object, p, o);
      HeapObject* obj = HeapObject::cast(o);
      MarkBit mark = ObjectMarking::MarkBitFrom(obj);
      if (Marking::IsBlackOrGrey(mark)) continue;
      VisitUnmarkedObject(collector, obj);
    }
    return true;
  }

 private:
  // Code flushing support.

  static const int kRegExpCodeThreshold = 5;

  static void UpdateRegExpCodeAgeAndFlush(Heap* heap, JSRegExp* re,
                                          bool is_one_byte) {
    // Make sure that the fixed array is in fact initialized on the RegExp.
    // We could potentially trigger a GC when initializing the RegExp.
    if (HeapObject::cast(re->data())->map()->instance_type() !=
        FIXED_ARRAY_TYPE)
      return;

    // Make sure this is a RegExp that actually contains code.
    if (re->TypeTag() != JSRegExp::IRREGEXP) return;

    Object* code = re->DataAt(JSRegExp::code_index(is_one_byte));
    if (!code->IsSmi() &&
        HeapObject::cast(code)->map()->instance_type() == CODE_TYPE) {
      // Save a copy that can be reinstated if we need the code again.
      re->SetDataAt(JSRegExp::saved_code_index(is_one_byte), code);

      // Saving a copy might create a pointer into compaction candidate
      // that was not observed by marker.  This might happen if JSRegExp data
      // was marked through the compilation cache before marker reached JSRegExp
      // object.
      FixedArray* data = FixedArray::cast(re->data());
      if (Marking::IsBlackOrGrey(ObjectMarking::MarkBitFrom(data))) {
        Object** slot =
            data->data_start() + JSRegExp::saved_code_index(is_one_byte);
        heap->mark_compact_collector()->RecordSlot(data, slot, code);
      }

      // Set a number in the 0-255 range to guarantee no smi overflow.
      re->SetDataAt(JSRegExp::code_index(is_one_byte),
                    Smi::FromInt(heap->ms_count() & 0xff));
    } else if (code->IsSmi()) {
      int value = Smi::cast(code)->value();
      // The regexp has not been compiled yet or there was a compilation error.
      if (value == JSRegExp::kUninitializedValue ||
          value == JSRegExp::kCompilationErrorValue) {
        return;
      }

      // Check if we should flush now.
      if (value == ((heap->ms_count() - kRegExpCodeThreshold) & 0xff)) {
        re->SetDataAt(JSRegExp::code_index(is_one_byte),
                      Smi::FromInt(JSRegExp::kUninitializedValue));
        re->SetDataAt(JSRegExp::saved_code_index(is_one_byte),
                      Smi::FromInt(JSRegExp::kUninitializedValue));
      }
    }
  }


  // Works by setting the current sweep_generation (as a smi) in the
  // code object place in the data array of the RegExp and keeps a copy
  // around that can be reinstated if we reuse the RegExp before flushing.
  // If we did not use the code for kRegExpCodeThreshold mark sweep GCs
  // we flush the code.
  static void VisitRegExpAndFlushCode(Map* map, HeapObject* object) {
    Heap* heap = map->GetHeap();
    MarkCompactCollector* collector = heap->mark_compact_collector();
    if (!collector->is_code_flushing_enabled()) {
      VisitJSRegExp(map, object);
      return;
    }
    JSRegExp* re = reinterpret_cast<JSRegExp*>(object);
    // Flush code or set age on both one byte and two byte code.
    UpdateRegExpCodeAgeAndFlush(heap, re, true);
    UpdateRegExpCodeAgeAndFlush(heap, re, false);
    // Visit the fields of the RegExp, including the updated FixedArray.
    VisitJSRegExp(map, object);
  }
};


void MarkCompactMarkingVisitor::Initialize() {
  StaticMarkingVisitor<MarkCompactMarkingVisitor>::Initialize();

  table_.Register(kVisitJSRegExp, &VisitRegExpAndFlushCode);
}


class CodeMarkingVisitor : public ThreadVisitor {
 public:
  explicit CodeMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    collector_->PrepareThreadForCodeFlushing(isolate, top);
  }

 private:
  MarkCompactCollector* collector_;
};


class SharedFunctionInfoMarkingVisitor : public ObjectVisitor {
 public:
  explicit SharedFunctionInfoMarkingVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  void VisitPointers(Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) VisitPointer(p);
  }

  void VisitPointer(Object** slot) override {
    Object* obj = *slot;
    if (obj->IsSharedFunctionInfo()) {
      SharedFunctionInfo* shared = reinterpret_cast<SharedFunctionInfo*>(obj);
      MarkBit shared_mark = ObjectMarking::MarkBitFrom(shared);
      MarkBit code_mark = ObjectMarking::MarkBitFrom(shared->code());
      collector_->MarkObject(shared->code(), code_mark);
      collector_->MarkObject(shared, shared_mark);
    }
  }

 private:
  MarkCompactCollector* collector_;
};


void MarkCompactCollector::PrepareThreadForCodeFlushing(Isolate* isolate,
                                                        ThreadLocalTop* top) {
  for (StackFrameIterator it(isolate, top); !it.done(); it.Advance()) {
    // Note: for the frame that has a pending lazy deoptimization
    // StackFrame::unchecked_code will return a non-optimized code object for
    // the outermost function and StackFrame::LookupCode will return
    // actual optimized code object.
    StackFrame* frame = it.frame();
    Code* code = frame->unchecked_code();
    MarkBit code_mark = ObjectMarking::MarkBitFrom(code);
    MarkObject(code, code_mark);
    if (frame->is_optimized()) {
      Code* optimized_code = frame->LookupCode();
      MarkBit optimized_code_mark = ObjectMarking::MarkBitFrom(optimized_code);
      MarkObject(optimized_code, optimized_code_mark);
    }
  }
}


void MarkCompactCollector::PrepareForCodeFlushing() {
  // If code flushing is disabled, there is no need to prepare for it.
  if (!is_code_flushing_enabled()) return;

  // Make sure we are not referencing the code from the stack.
  DCHECK(this == heap()->mark_compact_collector());
  PrepareThreadForCodeFlushing(heap()->isolate(),
                               heap()->isolate()->thread_local_top());

  // Iterate the archived stacks in all threads to check if
  // the code is referenced.
  CodeMarkingVisitor code_marking_visitor(this);
  heap()->isolate()->thread_manager()->IterateArchivedThreads(
      &code_marking_visitor);

  SharedFunctionInfoMarkingVisitor visitor(this);
  heap()->isolate()->compilation_cache()->IterateFunctions(&visitor);
  heap()->isolate()->handle_scope_implementer()->Iterate(&visitor);

  ProcessMarkingDeque();
}


// Visitor class for marking heap roots.
class RootMarkingVisitor : public ObjectVisitor {
 public:
  explicit RootMarkingVisitor(Heap* heap)
      : collector_(heap->mark_compact_collector()) {}

  void VisitPointer(Object** p) override { MarkObjectByPointer(p); }

  void VisitPointers(Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) MarkObjectByPointer(p);
  }

  // Skip the weak next code link in a code object, which is visited in
  // ProcessTopOptimizedFrame.
  void VisitNextCodeLink(Object** p) override {}

 private:
  void MarkObjectByPointer(Object** p) {
    if (!(*p)->IsHeapObject()) return;

    HeapObject* object = HeapObject::cast(*p);

    MarkBit mark_bit = ObjectMarking::MarkBitFrom(object);
    if (Marking::IsBlackOrGrey(mark_bit)) return;

    Map* map = object->map();
    // Mark the object.
    collector_->SetMark(object, mark_bit);

    // Mark the map pointer and body, and push them on the marking stack.
    MarkBit map_mark = ObjectMarking::MarkBitFrom(map);
    collector_->MarkObject(map, map_mark);
    MarkCompactMarkingVisitor::IterateBody(map, object);

    // Mark all the objects reachable from the map and body.  May leave
    // overflowed objects in the heap.
    collector_->EmptyMarkingDeque();
  }

  MarkCompactCollector* collector_;
};


// Helper class for pruning the string table.
template <bool finalize_external_strings, bool record_slots>
class StringTableCleaner : public ObjectVisitor {
 public:
  StringTableCleaner(Heap* heap, HeapObject* table)
      : heap_(heap), pointers_removed_(0), table_(table) {
    DCHECK(!record_slots || table != nullptr);
  }

  void VisitPointers(Object** start, Object** end) override {
    // Visit all HeapObject pointers in [start, end).
    MarkCompactCollector* collector = heap_->mark_compact_collector();
    for (Object** p = start; p < end; p++) {
      Object* o = *p;
      if (o->IsHeapObject()) {
        if (Marking::IsWhite(ObjectMarking::MarkBitFrom(HeapObject::cast(o)))) {
          if (finalize_external_strings) {
            DCHECK(o->IsExternalString());
            heap_->FinalizeExternalString(String::cast(*p));
          } else {
            pointers_removed_++;
          }
          // Set the entry to the_hole_value (as deleted).
          *p = heap_->the_hole_value();
        } else if (record_slots) {
          // StringTable contains only old space strings.
          DCHECK(!heap_->InNewSpace(o));
          collector->RecordSlot(table_, p, o);
        }
      }
    }
  }

  int PointersRemoved() {
    DCHECK(!finalize_external_strings);
    return pointers_removed_;
  }

 private:
  Heap* heap_;
  int pointers_removed_;
  HeapObject* table_;
};

typedef StringTableCleaner<false, true> InternalizedStringTableCleaner;
typedef StringTableCleaner<true, false> ExternalStringTableCleaner;

// Implementation of WeakObjectRetainer for mark compact GCs. All marked objects
// are retained.
class MarkCompactWeakObjectRetainer : public WeakObjectRetainer {
 public:
  virtual Object* RetainAs(Object* object) {
    MarkBit mark_bit = ObjectMarking::MarkBitFrom(HeapObject::cast(object));
    DCHECK(!Marking::IsGrey(mark_bit));
    if (Marking::IsBlack(mark_bit)) {
      return object;
    } else if (object->IsAllocationSite() &&
               !(AllocationSite::cast(object)->IsZombie())) {
      // "dead" AllocationSites need to live long enough for a traversal of new
      // space. These sites get a one-time reprieve.
      AllocationSite* site = AllocationSite::cast(object);
      site->MarkZombie();
      site->GetHeap()->mark_compact_collector()->MarkAllocationSite(site);
      return object;
    } else {
      return NULL;
    }
  }
};


// Fill the marking stack with overflowed objects returned by the given
// iterator.  Stop when the marking stack is filled or the end of the space
// is reached, whichever comes first.
template <class T>
void MarkCompactCollector::DiscoverGreyObjectsWithIterator(T* it) {
  // The caller should ensure that the marking stack is initially not full,
  // so that we don't waste effort pointlessly scanning for objects.
  DCHECK(!marking_deque()->IsFull());

  Map* filler_map = heap()->one_pointer_filler_map();
  for (HeapObject* object = it->Next(); object != NULL; object = it->Next()) {
    MarkBit markbit = ObjectMarking::MarkBitFrom(object);
    if ((object->map() != filler_map) && Marking::IsGrey(markbit)) {
      Marking::GreyToBlack(markbit);
      PushBlack(object);
      if (marking_deque()->IsFull()) return;
    }
  }
}

void MarkCompactCollector::DiscoverGreyObjectsOnPage(MemoryChunk* p) {
  DCHECK(!marking_deque()->IsFull());
  LiveObjectIterator<kGreyObjects> it(p);
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    MarkBit markbit = ObjectMarking::MarkBitFrom(object);
    DCHECK(Marking::IsGrey(markbit));
    Marking::GreyToBlack(markbit);
    PushBlack(object);
    if (marking_deque()->IsFull()) return;
  }
}

class RecordMigratedSlotVisitor final : public ObjectVisitor {
 public:
  explicit RecordMigratedSlotVisitor(MarkCompactCollector* collector)
      : collector_(collector) {}

  inline void VisitPointer(Object** p) final {
    RecordMigratedSlot(*p, reinterpret_cast<Address>(p));
  }

  inline void VisitPointers(Object** start, Object** end) final {
    while (start < end) {
      RecordMigratedSlot(*start, reinterpret_cast<Address>(start));
      ++start;
    }
  }

  inline void VisitCodeEntry(Address code_entry_slot) final {
    Address code_entry = Memory::Address_at(code_entry_slot);
    if (Page::FromAddress(code_entry)->IsEvacuationCandidate()) {
      RememberedSet<OLD_TO_OLD>::InsertTyped(Page::FromAddress(code_entry_slot),
                                             nullptr, CODE_ENTRY_SLOT,
                                             code_entry_slot);
    }
  }

  inline void VisitCodeTarget(RelocInfo* rinfo) final {
    DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    Code* host = rinfo->host();
    // The target is always in old space, we don't have to record the slot in
    // the old-to-new remembered set.
    DCHECK(!collector_->heap()->InNewSpace(target));
    collector_->RecordRelocSlot(host, rinfo, target);
  }

  inline void VisitDebugTarget(RelocInfo* rinfo) final {
    DCHECK(RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
           rinfo->IsPatchedDebugBreakSlotSequence());
    Code* target = Code::GetCodeFromTargetAddress(rinfo->debug_call_address());
    Code* host = rinfo->host();
    // The target is always in old space, we don't have to record the slot in
    // the old-to-new remembered set.
    DCHECK(!collector_->heap()->InNewSpace(target));
    collector_->RecordRelocSlot(host, rinfo, target);
  }

  inline void VisitEmbeddedPointer(RelocInfo* rinfo) final {
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    HeapObject* object = HeapObject::cast(rinfo->target_object());
    Code* host = rinfo->host();
    collector_->heap()->RecordWriteIntoCode(host, rinfo, object);
    collector_->RecordRelocSlot(host, rinfo, object);
  }

  inline void VisitCell(RelocInfo* rinfo) final {
    DCHECK(rinfo->rmode() == RelocInfo::CELL);
    Cell* cell = rinfo->target_cell();
    Code* host = rinfo->host();
    // The cell is always in old space, we don't have to record the slot in
    // the old-to-new remembered set.
    DCHECK(!collector_->heap()->InNewSpace(cell));
    collector_->RecordRelocSlot(host, rinfo, cell);
  }

  // Entries that will never move.
  inline void VisitCodeAgeSequence(RelocInfo* rinfo) final {
    DCHECK(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
    Code* stub = rinfo->code_age_stub();
    USE(stub);
    DCHECK(!Page::FromAddress(stub->address())->IsEvacuationCandidate());
  }

  // Entries that are skipped for recording.
  inline void VisitExternalReference(RelocInfo* rinfo) final {}
  inline void VisitExternalReference(Address* p) final {}
  inline void VisitRuntimeEntry(RelocInfo* rinfo) final {}
  inline void VisitExternalOneByteString(
      v8::String::ExternalOneByteStringResource** resource) final {}
  inline void VisitExternalTwoByteString(
      v8::String::ExternalStringResource** resource) final {}
  inline void VisitInternalReference(RelocInfo* rinfo) final {}
  inline void VisitEmbedderReference(Object** p, uint16_t class_id) final {}

 private:
  inline void RecordMigratedSlot(Object* value, Address slot) {
    if (value->IsHeapObject()) {
      Page* p = Page::FromAddress(reinterpret_cast<Address>(value));
      if (p->InNewSpace()) {
        RememberedSet<OLD_TO_NEW>::Insert(Page::FromAddress(slot), slot);
      } else if (p->IsEvacuationCandidate()) {
        RememberedSet<OLD_TO_OLD>::Insert(Page::FromAddress(slot), slot);
      }
    }
  }

  MarkCompactCollector* collector_;
};

class MarkCompactCollector::HeapObjectVisitor {
 public:
  virtual ~HeapObjectVisitor() {}
  virtual bool Visit(HeapObject* object) = 0;
};

class MarkCompactCollector::EvacuateVisitorBase
    : public MarkCompactCollector::HeapObjectVisitor {
 protected:
  enum MigrationMode { kFast, kProfiled };

  EvacuateVisitorBase(Heap* heap, CompactionSpaceCollection* compaction_spaces)
      : heap_(heap),
        compaction_spaces_(compaction_spaces),
        profiling_(
            heap->isolate()->is_profiling() ||
            heap->isolate()->logger()->is_logging_code_events() ||
            heap->isolate()->heap_profiler()->is_tracking_object_moves()) {}

  inline bool TryEvacuateObject(PagedSpace* target_space, HeapObject* object,
                                HeapObject** target_object) {
#ifdef VERIFY_HEAP
    if (AbortCompactionForTesting(object)) return false;
#endif  // VERIFY_HEAP
    int size = object->Size();
    AllocationAlignment alignment = object->RequiredAlignment();
    AllocationResult allocation = target_space->AllocateRaw(size, alignment);
    if (allocation.To(target_object)) {
      MigrateObject(*target_object, object, size, target_space->identity());
      return true;
    }
    return false;
  }

  inline void MigrateObject(HeapObject* dst, HeapObject* src, int size,
                            AllocationSpace dest) {
    if (profiling_) {
      MigrateObject<kProfiled>(dst, src, size, dest);
    } else {
      MigrateObject<kFast>(dst, src, size, dest);
    }
  }

  template <MigrationMode mode>
  inline void MigrateObject(HeapObject* dst, HeapObject* src, int size,
                            AllocationSpace dest) {
    Address dst_addr = dst->address();
    Address src_addr = src->address();
    DCHECK(heap_->AllowedToBeMigrated(src, dest));
    DCHECK(dest != LO_SPACE);
    if (dest == OLD_SPACE) {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(IsAligned(size, kPointerSize));
      heap_->CopyBlock(dst_addr, src_addr, size);
      if ((mode == kProfiled) && FLAG_ignition && dst->IsBytecodeArray()) {
        PROFILE(heap_->isolate(),
                CodeMoveEvent(AbstractCode::cast(src), dst_addr));
      }
      RecordMigratedSlotVisitor visitor(heap_->mark_compact_collector());
      dst->IterateBodyFast(dst->map()->instance_type(), size, &visitor);
    } else if (dest == CODE_SPACE) {
      DCHECK_CODEOBJECT_SIZE(size, heap_->code_space());
      if (mode == kProfiled) {
        PROFILE(heap_->isolate(),
                CodeMoveEvent(AbstractCode::cast(src), dst_addr));
      }
      heap_->CopyBlock(dst_addr, src_addr, size);
      Code::cast(dst)->Relocate(dst_addr - src_addr);
      RecordMigratedSlotVisitor visitor(heap_->mark_compact_collector());
      dst->IterateBodyFast(dst->map()->instance_type(), size, &visitor);
    } else {
      DCHECK_OBJECT_SIZE(size);
      DCHECK(dest == NEW_SPACE);
      heap_->CopyBlock(dst_addr, src_addr, size);
    }
    if (mode == kProfiled) {
      heap_->OnMoveEvent(dst, src, size);
    }
    Memory::Address_at(src_addr) = dst_addr;
  }

#ifdef VERIFY_HEAP
  bool AbortCompactionForTesting(HeapObject* object) {
    if (FLAG_stress_compaction) {
      const uintptr_t mask = static_cast<uintptr_t>(FLAG_random_seed) &
                             Page::kPageAlignmentMask & ~kPointerAlignmentMask;
      if ((reinterpret_cast<uintptr_t>(object->address()) &
           Page::kPageAlignmentMask) == mask) {
        Page* page = Page::FromAddress(object->address());
        if (page->IsFlagSet(Page::COMPACTION_WAS_ABORTED_FOR_TESTING)) {
          page->ClearFlag(Page::COMPACTION_WAS_ABORTED_FOR_TESTING);
        } else {
          page->SetFlag(Page::COMPACTION_WAS_ABORTED_FOR_TESTING);
          return true;
        }
      }
    }
    return false;
  }
#endif  // VERIFY_HEAP

  Heap* heap_;
  CompactionSpaceCollection* compaction_spaces_;
  bool profiling_;
};

class MarkCompactCollector::EvacuateNewSpaceVisitor final
    : public MarkCompactCollector::EvacuateVisitorBase {
 public:
  static const intptr_t kLabSize = 4 * KB;
  static const intptr_t kMaxLabObjectSize = 256;

  explicit EvacuateNewSpaceVisitor(Heap* heap,
                                   CompactionSpaceCollection* compaction_spaces,
                                   base::HashMap* local_pretenuring_feedback)
      : EvacuateVisitorBase(heap, compaction_spaces),
        buffer_(LocalAllocationBuffer::InvalidBuffer()),
        space_to_allocate_(NEW_SPACE),
        promoted_size_(0),
        semispace_copied_size_(0),
        local_pretenuring_feedback_(local_pretenuring_feedback) {}

  inline bool Visit(HeapObject* object) override {
    heap_->UpdateAllocationSite<Heap::kCached>(object,
                                               local_pretenuring_feedback_);
    int size = object->Size();
    HeapObject* target_object = nullptr;
    if (heap_->ShouldBePromoted<DEFAULT_PROMOTION>(object->address(), size) &&
        TryEvacuateObject(compaction_spaces_->Get(OLD_SPACE), object,
                          &target_object)) {
      promoted_size_ += size;
      return true;
    }
    HeapObject* target = nullptr;
    AllocationSpace space = AllocateTargetObject(object, &target);
    MigrateObject(HeapObject::cast(target), object, size, space);
    semispace_copied_size_ += size;
    return true;
  }

  intptr_t promoted_size() { return promoted_size_; }
  intptr_t semispace_copied_size() { return semispace_copied_size_; }

 private:
  enum NewSpaceAllocationMode {
    kNonstickyBailoutOldSpace,
    kStickyBailoutOldSpace,
  };

  inline AllocationSpace AllocateTargetObject(HeapObject* old_object,
                                              HeapObject** target_object) {
    const int size = old_object->Size();
    AllocationAlignment alignment = old_object->RequiredAlignment();
    AllocationResult allocation;
    AllocationSpace space_allocated_in = space_to_allocate_;
    if (space_to_allocate_ == NEW_SPACE) {
      if (size > kMaxLabObjectSize) {
        allocation =
            AllocateInNewSpace(size, alignment, kNonstickyBailoutOldSpace);
      } else {
        allocation = AllocateInLab(size, alignment);
      }
    }
    if (allocation.IsRetry() || (space_to_allocate_ == OLD_SPACE)) {
      allocation = AllocateInOldSpace(size, alignment);
      space_allocated_in = OLD_SPACE;
    }
    bool ok = allocation.To(target_object);
    DCHECK(ok);
    USE(ok);
    return space_allocated_in;
  }

  inline bool NewLocalAllocationBuffer() {
    AllocationResult result =
        AllocateInNewSpace(kLabSize, kWordAligned, kStickyBailoutOldSpace);
    LocalAllocationBuffer saved_old_buffer = buffer_;
    buffer_ = LocalAllocationBuffer::FromResult(heap_, result, kLabSize);
    if (buffer_.IsValid()) {
      buffer_.TryMerge(&saved_old_buffer);
      return true;
    }
    return false;
  }

  inline AllocationResult AllocateInNewSpace(int size_in_bytes,
                                             AllocationAlignment alignment,
                                             NewSpaceAllocationMode mode) {
    AllocationResult allocation =
        heap_->new_space()->AllocateRawSynchronized(size_in_bytes, alignment);
    if (allocation.IsRetry()) {
      if (!heap_->new_space()->AddFreshPageSynchronized()) {
        if (mode == kStickyBailoutOldSpace) space_to_allocate_ = OLD_SPACE;
      } else {
        allocation = heap_->new_space()->AllocateRawSynchronized(size_in_bytes,
                                                                 alignment);
        if (allocation.IsRetry()) {
          if (mode == kStickyBailoutOldSpace) space_to_allocate_ = OLD_SPACE;
        }
      }
    }
    return allocation;
  }

  inline AllocationResult AllocateInOldSpace(int size_in_bytes,
                                             AllocationAlignment alignment) {
    AllocationResult allocation =
        compaction_spaces_->Get(OLD_SPACE)->AllocateRaw(size_in_bytes,
                                                        alignment);
    if (allocation.IsRetry()) {
      v8::internal::Heap::FatalProcessOutOfMemory(
          "MarkCompactCollector: semi-space copy, fallback in old gen", true);
    }
    return allocation;
  }

  inline AllocationResult AllocateInLab(int size_in_bytes,
                                        AllocationAlignment alignment) {
    AllocationResult allocation;
    if (!buffer_.IsValid()) {
      if (!NewLocalAllocationBuffer()) {
        space_to_allocate_ = OLD_SPACE;
        return AllocationResult::Retry(OLD_SPACE);
      }
    }
    allocation = buffer_.AllocateRawAligned(size_in_bytes, alignment);
    if (allocation.IsRetry()) {
      if (!NewLocalAllocationBuffer()) {
        space_to_allocate_ = OLD_SPACE;
        return AllocationResult::Retry(OLD_SPACE);
      } else {
        allocation = buffer_.AllocateRawAligned(size_in_bytes, alignment);
        if (allocation.IsRetry()) {
          space_to_allocate_ = OLD_SPACE;
          return AllocationResult::Retry(OLD_SPACE);
        }
      }
    }
    return allocation;
  }

  LocalAllocationBuffer buffer_;
  AllocationSpace space_to_allocate_;
  intptr_t promoted_size_;
  intptr_t semispace_copied_size_;
  base::HashMap* local_pretenuring_feedback_;
};

class MarkCompactCollector::EvacuateNewSpacePageVisitor final
    : public MarkCompactCollector::HeapObjectVisitor {
 public:
  explicit EvacuateNewSpacePageVisitor(Heap* heap)
      : heap_(heap), promoted_size_(0), semispace_copied_size_(0) {}

  static void MoveToOldSpace(Page* page, PagedSpace* owner) {
    page->Unlink();
    Page* new_page = Page::ConvertNewToOld(page, owner);
    new_page->SetFlag(Page::PAGE_NEW_OLD_PROMOTION);
  }

  static void MoveToToSpace(Page* page) {
    page->heap()->new_space()->MovePageFromSpaceToSpace(page);
    page->SetFlag(Page::PAGE_NEW_NEW_PROMOTION);
  }

  inline bool Visit(HeapObject* object) {
    RecordMigratedSlotVisitor visitor(heap_->mark_compact_collector());
    object->IterateBodyFast(&visitor);
    promoted_size_ += object->Size();
    return true;
  }

  intptr_t promoted_size() { return promoted_size_; }
  intptr_t semispace_copied_size() { return semispace_copied_size_; }

  void account_semispace_copied(intptr_t copied) {
    semispace_copied_size_ += copied;
  }

 private:
  Heap* heap_;
  intptr_t promoted_size_;
  intptr_t semispace_copied_size_;
};

class MarkCompactCollector::EvacuateOldSpaceVisitor final
    : public MarkCompactCollector::EvacuateVisitorBase {
 public:
  EvacuateOldSpaceVisitor(Heap* heap,
                          CompactionSpaceCollection* compaction_spaces)
      : EvacuateVisitorBase(heap, compaction_spaces) {}

  inline bool Visit(HeapObject* object) override {
    CompactionSpace* target_space = compaction_spaces_->Get(
        Page::FromAddress(object->address())->owner()->identity());
    HeapObject* target_object = nullptr;
    if (TryEvacuateObject(target_space, object, &target_object)) {
      DCHECK(object->map_word().IsForwardingAddress());
      return true;
    }
    return false;
  }
};

class MarkCompactCollector::EvacuateRecordOnlyVisitor final
    : public MarkCompactCollector::HeapObjectVisitor {
 public:
  explicit EvacuateRecordOnlyVisitor(Heap* heap) : heap_(heap) {}

  inline bool Visit(HeapObject* object) {
    RecordMigratedSlotVisitor visitor(heap_->mark_compact_collector());
    object->IterateBody(&visitor);
    return true;
  }

 private:
  Heap* heap_;
};

void MarkCompactCollector::DiscoverGreyObjectsInSpace(PagedSpace* space) {
  for (Page* p : *space) {
    DiscoverGreyObjectsOnPage(p);
    if (marking_deque()->IsFull()) return;
  }
}


void MarkCompactCollector::DiscoverGreyObjectsInNewSpace() {
  NewSpace* space = heap()->new_space();
  for (Page* page : NewSpacePageRange(space->bottom(), space->top())) {
    DiscoverGreyObjectsOnPage(page);
    if (marking_deque()->IsFull()) return;
  }
}


bool MarkCompactCollector::IsUnmarkedHeapObject(Object** p) {
  Object* o = *p;
  if (!o->IsHeapObject()) return false;
  HeapObject* heap_object = HeapObject::cast(o);
  MarkBit mark = ObjectMarking::MarkBitFrom(heap_object);
  return Marking::IsWhite(mark);
}


bool MarkCompactCollector::IsUnmarkedHeapObjectWithHeap(Heap* heap,
                                                        Object** p) {
  Object* o = *p;
  DCHECK(o->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(o);
  MarkBit mark = ObjectMarking::MarkBitFrom(heap_object);
  return Marking::IsWhite(mark);
}


void MarkCompactCollector::MarkStringTable(RootMarkingVisitor* visitor) {
  StringTable* string_table = heap()->string_table();
  // Mark the string table itself.
  MarkBit string_table_mark = ObjectMarking::MarkBitFrom(string_table);
  if (Marking::IsWhite(string_table_mark)) {
    // String table could have already been marked by visiting the handles list.
    SetMark(string_table, string_table_mark);
  }
  // Explicitly mark the prefix.
  string_table->IteratePrefix(visitor);
  ProcessMarkingDeque();
}


void MarkCompactCollector::MarkAllocationSite(AllocationSite* site) {
  MarkBit mark_bit = ObjectMarking::MarkBitFrom(site);
  SetMark(site, mark_bit);
}


void MarkCompactCollector::MarkRoots(RootMarkingVisitor* visitor) {
  // Mark the heap roots including global variables, stack variables,
  // etc., and all objects reachable from them.
  heap()->IterateStrongRoots(visitor, VISIT_ONLY_STRONG);

  // Handle the string table specially.
  MarkStringTable(visitor);

  // There may be overflowed objects in the heap.  Visit them now.
  while (marking_deque_.overflowed()) {
    RefillMarkingDeque();
    EmptyMarkingDeque();
  }
}


void MarkCompactCollector::MarkImplicitRefGroups(
    MarkObjectFunction mark_object) {
  List<ImplicitRefGroup*>* ref_groups =
      isolate()->global_handles()->implicit_ref_groups();

  int last = 0;
  for (int i = 0; i < ref_groups->length(); i++) {
    ImplicitRefGroup* entry = ref_groups->at(i);
    DCHECK(entry != NULL);

    if (!IsMarked(*entry->parent)) {
      (*ref_groups)[last++] = entry;
      continue;
    }

    Object*** children = entry->children;
    // A parent object is marked, so mark all child heap objects.
    for (size_t j = 0; j < entry->length; ++j) {
      if ((*children[j])->IsHeapObject()) {
        mark_object(heap(), HeapObject::cast(*children[j]));
      }
    }

    // Once the entire group has been marked, dispose it because it's
    // not needed anymore.
    delete entry;
  }
  ref_groups->Rewind(last);
}


// Mark all objects reachable from the objects on the marking stack.
// Before: the marking stack contains zero or more heap object pointers.
// After: the marking stack is empty, and all objects reachable from the
// marking stack have been marked, or are overflowed in the heap.
void MarkCompactCollector::EmptyMarkingDeque() {
  while (!marking_deque_.IsEmpty()) {
    HeapObject* object = marking_deque_.Pop();

    DCHECK(!object->IsFiller());
    DCHECK(object->IsHeapObject());
    DCHECK(heap()->Contains(object));
    DCHECK(!Marking::IsWhite(ObjectMarking::MarkBitFrom(object)));

    Map* map = object->map();
    MarkBit map_mark = ObjectMarking::MarkBitFrom(map);
    MarkObject(map, map_mark);

    MarkCompactMarkingVisitor::IterateBody(map, object);
  }
}


// Sweep the heap for overflowed objects, clear their overflow bits, and
// push them on the marking stack.  Stop early if the marking stack fills
// before sweeping completes.  If sweeping completes, there are no remaining
// overflowed objects in the heap so the overflow flag on the markings stack
// is cleared.
void MarkCompactCollector::RefillMarkingDeque() {
  isolate()->CountUsage(v8::Isolate::UseCounterFeature::kMarkDequeOverflow);
  DCHECK(marking_deque_.overflowed());

  DiscoverGreyObjectsInNewSpace();
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap()->old_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap()->code_space());
  if (marking_deque_.IsFull()) return;

  DiscoverGreyObjectsInSpace(heap()->map_space());
  if (marking_deque_.IsFull()) return;

  LargeObjectIterator lo_it(heap()->lo_space());
  DiscoverGreyObjectsWithIterator(&lo_it);
  if (marking_deque_.IsFull()) return;

  marking_deque_.ClearOverflowed();
}


// Mark all objects reachable (transitively) from objects on the marking
// stack.  Before: the marking stack contains zero or more heap object
// pointers.  After: the marking stack is empty and there are no overflowed
// objects in the heap.
void MarkCompactCollector::ProcessMarkingDeque() {
  EmptyMarkingDeque();
  while (marking_deque_.overflowed()) {
    RefillMarkingDeque();
    EmptyMarkingDeque();
  }
}

// Mark all objects reachable (transitively) from objects on the marking
// stack including references only considered in the atomic marking pause.
void MarkCompactCollector::ProcessEphemeralMarking(
    ObjectVisitor* visitor, bool only_process_harmony_weak_collections) {
  DCHECK(marking_deque_.IsEmpty() && !marking_deque_.overflowed());
  bool work_to_do = true;
  while (work_to_do) {
    if (UsingEmbedderHeapTracer()) {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WRAPPER_TRACING);
      RegisterWrappersWithEmbedderHeapTracer();
      embedder_heap_tracer()->AdvanceTracing(
          0, EmbedderHeapTracer::AdvanceTracingActions(
                 EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
    }
    if (!only_process_harmony_weak_collections) {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_OBJECT_GROUPING);
      isolate()->global_handles()->IterateObjectGroups(
          visitor, &IsUnmarkedHeapObjectWithHeap);
      MarkImplicitRefGroups(&MarkCompactMarkingVisitor::MarkObject);
    }
    ProcessWeakCollections();
    work_to_do = !marking_deque_.IsEmpty();
    ProcessMarkingDeque();
  }
}

void MarkCompactCollector::ProcessTopOptimizedFrame(ObjectVisitor* visitor) {
  for (StackFrameIterator it(isolate(), isolate()->thread_local_top());
       !it.done(); it.Advance()) {
    if (it.frame()->type() == StackFrame::JAVA_SCRIPT) {
      return;
    }
    if (it.frame()->type() == StackFrame::OPTIMIZED) {
      Code* code = it.frame()->LookupCode();
      if (!code->CanDeoptAt(it.frame()->pc())) {
        Code::BodyDescriptor::IterateBody(code, visitor);
      }
      ProcessMarkingDeque();
      return;
    }
  }
}


void MarkCompactCollector::EnsureMarkingDequeIsReserved() {
  DCHECK(!marking_deque_.in_use());
  if (marking_deque_memory_ == NULL) {
    marking_deque_memory_ = new base::VirtualMemory(kMaxMarkingDequeSize);
    marking_deque_memory_committed_ = 0;
  }
  if (marking_deque_memory_ == NULL) {
    V8::FatalProcessOutOfMemory("EnsureMarkingDequeIsReserved");
  }
}


void MarkCompactCollector::EnsureMarkingDequeIsCommitted(size_t max_size) {
  // If the marking deque is too small, we try to allocate a bigger one.
  // If that fails, make do with a smaller one.
  CHECK(!marking_deque_.in_use());
  for (size_t size = max_size; size >= kMinMarkingDequeSize; size >>= 1) {
    base::VirtualMemory* memory = marking_deque_memory_;
    size_t currently_committed = marking_deque_memory_committed_;

    if (currently_committed == size) return;

    if (currently_committed > size) {
      bool success = marking_deque_memory_->Uncommit(
          reinterpret_cast<Address>(marking_deque_memory_->address()) + size,
          currently_committed - size);
      if (success) {
        marking_deque_memory_committed_ = size;
        return;
      }
      UNREACHABLE();
    }

    bool success = memory->Commit(
        reinterpret_cast<Address>(memory->address()) + currently_committed,
        size - currently_committed,
        false);  // Not executable.
    if (success) {
      marking_deque_memory_committed_ = size;
      return;
    }
  }
  V8::FatalProcessOutOfMemory("EnsureMarkingDequeIsCommitted");
}


void MarkCompactCollector::InitializeMarkingDeque() {
  DCHECK(!marking_deque_.in_use());
  DCHECK(marking_deque_memory_committed_ > 0);
  Address addr = static_cast<Address>(marking_deque_memory_->address());
  size_t size = marking_deque_memory_committed_;
  if (FLAG_force_marking_deque_overflows) size = 64 * kPointerSize;
  marking_deque_.Initialize(addr, addr + size);
}


void MarkingDeque::Initialize(Address low, Address high) {
  DCHECK(!in_use_);
  HeapObject** obj_low = reinterpret_cast<HeapObject**>(low);
  HeapObject** obj_high = reinterpret_cast<HeapObject**>(high);
  array_ = obj_low;
  mask_ = base::bits::RoundDownToPowerOfTwo32(
              static_cast<uint32_t>(obj_high - obj_low)) -
          1;
  top_ = bottom_ = 0;
  overflowed_ = false;
  in_use_ = true;
}


void MarkingDeque::Uninitialize(bool aborting) {
  if (!aborting) {
    DCHECK(IsEmpty());
    DCHECK(!overflowed_);
  }
  DCHECK(in_use_);
  top_ = bottom_ = 0xdecbad;
  in_use_ = false;
}

void MarkCompactCollector::SetEmbedderHeapTracer(EmbedderHeapTracer* tracer) {
  DCHECK_NOT_NULL(tracer);
  CHECK_NULL(embedder_heap_tracer_);
  embedder_heap_tracer_ = tracer;
}

void MarkCompactCollector::RegisterWrappersWithEmbedderHeapTracer() {
  DCHECK(UsingEmbedderHeapTracer());
  if (wrappers_to_trace_.empty()) {
    return;
  }
  embedder_heap_tracer()->RegisterV8References(wrappers_to_trace_);
  wrappers_to_trace_.clear();
}

void MarkCompactCollector::TracePossibleWrapper(JSObject* js_object) {
  DCHECK(js_object->WasConstructedFromApiFunction());
  if (js_object->GetInternalFieldCount() >= 2 &&
      js_object->GetInternalField(0) &&
      js_object->GetInternalField(0) != heap_->undefined_value() &&
      js_object->GetInternalField(1) != heap_->undefined_value()) {
    DCHECK(reinterpret_cast<intptr_t>(js_object->GetInternalField(0)) % 2 == 0);
    wrappers_to_trace_.push_back(std::pair<void*, void*>(
        reinterpret_cast<void*>(js_object->GetInternalField(0)),
        reinterpret_cast<void*>(js_object->GetInternalField(1))));
  }
}

class MarkCompactCollector::ObjectStatsVisitor
    : public MarkCompactCollector::HeapObjectVisitor {
 public:
  ObjectStatsVisitor(Heap* heap, ObjectStats* live_stats,
                     ObjectStats* dead_stats)
      : live_collector_(heap, live_stats), dead_collector_(heap, dead_stats) {
    DCHECK_NOT_NULL(live_stats);
    DCHECK_NOT_NULL(dead_stats);
    // Global objects are roots and thus recorded as live.
    live_collector_.CollectGlobalStatistics();
  }

  bool Visit(HeapObject* obj) override {
    if (Marking::IsBlack(ObjectMarking::MarkBitFrom(obj))) {
      live_collector_.CollectStatistics(obj);
    } else {
      DCHECK(!Marking::IsGrey(ObjectMarking::MarkBitFrom(obj)));
      dead_collector_.CollectStatistics(obj);
    }
    return true;
  }

 private:
  ObjectStatsCollector live_collector_;
  ObjectStatsCollector dead_collector_;
};

void MarkCompactCollector::VisitAllObjects(HeapObjectVisitor* visitor) {
  SpaceIterator space_it(heap());
  HeapObject* obj = nullptr;
  while (space_it.has_next()) {
    ObjectIterator* it = space_it.next();
    while ((obj = it->Next()) != nullptr) {
      visitor->Visit(obj);
    }
  }
}

void MarkCompactCollector::RecordObjectStats() {
  if (FLAG_track_gc_object_stats) {
    ObjectStatsVisitor visitor(heap(), heap()->live_object_stats_,
                               heap()->dead_object_stats_);
    VisitAllObjects(&visitor);
    if (FLAG_trace_gc_object_stats) {
      heap()->live_object_stats_->PrintJSON("live");
      heap()->dead_object_stats_->PrintJSON("dead");
    }
    heap()->live_object_stats_->CheckpointObjectStats();
    heap()->dead_object_stats_->ClearObjectStats();
  }
}

void MarkCompactCollector::MarkLiveObjects() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK);
  double start_time = 0.0;
  if (FLAG_print_cumulative_gc_stat) {
    start_time = heap_->MonotonicallyIncreasingTimeInMs();
  }
  // The recursive GC marker detects when it is nearing stack overflow,
  // and switches to a different marking system.  JS interrupts interfere
  // with the C stack limit check.
  PostponeInterruptsScope postpone(isolate());

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_FINISH_INCREMENTAL);
    IncrementalMarking* incremental_marking = heap_->incremental_marking();
    if (was_marked_incrementally_) {
      incremental_marking->Finalize();
    } else {
      // Abort any pending incremental activities e.g. incremental sweeping.
      incremental_marking->Stop();
      if (marking_deque_.in_use()) {
        marking_deque_.Uninitialize(true);
      }
    }
  }

#ifdef DEBUG
  DCHECK(state_ == PREPARE_GC);
  state_ = MARK_LIVE_OBJECTS;
#endif

  EnsureMarkingDequeIsCommittedAndInitialize(
      MarkCompactCollector::kMaxMarkingDequeSize);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_PREPARE_CODE_FLUSH);
    PrepareForCodeFlushing();
  }

  RootMarkingVisitor root_visitor(heap());

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_ROOTS);
    MarkRoots(&root_visitor);
    ProcessTopOptimizedFrame(&root_visitor);
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WEAK_CLOSURE);

    // The objects reachable from the roots are marked, yet unreachable
    // objects are unmarked.  Mark objects reachable due to host
    // application specific logic or through Harmony weak maps.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_EPHEMERAL);
      ProcessEphemeralMarking(&root_visitor, false);
    }

    // The objects reachable from the roots, weak maps or object groups
    // are marked. Objects pointed to only by weak global handles cannot be
    // immediately reclaimed. Instead, we have to mark them as pending and mark
    // objects reachable from them.
    //
    // First we identify nonlive weak handles and mark them as pending
    // destruction.
    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_WEAK_HANDLES);
      heap()->isolate()->global_handles()->IdentifyWeakHandles(
          &IsUnmarkedHeapObject);
      ProcessMarkingDeque();
    }
    // Then we mark the objects.

    {
      TRACE_GC(heap()->tracer(),
               GCTracer::Scope::MC_MARK_WEAK_CLOSURE_WEAK_ROOTS);
      heap()->isolate()->global_handles()->IterateWeakRoots(&root_visitor);
      ProcessMarkingDeque();
    }

    // Repeat Harmony weak maps marking to mark unmarked objects reachable from
    // the weak roots we just marked as pending destruction.
    //
    // We only process harmony collections, as all object groups have been fully
    // processed and no weakly reachable node can discover new objects groups.
    {
      TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WEAK_CLOSURE_HARMONY);
      ProcessEphemeralMarking(&root_visitor, true);
      if (UsingEmbedderHeapTracer()) {
        TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_WRAPPER_EPILOGUE);
        embedder_heap_tracer()->TraceEpilogue();
      }
    }
  }

  if (FLAG_print_cumulative_gc_stat) {
    heap_->tracer()->AddMarkingTime(heap_->MonotonicallyIncreasingTimeInMs() -
                                    start_time);
  }
}


void MarkCompactCollector::ClearNonLiveReferences() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_STRING_TABLE);

    // Prune the string table removing all strings only pointed to by the
    // string table.  Cannot use string_table() here because the string
    // table is marked.
    StringTable* string_table = heap()->string_table();
    InternalizedStringTableCleaner internalized_visitor(heap(), string_table);
    string_table->IterateElements(&internalized_visitor);
    string_table->ElementsRemoved(internalized_visitor.PointersRemoved());

    ExternalStringTableCleaner external_visitor(heap(), nullptr);
    heap()->external_string_table_.Iterate(&external_visitor);
    heap()->external_string_table_.CleanUp();
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_LISTS);
    // Process the weak references.
    MarkCompactWeakObjectRetainer mark_compact_object_retainer;
    heap()->ProcessAllWeakReferences(&mark_compact_object_retainer);
  }

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_GLOBAL_HANDLES);

    // Remove object groups after marking phase.
    heap()->isolate()->global_handles()->RemoveObjectGroups();
    heap()->isolate()->global_handles()->RemoveImplicitRefGroups();
  }

  // Flush code from collected candidates.
  if (is_code_flushing_enabled()) {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_CODE_FLUSH);
    code_flusher_->ProcessCandidates();
  }


  DependentCode* dependent_code_list;
  Object* non_live_map_list;
  ClearWeakCells(&non_live_map_list, &dependent_code_list);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_MAPS);
    ClearSimpleMapTransitions(non_live_map_list);
    ClearFullMapTransitions();
  }

  MarkDependentCodeForDeoptimization(dependent_code_list);

  ClearWeakCollections();

  ClearInvalidRememberedSetSlots();
}


void MarkCompactCollector::MarkDependentCodeForDeoptimization(
    DependentCode* list_head) {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_DEPENDENT_CODE);
  Isolate* isolate = this->isolate();
  DependentCode* current = list_head;
  while (current->length() > 0) {
    have_code_to_deoptimize_ |= current->MarkCodeForDeoptimization(
        isolate, DependentCode::kWeakCodeGroup);
    current = current->next_link();
  }

  {
    ArrayList* list = heap_->weak_new_space_object_to_code_list();
    int counter = 0;
    for (int i = 0; i < list->Length(); i += 2) {
      WeakCell* obj = WeakCell::cast(list->Get(i));
      WeakCell* dep = WeakCell::cast(list->Get(i + 1));
      if (obj->cleared() || dep->cleared()) {
        if (!dep->cleared()) {
          Code* code = Code::cast(dep->value());
          if (!code->marked_for_deoptimization()) {
            DependentCode::SetMarkedForDeoptimization(
                code, DependentCode::DependencyGroup::kWeakCodeGroup);
            code->InvalidateEmbeddedObjects();
            have_code_to_deoptimize_ = true;
          }
        }
      } else {
        // We record the slot manually because marking is finished at this
        // point and the write barrier would bailout.
        list->Set(counter, obj, SKIP_WRITE_BARRIER);
        RecordSlot(list, list->Slot(counter), obj);
        counter++;
        list->Set(counter, dep, SKIP_WRITE_BARRIER);
        RecordSlot(list, list->Slot(counter), dep);
        counter++;
      }
    }
  }

  WeakHashTable* table = heap_->weak_object_to_code_table();
  uint32_t capacity = table->Capacity();
  for (uint32_t i = 0; i < capacity; i++) {
    uint32_t key_index = table->EntryToIndex(i);
    Object* key = table->get(key_index);
    if (!table->IsKey(isolate, key)) continue;
    uint32_t value_index = table->EntryToValueIndex(i);
    Object* value = table->get(value_index);
    DCHECK(key->IsWeakCell());
    if (WeakCell::cast(key)->cleared()) {
      have_code_to_deoptimize_ |=
          DependentCode::cast(value)->MarkCodeForDeoptimization(
              isolate, DependentCode::kWeakCodeGroup);
      table->set(key_index, heap_->the_hole_value());
      table->set(value_index, heap_->the_hole_value());
      table->ElementRemoved();
    }
  }
}


void MarkCompactCollector::ClearSimpleMapTransitions(
    Object* non_live_map_list) {
  Object* the_hole_value = heap()->the_hole_value();
  Object* weak_cell_obj = non_live_map_list;
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = WeakCell::cast(weak_cell_obj);
    Map* map = Map::cast(weak_cell->value());
    DCHECK(Marking::IsWhite(ObjectMarking::MarkBitFrom(map)));
    Object* potential_parent = map->constructor_or_backpointer();
    if (potential_parent->IsMap()) {
      Map* parent = Map::cast(potential_parent);
      if (Marking::IsBlackOrGrey(ObjectMarking::MarkBitFrom(parent)) &&
          parent->raw_transitions() == weak_cell) {
        ClearSimpleMapTransition(parent, map);
      }
    }
    weak_cell->clear();
    weak_cell_obj = weak_cell->next();
    weak_cell->clear_next(the_hole_value);
  }
}


void MarkCompactCollector::ClearSimpleMapTransition(Map* map,
                                                    Map* dead_transition) {
  // A previously existing simple transition (stored in a WeakCell) is going
  // to be cleared. Clear the useless cell pointer, and take ownership
  // of the descriptor array.
  map->set_raw_transitions(Smi::FromInt(0));
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  DescriptorArray* descriptors = map->instance_descriptors();
  if (descriptors == dead_transition->instance_descriptors() &&
      number_of_own_descriptors > 0) {
    TrimDescriptorArray(map, descriptors);
    DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
    map->set_owns_descriptors(true);
  }
}


void MarkCompactCollector::ClearFullMapTransitions() {
  HeapObject* undefined = heap()->undefined_value();
  Object* obj = heap()->encountered_transition_arrays();
  while (obj != Smi::FromInt(0)) {
    TransitionArray* array = TransitionArray::cast(obj);
    int num_transitions = array->number_of_entries();
    DCHECK_EQ(TransitionArray::NumberOfTransitions(array), num_transitions);
    if (num_transitions > 0) {
      Map* map = array->GetTarget(0);
      Map* parent = Map::cast(map->constructor_or_backpointer());
      bool parent_is_alive =
          Marking::IsBlackOrGrey(ObjectMarking::MarkBitFrom(parent));
      DescriptorArray* descriptors =
          parent_is_alive ? parent->instance_descriptors() : nullptr;
      bool descriptors_owner_died =
          CompactTransitionArray(parent, array, descriptors);
      if (descriptors_owner_died) {
        TrimDescriptorArray(parent, descriptors);
      }
    }
    obj = array->next_link();
    array->set_next_link(undefined, SKIP_WRITE_BARRIER);
  }
  heap()->set_encountered_transition_arrays(Smi::FromInt(0));
}


bool MarkCompactCollector::CompactTransitionArray(
    Map* map, TransitionArray* transitions, DescriptorArray* descriptors) {
  int num_transitions = transitions->number_of_entries();
  bool descriptors_owner_died = false;
  int transition_index = 0;
  // Compact all live transitions to the left.
  for (int i = 0; i < num_transitions; ++i) {
    Map* target = transitions->GetTarget(i);
    DCHECK_EQ(target->constructor_or_backpointer(), map);
    if (Marking::IsWhite(ObjectMarking::MarkBitFrom(target))) {
      if (descriptors != nullptr &&
          target->instance_descriptors() == descriptors) {
        descriptors_owner_died = true;
      }
    } else {
      if (i != transition_index) {
        Name* key = transitions->GetKey(i);
        transitions->SetKey(transition_index, key);
        Object** key_slot = transitions->GetKeySlot(transition_index);
        RecordSlot(transitions, key_slot, key);
        // Target slots do not need to be recorded since maps are not compacted.
        transitions->SetTarget(transition_index, transitions->GetTarget(i));
      }
      transition_index++;
    }
  }
  // If there are no transitions to be cleared, return.
  if (transition_index == num_transitions) {
    DCHECK(!descriptors_owner_died);
    return false;
  }
  // Note that we never eliminate a transition array, though we might right-trim
  // such that number_of_transitions() == 0. If this assumption changes,
  // TransitionArray::Insert() will need to deal with the case that a transition
  // array disappeared during GC.
  int trim = TransitionArray::Capacity(transitions) - transition_index;
  if (trim > 0) {
    heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(
        transitions, trim * TransitionArray::kTransitionSize);
    transitions->SetNumberOfTransitions(transition_index);
  }
  return descriptors_owner_died;
}


void MarkCompactCollector::TrimDescriptorArray(Map* map,
                                               DescriptorArray* descriptors) {
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) {
    DCHECK(descriptors == heap_->empty_descriptor_array());
    return;
  }

  int number_of_descriptors = descriptors->number_of_descriptors_storage();
  int to_trim = number_of_descriptors - number_of_own_descriptors;
  if (to_trim > 0) {
    heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(
        descriptors, to_trim * DescriptorArray::kDescriptorSize);
    descriptors->SetNumberOfDescriptors(number_of_own_descriptors);

    if (descriptors->HasEnumCache()) TrimEnumCache(map, descriptors);
    descriptors->Sort();

    if (FLAG_unbox_double_fields) {
      LayoutDescriptor* layout_descriptor = map->layout_descriptor();
      layout_descriptor = layout_descriptor->Trim(heap_, map, descriptors,
                                                  number_of_own_descriptors);
      SLOW_DCHECK(layout_descriptor->IsConsistentWithMap(map, true));
    }
  }
  DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
  map->set_owns_descriptors(true);
}


void MarkCompactCollector::TrimEnumCache(Map* map,
                                         DescriptorArray* descriptors) {
  int live_enum = map->EnumLength();
  if (live_enum == kInvalidEnumCacheSentinel) {
    live_enum =
        map->NumberOfDescribedProperties(OWN_DESCRIPTORS, ENUMERABLE_STRINGS);
  }
  if (live_enum == 0) return descriptors->ClearEnumCache();

  FixedArray* enum_cache = descriptors->GetEnumCache();

  int to_trim = enum_cache->length() - live_enum;
  if (to_trim <= 0) return;
  heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(
      descriptors->GetEnumCache(), to_trim);

  if (!descriptors->HasEnumIndicesCache()) return;
  FixedArray* enum_indices_cache = descriptors->GetEnumIndicesCache();
  heap_->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(enum_indices_cache,
                                                          to_trim);
}


void MarkCompactCollector::ProcessWeakCollections() {
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    DCHECK(MarkCompactCollector::IsMarked(weak_collection));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      for (int i = 0; i < table->Capacity(); i++) {
        if (MarkCompactCollector::IsMarked(HeapObject::cast(table->KeyAt(i)))) {
          Object** key_slot =
              table->RawFieldOfElementAt(ObjectHashTable::EntryToIndex(i));
          RecordSlot(table, key_slot, *key_slot);
          Object** value_slot =
              table->RawFieldOfElementAt(ObjectHashTable::EntryToValueIndex(i));
          MarkCompactMarkingVisitor::MarkObjectByPointer(this, table,
                                                         value_slot);
        }
      }
    }
    weak_collection_obj = weak_collection->next();
  }
}


void MarkCompactCollector::ClearWeakCollections() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_COLLECTIONS);
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    DCHECK(MarkCompactCollector::IsMarked(weak_collection));
    if (weak_collection->table()->IsHashTable()) {
      ObjectHashTable* table = ObjectHashTable::cast(weak_collection->table());
      for (int i = 0; i < table->Capacity(); i++) {
        HeapObject* key = HeapObject::cast(table->KeyAt(i));
        if (!MarkCompactCollector::IsMarked(key)) {
          table->RemoveEntry(i);
        }
      }
    }
    weak_collection_obj = weak_collection->next();
    weak_collection->set_next(heap()->undefined_value());
  }
  heap()->set_encountered_weak_collections(Smi::FromInt(0));
}


void MarkCompactCollector::AbortWeakCollections() {
  Object* weak_collection_obj = heap()->encountered_weak_collections();
  while (weak_collection_obj != Smi::FromInt(0)) {
    JSWeakCollection* weak_collection =
        reinterpret_cast<JSWeakCollection*>(weak_collection_obj);
    weak_collection_obj = weak_collection->next();
    weak_collection->set_next(heap()->undefined_value());
  }
  heap()->set_encountered_weak_collections(Smi::FromInt(0));
}


void MarkCompactCollector::ClearWeakCells(Object** non_live_map_list,
                                          DependentCode** dependent_code_list) {
  Heap* heap = this->heap();
  TRACE_GC(heap->tracer(), GCTracer::Scope::MC_CLEAR_WEAK_CELLS);
  Object* weak_cell_obj = heap->encountered_weak_cells();
  Object* the_hole_value = heap->the_hole_value();
  DependentCode* dependent_code_head =
      DependentCode::cast(heap->empty_fixed_array());
  Object* non_live_map_head = Smi::FromInt(0);
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    Object* next_weak_cell = weak_cell->next();
    bool clear_value = true;
    bool clear_next = true;
    // We do not insert cleared weak cells into the list, so the value
    // cannot be a Smi here.
    HeapObject* value = HeapObject::cast(weak_cell->value());
    if (!MarkCompactCollector::IsMarked(value)) {
      // Cells for new-space objects embedded in optimized code are wrapped in
      // WeakCell and put into Heap::weak_object_to_code_table.
      // Such cells do not have any strong references but we want to keep them
      // alive as long as the cell value is alive.
      // TODO(ulan): remove this once we remove Heap::weak_object_to_code_table.
      if (value->IsCell()) {
        Object* cell_value = Cell::cast(value)->value();
        if (cell_value->IsHeapObject() &&
            MarkCompactCollector::IsMarked(HeapObject::cast(cell_value))) {
          // Resurrect the cell.
          MarkBit mark = ObjectMarking::MarkBitFrom(value);
          SetMark(value, mark);
          Object** slot = HeapObject::RawField(value, Cell::kValueOffset);
          RecordSlot(value, slot, *slot);
          slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
          RecordSlot(weak_cell, slot, *slot);
          clear_value = false;
        }
      }
      if (value->IsMap()) {
        // The map is non-live.
        Map* map = Map::cast(value);
        // Add dependent code to the dependent_code_list.
        DependentCode* candidate = map->dependent_code();
        // We rely on the fact that the weak code group comes first.
        STATIC_ASSERT(DependentCode::kWeakCodeGroup == 0);
        if (candidate->length() > 0 &&
            candidate->group() == DependentCode::kWeakCodeGroup) {
          candidate->set_next_link(dependent_code_head);
          dependent_code_head = candidate;
        }
        // Add the weak cell to the non_live_map list.
        weak_cell->set_next(non_live_map_head);
        non_live_map_head = weak_cell;
        clear_value = false;
        clear_next = false;
      }
    } else {
      // The value of the weak cell is alive.
      Object** slot = HeapObject::RawField(weak_cell, WeakCell::kValueOffset);
      RecordSlot(weak_cell, slot, *slot);
      clear_value = false;
    }
    if (clear_value) {
      weak_cell->clear();
    }
    if (clear_next) {
      weak_cell->clear_next(the_hole_value);
    }
    weak_cell_obj = next_weak_cell;
  }
  heap->set_encountered_weak_cells(Smi::FromInt(0));
  *non_live_map_list = non_live_map_head;
  *dependent_code_list = dependent_code_head;
}


void MarkCompactCollector::AbortWeakCells() {
  Object* the_hole_value = heap()->the_hole_value();
  Object* weak_cell_obj = heap()->encountered_weak_cells();
  while (weak_cell_obj != Smi::FromInt(0)) {
    WeakCell* weak_cell = reinterpret_cast<WeakCell*>(weak_cell_obj);
    weak_cell_obj = weak_cell->next();
    weak_cell->clear_next(the_hole_value);
  }
  heap()->set_encountered_weak_cells(Smi::FromInt(0));
}


void MarkCompactCollector::AbortTransitionArrays() {
  HeapObject* undefined = heap()->undefined_value();
  Object* obj = heap()->encountered_transition_arrays();
  while (obj != Smi::FromInt(0)) {
    TransitionArray* array = TransitionArray::cast(obj);
    obj = array->next_link();
    array->set_next_link(undefined, SKIP_WRITE_BARRIER);
  }
  heap()->set_encountered_transition_arrays(Smi::FromInt(0));
}

void MarkCompactCollector::RecordRelocSlot(Code* host, RelocInfo* rinfo,
                                           Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(host));
  if (target_page->IsEvacuationCandidate() &&
      (rinfo->host() == NULL ||
       !ShouldSkipEvacuationSlotRecording(rinfo->host()))) {
    RelocInfo::Mode rmode = rinfo->rmode();
    Address addr = rinfo->pc();
    SlotType slot_type = SlotTypeForRelocInfoMode(rmode);
    if (rinfo->IsInConstantPool()) {
      addr = rinfo->constant_pool_entry_address();
      if (RelocInfo::IsCodeTarget(rmode)) {
        slot_type = CODE_ENTRY_SLOT;
      } else {
        DCHECK(RelocInfo::IsEmbeddedObject(rmode));
        slot_type = OBJECT_SLOT;
      }
    }
    RememberedSet<OLD_TO_OLD>::InsertTyped(
        source_page, reinterpret_cast<Address>(host), slot_type, addr);
  }
}

static inline SlotCallbackResult UpdateSlot(Object** slot) {
  Object* obj = reinterpret_cast<Object*>(
      base::NoBarrier_Load(reinterpret_cast<base::AtomicWord*>(slot)));

  if (obj->IsHeapObject()) {
    HeapObject* heap_obj = HeapObject::cast(obj);
    MapWord map_word = heap_obj->map_word();
    if (map_word.IsForwardingAddress()) {
      DCHECK(heap_obj->GetHeap()->InFromSpace(heap_obj) ||
             MarkCompactCollector::IsOnEvacuationCandidate(heap_obj) ||
             Page::FromAddress(heap_obj->address())
                 ->IsFlagSet(Page::COMPACTION_WAS_ABORTED));
      HeapObject* target = map_word.ToForwardingAddress();
      base::NoBarrier_CompareAndSwap(
          reinterpret_cast<base::AtomicWord*>(slot),
          reinterpret_cast<base::AtomicWord>(obj),
          reinterpret_cast<base::AtomicWord>(target));
      DCHECK(!heap_obj->GetHeap()->InFromSpace(target) &&
             !MarkCompactCollector::IsOnEvacuationCandidate(target));
    }
  }
  return REMOVE_SLOT;
}

// Visitor for updating pointers from live objects in old spaces to new space.
// It does not expect to encounter pointers to dead objects.
class PointersUpdatingVisitor : public ObjectVisitor {
 public:
  void VisitPointer(Object** p) override { UpdateSlot(p); }

  void VisitPointers(Object** start, Object** end) override {
    for (Object** p = start; p < end; p++) UpdateSlot(p);
  }

  void VisitCell(RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateCell(rinfo, UpdateSlot);
  }

  void VisitEmbeddedPointer(RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateEmbeddedPointer(rinfo, UpdateSlot);
  }

  void VisitCodeTarget(RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateCodeTarget(rinfo, UpdateSlot);
  }

  void VisitCodeEntry(Address entry_address) override {
    UpdateTypedSlotHelper::UpdateCodeEntry(entry_address, UpdateSlot);
  }

  void VisitDebugTarget(RelocInfo* rinfo) override {
    UpdateTypedSlotHelper::UpdateDebugTarget(rinfo, UpdateSlot);
  }
};

static String* UpdateReferenceInExternalStringTableEntry(Heap* heap,
                                                         Object** p) {
  MapWord map_word = HeapObject::cast(*p)->map_word();

  if (map_word.IsForwardingAddress()) {
    return String::cast(map_word.ToForwardingAddress());
  }

  return String::cast(*p);
}

bool MarkCompactCollector::IsSlotInBlackObject(MemoryChunk* p, Address slot) {
  Space* owner = p->owner();
  DCHECK(owner != heap_->lo_space() && owner != nullptr);
  USE(owner);

  // We may be part of a black area.
  if (Marking::IsBlackOrGrey(ObjectMarking::MarkBitFrom(slot))) {
    return true;
  }

  uint32_t mark_bit_index = p->AddressToMarkbitIndex(slot);
  unsigned int cell_index = mark_bit_index >> Bitmap::kBitsPerCellLog2;
  MarkBit::CellType index_mask = 1u << Bitmap::IndexInCell(mark_bit_index);
  MarkBit::CellType* cells = p->markbits()->cells();
  Address base_address = p->area_start();
  unsigned int base_address_cell_index = Bitmap::IndexToCell(
      Bitmap::CellAlignIndex(p->AddressToMarkbitIndex(base_address)));

  // Check if the slot points to the start of an object. This can happen e.g.
  // when we left trim a fixed array. Such slots are invalid and we can remove
  // them.
  if (index_mask > 1) {
    if ((cells[cell_index] & index_mask) != 0 &&
        (cells[cell_index] & (index_mask >> 1)) == 0) {
      return false;
    }
  } else {
    // Left trimming moves the mark bits so we cannot be in the very first cell.
    DCHECK(cell_index != base_address_cell_index);
    if ((cells[cell_index] & index_mask) != 0 &&
        (cells[cell_index - 1] & (1u << Bitmap::kBitIndexMask)) == 0) {
      return false;
    }
  }

  // Check if the object is in the current cell.
  MarkBit::CellType slot_mask;
  if ((cells[cell_index] == 0) ||
      (base::bits::CountTrailingZeros32(cells[cell_index]) >
       base::bits::CountTrailingZeros32(cells[cell_index] | index_mask))) {
    // If we are already in the first cell, there is no live object.
    if (cell_index == base_address_cell_index) return false;

    // If not, find a cell in a preceding cell slot that has a mark bit set.
    do {
      cell_index--;
    } while (cell_index > base_address_cell_index && cells[cell_index] == 0);

    // The slot must be in a dead object if there are no preceding cells that
    // have mark bits set.
    if (cells[cell_index] == 0) {
      return false;
    }

    // The object is in a preceding cell. Set the mask to find any object.
    slot_mask = ~0u;
  } else {
    // We are interested in object mark bits right before the slot.
    slot_mask = index_mask + (index_mask - 1);
  }

  MarkBit::CellType current_cell = cells[cell_index];
  CHECK(current_cell != 0);

  // Find the last live object in the cell.
  unsigned int leading_zeros =
      base::bits::CountLeadingZeros32(current_cell & slot_mask);
  CHECK(leading_zeros != Bitmap::kBitsPerCell);
  int offset = static_cast<int>(Bitmap::kBitIndexMask - leading_zeros) - 1;

  base_address += (cell_index - base_address_cell_index) *
                  Bitmap::kBitsPerCell * kPointerSize;
  Address address = base_address + offset * kPointerSize;

  // If the found mark bit is part of a black area, the slot cannot be part
  // of a live object since it is not marked.
  if (p->IsBlackAreaEndMarker(address + kPointerSize)) return false;

  HeapObject* object = HeapObject::FromAddress(address);
  CHECK(Marking::IsBlack(ObjectMarking::MarkBitFrom(object)));
  CHECK(object->address() < reinterpret_cast<Address>(slot));
  if ((object->address() + kPointerSize) <= slot &&
      (object->address() + object->Size()) > slot) {
    // If the slot is within the last found object in the cell, the slot is
    // in a live object.
    // Slots pointing to the first word of an object are invalid and removed.
    // This can happen when we move the object header while left trimming.
    return true;
  }
  return false;
}

HeapObject* MarkCompactCollector::FindBlackObjectBySlotSlow(Address slot) {
  Page* p = Page::FromAddress(slot);
  Space* owner = p->owner();
  if (owner == heap_->lo_space() || owner == nullptr) {
    Object* large_object = heap_->lo_space()->FindObject(slot);
    // This object has to exist, otherwise we would not have recorded a slot
    // for it.
    CHECK(large_object->IsHeapObject());
    HeapObject* large_heap_object = HeapObject::cast(large_object);

    if (IsMarked(large_heap_object)) {
      return large_heap_object;
    }
    return nullptr;
  }

  LiveObjectIterator<kBlackObjects> it(p);
  HeapObject* object = nullptr;
  while ((object = it.Next()) != nullptr) {
    int size = object->Size();
    if (object->address() > slot) return nullptr;
    if (object->address() <= slot && slot < (object->address() + size)) {
      return object;
    }
  }

  return nullptr;
}


void MarkCompactCollector::EvacuateNewSpacePrologue() {
  NewSpace* new_space = heap()->new_space();
  // Append the list of new space pages to be processed.
  for (Page* p : NewSpacePageRange(new_space->bottom(), new_space->top())) {
    newspace_evacuation_candidates_.Add(p);
  }
  new_space->Flip();
  new_space->ResetAllocationInfo();
}

class MarkCompactCollector::Evacuator : public Malloced {
 public:
  enum EvacuationMode {
    kObjectsNewToOld,
    kPageNewToOld,
    kObjectsOldToOld,
    kPageNewToNew,
  };

  static inline EvacuationMode ComputeEvacuationMode(MemoryChunk* chunk) {
    // Note: The order of checks is important in this function.
    if (chunk->IsFlagSet(MemoryChunk::PAGE_NEW_OLD_PROMOTION))
      return kPageNewToOld;
    if (chunk->IsFlagSet(MemoryChunk::PAGE_NEW_NEW_PROMOTION))
      return kPageNewToNew;
    if (chunk->InNewSpace()) return kObjectsNewToOld;
    DCHECK(chunk->IsEvacuationCandidate());
    return kObjectsOldToOld;
  }

  // NewSpacePages with more live bytes than this threshold qualify for fast
  // evacuation.
  static int PageEvacuationThreshold() {
    if (FLAG_page_promotion)
      return FLAG_page_promotion_threshold * Page::kAllocatableMemory / 100;
    return Page::kAllocatableMemory + kPointerSize;
  }

  explicit Evacuator(MarkCompactCollector* collector)
      : collector_(collector),
        compaction_spaces_(collector->heap()),
        local_pretenuring_feedback_(base::HashMap::PointersMatch,
                                    kInitialLocalPretenuringFeedbackCapacity),
        new_space_visitor_(collector->heap(), &compaction_spaces_,
                           &local_pretenuring_feedback_),
        new_space_page_visitor(collector->heap()),
        old_space_visitor_(collector->heap(), &compaction_spaces_),
        duration_(0.0),
        bytes_compacted_(0) {}

  inline bool EvacuatePage(Page* chunk);

  // Merge back locally cached info sequentially. Note that this method needs
  // to be called from the main thread.
  inline void Finalize();

  CompactionSpaceCollection* compaction_spaces() { return &compaction_spaces_; }

 private:
  static const int kInitialLocalPretenuringFeedbackCapacity = 256;

  inline Heap* heap() { return collector_->heap(); }

  void ReportCompactionProgress(double duration, intptr_t bytes_compacted) {
    duration_ += duration;
    bytes_compacted_ += bytes_compacted;
  }

  MarkCompactCollector* collector_;

  // Locally cached collector data.
  CompactionSpaceCollection compaction_spaces_;
  base::HashMap local_pretenuring_feedback_;

  // Visitors for the corresponding spaces.
  EvacuateNewSpaceVisitor new_space_visitor_;
  EvacuateNewSpacePageVisitor new_space_page_visitor;
  EvacuateOldSpaceVisitor old_space_visitor_;

  // Book keeping info.
  double duration_;
  intptr_t bytes_compacted_;
};

bool MarkCompactCollector::Evacuator::EvacuatePage(Page* page) {
  bool success = false;
  DCHECK(page->SweepingDone());
  int saved_live_bytes = page->LiveBytes();
  double evacuation_time = 0.0;
  Heap* heap = page->heap();
  {
    AlwaysAllocateScope always_allocate(heap->isolate());
    TimedScope timed_scope(&evacuation_time);
    switch (ComputeEvacuationMode(page)) {
      case kObjectsNewToOld:
        success = collector_->VisitLiveObjects(page, &new_space_visitor_,
                                               kClearMarkbits);
        ArrayBufferTracker::ProcessBuffers(
            page, ArrayBufferTracker::kUpdateForwardedRemoveOthers);
        DCHECK(success);
        break;
      case kPageNewToOld:
        success = collector_->VisitLiveObjects(page, &new_space_page_visitor,
                                               kKeepMarking);
        // ArrayBufferTracker will be updated during sweeping.
        DCHECK(success);
        break;
      case kPageNewToNew:
        new_space_page_visitor.account_semispace_copied(page->LiveBytes());
        // ArrayBufferTracker will be updated during sweeping.
        success = true;
        break;
      case kObjectsOldToOld:
        success = collector_->VisitLiveObjects(page, &old_space_visitor_,
                                               kClearMarkbits);
        if (!success) {
          // Aborted compaction page. We have to record slots here, since we
          // might not have recorded them in first place.
          // Note: We mark the page as aborted here to be able to record slots
          // for code objects in |RecordMigratedSlotVisitor|.
          page->SetFlag(Page::COMPACTION_WAS_ABORTED);
          EvacuateRecordOnlyVisitor record_visitor(collector_->heap());
          success =
              collector_->VisitLiveObjects(page, &record_visitor, kKeepMarking);
          ArrayBufferTracker::ProcessBuffers(
              page, ArrayBufferTracker::kUpdateForwardedKeepOthers);
          DCHECK(success);
          // We need to return failure here to indicate that we want this page
          // added to the sweeper.
          success = false;
        } else {
          ArrayBufferTracker::ProcessBuffers(
              page, ArrayBufferTracker::kUpdateForwardedRemoveOthers);
        }
        break;
      default:
        UNREACHABLE();
    }
  }
  ReportCompactionProgress(evacuation_time, saved_live_bytes);
  if (FLAG_trace_evacuation) {
    PrintIsolate(heap->isolate(),
                 "evacuation[%p]: page=%p new_space=%d "
                 "page_evacuation=%d executable=%d contains_age_mark=%d "
                 "live_bytes=%d time=%f\n",
                 static_cast<void*>(this), static_cast<void*>(page),
                 page->InNewSpace(),
                 page->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION) ||
                     page->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION),
                 page->IsFlagSet(MemoryChunk::IS_EXECUTABLE),
                 page->Contains(heap->new_space()->age_mark()),
                 saved_live_bytes, evacuation_time);
  }
  return success;
}

void MarkCompactCollector::Evacuator::Finalize() {
  heap()->old_space()->MergeCompactionSpace(compaction_spaces_.Get(OLD_SPACE));
  heap()->code_space()->MergeCompactionSpace(
      compaction_spaces_.Get(CODE_SPACE));
  heap()->tracer()->AddCompactionEvent(duration_, bytes_compacted_);
  heap()->IncrementPromotedObjectsSize(new_space_visitor_.promoted_size() +
                                       new_space_page_visitor.promoted_size());
  heap()->IncrementSemiSpaceCopiedObjectSize(
      new_space_visitor_.semispace_copied_size() +
      new_space_page_visitor.semispace_copied_size());
  heap()->IncrementYoungSurvivorsCounter(
      new_space_visitor_.promoted_size() +
      new_space_visitor_.semispace_copied_size() +
      new_space_page_visitor.promoted_size() +
      new_space_page_visitor.semispace_copied_size());
  heap()->MergeAllocationSitePretenuringFeedback(local_pretenuring_feedback_);
}

int MarkCompactCollector::NumberOfParallelCompactionTasks(int pages,
                                                          intptr_t live_bytes) {
  if (!FLAG_parallel_compaction) return 1;
  // Compute the number of needed tasks based on a target compaction time, the
  // profiled compaction speed and marked live memory.
  //
  // The number of parallel compaction tasks is limited by:
  // - #evacuation pages
  // - (#cores - 1)
  const double kTargetCompactionTimeInMs = 1;
  const int kNumSweepingTasks = 3;

  double compaction_speed =
      heap()->tracer()->CompactionSpeedInBytesPerMillisecond();

  const int available_cores = Max(
      1, static_cast<int>(
             V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads()) -
             kNumSweepingTasks - 1);
  int tasks;
  if (compaction_speed > 0) {
    tasks = 1 + static_cast<int>(live_bytes / compaction_speed /
                                 kTargetCompactionTimeInMs);
  } else {
    tasks = pages;
  }
  const int tasks_capped_pages = Min(pages, tasks);
  return Min(available_cores, tasks_capped_pages);
}

class EvacuationJobTraits {
 public:
  typedef int* PerPageData;  // Pointer to number of aborted pages.
  typedef MarkCompactCollector::Evacuator* PerTaskData;

  static const bool NeedSequentialFinalization = true;

  static bool ProcessPageInParallel(Heap* heap, PerTaskData evacuator,
                                    MemoryChunk* chunk, PerPageData) {
    return evacuator->EvacuatePage(reinterpret_cast<Page*>(chunk));
  }

  static void FinalizePageSequentially(Heap* heap, MemoryChunk* chunk,
                                       bool success, PerPageData data) {
    using Evacuator = MarkCompactCollector::Evacuator;
    Page* p = static_cast<Page*>(chunk);
    switch (Evacuator::ComputeEvacuationMode(p)) {
      case Evacuator::kPageNewToOld:
        break;
      case Evacuator::kPageNewToNew:
        DCHECK(success);
        break;
      case Evacuator::kObjectsNewToOld:
        DCHECK(success);
        break;
      case Evacuator::kObjectsOldToOld:
        if (success) {
          DCHECK(p->IsEvacuationCandidate());
          DCHECK(p->SweepingDone());
          p->Unlink();
        } else {
          // We have partially compacted the page, i.e., some objects may have
          // moved, others are still in place.
          p->ClearEvacuationCandidate();
          // Slots have already been recorded so we just need to add it to the
          // sweeper, which will happen after updating pointers.
          *data += 1;
        }
        break;
      default:
        UNREACHABLE();
    }
  }
};

void MarkCompactCollector::EvacuatePagesInParallel() {
  PageParallelJob<EvacuationJobTraits> job(
      heap_, heap_->isolate()->cancelable_task_manager(),
      &page_parallel_job_semaphore_);

  int abandoned_pages = 0;
  intptr_t live_bytes = 0;
  for (Page* page : evacuation_candidates_) {
    live_bytes += page->LiveBytes();
    job.AddPage(page, &abandoned_pages);
  }

  const Address age_mark = heap()->new_space()->age_mark();
  for (Page* page : newspace_evacuation_candidates_) {
    live_bytes += page->LiveBytes();
    if (!page->NeverEvacuate() &&
        (page->LiveBytes() > Evacuator::PageEvacuationThreshold()) &&
        !page->Contains(age_mark)) {
      if (page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK)) {
        EvacuateNewSpacePageVisitor::MoveToOldSpace(page, heap()->old_space());
      } else {
        EvacuateNewSpacePageVisitor::MoveToToSpace(page);
      }
    }

    job.AddPage(page, &abandoned_pages);
  }
  DCHECK_GE(job.NumberOfPages(), 1);

  // Used for trace summary.
  double compaction_speed = 0;
  if (FLAG_trace_evacuation) {
    compaction_speed = heap()->tracer()->CompactionSpeedInBytesPerMillisecond();
  }

  const int wanted_num_tasks =
      NumberOfParallelCompactionTasks(job.NumberOfPages(), live_bytes);
  Evacuator** evacuators = new Evacuator*[wanted_num_tasks];
  for (int i = 0; i < wanted_num_tasks; i++) {
    evacuators[i] = new Evacuator(this);
  }
  job.Run(wanted_num_tasks, [evacuators](int i) { return evacuators[i]; });
  for (int i = 0; i < wanted_num_tasks; i++) {
    evacuators[i]->Finalize();
    delete evacuators[i];
  }
  delete[] evacuators;

  if (FLAG_trace_evacuation) {
    PrintIsolate(isolate(),
                 "%8.0f ms: evacuation-summary: parallel=%s pages=%d "
                 "aborted=%d wanted_tasks=%d tasks=%d cores=%" PRIuS
                 " live_bytes=%" V8PRIdPTR " compaction_speed=%.f\n",
                 isolate()->time_millis_since_init(),
                 FLAG_parallel_compaction ? "yes" : "no", job.NumberOfPages(),
                 abandoned_pages, wanted_num_tasks, job.NumberOfTasks(),
                 V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads(),
                 live_bytes, compaction_speed);
  }
}

class EvacuationWeakObjectRetainer : public WeakObjectRetainer {
 public:
  virtual Object* RetainAs(Object* object) {
    if (object->IsHeapObject()) {
      HeapObject* heap_object = HeapObject::cast(object);
      MapWord map_word = heap_object->map_word();
      if (map_word.IsForwardingAddress()) {
        return map_word.ToForwardingAddress();
      }
    }
    return object;
  }
};

int MarkCompactCollector::Sweeper::RawSweep(
    Page* p, FreeListRebuildingMode free_list_mode,
    FreeSpaceTreatmentMode free_space_mode) {
  Space* space = p->owner();
  DCHECK_NOT_NULL(space);
  DCHECK(free_list_mode == IGNORE_FREE_LIST || space->identity() == OLD_SPACE ||
         space->identity() == CODE_SPACE || space->identity() == MAP_SPACE);
  DCHECK(!p->IsEvacuationCandidate() && !p->SweepingDone());

  // Before we sweep objects on the page, we free dead array buffers which
  // requires valid mark bits.
  ArrayBufferTracker::FreeDead(p);

  // We also release the black area markers here.
  p->ReleaseBlackAreaEndMarkerMap();

  Address free_start = p->area_start();
  DCHECK(reinterpret_cast<intptr_t>(free_start) % (32 * kPointerSize) == 0);

  // If we use the skip list for code space pages, we have to lock the skip
  // list because it could be accessed concurrently by the runtime or the
  // deoptimizer.
  const bool rebuild_skip_list =
      space->identity() == CODE_SPACE && p->skip_list() != nullptr;
  SkipList* skip_list = p->skip_list();
  if (rebuild_skip_list) {
    skip_list->Clear();
  }

  intptr_t freed_bytes = 0;
  intptr_t max_freed_bytes = 0;
  int curr_region = -1;

  LiveObjectIterator<kBlackObjects> it(p);
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    DCHECK(Marking::IsBlack(ObjectMarking::MarkBitFrom(object)));
    Address free_end = object->address();
    if (free_end != free_start) {
      int size = static_cast<int>(free_end - free_start);
      if (free_space_mode == ZAP_FREE_SPACE) {
        memset(free_start, 0xcc, size);
      }
      if (free_list_mode == REBUILD_FREE_LIST) {
        freed_bytes = reinterpret_cast<PagedSpace*>(space)->UnaccountedFree(
            free_start, size);
        max_freed_bytes = Max(freed_bytes, max_freed_bytes);
      } else {
        p->heap()->CreateFillerObjectAt(free_start, size,
                                        ClearRecordedSlots::kNo);
      }
    }
    Map* map = object->synchronized_map();
    int size = object->SizeFromMap(map);
    if (rebuild_skip_list) {
      int new_region_start = SkipList::RegionNumber(free_end);
      int new_region_end =
          SkipList::RegionNumber(free_end + size - kPointerSize);
      if (new_region_start != curr_region || new_region_end != curr_region) {
        skip_list->AddObject(free_end, size);
        curr_region = new_region_end;
      }
    }
    free_start = free_end + size;
  }

  // Clear the mark bits of that page and reset live bytes count.
  p->ClearLiveness();

  if (free_start != p->area_end()) {
    int size = static_cast<int>(p->area_end() - free_start);
    if (free_space_mode == ZAP_FREE_SPACE) {
      memset(free_start, 0xcc, size);
    }
    if (free_list_mode == REBUILD_FREE_LIST) {
      freed_bytes = reinterpret_cast<PagedSpace*>(space)->UnaccountedFree(
          free_start, size);
      max_freed_bytes = Max(freed_bytes, max_freed_bytes);
    } else {
      p->heap()->CreateFillerObjectAt(free_start, size,
                                      ClearRecordedSlots::kNo);
    }
  }
  p->concurrent_sweeping_state().SetValue(Page::kSweepingDone);
  if (free_list_mode == IGNORE_FREE_LIST) return 0;
  return FreeList::GuaranteedAllocatable(static_cast<int>(max_freed_bytes));
}

void MarkCompactCollector::InvalidateCode(Code* code) {
  Page* page = Page::FromAddress(code->address());
  Address start = code->instruction_start();
  Address end = code->address() + code->Size();

  RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(page, start, end);

  if (heap_->incremental_marking()->IsCompacting() &&
      !ShouldSkipEvacuationSlotRecording(code)) {
    DCHECK(compacting_);

    // If the object is white than no slots were recorded on it yet.
    MarkBit mark_bit = ObjectMarking::MarkBitFrom(code);
    if (Marking::IsWhite(mark_bit)) return;

    // Ignore all slots that might have been recorded in the body of the
    // deoptimized code object. Assumption: no slots will be recorded for
    // this object after invalidating it.
    RememberedSet<OLD_TO_OLD>::RemoveRangeTyped(page, start, end);
  }
}


// Return true if the given code is deoptimized or will be deoptimized.
bool MarkCompactCollector::WillBeDeoptimized(Code* code) {
  return code->is_optimized_code() && code->marked_for_deoptimization();
}


#ifdef VERIFY_HEAP
static void VerifyAllBlackObjects(MemoryChunk* page) {
  LiveObjectIterator<kAllLiveObjects> it(page);
  HeapObject* object = NULL;
  while ((object = it.Next()) != NULL) {
    CHECK(Marking::IsBlack(ObjectMarking::MarkBitFrom(object)));
  }
}
#endif  // VERIFY_HEAP

template <class Visitor>
bool MarkCompactCollector::VisitLiveObjects(MemoryChunk* page, Visitor* visitor,
                                            IterationMode mode) {
#ifdef VERIFY_HEAP
  VerifyAllBlackObjects(page);
#endif  // VERIFY_HEAP

  LiveObjectIterator<kBlackObjects> it(page);
  HeapObject* object = nullptr;
  while ((object = it.Next()) != nullptr) {
    DCHECK(Marking::IsBlack(ObjectMarking::MarkBitFrom(object)));
    if (!visitor->Visit(object)) {
      if (mode == kClearMarkbits) {
        page->markbits()->ClearRange(
            page->AddressToMarkbitIndex(page->area_start()),
            page->AddressToMarkbitIndex(object->address()));
        if (page->old_to_new_slots() != nullptr) {
          page->old_to_new_slots()->RemoveRange(
              0, static_cast<int>(object->address() - page->address()));
        }
        if (page->typed_old_to_new_slots() != nullptr) {
          RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(page, page->address(),
                                                      object->address());
        }
        RecomputeLiveBytes(page);
      }
      return false;
    }
  }
  if (mode == kClearMarkbits) {
    page->ClearLiveness();
  }
  return true;
}


void MarkCompactCollector::RecomputeLiveBytes(MemoryChunk* page) {
  LiveObjectIterator<kBlackObjects> it(page);
  int new_live_size = 0;
  HeapObject* object = nullptr;
  while ((object = it.Next()) != nullptr) {
    new_live_size += object->Size();
  }
  page->SetLiveBytes(new_live_size);
}

void MarkCompactCollector::Sweeper::AddSweptPageSafe(PagedSpace* space,
                                                     Page* page) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  swept_list_[space->identity()].Add(page);
}

void MarkCompactCollector::EvacuateNewSpaceAndCandidates() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE);
  Heap::RelocationLock relocation_lock(heap());

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_COPY);
    EvacuationScope evacuation_scope(this);

    EvacuateNewSpacePrologue();
    EvacuatePagesInParallel();
    heap()->new_space()->set_age_mark(heap()->new_space()->top());
  }

  UpdatePointersAfterEvacuation();

  if (!heap()->new_space()->Rebalance()) {
    FatalProcessOutOfMemory("NewSpace::Rebalance");
  }

  // Give pages that are queued to be freed back to the OS. Note that filtering
  // slots only handles old space (for unboxed doubles), and thus map space can
  // still contain stale pointers. We only free the chunks after pointer updates
  // to still have access to page headers.
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_CLEAN_UP);

    for (Page* p : newspace_evacuation_candidates_) {
      if (p->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) {
        p->ClearFlag(Page::PAGE_NEW_NEW_PROMOTION);
        sweeper().AddLatePage(p->owner()->identity(), p);
      } else if (p->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)) {
        p->ClearFlag(Page::PAGE_NEW_OLD_PROMOTION);
        p->ForAllFreeListCategories(
            [](FreeListCategory* category) { DCHECK(!category->is_linked()); });
        sweeper().AddLatePage(p->owner()->identity(), p);
      }
    }
    newspace_evacuation_candidates_.Rewind(0);

    for (Page* p : evacuation_candidates_) {
      // Important: skip list should be cleared only after roots were updated
      // because root iteration traverses the stack and might have to find
      // code objects from non-updated pc pointing into evacuation candidate.
      SkipList* list = p->skip_list();
      if (list != NULL) list->Clear();
      if (p->IsFlagSet(Page::COMPACTION_WAS_ABORTED)) {
        sweeper().AddLatePage(p->owner()->identity(), p);
        p->ClearFlag(Page::COMPACTION_WAS_ABORTED);
      }
    }

    // Deallocate evacuated candidate pages.
    ReleaseEvacuationCandidates();
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && !sweeper().sweeping_in_progress()) {
    VerifyEvacuation(heap());
  }
#endif
}

template <PointerDirection direction>
class PointerUpdateJobTraits {
 public:
  typedef int PerPageData;  // Per page data is not used in this job.
  typedef int PerTaskData;  // Per task data is not used in this job.

  static bool ProcessPageInParallel(Heap* heap, PerTaskData, MemoryChunk* chunk,
                                    PerPageData) {
    UpdateUntypedPointers(heap, chunk);
    UpdateTypedPointers(heap, chunk);
    return true;
  }
  static const bool NeedSequentialFinalization = false;
  static void FinalizePageSequentially(Heap*, MemoryChunk*, bool, PerPageData) {
  }

 private:
  static void UpdateUntypedPointers(Heap* heap, MemoryChunk* chunk) {
    if (direction == OLD_TO_NEW) {
      RememberedSet<OLD_TO_NEW>::Iterate(chunk, [heap, chunk](Address slot) {
        return CheckAndUpdateOldToNewSlot(heap, slot);
      });
    } else {
      RememberedSet<OLD_TO_OLD>::Iterate(chunk, [](Address slot) {
        return UpdateSlot(reinterpret_cast<Object**>(slot));
      });
    }
  }

  static void UpdateTypedPointers(Heap* heap, MemoryChunk* chunk) {
    if (direction == OLD_TO_OLD) {
      Isolate* isolate = heap->isolate();
      RememberedSet<OLD_TO_OLD>::IterateTyped(
          chunk, [isolate](SlotType type, Address host_addr, Address slot) {
            return UpdateTypedSlotHelper::UpdateTypedSlot(isolate, type, slot,
                                                          UpdateSlot);
          });
    } else {
      Isolate* isolate = heap->isolate();
      RememberedSet<OLD_TO_NEW>::IterateTyped(
          chunk,
          [isolate, heap](SlotType type, Address host_addr, Address slot) {
            return UpdateTypedSlotHelper::UpdateTypedSlot(
                isolate, type, slot, [heap](Object** slot) {
                  return CheckAndUpdateOldToNewSlot(
                      heap, reinterpret_cast<Address>(slot));
                });
          });
    }
  }

  static SlotCallbackResult CheckAndUpdateOldToNewSlot(Heap* heap,
                                                       Address slot_address) {
    Object** slot = reinterpret_cast<Object**>(slot_address);
    if (heap->InFromSpace(*slot)) {
      HeapObject* heap_object = reinterpret_cast<HeapObject*>(*slot);
      DCHECK(heap_object->IsHeapObject());
      MapWord map_word = heap_object->map_word();
      // There could still be stale pointers in large object space, map space,
      // and old space for pages that have been promoted.
      if (map_word.IsForwardingAddress()) {
        // Update the corresponding slot.
        *slot = map_word.ToForwardingAddress();
      }
      // If the object was in from space before and is after executing the
      // callback in to space, the object is still live.
      // Unfortunately, we do not know about the slot. It could be in a
      // just freed free space object.
      if (heap->InToSpace(*slot)) {
        return KEEP_SLOT;
      }
    } else if (heap->InToSpace(*slot)) {
      // Slots can point to "to" space if the page has been moved, or if the
      // slot has been recorded multiple times in the remembered set. Since
      // there is no forwarding information present we need to check the
      // markbits to determine liveness.
      if (Marking::IsBlack(
              ObjectMarking::MarkBitFrom(reinterpret_cast<HeapObject*>(*slot))))
        return KEEP_SLOT;
    } else {
      DCHECK(!heap->InNewSpace(*slot));
    }
    return REMOVE_SLOT;
  }
};

int NumberOfPointerUpdateTasks(int pages) {
  if (!FLAG_parallel_pointer_update) return 1;
  const int kMaxTasks = 4;
  const int kPagesPerTask = 4;
  return Min(kMaxTasks, (pages + kPagesPerTask - 1) / kPagesPerTask);
}

template <PointerDirection direction>
void UpdatePointersInParallel(Heap* heap, base::Semaphore* semaphore) {
  // Work-around bug in clang-3.4
  // https://github.com/nodejs/node/issues/8323
  struct MemoryChunkVisitor {
    PageParallelJob<PointerUpdateJobTraits<direction> >& job_;
    MemoryChunkVisitor(PageParallelJob<PointerUpdateJobTraits<direction> >& job)
      : job_(job) {}
    void operator()(MemoryChunk* chunk) {
      job_.AddPage(chunk, 0);
    }
  };

  PageParallelJob<PointerUpdateJobTraits<direction> > job(
      heap, heap->isolate()->cancelable_task_manager(), semaphore);
  RememberedSet<direction>::IterateMemoryChunks(heap, MemoryChunkVisitor(job));
  int num_pages = job.NumberOfPages();
  int num_tasks = NumberOfPointerUpdateTasks(num_pages);
  job.Run(num_tasks, [](int i) { return 0; });
}

class ToSpacePointerUpdateJobTraits {
 public:
  typedef std::pair<Address, Address> PerPageData;
  typedef PointersUpdatingVisitor* PerTaskData;

  static bool ProcessPageInParallel(Heap* heap, PerTaskData visitor,
                                    MemoryChunk* chunk, PerPageData limits) {
    if (chunk->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION)) {
      // New->new promoted pages contain garbage so they require iteration
      // using markbits.
      ProcessPageInParallelVisitLive(heap, visitor, chunk, limits);
    } else {
      ProcessPageInParallelVisitAll(heap, visitor, chunk, limits);
    }
    return true;
  }

  static const bool NeedSequentialFinalization = false;
  static void FinalizePageSequentially(Heap*, MemoryChunk*, bool, PerPageData) {
  }

 private:
  static void ProcessPageInParallelVisitAll(Heap* heap, PerTaskData visitor,
                                            MemoryChunk* chunk,
                                            PerPageData limits) {
    for (Address cur = limits.first; cur < limits.second;) {
      HeapObject* object = HeapObject::FromAddress(cur);
      Map* map = object->map();
      int size = object->SizeFromMap(map);
      object->IterateBody(map->instance_type(), size, visitor);
      cur += size;
    }
  }

  static void ProcessPageInParallelVisitLive(Heap* heap, PerTaskData visitor,
                                             MemoryChunk* chunk,
                                             PerPageData limits) {
    LiveObjectIterator<kBlackObjects> it(chunk);
    HeapObject* object = NULL;
    while ((object = it.Next()) != NULL) {
      Map* map = object->map();
      int size = object->SizeFromMap(map);
      object->IterateBody(map->instance_type(), size, visitor);
    }
  }
};

void UpdateToSpacePointersInParallel(Heap* heap, base::Semaphore* semaphore) {
  PageParallelJob<ToSpacePointerUpdateJobTraits> job(
      heap, heap->isolate()->cancelable_task_manager(), semaphore);
  Address space_start = heap->new_space()->bottom();
  Address space_end = heap->new_space()->top();
  for (Page* page : NewSpacePageRange(space_start, space_end)) {
    Address start =
        page->Contains(space_start) ? space_start : page->area_start();
    Address end = page->Contains(space_end) ? space_end : page->area_end();
    job.AddPage(page, std::make_pair(start, end));
  }
  PointersUpdatingVisitor visitor;
  int num_tasks = FLAG_parallel_pointer_update ? job.NumberOfPages() : 1;
  job.Run(num_tasks, [&visitor](int i) { return &visitor; });
}

void MarkCompactCollector::UpdatePointersAfterEvacuation() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS);

  PointersUpdatingVisitor updating_visitor;

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_TO_NEW);
    UpdateToSpacePointersInParallel(heap_, &page_parallel_job_semaphore_);
    // Update roots.
    heap_->IterateRoots(&updating_visitor, VISIT_ALL_IN_SWEEP_NEWSPACE);
    UpdatePointersInParallel<OLD_TO_NEW>(heap_, &page_parallel_job_semaphore_);
  }

  {
    Heap* heap = this->heap();
    TRACE_GC(heap->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_TO_EVACUATED);
    UpdatePointersInParallel<OLD_TO_OLD>(heap_, &page_parallel_job_semaphore_);
  }

  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_EVACUATE_UPDATE_POINTERS_WEAK);
    // Update pointers from external string table.
    heap_->UpdateReferencesInExternalStringTable(
        &UpdateReferenceInExternalStringTableEntry);

    EvacuationWeakObjectRetainer evacuation_object_retainer;
    heap()->ProcessWeakListRoots(&evacuation_object_retainer);
  }
}


void MarkCompactCollector::ReleaseEvacuationCandidates() {
  for (Page* p : evacuation_candidates_) {
    if (!p->IsEvacuationCandidate()) continue;
    PagedSpace* space = static_cast<PagedSpace*>(p->owner());
    p->ResetLiveBytes();
    CHECK(p->SweepingDone());
    space->ReleasePage(p);
  }
  evacuation_candidates_.Rewind(0);
  compacting_ = false;
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
}

int MarkCompactCollector::Sweeper::ParallelSweepSpace(AllocationSpace identity,
                                                      int required_freed_bytes,
                                                      int max_pages) {
  int max_freed = 0;
  int pages_freed = 0;
  Page* page = nullptr;
  while ((page = GetSweepingPageSafe(identity)) != nullptr) {
    int freed = ParallelSweepPage(page, identity);
    pages_freed += 1;
    DCHECK_GE(freed, 0);
    max_freed = Max(max_freed, freed);
    if ((required_freed_bytes) > 0 && (max_freed >= required_freed_bytes))
      return max_freed;
    if ((max_pages > 0) && (pages_freed >= max_pages)) return max_freed;
  }
  return max_freed;
}

int MarkCompactCollector::Sweeper::ParallelSweepPage(Page* page,
                                                     AllocationSpace identity) {
  int max_freed = 0;
  if (page->mutex()->TryLock()) {
    // If this page was already swept in the meantime, we can return here.
    if (page->concurrent_sweeping_state().Value() != Page::kSweepingPending) {
      page->mutex()->Unlock();
      return 0;
    }
    page->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
    const Sweeper::FreeSpaceTreatmentMode free_space_mode =
        Heap::ShouldZapGarbage() ? ZAP_FREE_SPACE : IGNORE_FREE_SPACE;
    if (identity == NEW_SPACE) {
      RawSweep(page, IGNORE_FREE_LIST, free_space_mode);
    } else if (identity == OLD_SPACE) {
      max_freed = RawSweep(page, REBUILD_FREE_LIST, free_space_mode);
    } else if (identity == CODE_SPACE) {
      max_freed = RawSweep(page, REBUILD_FREE_LIST, free_space_mode);
    } else {
      max_freed = RawSweep(page, REBUILD_FREE_LIST, free_space_mode);
    }
    {
      base::LockGuard<base::Mutex> guard(&mutex_);
      swept_list_[identity].Add(page);
    }
    page->concurrent_sweeping_state().SetValue(Page::kSweepingDone);
    page->mutex()->Unlock();
  }
  return max_freed;
}

void MarkCompactCollector::Sweeper::AddPage(AllocationSpace space, Page* page) {
  DCHECK(!sweeping_in_progress_);
  PrepareToBeSweptPage(space, page);
  sweeping_list_[space].push_back(page);
}

void MarkCompactCollector::Sweeper::AddLatePage(AllocationSpace space,
                                                Page* page) {
  DCHECK(sweeping_in_progress_);
  PrepareToBeSweptPage(space, page);
  late_pages_ = true;
  AddSweepingPageSafe(space, page);
}

void MarkCompactCollector::Sweeper::PrepareToBeSweptPage(AllocationSpace space,
                                                         Page* page) {
  page->concurrent_sweeping_state().SetValue(Page::kSweepingPending);
  int to_sweep = page->area_size() - page->LiveBytes();
  if (space != NEW_SPACE)
    heap_->paged_space(space)->accounting_stats_.ShrinkSpace(to_sweep);
}

Page* MarkCompactCollector::Sweeper::GetSweepingPageSafe(
    AllocationSpace space) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  Page* page = nullptr;
  if (!sweeping_list_[space].empty()) {
    page = sweeping_list_[space].front();
    sweeping_list_[space].pop_front();
  }
  return page;
}

void MarkCompactCollector::Sweeper::AddSweepingPageSafe(AllocationSpace space,
                                                        Page* page) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  sweeping_list_[space].push_back(page);
}

void MarkCompactCollector::StartSweepSpace(PagedSpace* space) {
  space->ClearStats();

  int will_be_swept = 0;
  bool unused_page_present = false;

  // Loop needs to support deletion if live bytes == 0 for a page.
  for (auto it = space->begin(); it != space->end();) {
    Page* p = *(it++);
    DCHECK(p->SweepingDone());

    if (p->IsEvacuationCandidate()) {
      // Will be processed in EvacuateNewSpaceAndCandidates.
      DCHECK(evacuation_candidates_.length() > 0);
      DCHECK(!p->HasBlackAreas());
      continue;
    }

    if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      // We need to sweep the page to get it into an iterable state again. Note
      // that this adds unusable memory into the free list that is later on
      // (in the free list) dropped again. Since we only use the flag for
      // testing this is fine.
      p->concurrent_sweeping_state().SetValue(Page::kSweepingInProgress);
      Sweeper::RawSweep(p, Sweeper::IGNORE_FREE_LIST,
                        Heap::ShouldZapGarbage() ? Sweeper::ZAP_FREE_SPACE
                                                 : Sweeper::IGNORE_FREE_SPACE);
      continue;
    }

    // One unused page is kept, all further are released before sweeping them.
    if (p->LiveBytes() == 0) {
      if (unused_page_present) {
        if (FLAG_gc_verbose) {
          PrintIsolate(isolate(), "sweeping: released page: %p",
                       static_cast<void*>(p));
        }
        ArrayBufferTracker::FreeAll(p);
        space->ReleasePage(p);
        continue;
      }
      unused_page_present = true;
    }

    sweeper().AddPage(space->identity(), p);
    will_be_swept++;
  }

  if (FLAG_gc_verbose) {
    PrintIsolate(isolate(), "sweeping: space=%s initialized_for_sweeping=%d",
                 AllocationSpaceName(space->identity()), will_be_swept);
  }
}


void MarkCompactCollector::SweepSpaces() {
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_SWEEP);
  double start_time = 0.0;
  if (FLAG_print_cumulative_gc_stat) {
    start_time = heap_->MonotonicallyIncreasingTimeInMs();
  }

#ifdef DEBUG
  state_ = SWEEP_SPACES;
#endif

  {
    {
      GCTracer::Scope sweep_scope(heap()->tracer(),
                                  GCTracer::Scope::MC_SWEEP_OLD);
      StartSweepSpace(heap()->old_space());
    }
    {
      GCTracer::Scope sweep_scope(heap()->tracer(),
                                  GCTracer::Scope::MC_SWEEP_CODE);
      StartSweepSpace(heap()->code_space());
    }
    {
      GCTracer::Scope sweep_scope(heap()->tracer(),
                                  GCTracer::Scope::MC_SWEEP_MAP);
      StartSweepSpace(heap()->map_space());
    }
    sweeper().StartSweeping();
  }

  // Deallocate unmarked large objects.
  heap_->lo_space()->FreeUnmarkedObjects();

  if (FLAG_print_cumulative_gc_stat) {
    heap_->tracer()->AddSweepingTime(heap_->MonotonicallyIncreasingTimeInMs() -
                                     start_time);
  }
}

Isolate* MarkCompactCollector::isolate() const { return heap_->isolate(); }


void MarkCompactCollector::Initialize() {
  MarkCompactMarkingVisitor::Initialize();
  IncrementalMarking::Initialize();
}

void MarkCompactCollector::RecordCodeEntrySlot(HeapObject* host, Address slot,
                                               Code* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(host));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(host)) {
    // TODO(ulan): remove this check after investigating crbug.com/414964.
    CHECK(target->IsCode());
    RememberedSet<OLD_TO_OLD>::InsertTyped(
        source_page, reinterpret_cast<Address>(host), CODE_ENTRY_SLOT, slot);
  }
}


void MarkCompactCollector::RecordCodeTargetPatch(Address pc, Code* target) {
  DCHECK(heap()->gc_state() == Heap::MARK_COMPACT);
  if (is_compacting()) {
    Code* host =
        isolate()->inner_pointer_to_code_cache()->GcSafeFindCodeForInnerPointer(
            pc);
    MarkBit mark_bit = ObjectMarking::MarkBitFrom(host);
    if (Marking::IsBlack(mark_bit)) {
      RelocInfo rinfo(isolate(), pc, RelocInfo::CODE_TARGET, 0, host);
      // The target is always in old space, we don't have to record the slot in
      // the old-to-new remembered set.
      DCHECK(!heap()->InNewSpace(target));
      RecordRelocSlot(host, &rinfo, target);
    }
  }
}

}  // namespace internal
}  // namespace v8
