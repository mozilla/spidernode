/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Statistics.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "jsprf.h"
#include "jsutil.h"

#include "gc/Memory.h"
#include "vm/Debugger.h"
#include "vm/HelperThreads.h"
#include "vm/Runtime.h"
#include "vm/Time.h"

using namespace js;
using namespace js::gc;
using namespace js::gcstats;

using mozilla::DebugOnly;
using mozilla::IntegerRange;
using mozilla::PodArrayZero;
using mozilla::PodZero;
using mozilla::TimeStamp;
using mozilla::TimeDuration;

/*
 * If this fails, then you can either delete this assertion and allow all
 * larger-numbered reasons to pile up in the last telemetry bucket, or switch
 * to GC_REASON_3 and bump the max value.
 */
JS_STATIC_ASSERT(JS::gcreason::NUM_TELEMETRY_REASONS >= JS::gcreason::NUM_REASONS);

static inline decltype(mozilla::MakeEnumeratedRange(PHASE_FIRST, PHASE_LIMIT))
AllPhases()
{
    return mozilla::MakeEnumeratedRange(PHASE_FIRST, PHASE_LIMIT);
}

const char*
js::gcstats::ExplainInvocationKind(JSGCInvocationKind gckind)
{
    MOZ_ASSERT(gckind == GC_NORMAL || gckind == GC_SHRINK);
    if (gckind == GC_NORMAL)
         return "Normal";
    else
         return "Shrinking";
}

JS_PUBLIC_API(const char*)
JS::gcreason::ExplainReason(JS::gcreason::Reason reason)
{
    switch (reason) {
#define SWITCH_REASON(name)                         \
        case JS::gcreason::name:                    \
          return #name;
        GCREASONS(SWITCH_REASON)

        default:
          MOZ_CRASH("bad GC reason");
#undef SWITCH_REASON
    }
}

const char*
js::gcstats::ExplainAbortReason(gc::AbortReason reason)
{
    switch (reason) {
#define SWITCH_REASON(name)                         \
        case gc::AbortReason::name:                 \
          return #name;
        GC_ABORT_REASONS(SWITCH_REASON)

        default:
          MOZ_CRASH("bad GC abort reason");
#undef SWITCH_REASON
    }
}

static double
t(TimeDuration duration)
{
    return duration.ToMilliseconds();
}

struct PhaseInfo
{
    Phase index;
    const char* name;
    Phase parent;
    uint8_t telemetryBucket;
};

// The zeroth entry in the timing arrays is used for phases that have a
// unique lineage.
static const size_t PHASE_DAG_NONE = 0;

// These are really just fields of PhaseInfo, but I have to initialize them
// programmatically, which prevents making phases[] const. (And marking these
// fields mutable does not work on Windows; the whole thing gets created in
// read-only memory anyway.)
struct ExtraPhaseInfo
{
    // Depth in the tree of each phase type
    size_t depth;

    // Index into the set of parallel arrays of timing data, for parents with
    // at least one multi-parented child
    size_t dagSlot;

    ExtraPhaseInfo() : depth(0), dagSlot(0) {}
};

static const Phase PHASE_NO_PARENT = PHASE_LIMIT;

struct DagChildEdge {
    Phase parent;
    Phase child;
} dagChildEdges[] = {
    { PHASE_MARK, PHASE_MARK_ROOTS },
    { PHASE_MINOR_GC, PHASE_MARK_ROOTS },
    { PHASE_TRACE_HEAP, PHASE_MARK_ROOTS },
    { PHASE_EVICT_NURSERY, PHASE_MARK_ROOTS },
    { PHASE_COMPACT_UPDATE, PHASE_MARK_ROOTS }
};

/*
 * Note that PHASE_MUTATOR never has any child phases. If beginPhase is called
 * while PHASE_MUTATOR is active, it will automatically be suspended and
 * resumed when the phase stack is next empty. Timings for these phases are
 * thus exclusive of any other phase.
 */

