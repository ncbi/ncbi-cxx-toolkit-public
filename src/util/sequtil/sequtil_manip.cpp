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
 * 
 */   
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <vector>
#include <algorithm>

#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_expt.hpp>
#include <util/sequtil/sequtil_manip.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include "sequtil_shared.hpp"
#include "sequtil_tables.hpp"


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
// Reverse

// When reversing a sequence the packed formats ncbi2na and ncbi4na 
// get "special" treatment, since the requesetd interval might not
// fall on a byte boundry.
// Other formats perform a simple conversion on the sequence. Note that
// if the original sequnece is erroneous (e.g. lower case) the reverse
// isn't "fixed". 

static SIZE_TYPE s_2naReverse
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    const char* begin = src + (pos / 4);
    const char* end   = src + ((pos + length - 1) / 4) + 1;
    const char* iter = end;

    size_t offset = (pos + length - 1) % 4;
    const Uint1* table = C2naReverse::GetTable(offset);

    if ( offset == 3 ) { // byte boundry when viewed from the end
        for ( ; iter != begin; ++dst ) {
            *dst = table[static_cast<Uint1>(*--iter)];
        }
        --dst;
    } else {
        --iter;
        for ( size_t count = length / 4;  count; --count, ++dst ) {
            *dst = 
                table[static_cast<Uint1>(*iter) * 2 + 1] |
                table[static_cast<Uint1>(*(iter - 1)) * 2];
            --iter;
        }

        // handle the overhang
        if ( length % 4 != 0 ) {
            *dst = table[static_cast<Uint1>(*iter) * 2 + 1];
            if ( iter != begin ) {
                --iter;
                *dst |= table[static_cast<Uint1>(*iter) * 2];
            }
        }
    }

    // now, take care of the last byte
    *dst &= (0xFF << ((4 - (length % 4)) % 4) * 2);

    return length;
}


static SIZE_TYPE s_4naReverse
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    size_t start_offset = (pos + length - 1) % 2;

    const Uint1* table = C4naReverse::GetTable();
    
    const char* begin = src + (pos / 2);
    const char* end   = src + ((pos + length - 1) / 2) + 1;
    const char* iter = end;

    switch ( start_offset ) {
    case 1:
        // byte boundry
        {{
            for ( ; iter != begin; ++dst ) {
                *dst = table[static_cast<Uint1>(*--iter)];
            }
            --dst;
            if ( length % 2 != 0 ) {
                *dst &= 0xF0;
            }
        }}
        break;

    case 0:
        {{
            for ( size_t count = length / 2; count; --count, ++dst ) {
                --iter;
                *dst = (static_cast<Uint1>(*iter) & 0xF0) |
                         (static_cast<Uint1>(*(iter - 1)) & 0x0F);
            }

            if ( length % 2 != 0 ) {
                --iter;
                *dst = static_cast<Uint1>(*iter) & 0xF0;
            }
        }}
        break;
    }

    return length;
}


template <typename SrcCont, typename DstCont>
SIZE_TYPE s_Reverse
(const SrcCont& src, 
 CSeqUtil::TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 DstCont& dst)
{
    _ASSERT(!OutOfRange(pos, src, src_coding));
    if ( src.empty()  ||  (length == 0) ) {
        return 0;
    }
    
    AdjustLength(src, src_coding, pos, length);
    ResizeDst(dst, src_coding, length);

    return CSeqManip::Reverse(&*src.begin(), src_coding,
                              pos, length, &*dst.begin());
}


SIZE_TYPE CSeqManip::Reverse
(const string& src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 string& dst)
{
    // call the templated version
    return s_Reverse(src, src_coding, pos, length, dst);
}


SIZE_TYPE CSeqManip::Reverse
(const vector<char>& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 vector<char>& dst)
{
    // call the templated version
    return s_Reverse(src, coding,pos, length, dst);
}


SIZE_TYPE CSeqManip::Reverse
(const char* src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    _ASSERT((dst != 0)  &&  (src != 0));

    switch ( src_coding ) {

    // "special" treatment
    case CSeqUtil::e_Ncbi2na:
        return s_2naReverse(src, pos, length, dst);

    case CSeqUtil::e_Ncbi4na:
        return s_4naReverse(src, pos, length, dst);

    // a simple reverse
    default:
        reverse_copy(src + pos, src + pos + length, dst);
        return length;
    }

    NCBI_THROW(CSeqUtilException, eInvalidCoding, "Unknown coding");
}


