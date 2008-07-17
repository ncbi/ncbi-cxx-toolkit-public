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

/// @file seqdbcol.cpp
/// This is the implementation file for the CSeqDBColumnReader,
/// CSeqDBColumn, CSeqDBColumnFlush, and CSeqDB_ColumnEntry classes,
/// which support read operations on BlastDb format database columns.

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/column_reader.hpp>
#include "seqdbcol.hpp"
#include "seqdbgeneral.hpp"

BEGIN_NCBI_SCOPE

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
// CSeqDB_ColumnReader

CSeqDB_ColumnReader::
CSeqDB_ColumnReader(const string & volname, char file_id)
    : m_Impl(NULL)
{
    _ASSERT(isalnum(file_id));
    
    string index_extn = "x_a";
    index_extn[1] = file_id;
    
    string data_extn = index_extn;
    data_extn[2] = 'b';
    
    // Create the actual column object.
    m_Impl = new CSeqDBColumn(volname, index_extn, data_extn, NULL);
}

CSeqDB_ColumnReader::~CSeqDB_ColumnReader()
{
    delete m_Impl;
}

const string & CSeqDB_ColumnReader::GetTitle() const
{
    return m_Impl->GetTitle();
}

const map<string,string> & CSeqDB_ColumnReader::GetMetaData()
{
    return m_Impl->GetMetaData();
}

const string & CSeqDB_ColumnReader::GetValue(const string & key)
{
    static string mt;
    return SeqDB_MapFind(GetMetaData(), key, mt);
}

int CSeqDB_ColumnReader::GetNumOIDs() const
{
    return m_Impl->GetNumOIDs();
}

void CSeqDB_ColumnReader::GetBlob(int            oid,
                                  CBlastDbBlob & blob)
{
    // The blob Clear() must be done in a path where this thread does
    // *not* hold the atlas lock, otherwise the destructor for the
    // blob's 'lifetime' object might try to get the same lock and the
    // thread would self-deadlock.
    
    blob.Clear();
    return m_Impl->GetBlob(oid, blob, true, NULL);
}


// CSeqDBColumn

CSeqDBColumn::CSeqDBColumn(const string   & basename,
                           const string   & index_extn,
                           const string   & data_extn,
                           CSeqDBLockHold * lockedp)
    : m_AtlasHolder      (true, & m_FlushCB, lockedp),
      m_Atlas            (m_AtlasHolder.Get()),
      m_IndexFile        (m_Atlas),
      m_IndexLease       (m_Atlas),
      m_DataFile         (m_Atlas),
      m_DataLease        (m_Atlas),
      m_NumOIDs          (0),
      m_DataLength       (0),
      m_MetaDataStart    (0),
      m_OffsetArrayStart (0)
{
    CSeqDBLockHold locked2(m_Atlas);
    
    if (lockedp == NULL) {
        lockedp = & locked2;
    }
    
    m_Atlas.Lock(*lockedp);
    
    try {
        CSeqDB_Path fn1(basename + "." + index_extn);
        CSeqDB_Path fn2(basename + "." + data_extn);
        
        bool found1 = m_IndexFile.Open(fn1, *lockedp);
        bool found2 = m_DataFile.Open(fn2, *lockedp);
        
        if (! (found1 && found2)) {
            NCBI_THROW(CSeqDBException, eFileErr,
                       "Could not open database column files.");
        }
        
        x_ReadFields(*lockedp);
        x_ReadMetaData(*lockedp);
    }
    catch(...) {
        m_Atlas.Unlock(*lockedp);
        throw;
    }
    
    m_FlushCB.SetColumn(this);
}

CSeqDBColumn::~CSeqDBColumn()
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    
    m_FlushCB.SetColumn(NULL);
    Flush();
}

bool CSeqDBColumn::ColumnExists(const string   & basename,
                                const string   & index_extn,
                                const string   & data_extn,
                                CSeqDBAtlas    & atlas,
                                CSeqDBLockHold & locked)
{
    string fn1(basename + "." + index_extn);
    string fn2(basename + "." + data_extn);
    
    return (atlas.DoesFileExist(fn1, locked) &&
            atlas.DoesFileExist(fn1, locked));
}

const string & CSeqDBColumn::GetTitle() const
{
    _ASSERT(m_Title.length());
    return m_Title;
}

int CSeqDBColumn::GetNumOIDs() const
{
    return m_NumOIDs;
}

void CSeqDBColumn::Flush()
{
    m_IndexLease.Clear();
    m_DataLease.Clear();
}

void CSeqDBColumn::x_GetFileRange(TIndx            begin,
                                  TIndx            end,
                                  ESelectFile      select_file,
                                  bool             lifetime,
                                  CBlastDbBlob   & blob,
                                  CSeqDBLockHold & locked)
{
    bool index = (select_file == e_Index);
    _ASSERT(index || (select_file == e_Data));
    
    CSeqDBRawFile  & file  = index ? m_IndexFile  : m_DataFile;
    CSeqDBMemLease & lease = index ? m_IndexLease : m_DataLease;
    
    const char * ptr = file.GetRegion(lease, begin, end, locked);
    
    CTempString data(ptr, end-begin);
    
    if (lifetime) {
        CRef<CObject> hold(new CSeqDB_AtlasRegionHolder(m_Atlas, ptr));
        blob.ReferTo(data, hold);
        lease.IncrementRefCnt();
    } else {
        blob.ReferTo(data);
    }
}

