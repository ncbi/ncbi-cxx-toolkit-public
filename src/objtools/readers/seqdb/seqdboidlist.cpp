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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdboidlist.cpp
/// Implementation for the CSeqDBOIDList class, an array of bits
/// describing a subset of the virtual oid space.

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include "seqdboidlist.hpp"
#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

CSeqDBOIDList::CSeqDBOIDList(CSeqDBAtlas        & atlas,
                             const CSeqDBVolSet & volset,
                             CSeqDBLockHold     & locked)
    : m_Atlas   (atlas),
      m_Lease   (atlas),
      m_NumOIDs (0),
      m_Bits    (0),
      m_BitEnd  (0),
      m_BitOwner(false)
{
    _ASSERT( volset.HasFilter() );
    
    if (volset.HasSimpleMask()) {
        x_Setup( volset.GetSimpleMask(), locked );
    } else {
        x_Setup( volset, locked );
    }
}

CSeqDBOIDList::~CSeqDBOIDList()
{
    CSeqDBLockHold locked(m_Atlas);

    if (m_BitOwner) {
        _ASSERT(m_Bits != 0);
        m_Atlas.Free((const char *) m_Bits, locked);
    }
}

void CSeqDBOIDList::x_Setup(const string   & filename,
                            CSeqDBLockHold & locked)
{
    CSeqDBAtlas::TIndx file_length = 0;
    
    m_Atlas.GetFile(m_Lease, filename, file_length, locked);
    
    m_NumOIDs = SeqDB_GetStdOrd((int *) m_Lease.GetPtr(0)) + 1;
    m_Bits    = (unsigned char*) m_Lease.GetPtr(sizeof(Uint4));
    m_BitEnd  = m_Bits + file_length - sizeof(Uint4);
}


// The general rule I am following in these methods is to use byte
// computations except during actual looping.

void CSeqDBOIDList::x_Setup(const CSeqDBVolSet & volset,
                            CSeqDBLockHold     & locked)
{
    _ASSERT(volset.HasFilter() && (! volset.HasSimpleMask()));
    
    // First, get the memory space and clear it.
    
    // Pad memory space to word boundary, add 8 bytes for "insurance".  Some
    // of the algorithms here need to do bit shifting and OR half of a source
    // element into this destination element, and the other half into this
    // other destination element.  Rather than sprinkle this code with range
    // checks, padding is used.
    
    int num_oids = volset.GetNumOIDs();
    int byte_length = ((num_oids + 31) / 32) * 4 + 8;
    
    m_Bits   = (TUC*) m_Atlas.Alloc(byte_length, locked);
    m_BitEnd = m_Bits + byte_length;
    m_BitOwner = true;
    
    try {
        memset((void*) m_Bits, 0, byte_length);
        
        // Then get the list of filenames and offsets to overlay onto it.
        
        for(int i = 0; i < volset.GetNumVols(); i++) {
            if (volset.GetIncludeAllOIDs(i)) {
                const CSeqDBVolEntry * ve = volset.GetVolEntry(i);
                x_SetBitRange(ve->OIDStart(), ve->OIDEnd());
                continue;
            }
            
            ITERATE(CSeqDBVolSet::TFilters, it, volset.GetFilterSet(i)) {
                const CSeqDBVolEntry * v1 = volset.GetVolEntry(i);
                CRef<CSeqDBVolFilter> v2(*it);
                x_ApplyFilter(v2, v1, locked);
            }
        }
    }
    catch(...) {
        if (m_Bits) {
            m_Atlas.Free((const char*) m_Bits, locked);
        }
        throw;
    }
    
    m_NumOIDs = num_oids;
    
    while(m_NumOIDs && (! x_IsSet(m_NumOIDs - 1))) {
        -- m_NumOIDs;
    }
}

