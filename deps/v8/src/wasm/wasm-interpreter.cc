// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "src/wasm/wasm-interpreter.h"

#include "src/assembler-inl.h"
#include "src/conversions.h"
#include "src/identity-map.h"
#include "src/objects-inl.h"
#include "src/utils.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-external-refs.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"

#include "src/zone/accounting-allocator.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

#if DEBUG
#define TRACE(...)                                        \
  do {                                                    \
    if (FLAG_trace_wasm_interpreter) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif

#define FOREACH_INTERNAL_OPCODE(V) V(Breakpoint, 0xFF)

#define WASM_CTYPES(V) \
  V(I32, int32_t) V(I64, int64_t) V(F32, float) V(F64, double)

#define FOREACH_SIMPLE_BINOP(V) \
  V(I32Add, uint32_t, +)        \
  V(I32Sub, uint32_t, -)        \
  V(I32Mul, uint32_t, *)        \
  V(I32And, uint32_t, &)        \
  V(I32Ior, uint32_t, |)        \
  V(I32Xor, uint32_t, ^)        \
  V(I32Eq, uint32_t, ==)        \
  V(I32Ne, uint32_t, !=)        \
  V(I32LtU, uint32_t, <)        \
  V(I32LeU, uint32_t, <=)       \
  V(I32GtU, uint32_t, >)        \
  V(I32GeU, uint32_t, >=)       \
  V(I32LtS, int32_t, <)         \
  V(I32LeS, int32_t, <=)        \
  V(I32GtS, int32_t, >)         \
  V(I32GeS, int32_t, >=)        \
  V(I64Add, uint64_t, +)        \
  V(I64Sub, uint64_t, -)        \
  V(I64Mul, uint64_t, *)        \
  V(I64And, uint64_t, &)        \
  V(I64Ior, uint64_t, |)        \
  V(I64Xor, uint64_t, ^)        \
  V(I64Eq, uint64_t, ==)        \
  V(I64Ne, uint64_t, !=)        \
  V(I64LtU, uint64_t, <)        \
  V(I64LeU, uint64_t, <=)       \
  V(I64GtU, uint64_t, >)        \
  V(I64GeU, uint64_t, >=)       \
  V(I64LtS, int64_t, <)         \
  V(I64LeS, int64_t, <=)        \
  V(I64GtS, int64_t, >)         \
  V(I64GeS, int64_t, >=)        \
  V(F32Add, float, +)           \
  V(F32Sub, float, -)           \
  V(F32Eq, float, ==)           \
  V(F32Ne, float, !=)           \
  V(F32Lt, float, <)            \
  V(F32Le, float, <=)           \
  V(F32Gt, float, >)            \
  V(F32Ge, float, >=)           \
  V(F64Add, double, +)          \
  V(F64Sub, double, -)          \
  V(F64Eq, double, ==)          \
  V(F64Ne, double, !=)          \
  V(F64Lt, double, <)           \
  V(F64Le, double, <=)          \
  V(F64Gt, double, >)           \
  V(F64Ge, double, >=)          \
  V(F32Mul, float, *)           \
  V(F64Mul, double, *)          \
  V(F32Div, float, /)           \
  V(F64Div, double, /)

#define FOREACH_OTHER_BINOP(V) \
  V(I32DivS, int32_t)          \
  V(I32DivU, uint32_t)         \
  V(I32RemS, int32_t)          \
  V(I32RemU, uint32_t)         \
  V(I32Shl, uint32_t)          \
  V(I32ShrU, uint32_t)         \
  V(I32ShrS, int32_t)          \
  V(I64DivS, int64_t)          \
  V(I64DivU, uint64_t)         \
  V(I64RemS, int64_t)          \
  V(I64RemU, uint64_t)         \
  V(I64Shl, uint64_t)          \
  V(I64ShrU, uint64_t)         \
  V(I64ShrS, int64_t)          \
  V(I32Ror, int32_t)           \
  V(I32Rol, int32_t)           \
  V(I64Ror, int64_t)           \
  V(I64Rol, int64_t)           \
  V(F32Min, float)             \
  V(F32Max, float)             \
  V(F64Min, double)            \
  V(F64Max, double)            \
  V(I32AsmjsDivS, int32_t)     \
  V(I32AsmjsDivU, uint32_t)    \
  V(I32AsmjsRemS, int32_t)     \
  V(I32AsmjsRemU, uint32_t)

#define FOREACH_OTHER_UNOP(V)    \
  V(I32Clz, uint32_t)            \
  V(I32Ctz, uint32_t)            \
  V(I32Popcnt, uint32_t)         \
  V(I32Eqz, uint32_t)            \
  V(I64Clz, uint64_t)            \
  V(I64Ctz, uint64_t)            \
  V(I64Popcnt, uint64_t)         \
  V(I64Eqz, uint64_t)            \
  V(F32Abs, float)               \
  V(F32Neg, float)               \
  V(F32Ceil, float)              \
  V(F32Floor, float)             \
  V(F32Trunc, float)             \
  V(F32NearestInt, float)        \
  V(F64Abs, double)              \
  V(F64Neg, double)              \
  V(F64Ceil, double)             \
  V(F64Floor, double)            \
  V(F64Trunc, double)            \
  V(F64NearestInt, double)       \
  V(I32SConvertF32, float)       \
  V(I32SConvertF64, double)      \
  V(I32UConvertF32, float)       \
  V(I32UConvertF64, double)      \
  V(I32ConvertI64, int64_t)      \
  V(I64SConvertF32, float)       \
  V(I64SConvertF64, double)      \
  V(I64UConvertF32, float)       \
  V(I64UConvertF64, double)      \
  V(I64SConvertI32, int32_t)     \
  V(I64UConvertI32, uint32_t)    \
  V(F32SConvertI32, int32_t)     \
  V(F32UConvertI32, uint32_t)    \
  V(F32SConvertI64, int64_t)     \
  V(F32UConvertI64, uint64_t)    \
  V(F32ConvertF64, double)       \
  V(F32ReinterpretI32, int32_t)  \
  V(F64SConvertI32, int32_t)     \
  V(F64UConvertI32, uint32_t)    \
  V(F64SConvertI64, int64_t)     \
  V(F64UConvertI64, uint64_t)    \
  V(F64ConvertF32, float)        \
  V(F64ReinterpretI64, int64_t)  \
  V(I32AsmjsSConvertF32, float)  \
  V(I32AsmjsUConvertF32, float)  \
  V(I32AsmjsSConvertF64, double) \
  V(I32AsmjsUConvertF64, double) \
  V(F32Sqrt, float)              \
  V(F64Sqrt, double)

namespace {

inline int32_t ExecuteI32DivS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  if (b == -1 && a == std::numeric_limits<int32_t>::min()) {
    *trap = kTrapDivUnrepresentable;
    return 0;
  }
  return a / b;
}

inline uint32_t ExecuteI32DivU(uint32_t a, uint32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  return a / b;
}

inline int32_t ExecuteI32RemS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  if (b == -1) return 0;
  return a % b;
}

inline uint32_t ExecuteI32RemU(uint32_t a, uint32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  return a % b;
}

inline uint32_t ExecuteI32Shl(uint32_t a, uint32_t b, TrapReason* trap) {
  return a << (b & 0x1f);
}

inline uint32_t ExecuteI32ShrU(uint32_t a, uint32_t b, TrapReason* trap) {
  return a >> (b & 0x1f);
}

inline int32_t ExecuteI32ShrS(int32_t a, int32_t b, TrapReason* trap) {
  return a >> (b & 0x1f);
}

inline int64_t ExecuteI64DivS(int64_t a, int64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  if (b == -1 && a == std::numeric_limits<int64_t>::min()) {
    *trap = kTrapDivUnrepresentable;
    return 0;
  }
  return a / b;
}

inline uint64_t ExecuteI64DivU(uint64_t a, uint64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  return a / b;
}

inline int64_t ExecuteI64RemS(int64_t a, int64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  if (b == -1) return 0;
  return a % b;
}

inline uint64_t ExecuteI64RemU(uint64_t a, uint64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  return a % b;
}

inline uint64_t ExecuteI64Shl(uint64_t a, uint64_t b, TrapReason* trap) {
  return a << (b & 0x3f);
}

inline uint64_t ExecuteI64ShrU(uint64_t a, uint64_t b, TrapReason* trap) {
  return a >> (b & 0x3f);
}

inline int64_t ExecuteI64ShrS(int64_t a, int64_t b, TrapReason* trap) {
  return a >> (b & 0x3f);
}

inline uint32_t ExecuteI32Ror(uint32_t a, uint32_t b, TrapReason* trap) {
  uint32_t shift = (b & 0x1f);
  return (a >> shift) | (a << (32 - shift));
}

inline uint32_t ExecuteI32Rol(uint32_t a, uint32_t b, TrapReason* trap) {
  uint32_t shift = (b & 0x1f);
  return (a << shift) | (a >> (32 - shift));
}

inline uint64_t ExecuteI64Ror(uint64_t a, uint64_t b, TrapReason* trap) {
  uint32_t shift = (b & 0x3f);
  return (a >> shift) | (a << (64 - shift));
}

inline uint64_t ExecuteI64Rol(uint64_t a, uint64_t b, TrapReason* trap) {
  uint32_t shift = (b & 0x3f);
  return (a << shift) | (a >> (64 - shift));
}

inline float ExecuteF32Min(float a, float b, TrapReason* trap) {
  return JSMin(a, b);
}

inline float ExecuteF32Max(float a, float b, TrapReason* trap) {
  return JSMax(a, b);
}

inline float ExecuteF32CopySign(float a, float b, TrapReason* trap) {
  return copysignf(a, b);
}

inline double ExecuteF64Min(double a, double b, TrapReason* trap) {
  return JSMin(a, b);
}

inline double ExecuteF64Max(double a, double b, TrapReason* trap) {
  return JSMax(a, b);
}

inline double ExecuteF64CopySign(double a, double b, TrapReason* trap) {
  return copysign(a, b);
}

inline int32_t ExecuteI32AsmjsDivS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) return 0;
  if (b == -1 && a == std::numeric_limits<int32_t>::min()) {
    return std::numeric_limits<int32_t>::min();
  }
  return a / b;
}

inline uint32_t ExecuteI32AsmjsDivU(uint32_t a, uint32_t b, TrapReason* trap) {
  if (b == 0) return 0;
  return a / b;
}

inline int32_t ExecuteI32AsmjsRemS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) return 0;
  if (b == -1) return 0;
  return a % b;
}

inline uint32_t ExecuteI32AsmjsRemU(uint32_t a, uint32_t b, TrapReason* trap) {
  if (b == 0) return 0;
  return a % b;
}

inline int32_t ExecuteI32AsmjsSConvertF32(float a, TrapReason* trap) {
  return DoubleToInt32(a);
}

inline uint32_t ExecuteI32AsmjsUConvertF32(float a, TrapReason* trap) {
  return DoubleToUint32(a);
}

inline int32_t ExecuteI32AsmjsSConvertF64(double a, TrapReason* trap) {
  return DoubleToInt32(a);
}

inline uint32_t ExecuteI32AsmjsUConvertF64(double a, TrapReason* trap) {
  return DoubleToUint32(a);
}

int32_t ExecuteI32Clz(uint32_t val, TrapReason* trap) {
  return base::bits::CountLeadingZeros32(val);
}

