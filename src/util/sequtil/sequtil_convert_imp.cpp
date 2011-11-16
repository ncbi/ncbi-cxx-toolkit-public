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

#include <util/sequtil/sequtil_expt.hpp>
#include <util/range.hpp>

#include "sequtil_convert_imp.hpp"
#include "sequtil_shared.hpp"
#include "sequtil_tables.hpp"

#include <stdlib.h>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// Conversions

// NB: We try to use conversion tables wherever possible, minimizing bit 
// shifting or any other operation within the main conversion loop.

// All conversion functions takes the following parameters:
// src - input sequence
// pos - starting position in sequence coordinates
// length - number of residues to convert
// dst - an output container


SIZE_TYPE CSeqConvert_imp::Convert
(const char* src,
 TCoding src_coding,
 TSeqPos pos,
 TSeqPos length,
 char* dst,
 TCoding dst_coding)
{
    _ASSERT((dst != 0)  &&  (src != 0));
    _ASSERT(CSeqUtil::GetCodingType(src_coding) == 
            CSeqUtil::GetCodingType(dst_coding));

    if ( length == 0 ) {
        return 0;
    }

    // conversion from a coding to itself.
    if ( src_coding == dst_coding ) {
        return Subseq(src, src_coding, pos, length, dst);
    }
    
    // all other conversions
    switch ( src_coding ) {
        
    // --- NA conversions 
        
    // iupacna -> ...
    case CSeqUtil::e_Iupacna:
        switch ( dst_coding ) {
        case CSeqUtil::e_Ncbi2na:
            return x_ConvertIupacnaTo2na(src, pos, length, dst);
        case CSeqUtil::e_Ncbi2na_expand:
            return x_ConvertIupacnaTo2naExpand(src, pos, length, dst);
        case CSeqUtil::e_Ncbi4na:
            return x_ConvertIupacnaTo4na(src, pos, length, dst);
        case CSeqUtil::e_Ncbi4na_expand:
        case CSeqUtil::e_Ncbi8na:
            return x_ConvertIupacnaTo8na(src, pos, length, dst);
        default:
            break;
        }
        break;
    
    // ncbi2na -> ...
    case CSeqUtil::e_Ncbi2na:
        switch ( dst_coding ) {
        case CSeqUtil::e_Iupacna:
            return x_Convert2naToIupacna(src, pos, length, dst);
        case CSeqUtil::e_Ncbi2na_expand:
            return x_Convert2naTo2naExpand(src, pos, length, dst);
        case CSeqUtil::e_Ncbi4na:
            return x_Convert2naTo4na(src, pos, length, dst);
        case CSeqUtil::e_Ncbi4na_expand:
        case CSeqUtil::e_Ncbi8na:
            return x_Convert2naTo8na(src, pos, length, dst);
        default:
            break;
        }
        break;

    // ncbi2na_expand -> ...
    case CSeqUtil::e_Ncbi2na_expand:
        switch ( dst_coding ) {
        case CSeqUtil::e_Iupacna:
            return x_Convert2naExpandToIupacna(src, pos, length, dst);
        case CSeqUtil::e_Ncbi2na:
            return x_Convert2naExpandTo2na(src, pos, length, dst);
        case CSeqUtil::e_Ncbi4na:
            return x_Convert2naExpandTo4na(src, pos, length, dst);
        case CSeqUtil::e_Ncbi4na_expand:
        case CSeqUtil::e_Ncbi8na:
            return x_Convert2naExpandTo8na(src, pos, length, dst);
        default:
            break;
        }
        break;
        
    // ncbi4na -> ...
    case CSeqUtil::e_Ncbi4na:
        switch ( dst_coding ) {
        case CSeqUtil::e_Iupacna:
            return x_Convert4naToIupacna(src, pos, length, dst);
        case CSeqUtil::e_Ncbi2na:
            return x_Convert4naTo2na(src, pos, length, dst);
        case CSeqUtil::e_Ncbi2na_expand:
            return x_Convert4naTo2naExpand(src, pos, length, dst);
        case CSeqUtil::e_Ncbi4na_expand:
        case CSeqUtil::e_Ncbi8na:
            return x_Convert4naTo8na(src, pos, length, dst);
        default:
            break;
        }
        break;
        
    // ncbi8na / ncbi4na_expand -> ...
    case CSeqUtil::e_Ncbi8na:
    case CSeqUtil::e_Ncbi4na_expand:
        switch ( dst_coding ) {
        case CSeqUtil::e_Iupacna:
            return x_Convert8naToIupacna(src, pos, length, dst);
        case CSeqUtil::e_Ncbi2na:
            return x_Convert8naTo2na(src, pos, length, dst);
        case CSeqUtil::e_Ncbi2na_expand:
            return x_Convert8naTo2naExpand(src, pos, length, dst);
        case CSeqUtil::e_Ncbi4na:
            return x_Convert8naTo4na(src, pos, length, dst);
        case CSeqUtil::e_Ncbi8na:
        case CSeqUtil::e_Ncbi4na_expand:
            return Subseq(src, src_coding, pos, length, dst);
        default:
            break;
        }
        break;
        
    // --- AA conversions 
                        
    // NB: currently ncbi8aa is the same as ncbistdaa.
              
    // iupacaa -> ...
    case CSeqUtil::e_Iupacaa:
        switch ( dst_coding ) {
        case CSeqUtil::e_Ncbieaa:
            return x_ConvertIupacaaToEaa(src, pos, length, dst);
        case CSeqUtil::e_Ncbistdaa:
        case CSeqUtil::e_Ncbi8aa:
            return x_ConvertIupacaaToStdaa(src, pos, length, dst);
        default:
            break;
        }
        break;
        
    // ncbieaa -> ...
    case CSeqUtil::e_Ncbieaa:
        switch ( dst_coding ) {
        case CSeqUtil::e_Iupacaa:
            return x_ConvertEaaToIupacaa(src, pos, length, dst);
        case CSeqUtil::e_Ncbistdaa:
        case CSeqUtil::e_Ncbi8aa:
            return x_ConvertEaaToStdaa(src, pos, length, dst);
        default:
            break;
        }
        break;
        
    // ncbistdaa / ncbi8aa -> ...
    case CSeqUtil::e_Ncbi8aa:
    case CSeqUtil::e_Ncbistdaa:
        switch ( dst_coding ) {
        case CSeqUtil::e_Ncbieaa:
            return x_ConvertStdaaToEaa(src, pos, length, dst);
        case CSeqUtil::e_Iupacaa:
            return x_ConvertStdaaToIupacaa(src, pos, length, dst);
        case CSeqUtil::e_Ncbi8aa:
        case CSeqUtil::e_Ncbistdaa:
            return Subseq(src, src_coding, pos, length, dst);
        default:
            break;
        }
        break;

    default:
        break;
    }

    // We should never reach here
    NCBI_THROW(CSeqUtilException, eInvalidCoding, "Unknown conversion.");
}