/////////////////////////////////////////////////////////////////////////////
//
// Complement

template <typename SrcCont, typename DstCont>
SIZE_TYPE s_Complement
(const SrcCont& src, 
 CSeqUtil::TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 DstCont& dst)
{
    _ASSERT(!OutOfRange(pos, src, src_coding));
    if ( src.empty()  ||  (length == 0) ) {
        return 0;
    }
    
    AdjustLength(src, src_coding, pos, length);
    ResizeDst(dst, src_coding, length);

    return CSeqManip::Complement(&*src.begin(), src_coding,
                                 pos, length, &*dst.begin());
}


SIZE_TYPE CSeqManip::Complement
(const string& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 string& dst)
{
    // call the templated version
    return s_Complement(src, coding,pos, length, dst);
}


SIZE_TYPE CSeqManip::Complement
(const vector<char>& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 vector<char>& dst)
{
    // call the templated version
    return s_Complement(src, coding,pos, length, dst);
}


static SIZE_TYPE s_Ncbi2naComplement
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    const char* iter = src + (pos / 4);
    const char* end  = src + ((pos + length - 1) / 4) + 1;

    if ( pos % 4 == 0 ) {
        for ( ; iter != end; ++iter, ++dst ) {
            *dst = ~(*iter);
        }

        if ( length % 4 != 0 ) {
            *(--dst) &= (0xFF << (8 - (length % 4) * 2));
        }
    } else {
        const Uint1* table = C2naCmp::GetTable(pos % 4);

        for ( size_t count = length / 4;  count; --count, ++dst, ++iter ) {
            *dst= 
                table[static_cast<Uint1>(*iter) * 2] |
                table[static_cast<Uint1>(*(iter + 1)) * 2 + 1];
        }

        // handle the overhang
        if ( length % 4 != 0 ) {
            *dst = table[static_cast<Uint1>(*iter) * 2];
            if ( ++iter != end ) {
                *dst |= table[static_cast<Uint1>(*iter) * 2 + 1];
            }
        }
    }
    // now, take care of the last byte
    *dst &= (0xFF << ((4 - (length % 4)) % 4) * 2);

    return length;
}


static SIZE_TYPE s_Ncbi2naExpandComplement
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    const char* end  = src + pos + length;
    const char* iter = src + pos;

    for ( ; iter != end; ++iter, ++dst ) {
        *dst = 3 - static_cast<Uint1>(*iter);
    }

    return length;
}


static SIZE_TYPE s_Ncbi4naComplement
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    const char* iter = src + (pos / 2);
    const char* end  = src + (pos + length - 1) / 2 + 1;

    const Uint1* table = C4naCmp::GetTable(pos % 2);

    switch ( pos % 2 ) {
    case 0:
        {{
            for ( ; iter != end; ++iter, ++dst ) {
                *dst = table[static_cast<Uint1>(*iter)];
            }

            if ( length % 2 != 0 ) {
                *dst &= 0xF0;
            }
        }}
        break;

    case 1:
        {{
            for ( size_t count = length / 2;  count; --count, ++iter, ++dst ) {
                *dst =
                    table[static_cast<Uint1>(*iter) * 2] |
                    table[static_cast<Uint1>(*(iter + 1)) * 2 + 1];
            }

            if ( length % 2 != 0 ) {
                *dst = table[static_cast<Uint1>(*iter) * 2];
            }
        }}
        break;
    }

    return length;
}


SIZE_TYPE CSeqManip::Complement
(const char* src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    _ASSERT((dst != 0)  &&  (src != 0));

    switch ( src_coding ) {
    case CSeqUtil::e_Iupacna:
        return convert_1_to_1(src, pos, length, dst, CIupacnaCmp::GetTable());

    case CSeqUtil::e_Ncbi2na:
        return s_Ncbi2naComplement(src, pos, length, dst);

    case CSeqUtil::e_Ncbi2na_expand:
        return s_Ncbi2naExpandComplement(src, pos, length, dst);

    case CSeqUtil::e_Ncbi4na:
        return s_Ncbi4naComplement(src, pos, length, dst);

    case CSeqUtil::e_Ncbi8na:
    case CSeqUtil::e_Ncbi4na_expand:
        return convert_1_to_1(src, pos, length, dst, C8naCmp::GetTable());

    default:
        break;
    }

    NCBI_THROW(CSeqUtilException, eInvalidCoding,
        "There is no complement for the specified coding.");
}

