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
 * Author:  Ning Ma
 *
 */

/// @file seqdbgimask.cpp
/// This is the implementation file for the CSeqDBGiMask class,
/// which support read operations on Gi based BlastDB masks.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "seqdbgimask.hpp"
#include "seqdbgeneral.hpp"

BEGIN_NCBI_SCOPE

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )

CSeqDBGiMask::CSeqDBGiMask(CSeqDBAtlas           & atlas,
                           const vector <string> & mask_name)
    : m_Atlas            (atlas),
      m_MaskNames        (mask_name),
      m_AlgoId           (-1),     
      m_IndexFile        (m_Atlas),
      m_IndexLease       (m_Atlas),
      m_OffsetFile       (m_Atlas),
      m_OffsetLease      (m_Atlas),
      m_DataFile         (m_Atlas),
      m_DataLease        (m_Atlas)
{ }

const string & 
CSeqDBGiMask::GetDesc(int               algo_id, 
                      CSeqDBLockHold  & locked)
{
    m_Atlas.Lock(locked);
    x_Open(algo_id, locked);
    return m_Desc;
}

void 
CSeqDBGiMask::GetMaskData(int                     algo_id,
                          int                     gi,
                          CSeqDB::TSequenceRanges &ranges,
                          CSeqDBLockHold          &locked)
{
    m_Atlas.Lock(locked); 

    x_Open(algo_id, locked);

    // search for page
    int page(0);
    int retv = s_BinarySearch(m_GiIndex, m_GiIndex+m_NumIndex, m_NumIndex, gi, page);
   
    if (retv == -1) {
        if (page == -1) return;

        int pagesize(m_PageSize);
        if (page * m_PageSize + pagesize > m_NumGi) {
            pagesize = m_NumGi - page * m_PageSize;
        }

        TIndx begin = page * m_PageSize * (m_GiSize + m_OffsetSize);
        TIndx end = begin + pagesize * (m_GiSize + m_OffsetSize);
        const Int4 * offset = (const Int4 *)
              m_OffsetFile.GetRegion(m_OffsetLease, begin, end, locked);

        retv = s_BinarySearch(offset, offset+pagesize, pagesize, gi, page);
        if (retv == -1) return;
    }

    // Retrieving the mask data
    const Int4 * datap = (const Int4 *)
              m_DataFile.GetRegion(m_DataLease, retv, retv+4, locked);
    ranges._size = *datap;
    ranges.resize(ranges._size);
    // Remapping the mask data
    datap = (const Int4 *)
            m_DataFile.GetRegion(m_DataLease, retv+4, retv + 8*ranges._size + 4, locked);
    memcpy(ranges._data, datap, ranges._size * 8);
    return;
}

void CSeqDBGiMask::x_Open(Int4              algo_id,
                          CSeqDBLockHold  & locked) 
{
    if (algo_id == m_AlgoId) {
        return;
    }

    if (algo_id < 0 || algo_id >= m_MaskNames.size()) {
        NCBI_THROW(CSeqDBException, eFileErr,
                       "Could not open gi-mask files.");
    }

    string ext_i(".gmi");
    string ext_o(".gmo");
    string ext_d(".gmd");

    const Int2 bytetest = 0x0011;
    const char *ptr = (const char *) &bytetest;
    if (ptr[0] == 0x11) {
        // Use small endian instead.
        ext_i[2] = ext_o[2] = ext_d[2] = 'n';
    }

    m_Atlas.Lock(locked);
    
    try {
        // Find the path and name...
        CSeqDB_Path fn_i(SeqDB_ResolveDbPath(m_MaskNames[algo_id] + ext_i));
        CSeqDB_Path fn_o(SeqDB_ResolveDbPath(m_MaskNames[algo_id] + ext_o));
        CSeqDB_Path fn_d(SeqDB_ResolveDbPath(m_MaskNames[algo_id] + ext_d));

        bool found_i = m_IndexFile.Open(fn_i, locked);
        bool found_o = m_OffsetFile.Open(fn_o, locked);
        bool found_d = m_DataFile.Open(fn_d, locked);
        
        if (! (found_i && found_o && found_d)) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Could not open gi-mask files.");
        }

        m_AlgoId = algo_id;
        x_ReadFields(locked);
    }
    catch(...) {
        m_AlgoId = -1;
        m_Atlas.Unlock(locked);
        throw;
    }
}

void CSeqDBGiMask::s_GetFileRange(TIndx            begin,
                                  TIndx            end,
                                  CSeqDBRawFile  & file,
                                  CSeqDBMemLease & lease,
                                  CBlastDbBlob   & blob,
                                  CSeqDBLockHold & locked)
{
    const char * ptr = file.GetRegion(lease, begin, end, locked);
    CTempString data(ptr, end-begin);
    blob.ReferTo(data);
}

void CSeqDBGiMask::x_ReadFields(CSeqDBLockHold & locked)
{
    const int kFixedFieldBytes = 32;
    
    m_Atlas.Lock(locked);
    
    // First, get the 32 bytes of fields that we know exist.
    
    CBlastDbBlob header;
    s_GetFileRange(0, kFixedFieldBytes, m_IndexFile, m_IndexLease, header, locked);
    
    int fmt_version = header.ReadInt4();
    
    if (fmt_version != 1) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Gi-mask file uses unknown format_version.");
    }
    
    int data_type = header.ReadInt4();
    
    if (data_type != 0) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Gi-mask file uses unknown data type.");
    }
    
    m_GiSize = header.ReadInt4();
    m_OffsetSize = header.ReadInt4();
    m_PageSize = header.ReadInt4();

    m_NumIndex = header.ReadInt4();
    m_NumGi  = header.ReadInt4();

    m_IndexStart = header.ReadInt4();
    
    SEQDB_FILE_ASSERT(m_IndexStart >= 0);
    SEQDB_FILE_ASSERT(m_IndexFile.GetFileLength() >= m_IndexStart);
    
    // Now we know how long the header actually is, so expand the blob
    // to reference the whole thing.  (The memory lease should already
    // hold the data, so this will just adjust a few integer fields.)
    
    s_GetFileRange(0, m_IndexStart, m_IndexFile, m_IndexLease, header, locked);
    
    // Get string type header fields.
    
    m_Desc  = header.ReadString (kStringFmt);
    m_Date  = header.ReadString (kStringFmt);

    SEQDB_FILE_ASSERT(m_Desc.size());
    SEQDB_FILE_ASSERT(m_Date.size());
    
    // Map the index file
    TIndx begin = m_IndexStart;
    TIndx end = begin + m_NumIndex * (m_GiSize + m_OffsetSize);
    m_GiIndex = (const Int4 *) 
              m_IndexFile.GetRegion(m_IndexLease, begin, end, locked);
}

// TODO: if gi or offset becomes 8-bytes long, this may better be 
// implemented as template functions
int
CSeqDBGiMask::s_BinarySearch(const int *keys,
                             const int *values,
                             const int  n,
                             const int  key,
                             int       &idx) {
    int lower(0), upper(n-1);

    if (key > keys[upper] || key < keys[lower]) {
        // out of range
        idx = -1;
        return -1;
    }

    if (key == keys[upper]) {
        return values[upper];
    }

    if (key == keys[lower]) {
        return values[lower];
    }

    idx = (lower + upper)/2;

    while (idx != lower) {
        if (key > keys[idx]) {
            lower = idx;
            idx = (lower + upper)/2;
        } else if (key < keys[idx]) {
            upper = idx;
            idx = (lower + upper)/2;
        } else {
            // value found
            return values[idx];
        }
    }
    // value not found
    return -1;
}

#endif

END_NCBI_SCOPE

