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
*   Input buffer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.26  2001/05/17 15:07:15  lavr
* Typos corrected
*
* Revision 1.25  2001/05/11 13:58:52  grichenk
* Removed "while" reading loop in CIStreamBuffer::FillBuffer()
*
* Revision 1.24  2001/04/17 21:47:29  vakatov
* COStreamBuffer::Flush() -- try to flush the underlying output stream
* regardless of its state (clear the state temporarily before flushing,
* restore it afterwards).
*
* Revision 1.23  2001/03/14 17:59:26  vakatov
* COStreamBuffer::  renamed GetFreeSpace() -> GetAvailableSpace()
* to avoid clash with MS-Win system headers' #define
*
* Revision 1.22  2001/01/05 20:09:05  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.21  2000/12/15 15:38:46  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.20  2000/10/20 15:51:44  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.19  2000/10/13 20:59:21  vasilche
* Avoid using of ssize_t absent on some compilers.
*
* Revision 1.18  2000/10/13 20:22:57  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.17  2000/08/15 19:44:51  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.16  2000/07/03 18:42:47  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.15  2000/06/16 20:01:26  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.14  2000/06/01 19:07:05  vasilche
* Added parsing of XML data.
*
* Revision 1.13  2000/05/24 20:08:50  vasilche
* Implemented XML dump.
*
* Revision 1.12  2000/05/03 14:38:14  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.11  2000/04/28 16:58:14  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.10  2000/04/13 14:50:28  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.9  2000/04/06 16:11:01  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.8  2000/03/29 15:55:29  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.7  2000/03/10 21:16:47  vasilche
* Removed EOF workaround code.
*
* Revision 1.6  2000/03/10 17:59:21  vasilche
* Fixed error reporting.
* Added EOF bug workaround on MIPSpro compiler (not finished).
*
* Revision 1.5  2000/03/07 14:06:24  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.4  2000/02/17 20:02:45  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.3  2000/02/11 17:10:25  vasilche
* Optimized text parsing.
*
* Revision 1.2  2000/02/02 19:07:41  vasilche
* Added THROWS_NONE to constructor/destructor of exception.
*
* Revision 1.1  2000/02/01 21:47:23  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
*
* ===========================================================================
*/

#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/strbuffer.hpp>
#include <util/bytesrc.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

static const size_t KInitialBufferSize = 4096;

static inline
size_t BiggerBufferSize(size_t size) THROWS1_NONE
{
    return size * 2;
}

CIStreamBuffer::CIStreamBuffer(void)
    THROWS1((bad_alloc))
    : m_Error(0), m_BufferOffset(0),
      m_BufferSize(KInitialBufferSize), m_Buffer(new char[KInitialBufferSize]),
      m_CurrentPos(m_Buffer), m_DataEndPos(m_Buffer),
      m_Line(1),
      m_CollectPos(0)
{
}

CIStreamBuffer::~CIStreamBuffer(void)
{
    delete[] m_Buffer;
}

void CIStreamBuffer::Open(const CRef<CByteSourceReader>& reader)
{
    m_Input = reader;
    m_Error = 0;
}

void CIStreamBuffer::Close(void)
{
    m_Input.Reset();
    m_BufferOffset = 0;
    m_CurrentPos = m_Buffer;
    m_DataEndPos = m_Buffer;
    m_Line = 1;
    m_Error = 0;
}

void CIStreamBuffer::StartSubSource(void)
{
    _ASSERT(!m_Collector);
    _ASSERT(!m_CollectPos);

    m_CollectPos = m_CurrentPos;
    m_Collector = m_Input->SubSource(m_DataEndPos - m_CurrentPos);
}

CRef<CByteSource> CIStreamBuffer::EndSubSource(void)
{
    _ASSERT(m_Collector);
    _ASSERT(m_CollectPos);

    _ASSERT(m_CollectPos <= m_CurrentPos);
    if ( m_CurrentPos != m_CollectPos )
        m_Collector->AddChunk(m_CollectPos, m_CurrentPos - m_CollectPos);

    CRef<CByteSource> source = m_Collector->GetSource();

    m_CollectPos = 0;
    m_Collector.Reset();

    return source;
}

