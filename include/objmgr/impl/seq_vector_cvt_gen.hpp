#ifndef SEQ_VECTOR_CVT_GEN__HPP
#define SEQ_VECTOR_CVT_GEN__HPP
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
*   Generic Seq-vector conversion functions
*
*/

BEGIN_NCBI_SCOPE

template<class DstIter, class SrcCont>
inline
void copy_8bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos;
    for ( DstIter dst_end(dst + count); dst != dst_end; ++src, ++dst ) {
        *dst = *src;
    }
}


template<class DstIter, class SrcCont>
inline
void copy_8bit_table(DstIter dst, size_t count,
                     const SrcCont& srcCont, size_t srcPos,
                     const char* table)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos;
    for ( DstIter dst_end(dst + count); dst != dst_end; ++src, ++dst ) {
        *dst = table[static_cast<unsigned char>(*src)];
    }
}


template<class DstIter, class SrcCont>
inline
void copy_8bit_reverse(DstIter dst, size_t count,
                       const SrcCont& srcCont, size_t srcPos)
{
    srcPos += (count - 1);
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos;
    for ( DstIter dst_end(dst + count); dst != dst_end; --src, ++dst ) {
        *dst = *src;
    }
}


template<class DstIter, class SrcCont>
inline
void copy_8bit_table_reverse(DstIter dst, size_t count,
                             const SrcCont& srcCont, size_t srcPos,
                             const char* table)
{
    srcPos += (count - 1);
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos;
    for ( DstIter end(dst + count); dst != end; --src, ++dst ) {
        *dst = table[static_cast<unsigned char>(*src)];
    }
}


template<class DstIter, class SrcCont>
void copy_4bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 2;
    if ( srcPos % 2 ) {
        // odd char first
        char c = *src++;
        *(dst++) = (c     ) & 0x0f;
        --count;
    }
    for ( DstIter end(dst + (count & ~1)); dst != end; dst += 2, ++src ) {
        char c = *src;
        *(dst  ) = (c >> 4) & 0x0f;
        *(dst+1) = (c     ) & 0x0f;
    }
    if ( count % 2 ) {
        // remaining odd char
        char c = *src;
        *(dst  ) = (c >> 4) & 0x0f;
    }
}


template<class DstIter, class SrcCont>
void copy_4bit_table(DstIter dst, size_t count,
                     const SrcCont& srcCont, size_t srcPos,
                     const char* table)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 2;
    if ( srcPos % 2 ) {
        // odd char first
        char c = *src++;
        *(dst++) = table[(c    ) & 0x0f];
        --count;
    }
    for ( DstIter end(dst + (count & ~1)); dst != end; dst += 2, ++src ) {
        char c = *src;
        *(dst  ) = table[(c >> 4) & 0x0f];
        *(dst+1) = table[(c     ) & 0x0f];
    }
    if ( count % 2 ) {
        // remaining odd char
        char c = *src;
        *(dst  ) = table[(c >> 4) & 0x0f];
    }
}


template<class DstIter, class SrcCont>
void copy_4bit_reverse(DstIter dst, size_t count,
                       const SrcCont& srcCont, size_t srcPos)
{
    srcPos += (count - 1);
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 2;
    if ( srcPos % 2 ) {
        // odd char first
        char c = *src--;
        *(dst++) = (c >> 4) & 0x0f;
        --count;
    }
    for ( DstIter end(dst + (count & ~1)); dst != end; dst += 2, --src ) {
        char c = *src;
        *(dst  ) = (c     ) & 0x0f;
        *(dst+1) = (c >> 4) & 0x0f;
    }
    if ( count % 2 ) {
        // remaining odd char
        char c = *src;
        *(dst  ) = (c     ) & 0x0f;
    }
}


template<class DstIter, class SrcCont>
void copy_4bit_table_reverse(DstIter dst, size_t count,
                             const SrcCont& srcCont, size_t srcPos,
                             const char* table)
{
    srcPos += (count - 1);
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 2;
    if ( srcPos % 2 ) {
        // odd char first
        char c = *src--;
        *(dst++) = table[(c >> 4) & 0x0f];
        --count;
    }
    for ( DstIter end(dst + (count & ~1)); dst != end; dst += 2, --src ) {
        char c = *src;
        *(dst  ) = table[(c     ) & 0x0f];
        *(dst+1) = table[(c >> 4) & 0x0f];
    }
    if ( count % 2 ) {
        // remaining odd char
        char c = *src;
        *(dst  ) = table[(c     ) & 0x0f];
    }
}


