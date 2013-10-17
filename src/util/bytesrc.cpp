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
 *   Implementation of CByteSource and CByteSourceReader
 *   and their specializations'.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/stream_utils.hpp>
#include <util/bytesrc.hpp>
#include <util/util_exception.hpp>
#include <util/error_codes.hpp>
#include <algorithm>


#define NCBI_USE_ERRCODE_X   Util_ByteSrc


BEGIN_NCBI_SCOPE


typedef CFileSourceCollector::TFilePos TFilePos;
typedef CFileSourceCollector::TFileOff TFileOff;

/////////////////////////////////////////////////////////////////////////////
// CByteSource
/////////////////////////////////////////////////////////////////////////////

CByteSource::CByteSource(void)
{
}


CByteSource::~CByteSource(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// CByteSourceReader
/////////////////////////////////////////////////////////////////////////////

CByteSourceReader::CByteSourceReader(void)
{
}


CByteSourceReader::~CByteSourceReader(void)
{
}


bool CByteSourceReader::EndOfData(void) const
{
    return true;
}


bool CByteSourceReader::Pushback(const char* /*data*/, size_t size)
{
    if ( size ) {
        ERR_POST_X(1, "CByteSourceReader::Pushback: "
                      "unable to push back " << size << " byte(s)");
        return false;
    }
    return true;
}

void CByteSourceReader::Seekg(CNcbiStreampos /* pos */)
{
    NCBI_THROW(CUtilException,eWrongCommand,"CByteSourceReader::Seekg: unable to seek");
}

CRef<CSubSourceCollector>
CByteSourceReader::SubSource(size_t /*prevent*/,
                             CRef<CSubSourceCollector> parent)
{
    return CRef<CSubSourceCollector>(new CMemorySourceCollector(parent));
}


/////////////////////////////////////////////////////////////////////////////
// CSubSourceCollector
/////////////////////////////////////////////////////////////////////////////


CSubSourceCollector::CSubSourceCollector(CRef<CSubSourceCollector> parent)
    : m_ParentCollector(parent)
{
}


CSubSourceCollector::~CSubSourceCollector(void)
{
}


void CSubSourceCollector::AddChunk(const char* buffer, size_t bufferLength)
{
    if ( m_ParentCollector ) {
        m_ParentCollector->AddChunk(buffer, bufferLength);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CStreamByteSource
/////////////////////////////////////////////////////////////////////////////

/* Mac compiler doesn't like getting these flags as unsigned int (thiessen)
static inline
unsigned IFStreamFlags(bool binary)
{
    return binary ? (IOS_BASE::in | IOS_BASE::binary) : IOS_BASE::in;
}
*/
#define IFStreamFlags(isBinary) \
  (isBinary? (IOS_BASE::in | IOS_BASE::binary): IOS_BASE::in)


CStreamByteSource::CStreamByteSource(CNcbiIstream& in)
    : m_Stream(&in)
{
}


CStreamByteSource::~CStreamByteSource(void)
{
}


CRef<CByteSourceReader> CStreamByteSource::Open(void)
{
    return CRef<CByteSourceReader>
        (new CStreamByteSourceReader(this, m_Stream));
}


/////////////////////////////////////////////////////////////////////////////
// CStreamByteSourceReader
/////////////////////////////////////////////////////////////////////////////

CStreamByteSourceReader::CStreamByteSourceReader(const CByteSource* source,
                                                 CNcbiIstream* stream)
    : m_Source(source), m_Stream(stream)
{
}


CStreamByteSourceReader::~CStreamByteSourceReader(void)
{
}


size_t CStreamByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    return (size_t)CStreamUtils::Readsome(*m_Stream, buffer, bufferLength);
}


bool CStreamByteSourceReader::EndOfData(void) const
{
    return m_Stream->eof();
}


bool CStreamByteSourceReader::Pushback(const char* data, size_t size)
{
    CStreamUtils::Stepback(*m_Stream, (CT_CHAR_TYPE*) data, (streamsize) size);
    m_Stream->clear();
    return true;
}


void CStreamByteSourceReader::Seekg(CNcbiStreampos pos)
{
    m_Stream->clear();
    m_Stream->seekg(pos);
    if (m_Stream->fail()) {
        NCBI_THROW(CIOException, eRead, "Failed to set read position");
    }
}


/////////////////////////////////////////////////////////////////////////////
// CIRByteSourceReader
/////////////////////////////////////////////////////////////////////////////

CIRByteSourceReader::CIRByteSourceReader(IReader* reader)
    : m_Reader(reader), m_EOF(false)
{
}


CIRByteSourceReader::~CIRByteSourceReader(void)
{
}


size_t CIRByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    size_t bytes_read;
    ERW_Result result = m_Reader->Read(buffer, bufferLength, &bytes_read);
    if ( result == eRW_Eof ) {
        m_EOF = true;
    }
    return bytes_read;
}


bool CIRByteSourceReader::EndOfData(void) const
{
    return m_EOF;
}


/////////////////////////////////////////////////////////////////////////////
// CFStreamByteSource
/////////////////////////////////////////////////////////////////////////////

CFStreamByteSource::CFStreamByteSource(CNcbiIstream& in)
    : CStreamByteSource(in)
{
}


CFStreamByteSource::CFStreamByteSource(const string& fileName, bool binary)
    : CStreamByteSource(*new CNcbiIfstream(fileName.c_str(),
                                           IFStreamFlags(binary)))
{
    if ( !*m_Stream ) {
        NCBI_THROW(CUtilException,eNoInput,"file not found: " + fileName);
    }
}


CFStreamByteSource::~CFStreamByteSource(void)
{
    delete m_Stream;
}


/////////////////////////////////////////////////////////////////////////////
// CFileByteSource
/////////////////////////////////////////////////////////////////////////////

CFileByteSource::CFileByteSource(const string& fileName, bool binary)
    : m_FileName(fileName), m_Binary(binary)
{
    return;
}


CFileByteSource::CFileByteSource(const CFileByteSource& file)
    : m_FileName(file.m_FileName), m_Binary(file.m_Binary)
{
    return;
}


CFileByteSource::~CFileByteSource(void)
{
}


CRef<CByteSourceReader> CFileByteSource::Open(void)
{
    return CRef<CByteSourceReader>(new CFileByteSourceReader(this));
}


/////////////////////////////////////////////////////////////////////////////
// CSubFileByteSource
/////////////////////////////////////////////////////////////////////////////

CSubFileByteSource::CSubFileByteSource(const CFileByteSource& file,
                                       TFilePos start, TFileOff length)
    : CParent(file), m_Start(start), m_Length(length)
{
    return;
}


CSubFileByteSource::~CSubFileByteSource(void)
{
}


CRef<CByteSourceReader> CSubFileByteSource::Open(void)
{
    return CRef<CByteSourceReader>
        (new CSubFileByteSourceReader(this, m_Start, m_Length));
}

/////////////////////////////////////////////////////////////////////////////
// CFileByteSourceReader
/////////////////////////////////////////////////////////////////////////////

CFileByteSourceReader::CFileByteSourceReader(const CFileByteSource* source)
    : CStreamByteSourceReader(source, 0),
      m_FileSource(source),
      m_FStream(source->GetFileName().c_str(),
                IFStreamFlags(source->IsBinary()))
{
    if ( !m_FStream ) {
//        THROW1_TRACE(runtime_error, "file not found: " +
//                     source->GetFileName());
        NCBI_THROW(CUtilException,eNoInput,
                   "file not found: " +source->GetFileName());
    }
    m_Stream = &m_FStream;
}


CFileByteSourceReader::~CFileByteSourceReader(void)
{
}


CRef<CSubSourceCollector> 
CFileByteSourceReader::SubSource(size_t prepend, 
                                 CRef<CSubSourceCollector> parent)
{
    return CRef<CSubSourceCollector>(new CFileSourceCollector(m_FileSource,
                                    m_Stream->tellg() - TFileOff(prepend),
                                    parent));
}

/////////////////////////////////////////////////////////////////////////////
// CSubFileByteSourceReader
/////////////////////////////////////////////////////////////////////////////

CSubFileByteSourceReader::CSubFileByteSourceReader(const CFileByteSource* s,
                                                   TFilePos start,
                                                   TFileOff length)
    : CParent(s), m_Length(length)
{
    m_Stream->seekg(start);
}


CSubFileByteSourceReader::~CSubFileByteSourceReader(void)
{
}


size_t CSubFileByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    if ( TFileOff(bufferLength) > m_Length ) {
        bufferLength = size_t(m_Length);
    }
    size_t count = CParent::Read(buffer, bufferLength);
    m_Length -= TFileOff(count);
    return count;
}