uint32_t ExecuteI32Ctz(uint32_t val, TrapReason* trap) {
  return base::bits::CountTrailingZeros32(val);
}

uint32_t ExecuteI32Popcnt(uint32_t val, TrapReason* trap) {
  return word32_popcnt_wrapper(&val);
}

inline uint32_t ExecuteI32Eqz(uint32_t val, TrapReason* trap) {
  return val == 0 ? 1 : 0;
}

int64_t ExecuteI64Clz(uint64_t val, TrapReason* trap) {
  return base::bits::CountLeadingZeros64(val);
}

inline uint64_t ExecuteI64Ctz(uint64_t val, TrapReason* trap) {
  return base::bits::CountTrailingZeros64(val);
}

inline int64_t ExecuteI64Popcnt(uint64_t val, TrapReason* trap) {
  return word64_popcnt_wrapper(&val);
}

inline int32_t ExecuteI64Eqz(uint64_t val, TrapReason* trap) {
  return val == 0 ? 1 : 0;
}

inline float ExecuteF32Abs(float a, TrapReason* trap) {
  return bit_cast<float>(bit_cast<uint32_t>(a) & 0x7fffffff);
}

inline float ExecuteF32Neg(float a, TrapReason* trap) {
  return bit_cast<float>(bit_cast<uint32_t>(a) ^ 0x80000000);
}

inline float ExecuteF32Ceil(float a, TrapReason* trap) { return ceilf(a); }

inline float ExecuteF32Floor(float a, TrapReason* trap) { return floorf(a); }

inline float ExecuteF32Trunc(float a, TrapReason* trap) { return truncf(a); }

inline float ExecuteF32NearestInt(float a, TrapReason* trap) {
  return nearbyintf(a);
}

inline float ExecuteF32Sqrt(float a, TrapReason* trap) {
  float result = sqrtf(a);
  return result;
}

inline double ExecuteF64Abs(double a, TrapReason* trap) {
  return bit_cast<double>(bit_cast<uint64_t>(a) & 0x7fffffffffffffff);
}

inline double ExecuteF64Neg(double a, TrapReason* trap) {
  return bit_cast<double>(bit_cast<uint64_t>(a) ^ 0x8000000000000000);
}

inline double ExecuteF64Ceil(double a, TrapReason* trap) { return ceil(a); }

inline double ExecuteF64Floor(double a, TrapReason* trap) { return floor(a); }

inline double ExecuteF64Trunc(double a, TrapReason* trap) { return trunc(a); }

inline double ExecuteF64NearestInt(double a, TrapReason* trap) {
  return nearbyint(a);
}

inline double ExecuteF64Sqrt(double a, TrapReason* trap) { return sqrt(a); }

int32_t ExecuteI32SConvertF32(float a, TrapReason* trap) {
  // The upper bound is (INT32_MAX + 1), which is the lowest float-representable
  // number above INT32_MAX which cannot be represented as int32.
  float upper_bound = 2147483648.0f;
  // We use INT32_MIN as a lower bound because (INT32_MIN - 1) is not
  // representable as float, and no number between (INT32_MIN - 1) and INT32_MIN
  // is.
  float lower_bound = static_cast<float>(INT32_MIN);
  if (a < upper_bound && a >= lower_bound) {
    return static_cast<int32_t>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

int32_t ExecuteI32SConvertF64(double a, TrapReason* trap) {
  // The upper bound is (INT32_MAX + 1), which is the lowest double-
  // representable number above INT32_MAX which cannot be represented as int32.
  double upper_bound = 2147483648.0;
  // The lower bound is (INT32_MIN - 1), which is the greatest double-
  // representable number below INT32_MIN which cannot be represented as int32.
  double lower_bound = -2147483649.0;
  if (a < upper_bound && a > lower_bound) {
    return static_cast<int32_t>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

uint32_t ExecuteI32UConvertF32(float a, TrapReason* trap) {
  // The upper bound is (UINT32_MAX + 1), which is the lowest
  // float-representable number above UINT32_MAX which cannot be represented as
  // uint32.
  double upper_bound = 4294967296.0f;
  double lower_bound = -1.0f;
  if (a < upper_bound && a > lower_bound) {
    return static_cast<uint32_t>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

uint32_t ExecuteI32UConvertF64(double a, TrapReason* trap) {
  // The upper bound is (UINT32_MAX + 1), which is the lowest
  // double-representable number above UINT32_MAX which cannot be represented as
  // uint32.
  double upper_bound = 4294967296.0;
  double lower_bound = -1.0;
  if (a < upper_bound && a > lower_bound) {
    return static_cast<uint32_t>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

inline uint32_t ExecuteI32ConvertI64(int64_t a, TrapReason* trap) {
  return static_cast<uint32_t>(a & 0xFFFFFFFF);
}

int64_t ExecuteI64SConvertF32(float a, TrapReason* trap) {
  int64_t output;
  if (!float32_to_int64_wrapper(&a, &output)) {
    *trap = kTrapFloatUnrepresentable;
  }
  return output;
}

int64_t ExecuteI64SConvertF64(double a, TrapReason* trap) {
  int64_t output;
  if (!float64_to_int64_wrapper(&a, &output)) {
    *trap = kTrapFloatUnrepresentable;
  }
  return output;
}

uint64_t ExecuteI64UConvertF32(float a, TrapReason* trap) {
  uint64_t output;
  if (!float32_to_uint64_wrapper(&a, &output)) {
    *trap = kTrapFloatUnrepresentable;
  }
  return output;
}

uint64_t ExecuteI64UConvertF64(double a, TrapReason* trap) {
  uint64_t output;
  if (!float64_to_uint64_wrapper(&a, &output)) {
    *trap = kTrapFloatUnrepresentable;
  }
  return output;
}

inline int64_t ExecuteI64SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<int64_t>(a);
}

inline int64_t ExecuteI64UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<uint64_t>(a);
}

inline float ExecuteF32SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<float>(a);
}

inline float ExecuteF32UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<float>(a);
}

inline float ExecuteF32SConvertI64(int64_t a, TrapReason* trap) {
  float output;
  int64_to_float32_wrapper(&a, &output);
  return output;
}

inline float ExecuteF32UConvertI64(uint64_t a, TrapReason* trap) {
  float output;
  uint64_to_float32_wrapper(&a, &output);
  return output;
}

inline float ExecuteF32ConvertF64(double a, TrapReason* trap) {
  return static_cast<float>(a);
}

inline float ExecuteF32ReinterpretI32(int32_t a, TrapReason* trap) {
  return bit_cast<float>(a);
}

inline double ExecuteF64SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<double>(a);
}

inline double ExecuteF64UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<double>(a);
}

inline double ExecuteF64SConvertI64(int64_t a, TrapReason* trap) {
  double output;
  int64_to_float64_wrapper(&a, &output);
  return output;
}

inline double ExecuteF64UConvertI64(uint64_t a, TrapReason* trap) {
  double output;
  uint64_to_float64_wrapper(&a, &output);
  return output;
}

inline double ExecuteF64ConvertF32(float a, TrapReason* trap) {
  return static_cast<double>(a);
}

inline double ExecuteF64ReinterpretI64(int64_t a, TrapReason* trap) {
  return bit_cast<double>(a);
}

inline int32_t ExecuteI32ReinterpretF32(WasmVal a) {
  return a.to_unchecked<int32_t>();
}

inline int64_t ExecuteI64ReinterpretF64(WasmVal a) {
  return a.to_unchecked<int64_t>();
}

inline int32_t ExecuteGrowMemory(uint32_t delta_pages,
                                 MaybeHandle<WasmInstanceObject> instance_obj,
                                 WasmInstance* instance) {
  DCHECK_EQ(0, instance->mem_size % WasmModule::kPageSize);
  uint32_t old_pages = instance->mem_size / WasmModule::kPageSize;

  // If an instance is set, execute GrowMemory on the instance. This will also
  // update the WasmInstance struct used here.
  if (!instance_obj.is_null()) {
    Isolate* isolate = instance_obj.ToHandleChecked()->GetIsolate();
    int32_t ret = WasmInstanceObject::GrowMemory(
        isolate, instance_obj.ToHandleChecked(), delta_pages);
    // Some sanity checks.
    DCHECK_EQ(ret == -1 ? old_pages : old_pages + delta_pages,
              instance->mem_size / WasmModule::kPageSize);
    DCHECK(ret == -1 || static_cast<uint32_t>(ret) == old_pages);
    return ret;
  }

  // TODO(ahaas): Move memory allocation to wasm-module.cc for better
  // encapsulation.
  if (delta_pages > FLAG_wasm_max_mem_pages ||
      delta_pages > instance->module->max_mem_pages) {
    return -1;
  }

  uint32_t new_pages = old_pages + delta_pages;
  if (new_pages > FLAG_wasm_max_mem_pages ||
      new_pages > instance->module->max_mem_pages) {
    return -1;
  }

  byte* new_mem_start;
  if (instance->mem_size == 0) {
    // TODO(gdeepti): Fix bounds check to take into account size of memtype.
    new_mem_start = static_cast<byte*>(
        calloc(new_pages * WasmModule::kPageSize, sizeof(byte)));
    if (!new_mem_start) return -1;
  } else {
    DCHECK_NOT_NULL(instance->mem_start);
    if (EnableGuardRegions()) {
      v8::base::OS::Unprotect(instance->mem_start,
                              new_pages * WasmModule::kPageSize);
      new_mem_start = instance->mem_start;
    } else {
      new_mem_start = static_cast<byte*>(
          realloc(instance->mem_start, new_pages * WasmModule::kPageSize));
      if (!new_mem_start) return -1;
    }
    // Zero initializing uninitialized memory from realloc
    memset(new_mem_start + old_pages * WasmModule::kPageSize, 0,
           delta_pages * WasmModule::kPageSize);
  }
  instance->mem_start = new_mem_start;
  instance->mem_size = new_pages * WasmModule::kPageSize;
  return static_cast<int32_t>(old_pages);
}

enum InternalOpcode {
#define DECL_INTERNAL_ENUM(name, value) kInternal##name = value,
  FOREACH_INTERNAL_OPCODE(DECL_INTERNAL_ENUM)
#undef DECL_INTERNAL_ENUM
};

const char* OpcodeName(uint32_t val) {
  switch (val) {
#define DECL_INTERNAL_CASE(name, value) \
  case kInternal##name:                 \
    return "Internal" #name;
    FOREACH_INTERNAL_OPCODE(DECL_INTERNAL_CASE)
#undef DECL_INTERNAL_CASE
  }
  return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(val));
}

// Unwrap a wasm to js wrapper, return the callable heap object.
// If the wrapper would throw a TypeError, return a null handle.
Handle<HeapObject> UnwrapWasmToJSWrapper(Isolate* isolate,
                                         Handle<Code> js_wrapper) {
  DCHECK_EQ(Code::WASM_TO_JS_FUNCTION, js_wrapper->kind());
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(*js_wrapper, mask); !it.done(); it.next()) {
    HeapObject* obj = it.rinfo()->target_object();
    if (!obj->IsCallable()) continue;
#ifdef DEBUG
    // There should only be this one reference to a callable object.
    for (it.next(); !it.done(); it.next()) {
      HeapObject* other = it.rinfo()->target_object();
      DCHECK(!other->IsCallable());
    }
#endif
    return handle(obj, isolate);
  }
  // If we did not find a callable object, then there must be a reference to
  // the WasmThrowTypeError runtime function.
  // TODO(clemensh): Check that this is the case.
  return Handle<HeapObject>::null();
}