template<class DstIter, class SrcCont>
void copy_2bit(DstIter dst, size_t count,
               const SrcCont& srcCont, size_t srcPos)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
    {
        // odd chars first
        char c = *src;
        switch ( srcPos % 4 ) {
        case 1:
            *(dst++) = (c >> 4) & 0x03;
            if ( --count == 0 ) return;
            // intentional fall through 
        case 2:
            *(dst++) = (c >> 2) & 0x03;
            if ( --count == 0 ) return;
            // intentional fall through 
        case 3:
            *(dst++) = (c     ) & 0x03;
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
        c0 &= 0x03;
        c1 &= 0x03;
        c2 &= 0x03;
        c3 &= 0x03;
        *(dst  ) = c0;
        *(dst+1) = c1;
        *(dst+2) = c2;
        *(dst+3) = c3;
    }
    // remaining odd chars
    switch ( count % 4 ) {
    case 3:
        *(dst+2) = (*src >> 2) & 0x03;
        // intentional fall through
    case 2:
        *(dst+1) = (*src >> 4) & 0x03;
        // intentional fall through
    case 1:
        *(dst  ) = (*src >> 6) & 0x03;
        break;
    }
}


template<class DstIter, class SrcCont>
void copy_2bit_reverse(DstIter dst, size_t count,
                       const SrcCont& srcCont, size_t srcPos)
{
    srcPos += (count - 1);
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
    {
        // odd chars first
        char c = *src;
        switch ( srcPos % 4 ) {
        case 2:
            *(dst++) = (c >> 2) & 0x03;
            if ( --count == 0 ) return;
            // intentional fall through 
        case 1:
            *(dst++) = (c >> 3) & 0x03;
            if ( --count == 0 ) return;
            // intentional fall through 
        case 0:
            *(dst++) = (c >> 6) & 0x03;
            --src;
            --count;
            break;
        }
    }
    for ( DstIter end = dst + (count & ~3); dst != end; dst += 4, --src ) {
        char c0 = *src;
        char c1 = c0 >> 2;
        char c2 = c0 >> 4;
        char c3 = c0 >> 6;
        c0 &= 0x03;
        c1 &= 0x03;
        c2 &= 0x03;
        c3 &= 0x03;
        *(dst  ) = c0;
        *(dst+1) = c1;
        *(dst+2) = c2;
        *(dst+3) = c3;
    }
    // remaining odd chars
    switch ( count % 4 ) {
    case 3:
        *(dst+2) = (*src >> 4) & 0x03;
        // intentional fall through
    case 2:
        *(dst+1) = (*src >> 2) & 0x03;
        // intentional fall through
    case 1:
        *(dst  ) = (*src     ) & 0x03;
        break;
    }
}


template<class DstIter, class SrcCont>
void copy_2bit_table(DstIter dst, size_t count,
                     const SrcCont& srcCont, size_t srcPos,
                     const char* table)
{
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
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
}


template<class DstIter, class SrcCont>
void copy_2bit_table_reverse(DstIter dst, size_t count,
                             const SrcCont& srcCont, size_t srcPos,
                             const char* table)
{
    srcPos += (count - 1);
    typename SrcCont::const_iterator src = srcCont.begin() + srcPos / 4;
    {
        // odd chars first  
        char c = *src;
        switch ( srcPos % 4 ) {
        case 2:
            *(dst++) = table[(c >> 2) & 0x03];
            if ( --count == 0 ) return;
            // intentional fall thro    ugh
        case 1:
            *(dst++) = table[(c >> 4) & 0x03];
            if ( --count == 0 ) return;
            // intentional fall thro    ugh
        case 0:
            *(dst++) = table[(c >> 6) & 0x03];
            --src;
            --count;
            break;
        }
    }
    for ( DstIter end = dst + (count & ~3); dst != end; dst += 4, --src ) {
        char c0 = *src;
        char c1 = c0 >> 2;
        char c2 = c0 >> 4;
        char c3 = c0 >> 6;
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
        *(dst+2) = table[(*src >> 4) & 0x03];
        // intentional fall through
    case 2:
        *(dst+1) = table[(*src >> 2) & 0x03];
        // intentional fall through
    case 1:
        *(dst  ) = table[(*src     ) & 0x03];
        break;
    }
}

END_NCBI_SCOPE

#endif//SEQ_VECTOR_CVT_GEN__HPP