bool CSubFileByteSourceReader::EndOfData(void) const
{
    return m_Length == 0;
}


/////////////////////////////////////////////////////////////////////////////
// CFileSourceCollector
/////////////////////////////////////////////////////////////////////////////


CFileSourceCollector::CFileSourceCollector(CConstRef<CFileByteSource> s,
                                           TFilePos start,
                                           CRef<CSubSourceCollector> parent)
    : CSubSourceCollector(parent),
      m_FileSource(s),
      m_Start(start),
      m_Length(0)
{
}


CFileSourceCollector::~CFileSourceCollector(void)
{
}


void CFileSourceCollector::AddChunk(const char* buffer,
                                    size_t bufferLength)
{
    CSubSourceCollector::AddChunk(buffer, bufferLength);
    m_Length += TFileOff(bufferLength);
}


CRef<CByteSource> CFileSourceCollector::GetSource(void)
{
    return CRef<CByteSource>(new CSubFileByteSource(*m_FileSource,
                                  m_Start, m_Length));
}


/////////////////////////////////////////////////////////////////////////////
// CMemoryChunk
/////////////////////////////////////////////////////////////////////////////

CMemoryChunk::CMemoryChunk(const char* data, size_t dataSize,
                           CRef<CMemoryChunk> prevChunk)
    : m_Data(new char[dataSize]),
      m_DataSize(dataSize)
{
    memcpy(m_Data, data, dataSize);
    if ( prevChunk ) {
        prevChunk->m_NextChunk = this;
    }
}