// --- NA conversions:


// from IUPACna to ...
//===========================================================================

// IUPACna -> IUPACna
// This is not a simple copy since we handle conversion of lower to upper 
// case and conversion of 'U'('u') to 'T'

SIZE_TYPE CSeqConvert_imp::x_ConvertIupacnaToIupacna
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    // call the shared implementation for converting 1 byte to another byte
    // given a specific conversion table.
    // the iupacna to iupacna table converts upper and lower case to upper case
    // and U (u) to T
    return convert_1_to_1(src, pos, length, dst, 
                          CIupacnaToIupacna::GetTable());
}


// IUPACna -> NCBI2na
// convert 4 IUPACna characters into a single NCBI2na byte

SIZE_TYPE CSeqConvert_imp::x_ConvertIupacnaTo2na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    // The iupacna to ncbi2na table is constructed such that each row
    // correspond to an iupacna letter and each column corresponds to
    // that letter being in one of the 4 possible offsets within the 
    // ncbi2na byte
    const Uint1* table = CIupacnaTo2na::GetTable();
    
    const char* src_i = src + pos;
    for ( size_t count = length / 4; count; --count ) {
        *dst = 
            table[*src_i * 4          ] | 
            table[*(src_i + 1) * 4 + 1] |
            table[*(src_i + 2) * 4 + 2] |
            table[*(src_i + 3) * 4 + 3];
        src_i += 4;
        ++dst;
    }
    
    // Handle overhang
    if ( length % 4 != 0 ) {
        *dst = 0x0;
        for( size_t i = 0; i < (length % 4); ++i, ++src_i ) {
            *dst |= table[static_cast<Uint1>(*src_i) * 4 + i];
        }
    }
    
    return length;
}


// IUPACna -> NCBI2na_expand
// convert a single IUPACna character into a single NCBI2na_expand byte.

SIZE_TYPE CSeqConvert_imp::x_ConvertIupacnaTo2naExpand
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    // call the shared implementation for converting 1 byte to another byte
    // given a specific conversion table.
    // the iupacna to ncbi2na_expand table converts upper and lower case IUPACna
    // into a single ncbi2na_expand byte.
    return convert_1_to_1(src, pos, length, dst, 
                          CIupacnaTo2naExpand::GetTable());
}


// IUPACna -> NCBI4na
// convert 2 IUPACna characters into a single NCBI4na byte

SIZE_TYPE CSeqConvert_imp::x_ConvertIupacnaTo4na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{

    // The iupacna to ncbi4na table is constructed such that each row
    // correspond to an iupacna letter and each column corresponds to
    // that letter being in one of the 2 possible offsets within the 
    // ncbi4na byte
    const Uint1* table = CIupacnaTo4na::GetTable();
    
    const char* src_i = src + pos;
    
    for ( size_t count = length / 2; count; --count ) {
        *dst = table[*src_i * 2] | table[*(src_i + 1) * 2 + 1];
        src_i += 2;
        ++dst;
    }
    
    // handle overhang
    if ( length % 2 != 0 ) {
        *dst = table[static_cast<Uint1>(*src_i) * 2];
    }
    
    return length;
}


