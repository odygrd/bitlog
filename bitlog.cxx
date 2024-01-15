// Global module fragment where #includes can happen
module;

#include <iostream>

#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <system_error>
#include <type_traits>
#include <variant>
#include <vector>

#include <fcntl.h>
#include <linux/mman.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(_M_X64)
  #define X86_ARCHITECTURE_ENABLED
#endif

#if defined(X86_ARCHITECTURE_ENABLED)
  #include <immintrin.h>
  #include <x86intrin.h>
#endif

// first thing after the Global module fragment must be a module command
export module bitlog;

// Attributes and Macros
#ifdef NDEBUG // Check if not in debug mode
  #if defined(__GNUC__) || defined(__clang__)
    #define ALWAYS_INLINE [[gnu::always_inline]]
  #elif defined(_MSC_VER)
    #define ALWAYS_INLINE [[msvc::forceinline]]
  #else
    #define ALWAYS_INLINE inline
  #endif
#else
  #define ALWAYS_INLINE
#endif

#define __rte_packed __attribute__((__packed__))
#define __rte_may_alias __attribute__((__may_alias__))

#if defined(X86_ARCHITECTURE_ENABLED)
namespace rte
{
extern "C"
{
  static ALWAYS_INLINE void* rte_memcpy(void* dst, const void* src, size_t n);

  static ALWAYS_INLINE void* rte_mov15_or_less(void* dst, const void* src, size_t n)
  {
    struct rte_uint64_alias
    {
      uint64_t val;
    } __rte_packed __rte_may_alias;
    struct rte_uint32_alias
    {
      uint32_t val;
    } __rte_packed __rte_may_alias;
    struct rte_uint16_alias
    {
      uint16_t val;
    } __rte_packed __rte_may_alias;

    void* ret = dst;
    if (n & 8)
    {
      ((struct rte_uint64_alias*)dst)->val = ((const struct rte_uint64_alias*)src)->val;
      src = (const uint64_t*)src + 1;
      dst = (uint64_t*)dst + 1;
    }
    if (n & 4)
    {
      ((struct rte_uint32_alias*)dst)->val = ((const struct rte_uint32_alias*)src)->val;
      src = (const uint32_t*)src + 1;
      dst = (uint32_t*)dst + 1;
    }
    if (n & 2)
    {
      ((struct rte_uint16_alias*)dst)->val = ((const struct rte_uint16_alias*)src)->val;
      src = (const uint16_t*)src + 1;
      dst = (uint16_t*)dst + 1;
    }
    if (n & 1)
      *(uint8_t*)dst = *(const uint8_t*)src;
    return ret;
  }

  #if defined __AVX2__

    #define ALIGNMENT_MASK 0x1F

  static ALWAYS_INLINE void rte_mov16(uint8_t* dst, const uint8_t* src)
  {
    __m128i xmm0;

    xmm0 = _mm_loadu_si128((const __m128i*)(const void*)src);
    _mm_storeu_si128((__m128i*)(void*)dst, xmm0);
  }

  static ALWAYS_INLINE void rte_mov32(uint8_t* dst, const uint8_t* src)
  {
    __m256i ymm0;

    ymm0 = _mm256_loadu_si256((const __m256i*)(const void*)src);
    _mm256_storeu_si256((__m256i*)(void*)dst, ymm0);
  }

  static ALWAYS_INLINE void rte_mov64(uint8_t* dst, const uint8_t* src)
  {
    rte_mov32((uint8_t*)dst + 0 * 32, (const uint8_t*)src + 0 * 32);
    rte_mov32((uint8_t*)dst + 1 * 32, (const uint8_t*)src + 1 * 32);
  }

  static ALWAYS_INLINE void rte_mov128(uint8_t* dst, const uint8_t* src)
  {
    rte_mov32((uint8_t*)dst + 0 * 32, (const uint8_t*)src + 0 * 32);
    rte_mov32((uint8_t*)dst + 1 * 32, (const uint8_t*)src + 1 * 32);
    rte_mov32((uint8_t*)dst + 2 * 32, (const uint8_t*)src + 2 * 32);
    rte_mov32((uint8_t*)dst + 3 * 32, (const uint8_t*)src + 3 * 32);
  }

  static ALWAYS_INLINE void rte_mov256(uint8_t* dst, const uint8_t* src)
  {
    rte_mov32((uint8_t*)dst + 0 * 32, (const uint8_t*)src + 0 * 32);
    rte_mov32((uint8_t*)dst + 1 * 32, (const uint8_t*)src + 1 * 32);
    rte_mov32((uint8_t*)dst + 2 * 32, (const uint8_t*)src + 2 * 32);
    rte_mov32((uint8_t*)dst + 3 * 32, (const uint8_t*)src + 3 * 32);
    rte_mov32((uint8_t*)dst + 4 * 32, (const uint8_t*)src + 4 * 32);
    rte_mov32((uint8_t*)dst + 5 * 32, (const uint8_t*)src + 5 * 32);
    rte_mov32((uint8_t*)dst + 6 * 32, (const uint8_t*)src + 6 * 32);
    rte_mov32((uint8_t*)dst + 7 * 32, (const uint8_t*)src + 7 * 32);
  }

  static ALWAYS_INLINE void rte_mov128blocks(uint8_t* dst, const uint8_t* src, size_t n)
  {
    __m256i ymm0, ymm1, ymm2, ymm3;

    while (n >= 128)
    {
      ymm0 = _mm256_loadu_si256((const __m256i*)(const void*)((const uint8_t*)src + 0 * 32));
      n -= 128;
      ymm1 = _mm256_loadu_si256((const __m256i*)(const void*)((const uint8_t*)src + 1 * 32));
      ymm2 = _mm256_loadu_si256((const __m256i*)(const void*)((const uint8_t*)src + 2 * 32));
      ymm3 = _mm256_loadu_si256((const __m256i*)(const void*)((const uint8_t*)src + 3 * 32));
      src = (const uint8_t*)src + 128;
      _mm256_storeu_si256((__m256i*)(void*)((uint8_t*)dst + 0 * 32), ymm0);
      _mm256_storeu_si256((__m256i*)(void*)((uint8_t*)dst + 1 * 32), ymm1);
      _mm256_storeu_si256((__m256i*)(void*)((uint8_t*)dst + 2 * 32), ymm2);
      _mm256_storeu_si256((__m256i*)(void*)((uint8_t*)dst + 3 * 32), ymm3);
      dst = (uint8_t*)dst + 128;
    }
  }

  static ALWAYS_INLINE void* rte_memcpy_generic(void* dst, const void* src, size_t n)
  {
    void* ret = dst;
    size_t dstofss;
    size_t bits;

    if (n < 16)
    {
      return rte_mov15_or_less(dst, src, n);
    }

    if (n <= 32)
    {
      rte_mov16((uint8_t*)dst, (const uint8_t*)src);
      rte_mov16((uint8_t*)dst - 16 + n, (const uint8_t*)src - 16 + n);
      return ret;
    }
    if (n <= 48)
    {
      rte_mov16((uint8_t*)dst, (const uint8_t*)src);
      rte_mov16((uint8_t*)dst + 16, (const uint8_t*)src + 16);
      rte_mov16((uint8_t*)dst - 16 + n, (const uint8_t*)src - 16 + n);
      return ret;
    }
    if (n <= 64)
    {
      rte_mov32((uint8_t*)dst, (const uint8_t*)src);
      rte_mov32((uint8_t*)dst - 32 + n, (const uint8_t*)src - 32 + n);
      return ret;
    }
    if (n <= 256)
    {
      if (n >= 128)
      {
        n -= 128;
        rte_mov128((uint8_t*)dst, (const uint8_t*)src);
        src = (const uint8_t*)src + 128;
        dst = (uint8_t*)dst + 128;
      }
    COPY_BLOCK_128_BACK31:
      if (n >= 64)
      {
        n -= 64;
        rte_mov64((uint8_t*)dst, (const uint8_t*)src);
        src = (const uint8_t*)src + 64;
        dst = (uint8_t*)dst + 64;
      }
      if (n > 32)
      {
        rte_mov32((uint8_t*)dst, (const uint8_t*)src);
        rte_mov32((uint8_t*)dst - 32 + n, (const uint8_t*)src - 32 + n);
        return ret;
      }
      if (n > 0)
      {
        rte_mov32((uint8_t*)dst - 32 + n, (const uint8_t*)src - 32 + n);
      }
      return ret;
    }

    dstofss = (uintptr_t)dst & 0x1F;
    if (dstofss > 0)
    {
      dstofss = 32 - dstofss;
      n -= dstofss;
      rte_mov32((uint8_t*)dst, (const uint8_t*)src);
      src = (const uint8_t*)src + dstofss;
      dst = (uint8_t*)dst + dstofss;
    }

    rte_mov128blocks((uint8_t*)dst, (const uint8_t*)src, n);
    bits = n;
    n = n & 127;
    bits -= n;
    src = (const uint8_t*)src + bits;
    dst = (uint8_t*)dst + bits;

    goto COPY_BLOCK_128_BACK31;
  }

  #else
    #define ALIGNMENT_MASK 0x0F

  static ALWAYS_INLINE void rte_mov16(uint8_t* dst, const uint8_t* src)
  {
    __m128i xmm0;

    xmm0 = _mm_loadu_si128((const __m128i*)(const void*)src);
    _mm_storeu_si128((__m128i*)(void*)dst, xmm0);
  }

  static ALWAYS_INLINE void rte_mov32(uint8_t* dst, const uint8_t* src)
  {
    rte_mov16((uint8_t*)dst + 0 * 16, (const uint8_t*)src + 0 * 16);
    rte_mov16((uint8_t*)dst + 1 * 16, (const uint8_t*)src + 1 * 16);
  }

  static ALWAYS_INLINE void rte_mov64(uint8_t* dst, const uint8_t* src)
  {
    rte_mov16((uint8_t*)dst + 0 * 16, (const uint8_t*)src + 0 * 16);
    rte_mov16((uint8_t*)dst + 1 * 16, (const uint8_t*)src + 1 * 16);
    rte_mov16((uint8_t*)dst + 2 * 16, (const uint8_t*)src + 2 * 16);
    rte_mov16((uint8_t*)dst + 3 * 16, (const uint8_t*)src + 3 * 16);
  }

  static ALWAYS_INLINE void rte_mov128(uint8_t* dst, const uint8_t* src)
  {
    rte_mov16((uint8_t*)dst + 0 * 16, (const uint8_t*)src + 0 * 16);
    rte_mov16((uint8_t*)dst + 1 * 16, (const uint8_t*)src + 1 * 16);
    rte_mov16((uint8_t*)dst + 2 * 16, (const uint8_t*)src + 2 * 16);
    rte_mov16((uint8_t*)dst + 3 * 16, (const uint8_t*)src + 3 * 16);
    rte_mov16((uint8_t*)dst + 4 * 16, (const uint8_t*)src + 4 * 16);
    rte_mov16((uint8_t*)dst + 5 * 16, (const uint8_t*)src + 5 * 16);
    rte_mov16((uint8_t*)dst + 6 * 16, (const uint8_t*)src + 6 * 16);
    rte_mov16((uint8_t*)dst + 7 * 16, (const uint8_t*)src + 7 * 16);
  }

  static inline void rte_mov256(uint8_t* dst, const uint8_t* src)
  {
    rte_mov16((uint8_t*)dst + 0 * 16, (const uint8_t*)src + 0 * 16);
    rte_mov16((uint8_t*)dst + 1 * 16, (const uint8_t*)src + 1 * 16);
    rte_mov16((uint8_t*)dst + 2 * 16, (const uint8_t*)src + 2 * 16);
    rte_mov16((uint8_t*)dst + 3 * 16, (const uint8_t*)src + 3 * 16);
    rte_mov16((uint8_t*)dst + 4 * 16, (const uint8_t*)src + 4 * 16);
    rte_mov16((uint8_t*)dst + 5 * 16, (const uint8_t*)src + 5 * 16);
    rte_mov16((uint8_t*)dst + 6 * 16, (const uint8_t*)src + 6 * 16);
    rte_mov16((uint8_t*)dst + 7 * 16, (const uint8_t*)src + 7 * 16);
    rte_mov16((uint8_t*)dst + 8 * 16, (const uint8_t*)src + 8 * 16);
    rte_mov16((uint8_t*)dst + 9 * 16, (const uint8_t*)src + 9 * 16);
    rte_mov16((uint8_t*)dst + 10 * 16, (const uint8_t*)src + 10 * 16);
    rte_mov16((uint8_t*)dst + 11 * 16, (const uint8_t*)src + 11 * 16);
    rte_mov16((uint8_t*)dst + 12 * 16, (const uint8_t*)src + 12 * 16);
    rte_mov16((uint8_t*)dst + 13 * 16, (const uint8_t*)src + 13 * 16);
    rte_mov16((uint8_t*)dst + 14 * 16, (const uint8_t*)src + 14 * 16);
    rte_mov16((uint8_t*)dst + 15 * 16, (const uint8_t*)src + 15 * 16);
  }

    #define MOVEUNALIGNED_LEFT47_IMM(dst, src, len, offset)                                                   \
      __extension__({                                                                                         \
        size_t tmp;                                                                                           \
        while (len >= 128 + 16 - offset)                                                                      \
        {                                                                                                     \
          xmm0 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 0 * 16));       \
          len -= 128;                                                                                         \
          xmm1 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 1 * 16));       \
          xmm2 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 2 * 16));       \
          xmm3 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 3 * 16));       \
          xmm4 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 4 * 16));       \
          xmm5 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 5 * 16));       \
          xmm6 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 6 * 16));       \
          xmm7 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 7 * 16));       \
          xmm8 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 8 * 16));       \
          src = (const uint8_t*)src + 128;                                                                    \
          _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 0 * 16), _mm_alignr_epi8(xmm1, xmm0, offset));   \
          _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 1 * 16), _mm_alignr_epi8(xmm2, xmm1, offset));   \
          _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 2 * 16), _mm_alignr_epi8(xmm3, xmm2, offset));   \
          _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 3 * 16), _mm_alignr_epi8(xmm4, xmm3, offset));   \
          _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 4 * 16), _mm_alignr_epi8(xmm5, xmm4, offset));   \
          _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 5 * 16), _mm_alignr_epi8(xmm6, xmm5, offset));   \
          _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 6 * 16), _mm_alignr_epi8(xmm7, xmm6, offset));   \
          _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 7 * 16), _mm_alignr_epi8(xmm8, xmm7, offset));   \
          dst = (uint8_t*)dst + 128;                                                                          \
        }                                                                                                     \
        tmp = len;                                                                                            \
        len = ((len - 16 + offset) & 127) + 16 - offset;                                                      \
        tmp -= len;                                                                                           \
        src = (const uint8_t*)src + tmp;                                                                      \
        dst = (uint8_t*)dst + tmp;                                                                            \
        if (len >= 32 + 16 - offset)                                                                          \
        {                                                                                                     \
          while (len >= 32 + 16 - offset)                                                                     \
          {                                                                                                   \
            xmm0 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 0 * 16));     \
            len -= 32;                                                                                        \
            xmm1 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 1 * 16));     \
            xmm2 = _mm_loadu_si128((const __m128i*)(const void*)((const uint8_t*)src - offset + 2 * 16));     \
            src = (const uint8_t*)src + 32;                                                                   \
            _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 0 * 16), _mm_alignr_epi8(xmm1, xmm0, offset)); \
            _mm_storeu_si128((__m128i*)(void*)((uint8_t*)dst + 1 * 16), _mm_alignr_epi8(xmm2, xmm1, offset)); \
            dst = (uint8_t*)dst + 32;                                                                         \
          }                                                                                                   \
          tmp = len;                                                                                          \
          len = ((len - 16 + offset) & 31) + 16 - offset;                                                     \
          tmp -= len;                                                                                         \
          src = (const uint8_t*)src + tmp;                                                                    \
          dst = (uint8_t*)dst + tmp;                                                                          \
        }                                                                                                     \
      })

    #define MOVEUNALIGNED_LEFT47(dst, src, len, offset)                                            \
      __extension__({                                                                              \
        switch (offset)                                                                            \
        {                                                                                          \
        case 0x01:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x01);                                             \
          break;                                                                                   \
        case 0x02:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x02);                                             \
          break;                                                                                   \
        case 0x03:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x03);                                             \
          break;                                                                                   \
        case 0x04:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x04);                                             \
          break;                                                                                   \
        case 0x05:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x05);                                             \
          break;                                                                                   \
        case 0x06:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x06);                                             \
          break;                                                                                   \
        case 0x07:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x07);                                             \
          break;                                                                                   \
        case 0x08:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x08);                                             \
          break;                                                                                   \
        case 0x09:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x09);                                             \
          break;                                                                                   \
        case 0x0A:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0A);                                             \
          break;                                                                                   \
        case 0x0B:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0B);                                             \
          break;                                                                                   \
        case 0x0C:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0C);                                             \
          break;                                                                                   \
        case 0x0D:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0D);                                             \
          break;                                                                                   \
        case 0x0E:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0E);                                             \
          break;                                                                                   \
        case 0x0F:                                                                                 \
          MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0F);                                             \
          break;                                                                                   \
        default:;                                                                                  \
        }                                                                                          \
      })

  static ALWAYS_INLINE void* rte_memcpy_generic(void* dst, const void* src, size_t n)
  {
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;
    void* ret = dst;
    size_t dstofss;
    size_t srcofs;

    if (n < 16)
    {
      return rte_mov15_or_less(dst, src, n);
    }

    if (n <= 32)
    {
      rte_mov16((uint8_t*)dst, (const uint8_t*)src);
      rte_mov16((uint8_t*)dst - 16 + n, (const uint8_t*)src - 16 + n);
      return ret;
    }
    if (n <= 48)
    {
      rte_mov32((uint8_t*)dst, (const uint8_t*)src);
      rte_mov16((uint8_t*)dst - 16 + n, (const uint8_t*)src - 16 + n);
      return ret;
    }
    if (n <= 64)
    {
      rte_mov32((uint8_t*)dst, (const uint8_t*)src);
      rte_mov16((uint8_t*)dst + 32, (const uint8_t*)src + 32);
      rte_mov16((uint8_t*)dst - 16 + n, (const uint8_t*)src - 16 + n);
      return ret;
    }
    if (n <= 128)
    {
      goto COPY_BLOCK_128_BACK15;
    }
    if (n <= 512)
    {
      if (n >= 256)
      {
        n -= 256;
        rte_mov128((uint8_t*)dst, (const uint8_t*)src);
        rte_mov128((uint8_t*)dst + 128, (const uint8_t*)src + 128);
        src = (const uint8_t*)src + 256;
        dst = (uint8_t*)dst + 256;
      }
    COPY_BLOCK_255_BACK15:
      if (n >= 128)
      {
        n -= 128;
        rte_mov128((uint8_t*)dst, (const uint8_t*)src);
        src = (const uint8_t*)src + 128;
        dst = (uint8_t*)dst + 128;
      }
    COPY_BLOCK_128_BACK15:
      if (n >= 64)
      {
        n -= 64;
        rte_mov64((uint8_t*)dst, (const uint8_t*)src);
        src = (const uint8_t*)src + 64;
        dst = (uint8_t*)dst + 64;
      }
    COPY_BLOCK_64_BACK15:
      if (n >= 32)
      {
        n -= 32;
        rte_mov32((uint8_t*)dst, (const uint8_t*)src);
        src = (const uint8_t*)src + 32;
        dst = (uint8_t*)dst + 32;
      }
      if (n > 16)
      {
        rte_mov16((uint8_t*)dst, (const uint8_t*)src);
        rte_mov16((uint8_t*)dst - 16 + n, (const uint8_t*)src - 16 + n);
        return ret;
      }
      if (n > 0)
      {
        rte_mov16((uint8_t*)dst - 16 + n, (const uint8_t*)src - 16 + n);
      }
      return ret;
    }

    dstofss = (uintptr_t)dst & 0x0F;
    if (dstofss > 0)
    {
      dstofss = 16 - dstofss + 16;
      n -= dstofss;
      rte_mov32((uint8_t*)dst, (const uint8_t*)src);
      src = (const uint8_t*)src + dstofss;
      dst = (uint8_t*)dst + dstofss;
    }
    srcofs = ((uintptr_t)src & 0x0F);

    if (srcofs == 0)
    {
      for (; n >= 256; n -= 256)
      {
        rte_mov256((uint8_t*)dst, (const uint8_t*)src);
        dst = (uint8_t*)dst + 256;
        src = (const uint8_t*)src + 256;
      }

      goto COPY_BLOCK_255_BACK15;
    }

    MOVEUNALIGNED_LEFT47(dst, src, n, srcofs);

    goto COPY_BLOCK_64_BACK15;
  }

  #endif

  static ALWAYS_INLINE void* rte_memcpy_aligned(void* dst, const void* src, size_t n)
  {
    void* ret = dst;

    if (n < 16)
    {
      return rte_mov15_or_less(dst, src, n);
    }

    if (n <= 32)
    {
      rte_mov16((uint8_t*)dst, (const uint8_t*)src);
      rte_mov16((uint8_t*)dst - 16 + n, (const uint8_t*)src - 16 + n);

      return ret;
    }

    if (n <= 64)
    {
      rte_mov32((uint8_t*)dst, (const uint8_t*)src);
      rte_mov32((uint8_t*)dst - 32 + n, (const uint8_t*)src - 32 + n);

      return ret;
    }

    for (; n > 64; n -= 64)
    {
      rte_mov64((uint8_t*)dst, (const uint8_t*)src);
      dst = (uint8_t*)dst + 64;
      src = (const uint8_t*)src + 64;
    }

    rte_mov64((uint8_t*)dst - 64 + n, (const uint8_t*)src - 64 + n);

    return ret;
  }

  static ALWAYS_INLINE void* rte_memcpy(void* dst, const void* src, size_t n)
  {
    if (!(((uintptr_t)dst | (uintptr_t)src) & ALIGNMENT_MASK))
      return rte_memcpy_aligned(dst, src, n);
    else
      return rte_memcpy_generic(dst, src, n);
  }

  #undef ALIGNMENT_MASK
}
} // namespace rte
#endif

