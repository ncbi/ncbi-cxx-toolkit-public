#ifndef NCBI_BSWAP__HPP
#define NCBI_BSWAP__HPP
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: Byte swapping functions.
 *
 */

#include <corelib/ncbitype.h>

BEGIN_NCBI_SCOPE


inline
Int2 ByteSwapGetInt2(const unsigned char* ptr)
{
    Int2 ret = (ptr[0] << 8) | (ptr[1]);
    return ret;
}

inline
void ByteSwapPutInt2(const unsigned char* ptr, Int2 value)
{
    ptr[0] = (unsigned char)(value >> 8);
    ptr[1] = (unsigned char)(value);
}


inline
Int4 ByteSwapGetInt4(const unsigned char* ptr)
{
    Int4 ret = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | (ptr[3]);
    return ret;
}

inline
void ByteSwapPutInt4(unsigned char* ptr, Int4 value)
{
    ptr[0] = (unsigned char)(value >> 24);
    ptr[1] = (unsigned char)(value >> 16);
    ptr[2] = (unsigned char)(value >> 8);
    ptr[3] = (unsigned char)(value);
}

inline
Int8 ByteSwapGetInt8(const unsigned char* ptr)
{
    Int8 ret = (ptr[0] << 56) | 
               (ptr[1] << 48) | 
               (ptr[2] << 40) | 
               (ptr[3] << 32) |
               (ptr[4] << 24) |
               (ptr[5] << 16) |
               (ptr[6] << 8)  |
               (ptr[7]);
    return ret;
}

inline
void ByteSwapPutInt8(unsigned char* ptr, Int8 value)
{
    ptr[0] = (unsigned char)(value >> 56);
    ptr[1] = (unsigned char)(value >> 48);
    ptr[2] = (unsigned char)(value >> 40);
    ptr[3] = (unsigned char)(value >> 32);
    ptr[4] = (unsigned char)(value >> 24);
    ptr[5] = (unsigned char)(value >> 16);
    ptr[6] = (unsigned char)(value >> 8);
    ptr[7] = (unsigned char)(value);
}


inline
float ByteSwapGetFloat(const unsigned char* ptr)
{
    Int4 ret = ByteSwapGetInt4(ptr);
    return reinterpret_cast<float>(ret);
}

inline
void ByteSwapPutFloat(unsigned char* ptr, float value)
{
    ByteSwapPutInt4(ptr, reinterpret_cast<Int4>(value));
}


inline
double ByteSwapGetDouble(const unsigned char* ptr)
{
    Int8 ret = ByteSwapGetInt8(ptr);
    return reinterpret_cast<double>(ret);
}

inline
void ByteSwapPutDouble(unsigned char* ptr, double value)
{
    ByteSwapPutInt8(ptr, reinterpret_cast<Int8>(value));
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/09/08 20:36:51  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* NCBI_BSWAP__HPP */
