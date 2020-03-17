#include <math.h>
#include <string.h>

#include "increment.h"
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x) __builtin_expect(!!(x), 1)

#define TRAP(x) (wasm_rt_trap(WASM_RT_TRAP_##x), 0)

#define FUNC_PROLOGUE                                            \
  if (++wasm_rt_call_stack_depth > WASM_RT_MAX_CALL_STACK_DEPTH) \
    TRAP(EXHAUSTION)

#define FUNC_EPILOGUE --wasm_rt_call_stack_depth

#define UNREACHABLE TRAP(UNREACHABLE)

#define CALL_INDIRECT(table, t, ft, x, ...)          \
  (LIKELY((x) < table.size && table.data[x].func &&  \
          table.data[x].func_type == func_types[ft]) \
       ? ((t)table.data[x].func)(__VA_ARGS__)        \
       : TRAP(CALL_INDIRECT))

#define MEMCHECK(mem, a, t)  \
  if (UNLIKELY((a) + sizeof(t) > mem->size)) TRAP(OOB)

#define DEFINE_LOAD(name, t1, t2, t3)              \
  static inline t3 name(wasm_rt_memory_t* mem, u64 addr) {   \
    MEMCHECK(mem, addr, t1);                       \
    t1 result;                                     \
    memcpy(&result, &mem->data[addr], sizeof(t1)); \
    return (t3)(t2)result;                         \
  }

#define DEFINE_STORE(name, t1, t2)                           \
  static inline void name(wasm_rt_memory_t* mem, u64 addr, t2 value) { \
    MEMCHECK(mem, addr, t1);                                 \
    t1 wrapped = (t1)value;                                  \
    memcpy(&mem->data[addr], &wrapped, sizeof(t1));          \
  }

DEFINE_LOAD(i32_load, u32, u32, u32);
DEFINE_LOAD(i64_load, u64, u64, u64);
DEFINE_LOAD(f32_load, f32, f32, f32);
DEFINE_LOAD(f64_load, f64, f64, f64);
DEFINE_LOAD(i32_load8_s, s8, s32, u32);
DEFINE_LOAD(i64_load8_s, s8, s64, u64);
DEFINE_LOAD(i32_load8_u, u8, u32, u32);
DEFINE_LOAD(i64_load8_u, u8, u64, u64);
DEFINE_LOAD(i32_load16_s, s16, s32, u32);
DEFINE_LOAD(i64_load16_s, s16, s64, u64);
DEFINE_LOAD(i32_load16_u, u16, u32, u32);
DEFINE_LOAD(i64_load16_u, u16, u64, u64);
DEFINE_LOAD(i64_load32_s, s32, s64, u64);
DEFINE_LOAD(i64_load32_u, u32, u64, u64);
DEFINE_STORE(i32_store, u32, u32);
DEFINE_STORE(i64_store, u64, u64);
DEFINE_STORE(f32_store, f32, f32);
DEFINE_STORE(f64_store, f64, f64);
DEFINE_STORE(i32_store8, u8, u32);
DEFINE_STORE(i32_store16, u16, u32);
DEFINE_STORE(i64_store8, u8, u64);
DEFINE_STORE(i64_store16, u16, u64);
DEFINE_STORE(i64_store32, u32, u64);

#define I32_CLZ(x) ((x) ? __builtin_clz(x) : 32)
#define I64_CLZ(x) ((x) ? __builtin_clzll(x) : 64)
#define I32_CTZ(x) ((x) ? __builtin_ctz(x) : 32)
#define I64_CTZ(x) ((x) ? __builtin_ctzll(x) : 64)
#define I32_POPCNT(x) (__builtin_popcount(x))
#define I64_POPCNT(x) (__builtin_popcountll(x))

#define DIV_S(ut, min, x, y)                                 \
   ((UNLIKELY((y) == 0)) ?                TRAP(DIV_BY_ZERO)  \
  : (UNLIKELY((x) == min && (y) == -1)) ? TRAP(INT_OVERFLOW) \
  : (ut)((x) / (y)))

#define REM_S(ut, min, x, y)                                \
   ((UNLIKELY((y) == 0)) ?                TRAP(DIV_BY_ZERO) \
  : (UNLIKELY((x) == min && (y) == -1)) ? 0                 \
  : (ut)((x) % (y)))

#define I32_DIV_S(x, y) DIV_S(u32, INT32_MIN, (s32)x, (s32)y)
#define I64_DIV_S(x, y) DIV_S(u64, INT64_MIN, (s64)x, (s64)y)
#define I32_REM_S(x, y) REM_S(u32, INT32_MIN, (s32)x, (s32)y)
#define I64_REM_S(x, y) REM_S(u64, INT64_MIN, (s64)x, (s64)y)

#define DIVREM_U(op, x, y) \
  ((UNLIKELY((y) == 0)) ? TRAP(DIV_BY_ZERO) : ((x) op (y)))