namespace bitlog
{

// Constants
static constexpr size_t CACHE_LINE_SIZE_BYTES{64u};
static constexpr size_t CACHE_LINE_ALIGNED{2 * CACHE_LINE_SIZE_BYTES};

/**
 * Checks if a number is a power of 2
 * @param number the number to check against
 * @return true if the number is a power of 2, false otherwise
 */
[[nodiscard]] constexpr bool is_power_of_two(uint64_t number) noexcept
{
  return (number != 0) && ((number & (number - 1)) == 0);
}

/**
 * Finds the next power of 2 greater than or equal to the given input
 * @param n input number
 * @return the next power of 2
 */
template <typename T>
[[nodiscard]] T next_power_of_2(T n) noexcept
{
  if (n >= std::numeric_limits<T>::max() / 2)
  {
    return std::numeric_limits<T>::max() / 2;
  }

  if (is_power_of_two(n))
  {
    return n;
  }

  T power = 1;
  while (power < n)
  {
    power <<= 1;
  }

  return power;
}

/**
 * Aligns a pointer to the specified boundary
 * @tparam Alignment desired alignment
 * @tparam T desired return type
 * @param pointer a pointer to an object
 * @return an aligned pointer for the given object
 */
template <uint64_t Alignment, typename T>
[[nodiscard]] constexpr T* align_to_boundary(void* pointer) noexcept
{
  static_assert(is_power_of_two(Alignment), "Alignment must be a power of two");
  return reinterpret_cast<T*>((reinterpret_cast<uintptr_t>(pointer) + (Alignment - 1u)) & ~(Alignment - 1u));
}

/**
 * Round up a value to the nearest multiple of a specified size.
 * @tparam T Type of value to round.
 * @param value The value to be rounded up.
 * @param roundTo The size to which the value should be rounded.
 * @return The rounded-up value.
 */
template <typename T>
[[nodiscard]] T round_up_to_nearest(T value, T roundTo) noexcept
{
  return ((value + roundTo - 1) / roundTo) * roundTo;
}

/**
 * @brief Retrieves the ID of the current thread.
 *
 * Retrieves the ID of the current thread using system-specific calls.
 *
 * @return The ID of the current thread.
 */
[[nodiscard]] uint32_t get_thread_id() noexcept
{
  thread_local uint32_t thread_id{0};

  if (!thread_id)
  {
    thread_id = static_cast<uint32_t>(::syscall(SYS_gettid));
  }

  return thread_id;
}

/**
 * @brief Retrieves the system's page size or returns the specified memory page size.
 *
 * Retrieves the system's page size if MemoryPageSize is set to RegularPage;
 * otherwise, returns the specified memory page size.
 *
 * @param memory_page_size The specified memory page size to use
 * @return The size of the system's page or the specified memory page size.
 */
[[nodiscard]] size_t get_page_size(size_t memory_page_size) noexcept
{
  if (!memory_page_size)
  {
    thread_local uint32_t page_size{0};
    if (!page_size)
    {
      page_size = static_cast<uint32_t>(sysconf(_SC_PAGESIZE));
    }
    return page_size;
  }
  else
  {
    return memory_page_size;
  }
}
} // namespace bitlog

