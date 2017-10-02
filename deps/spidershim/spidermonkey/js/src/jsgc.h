/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS Garbage Collector. */

#ifndef jsgc_h
#define jsgc_h

#include "mozilla/Atomics.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"

#include "js/GCAPI.h"
#include "js/SliceBudget.h"
#include "js/Vector.h"
#include "threading/ConditionVariable.h"
#include "threading/Thread.h"
#include "vm/NativeObject.h"

namespace js {

class AutoLockHelperThreadState;

namespace gcstats {
struct Statistics;
} // namespace gcstats

class Nursery;

namespace gc {

struct FinalizePhase;

#define GCSTATES(D) \
    D(NotActive) \
    D(MarkRoots) \
    D(Mark) \
    D(Sweep) \
    D(Finalize) \
    D(Compact) \
    D(Decommit)
enum class State {
#define MAKE_STATE(name) name,
    GCSTATES(MAKE_STATE)
#undef MAKE_STATE
};

// Reasons we reset an ongoing incremental GC or perform a non-incremental GC.
#define GC_ABORT_REASONS(D) \
    D(None) \
    D(NonIncrementalRequested) \
    D(AbortRequested) \
    D(Unused1) \
    D(IncrementalDisabled) \
    D(ModeChange) \
    D(MallocBytesTrigger) \
    D(GCBytesTrigger) \
    D(ZoneChange) \
    D(CompartmentRevived)
enum class AbortReason {
#define MAKE_REASON(name) name,
    GC_ABORT_REASONS(MAKE_REASON)
#undef MAKE_REASON
};

/*
 * Map from C++ type to alloc kind for non-object types. JSObject does not have
 * a 1:1 mapping, so must use Arena::thingSize.
 *
 * The AllocKind is available as MapTypeToFinalizeKind<SomeType>::kind.
 */
template <typename T> struct MapTypeToFinalizeKind {};
#define EXPAND_MAPTYPETOFINALIZEKIND(allocKind, traceKind, type, sizedType) \
    template <> struct MapTypeToFinalizeKind<type> { \
        static const AllocKind kind = AllocKind::allocKind; \
    };
FOR_EACH_NONOBJECT_ALLOCKIND(EXPAND_MAPTYPETOFINALIZEKIND)
#undef EXPAND_MAPTYPETOFINALIZEKIND

template <typename T> struct ParticipatesInCC {};
#define EXPAND_PARTICIPATES_IN_CC(_, type, addToCCKind) \
    template <> struct ParticipatesInCC<type> { static const bool value = addToCCKind; };
JS_FOR_EACH_TRACEKIND(EXPAND_PARTICIPATES_IN_CC)
#undef EXPAND_PARTICIPATES_IN_CC

static inline bool
IsNurseryAllocable(AllocKind kind)
{
    MOZ_ASSERT(IsValidAllocKind(kind));
    static const bool map[] = {
        true,      /* AllocKind::FUNCTION */
        true,      /* AllocKind::FUNCTION_EXTENDED */
        false,     /* AllocKind::OBJECT0 */
        true,      /* AllocKind::OBJECT0_BACKGROUND */
        false,     /* AllocKind::OBJECT2 */
        true,      /* AllocKind::OBJECT2_BACKGROUND */
        false,     /* AllocKind::OBJECT4 */
        true,      /* AllocKind::OBJECT4_BACKGROUND */
        false,     /* AllocKind::OBJECT8 */
        true,      /* AllocKind::OBJECT8_BACKGROUND */
        false,     /* AllocKind::OBJECT12 */
        true,      /* AllocKind::OBJECT12_BACKGROUND */
        false,     /* AllocKind::OBJECT16 */
        true,      /* AllocKind::OBJECT16_BACKGROUND */
        false,     /* AllocKind::SCRIPT */
        false,     /* AllocKind::LAZY_SCRIPT */
        false,     /* AllocKind::SHAPE */
        false,     /* AllocKind::ACCESSOR_SHAPE */
        false,     /* AllocKind::BASE_SHAPE */
        false,     /* AllocKind::OBJECT_GROUP */
        false,     /* AllocKind::FAT_INLINE_STRING */
        false,     /* AllocKind::STRING */
        false,     /* AllocKind::EXTERNAL_STRING */
        false,     /* AllocKind::FAT_INLINE_ATOM */
        false,     /* AllocKind::ATOM */
        false,     /* AllocKind::SYMBOL */
        false,     /* AllocKind::JITCODE */
        false,     /* AllocKind::SCOPE */
        false,     /* AllocKind::REGEXP_SHARED */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == size_t(AllocKind::LIMIT));
    return map[size_t(kind)];
}

static inline bool
IsBackgroundFinalized(AllocKind kind)
{
    MOZ_ASSERT(IsValidAllocKind(kind));
    static const bool map[] = {
        true,      /* AllocKind::FUNCTION */
        true,      /* AllocKind::FUNCTION_EXTENDED */
        false,     /* AllocKind::OBJECT0 */
        true,      /* AllocKind::OBJECT0_BACKGROUND */
        false,     /* AllocKind::OBJECT2 */
        true,      /* AllocKind::OBJECT2_BACKGROUND */
        false,     /* AllocKind::OBJECT4 */
        true,      /* AllocKind::OBJECT4_BACKGROUND */
        false,     /* AllocKind::OBJECT8 */
        true,      /* AllocKind::OBJECT8_BACKGROUND */
        false,     /* AllocKind::OBJECT12 */
        true,      /* AllocKind::OBJECT12_BACKGROUND */
        false,     /* AllocKind::OBJECT16 */
        true,      /* AllocKind::OBJECT16_BACKGROUND */
        false,     /* AllocKind::SCRIPT */
        true,      /* AllocKind::LAZY_SCRIPT */
        true,      /* AllocKind::SHAPE */
        true,      /* AllocKind::ACCESSOR_SHAPE */
        true,      /* AllocKind::BASE_SHAPE */
        true,      /* AllocKind::OBJECT_GROUP */
        true,      /* AllocKind::FAT_INLINE_STRING */
        true,      /* AllocKind::STRING */
        true,      /* AllocKind::EXTERNAL_STRING */
        true,      /* AllocKind::FAT_INLINE_ATOM */
        true,      /* AllocKind::ATOM */
        true,      /* AllocKind::SYMBOL */
        false,     /* AllocKind::JITCODE */
        true,      /* AllocKind::SCOPE */
        true,      /* AllocKind::REGEXP_SHARED */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == size_t(AllocKind::LIMIT));
    return map[size_t(kind)];
}

static inline bool
CanBeFinalizedInBackground(AllocKind kind, const Class* clasp)
{
    MOZ_ASSERT(IsObjectAllocKind(kind));
    /* If the class has no finalizer or a finalizer that is safe to call on
     * a different thread, we change the alloc kind. For example,
     * AllocKind::OBJECT0 calls the finalizer on the active thread,
     * AllocKind::OBJECT0_BACKGROUND calls the finalizer on the gcHelperThread.
     * IsBackgroundFinalized is called to prevent recursively incrementing
     * the alloc kind; kind may already be a background finalize kind.
     */
    return (!IsBackgroundFinalized(kind) &&
            (!clasp->hasFinalize() || (clasp->flags & JSCLASS_BACKGROUND_FINALIZE)));
}

/* Capacity for slotsToThingKind */
const size_t SLOTS_TO_THING_KIND_LIMIT = 17;

extern const AllocKind slotsToThingKind[];

/* Get the best kind to use when making an object with the given slot count. */
static inline AllocKind
GetGCObjectKind(size_t numSlots)
{
    if (numSlots >= SLOTS_TO_THING_KIND_LIMIT)
        return AllocKind::OBJECT16;
    return slotsToThingKind[numSlots];
}

/* As for GetGCObjectKind, but for dense array allocation. */
static inline AllocKind
GetGCArrayKind(size_t numElements)
{
    /*
     * Dense arrays can use their fixed slots to hold their elements array
     * (less two Values worth of ObjectElements header), but if more than the
     * maximum number of fixed slots is needed then the fixed slots will be
     * unused.
     */
    JS_STATIC_ASSERT(ObjectElements::VALUES_PER_HEADER == 2);
    if (numElements > NativeObject::MAX_DENSE_ELEMENTS_COUNT ||
        numElements + ObjectElements::VALUES_PER_HEADER >= SLOTS_TO_THING_KIND_LIMIT)
    {
        return AllocKind::OBJECT2;
    }
    return slotsToThingKind[numElements + ObjectElements::VALUES_PER_HEADER];
}

static inline AllocKind
GetGCObjectFixedSlotsKind(size_t numFixedSlots)
{
    MOZ_ASSERT(numFixedSlots < SLOTS_TO_THING_KIND_LIMIT);
    return slotsToThingKind[numFixedSlots];
}

// Get the best kind to use when allocating an object that needs a specific
// number of bytes.
static inline AllocKind
GetGCObjectKindForBytes(size_t nbytes)
{
    MOZ_ASSERT(nbytes <= JSObject::MAX_BYTE_SIZE);

    if (nbytes <= sizeof(NativeObject))
        return AllocKind::OBJECT0;
    nbytes -= sizeof(NativeObject);

    size_t dataSlots = AlignBytes(nbytes, sizeof(Value)) / sizeof(Value);
    MOZ_ASSERT(nbytes <= dataSlots * sizeof(Value));
    return GetGCObjectKind(dataSlots);
}

static inline AllocKind
GetBackgroundAllocKind(AllocKind kind)
{
    MOZ_ASSERT(!IsBackgroundFinalized(kind));
    MOZ_ASSERT(IsObjectAllocKind(kind));
    return AllocKind(size_t(kind) + 1);
}

/* Get the number of fixed slots and initial capacity associated with a kind. */
static inline size_t
GetGCKindSlots(AllocKind thingKind)
{
    /* Using a switch in hopes that thingKind will usually be a compile-time constant. */
    switch (thingKind) {
      case AllocKind::FUNCTION:
      case AllocKind::OBJECT0:
      case AllocKind::OBJECT0_BACKGROUND:
        return 0;
      case AllocKind::FUNCTION_EXTENDED:
      case AllocKind::OBJECT2:
      case AllocKind::OBJECT2_BACKGROUND:
        return 2;
      case AllocKind::OBJECT4:
      case AllocKind::OBJECT4_BACKGROUND:
        return 4;
      case AllocKind::OBJECT8:
      case AllocKind::OBJECT8_BACKGROUND:
        return 8;
      case AllocKind::OBJECT12:
      case AllocKind::OBJECT12_BACKGROUND:
        return 12;
      case AllocKind::OBJECT16:
      case AllocKind::OBJECT16_BACKGROUND:
        return 16;
      default:
        MOZ_CRASH("Bad object alloc kind");
    }
}

static inline size_t
GetGCKindSlots(AllocKind thingKind, const Class* clasp)
{
    size_t nslots = GetGCKindSlots(thingKind);

    /* An object's private data uses the space taken by its last fixed slot. */
    if (clasp->flags & JSCLASS_HAS_PRIVATE) {
        MOZ_ASSERT(nslots > 0);
        nslots--;
    }

    /*
     * Functions have a larger alloc kind than AllocKind::OBJECT to reserve
     * space for the extra fields in JSFunction, but have no fixed slots.
     */
    if (clasp == FunctionClassPtr)
        nslots = 0;

    return nslots;
}

static inline size_t
GetGCKindBytes(AllocKind thingKind)
{
    return sizeof(JSObject_Slots0) + GetGCKindSlots(thingKind) * sizeof(Value);
}

// Class to assist in triggering background chunk allocation. This cannot be done
// while holding the GC or worker thread state lock due to lock ordering issues.
// As a result, the triggering is delayed using this class until neither of the
// above locks is held.
class AutoMaybeStartBackgroundAllocation;

/*
 * A single segment of a SortedArenaList. Each segment has a head and a tail,
 * which track the start and end of a segment for O(1) append and concatenation.
 */
struct SortedArenaListSegment
{
    Arena* head;
    Arena** tailp;