// A helper class to compute the control transfers for each bytecode offset.
// Control transfers allow Br, BrIf, BrTable, If, Else, and End bytecodes to
// be directly executed without the need to dynamically track blocks.
class ControlTransfers : public ZoneObject {
 public:
  ControlTransferMap map_;

  ControlTransfers(Zone* zone, BodyLocalDecls* locals, const byte* start,
                   const byte* end)
      : map_(zone) {
    // Represents a control flow label.
    struct CLabel : public ZoneObject {
      const byte* target;
      ZoneVector<const byte*> refs;

      explicit CLabel(Zone* zone) : target(nullptr), refs(zone) {}

      // Bind this label to the given PC.
      void Bind(ControlTransferMap* map, const byte* start, const byte* pc) {
        DCHECK_NULL(target);
        target = pc;
        for (auto from_pc : refs) {
          auto pcdiff = static_cast<pcdiff_t>(target - from_pc);
          size_t offset = static_cast<size_t>(from_pc - start);
          (*map)[offset] = pcdiff;
        }
      }

      // Reference this label from the given location.
      void Ref(ControlTransferMap* map, const byte* start,
               const byte* from_pc) {
        if (target) {
          // Target being bound before a reference means this is a loop.
          DCHECK_EQ(kExprLoop, *target);
          auto pcdiff = static_cast<pcdiff_t>(target - from_pc);
          size_t offset = static_cast<size_t>(from_pc - start);
          (*map)[offset] = pcdiff;
        } else {
          refs.push_back(from_pc);
        }
      }
    };

    // An entry in the control stack.
    struct Control {
      const byte* pc;
      CLabel* end_label;
      CLabel* else_label;

      void Ref(ControlTransferMap* map, const byte* start,
               const byte* from_pc) {
        end_label->Ref(map, start, from_pc);
      }
    };

    // Compute the ControlTransfer map.
    // This algorithm maintains a stack of control constructs similar to the
    // AST decoder. The {control_stack} allows matching {br,br_if,br_table}
    // bytecodes with their target, as well as determining whether the current
    // bytecodes are within the true or false block of an else.
    std::vector<Control> control_stack;
    CLabel* func_label = new (zone) CLabel(zone);
    control_stack.push_back({start, func_label, nullptr});
    for (BytecodeIterator i(start, end, locals); i.has_next(); i.next()) {
      WasmOpcode opcode = i.current();
      TRACE("@%u: control %s\n", i.pc_offset(),
            WasmOpcodes::OpcodeName(opcode));
      switch (opcode) {
        case kExprBlock: {
          TRACE("control @%u: Block\n", i.pc_offset());
          CLabel* label = new (zone) CLabel(zone);
          control_stack.push_back({i.pc(), label, nullptr});
          break;
        }
        case kExprLoop: {
          TRACE("control @%u: Loop\n", i.pc_offset());
          CLabel* label = new (zone) CLabel(zone);
          control_stack.push_back({i.pc(), label, nullptr});
          label->Bind(&map_, start, i.pc());
          break;
        }
        case kExprIf: {
          TRACE("control @%u: If\n", i.pc_offset());
          CLabel* end_label = new (zone) CLabel(zone);
          CLabel* else_label = new (zone) CLabel(zone);
          control_stack.push_back({i.pc(), end_label, else_label});
          else_label->Ref(&map_, start, i.pc());
          break;
        }
        case kExprElse: {
          Control* c = &control_stack.back();
          TRACE("control @%u: Else\n", i.pc_offset());
          c->end_label->Ref(&map_, start, i.pc());
          DCHECK_NOT_NULL(c->else_label);
          c->else_label->Bind(&map_, start, i.pc() + 1);
          c->else_label = nullptr;
          break;
        }
        case kExprEnd: {
          Control* c = &control_stack.back();
          TRACE("control @%u: End\n", i.pc_offset());
          if (c->end_label->target) {
            // only loops have bound labels.
            DCHECK_EQ(kExprLoop, *c->pc);
          } else {
            if (c->else_label) c->else_label->Bind(&map_, start, i.pc());
            c->end_label->Bind(&map_, start, i.pc() + 1);
          }
          control_stack.pop_back();
          break;
        }
        case kExprBr: {
          BreakDepthOperand<false> operand(&i, i.pc());
          TRACE("control @%u: Br[depth=%u]\n", i.pc_offset(), operand.depth);
          Control* c = &control_stack[control_stack.size() - operand.depth - 1];
          c->Ref(&map_, start, i.pc());
          break;
        }
        case kExprBrIf: {
          BreakDepthOperand<false> operand(&i, i.pc());
          TRACE("control @%u: BrIf[depth=%u]\n", i.pc_offset(), operand.depth);
          Control* c = &control_stack[control_stack.size() - operand.depth - 1];
          c->Ref(&map_, start, i.pc());
          break;
        }
        case kExprBrTable: {
          BranchTableOperand<false> operand(&i, i.pc());
          BranchTableIterator<false> iterator(&i, operand);
          TRACE("control @%u: BrTable[count=%u]\n", i.pc_offset(),
                operand.table_count);
          while (iterator.has_next()) {
            uint32_t j = iterator.cur_index();
            uint32_t target = iterator.next();
            Control* c = &control_stack[control_stack.size() - target - 1];
            c->Ref(&map_, start, i.pc() + j);
          }
          break;
        }
        default: {
          break;
        }
      }
    }
    if (!func_label->target) func_label->Bind(&map_, start, end);
  }

  pcdiff_t Lookup(pc_t from) {
    auto result = map_.find(from);
    if (result == map_.end()) {
      V8_Fatal(__FILE__, __LINE__, "no control target for pc %zu", from);
    }
    return result->second;
  }
};

// Code and metadata needed to execute a function.
struct InterpreterCode {
  const WasmFunction* function;  // wasm function
  BodyLocalDecls locals;         // local declarations
  const byte* orig_start;        // start of original code
  const byte* orig_end;          // end of original code
  byte* start;                   // start of (maybe altered) code
  byte* end;                     // end of (maybe altered) code
  ControlTransfers* targets;     // helper for control flow.

  const byte* at(pc_t pc) { return start + pc; }
};

struct ExternalCallResult {
  enum Type {
    // The function should be executed inside this interpreter.
    INTERNAL,
    // For indirect calls: Table or function does not exist.
    INVALID_FUNC,
    // For indirect calls: Signature does not match expected signature.
    SIGNATURE_MISMATCH,
    // The function was executed and returned normally.
    EXTERNAL_RETURNED,
    // The function was executed, threw an exception, and the stack was unwound.
    EXTERNAL_UNWOUND
  };
  Type type;
  // If type is INTERNAL, this field holds the function to call internally.
  InterpreterCode* interpreter_code;

  ExternalCallResult(Type type) : type(type) {  // NOLINT
    DCHECK_NE(INTERNAL, type);
  }
  ExternalCallResult(Type type, InterpreterCode* code)
      : type(type), interpreter_code(code) {
    DCHECK_EQ(INTERNAL, type);
  }
};

// The main storage for interpreter code. It maps {WasmFunction} to the
// metadata needed to execute each function.
class CodeMap {
  Zone* zone_;
  const WasmModule* module_;
  ZoneVector<InterpreterCode> interpreter_code_;
  // Global handle to the wasm instance.
  Handle<WasmInstanceObject> instance_;
  // Global handle to array of unwrapped imports.
  Handle<FixedArray> imported_functions_;
  // Map from WASM_TO_JS wrappers to unwrapped imports (indexes into
  // imported_functions_).
  IdentityMap<int, ZoneAllocationPolicy> unwrapped_imports_;

 public:
  CodeMap(Isolate* isolate, const WasmModule* module,
          const uint8_t* module_start, Zone* zone)
      : zone_(zone),
        module_(module),
        interpreter_code_(zone),
        unwrapped_imports_(isolate->heap(), ZoneAllocationPolicy(zone)) {
    if (module == nullptr) return;
    interpreter_code_.reserve(module->functions.size());
    for (const WasmFunction& function : module->functions) {
      if (function.imported) {
        DCHECK_EQ(function.code_start_offset, function.code_end_offset);
        AddFunction(&function, nullptr, nullptr);
      } else {
        const byte* code_start = module_start + function.code_start_offset;
        const byte* code_end = module_start + function.code_end_offset;
        AddFunction(&function, code_start, code_end);
      }
    }
  }

  ~CodeMap() {
    // Destroy the global handles.
    // Cast the location, not the handle, because the handle cast might access
    // the object behind the handle.
    GlobalHandles::Destroy(reinterpret_cast<Object**>(instance_.location()));
    GlobalHandles::Destroy(
        reinterpret_cast<Object**>(imported_functions_.location()));
  }

  const WasmModule* module() const { return module_; }
  bool has_instance() const { return !instance_.is_null(); }
  Handle<WasmInstanceObject> instance() const {
    DCHECK(has_instance());
    return instance_;
  }
  MaybeHandle<WasmInstanceObject> maybe_instance() const {
    return has_instance() ? instance_ : MaybeHandle<WasmInstanceObject>();
  }

  void SetInstanceObject(WasmInstanceObject* instance) {
    // Only set the instance once (otherwise we have to destroy the global
    // handle first).
    DCHECK(instance_.is_null());
    DCHECK_EQ(instance->module(), module_);
    instance_ = instance->GetIsolate()->global_handles()->Create(instance);
  }

  Code* GetImportedFunction(uint32_t function_index) {
    DCHECK(!instance_.is_null());
    DCHECK_GT(module_->num_imported_functions, function_index);
    FixedArray* code_table = instance_->compiled_module()->ptr_to_code_table();
    return Code::cast(code_table->get(static_cast<int>(function_index)));
  }

  InterpreterCode* GetCode(const WasmFunction* function) {
    InterpreterCode* code = GetCode(function->func_index);
    DCHECK_EQ(function, code->function);
    return code;
  }

  InterpreterCode* GetCode(uint32_t function_index) {
    DCHECK_LT(function_index, interpreter_code_.size());
    return Preprocess(&interpreter_code_[function_index]);
  }

  InterpreterCode* GetIndirectCode(uint32_t table_index, uint32_t entry_index) {
    if (table_index >= module_->function_tables.size()) return nullptr;
    const WasmIndirectFunctionTable* table =
        &module_->function_tables[table_index];
    if (entry_index >= table->values.size()) return nullptr;
    uint32_t index = table->values[entry_index];
    if (index >= interpreter_code_.size()) return nullptr;
    return GetCode(index);
  }

  InterpreterCode* Preprocess(InterpreterCode* code) {
    DCHECK_EQ(code->function->imported, code->start == nullptr);
    if (code->targets == nullptr && code->start != nullptr) {
      // Compute the control targets map and the local declarations.
      CHECK(DecodeLocalDecls(&code->locals, code->start, code->end));
      code->targets = new (zone_) ControlTransfers(
          zone_, &code->locals, code->orig_start, code->orig_end);
    }
    return code;
  }

