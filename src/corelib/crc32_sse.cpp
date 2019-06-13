/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Sergiy Gotvyanskyy
 *
 * File Description:
 *   CPU specific implementations of CRC32
 *
 */

#include <ncbi_pch.hpp>


#include <corelib/impl/ct_crc32.hpp>

//#define __ENABLE_SSE42

#ifdef  __ENABLE_SSE42
#if __has_include (<x86intrin.h>)
#include <x86intrin.h>
#elif __has_include (<intrin.h>)
#include <intrin.h>
#else
#error "need Intel/AMD64 intrinsics include"
#endif

#if __has_include (<immintrin.h>)
#include <immintrin.h>
#endif

#endif

static
constexpr
auto g_crc32_table = crc32::ct_crc32<crc32::platform_poly>::MakeCRC32Table{}();

static
// this should generate non branching lower case functionality
int convert_lower_case(int c) noexcept
{
    int offset = 0;
    if (c >= int('A'))
        offset = 'a' - 'A';
    if (c > int('Z'))
        offset = 0;
    return c + offset;
}

struct tabled_crc32
{
    using type = uint32_t;

    static type update(type crc, uint8_t b) noexcept
    {
        crc = g_crc32_table.m_data[(crc ^ b) & 0xFFL] ^ (crc >> 8);

        return crc;
    }
    static type update(type crc, type d32) noexcept
    {
        size_t len = 4;
        while (len--) {
            uint8_t b = d32;
            d32 = d32 >> 8;
            crc = update(crc, b);
        }
        return crc;
    }
};

#ifdef  __ENABLE_SSE42
struct sse42_crc32
{
    using type = uint32_t;

    static type update(type crc, type d32) noexcept
    {
        return _mm_crc32_u32(crc, d32);
    }
    static type update(type crc, uint8_t d8) noexcept
    {
        return _mm_crc32_u8(crc, d8);
    }
    static type update(type crc, const uint8_t *buf, size_t len) noexcept
    {
#ifdef SLOW_UNALIGNED_MEMORY_ACCESS
        size_t aligned = (uintptr_t(buf) & 2);
        while (aligned-- && len--) {
            uint8_t b = *buf++;
            crc = _mm_crc32_u8(crc, b);
        }
#endif

        while (len > 3) {
            uint32_t d32 = *(const uint32_t*)buf;
            buf += 4;
            len -= 4;
            crc = _mm_crc32_u32(crc, d32);
        }

        while (len--) {
            uint8_t b = *buf++;
            crc = _mm_crc32_u8(crc, b);
        }
        return crc;
    }
};

template<bool case_sensitive>
uint32_t ct::SaltedCRC32<case_sensitive>::sse42(const char* s, size_t size) noexcept
{
    uint32_t len = (uint32_t)size;
    uint32_t hash = sse42_crc32::update(0, len);
    while (len--)
    {
        int c = static_cast<uint8_t>(*s++);
        if (!case_sensitive)
            c = convert_lower_case(c);
        hash = sse42_crc32::update(hash, static_cast<uint8_t>(c));
    }
    return hash;
}

#endif

template<bool case_sensitive>
uint32_t ct::SaltedCRC32<case_sensitive>::general(const char* s, size_t size) noexcept
{
    uint32_t len = (uint32_t)size;
    uint32_t hash = tabled_crc32::update(0, len);
    while (len--)
    {
        int c = static_cast<uint8_t>(*s++);
        if (!case_sensitive)
            c = convert_lower_case(c);
        hash = tabled_crc32::update(hash, static_cast<uint8_t>(c));
    }
    return hash;
}

template uint32_t ct::SaltedCRC32<true>::general(const char* s, size_t size) noexcept;
template uint32_t ct::SaltedCRC32<false>::general(const char* s, size_t size) noexcept;
#ifdef  __ENABLE_SSE42
template uint32_t ct::SaltedCRC32<true>::sse42(const char* s, size_t size) noexcept;
template uint32_t ct::SaltedCRC32<false>::sse42(const char* s, size_t size) noexcept;
#endif