    void clear() {
        head = nullptr;
        tailp = &head;
    }

    bool isEmpty() const {
        return tailp == &head;
    }

    // Appends |arena| to this segment.
    void append(Arena* arena) {
        MOZ_ASSERT(arena);
        MOZ_ASSERT_IF(head, head->getAllocKind() == arena->getAllocKind());
        *tailp = arena;
        tailp = &arena->next;
    }

    // Points the tail of this segment at |arena|, which may be null. Note
    // that this does not change the tail itself, but merely which arena
    // follows it. This essentially turns the tail into a cursor (see also the
    // description of ArenaList), but from the perspective of a SortedArenaList
    // this makes no difference.
    void linkTo(Arena* arena) {
        *tailp = arena;
    }
};

/*
 * Arena lists have a head and a cursor. The cursor conceptually lies on arena
 * boundaries, i.e. before the first arena, between two arenas, or after the
 * last arena.
 *
 * Arenas are usually sorted in order of increasing free space, with the cursor
 * following the Arena currently being allocated from. This ordering should not
 * be treated as an invariant, however, as the free lists may be cleared,
 * leaving arenas previously used for allocation partially full. Sorting order
 * is restored during sweeping.

 * Arenas following the cursor should not be full.
 */
class ArenaList {
    // The cursor is implemented via an indirect pointer, |cursorp_|, to allow
    // for efficient list insertion at the cursor point and other list
    // manipulations.
    //
    // - If the list is empty: |head| is null, |cursorp_| points to |head|, and
    //   therefore |*cursorp_| is null.
    //
    // - If the list is not empty: |head| is non-null, and...
    //
    //   - If the cursor is at the start of the list: |cursorp_| points to
    //     |head|, and therefore |*cursorp_| points to the first arena.
    //
    //   - If cursor is at the end of the list: |cursorp_| points to the |next|
    //     field of the last arena, and therefore |*cursorp_| is null.
    //
    //   - If the cursor is at neither the start nor the end of the list:
    //     |cursorp_| points to the |next| field of the arena preceding the
    //     cursor, and therefore |*cursorp_| points to the arena following the
    //     cursor.
    //
    // |cursorp_| is never null.
    //
    Arena* head_;
    Arena** cursorp_;

