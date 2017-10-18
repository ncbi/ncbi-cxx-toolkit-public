#ifndef CORELIB___NCBI_FAST__HPP
#define CORELIB___NCBI_FAST__HPP
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
#include <ncbiconf.h>
#include <corelib/ncbistl.hpp>
#include <immintrin.h>

#if defined(NCBI_COMPILER_MSVC) || NCBI_SSE > 40

BEGIN_NCBI_SCOPE
BEGIN_NAMESPACE(NFast);

// fastest fill of count ints at dst with zeroes
// dst and count must be aligned by 16
void NCBI_XNCBI_EXPORT fill_n_zeros_aligned16(int* dst, size_t count);
// fastest fill of count chars at dst with zeroes
// dst and count must be aligned by 16
void NCBI_XNCBI_EXPORT fill_n_zeros_aligned16(char* dst, size_t count);

inline void fill_n_zeros_aligned16(unsigned* dst, size_t count)
{
    fill_n_zeros_aligned16(reinterpret_cast<int*>(dst), count);
}

// fastest fill of count ints at dst with zeroes
void NCBI_XNCBI_EXPORT fill_n_zeros(int* dst, size_t count);
// fastest fill of count chars at dst with zeroes
void NCBI_XNCBI_EXPORT fill_n_zeros(char* dst, size_t count);

// convert count unsigned chars to ints
// src, dst, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_n_bytes_aligned16(const char* src, size_t count, int* dst);

// convert count ints to chars
// src, dst, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_n_aligned16(const int* src, size_t count, char* dst);

// copy count ints
// src, dst, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_n_aligned16(const int* src, size_t count, int* dst);

inline void copy_n_aligned16(const unsigned* src, size_t count, char* dst)
{
    copy_n_aligned16(reinterpret_cast<const int*>(src), count, dst);
}

inline void copy_n_aligned16(const unsigned* src, size_t count, int* dst)
{
    copy_n_aligned16(reinterpret_cast<const int*>(src), count, dst);
}

inline void copy_n_aligned16(const unsigned* src, size_t count, unsigned* dst)
{
    copy_n_aligned16(reinterpret_cast<const int*>(src), count, reinterpret_cast<int*>(dst));
}

// copy count*4 ints splitting them into 4 arrays
// src, dsts, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_4n_split_aligned16(const int* src, size_t count,
                                                 int* dst0, int* dst1, int* dst2, int* dst3);

// convert count*4 ints int chars splitting them into 4 arrays
// src, dsts, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_4n_split_aligned16(const int* src, size_t count,
                                                 char* dst0, char* dst1, char* dst2, char* dst3);

inline void copy_4n_split_aligned16(const unsigned* src, size_t count,
                                    char* dst0, char* dst1, char* dst2, char* dst3)
{
    copy_4n_split_aligned16(reinterpret_cast<const int*>(src), count, dst0, dst1, dst2, dst3);
}

inline void copy_4n_split_aligned16(const unsigned* src, size_t count,
                                    int* dst0, int* dst1, int* dst2, int* dst3)
{
    copy_4n_split_aligned16(reinterpret_cast<const int*>(src), count, dst0, dst1, dst2, dst3);
}

inline void copy_4n_split_aligned16(const unsigned* src, size_t count,
                                    unsigned* dst0, unsigned* dst1, unsigned* dst2, unsigned* dst3)
{
    copy_4n_split_aligned16(reinterpret_cast<const int*>(src), count,
                            reinterpret_cast<int*>(dst0),
                            reinterpret_cast<int*>(dst1),
                            reinterpret_cast<int*>(dst2),
                            reinterpret_cast<int*>(dst3));
}

// append count unitialized elements to dst vector
// return pointer to appended elements for proper initialization
// vector must have enough memory reserved
template<class V, class A>
V* append_uninitialized(vector<V, A>& dst, size_t count);

// append count zeros to dst vector
// vector must have enough memory reserved
template<class V, class A>
inline void append_zeros(vector<V, A>& dst, size_t count);

// append count zeros to dst vector
// vector must have enough memory reserved
// dst.end() pointer and count must be aligned by 16
template<class V, class A>
inline void append_zeros_aligned16(vector<V, A>& dst, size_t count);

#ifdef NCBI_COMPILER_GCC
// fast implementation using internal layout of vector
template<class V, class A>
inline V* append_uninitialized(vector<V, A>& dst, size_t count)
{
    if ( sizeof(dst) == 3*sizeof(V*) ) {
        V*& end_ptr = ((V**)&dst)[1];
        V* ret = end_ptr;
        end_ptr = ret + count;
        return ret;
    }
    else {
        // wrong vector size and probably layout
        size_t size = dst.size();
        dst.resize(size+count);
        return dst.data()+size;
    }
}
template<class V, class A>
inline void append_zeros(vector<V, A>& dst, size_t count)
{
    fill_n_zeros(append_uninitialized(dst, count), count);
}
template<class V, class A>
inline void append_zeros_aligned16(vector<V, A>& dst, size_t count)
{
    fill_n_zeros_aligned16(append_uninitialized(dst, count), count);
}
#else
// default implementation
template<class V, class A>
inline V* append_uninitialized(vector<V, A>& dst, size_t count)
{
    size_t size = dst.size();
    dst.resize(size+count);
    return dst.data()+size;
}
template<class V, class A>
inline void append_zeros(vector<V, A>& dst, size_t count)
{
    dst.resize(dst.size()+count);
}
template<class V, class A>
inline void append_zeros_aligned16(vector<V, A>& dst, size_t count)
{
    dst.resize(dst.size()+count);
}
#endif

// return max element from unsigned array
// src and count must be aligned by 16
unsigned NCBI_XNCBI_EXPORT max_element_n_aligned16(const unsigned* src, size_t count);

void NCBI_XNCBI_EXPORT max_element_n_aligned16(const unsigned* src, size_t count, unsigned& dst);
void NCBI_XNCBI_EXPORT max_4elements_n_aligned16(const unsigned* src, size_t count, unsigned dst[4]);

END_NAMESPACE(NFast);
END_NCBI_SCOPE
#endif //NCBI_SSE

#endif // CORELIB___NCBI_FAST__HPP