void CSeqDBOIDList::x_ApplyFilter(CRef<CSeqDBVolFilter>   filter,
                                  const CSeqDBVolEntry  * vol,
                                  CSeqDBLockHold        & locked)
{
    // Volume-relative OIDs
    
    int start_vr = filter->BeginOID();
    int end_vr   = filter->EndOID();
    
    // Global OIDs
    
    int vol_start = vol->OIDStart();
    int vol_end   = vol->OIDEnd();
    
    // Adjust oids from volume to global, avoiding overflow, and
    // trimming ranges.
    
    
    // Global OIDs
    
    int start_g(0);
    int end_g(0);
    
    if (end_vr < (vol_end - vol_start)) {
        end_g = end_vr + vol_start;
    } else {
        end_g = vol_end;
    }
    
    if (start_vr >= (end_g - vol_start)) {
        // Specified starting oid is past end of volume.
        return;
    }
    
    start_g = start_vr + vol_start;
    
    if (! filter->GetOIDMask().empty()) {
        x_OrMaskBits(filter->GetOIDMask(), vol_start, start_g, end_g, locked);
    } else if (! filter->GetGIList().empty()) {
        x_OrGiFileBits(filter->GetGIList(),
                       vol->Vol(),
                       vol->OIDStart(),
                       vol->OIDEnd(),
                       start_g,
                       end_g,
                       locked);
    } else {
        x_SetBitRange(start_g, end_g);
    }
}

// This does not implement OID ranges yet.  To do so, it will be
// necessary to modify the loops below, which are complex enough that
// this should be done carefully.
//
// For each loop:
//
// 1. Modify starting condition (i.e. adjust "bitmapped" part) to skip
//    to first "input" character / word.
//
// 2. Blank out or ignore the portion of that data that are before the
//    input range.
//
// 3. If the entire OID range is encompassed in that element, blank
//    out the end of it too; OR the piece of data into the array as
//    shown below - and we're done.
//
// 4. Otherwise, OR it in, then iterate over the "middle" sections as
//    normal.
//
// 5. When the end of the data area is reached, do the same partial
//    element blanking as above, with the last element.

// A. Possibly rewrite this in a more readable way.

// a1. Iterate bitwise over the input array, setting particular bits
//     in the output array.
//
// a2. If data is aligned, skip to the fast mechanism?
//
// B. Another rewrite:
//
// 1. Write a generic function for ORring a range of bits from one
//    array to another.
//
// 2. That function would use two or three cases as shown below, to
//    accomplish the task for varying degrees of alignment.
// 
// 3. Possibly:
//
//    1. One case for bit arrays both aligned on 32 bit boundaries.
//    2. Another case for bit arrays aligned on 8 bit boundaries.
//    3. Another case for bit arrays not aligned.
//    4. Another case for bit arrays with "partial" start/end.
//
//   .. Later cases would probably devolve to earlier ones internally?

