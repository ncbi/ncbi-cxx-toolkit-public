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
 * Authors:  Eugene Vasilchenko, Andrei Gourianov
 *
 * File Description:
 *   SSE utility functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbi_fast.hpp>

#if defined(NCBI_HAVE_FAST_OPS)
BEGIN_NCBI_SCOPE
BEGIN_NAMESPACE(NFast);
//USING_NCBI_SCOPE;


void fill_n_zeros_aligned16(char* dst, size_t count)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 16 ) {
        _mm_store_si128((__m128i*)dst, zero);
    }
}

void fill_n_zeros_aligned16(int* dst, size_t count)
{
#if 1
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 16 ) {
        _mm_store_si128((__m128i*)dst+0, zero);
        _mm_store_si128((__m128i*)dst+1, zero);
        _mm_store_si128((__m128i*)dst+2, zero);
        _mm_store_si128((__m128i*)dst+3, zero);
    }
#else
    _ASSERT(count%4 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 4 ) {
        _mm_store_si128((__m128i*)dst, zero);
    }
#endif
}

void fill_n_zeros(char* dst, size_t count)
{
    memset(dst, 0, count*sizeof(*dst));
}

void fill_n_zeros(int* dst, size_t count)
{
    memset(dst, 0, count*sizeof(*dst));
}


#if 0
void copy_n_aligned16(const char* src, size_t count, char* dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    for ( auto src_end = src+count; src < src_end; dst += 16, src += 16 ) {
        _mm_store_si128((__m128i*)dst, *(__m128i*)src);
    }
}
#endif

void copy_n_aligned16(const int* src, size_t count, int* dst)
{
#if 1
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    for ( auto src_end = src+count; src < src_end; dst += 16, src += 16 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);
        _mm_store_si128((__m128i*)dst+0, ww0);
        _mm_store_si128((__m128i*)dst+1, ww1);
        _mm_store_si128((__m128i*)dst+2, ww2);
        _mm_store_si128((__m128i*)dst+3, ww3);
    }
#elif 0
    _ASSERT(count%4 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    for ( auto src_end = src+count; src < src_end; dst += 4, src += 4 ) {
        _mm_store_si128((__m128i*)dst, *(__m128i*)src);
    }
#else
    _ASSERT(count%4 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    for ( auto src_end = src+count; src < src_end; dst += 4, src += 4 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src);
        _mm_store_si128((__m128i*)dst, ww0);
    }
#endif
}





void copy_n_bytes_aligned16(const char* src, size_t count, int* dst)
{
#if 1
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i mask = _mm_set_epi8(-128, -128, -128, 3,
                                -128, -128, -128, 2,
                                -128, -128, -128, 1,
                                -128, -128, -128, 0);
    for ( auto src_end = src+count; src < src_end; dst += 16, src += 16 ) {
        uint32_t bb0 = ((const uint32_t*)src)[0];
        uint32_t bb1 = ((const uint32_t*)src)[1];
        uint32_t bb2 = ((const uint32_t*)src)[2];
        uint32_t bb3 = ((const uint32_t*)src)[3];
        __m128i ww0 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb0), mask);
        __m128i ww1 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb1), mask);
        __m128i ww2 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb2), mask);
        __m128i ww3 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb3), mask);
        _mm_store_si128((__m128i*)dst+0, ww0);
        _mm_store_si128((__m128i*)dst+1, ww1);
        _mm_store_si128((__m128i*)dst+2, ww2);
        _mm_store_si128((__m128i*)dst+3, ww3);
    }
#else
    _ASSERT(count%4 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i mask = _mm_set_epi8(-128, -128, -128, 3,
                                -128, -128, -128, 2,
                                -128, -128, -128, 1,
                                -128, -128, -128, 0);
    for ( auto src_end = src+count; src < src_end; dst += 4, src += 4 ) {
        uint32_t bb0 = ((const uint32_t*)src)[0];
        __m128i ww0 = _mm_shuffle_epi8(_mm_cvtsi32_si128(bb0), mask);
        _mm_store_si128((__m128i*)dst+0, ww0);
    }
#endif
}


void copy_n_aligned16(const int* src, size_t count, char* dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst)%16 == 0);
    __m128i mask = _mm_set_epi8(-128, -128, -128, -128,
                                -128, -128, -128, -128,
                                -128, -128, -128, -128,
                                12, 8, 4, 0);
    for ( auto src_end = src+count; src < src_end; dst += 16, src += 16 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);
        ww0 = _mm_shuffle_epi8(ww0, mask);
        ww1 = _mm_shuffle_epi8(ww1, mask);
        ww2 = _mm_shuffle_epi8(ww2, mask);
        ww3 = _mm_shuffle_epi8(ww3, mask);
        ww0 = _mm_or_si128(ww0, _mm_slli_si128(ww1, 4));
        ww2 = _mm_or_si128(ww2, _mm_slli_si128(ww3, 4));
        ww0 = _mm_or_si128(ww0, _mm_slli_si128(ww2, 8));
        _mm_store_si128((__m128i*)dst, ww0);
    }
}
    