// this method is highly optimized
char CIStreamBuffer::SkipSpaces(void)
    THROWS1((CIOException))
{
    // cache pointers
    char* pos = m_CurrentPos;
    char* end = m_DataEndPos;
    // make sure thire is at least one char in buffer
    if ( pos == end ) {
        // fill buffer
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
    // main cycle
    // at the beginning:
    //     pos == m_CurrentPos
    //     end == m_DataEndPos
    //     pos < end
    for (;;) {
        // we use do{}while() cycle because
        // condition is true at the beginning ( pos < end )
        do {
            // cache current char
            char c = *pos;
            if ( c != ' ' ) { // it's not space (' ')
                // point m_CurrentPos to first non space char
                m_CurrentPos = pos;
                // return char value
                return c;
            }
            // skip space char
        } while ( ++pos < end );
        // here pos == end == m_DataEndPos
        // point m_CurrentPos to end of buffer
        m_CurrentPos = pos;
        // fill next portion
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
}

// this method is highly optimized
void CIStreamBuffer::FindChar(char c)
    THROWS1((CIOException))
{
    // cache pointers
    char* pos = m_CurrentPos;
    char* end = m_DataEndPos;
    // make sure thire is at least one char in buffer
    if ( pos == end ) {
        // fill buffer
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
    // main cycle
    // at the beginning:
    //     pos == m_CurrentPos
    //     end == m_DataEndPos
    //     pos < end
    for (;;) {
        char* found = static_cast<char*>(memchr(pos, c, end - pos));
        if ( found ) {
            m_CurrentPos = found;
            return;
        }
        // point m_CurrentPos to end of buffer
        m_CurrentPos = end;
        // fill next portion
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
}

// this method is highly optimized
size_t CIStreamBuffer::PeekFindChar(char c, size_t limit)
    THROWS1((CIOException))
{
    _ASSERT(limit > 0);
    PeekCharNoEOF(limit - 1);
    // cache pointers
    char* pos = m_CurrentPos;
    size_t bufferSize = m_DataEndPos - pos;
    if ( bufferSize != 0 ) {
        char* found =
            static_cast<char*>(memchr(pos, c, min(limit, bufferSize)));
        if ( found )
            return found - pos;
    }
    return limit;
}

char* CIStreamBuffer::FillBuffer(char* pos, bool noEOF)
    THROWS1((CIOException, bad_alloc))
{
    _ASSERT(pos >= m_DataEndPos);
    // remove unused portion of buffer at the beginning
    _ASSERT(m_CurrentPos >= m_Buffer);
    size_t erase = m_CurrentPos - m_Buffer;
    if ( erase > 0 ) {
        char* newPos = m_CurrentPos - erase;
        if ( m_Collector ) {
            _ASSERT(m_CollectPos);
            size_t count = m_CurrentPos - m_CollectPos;
            if ( count > 0 )
                m_Collector->AddChunk(m_CollectPos, count);
            m_CollectPos = newPos;
        }
        memmove(newPos, m_CurrentPos, m_DataEndPos - m_CurrentPos);
        m_CurrentPos = newPos;
        m_DataEndPos -= erase;
        m_BufferOffset += erase;
        pos -= erase;
    }
    size_t dataSize = m_DataEndPos - m_Buffer;
    size_t newPosOffset = pos - m_Buffer;
    if ( newPosOffset >= m_BufferSize ) {
        // reallocate buffer
        size_t newSize = BiggerBufferSize(m_BufferSize);
        while ( newPosOffset >= newSize ) {
            newSize = BiggerBufferSize(newSize);
        }
        char* newBuffer = new char[newSize];
        memcpy(newBuffer, m_Buffer, dataSize);
        m_CurrentPos = newBuffer + (m_CurrentPos - m_Buffer);
        if ( m_CollectPos )
            m_CollectPos = newBuffer + (m_CollectPos - m_Buffer);
        pos = newBuffer + newPosOffset;
        m_DataEndPos = newBuffer + dataSize;
        delete[] m_Buffer;
        m_Buffer = newBuffer;
        m_BufferSize = newSize;
    }
    size_t load = m_BufferSize - dataSize;
    if ( load > 0 ) {
        size_t count = m_Input->Read(m_DataEndPos, load);
        if ( count == 0 ) {
            if ( pos < m_DataEndPos )
                return pos;
            if ( m_Input->EndOfData() ) {
                if ( noEOF ) {
                    // ignore EOF
                    _ASSERT(m_Buffer <= m_CurrentPos);
                    _ASSERT(m_CurrentPos <= pos);
                    _ASSERT(m_DataEndPos <= m_Buffer + m_BufferSize);
                    _ASSERT(!m_CollectPos ||
                            (m_CollectPos>=m_Buffer &&
                             m_CollectPos<=m_CurrentPos));
                    return pos;
                }
                m_Error = "end of file";
                THROW0_TRACE(CEofException());
            }
            else {
                m_Error = "read fault";
                THROW1_TRACE(CIOException, "read fault");
            }
        }
        m_DataEndPos += count;
        load -= count;
    }
    _ASSERT(m_Buffer <= m_CurrentPos);
    _ASSERT(m_CurrentPos <= pos);
    _ASSERT(pos < m_DataEndPos);
    _ASSERT(m_DataEndPos <= m_Buffer + m_BufferSize);
    _ASSERT(!m_CollectPos || (m_CollectPos>=m_Buffer && m_CollectPos<=m_CurrentPos));
    return pos;
}

char CIStreamBuffer::FillBufferNoEOF(char* pos)
    THROWS1((CIOException, bad_alloc))
{
    pos = FillBuffer(pos, true);
    if ( pos >= m_DataEndPos )
        return 0;
    else
        return *pos;
}

void CIStreamBuffer::GetChars(char* buffer, size_t count)
    THROWS1((CIOException))
{
    // cache pos
    char* pos = m_CurrentPos;
    for ( ;; ) {
        size_t c = m_DataEndPos - pos;
        if ( c >= count ) {
            // all data is already in buffer -> copy it
            memcpy(buffer, pos, count);
            m_CurrentPos = pos + count;
            return;
        }
        else {
            memcpy(buffer, pos, c);
            buffer += c;
            pos += c;
            count -= c;
            pos = FillBuffer(pos);
        }
    }
}

void CIStreamBuffer::GetChars(size_t count)
    THROWS1((CIOException))
{
    // cache pos
    char* pos = m_CurrentPos;
    for ( ;; ) {
        size_t c = m_DataEndPos - pos;
        if ( c >= count ) {
            // all data is already in buffer -> skip it
            m_CurrentPos = pos + count;
            return;
        }
        else {
            pos += c;
            count -= c;
            pos = FillBuffer(pos);
        }
    }
}

void CIStreamBuffer::SkipEndOfLine(char lastChar)
    THROWS1((CIOException))
{
    _ASSERT(lastChar == '\n' || lastChar == '\r');
    _ASSERT(m_CurrentPos > m_Buffer && m_CurrentPos[-1] == lastChar);
    m_Line++;
    char nextChar;
    try {
        nextChar = PeekChar();
    }
    catch ( CEofException& /* ignored */ ) {
        return;
    }
    // lastChar either '\r' or \n'
    // if nextChar is compliment, skip it
    if ( (lastChar + nextChar) == ('\r' + '\n') )
        SkipChar();
}

size_t CIStreamBuffer::ReadLine(char* buff, size_t size)
    THROWS1((CIOException))
{
    size_t count = 0;
    try {
        while ( size > 0 ) {
            char c = *buff++ = GetChar();
            count++;
            size--;
            switch ( c ) {
            case '\r':
                // replace leading '\r' by '\n'
                buff[-1] = '\n';
                if ( PeekChar() == '\n' )
                    SkipChar();
                return count;
            case '\n':
                if ( PeekChar() == '\r' )
                    SkipChar();
                return count;
            }
        }
        return count;
    }
    catch ( CEofException& /*ignored*/ ) {
        return count;
    }
}

void CIStreamBuffer::BadNumber(void)
{
    m_Error = "bad number";
    THROW1_TRACE(runtime_error, "bad number");
}

Int4 CIStreamBuffer::GetInt4(void)
{
    bool sign;
    char c = GetChar();
    switch ( c ) {
    case '-':
        sign = true;
        c = GetChar();
        break;
    case '+':
        sign = false;
        c = GetChar();
        break;
    default:
        sign = false;
        break;
    }
    if ( c < '0' || c > '9' )
        BadNumber();

    Int4 n = c - '0';
    for ( ;; ) {
        c = PeekCharNoEOF();
        if  ( c < '0' || c > '9' )
            break;

        SkipChar();
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    if ( sign )
        return -n;
    else
        return n;
}

Uint4 CIStreamBuffer::GetUint4(void)
{
    char c = GetChar();
    if ( c == '+' )
        c = GetChar();
    if ( c < '0' || c > '9' )
        BadNumber();

    Uint4 n = c - '0';
    for ( ;; ) {
        c = PeekCharNoEOF();
        if  ( c < '0' || c > '9' )
            break;

        SkipChar();
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    return n;
}

Int8 CIStreamBuffer::GetInt8(void)
    THROWS1((CIOException))
{
    bool sign;
    char c = GetChar();
    switch ( c ) {
    case '-':
        sign = true;
        c = GetChar();
        break;
    case '+':
        sign = false;
        c = GetChar();
        break;
    default:
        sign = false;
        break;
    }
    if ( c < '0' || c > '9' )
        BadNumber();

    Int8 n = c - '0';
    for ( ;; ) {
        c = PeekCharNoEOF();
        if  ( c < '0' || c > '9' )
            break;

        SkipChar();
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    if ( sign )
        return -n;
    else
        return n;
}

Uint8 CIStreamBuffer::GetUint8(void)
    THROWS1((CIOException))
{
    char c = GetChar();
    if ( c == '+' )
        c = GetChar();
    if ( c < '0' || c > '9' )
        BadNumber();

    Uint8 n = c - '0';
    for ( ;; ) {
        c = PeekCharNoEOF();
        if  ( c < '0' || c > '9' )
            break;

        SkipChar();
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    return n;
}

COStreamBuffer::COStreamBuffer(CNcbiOstream& out, bool deleteOut)
    THROWS1((bad_alloc))
    : m_Output(out), m_DeleteOutput(deleteOut), m_Error(0),
      m_IndentLevel(0), m_BufferOffset(0),
      m_Buffer(new char[KInitialBufferSize]),
      m_CurrentPos(m_Buffer),
      m_BufferEnd(m_Buffer + KInitialBufferSize),
      m_Line(1), m_LineLength(0),
      m_BackLimit(0)
{
}

COStreamBuffer::~COStreamBuffer(void)
    THROWS1((CIOException))
{
    Close();
    delete[] m_Buffer;
}

void COStreamBuffer::Close(void)
{
    Flush();
    if ( m_DeleteOutput )
        delete &m_Output;
    m_DeleteOutput = false;
    m_Error = 0;
    m_IndentLevel = 0;
    m_CurrentPos = m_Buffer;
    m_Line = 1;
    m_LineLength = 0;
}

void COStreamBuffer::FlushBuffer(bool fullBuffer)
    THROWS1((CIOException))
{
    size_t used = GetUsedSpace();
    size_t count;
    size_t leave;
    if ( fullBuffer ) {
        count = used;
        leave = 0;
    }
    else {
        leave = m_BackLimit;
        if ( used < leave )
            return; // none to flush
        count = used - leave;
    }
    if ( count != 0 ) {
        if ( !m_Output.write(m_Buffer, count) ) {
            m_Error = "write fault";
            THROW1_TRACE(CIOException, "write fault");
        }
        if ( leave != 0 ) {
            memmove(m_Buffer, m_Buffer + count, leave);
            m_CurrentPos -= count;
        }
        else {
            m_CurrentPos = m_Buffer;
        }
        m_BufferOffset += count;
    }
}


void COStreamBuffer::Flush(void)
    THROWS1((CIOException))
{
    FlushBuffer();
    IOS_BASE::iostate state = m_Output.rdstate();
    m_Output.clear();
    try {
        if ( !m_Output.flush() )
            THROW1_TRACE(CIOException, "COStreamBuffer::Flush() failed");
    } catch (...) {
        m_Output.clear(state);
        throw;
    }
    m_Output.clear(state);
}


char* COStreamBuffer::DoReserve(size_t count)
    THROWS1((CIOException, bad_alloc))
{
    FlushBuffer(false);
    size_t usedSize = m_CurrentPos - m_Buffer;
    size_t needSize = usedSize + count;
    size_t bufferSize = m_BufferEnd - m_Buffer;
    if ( bufferSize < needSize ) {
        // realloc too small buffer
        do {
            bufferSize = BiggerBufferSize(bufferSize);
        } while ( bufferSize < needSize );
        if ( usedSize == 0 ) {
            delete[] m_Buffer;
            m_CurrentPos = m_Buffer = new char[bufferSize];
            m_BufferEnd = m_Buffer + bufferSize;
        }
        else {
            char* oldBuffer = m_Buffer;
            m_Buffer = new char[bufferSize];
            m_BufferEnd = m_Buffer + bufferSize;
            memcpy(m_Buffer, oldBuffer, usedSize);
            delete[] oldBuffer;
            m_CurrentPos = m_Buffer + usedSize;
        }
    }
    return m_CurrentPos;
}

void COStreamBuffer::PutInt4(Int4 v)
    THROWS1((CIOException, bad_alloc))
{
    const size_t BSIZE = (sizeof(v)*CHAR_BIT) / 3 + 2;
    char b[BSIZE];
    char* pos = b + BSIZE;
    if ( v == 0 ) {
        *--pos = '0';
    }
    else {
        bool sign = v < 0;
        if ( sign )
            v = -v;
        
        do {
            *--pos = char('0' + (v % 10));
            v /= 10;
        } while ( v );
        
        if ( sign )
            *--pos = '-';
    }
    PutString(pos, b + BSIZE - pos);
}

void COStreamBuffer::PutUint4(Uint4 v)
    THROWS1((CIOException, bad_alloc))
{
    const size_t BSIZE = (sizeof(v)*CHAR_BIT) / 3 + 2;
    char b[BSIZE];
    char* pos = b + BSIZE;
    if ( v == 0 ) {
        *--pos = '0';
    }
    else {
        do {
            *--pos = char('0' + (v % 10));
            v /= 10;
        } while ( v );
    }
    PutString(pos, b + BSIZE - pos);
}

void COStreamBuffer::PutInt8(Int8 v)
    THROWS1((CIOException, bad_alloc))
{
    const size_t BSIZE = (sizeof(v)*CHAR_BIT) / 3 + 2;
    char b[BSIZE];
    char* pos = b + BSIZE;
    if ( v == 0 ) {
        *--pos = '0';
    }
    else {
        bool sign = v < 0;
        if ( sign )
            v = -v;
        
        do {
            *--pos = char('0' + (v % 10));
            v /= 10;
        } while ( v );
        
        if ( sign )
            *--pos = '-';
    }
    PutString(pos, b + BSIZE - pos);
}

void COStreamBuffer::PutUint8(Uint8 v)
    THROWS1((CIOException, bad_alloc))
{
    const size_t BSIZE = (sizeof(v)*CHAR_BIT) / 3 + 2;
    char b[BSIZE];
    char* pos = b + BSIZE;
    if ( v == 0 ) {
        *--pos = '0';
    }
    else {
        do {
            *--pos = char('0' + (v % 10));
            v /= 10;
        } while ( v );
    }
    PutString(pos, b + BSIZE - pos);
}

void COStreamBuffer::PutEolAtWordEnd(size_t lineLength)
    THROWS1((CIOException, bad_alloc))
{
    Reserve(1);
    size_t linePos = m_LineLength;
    char* pos = m_CurrentPos;
    bool goodPlace = false;
    while ( pos > m_Buffer && linePos > 0 ) {
        --pos;
        --linePos;
        if ( linePos <= lineLength && (isspace(*pos) || *pos == '\'') ) {
            goodPlace = true;
            break;
        }
        else if ( *pos == '\n' || *pos == '"' ) {
            // no suitable space found
            break;
        }
    }
    if ( !goodPlace ) {
        // no suitable space found
        if ( linePos < lineLength ) {
            pos += lineLength - linePos;
            linePos = lineLength;
        }
        // assure we will not break double ""
        while ( pos > m_Buffer && *(pos - 1) == '"' ) {
            --pos;
            --linePos;
        }
        if ( pos == m_Buffer ) {
            // it's possible that we already put some " before...
            while ( pos < m_CurrentPos && *pos == '"' ) {
                ++pos;
                ++linePos;
            }
        }
    }
    // split there
    // insert '\n'
    size_t count = m_CurrentPos - pos;
    memmove(pos + 1, pos, count);
    m_LineLength = count;
    ++m_CurrentPos;
    *pos = '\n';
    ++m_Line;
}

void COStreamBuffer::Write(const char* data, size_t dataLength)
    THROWS1((CIOException, bad_alloc))
{
    while ( dataLength > 0 ) {
        size_t available = GetAvailableSpace();
        if ( available == 0 ) {
            FlushBuffer(false);
            available = GetAvailableSpace();
        }
        if ( available >= dataLength )
            break; // current chunk will fit in buffer
        memcpy(m_CurrentPos, data, available);
        m_CurrentPos += available;
        data += available;
        dataLength -= available;
    }
    memcpy(m_CurrentPos, data, dataLength);
    m_CurrentPos += dataLength;
}

void COStreamBuffer::Write(const CRef<CByteSourceReader>& reader)
    THROWS1((CIOException, bad_alloc))
{
    for ( ;; ) {
        size_t available = GetAvailableSpace();
        if ( available == 0 ) {
            FlushBuffer(false);
            available = GetAvailableSpace();
        }
        size_t count = reader.GetObject().Read(m_CurrentPos, available);
        if ( count == 0 ) {
            if ( reader->EndOfData() )
                return;
            else
                THROW1_TRACE(CIOException, "buffer read fault");
        }
        m_CurrentPos += count;
    }
}

CIOException::CIOException(const string& msg) THROWS_NONE
    : runtime_error(msg)
{
}

CIOException::~CIOException(void) THROWS_NONE
{
}

CEofException::CEofException(void) THROWS_NONE
    : CIOException("end of file")
{
}

CEofException::~CEofException(void) THROWS_NONE
{
}

END_NCBI_SCOPE