static const PhaseInfo phases[] = {
    { PHASE_MUTATOR, "Mutator Running", PHASE_NO_PARENT, 0 },
    { PHASE_GC_BEGIN, "Begin Callback", PHASE_NO_PARENT, 1 },
    { PHASE_WAIT_BACKGROUND_THREAD, "Wait Background Thread", PHASE_NO_PARENT, 2 },
    { PHASE_MARK_DISCARD_CODE, "Mark Discard Code", PHASE_NO_PARENT, 3 },
    { PHASE_RELAZIFY_FUNCTIONS, "Relazify Functions", PHASE_NO_PARENT, 4 },
    { PHASE_PURGE, "Purge", PHASE_NO_PARENT, 5 },
    { PHASE_MARK, "Mark", PHASE_NO_PARENT, 6 },
        { PHASE_UNMARK, "Unmark", PHASE_MARK, 7 },
        /* PHASE_MARK_ROOTS */
        { PHASE_MARK_DELAYED, "Mark Delayed", PHASE_MARK, 8 },
    { PHASE_SWEEP, "Sweep", PHASE_NO_PARENT, 9 },
        { PHASE_SWEEP_MARK, "Mark During Sweeping", PHASE_SWEEP, 10 },
            { PHASE_SWEEP_MARK_TYPES, "Mark Types During Sweeping", PHASE_SWEEP_MARK, 11 },
            { PHASE_SWEEP_MARK_INCOMING_BLACK, "Mark Incoming Black Pointers", PHASE_SWEEP_MARK, 12 },
            { PHASE_SWEEP_MARK_WEAK, "Mark Weak", PHASE_SWEEP_MARK, 13 },
            { PHASE_SWEEP_MARK_INCOMING_GRAY, "Mark Incoming Gray Pointers", PHASE_SWEEP_MARK, 14 },
            { PHASE_SWEEP_MARK_GRAY, "Mark Gray", PHASE_SWEEP_MARK, 15 },
            { PHASE_SWEEP_MARK_GRAY_WEAK, "Mark Gray and Weak", PHASE_SWEEP_MARK, 16 },
        { PHASE_FINALIZE_START, "Finalize Start Callbacks", PHASE_SWEEP, 17 },
            { PHASE_WEAK_ZONES_CALLBACK, "Per-Slice Weak Callback", PHASE_FINALIZE_START, 57 },
            { PHASE_WEAK_COMPARTMENT_CALLBACK, "Per-Compartment Weak Callback", PHASE_FINALIZE_START, 58 },
        { PHASE_SWEEP_ATOMS, "Sweep Atoms", PHASE_SWEEP, 18 },
        { PHASE_SWEEP_COMPARTMENTS, "Sweep Compartments", PHASE_SWEEP, 20 },
            { PHASE_SWEEP_DISCARD_CODE, "Sweep Discard Code", PHASE_SWEEP_COMPARTMENTS, 21 },
            { PHASE_SWEEP_INNER_VIEWS, "Sweep Inner Views", PHASE_SWEEP_COMPARTMENTS, 22 },
            { PHASE_SWEEP_CC_WRAPPER, "Sweep Cross Compartment Wrappers", PHASE_SWEEP_COMPARTMENTS, 23 },
            { PHASE_SWEEP_BASE_SHAPE, "Sweep Base Shapes", PHASE_SWEEP_COMPARTMENTS, 24 },
            { PHASE_SWEEP_INITIAL_SHAPE, "Sweep Initial Shapes", PHASE_SWEEP_COMPARTMENTS, 25 },
            { PHASE_SWEEP_TYPE_OBJECT, "Sweep Type Objects", PHASE_SWEEP_COMPARTMENTS, 26 },
            { PHASE_SWEEP_BREAKPOINT, "Sweep Breakpoints", PHASE_SWEEP_COMPARTMENTS, 27 },
            { PHASE_SWEEP_REGEXP, "Sweep Regexps", PHASE_SWEEP_COMPARTMENTS, 28 },
            { PHASE_SWEEP_COMPRESSION, "Sweep Compression Tasks", PHASE_SWEEP_COMPARTMENTS, 62 },
            { PHASE_SWEEP_WEAKMAPS, "Sweep WeakMaps", PHASE_SWEEP_COMPARTMENTS, 63 },
            { PHASE_SWEEP_UNIQUEIDS, "Sweep Unique IDs", PHASE_SWEEP_COMPARTMENTS, 64 },
            { PHASE_SWEEP_JIT_DATA, "Sweep JIT Data", PHASE_SWEEP_COMPARTMENTS, 65 },
            { PHASE_SWEEP_WEAK_CACHES, "Sweep Weak Caches", PHASE_SWEEP_COMPARTMENTS, 66 },
            { PHASE_SWEEP_MISC, "Sweep Miscellaneous", PHASE_SWEEP_COMPARTMENTS, 29 },
            { PHASE_SWEEP_TYPES, "Sweep type information", PHASE_SWEEP_COMPARTMENTS, 30 },
                { PHASE_SWEEP_TYPES_BEGIN, "Sweep type tables and compilations", PHASE_SWEEP_TYPES, 31 },
                { PHASE_SWEEP_TYPES_END, "Free type arena", PHASE_SWEEP_TYPES, 32 },
        { PHASE_SWEEP_OBJECT, "Sweep Object", PHASE_SWEEP, 33 },
        { PHASE_SWEEP_STRING, "Sweep String", PHASE_SWEEP, 34 },
        { PHASE_SWEEP_SCRIPT, "Sweep Script", PHASE_SWEEP, 35 },
        { PHASE_SWEEP_SCOPE, "Sweep Scope", PHASE_SWEEP, 59 },
        { PHASE_SWEEP_REGEXP_SHARED, "Sweep RegExpShared", PHASE_SWEEP, 61 },
        { PHASE_SWEEP_SHAPE, "Sweep Shape", PHASE_SWEEP, 36 },
        { PHASE_SWEEP_JITCODE, "Sweep JIT code", PHASE_SWEEP, 37 },
        { PHASE_FINALIZE_END, "Finalize End Callback", PHASE_SWEEP, 38 },
        { PHASE_DESTROY, "Deallocate", PHASE_SWEEP, 39 },
    { PHASE_COMPACT, "Compact", PHASE_NO_PARENT, 40 },
        { PHASE_COMPACT_MOVE, "Compact Move", PHASE_COMPACT, 41 },
        { PHASE_COMPACT_UPDATE, "Compact Update", PHASE_COMPACT, 42 },
            /* PHASE_MARK_ROOTS */
            { PHASE_COMPACT_UPDATE_CELLS, "Compact Update Cells", PHASE_COMPACT_UPDATE, 43 },
    { PHASE_GC_END, "End Callback", PHASE_NO_PARENT, 44 },
    { PHASE_MINOR_GC, "All Minor GCs", PHASE_NO_PARENT, 45 },
        /* PHASE_MARK_ROOTS */
    { PHASE_EVICT_NURSERY, "Minor GCs to Evict Nursery", PHASE_NO_PARENT, 46 },
        /* PHASE_MARK_ROOTS */
    { PHASE_TRACE_HEAP, "Trace Heap", PHASE_NO_PARENT, 47 },
        /* PHASE_MARK_ROOTS */
    { PHASE_BARRIER, "Barriers", PHASE_NO_PARENT, 55 },
        { PHASE_UNMARK_GRAY, "Unmark gray", PHASE_BARRIER, 56 },
    { PHASE_MARK_ROOTS, "Mark Roots", PHASE_MULTI_PARENTS, 48 },
        { PHASE_BUFFER_GRAY_ROOTS, "Buffer Gray Roots", PHASE_MARK_ROOTS, 49 },
        { PHASE_MARK_CCWS, "Mark Cross Compartment Wrappers", PHASE_MARK_ROOTS, 50 },
        { PHASE_MARK_STACK, "Mark C and JS stacks", PHASE_MARK_ROOTS, 51 },
        { PHASE_MARK_RUNTIME_DATA, "Mark Runtime-wide Data", PHASE_MARK_ROOTS, 52 },
        { PHASE_MARK_EMBEDDING, "Mark Embedding", PHASE_MARK_ROOTS, 53 },
        { PHASE_MARK_COMPARTMENTS, "Mark Compartments", PHASE_MARK_ROOTS, 54 },
    { PHASE_PURGE_SHAPE_TABLES, "Purge ShapeTables", PHASE_NO_PARENT, 60 },

    { PHASE_LIMIT, nullptr, PHASE_NO_PARENT, 66 }

    // The current number of telemetryBuckets is equal to the value for
    // PHASE_LIMIT. If you insert new phases somewhere, start at that number and
    // count up. Do not change any existing numbers.
};

static mozilla::EnumeratedArray<Phase, PHASE_LIMIT, ExtraPhaseInfo> phaseExtra;

// Mapping from all nodes with a multi-parented child to a Vector of all
// multi-parented children and their descendants. (Single-parented children will
// not show up in this list.)
static mozilla::Vector<Phase, 0, SystemAllocPolicy> dagDescendants[Statistics::NumTimingArrays];

// Preorder iterator over all phases in the expanded tree. Positions are
// returned as <phase,dagSlot> pairs (dagSlot will be zero aka PHASE_DAG_NONE
// for the top nodes with a single path from the parent, and 1 or more for
// nodes in multiparented subtrees).
struct AllPhaseIterator {
    // If 'descendants' is empty, the current Phase position.
    int current;

    // The depth of the current multiparented node that we are processing, or
    // zero if we are pointing to the top portion of the tree.
    int baseLevel;

    // When looking at multiparented descendants, the dag slot (index into
    // PhaseTimeTables) containing the entries for the current parent.
    size_t activeSlot;

    // When iterating over a multiparented subtree, the list of (remaining)
    // subtree nodes.
    mozilla::Vector<Phase, 0, SystemAllocPolicy>::Range descendants;

    explicit AllPhaseIterator()
      : current(0)
      , baseLevel(0)
      , activeSlot(PHASE_DAG_NONE)
      , descendants(dagDescendants[PHASE_DAG_NONE].all()) /* empty range */
    {
    }

    void get(Phase* phase, size_t* dagSlot, int* level = nullptr) {
        MOZ_ASSERT(!done());
        *dagSlot = activeSlot;
        *phase = descendants.empty() ? Phase(current) : descendants.front();
        if (level)
            *level = phaseExtra[*phase].depth + baseLevel;
    }

