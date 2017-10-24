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
#include <vector>
#include <memory.h>

#if (defined(NCBI_COMPILER_MSVC) || NCBI_SSE > 40)
#   include <immintrin.h>
#   define NCBI_HAVE_FAST_OPS
#endif //NCBI_SSE

BEGIN_NCBI_SCOPE
BEGIN_NAMESPACE(NFast);

//---------------------------------------------------------------------------
//  Methods are specifically optimized for cases when
//  count is multiple of 16
//  and source and destination buffer are aligned to 16 bytes

/// Fill destination memory buffer with zeros
///
/// @param dest
///   destination memory buffer
/// @param count
///   number of elements in the destination buffer

void Zero_memory(        char* dest, size_t count);
void Zero_memory(         int* dest, size_t count);
void Zero_memory(unsigned int* dest, size_t count);


/// Copy memory buffer
///
/// @param src
///   Source memory buffer
/// @param count
///   Number of elements to copy
/// @param dest
///   Destination memory buffer

void Copy_memory(         const int* src, size_t count,          int* dest);
void Copy_memory(const unsigned int* src, size_t count,          int* dest);
void Copy_memory(const unsigned int* src, size_t count, unsigned int* dest);


/// Convert memory buffer elements from one type to another
///
/// @param src
///   Source memory buffer
/// @param count
///   Number of elements to convert
/// @param dest
///   Destination memory buffer

void Convert_memory(const         char* src, size_t count,  int* dest);
void Convert_memory(const          int* src, size_t count, char* dest);
void Convert_memory(const unsigned int* src, size_t count, char* dest);


/// Split source memory buffer into 4 buffers
/// Source buffer contains 4*count elements
/// Each destination buffer size equals to count
/// as a result
///   dest0 will have elements src[0], src[4], src[8] etc
///   dest1 will have elements src[1], src[5], src[9] etc
///   dest2 will have elements src[2], src[6], src[10] etc
///   dest3 will have elements src[3], src[7], src[11] etc

void Split_into4(const int* src, size_t count, int*  dest0, int*  dest1, int*  dest2, int*  dest3);
void Split_into4(const int* src, size_t count, char* dest0, char* dest1, char* dest2, char* dest3);
void Split_into4(const unsigned int* src, size_t count,
                 int*  dest0, int*  dest1, int*  dest2, int*  dest3);
void Split_into4(const unsigned int* src, size_t count,
                 unsigned int*  dest0, unsigned int*  dest1, unsigned int*  dest2, unsigned int*  dest3);
void Split_into4(const unsigned int* src, size_t count,
                 char* dest0, char* dest1, char* dest2, char* dest3);


/// Find maximum value in an array
///
/// @param src
///   Source memory buffer
/// @param count
///   Number of elements
unsigned int Max_element(const unsigned int* src, size_t count);

/// Find maximum value in an array, or dest
///
/// @param src
///   Source memory buffer
/// @param count
///   Number of elements
/// @param dest
///   Number of elements
/// @return 
///   dest
void Max_element(const unsigned int* src, size_t count, unsigned int& dest);

/// Find maximum values in 4 arrays, or dest
/// Source buffer contains 4*count elements with the following layout:
/// a0[0], a1[0], a2[0], a3[0], a0[1], a1[1], a2[1], a3[1], a0[2], a1[2], a2[2], a3[2], etc
/// result:
///   dest[0] = max(dest[0], max element in a0)
///   dest[1] = max(dest[1], max element in a1)
///   dest[2] = max(dest[2], max element in a2)
///   dest[3] = max(dest[3], max element in a3)
///
/// @param src
///   Source memory buffer
/// @param count
///   Number of elements
/// @param dest
///   Number of elements
/// @return 
///   dest
void Max_4element(const unsigned int* src, size_t count, unsigned int dest[4]);


/// Append count unitialized elements to dest vector
/// return pointer to appended elements for proper initialization
/// vector must have enough memory reserved
template<class V, class A>
V* append_uninitialized(vector<V, A>& dest, size_t count);

/// Append count zeros to dest vector
/// vector must have enough memory reserved
template<class V, class A>
void append_zeros(vector<V, A>& dest, size_t count);

/// Append count zeros to dest vector
/// vector must have enough memory reserved
/// dst.end() pointer and count must be aligned by 16
template<class V, class A>
void append_zeros_aligned16(vector<V, A>& dest, size_t count);



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Implementations
// Please do not use them directly
// Instead, use wrappers defined above
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