    void copy(const ArenaList& other) {
        other.check();
        head_ = other.head_;
        cursorp_ = other.isCursorAtHead() ? &head_ : other.cursorp_;
        check();
    }

  public:
    ArenaList() {
        clear();
    }

    ArenaList(const ArenaList& other) {
        copy(other);
    }

    ArenaList& operator=(const ArenaList& other) {
        copy(other);
        return *this;
    }

    explicit ArenaList(const SortedArenaListSegment& segment) {
        head_ = segment.head;
        cursorp_ = segment.isEmpty() ? &head_ : segment.tailp;
        check();
    }

    // This does checking just of |head_| and |cursorp_|.
    void check() const {
#ifdef DEBUG
        // If the list is empty, it must have this form.
        MOZ_ASSERT_IF(!head_, cursorp_ == &head_);

        // If there's an arena following the cursor, it must not be full.
        Arena* cursor = *cursorp_;
        MOZ_ASSERT_IF(cursor, cursor->hasFreeThings());
#endif
    }

    void clear() {
        head_ = nullptr;
        cursorp_ = &head_;
        check();
    }

    ArenaList copyAndClear() {
        ArenaList result = *this;
        clear();
        return result;
    }

    bool isEmpty() const {
        check();
        return !head_;
    }

    // This returns nullptr if the list is empty.
    Arena* head() const {
        check();
        return head_;
    }

    bool isCursorAtHead() const {
        check();
        return cursorp_ == &head_;
    }

    bool isCursorAtEnd() const {
        check();
        return !*cursorp_;
    }

    void moveCursorToEnd() {
        while (!isCursorAtEnd())
            cursorp_ = &(*cursorp_)->next;
    }

    // This can return nullptr.
    Arena* arenaAfterCursor() const {
        check();
        return *cursorp_;
    }

    // This returns the arena after the cursor and moves the cursor past it.
    Arena* takeNextArena() {
        check();
        Arena* arena = *cursorp_;
        if (!arena)
            return nullptr;
        cursorp_ = &arena->next;
        check();
        return arena;
    }

    // This does two things.
    // - Inserts |a| at the cursor.
    // - Leaves the cursor sitting just before |a|, if |a| is not full, or just
    //   after |a|, if |a| is full.
    void insertAtCursor(Arena* a) {
        check();
        a->next = *cursorp_;
        *cursorp_ = a;
        // At this point, the cursor is sitting before |a|. Move it after |a|
        // if necessary.
        if (!a->hasFreeThings())
            cursorp_ = &a->next;
        check();
    }

    // Inserts |a| at the cursor, then moves the cursor past it.
    void insertBeforeCursor(Arena* a) {
        check();
        a->next = *cursorp_;
        *cursorp_ = a;
        cursorp_ = &a->next;
        check();
    }

    // This inserts |other|, which must be full, at the cursor of |this|.
    ArenaList& insertListWithCursorAtEnd(const ArenaList& other) {
        check();
        other.check();
        MOZ_ASSERT(other.isCursorAtEnd());
        if (other.isCursorAtHead())
            return *this;
        // Insert the full arenas of |other| after those of |this|.
        *other.cursorp_ = *cursorp_;
        *cursorp_ = other.head_;
        cursorp_ = other.cursorp_;
        check();
        return *this;
    }

