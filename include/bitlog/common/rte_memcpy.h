#pragma once

#include "bitlog/common/common.h"

#define BITLOG_RTE_PACKED __attribute__((__packed__))
#define BITLOG_RTE_MAY_ALIAS __attribute__((__may_alias__))

#if defined(BITLOG_X86_ARCHITECTURE_ENABLED)
  #include <immintrin.h>
  #include <x86intrin.h>

namespace bitlog::detail
{
extern "C"
{
  BITLOG_ALWAYS_INLINE inline void* rte_mov15_or_less(void* dst, const void* src, size_t n)
  {
    struct rte_uint64_alias
    {
      uint64_t val;
    } BITLOG_RTE_PACKED BITLOG_RTE_MAY_ALIAS;
    struct rte_uint32_alias
    {
      uint32_t val;
    } BITLOG_RTE_PACKED BITLOG_RTE_MAY_ALIAS;
    struct rte_uint16_alias
    {
      uint16_t val;
    } BITLOG_RTE_PACKED BITLOG_RTE_MAY_ALIAS;

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
    #define BITLOG_ALIGNMENT_MASK 0x1F

  BITLOG_ALWAYS_INLINE inline void rte_mov16(uint8_t* dst, const uint8_t* src)
  {
    __m128i xmm0;

    xmm0 = _mm_loadu_si128((const __m128i*)(const void*)src);
    _mm_storeu_si128((__m128i*)(void*)dst, xmm0);
  }

  BITLOG_ALWAYS_INLINE inline void rte_mov32(uint8_t* dst, const uint8_t* src)
  {
    __m256i ymm0;

    ymm0 = _mm256_loadu_si256((const __m256i*)(const void*)src);
    _mm256_storeu_si256((__m256i*)(void*)dst, ymm0);
  }

  BITLOG_ALWAYS_INLINE inline void rte_mov64(uint8_t* dst, const uint8_t* src)
  {
    rte_mov32((uint8_t*)dst + 0 * 32, (const uint8_t*)src + 0 * 32);
    rte_mov32((uint8_t*)dst + 1 * 32, (const uint8_t*)src + 1 * 32);
  }

  BITLOG_ALWAYS_INLINE inline void rte_mov128(uint8_t* dst, const uint8_t* src)
  {
    rte_mov32((uint8_t*)dst + 0 * 32, (const uint8_t*)src + 0 * 32);
    rte_mov32((uint8_t*)dst + 1 * 32, (const uint8_t*)src + 1 * 32);
    rte_mov32((uint8_t*)dst + 2 * 32, (const uint8_t*)src + 2 * 32);
    rte_mov32((uint8_t*)dst + 3 * 32, (const uint8_t*)src + 3 * 32);
  }

  BITLOG_ALWAYS_INLINE inline void rte_mov256(uint8_t* dst, const uint8_t* src)
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

  BITLOG_ALWAYS_INLINE inline void rte_mov128blocks(uint8_t* dst, const uint8_t* src, size_t n)
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

  inline void* rte_memcpy_generic(void* dst, const void* src, size_t n)
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
    #define BITLOG_ALIGNMENT_MASK 0x0F

  BITLOG_ALWAYS_INLINE inline void rte_mov16(uint8_t* dst, const uint8_t* src)
  {
    __m128i xmm0;

    xmm0 = _mm_loadu_si128((const __m128i*)(const void*)src);
    _mm_storeu_si128((__m128i*)(void*)dst, xmm0);
  }

  BITLOG_ALWAYS_INLINE inline void rte_mov32(uint8_t* dst, const uint8_t* src)
  {
    rte_mov16((uint8_t*)dst + 0 * 16, (const uint8_t*)src + 0 * 16);
    rte_mov16((uint8_t*)dst + 1 * 16, (const uint8_t*)src + 1 * 16);
  }

  BITLOG_ALWAYS_INLINE inline void rte_mov64(uint8_t* dst, const uint8_t* src)
  {
    rte_mov16((uint8_t*)dst + 0 * 16, (const uint8_t*)src + 0 * 16);
    rte_mov16((uint8_t*)dst + 1 * 16, (const uint8_t*)src + 1 * 16);
    rte_mov16((uint8_t*)dst + 2 * 16, (const uint8_t*)src + 2 * 16);
    rte_mov16((uint8_t*)dst + 3 * 16, (const uint8_t*)src + 3 * 16);
  }

  BITLOG_ALWAYS_INLINE inline void rte_mov128(uint8_t* dst, const uint8_t* src)
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

  inline void rte_mov256(uint8_t* dst, const uint8_t* src)
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

    #define BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, len, offset)                                     \
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

    #define BITLOG_MOVEUNALIGNED_LEFT47(dst, src, len, offset)                                     \
      __extension__({                                                                              \
        switch (offset)                                                                            \
        {                                                                                          \
        case 0x01:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x01);                               \
          break;                                                                                   \
        case 0x02:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x02);                               \
          break;                                                                                   \
        case 0x03:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x03);                               \
          break;                                                                                   \
        case 0x04:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x04);                               \
          break;                                                                                   \
        case 0x05:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x05);                               \
          break;                                                                                   \
        case 0x06:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x06);                               \
          break;                                                                                   \
        case 0x07:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x07);                               \
          break;                                                                                   \
        case 0x08:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x08);                               \
          break;                                                                                   \
        case 0x09:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x09);                               \
          break;                                                                                   \
        case 0x0A:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0A);                               \
          break;                                                                                   \
        case 0x0B:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0B);                               \
          break;                                                                                   \
        case 0x0C:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0C);                               \
          break;                                                                                   \
        case 0x0D:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0D);                               \
          break;                                                                                   \
        case 0x0E:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0E);                               \
          break;                                                                                   \
        case 0x0F:                                                                                 \
          BITLOG_BITLOG_MOVEUNALIGNED_LEFT47_IMM(dst, src, n, 0x0F);                               \
          break;                                                                                   \
        default:;                                                                                  \
        }                                                                                          \
      })

  inline void* rte_memcpy_generic(void* dst, const void* src, size_t n)
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

    BITLOG_MOVEUNALIGNED_LEFT47(dst, src, n, srcofs);

    goto COPY_BLOCK_64_BACK15;
  }

  #endif

  BITLOG_ALWAYS_INLINE inline void* rte_memcpy_aligned(void* dst, const void* src, size_t n)
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

  BITLOG_ALWAYS_INLINE inline void* rte_memcpy(void* dst, const void* src, size_t n)
  {
    if (!(((uintptr_t)dst | (uintptr_t)src) & BITLOG_ALIGNMENT_MASK))
      return rte_memcpy_aligned(dst, src, n);
    else
      return rte_memcpy_generic(dst, src, n);
  }

  #undef BITLOG_ALIGNMENT_MASK
}
} // namespace bitlog::detail
#endif