    void advance() {
        MOZ_ASSERT(!done());

        if (!descendants.empty()) {
            // Currently iterating over a multiparented subtree.
            descendants.popFront();
            if (!descendants.empty())
                return;

            // Just before leaving the last child, reset the iterator to look
            // at "main" phases (in PHASE_DAG_NONE) instead of multiparented
            // subtree phases.
            ++current;
            activeSlot = PHASE_DAG_NONE;
            baseLevel = 0;
            return;
        }

        auto phase = Phase(current);
        if (phaseExtra[phase].dagSlot != PHASE_DAG_NONE) {
            // The current phase has a shared subtree. Load them up into
            // 'descendants' and advance to the first child.
            activeSlot = phaseExtra[phase].dagSlot;
            descendants = dagDescendants[activeSlot].all();
            MOZ_ASSERT(!descendants.empty());
            baseLevel += phaseExtra[phase].depth + 1;
            return;
        }

        ++current;
    }

    bool done() const {
        return phases[current].parent == PHASE_MULTI_PARENTS;
    }
};

void
Statistics::gcDuration(TimeDuration* total, TimeDuration* maxPause) const
{
    *total = *maxPause = 0;
    for (auto& slice : slices_) {
        *total += slice.duration();
        if (slice.duration() > *maxPause)
            *maxPause = slice.duration();
    }
    if (*maxPause > maxPauseInInterval)
        maxPauseInInterval = *maxPause;
}

void
Statistics::sccDurations(TimeDuration* total, TimeDuration* maxPause) const
{
    *total = *maxPause = 0;
    for (size_t i = 0; i < sccTimes.length(); i++) {
        *total += sccTimes[i];
        *maxPause = Max(*maxPause, sccTimes[i]);
    }
}

typedef Vector<UniqueChars, 8, SystemAllocPolicy> FragmentVector;

static UniqueChars
Join(const FragmentVector& fragments, const char* separator = "") {
    const size_t separatorLength = strlen(separator);
    size_t length = 0;
    for (size_t i = 0; i < fragments.length(); ++i) {
        length += fragments[i] ? strlen(fragments[i].get()) : 0;
        if (i < (fragments.length() - 1))
            length += separatorLength;
    }

    char* joined = js_pod_malloc<char>(length + 1);
    joined[length] = '\0';

    char* cursor = joined;
    for (size_t i = 0; i < fragments.length(); ++i) {
        if (fragments[i])
            strcpy(cursor, fragments[i].get());
        cursor += fragments[i] ? strlen(fragments[i].get()) : 0;
        if (i < (fragments.length() - 1)) {
            if (separatorLength)
                strcpy(cursor, separator);
            cursor += separatorLength;
        }
    }

    return UniqueChars(joined);
}

static TimeDuration
SumChildTimes(size_t phaseSlot, Phase phase, const Statistics::PhaseTimeTable& phaseTimes)
{
    // Sum the contributions from single-parented children.
    TimeDuration total = 0;
    size_t depth = phaseExtra[phase].depth;
    for (unsigned i = phase + 1; i < PHASE_LIMIT && phaseExtra[Phase(i)].depth > depth; i++) {
        if (phases[i].parent == phase)
            total += phaseTimes[phaseSlot][Phase(i)];
    }

    // Sum the contributions from multi-parented children.
    size_t dagSlot = phaseExtra[phase].dagSlot;
    MOZ_ASSERT(dagSlot <= Statistics::MaxMultiparentPhases - 1);
    if (dagSlot != PHASE_DAG_NONE) {
        for (auto edge : dagChildEdges) {
            if (edge.parent == phase)
                total += phaseTimes[dagSlot][edge.child];
        }
    }
    return total;
}

UniqueChars
Statistics::formatCompactSliceMessage() const
{
    // Skip if we OOM'ed.
    if (slices_.length() == 0)
        return UniqueChars(nullptr);

    const size_t index = slices_.length() - 1;
    const SliceData& slice = slices_.back();

    char budgetDescription[200];
    slice.budget.describe(budgetDescription, sizeof(budgetDescription) - 1);

    const char* format =
        "GC Slice %u - Pause: %.3fms of %s budget (@ %.3fms); Reason: %s; Reset: %s%s; Times: ";
    char buffer[1024];
    SprintfLiteral(buffer, format, index,
                   t(slice.duration()), budgetDescription, t(slice.start - slices_[0].start),
                   ExplainReason(slice.reason),
                   slice.wasReset() ? "yes - " : "no",
                   slice.wasReset() ? ExplainAbortReason(slice.resetReason) : "");

    FragmentVector fragments;
    if (!fragments.append(DuplicateString(buffer)) ||
        !fragments.append(formatCompactSlicePhaseTimes(slices_[index].phaseTimes)))
    {
        return UniqueChars(nullptr);
    }
    return Join(fragments);
}

UniqueChars
Statistics::formatCompactSummaryMessage() const
{
    const double bytesPerMiB = 1024 * 1024;

    FragmentVector fragments;
    if (!fragments.append(DuplicateString("Summary - ")))
        return UniqueChars(nullptr);

    TimeDuration total, longest;
    gcDuration(&total, &longest);

    const double mmu20 = computeMMU(TimeDuration::FromMilliseconds(20));
    const double mmu50 = computeMMU(TimeDuration::FromMilliseconds(50));

    char buffer[1024];
    if (!nonincremental()) {
        SprintfLiteral(buffer,
                       "Max Pause: %.3fms; MMU 20ms: %.1f%%; MMU 50ms: %.1f%%; Total: %.3fms; ",
                       t(longest), mmu20 * 100., mmu50 * 100., t(total));
    } else {
        SprintfLiteral(buffer, "Non-Incremental: %.3fms (%s); ",
                       t(total), ExplainAbortReason(nonincrementalReason_));
    }
    if (!fragments.append(DuplicateString(buffer)))
        return UniqueChars(nullptr);

    SprintfLiteral(buffer,
                   "Zones: %d of %d (-%d); Compartments: %d of %d (-%d); HeapSize: %.3f MiB; " \
                   "HeapChange (abs): %+d (%d); ",
                   zoneStats.collectedZoneCount, zoneStats.zoneCount, zoneStats.sweptZoneCount,
                   zoneStats.collectedCompartmentCount, zoneStats.compartmentCount,
                   zoneStats.sweptCompartmentCount,
                   double(preBytes) / bytesPerMiB,
                   counts[STAT_NEW_CHUNK] - counts[STAT_DESTROY_CHUNK],
                   counts[STAT_NEW_CHUNK] + counts[STAT_DESTROY_CHUNK]);
    if (!fragments.append(DuplicateString(buffer)))
        return UniqueChars(nullptr);

    MOZ_ASSERT_IF(counts[STAT_ARENA_RELOCATED], gckind == GC_SHRINK);
    if (gckind == GC_SHRINK) {
        SprintfLiteral(buffer,
                       "Kind: %s; Relocated: %.3f MiB; ",
                       ExplainInvocationKind(gckind),
                       double(ArenaSize * counts[STAT_ARENA_RELOCATED]) / bytesPerMiB);
        if (!fragments.append(DuplicateString(buffer)))
            return UniqueChars(nullptr);
    }

    return Join(fragments);
}