    Arena* removeRemainingArenas(Arena** arenap);
    Arena** pickArenasToRelocate(size_t& arenaTotalOut, size_t& relocTotalOut);
    Arena* relocateArenas(Arena* toRelocate, Arena* relocated,
                          SliceBudget& sliceBudget, gcstats::Statistics& stats);
};

/*
 * A class that holds arenas in sorted order by appending arenas to specific
 * segments. Each segment has a head and a tail, which can be linked up to
 * other segments to create a contiguous ArenaList.
 */
class SortedArenaList
{
  public:
    // The minimum size, in bytes, of a GC thing.
    static const size_t MinThingSize = 16;

    static_assert(ArenaSize <= 4096, "When increasing the Arena size, please consider how"\
                                     " this will affect the size of a SortedArenaList.");

    static_assert(MinThingSize >= 16, "When decreasing the minimum thing size, please consider"\
                                      " how this will affect the size of a SortedArenaList.");

  private:
    // The maximum number of GC things that an arena can hold.
    static const size_t MaxThingsPerArena = (ArenaSize - ArenaHeaderSize) / MinThingSize;

    size_t thingsPerArena_;
    SortedArenaListSegment segments[MaxThingsPerArena + 1];

    // Convenience functions to get the nth head and tail.
    Arena* headAt(size_t n) { return segments[n].head; }
    Arena** tailAt(size_t n) { return segments[n].tailp; }

  public:
    explicit SortedArenaList(size_t thingsPerArena = MaxThingsPerArena) {
        reset(thingsPerArena);
    }

    void setThingsPerArena(size_t thingsPerArena) {
        MOZ_ASSERT(thingsPerArena && thingsPerArena <= MaxThingsPerArena);
        thingsPerArena_ = thingsPerArena;
    }

    // Resets the first |thingsPerArena| segments of this list for further use.
    void reset(size_t thingsPerArena = MaxThingsPerArena) {
        setThingsPerArena(thingsPerArena);
        // Initialize the segments.
        for (size_t i = 0; i <= thingsPerArena; ++i)
            segments[i].clear();
    }

    // Inserts an arena, which has room for |nfree| more things, in its segment.
    void insertAt(Arena* arena, size_t nfree) {
        MOZ_ASSERT(nfree <= thingsPerArena_);
        segments[nfree].append(arena);
    }

    // Remove all empty arenas, inserting them as a linked list.
    void extractEmpty(Arena** empty) {
        SortedArenaListSegment& segment = segments[thingsPerArena_];
        if (segment.head) {
            *segment.tailp = *empty;
            *empty = segment.head;
            segment.clear();
        }
    }

    // Links up the tail of each non-empty segment to the head of the next
    // non-empty segment, creating a contiguous list that is returned as an
    // ArenaList. This is not a destructive operation: neither the head nor tail
    // of any segment is modified. However, note that the Arenas in the
    // resulting ArenaList should be treated as read-only unless the
    // SortedArenaList is no longer needed: inserting or removing arenas would
    // invalidate the SortedArenaList.
    ArenaList toArenaList() {
        // Link the non-empty segment tails up to the non-empty segment heads.
        size_t tailIndex = 0;
        for (size_t headIndex = 1; headIndex <= thingsPerArena_; ++headIndex) {
            if (headAt(headIndex)) {
                segments[tailIndex].linkTo(headAt(headIndex));
                tailIndex = headIndex;
            }
        }
        // Point the tail of the final non-empty segment at null. Note that if
        // the list is empty, this will just set segments[0].head to null.
        segments[tailIndex].linkTo(nullptr);
        // Create an ArenaList with head and cursor set to the head and tail of
        // the first segment (if that segment is empty, only the head is used).
        return ArenaList(segments[0]);
    }
};

enum ShouldCheckThresholds
{
    DontCheckThresholds = 0,
    CheckThresholds = 1
};

class ArenaLists
{
    JSRuntime* const runtime_;

    /*
     * For each arena kind its free list is represented as the first span with
     * free things. Initially all the spans are initialized as empty. After we
     * find a new arena with available things we move its first free span into
     * the list and set the arena as fully allocated. way we do not need to
     * update the arena after the initial allocation. When starting the
     * GC we only move the head of the of the list of spans back to the arena
     * only for the arena that was not fully allocated.
     */
    ZoneGroupData<AllAllocKindArray<FreeSpan*>> freeLists_;
    FreeSpan*& freeLists(AllocKind i) { return freeLists_.ref()[i]; }
    FreeSpan* freeLists(AllocKind i) const { return freeLists_.ref()[i]; }

    // Because the JITs can allocate from the free lists, they cannot be null.
    // We use a placeholder FreeSpan that is empty (and wihout an associated
    // Arena) so the JITs can fall back gracefully.
    static FreeSpan placeholder;

    ZoneGroupOrGCTaskData<AllAllocKindArray<ArenaList>> arenaLists_;
    ArenaList& arenaLists(AllocKind i) { return arenaLists_.ref()[i]; }
    const ArenaList& arenaLists(AllocKind i) const { return arenaLists_.ref()[i]; }

    enum BackgroundFinalizeStateEnum { BFS_DONE, BFS_RUN };

    typedef mozilla::Atomic<BackgroundFinalizeStateEnum, mozilla::SequentiallyConsistent>
        BackgroundFinalizeState;

    /* The current background finalization state, accessed atomically. */
    UnprotectedData<AllAllocKindArray<BackgroundFinalizeState>> backgroundFinalizeState_;
    BackgroundFinalizeState& backgroundFinalizeState(AllocKind i) { return backgroundFinalizeState_.ref()[i]; }
    const BackgroundFinalizeState& backgroundFinalizeState(AllocKind i) const { return backgroundFinalizeState_.ref()[i]; }