export namespace bitlog
{

enum MemoryPageSize : size_t
{
  RegularPage = 0,
  HugePage2MB = 2 * 1024 * 1024,
  HugePage1GB = 1024 * 1024 * 1024
};

enum ErrorCode : uint32_t
{
  InvalidSharedMemoryPath = 10000
};

/**
 * A bounded single producer single consumer ring buffer.
 */
template <typename T, bool X86_CACHE_COHERENCE_OPTIMIZATION = false>
class BoundedQueueImpl
{
public:
  using integer_type = T;

private:
  struct Metadata
  {
    integer_type capacity;
    integer_type mask;
    integer_type bytes_per_batch;

    alignas(CACHE_LINE_ALIGNED) std::atomic<integer_type> atomic_writer_pos;
    alignas(CACHE_LINE_ALIGNED) integer_type writer_pos;
    integer_type last_flushed_writer_pos;
    integer_type reader_pos_cache;

    alignas(CACHE_LINE_ALIGNED) std::atomic<integer_type> atomic_reader_pos;
    alignas(CACHE_LINE_ALIGNED) integer_type reader_pos;
    integer_type last_flushed_reader_pos;
    integer_type writer_pos_cache;
  };

public:
  BoundedQueueImpl() = default;
  BoundedQueueImpl(BoundedQueueImpl const&) = delete;
  BoundedQueueImpl& operator=(BoundedQueueImpl const&) = delete;