#define DIV_U(x, y) DIVREM_U(/, x, y)
#define REM_U(x, y) DIVREM_U(%, x, y)

#define ROTL(x, y, mask) \
  (((x) << ((y) & (mask))) | ((x) >> (((mask) - (y) + 1) & (mask))))
#define ROTR(x, y, mask) \
  (((x) >> ((y) & (mask))) | ((x) << (((mask) - (y) + 1) & (mask))))

#define I32_ROTL(x, y) ROTL(x, y, 31)
#define I64_ROTL(x, y) ROTL(x, y, 63)
#define I32_ROTR(x, y) ROTR(x, y, 31)
#define I64_ROTR(x, y) ROTR(x, y, 63)

#define FMIN(x, y)                                          \
   ((UNLIKELY((x) != (x))) ? NAN                            \
  : (UNLIKELY((y) != (y))) ? NAN                            \
  : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? x : y) \
  : (x < y) ? x : y)

#define FMAX(x, y)                                          \
   ((UNLIKELY((x) != (x))) ? NAN                            \
  : (UNLIKELY((y) != (y))) ? NAN                            \
  : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? y : x) \
  : (x > y) ? x : y)

#define TRUNC_S(ut, st, ft, min, max, maxop, x)                             \
   ((UNLIKELY((x) != (x))) ? TRAP(INVALID_CONVERSION)                       \
  : (UNLIKELY((x) < (ft)(min) || (x) maxop (ft)(max))) ? TRAP(INT_OVERFLOW) \
  : (ut)(st)(x))

#define I32_TRUNC_S_F32(x) TRUNC_S(u32, s32, f32, INT32_MIN, INT32_MAX, >=, x)
#define I64_TRUNC_S_F32(x) TRUNC_S(u64, s64, f32, INT64_MIN, INT64_MAX, >=, x)
#define I32_TRUNC_S_F64(x) TRUNC_S(u32, s32, f64, INT32_MIN, INT32_MAX, >,  x)
#define I64_TRUNC_S_F64(x) TRUNC_S(u64, s64, f64, INT64_MIN, INT64_MAX, >=, x)

#define TRUNC_U(ut, ft, max, maxop, x)                                    \
   ((UNLIKELY((x) != (x))) ? TRAP(INVALID_CONVERSION)                     \
  : (UNLIKELY((x) <= (ft)-1 || (x) maxop (ft)(max))) ? TRAP(INT_OVERFLOW) \
  : (ut)(x))

#define I32_TRUNC_U_F32(x) TRUNC_U(u32, f32, UINT32_MAX, >=, x)
#define I64_TRUNC_U_F32(x) TRUNC_U(u64, f32, UINT64_MAX, >=, x)
#define I32_TRUNC_U_F64(x) TRUNC_U(u32, f64, UINT32_MAX, >,  x)
#define I64_TRUNC_U_F64(x) TRUNC_U(u64, f64, UINT64_MAX, >=, x)

#define DEFINE_REINTERPRET(name, t1, t2)  \
  static inline t2 name(t1 x) {           \
    t2 result;                            \
    memcpy(&result, &x, sizeof(result));  \
    return result;                        \
  }

DEFINE_REINTERPRET(f32_reinterpret_i32, u32, f32)
DEFINE_REINTERPRET(i32_reinterpret_f32, f32, u32)
DEFINE_REINTERPRET(f64_reinterpret_i64, u64, f64)
DEFINE_REINTERPRET(i64_reinterpret_f64, f64, u64)


static u32 func_types[8];