  void AddFunction(const WasmFunction* function, const byte* code_start,
                   const byte* code_end) {
    InterpreterCode code = {
        function, BodyLocalDecls(zone_),         code_start,
        code_end, const_cast<byte*>(code_start), const_cast<byte*>(code_end),
        nullptr};

    DCHECK_EQ(interpreter_code_.size(), function->func_index);
    interpreter_code_.push_back(code);
  }

  void SetFunctionCode(const WasmFunction* function, const byte* start,
                       const byte* end) {
    DCHECK_LT(function->func_index, interpreter_code_.size());
    InterpreterCode* code = &interpreter_code_[function->func_index];
    DCHECK_EQ(function, code->function);
    code->targets = nullptr;
    code->orig_start = start;
    code->orig_end = end;
    code->start = const_cast<byte*>(start);
    code->end = const_cast<byte*>(end);
    Preprocess(code);
  }

  // Returns a callable object if the imported function has a JS-compatible
  // signature, or a null handle otherwise.
  Handle<HeapObject> GetCallableObjectForJSImport(Isolate* isolate,
                                                  Handle<Code> code) {
    DCHECK_EQ(Code::WASM_TO_JS_FUNCTION, code->kind());
    int* unwrapped_index = unwrapped_imports_.Find(code);
    if (unwrapped_index) {
      return handle(
          HeapObject::cast(imported_functions_->get(*unwrapped_index)),
          isolate);
    }
    Handle<HeapObject> called_obj = UnwrapWasmToJSWrapper(isolate, code);
    if (!called_obj.is_null()) {
      // Cache the unwrapped callable object.
      if (imported_functions_.is_null()) {
        // This is the first call to an imported function. Allocate the
        // FixedArray to cache unwrapped objects.
        constexpr int kInitialCacheSize = 8;
        Handle<FixedArray> new_imported_functions =
            isolate->factory()->NewFixedArray(kInitialCacheSize, TENURED);
        // First entry: Number of occupied slots.
        new_imported_functions->set(0, Smi::kZero);
        imported_functions_ =
            isolate->global_handles()->Create(*new_imported_functions);
      }
      int this_idx = Smi::cast(imported_functions_->get(0))->value() + 1;
      if (this_idx == imported_functions_->length()) {
        Handle<FixedArray> new_imported_functions =
            isolate->factory()->CopyFixedArrayAndGrow(imported_functions_,
                                                      this_idx / 2, TENURED);
        // Update the existing global handle:
        *imported_functions_.location() = *new_imported_functions;
      }
      DCHECK_GT(imported_functions_->length(), this_idx);
      DCHECK(imported_functions_->get(this_idx)->IsUndefined(isolate));
      imported_functions_->set(0, Smi::FromInt(this_idx));
      imported_functions_->set(this_idx, *called_obj);
      unwrapped_imports_.Set(code, this_idx);
    }
    return called_obj;
  }
};

Handle<Object> WasmValToNumber(Factory* factory, WasmVal val,
                               wasm::ValueType type) {
  switch (type) {
    case kWasmI32:
      return factory->NewNumberFromInt(val.to<int32_t>());
    case kWasmI64:
      // wasm->js and js->wasm is illegal for i64 type.
      UNREACHABLE();
      return Handle<Object>::null();
    case kWasmF32:
      return factory->NewNumber(val.to<float>());
    case kWasmF64:
      return factory->NewNumber(val.to<double>());
    default:
      // TODO(wasm): Implement simd.
      UNIMPLEMENTED();
      return Handle<Object>::null();
  }
}

// Convert JS value to WebAssembly, spec here:
// https://github.com/WebAssembly/design/blob/master/JS.md#towebassemblyvalue
WasmVal ToWebAssemblyValue(Isolate* isolate, Handle<Object> value,
                           wasm::ValueType type) {
  switch (type) {
    case kWasmI32: {
      MaybeHandle<Object> maybe_i32 = Object::ToInt32(isolate, value);
      // TODO(clemensh): Handle failure here (unwind).
      int32_t value;
      CHECK(maybe_i32.ToHandleChecked()->ToInt32(&value));
      return WasmVal(value);
    }
    case kWasmI64:
      // If the signature contains i64, a type error was thrown before.
      UNREACHABLE();
    case kWasmF32: {
      MaybeHandle<Object> maybe_number = Object::ToNumber(value);
      // TODO(clemensh): Handle failure here (unwind).
      return WasmVal(
          static_cast<float>(maybe_number.ToHandleChecked()->Number()));
    }
    case kWasmF64: {
      MaybeHandle<Object> maybe_number = Object::ToNumber(value);
      // TODO(clemensh): Handle failure here (unwind).
      return WasmVal(maybe_number.ToHandleChecked()->Number());
    }
    default:
      // TODO(wasm): Handle simd.
      UNIMPLEMENTED();
      return WasmVal();
  }
}

// Responsible for executing code directly.
class ThreadImpl {
  struct Activation {
    uint32_t fp;
    uint32_t sp;
    Activation(uint32_t fp, uint32_t sp) : fp(fp), sp(sp) {}
  };

 public:
  ThreadImpl(Zone* zone, CodeMap* codemap, WasmInstance* instance)
      : codemap_(codemap),
        instance_(instance),
        stack_(zone),
        frames_(zone),
        blocks_(zone),
        activations_(zone) {}

  //==========================================================================
  // Implementation of public interface for WasmInterpreter::Thread.
  //==========================================================================

  WasmInterpreter::State state() { return state_; }

  void InitFrame(const WasmFunction* function, WasmVal* args) {
    DCHECK_EQ(current_activation().fp, frames_.size());
    InterpreterCode* code = codemap()->GetCode(function);
    for (size_t i = 0; i < function->sig->parameter_count(); ++i) {
      stack_.push_back(args[i]);
    }
    PushFrame(code);
  }

  WasmInterpreter::State Run(int num_steps = -1) {
    DCHECK(state_ == WasmInterpreter::STOPPED ||
           state_ == WasmInterpreter::PAUSED);
    DCHECK(num_steps == -1 || num_steps > 0);
    if (num_steps == -1) {
      TRACE("  => Run()\n");
    } else if (num_steps == 1) {
      TRACE("  => Step()\n");
    } else {
      TRACE("  => Run(%d)\n", num_steps);
    }
    state_ = WasmInterpreter::RUNNING;
    Execute(frames_.back().code, frames_.back().pc, num_steps);
    // If state_ is STOPPED, the current activation must be fully unwound.
    DCHECK_IMPLIES(state_ == WasmInterpreter::STOPPED,
                   current_activation().fp == frames_.size());
    return state_;
  }

  void Pause() { UNIMPLEMENTED(); }

  void Reset() {
    TRACE("----- RESET -----\n");
    stack_.clear();
    frames_.clear();
    state_ = WasmInterpreter::STOPPED;
    trap_reason_ = kTrapCount;
    possible_nondeterminism_ = false;
  }

  int GetFrameCount() {
    DCHECK_GE(kMaxInt, frames_.size());
    return static_cast<int>(frames_.size());
  }

  template <typename FrameCons>
  InterpretedFrame GetMutableFrame(int index, FrameCons frame_cons) {
    DCHECK_LE(0, index);
    DCHECK_GT(frames_.size(), index);
    Frame* frame = &frames_[index];
    DCHECK_GE(kMaxInt, frame->pc);
    DCHECK_GE(kMaxInt, frame->sp);
    DCHECK_GE(kMaxInt, frame->llimit());
    return frame_cons(frame->code->function, static_cast<int>(frame->pc),
                      static_cast<int>(frame->sp),
                      static_cast<int>(frame->llimit()));
  }

  WasmVal GetReturnValue(uint32_t index) {
    if (state_ == WasmInterpreter::TRAPPED) return WasmVal(0xdeadbeef);
    DCHECK_EQ(WasmInterpreter::FINISHED, state_);
    Activation act = current_activation();
    // Current activation must be finished.
    DCHECK_EQ(act.fp, frames_.size());
    DCHECK_GT(stack_.size(), act.sp + index);
    return stack_[act.sp + index];
  }

  TrapReason GetTrapReason() { return trap_reason_; }

  pc_t GetBreakpointPc() { return break_pc_; }

  bool PossibleNondeterminism() { return possible_nondeterminism_; }

  uint64_t NumInterpretedCalls() { return num_interpreted_calls_; }

  void AddBreakFlags(uint8_t flags) { break_flags_ |= flags; }

  void ClearBreakFlags() { break_flags_ = WasmInterpreter::BreakFlag::None; }

  uint32_t NumActivations() {
    return static_cast<uint32_t>(activations_.size());
  }

  uint32_t StartActivation() {
    TRACE("----- START ACTIVATION %zu -----\n", activations_.size());
    // If you use activations, use them consistently:
    DCHECK_IMPLIES(activations_.empty(), frames_.empty());
    DCHECK_IMPLIES(activations_.empty(), stack_.empty());
    uint32_t activation_id = static_cast<uint32_t>(activations_.size());
    activations_.emplace_back(static_cast<uint32_t>(frames_.size()),
                              static_cast<uint32_t>(stack_.size()));
    state_ = WasmInterpreter::STOPPED;
    return activation_id;
  }

  void FinishActivation(uint32_t id) {
    TRACE("----- FINISH ACTIVATION %zu -----\n", activations_.size() - 1);
    DCHECK_LT(0, activations_.size());
    DCHECK_EQ(activations_.size() - 1, id);
    // Stack height must match the start of this activation (otherwise unwind
    // first).
    DCHECK_EQ(activations_.back().fp, frames_.size());
    DCHECK_LE(activations_.back().sp, stack_.size());
    stack_.resize(activations_.back().sp);
    activations_.pop_back();
  }

  uint32_t ActivationFrameBase(uint32_t id) {
    DCHECK_GT(activations_.size(), id);
    return activations_[id].fp;
  }

  // Handle a thrown exception. Returns whether the exception was handled inside
  // the current activation. Unwinds the interpreted stack accordingly.
  WasmInterpreter::Thread::ExceptionHandlingResult HandleException(
      Isolate* isolate) {
    DCHECK(isolate->has_pending_exception());
    // TODO(wasm): Add wasm exception handling (would return true).
    USE(isolate->pending_exception());
    TRACE("----- UNWIND -----\n");
    DCHECK_LT(0, activations_.size());
    Activation& act = activations_.back();
    DCHECK_LE(act.fp, frames_.size());
    frames_.resize(act.fp);
    DCHECK_LE(act.sp, stack_.size());
    stack_.resize(act.sp);
    state_ = WasmInterpreter::STOPPED;
    return WasmInterpreter::Thread::UNWOUND;
  }

 private:
  // Entries on the stack of functions being evaluated.
  struct Frame {
    InterpreterCode* code;
    pc_t pc;
    sp_t sp;

    // Limit of parameters.
    sp_t plimit() { return sp + code->function->sig->parameter_count(); }
    // Limit of locals.
    sp_t llimit() { return plimit() + code->locals.type_list.size(); }
  };

  struct Block {
    pc_t pc;
    sp_t sp;
    size_t fp;
    unsigned arity;
  };

