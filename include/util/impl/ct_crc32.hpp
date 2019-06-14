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

namespace compile_time_bits
{
    template<size_t N, typename _Ret>
    struct forward_sequence
    {
        using next_t = forward_sequence<N - 1, _Ret>;
        template<typename T, typename...TArgs>
        constexpr _Ret operator()(const T& op, TArgs&&...args) const noexcept
        {
            return next_t{}(op, op(N - 1), std::forward<TArgs>(args)...);
        }
    };

    template<typename _Ret>
    struct forward_sequence<0, _Ret>
    {
        template<typename T, typename...TArgs>
        constexpr _Ret operator()(const T&, TArgs&&...args) const noexcept
        {
            return { {std::forward<TArgs>(args)...} };
        }
    };
};

namespace crc32
{

    constexpr uint32_t sse42_poly = 0x82f63b78;
    constexpr uint32_t armv8_poly = 0x04c11db7;
    constexpr uint32_t platform_poly = sse42_poly;

    template<uint32_t poly>
    struct ct_crc32
    { // compile time CRC32, C++11 compatible
        using type = uint32_t;

#ifdef NCBI_CPP17
        static constexpr type update17(type crc, uint8_t b)
        {
            crc ^= b;
            for (int k{ 0 }; k < 8; k++)
                crc = crc & 1 ? (crc >> 1) ^ poly : crc >> 1;

            return crc;
        }

        static constexpr type update17(type crc, type d32)
        {
            size_t len = 4;
            while (len--) {
                uint8_t b = d32;
                d32 = d32 >> 8;
                crc = update17(crc, b);
            }
            return crc;
        }
        template<size_t N>
        static constexpr type SaltedHash17(const char(&base)[N])
        {
            type hash = update17(type(0), type(N - 1));
            for (size_t o{ 0 }; base[o] != 0; ++o)
            {
                hash = update17(hash, static_cast<uint8_t>(base[o]));
            }
            return hash;
        }
#endif

        static constexpr type iterate(type crc) noexcept
        {
            return crc & 1 ? (crc >> 1) ^ poly : crc >> 1;
        }

        template<size_t i, typename...Unused>
        struct update_t
        {
            using next_t = update_t<i - 1, Unused...>;
            constexpr type operator()(type crc) const noexcept
            {
                return next_t{}(iterate(crc));
            }
        };

        template<typename...Unused>
        struct update_t<0, Unused...>
        {
            constexpr type operator()(type crc) const noexcept
            {
                return crc;
            }
        };

        static constexpr type update(type crc, uint8_t b) noexcept
        {
            return update_t<8>{}(crc ^ b);
        }

        template<size_t i, typename...Unused>
        struct update4_t
        {
            using next_t = update4_t<i - 1>;
            constexpr type operator()(type crc, uint32_t rest) const noexcept
            {
                return next_t{}(update(crc, rest & 255), rest >> 8);
            }
        };
        template<typename...Unused>
        struct update4_t<0, Unused...>
        {
            constexpr type operator()(type crc, uint32_t rest) const noexcept
            {
                return crc;
            }
        };

        template<size_t N, bool _lowercase>
        struct fetch_t
        {
            static constexpr char lower_case(char c) noexcept
            {
                return (_lowercase && 'A' <= c && c <= 'Z') ? c + 'a' - 'A' : c;
            }
            constexpr uint8_t operator()(const char* in, size_t i) const noexcept
            {
                return static_cast<uint8_t>(lower_case(in[N - i]));
            }
        };

        template<size_t i, typename _Fetch>
        struct ct_hash_t
        {
            using next_t = ct_hash_t<i - 1, _Fetch>;

            constexpr type operator()(type hash, const char* s) const noexcept
            {
                return (_Fetch{}(s, i) == 0) ? hash :
                    next_t{}(update(hash, _Fetch{}(s, i)), s);
            }
        };

        template<typename _Fetch>
        struct ct_hash_t<0, _Fetch>
        {
            constexpr type operator()(type hash, const char* s) const noexcept
            {
                return hash;
            };
        };

        template<bool lower_case, size_t N>
        static constexpr type SaltedHash(const char(&s)[N]) noexcept
        {
            return ct_hash_t<N, fetch_t<N, lower_case>>{}(
                update4_t<4>{}(type(0), type(N - 1)),
                s);
        }

        class MakeCRC32Table
        {
        private:
            struct next_value
            {//should be replaced with C++17 constexpr lambda
                constexpr uint32_t operator()(size_t i) const noexcept
                {
                    return update(0, (uint8_t)i);
                }
            };
        public:
            struct cont_t
            {
                uint32_t m_data[256];
            };

            constexpr cont_t operator()() const noexcept
            {
                return compile_time_bits::forward_sequence<256, cont_t >{}(next_value{});
            }
        };
    }; // ct_crc32
}; // namespace crc32

namespace ct
{
    template<bool case_sensitive>
    struct SaltedCRC32
    {
        using type = uint32_t;

        template<size_t N>
        static type constexpr ct(const char(&s)[N]) noexcept
        {
            return crc32::ct_crc32<crc32::platform_poly>::SaltedHash<!case_sensitive, N>(s);
        }
        static type sse42(const char* s, size_t realsize) noexcept;
        static type general(const char* s, size_t realsize) noexcept;
    };
};

constexpr
auto hash_good = ct::SaltedCRC32<true>::ct("Good");
static_assert(948072359 == hash_good, "not good");
static_assert(
    ct::SaltedCRC32<false>::ct("Good") == ct::SaltedCRC32<true>::ct("good"),
    "not good");

#endif