// IUPACna -> NCBI8na (NCBI4na_expand)
// convert a single IUPACna character into a single NCBI8na byte.

SIZE_TYPE CSeqConvert_imp::x_ConvertIupacnaTo8na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    // call the shared implementation for converting 1 byte to another byte
    // given a specific conversion table.
    // the iupacna to ncbi8na table converts upper and lower case IUPACna
    // into a single ncbi8na byte (which is the same as ncbi4na_expand)
    return convert_1_to_1(src, pos, length, dst, 
                          CIupacnaTo8na::GetTable());
}


// from NCBI2na to ...
//===========================================================================

// NCBI2na -> IUPACna
// convert a NCBI2na byte into 4 IUPACna characters.

SIZE_TYPE CSeqConvert_imp::x_Convert2naToIupacna
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    return convert_1_to_4(src, pos, length, dst, C2naToIupacna::GetTable());
}


// NCBI2na -> NCBI2na_expand
// convert a NCBI2na byte into 4 NCBI2na_expand characters.

SIZE_TYPE CSeqConvert_imp::x_Convert2naTo2naExpand
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    return convert_1_to_4(src, pos, length, dst, C2naTo2naExpand::GetTable());
}


// NCBI2na -> NCBI4na
// convert a NCBI2na byte into 2 NCBI4na bytes.

SIZE_TYPE CSeqConvert_imp::x_Convert2naTo4na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    const Uint1* table = C2naTo4na::GetTable(pos % 2 == 0);
    const Uint2* table2 = reinterpret_cast<const Uint2*>(table);

    const char* iter = src + (pos / 4);
    
    size_t size = length;
    // NB: branch within the inner loop as devastating consequences
    // on performance.
    
    // we handle two different cases, the first being offsets 0 and 2.
    // the second are offsets 1 and 3.
    // for offsets 0,2 we use a 2 column table, where the first column
    // corresponds to the lower 4 bits of the ncbi2na coding (entry 0)
    // and the second column corresponds to the upper 4 bits (entry 1).
    // in this case once we set the initial entry progress is the same.
    // the overhang for this case is either 0 or 1.
    // the 1\3 offset is a more complex one. for it we use a 3 column table.
    // the first column corresponds to the lower 2 bits of the ncbi2na 
    // coding, the second corresponds to the middle 4 bits and the third
    // correspond to the upper 2 bits.
    // we handle all cases as offset 3. for offset one we simply handle 
    // the first 4 bits, which will being us to offset 3.
    // as handling the middle 4 bits or the combination of the 2 lower
    // ones and 2 upper ones are done differently, we handle 4 letters (8 bits)
    // at a time, in oredr to prevent branching withing the inner loop.
    // overhang for this case is 1, 2 or 3.
    
    switch ( pos % 4 ) {
        // --- table entry size for offsets 0,2 is 2
    case 2:
        {{
            *dst = table[static_cast<Uint1>(*iter) * 2 + 1];
            if ( length == 1 ) {
                *dst &= 0xf0;
                return length;
            }
            size -= 2;
            ++iter;
            ++dst;
        }}

        // intentional fall through
    case 0:
        {{
            // "trick" the compiler so that each assignment will
            // be of 2 bytes.
            Uint2* dst2 = reinterpret_cast<Uint2*>(dst);
            for ( size_t i = size / 4; i; --i , ++dst2, ++iter ) {
                *dst2 = table2[static_cast<Uint1>(*iter)];
            }
            dst = reinterpret_cast<char*>(dst2);
        }}

        // handle overhang
        if ( (size % 4) != 0 ) {
            switch ( size % 4 ) {
            case 1:
                *dst = table[static_cast<Uint1>(*iter) * 2] & 0xf0;
                break;
            case 2:
                *dst = table[static_cast<Uint1>(*iter) * 2];
                break;
            case 3:
                *dst = table[static_cast<Uint1>(*iter) * 2];
                ++dst;
                *dst = table[static_cast<Uint1>(*iter) * 2 + 1] & 0xf0;
                break;
            }
        }
        break;
        
        // --- table entry size for offsets 1,3 is 3
    case 3:
        {{
            if ( length == 1 ) {
                *dst = table[static_cast<Uint1>(*iter) * 3 + 2];
                return length;
            } else {
                *dst = table[static_cast<Uint1>(*iter) * 3 + 2] |
                       table[static_cast<Uint1>(*(iter + 1)) * 3];
                ++dst;
                ++iter;
                size -= 2;
            }
        }}
        // intentional fall through
    case 1:
        {{
            for ( size_t i = size / 4; i; --i, ++iter ) {
                *dst = table[static_cast<Uint1>(*iter) * 3 + 1];
                ++dst;
                *dst = table[static_cast<Uint1>(*iter) * 3 + 2] |
                       table[static_cast<Uint1>(*(iter + 1)) * 3];
                ++dst;
            }
        }}

        // handle overhang
        if ( size % 4 != 0 ) {
            switch ( size % 4 ) {
            case 1:
                *dst = table[static_cast<Uint1>(*iter) * 3 + 1] & 0xF0;
                break;

            case 2:
                *dst = table[static_cast<Uint1>(*iter) * 3 + 1];
                break;

            case 3:
                *dst = table[static_cast<Uint1>(*iter) * 3 + 1];
                ++dst;
                *dst = table[static_cast<Uint1>(*iter) * 3 + 2];
                break;
            }
        }
        break;
    } // end of switch ( offset )
    
    return length;
}
    

