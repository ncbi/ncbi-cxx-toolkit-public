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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/04/28 16:58:11  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/bytesrc.hpp>

BEGIN_NCBI_SCOPE

typedef CFileSourceCollector::TFilePos TFilePos;
typedef CFileSourceCollector::TFileOff TFileOff;

CByteSource::~CByteSource(void)
{
}

CByteSourceReader::~CByteSourceReader(void)
{
}

CRef<CSubSourceCollector> CByteSourceReader::SubSource(char* buffer,
                                                       size_t bufferLength)
{
    return new CMemorySourceCollector(eCanDelete, buffer, bufferLength);
}

CSubSourceCollector::~CSubSourceCollector(void)
{
}

static inline
unsigned IFStreamFlags(bool binary)
{
    return binary? (IOS_BASE::in | IOS_BASE::binary): IOS_BASE::in;
}

bool CByteSourceReader::EndOfData(void) const
{
    return true;
}

CRef<CByteSourceReader> CStreamByteSource::Open(void)
{
    return new CStreamByteSourceReader(eCanDelete, this, m_Stream);
}

size_t CStreamByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    m_Stream->read(buffer, bufferLength);
    return m_Stream->gcount();
}

bool CStreamByteSourceReader::EndOfData(void) const
{
    return m_Stream->eof();
}

CFStreamByteSource::CFStreamByteSource(ECanDelete canDelete,
                                       const string& fileName, bool binary)
    : CStreamByteSource(canDelete,
                        *new CNcbiIfstream(fileName.c_str(),
                                           IFStreamFlags(binary)))
{
}

CFStreamByteSource::~CFStreamByteSource(void)
{
    delete m_Stream;
}

CFileByteSource::CFileByteSource(ECanDelete canDelete,
                                 const string& fileName, bool binary)
    : CByteSource(canDelete),
      m_FileName(fileName), m_Binary(binary)
{
}

CFileByteSource::CFileByteSource(ECanDelete canDelete,
                                 const CFileByteSource& file)
    : CByteSource(canDelete),
      m_FileName(file.m_FileName), m_Binary(file.m_Binary)
{
}

CRef<CByteSourceReader> CFileByteSource::Open(void)
{
    return new CFileByteSourceReader(eCanDelete, this);
}

CSubFileByteSource::CSubFileByteSource(ECanDelete canDelete, 
                                       const CFileByteSource& file,
                                       TFilePos start, TFileOff length)
    : CParent(canDelete, file),
      m_Start(start), m_Length(length)
{
}

CRef<CByteSourceReader> CSubFileByteSource::Open(void)
{
    return new CSubFileByteSourceReader(eCanDelete, this, m_Start, m_Length);
}

CFileByteSourceReader::CFileByteSourceReader(ECanDelete canDelete,
                                             const CFileByteSource* source)
    : CStreamByteSourceReader(canDelete, source, 0),
      m_FileSource(source),
      m_FStream(source->GetFileName().c_str(),
                IFStreamFlags(source->IsBinary()))
{
    m_Stream = &m_FStream;
}

CSubFileByteSourceReader::CSubFileByteSourceReader(ECanDelete canDelete, 
                                                   const CFileByteSource* s,
                                                   TFilePos start,
                                                   TFileOff length)
    : CParent(canDelete, s), m_Length(length)
{
    m_Stream->seekg(start);
}

size_t CSubFileByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    if ( bufferLength > m_Length )
        bufferLength = size_t(m_Length);
    size_t count = CParent::Read(buffer, bufferLength);
    m_Length -= count;
    return count;
}

bool CSubFileByteSourceReader::EndOfData(void) const
{
    return m_Length == 0;
}

CRef<CSubSourceCollector> CFileByteSourceReader::SubSource(char* /*buffer*/,
                                                           size_t bufferLength)
{
    return new CFileSourceCollector(eCanDelete, m_FileSource,
                                    m_Stream->tellg(), bufferLength);
}