  ~BoundedQueueImpl()
  {
    if (_storage && _metadata)
    {
      ::munmap(_storage, 2ull * static_cast<uint64_t>(_metadata->capacity));
      ::munmap(_metadata, sizeof(Metadata));
    }

    if (_filelock_fd >= 0)
    {
      ::flock(_filelock_fd, LOCK_UN | LOCK_NB);
      ::close(_filelock_fd);
    }
  }

  /**
   * @brief Initializes Creates shared memory for storage, metadata, lock, and ready indicators.
   *
   * @param capacity The capacity of the shared memory storage.
   * @param path_base The path within shared memory.
   * @param page_size The size of memory pages (default is RegularPage).
   * @param reader_store_percentage The percentage of memory reserved for the reader (default is 5%).
   *
   * @return An std::error_code indicating the success or failure of the initialization.
   */
  [[nodiscard]] std::error_code create(integer_type capacity, std::filesystem::path path_base,
                                       MemoryPageSize page_size = MemoryPageSize::RegularPage,
                                       integer_type reader_store_percentage = 5) noexcept
  {
    std::error_code ec{};

    // capacity must be always rounded to page size otherwise mmap will fail
    capacity = round_up_to_nearest(capacity, static_cast<integer_type>(get_page_size(page_size)));

    // 1. Create a storage file in shared memory
    path_base.replace_extension(".data");
    int storage_fd = ::open(path_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

    if (storage_fd >= 0)
    {
      if (ftruncate(storage_fd, static_cast<off_t>(capacity)) == 0)
      {
        ec = _memory_map_storage(storage_fd, capacity, page_size);

        if (!ec)
        {
          std::memset(_storage, 0, capacity);

          // 2. Create metadata file in shared memory
          path_base.replace_extension("members");
          int metadata_fd = ::open(path_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

          if (metadata_fd >= 0)
          {
            if (ftruncate(metadata_fd, static_cast<off_t>(sizeof(Metadata))) == 0)
            {
              ec = _memory_map_metadata(metadata_fd, sizeof(Metadata), page_size);

              if (!ec)
              {
                std::memset(_metadata, 0, sizeof(Metadata));

                _metadata->capacity = capacity;
                _metadata->mask = capacity - 1;
                _metadata->bytes_per_batch =
                  static_cast<integer_type>(capacity * static_cast<double>(reader_store_percentage) / 100.0);
                _metadata->atomic_writer_pos = 0;
                _metadata->writer_pos = 0;
                _metadata->last_flushed_writer_pos = 0;
                _metadata->reader_pos_cache = 0;
                _metadata->atomic_reader_pos = 0;
                _metadata->reader_pos = 0;
                _metadata->last_flushed_reader_pos = 0;
                _metadata->writer_pos_cache = 0;

                // 3. Create a lock file that works as a heartbeat mechanism for the reader
                path_base.replace_extension("lock");
                _filelock_fd = ::open(path_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

                if (_filelock_fd >= 0)
                {
                  if (::flock(_filelock_fd, LOCK_EX | LOCK_NB) == 0)
                  {
                    // 4. Create a ready file, indicating everything is ready and initialised and
                    // the reader can start loading the files
                    path_base.replace_extension("ready");
                    int readyfile_fd = ::open(path_base.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

                    if (readyfile_fd >= 0)
                    {
                      ::close(readyfile_fd);

                      // Queue creation is done
#if defined(X86_ARCHITECTURE_ENABLED)
                      if constexpr (X86_CACHE_COHERENCE_OPTIMIZATION)
                      {
                        // remove log memory from cache
                        for (uint64_t i = 0; i < (2ull * static_cast<uint64_t>(_metadata->capacity));
                             i += CACHE_LINE_SIZE_BYTES)
                        {
                          _mm_clflush(_storage + i);
                        }

                        uint64_t constexpr prefetched_cache_lines = 16;

                        for (uint64_t i = 0; i < prefetched_cache_lines; ++i)
                        {
                          _mm_prefetch(
                            reinterpret_cast<char const*>(_storage + (CACHE_LINE_SIZE_BYTES * i)), _MM_HINT_T0);
                        }
                      }
#endif
                    }
                    else
                    {
                      ec = std::error_code{errno, std::generic_category()};
                    }
                  }
                  else
                  {
                    ec = std::error_code{errno, std::generic_category()};
                  }
                }
                else
                {
                  ec = std::error_code{errno, std::generic_category()};
                }
              }
            }
            else
            {
              ec = std::error_code{errno, std::generic_category()};
            }

            ::close(metadata_fd);
          }
          else
          {
            ec = std::error_code{errno, std::generic_category()};
          }
        }
      }
      else
      {
        ec = std::error_code{errno, std::generic_category()};
      }

      ::close(storage_fd);
    }
    else
    {
      ec = std::error_code{errno, std::generic_category()};
    }

    return ec;
  }

  /**
   * @brief Opens existing shared memory storage and metadata files associated with a specified postfix ID.
   *
   * @param unique_id The unique ID used to identify the storage and metadata files.
   * @param path_base The path within shared memory.
   * @param page_size The size of memory pages (default is RegularPage).
   *
   * @return An std::error_code indicating the success or failure of the initialization.
   */
  [[nodiscard]] std::error_code open(std::string const& unique_id, std::filesystem::path path_base,
                                     MemoryPageSize page_size = MemoryPageSize::RegularPage) noexcept
  {
    std::error_code ec{};

    path_base.replace_extension("data");
    int storage_fd = ::open(path_base.c_str(), O_RDWR, 0660);

    if (storage_fd >= 0)
    {
      size_t const storage_file_size = std::filesystem::file_size(path_base, ec);

      if (!ec)
      {
        ec = _memory_map_storage(storage_fd, storage_file_size, page_size);

        if (!ec)
        {
          path_base.replace_extension("members");
          int metadata_fd = ::open(path_base.c_str(), O_RDWR, 0660);

          if (metadata_fd >= 0)
          {
            size_t const metadata_file_size = std::filesystem::file_size(path_base, ec);

            if (!ec)
            {
              ec = _memory_map_metadata(metadata_fd, metadata_file_size, page_size);

              if (!ec)
              {
                // finally we can also open the lockfile
                path_base.replace_extension("lock");
                _filelock_fd = ::open(path_base.c_str(), O_RDWR, 0660);

                if (_filelock_fd == -1)
                {
                  ec = std::error_code{errno, std::generic_category()};
                }
              }
            }

            ::close(metadata_fd);
          }
          else
          {
            ec = std::error_code{errno, std::generic_category()};
          }
        }
      }

      ::close(storage_fd);
    }
    else
    {
      ec = std::error_code{errno, std::generic_category()};
    }

    return ec;
  }

  /**
   * @brief Searches for ready files under the provided directory and retrieves unique IDs.
   *
   * This function scans the specified directory in shared memory and gathers unique IDs
   * from filenames that match the "ready-" pattern.
   *
   * @param unique_ids A vector to store the unique IDs found in the filenames.
   * @param path_base The path within shared memory.
   *
   * @return An std::error_code indicating the success or failure of the discovery process.
   */
  [[nodiscard]] static std::error_code discover_writers(std::vector<std::string>& unique_ids,
                                                        std::filesystem::path const& path_base) noexcept
  {
    std::error_code ec{};

    unique_ids.clear();

    auto di = std::filesystem::directory_iterator(path_base, ec);

    if (!ec)
    {
      for (auto const& entry : di)
      {
        if (entry.is_regular_file())
        {
          if (entry.path().extension().string() == ".ready")
          {
            unique_ids.emplace_back(entry.path().filename().stem());
          }
        }
      }
    }
    return ec;
  }

  [[nodiscard]] static std::expected<bool, std::error_code> is_created(std::string const& unique_id,
                                                                       std::filesystem::path const& path_base)
  {
    std::expected<bool, std::error_code> created;

    std::filesystem::path shm_file_base = path_base / unique_id;
    shm_file_base.replace_extension(".ready");

    std::error_code ec;
    created = std::filesystem::exists(shm_file_base, ec);

    if (ec)
    {
      created = std::unexpected{ec};
    }

    return created;
  }

  /**
   * @brief Removes shared memory files associated with a unique identifier.
   *
   * This function removes shared memory files (storage, metadata, ready, lock)
   * associated with a specified unique identifier.
   *
   * @param unique_id The unique identifier used to identify the shared memory files.
   * @param path_base The path within shared memory.
   *
   * @return An std::error_code indicating the success or failure
   */
  [[nodiscard]] static std::error_code remove_shm_files(std::string const& unique_id,
                                                        std::filesystem::path path_base) noexcept
  {
    std::error_code ec{};

    std::filesystem::path shm_file_base = path_base / unique_id;
    shm_file_base.replace_extension(".data");
    std::filesystem::remove(shm_file_base, ec);

    shm_file_base.replace_extension(".members");
    std::filesystem::remove(shm_file_base, ec);

    shm_file_base.replace_extension(".ready");
    std::filesystem::remove(shm_file_base, ec);

    shm_file_base.replace_extension(".lock");
    std::filesystem::remove(shm_file_base, ec);

    return ec;
  }

  /**
   * Checks if the process that created the queue is currently running by attempting to acquire
   * a file lock.
   * @return An std::expected<bool, std::error_code>:
   *         - std::expected(false) if the creator process is not running and the lock is acquired successfully.
   *         - std::expected(true) if the creator process is running and the file lock is unavailable.
   *         - std::unexpected<std::error_code> containing an error code if an error occurs during the check.
   */
  [[nodiscard]] std::expected<bool, std::error_code> is_creator_process_running() const noexcept
  {
    std::expected<bool, std::error_code> is_running;

    if (::flock(_filelock_fd, LOCK_EX | LOCK_NB) == 0)
    {
      // if we can lock the file, then the process is not running
      is_running = false;

      // we also want to unlock the file in case we repeat the check
      if (::flock(_filelock_fd, LOCK_UN | LOCK_NB) == -1)
      {
        is_running = std::unexpected{std::error_code{errno, std::generic_category()}};
      }
    }
    else if (errno == EWOULDBLOCK)
    {
      // the file is still locked
      is_running = true;
    }
    else
    {
      is_running = std::unexpected{std::error_code{errno, std::generic_category()}};
    }

    return is_running;
  }

  /**
   * @brief Prepares space for writing in the shared memory.
   *
   * This function checks available space in the storage to accommodate the given size for writing.
   * If there isn't enough space, it returns nullptr; otherwise, it returns the address for writing.
   *
   * @param n The size requested for writing.
   * @return A pointer to the location in memory for writing, or nullptr if insufficient space is available.
   */
  ALWAYS_INLINE [[nodiscard]] uint8_t* prepare_write(integer_type n) noexcept
  {
    if ((_metadata->capacity - static_cast<integer_type>(_metadata->writer_pos - _metadata->reader_pos_cache)) < n)
    {
      // not enough space, we need to load reader and re-check
      _metadata->reader_pos_cache = _metadata->atomic_reader_pos.load(std::memory_order_acquire);

      if ((_metadata->capacity - static_cast<integer_type>(_metadata->writer_pos - _metadata->reader_pos_cache)) < n)
      {
        return nullptr;
      }
    }

    return _storage + (_metadata->writer_pos & _metadata->mask);
  }

  /**
   * @brief Marks the completion of a write operation in the shared memory.
   *
   * This function updates the writer position by the given size after finishing a write operation.
   *
   * @param n The size of the completed write operation.
   */
  ALWAYS_INLINE void finish_write(integer_type n) noexcept { _metadata->writer_pos += n; }

  /**
   * @brief Commits the finished write operation in the shared memory.
   *
   * This function sets an atomic flag to indicate that the written data is available for reading.
   * It also performs cache-related operations for optimization if the architecture allows.
   */
  ALWAYS_INLINE void commit_write() noexcept
  {
    // set the atomic flag so the reader can see write
    _metadata->atomic_writer_pos.store(_metadata->writer_pos, std::memory_order_release);

#if defined(X86_ARCHITECTURE_ENABLED)
    if constexpr (X86_CACHE_COHERENCE_OPTIMIZATION)
    {
      // flush writen cache lines
      _flush_cachelines(_metadata->last_flushed_writer_pos, _metadata->writer_pos);

      // prefetch a future cache line
      _mm_prefetch(reinterpret_cast<char const*>(_storage + (_metadata->writer_pos & _metadata->mask) +
                                                 (CACHE_LINE_SIZE_BYTES * 10)),
                   _MM_HINT_T0);
    }
#endif
  }

  /**
   * @brief Prepares for reading from the shared memory.
   *
   * This function checks if there is data available for reading in the shared memory.
   * If data is available, it returns the address for reading; otherwise, it returns nullptr.
   *
   * @return A pointer to the location in memory for reading, or nullptr if no data is available.
   */
  ALWAYS_INLINE [[nodiscard]] uint8_t* prepare_read() noexcept
  {
    if (_metadata->writer_pos_cache == _metadata->reader_pos)
    {
      _metadata->writer_pos_cache = _metadata->atomic_writer_pos.load(std::memory_order_acquire);

      if (_metadata->writer_pos_cache == _metadata->reader_pos)
      {
        return nullptr;
      }
    }

    return _storage + (_metadata->reader_pos & _metadata->mask);
  }

  /**
   * @brief Marks the completion of a read operation in the shared memory.
   *
   * This function updates the reader position by the given size after finishing a read operation.
   *
   * @param n The size of the completed read operation.
   */
  ALWAYS_INLINE void finish_read(integer_type n) noexcept { _metadata->reader_pos += n; }

  /**
   * @brief Commits the finished read operation in the shared memory.
   *
   * This function updates the atomic reader position and performs cache-related operations
   * for optimization, if applicable based on the architecture.
   */
  ALWAYS_INLINE void commit_read() noexcept
  {
    if (static_cast<integer_type>(_metadata->reader_pos - _metadata->atomic_reader_pos.load(std::memory_order_relaxed)) >=
        _metadata->bytes_per_batch)
    {
      _metadata->atomic_reader_pos.store(_metadata->reader_pos, std::memory_order_release);

#if defined(X86_ARCHITECTURE_ENABLED)
      if constexpr (X86_CACHE_COHERENCE_OPTIMIZATION)
      {
        _flush_cachelines(_metadata->last_flushed_reader_pos, _metadata->reader_pos);
      }
#endif
    }
  }

  /**
   * @brief Checks if the shared memory storage is empty.
   *
   * This function is intended to be called by the reader to determine if the storage is empty.
   *
   * @return True if the storage is empty, otherwise false.
   */
  [[nodiscard]] bool empty() const noexcept
  {
    return _metadata->reader_pos == _metadata->atomic_writer_pos.load(std::memory_order_relaxed);
  }

  /**
   * @brief Retrieves the capacity of the storage.
   * @return An integer representing the storage capacity.
   */
  [[nodiscard]] integer_type capacity() const noexcept { return _metadata->capacity; }

private:
  /**
   * @brief Resolves the flags for memory mapping based on the specified page size.
   *
   * Resolves the flags required for memory mapping according to the specified memory page size.
   *
   * @param page_size The size of memory pages.
   * @return An integer representing the resolved mapping flags.
   */
  [[nodiscard]] static int resolve_mmap_flags(MemoryPageSize page_size) noexcept
  {
    int flags = MAP_SHARED;

    if (page_size == MemoryPageSize::HugePage2MB)
    {
      flags |= MAP_HUGETLB | MAP_HUGE_2MB;
    }
    else if (page_size == MemoryPageSize::HugePage1GB)
    {
      flags |= MAP_HUGETLB | MAP_HUGE_1GB;
    }

    return flags;
  }

  /**
   * @brief Maps the storage into memory.
   *
   * Maps the storage into memory using mmap and sets the storage address.
   *
   * @param fd The file descriptor for the storage.
   * @param size The size of the storage.
   * @param memory_page_size The size of memory pages.
   * @return An error code indicating success or failure of memory mapping.
   */
  [[nodiscard]] std::error_code _memory_map_storage(int fd, size_t size, MemoryPageSize memory_page_size) noexcept
  {
    // ask mmap for a good address where we can put both virtual copies of the buffer
    auto storage_address =
      static_cast<uint8_t*>(::mmap(nullptr, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    if (storage_address == MAP_FAILED)
    {
      return std::error_code{errno, std::generic_category()};
    }

    // map first region
    if (auto storage_address_1 = ::mmap(storage_address, size, PROT_READ | PROT_WRITE,
                                        MAP_FIXED | resolve_mmap_flags(memory_page_size), fd, 0);
        (storage_address_1 == MAP_FAILED) || (storage_address_1 != storage_address))
    {
      ::munmap(storage_address, 2 * size);
      return std::error_code{errno, std::generic_category()};
    }

    // map second region
    if (auto storage_address_2 = ::mmap(storage_address + size, size, PROT_READ | PROT_WRITE,
                                        MAP_FIXED | resolve_mmap_flags(memory_page_size), fd, 0);
        (storage_address_2 == MAP_FAILED) || (storage_address_2 != storage_address + size))
    {
      ::munmap(storage_address, 2 * size);
      return std::error_code{errno, std::generic_category()};
    }

    _storage = storage_address;

    return std::error_code{};
  }

  /**
   * @brief Maps the metadata into memory.
   *
   * Maps the metadata into memory using mmap and sets the metadata address.
   *
   * @param fd The file descriptor for the metadata.
   * @param size The size of the metadata.
   * @param memory_page_size The size of memory pages.
   * @return An error code indicating success or failure of memory mapping.
   */
  [[nodiscard]] std::error_code _memory_map_metadata(int fd, size_t size, MemoryPageSize memory_page_size) noexcept
  {
    auto metadata_address =
      ::mmap(nullptr, size, PROT_READ | PROT_WRITE, resolve_mmap_flags(memory_page_size), fd, 0);

    if (metadata_address == MAP_FAILED)
    {
      return std::error_code{errno, std::generic_category()};
    }

    // do not use std::construct_at(), it will perform value initialization
    _metadata = static_cast<Metadata*>(metadata_address);

    return std::error_code{};
  }

#if defined(X86_ARCHITECTURE_ENABLED)
  /**
   * @brief Flushes cache lines for optimization.
   *
   * Flushes cache lines in a specific range for optimization on x86 architectures.
   *
   * @param last The last position processed.
   * @param offset The current offset position.
   */
  void _flush_cachelines(integer_type& last, integer_type offset) noexcept
  {
    integer_type last_diff = last - (last & CACHELINE_MASK);
    integer_type const cur_diff = offset - (offset & CACHELINE_MASK);

    while (cur_diff > last_diff)
    {
      _mm_clflushopt(reinterpret_cast<void*>(_storage + (last_diff & _metadata->mask)));
      last_diff += CACHE_LINE_SIZE_BYTES;
      last = last_diff;
    }
  }
#endif

private:
  static constexpr integer_type CACHELINE_MASK{CACHE_LINE_SIZE_BYTES - 1};

  Metadata* _metadata{nullptr};
  uint8_t* _storage{nullptr};

  // local variables - not stored in shm
  int _filelock_fd{-1};
};

using BoundedQueue = BoundedQueueImpl<uint64_t, false>;
using BoundedQueueX86 = BoundedQueueImpl<uint64_t, true>;

/**
 * Literal class type that wraps a constant expression string.
 */
template <size_t N>
struct StringLiteral
{
  constexpr StringLiteral(char const (&str)[N]) { std::copy_n(str, N, value); }
  char value[N];
};

/**
 * Enum defining different type descriptors
 */
enum class TypeDescriptorName : uint8_t
{
  None = 0,
  SignedChar,
  UnsignedChar,
  ShortInt,
  UnsignedShortInt,
  Int,
  UnsignedInt,
  LongInt,
  UnsignedLongInt,
  LongLongInt,
  UnsignedLongLongInt,
  Float,
  Double
};

/**
 * Primary template for unsupported types
 */
template <typename T>
struct GetTypeDescriptor
{
  static_assert(false, "Unsupported type");
};

template <>
struct GetTypeDescriptor<char>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::SignedChar};
};

template <>
struct GetTypeDescriptor<unsigned char>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedChar};
};

template <>
struct GetTypeDescriptor<short int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::ShortInt};
};

template <>
struct GetTypeDescriptor<unsigned short int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedShortInt};
};