// NCBI2na -> NCBI8na (NCBI4na_expand)
// convert a ncbi2na byte into 4 ncbi4na bytes.

SIZE_TYPE CSeqConvert_imp::x_Convert2naTo8na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    return convert_1_to_4(src, pos, length, dst, C2naTo8na::GetTable());
}


// from NCBI2na_expand to ...
//===========================================================================

// NCBI2na_expand -> IUPACna
// convert a single NCBI2na_expand byte into a single IUPACna byte.

SIZE_TYPE CSeqConvert_imp::x_Convert2naExpandToIupacna
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    return convert_1_to_1(src, pos, length, dst, 
                          C2naExpandToIupacna::GetTable());
}


// NCBI2na_expand -> NCBI2na
// convert 4 NCBI2na_expand bytes to a single NCBI2na one.

SIZE_TYPE CSeqConvert_imp::x_Convert2naExpandTo2na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    const char* iter = src + pos;
    
    // main loop. pack 4 ncbi2na_expand bytes into a single bye and add it
    // to the output container
    for ( size_t i = length / 4; i; --i, ++dst ) {
        *dst = (*iter << 6) | (*(iter + 1) << 4) | 
               (*(iter + 2) << 2) | (*(iter + 3));
        iter += 4;
    }

    switch ( length % 4 ) {
    case 1:
        *dst = (*iter << 6);
        break;
    case 2:
        *dst = (*iter << 6) | (*(iter + 1) << 4);
        break;
    case 3:
        *dst = (*iter << 6) | (*(iter + 1) << 4) | (*(iter + 2) << 2);
        break;
    }
    
    return length;
}


// NCBI2na_expand -> NCBI4na
// convert 2 NCBI2na_expand bytes into a single NCBI4na byte.

SIZE_TYPE CSeqConvert_imp::x_Convert2naExpandTo4na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    // A simple conversion table that converts a ncbi2na_expand byte
    // into a "half" ncbi4na byte, based on the position of the 
    // ncbi2na_expand byte within the ncbi4na byte
    // positions 0 and 1 corresponds to the lower and upper 4 bits in 
    // a ncbi4na byte respectively.
    static Uint1 table[8] = {
      //  0     1
        0x10, 0x01,  // A 
        0x20, 0x02,  // C
        0x40, 0x04,  // G
        0x80, 0x08   // T
    };

    const char* iter = src + pos;

    for ( size_t i = length / 2; i; --i, ++dst ) {
        *dst = table[*iter * 2] | table[*(iter + 1) * 2 + 1];
        iter += 2;
    }
    
    if ( length % 2 != 0 ) { // == 1
        *dst = table[*iter * 2];
    }

    return length;
}


// NCBI2na_expand -> NCBI8na (NCBI4na_expand)
// convert a single NCBI2na_expand byte into a single NCBI8na one.

SIZE_TYPE CSeqConvert_imp::x_Convert2naExpandTo8na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    // simple
    static Uint1 table[4] = {
        0x01,  // A  0 -> 1
        0x02,  // C  1 -> 2
        0x04,  // G  2 -> 4
        0x08   // T  3 -> 8
    };
    
    return convert_1_to_1(src, pos, length, dst, table);
}


// from NCBI4na to ...
//===========================================================================


// NCBI4na -> IUPACna
// convert a NCBI4na byte into 2 IUPACna characters.

SIZE_TYPE CSeqConvert_imp::x_Convert4naToIupacna
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    return convert_1_to_2(src, pos, length, dst, C4naToIupacna::GetTable());
}

// NCBI4na -> NCBI2na
// convert 2 NCBI4na bytes into a NCBI2na byte.

