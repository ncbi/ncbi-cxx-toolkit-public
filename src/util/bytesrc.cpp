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
* Revision 1.13  2001/05/29 19:35:23  grichenk
* Fixed non-blocking stream reading for GCC
*
* Revision 1.12  2001/05/17 15:07:15  lavr
* Typos corrected
*
* Revision 1.11  2001/05/16 17:55:40  grichenk
* Redesigned support for non-blocking stream read operations
*
* Revision 1.10  2001/05/11 20:41:19  grichenk
* Added support for non-blocking stream reading
*
* Revision 1.9  2001/05/11 14:00:21  grichenk
* CStreamByteSourceReader::Read() -- use readsome() instead of read()
*
* Revision 1.8  2001/01/05 20:09:05  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.7  2000/11/01 20:38:37  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.6  2000/09/01 13:16:14  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.5  2000/07/03 18:42:42  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.4  2000/06/22 16:13:48  thiessen
* change IFStreamFlags to macro
*
* Revision 1.3  2000/05/03 14:38:13  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.2  2000/04/28 19:14:37  vasilche
* Fixed stream position and offset typedefs.
*
* Revision 1.1  2000/04/28 16:58:11  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <util/bytesrc.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

typedef CFileSourceCollector::TFilePos TFilePos;
typedef CFileSourceCollector::TFileOff TFileOff;

CByteSource::~CByteSource(void)
{
}

CByteSourceReader::~CByteSourceReader(void)
{
}

CRef<CSubSourceCollector> CByteSourceReader::SubSource(size_t /*prevent*/)
{
    return new CMemorySourceCollector();
}

CSubSourceCollector::~CSubSourceCollector(void)
{
}

/* Mac compiler doesn't like getting these flags as unsigned int (thiessen)
static inline
unsigned IFStreamFlags(bool binary)
{
    return binary? (IOS_BASE::in | IOS_BASE::binary): IOS_BASE::in;
}
*/
#define IFStreamFlags(isBinary) \
  (isBinary? (IOS_BASE::in | IOS_BASE::binary): IOS_BASE::in)

bool CByteSourceReader::EndOfData(void) const
{
    return true;
}

CRef<CByteSourceReader> CStreamByteSource::Open(void)
{
    return new CStreamByteSourceReader(this, m_Stream);
}

size_t CStreamByteSourceReader::Read(char* buffer, size_t bufferLength)
{
#ifdef NCBI_COMPILER_GCC
    m_Stream->read(buffer, bufferLength);
    size_t count = m_Stream->gcount();
    if (count  &&  m_Stream->eof()) {
        m_Stream->clear(ios::eofbit);
    }
    return count;
#else
    size_t n = m_Stream->readsome(buffer, bufferLength);
    if (n != 0  ||  m_Stream->eof())
        return n;
    m_Stream->read(buffer, 1);
    return m_Stream->readsome(buffer+1, bufferLength-1) + 1;
#endif
}

bool CStreamByteSourceReader::EndOfData(void) const
{
    return m_Stream->eof();
}

CFStreamByteSource::CFStreamByteSource(const string& fileName, bool binary)
    : CStreamByteSource(*new CNcbiIfstream(fileName.c_str(),
                                           IFStreamFlags(binary)))
{
    if ( !*m_Stream )
        THROW1_TRACE(runtime_error, "file not found: "+fileName);
}

CFStreamByteSource::~CFStreamByteSource(void)
{
    delete m_Stream;
}

CFileByteSource::CFileByteSource(const string& fileName, bool binary)
    : m_FileName(fileName), m_Binary(binary)
{
}

CFileByteSource::CFileByteSource(const CFileByteSource& file)
    : m_FileName(file.m_FileName), m_Binary(file.m_Binary)
{
}

CRef<CByteSourceReader> CFileByteSource::Open(void)
{
    return new CFileByteSourceReader(this);
}

CSubFileByteSource::CSubFileByteSource(const CFileByteSource& file,
                                       TFilePos start, TFileOff length)
    : CParent(file), m_Start(start), m_Length(length)
{
}

CRef<CByteSourceReader> CSubFileByteSource::Open(void)
{
    return new CSubFileByteSourceReader(this, m_Start, m_Length);
}

CFileByteSourceReader::CFileByteSourceReader(const CFileByteSource* source)
    : CStreamByteSourceReader(source, 0),
      m_FileSource(source),
      m_FStream(source->GetFileName().c_str(),
                IFStreamFlags(source->IsBinary()))
{
    if ( !m_FStream )
        THROW1_TRACE(runtime_error, "file not found: "+source->GetFileName());
    m_Stream = &m_FStream;
}

CSubFileByteSourceReader::CSubFileByteSourceReader(const CFileByteSource* s,
                                                   TFilePos start,
                                                   TFileOff length)
    : CParent(s), m_Length(length)
{
    m_Stream->seekg(start);
}

size_t CSubFileByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    if ( TFileOff(bufferLength) > m_Length )
        bufferLength = size_t(m_Length);
    size_t count = CParent::Read(buffer, bufferLength);
    m_Length -= TFileOff(count);
    return count;
}

bool CSubFileByteSourceReader::EndOfData(void) const
{
    return m_Length == 0;
}

CRef<CSubSourceCollector> CFileByteSourceReader::SubSource(size_t prepend)
{
    return new CFileSourceCollector(m_FileSource,
                                    m_Stream->tellg() - TFileOff(prepend));
}

CFileSourceCollector::CFileSourceCollector(const CConstRef<CFileByteSource>& s,
                                           TFilePos start)
    : m_FileSource(s),
      m_Start(start), m_Length(0)
{
}

void CFileSourceCollector::AddChunk(const char* /*buffer*/,
                                    size_t bufferLength)
{
    m_Length += TFileOff(bufferLength);
}

CRef<CByteSource> CFileSourceCollector::GetSource(void)
{
    return new CSubFileByteSource(*m_FileSource,
                                  m_Start, m_Length);
}


CMemoryChunk::CMemoryChunk(const char* data, size_t dataSize,
                           CRef<CMemoryChunk>& prevChunk)
    : m_Data(new char[dataSize]), m_DataSize(dataSize)
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

CMemoryByteSource::~CMemoryByteSource(void)
{
}

CRef<CByteSourceReader> CMemoryByteSource::Open(void)
{
    return new CMemoryByteSourceReader(m_Bytes);
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

void CMemorySourceCollector::AddChunk(const char* buffer,
                                      size_t bufferLength)
{
    m_LastChunk = new CMemoryChunk(buffer, bufferLength, m_LastChunk);
    if ( !m_FirstChunk )
        m_FirstChunk = m_LastChunk;
}

CRef<CByteSource> CMemorySourceCollector::GetSource(void)
{
    return new CMemoryByteSource(m_FirstChunk);
}

END_NCBI_SCOPE
