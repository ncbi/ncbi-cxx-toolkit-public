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

BEGIN_NCBI_SCOPE

//USING_NCBI_SCOPE;

#if defined(NCBI_HAVE_FAST_OPS)

void NFast::x_sse_ClearBuffer(char* dst, size_t count)
{
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 16 ) {
        _mm_store_si128((__m128i*)dst, zero);
    }
}

void NFast::x_sse_ClearBuffer(int* dst, size_t count)
{
#if 1
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 16 ) {
        _mm_store_si128((__m128i*)dst+0, zero);
        _mm_store_si128((__m128i*)dst+1, zero);
        _mm_store_si128((__m128i*)dst+2, zero);
        _mm_store_si128((__m128i*)dst+3, zero);
    }
#else
    _ASSERT(count%4 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
    __m128i zero = _mm_setzero_si128();
    for ( auto dst_end = dst+count; dst < dst_end; dst += 4 ) {
        _mm_store_si128((__m128i*)dst, zero);
    }
#endif
}

void NFast::x_ClearBuffer(char* dst, size_t count)
{
    memset(dst, 0, count*sizeof(*dst));
}

void NFast::x_ClearBuffer(int* dst, size_t count)
{
    memset(dst, 0, count*sizeof(*dst));
}


#if 0
void NFast::x_sse_CopyBuffer(const char* src, size_t count, char* dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
    for ( auto src_end = src+count; src < src_end; dst += 16, src += 16 ) {
        _mm_store_si128((__m128i*)dst, *(__m128i*)src);
    }
}
#endif

void NFast::x_sse_CopyBuffer(const int* src, size_t count, int* dst)
{
#if 1
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
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
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
    for ( auto src_end = src+count; src < src_end; dst += 4, src += 4 ) {
        _mm_store_si128((__m128i*)dst, *(__m128i*)src);
    }
#else
    _ASSERT(count%4 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
    for ( auto src_end = src+count; src < src_end; dst += 4, src += 4 ) {
        __m128i ww0 = _mm_load_si128((const __m128i*)src);
        _mm_store_si128((__m128i*)dst, ww0);
    }
#endif
}


void NFast::x_sse_ConvertBuffer(const char* src, size_t count, int* dst)
{
#if 1
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
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
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
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


void NFast::x_sse_ConvertBuffer(const int* src, size_t count, char* dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst)%16 == 0);
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
    


void NFast::x_sse_SplitBufferInto4(const int* src, size_t count,
                             char* dst0, char* dst1, char* dst2, char* dst3)
{
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst0)%16 == 0);
    _ASSERT(uintptr_t(dst1)%16 == 0);
    _ASSERT(uintptr_t(dst2)%16 == 0);
    _ASSERT(uintptr_t(dst3)%16 == 0);
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
}
    

void NFast::x_sse_SplitBufferInto4(const int* src, size_t count,
                             int* dst0, int* dst1, int* dst2, int* dst3)
{
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
    _ASSERT(uintptr_t(dst0)%16 == 0);
    _ASSERT(uintptr_t(dst1)%16 == 0);
    _ASSERT(uintptr_t(dst2)%16 == 0);
    _ASSERT(uintptr_t(dst3)%16 == 0);
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
    for ( ; count && (uintptr_t(dst)%4); ++dst, --count, ++src ) {
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
    for ( ; count && (uintptr_t(dst)%16); ++dst, --count, ++src ) {
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

unsigned int NFast::x_sse_FindMaxElement(const unsigned int* src, size_t count)
{
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
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


void NFast::x_sse_FindMaxElement(const unsigned int* src, size_t count, unsigned int& dst)
{
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
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


void NFast::x_sse_Find4MaxElements(const unsigned int* src, size_t count, unsigned int dst[4])
{
    _ASSERT(count%16 == 0);
    _ASSERT(uintptr_t(src)%16 == 0);
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

#endif // NCBI_HAVE_FAST_OPS

void NFast::x_no_sse_SplitBufferInto4(const int* src, size_t count, int*  dest0, int*  dest1, int*  dest2, int*  dest3)
{
    for (size_t e = 0; e < count; ++e) {
        int v0 = src[e*4+0];
        int v1 = src[e*4+1];
        int v2 = src[e*4+2];
        int v3 = src[e*4+3];
        dest0[e] = v0;
        dest1[e] = v1;
        dest2[e] = v2;
        dest3[e] = v3;
    }
}

void NFast::x_no_sse_SplitBufferInto4(const int* src, size_t count, char* dest0, char* dest1, char* dest2, char* dest3)
{
    for (size_t e = 0; e < count; ++e) {
        char v0 = char(src[e*4+0]);
        char v1 = char(src[e*4+1]);
        char v2 = char(src[e*4+2]);
        char v3 = char(src[e*4+3]);
        dest0[e] = v0;
        dest1[e] = v1;
        dest2[e] = v2;
        dest3[e] = v3;
    }
}

unsigned int NFast::x_no_sse_FindMaxElement(const unsigned int* src, size_t count, unsigned int v)
{
    unsigned int result = v;
    for (size_t e = 0; e < count; ++e) {
        if (result < *(src + e)) {
            result = *(src + e);
        }
    } 
    return result;
}

void NFast::x_no_sse_Find4MaxElements(const unsigned int* src, size_t count, unsigned int dest[4])
{
    unsigned int result[4];
    memcpy(result, dest, sizeof(unsigned int) * 4);
    for (size_t e = 0; e < 4*count; e += 4) {
        if (result[0] < *(src + e)) {
            result[0] = *(src + e);
        }
        if (result[1] < *(src + e + 1)) {
            result[1] = *(src + e + 1);
        }
        if (result[2] < *(src + e + 2)) {
            result[2] = *(src + e + 2);
        }
        if (result[3] < *(src + e + 3)) {
            result[3] = *(src + e + 3);
        }
    } 
    memcpy(dest, result, sizeof(unsigned int) * 4);
}



END_NCBI_SCOPE