/////////////////////////////////////////////////////////////////////////////
//
// ReverseComplement

template <typename SrcCont, typename DstCont>
SIZE_TYPE s_ReverseComplement
(const SrcCont& src, 
 CSeqUtil::TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 DstCont& dst)
{
    _ASSERT(!OutOfRange(pos, src, src_coding));
    if ( src.empty()  ||  (length == 0) ) {
        return 0;
    }
    
    AdjustLength(src, src_coding, pos, length);
    ResizeDst(dst, src_coding, length);

    return CSeqManip::ReverseComplement(&*src.begin(), src_coding, 
                                        pos, length, &*dst.begin());
}


SIZE_TYPE CSeqManip::ReverseComplement
(const string& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 string& dst)
{
    // call the templated version
    return s_ReverseComplement(src, coding,pos, length, dst);
}


SIZE_TYPE CSeqManip::ReverseComplement
(const vector<char>& src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 vector<char>& dst)
{
    // call the templated version
    return s_ReverseComplement(src, coding,pos, length, dst);
}


static SIZE_TYPE s_Ncbi2naRevCmp
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    size_t offset = (pos + length - 1) % 4;
    const Uint1* table = C2naRevCmp::GetTable(offset);

    const char* begin = src + (pos / 4);
    const char* iter = src + (pos + length - 1) / 4 + 1;
    switch ( offset ) {
    case 0:
    case 1:
    case 2:
        --iter;
        for ( size_t count = length / 4;  count; --count, ++dst, --iter ) {
            *dst = 
                table[static_cast<Uint1>(*iter) * 2] |
                table[static_cast<Uint1>(*(iter - 1)) * 2 + 1];
        }

        // handle the overhang
        if ( length % 4 != 0 ) {
            *dst = table[static_cast<Uint1>(*iter) * 2];
            if ( iter != begin ) {
                --iter;
                *dst |= table[static_cast<Uint1>(*iter) * 2 + 1];
            }
        }
        break;

    case 3:
        // aligned operation
        for ( ; iter != begin; ++dst ) {
            *dst = table[static_cast<Uint1>(*--iter)];
        }
        break;
    }

    // zero redundent bits
    *dst &= (0xFF << ((4 - (length % 4)) % 4) * 2);

    return length;
}


static SIZE_TYPE s_Ncbi2naExpandRevCmp
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    const char* begin = src + pos;
    const char* iter  = src + pos + length;

    for ( ; iter != begin; ++dst ) {
        *dst = 3 - static_cast<Uint1>(*--iter);
    }

    return length;
}


static SIZE_TYPE s_Ncbi4naRevCmp
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    const char* begin = src + (pos / 2);
    const char* iter  = src + ((pos + length - 1) / 2) + 1;

    size_t offset = (pos + length - 1) % 2;
    const Uint1* table = C4naRevCmp::GetTable(offset);

    switch ( offset ) {
    case 0:
        {{
            --iter;
            for ( size_t count = length / 2;  count; --count, --iter, ++dst ) {
                *dst =
                    table[static_cast<Uint1>(*iter) * 2] |
                    table[static_cast<Uint1>(*(iter - 1)) * 2 + 1];
            }

            if ( length % 2 != 0 ) {
                *dst = table[static_cast<Uint1>(*iter) * 2];
            }
        }}
        break;

    case 1:
        {{
            for ( ; iter != begin; ++dst ) {
                *dst = table[static_cast<Uint1>(*--iter)];
            }

            if ( length % 2 != 0 ) {
                *dst &= 0xF0;
            }
        }}
        break;
    }

    return length;
}


