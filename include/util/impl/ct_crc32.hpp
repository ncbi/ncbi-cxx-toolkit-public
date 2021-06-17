#ifndef __CT_CRC32_HPP_INCLUDED__
#define __CT_CRC32_HPP_INCLUDED__

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
 *
 *  crc32   -- constexpr capable crc32 functions
 *
 *
 */
#include <cstddef>
#include <utility>
#include <cstdint>

#include <corelib/ncbistr.hpp>
#include <common/ncbi_export.h>

namespace compile_time_bits
{
    constexpr uint32_t sse42_poly = 0x82f63b78;
    constexpr uint32_t armv8_poly = 0x04c11db7;
    constexpr uint32_t platform_poly = sse42_poly;

    template<uint32_t poly>
    struct ct_crc32
    { // compile time CRC32, C++14 compatible
        using type = uint32_t;

        static constexpr type update(type crc, uint8_t b)
        {
            crc ^= b;
            for (int k{ 0 }; k < 8; k++)
                crc = crc & 1 ? (crc >> 1) ^ poly : crc >> 1;

            return crc;
        }

        static constexpr type update4(type crc, type d32)
        {
            size_t len = 4;
            while (len--) {
                uint8_t b = d32;
                d32 = d32 >> 8;
                crc = update(crc, b);
            }
            return crc;
        }

        template<bool _lowercase, size_t N>
        static constexpr type SaltedHash(const char(&s)[N]) noexcept
        {
            type hash{ update4(type(0), type(N - 1))};
            for (size_t i=0; i<N-1; ++i)
            {
                char c = s[i];
                uint8_t b = static_cast<uint8_t>((_lowercase && 'A' <= c && c <= 'Z') ? c + 'a' - 'A' : c);
                hash = update(hash, b);
            }
            return hash;
        }

        class MakeCRC32Table
        {
        public:
            struct cont_t
            {
                uint32_t m_data[256];
            };

            constexpr cont_t operator()() const noexcept
            {
                cont_t ret{};
                for (size_t i=0; i<256; ++i)
                    ret.m_data[i] = update(0, (uint8_t)i);
                return ret;
            }
        };
    }; // ct_crc32
} // namespace compile_time_bits

namespace ct
{
    template<ncbi::NStr::ECase case_sensitive>
    struct NCBI_XUTIL_EXPORT SaltedCRC32
    {
        using type = uint32_t;

        template<size_t N>
        static type constexpr ct(const char(&s)[N]) noexcept
        {
            auto hash = compile_time_bits::ct_crc32<compile_time_bits::platform_poly>::SaltedHash<case_sensitive==ncbi::NStr::eNocase, N>(s);
#if 0 
//def NCBI_COMPILER_ICC
// Intel compiler ignores uint32_to int32_t conversion at compile time
            if (hash >= 0x8000'0000)
            {
                int64_t h64 = hash-1;
                h64 ^= 0xFFFF'FFFF;
                return -h64;
            }
#endif
            return hash;
        }
        static type rt(const char* s, size_t realsize) noexcept
        {
#if defined(NCBI_SSE)  &&  NCBI_SSE >= 42
            auto hash = sse42(s, realsize);
#else
            auto hash = general(s, realsize);
#endif
            return hash;
        }
        static uint32_t general(const char* s, size_t realsize) noexcept;
#if defined(NCBI_SSE)  &&  NCBI_SSE >= 42
        static uint32_t sse42(const char* s, size_t realsize) noexcept;
#endif
    };
}

#endif