void CSeqDBOIDList::x_OrMaskBits(const string   & mask_fname,
                                 int              vol_start,
                                 int              oid_start,
                                 int              oid_end,
                                 CSeqDBLockHold & locked)
{
    _ASSERT(oid_end > oid_start);
    
    m_Atlas.Lock(locked);
    
    // Open file and get pointers
    
    TCUC* bitmap = 0;
    TCUC* bitend = 0;
    
    CSeqDBRawFile volmask(m_Atlas);
    CSeqDBMemLease lease(m_Atlas);
    
    Uint4 num_oids = 0;
    
    {
        volmask.Open(mask_fname, locked);
        
        volmask.ReadSwapped(lease, 0, & num_oids, locked);
        
        // This is the index of the last oid, not the count of oids...
        num_oids++;
        
        size_t file_length = (size_t) volmask.GetFileLength();
        
        // Cast forces signed/unsigned conversion.
        
        volmask.GetRegion(lease, sizeof(Int4), file_length, locked);
        bitmap = (TCUC*) lease.GetPtr(sizeof(Int4));
        
        //bitend = bitmap + file_length - sizeof(Int4);
        bitend = bitmap + (((num_oids + 31) / 32) * 4);
    }
    
    bool range_filter =
        ((vol_start != oid_start) || ((oid_end - oid_start) < num_oids));
    
    // Fold bitmap/bitend into m_Bits/m_BitEnd at bit offset oid_start.
    
    if ((! range_filter) && (0 == (vol_start & 31))) {
        // If the new data is "word aligned", we can use a fast algorithm.
        
        TCUC * srcp = bitmap;
        TUC  * locp = m_Bits + (vol_start / 8);
        TUC  * endp = locp + (bitend-bitmap);
        
        _ASSERT(endp <= m_BitEnd);
        
        Uint4 * wsrcp = (Uint4*) srcp;
        Uint4 * wlocp = (Uint4*) locp;
        Uint4 * wendp = wlocp + ((bitend - bitmap) / 4);
        
        while(wlocp < wendp) {
            *wlocp++ |= *wsrcp++;
        }
        
        srcp = (TCUC*) wsrcp;
        locp = (unsigned char*) wlocp;
        
        while(locp < endp) {
            *locp++ |= *(srcp++);
        }
    } else if ((! range_filter) && (0 == (vol_start & 7))) {
        // If the new data is "byte aligned", we can use a less fast algorithm.
        
        TCUC * srcp = bitmap;
        TUC  * locp = m_Bits + (vol_start / 8);
        TUC  * endp = locp + (bitend-bitmap);
        
        _ASSERT(endp <= m_BitEnd);
        
        while(locp < endp) {
            *locp++ |= *srcp++;
        }
    } else {
        // Otherwise... we have to use a slower, byte splicing
        // algorithm.  All range filtered cases go here - less
        // modified code means less testing, and OID ranges are
        // usually small, so the performance issue is probably not
        // significant.
        
        Uint4 Rshift = vol_start & 7;
        Uint4 Lshift = 8 - Rshift;
        
        TCUC * srcp = bitmap;
        TUC  * locp = m_Bits + (vol_start / 8);
        TUC  * endp = locp + (bitend-bitmap);
        
        _ASSERT(endp <= m_BitEnd);
        
        // This loop iterates over the source bytes.  Each byte is
        // split over two destination bytes.
        
        bool trimmed_last_byte = false;
        
        if (oid_start != vol_start) {
            Uint4 skip_oids = (oid_start - vol_start);
            
            srcp += skip_oids / 8;
            locp += skip_oids / 8;
            
            // Store (part of) left half of source char in one
            // location.
            
            Uint4 mask = 255 >> (skip_oids & 7);
            
            // Special case: begin and end fall in same block
            
            if ((oid_end/8) == (oid_start/8)) {
                // Another intuitive formula.
                
                // oid_end:  first disincluded oid after the included range.
                // (oid_end % 8):  disincluded oids in last examined byte.
                
                Uint4 oids_in_last_byte = (oid_end % 8);
                
                Uint4 last_byte_mask =
                    (255 & (255 << (8 - oids_in_last_byte)));
                
                mask &= last_byte_mask;
                
                trimmed_last_byte = true;
            }
            
            TCUC source = (*srcp) & mask;
            
            *locp |= (source >> Rshift);
            locp++;
            
            // Store right half of source in the next location.
            *locp |= (source << Lshift);
            srcp++;
        }
        
        if (! trimmed_last_byte) {
            if ((oid_end - oid_start) < num_oids) {
                Uint4 oids_in_last_byte = (oid_end % 8);
                
                Uint4 last_byte_mask =
                    (255 & (255 << (8 - oids_in_last_byte)));
                
                // Find location of last source byte.
                TCUC * last_srcp = bitmap + (oid_end / 8);
                
                // Adjust endp, trimming last byte.
                endp = m_Bits + (oid_end / 8);
                
                if (oid_end & 7) {
                    TCUC source = (*last_srcp) & last_byte_mask;
                    
                    TUC * locp_end = m_Bits + (oid_end / 8);
                    *locp_end |= (source >> Rshift);
                    locp_end++;
                    
                    // Store right half of source in the next location.
                    *locp_end |= (source << Lshift);
                }
            }
            
            while(locp < endp) {
                // Store left half of source char in one location.
                TCUC source = *srcp;
                *locp |= (source >> Rshift);
                locp++;
                
                // Store right half of source in the next location.
                *locp |= (source << Lshift);
                srcp++;
            }
            
            _ASSERT(locp == endp);
        }
    }
    
    m_Atlas.RetRegion(lease);
}

// This reads through the list of gis and turns on the corresponding
// oids.

// This is inefficient (and incorrect) for a number of reasons:

// 1. All processing for the GI list is done for each volume.  Three
// volumes, three times the cost.  There should be an object that
// manages these translations.  It would be like vol-set but for gis,
// oids, and gi lists.