SIZE_TYPE CSeqConvert_imp::x_Convert4naTo2na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    Uint1 offset = pos % 2;
    const Uint1* table = C4naTo2na::GetTable(offset);
    
    size_t overhang = length % 4;

    const char* iter = src + (pos / 2);
    
    switch ( offset ) {
    case 0:
        // aligned copy
        {{
            for ( size_t i = length / 4; i; --i, ++dst ) {
                *dst = 
                    table[static_cast<Uint1>(*iter) * 2] |
                    table[static_cast<Uint1>(*(iter + 1)) * 2 + 1];
                iter += 2;
            }
            
            // handle overhang
            if ( overhang != 0 ) {
                switch ( overhang ) {
                case 1:
                    // leave just the 2 lower bits
                    *dst = (table[static_cast<Uint1>(*iter) * 2]) & 0xC0;
                    break;
                case 2:
                    // leave just the 4 lower bits
                    *dst = (table[static_cast<Uint1>(*iter) * 2]) & 0xF0;
                    break;
                case 3:
                    *dst = 
                        table[static_cast<Uint1>(*iter) * 2]           |
                        table[static_cast<Uint1>(*(iter + 1)) * 2 + 1] &
                        0xFC;
                    break;
                }
            }
        }}
        break;
        
    case 1:
        // unaligned copy
        {{
            for ( size_t i = length / 4; i; --i, ++dst ) {
                *dst  = 
                    table[static_cast<Uint1>(*iter) * 3]             |
                    table[static_cast<Uint1>(*(iter + 1)) * 3 + 1]   |
                    table[static_cast<Uint1>(*(iter + 2)) * 3 + 2];
                iter += 2;
            }
            
            // handle overhang
            if ( overhang != 0 ) {
                switch ( overhang ) {
                case 1:
                    *dst = table[static_cast<Uint1>(*iter) * 3] & 0xC0;
                    break;
                case 2:
                    *dst = 
                        table[static_cast<Uint1>(*iter) * 3]           |
                        table[static_cast<Uint1>(*(iter + 1)) * 3 + 1] &
                        0xF0;
                    break;
                case 3:
                    *dst = 
                        table[static_cast<Uint1>(*iter) * 3]           |
                        table[static_cast<Uint1>(*(iter + 1)) * 3 + 1] &
                        0xFC;
                    break;
                }
            }
        }}
        break;
    }
    
    return length;
}

// NCBI4na -> NCBI2na_expand
// convert a NCBI4na byte into 2 NCBI2na_expand bytes.

SIZE_TYPE CSeqConvert_imp::x_Convert4naTo2naExpand
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    return convert_1_to_2(src, pos, length, dst, 
                          C4naTo2naExpand::GetTable());
}


// NCBI4na -> NCBI8na (NCBI4na_expand)
// convert a NCBI2na byte into 4 IUPACna characters.

SIZE_TYPE CSeqConvert_imp::x_Convert4naTo8na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    return convert_1_to_2(src, pos, length, dst, C4naTo8na::GetTable());
}


// from NCBI8na (NCBI4na_expand) to ...
//===========================================================================

// NCBI8na -> IUPACna
// convert a single NCBI8na byte into a single IUPACna byte.

SIZE_TYPE CSeqConvert_imp::x_Convert8naToIupacna
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    return convert_1_to_1(src, pos, length, dst, C8naToIupacna::GetTable());
}


// NCBI8na -> NCBI2na
// convert 4 NCBI8na bytes into a single NCBI2na byte.

SIZE_TYPE CSeqConvert_imp::x_Convert8naTo2na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    const Uint1* table = C8naTo2na::GetTable();
    
    const char* iter = src + pos;
    
    for ( size_t i = length / 4; i; --i, ++dst ) {
        *dst = table[static_cast<Uint1>(*iter) * 4] |
            table[static_cast<Uint1>(*(iter + 1)) * 4 + 1] |
            table[static_cast<Uint1>(*(iter + 2)) * 4 + 2] |
            table[static_cast<Uint1>(*(iter + 3)) * 4 + 3];
        iter += 4;
    }
    
    // Handle overhang
    if ( (length % 4) != 0 ) {
        *dst = 0;
        for( size_t i = 0; i < (length % 4); ++i, ++iter ) {
            *dst |= table[static_cast<Uint1>(*iter) * 4 + i];
        }
    }
    
    return length;
}


// NCBI8na -> NCBI2na_expand
// convert a single NCBI8na byte into a single NCBI2na_expand byte.

SIZE_TYPE CSeqConvert_imp::x_Convert8naTo2naExpand
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    // simple conversion table
    static const Uint1 table[16] = {
        0x03,    // gap -> T
        0x00,    // A -> A
        0x01,    // C -> C
        0x01,    // M -> C
        0x02,    // G -> G
        0x02,    // R -> G
        0x01,    // S -> C
        0x00,    // V -> A
        0x03,    // T -> T
        0x03,    // W -> T
        0x03,    // Y -> T
        0x00,    // H -> A
        0x02,    // K -> G
        0x02,    // D -> G
        0x01,    // B -> C
        0x00     // N -> A
    };
    
    return convert_1_to_1(src, pos, length, dst, table);
}