CFileSourceCollector::CFileSourceCollector(ECanDelete canDelete,
                                           const CConstRef<CFileByteSource>& s,
                                           TFilePos start, size_t addBytes)
    : CSubSourceCollector(canDelete),
      m_FileSource(s),
      m_Start(start - addBytes), m_Length(addBytes)
{
}

void CFileSourceCollector::AddChunk(const char* /*buffer*/,
                                    size_t bufferLength)
{
    m_Length += bufferLength;
}

void CFileSourceCollector::ReduceLastChunkBy(size_t unused)
{
    m_Length -= unused;
}

CRef<CByteSource> CFileSourceCollector::GetSource(void)
{
    return new CSubFileByteSource(eCanDelete, *m_FileSource,
                                  m_Start, m_Length);
}


CMemoryChunk::CMemoryChunk(ECanDelete canDelete,
                           const char* data, size_t dataSize,
                           CRef<CMemoryChunk>& prevChunk)
    : CObject(canDelete), m_Data(new char[dataSize]), m_DataSize(dataSize)
{
    memcpy(m_Data, data, dataSize);
    if ( prevChunk )
        prevChunk->m_NextChunk = this;
    prevChunk = this;
}

CMemoryChunk::~CMemoryChunk(void)
{
    delete m_Data;
}

void CMemoryChunk::ReduceBy(size_t unused)
{
    _ASSERT(GetDataSize() >= unused);
    if ( unused != 0 ) {
        size_t oldSize = m_DataSize;
        char* oldData = m_Data;
        size_t newSize = oldSize - unused;
        char* newData = new char[newSize];
        memcpy(newData, oldData, newSize);
        m_Data = newData;
        m_DataSize = newSize;
        delete[] oldData;
    }
}

CMemoryByteSource::~CMemoryByteSource(void)
{
}

CRef<CByteSourceReader> CMemoryByteSource::Open(void)
{
    return new CMemoryByteSourceReader(eCanDelete, m_Bytes);
}

size_t CMemoryByteSourceReader::GetCurrentChunkAvailable(void) const
{
    return m_CurrentChunk->GetDataSize() - m_CurrentChunkOffset;
}

size_t CMemoryByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    while ( m_CurrentChunk ) {
        size_t avail = GetCurrentChunkAvailable();
        if ( avail == 0 ) {
            // end of current chunk
            CConstRef<CMemoryChunk> rest = m_CurrentChunk->GetNextChunk();
            m_CurrentChunk = rest;
            m_CurrentChunkOffset = 0;
        }
        else {
            size_t c = min(bufferLength, avail);
            memcpy(buffer, m_CurrentChunk->GetData(m_CurrentChunkOffset), c);
            m_CurrentChunkOffset += c;
            return c;
        }
    }
    return 0;
}

bool CMemoryByteSourceReader::EndOfData(void) const
{
    return !m_CurrentChunk;
}

CMemorySourceCollector::CMemorySourceCollector(ECanDelete canDelete, 
                                               const char* buffer,
                                               size_t bufferLength)
    : CSubSourceCollector(canDelete)
{
    AddChunk(buffer, bufferLength);
}

void CMemorySourceCollector::AddChunk(const char* buffer,
                                      size_t bufferLength)
{
    m_LastChunk = new CMemoryChunk(eCanDelete, buffer, bufferLength,
                                   m_LastChunk);
    if ( !m_FirstChunk )
        m_FirstChunk = m_LastChunk;
}

void CMemorySourceCollector::ReduceLastChunkBy(size_t unused)
{
    _ASSERT(m_LastChunk);
    m_LastChunk->ReduceBy(unused);
}

CRef<CByteSource> CMemorySourceCollector::GetSource(void)
{
    return new CMemoryByteSource(eCanDelete, m_FirstChunk);
}

END_NCBI_SCOPE