void copy_4n_split_aligned16(const int* src, size_t count,
                             char* dst0, char* dst1, char* dst2, char* dst3)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst0)%16 == 0);
    _ASSERT(intptr_t(dst1)%16 == 0);
    _ASSERT(intptr_t(dst2)%16 == 0);
    _ASSERT(intptr_t(dst3)%16 == 0);
    for ( auto src_end = src+count*4; src < src_end;
          src += 64, dst0 += 16, dst1 += 16, dst2 += 16, dst3 += 16 ) {
        __m128i ww0, ww1, ww2, ww3;
        {
            __m128i tt0 = _mm_load_si128((const __m128i*)src+0);
            __m128i tt1 = _mm_load_si128((const __m128i*)src+1);
            __m128i tt2 = _mm_load_si128((const __m128i*)src+2);
            __m128i tt3 = _mm_load_si128((const __m128i*)src+3);
            tt0 = _mm_or_si128(tt0, _mm_slli_si128(tt1, 1));
            tt2 = _mm_or_si128(tt2, _mm_slli_si128(tt3, 1));
            ww0 = _mm_or_si128(tt0, _mm_slli_si128(tt2, 2));
        }
        {
            __m128i tt0 = _mm_load_si128((const __m128i*)src+4);
            __m128i tt1 = _mm_load_si128((const __m128i*)src+5);
            __m128i tt2 = _mm_load_si128((const __m128i*)src+6);
            __m128i tt3 = _mm_load_si128((const __m128i*)src+7);
            tt0 = _mm_or_si128(tt0, _mm_slli_si128(tt1, 1));
            tt2 = _mm_or_si128(tt2, _mm_slli_si128(tt3, 1));
            ww1 = _mm_or_si128(tt0, _mm_slli_si128(tt2, 2));
        }
        {
            __m128i tt0 = _mm_load_si128((const __m128i*)src+8);
            __m128i tt1 = _mm_load_si128((const __m128i*)src+9);
            __m128i tt2 = _mm_load_si128((const __m128i*)src+10);
            __m128i tt3 = _mm_load_si128((const __m128i*)src+11);
            tt0 = _mm_or_si128(tt0, _mm_slli_si128(tt1, 1));
            tt2 = _mm_or_si128(tt2, _mm_slli_si128(tt3, 1));
            ww2 = _mm_or_si128(tt0, _mm_slli_si128(tt2, 2));
        }
        {
            __m128i tt0 = _mm_load_si128((const __m128i*)src+12);
            __m128i tt1 = _mm_load_si128((const __m128i*)src+13);
            __m128i tt2 = _mm_load_si128((const __m128i*)src+14);
            __m128i tt3 = _mm_load_si128((const __m128i*)src+15);
            tt0 = _mm_or_si128(tt0, _mm_slli_si128(tt1, 1));
            tt2 = _mm_or_si128(tt2, _mm_slli_si128(tt3, 1));
            ww3 = _mm_or_si128(tt0, _mm_slli_si128(tt2, 2));
        }

        {
            // transpose 4x4
            __m128i tt0 = _mm_unpacklo_epi32(ww0, ww1);
            __m128i tt1 = _mm_unpacklo_epi32(ww2, ww3);
            __m128i tt2 = _mm_unpackhi_epi32(ww0, ww1);
            __m128i tt3 = _mm_unpackhi_epi32(ww2, ww3);
            
            ww0 = _mm_unpacklo_epi64(tt0, tt1);
            ww1 = _mm_unpackhi_epi64(tt0, tt1);
            ww2 = _mm_unpacklo_epi64(tt2, tt3);
            ww3 = _mm_unpackhi_epi64(tt2, tt3);
        }
        
        _mm_store_si128((__m128i*)dst0, ww0);
        _mm_store_si128((__m128i*)dst1, ww1);
        _mm_store_si128((__m128i*)dst2, ww2);
        _mm_store_si128((__m128i*)dst3, ww3);
    }
    for ( size_t i = 0; i < count; ++i ) {
        _ASSERT(Uint1(dst0[i-count]) == src[4*(i-count)+0]);
        _ASSERT(Uint1(dst1[i-count]) == src[4*(i-count)+1]);
        _ASSERT(Uint1(dst2[i-count]) == src[4*(i-count)+2]);
        _ASSERT(Uint1(dst3[i-count]) == src[4*(i-count)+3]);
    }
}
    