UniqueChars
Statistics::formatCompactSlicePhaseTimes(const PhaseTimeTable& phaseTimes) const
{
    static const TimeDuration MaxUnaccountedTime = TimeDuration::FromMicroseconds(100);

    FragmentVector fragments;
    char buffer[128];
    for (AllPhaseIterator iter; !iter.done(); iter.advance()) {
        Phase phase;
        size_t dagSlot;
        int level;
        iter.get(&phase, &dagSlot, &level);
        MOZ_ASSERT(level < 4);

        TimeDuration ownTime = phaseTimes[dagSlot][phase];
        TimeDuration childTime = SumChildTimes(dagSlot, phase, phaseTimes);
        if (ownTime > MaxUnaccountedTime) {
            SprintfLiteral(buffer, "%s: %.3fms", phases[phase].name, t(ownTime));
            if (!fragments.append(DuplicateString(buffer)))
                return UniqueChars(nullptr);

            if (childTime && (ownTime - childTime) > MaxUnaccountedTime) {
                MOZ_ASSERT(level < 3);
                SprintfLiteral(buffer, "%s: %.3fms", "Other", t(ownTime - childTime));
                if (!fragments.append(DuplicateString(buffer)))
                    return UniqueChars(nullptr);
            }
        }
    }
    return Join(fragments, ", ");
}

UniqueChars
Statistics::formatDetailedMessage() const
{
    FragmentVector fragments;

    if (!fragments.append(formatDetailedDescription()))
        return UniqueChars(nullptr);

    if (!slices_.empty()) {
        for (unsigned i = 0; i < slices_.length(); i++) {
            if (!fragments.append(formatDetailedSliceDescription(i, slices_[i])))
                return UniqueChars(nullptr);
            if (!fragments.append(formatDetailedPhaseTimes(slices_[i].phaseTimes)))
                return UniqueChars(nullptr);
        }
    }
    if (!fragments.append(formatDetailedTotals()))
        return UniqueChars(nullptr);
    if (!fragments.append(formatDetailedPhaseTimes(phaseTimes)))
        return UniqueChars(nullptr);

    return Join(fragments);
}

UniqueChars
Statistics::formatDetailedDescription() const
{
    const double bytesPerMiB = 1024 * 1024;

    TimeDuration sccTotal, sccLongest;
    sccDurations(&sccTotal, &sccLongest);

    const double mmu20 = computeMMU(TimeDuration::FromMilliseconds(20));
    const double mmu50 = computeMMU(TimeDuration::FromMilliseconds(50));

    const char* format =
"=================================================================\n\
  Invocation Kind: %s\n\
  Reason: %s\n\
  Incremental: %s%s\n\
  Zones Collected: %d of %d (-%d)\n\
  Compartments Collected: %d of %d (-%d)\n\
  MinorGCs since last GC: %d\n\
  Store Buffer Overflows: %d\n\
  MMU 20ms:%.1f%%; 50ms:%.1f%%\n\
  SCC Sweep Total (MaxPause): %.3fms (%.3fms)\n\
  HeapSize: %.3f MiB\n\
  Chunk Delta (magnitude): %+d  (%d)\n\
  Arenas Relocated: %.3f MiB\n\
";
    char buffer[1024];
    SprintfLiteral(buffer, format,
                   ExplainInvocationKind(gckind),
                   ExplainReason(slices_[0].reason),
                   nonincremental() ? "no - " : "yes",
                   nonincremental() ? ExplainAbortReason(nonincrementalReason_) : "",
                   zoneStats.collectedZoneCount, zoneStats.zoneCount, zoneStats.sweptZoneCount,
                   zoneStats.collectedCompartmentCount, zoneStats.compartmentCount,
                   zoneStats.sweptCompartmentCount,
                   getCount(STAT_MINOR_GC),
                   getCount(STAT_STOREBUFFER_OVERFLOW),
                   mmu20 * 100., mmu50 * 100.,
                   t(sccTotal), t(sccLongest),
                   double(preBytes) / bytesPerMiB,
                   getCount(STAT_NEW_CHUNK) - getCount(STAT_DESTROY_CHUNK),
                   getCount(STAT_NEW_CHUNK) + getCount(STAT_DESTROY_CHUNK),
                   double(ArenaSize * getCount(STAT_ARENA_RELOCATED)) / bytesPerMiB);
    return DuplicateString(buffer);
}

UniqueChars
Statistics::formatDetailedSliceDescription(unsigned i, const SliceData& slice) const
{
    char budgetDescription[200];
    slice.budget.describe(budgetDescription, sizeof(budgetDescription) - 1);

    const char* format =
"\
  ---- Slice %u ----\n\
    Reason: %s\n\
    Reset: %s%s\n\
    State: %s -> %s\n\
    Page Faults: %ld\n\
    Pause: %.3fms of %s budget (@ %.3fms)\n\
";
    char buffer[1024];
    SprintfLiteral(buffer, format, i, ExplainReason(slice.reason),
                   slice.wasReset() ? "yes - " : "no",
                   slice.wasReset() ? ExplainAbortReason(slice.resetReason) : "",
                   gc::StateName(slice.initialState), gc::StateName(slice.finalState),
                   uint64_t(slice.endFaults - slice.startFaults),
                   t(slice.duration()), budgetDescription, t(slice.start - slices_[0].start));
    return DuplicateString(buffer);
}

UniqueChars
Statistics::formatDetailedPhaseTimes(const PhaseTimeTable& phaseTimes) const
{
    static const TimeDuration MaxUnaccountedChildTime = TimeDuration::FromMicroseconds(50);

    FragmentVector fragments;
    char buffer[128];
    for (AllPhaseIterator iter; !iter.done(); iter.advance()) {
        Phase phase;
        size_t dagSlot;
        int level;
        iter.get(&phase, &dagSlot, &level);

        TimeDuration ownTime = phaseTimes[dagSlot][phase];
        TimeDuration childTime = SumChildTimes(dagSlot, phase, phaseTimes);
        if (!ownTime.IsZero()) {
            SprintfLiteral(buffer, "      %*s: %.3fms\n",
                           level * 2, phases[phase].name, t(ownTime));
            if (!fragments.append(DuplicateString(buffer)))
                return UniqueChars(nullptr);

            if (childTime && (ownTime - childTime) > MaxUnaccountedChildTime) {
                SprintfLiteral(buffer, "      %*s: %.3fms\n",
                               (level + 1) * 2, "Other", t(ownTime - childTime));
                if (!fragments.append(DuplicateString(buffer)))
                    return UniqueChars(nullptr);
            }
        }
    }
    return Join(fragments);
}

UniqueChars
Statistics::formatDetailedTotals() const
{
    TimeDuration total, longest;
    gcDuration(&total, &longest);

    const char* format =
"\
  ---- Totals ----\n\
    Total Time: %.3fms\n\
    Max Pause: %.3fms\n\
";
    char buffer[1024];
    SprintfLiteral(buffer, format, t(total), t(longest));
    return DuplicateString(buffer);
}

void
Statistics::formatJsonSlice(size_t sliceNum, JSONPrinter& json) const
{
    json.beginObject();
    formatJsonSliceDescription(sliceNum, slices_[sliceNum], json);

    json.beginObjectProperty("times");
    formatJsonPhaseTimes(slices_[sliceNum].phaseTimes, json);
    json.endObject();

    json.endObject();
}

UniqueChars
Statistics::renderJsonSlice(size_t sliceNum) const
{
    Sprinter printer(nullptr, false);
    if (!printer.init())
        return UniqueChars(nullptr);
    JSONPrinter json(printer);

    formatJsonSlice(sliceNum, json);
    return UniqueChars(printer.release());
}