// NCBI8na -> NCBI4na
// convert 2 NCBI8na bytes into  a single NCBI4na byte.

SIZE_TYPE CSeqConvert_imp::x_Convert8naTo4na
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    const char* iter = src + pos;

    for ( size_t i = length / 2; i; --i, ++dst ) {
        *dst = (*iter << 4) | (*(iter + 1));
        iter += 2;
    }

    if ( (length % 2) != 0 ) {
        *dst = (*iter << 4) & 0xf0;
    }
    
    return length;
}


// AA conversions:

// All AA conversions ara a 1 to 1 conversion

// from IUPACaa to ...
//===========================================================================

// IUPACaa -> NCBIeaa

SIZE_TYPE CSeqConvert_imp::x_ConvertIupacaaToEaa
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    // they map directly
    return Subseq(src, CSeqUtil::e_Iupacaa, pos, length, dst);
}


// IUPACaa -> NCBIstdaa (NCBI8aa)

SIZE_TYPE CSeqConvert_imp::x_ConvertIupacaaToStdaa
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    return convert_1_to_1(src, pos, length, dst, CIupacaaToStdaa::GetTable());
}


// from NCBIeaa to ...
//===========================================================================

// NCBIeaa -> IUPACaa

SIZE_TYPE CSeqConvert_imp::x_ConvertEaaToIupacaa
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    return convert_1_to_1(src, pos, length, dst, CEaaToIupacaa::GetTable());
}


// NCBIeaa -> NCBIstdaa (NCBI8aa)

SIZE_TYPE CSeqConvert_imp::x_ConvertEaaToStdaa
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    return convert_1_to_1(src, pos, length, dst, CEaaToStdaa::GetTable());
}


// from NCBIstdaa (NCBI8aa) to ...
//===========================================================================

// NCBIstdaa (NCBI8aa) -> IUPACaa

SIZE_TYPE CSeqConvert_imp::x_ConvertStdaaToIupacaa
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    return convert_1_to_1(src, pos, length, dst, CStdaaToIupacaa::GetTable());
}

// NCBIstdaa (NCBI8aa) -> NCBIeaa

SIZE_TYPE CSeqConvert_imp::x_ConvertStdaaToEaa
(const char* src,
 TSeqPos pos,
 TSeqPos length,
 char *dst)
{
    return convert_1_to_1(src, pos, length, dst, CStdaaToEaa::GetTable());
}


/////////////////////////////////////////////////////////////////////////////
//
// Subseq - partial sequence

SIZE_TYPE CSeqConvert_imp::Subseq
(const char* src,
 TCoding coding,
 TSeqPos pos,
 TSeqPos length,
 char* dst)
{
    SIZE_TYPE converted = 0;
    char *buf = 0;
    try {
        switch ( coding ) {
            
        // for packed coding (2na and 4na) first expand, then re-pack
        case CSeqUtil::e_Ncbi2na:
            {{
                buf = new char[length];
                x_Convert2naTo2naExpand(src, pos, length, buf);
                converted = x_Convert2naExpandTo2na(buf, 0, length, dst);
                delete[] buf;
                buf = 0;
            }}
            break;
            
        case CSeqUtil::e_Ncbi4na:
            {{
                buf = new char[length];
                x_Convert4naTo8na(src, pos, length, buf);
                converted = x_Convert8naTo4na(buf, 0, length, dst);
                delete[] buf;
                buf = 0;
            }}
            break;
            
        // iupacna may contain 'U' that needs to be converted to 'T'
        case CSeqUtil::e_Iupacna:
            {{
                converted = convert_1_to_1(src, pos, length, dst, 
                    CIupacnaToIupacna::GetTable());
            }}
            break;
            
        // for other ascii codings make sure the output is upper case
        case CSeqUtil::e_Iupacaa:
        case CSeqUtil::e_Ncbieaa:
            {{
                const char* iter = src + pos;
                const char* end  = iter + length;
                
                for ( ; iter != end; ++iter, ++dst ) {
                    *dst = toupper((unsigned char)(*iter));
                }
                converted = length;
            }}
            break;
            
        // for the rest of the codings (e.g. 8na, stdaa) copy
        // the desired range.
        default:
            {{
                copy(src + pos, src + pos + length, dst);
                converted = length;
            }}
            break;
        } // end of switch statement
    } catch ( ... ) {
        if ( buf != 0 ) {
            delete[] buf;
        }
        throw;
    }

    return converted;
}


/////////////////////////////////////////////////////////////////////////////
//
// Packing

SIZE_TYPE CSeqConvert_imp::Pack
(const char* src,
 TSeqPos length,
 TCoding src_coding,
 char* dst,
 TCoding& dst_coding)
{
    dst_coding = x_HasAmbig(src, src_coding, length) ?
        CSeqUtil::e_Ncbi4na : CSeqUtil::e_Ncbi2na;
    
    return Convert(src, src_coding, 0, length, dst, dst_coding);
}