template <>
struct GetTypeDescriptor<int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::Int};
};

template <>
struct GetTypeDescriptor<unsigned int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedInt};
};

template <>
struct GetTypeDescriptor<long int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::LongInt};
};

template <>
struct GetTypeDescriptor<unsigned long int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedLongInt};
};

template <>
struct GetTypeDescriptor<long long int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::LongLongInt};
};

template <>
struct GetTypeDescriptor<unsigned long long int>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::UnsignedLongLongInt};
};

template <>
struct GetTypeDescriptor<float>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::Float};
};

template <>
struct GetTypeDescriptor<double>
{
  static constexpr TypeDescriptorName value{TypeDescriptorName::Double};
};

/**
 * Represents log levels
 */
enum class LogLevel : uint8_t
{
  TraceL3,
  TraceL2,
  TraceL1,
  Debug,
  Info,
  Warning,
  Error,
  Critical,
  None,
};

/**
 * Stores macro metadata
 */
struct MacroMetadata
{
  constexpr MacroMetadata(std::string_view file, std::string_view function, uint32_t line, LogLevel level,
                          std::string_view log_format, std::span<TypeDescriptorName const> type_descriptors)
    : type_descriptors(type_descriptors), file(file), function(function), log_format(log_format), line(line), level(level)
  {
  }