static void init_func_types(void) {
  func_types[0] = wasm_rt_register_func_type(1, 0, WASM_RT_I32);
  func_types[1] = wasm_rt_register_func_type(2, 0, WASM_RT_I32, WASM_RT_I32);
  func_types[2] = wasm_rt_register_func_type(1, 1, WASM_RT_I32, WASM_RT_I32);
  func_types[3] = wasm_rt_register_func_type(3, 0, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[4] = wasm_rt_register_func_type(2, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
  func_types[5] = wasm_rt_register_func_type(0, 0);
  func_types[6] = wasm_rt_register_func_type(0, 1, WASM_RT_I32);
  func_types[7] = wasm_rt_register_func_type(3, 1, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32, WASM_RT_I32);
}

static void f0(u32, u32);
static void f1(u32, u32);
static void f2(u32, u32, u32);
static u32 f3(void);
static u32 f4(u32);
static u32 f5(u32, u32);
static void f6(u32, u32);
static void f7(u32, u32, u32);
static u32 f8(u32, u32, u32);
static u32 __alloc(u32, u32);
static void f10(u32);
static u32 __retain(u32);
static void __release(u32);
static u32 loadAndIncrement(u32);
static void __collect(void);
static void f15(u32);
static void f16(u32);

static u32 g0;
static u32 g1;
static u32 __rtti_base;

static void init_globals(void) {
  g0 = 0u;
  g1 = 0u;
  __rtti_base = 16u;
}

static wasm_rt_memory_t memory;

static void f0(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0, l5 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l3 = i0;
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = l3;
  i1 = 4294967292u;
  i0 &= i1;
  l2 = i0;
  i1 = 16u;
  i0 = i0 >= i1;
  if (i0) {
    i0 = l2;
    i1 = 1073741808u;
    i0 = i0 < i1;
  } else {
    i0 = 0u;
  }
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = l2;
  i1 = 256u;
  i0 = i0 < i1;
  if (i0) {
    i0 = l2;
    i1 = 4u;
    i0 >>= (i1 & 31);
    l2 = i0;
    i0 = 0u;
  } else {
    i0 = l2;
    i1 = 31u;
    i2 = l2;
    i2 = I32_CLZ(i2);
    i1 -= i2;
    l3 = i1;
    i2 = 4u;
    i1 -= i2;
    i0 >>= (i1 & 31);
    i1 = 16u;
    i0 ^= i1;
    l2 = i0;
    i0 = l3;
    i1 = 7u;
    i0 -= i1;
  }
  l3 = i0;
  i1 = 23u;
  i0 = i0 < i1;
  if (i0) {
    i0 = l2;
    i1 = 16u;
    i0 = i0 < i1;
  } else {
    i0 = 0u;
  }
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 20));
  l4 = i0;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0 + 16));
  l5 = i0;
  if (i0) {
    i0 = l5;
    i1 = l4;
    i32_store((&memory), (u64)(i0 + 20), i1);
  }
  i0 = l4;
  if (i0) {
    i0 = l4;
    i1 = l5;
    i32_store((&memory), (u64)(i0 + 16), i1);
  }
  i0 = p1;
  i1 = p0;
  i2 = l2;
  i3 = l3;
  i4 = 4u;
  i3 <<= (i4 & 31);
  i2 += i3;
  i3 = 2u;
  i2 <<= (i3 & 31);
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1 + 96));
  i0 = i0 == i1;
  if (i0) {
    i0 = p0;
    i1 = l2;
    i2 = l3;
    i3 = 4u;
    i2 <<= (i3 & 31);
    i1 += i2;
    i2 = 2u;
    i1 <<= (i2 & 31);
    i0 += i1;
    i1 = l4;
    i32_store((&memory), (u64)(i0 + 96), i1);
    i0 = l4;
    i0 = !(i0);
    if (i0) {
      i0 = p0;
      i1 = l3;
      i2 = 2u;
      i1 <<= (i2 & 31);
      i0 += i1;
      i1 = p0;
      i2 = l3;
      i3 = 2u;
      i2 <<= (i3 & 31);
      i1 += i2;
      i1 = i32_load((&memory), (u64)(i1 + 4));
      i2 = 1u;
      i3 = l2;
      i2 <<= (i3 & 31);
      i3 = 4294967295u;
      i2 ^= i3;
      i1 &= i2;
      p1 = i1;
      i32_store((&memory), (u64)(i0 + 4), i1);
      i0 = p1;
      i0 = !(i0);
      if (i0) {
        i0 = p0;
        i1 = p0;
        i1 = i32_load((&memory), (u64)(i1));
        i2 = 1u;
        i3 = l3;
        i2 <<= (i3 & 31);
        i3 = 4294967295u;
        i2 ^= i3;
        i1 &= i2;
        i32_store((&memory), (u64)(i0), i1);
      }
    }
  }
  FUNC_EPILOGUE;
}