  CodeMap* codemap_;
  WasmInstance* instance_;
  ZoneVector<WasmVal> stack_;
  ZoneVector<Frame> frames_;
  ZoneVector<Block> blocks_;
  WasmInterpreter::State state_ = WasmInterpreter::STOPPED;
  pc_t break_pc_ = kInvalidPc;
  TrapReason trap_reason_ = kTrapCount;
  bool possible_nondeterminism_ = false;
  uint8_t break_flags_ = 0;  // a combination of WasmInterpreter::BreakFlag
  uint64_t num_interpreted_calls_ = 0;
  // Store the stack height of each activation (for unwind and frame
  // inspection).
  ZoneVector<Activation> activations_;

  CodeMap* codemap() { return codemap_; }
  WasmInstance* instance() { return instance_; }
  const WasmModule* module() { return instance_->module; }

  void DoTrap(TrapReason trap, pc_t pc) {
    state_ = WasmInterpreter::TRAPPED;
    trap_reason_ = trap;
    CommitPc(pc);
  }

  // Push a frame with arguments already on the stack.
  void PushFrame(InterpreterCode* code) {
    DCHECK_NOT_NULL(code);
    ++num_interpreted_calls_;
    size_t arity = code->function->sig->parameter_count();
    // The parameters will overlap the arguments already on the stack.
    DCHECK_GE(stack_.size(), arity);
    frames_.push_back({code, 0, stack_.size() - arity});
    blocks_.push_back(
        {0, stack_.size(), frames_.size(),
         static_cast<uint32_t>(code->function->sig->return_count())});
    frames_.back().pc = InitLocals(code);
    TRACE("  => PushFrame #%zu (#%u @%zu)\n", frames_.size() - 1,
          code->function->func_index, frames_.back().pc);
  }

  pc_t InitLocals(InterpreterCode* code) {
    for (auto p : code->locals.type_list) {
      WasmVal val;
      switch (p) {
#define CASE_TYPE(wasm, ctype)            \
  case kWasm##wasm:                       \
    val = WasmVal(static_cast<ctype>(0)); \
    break;
        WASM_CTYPES(CASE_TYPE)
#undef CASE_TYPE
        default:
          UNREACHABLE();
          break;
      }
      stack_.push_back(val);
    }
    return code->locals.encoded_size;
  }

  void CommitPc(pc_t pc) {
    DCHECK(!frames_.empty());
    frames_.back().pc = pc;
  }

  bool SkipBreakpoint(InterpreterCode* code, pc_t pc) {
    if (pc == break_pc_) {
      // Skip the previously hit breakpoint when resuming.
      break_pc_ = kInvalidPc;
      return true;
    }
    return false;
  }

  int LookupTarget(InterpreterCode* code, pc_t pc) {
    return static_cast<int>(code->targets->Lookup(pc));
  }

  int DoBreak(InterpreterCode* code, pc_t pc, size_t depth) {
    size_t bp = blocks_.size() - depth - 1;
    Block* target = &blocks_[bp];
    DoStackTransfer(target->sp, target->arity);
    blocks_.resize(bp);
    return LookupTarget(code, pc);
  }

  pc_t ReturnPc(Decoder* decoder, InterpreterCode* code, pc_t pc) {
    switch (code->orig_start[pc]) {
      case kExprCallFunction: {
        CallFunctionOperand<false> operand(decoder, code->at(pc));
        return pc + 1 + operand.length;
      }
      case kExprCallIndirect: {
        CallIndirectOperand<false> operand(decoder, code->at(pc));
        return pc + 1 + operand.length;
      }
      default:
        UNREACHABLE();
        return 0;
    }
  }

  bool DoReturn(Decoder* decoder, InterpreterCode** code, pc_t* pc, pc_t* limit,
                size_t arity) {
    DCHECK_GT(frames_.size(), 0);
    // Pop all blocks for this frame.
    while (!blocks_.empty() && blocks_.back().fp == frames_.size()) {
      blocks_.pop_back();
    }

    sp_t dest = frames_.back().sp;
    frames_.pop_back();
    if (frames_.size() == current_activation().fp) {
      // A return from the last frame terminates the execution.
      state_ = WasmInterpreter::FINISHED;
      DoStackTransfer(dest, arity);
      TRACE("  => finish\n");
      return false;
    } else {
      // Return to caller frame.
      Frame* top = &frames_.back();
      *code = top->code;
      decoder->Reset((*code)->start, (*code)->end);
      *pc = ReturnPc(decoder, *code, top->pc);
      *limit = top->code->end - top->code->start;
      TRACE("  => Return to #%zu (#%u @%zu)\n", frames_.size() - 1,
            (*code)->function->func_index, *pc);
      DoStackTransfer(dest, arity);
      return true;
    }
  }

  // Returns true if the call was successful, false if the stack check failed
  // and the current activation was fully unwound.
  bool DoCall(Decoder* decoder, InterpreterCode* target, pc_t* pc,
              pc_t* limit) WARN_UNUSED_RESULT {
    frames_.back().pc = *pc;
    PushFrame(target);
    if (!DoStackCheck()) return false;
    *pc = frames_.back().pc;
    *limit = target->end - target->start;
    decoder->Reset(target->start, target->end);
    return true;
  }

  // Copies {arity} values on the top of the stack down the stack to {dest},
  // dropping the values in-between.
  void DoStackTransfer(sp_t dest, size_t arity) {
    // before: |---------------| pop_count | arity |
    //         ^ 0             ^ dest              ^ stack_.size()
    //
    // after:  |---------------| arity |
    //         ^ 0                     ^ stack_.size()
    DCHECK_LE(dest, stack_.size());
    DCHECK_LE(dest + arity, stack_.size());
    size_t pop_count = stack_.size() - dest - arity;
    for (size_t i = 0; i < arity; i++) {
      stack_[dest + i] = stack_[dest + pop_count + i];
    }
    stack_.resize(stack_.size() - pop_count);
  }

  template <typename mtype>
  inline bool BoundsCheck(uint32_t mem_size, uint32_t offset, uint32_t index) {
    return sizeof(mtype) <= mem_size && offset <= mem_size - sizeof(mtype) &&
           index <= mem_size - sizeof(mtype) - offset;
  }

  template <typename ctype, typename mtype>
  bool ExecuteLoad(Decoder* decoder, InterpreterCode* code, pc_t pc, int& len) {
    MemoryAccessOperand<false> operand(decoder, code->at(pc), sizeof(ctype));
    uint32_t index = Pop().to<uint32_t>();
    if (!BoundsCheck<mtype>(instance()->mem_size, operand.offset, index)) {
      DoTrap(kTrapMemOutOfBounds, pc);
      return false;
    }
    byte* addr = instance()->mem_start + operand.offset + index;
    WasmVal result(static_cast<ctype>(ReadLittleEndianValue<mtype>(addr)));

    Push(pc, result);
    len = 1 + operand.length;
    return true;
  }

  template <typename ctype, typename mtype>
  bool ExecuteStore(Decoder* decoder, InterpreterCode* code, pc_t pc,
                    int& len) {
    MemoryAccessOperand<false> operand(decoder, code->at(pc), sizeof(ctype));
    WasmVal val = Pop();

    uint32_t index = Pop().to<uint32_t>();
    if (!BoundsCheck<mtype>(instance()->mem_size, operand.offset, index)) {
      DoTrap(kTrapMemOutOfBounds, pc);
      return false;
    }
    byte* addr = instance()->mem_start + operand.offset + index;
    WriteLittleEndianValue<mtype>(addr, static_cast<mtype>(val.to<ctype>()));
    len = 1 + operand.length;

    if (std::is_same<float, ctype>::value) {
      possible_nondeterminism_ |= std::isnan(val.to<float>());
    } else if (std::is_same<double, ctype>::value) {
      possible_nondeterminism_ |= std::isnan(val.to<double>());
    }
    return true;
  }

  // Check if our control stack (frames_) exceeds the limit. Trigger stack
  // overflow if it does, and unwinding the current frame.
  // Returns true if execution can continue, false if the current activation was
  // fully unwound.
  // Do call this function immediately *after* pushing a new frame. The pc of
  // the top frame will be reset to 0 if the stack check fails.
  bool DoStackCheck() WARN_UNUSED_RESULT {
    // Sum up the size of all dynamically growing structures.
    if (V8_LIKELY(frames_.size() <= kV8MaxWasmInterpretedStackSize)) {
      return true;
    }
    if (!codemap()->has_instance()) {
      // In test mode: Just abort.
      FATAL("wasm interpreter: stack overflow");
    }
    // The pc of the top frame is initialized to the first instruction. We reset
    // it to 0 here such that we report the same position as in compiled code.
    frames_.back().pc = 0;
    Isolate* isolate = codemap()->instance()->GetIsolate();
    HandleScope handle_scope(isolate);
    isolate->StackOverflow();
    return HandleException(isolate) == WasmInterpreter::Thread::HANDLED;
  }

  void Execute(InterpreterCode* code, pc_t pc, int max) {
    Decoder decoder(code->start, code->end);
    pc_t limit = code->end - code->start;
    bool hit_break = false;

    while (true) {
#define PAUSE_IF_BREAK_FLAG(flag)                                     \
  if (V8_UNLIKELY(break_flags_ & WasmInterpreter::BreakFlag::flag)) { \
    hit_break = true;                                                 \
    max = 0;                                                          \
  }

      DCHECK_GT(limit, pc);
      DCHECK_NOT_NULL(code->start);

      // Do first check for a breakpoint, in order to set hit_break correctly.
      const char* skip = "        ";
      int len = 1;
      byte opcode = code->start[pc];
      byte orig = opcode;
      if (V8_UNLIKELY(opcode == kInternalBreakpoint)) {
        orig = code->orig_start[pc];
        if (SkipBreakpoint(code, pc)) {
          // skip breakpoint by switching on original code.
          skip = "[skip]  ";
        } else {
          TRACE("@%-3zu: [break] %-24s:", pc,
                WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(orig)));
          TraceValueStack();
          TRACE("\n");
          hit_break = true;
          break;
        }
      }

      // If max is 0, break. If max is positive (a limit is set), decrement it.
      if (max == 0) break;
      if (max > 0) --max;

      USE(skip);
      TRACE("@%-3zu: %s%-24s:", pc, skip,
            WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(orig)));
      TraceValueStack();
      TRACE("\n");