CMemoryChunk::~CMemoryChunk(void)
{
    delete[] m_Data;
    CRef<CMemoryChunk> next = m_NextChunk;
    m_NextChunk.Reset();
    while ( next && next->ReferencedOnlyOnce() ) {
        CRef<CMemoryChunk> cur = next;
        next = cur->m_NextChunk;
        cur->m_NextChunk.Reset();
    }
}


/////////////////////////////////////////////////////////////////////////////
// CMemoryByteSource
/////////////////////////////////////////////////////////////////////////////

CMemoryByteSource::CMemoryByteSource(CConstRef<CMemoryChunk> bytes)
    : m_Bytes(bytes)
{
}


CMemoryByteSource::~CMemoryByteSource(void)
{
}


CRef<CByteSourceReader> CMemoryByteSource::Open(void)
{
    return CRef<CByteSourceReader>(new CMemoryByteSourceReader(m_Bytes));
}


/////////////////////////////////////////////////////////////////////////////
// CMemoryByteSourceReader
/////////////////////////////////////////////////////////////////////////////


CMemoryByteSourceReader::CMemoryByteSourceReader(CConstRef<CMemoryChunk> bytes)
    : m_CurrentChunk(bytes), m_CurrentChunkOffset(0)
{
}


CMemoryByteSourceReader::~CMemoryByteSourceReader(void)
{
}


