#ifndef STRBUFFER__HPP
#define STRBUFFER__HPP

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
*   Reading buffer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2000/05/24 20:08:16  vasilche
* Implemented XML dump.
*
* Revision 1.10  2000/05/03 14:38:06  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.9  2000/04/28 16:58:03  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.8  2000/04/13 14:50:18  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.7  2000/04/10 21:01:40  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.6  2000/04/06 16:10:52  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.5  2000/03/07 14:05:33  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.4  2000/02/17 20:02:29  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.3  2000/02/11 17:10:20  vasilche
* Optimized text parsing.
*
* Revision 1.2  2000/02/02 19:07:38  vasilche
* Added THROWS_NONE to constructor/destructor of exception.
*
* Revision 1.1  2000/02/01 21:44:36  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/exception.hpp>
#include <string.h>

BEGIN_NCBI_SCOPE

class CByteSource;
class CByteSourceReader;
class CSubSourceCollector;

class CIStreamBuffer
{
public:
    CIStreamBuffer(void)
        THROWS((bad_alloc));
    ~CIStreamBuffer(void);

    void Open(const CRef<CByteSourceReader>& reader);
    void Close(void);

    char PeekChar(size_t offset = 0)
        THROWS((CSerialIOException, bad_alloc))
        {
            _ASSERT(offset >= 0);
            char* pos = m_CurrentPos + offset;
            if ( pos >= m_DataEndPos )
                pos = FillBuffer(pos);
            return *pos;
        }
    char PeekCharNoEOF(size_t offset = 0)
        THROWS((CSerialIOException, bad_alloc))
        {
            _ASSERT(offset >= 0);
            char* pos = m_CurrentPos + offset;
            if ( pos >= m_DataEndPos )
                return FillBufferNoEOF(pos);
            return *pos;
        }
    char GetChar(void) THROWS((CSerialIOException, bad_alloc))
        {
            char* pos = m_CurrentPos;
            if ( pos >= m_DataEndPos )
                pos = FillBuffer(pos);
            m_CurrentPos = pos + 1;
            return *pos;
        }
    // precondition: GetChar or SkipChar was last method called
    void UngetChar(void)
        {
            char* pos = m_CurrentPos;
            _ASSERT(pos > m_Buffer);
            m_CurrentPos = pos - 1;
        }
    // precondition: PeekChar(c) was called when c >= count
    void SkipChars(size_t count)
        {
            _ASSERT(m_CurrentPos + count > m_CurrentPos);
            _ASSERT(m_CurrentPos + count <= m_DataEndPos);
            m_CurrentPos += count;
        }
    // equivalent of SkipChars(1)
    void SkipChar(void)
        {
            SkipChars(1);
        }

    // read chars in buffer
    void GetChars(char* buffer, size_t count)
        THROWS((CSerialIOException));
    // skip chars which may not be in buffer
    void GetChars(size_t count)
        THROWS((CSerialIOException));

    // precondition: last char extracted was either '\r' or '\n'
    // action: inctement line count and
    //         extract next complimentary '\r' or '\n' char if any
    void SkipEndOfLine(char lastChar)
        THROWS((CSerialIOException));
    // action: skip all spaces (' ') and return next non space char
    //         without extracting it
    char SkipSpaces(void)
        THROWS((CSerialIOException));

    const char* GetCurrentPos(void) const THROWS_NONE
        {
            return m_CurrentPos;
        }

    // return: current line counter
    size_t GetLine(void) const THROWS_NONE
        {
            return m_Line;
        }
    size_t GetStreamOffset(void) const THROWS_NONE
        {
            return m_BufferOffset + (m_CurrentPos - m_Buffer);
        }

    // action: read in buffer up to end of line
    size_t ReadLine(char* buff, size_t size)
        THROWS((CSerialIOException));

    void StartSubSource(void);
    CRef<CByteSource> EndSubSource(void);

protected:
    // action: fill buffer so *pos char is valid
    // return: new value of pos pointer if buffer content was shifted
    char* FillBuffer(char* pos, bool noEOF = false)
        THROWS((CSerialIOException, bad_alloc));
    char FillBufferNoEOF(char* pos)
        THROWS((CSerialIOException, bad_alloc));

private:
    CRef<CByteSourceReader> m_Input;

    size_t m_BufferOffset;    // offset of current buffer in source stream
    size_t m_BufferSize;      // buffer size
    char* m_Buffer;           // buffer pointer
    char* m_CurrentPos;       // current char position in buffer
    char* m_DataEndPos;       // end of valid content in buffer
    size_t m_Line;            // current line counter

    char* m_CollectPos;
    CRef<CSubSourceCollector> m_Collector;
};

class COStreamBuffer
{
public:
    COStreamBuffer(CNcbiOstream& out, bool deleteOut = false)
        THROWS((bad_alloc));
    ~COStreamBuffer(void) THROWS((CSerialIOException));

    size_t GetCurrentLineNumber(void) const THROWS_NONE
        {
            return m_Line;
        }

    size_t GetCurrentLineLength(void) const THROWS_NONE
        {
            return m_LineLength;
        }

