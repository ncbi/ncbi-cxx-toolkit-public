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
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   Test SSE utility functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_fast.hpp>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;


#if 0
#define SSETEST_BUFSIZE 128
#define SSETEST_COUNT   100
#elif 1
#define SSETEST_BUFSIZE 51200
#define SSETEST_COUNT   500000
#elif 0
#define SSETEST_BUFSIZE 102400
#define SSETEST_COUNT   100000
#else
#define SSETEST_BUFSIZE 1'280'000
#define SSETEST_COUNT   1'000'000
#endif
#ifdef _DEBUG
#undef SSETEST_COUNT
#if 0
#define SSETEST_COUNT 1
#else
#define SSETEST_COUNT 5000
#endif
#endif

#ifdef NCBI_COMPILER_MSVC
inline Uint8 QUERY_PERF_COUNTER(void) {
    LARGE_INTEGER p;
    return QueryPerformanceCounter(&p) ? p.QuadPart : 0;
}
#else
inline Uint8 QUERY_PERF_COUNTER(void) {
    timespec p;
    clock_gettime(CLOCK_REALTIME, &p);
    return p.tv_sec * 1000 * 1000 + p.tv_nsec / 1000;
}
#endif

/////////////////////////////////////////////////////////////////////////////

template<class V>
void s_set_zero(V* buf, size_t buf_size)
{
    memset(buf, 0, buf_size*sizeof(*buf));
}
template<class V>
void s_set_non_zero(V* buf, size_t buf_size)
{
    memset(buf, 0xaa, buf_size*sizeof(*buf));
}
template<class V>
void s_check_zero(const V* buf, size_t buf_size)
{
    for ( size_t i = 0; i < buf_size; ++i ) {
        _ASSERT(buf[i] == V(0));
    }
}
template<class V>
void s_check_non_zero(const V* buf, size_t buf_size)
{
    for ( size_t i = 0; i < buf_size; ++i ) {
        _ASSERT(buf[i] == V(0xaaaaaaaa));
    }
}
template<class V1, class V2>
void s_check_equal(const V1* buf, size_t buf_size, const V2* dst)
{
    for ( size_t i = 0; i < buf_size; ++i ) {
        V2 v = V2(buf[i]);
        if ( sizeof(V1) == 1 && sizeof(V2) == 4 ) {
            // unsigned char to int
            v &= 0xff;
        }
        _ASSERT(dst[i] == v);
    }
}
template<class V>
inline void s_payload(V* buf, size_t buf_size)
{
    for (size_t j = 0; j < buf_size; j += 64/sizeof(V)) {
        buf[j] += 1;
    }
}

#if 1
BOOST_AUTO_TEST_CASE(TestFillZerosChar)
{
#ifdef NCBI_HAVE_FAST_OPS
    cout << "NCBI_HAVE_FAST_OPS defined " << endl;
#else
    cout << "NCBI_HAVE_FAST_OPS not defined " << endl;
#endif
    cout << "SSETEST_BUFSIZE = " << SSETEST_BUFSIZE << endl;
    cout << "SSETEST_COUNT = " << SSETEST_COUNT << endl;
    cout << "sizeof(char) = " << sizeof(char) << endl;
    cout << "sizeof(int) = "  << sizeof(int) << endl;


    cout << endl << "TestFillZerosChar" << endl;
    const size_t buf_size = 4 * SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    char* buf = (char*)malloc(buf_size * sizeof(char));
    Uint8 start, finish;

#ifdef NCBI_HAVE_FAST_OPS
    s_set_non_zero(buf, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(buf, buf_size);
        NFast::fill_n_zeros_aligned16(buf,buf_size);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::fill_n_zeros_aligned16(char)" << endl;
    s_check_zero(buf, buf_size);

    s_set_non_zero(buf, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(buf, buf_size);
        NFast::fill_n_zeros(buf,buf_size);
    }
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::fill_n_zeros(char)" << endl;
    s_check_zero(buf, buf_size);
#endif

    s_set_non_zero(buf, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(buf, buf_size);
        NFast::x_no_ncbi_sse_fill_n_zeros(buf, buf_size);
    }
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_fill_n_zeros(char)" << endl;
    s_check_zero(buf, buf_size);

    s_set_non_zero(buf, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(buf, buf_size);
        NFast::Zero_memory(buf,buf_size);
    }
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Zero_memory(char)"
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0) ? "ok" : "wrong")
         << endl;
    s_check_zero(buf, buf_size);

    free(buf);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(TestFillZerosInt)
{
    cout << endl << "TestFillZerosInt" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    int* buf = (int*)malloc(buf_size * sizeof(int));
    Uint8 start, finish;

#ifdef NCBI_HAVE_FAST_OPS
    s_set_non_zero(buf, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(buf, buf_size);
        NFast::fill_n_zeros_aligned16(buf,buf_size);
    }
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::fill_n_zeros_aligned16(int)" << endl;
    s_check_zero(buf, buf_size);

    s_set_non_zero(buf, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(buf, buf_size);
        NFast::fill_n_zeros(buf,buf_size);
    }
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::fill_n_zeros(int) " << endl;
    s_check_zero(buf, buf_size);
#endif

    s_set_non_zero(buf, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(buf, buf_size);
        NFast::x_no_ncbi_sse_fill_n_zeros(buf, buf_size);
    }
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_fill_n_zeros(int)" << endl;
    s_check_zero(buf, buf_size);

    s_set_non_zero(buf, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(buf, buf_size);
        NFast::Zero_memory(buf,buf_size);
    }
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Zero_memory(int)"
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0) ? "ok" : "wrong")
         << endl;
    s_check_zero(buf, buf_size);

    free(buf);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(TestCopyInt)
{
    cout << endl << "TestCopyInt" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    int* buf = (int*)malloc(buf_size * sizeof(int));
    int* dst = (int*)malloc(buf_size * sizeof(int));
    Uint8 start, finish;
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = i & 0xFF;
    }