  std::span<TypeDescriptorName const> type_descriptors;
  std::string_view file;
  std::string_view function;
  std::string_view log_format;
  uint32_t line;
  LogLevel level;
};

/**
 * @brief Function to retrieve a reference to macro metadata.
 * @return Reference to the macro metadata.
 */
template <StringLiteral File, StringLiteral Function, uint32_t Line, LogLevel Level, StringLiteral LogFormat, typename... Args>
[[nodiscard]] MacroMetadata const& get_macro_metadata() noexcept
{
  static constexpr std::array<TypeDescriptorName, sizeof...(Args)> type_descriptors{
    GetTypeDescriptor<Args>::value...};

  static constexpr MacroMetadata macro_metadata{
    File.value, Function.value,  Line,
    Level,      LogFormat.value, std::span<TypeDescriptorName const>{type_descriptors}};

  return macro_metadata;
}

// Forward declaration
struct MacroMetadataNode;

/**
 * @brief Function returning a reference to the head of the macro metadata nodes.
 * @return Reference to the head of macro metadata nodes.
 */
[[nodiscard]] MacroMetadataNode*& macro_metadata_head() noexcept
{
  static MacroMetadataNode* head{nullptr};
  return head;
}

/**
 * @brief Generates unique IDs for instances of the specified template parameter type.
 *
 * This class is designed to produce unique identifiers, which can be utilized for various purposes
 * such as managing metadata, loggers, thread contexts.
 */
template <typename TDerived>
struct UniqueId
{
public:
  UniqueId() : id(_gen_id()) {}
  uint32_t const id;

private:
  [[nodiscard]] static uint32_t _gen_id() noexcept
  {
    static std::atomic<uint32_t> unique_id{0};
    return unique_id.fetch_add(1u, std::memory_order_relaxed);
  }
};

/**
 * Node to store macro metadata
 */
struct MacroMetadataNode : public UniqueId<MacroMetadataNode>
{
  explicit MacroMetadataNode(MacroMetadata const& macro_metadata)
    : macro_metadata(macro_metadata), next(std::exchange(macro_metadata_head(), this))
  {
  }

  MacroMetadata const& macro_metadata;
  MacroMetadataNode const* next;
};

/**
 * @brief Template instance for macro metadata node initialization.
 */
template <StringLiteral File, StringLiteral Function, uint32_t Line, LogLevel Level, StringLiteral LogFormat, typename... Args>
MacroMetadataNode marco_metadata_node{get_macro_metadata<File, Function, Line, Level, LogFormat, Args...>()};

template <typename TConfig>
class ThreadContext : public UniqueId<ThreadContext<TConfig>>
{
public:
  using queue_t = typename TConfig::queue_t;

  ThreadContext(ThreadContext const&) = delete;
  ThreadContext& operator=(ThreadContext const&) = delete;

  ThreadContext(TConfig const& config) : _config(config)
  {
    // TODO:: capacity and application_instance_id, also need to mkdir application_instance_id
    // TODO:: std::error_code res = _queue.create(4096, "application_instance_id");

    std::string const queue_file_base = std::format("{}.{}.ext", this->id, _queue_id++);
    std::error_code res = _queue.create(4096, _config.instance_dir() / queue_file_base);

    if (res)
    {
      // TODO:: Error handling ?
    }
  }

  [[nodiscard]] queue_t& get_queue() noexcept { return _queue; }

private:
  TConfig const& _config;
  queue_t _queue;
  uint32_t _queue_id{0}; /** Unique counter for each spawned queue by this thread context */
};