// 2. Converting the set of GIs to OIDS is unacceptably slow, even if
// it were only done once.  This is partially due to the various
// performance inadequacies of the isam code, and partially due to the
// inefficiency of using it in this way.  It should be set up to
// accept a list of GIs, in sorted order.  It would search down to
// find the block containing the first object, and scan that block to
// find the object, but it would continue to scan that block for any
// other gis in the range.  It would report that it had translated N
// gis and produced M oids.  It might continue the loop internally.

// 3. The volume distribution code is not done.  So attaching a gilist
// to the "top" of SeqDB is impossible at this point.

// 4. To do #3, this code should be factored out into several discrete
// smaller functions.  One of them would read and swap the GI list.
// The next would convert this list into OIDs for a given volume.
// Then there needs to be the "gilist manager" code that can insure
// that the previously mentioned computation is only done once for a
// given gilist+oidlist.

// 5. ON THE OTHER HAND: How long does the translation of a gilist to
// an OIDLIST take?  It seems like nothing is gained above the volume
// level, except the swapping of GIs.  So, the gilist -> vector<int>
// can be done in one fell swoop.  After that, the gilist must be
// searched seperately against each volume, because doing a single
// translation would have to do that internally anyway.


void CSeqDBOIDList::x_OrGiFileBits(const string    & gilist_fname,
                                   const CSeqDBVol * volp,
                                   int               vol_start,
                                   int               vol_end,
                                   int               oid_start,
                                   int               oid_end,
                                   CSeqDBLockHold  & locked)
{
    _ASSERT(volp);
    
    m_Atlas.Lock(locked);
    
    // Open file and get pointers
    
    CSeqDBRawFile gilist(m_Atlas);
    CSeqDBMemLease lease(m_Atlas);
    
    Uint4 num_gis(0);
    TIndx file_length(0);
    bool  is_binary(false);
    
    try {
        // Take exception to empty file
        
        gilist.Open(gilist_fname, locked);
        file_length = (TIndx) gilist.GetFileLength();
        
        if (! file_length) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Empty file specified for GI list.");
        }
        
        // File may either be text (starting with a digit) or a
        // correctly formatted binary number array.  Determine which
        // type it is and verify correctness of the metadata.
        
        char first_char(0);
        gilist.ReadBytes(lease, & first_char, 0, 1);
        
        if (isdigit(first_char)) {
            is_binary = false;
        } else if ((((first_char) & 0xFF) == 0xFF) && (file_length >= 8)) {
            is_binary = true;
            Uint4 marker(0);
            
            gilist.ReadSwapped(lease, 0, & marker, locked);
            gilist.ReadSwapped(lease, 4, & num_gis, locked);
            
            if (marker != Uint4(-1)) {
                NCBI_THROW(CSeqDBException,
                           eFileErr,
                           "Unrecognized magic number in GI list file.");
            }
            
            if (num_gis != ((file_length - 8) / 4)) {
                NCBI_THROW(CSeqDBException,
                           eFileErr,
                           "GI list file size does not match internal count.");
            }
        } else {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "File specified for GI list has invalid data.");
        }
        
        // Now we have either a text or binary GI file -- read in the gis.
        
        vector<int> gis;
    
        if (is_binary) {
            gis.reserve(num_gis);
            x_ReadBinaryGiList(gilist, lease, num_gis, gis, locked);
        } else {
            // Assume average gi is at least 6 digits plus newline.
            gis.reserve(file_length / 7);
            x_ReadTextGiList(gilist, lease, gis, locked);
        }
        
        // Iterate over the gis from the file, look up the
        // corresponding oid, and turn on that mask bit.
        
        int vol_size = vol_end - vol_start;
        
        ITERATE(vector<int>, gi_iter, gis) {
            int vol_oid(0);
            
            volp->GiToOid(*gi_iter, vol_oid, locked);
            
            if (vol_oid != -1) {
                if (vol_oid < vol_size) {
                    int oid = vol_oid + vol_start;
                    
                    if ((oid >= oid_start) && (oid < oid_end)) {
                        x_SetBit(oid);
                    }
                }
            }
        }
    }
    catch(...) {
        m_Atlas.RetRegion(lease);
        throw;
    }
    
    m_Atlas.RetRegion(lease);
}