#ifdef NCBI_HAVE_FAST_OPS
    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::copy_n_aligned16(buf, buf_size, dst);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::copy_n_aligned16(int) "  << endl;
    s_check_equal(buf, buf_size, dst);
#endif

    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::x_no_ncbi_sse_copy_mem(dst,buf,buf_size);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_copy_mem(int)" << endl;
    s_check_equal(buf, buf_size, dst);

    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::Copy_memory(buf, buf_size, dst);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Copy_memory(int)"
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0 && intptr_t(dst)%16 == 0) ? "ok" : "wrong")
         << endl;
    s_check_equal(buf, buf_size, dst);

    free(buf);
    free(dst);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(TestConvertCharInt)
{
    cout << endl << "TestConvertCharInt" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    char* buf = (char*)malloc(buf_size * sizeof(char));
    int*  dst = (int* )malloc(buf_size * sizeof(int));
    Uint8 start, finish;
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = i & 0xFF;
    }

#ifdef NCBI_HAVE_FAST_OPS
    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::copy_n_bytes_aligned16(buf, buf_size, dst);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::copy_n_bytes_aligned16(char -> int) "  << endl;
    s_check_equal(buf, buf_size, dst);
#endif

    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::x_no_ncbi_sse_convert_memory(dst, buf, buf_size);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_convert_memory(char -> int)" << endl;
    s_check_equal(buf, buf_size, dst);

    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::Convert_memory(buf, buf_size, dst);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Convert_memory(char -> int)"
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0 && intptr_t(dst)%16 == 0) ? "ok" : "wrong")
         << endl;
    s_check_equal(buf, buf_size, dst);

    free(buf);
    free(dst);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(TestConvertIntChar)
{
    cout << endl << "TestConvertIntChar" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    int*  buf = (int* )malloc(buf_size * sizeof(int));
    char* dst = (char*)malloc(buf_size * sizeof(char));
    Uint8 start, finish;
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = i & 0xFF;
    }

#ifdef NCBI_HAVE_FAST_OPS
    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::copy_n_aligned16(buf, buf_size, dst);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::copy_n_aligned16(int -> char) "  << endl;
    s_check_equal(buf, buf_size, dst);
#endif

    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::x_no_ncbi_sse_convert_memory(dst, buf, buf_size);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_convert_memory(int -> char)" << endl;
    s_check_equal(buf, buf_size, dst);

    s_set_non_zero(dst, buf_size);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        s_payload(dst, buf_size);
        NFast::Convert_memory(buf, buf_size, dst);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Convert_memory(int -> char)"
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0 && intptr_t(dst)%16 == 0) ? "ok" : "wrong")
         << endl;
    s_check_equal(buf, buf_size, dst);

    free(buf);
    free(dst);
}
#endif

