#ifndef CORELIB___NCBI_BSWAP__HPP
#define CORELIB___NCBI_BSWAP__HPP
/* $Id$
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
 * Author:  Anatoliy Kuznetsov, Kyrill Rotmistrovsky
 *   
 * File Description: Byte swapping functions.
 *
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbitype.h>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CByteSwap --
///
/// Class encapsulates byte swapping functions to convert between 
/// big endian - little endian architectures
///
/// Get and Put functions always do the byte swapping 
/// (change the byte order). If the input is BIG ENDIAN it is
/// converted to LITTLE ENDIAN and vice versa.
/// This group of functions is used when we know upfront that the 
/// incoming data were created on architecture with a different byte order 
/// and byte swapping is necessary.
///
///
/// Use case:
///
/// Usually it means sender writes all the data without conversion to network 
/// byte order, instead adds a small characteristic word describing the 
/// original byte order. Reader checks the byte order first and if it is 
/// the same just interprets the data in the regular manner without any 
/// conversion. 
/// In most cases when the data does not cross the border we have no 
/// performance impact (do not call CByteSwap::Get & Put methods).
/// Such "on demand" conversion scheme has potential performance advantage 
/// over the unconditional conversion to network byte order.
///

class CByteSwap
{
public:
    static Int2 GetInt2(const unsigned char* ptr);
    static void PutInt2(unsigned char* ptr, Int2 value);
    static Int4 GetInt4(const unsigned char* ptr);
    static void PutInt4(unsigned char* ptr, Int4 value);
    static Int8 GetInt8(const unsigned char* ptr);
    static void PutInt8(unsigned char* ptr, Int8 value);
    static float GetFloat(const unsigned char* ptr);
    static void PutFloat(unsigned char* ptr, float value);
    static double GetDouble(const unsigned char* ptr);
    static void PutDouble(unsigned char* ptr, double value);

private:
    union UFloatInt4 {
        float f;
        Int4  i;
    };

    union UDoubleInt8 {
        double d;
        Int8   i;
    };
};



inline
Int2 CByteSwap::GetInt2(const unsigned char* ptr)
{
#ifdef WORDS_BIGENDIAN
    Int2 ret = (Int2)( (Int2(ptr[1]) << 8) | (Int2(ptr[0])) );
#else
    Int2 ret = (Int2)( (Int2(ptr[0]) << 8) | (Int2(ptr[1])) );
#endif

    return ret;
}

inline
void CByteSwap::PutInt2(unsigned char* ptr, Int2 value)
{
#ifdef WORDS_BIGENDIAN
    ptr[1] = (unsigned char)(value >> 8);
    ptr[0] = (unsigned char)(value);
#else
    ptr[0] = (unsigned char)(value >> 8);
    ptr[1] = (unsigned char)(value);
#endif
}


inline
Int4 CByteSwap::GetInt4(const unsigned char* ptr)
{
#ifdef WORDS_BIGENDIAN
    Int4 ret = (Int4(ptr[3]) << 24) | 
               (Int4(ptr[2]) << 16) | 
               (Int4(ptr[1]) << 8)  | 
               (Int4(ptr[0]));
#else
    Int4 ret = (Int4(ptr[0]) << 24) | 
               (Int4(ptr[1]) << 16) | 
               (Int4(ptr[2]) << 8)  | 
               (Int4(ptr[3]));
#endif
    return ret;
}

inline
void CByteSwap::PutInt4(unsigned char* ptr, Int4 value)
{
#ifdef WORDS_BIGENDIAN
    ptr[3] = (unsigned char)(value >> 24);
    ptr[2] = (unsigned char)(value >> 16);
    ptr[1] = (unsigned char)(value >> 8);
    ptr[0] = (unsigned char)(value);
#else
    ptr[0] = (unsigned char)(value >> 24);
    ptr[1] = (unsigned char)(value >> 16);
    ptr[2] = (unsigned char)(value >> 8);
    ptr[3] = (unsigned char)(value);
#endif
}

inline
Int8 CByteSwap::GetInt8(const unsigned char* ptr)
{
#ifdef WORDS_BIGENDIAN
    Int8 ret = (Int8(ptr[7]) << 56) | 
               (Int8(ptr[6]) << 48) | 
               (Int8(ptr[5]) << 40) | 
               (Int8(ptr[4]) << 32) |
               (Int8(ptr[3]) << 24) |
               (Int8(ptr[2]) << 16) |
               (Int8(ptr[1]) << 8)  |
               (Int8(ptr[0]));
#else
    Int8 ret = (Int8(ptr[0]) << 56) | 
               (Int8(ptr[1]) << 48) | 
               (Int8(ptr[2]) << 40) | 
               (Int8(ptr[3]) << 32) |
               (Int8(ptr[4]) << 24) |
               (Int8(ptr[5]) << 16) |
               (Int8(ptr[6]) << 8)  |
               (Int8(ptr[7]));
#endif

    return ret;
}

inline
void CByteSwap::PutInt8(unsigned char* ptr, Int8 value)
{
#ifdef WORDS_BIGENDIAN
    ptr[7] = (unsigned char)(value >> 56);
    ptr[6] = (unsigned char)(value >> 48);
    ptr[5] = (unsigned char)(value >> 40);
    ptr[4] = (unsigned char)(value >> 32);
    ptr[3] = (unsigned char)(value >> 24);
    ptr[2] = (unsigned char)(value >> 16);
    ptr[1] = (unsigned char)(value >> 8);
    ptr[0] = (unsigned char)(value);
#else
    ptr[0] = (unsigned char)(value >> 56);
    ptr[1] = (unsigned char)(value >> 48);
    ptr[2] = (unsigned char)(value >> 40);
    ptr[3] = (unsigned char)(value >> 32);
    ptr[4] = (unsigned char)(value >> 24);
    ptr[5] = (unsigned char)(value >> 16);
    ptr[6] = (unsigned char)(value >> 8);
    ptr[7] = (unsigned char)(value);
#endif
}


inline
float CByteSwap::GetFloat(const unsigned char* ptr)
{
    UFloatInt4 u;
    u.i = CByteSwap::GetInt4(ptr);
    return u.f;
}

inline
void CByteSwap::PutFloat(unsigned char* ptr, float value)
{
    UFloatInt4 u;
    u.f = value;
    CByteSwap::PutInt4(ptr, u.i);
}


inline
double CByteSwap::GetDouble(const unsigned char* ptr)
{
    UDoubleInt8 u;
    u.i = CByteSwap::GetInt8(ptr);
    return u.d;
}

inline
void CByteSwap::PutDouble(unsigned char* ptr, double value)
{
    UDoubleInt8 u;
    u.d = value;
    CByteSwap::PutInt8(ptr, u.i);
}



END_NCBI_SCOPE

#endif /* NCBI_BSWAP__HPP */