    /* For each arena kind, a list of arenas remaining to be swept. */
    ActiveThreadOrGCTaskData<AllAllocKindArray<Arena*>> arenaListsToSweep_;
    Arena*& arenaListsToSweep(AllocKind i) { return arenaListsToSweep_.ref()[i]; }
    Arena* arenaListsToSweep(AllocKind i) const { return arenaListsToSweep_.ref()[i]; }

    /* During incremental sweeping, a list of the arenas already swept. */
    ZoneGroupOrGCTaskData<AllocKind> incrementalSweptArenaKind;
    ZoneGroupOrGCTaskData<ArenaList> incrementalSweptArenas;

    // Arena lists which have yet to be swept, but need additional foreground
    // processing before they are swept.
    ZoneGroupData<Arena*> gcShapeArenasToUpdate;
    ZoneGroupData<Arena*> gcAccessorShapeArenasToUpdate;
    ZoneGroupData<Arena*> gcScriptArenasToUpdate;
    ZoneGroupData<Arena*> gcObjectGroupArenasToUpdate;

    // While sweeping type information, these lists save the arenas for the
    // objects which have already been finalized in the foreground (which must
    // happen at the beginning of the GC), so that type sweeping can determine
    // which of the object pointers are marked.
    ZoneGroupData<ObjectAllocKindArray<ArenaList>> savedObjectArenas_;
    ArenaList& savedObjectArenas(AllocKind i) { return savedObjectArenas_.ref()[i]; }
    ZoneGroupData<Arena*> savedEmptyObjectArenas;

  public:
    explicit ArenaLists(JSRuntime* rt, ZoneGroup* group);
    ~ArenaLists();

    const void* addressOfFreeList(AllocKind thingKind) const {
        return reinterpret_cast<const void*>(&freeLists_.refNoCheck()[thingKind]);
    }

    Arena* getFirstArena(AllocKind thingKind) const {
        return arenaLists(thingKind).head();
    }

    Arena* getFirstArenaToSweep(AllocKind thingKind) const {
        return arenaListsToSweep(thingKind);
    }

    Arena* getFirstSweptArena(AllocKind thingKind) const {
        if (thingKind != incrementalSweptArenaKind.ref())
            return nullptr;
        return incrementalSweptArenas.ref().head();
    }

    Arena* getArenaAfterCursor(AllocKind thingKind) const {
        return arenaLists(thingKind).arenaAfterCursor();
    }

    bool arenaListsAreEmpty() const {
        for (auto i : AllAllocKinds()) {
            /*
             * The arena cannot be empty if the background finalization is not yet
             * done.
             */
            if (backgroundFinalizeState(i) != BFS_DONE)
                return false;
            if (!arenaLists(i).isEmpty())
                return false;
        }
        return true;
    }

    void unmarkAll() {
        for (auto i : AllAllocKinds()) {
            /* The background finalization must have stopped at this point. */
            MOZ_ASSERT(backgroundFinalizeState(i) == BFS_DONE);
            for (Arena* arena = arenaLists(i).head(); arena; arena = arena->next)
                arena->unmarkAll();
        }
    }

    bool doneBackgroundFinalize(AllocKind kind) const {
        return backgroundFinalizeState(kind) == BFS_DONE;
    }

    bool needBackgroundFinalizeWait(AllocKind kind) const {
        return backgroundFinalizeState(kind) != BFS_DONE;
    }

    /*
     * Clear the free lists so we won't try to allocate from swept arenas.
     */
    void purge() {
        for (auto i : AllAllocKinds())
            freeLists(i) = &placeholder;
    }

    inline void prepareForIncrementalGC();

    /* Check if this arena is in use. */
    bool arenaIsInUse(Arena* arena, AllocKind kind) const {
        MOZ_ASSERT(arena);
        return arena == freeLists(kind)->getArenaUnchecked();
    }

    MOZ_ALWAYS_INLINE TenuredCell* allocateFromFreeList(AllocKind thingKind, size_t thingSize) {
        return freeLists(thingKind)->allocate(thingSize);
    }

    /*
     * Moves all arenas from |fromArenaLists| into |this|.
     */
    void adoptArenas(JSRuntime* runtime, ArenaLists* fromArenaLists, bool targetZoneIsCollecting);

    /* True if the Arena in question is found in this ArenaLists */
    bool containsArena(JSRuntime* runtime, Arena* arena);

    void checkEmptyFreeLists() {
#ifdef DEBUG
        for (auto i : AllAllocKinds())
            checkEmptyFreeList(i);
#endif
    }

    bool checkEmptyArenaLists() {
        bool empty = true;
#ifdef DEBUG
        for (auto i : AllAllocKinds()) {
            if (!checkEmptyArenaList(i))
                empty = false;
        }
#endif
        return empty;
    }

    void checkEmptyFreeList(AllocKind kind) {
        MOZ_ASSERT(freeLists(kind)->isEmpty());
    }

    bool checkEmptyArenaList(AllocKind kind);

    bool relocateArenas(Zone* zone, Arena*& relocatedListOut, JS::gcreason::Reason reason,
                        SliceBudget& sliceBudget, gcstats::Statistics& stats);

    void queueForegroundObjectsForSweep(FreeOp* fop);
    void queueForegroundThingsForSweep(FreeOp* fop);

    void mergeForegroundSweptObjectArenas();

    bool foregroundFinalize(FreeOp* fop, AllocKind thingKind, SliceBudget& sliceBudget,
                            SortedArenaList& sweepList);
    static void backgroundFinalize(FreeOp* fop, Arena* listHead, Arena** empty);

    // When finalizing arenas, whether to keep empty arenas on the list or
    // release them immediately.
    enum KeepArenasEnum {
        RELEASE_ARENAS,
        KEEP_ARENAS
    };

  private:
    inline void queueForForegroundSweep(FreeOp* fop, const FinalizePhase& phase);
    inline void queueForBackgroundSweep(FreeOp* fop, const FinalizePhase& phase);
    inline void queueForForegroundSweep(FreeOp* fop, AllocKind thingKind);
    inline void queueForBackgroundSweep(FreeOp* fop, AllocKind thingKind);
    inline void mergeSweptArenas(AllocKind thingKind);