SIZE_TYPE CSeqConvert_imp::Pack(const char* src, TSeqPos length,
                                TCoding src_coding, IPackTarget& dst)
{
    if (length == 0) {
        return 0;
    }
    bool gaps_ok = dst.GapsOK(CSeqUtil::GetCodingType(src_coding));
    const SBestCodings& best_codings = (gaps_ok ? kBestCodingsWithGaps
                                        : kBestCodingsWithoutGaps);
    const TCoding* best_coding = NULL;
    switch (src_coding) {
    case CSeqUtil::e_Iupacna:
        best_coding = best_codings.iupacna;
        break;

    case CSeqUtil::e_Ncbi2na_expand:
        return Convert(src, src_coding, 0, length,
                       dst.NewSegment(CSeqUtil::e_Ncbi2na, length),
                       CSeqUtil::e_Ncbi2na);

    case CSeqUtil::e_Ncbi4na:
        best_coding = best_codings.ncbi4na;
        break;

    case CSeqUtil::e_Ncbi4na_expand:
    case CSeqUtil::e_Ncbi8na:
        best_coding = best_codings.ncbi8na;
        break;

    case CSeqUtil::e_Iupacaa:
    case CSeqUtil::e_Ncbieaa:
        if (gaps_ok) { // no point otherwise
            best_coding = best_codings.ncbieaa;
        }
        break;

    case CSeqUtil::e_Ncbi8aa:
    case CSeqUtil::e_Ncbistdaa:
        if (gaps_ok) {
            best_coding = best_codings.ncbi8aa;
        }
        break;

    default:
        break;
    }

    if (best_coding != NULL) {
        return CPacker(src_coding, best_coding, gaps_ok, dst)
            .Pack(src, length);
    } else {
        memcpy(dst.NewSegment(src_coding, length), src,
               GetBytesNeeded(src_coding, length));
        return length;
    }    
}

const CSeqUtil::ECoding CSeqConvert_imp::CPacker::kNoCoding
    = CSeqUtil::e_Ncbi2na_expand; // never used by best_coding

SIZE_TYPE CSeqConvert_imp::CPacker::Pack(const char* src, TSeqPos length)
{
    const char* src_end   = src + GetBytesNeeded(m_SrcCoding, length);
    TCoding     prev_type = kNoCoding;

    for (const char* p = src;  p < src_end;  ++p) {
        unsigned char residue;
        TCoding       curr_type;
        do {
            residue = static_cast<unsigned char>(*p);
            curr_type = m_BestCoding[residue];
        } while (curr_type == prev_type  &&  ++p < src_end);

        if (curr_type == CSeqUtil::e_Ncbi4na_expand) {
            TCoding type1 = m_BestCoding[(residue >> 4)   * 0x11],
                    type2 = m_BestCoding[(residue & 0x0F) * 0x11];
            if (type1 != prev_type) {
                x_AddBoundary((p - src) * 2, type1);
            }
            x_AddBoundary((p - src) * 2 + 1, type2);
            prev_type = type2;
        } else if (p != src_end) {
            _ASSERT(curr_type != kNoCoding);
            x_AddBoundary((p - src) * m_SrcDensity, curr_type);
            prev_type = curr_type;
        }
    }
    x_AddBoundary(length, kNoCoding);

    _ASSERT(m_Boundaries.at(0) == 0);

    SArrangement* best_arrangement
        = (m_EndingNarrow.cost < m_EndingWide.cost
           ? &m_EndingNarrow : &m_EndingWide);

    // XXX - fine-tune 4na/2na boundaries to minimize wastage?

    size_t n = best_arrangement->codings.size();
    _ASSERT(n == m_Boundaries.size() - 1);

    SIZE_TYPE result = 0;
    for (size_t i = 0;  i < n;  ++i) {
        TCoding coding  = best_arrangement->codings[i];
        TSeqPos start   = m_Boundaries[i];
        while (i < n - 1  &&  best_arrangement->codings[i + 1] == coding) {
            ++i; // merge when possible
        }

        TSeqPos length  = m_Boundaries[i + 1] - start;
        char *  segment = m_Target.NewSegment(coding, length);
        if (coding == CSeqUtil::e_not_set) { // gap
            _ASSERT(m_GapsOK);
            result += length;
        } else {
            result += CSeqConvert::Convert(src, m_SrcCoding, start, length,
                                           segment, coding);
        }
    }

    return result;
}