void CSeqDBOIDList::x_SetBitRange(int oid_start,
                                  int oid_end)
{
    // Set bits at the front and back, closing to a range of full-byte
    // addresses.
    
    while((oid_start & 0x7) && (oid_start < oid_end)) {
        x_SetBit(oid_start);
        ++oid_start;
    }
    
    while((oid_end & 0x7) && (oid_start < oid_end)) {
        x_SetBit(oid_end - 1);
        --oid_end;
    }
    
    if (oid_start < oid_end) {
        TUC * bp_start = m_Bits + (oid_start >> 3);
        TUC * bp_end   = m_Bits + (oid_end   >> 3);
        
        _ASSERT(bp_end   <= m_BitEnd);
        _ASSERT(bp_start <  bp_end);
        
        memset(bp_start, 0xFF, (bp_end - bp_start));
    }
}

void CSeqDBOIDList::x_SetBit(TOID oid)
{
    TUC * bp = m_Bits + (oid >> 3);
    
    Int4 bitnum = (oid & 7);
    
    if (bp < m_BitEnd) {
        *bp |= (0x80 >> bitnum);
    }
}

bool CSeqDBOIDList::x_FindNext(TOID & oid) const
{
    // If the specified OID is valid, use it.
    
    if (x_IsSet(oid)) {
        return true;
    }
    
    // OPTIONAL portion
    
    Uint4 whole_word_oids = m_NumOIDs & -32;
    
    while(oid < whole_word_oids) {
        if (x_IsSet(oid)) {
            return true;
        }
        
        oid ++;
        
        // Try the simpler road fer now.
        
        if ((oid & 31) == 0) {
            const Uint4 * bp = ((const Uint4*) m_Bits + (oid             >> 5));
            const Uint4 * ep = ((const Uint4*) m_Bits + (whole_word_oids >> 5));
           
            while((bp < ep) && (0 == *bp)) {
                ++ bp;
                oid += 32;
            }
        }
    }
    
    // END of OPTIONAL portion
    
    while(oid < m_NumOIDs) {
        if (x_IsSet(oid)) {
            return true;
        }
        
        oid++;
    }
    
    return false;
}

void CSeqDBOIDList::x_ReadBinaryGiList(CSeqDBRawFile  & gilist,
                                       CSeqDBMemLease & lease,
                                       int              num_gis,
                                       vector<int>    & gis,
                                       CSeqDBLockHold & locked)
{
    int gisize = (int) sizeof(Uint4);
    int start = 8;
    TIndx end = num_gis * gisize + start;
    
    for(TIndx offset = start; offset < end; offset += gisize) {
        Uint4 elem(0);
        gilist.ReadSwapped(lease, offset, & elem, locked);
        gis.push_back(elem);
    }
}

void CSeqDBOIDList::x_ReadTextGiList(CSeqDBRawFile  & gilist,
                                     CSeqDBMemLease & lease,
                                     vector<int>    & gis,
                                     CSeqDBLockHold & locked)
{
    Uint4 file_length = (Uint4) gilist.GetFileLength();
    
    const char * beginp = gilist.GetRegion(lease, 0, file_length, locked);
    const char * endp   = beginp + file_length;
    
    Uint4 elem(0);
    
    for(const char * p = beginp; p < endp; p ++) {
        Uint4 dig = 0;
        
        switch(*p) {
        case '0':
            dig = 0;
            break;
            
        case '1':
            dig = 1;
            break;
            
        case '2':
            dig = 2;
            break;
            
        case '3':
            dig = 3;
            break;
            
        case '4':
            dig = 4;
            break;
            
        case '5':
            dig = 5;
            break;
            
        case '6':
            dig = 6;
            break;
            
        case '7':
            dig = 7;
            break;
            
        case '8':
            dig = 8;
            break;
            
        case '9':
            dig = 9;
            break;
            
        case '\n':
            // Allow for blank lines
            if (elem != 0) {
                gis.push_back(elem);
            }
            elem = 0;
            continue;
            
        default:
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Empty file specified for GI list.");
        }
        
        elem *= 10;
        elem += dig;
    }
    
    m_Atlas.RetRegion(lease);
}

END_NCBI_SCOPE