#if defined(NCBI_HAVE_FAST_OPS)
//---------------------------------------------------------------------------
// Fill destination buffer with zeros
// dst and count must be aligned by 16
void NCBI_XNCBI_EXPORT fill_n_zeros_aligned16(char* dst, size_t count);
void NCBI_XNCBI_EXPORT fill_n_zeros_aligned16(int*  dst, size_t count);

void NCBI_XNCBI_EXPORT fill_n_zeros(char* dst, size_t count);
void NCBI_XNCBI_EXPORT fill_n_zeros(int*  dst, size_t count);

inline
void fill_n_zeros_aligned16(unsigned int* dst, size_t count) {
    fill_n_zeros_aligned16(reinterpret_cast<int*>(dst), count);
}

//---------------------------------------------------------------------------
// copy count ints
// src, dst, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_n_aligned16(const int* src, size_t count, int* dst);

inline
void copy_n_aligned16(const unsigned int* src, size_t count, int* dst) {
    copy_n_aligned16(reinterpret_cast<const int*>(src), count, dst);
}
inline
void copy_n_aligned16(const unsigned int* src, size_t count, unsigned int* dst) {
    copy_n_aligned16(reinterpret_cast<const int*>(src), count, reinterpret_cast<int*>(dst));
}

//---------------------------------------------------------------------------
// convert count unsigned chars to ints
// src, dst, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_n_bytes_aligned16(const char* src, size_t count, int* dst);

// convert count ints to chars
// src, dst, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_n_aligned16(const int* src, size_t count, char* dst);

inline
void copy_n_aligned16(const unsigned int* src, size_t count, char* dst) {
    copy_n_aligned16(reinterpret_cast<const int*>(src), count, dst);
}

//---------------------------------------------------------------------------
// copy count*4 ints splitting them into 4 arrays
// src, dsts, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_4n_split_aligned16(const int* src, size_t count,
                                                 int* dst0, int* dst1, int* dst2, int* dst3);
// convert count*4 ints into chars splitting them into 4 arrays
// src, dsts, and count must be aligned by 16
void NCBI_XNCBI_EXPORT copy_4n_split_aligned16(const int* src, size_t count,
                                               char* dst0, char* dst1, char* dst2, char* dst3);
inline
void copy_4n_split_aligned16(const unsigned int* src, size_t count,
                             int* dst0, int* dst1, int* dst2, int* dst3) {
    copy_4n_split_aligned16(reinterpret_cast<const int*>(src), count, dst0, dst1, dst2, dst3);
}
inline
void copy_4n_split_aligned16(const unsigned int* src, size_t count,
                             unsigned int* dst0, unsigned int* dst1, unsigned int* dst2, unsigned int* dst3) {
    copy_4n_split_aligned16(reinterpret_cast<const int*>(src), count,
                            reinterpret_cast<int*>(dst0),
                            reinterpret_cast<int*>(dst1),
                            reinterpret_cast<int*>(dst2),
                            reinterpret_cast<int*>(dst3));
}
inline
void copy_4n_split_aligned16(const unsigned int* src, size_t count,
                             char* dst0, char* dst1, char* dst2, char* dst3) {
    copy_4n_split_aligned16(reinterpret_cast<const int*>(src), count, dst0, dst1, dst2, dst3);
}

//---------------------------------------------------------------------------
// return max element from unsigned array
// src and count must be aligned by 16
unsigned int NCBI_XNCBI_EXPORT max_element_n_aligned16(const unsigned int* src, size_t count);
void NCBI_XNCBI_EXPORT max_element_n_aligned16(const unsigned int* src, size_t count, unsigned int& dst);
void NCBI_XNCBI_EXPORT max_4elements_n_aligned16(const unsigned int* src, size_t count, unsigned int dst[4]);

#endif //NCBI_HAVE_FAST_OPS