void CSeqDBColumn::x_ReadFields(CSeqDBLockHold & locked)
{
    const int kFixedFieldBytes = 32;
    
    m_Atlas.Lock(locked);
    
    // First, get the 32 bytes of fields that we know exist.
    
    CBlastDbBlob header;
    x_GetFileRange(0, kFixedFieldBytes, e_Index, false, header, locked);
    
    int fmt_version = header.ReadInt4();
    
    if (fmt_version != 1) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Column file uses unknown format_version.");
    }
    
    int column_type = header.ReadInt4();
    
    if (column_type != 1) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Column file uses unknown data type.");
    }
    
    int offset_size = header.ReadInt4();
    
    if (offset_size != 4) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Column file uses unsupported offset size.");
    }
    
    m_NumOIDs = header.ReadInt4();
    m_DataLength = header.ReadInt8();
    m_MetaDataStart = header.ReadInt4();
    m_OffsetArrayStart = header.ReadInt4();
    
    SEQDB_FILE_ASSERT(m_NumOIDs || (! m_DataLength));
    
    SEQDB_FILE_ASSERT(m_MetaDataStart >= 0);
    SEQDB_FILE_ASSERT(m_OffsetArrayStart >= m_MetaDataStart);
    SEQDB_FILE_ASSERT(m_IndexFile.GetFileLength() >= m_OffsetArrayStart);
    
    // Now we know how long the header actually is, so expand the blob
    // to reference the whole thing.  (The memory lease should already
    // hold the data, so this will just adjust a few integer fields.)
    
    x_GetFileRange(0, m_MetaDataStart, e_Index, false, header, locked);
    
    // Get string type header fields.
    
    m_Title = header.ReadString (kStringFmt);
    m_Date  = header.ReadString (kStringFmt);
    
    SEQDB_FILE_ASSERT(m_Title.size());
    SEQDB_FILE_ASSERT(m_Date.size());
    
    if (header.GetReadOffset() != m_MetaDataStart) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "CSeqDBColumn: File format error.");
    }
}

void CSeqDBColumn::x_ReadMetaData(CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);
    
    int begin = m_MetaDataStart;
    int end = m_OffsetArrayStart;
    
    _ASSERT(begin > 0 && end > begin);
    
    CBlastDbBlob metadata;
    x_GetFileRange(begin, end, e_Index, false, metadata, locked);
    
    Int8 count8 = metadata.ReadVarInt();
    
    if (count8 >> 31) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "CSeqDBColumn: File format error.");
    }
    
    int count = (int) count8;
    
    for(int j = 0; j < count; j++) {
        string key = metadata.ReadString(kStringFmt);
        string value = metadata.ReadString(kStringFmt);
        
        if (m_MetaData.find(key) != m_MetaData.end()) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "CSeqDBColumn: Error; duplicate metadata key.");
        }
        
        m_MetaData[key] = value;
    }
    
    // Align to an 8 byte multiple; eString means that we can change
    // the alignment of this field without losing compatibility.
    
    metadata.SkipPadBytes(8, CBlastDbBlob::eString);
    
    int header_bytes = m_OffsetArrayStart - m_MetaDataStart;
    
    if (metadata.GetReadOffset() != header_bytes) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "CSeqDBColumn: File format error.");
    }
}

void CSeqDBColumn::GetBlob(int              oid,
                           CBlastDbBlob   & blob,
                           bool             keep,
                           CSeqDBLockHold * lockedp)
{
    _ASSERT(0 == blob.Size());
    
    CSeqDBLockHold locked2(m_Atlas);
    
    if (lockedp == NULL) {
        lockedp = & locked2;
    }
    
    int item_size = 4;
    int istart = m_OffsetArrayStart + item_size*oid;
    int iend = istart + (2 * item_size);
    
    CBlastDbBlob offsets;
    x_GetFileRange(istart, iend, e_Index, false, offsets, *lockedp);
    
    int dstart = offsets.ReadInt4();
    int dend = offsets.ReadInt4();
    
    SEQDB_FILE_ASSERT(dend >= dstart);
    
    if (dend > dstart) {
        x_GetFileRange(dstart, dend, e_Data, keep, blob, *lockedp);
    } else {
        _ASSERT(! blob.Size());
    }
}


// CSeqDBColumnFlush

void CSeqDBColumnFlush::operator()()
{
    if (m_Column) {
        m_Column->Flush();
    }
}

const map<string,string> & CSeqDBColumn::GetMetaData()
{
    return m_MetaData;
}


// CSeqDB_ColumnEntry

CSeqDB_ColumnEntry::CSeqDB_ColumnEntry(const vector<int> & indices)
    : m_VolIndices(indices), m_HaveMap(false)
{
}

void CSeqDB_ColumnEntry::SetMapValue(const string & k, const string & v)
{
    // Store a map value, but only if this key's value has not been set.
    
    if (m_Map.find(k) == m_Map.end()) {
        m_Map[k] = v;
    }
}
#endif

END_NCBI_SCOPE