/**
 * This function retrieves the thread-specific context.
 *
 * The purpose is to ensure that only one instance of ThreadContext is created
 * per thread. Without this approach, using thread_local within a templated
 * function, e.g., log<>, could lead to multiple ThreadContext instances being
 * created for the same thread.
 */
template <typename TConfig>
ThreadContext<TConfig>& get_thread_context(TConfig const& config) noexcept
{
  thread_local ThreadContext<TConfig> thread_context{config};
  return thread_context;
}

/**
 * @brief Checks if the provided type is a C-style string (char const*, char*, or const char[]).
 * @return True if the type is a C-style string, false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool is_c_style_string_type() noexcept
{
  using arg_type = std::decay_t<T>;
  return std::disjunction_v<std::is_same<arg_type, char const*>, std::is_same<arg_type, char*>>;
}

/**
 * @brief Checks if the provided type is a std::string or std::string_view type.
 * @return True if the type is a std::string or std::string_view type, false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool is_std_string_type() noexcept
{
  using arg_type = std::decay_t<T>;
  return std::disjunction_v<std::is_same<arg_type, std::string>, std::is_same<arg_type, std::string_view>>;
}

/**
 * @brief Checks if the provided type is a char array (char[]).
 * @return True if the type is a char array, false otherwise.
 */
template <typename T>
[[nodiscard]] constexpr bool is_char_array_type() noexcept
{
  using arg_type = std::remove_cvref_t<T>;
  return std::conjunction_v<std::is_array<arg_type>, std::is_same<std::remove_cvref_t<std::remove_extent_t<T>>, char>>;
}

/**
 * @brief Counts the number of C-style string arguments among the provided arguments.
 * @return The count of C-style string arguments.
 */
template <typename... Args>
[[nodiscard]] constexpr uint32_t count_c_style_strings() noexcept
{
  return (0u + ... + is_c_style_string_type<Args>());
}

/**
 * @brief Calculates the total size required to encode the provided arguments and populates the c_style_string_lengths array.
 * @tparam Args Variadic template for the function arguments.
 * @param c_style_string_lengths Array to store the c_style_string_lengths of C-style strings and char arrays.
 * @param args The arguments to be encoded.
 * @return The total size required to encode the arguments.
 */
template <typename... Args>
constexpr uint32_t calculate_args_size_and_populate_string_lengths(uint32_t* c_style_string_lengths,
                                                                   Args const&... args) noexcept
{
  uint32_t c_style_string_lengths_index{0};
  auto get_arg_size = []<typename T>(uint32_t* c_style_string_lengths,
                                     uint32_t& c_style_string_lengths_index, T const& arg)
  {
    if constexpr (is_char_array_type<T>())
    {
      c_style_string_lengths[c_style_string_lengths_index] = strnlen(arg, std::extent_v<T>);
      return c_style_string_lengths[c_style_string_lengths_index++];
    }
    else if constexpr (is_c_style_string_type<T>())
    {
      // include one extra for the zero termination
      c_style_string_lengths[c_style_string_lengths_index] = strlen(arg) + 1u;
      return c_style_string_lengths[c_style_string_lengths_index++];
    }
    else if constexpr (is_std_string_type<T>())
    {
      return static_cast<uint32_t>(sizeof(uint32_t) + arg.length());
    }
    else
    {
      return static_cast<uint32_t>(sizeof(arg));
    }
  };

  return (0u + ... + get_arg_size(c_style_string_lengths, c_style_string_lengths_index, args));
}

/**
 * @brief Encodes an argument into a buffer.
 * @param buffer Pointer to the buffer for encoding.
 * @param c_style_string_lengths Array storing the c_style_string_lengths of C-style strings and char arrays.
 * @param c_style_string_lengths_index Index of the current string/array length in c_style_string_lengths.
 * @param arg The argument to be encoded.
 */
template <bool CustomMemcpy, typename T>
void encode_arg(uint8_t*& buffer, uint32_t const* c_style_string_lengths,
                uint32_t& c_style_string_lengths_index, T const& arg) noexcept
{
  if constexpr (is_char_array_type<T>())
  {
    // To support non-zero terminated arrays, copy the length first and then the actual string
    uint32_t const len = c_style_string_lengths[c_style_string_lengths_index++];

    if (CustomMemcpy)
    {
      rte::rte_memcpy(buffer, &len, sizeof(uint32_t));
      rte::rte_memcpy(buffer + sizeof(uint32_t), arg, len);
    }
    else
    {
      std::memcpy(buffer, &len, sizeof(uint32_t));
      std::memcpy(buffer + sizeof(uint32_t), arg, len);
    }

    buffer += sizeof(uint32_t) + len;
  }
  else if constexpr (is_c_style_string_type<T>())
  {
    uint32_t const len = c_style_string_lengths[c_style_string_lengths_index++];

    if (CustomMemcpy)
    {
      rte::rte_memcpy(buffer, arg, len);
    }
    else
    {
      std::memcpy(buffer, arg, len);
    }

    buffer += len;
  }
  else if constexpr (is_std_string_type<T>())
  {
    // Copy the length first and then the actual string
    auto const len = static_cast<uint32_t>(arg.length());

    if (CustomMemcpy)
    {
      rte::rte_memcpy(buffer, &len, sizeof(uint32_t));
      rte::rte_memcpy(buffer + sizeof(uint32_t), arg.data(), len);
    }
    else
    {
      std::memcpy(buffer, &len, sizeof(uint32_t));
      std::memcpy(buffer + sizeof(uint32_t), arg.data(), len);
    }

    buffer += sizeof(uint32_t) + len;
  }
  else
  {
    if (CustomMemcpy)
    {
      rte::rte_memcpy(buffer, &arg, sizeof(arg));
    }
    else
    {
      std::memcpy(buffer, &arg, sizeof(arg));
    }

    buffer += sizeof(arg);
  }
}

/**
 * @brief Encodes multiple arguments into a buffer.
 * @param buffer Pointer to the buffer for encoding.
 * @param c_style_string_lengths Array storing the c_style_string_lengths of C-style strings and char arrays.
 * @param args The arguments to be encoded.
 */
template <bool CustomMemcpy, typename... Args>
void encode(uint8_t*& buffer, uint32_t const* c_style_string_lengths, Args const&... args) noexcept
{
  uint32_t c_style_string_lengths_index{0};
  (encode_arg<CustomMemcpy>(buffer, c_style_string_lengths, c_style_string_lengths_index, args), ...);
}

class LoggerBase : public UniqueId<LoggerBase>
{
public:
  explicit LoggerBase(std::string name) : _name(std::move(name)){};

  /**
   * @return The log level of the logger
   */
  [[nodiscard]] LogLevel log_level() const noexcept
  {
    return _log_level.load(std::memory_order_relaxed);
  }

  /**
   * @return The name of the logger
   */
  [[nodiscard]] std::string const& name() const noexcept { return _name; }

  /**
   * Set the log level of the logger
   * @param log_level The new log level
   */
  void set_log_level(LogLevel level) { _log_level.store(level, std::memory_order_relaxed); }

  /**
   * Checks if the given log_statement_level can be logged by this logger
   * @param log_statement_level The log level of the log statement to be logged
   * @return bool if a message can be logged based on the current log level
   */
  [[nodiscard]] bool should_log(LogLevel log_statement_level) const noexcept
  {
    return log_statement_level >= log_level();
  }

private:
  std::string _name;
  std::atomic<LogLevel> _log_level{LogLevel::Info};
};

template <typename TQueue, bool CustomMemcpyX86>
class LogClientConfig
{
public:
  explicit LogClientConfig(std::string application_id,
                           std::filesystem::path shm_root_dir = std::filesystem::path{})
    : _application_id(std::move(application_id)), _root_dir(std::move(shm_root_dir))
  {
    // resolve shared memory directories
    if (_root_dir.empty())
    {
      if (std::filesystem::exists("/dev/shm"))
      {
        _root_dir = "/dev/shm";
      }
      else if (std::filesystem::exists("/tmp"))
      {
        _root_dir = "/tmp";
      }
    }

    _app_dir = _root_dir / _application_id;

    std::error_code ec{};

    if (!std::filesystem::exists(_app_dir))
    {
      std::filesystem::create_directories(_app_dir, ec);

      if (ec)
      {
        std::abort();
      }
    }

    uint64_t const start_ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();

    _instance_dir = _app_dir / std::to_string(start_ts);

    if (std::filesystem::exists(_instance_dir))
    {
      std::abort();
    }

    std::filesystem::create_directories(_instance_dir, ec);

    if (ec)
    {
      std::abort();
    }
  }

  using queue_t = TQueue;
  static constexpr bool use_custom_memcpy_x86 = CustomMemcpyX86;

