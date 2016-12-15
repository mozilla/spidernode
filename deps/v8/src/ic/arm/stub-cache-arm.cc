// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

static void ProbeTable(StubCache* stub_cache, MacroAssembler* masm,
                       StubCache::Table table, Register receiver, Register name,
                       // The offset is scaled by 4, based on
                       // kCacheIndexShift, which is two bits
                       Register offset, Register scratch, Register scratch2,
                       Register offset_scratch) {
  ExternalReference key_offset(stub_cache->key_reference(table));
  ExternalReference value_offset(stub_cache->value_reference(table));
  ExternalReference map_offset(stub_cache->map_reference(table));

  uint32_t key_off_addr = reinterpret_cast<uint32_t>(key_offset.address());
  uint32_t value_off_addr = reinterpret_cast<uint32_t>(value_offset.address());
  uint32_t map_off_addr = reinterpret_cast<uint32_t>(map_offset.address());

  // Check the relative positions of the address fields.
  DCHECK(value_off_addr > key_off_addr);
  DCHECK((value_off_addr - key_off_addr) % 4 == 0);
  DCHECK((value_off_addr - key_off_addr) < (256 * 4));
  DCHECK(map_off_addr > key_off_addr);
  DCHECK((map_off_addr - key_off_addr) % 4 == 0);
  DCHECK((map_off_addr - key_off_addr) < (256 * 4));

  Label miss;
  Register base_addr = scratch;
  scratch = no_reg;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ add(offset_scratch, offset, Operand(offset, LSL, 1));

  // Calculate the base address of the entry.
  __ add(base_addr, offset_scratch, Operand(key_offset));

  // Check that the key in the entry matches the name.
  __ ldr(ip, MemOperand(base_addr, 0));
  __ cmp(name, ip);
  __ b(ne, &miss);

  // Check the map matches.
  __ ldr(ip, MemOperand(base_addr, map_off_addr - key_off_addr));
  __ ldr(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ cmp(ip, scratch2);
  __ b(ne, &miss);

  // Get the code entry from the cache.
  Register code = scratch2;
  scratch2 = no_reg;
  __ ldr(code, MemOperand(base_addr, value_off_addr - key_off_addr));

#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    __ jmp(&miss);
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    __ jmp(&miss);
  }
#endif

  // Jump to the first instruction in the code stub.
  __ add(pc, code, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Miss: fall through.
  __ bind(&miss);
}

void StubCache::GenerateProbe(MacroAssembler* masm, Register receiver,
                              Register name, Register scratch, Register extra,
                              Register extra2, Register extra3) {
  Label miss;

  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 12.
  DCHECK(sizeof(Entry) == 12);

  // Make sure that there are no register conflicts.
  DCHECK(!AreAliased(receiver, name, scratch, extra, extra2, extra3));

  // Check scratch, extra and extra2 registers are valid.
  DCHECK(!scratch.is(no_reg));
  DCHECK(!extra.is(no_reg));
  DCHECK(!extra2.is(no_reg));
  DCHECK(!extra3.is(no_reg));

#ifdef DEBUG
  // If vector-based ics are in use, ensure that scratch, extra, extra2 and
  // extra3 don't conflict with the vector and slot registers, which need
  // to be preserved for a handler call or miss.
  if (IC::ICUseVector(ic_kind_)) {
    Register vector, slot;
    if (ic_kind_ == Code::STORE_IC || ic_kind_ == Code::KEYED_STORE_IC) {
      vector = StoreWithVectorDescriptor::VectorRegister();
      slot = StoreWithVectorDescriptor::SlotRegister();
    } else {
      DCHECK(ic_kind_ == Code::LOAD_IC || ic_kind_ == Code::KEYED_LOAD_IC);
      vector = LoadWithVectorDescriptor::VectorRegister();
      slot = LoadWithVectorDescriptor::SlotRegister();
    }
    DCHECK(!AreAliased(vector, slot, scratch, extra, extra2, extra3));
  }
#endif

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1, extra2,
                      extra3);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ ldr(scratch, FieldMemOperand(name, Name::kHashFieldOffset));
  __ ldr(ip, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ add(scratch, scratch, Operand(ip));
  __ eor(scratch, scratch, Operand(kPrimaryMagic));
  __ mov(ip, Operand(kPrimaryTableSize - 1));
  __ and_(scratch, scratch, Operand(ip, LSL, kCacheIndexShift));

  // Probe the primary table.
  ProbeTable(this, masm, kPrimary, receiver, name, scratch, extra, extra2,
             extra3);

  // Primary miss: Compute hash for secondary probe.
  __ sub(scratch, scratch, Operand(name));
  __ add(scratch, scratch, Operand(kSecondaryMagic));
  __ mov(ip, Operand(kSecondaryTableSize - 1));
  __ and_(scratch, scratch, Operand(ip, LSL, kCacheIndexShift));

  // Probe the secondary table.
  ProbeTable(this, masm, kSecondary, receiver, name, scratch, extra, extra2,
             extra3);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1, extra2,
                      extra3);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
