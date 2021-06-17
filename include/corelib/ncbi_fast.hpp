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
#include <corelib/ncbidbg.hpp>
#include <memory.h>

#if NCBI_SSE > 40
#   include <immintrin.h>
#   define NCBI_HAVE_FAST_OPS
#endif //NCBI_SSE

BEGIN_NCBI_SCOPE

//---------------------------------------------------------------------------
//  Methods are specifically optimized for cases when
//  count is multiple of 16
//  and source and destination buffer are aligned to 16 bytes


class NCBI_XNCBI_EXPORT NFast {
public:

    /// Fill destination memory buffer with zeros
    ///
    /// @param dest
    ///   destination memory buffer
    /// @param count
    ///   number of elements in the destination buffer

    static void ClearBuffer(        char* dest, size_t count);
    static void ClearBuffer(         int* dest, size_t count);
    static void ClearBuffer(unsigned int* dest, size_t count);


    /// Copy memory buffer (only when source and destination do not overlap!).
    /// Use MoveBuffer() for overlapping buffers.
    /// @attention  If src and dest buffers overlap - undefined behavior
    ///
    /// @param src
    ///   Source memory buffer
    /// @param count
    ///   Number of elements to copy
    /// @param dest
    ///   Destination memory buffer

    static void CopyBuffer(         const int* src, size_t count,          int* dest);
    static void CopyBuffer(const unsigned int* src, size_t count,          int* dest);
    static void CopyBuffer(const unsigned int* src, size_t count, unsigned int* dest);


    /// Copy memory buffer when source and destination overlap
    ///
    /// @param src
    ///   Source memory buffer
    /// @param count
    ///   Number of elements to copy
    /// @param dest
    ///   Destination memory buffer

    static void MoveBuffer(         const int* src, size_t count,          int* dest);
    static void MoveBuffer(const unsigned int* src, size_t count,          int* dest);
    static void MoveBuffer(const unsigned int* src, size_t count, unsigned int* dest);


    /// Convert memory buffer elements from one type to another
    ///
    /// @param src
    ///   Source memory buffer
    /// @param count
    ///   Number of elements to convert
    /// @param dest
    ///   Destination memory buffer

    static void ConvertBuffer(const         char* src, size_t count,  int* dest);
    static void ConvertBuffer(const          int* src, size_t count, char* dest);
    static void ConvertBuffer(const unsigned int* src, size_t count, char* dest);


    /// Split source memory buffer into 4 buffers
    /// Source buffer contains 4*count elements
    /// Each destination buffer size equals to count
    /// as a result
    ///   dest0 will have elements src[0], src[4], src[8] etc
    ///   dest1 will have elements src[1], src[5], src[9] etc
    ///   dest2 will have elements src[2], src[6], src[10] etc
    ///   dest3 will have elements src[3], src[7], src[11] etc

    static void SplitBufferInto4(const int* src, size_t count, int*  dest0, int*  dest1, int*  dest2, int*  dest3);
    static void SplitBufferInto4(const int* src, size_t count, char* dest0, char* dest1, char* dest2, char* dest3);
    static void SplitBufferInto4(const unsigned int* src, size_t count,
                                 int*  dest0, int*  dest1, int*  dest2, int*  dest3);
    static void SplitBufferInto4(const unsigned int* src, size_t count,
                                 unsigned int*  dest0, unsigned int*  dest1, unsigned int*  dest2, unsigned int*  dest3);
    static void SplitBufferInto4(const unsigned int* src, size_t count,
                                 char* dest0, char* dest1, char* dest2, char* dest3);


    /// Find maximum value in an array
    ///
    /// @param src
    ///   Source memory buffer
    /// @param count
    ///   Number of elements
    static unsigned int FindMaxElement(const unsigned int* src, size_t count);

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
    static void FindMaxElement(const unsigned int* src, size_t count, unsigned int& dest);

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
    static void Find4MaxElements(const unsigned int* src, size_t count, unsigned int dest[4]);


