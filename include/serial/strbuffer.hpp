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

#include <corelib/ncbistre.hpp>
#include <corelib/ncbidbg.hpp>
#include <serial/exception.hpp>

BEGIN_NCBI_SCOPE

class CStreamBuffer
{
public:
    CStreamBuffer(CNcbiIstream& in);
    ~CStreamBuffer(void);

    char PeekChar(size_t offset = 0) THROWS((CSerialIOException))
        {
            _ASSERT(offset >= 0);
            char* pos = m_CurrentPos + offset;
            if ( pos >= m_DataEndPos )
                pos = FillBuffer(pos);
            return *pos;
        }
    char GetChar(void) THROWS((CSerialIOException))
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
    void GetChars(char* buffer, size_t count) THROWS((CSerialIOException));
    // skip chars which may not be in buffer
    void GetChars(size_t count) THROWS((CSerialIOException));

    // precondition: last char extracted was either '\r' or '\n'
    // action: inctement line count and
    //         extract next complimentary '\r' or '\n' char if any
    void SkipEndOfLine(char lastChar) THROWS((CSerialIOException));
    // action: skip all spaces (' ') and return next non space char
    //         without extracting it
    char SkipSpaces(void) THROWS((CSerialIOException));

    const char* GetCurrentPos(void) const
        {
            return m_CurrentPos;
        }

    // return: current line counter
    size_t GetLine(void) const
        {
            return m_Line;
        }
    size_t GetStreamOffset(void) const
        {
            return m_BufferOffset + (m_CurrentPos - m_Buffer);
        }

    // action: read in buffer up to end of line
    size_t ReadLine(char* buff, size_t size) THROWS((CSerialIOException));

protected:
    // action: fill buffer so *pos char is valid
    // return: new value of pos pointer if buffer content was shifted
    char* FillBuffer(char* pos) THROWS((CSerialIOException));

private:
    CNcbiIstream& m_Input;    // source stream
    size_t m_BufferOffset;   // offset of current buffer in source stream
    size_t m_BufferSize;      // buffer size
    char* m_Buffer;           // buffer pointer
    char* m_CurrentPos;       // current char position in buffer
    char* m_DataEndPos;       // end of valid content in buffer
    size_t m_Line;            // current line counter
};

END_NCBI_SCOPE

#endif