SIZE_TYPE CSeqManip::ReverseComplement
(const char* src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    _ASSERT((dst != 0)  &&  (src != 0));

    switch ( src_coding ) {
    case CSeqUtil::e_Iupacna:
        return copy_1_to_1_reverse(src, pos, length, dst, 
                                   CIupacnaCmp::GetTable());

    case CSeqUtil::e_Ncbi2na:
        return s_Ncbi2naRevCmp(src, pos, length, dst);

    case CSeqUtil::e_Ncbi2na_expand:
        return s_Ncbi2naExpandRevCmp(src, pos, length, dst);

    case CSeqUtil::e_Ncbi4na:
        return s_Ncbi4naRevCmp(src, pos, length, dst);

    case CSeqUtil::e_Ncbi8na:
    case CSeqUtil::e_Ncbi4na_expand:
        return copy_1_to_1_reverse(src, pos, length, dst, 
                                   C8naCmp::GetTable());
    default:
        break;
    }

    NCBI_THROW(CSeqUtilException, eInvalidCoding,
        "There is no complement for the specified coding.");
}


// in place

template <typename SrcCont>
SIZE_TYPE s_ReverseComplement
(SrcCont& src, 
 CSeqUtil::TCoding src_coding,
 TSeqPos pos,
 TSeqPos length)
{
    _ASSERT(!OutOfRange(pos, src, src_coding));
    if ( src.empty()  ||  (length == 0) ) {
        return 0;
    }
    
    AdjustLength(src, src_coding, pos, length);

    return CSeqManip::ReverseComplement(&*src.begin(), src_coding,
                                        pos, length);
}


SIZE_TYPE CSeqManip::ReverseComplement
(string& src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length)
{
    // call the templated version
    return s_ReverseComplement(src, src_coding, pos, length);
}


SIZE_TYPE CSeqManip::ReverseComplement
(vector<char>& src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length)
{
    // call the templated version
    return s_ReverseComplement(src, src_coding, pos, length);
}


static SIZE_TYPE s_Ncbi2naExpandRevCmp
(char* src,
 TSeqPos pos,
 TSeqPos length)
{
    char* first = src + pos;    
    char* last  = first + length;
    char temp;

    for ( ; first <= last; ++first, --last ) {
        temp = 3 - *first;
        *first = 3 - *last;
        *last = temp;
    }

    if ( pos != 0 ) {
        copy(src + pos, src + pos + length, src);
    }

    return length;
}


static SIZE_TYPE s_Ncbi2naRevCmp
(char* src,
 TSeqPos pos,
 TSeqPos length)
{
    char* buf = new char[length];
    CSeqConvert::Convert(src, CSeqUtil::e_Ncbi2na, pos, length, 
        buf, CSeqUtil::e_Ncbi8na);
    revcmp(buf, pos, length, C8naCmp::GetTable());
    CSeqConvert::Convert(buf, CSeqUtil::e_Ncbi8na, 0, length, 
        src, CSeqUtil::e_Ncbi2na);
    delete[] buf;

    return length;
}


static SIZE_TYPE s_Ncbi4naRevCmp
(char* src,
 TSeqPos pos,
 TSeqPos length)
{
    char* buf = new char[length];
    CSeqConvert::Convert(src, CSeqUtil::e_Ncbi4na, pos, length, 
        buf, CSeqUtil::e_Ncbi8na);
    revcmp(buf, pos, length, C8naCmp::GetTable());
    CSeqConvert::Convert(buf, CSeqUtil::e_Ncbi8na, 0, length, 
        src, CSeqUtil::e_Ncbi4na);
    delete[] buf;

    return length;
}


SIZE_TYPE CSeqManip::ReverseComplement
(char* src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length)
{
    _ASSERT(src != 0);

    switch ( src_coding ) {
    case CSeqUtil::e_Iupacna:
        return revcmp(src, pos, length, CIupacnaCmp::GetTable());

    case CSeqUtil::e_Ncbi2na:
        return s_Ncbi2naRevCmp(src, pos, length);

    case CSeqUtil::e_Ncbi2na_expand:
        return s_Ncbi2naExpandRevCmp(src, pos, length);

    case CSeqUtil::e_Ncbi4na:
        return s_Ncbi4naRevCmp(src, pos, length);

    case CSeqUtil::e_Ncbi8na:
    case CSeqUtil::e_Ncbi4na_expand:
        return revcmp(src, pos, length, C8naCmp::GetTable());

    default:
        break;
    }

    NCBI_THROW(CSeqUtilException, eInvalidCoding,
        "There is no complement for the specified coding.");
}

END_NCBI_SCOPE