static void f1(u32 p0, u32 p1) {
  u32 l2 = 0, l3 = 0, l4 = 0, l5 = 0, l6 = 0, l7 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = p1;
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l3 = i0;
  i1 = 1u;
  i0 &= i1;
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = p1;
  i1 = 16u;
  i0 += i1;
  i1 = p1;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 4294967292u;
  i1 &= i2;
  i0 += i1;
  l4 = i0;
  i0 = i32_load((&memory), (u64)(i0));
  l5 = i0;
  i1 = 1u;
  i0 &= i1;
  if (i0) {
    i0 = l3;
    i1 = 4294967292u;
    i0 &= i1;
    i1 = 16u;
    i0 += i1;
    i1 = l5;
    i2 = 4294967292u;
    i1 &= i2;
    i0 += i1;
    l2 = i0;
    i1 = 1073741808u;
    i0 = i0 < i1;
    if (i0) {
      i0 = p0;
      i1 = l4;
      f0(i0, i1);
      i0 = p1;
      i1 = l2;
      i2 = l3;
      i3 = 3u;
      i2 &= i3;
      i1 |= i2;
      l3 = i1;
      i32_store((&memory), (u64)(i0), i1);
      i0 = p1;
      i1 = 16u;
      i0 += i1;
      i1 = p1;
      i1 = i32_load((&memory), (u64)(i1));
      i2 = 4294967292u;
      i1 &= i2;
      i0 += i1;
      l4 = i0;
      i0 = i32_load((&memory), (u64)(i0));
      l5 = i0;
    }
  }
  i0 = l3;
  i1 = 2u;
  i0 &= i1;
  if (i0) {
    i0 = p1;
    i1 = 4u;
    i0 -= i1;
    i0 = i32_load((&memory), (u64)(i0));
    l2 = i0;
    i0 = i32_load((&memory), (u64)(i0));
    l6 = i0;
    i1 = 1u;
    i0 &= i1;
    i0 = !(i0);
    if (i0) {
      UNREACHABLE;
    }
    i0 = l6;
    i1 = 4294967292u;
    i0 &= i1;
    i1 = 16u;
    i0 += i1;
    i1 = l3;
    i2 = 4294967292u;
    i1 &= i2;
    i0 += i1;
    l7 = i0;
    i1 = 1073741808u;
    i0 = i0 < i1;
    if (i0) {
      i0 = p0;
      i1 = l2;
      f0(i0, i1);
      i0 = l2;
      i1 = l7;
      i2 = l6;
      i3 = 3u;
      i2 &= i3;
      i1 |= i2;
      l3 = i1;
      i32_store((&memory), (u64)(i0), i1);
      i0 = l2;
      p1 = i0;
    }
  }
  i0 = l4;
  i1 = l5;
  i2 = 2u;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l3;
  i1 = 4294967292u;
  i0 &= i1;
  l2 = i0;
  i1 = 16u;
  i0 = i0 >= i1;
  if (i0) {
    i0 = l2;
    i1 = 1073741808u;
    i0 = i0 < i1;
  } else {
    i0 = 0u;
  }
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = l2;
  i1 = p1;
  i2 = 16u;
  i1 += i2;
  i0 += i1;
  i1 = l4;
  i0 = i0 != i1;
  if (i0) {
    UNREACHABLE;
  }
  i0 = l4;
  i1 = 4u;
  i0 -= i1;
  i1 = p1;
  i32_store((&memory), (u64)(i0), i1);
  i0 = l2;
  i1 = 256u;
  i0 = i0 < i1;
  if (i0) {
    i0 = l2;
    i1 = 4u;
    i0 >>= (i1 & 31);
    l4 = i0;
    i0 = 0u;
  } else {
    i0 = l2;
    i1 = 31u;
    i2 = l2;
    i2 = I32_CLZ(i2);
    i1 -= i2;
    l2 = i1;
    i2 = 4u;
    i1 -= i2;
    i0 >>= (i1 & 31);
    i1 = 16u;
    i0 ^= i1;
    l4 = i0;
    i0 = l2;
    i1 = 7u;
    i0 -= i1;
  }
  l3 = i0;
  i1 = 23u;
  i0 = i0 < i1;
  if (i0) {
    i0 = l4;
    i1 = 16u;
    i0 = i0 < i1;
  } else {
    i0 = 0u;
  }
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = p0;
  i1 = l4;
  i2 = l3;
  i3 = 4u;
  i2 <<= (i3 & 31);
  i1 += i2;
  i2 = 2u;
  i1 <<= (i2 & 31);
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0 + 96));
  l2 = i0;
  i0 = p1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 16), i1);
  i0 = p1;
  i1 = l2;
  i32_store((&memory), (u64)(i0 + 20), i1);
  i0 = l2;
  if (i0) {
    i0 = l2;
    i1 = p1;
    i32_store((&memory), (u64)(i0 + 16), i1);
  }
  i0 = p0;
  i1 = l4;
  i2 = l3;
  i3 = 4u;
  i2 <<= (i3 & 31);
  i1 += i2;
  i2 = 2u;
  i1 <<= (i2 & 31);
  i0 += i1;
  i1 = p1;
  i32_store((&memory), (u64)(i0 + 96), i1);
  i0 = p0;
  i1 = p0;
  i1 = i32_load((&memory), (u64)(i1));
  i2 = 1u;
  i3 = l3;
  i2 <<= (i3 & 31);
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = l3;
  i2 = 2u;
  i1 <<= (i2 & 31);
  i0 += i1;
  i1 = p0;
  i2 = l3;
  i3 = 2u;
  i2 <<= (i3 & 31);
  i1 += i2;
  i1 = i32_load((&memory), (u64)(i1 + 4));
  i2 = 1u;
  i3 = l4;
  i2 <<= (i3 & 31);
  i1 |= i2;
  i32_store((&memory), (u64)(i0 + 4), i1);
  FUNC_EPILOGUE;
}