    /// Append count unitialized elements to dest vector
    /// return pointer to appended elements for proper initialization
    /// vector must have enough memory reserved
    template<class V, class A>
    static V* AppendUninitialized(vector<V, A>& dest, size_t count);

    /// Append count zeros to dest vector
    /// vector must have enough memory reserved
    template<class V, class A>
    static void AppendZeros(vector<V, A>& dest, size_t count);

    /// Append count zeros to dest vector
    /// vector must have enough memory reserved
    /// dst.end() pointer and count must be aligned by 16
    template<class V, class A>
    static void AppendZerosAligned16(vector<V, A>& dest, size_t count);


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Implementations
protected:

#if defined(NCBI_HAVE_FAST_OPS)
//---------------------------------------------------------------------------
    // Fill destination buffer with zeros
    // dst and count must be aligned by 16
    static void x_sse_ClearBuffer(char* dst, size_t count);
    static void x_sse_ClearBuffer(int*  dst, size_t count);

    static void x_ClearBuffer(char* dst, size_t count);
    static void x_ClearBuffer(int*  dst, size_t count);

    inline
    static void x_sse_ClearBuffer(unsigned int* dst, size_t count) {
        x_sse_ClearBuffer(reinterpret_cast<int*>(dst), count);
    }

//---------------------------------------------------------------------------
    // copy count ints
    // src, dst, and count must be aligned by 16
    static void x_sse_CopyBuffer(const int* src, size_t count, int* dst);

    inline
    static void x_sse_CopyBuffer(const unsigned int* src, size_t count, int* dst) {
        x_sse_CopyBuffer(reinterpret_cast<const int*>(src), count, dst);
    }
    inline
    static void x_sse_CopyBuffer(const unsigned int* src, size_t count, unsigned int* dst) {
        x_sse_CopyBuffer(reinterpret_cast<const int*>(src), count, reinterpret_cast<int*>(dst));
    }

#if 0
    static void x_sse_MoveBuffer(const int* src, size_t count, int* dst);

    inline
    static void x_sse_MoveBuffer(const unsigned int* src, size_t count, int* dst) {
        x_sse_MoveBuffer(reinterpret_cast<const int*>(src), count, dst);
    }
    inline
    static void x_sse_MoveBuffer(const unsigned int* src, size_t count, unsigned int* dst) {
        x_sse_MoveBuffer(reinterpret_cast<const int*>(src), count, reinterpret_cast<int*>(dst));
    }
#endif

//---------------------------------------------------------------------------
    // convert count unsigned chars to ints
    // src, dst, and count must be aligned by 16
    static void x_sse_ConvertBuffer(const char* src, size_t count, int* dst);

    // convert count ints to chars
    // src, dst, and count must be aligned by 16
    static void x_sse_ConvertBuffer(const int* src, size_t count, char* dst);