UniqueChars
Statistics::renderNurseryJson(JSRuntime* rt) const
{
    Sprinter printer(nullptr, false);
    if (!printer.init())
        return UniqueChars(nullptr);
    JSONPrinter json(printer);
    rt->gc.nursery().renderProfileJSON(json);
    return UniqueChars(printer.release());
}

UniqueChars
Statistics::renderJsonMessage(uint64_t timestamp, bool includeSlices) const
{
    if (aborted)
        return DuplicateString("{status:\"aborted\"}"); // May return nullptr

    Sprinter printer(nullptr, false);
    if (!printer.init())
        return UniqueChars(nullptr);
    JSONPrinter json(printer);

    json.beginObject();
    formatJsonDescription(timestamp, json);

    if (includeSlices) {
        json.beginListProperty("slices");
        for (unsigned i = 0; i < slices_.length(); i++)
            formatJsonSlice(i, json);
        json.endList();
    }

    json.beginObjectProperty("totals");
    formatJsonPhaseTimes(phaseTimes, json);
    json.endObject();

    json.endObject();

    return UniqueChars(printer.release());
}

void
Statistics::formatJsonDescription(uint64_t timestamp, JSONPrinter& json) const
{
    json.property("timestamp", timestamp);

    TimeDuration total, longest;
    gcDuration(&total, &longest);
    json.property("max_pause", longest, JSONPrinter::MILLISECONDS);
    json.property("total_time", total, JSONPrinter::MILLISECONDS);

    json.property("reason", ExplainReason(slices_[0].reason));
    json.property("zones_collected", zoneStats.collectedZoneCount);
    json.property("total_zones", zoneStats.zoneCount);
    json.property("total_compartments", zoneStats.compartmentCount);
    json.property("minor_gcs", counts[STAT_MINOR_GC]);
    json.property("store_buffer_overflows", counts[STAT_STOREBUFFER_OVERFLOW]);

    const double mmu20 = computeMMU(TimeDuration::FromMilliseconds(20));
    const double mmu50 = computeMMU(TimeDuration::FromMilliseconds(50));
    json.property("mmu_20ms", int(mmu20 * 100));
    json.property("mmu_50ms", int(mmu50 * 100));

    TimeDuration sccTotal, sccLongest;
    sccDurations(&sccTotal, &sccLongest);
    json.property("scc_sweep_total", sccTotal, JSONPrinter::MILLISECONDS);
    json.property("scc_sweep_max_pause", sccLongest, JSONPrinter::MILLISECONDS);

    json.property("nonincremental_reason", ExplainAbortReason(nonincrementalReason_));
    json.property("allocated", uint64_t(preBytes) / 1024 / 1024);
    json.property("added_chunks", getCount(STAT_NEW_CHUNK));
    json.property("removed_chunks", getCount(STAT_DESTROY_CHUNK));
    json.property("major_gc_number", startingMajorGCNumber);
    json.property("minor_gc_number", startingMinorGCNumber);
}

void
Statistics::formatJsonSliceDescription(unsigned i, const SliceData& slice, JSONPrinter& json) const
{
    TimeDuration when = slice.start - slices_[0].start;
    char budgetDescription[200];
    slice.budget.describe(budgetDescription, sizeof(budgetDescription) - 1);
    TimeStamp originTime = TimeStamp::ProcessCreation();

    json.property("slice", i);
    json.property("pause", slice.duration(), JSONPrinter::MILLISECONDS);
    json.property("when", when, JSONPrinter::MILLISECONDS);
    json.property("reason", ExplainReason(slice.reason));
    json.property("initial_state", gc::StateName(slice.initialState));
    json.property("final_state", gc::StateName(slice.finalState));
    json.property("budget", budgetDescription);
    json.property("page_faults", int64_t(slice.endFaults - slice.startFaults));
    json.property("start_timestamp", slice.start - originTime, JSONPrinter::SECONDS);
    json.property("end_timestamp", slice.end - originTime, JSONPrinter::SECONDS);
}

static UniqueChars
SanitizeJsonKey(const char* const buffer)
{
    char* mut = strdup(buffer);
    if (!mut)
        return UniqueChars(nullptr);

    char* c = mut;
    while (*c) {
        if (!isalpha(*c))
            *c = '_';
        else if (isupper(*c))
            *c = tolower(*c);
        ++c;
    }

    return UniqueChars(mut);
}

void
Statistics::formatJsonPhaseTimes(const PhaseTimeTable& phaseTimes, JSONPrinter& json) const
{
    for (AllPhaseIterator iter; !iter.done(); iter.advance()) {
        Phase phase;
        size_t dagSlot;
        iter.get(&phase, &dagSlot);

        UniqueChars name = SanitizeJsonKey(phases[phase].name);
        if (!name)
            json.outOfMemory();
        TimeDuration ownTime = phaseTimes[dagSlot][phase];
        if (!ownTime.IsZero())
            json.property(name.get(), ownTime, JSONPrinter::MILLISECONDS);
    }
}

Statistics::Statistics(JSRuntime* rt)
  : runtime(rt),
    fp(nullptr),
    nonincrementalReason_(gc::AbortReason::None),
    preBytes(0),
    maxPauseInInterval(0),
    phaseNestingDepth(0),
    activeDagSlot(PHASE_DAG_NONE),
    suspended(0),
    sliceCallback(nullptr),
    nurseryCollectionCallback(nullptr),
    aborted(false),
    enableProfiling_(false),
    sliceCount_(0)
{
    for (auto& count : counts)
        count = 0;

    const char* env = getenv("MOZ_GCTIMER");
    if (env) {
        if (strcmp(env, "none") == 0) {
            fp = nullptr;
        } else if (strcmp(env, "stdout") == 0) {
            fp = stdout;
        } else if (strcmp(env, "stderr") == 0) {
            fp = stderr;
        } else {
            fp = fopen(env, "a");
            if (!fp)
                MOZ_CRASH("Failed to open MOZ_GCTIMER log file.");
        }
    }

    env = getenv("JS_GC_PROFILE");
    if (env) {
        if (0 == strcmp(env, "help")) {
            fprintf(stderr, "JS_GC_PROFILE=N\n"
                    "\tReport major GC's taking more than N milliseconds.\n");
            exit(0);
        }
        enableProfiling_ = true;
        profileThreshold_ = TimeDuration::FromMilliseconds(atoi(env));
    }

    PodZero(&totalTimes_);
}

Statistics::~Statistics()
{
    if (fp && fp != stdout && fp != stderr)
        fclose(fp);
}

