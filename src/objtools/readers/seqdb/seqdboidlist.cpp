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
    _ASSERT( volset.HasMask() );
    
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
    
    m_NumOIDs = SeqDB_GetStdOrd((Uint4 *) m_Lease.GetPtr(0));
    m_Bits    = (unsigned char*) m_Lease.GetPtr(sizeof(Uint4));
    m_BitEnd  = m_Bits + file_length - sizeof(Uint4);
}


// The general rule I am following in these methods is to use byte
// computations except during actual looping.

void CSeqDBOIDList::x_Setup(CSeqDBVolSet & volset, CSeqDBLockHold & locked)
{
    _ASSERT(volset.HasMask() && (! volset.HasSimpleMask()));
    
    // First, get the memory space, clear it.
    
    // Pad memory space to word boundary.
    
    Uint4 num_oids = volset.GetNumSeqs();
    Uint4 byte_length = ((num_oids + 31) / 32) * 4;
    
    m_Bits   = (TUC*) m_Atlas.Alloc(byte_length, locked);
    m_BitEnd = m_Bits + byte_length;
    m_BitOwner = true;
    
    memset((void*) m_Bits, 0, byte_length);
    
    // Then get the list of filenames and offsets to overlay onto it.
    
    for(Uint4 i = 0; i < volset.GetNumVols(); i++) {
        bool         all_oids (false);
        list<string> mask_files;
        Uint4        oid_start(0);
        Uint4        oid_end  (0);
        
        volset.GetMaskFiles(i, all_oids, mask_files, oid_start, oid_end);
        
        if (all_oids) {
            x_SetBitRange(oid_start, oid_end);
        } else {
            // For each file, copy bits into array.
            
            for(list<string>::iterator mask_iter = mask_files.begin();
                mask_iter != mask_files.end();
                ++mask_iter) {
                
                x_OrFileBits(*mask_iter, oid_start, oid_end, locked);
            }
        }
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

void CSeqDBOIDList::x_OrFileBits(const string   & mask_fname,
                                 Uint4            oid_start,
                                 Uint4            /*oid_end*/,
                                 CSeqDBLockHold & locked)
{
    // Open file and get pointers
    
    TCUC* bitmap = 0;
    TCUC* bitend = 0;
    
    CSeqDBRawFile volmask(m_Atlas);
    
    {
        Uint4 num_oids = 0;
        
        volmask.Open(mask_fname);
        
        CSeqDBMemLease lease(m_Atlas);
        volmask.ReadSwapped(lease, 0, & num_oids, locked);
        
        Uint4 file_length = (Uint4) volmask.GetFileLength();
        
        // Cast forces signed/unsigned conversion.
        
        bitmap = (TCUC*) volmask.GetRegion(sizeof(Int4), file_length, locked);
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
        
        TCUC * endp2 = endp - 1;
        
        // This loop iterates over the source bytes.  Each byte is
        // split over two destination bytes.
        
        while(locp < endp2) {
            // Store left half of source char in one location.
            TCUC source = *srcp;
            *locp |= (source >> Rshift);
            locp++;
            
            // Store right half of source in the next location.
            *locp |= (source << Lshift);
            srcp++;
        }
    }
    
    m_Atlas.RetRegion((const char*) bitmap, locked);
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

bool CSeqDBOIDList::x_IsSet(TOID oid) const
{
    TCUC * bp = m_Bits + (oid >> 3);
    
    Int4 bitnum = (oid & 7);
    
    if (bp < m_BitEnd) {
        if (*bp & (0x80 >> bitnum)) {
            return true;
        }
    }
    
    return false;
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

END_NCBI_SCOPE