    inline
    static void x_sse_ConvertBuffer(const unsigned int* src, size_t count, char* dst) {
        x_sse_ConvertBuffer(reinterpret_cast<const int*>(src), count, dst);
    }

//---------------------------------------------------------------------------
    // copy count*4 ints splitting them into 4 arrays
    // src, dsts, and count must be aligned by 16
    static void x_sse_SplitBufferInto4(const int* src, size_t count,
                                       int* dst0, int* dst1, int* dst2, int* dst3);
    // convert count*4 ints into chars splitting them into 4 arrays
    // src, dsts, and count must be aligned by 16
    static void x_sse_SplitBufferInto4(const int* src, size_t count,
                                       char* dst0, char* dst1, char* dst2, char* dst3);
    inline
    static void x_sse_SplitBufferInto4(const unsigned int* src, size_t count,
                                 int* dst0, int* dst1, int* dst2, int* dst3) {
        x_sse_SplitBufferInto4(reinterpret_cast<const int*>(src), count, dst0, dst1, dst2, dst3);
    }
    inline
    static void x_sse_SplitBufferInto4(const unsigned int* src, size_t count,
                                       unsigned int* dst0, unsigned int* dst1, unsigned int* dst2, unsigned int* dst3) {
        x_sse_SplitBufferInto4(reinterpret_cast<const int*>(src), count,
                                reinterpret_cast<int*>(dst0),
                                reinterpret_cast<int*>(dst1),
                                reinterpret_cast<int*>(dst2),
                                reinterpret_cast<int*>(dst3));
    }
    inline
    static void x_sse_SplitBufferInto4(const unsigned int* src, size_t count,
                                 char* dst0, char* dst1, char* dst2, char* dst3) {
        x_sse_SplitBufferInto4(reinterpret_cast<const int*>(src), count, dst0, dst1, dst2, dst3);
    }

//---------------------------------------------------------------------------
    // return max element from unsigned array
    // src and count must be aligned by 16
    static unsigned int x_sse_FindMaxElement(const unsigned int* src, size_t count);
    static void x_sse_FindMaxElement(const unsigned int* src, size_t count, unsigned int& dst);
    static void x_sse_Find4MaxElements(const unsigned int* src, size_t count, unsigned int dst[4]);

#endif //NCBI_HAVE_FAST_OPS

//---------------------------------------------------------------------------
    inline
    static void x_no_sse_ClearBuffer(char* dst, size_t count) {
        memset(dst, 0, count * sizeof(*dst));
    }
    inline
    static void x_no_sse_ClearBuffer(int* dst, size_t count) {
        memset(dst, 0, count * sizeof(*dst));
    }
    inline
    static void x_no_sse_CopyBuffer(int* dest, const int* src, size_t count) {
        memcpy(dest, src, count * sizeof(*dest));
    }
    inline
    static void x_no_sse_MoveBuffer(int* dest, const int* src, size_t count) {
        memmove(dest, src, count * sizeof(*dest));
    }
    inline 
    static void x_no_sse_ConvertBuffer( int* dest, const char* src, size_t count) {
        for (size_t e = 0; e < count; ++e) {
            *(dest + e) = (unsigned char)*(src + e);
        } 
    }
    inline
    static void x_no_sse_ConvertBuffer(char* dest, const int*  src, size_t count) {
        for (size_t e = 0; e < count; ++e) {
            *(dest + e) = (char)*(src + e);
        } 
    }

    static void x_no_sse_SplitBufferInto4(const int* src, size_t count, int*  dest0, int*  dest1, int*  dest2, int*  dest3);
    static void x_no_sse_SplitBufferInto4(const int* src, size_t count, char* dest0, char* dest1, char* dest2, char* dest3);

    static unsigned int x_no_sse_FindMaxElement(const unsigned int* src, size_t count, unsigned int v);
    static void x_no_sse_Find4MaxElements(const unsigned int* src, size_t count, unsigned int dest[4]);


}; // NFast


#if defined(NCBI_COMPILER_GCC) && defined(NCBI_HAVE_FAST_OPS)
// fast implementation using internal layout of vector
template<class V, class A>
inline
V* NFast::AppendUninitialized(vector<V, A>& dst, size_t count)
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
void NFast::AppendZeros(vector<V, A>& dst, size_t count) {
    x_ClearBuffer(AppendUninitialized(dst, count), count);
}
template<class V, class A>
inline
void NFast::AppendZerosAligned16(vector<V, A>& dst, size_t count) {
    x_sse_ClearBuffer(AppendUninitialized(dst, count), count);
}
#else
// default implementation
template<class V, class A>
inline
V* NFast::AppendUninitialized(vector<V, A>& dst, size_t count)
{
    size_t size = dst.size();
    dst.resize(size+count);
    return dst.data()+size;
}
template<class V, class A>
inline
void NFast::AppendZeros(vector<V, A>& dst, size_t count) {
    dst.resize(dst.size()+count);
}
template<class V, class A>
inline
void NFast::AppendZerosAligned16(vector<V, A>& dst, size_t count) {
    dst.resize(dst.size()+count);
}
#endif

