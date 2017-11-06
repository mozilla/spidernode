/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * GC-internal iterators for various data structures.
 */

#ifndef gc_Iteration_inl_h
#define gc_Iteration_inl_h

#include "jsgcinlines.h"

namespace js {
namespace gc {

class ArenaCellIterUnderGC : public ArenaCellIterImpl
{
  public:
    explicit ArenaCellIterUnderGC(Arena* arena)
      : ArenaCellIterImpl(arena, CellIterDoesntNeedBarrier)
    {
        MOZ_ASSERT(CurrentThreadIsPerformingGC());
    }
};

class ArenaCellIterUnderFinalize : public ArenaCellIterImpl
{
  public:
    explicit ArenaCellIterUnderFinalize(Arena* arena)
      : ArenaCellIterImpl(arena, CellIterDoesntNeedBarrier)
    {
        MOZ_ASSERT(CurrentThreadIsGCSweeping());
    }
};

class ArenaCellIterUnbarriered : public ArenaCellIterImpl
{
  public:
    explicit ArenaCellIterUnbarriered(Arena* arena)
      : ArenaCellIterImpl(arena, CellIterDoesntNeedBarrier)
    {}
};

class GrayObjectIter : public ZoneCellIter<js::gc::TenuredCell> {
  public:
    explicit GrayObjectIter(JS::Zone* zone, AllocKind kind) : ZoneCellIter<js::gc::TenuredCell>() {
        initForTenuredIteration(zone, kind);
    }

    JSObject* get() const { return ZoneCellIter<js::gc::TenuredCell>::get<JSObject>(); }
    operator JSObject*() const { return get(); }
    JSObject* operator ->() const { return get(); }
};

class GCZonesIter
{
  private:
    ZonesIter zone;

  public:
    explicit GCZonesIter(JSRuntime* rt, ZoneSelector selector = WithAtoms) : zone(rt, selector) {
        MOZ_ASSERT(JS::CurrentThreadIsHeapBusy());
        if (!zone->isCollectingFromAnyThread())
            next();
    }

    bool done() const { return zone.done(); }

    void next() {
        MOZ_ASSERT(!done());
        do {
            zone.next();
        } while (!zone.done() && !zone->isCollectingFromAnyThread());
    }

    JS::Zone* get() const {
        MOZ_ASSERT(!done());
        return zone;
    }

    operator JS::Zone*() const { return get(); }
    JS::Zone* operator->() const { return get(); }
};

typedef CompartmentsIterT<GCZonesIter> GCCompartmentsIter;

/* Iterates over all zones in the current sweep group. */
class SweepGroupZonesIter {
    JS::Zone* current;

  public:
    explicit SweepGroupZonesIter(JSRuntime* rt) {
        MOZ_ASSERT(CurrentThreadIsPerformingGC());
        current = rt->gc.getCurrentSweepGroup();
    }

    bool done() const { return !current; }

    void next() {
        MOZ_ASSERT(!done());
        current = current->nextNodeInGroup();
    }

    JS::Zone* get() const {
        MOZ_ASSERT(!done());
        return current;
    }

    operator JS::Zone*() const { return get(); }
    JS::Zone* operator->() const { return get(); }
};

typedef CompartmentsIterT<SweepGroupZonesIter> SweepGroupCompartmentsIter;

} // namespace gc
} // namespace js

#endif // gc_Iteration_h