static void f2(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0, l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4, i5;
  i0 = p2;
  i1 = 15u;
  i0 &= i1;
  i0 = !(i0);
  i1 = 0u;
  i2 = p1;
  i3 = 15u;
  i2 &= i3;
  i2 = !(i2);
  i3 = 0u;
  i4 = p1;
  i5 = p2;
  i4 = i4 <= i5;
  i2 = i4 ? i2 : i3;
  i0 = i2 ? i0 : i1;
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 1568));
  l3 = i0;
  if (i0) {
    i0 = p1;
    i1 = l3;
    i2 = 16u;
    i1 += i2;
    i0 = i0 < i1;
    if (i0) {
      UNREACHABLE;
    }
    i0 = l3;
    i1 = p1;
    i2 = 16u;
    i1 -= i2;
    i0 = i0 == i1;
    if (i0) {
      i0 = l3;
      i0 = i32_load((&memory), (u64)(i0));
      l4 = i0;
      i0 = p1;
      i1 = 16u;
      i0 -= i1;
      p1 = i0;
    }
  } else {
    i0 = p1;
    i1 = p0;
    i2 = 1572u;
    i1 += i2;
    i0 = i0 < i1;
    if (i0) {
      UNREACHABLE;
    }
  }
  i0 = p2;
  i1 = p1;
  i0 -= i1;
  p2 = i0;
  i1 = 48u;
  i0 = i0 < i1;
  if (i0) {
    goto Bfunc;
  }
  i0 = p1;
  i1 = l4;
  i2 = 2u;
  i1 &= i2;
  i2 = p2;
  i3 = 32u;
  i2 -= i3;
  i3 = 1u;
  i2 |= i3;
  i1 |= i2;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 16), i1);
  i0 = p1;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 20), i1);
  i0 = p1;
  i1 = p2;
  i0 += i1;
  i1 = 16u;
  i0 -= i1;
  p2 = i0;
  i1 = 2u;
  i32_store((&memory), (u64)(i0), i1);
  i0 = p0;
  i1 = p2;
  i32_store((&memory), (u64)(i0 + 1568), i1);
  i0 = p0;
  i1 = p1;
  f1(i0, i1);
  Bfunc:;
  FUNC_EPILOGUE;
}