/* static */ bool
Statistics::initialize()
{
#ifdef DEBUG
    for (auto i : AllPhases()) {
        MOZ_ASSERT(phases[i].index == i);
        for (auto j : AllPhases())
            MOZ_ASSERT_IF(i != j, phases[i].telemetryBucket != phases[j].telemetryBucket);
    }
#endif

    // Create a static table of descendants for every phase with multiple
    // children. This assumes that all descendants come linearly in the
    // list, which is reasonable since full dags are not supported; any
    // path from the leaf to the root must encounter at most one node with
    // multiple parents.
    size_t dagSlot = 0;
    for (size_t i = 0; i < mozilla::ArrayLength(dagChildEdges); i++) {
        Phase parent = dagChildEdges[i].parent;
        if (!phaseExtra[parent].dagSlot)
            phaseExtra[parent].dagSlot = ++dagSlot;

        Phase child = dagChildEdges[i].child;
        MOZ_ASSERT(phases[child].parent == PHASE_MULTI_PARENTS);
        int j = child;
        do {
            if (!dagDescendants[phaseExtra[parent].dagSlot].append(Phase(j)))
                return false;
            j++;
        } while (j != PHASE_LIMIT && phases[j].parent != PHASE_MULTI_PARENTS);
    }
    MOZ_ASSERT(dagSlot <= MaxMultiparentPhases - 1);

    // Fill in the depth of each node in the tree. Multi-parented nodes
    // have depth 0.
    mozilla::Vector<Phase, 0, SystemAllocPolicy> stack;
    if (!stack.append(PHASE_LIMIT)) // Dummy entry to avoid special-casing the first node
        return false;
    for (auto i : AllPhases()) {
        if (phases[i].parent == PHASE_NO_PARENT ||
            phases[i].parent == PHASE_MULTI_PARENTS)
        {
            stack.clear();
        } else {
            while (stack.back() != phases[i].parent)
                stack.popBack();
        }
        phaseExtra[i].depth = stack.length();
        if (!stack.append(i))
            return false;
    }

    return true;
}

JS::GCSliceCallback
Statistics::setSliceCallback(JS::GCSliceCallback newCallback)
{
    JS::GCSliceCallback oldCallback = sliceCallback;
    sliceCallback = newCallback;
    return oldCallback;
}

JS::GCNurseryCollectionCallback
Statistics::setNurseryCollectionCallback(JS::GCNurseryCollectionCallback newCallback)
{
    auto oldCallback = nurseryCollectionCallback;
    nurseryCollectionCallback = newCallback;
    return oldCallback;
}

TimeDuration
Statistics::clearMaxGCPauseAccumulator()
{
    TimeDuration prior = maxPauseInInterval;
    maxPauseInInterval = 0;
    return prior;
}

TimeDuration
Statistics::getMaxGCPauseSinceClear()
{
    return maxPauseInInterval;
}

// Sum up the time for a phase, including instances of the phase with different
// parents.
static TimeDuration
SumPhase(Phase phase, const Statistics::PhaseTimeTable& times)
{
    TimeDuration sum = 0;
    for (const auto& phaseTimes : times)
        sum += phaseTimes[phase];
    return sum;
}

static void
CheckSelfTime(Phase parent, Phase child, const Statistics::PhaseTimeTable& times, TimeDuration selfTimes[PHASE_LIMIT], TimeDuration childTime)
{
    if (selfTimes[parent] < childTime) {
        fprintf(stderr,
                "Parent %s time = %.3fms"
                " with %.3fms remaining, "
                "child %s time %.3fms\n",
                phases[parent].name, SumPhase(parent, times).ToMilliseconds(),
                selfTimes[parent].ToMilliseconds(),
                phases[child].name, childTime.ToMilliseconds());
    }
}

static Phase
LongestPhaseSelfTime(const Statistics::PhaseTimeTable& times)
{
    TimeDuration selfTimes[PHASE_LIMIT];

    // Start with total times, including children's times.
    for (auto i : AllPhases())
        selfTimes[i] = SumPhase(i, times);

    // We have the total time spent in each phase, including descendant times.
    // Loop over the children and subtract their times from their parent's self
    // time.
    for (auto i : AllPhases()) {
        Phase parent = phases[i].parent;
        if (parent == PHASE_MULTI_PARENTS) {
            // Current phase i has multiple parents. Each "instance" of this
            // phase is in a parallel array of times indexed by 'dagSlot', so
            // subtract only the dagSlot-specific child's time from the parent.
            for (auto edge : dagChildEdges) {
                if (edge.parent == i) {
                    size_t dagSlot = phaseExtra[edge.parent].dagSlot;
                    MOZ_ASSERT(dagSlot <= Statistics::MaxMultiparentPhases - 1);
                    CheckSelfTime(edge.parent, edge.child, times,
                                  selfTimes, times[dagSlot][edge.child]);
                    MOZ_ASSERT(selfTimes[edge.parent] >= times[dagSlot][edge.child]);
                    selfTimes[edge.parent] -= times[dagSlot][edge.child];
                }
            }
        } else if (parent != PHASE_NO_PARENT) {
            CheckSelfTime(parent, i, times, selfTimes, selfTimes[i]);
            MOZ_ASSERT(selfTimes[parent] >= selfTimes[i]);
            selfTimes[parent] -= selfTimes[i];
        }
    }

    TimeDuration longestTime = 0;
    Phase longestPhase = PHASE_NONE;
    for (auto i : AllPhases()) {
        if (selfTimes[i] > longestTime) {
            longestTime = selfTimes[i];
            longestPhase = i;
        }
    }

    return longestPhase;
}

void
Statistics::printStats()
{
    if (aborted) {
        fprintf(fp, "OOM during GC statistics collection. The report is unavailable for this GC.\n");
    } else {
        UniqueChars msg = formatDetailedMessage();
        if (msg) {
            double secSinceStart =
                (slices_[0].start - TimeStamp::ProcessCreation()).ToSeconds();
            fprintf(fp, "GC(T+%.3fs) %s\n", secSinceStart, msg.get());
        }
    }
    fflush(fp);
}

void
Statistics::beginGC(JSGCInvocationKind kind)
{
    slices_.clearAndFree();
    sccTimes.clearAndFree();
    gckind = kind;
    nonincrementalReason_ = gc::AbortReason::None;

    preBytes = runtime->gc.usage.gcBytes();
    startingMajorGCNumber = runtime->gc.majorGCCount();
}

void
Statistics::endGC()
{
    for (auto j : IntegerRange(NumTimingArrays)) {
        for (auto i : AllPhases())
            phaseTotals[j][i] += phaseTimes[j][i];
    }

    TimeDuration sccTotal, sccLongest;
    sccDurations(&sccTotal, &sccLongest);

    runtime->addTelemetry(JS_TELEMETRY_GC_IS_ZONE_GC, !zoneStats.isCollectingAllZones());
    TimeDuration markTotal = SumPhase(PHASE_MARK, phaseTimes);
    TimeDuration markRootsTotal = SumPhase(PHASE_MARK_ROOTS, phaseTimes);
    runtime->addTelemetry(JS_TELEMETRY_GC_MARK_MS, t(markTotal));
    runtime->addTelemetry(JS_TELEMETRY_GC_SWEEP_MS, t(phaseTimes[PHASE_DAG_NONE][PHASE_SWEEP]));
    if (runtime->gc.isCompactingGc()) {
        runtime->addTelemetry(JS_TELEMETRY_GC_COMPACT_MS,
                              t(phaseTimes[PHASE_DAG_NONE][PHASE_COMPACT]));
    }
    runtime->addTelemetry(JS_TELEMETRY_GC_MARK_ROOTS_MS, t(markRootsTotal));
    runtime->addTelemetry(JS_TELEMETRY_GC_MARK_GRAY_MS, t(phaseTimes[PHASE_DAG_NONE][PHASE_SWEEP_MARK_GRAY]));
    runtime->addTelemetry(JS_TELEMETRY_GC_NON_INCREMENTAL, nonincremental());
    if (nonincremental())
        runtime->addTelemetry(JS_TELEMETRY_GC_NON_INCREMENTAL_REASON, uint32_t(nonincrementalReason_));
    runtime->addTelemetry(JS_TELEMETRY_GC_INCREMENTAL_DISABLED, !runtime->gc.isIncrementalGCAllowed());
    runtime->addTelemetry(JS_TELEMETRY_GC_SCC_SWEEP_TOTAL_MS, t(sccTotal));
    runtime->addTelemetry(JS_TELEMETRY_GC_SCC_SWEEP_MAX_PAUSE_MS, t(sccLongest));

    if (!aborted) {
        TimeDuration total, longest;
        gcDuration(&total, &longest);

        runtime->addTelemetry(JS_TELEMETRY_GC_MS, t(total));
        runtime->addTelemetry(JS_TELEMETRY_GC_MAX_PAUSE_MS, t(longest));

        const double mmu50 = computeMMU(TimeDuration::FromMilliseconds(50));
        runtime->addTelemetry(JS_TELEMETRY_GC_MMU_50, mmu50 * 100);
    }

    if (fp)
        printStats();

    // Clear the OOM flag.
    aborted = false;
}

