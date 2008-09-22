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

/// @file writedb_column.cpp
/// Implementation for the CWriteDB_Column and related classes.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include "writedb_column.hpp"

BEGIN_NCBI_SCOPE

/// Import C++ std namespace.
USING_SCOPE(std);

// CWriteDB_Column

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
CWriteDB_Column::CWriteDB_Column(const string      & dbname,
                                 const string      & extn1,
                                 const string      & extn2,
                                 int                 index,
                                 const string      & title,
                                 const TColumnMeta & meta,
                                 Uint8               max_file_size)
{
    m_DFile.Reset(new CWriteDB_ColumnData(dbname,
                                          extn2,
                                          index,
                                          max_file_size));
    
    m_IFile.Reset(new CWriteDB_ColumnIndex(dbname,
                                           extn1,
                                           index,
                                           *m_DFile,
                                           title,
                                           meta,
                                           max_file_size));
}

CWriteDB_Column::~CWriteDB_Column()
{
}

void CWriteDB_Column::ListFiles(vector<string> & files, bool skip_empty) const
{
    if (! (skip_empty && m_DFile->Empty())) {
        files.push_back(m_IFile->GetFilename());
        files.push_back(m_DFile->GetFilename());
    }
}

void CWriteDB_Column::AddBlob(const CBlastDbBlob & blob)
{
    // Note that data size is the size *after* the blob has been
    // written.  The initial (zero) offset is written during file
    // creation.
    
    Int8 data_size = m_DFile->WriteBlob(blob);
    m_IFile->WriteBlobIndex(data_size);
}

void CWriteDB_Column::Close()
{
    m_IFile->Close();
    m_DFile->Close();
}

bool CWriteDB_Column::CanFit(int size) const
{
    return m_IFile->CanFit() && m_DFile->CanFit(size);
}

void CWriteDB_Column::RenameSingle()
{
    m_IFile->RenameSingle();
    m_DFile->RenameSingle();
}

void CWriteDB_Column::AddMetaData(const string & key, const string & value)
{
    return m_IFile->AddMetaData(key, value);
}


// CWriteDB_ColumnIndex

// Format (see BlastDb .pin file for comparison)
//
// Notes:
//  A. Fixed width stuff at top.
//  B. Strings use prefixed lengths.
//  C. Padding is done explicitly.
//  D. Each of these is 4 bytes unless indicated.
//
// 0:  Format version. (always 1 for now)
// 4:  Column type. (always 1 (blob) for now)
// 8:  Size of offsets (always 4 for now)
// 12: OID count.
// 16: Data file length (8).
// 24: Offset of meta data (4).
// 28: Offset of offset array (4). (#O).
// 32: Title (identifies this column file) (varies).
// ??: Create date (varies).
// ??: Metadata count (4).
// ??: Meta-data (varies:)
//
// (For each meta data element:)
//   Key (varies)
//   Value (varies
//
// Pad string (varies from 0 to 8 bytes).
//
// Offset #0 (8)
// Offset #1 (8)
// ...
//
// The rule of thumb is that integers which appear in fixed or aligned
// positions in the data stream are encoded as fixed width values, and
// other integers are packed as variable width values.  This permits

// rapid access to fixed width data without considering nearby values.
//
// I'm not sure that this argument is compelling in this case (as the
// total number of bytes here is small), but it may be important for
// other cases such as building large arrays of similar structures for
// fixed-width column data.)

CWriteDB_ColumnIndex::
CWriteDB_ColumnIndex(const string        & dbname,
                     const string        & extn,
                     int                   index,
                     CWriteDB_ColumnData & datafile,
                     const string        & title,
                     const TColumnMeta   & meta,
                     Uint8                 max_file_size)
    : CWriteDB_File (dbname, extn, index, max_file_size, false),
      m_DataFile    (& datafile),
      m_MetaData    (meta),
      m_Title       (title),
      m_OIDs        (0),
      m_DataLength  (0)
{
    m_Date = CTime(CTime::eCurrent).AsString();
}

CWriteDB_ColumnIndex::~CWriteDB_ColumnIndex()
{
}

void CWriteDB_ColumnIndex::WriteBlobIndex(Int8 offset)
{
    _ASSERT(0 == (offset >> 32));
    
    if (m_Header.Empty()) {
        m_Header.Reset(new CBlastDbBlob(256));
        m_Offsets.Reset(new CBlastDbBlob(4096));
        
        // We build these now so that m_DataLength is accurate.  They
        // will be rebuilt just before they are written, when the file
        // is closed.
        
        x_BuildHeaderFields();
        x_BuildHeaderStrings();
        
        // Offset of first data element (always zero).
        m_Offsets->WriteInt4(0);
        
        m_DataLength = m_Header->Size() + m_Offsets->Size();
    }
    
    m_Offsets->WriteInt4((Int4) offset);
    m_OIDs ++;
}