    TenuredCell* allocateFromArena(JS::Zone* zone, AllocKind thingKind,
                                   ShouldCheckThresholds checkThresholds,
                                   AutoMaybeStartBackgroundAllocation& maybeStartBGAlloc);
    inline TenuredCell* allocateFromArenaInner(JS::Zone* zone, Arena* arena, AllocKind kind);

    friend class GCRuntime;
    friend class js::Nursery;
    friend class js::TenuringTracer;
};

/* The number of GC cycles an empty chunk can survive before been released. */
const size_t MAX_EMPTY_CHUNK_AGE = 4;

extern bool
InitializeStaticData();

} /* namespace gc */

class InterpreterFrame;

extern void
TraceRuntime(JSTracer* trc);

extern void
ReleaseAllJITCode(FreeOp* op);

extern void
PrepareForDebugGC(JSRuntime* rt);

/* Functions for managing cross compartment gray pointers. */

extern void
DelayCrossCompartmentGrayMarking(JSObject* src);

extern void
NotifyGCNukeWrapper(JSObject* o);

extern unsigned
NotifyGCPreSwap(JSObject* a, JSObject* b);

extern void
NotifyGCPostSwap(JSObject* a, JSObject* b, unsigned preResult);

/*
 * Helper state for use when JS helper threads sweep and allocate GC thing kinds
 * that can be swept and allocated off thread.
 *
 * In non-threadsafe builds, all actual sweeping and allocation is performed
 * on the active thread, but GCHelperState encapsulates this from clients as
 * much as possible.
 */
class GCHelperState
{
    enum State {
        IDLE,
        SWEEPING
    };

    // Associated runtime.
    JSRuntime* const rt;

    // Condvar for notifying the active thread when work has finished. This is
    // associated with the runtime's GC lock --- the worker thread state
    // condvars can't be used here due to lock ordering issues.
    js::ConditionVariable done;

    // Activity for the helper to do, protected by the GC lock.
    ActiveThreadOrGCTaskData<State> state_;

    // Whether work is being performed on some thread.
    GCLockData<bool> hasThread;

    void startBackgroundThread(State newState, const AutoLockGC& lock,
                               const AutoLockHelperThreadState& helperLock);
    void waitForBackgroundThread(js::AutoLockGC& lock);

    State state(const AutoLockGC&);
    void setState(State state, const AutoLockGC&);

    friend class js::gc::ArenaLists;

    static void freeElementsAndArray(void** array, void** end) {
        MOZ_ASSERT(array <= end);
        for (void** p = array; p != end; ++p)
            js_free(*p);
        js_free(array);
    }

    void doSweep(AutoLockGC& lock);

  public:
    explicit GCHelperState(JSRuntime* rt)
      : rt(rt),
        done(),
        state_(IDLE)
    { }

    JSRuntime* runtime() { return rt; }

    void finish();

    void work();

    void maybeStartBackgroundSweep(const AutoLockGC& lock,
                                   const AutoLockHelperThreadState& helperLock);
    void startBackgroundShrink(const AutoLockGC& lock);

    /* Must be called without the GC lock taken. */
    void waitBackgroundSweepEnd();

#ifdef DEBUG
    bool onBackgroundThread();
#endif

    /*
     * Outside the GC lock may give true answer when in fact the sweeping has
     * been done.
     */
    bool isBackgroundSweeping() const {
        return state_ == SWEEPING;
    }
};

// A generic task used to dispatch work to the helper thread system.
// Users should derive from GCParallelTask add what data they need and
// override |run|.
class GCParallelTask
{
    JSRuntime* const runtime_;

    // The state of the parallel computation.
    enum TaskState {
        NotStarted,
        Dispatched,
        Finished,
    };
    UnprotectedData<TaskState> state;

    // Amount of time this task took to execute.
    ActiveThreadOrGCTaskData<mozilla::TimeDuration> duration_;

    explicit GCParallelTask(const GCParallelTask&) = delete;

  protected:
    // A flag to signal a request for early completion of the off-thread task.
    mozilla::Atomic<bool> cancel_;

    virtual void run() = 0;

  public:
    explicit GCParallelTask(JSRuntime* runtime) : runtime_(runtime), state(NotStarted), duration_(nullptr) {}
    GCParallelTask(GCParallelTask&& other)
      : runtime_(other.runtime_),
        state(other.state),
        duration_(nullptr),
        cancel_(false)
    {}

    // Derived classes must override this to ensure that join() gets called
    // before members get destructed.
    virtual ~GCParallelTask();

    JSRuntime* runtime() { return runtime_; }

    // Time spent in the most recent invocation of this task.
    mozilla::TimeDuration duration() const { return duration_; }

    // The simple interface to a parallel task works exactly like pthreads.
    bool start();
    void join();

    // If multiple tasks are to be started or joined at once, it is more
    // efficient to take the helper thread lock once and use these methods.
    bool startWithLockHeld(AutoLockHelperThreadState& locked);
    void joinWithLockHeld(AutoLockHelperThreadState& locked);

    // Instead of dispatching to a helper, run the task on the current thread.
    void runFromActiveCooperatingThread(JSRuntime* rt);

    // Dispatch a cancelation request.
    enum CancelMode { CancelNoWait, CancelAndWait};
    void cancel(CancelMode mode = CancelNoWait) {
        cancel_ = true;
        if (mode == CancelAndWait)
            join();
    }

    // Check if a task is actively running.
    bool isRunningWithLockHeld(const AutoLockHelperThreadState& locked) const;
    bool isRunning() const;