  [[nodiscard]] std::filesystem::path const& root_dir() const noexcept { return _root_dir; }
  [[nodiscard]] std::filesystem::path const& app_dir() const noexcept { return _app_dir; }
  [[nodiscard]] std::filesystem::path const& instance_dir() const noexcept { return _instance_dir; }

private:
  std::string _application_id;
  std::filesystem::path _root_dir;
  std::filesystem::path _app_dir;
  std::filesystem::path _instance_dir;
};

// Forward declaration
template <typename TConfig>
class Bitlog;

template <typename TConfig>
class Logger : public LoggerBase
{
public:
  using config_t = TConfig;
  using queue_t = typename config_t::queue_t;

  template <StringLiteral File, StringLiteral Function, uint32_t Line, LogLevel Level, StringLiteral LogFormat, typename... Args>
  void log(Args const&... args)
  {
    uint32_t c_style_string_lengths[(std::max)(count_c_style_strings<Args...>(), static_cast<uint32_t>(1))];

    // Also reserve space for the timestamp and the metadata id
    uint32_t const args_size = static_cast<uint32_t>(sizeof(uint64_t) + sizeof(uint32_t)) +
      calculate_args_size_and_populate_string_lengths(c_style_string_lengths, args...);

    auto& thread_context = get_thread_context<config_t>(_config);
    uint8_t* write_buffer = thread_context.get_queue().prepare_write(args_size);

    if (write_buffer)
    {
      uint64_t const timestamp = std::chrono::system_clock::now().time_since_epoch().count();

      encode<TConfig::use_custom_memcpy_x86>(
        write_buffer, c_style_string_lengths, timestamp,
        marco_metadata_node<File, Function, Line, Level, LogFormat, Args...>.id, args...);

      thread_context.get_queue().finish_write(args_size);
      thread_context.get_queue().commit_write();
    }
    else
    {
      // Queue is full TODO
    }
  }

private:
  friend class Bitlog<TConfig>;

  // Private ctor
  Logger(std::string name, TConfig const& config) : LoggerBase(std::move(name)), _config(config) {}

  config_t const& _config;
};

class YAMLWriter
{
public:
  explicit YAMLWriter(std::filesystem::path const path)
  {
    _file_fd = ::open(path.c_str(), O_CREAT | O_RDWR | O_EXCL, 0660);

    if (_file_fd < 0)
    {
      std::abort();
    }
  }

  ~YAMLWriter()
  {
    if (_file_fd >= 0)
    {
      ::close(_file_fd);
    }
  }

  [[nodiscard]] std::error_code lock_file()
  {
    std::error_code ec{};

    if (::flock(_file_fd, LOCK_EX | LOCK_NB) == -1)
    {
      ec = std::error_code{errno, std::generic_category()};
    }

    return ec;
  }

  [[nodiscard]] std::error_code unlock_file()
  {
    std::error_code ec{};

    if (::flock(_file_fd, LOCK_UN | LOCK_NB) == -1)
    {
      ec = std::error_code{errno, std::generic_category()};
    }

    return ec;
  }

  [[nodiscard]] std::error_code write(MacroMetadataNode const* macro_metadata_node)
  {
    std::string type_descriptors_str;
    for (size_t i = 0; i < macro_metadata_node->macro_metadata.type_descriptors.size(); ++i)
    {
      type_descriptors_str +=
        std::to_string(static_cast<uint32_t>(macro_metadata_node->macro_metadata.type_descriptors[i]));

      if (i != macro_metadata_node->macro_metadata.type_descriptors.size() - 1)
      {
        type_descriptors_str += " ";
      }
    }

    std::string const result = std::format(
      "- id: {}\n  file: {}\n  line: {}\n  function: {}\n  log_format: {}\n  type_descriptors: "
      "{}\n  log_level: {}\n",
      macro_metadata_node->id, macro_metadata_node->macro_metadata.file,
      macro_metadata_node->macro_metadata.line, macro_metadata_node->macro_metadata.function,
      macro_metadata_node->macro_metadata.log_format, type_descriptors_str,
      static_cast<uint32_t>(macro_metadata_node->macro_metadata.level));

    return write(result.data(), result.size());
  }

  [[nodiscard]] std::error_code write(LoggerBase const* logger)
  {
    std::string const result = std::format("- id: {}\n  name: {}\n", logger->id, logger->name());

    return write(result.data(), result.size());
  }

private:
  std::error_code write(void const* buffer, size_t count)
  {
    std::error_code ec{};
    size_t total = 0;

    while (total < count)
    {
      ssize_t written = ::write(_file_fd, static_cast<uint8_t const*>(buffer) + total, count - total);

      if (written == -1)
      {
        if (errno == EINTR)
        {
          // Write was interrupted, retry
          continue;
        }
        ec = std::error_code{errno, std::generic_category()};
      }
      total += written;
    }

    return ec;
  }

private:
  int _file_fd{-1};
};

/**
 * Inits bitlog and also makes sure init runs once even when a different type of template
 * parameter is passed to Bitlog singleton
 * @return true if init run, false otherwise
 */
[[nodiscard]] bool bitlog_init()
{
  static std::once_flag once_flag;

  bool init_called{false};

  std::call_once(once_flag, [&init_called]() mutable { init_called = true; });

  return init_called;
}

template <typename TConfig>
class Bitlog
{
public:
  static void init(TConfig const& config) noexcept
  {
    if (bitlog_init())
    {
      // Set up the singleton ...
      _instance.reset(new Bitlog<TConfig>{config});

      // Then we proceed to write the log metadata file
      YAMLWriter metadata_writer{_instance->_config.instance_dir() / std::string{"log-statements-metadata.yaml"}};
      auto ec = metadata_writer.lock_file();

      if (ec)
      {
        std::abort();
      }

      // Store them in a vector to write them in reverse. It doesn't make difference just makes
      // the metadata file look consistent with the logger-metadata
      std::vector<bitlog::MacroMetadataNode const*> metadata_list;
      metadata_list.reserve(128);

      bitlog::MacroMetadataNode const* cur = bitlog::macro_metadata_head();
      while (cur)
      {
        metadata_list.push_back(cur);
        cur = cur->next;
      }

      for (auto it = metadata_list.rbegin(); it != metadata_list.rend(); ++it)
      {
        ec = metadata_writer.write(*it);

        if (ec)
        {
          std::abort();
        }
      }

      ec = metadata_writer.unlock_file();

      if (ec)
      {
        std::abort();
      }

      // Create another file for the logger-metadata
      _instance->_logger_metadata_writer =
        std::make_unique<YAMLWriter>(config.instance_dir() / std::string{"loggers-metadata.yaml"});
    }
  }

  static Bitlog<TConfig>& instance() noexcept
  {
    if (!_instance) [[unlikely]]
    {
      std::abort();
    }

    return *_instance;
  }

  TConfig const& config() const noexcept { return _config; }

  Logger<TConfig>* create_logger(std::string const& logger_name)
  {
    // TODO::
    // Construct Logger and also Sink
    // stdout sink, stderr sink, file sink, rotating file sink

    // Need to pass logger name, sink, formatter pattern

    std::lock_guard lock{_lock};

    auto search_it = std::lower_bound(std::begin(_loggers), std::end(_loggers), logger_name,
                                      [](std::unique_ptr<Logger<TConfig>> const& elem, std::string const& name)
                                      { return elem->name() < name; });

    // Check if the element with the same name already exists
    if (search_it == std::end(_loggers) || (*search_it)->name() != logger_name)
    {
      // Insert the new element while maintaining sorted order
      search_it = _loggers.emplace(search_it, new Logger<TConfig>(logger_name, _config));

      auto ec = _logger_metadata_writer->lock_file();

      if (ec)
      {
        // TODO:: maybe return nullptr or keep trying to lock here
        std::abort();
      }

      ec = _logger_metadata_writer->write(search_it->get());

      if (ec)
      {
        std::abort();
      }

      ec = _logger_metadata_writer->unlock_file();

      if (ec)
      {
        std::abort();
      }
    }

    return search_it->get();
  }

  Logger<TConfig>* get_logger(std::string const& name) const
  {
    std::lock_guard lock{_lock};

    auto search_it = std::lower_bound(std::begin(_loggers), std::end(_loggers), name,
                                      [](std::unique_ptr<Logger<TConfig>> const& elem, std::string const& name)
                                      { return elem->name() < name; });

    return (search_it != std::end(_loggers) && (*search_it)->name() == name) ? search_it->get() : nullptr;
  }

private:
  explicit Bitlog(TConfig const& config) : _config(config){};

  static inline std::unique_ptr<Bitlog<TConfig>> _instance;

  mutable std::mutex _lock;
  TConfig _config;
  std::vector<std::unique_ptr<Logger<TConfig>>> _loggers;
  std::unique_ptr<YAMLWriter> _logger_metadata_writer;
};
} // namespace bitlog
