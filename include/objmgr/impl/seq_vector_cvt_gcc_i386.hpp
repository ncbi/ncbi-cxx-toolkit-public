#ifndef SEQ_VECTOR_CVT_GCC_I386__HPP
#define SEQ_VECTOR_CVT_GCC_I386__HPP
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
* Author: Eugene Vasilchenko
*
* File Description:
*   Seq-vector conversion functions for Intel CPU with GCC.
*
*/

#include <objmgr/impl/seq_vector_cvt_gen.hpp>

#if 0
template<class SrcCont>
void copy_2bit_table(char* dst, size_t count,
                     const SrcCont& srcCont, size_t srcPos,
                     const char* table)
{
    const char* src = &srcCont[srcPos / 4];
    {
        // odd chars first
        char c = *src;
        switch ( srcPos % 4 ) {
        case 1:
            *(dst++) = table[(c >> 4) & 0x03];
            if ( --count == 0 ) return;
            // intentional fall through 
        case 2:
            *(dst++) = table[(c >> 2) & 0x03];
            if ( --count == 0 ) return;
            // intentional fall through 
        case 3:
            *(dst++) = table[(c     ) & 0x03];
            ++src;
            --count;
            break;
        }
    }
    for ( DstIter end = dst + (count & ~3); dst != end; dst += 4, ++src ) {
        char c3 = *src;
        char c0 = c3 >> 6;
        char c1 = c3 >> 4;
        char c2 = c3 >> 2;
        c0 = table[c0 & 0x03];
        c1 = table[c1 & 0x03];
        *(dst  ) = c0;
        c2 = table[c2 & 0x03];
        *(dst+1) = c1;
        c3 = table[c3 & 0x03];
        *(dst+2) = c2;
        *(dst+3) = c3;
    }
    // remaining odd chars
    switch ( count % 4 ) {
    case 3:
        *(dst+2) = table[(*src >> 2) & 0x03];
        // intentional fall through
    case 2:
        *(dst+1) = table[(*src >> 4) & 0x03];
        // intentional fall through
    case 1:
        *(dst  ) = table[(*src >> 6) & 0x03];
        break;
    }


    copy_2bit_table_up(dst, count, src, table
    copy_2bit_table(ds
    asm volatile("lock; xaddl %1, %0" : "=m" (*nv_value_p), "=r" (result)
                 : "1" (delta), "m" (*nv_value_p));


#endif

#endif//SEQ_VECTOR_CVT_GCC_I386__HPP