    // This should be friended to HelperThread, but cannot be because it
    // would introduce several circular dependencies.
  public:
    void runFromHelperThread(AutoLockHelperThreadState& locked);
};

typedef void (*IterateChunkCallback)(JSRuntime* rt, void* data, gc::Chunk* chunk);
typedef void (*IterateZoneCallback)(JSRuntime* rt, void* data, JS::Zone* zone);
typedef void (*IterateArenaCallback)(JSRuntime* rt, void* data, gc::Arena* arena,
                                     JS::TraceKind traceKind, size_t thingSize);
typedef void (*IterateCellCallback)(JSRuntime* rt, void* data, void* thing,
                                    JS::TraceKind traceKind, size_t thingSize);

/*
 * This function calls |zoneCallback| on every zone, |compartmentCallback| on
 * every compartment, |arenaCallback| on every in-use arena, and |cellCallback|
 * on every in-use cell in the GC heap.
 *
 * Note that no read barrier is triggered on the cells passed to cellCallback,
 * so no these pointers must not escape the callback.
 */
extern void
IterateHeapUnbarriered(JSContext* cx, void* data,
                       IterateZoneCallback zoneCallback,
                       JSIterateCompartmentCallback compartmentCallback,
                       IterateArenaCallback arenaCallback,
                       IterateCellCallback cellCallback);

/*
 * This function is like IterateZonesCompartmentsArenasCells, but does it for a
 * single zone.
 */
extern void
IterateHeapUnbarrieredForZone(JSContext* cx, Zone* zone, void* data,
                              IterateZoneCallback zoneCallback,
                              JSIterateCompartmentCallback compartmentCallback,
                              IterateArenaCallback arenaCallback,
                              IterateCellCallback cellCallback);

/*
 * Invoke chunkCallback on every in-use chunk.
 */
extern void
IterateChunks(JSContext* cx, void* data, IterateChunkCallback chunkCallback);

typedef void (*IterateScriptCallback)(JSRuntime* rt, void* data, JSScript* script);

/*
 * Invoke scriptCallback on every in-use script for
 * the given compartment or for all compartments if it is null.
 */
extern void
IterateScripts(JSContext* cx, JSCompartment* compartment,
               void* data, IterateScriptCallback scriptCallback);

extern void
FinalizeStringRT(JSRuntime* rt, JSString* str);

JSCompartment*
NewCompartment(JSContext* cx, JSPrincipals* principals,
               const JS::CompartmentOptions& options);

namespace gc {

/*
 * Merge all contents of source into target. This can only be used if source is
 * the only compartment in its zone.
 */
void
MergeCompartments(JSCompartment* source, JSCompartment* target);

/*
 * This structure overlays a Cell in the Nursery and re-purposes its memory
 * for managing the Nursery collection process.
 */
class RelocationOverlay
{
    /* The low bit is set so this should never equal a normal pointer. */
    static const uintptr_t Relocated = uintptr_t(0xbad0bad1);

    /* Set to Relocated when moved. */
    uintptr_t magic_;

    /* The location |this| was moved to. */
    Cell* newLocation_;

    /* A list entry to track all relocated things. */
    RelocationOverlay* next_;

  public:
    static RelocationOverlay* fromCell(Cell* cell) {
        return reinterpret_cast<RelocationOverlay*>(cell);
    }

    bool isForwarded() const {
        return magic_ == Relocated;
    }

    Cell* forwardingAddress() const {
        MOZ_ASSERT(isForwarded());
        return newLocation_;
    }

    void forwardTo(Cell* cell);

    RelocationOverlay*& nextRef() {
        MOZ_ASSERT(isForwarded());
        return next_;
    }

    RelocationOverlay* next() const {
        MOZ_ASSERT(isForwarded());
        return next_;
    }

    static bool isCellForwarded(Cell* cell) {
        return fromCell(cell)->isForwarded();
    }
};

// Functions for checking and updating GC thing pointers that might have been
// moved by compacting GC. Overloads are also provided that work with Values.
//
// IsForwarded    - check whether a pointer refers to an GC thing that has been
//                  moved.
//
// Forwarded      - return a pointer to the new location of a GC thing given a
//                  pointer to old location.
//
// MaybeForwarded - used before dereferencing a pointer that may refer to a
//                  moved GC thing without updating it. For JSObjects this will
//                  also update the object's shape pointer if it has been moved
//                  to allow slots to be accessed.

template <typename T>
inline bool IsForwarded(T* t);
inline bool IsForwarded(const JS::Value& value);

template <typename T>
inline T* Forwarded(T* t);

inline Value Forwarded(const JS::Value& value);

template <typename T>
inline T MaybeForwarded(T t);

#ifdef JSGC_HASH_TABLE_CHECKS

template <typename T>
inline bool IsGCThingValidAfterMovingGC(T* t);

template <typename T>
inline void CheckGCThingAfterMovingGC(T* t);

template <typename T>
inline void CheckGCThingAfterMovingGC(const ReadBarriered<T*>& t);

inline void CheckValueAfterMovingGC(const JS::Value& value);

#endif // JSGC_HASH_TABLE_CHECKS

#define JS_FOR_EACH_ZEAL_MODE(D)               \
            D(RootsChange, 1)                  \
            D(Alloc, 2)                        \
            D(FrameGC, 3)                      \
            D(VerifierPre, 4)                  \
            D(FrameVerifierPre, 5)             \
            D(GenerationalGC, 7)               \
            D(IncrementalRootsThenFinish, 8)   \
            D(IncrementalMarkAllThenFinish, 9) \
            D(IncrementalMultipleSlices, 10)   \
            D(IncrementalMarkingValidator, 11) \
            D(ElementsBarrier, 12)             \
            D(CheckHashTablesOnMinorGC, 13)    \
            D(Compact, 14)                     \
            D(CheckHeapAfterGC, 15)            \
            D(CheckNursery, 16)                \
            D(IncrementalSweepThenFinish, 17)

enum class ZealMode {
#define ZEAL_MODE(name, value) name = value,
    JS_FOR_EACH_ZEAL_MODE(ZEAL_MODE)
#undef ZEAL_MODE
    Limit = 17
};

enum VerifierType {
    PreBarrierVerifier
};

#ifdef JS_GC_ZEAL

extern const char* ZealModeHelpText;

/* Check that write barriers have been used correctly. See jsgc.cpp. */
void
VerifyBarriers(JSRuntime* rt, VerifierType type);

void
MaybeVerifyBarriers(JSContext* cx, bool always = false);

void DumpArenaInfo();

#else

static inline void
VerifyBarriers(JSRuntime* rt, VerifierType type)
{
}

static inline void
MaybeVerifyBarriers(JSContext* cx, bool always = false)
{
}

#endif

/*
 * Instances of this class set the |JSRuntime::suppressGC| flag for the duration
 * that they are live. Use of this class is highly discouraged. Please carefully
 * read the comment in vm/Runtime.h above |suppressGC| and take all appropriate
 * precautions before instantiating this class.
 */
class MOZ_RAII JS_HAZ_GC_SUPPRESSED AutoSuppressGC
{
    int32_t& suppressGC_;