void
Statistics::beginNurseryCollection(JS::gcreason::Reason reason)
{
    count(STAT_MINOR_GC);
    startingMinorGCNumber = runtime->gc.minorGCCount();
    if (nurseryCollectionCallback) {
        (*nurseryCollectionCallback)(TlsContext.get(),
                                     JS::GCNurseryProgress::GC_NURSERY_COLLECTION_START,
                                     reason);
    }
}

void
Statistics::endNurseryCollection(JS::gcreason::Reason reason)
{
    if (nurseryCollectionCallback) {
        (*nurseryCollectionCallback)(TlsContext.get(),
                                     JS::GCNurseryProgress::GC_NURSERY_COLLECTION_END,
                                     reason);
    }
}

void
Statistics::beginSlice(const ZoneGCStats& zoneStats, JSGCInvocationKind gckind,
                       SliceBudget budget, JS::gcreason::Reason reason)
{
    this->zoneStats = zoneStats;

    bool first = !runtime->gc.isIncrementalGCInProgress();
    if (first)
        beginGC(gckind);

    if (!slices_.emplaceBack(budget,
                             reason,
                             TimeStamp::Now(),
                             GetPageFaultCount(),
                             runtime->gc.state()))
    {
        // If we are OOM, set a flag to indicate we have missing slice data.
        aborted = true;
        return;
    }

    runtime->addTelemetry(JS_TELEMETRY_GC_REASON, reason);

    // Slice callbacks should only fire for the outermost level.
    bool wasFullGC = zoneStats.isCollectingAllZones();
    if (sliceCallback)
        (*sliceCallback)(TlsContext.get(),
                         first ? JS::GC_CYCLE_BEGIN : JS::GC_SLICE_BEGIN,
                         JS::GCDescription(!wasFullGC, gckind, reason));
}

void
Statistics::endSlice()
{
    if (!aborted) {
        slices_.back().end = TimeStamp::Now();
        slices_.back().endFaults = GetPageFaultCount();
        slices_.back().finalState = runtime->gc.state();

        TimeDuration sliceTime = slices_.back().end - slices_.back().start;
        runtime->addTelemetry(JS_TELEMETRY_GC_SLICE_MS, t(sliceTime));
        runtime->addTelemetry(JS_TELEMETRY_GC_RESET, slices_.back().wasReset());
        if (slices_.back().wasReset())
            runtime->addTelemetry(JS_TELEMETRY_GC_RESET_REASON, uint32_t(slices_.back().resetReason));

        if (slices_.back().budget.isTimeBudget()) {
            int64_t budget_ms = slices_.back().budget.timeBudget.budget;
            runtime->addTelemetry(JS_TELEMETRY_GC_BUDGET_MS, budget_ms);
            if (budget_ms == runtime->gc.defaultSliceBudget())
                runtime->addTelemetry(JS_TELEMETRY_GC_ANIMATION_MS, t(sliceTime));

            // Record any phase that goes more than 2x over its budget.
            if (sliceTime.ToMilliseconds() > 2 * budget_ms) {
                Phase longest = LongestPhaseSelfTime(slices_.back().phaseTimes);
                runtime->addTelemetry(JS_TELEMETRY_GC_SLOW_PHASE, phases[longest].telemetryBucket);
            }
        }

        sliceCount_++;
    }

    bool last = !runtime->gc.isIncrementalGCInProgress();
    if (last)
        endGC();

    if (enableProfiling_ && !aborted && slices_.back().duration() >= profileThreshold_)
        printSliceProfile();

    // Slice callbacks should only fire for the outermost level.
    if (!aborted) {
        bool wasFullGC = zoneStats.isCollectingAllZones();
        if (sliceCallback)
            (*sliceCallback)(TlsContext.get(),
                             last ? JS::GC_CYCLE_END : JS::GC_SLICE_END,
                             JS::GCDescription(!wasFullGC, gckind, slices_.back().reason));
    }

    // Do this after the slice callback since it uses these values.
    if (last) {
        for (auto& count : counts)
            count = 0;

        // Clear the timers at the end of a GC because we accumulate time in
        // between GCs for some (which come before PHASE_GC_BEGIN in the list.)
        PodZero(&phaseStartTimes[PHASE_GC_BEGIN], PHASE_LIMIT - PHASE_GC_BEGIN);
        for (size_t d = PHASE_DAG_NONE; d < NumTimingArrays; d++)
            PodZero(&phaseTimes[d][PHASE_GC_BEGIN], PHASE_LIMIT - PHASE_GC_BEGIN);
    }
}

bool
Statistics::startTimingMutator()
{
    if (phaseNestingDepth != 0) {
        // Should only be called from outside of GC.
        MOZ_ASSERT(phaseNestingDepth == 1);
        MOZ_ASSERT(phaseNesting[0] == PHASE_MUTATOR);
        return false;
    }

    MOZ_ASSERT(suspended == 0);

    timedGCTime = 0;
    phaseStartTimes[PHASE_MUTATOR] = TimeStamp();
    phaseTimes[PHASE_DAG_NONE][PHASE_MUTATOR] = 0;
    timedGCStart = TimeStamp();

    beginPhase(PHASE_MUTATOR);
    return true;
}

bool
Statistics::stopTimingMutator(double& mutator_ms, double& gc_ms)
{
    // This should only be called from outside of GC, while timing the mutator.
    if (phaseNestingDepth != 1 || phaseNesting[0] != PHASE_MUTATOR)
        return false;

    endPhase(PHASE_MUTATOR);
    mutator_ms = t(phaseTimes[PHASE_DAG_NONE][PHASE_MUTATOR]);
    gc_ms = t(timedGCTime);

    return true;
}

void
Statistics::suspendPhases(Phase suspension)
{
    MOZ_ASSERT(suspension == PHASE_EXPLICIT_SUSPENSION || suspension == PHASE_IMPLICIT_SUSPENSION);
    while (phaseNestingDepth) {
        MOZ_ASSERT(suspended < mozilla::ArrayLength(suspendedPhases));
        Phase parent = phaseNesting[phaseNestingDepth - 1];
        suspendedPhases[suspended++] = parent;
        recordPhaseEnd(parent);
    }
    suspendedPhases[suspended++] = suspension;
}

