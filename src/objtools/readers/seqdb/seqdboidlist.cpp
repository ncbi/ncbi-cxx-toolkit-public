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

CSeqDBOIDList::CSeqDBOIDList(CSeqDBAtlas  & atlas,
                             CSeqDBVolSet & volset)
    : m_Atlas   (atlas),
      m_Lease   (atlas),
      m_NumOIDs (0),
      m_Bits    (0),
      m_BitEnd  (0),
      m_BitOwner(false)
{
    CSeqDBLockHold locked(m_Atlas);
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
    
    m_NumOIDs = SeqDB_GetStdOrd((Uint4 *) m_Lease.GetPtr(0)) + 1;
    m_Bits    = (unsigned char*) m_Lease.GetPtr(sizeof(Uint4));
    m_BitEnd  = m_Bits + file_length - sizeof(Uint4);
}


// The general rule I am following in these methods is to use byte
// computations except during actual looping.

void CSeqDBOIDList::x_Setup(CSeqDBVolSet   & volset,
                            CSeqDBLockHold & locked)
{
    _ASSERT(volset.HasFilter() && (! volset.HasSimpleMask()));
    
    // First, get the memory space, clear it.
    
    // Pad memory space to word boundary, add 8 bytes for "insurance".
    
    Uint4 num_oids = volset.GetNumOIDs();
    Uint4 byte_length = ((num_oids + 31) / 32) * 4 + 8;
    
    m_Bits   = (TUC*) m_Atlas.Alloc(byte_length, locked);
    m_BitEnd = m_Bits + byte_length;
    m_BitOwner = true;
    
    try {
        memset((void*) m_Bits, 0, byte_length);
    
        // Then get the list of filenames and offsets to overlay onto it.
    
        for(Uint4 i = 0; i < volset.GetNumVols(); i++) {
            bool         all_oids (false);
            list<string> mask_files;
            list<string> gilist_files;
            Uint4        oid_start(0);
            Uint4        oid_end  (0);
            
            volset.GetMaskFiles(i,
                                all_oids,
                                mask_files,
                                gilist_files,
                                oid_start,
                                oid_end);
            
            if (all_oids) {
                x_SetBitRange(oid_start, oid_end);
            } else {
                // For each file, copy bits into array.
                
                ITERATE(list<string>, mask_iter, mask_files) {
                    x_OrMaskBits(*mask_iter, oid_start, oid_end, locked);
                }
                
                ITERATE(list<string>, gilist_iter, gilist_files) {
                    x_OrGiFileBits(*gilist_iter,
                                   volset.GetVol(i),
                                   oid_start,
                                   oid_end,
                                   locked);
                }
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
    
    if (seqdb_debug_class & debug_oid) {
        cout << "x_Setup: Dumping OID map data." << endl;
    
        unsigned cnt = 0;
    
        cout << hex;
    
        for(TCUC * bp = m_Bits; bp < m_BitEnd; bp ++) {
            unsigned int ubp = (unsigned int)(*bp);
        
            if (ubp >= 16) {
                cout << ubp << " ";
            } else {
                cout << "0" << ubp << " ";
            }
        
            cnt++;
        
            if (cnt == 32) {
                cout << "\n";
                cnt = 0;
            }
        }
    
        cout << dec << "\n" << endl;
    }
}

// oid_end is not used - it could be.  One use would be to trim the
// "incoming bits" to that length; specifically, to assume that the
// file may contain nonzero "junk data" after the official end point.
//
// This implies that two oid sets share the oid mask file, but one
// used a smaller subset of that file.  That really should never
// happen; it would be so unlikely for that optimization to "buy
// anything" that the code would almost certainly never be written
// that way.  For this reason, I have not yet implemented trimming.

void CSeqDBOIDList::x_OrMaskBits(const string   & mask_fname,
                                 Uint4            oid_start,
                                 Uint4            /*oid_end*/,
                                 CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);
    
    // Open file and get pointers
    
    TCUC* bitmap = 0;
    TCUC* bitend = 0;
    
    CSeqDBRawFile volmask(m_Atlas);
    CSeqDBMemLease lease(m_Atlas);
    
    {
        Uint4 num_oids = 0;
        
        volmask.Open(mask_fname);
        
        volmask.ReadSwapped(lease, 0, & num_oids, locked);
        
        // This is the index of the last oid, not the count of oids...
        num_oids++;
        
        Uint4 file_length = (Uint4) volmask.GetFileLength();
        
        // Cast forces signed/unsigned conversion.
        
        volmask.GetRegion(lease, sizeof(Int4), file_length, locked);
        bitmap = (TCUC*) lease.GetPtr(sizeof(Int4));
        
        //bitend = bitmap + file_length - sizeof(Int4);
        bitend = bitmap + (((num_oids + 31) / 32) * 4);
    }
    
    // Fold bitmap/bitend into m_Bits/m_BitEnd at bit offset oid_start.
    
    
    if (0 == (oid_start & 31)) {
        // If the new data is "word aligned", we can use a fast algorithm.
        
        TCUC * srcp = bitmap;
        TUC  * locp = m_Bits + (oid_start / 8);
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
    } else if (0 == (oid_start & 7)) {
        // If the new data is "byte aligned", we can use a less fast algorithm.
        
        TCUC * srcp = bitmap;
        TUC  * locp = m_Bits + (oid_start / 8);
        TUC  * endp = locp + (bitend-bitmap);
        
        _ASSERT(endp <= m_BitEnd);
        
        while(locp < endp) {
            *locp++ |= *srcp++;
        }
    } else {
        // Otherwise... we have to use a slower, byte splicing algorithm.
        
        Uint4 Rshift = oid_start & 7;
        Uint4 Lshift = 8 - Rshift;
        
        TCUC * srcp = bitmap;
        TUC  * locp = m_Bits + (oid_start / 8);
        TUC  * endp = locp + (bitend-bitmap);
        
        _ASSERT(endp <= m_BitEnd);
        
        // This loop iterates over the source bytes.  Each byte is
        // split over two destination bytes.
        
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
// level, except the swapping of GIs.  So, the gilist->vector<Uint4>
// can be done in one fell swoop.  After that, the gilist must be
// searched seperately against each volume, because doing a single
// translation would have to do that internally anyway.

void CSeqDBOIDList::x_OrGiFileBits(const string    & gilist_fname,
                                   const CSeqDBVol * volp,
                                   Uint4             oid_start,
                                   Uint4             oid_end,
                                   CSeqDBLockHold  & locked)
{
    _ASSERT(volp);
    
    m_Atlas.Lock(locked);
    
    // Open file and get pointers
    
    CSeqDBRawFile gilist(m_Atlas);
    CSeqDBMemLease lease(m_Atlas);
    
    Uint4 num_gis(0);
    Uint4 file_length(0);
    bool is_binary(false);
    
    try {
        // Take exception to empty file
        
        gilist.Open(gilist_fname);
        file_length = (Uint4) gilist.GetFileLength();
        
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
        
        vector<Uint4> gis;
    
        if (is_binary) {
            gis.reserve(num_gis);
            x_ReadBinaryGiList(gilist, lease, 8, num_gis, gis, locked);
        } else {
            // Assume average gi is at least 6 digits plus newline.
            gis.reserve(file_length / 7);
            x_ReadTextGiList(gilist, lease, gis, locked);
        }
        
        // Iterate over the gis from the file, look up the
        // corresponding oid, and turn on that mask bit.
        
        Uint4 vol_size = oid_end - oid_start;
        
        ITERATE(vector<Uint4>, gi_iter, gis) {
            Uint4 vol_oid(0);
            
            volp->GiToOid(*gi_iter, vol_oid, locked);
            
            if (vol_oid != Uint4(-1)) {
                if (vol_oid < vol_size) {
                    Uint4 oid = vol_oid + oid_start;
                    x_SetBit(oid);
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

void CSeqDBOIDList::x_SetBitRange(Uint4          oid_start,
                                  Uint4          oid_end)
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
                                       Uint4            start, // 8
                                       Uint4            num_gis,
                                       vector<Uint4>  & gis,
                                       CSeqDBLockHold & locked)
{
    Uint4 gisize = sizeof(Uint4);
    Uint4 end = num_gis * gisize + start;
    
    for(Uint4 offset = start; offset < end; offset += gisize) {
        Uint4 elem(0);
        gilist.ReadSwapped(lease, offset, & elem, locked);
        gis.push_back(elem);
    }
}

void CSeqDBOIDList::x_ReadTextGiList(CSeqDBRawFile  & gilist,
                                     CSeqDBMemLease & lease,
                                     vector<Uint4>  & gis,
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

