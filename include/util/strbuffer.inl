#if defined(STRBUFFER__HPP)  &&  !defined(STRBUFFER__INL)
#define STRBUFFER__INL

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
* Revision 1.8  2005/04/26 14:11:04  vasilche
* Implemented optimized reading methods CSkipExpected*() and GetChars(string&).
*
* Revision 1.7  2005/02/23 21:06:13  vasilche
* Added HasMore().
*
* Revision 1.6  2004/08/30 18:14:23  gouriano
* use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.5  2004/05/24 18:12:44  gouriano
* In text output files make indentation optional
*
* Revision 1.4  2001/08/15 20:53:09  juran
* Heed warnings.
*
* Revision 1.3  2001/03/14 17:59:24  vakatov
* COStreamBuffer::  renamed GetFreeSpace() -> GetAvailableSpace()
* to avoid clash with MS-Win system headers' #define
*
* Revision 1.2  2001/01/05 20:08:53  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.1  2000/12/26 22:23:45  vasilche
* Fixed errors of compilation on Mac.
*
* ===========================================================================
*/

inline
bool CIStreamBuffer::fail(void) const
{
    return m_Error != 0;
}

inline
void CIStreamBuffer::ResetFail(void)
{
    m_Error = 0;
}

inline
const char* CIStreamBuffer::GetError(void) const
{
    return m_Error;
}

inline
char CIStreamBuffer::PeekChar(size_t offset)
    THROWS1((CIOException, bad_alloc))
{
    char* pos = m_CurrentPos + offset;
    if ( pos >= m_DataEndPos )
        pos = FillBuffer(pos);
    return *pos;
}

inline
char CIStreamBuffer::PeekCharNoEOF(size_t offset)
{
    char* pos = m_CurrentPos + offset;
    if ( pos >= m_DataEndPos )
        return FillBufferNoEOF(pos);
    return *pos;
}

inline
bool CIStreamBuffer::SkipExpectedChar(char c, size_t offset)
{
    char* pos = m_CurrentPos+offset;
    if ( pos >= m_DataEndPos )
        pos = FillBuffer(pos);
    if ( *pos != c )
        return false;
    m_CurrentPos = pos+1;
    return true;
}

inline
bool CIStreamBuffer::SkipExpectedChars(char c1, char c2, size_t offset)
{
    char* pos = m_CurrentPos+offset+1;
    if ( pos >= m_DataEndPos )
        pos = FillBuffer(pos);
    if ( pos[-1] != c1 || pos[0] != c2 )
        return false;
    m_CurrentPos = pos+1;
    return true;
}

inline
bool CIStreamBuffer::HasMore(void)
{
    return m_CurrentPos < m_DataEndPos || TryToFillBuffer();
}

inline
char CIStreamBuffer::GetChar(void)
    THROWS1((CIOException, bad_alloc))
{
    char* pos = m_CurrentPos;
    if ( pos >= m_DataEndPos )
        pos = FillBuffer(pos);
    m_CurrentPos = pos + 1;
    return *pos;
}

inline
void CIStreamBuffer::UngetChar(char _DEBUG_ARG(c))
{
    char* pos = m_CurrentPos;
    _ASSERT(pos > m_Buffer);
    _ASSERT(pos[-1] == c);
    m_CurrentPos = pos - 1;
}

inline
void CIStreamBuffer::SkipChars(size_t count)
{
    _ASSERT(m_CurrentPos + count > m_CurrentPos);
    _ASSERT(m_CurrentPos + count <= m_DataEndPos);
    m_CurrentPos += count;
}

inline
void CIStreamBuffer::SkipChar(void)
{
    SkipChars(1);
}

inline
const char* CIStreamBuffer::GetCurrentPos(void) const
    THROWS1_NONE
{
    return m_CurrentPos;
}

inline
size_t CIStreamBuffer::GetLine(void) const
    THROWS1_NONE
{
    return m_Line;
}

inline
CNcbiStreamoff CIStreamBuffer::GetStreamOffset(void) const
    THROWS1_NONE
{
    return m_BufferOffset + (m_CurrentPos - m_Buffer);
}

inline
bool COStreamBuffer::fail(void) const
{
    return m_Error != 0;
}

inline
void COStreamBuffer::ResetFail(void)
{
    m_Error = 0;
}