template<class V>
void s_clear_split_dst(size_t buf_size, V* dst0, V* dst1, V* dst2, V* dst3)
{
    s_set_zero(dst0, buf_size/4);
    s_set_zero(dst1, buf_size/4);
    s_set_zero(dst2, buf_size/4);
    s_set_zero(dst3, buf_size/4);
}
template<class V>
void s_set_split_buf(V* buf, size_t buf_size)
{
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = V(i & 0xFF);
    }
}
template<class V>
void s_check_split_dst(size_t buf_size, const V* dst0, const V* dst1, const V* dst2, const V* dst3)
{
    for ( size_t i = 0; i < buf_size/4; ++i ) {
        _ASSERT(dst0[i] == V((i*4+0) & 0xff));
        _ASSERT(dst1[i] == V((i*4+1) & 0xff));
        _ASSERT(dst2[i] == V((i*4+2) & 0xff));
        _ASSERT(dst3[i] == V((i*4+3) & 0xff));
    }
}

#if 1
BOOST_AUTO_TEST_CASE(TestSplitIntInt)
{
    cout << endl << "TestSplitIntInt" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    int*  buf = (int* )malloc(buf_size * sizeof(int));
    int*  dst0 = (int*)malloc((buf_size/4) * sizeof(int));
    int*  dst1 = (int*)malloc((buf_size/4) * sizeof(int));
    int*  dst2 = (int*)malloc((buf_size/4) * sizeof(int));
    int*  dst3 = (int*)malloc((buf_size/4) * sizeof(int));
    Uint8 start, finish;

    s_set_split_buf(buf, buf_size);

#ifdef NCBI_HAVE_FAST_OPS
    s_clear_split_dst(buf_size, dst0, dst1, dst2, dst3);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::copy_4n_split_aligned16(buf, buf_size/4, dst0, dst1, dst2, dst3);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::copy_4n_split_aligned16(int -> int)"  << endl;
    s_check_split_dst(buf_size, dst0, dst1, dst2, dst3);
#endif

    s_clear_split_dst(buf_size, dst0, dst1, dst2, dst3);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::x_no_ncbi_sse_split_into4(buf, buf_size/4, dst0, dst1, dst2, dst3);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_split_into4(int -> int)" << endl;
    s_check_split_dst(buf_size, dst0, dst1, dst2, dst3);

    s_clear_split_dst(buf_size, dst0, dst1, dst2, dst3);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::Split_into4(buf, buf_size/4, dst0, dst1, dst2, dst3);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Split_into4(int -> int)"
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0 &&
            intptr_t(dst0)%16 == 0 && intptr_t(dst1)%16 == 0 && intptr_t(dst2)%16 == 0 && intptr_t(dst3)%16 == 0) ? "ok" : "wrong")
         << endl;
    s_check_split_dst(buf_size, dst0, dst1, dst2, dst3);

    free(buf);
    free(dst0);
    free(dst1);
    free(dst2);
    free(dst3);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(TestSplitIntChar)
{
    cout << endl << "TestSplitIntChar" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    int*  buf  = (int* )malloc(buf_size * sizeof(int));
    char* dst0 = (char*)malloc((buf_size/4) * sizeof(char));
    char* dst1 = (char*)malloc((buf_size/4) * sizeof(char));
    char* dst2 = (char*)malloc((buf_size/4) * sizeof(char));
    char* dst3 = (char*)malloc((buf_size/4) * sizeof(char));
    Uint8 start, finish;

    s_set_split_buf(buf, buf_size);

#ifdef NCBI_HAVE_FAST_OPS
    s_clear_split_dst(buf_size, dst0, dst1, dst2, dst3);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::copy_4n_split_aligned16(buf, buf_size/4, dst0, dst1, dst2, dst3);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::copy_4n_split_aligned16(int -> char)"  << endl;
    s_check_split_dst(buf_size, dst0, dst1, dst2, dst3);
#endif

    s_clear_split_dst(buf_size, dst0, dst1, dst2, dst3);
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::x_no_ncbi_sse_split_into4(buf, buf_size/4, dst0, dst1, dst2, dst3);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_split_into4(int -> char)" << endl;
    s_check_split_dst(buf_size, dst0, dst1, dst2, dst3);

    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::Split_into4(buf, buf_size/4, dst0, dst1, dst2, dst3);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Split_into4(int -> char)"
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0 &&
            intptr_t(dst0)%16 == 0 && intptr_t(dst1)%16 == 0 && intptr_t(dst2)%16 == 0 && intptr_t(dst3)%16 == 0) ? "ok" : "wrong")
         << endl;
    s_check_split_dst(buf_size, dst0, dst1, dst2, dst3);

    free(buf);
    free(dst0);
    free(dst1);
    free(dst2);
    free(dst3);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(TestMaxElement1)
{
    cout << endl << "TestMaxElement1" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    unsigned int*  buf  = (unsigned int*)malloc(buf_size * sizeof(unsigned int));
    unsigned int result = 0;
    Uint8 start, finish;
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = (i*256/SSETEST_BUFSIZE) & 0xFF;
    }