void copy_4n_split_aligned16(const int* src, size_t count,
                             int* dst0, int* dst1, int* dst2, int* dst3)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    _ASSERT(intptr_t(dst0)%16 == 0);
    _ASSERT(intptr_t(dst1)%16 == 0);
    _ASSERT(intptr_t(dst2)%16 == 0);
    _ASSERT(intptr_t(dst3)%16 == 0);
    for ( auto src_end = src+count*4; src < src_end;
          src += 16, dst0 += 4, dst1 += 4, dst2 += 4, dst3 += 4 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);

        {
            // transpose 4x4
            __m128i tt0 = _mm_unpacklo_epi32(ww0, ww1);
            __m128i tt1 = _mm_unpacklo_epi32(ww2, ww3);
            __m128i tt2 = _mm_unpackhi_epi32(ww0, ww1);
            __m128i tt3 = _mm_unpackhi_epi32(ww2, ww3);
            
            ww0 = _mm_unpacklo_epi64(tt0, tt1);
            ww1 = _mm_unpackhi_epi64(tt0, tt1);
            ww2 = _mm_unpacklo_epi64(tt2, tt3);
            ww3 = _mm_unpackhi_epi64(tt2, tt3);
        }
        
        _mm_store_si128((__m128i*)dst0, ww0);
        _mm_store_si128((__m128i*)dst1, ww1);
        _mm_store_si128((__m128i*)dst2, ww2);
        _mm_store_si128((__m128i*)dst3, ww3);
    }
}

/*
void copy_n(const int* src, size_t count, char* dst)
{
    for ( ; count && (intptr_t(dst)%4); ++dst, --count, ++src ) {
        *dst = *src;
    }
    __m128i mask = _mm_set_epi8(128, 128, 128, 128,
                                128, 128, 128, 128,
                                128, 128, 128, 128,
                                12, 8, 4, 0);
    for ( ; count >= 4; dst += 4, count -= 4, src += 4 ) {
        __m128i ww = _mm_load_si128((const __m128i*)src);
        uint32_t bb = _mm_cvtsi128_si32(_mm_shuffle_epi8(ww, mask));
        *(uint32_t*)dst = bb;
    }
    for ( ; count; ++dst, --count, ++src ) {
        *dst = *src;
    }
}


void copy_n(const int* src, size_t count, int* dst)
{
    for ( ; count && (intptr_t(dst)%16); ++dst, --count, ++src ) {
        *dst = *src;
    }
    __m128i mask = _mm_set_epi8(128, 128, 128, 128,
                                128, 128, 128, 128,
                                128, 128, 128, 128,
                                12, 8, 4, 0);
    for ( ; count >= 4; dst += 4, count -= 4, src += 4 ) {
        __m128i ww = _mm_load_si128((const __m128i*)src);
        uint32_t bb = _mm_cvtsi128_si32(_mm_shuffle_epi8(ww, mask));
        *(uint32_t*)dst = bb;
    }
    for ( ; count; ++dst, --count, ++src ) {
        *dst = *src;
    }
}
*/

unsigned max_element_n_aligned16(const unsigned* src, size_t count)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    __m128i max4 = _mm_setzero_si128();
    for ( auto src_end = src+count; src < src_end; src += 16 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);
        ww0 = _mm_max_epu32(ww0, ww1);
        ww2 = _mm_max_epu32(ww2, ww3);
        ww0 = _mm_max_epu32(ww0, ww2);
        max4 = _mm_max_epu32(max4, ww0);
    }
    max4 = _mm_max_epu32(max4, _mm_srli_si128(max4, 8));
    max4 = _mm_max_epu32(max4, _mm_srli_si128(max4, 4));
    return _mm_cvtsi128_si32(max4);
}


void max_element_n_aligned16(const unsigned* src, size_t count, unsigned& dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    __m128i max4 = _mm_set1_epi32(dst);
    for ( auto src_end = src+count; src < src_end; src += 16 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);
        ww0 = _mm_max_epu32(ww0, ww1);
        ww2 = _mm_max_epu32(ww2, ww3);
        ww0 = _mm_max_epu32(ww0, ww2);
        max4 = _mm_max_epu32(max4, ww0);
    }
    max4 = _mm_max_epu32(max4, _mm_srli_si128(max4, 8));
    max4 = _mm_max_epu32(max4, _mm_srli_si128(max4, 4));
    dst = _mm_cvtsi128_si32(max4);
}


void max_4elements_n_aligned16(const unsigned* src, size_t count, unsigned dst[4])
{
    _ASSERT(count%16 == 0);
    _ASSERT(intptr_t(src)%16 == 0);
    __m128i max4 = _mm_loadu_si128((__m128i*)dst);
    for ( auto src_end = src+count*4; src < src_end; src += 16 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src+0);
        __m128i ww1 = _mm_load_si128((const __m128i*)src+1);
        __m128i ww2 = _mm_load_si128((const __m128i*)src+2);
        __m128i ww3 = _mm_load_si128((const __m128i*)src+3);
        ww0 = _mm_max_epu32(ww0, ww1);
        ww2 = _mm_max_epu32(ww2, ww3);
        ww0 = _mm_max_epu32(ww0, ww2);
        max4 = _mm_max_epu32(max4, ww0);
    }
    _mm_storeu_si128((__m128i*)dst, max4);
}


END_NAMESPACE(NFast);

END_NCBI_SCOPE
#endif // NCBI_HAVE_FAST_OPS

