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
#include <serial/strbuffer.hpp>

BEGIN_NCBI_SCOPE

static const size_t KInitialBufferSize = 8192;

static inline
size_t BiggerBufferSize(size_t size)
{
    return size * 2;
}

CStreamBuffer::CStreamBuffer(CNcbiIstream& in)
    : m_Input(in), m_BufferOffset(0),
      m_BufferSize(KInitialBufferSize), m_Buffer(new char[KInitialBufferSize]),
      m_CurrentPos(m_Buffer), m_DataEndPos(m_Buffer),
      m_Line(1)
{
}

CStreamBuffer::~CStreamBuffer(void)
{
    delete[] m_Buffer;
}

// this method is highly optimized
char CStreamBuffer::SkipSpaces(void)
    THROWS((CSerialIOException))
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

char* CStreamBuffer::FillBuffer(char* pos)
    THROWS((CSerialIOException))
{
    _ASSERT(pos >= m_DataEndPos);
    // remove unused portion of buffer at the beginning
    _ASSERT(m_CurrentPos >= m_Buffer);
    size_t erase = m_CurrentPos - m_Buffer;
    if ( erase > 0 ) {
        char* newPos = m_CurrentPos - erase;
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
        pos = newBuffer + newPosOffset;
        m_DataEndPos = newBuffer + dataSize;
        delete[] m_Buffer;
        m_Buffer = newBuffer;
        m_BufferSize = newSize;
    }
    size_t load = m_BufferSize - dataSize;
    while ( load > 0 ) {
        m_Input.read(m_DataEndPos, load);
        size_t count = m_Input.gcount();
        if ( count == 0 ) {
            if ( pos < m_DataEndPos )
                return pos;
            if ( m_Input.eof() )
                THROW0_TRACE(CSerialEofException());
            else
                THROW1_TRACE(CSerialIOException, "read fault");
        }
        m_DataEndPos += count;
        load -= count;
    }
    _ASSERT(m_Buffer <= m_CurrentPos);
    _ASSERT(m_CurrentPos <= pos);
    _ASSERT(pos < m_DataEndPos);
    _ASSERT(m_DataEndPos <= m_Buffer + m_BufferSize);
    return pos;
}

void CStreamBuffer::GetChars(char* buffer, size_t count)
    THROWS((CSerialIOException))
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

void CStreamBuffer::GetChars(size_t count)
    THROWS((CSerialIOException))
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

void CStreamBuffer::SkipEndOfLine(char lastChar)
    THROWS((CSerialIOException))
{
    _ASSERT(lastChar == '\n' || lastChar == '\r');
    _ASSERT(m_CurrentPos > m_Buffer && m_CurrentPos[-1] == lastChar);
    m_Line++;
    char nextChar;
    try {
        nextChar = PeekChar();
    }
    catch ( CSerialEofException& /* ignored */ ) {
        return;
    }
    // lastChar either '\r' or \n'
    // if nextChar is compliment, skip it
    if ( (lastChar + nextChar) == ('\r' + '\n') )
        SkipChar();
}

size_t CStreamBuffer::ReadLine(char* buff, size_t size)
    THROWS((CSerialIOException))
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
    catch ( CSerialEofException& /*ignored*/ ) {
        return count;
    }
}

END_NCBI_SCOPE