#ifdef NCBI_HAVE_FAST_OPS
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        result = NFast::max_element_n_aligned16(buf, buf_size);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::max_element_n_aligned16 " << result  << endl;
#endif

    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        result = NFast::x_no_ncbi_sse_max_element(buf, buf_size, 0);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_max_element " << result << endl;

    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        result = NFast::Max_element(buf, buf_size);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Max_element1 " << result
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0) ? "ok" : "wrong")
         << endl;

    free(buf);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(TestMaxElement2)
{
    cout << endl << "TestMaxElement2" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    unsigned int*  buf  = (unsigned int*)malloc(buf_size * sizeof(unsigned int));
    unsigned int result = 0;
    Uint8 start, finish;
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = (i*256/SSETEST_BUFSIZE) & 0xFF;
    }

#ifdef NCBI_HAVE_FAST_OPS
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::max_element_n_aligned16(buf, buf_size, result);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::max_element_n_aligned16 " << result  << endl;
#endif
    _ASSERT(result == 0xff);

    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        result = NFast::x_no_ncbi_sse_max_element(buf, buf_size, result);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_max_element " << result << endl;
    _ASSERT(result == 0xff);

    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::Max_element(buf, buf_size, result);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Max_element2 " << result
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0) ? "ok" : "wrong")
         << endl;
    _ASSERT(result == 0xff);

    free(buf);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(TestMaxElement3)
{
    cout << endl << "TestMaxElement3" << endl;
    const size_t buf_size = SSETEST_BUFSIZE;
    const size_t test_count = SSETEST_COUNT;
    unsigned int*  buf  = (unsigned int*)malloc(buf_size * sizeof(unsigned int));
    Uint8 start, finish;
    for (size_t i = 0; i < buf_size; ++i) {
        buf[i] = (i*64/SSETEST_BUFSIZE*4+i%4) & 0xFF;
    }

#ifdef NCBI_HAVE_FAST_OPS
    {
    unsigned int result[4]= {0};
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::max_4elements_n_aligned16(buf, buf_size/4, result);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::max_element_n_aligned16 " << result[0] <<' '<< result[1] <<' '<< result[2] <<' '<< result[3]  << endl;
    _ASSERT(result[0] == 0xfc);
    _ASSERT(result[1] == 0xfd);
    _ASSERT(result[2] == 0xfe);
    _ASSERT(result[3] == 0xff);
    }
#endif

    {
    unsigned int result[4]= {0};
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::x_no_ncbi_sse_max_4element(buf, buf_size/4, result);
    }
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::x_no_ncbi_sse_max_4element " << result[0] <<' '<< result[1] <<' '<< result[2] <<' '<< result[3] << endl;
    _ASSERT(result[0] == 0xfc);
    _ASSERT(result[1] == 0xfd);
    _ASSERT(result[2] == 0xfe);
    _ASSERT(result[3] == 0xff);
    }

    {
    unsigned int result[4]= {0};
    start = QUERY_PERF_COUNTER();
    for (size_t i = 0; i < test_count; ++i) {
        NFast::Max_4element(buf, buf_size/4, result);
    } 
    finish = QUERY_PERF_COUNTER();
    cout << (finish - start) << " - NFast::Max_4element " << result[0] <<' '<< result[1] <<' '<< result[2] <<' '<< result[3]
         << "  aligned " << ((buf_size%16 == 0 && intptr_t(buf)%16 == 0) ? "ok" : "wrong")
         << endl;
    _ASSERT(result[0] == 0xfc);
    _ASSERT(result[1] == 0xfd);
    _ASSERT(result[2] == 0xfe);
    _ASSERT(result[3] == 0xff);
    }

    free(buf);
}
#endif