//---------------------------------------------------------------------------
inline
void NFast::ClearBuffer(char* dest, size_t count) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && uintptr_t(dest)%16 == 0) {
        x_sse_ClearBuffer(dest,count);
    } else {
        x_no_sse_ClearBuffer(dest, count);
    }
#else
    x_no_sse_ClearBuffer(dest, count);
#endif
}
inline
void NFast::ClearBuffer(int* dest, size_t count) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && uintptr_t(dest)%16 == 0) {
        x_sse_ClearBuffer(dest,count);
    } else {
        x_no_sse_ClearBuffer(dest, count);
    }
#else
    x_no_sse_ClearBuffer(dest, count);
#endif

}
inline
void NFast::ClearBuffer(unsigned int* dest, size_t count) {
    ClearBuffer(reinterpret_cast<int*>(dest), count);
}

inline
void NFast::CopyBuffer(const int* src, size_t count, int* dest) {
    _ASSERT((src+count <= dest) || (dest+count <= src));
#if 0 // because memmove is faster
#if defined(NCBI_HAVE_FAST_OPS)  &&  !defined(NCBI_COMPILER_GCC) \
    &&  !defined(NCBI_COMPILER_ICC)  &&  !defined(NCBI_COMPILER_ANY_CLANG)
    if (count%16 == 0 && uintptr_t(dest)%16 == 0 && uintptr_t(src)%16 == 0) {
        x_sse_CopyBuffer(src, count, dest);
    } else {
        x_no_sse_CopyBuffer(dest, src, count);
    }
#else
    x_no_sse_CopyBuffer(dest, src, count);
#endif
#else
    x_no_sse_MoveBuffer(dest, src, count);
#endif
}
inline
void NFast::CopyBuffer(const unsigned int* src, size_t count, int* dest) {
    CopyBuffer(reinterpret_cast<const int*>(src), count, dest);
}
inline
void NFast::CopyBuffer(const unsigned int* src, size_t count, unsigned int* dest) {
    CopyBuffer(reinterpret_cast<const int*>(src), count, reinterpret_cast<int*>(dest));
}

inline
void NFast::MoveBuffer(const int* src, size_t count, int* dest) {
    x_no_sse_MoveBuffer(dest, src, count);
}
inline
void NFast::MoveBuffer(const unsigned int* src, size_t count, int* dest) {
    MoveBuffer(reinterpret_cast<const int*>(src), count, dest);
}
inline
void NFast::MoveBuffer(const unsigned int* src, size_t count, unsigned int* dest) {
    MoveBuffer(reinterpret_cast<const int*>(src), count, reinterpret_cast<int*>(dest));
}

inline
void NFast::ConvertBuffer(const char* src, size_t count, int* dest) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && uintptr_t(dest)%16 == 0 && uintptr_t(src)%16 == 0) {
        x_sse_ConvertBuffer(src, count, dest);
    } else {
        x_no_sse_ConvertBuffer(dest, src, count);
    }
#else
    x_no_sse_ConvertBuffer(dest, src, count);
#endif
}
inline
void NFast::ConvertBuffer(const int* src, size_t count, char* dest) {
#if defined(NCBI_HAVE_FAST_OPS) && !defined(NCBI_COMPILER_ICC)
    if (count%16 == 0 && uintptr_t(dest)%16 == 0 && uintptr_t(src)%16 == 0) {
        x_sse_ConvertBuffer(src, count, dest);
    } else {
        x_no_sse_ConvertBuffer(dest, src, count);
    }
#else
    x_no_sse_ConvertBuffer(dest, src, count);
#endif
}
inline
void NFast::ConvertBuffer(const unsigned int* src, size_t count, char* dest) {
    ConvertBuffer(reinterpret_cast<const int*>(src), count, dest); 
}