  public:
    explicit AutoSuppressGC(JSContext* cx);

    ~AutoSuppressGC()
    {
        suppressGC_--;
    }
};

// A singly linked list of zones.
class ZoneList
{
    static Zone * const End;

    Zone* head;
    Zone* tail;

  public:
    ZoneList();
    ~ZoneList();

    bool isEmpty() const;
    Zone* front() const;

    void append(Zone* zone);
    void transferFrom(ZoneList& other);
    void removeFront();
    void clear();

  private:
    explicit ZoneList(Zone* singleZone);
    void check() const;

    ZoneList(const ZoneList& other) = delete;
    ZoneList& operator=(const ZoneList& other) = delete;
};

JSObject*
NewMemoryStatisticsObject(JSContext* cx);

struct MOZ_RAII AutoAssertNoNurseryAlloc
{
#ifdef DEBUG
    AutoAssertNoNurseryAlloc();
    ~AutoAssertNoNurseryAlloc();
#else
    AutoAssertNoNurseryAlloc() {}
#endif
};

/*
 * There are a couple of classes here that serve mostly as "tokens" indicating
 * that a condition holds. Some functions force the caller to possess such a
 * token because they would misbehave if the condition were false, and it is
 * far more clear to make the condition visible at the point where it can be
 * affected rather than just crashing in an assertion down in the place where
 * it is relied upon.
 */

/*
 * Token meaning that the heap is busy and no allocations will be made.
 *
 * This class may be instantiated directly if it is known that the condition is
 * already true, or it can be used as a base class for another RAII class that
 * causes the condition to become true. Such base classes will use the no-arg
 * constructor, establish the condition, then call checkCondition() to assert
 * it and possibly record data needed to re-check the condition during
 * destruction.
 *
 * Ordinarily, you would do something like this with a Maybe<> member that is
 * emplaced during the constructor, but token-requiring functions want to
 * require a reference to a base class instance. That said, you can always pass
 * in the Maybe<> field as the token.
 */
class MOZ_RAII AutoAssertHeapBusy {
  protected:
    JSRuntime* rt;

    // Check that the heap really is busy, and record the rt for the check in
    // the destructor.
    void checkCondition(JSRuntime *rt);

    AutoAssertHeapBusy() : rt(nullptr) {
    }

  public:
    explicit AutoAssertHeapBusy(JSRuntime* rt) {
        checkCondition(rt);
    }

    ~AutoAssertHeapBusy() {
        MOZ_ASSERT(rt); // checkCondition must always be called.
        checkCondition(rt);
    }
};

/*
 * A class that serves as a token that the nursery in the current thread's zone
 * group is empty.
 */
class MOZ_RAII AutoAssertEmptyNursery
{
  protected:
    JSContext* cx;

    mozilla::Maybe<AutoAssertNoNurseryAlloc> noAlloc;

    // Check that the nursery is empty.
    void checkCondition(JSContext* cx);

    // For subclasses that need to empty the nursery in their constructors.
    AutoAssertEmptyNursery() : cx(nullptr) {
    }

  public:
    explicit AutoAssertEmptyNursery(JSContext* cx) : cx(nullptr) {
        checkCondition(cx);
    }

    AutoAssertEmptyNursery(const AutoAssertEmptyNursery& other) : AutoAssertEmptyNursery(other.cx)
    {
    }
};

/*
 * Evict the nursery upon construction. Serves as a token indicating that the
 * nursery is empty. (See AutoAssertEmptyNursery, above.)
 *
 * Note that this is very improper subclass of AutoAssertHeapBusy, in that the
 * heap is *not* busy within the scope of an AutoEmptyNursery. I will most
 * likely fix this by removing AutoAssertHeapBusy, but that is currently
 * waiting on jonco's review.
 */
class MOZ_RAII AutoEmptyNursery : public AutoAssertEmptyNursery
{
  public:
    explicit AutoEmptyNursery(JSContext* cx);
};

const char*
StateName(State state);

inline bool
IsOOMReason(JS::gcreason::Reason reason)
{
    return reason == JS::gcreason::LAST_DITCH ||
           reason == JS::gcreason::MEM_PRESSURE;
}

} /* namespace gc */

/* Use this to avoid assertions when manipulating the wrapper map. */
class MOZ_RAII AutoDisableProxyCheck
{
  public:
#ifdef DEBUG
    AutoDisableProxyCheck();
    ~AutoDisableProxyCheck();
#else
    AutoDisableProxyCheck() {}
#endif
};

struct MOZ_RAII AutoDisableCompactingGC
{
    explicit AutoDisableCompactingGC(JSContext* cx);
    ~AutoDisableCompactingGC();

  private:
    JSContext* cx;
};

// This is the same as IsInsideNursery, but not inlined.
bool
UninlinedIsInsideNursery(const gc::Cell* cell);

} /* namespace js */

#endif /* jsgc_h */
