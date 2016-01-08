#ifndef UTIL_SEQUTIL___SEQUTIL_SHARED__HPP
#define UTIL_SEQUTIL___SEQUTIL_SHARED__HPP

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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   Shared utility functions for the various sequtil classes.
 */   

#include <corelib/ncbistd.hpp>

#include <util/sequtil/sequtil.hpp>


BEGIN_NCBI_SCOPE


SIZE_TYPE convert_1_to_1(const char* src, 
                         TSeqPos pos, TSeqPos length,
                         char* dst, 
                         const Uint1* table);

SIZE_TYPE convert_1_to_2(const char* src,
                         TSeqPos pos, TSeqPos length,
                         char* dst,
                         const Uint1* table);

SIZE_TYPE convert_1_to_4(const char* src, 
                         TSeqPos pos, TSeqPos length,
                         char* dst, 
                         const Uint1* table);

SIZE_TYPE copy_1_to_1_reverse(const char* src,
                              TSeqPos pos, TSeqPos length,
                              char* dst, 
                              const Uint1* table);

SIZE_TYPE revcmp(char* buf, TSeqPos pos, TSeqPos length, const Uint1* table);


size_t GetBasesPerByte(CSeqUtil::TCoding coding);

SIZE_TYPE GetBytesNeeded(CSeqUtil::TCoding coding, TSeqPos length);

template <typename C>
bool OutOfRange(TSeqPos pos, const C& container, CSeqUtil::TCoding coding)
{
    size_t bases_per_byte = GetBasesPerByte(coding);
    
    if ( (pos == kInvalidSeqPos)  ||  
        (pos > (container.size() * bases_per_byte) - 1) ) {
        return true;
    }
    return false;
}


template <typename C>
void ResizeDst(C& container, CSeqUtil::TCoding coding, TSeqPos length)
{
    size_t new_size = GetBytesNeeded(coding, length);
    
    if ( container.size() < new_size ) {
        container.resize(new_size);
    }
}


template <typename C>
void AdjustLength(C& container, CSeqUtil::TCoding coding, 
                  TSeqPos pos, TSeqPos& length)
{
    size_t bases_per_byte = GetBasesPerByte(coding);
    
    if ( pos + length > container.size() * bases_per_byte ) {
        length = (TSeqPos)(container.size() * bases_per_byte - pos);
    }
}


END_NCBI_SCOPE


#endif  /* UTIL_SEQUTIL___SEQUTIL_SHARED__HPP */