    size_t GetIndentLevel(size_t step = 2) const THROWS_NONE
        {
            return m_IndentLevel / step;
        }
    void IncIndentLevel(size_t step = 2) THROWS_NONE
        {
            m_IndentLevel += step;
        }
    void DecIndentLevel(size_t step = 2) THROWS_NONE
        {
            _ASSERT(m_IndentLevel >= step);
            m_IndentLevel -= step;
        }

    void SetBackLimit(size_t limit)
        {
            m_BackLimit = limit;
        }

    void FlushBuffer(bool fullBuffer = true) THROWS((CSerialIOException));
    void Flush(void) THROWS((CSerialIOException));

protected:
    // flush contents of buffer to underlying stream
    // make sure 'reserve' char area is available in buffer
    // return beginning of area
    char* DoReserve(size_t reserve = 0)
        THROWS((CSerialIOException, bad_alloc));
    // flush contents of buffer to underlying stream
    // make sure 'reserve' char area is available in buffer
    // skip 'reserve' chars
    // return beginning of skipped area
    char* DoSkip(size_t reserve)
        THROWS((CSerialIOException, bad_alloc))
        {
            char* pos = DoReserve(reserve);
            m_CurrentPos = pos + reserve;
            m_LineLength += reserve;
            return pos;
        }

    // allocates count bytes area in buffer and skip this area
    // returns beginning of this area
    char* Skip(size_t count)
        THROWS((CSerialIOException, bad_alloc))
        {
            char* pos = m_CurrentPos;
            char* end = pos + count;
            if ( end <= m_BufferEnd ) {
                // enough space in buffer
                m_CurrentPos = end;
                m_LineLength += count;
                return pos;
            }
            else {
                return DoSkip(count);
            }
        }
    char* Reserve(size_t count)
        THROWS((CSerialIOException, bad_alloc))
        {
            char* pos = m_CurrentPos;
            char* end = pos + count;
            if ( end <= m_BufferEnd ) {
                // enough space in buffer
                return pos;
            }
            else {
                return DoReserve(count);
            }
        }

public:
    void PutChar(char c)
        THROWS((CSerialIOException))
        {
            *Skip(1) = c;
        }
    void BackChar(char _DEBUG_ARG(c))
        {
            _ASSERT(m_CurrentPos > m_Buffer);
            --m_CurrentPos;
            _ASSERT(*m_CurrentPos == c);
        }

    void PutString(const char* str, size_t length)
        THROWS((CSerialIOException, bad_alloc))
        {
            if ( length < 1024 ) {
                memcpy(Skip(length), str, length);
            }
            else {
                Write(str, length);
            }
        }
    void PutString(const char* str)
        THROWS((CSerialIOException, bad_alloc))
        {
            PutString(str, strlen(str));
        }
    void PutString(const string& str)
        THROWS((CSerialIOException, bad_alloc))
        {
            PutString(str.data(), str.size());
        }

    void PutIndent(void)
        THROWS((CSerialIOException, bad_alloc))
        {
            size_t count = m_IndentLevel;
            memset(Skip(count), ' ', count);
        }

    void PutEol(bool indent = true)
        THROWS((CSerialIOException, bad_alloc))
        {
            char* pos = Reserve(1);
            *pos = '\n';
            m_CurrentPos = pos + 1;
            ++m_Line;
            m_LineLength = 0;
            if ( indent )
                PutIndent();
        }

    void PutEolAtWordEnd(size_t lineLength)
        THROWS((CSerialIOException, bad_alloc));

    void WrapAt(size_t lineLength, bool keepWord)
        THROWS((CSerialIOException, bad_alloc))
        {
            if ( keepWord ) {
                if ( GetCurrentLineLength() > lineLength )
                    PutEolAtWordEnd(lineLength);
            }
            else {
                if ( GetCurrentLineLength() >= lineLength )
                    PutEol(false);
            }
        }


    void PutInt(int v)
        THROWS((CSerialIOException, bad_alloc));
    void PutUInt(unsigned v)
        THROWS((CSerialIOException, bad_alloc));
    void PutLong(long v)
        THROWS((CSerialIOException, bad_alloc));
    void PutULong(unsigned long v)
        THROWS((CSerialIOException, bad_alloc));

    void Write(const char* data, size_t dataLength)
        THROWS((CSerialIOException, bad_alloc));
    void Write(const CRef<CByteSourceReader>& reader)
        THROWS((CSerialIOException, bad_alloc));

private:
    CNcbiOstream& m_Output;
    bool m_DeleteOutput;

    size_t GetUsedSpace(void) const
        {
            return m_CurrentPos - m_Buffer;
        }
    size_t GetFreeSpace(void) const
        {
            return m_BufferEnd - m_CurrentPos;
        }
    size_t GetBufferSize(void) const
        {
            return m_BufferEnd - m_Buffer;
        }

    size_t m_IndentLevel;

    char* m_Buffer;           // buffer pointer
    char* m_CurrentPos;       // current char position in buffer
    char* m_BufferEnd;       // end of valid content in buffer
    size_t m_Line;            // current line counter
    size_t m_LineLength;
    size_t m_BackLimit;
};

END_NCBI_SCOPE

#endif