      switch (orig) {
        case kExprNop:
          break;
        case kExprBlock: {
          BlockTypeOperand<false> operand(&decoder, code->at(pc));
          blocks_.push_back({pc, stack_.size(), frames_.size(), operand.arity});
          len = 1 + operand.length;
          break;
        }
        case kExprLoop: {
          BlockTypeOperand<false> operand(&decoder, code->at(pc));
          blocks_.push_back({pc, stack_.size(), frames_.size(), 0});
          len = 1 + operand.length;
          break;
        }
        case kExprIf: {
          BlockTypeOperand<false> operand(&decoder, code->at(pc));
          WasmVal cond = Pop();
          bool is_true = cond.to<uint32_t>() != 0;
          blocks_.push_back({pc, stack_.size(), frames_.size(), operand.arity});
          if (is_true) {
            // fall through to the true block.
            len = 1 + operand.length;
            TRACE("  true => fallthrough\n");
          } else {
            len = LookupTarget(code, pc);
            TRACE("  false => @%zu\n", pc + len);
          }
          break;
        }
        case kExprElse: {
          blocks_.pop_back();
          len = LookupTarget(code, pc);
          TRACE("  end => @%zu\n", pc + len);
          break;
        }
        case kExprSelect: {
          WasmVal cond = Pop();
          WasmVal fval = Pop();
          WasmVal tval = Pop();
          Push(pc, cond.to<int32_t>() != 0 ? tval : fval);
          break;
        }
        case kExprBr: {
          BreakDepthOperand<false> operand(&decoder, code->at(pc));
          len = DoBreak(code, pc, operand.depth);
          TRACE("  br => @%zu\n", pc + len);
          break;
        }
        case kExprBrIf: {
          BreakDepthOperand<false> operand(&decoder, code->at(pc));
          WasmVal cond = Pop();
          bool is_true = cond.to<uint32_t>() != 0;
          if (is_true) {
            len = DoBreak(code, pc, operand.depth);
            TRACE("  br_if => @%zu\n", pc + len);
          } else {
            TRACE("  false => fallthrough\n");
            len = 1 + operand.length;
          }
          break;
        }
        case kExprBrTable: {
          BranchTableOperand<false> operand(&decoder, code->at(pc));
          BranchTableIterator<false> iterator(&decoder, operand);
          uint32_t key = Pop().to<uint32_t>();
          uint32_t depth = 0;
          if (key >= operand.table_count) key = operand.table_count;
          for (uint32_t i = 0; i <= key; i++) {
            DCHECK(iterator.has_next());
            depth = iterator.next();
          }
          len = key + DoBreak(code, pc + key, static_cast<size_t>(depth));
          TRACE("  br[%u] => @%zu\n", key, pc + key + len);
          break;
        }
        case kExprReturn: {
          size_t arity = code->function->sig->return_count();
          if (!DoReturn(&decoder, &code, &pc, &limit, arity)) return;
          PAUSE_IF_BREAK_FLAG(AfterReturn);
          continue;
        }
        case kExprUnreachable: {
          return DoTrap(kTrapUnreachable, pc);
        }
        case kExprEnd: {
          blocks_.pop_back();
          break;
        }
        case kExprI32Const: {
          ImmI32Operand<false> operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprI64Const: {
          ImmI64Operand<false> operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprF32Const: {
          ImmF32Operand<false> operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprF64Const: {
          ImmF64Operand<false> operand(&decoder, code->at(pc));
          Push(pc, WasmVal(operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprGetLocal: {
          LocalIndexOperand<false> operand(&decoder, code->at(pc));
          Push(pc, stack_[frames_.back().sp + operand.index]);
          len = 1 + operand.length;
          break;
        }
        case kExprSetLocal: {
          LocalIndexOperand<false> operand(&decoder, code->at(pc));
          WasmVal val = Pop();
          stack_[frames_.back().sp + operand.index] = val;
          len = 1 + operand.length;
          break;
        }
        case kExprTeeLocal: {
          LocalIndexOperand<false> operand(&decoder, code->at(pc));
          WasmVal val = Pop();
          stack_[frames_.back().sp + operand.index] = val;
          Push(pc, val);
          len = 1 + operand.length;
          break;
        }
        case kExprDrop: {
          Pop();
          break;
        }
        case kExprCallFunction: {
          CallFunctionOperand<false> operand(&decoder, code->at(pc));
          InterpreterCode* target = codemap()->GetCode(operand.index);
          if (target->function->imported) {
            CommitPc(pc);
            ExternalCallResult result =
                CallImportedFunction(target->function->func_index);
            switch (result.type) {
              case ExternalCallResult::INTERNAL:
                // The import is a function of this instance. Call it directly.
                target = result.interpreter_code;
                DCHECK(!target->function->imported);
                break;
              case ExternalCallResult::INVALID_FUNC:
              case ExternalCallResult::SIGNATURE_MISMATCH:
                // Direct calls are checked statically.
                UNREACHABLE();
              case ExternalCallResult::EXTERNAL_RETURNED:
                PAUSE_IF_BREAK_FLAG(AfterCall);
                len = 1 + operand.length;
                break;
              case ExternalCallResult::EXTERNAL_UNWOUND:
                return;
            }
            if (result.type != ExternalCallResult::INTERNAL) break;
          }
          // Execute an internal call.
          if (!DoCall(&decoder, target, &pc, &limit)) return;
          code = target;
          PAUSE_IF_BREAK_FLAG(AfterCall);
          continue;  // don't bump pc
        } break;
        case kExprCallIndirect: {
          CallIndirectOperand<false> operand(&decoder, code->at(pc));
          uint32_t entry_index = Pop().to<uint32_t>();
          // Assume only one table for now.
          DCHECK_LE(module()->function_tables.size(), 1u);
          ExternalCallResult result =
              CallIndirectFunction(0, entry_index, operand.index);
          switch (result.type) {
            case ExternalCallResult::INTERNAL:
              // The import is a function of this instance. Call it directly.
              if (!DoCall(&decoder, result.interpreter_code, &pc, &limit))
                return;
              code = result.interpreter_code;
              PAUSE_IF_BREAK_FLAG(AfterCall);
              continue;  // don't bump pc
            case ExternalCallResult::INVALID_FUNC:
              return DoTrap(kTrapFuncInvalid, pc);
            case ExternalCallResult::SIGNATURE_MISMATCH:
              return DoTrap(kTrapFuncSigMismatch, pc);
            case ExternalCallResult::EXTERNAL_RETURNED:
              PAUSE_IF_BREAK_FLAG(AfterCall);
              len = 1 + operand.length;
              break;
            case ExternalCallResult::EXTERNAL_UNWOUND:
              return;
          }
        } break;
        case kExprGetGlobal: {
          GlobalIndexOperand<false> operand(&decoder, code->at(pc));
          const WasmGlobal* global = &module()->globals[operand.index];
          byte* ptr = instance()->globals_start + global->offset;
          WasmVal val;
          switch (global->type) {
#define CASE_TYPE(wasm, ctype)                     \
  case kWasm##wasm:                                \
    val = WasmVal(*reinterpret_cast<ctype*>(ptr)); \
    break;
            WASM_CTYPES(CASE_TYPE)
#undef CASE_TYPE
            default:
              UNREACHABLE();
          }
          Push(pc, val);
          len = 1 + operand.length;
          break;
        }
        case kExprSetGlobal: {
          GlobalIndexOperand<false> operand(&decoder, code->at(pc));
          const WasmGlobal* global = &module()->globals[operand.index];
          byte* ptr = instance()->globals_start + global->offset;
          WasmVal val = Pop();
          switch (global->type) {
#define CASE_TYPE(wasm, ctype)                        \
  case kWasm##wasm:                                   \
    *reinterpret_cast<ctype*>(ptr) = val.to<ctype>(); \
    break;
            WASM_CTYPES(CASE_TYPE)
#undef CASE_TYPE
            default:
              UNREACHABLE();
          }
          len = 1 + operand.length;
          break;
        }

#define LOAD_CASE(name, ctype, mtype)                                \
  case kExpr##name: {                                                \
    if (!ExecuteLoad<ctype, mtype>(&decoder, code, pc, len)) return; \
    break;                                                           \
  }

          LOAD_CASE(I32LoadMem8S, int32_t, int8_t);
          LOAD_CASE(I32LoadMem8U, int32_t, uint8_t);
          LOAD_CASE(I32LoadMem16S, int32_t, int16_t);
          LOAD_CASE(I32LoadMem16U, int32_t, uint16_t);
          LOAD_CASE(I64LoadMem8S, int64_t, int8_t);
          LOAD_CASE(I64LoadMem8U, int64_t, uint8_t);
          LOAD_CASE(I64LoadMem16S, int64_t, int16_t);
          LOAD_CASE(I64LoadMem16U, int64_t, uint16_t);
          LOAD_CASE(I64LoadMem32S, int64_t, int32_t);
          LOAD_CASE(I64LoadMem32U, int64_t, uint32_t);
          LOAD_CASE(I32LoadMem, int32_t, int32_t);
          LOAD_CASE(I64LoadMem, int64_t, int64_t);
          LOAD_CASE(F32LoadMem, float, float);
          LOAD_CASE(F64LoadMem, double, double);
#undef LOAD_CASE

#define STORE_CASE(name, ctype, mtype)                                \
  case kExpr##name: {                                                 \
    if (!ExecuteStore<ctype, mtype>(&decoder, code, pc, len)) return; \
    break;                                                            \
  }

          STORE_CASE(I32StoreMem8, int32_t, int8_t);
          STORE_CASE(I32StoreMem16, int32_t, int16_t);
          STORE_CASE(I64StoreMem8, int64_t, int8_t);
          STORE_CASE(I64StoreMem16, int64_t, int16_t);
          STORE_CASE(I64StoreMem32, int64_t, int32_t);
          STORE_CASE(I32StoreMem, int32_t, int32_t);
          STORE_CASE(I64StoreMem, int64_t, int64_t);
          STORE_CASE(F32StoreMem, float, float);
          STORE_CASE(F64StoreMem, double, double);
#undef STORE_CASE

#define ASMJS_LOAD_CASE(name, ctype, mtype, defval)                 \
  case kExpr##name: {                                               \
    uint32_t index = Pop().to<uint32_t>();                          \
    ctype result;                                                   \
    if (!BoundsCheck<mtype>(instance()->mem_size, 0, index)) {      \
      result = defval;                                              \
    } else {                                                        \
      byte* addr = instance()->mem_start + index;                   \
      /* TODO(titzer): alignment for asmjs load mem? */             \
      result = static_cast<ctype>(*reinterpret_cast<mtype*>(addr)); \
    }                                                               \
    Push(pc, WasmVal(result));                                      \
    break;                                                          \
  }
          ASMJS_LOAD_CASE(I32AsmjsLoadMem8S, int32_t, int8_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem8U, int32_t, uint8_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem16S, int32_t, int16_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem16U, int32_t, uint16_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem, int32_t, int32_t, 0);
          ASMJS_LOAD_CASE(F32AsmjsLoadMem, float, float,
                          std::numeric_limits<float>::quiet_NaN());
          ASMJS_LOAD_CASE(F64AsmjsLoadMem, double, double,
                          std::numeric_limits<double>::quiet_NaN());
#undef ASMJS_LOAD_CASE

#define ASMJS_STORE_CASE(name, ctype, mtype)                                   \
  case kExpr##name: {                                                          \
    WasmVal val = Pop();                                                       \
    uint32_t index = Pop().to<uint32_t>();                                     \
    if (BoundsCheck<mtype>(instance()->mem_size, 0, index)) {                  \
      byte* addr = instance()->mem_start + index;                              \
      /* TODO(titzer): alignment for asmjs store mem? */                       \
      *(reinterpret_cast<mtype*>(addr)) = static_cast<mtype>(val.to<ctype>()); \
    }                                                                          \
    Push(pc, val);                                                             \
    break;                                                                     \
  }

          ASMJS_STORE_CASE(I32AsmjsStoreMem8, int32_t, int8_t);
          ASMJS_STORE_CASE(I32AsmjsStoreMem16, int32_t, int16_t);
          ASMJS_STORE_CASE(I32AsmjsStoreMem, int32_t, int32_t);
          ASMJS_STORE_CASE(F32AsmjsStoreMem, float, float);
          ASMJS_STORE_CASE(F64AsmjsStoreMem, double, double);
#undef ASMJS_STORE_CASE
        case kExprGrowMemory: {
          MemoryIndexOperand<false> operand(&decoder, code->at(pc));
          uint32_t delta_pages = Pop().to<uint32_t>();
          Push(pc, WasmVal(ExecuteGrowMemory(
                       delta_pages, codemap_->maybe_instance(), instance())));
          len = 1 + operand.length;
          break;
        }
        case kExprMemorySize: {
          MemoryIndexOperand<false> operand(&decoder, code->at(pc));
          Push(pc, WasmVal(static_cast<uint32_t>(instance()->mem_size /
                                                 WasmModule::kPageSize)));
          len = 1 + operand.length;
          break;
        }
        // We need to treat kExprI32ReinterpretF32 and kExprI64ReinterpretF64
        // specially to guarantee that the quiet bit of a NaN is preserved on
        // ia32 by the reinterpret casts.
        case kExprI32ReinterpretF32: {
          WasmVal val = Pop();
          WasmVal result(ExecuteI32ReinterpretF32(val));
          Push(pc, result);
          possible_nondeterminism_ |= std::isnan(val.to<float>());
          break;
        }
        case kExprI64ReinterpretF64: {
          WasmVal val = Pop();
          WasmVal result(ExecuteI64ReinterpretF64(val));
          Push(pc, result);
          possible_nondeterminism_ |= std::isnan(val.to<double>());
          break;
        }
#define EXECUTE_SIMPLE_BINOP(name, ctype, op)             \
  case kExpr##name: {                                     \
    WasmVal rval = Pop();                                 \
    WasmVal lval = Pop();                                 \
    WasmVal result(lval.to<ctype>() op rval.to<ctype>()); \
    Push(pc, result);                                     \
    break;                                                \
  }
          FOREACH_SIMPLE_BINOP(EXECUTE_SIMPLE_BINOP)
#undef EXECUTE_SIMPLE_BINOP

#define EXECUTE_OTHER_BINOP(name, ctype)              \
  case kExpr##name: {                                 \
    TrapReason trap = kTrapCount;                     \
    volatile ctype rval = Pop().to<ctype>();          \
    volatile ctype lval = Pop().to<ctype>();          \
    WasmVal result(Execute##name(lval, rval, &trap)); \
    if (trap != kTrapCount) return DoTrap(trap, pc);  \
    Push(pc, result);                                 \
    break;                                            \
  }
          FOREACH_OTHER_BINOP(EXECUTE_OTHER_BINOP)
#undef EXECUTE_OTHER_BINOP

        case kExprF32CopySign: {
          // Handle kExprF32CopySign separately because it may introduce
          // observable non-determinism.
          TrapReason trap = kTrapCount;
          volatile float rval = Pop().to<float>();
          volatile float lval = Pop().to<float>();
          WasmVal result(ExecuteF32CopySign(lval, rval, &trap));
          Push(pc, result);
          possible_nondeterminism_ |= std::isnan(rval);
          break;
        }
        case kExprF64CopySign: {
          // Handle kExprF32CopySign separately because it may introduce
          // observable non-determinism.
          TrapReason trap = kTrapCount;
          volatile double rval = Pop().to<double>();
          volatile double lval = Pop().to<double>();
          WasmVal result(ExecuteF64CopySign(lval, rval, &trap));
          Push(pc, result);
          possible_nondeterminism_ |= std::isnan(rval);
          break;
        }
#define EXECUTE_OTHER_UNOP(name, ctype)              \
  case kExpr##name: {                                \
    TrapReason trap = kTrapCount;                    \
    volatile ctype val = Pop().to<ctype>();          \
    WasmVal result(Execute##name(val, &trap));       \
    if (trap != kTrapCount) return DoTrap(trap, pc); \
    Push(pc, result);                                \
    break;                                           \
  }
          FOREACH_OTHER_UNOP(EXECUTE_OTHER_UNOP)
#undef EXECUTE_OTHER_UNOP

        default:
          V8_Fatal(__FILE__, __LINE__, "Unknown or unimplemented opcode #%d:%s",
                   code->start[pc], OpcodeName(code->start[pc]));
          UNREACHABLE();
      }

      pc += len;
      if (pc == limit) {
        // Fell off end of code; do an implicit return.
        TRACE("@%-3zu: ImplicitReturn\n", pc);
        if (!DoReturn(&decoder, &code, &pc, &limit,
                      code->function->sig->return_count()))
          return;
        PAUSE_IF_BREAK_FLAG(AfterReturn);
      }
    }

