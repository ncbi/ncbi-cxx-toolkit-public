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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   Shared utility functions for the various sequtil classes.
 */   
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <util/sequtil/sequtil.hpp>
#include "sequtil_shared.hpp"


BEGIN_NCBI_SCOPE

// converts one byte for another using the conversion table.
SIZE_TYPE convert_1_to_1
(const char* src, 
 TSeqPos pos,
 TSeqPos length,
 char* dst, 
 const Uint1* table)
{
    const char* iter = src + pos;
    const char* end = src + pos + length;

    for ( ; iter != end; ++iter, ++dst ) {
        *dst = table[static_cast<Uint1>(*iter)];
    }
    
    return length;
}


SIZE_TYPE convert_1_to_2
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst,
 const Uint1* table)
{
    size_t size = length;

    const char* iter = src + (pos / 2);

    // first position
    if ( pos % 2 != 0 ) {
        *dst = table[static_cast<unsigned char>(*iter) * 2 + 1];
        ++dst;
        ++iter;
        --size;
    }

    // NB: we "trick" the compiler so that we copy 2 bytes instead
    // of one with each assignment operation
    Uint2* out_i  = reinterpret_cast<Uint2*>(dst);
    const Uint2* table2 = reinterpret_cast<const Uint2*>(table);
    for( size_t i = size / 2; i; --i, ++out_i, ++iter ) {
        *out_i = table2[static_cast<Uint1>(*iter)];
    }

    // last position
    if ( size % 2 != 0 )
    {
        // just copy a single char
        char* last = reinterpret_cast<char*>(out_i);
        *last = table[static_cast<Uint1>(*iter) * 2];
    }

    return length;
}


SIZE_TYPE convert_1_to_4
(const char* src, 
 TSeqPos pos,
 TSeqPos length,
 char* dst, 
 const Uint1* table)
{
    size_t size = length;

    const char* iter = src + (pos / 4);

    // first position
    if ( pos % 4 != 0 ) {
        size_t to = min(static_cast<unsigned int>(4), (pos % 4) + length);
        for ( size_t i = pos % 4; i < to; ++i, ++dst ) {
            *dst = table[static_cast<Uint1>(*iter) * 4 + i];
        }
        ++iter;
        size -= to - (pos % 4);
    }

    // NB: we "trick" the compiler so that we copy 4 bytes instead
    // of one with each assignment operation
    Uint4* out_i  = reinterpret_cast<Uint4*>(dst);
    const Uint4* table4 = reinterpret_cast<const Uint4*>(table);
    for( size_t i = size / 4; i; --i, ++out_i, ++iter ) {
        *out_i = table4[static_cast<Uint1>(*iter)];
    }

    // last position
    if ( size % 4 != 0 )
    {
        char* last = reinterpret_cast<char*>(out_i);

        for ( size_t i = 0; i < size % 4; ++i, ++last ) {
            *last = table[static_cast<Uint1>(*iter) * 4 + i];
        }
    }

    return length;
}


SIZE_TYPE copy_1_to_1_reverse
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst, 
 const Uint1* table)
{
    const char* begin = src + pos;
    const char* iter = src + pos + length;

    for ( ; iter != begin; ++dst ) {
        *dst = table[static_cast<Uint1>(*--iter)];
    }
    
    return length;
}


SIZE_TYPE revcmp
(char* buf, 
 TSeqPos pos,
 TSeqPos length,
 const Uint1* table)
{
    char* first = buf + pos;
    char* last  = first + length - 1;
    char temp;

    for ( ; first <= last; ++first, --last ) {
        temp = table[static_cast<Uint1>(*first)];
        *first = table[static_cast<Uint1>(*last)];
        *last = temp;
    }

    if ( pos != 0 ) {
        copy(buf + pos, buf + pos + length, buf);
    }

    return length;
}


size_t GetBasesPerByte(CSeqUtil::TCoding coding)
{
    if ( coding == CSeqUtil::e_Ncbi2na ) {
        return 4;
    } else if ( coding == CSeqUtil::e_Ncbi4na ) {
        return 2;
    }
    
    return 1;
}


SIZE_TYPE GetBytesNeeded(CSeqUtil::TCoding coding, TSeqPos length)
{
    switch (coding) {
    case CSeqUtil::e_not_set:  return 0;
    case CSeqUtil::e_Ncbi2na:  return (length + 3) / 4;
    case CSeqUtil::e_Ncbi4na:  return (length + 1) / 2;
    default:                   return length;
    }
}

END_NCBI_SCOPE
