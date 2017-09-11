/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation, see jit/AtomicOperations.h */

#ifndef jit_arm64_AtomicOperations_arm64_h
#define jit_arm64_AtomicOperations_arm64_h

#include "mozilla/Assertions.h"
#include "mozilla/Types.h"

#if !defined(__clang__) && !defined(__GNUC__)
# error "This file only for gcc-compatible compilers"
#endif

inline bool
js::jit::AtomicOperations::hasAtomic8()
{
    return true;
}

inline bool
js::jit::AtomicOperations::isLockfree8()
{
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int8_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int16_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int32_t), 0));
    MOZ_ASSERT(__atomic_always_lock_free(sizeof(int64_t), 0));
    return true;
}

inline void
js::jit::AtomicOperations::fenceSeqCst()
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::loadSeqCst(T* addr)
{
    MOZ_ASSERT(tier1Constraints(addr));
    T v;
    __atomic_load(addr, &v, __ATOMIC_SEQ_CST);
    return v;
}

template<typename T>
inline void
js::jit::AtomicOperations::storeSeqCst(T* addr, T val)
{
    MOZ_ASSERT(tier1Constraints(addr));
    __atomic_store(addr, &val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::exchangeSeqCst(T* addr, T val)
{
    MOZ_ASSERT(tier1Constraints(addr));
    T v;
    __atomic_exchange(addr, &val, &v, __ATOMIC_SEQ_CST);
    return v;
}

template<typename T>
inline T
js::jit::AtomicOperations::compareExchangeSeqCst(T* addr, T oldval, T newval)
{
    MOZ_ASSERT(tier1Constraints(addr));
    __atomic_compare_exchange(addr, &oldval, &newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return oldval;
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAddSeqCst(T* addr, T val)
{
    MOZ_ASSERT(tier1Constraints(addr));
    return __atomic_fetch_add(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchSubSeqCst(T* addr, T val)
{
    MOZ_ASSERT(tier1Constraints(addr));
    return __atomic_fetch_sub(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchAndSeqCst(T* addr, T val)
{
    MOZ_ASSERT(tier1Constraints(addr));
    return __atomic_fetch_and(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchOrSeqCst(T* addr, T val)
{
    MOZ_ASSERT(tier1Constraints(addr));
    return __atomic_fetch_or(addr, val, __ATOMIC_SEQ_CST);
}

template<typename T>
inline T
js::jit::AtomicOperations::fetchXorSeqCst(T* addr, T val)
{
    MOZ_ASSERT(tier1Constraints(addr));
    return __atomic_fetch_xor(addr, val, __ATOMIC_SEQ_CST);
}

template <typename T>
inline T
js::jit::AtomicOperations::loadSafeWhenRacy(T* addr)
{
    MOZ_ASSERT(tier1Constraints(addr));
    T v;
    __atomic_load(addr, &v, __ATOMIC_RELAXED);
    return v;
}

template <typename T>
inline void
js::jit::AtomicOperations::storeSafeWhenRacy(T* addr, T val)
{
    MOZ_ASSERT(tier1Constraints(addr));
    __atomic_store(addr, &val, __ATOMIC_RELAXED);
}

inline void
js::jit::AtomicOperations::memcpySafeWhenRacy(void* dest, const void* src, size_t nbytes)
{
    MOZ_ASSERT(!((char*)dest <= (char*)src && (char*)src < (char*)dest+nbytes));
    MOZ_ASSERT(!((char*)src <= (char*)dest && (char*)dest < (char*)src+nbytes));
    memcpy(dest, src, nbytes);
}

inline void
js::jit::AtomicOperations::memmoveSafeWhenRacy(void* dest, const void* src,
                                               size_t nbytes)
{
    memmove(dest, src, nbytes);
}

template<size_t nbytes>
inline void
js::jit::RegionLock::acquire(void* addr)
{
    uint32_t zero = 0;
    uint32_t one = 1;
    while (!__atomic_compare_exchange(&spinlock, &zero, &one, false, __ATOMIC_ACQUIRE,
                                      __ATOMIC_ACQUIRE))
    {
        zero = 0;
        continue;
    }
}

template<size_t nbytes>
inline void
js::jit::RegionLock::release(void* addr)
{
    MOZ_ASSERT(AtomicOperations::loadSeqCst(&spinlock) == 1, "releasing unlocked region lock");
    uint32_t zero = 0;
    __atomic_store(&spinlock, &zero, __ATOMIC_SEQ_CST);
}

#endif // jit_arm64_AtomicOperations_arm64_h