void
Statistics::resumePhases()
{
    DebugOnly<Phase> popped = suspendedPhases[--suspended];
    MOZ_ASSERT(popped == PHASE_EXPLICIT_SUSPENSION || popped == PHASE_IMPLICIT_SUSPENSION);
    while (suspended &&
           suspendedPhases[suspended - 1] != PHASE_EXPLICIT_SUSPENSION &&
           suspendedPhases[suspended - 1] != PHASE_IMPLICIT_SUSPENSION)
    {
        Phase resumePhase = suspendedPhases[--suspended];
        if (resumePhase == PHASE_MUTATOR)
            timedGCTime += TimeStamp::Now() - timedGCStart;
        beginPhase(resumePhase);
    }
}

void
Statistics::beginPhase(Phase phase)
{
    // No longer timing these phases. We should never see these.
    MOZ_ASSERT(phase != PHASE_GC_BEGIN && phase != PHASE_GC_END);

    Phase parent = phaseNestingDepth ? phaseNesting[phaseNestingDepth - 1] : PHASE_NO_PARENT;

    // PHASE_MUTATOR is suspended while performing GC.
    if (parent == PHASE_MUTATOR) {
        suspendPhases(PHASE_IMPLICIT_SUSPENSION);
        parent = phaseNestingDepth ? phaseNesting[phaseNestingDepth - 1] : PHASE_NO_PARENT;
    }

    // Guard against any other re-entry.
    MOZ_ASSERT(!phaseStartTimes[phase]);

    MOZ_ASSERT(phases[phase].index == phase);
    MOZ_ASSERT(phaseNestingDepth < MAX_NESTING);
    MOZ_ASSERT(phases[phase].parent == parent || phases[phase].parent == PHASE_MULTI_PARENTS);

    phaseNesting[phaseNestingDepth] = phase;
    phaseNestingDepth++;

    if (phases[phase].parent == PHASE_MULTI_PARENTS) {
        MOZ_ASSERT(parent != PHASE_NO_PARENT);
        activeDagSlot = phaseExtra[parent].dagSlot;
    }
    MOZ_ASSERT(activeDagSlot <= MaxMultiparentPhases - 1);

    phaseStartTimes[phase] = TimeStamp::Now();
}

void
Statistics::recordPhaseEnd(Phase phase)
{
    TimeStamp now = TimeStamp::Now();

    if (phase == PHASE_MUTATOR)
        timedGCStart = now;

    phaseNestingDepth--;

    TimeDuration t = now - phaseStartTimes[phase];
    if (!slices_.empty())
        slices_.back().phaseTimes[activeDagSlot][phase] += t;
    phaseTimes[activeDagSlot][phase] += t;
    phaseStartTimes[phase] = TimeStamp();
}

void
Statistics::endPhase(Phase phase)
{
    recordPhaseEnd(phase);

    if (phases[phase].parent == PHASE_MULTI_PARENTS)
        activeDagSlot = PHASE_DAG_NONE;

    // When emptying the stack, we may need to return to timing the mutator
    // (PHASE_MUTATOR).
    if (phaseNestingDepth == 0 && suspended > 0 && suspendedPhases[suspended - 1] == PHASE_IMPLICIT_SUSPENSION)
        resumePhases();
}

void
Statistics::endParallelPhase(Phase phase, const GCParallelTask* task)
{
    phaseNestingDepth--;

    if (!slices_.empty())
        slices_.back().phaseTimes[PHASE_DAG_NONE][phase] += task->duration();
    phaseTimes[PHASE_DAG_NONE][phase] += task->duration();
    phaseStartTimes[phase] = TimeStamp();
}

TimeStamp
Statistics::beginSCC()
{
    return TimeStamp::Now();
}

void
Statistics::endSCC(unsigned scc, TimeStamp start)
{
    if (scc >= sccTimes.length() && !sccTimes.resize(scc + 1))
        return;

    sccTimes[scc] += TimeStamp::Now() - start;
}

/*
 * MMU (minimum mutator utilization) is a measure of how much garbage collection
 * is affecting the responsiveness of the system. MMU measurements are given
 * with respect to a certain window size. If we report MMU(50ms) = 80%, then
 * that means that, for any 50ms window of time, at least 80% of the window is
 * devoted to the mutator. In other words, the GC is running for at most 20% of
 * the window, or 10ms. The GC can run multiple slices during the 50ms window
 * as long as the total time it spends is at most 10ms.
 */
double
Statistics::computeMMU(TimeDuration window) const
{
    MOZ_ASSERT(!slices_.empty());

    TimeDuration gc = slices_[0].end - slices_[0].start;
    TimeDuration gcMax = gc;

    if (gc >= window)
        return 0.0;

    int startIndex = 0;
    for (size_t endIndex = 1; endIndex < slices_.length(); endIndex++) {
        auto* startSlice = &slices_[startIndex];
        auto& endSlice = slices_[endIndex];
        gc += endSlice.end - endSlice.start;

        while (endSlice.end - startSlice->end >= window) {
            gc -= startSlice->end - startSlice->start;
            startSlice = &slices_[++startIndex];
        }

        TimeDuration cur = gc;
        if (endSlice.end - startSlice->start > window)
            cur -= (endSlice.end - startSlice->start - window);
        if (cur > gcMax)
            gcMax = cur;
    }

    return double((window - gcMax) / window);
}

void
Statistics::maybePrintProfileHeaders()
{
    static int printedHeader = 0;
    if ((printedHeader++ % 200) == 0) {
        printProfileHeader();
        for (ZoneGroupsIter group(runtime); !group.done(); group.next()) {
            if (group->nursery().enableProfiling()) {
                Nursery::printProfileHeader();
                break;
            }
        }
    }
}

void
Statistics::printProfileHeader()
{
    if (!enableProfiling_)
        return;

    fprintf(stderr, "MajorGC:               Reason States      ");
    fprintf(stderr, " %6s", "total");
#define PRINT_PROFILE_HEADER(name, text, phase)                               \
    fprintf(stderr, " %6s", text);
FOR_EACH_GC_PROFILE_TIME(PRINT_PROFILE_HEADER)
#undef PRINT_PROFILE_HEADER
    fprintf(stderr, "\n");
}

/* static */ void
Statistics::printProfileTimes(const ProfileDurations& times)
{
    for (auto time : times)
        fprintf(stderr, " %6" PRIi64, static_cast<int64_t>(time.ToMilliseconds()));
    fprintf(stderr, "\n");
}

void
Statistics::printSliceProfile()
{
    const SliceData& slice = slices_.back();

    maybePrintProfileHeaders();

    fprintf(stderr, "MajorGC: %20s %1d -> %1d      ",
            ExplainReason(slice.reason), int(slice.initialState), int(slice.finalState));

    ProfileDurations times;
    times[ProfileKey::Total] = slice.duration();
    totalTimes_[ProfileKey::Total] += times[ProfileKey::Total];

#define GET_PROFILE_TIME(name, text, phase)                                   \
    times[ProfileKey::name] = slice.phaseTimes[PHASE_DAG_NONE][phase];                     \
    totalTimes_[ProfileKey::name] += times[ProfileKey::name];
FOR_EACH_GC_PROFILE_TIME(GET_PROFILE_TIME)
#undef GET_PROFILE_TIME

    printProfileTimes(times);
}

void
Statistics::printTotalProfileTimes()
{
    if (enableProfiling_) {
        fprintf(stderr, "MajorGC TOTALS: %7" PRIu64 " slices:           ", sliceCount_);
        printProfileTimes(totalTimes_);
    }
}