void CWriteDB_ColumnIndex::x_BuildHeaderFields()
{
    // The Blob type makes a great binary data stream type.
    
    const int kFormatVersion = 1; // SeqDB has one of these.
    const int kColumnType    = 1; // Blob (only choice right now)
    const int kOffsetSize    = 4; // Data file offset size (always 4)
    
    m_Header->SeekWrite(0);
    m_Header->WriteInt4(kFormatVersion);
    m_Header->WriteInt4(kColumnType);
    m_Header->WriteInt4(kOffsetSize);
    m_Header->WriteInt4(m_OIDs);
    m_Header->WriteInt8(m_DataFile->GetDataLength());
}

void CWriteDB_ColumnIndex::x_BuildHeaderStrings()
{
    // The write offset (in m_Header) when calling this function
    // should be immediately after the fixed-size header fields
    // written by BuildHeaderFields.
    
    int meta_data_p = m_Header->GetWriteOffset();
    m_Header->WriteInt4(0); // metadata start
    
    int array_offset_p = m_Header->GetWriteOffset();
    m_Header->WriteInt4(0); // offset array start
    
    m_Header->WriteString(m_Title, kStringFmt);
    m_Header->WriteString(m_Date, kStringFmt);
    
    int meta_off = m_Header->GetWriteOffset();
    m_Header->WriteInt4(meta_off, meta_data_p);
    
    x_BuildMetaData();
    
    // Align to an 8 byte multiple; eString means that we can change
    // the alignment of this field without losing compatibility.
    m_Header->WritePadBytes(8, CBlastDbBlob::eString);
    
    int array_off = m_Header->GetWriteOffset();
    m_Header->WriteInt4(array_off, array_offset_p);
    
    _ASSERT((array_off & 0x7) == 0);
}

void CWriteDB_ColumnIndex::x_BuildMetaData()
{
    _ASSERT(m_Header->GetWriteOffset() != 0);
    
    m_Header->WriteVarInt(m_MetaData.size());
    
    ITERATE(TColumnMeta, iter, m_MetaData) {
        CTempString key = iter->first, value = iter->second;
        m_Header->WriteString(key, kStringFmt);
        m_Header->WriteString(value, kStringFmt);
    }
}

void CWriteDB_ColumnIndex::x_Flush()
{
    if (! m_DataFile->Empty()) {
        if (! m_Created) {
            Create();
        }
        
        // These need to be rebuilt to write the correct values for
        // OID count, total length, and possibly meta data.
        
        x_BuildHeaderFields();
        x_BuildHeaderStrings();
        
        Write(m_Header->Str());
        Write(m_Offsets->Str());
        
        // We're done with these now, so free up the memory.
        
        m_Header.Reset();
        m_Offsets.Reset();
    }
}

bool CWriteDB_ColumnIndex::CanFit() const
{
    return (m_DataLength + kEntrySize) < m_MaxFileSize;
}

void CWriteDB_ColumnIndex::AddMetaData(const string & key, const string & value)
{
    m_DataLength += (key.size() +   CBlastDbBlob::VarIntSize(key.size()) +
                     value.size() + CBlastDbBlob::VarIntSize(value.size()));
    
    m_MetaData[key] = value;
}


// CWriteDB_ColumnData

CWriteDB_ColumnData::CWriteDB_ColumnData(const string     & dbname,
                                         const string     & extn,
                                         int                index,
                                         Uint8              max_file_size)
    : CWriteDB_File (dbname, extn, index, max_file_size, false),
      m_DataLength  (0)
{
}

CWriteDB_ColumnData::~CWriteDB_ColumnData()
{
}

Int8 CWriteDB_ColumnData::WriteBlob(const CBlastDbBlob & blob)
{
    if (! blob.Size()) {
        return m_DataLength;
    }
    
    if (! m_Created) {
        Create();
    }
    
    return m_DataLength = Write(blob.Str());
}

void CWriteDB_ColumnData::x_Flush()
{
    if ((! m_Created) && (m_DataLength != 0)) {
        Create();
    }
}

bool CWriteDB_ColumnData::CanFit(int size) const
{
    return Uint8(m_DataLength + size) < m_MaxFileSize;
}


// CWriteDB_ColumnBuilder

CWriteDB_ColumnBuilder::
CWriteDB_ColumnBuilder(const string & title,
                       const string & basename,
                       char           file_id)
    : m_Impl(NULL)
{
    _ASSERT(isalnum(file_id));
    
    string index_extn = "x_a";
    index_extn[1] = file_id;
    
    string data_extn = index_extn;
    data_extn[2] = 'b';
    
    map<string,string> meta;
    
    m_Impl = new CWriteDB_Column(basename,
                                 index_extn,
                                 data_extn,
                                 0,
                                 title,
                                 meta,
                                 0);
}

CWriteDB_ColumnBuilder::~CWriteDB_ColumnBuilder()
{
    delete m_Impl;
}

void CWriteDB_ColumnBuilder::ListFiles(vector<string> & files) const
{
    m_Impl->ListFiles(files, false);
}

void CWriteDB_ColumnBuilder::AddBlob(const CBlastDbBlob & blob)
{
    return m_Impl->AddBlob(blob);
}

void CWriteDB_ColumnBuilder::AddMetaData(const string & key, const string & value)
{
    return m_Impl->AddMetaData(key, value);
}

void CWriteDB_ColumnBuilder::Close()
{
    m_Impl->RenameSingle();
    m_Impl->Close();
}
#endif

END_NCBI_SCOPE