#if defined(NCBI_COMPILER_GCC) && defined(NCBI_HAVE_FAST_OPS)
// fast implementation using internal layout of vector
template<class V, class A>
inline
V* append_uninitialized(vector<V, A>& dst, size_t count)
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
inline
void append_zeros(vector<V, A>& dst, size_t count) {
    fill_n_zeros(append_uninitialized(dst, count), count);
}
template<class V, class A>
inline
void append_zeros_aligned16(vector<V, A>& dst, size_t count) {
    fill_n_zeros_aligned16(append_uninitialized(dst, count), count);
}
#else
// default implementation
template<class V, class A>
inline
V* append_uninitialized(vector<V, A>& dst, size_t count)
{
    size_t size = dst.size();
    dst.resize(size+count);
    return dst.data()+size;
}
template<class V, class A>
inline
void append_zeros(vector<V, A>& dst, size_t count) {
    dst.resize(dst.size()+count);
}
template<class V, class A>
inline
void append_zeros_aligned16(vector<V, A>& dst, size_t count) {
    dst.resize(dst.size()+count);
}
#endif

//---------------------------------------------------------------------------
inline
void x_no_ncbi_sse_fill_n_zeros(char* dst, size_t count) {
    memset(dst, 0, count * sizeof(*dst));
}
inline
void x_no_ncbi_sse_fill_n_zeros(int* dst, size_t count) {
    memset(dst, 0, count * sizeof(*dst));
}
inline
void x_no_ncbi_sse_copy_mem(int* dest, const int* src, size_t count) {
    memcpy(dest, src, count * sizeof(*dest));
}
inline 
void x_no_ncbi_sse_convert_memory( int* dest, const char* src, size_t count) {
    for (size_t e = 0; e < count; ++e) {
        *(dest + e) = (unsigned char)*(src + e);
    } 
}
inline
void x_no_ncbi_sse_convert_memory(char* dest, const int*  src, size_t count) {
    for (size_t e = 0; e < count; ++e) {
        *(dest + e) = *(src + e);
    } 
}
inline
void x_no_ncbi_sse_split_into4(const int* src, size_t count, int*  dest0, int*  dest1, int*  dest2, int*  dest3) {
    size_t d=0;
    int* dest[] = {dest0, dest1, dest2, dest3};
    for (size_t e = 0; e < 4*count; ++e, d%=4) {
        *(dest[d++]++) = *(src + e);
    }
}
inline
void x_no_ncbi_sse_split_into4(const int* src, size_t count, char* dest0, char* dest1, char* dest2, char* dest3) {
    size_t d=0;
    char* dest[] = {dest0, dest1, dest2, dest3};
    for (size_t e = 0; e < 4*count; ++e, d%=4) {
        *(dest[d++]++) = *(src + e);
    }
}
inline
unsigned int x_no_ncbi_sse_max_element(const unsigned int* src, size_t count, unsigned int v) {
    unsigned int result = v;
    for (size_t e = 0; e < count; ++e) {
        if (result < *(src + e)) {
            result = *(src + e);
        }
    } 
    return result;
}
inline
void x_no_ncbi_sse_max_4element(const unsigned int* src, size_t count, unsigned int dest[4]) {
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

//---------------------------------------------------------------------------
inline
void Zero_memory(char* dest, size_t count) {
#if !defined(NCBI_COMPILER_MSVC) && defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(dest)%16 == 0) {
        fill_n_zeros_aligned16(dest,count);
    } else {
        x_no_ncbi_sse_fill_n_zeros(dest, count);
    }
#else
    x_no_ncbi_sse_fill_n_zeros(dest, count);
#endif
}
inline
void Zero_memory(int* dest, size_t count) {
#if !defined(NCBI_COMPILER_MSVC) && defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(dest)%16 == 0) {
        fill_n_zeros_aligned16(dest,count);
    } else {
        x_no_ncbi_sse_fill_n_zeros(dest, count);
    }
#else
    x_no_ncbi_sse_fill_n_zeros(dest, count);
#endif

}
inline
void Zero_memory(unsigned int* dest, size_t count) {
    Zero_memory(reinterpret_cast<int*>(dest), count);
}

inline
void Copy_memory(const int* src, size_t count, int* dest) {
#if !defined(NCBI_COMPILER_MSVC) && defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(dest)%16 == 0 && intptr_t(src)%16 == 0) {
        copy_n_aligned16(src, count, dest);
    } else {
        x_no_ncbi_sse_copy_mem(dest, src, count);
    }
#else
    x_no_ncbi_sse_copy_mem(dest, src, count);
#endif
}
inline
void Copy_memory(const unsigned int* src, size_t count, int* dest) {
    Copy_memory(reinterpret_cast<const int*>(src), count, dest);
}
inline
void Copy_memory(const unsigned int* src, size_t count, unsigned int* dest) {
    Copy_memory(reinterpret_cast<const int*>(src), count, reinterpret_cast<int*>(dest));
}