static u32 f3(void) {
  u32 l0 = 0, l1 = 0, l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = g0;
  l0 = i0;
  i0 = !(i0);
  if (i0) {
    i0 = 1u;
    i1 = memory.pages;
    l0 = i1;
    i0 = (u32)((s32)i0 > (s32)i1);
    if (i0) {
      i0 = 1u;
      i1 = l0;
      i0 -= i1;
      i0 = wasm_rt_grow_memory((&memory), i0);
      i1 = 0u;
      i0 = (u32)((s32)i0 < (s32)i1);
    } else {
      i0 = 0u;
    }
    if (i0) {
      UNREACHABLE;
    }
    i0 = 48u;
    l0 = i0;
    i1 = 0u;
    i32_store((&memory), (u64)(i0), i1);
    i0 = 1616u;
    i1 = 0u;
    i32_store((&memory), (u64)(i0), i1);
    L3: 
      i0 = l1;
      i1 = 23u;
      i0 = i0 < i1;
      if (i0) {
        i0 = l1;
        i1 = 2u;
        i0 <<= (i1 & 31);
        i1 = 48u;
        i0 += i1;
        i1 = 0u;
        i32_store((&memory), (u64)(i0 + 4), i1);
        i0 = 0u;
        l2 = i0;
        L5: 
          i0 = l2;
          i1 = 16u;
          i0 = i0 < i1;
          if (i0) {
            i0 = l1;
            i1 = 4u;
            i0 <<= (i1 & 31);
            i1 = l2;
            i0 += i1;
            i1 = 2u;
            i0 <<= (i1 & 31);
            i1 = 48u;
            i0 += i1;
            i1 = 0u;
            i32_store((&memory), (u64)(i0 + 96), i1);
            i0 = l2;
            i1 = 1u;
            i0 += i1;
            l2 = i0;
            goto L5;
          }
        i0 = l1;
        i1 = 1u;
        i0 += i1;
        l1 = i0;
        goto L3;
      }
    i0 = 48u;
    i1 = 1632u;
    i2 = memory.pages;
    i3 = 16u;
    i2 <<= (i3 & 31);
    f2(i0, i1, i2);
    i0 = 48u;
    g0 = i0;
  }
  i0 = l0;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f4(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = p0;
  i1 = 1073741808u;
  i0 = i0 >= i1;
  if (i0) {
    UNREACHABLE;
  }
  i0 = p0;
  i1 = 15u;
  i0 += i1;
  i1 = 4294967280u;
  i0 &= i1;
  p0 = i0;
  i1 = 16u;
  i2 = p0;
  i3 = 16u;
  i2 = i2 > i3;
  i0 = i2 ? i0 : i1;
  FUNC_EPILOGUE;
  return i0;
}

static u32 f5(u32 p0, u32 p1) {
  u32 l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = p1;
  i1 = 256u;
  i0 = i0 < i1;
  if (i0) {
    i0 = p1;
    i1 = 4u;
    i0 >>= (i1 & 31);
    p1 = i0;
    i0 = 0u;
  } else {
    i0 = p1;
    i1 = 536870904u;
    i0 = i0 < i1;
    if (i0) {
      i0 = p1;
      i1 = 1u;
      i2 = 27u;
      i3 = p1;
      i3 = I32_CLZ(i3);
      i2 -= i3;
      i1 <<= (i2 & 31);
      i0 += i1;
      i1 = 1u;
      i0 -= i1;
      p1 = i0;
    }
    i0 = p1;
    i1 = 31u;
    i2 = p1;
    i2 = I32_CLZ(i2);
    i1 -= i2;
    l2 = i1;
    i2 = 4u;
    i1 -= i2;
    i0 >>= (i1 & 31);
    i1 = 16u;
    i0 ^= i1;
    p1 = i0;
    i0 = l2;
    i1 = 7u;
    i0 -= i1;
  }
  l2 = i0;
  i1 = 23u;
  i0 = i0 < i1;
  if (i0) {
    i0 = p1;
    i1 = 16u;
    i0 = i0 < i1;
  } else {
    i0 = 0u;
  }
  i0 = !(i0);
  if (i0) {
    UNREACHABLE;
  }
  i0 = p0;
  i1 = l2;
  i2 = 2u;
  i1 <<= (i2 & 31);
  i0 += i1;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  i1 = 4294967295u;
  i2 = p1;
  i1 <<= (i2 & 31);
  i0 &= i1;
  p1 = i0;
  if (i0) {
    i0 = p0;
    i1 = p1;
    i1 = I32_CTZ(i1);
    i2 = l2;
    i3 = 4u;
    i2 <<= (i3 & 31);
    i1 += i2;
    i2 = 2u;
    i1 <<= (i2 & 31);
    i0 += i1;
    i0 = i32_load((&memory), (u64)(i0 + 96));
  } else {
    i0 = p0;
    i0 = i32_load((&memory), (u64)(i0));
    i1 = 4294967295u;
    i2 = l2;
    i3 = 1u;
    i2 += i3;
    i1 <<= (i2 & 31);
    i0 &= i1;
    p1 = i0;
    if (i0) {
      i0 = p0;
      i1 = p1;
      i1 = I32_CTZ(i1);
      p1 = i1;
      i2 = 2u;
      i1 <<= (i2 & 31);
      i0 += i1;
      i0 = i32_load((&memory), (u64)(i0 + 4));
      l2 = i0;
      i0 = !(i0);
      if (i0) {
        UNREACHABLE;
      }
      i0 = p0;
      i1 = l2;
      i1 = I32_CTZ(i1);
      i2 = p1;
      i3 = 4u;
      i2 <<= (i3 & 31);
      i1 += i2;
      i2 = 2u;
      i1 <<= (i2 & 31);
      i0 += i1;
      i0 = i32_load((&memory), (u64)(i0 + 96));
    } else {
      i0 = 0u;
    }
  }
  FUNC_EPILOGUE;
  return i0;
}

static void f6(u32 p0, u32 p1) {
  u32 l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3, i4, i5;
  i0 = memory.pages;
  l2 = i0;
  i1 = 16u;
  i2 = p0;
  i2 = i32_load((&memory), (u64)(i2 + 1568));
  i3 = l2;
  i4 = 16u;
  i3 <<= (i4 & 31);
  i4 = 16u;
  i3 -= i4;
  i2 = i2 != i3;
  i1 <<= (i2 & 31);
  i2 = p1;
  i3 = 1u;
  i4 = 27u;
  i5 = p1;
  i5 = I32_CLZ(i5);
  i4 -= i5;
  i3 <<= (i4 & 31);
  i4 = 1u;
  i3 -= i4;
  i2 += i3;
  i3 = p1;
  i4 = p1;
  i5 = 536870904u;
  i4 = i4 < i5;
  i2 = i4 ? i2 : i3;
  i1 += i2;
  i2 = 65535u;
  i1 += i2;
  i2 = 4294901760u;
  i1 &= i2;
  i2 = 16u;
  i1 >>= (i2 & 31);
  p1 = i1;
  i2 = l2;
  i3 = p1;
  i2 = (u32)((s32)i2 > (s32)i3);
  i0 = i2 ? i0 : i1;
  i0 = wasm_rt_grow_memory((&memory), i0);
  i1 = 0u;
  i0 = (u32)((s32)i0 < (s32)i1);
  if (i0) {
    i0 = p1;
    i0 = wasm_rt_grow_memory((&memory), i0);
    i1 = 0u;
    i0 = (u32)((s32)i0 < (s32)i1);
    if (i0) {
      UNREACHABLE;
    }
  }
  i0 = p0;
  i1 = l2;
  i2 = 16u;
  i1 <<= (i2 & 31);
  i2 = memory.pages;
  i3 = 16u;
  i2 <<= (i3 & 31);
  f2(i0, i1, i2);
  FUNC_EPILOGUE;
}

static void f7(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0, l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = p1;
  i0 = i32_load((&memory), (u64)(i0));
  l3 = i0;
  i0 = p2;
  i1 = 15u;
  i0 &= i1;
  if (i0) {
    UNREACHABLE;
  }
  i0 = l3;
  i1 = 4294967292u;
  i0 &= i1;
  i1 = p2;
  i0 -= i1;
  l4 = i0;
  i1 = 32u;
  i0 = i0 >= i1;
  if (i0) {
    i0 = p1;
    i1 = p2;
    i2 = l3;
    i3 = 2u;
    i2 &= i3;
    i1 |= i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = p2;
    i1 = p1;
    i2 = 16u;
    i1 += i2;
    i0 += i1;
    p1 = i0;
    i1 = l4;
    i2 = 16u;
    i1 -= i2;
    i2 = 1u;
    i1 |= i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = p0;
    i1 = p1;
    f1(i0, i1);
  } else {
    i0 = p1;
    i1 = l3;
    i2 = 4294967294u;
    i1 &= i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = p1;
    i1 = 16u;
    i0 += i1;
    i1 = p1;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 4294967292u;
    i1 &= i2;
    i0 += i1;
    i1 = p1;
    i2 = 16u;
    i1 += i2;
    i2 = p1;
    i2 = i32_load((&memory), (u64)(i2));
    i3 = 4294967292u;
    i2 &= i3;
    i1 += i2;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 4294967293u;
    i1 &= i2;
    i32_store((&memory), (u64)(i0), i1);
  }
  FUNC_EPILOGUE;
}

static u32 f8(u32 p0, u32 p1, u32 p2) {
  u32 l3 = 0, l4 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = g1;
  if (i0) {
    UNREACHABLE;
  }
  i0 = p0;
  i1 = p1;
  i1 = f4(i1);
  l4 = i1;
  i0 = f5(i0, i1);
  l3 = i0;
  i0 = !(i0);
  if (i0) {
    i0 = 1u;
    g1 = i0;
    i0 = 0u;
    g1 = i0;
    i0 = p0;
    i1 = l4;
    i0 = f5(i0, i1);
    l3 = i0;
    i0 = !(i0);
    if (i0) {
      i0 = p0;
      i1 = l4;
      f6(i0, i1);
      i0 = p0;
      i1 = l4;
      i0 = f5(i0, i1);
      l3 = i0;
      i0 = !(i0);
      if (i0) {
        UNREACHABLE;
      }
    }
  }
  i0 = l3;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = 4294967292u;
  i0 &= i1;
  i1 = l4;
  i0 = i0 < i1;
  if (i0) {
    UNREACHABLE;
  }
  i0 = l3;
  i1 = 0u;
  i32_store((&memory), (u64)(i0 + 4), i1);
  i0 = l3;
  i1 = p2;
  i32_store((&memory), (u64)(i0 + 8), i1);
  i0 = l3;
  i1 = p1;
  i32_store((&memory), (u64)(i0 + 12), i1);
  i0 = p0;
  i1 = l3;
  f0(i0, i1);
  i0 = p0;
  i1 = l3;
  i2 = l4;
  f7(i0, i1, i2);
  i0 = l3;
  FUNC_EPILOGUE;
  return i0;
}

static u32 __alloc(u32 p0, u32 p1) {
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = f3();
  i1 = p0;
  i2 = p1;
  i0 = f8(i0, i1, i2);
  i1 = 16u;
  i0 += i1;
  FUNC_EPILOGUE;
  return i0;
}

static void f10(u32 p0) {
  u32 l1 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l1 = i0;
  i1 = 4026531840u;
  i0 &= i1;
  i1 = l1;
  i2 = 1u;
  i1 += i2;
  i2 = 4026531840u;
  i1 &= i2;
  i0 = i0 != i1;
  if (i0) {
    UNREACHABLE;
  }
  i0 = p0;
  i1 = l1;
  i2 = 1u;
  i1 += i2;
  i32_store((&memory), (u64)(i0 + 4), i1);
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  if (i0) {
    UNREACHABLE;
  }
  FUNC_EPILOGUE;
}

static u32 __retain(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = p0;
  i1 = 44u;
  i0 = i0 > i1;
  if (i0) {
    i0 = p0;
    i1 = 16u;
    i0 -= i1;
    f10(i0);
  }
  i0 = p0;
  FUNC_EPILOGUE;
  return i0;
}

static void __release(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = p0;
  i1 = 44u;
  i0 = i0 > i1;
  if (i0) {
    i0 = p0;
    i1 = 16u;
    i0 -= i1;
    f15(i0);
  }
  FUNC_EPILOGUE;
}

static u32 loadAndIncrement(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = 1u;
  i0 += i1;
  FUNC_EPILOGUE;
  return i0;
}

static void __collect(void) {
  FUNC_PROLOGUE;
  FUNC_EPILOGUE;
}

static void f15(u32 p0) {
  u32 l1 = 0, l2 = 0;
  FUNC_PROLOGUE;
  u32 i0, i1, i2, i3;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0 + 4));
  l2 = i0;
  i1 = 268435455u;
  i0 &= i1;
  l1 = i0;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  i1 = 1u;
  i0 &= i1;
  if (i0) {
    UNREACHABLE;
  }
  i0 = l1;
  i1 = 1u;
  i0 = i0 == i1;
  if (i0) {
    i0 = p0;
    i1 = 16u;
    i0 += i1;
    f16(i0);
    i0 = l2;
    i1 = 2147483648u;
    i0 &= i1;
    if (i0) {
      UNREACHABLE;
    }
    i0 = p0;
    i1 = p0;
    i1 = i32_load((&memory), (u64)(i1));
    i2 = 1u;
    i1 |= i2;
    i32_store((&memory), (u64)(i0), i1);
    i0 = g0;
    i1 = p0;
    f1(i0, i1);
  } else {
    i0 = l1;
    i1 = 0u;
    i0 = i0 <= i1;
    if (i0) {
      UNREACHABLE;
    }
    i0 = p0;
    i1 = l1;
    i2 = 1u;
    i1 -= i2;
    i2 = l2;
    i3 = 4026531840u;
    i2 &= i3;
    i1 |= i2;
    i32_store((&memory), (u64)(i0 + 4), i1);
  }
  FUNC_EPILOGUE;
}