inline
const char* COStreamBuffer::GetError(void) const
{
    return m_Error;
}

inline
size_t COStreamBuffer::GetLine(void) const
    THROWS1_NONE
{
    return m_Line;
}

inline
CNcbiStreamoff COStreamBuffer::GetStreamOffset(void) const
    THROWS1_NONE
{
    return m_BufferOffset + (m_CurrentPos - m_Buffer);
}

inline
size_t COStreamBuffer::GetCurrentLineLength(void) const
    THROWS1_NONE
{
    return m_LineLength;
}

inline
bool COStreamBuffer::ZeroIndentLevel(void) const
    THROWS1_NONE
{
    return m_IndentLevel == 0;
}

inline
size_t COStreamBuffer::GetIndentLevel(size_t step) const
    THROWS1_NONE
{
    return m_IndentLevel / step;
}

inline
void COStreamBuffer::IncIndentLevel(size_t step)
    THROWS1_NONE
{
    m_IndentLevel += step;
}

inline
void COStreamBuffer::DecIndentLevel(size_t step)
    THROWS1_NONE
{
    _ASSERT(m_IndentLevel >= step);
    m_IndentLevel -= step;
}

inline
void COStreamBuffer::SetBackLimit(size_t limit)
{
    m_BackLimit = limit;
}

inline
char* COStreamBuffer::DoSkip(size_t reserve)
    THROWS1((CIOException, bad_alloc))
{
    char* pos = DoReserve(reserve);
    m_CurrentPos = pos + reserve;
    m_LineLength += reserve;
    return pos;
}

inline
char* COStreamBuffer::Skip(size_t count)
    THROWS1((CIOException, bad_alloc))
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

inline
char* COStreamBuffer::Reserve(size_t count)
    THROWS1((CIOException, bad_alloc))
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

inline
void COStreamBuffer::PutChar(char c)
    THROWS1((CIOException))
{
    *Skip(1) = c;
}

inline
void COStreamBuffer::BackChar(char _DEBUG_ARG(c))
{
    _ASSERT(m_CurrentPos > m_Buffer);
    --m_CurrentPos;
    _ASSERT(*m_CurrentPos == c);
}

inline
void COStreamBuffer::PutString(const char* str, size_t length)
    THROWS1((CIOException, bad_alloc))
{
    if ( length < 1024 ) {
        memcpy(Skip(length), str, length);
    }
    else {
        Write(str, length);
    }
}

inline
void COStreamBuffer::PutString(const char* str)
    THROWS1((CIOException, bad_alloc))
{
    PutString(str, strlen(str));
}

inline
void COStreamBuffer::PutString(const string& str)
    THROWS1((CIOException, bad_alloc))
{
    PutString(str.data(), str.size());
}

inline
void COStreamBuffer::PutIndent(void)
    THROWS1((CIOException, bad_alloc))
{
    if (GetUseIndentation()) {
        size_t count = m_IndentLevel;
        memset(Skip(count), ' ', count);
    }
}

inline
void COStreamBuffer::PutEol(bool indent)
    THROWS1((CIOException, bad_alloc))
{
    char* pos = Reserve(1);
    *pos = '\n';
    m_CurrentPos = pos + 1;
    ++m_Line;
    m_LineLength = 0;
    if ( indent )
        PutIndent();
}

inline
void COStreamBuffer::WrapAt(size_t lineLength, bool keepWord)
    THROWS1((CIOException, bad_alloc))
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

inline
size_t COStreamBuffer::GetUsedSpace(void) const
{
    return static_cast<size_t>(m_CurrentPos - m_Buffer);
}

inline
size_t COStreamBuffer::GetAvailableSpace(void) const
{
    return static_cast<size_t>(m_BufferEnd - m_CurrentPos);
}

inline
size_t COStreamBuffer::GetBufferSize(void) const
{
    return static_cast<size_t>(m_BufferEnd - m_Buffer);
}

inline
void COStreamBuffer::SetUseIndentation(bool set)
{
    m_UseIndentation = set;
}

inline
bool COStreamBuffer::GetUseIndentation(void) const
{
    return m_UseIndentation;
}


#endif /* def STRBUFFER__HPP  &&  ndef STRBUFFER__INL */