inline
void Convert_memory(const char* src, size_t count, int* dest) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(dest)%16 == 0 && intptr_t(src)%16 == 0) {
        copy_n_bytes_aligned16(src, count, dest);
    } else {
        x_no_ncbi_sse_convert_memory(dest, src, count);
    }
#else
    x_no_ncbi_sse_convert_memory(dest, src, count);
#endif
}
inline
void Convert_memory(const int* src, size_t count, char* dest) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(dest)%16 == 0 && intptr_t(src)%16 == 0) {
        copy_n_aligned16(src, count, dest);
    } else {
        x_no_ncbi_sse_convert_memory(dest, src, count);
    }
#else
    x_no_ncbi_sse_convert_memory(dest, src, count);
#endif
}
inline
void Convert_memory(const unsigned int* src, size_t count, char* dest) {
    Convert_memory(reinterpret_cast<const int*>(src), count, dest); 
}

inline
void Split_into4(const int* src, size_t count, int*  dest0, int*  dest1, int*  dest2, int*  dest3) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(src)%16 == 0 &&
        intptr_t(dest0)%16 == 0 && intptr_t(dest1)%16 == 0 && intptr_t(dest2)%16 == 0 && intptr_t(dest3)%16 == 0) {
        copy_4n_split_aligned16(src, count, dest0, dest1, dest2, dest3);
    } else {
        x_no_ncbi_sse_split_into4(src, count, dest0, dest1, dest2, dest3);
    }
#else
    x_no_ncbi_sse_split_into4(src, count, dest0, dest1, dest2, dest3);
#endif
}
inline
void Split_into4(const int* src, size_t count, char* dest0, char* dest1, char* dest2, char* dest3) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(src)%16 == 0 &&
        intptr_t(dest0)%16 == 0 && intptr_t(dest1)%16 == 0 && intptr_t(dest2)%16 == 0 && intptr_t(dest3)%16 == 0) {
        copy_4n_split_aligned16(src, count, dest0, dest1, dest2, dest3);
    } else {
        x_no_ncbi_sse_split_into4(src, count, dest0, dest1, dest2, dest3);
    }
#else
    x_no_ncbi_sse_split_into4(src, count, dest0, dest1, dest2, dest3);
#endif
}
inline
void Split_into4(const unsigned int* src, size_t count,
                 int*  dest0, int*  dest1, int*  dest2, int*  dest3) {
    Split_into4(reinterpret_cast<const int*>(src), count, dest0, dest1, dest2, dest3);
}
inline
void Split_into4(const unsigned int* src, size_t count,
                 unsigned int*  dest0, unsigned int*  dest1, unsigned int*  dest2, unsigned int*  dest3) {
    Split_into4(reinterpret_cast<const int*>(src), count,
                reinterpret_cast<int*>(dest0), reinterpret_cast<int*>(dest1),
                reinterpret_cast<int*>(dest2), reinterpret_cast<int*>(dest3));
}
inline
void Split_into4(const unsigned int* src, size_t count,
                 char* dest0, char* dest1, char* dest2, char* dest3) {
    Split_into4(reinterpret_cast<const int*>(src), count, dest0, dest1, dest2, dest3);
}

inline
unsigned int Max_element(const unsigned int* src, size_t count) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(src)%16 == 0) {
        return max_element_n_aligned16(src,count);
    } else {
        return x_no_ncbi_sse_max_element(src, count, 0);
    }
#else
    return x_no_ncbi_sse_max_element(src, count, 0);
#endif
}
inline
void Max_element(const unsigned int* src, size_t count, unsigned int& dest) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(src)%16 == 0) {
        max_element_n_aligned16(src, count, dest);
    } else {
        dest = x_no_ncbi_sse_max_element(src, count, dest);
    }
#else
    dest = x_no_ncbi_sse_max_element(src, count, dest);
#endif
}
inline
void Max_4element(const unsigned int* src, size_t count, unsigned int dest[4]) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && intptr_t(src)%16 == 0) {
        max_4elements_n_aligned16(src, count, dest);
    } else {
        x_no_ncbi_sse_max_4element(src, count, dest);
    }
#else
    x_no_ncbi_sse_max_4element(src, count, dest);
#endif
}

END_NAMESPACE(NFast);
END_NCBI_SCOPE

#endif // CORELIB___NCBI_FAST__HPP