    state_ = WasmInterpreter::PAUSED;
    break_pc_ = hit_break ? pc : kInvalidPc;
    CommitPc(pc);
  }

  WasmVal Pop() {
    DCHECK_GT(stack_.size(), 0);
    DCHECK_GT(frames_.size(), 0);
    DCHECK_GT(stack_.size(), frames_.back().llimit());  // can't pop into locals
    WasmVal val = stack_.back();
    stack_.pop_back();
    return val;
  }

  void PopN(int n) {
    DCHECK_GE(stack_.size(), n);
    DCHECK_GT(frames_.size(), 0);
    size_t nsize = stack_.size() - n;
    DCHECK_GE(nsize, frames_.back().llimit());  // can't pop into locals
    stack_.resize(nsize);
  }

  WasmVal PopArity(size_t arity) {
    if (arity == 0) return WasmVal();
    CHECK_EQ(1, arity);
    return Pop();
  }

  void Push(pc_t pc, WasmVal val) {
    // TODO(titzer): store PC as well?
    DCHECK_NE(kWasmStmt, val.type);
    stack_.push_back(val);
  }

  void TraceStack(const char* phase, pc_t pc) {
    if (FLAG_trace_wasm_interpreter) {
      PrintF("%s @%zu", phase, pc);
      UNIMPLEMENTED();
      PrintF("\n");
    }
  }

  void TraceValueStack() {
#ifdef DEBUG
    Frame* top = frames_.size() > 0 ? &frames_.back() : nullptr;
    sp_t sp = top ? top->sp : 0;
    sp_t plimit = top ? top->plimit() : 0;
    sp_t llimit = top ? top->llimit() : 0;
    if (FLAG_trace_wasm_interpreter) {
      for (size_t i = sp; i < stack_.size(); ++i) {
        if (i < plimit)
          PrintF(" p%zu:", i);
        else if (i < llimit)
          PrintF(" l%zu:", i);
        else
          PrintF(" s%zu:", i);
        WasmVal val = stack_[i];
        switch (val.type) {
          case kWasmI32:
            PrintF("i32:%d", val.to<int32_t>());
            break;
          case kWasmI64:
            PrintF("i64:%" PRId64 "", val.to<int64_t>());
            break;
          case kWasmF32:
            PrintF("f32:%f", val.to<float>());
            break;
          case kWasmF64:
            PrintF("f64:%lf", val.to<double>());
            break;
          case kWasmStmt:
            PrintF("void");
            break;
          default:
            UNREACHABLE();
            break;
        }
      }
    }
#endif  // DEBUG
  }

  ExternalCallResult TryHandleException(Isolate* isolate) {
    if (HandleException(isolate) == WasmInterpreter::Thread::UNWOUND) {
      return {ExternalCallResult::EXTERNAL_UNWOUND};
    }
    return {ExternalCallResult::EXTERNAL_RETURNED};
  }

  ExternalCallResult CallCodeObject(Isolate* isolate, Handle<Code> code,
                                    FunctionSig* signature) {
    DCHECK(AllowHandleAllocation::IsAllowed());
    DCHECK(AllowHeapAllocation::IsAllowed());

    if (code->kind() == Code::WASM_FUNCTION) {
      FixedArray* deopt_data = code->deoptimization_data();
      DCHECK_EQ(2, deopt_data->length());
      WasmInstanceObject* target_instance =
          WasmInstanceObject::cast(WeakCell::cast(deopt_data->get(0))->value());
      if (target_instance != *codemap()->instance()) {
        // TODO(wasm): Implement calling functions of other instances/modules.
        UNIMPLEMENTED();
      }
      int target_func_idx = Smi::cast(deopt_data->get(1))->value();
      DCHECK_LE(0, target_func_idx);
      return {ExternalCallResult::INTERNAL,
              codemap()->GetCode(target_func_idx)};
    }

    Handle<HeapObject> target =
        codemap()->GetCallableObjectForJSImport(isolate, code);

    if (target.is_null()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kWasmTrapTypeError));
      return TryHandleException(isolate);
    }

#if DEBUG
    std::ostringstream oss;
    target->HeapObjectShortPrint(oss);
    TRACE("  => Calling imported function %s\n", oss.str().c_str());
#endif

    int num_args = static_cast<int>(signature->parameter_count());

    // Get all arguments as JS values.
    std::vector<Handle<Object>> args;
    args.reserve(num_args);
    WasmVal* wasm_args = stack_.data() + (stack_.size() - num_args);
    for (int i = 0; i < num_args; ++i) {
      args.push_back(WasmValToNumber(isolate->factory(), wasm_args[i],
                                     signature->GetParam(i)));
    }

    // The receiver is the global proxy if in sloppy mode (default), undefined
    // if in strict mode.
    Handle<Object> receiver = isolate->global_proxy();
    if (target->IsJSFunction() &&
        is_strict(JSFunction::cast(*target)->shared()->language_mode())) {
      receiver = isolate->factory()->undefined_value();
    }

    MaybeHandle<Object> maybe_retval =
        Execution::Call(isolate, target, receiver, num_args, args.data());
    if (maybe_retval.is_null()) return TryHandleException(isolate);

    Handle<Object> retval = maybe_retval.ToHandleChecked();
    // Pop arguments off the stack.
    stack_.resize(stack_.size() - num_args);
    if (signature->return_count() > 0) {
      // TODO(wasm): Handle multiple returns.
      DCHECK_EQ(1, signature->return_count());
      stack_.push_back(
          ToWebAssemblyValue(isolate, retval, signature->GetReturn()));
    }
    return {ExternalCallResult::EXTERNAL_RETURNED};
  }

  ExternalCallResult CallImportedFunction(uint32_t function_index) {
    // Use a new HandleScope to avoid leaking / accumulating handles in the
    // outer scope.
    Isolate* isolate = codemap()->instance()->GetIsolate();
    HandleScope handle_scope(isolate);

    Handle<Code> target(codemap()->GetImportedFunction(function_index),
                        isolate);
    return CallCodeObject(isolate, target,
                          codemap()->module()->functions[function_index].sig);
  }

  ExternalCallResult CallIndirectFunction(uint32_t table_index,
                                          uint32_t entry_index,
                                          uint32_t sig_index) {
    if (!codemap()->has_instance()) {
      // No instance. Rely on the information stored in the WasmModule.
      // TODO(wasm): This is only needed for testing. Refactor testing to use
      // the same paths as production.
      InterpreterCode* code =
          codemap()->GetIndirectCode(table_index, entry_index);
      if (!code) return {ExternalCallResult::INVALID_FUNC};
      if (code->function->sig_index != sig_index) {
        // If not an exact match, we have to do a canonical check.
        // TODO(titzer): make this faster with some kind of caching?
        const WasmIndirectFunctionTable* table =
            &module()->function_tables[table_index];
        int function_key = table->map.Find(code->function->sig);
        if (function_key < 0 ||
            (function_key !=
             table->map.Find(module()->signatures[sig_index]))) {
          return {ExternalCallResult::SIGNATURE_MISMATCH};
        }
      }
      return {ExternalCallResult::INTERNAL, code};
    }

    WasmCompiledModule* compiled_module =
        codemap()->instance()->compiled_module();
    Isolate* isolate = compiled_module->GetIsolate();

    Code* target;
    {
      DisallowHeapAllocation no_gc;
      // Get function to be called directly from the live instance to see latest
      // changes to the tables.

      // Canonicalize signature index.
      // TODO(titzer): make this faster with some kind of caching?
      const WasmIndirectFunctionTable* table =
          &module()->function_tables[table_index];
      FunctionSig* sig = module()->signatures[sig_index];
      uint32_t canonical_sig_index = table->map.Find(sig);

      // Check signature.
      FixedArray* sig_tables = compiled_module->ptr_to_signature_tables();
      if (table_index >= static_cast<uint32_t>(sig_tables->length())) {
        return {ExternalCallResult::INVALID_FUNC};
      }
      FixedArray* sig_table =
          FixedArray::cast(sig_tables->get(static_cast<int>(table_index)));
      if (entry_index >= static_cast<uint32_t>(sig_table->length())) {
        return {ExternalCallResult::INVALID_FUNC};
      }
      int found_sig =
          Smi::cast(sig_table->get(static_cast<int>(entry_index)))->value();
      if (static_cast<uint32_t>(found_sig) != canonical_sig_index) {
        return {ExternalCallResult::SIGNATURE_MISMATCH};
      }

      // Get code object.
      FixedArray* fun_tables = compiled_module->ptr_to_function_tables();
      DCHECK_EQ(sig_tables->length(), fun_tables->length());
      FixedArray* fun_table =
          FixedArray::cast(fun_tables->get(static_cast<int>(table_index)));
      DCHECK_EQ(sig_table->length(), fun_table->length());
      target = Code::cast(fun_table->get(static_cast<int>(entry_index)));
    }

    // Call the code object. Use a new HandleScope to avoid leaking /
    // accumulating handles in the outer scope.
    HandleScope handle_scope(isolate);
    FunctionSig* signature =
        &codemap()->module()->signatures[table_index][sig_index];
    return CallCodeObject(isolate, handle(target, isolate), signature);
  }

  inline Activation current_activation() {
    return activations_.empty() ? Activation(0, 0) : activations_.back();
  }
};