void CSeqConvert_imp::CPacker::x_AddBoundary(TSeqPos pos, TCoding new_coding)
{
    if (m_Boundaries.empty()) {
        _ASSERT(pos == 0);
        m_Boundaries.push_back(pos);
        m_EndingNarrow.codings.push_back(new_coding);
        m_EndingWide.codings.push_back(m_WideCoding);
        m_EndingWide.cost   = m_Target.GetOverhead(m_WideCoding);
        m_EndingNarrow.cost = m_Target.GetOverhead(new_coding);
        return;
    }

    TSeqPos last_length = pos - m_Boundaries.back();
    _ASSERT(last_length > 0);
    m_Boundaries.push_back(pos);

    TCoding last_narrow = m_EndingNarrow.codings.back();
    // This always rounds up to full bytes, and as such can slightly
    // overestimate the total cost of ncbi4na.
    m_EndingNarrow.cost += GetBytesNeeded(last_narrow,  last_length);
    m_EndingWide.cost   += GetBytesNeeded(m_WideCoding, last_length);
    if (last_narrow == m_WideCoding) {
        _ASSERT(m_EndingNarrow.cost == m_EndingWide.cost);
    }

    // _TRACE(static_cast<char>('@' + last_narrow) << last_length << ": ("
    //        << m_EndingNarrow.cost << ", " << m_EndingWide.cost << ')');

    _ASSERT(new_coding != last_narrow);
    if (new_coding == kNoCoding) {
        return;
    }

    if (new_coding != m_WideCoding
        &&  m_EndingNarrow.cost > m_EndingWide.cost) {
        m_EndingNarrow = m_EndingWide;
        // _TRACE("N <- W before N'");
    }

    SIZE_TYPE alt_wide_cost
        = m_EndingNarrow.cost + m_Target.GetOverhead(m_WideCoding);
    m_EndingNarrow.cost += m_Target.GetOverhead(new_coding);

    if (m_EndingWide.cost > alt_wide_cost) {
        m_EndingWide = m_EndingNarrow;
        m_EndingWide.cost = alt_wide_cost;
        // _TRACE("W <- N");
    } else if (new_coding == m_WideCoding) {
        m_EndingNarrow = m_EndingWide;
        // _TRACE("N <- W before W");
    }
    // _TRACE('(' << m_EndingNarrow.cost << ", " << m_EndingWide.cost << ')');

    m_EndingNarrow.codings.push_back(new_coding);
    m_EndingWide.codings.push_back(m_WideCoding);
    _ASSERT(m_EndingNarrow.codings.size() == m_Boundaries.size());
    _ASSERT(m_EndingWide.codings.size() == m_Boundaries.size());    
}

CSeqUtil::ECoding
CSeqConvert_imp::CPacker::x_GetWideCoding(const TCoding coding)
{
    switch (coding) {
    case CSeqUtil::e_Iupacna:
    case CSeqUtil::e_Ncbi4na_expand:
    case CSeqUtil::e_Ncbi8na: // ignore values >= 16 for now
        return CSeqUtil::e_Ncbi4na;

    case CSeqUtil::e_Ncbi2na_expand:
        return CSeqUtil::e_Ncbi2na;

    default:
        return coding;
    }
}


bool CSeqConvert_imp::x_HasAmbig
(const char* src,
 TCoding src_coding,
 size_t length)
{
    if ( length == 0 ) {
        return false;
    }
    
    switch ( src_coding ) {
    case CSeqUtil::e_Iupacna:
        return x_HasAmbigIupacna(src, length);
        
    case CSeqUtil::e_Ncbi4na:
        return x_HasAmbigNcbi4na(src, length);
        
    case CSeqUtil::e_Ncbi4na_expand:
    case CSeqUtil::e_Ncbi8na:
        return x_HasAmbigNcbi8na(src, length);
        
    case CSeqUtil::e_Ncbi2na:
    case CSeqUtil::e_Ncbi2na_expand:
        return false;

    default:
        break;
    }
    
    return false;
}


bool CSeqConvert_imp::x_HasAmbigIupacna(const char* src, size_t length)
{
    const bool *not_ambig = CIupacnaAmbig::GetTable();
    
    const char* end = src + length;
    
    const char* iter = src;
    while ( (iter != end)  &&  (not_ambig[static_cast<Uint1>(*iter)]) ) { 
          ++iter;
    }

    return iter != end;
}


bool CSeqConvert_imp::x_HasAmbigNcbi4na(const char* src, size_t length)
{
    const bool* not_ambig = CNcbi4naAmbig::GetTable();
    
    const char* end = src + (length / 2);
    
    const char* iter = src;
    while ( (iter != end)  &&  (not_ambig[static_cast<Uint1>(*iter)]) ) {
          ++iter;
    }
    
    if ( (iter == end)  &&  (length % 2) != 0 ) {
        return not_ambig[static_cast<Uint1>(*iter | 1) & 0xF1];
    }
    return iter != end;
}


bool CSeqConvert_imp::x_HasAmbigNcbi8na(const char* src, size_t length)
{
    const bool *not_ambig = CNcbi8naAmbig::GetTable();
    
    const char* end = src + length;
    
    const char* iter = src;
    while ( (iter != end)  &&  (not_ambig[static_cast<Uint1>(*iter)]) ) {
          ++iter;
    }
    
    return iter != end;
}


END_NCBI_SCOPE