static void f16(u32 p0) {
  FUNC_PROLOGUE;
  u32 i0, i1;
  i0 = p0;
  i1 = 8u;
  i0 -= i1;
  i0 = i32_load((&memory), (u64)(i0));
  switch (i0) {
    case 0: goto B2;
    case 1: goto B2;
    case 2: goto B1;
    default: goto B0;
  }
  B2:;
  goto Bfunc;
  B1:;
  i0 = p0;
  i0 = i32_load((&memory), (u64)(i0));
  p0 = i0;
  if (i0) {
    i0 = p0;
    i1 = 44u;
    i0 = i0 >= i1;
    if (i0) {
      i0 = p0;
      i1 = 16u;
      i0 -= i1;
      f15(i0);
    }
  }
  goto Bfunc;
  B0:;
  UNREACHABLE;
  Bfunc:;
  FUNC_EPILOGUE;
}

static const u8 data_segment_data_0[] = {
  0x03, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 
};

static void init_memory(void) {
  wasm_rt_allocate_memory((&memory), 1, 65536);
  memcpy(&(memory.data[16u]), data_segment_data_0, 21);
}

static void init_table(void) {
  uint32_t offset;
}

/* export: 'memory' */
wasm_rt_memory_t (*WASM_RT_ADD_PREFIX(Z_memory));
/* export: '__alloc' */
u32 (*WASM_RT_ADD_PREFIX(Z___allocZ_iii))(u32, u32);
/* export: '__retain' */
u32 (*WASM_RT_ADD_PREFIX(Z___retainZ_ii))(u32);
/* export: '__release' */
void (*WASM_RT_ADD_PREFIX(Z___releaseZ_vi))(u32);
/* export: '__collect' */
void (*WASM_RT_ADD_PREFIX(Z___collectZ_vv))(void);
/* export: '__rtti_base' */
u32 (*WASM_RT_ADD_PREFIX(Z___rtti_baseZ_i));
/* export: 'loadAndIncrement' */
u32 (*WASM_RT_ADD_PREFIX(Z_loadAndIncrementZ_ii))(u32);

static void init_exports(void) {
  /* export: 'memory' */
  WASM_RT_ADD_PREFIX(Z_memory) = (&memory);
  /* export: '__alloc' */
  WASM_RT_ADD_PREFIX(Z___allocZ_iii) = (&__alloc);
  /* export: '__retain' */
  WASM_RT_ADD_PREFIX(Z___retainZ_ii) = (&__retain);
  /* export: '__release' */
  WASM_RT_ADD_PREFIX(Z___releaseZ_vi) = (&__release);
  /* export: '__collect' */
  WASM_RT_ADD_PREFIX(Z___collectZ_vv) = (&__collect);
  /* export: '__rtti_base' */
  WASM_RT_ADD_PREFIX(Z___rtti_baseZ_i) = (&__rtti_base);
  /* export: 'loadAndIncrement' */
  WASM_RT_ADD_PREFIX(Z_loadAndIncrementZ_ii) = (&loadAndIncrement);
}

void WASM_RT_ADD_PREFIX(init)(void) {
  init_func_types();
  init_globals();
  init_memory();
  init_table();
  init_exports();
}