// Converters between WasmInterpreter::Thread and WasmInterpreter::ThreadImpl.
// Thread* is the public interface, without knowledge of the object layout.
// This cast is potentially risky, but as long as we always cast it back before
// accessing any data, it should be fine. UBSan is not complaining.
WasmInterpreter::Thread* ToThread(ThreadImpl* impl) {
  return reinterpret_cast<WasmInterpreter::Thread*>(impl);
}
ThreadImpl* ToImpl(WasmInterpreter::Thread* thread) {
  return reinterpret_cast<ThreadImpl*>(thread);
}

}  // namespace

//============================================================================
// Implementation of the pimpl idiom for WasmInterpreter::Thread.
// Instead of placing a pointer to the ThreadImpl inside of the Thread object,
// we just reinterpret_cast them. ThreadImpls are only allocated inside this
// translation unit anyway.
//============================================================================
WasmInterpreter::State WasmInterpreter::Thread::state() {
  return ToImpl(this)->state();
}
void WasmInterpreter::Thread::InitFrame(const WasmFunction* function,
                                        WasmVal* args) {
  ToImpl(this)->InitFrame(function, args);
}
WasmInterpreter::State WasmInterpreter::Thread::Run(int num_steps) {
  return ToImpl(this)->Run(num_steps);
}
void WasmInterpreter::Thread::Pause() { return ToImpl(this)->Pause(); }
void WasmInterpreter::Thread::Reset() { return ToImpl(this)->Reset(); }
WasmInterpreter::Thread::ExceptionHandlingResult
WasmInterpreter::Thread::HandleException(Isolate* isolate) {
  return ToImpl(this)->HandleException(isolate);
}
pc_t WasmInterpreter::Thread::GetBreakpointPc() {
  return ToImpl(this)->GetBreakpointPc();
}
int WasmInterpreter::Thread::GetFrameCount() {
  return ToImpl(this)->GetFrameCount();
}
const InterpretedFrame WasmInterpreter::Thread::GetFrame(int index) {
  return GetMutableFrame(index);
}
InterpretedFrame WasmInterpreter::Thread::GetMutableFrame(int index) {
  // We have access to the constructor of InterpretedFrame, but ThreadImpl has
  // not. So pass it as a lambda (should all get inlined).
  auto frame_cons = [](const WasmFunction* function, int pc, int fp, int sp) {
    return InterpretedFrame(function, pc, fp, sp);
  };
  return ToImpl(this)->GetMutableFrame(index, frame_cons);
}
WasmVal WasmInterpreter::Thread::GetReturnValue(int index) {
  return ToImpl(this)->GetReturnValue(index);
}
TrapReason WasmInterpreter::Thread::GetTrapReason() {
  return ToImpl(this)->GetTrapReason();
}
bool WasmInterpreter::Thread::PossibleNondeterminism() {
  return ToImpl(this)->PossibleNondeterminism();
}
uint64_t WasmInterpreter::Thread::NumInterpretedCalls() {
  return ToImpl(this)->NumInterpretedCalls();
}
void WasmInterpreter::Thread::AddBreakFlags(uint8_t flags) {
  ToImpl(this)->AddBreakFlags(flags);
}
void WasmInterpreter::Thread::ClearBreakFlags() {
  ToImpl(this)->ClearBreakFlags();
}
uint32_t WasmInterpreter::Thread::NumActivations() {
  return ToImpl(this)->NumActivations();
}
uint32_t WasmInterpreter::Thread::StartActivation() {
  return ToImpl(this)->StartActivation();
}
void WasmInterpreter::Thread::FinishActivation(uint32_t id) {
  ToImpl(this)->FinishActivation(id);
}
uint32_t WasmInterpreter::Thread::ActivationFrameBase(uint32_t id) {
  return ToImpl(this)->ActivationFrameBase(id);
}

//============================================================================
// The implementation details of the interpreter.
//============================================================================
class WasmInterpreterInternals : public ZoneObject {
 public:
  WasmInstance* instance_;
  // Create a copy of the module bytes for the interpreter, since the passed
  // pointer might be invalidated after constructing the interpreter.
  const ZoneVector<uint8_t> module_bytes_;
  CodeMap codemap_;
  ZoneVector<ThreadImpl> threads_;

  WasmInterpreterInternals(Isolate* isolate, Zone* zone,
                           const ModuleBytesEnv& env)
      : instance_(env.module_env.instance),
        module_bytes_(env.wire_bytes.start(), env.wire_bytes.end(), zone),
        codemap_(
            isolate,
            env.module_env.instance ? env.module_env.instance->module : nullptr,
            module_bytes_.data(), zone),
        threads_(zone) {
    threads_.emplace_back(zone, &codemap_, env.module_env.instance);
  }
};

//============================================================================
// Implementation of the public interface of the interpreter.
//============================================================================
WasmInterpreter::WasmInterpreter(Isolate* isolate, const ModuleBytesEnv& env)
    : zone_(isolate->allocator(), ZONE_NAME),
      internals_(new (&zone_) WasmInterpreterInternals(isolate, &zone_, env)) {}

WasmInterpreter::~WasmInterpreter() { internals_->~WasmInterpreterInternals(); }

void WasmInterpreter::Run() { internals_->threads_[0].Run(); }

void WasmInterpreter::Pause() { internals_->threads_[0].Pause(); }

bool WasmInterpreter::SetBreakpoint(const WasmFunction* function, pc_t pc,
                                    bool enabled) {
  InterpreterCode* code = internals_->codemap_.GetCode(function);
  size_t size = static_cast<size_t>(code->end - code->start);
  // Check bounds for {pc}.
  if (pc < code->locals.encoded_size || pc >= size) return false;
  // Make a copy of the code before enabling a breakpoint.
  if (enabled && code->orig_start == code->start) {
    code->start = reinterpret_cast<byte*>(zone_.New(size));
    memcpy(code->start, code->orig_start, size);
    code->end = code->start + size;
  }
  bool prev = code->start[pc] == kInternalBreakpoint;
  if (enabled) {
    code->start[pc] = kInternalBreakpoint;
  } else {
    code->start[pc] = code->orig_start[pc];
  }
  return prev;
}

bool WasmInterpreter::GetBreakpoint(const WasmFunction* function, pc_t pc) {
  InterpreterCode* code = internals_->codemap_.GetCode(function);
  size_t size = static_cast<size_t>(code->end - code->start);
  // Check bounds for {pc}.
  if (pc < code->locals.encoded_size || pc >= size) return false;
  // Check if a breakpoint is present at that place in the code.
  return code->start[pc] == kInternalBreakpoint;
}

bool WasmInterpreter::SetTracing(const WasmFunction* function, bool enabled) {
  UNIMPLEMENTED();
  return false;
}

void WasmInterpreter::SetInstanceObject(WasmInstanceObject* instance) {
  internals_->codemap_.SetInstanceObject(instance);
}

int WasmInterpreter::GetThreadCount() {
  return 1;  // only one thread for now.
}

WasmInterpreter::Thread* WasmInterpreter::GetThread(int id) {
  CHECK_EQ(0, id);  // only one thread for now.
  return ToThread(&internals_->threads_[id]);
}

size_t WasmInterpreter::GetMemorySize() {
  return internals_->instance_->mem_size;
}

WasmVal WasmInterpreter::ReadMemory(size_t offset) {
  UNIMPLEMENTED();
  return WasmVal();
}

void WasmInterpreter::WriteMemory(size_t offset, WasmVal val) {
  UNIMPLEMENTED();
}

void WasmInterpreter::AddFunctionForTesting(const WasmFunction* function) {
  internals_->codemap_.AddFunction(function, nullptr, nullptr);
}

void WasmInterpreter::SetFunctionCodeForTesting(const WasmFunction* function,
                                                const byte* start,
                                                const byte* end) {
  internals_->codemap_.SetFunctionCode(function, start, end);
}

ControlTransferMap WasmInterpreter::ComputeControlTransfersForTesting(
    Zone* zone, const byte* start, const byte* end) {
  ControlTransfers targets(zone, nullptr, start, end);
  return targets.map_;
}

//============================================================================
// Implementation of the frame inspection interface.
//============================================================================
int InterpretedFrame::GetParameterCount() const {
  USE(fp_);
  USE(sp_);
  // TODO(clemensh): Return the correct number of parameters.
  return 0;
}

WasmVal InterpretedFrame::GetLocalVal(int index) const {
  CHECK_GE(index, 0);
  UNIMPLEMENTED();
  WasmVal none;
  none.type = kWasmStmt;
  return none;
}

WasmVal InterpretedFrame::GetExprVal(int pc) const {
  UNIMPLEMENTED();
  WasmVal none;
  none.type = kWasmStmt;
  return none;
}

void InterpretedFrame::SetLocalVal(int index, WasmVal val) { UNIMPLEMENTED(); }

void InterpretedFrame::SetExprVal(int pc, WasmVal val) { UNIMPLEMENTED(); }

}  // namespace wasm
}  // namespace internal
}  // namespace v8