inline
void NFast::SplitBufferInto4(const int* src, size_t count, int*  dest0, int*  dest1, int*  dest2, int*  dest3) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && uintptr_t(src)%16 == 0 &&
        uintptr_t(dest0)%16 == 0 && uintptr_t(dest1)%16 == 0 && uintptr_t(dest2)%16 == 0 && uintptr_t(dest3)%16 == 0) {
        x_sse_SplitBufferInto4(src, count, dest0, dest1, dest2, dest3);
    } else {
        x_no_sse_SplitBufferInto4(src, count, dest0, dest1, dest2, dest3);
    }
#else
    x_no_sse_SplitBufferInto4(src, count, dest0, dest1, dest2, dest3);
#endif
}
inline
void NFast::SplitBufferInto4(const int* src, size_t count, char* dest0, char* dest1, char* dest2, char* dest3) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && uintptr_t(src)%16 == 0 &&
        uintptr_t(dest0)%16 == 0 && uintptr_t(dest1)%16 == 0 && uintptr_t(dest2)%16 == 0 && uintptr_t(dest3)%16 == 0) {
        x_sse_SplitBufferInto4(src, count, dest0, dest1, dest2, dest3);
    } else {
        x_no_sse_SplitBufferInto4(src, count, dest0, dest1, dest2, dest3);
    }
#else
    x_no_sse_SplitBufferInto4(src, count, dest0, dest1, dest2, dest3);
#endif
}
inline
void NFast::SplitBufferInto4(const unsigned int* src, size_t count,
                 int*  dest0, int*  dest1, int*  dest2, int*  dest3) {
    SplitBufferInto4(reinterpret_cast<const int*>(src), count, dest0, dest1, dest2, dest3);
}
inline
void NFast::SplitBufferInto4(const unsigned int* src, size_t count,
                 unsigned int*  dest0, unsigned int*  dest1, unsigned int*  dest2, unsigned int*  dest3) {
    SplitBufferInto4(reinterpret_cast<const int*>(src), count,
                reinterpret_cast<int*>(dest0), reinterpret_cast<int*>(dest1),
                reinterpret_cast<int*>(dest2), reinterpret_cast<int*>(dest3));
}
inline
void NFast::SplitBufferInto4(const unsigned int* src, size_t count,
                 char* dest0, char* dest1, char* dest2, char* dest3) {
    SplitBufferInto4(reinterpret_cast<const int*>(src), count, dest0, dest1, dest2, dest3);
}

inline
unsigned int NFast::FindMaxElement(const unsigned int* src, size_t count) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && uintptr_t(src)%16 == 0) {
        return x_sse_FindMaxElement(src,count);
    } else {
        return x_no_sse_FindMaxElement(src, count, 0);
    }
#else
    return x_no_sse_FindMaxElement(src, count, 0);
#endif
}
inline
void NFast::FindMaxElement(const unsigned int* src, size_t count, unsigned int& dest) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && uintptr_t(src)%16 == 0) {
        x_sse_FindMaxElement(src, count, dest);
    } else {
        dest = x_no_sse_FindMaxElement(src, count, dest);
    }
#else
    dest = x_no_sse_FindMaxElement(src, count, dest);
#endif
}
inline
void NFast::Find4MaxElements(const unsigned int* src, size_t count, unsigned int dest[4]) {
#if defined(NCBI_HAVE_FAST_OPS)
    if (count%16 == 0 && uintptr_t(src)%16 == 0) {
        x_sse_Find4MaxElements(src, count, dest);
    } else {
        x_no_sse_Find4MaxElements(src, count, dest);
    }
#else
    x_no_sse_Find4MaxElements(src, count, dest);
#endif
}

END_NCBI_SCOPE

#endif // CORELIB___NCBI_FAST__HPP