size_t CMemoryByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    while ( m_CurrentChunk ) {
        size_t avail = GetCurrentChunkAvailable();
        if ( avail == 0 ) {
            // End of current chunk
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


/////////////////////////////////////////////////////////////////////////////
// CMemorySourceCollector
/////////////////////////////////////////////////////////////////////////////

CMemorySourceCollector::CMemorySourceCollector(CRef<CSubSourceCollector>
                                               parent)
    : CSubSourceCollector(parent)
{
}


CMemorySourceCollector::~CMemorySourceCollector(void)
{
}


void CMemorySourceCollector::AddChunk(const char* buffer,
                                      size_t bufferLength)
{
    CSubSourceCollector::AddChunk(buffer, bufferLength);
    m_LastChunk = new CMemoryChunk(buffer, bufferLength, m_LastChunk);
    if ( !m_FirstChunk ) {
        m_FirstChunk = m_LastChunk;
    }
}


CRef<CByteSource> CMemorySourceCollector::GetSource(void)
{
    return CRef<CByteSource>(new CMemoryByteSource(m_FirstChunk));
}


/////////////////////////////////////////////////////////////////////////////
// CWriterSourceCollector
/////////////////////////////////////////////////////////////////////////////

CWriterSourceCollector::CWriterSourceCollector(IWriter*    writer, 
                                               EOwnership  own, 
                                       CRef<CSubSourceCollector>  parent)
    : CSubSourceCollector(parent),
      m_Writer(writer),
      m_Own(own)
{
    return;
}


CWriterSourceCollector::~CWriterSourceCollector()
{
    if ( m_Own ) {
        delete m_Writer;
    }
}


void CWriterSourceCollector::SetWriter(IWriter*   writer, 
                                       EOwnership own)
{
    if ( m_Own ) {
        delete m_Writer;
    }
    m_Writer = writer;
    m_Own = own;    
}


void CWriterSourceCollector::AddChunk(const char* buffer, size_t bufferLength)
{
    CSubSourceCollector::AddChunk(buffer, bufferLength);
    while ( bufferLength ) {
        size_t written;
        m_Writer->Write(buffer, bufferLength, &written);
        buffer += written;
        bufferLength -= written;
    }
}


CRef<CByteSource> CWriterSourceCollector::GetSource(void)
{
    // Return NULL byte source, this happens because we cannot derive
    // any readers from IWriter (one way interface)
    return CRef<CByteSource>();
}


/////////////////////////////////////////////////////////////////////////////
// CWriterByteSourceReader
/////////////////////////////////////////////////////////////////////////////

CWriterByteSourceReader::CWriterByteSourceReader(CNcbiIstream* stream,
                                                 IWriter* writer)
    : CStreamByteSourceReader(0 /* CByteSource* */, stream),
      m_Writer(writer)
{
    _ASSERT(writer);
}


CWriterByteSourceReader::~CWriterByteSourceReader(void)
{
}


CRef<CSubSourceCollector> 
CWriterByteSourceReader::SubSource(size_t /*prepend*/,
                                   CRef<CSubSourceCollector> parent)
{
    return 
        CRef<CSubSourceCollector>(
            new CWriterSourceCollector(m_Writer, eNoOwnership, parent));
}


/////////////////////////////////////////////////////////////////////////////
// CWriterCopyByteSourceReader
/////////////////////////////////////////////////////////////////////////////

CWriterCopyByteSourceReader::CWriterCopyByteSourceReader(CByteSourceReader* reader, IWriter* writer)
    : m_Reader(reader),
      m_Writer(writer)
{
    _ASSERT(reader);
    _ASSERT(writer);
}


CWriterCopyByteSourceReader::~CWriterCopyByteSourceReader(void)
{
}


size_t CWriterCopyByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    return m_Reader->Read(buffer, bufferLength);
}


bool CWriterCopyByteSourceReader::EndOfData(void) const
{
    return m_Reader->EndOfData();
}


CRef<CSubSourceCollector>
CWriterCopyByteSourceReader::SubSource(size_t /*prepend*/,
                                       CRef<CSubSourceCollector> parent)
{
    return 
        CRef<CSubSourceCollector>(
            new CWriterSourceCollector(m_Writer, eNoOwnership, parent));
}


END_NCBI_SCOPE
