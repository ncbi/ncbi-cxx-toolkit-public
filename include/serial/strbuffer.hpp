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
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class CEofException : public runtime_error
{
public:
    CEofException(void) THROWS_NONE;
    ~CEofException(void) THROWS_NONE;
};

class CStreamBuffer
{
public:
    CStreamBuffer(CNcbiIstream& in);
    ~CStreamBuffer(void);

    char PeekChar(size_t offset = 0)
        {
            size_t pos = m_CurrentPos + offset;
            if ( pos >= m_DataEndPos )
                pos = FillBuffer(pos);
            return m_Buffer[pos];
        }
    char GetChar(void)
        {
            size_t pos = m_CurrentPos;
            if ( pos >= m_DataEndPos )
                pos = FillBuffer(pos);
            m_CurrentPos = pos + 1;
            return m_Buffer[pos];
        }
    void UngetChar(void)
        {
            size_t pos = m_CurrentPos;
            _ASSERT(pos > 0);
            m_CurrentPos = pos - 1;
        }
    void SkipChars(size_t count)
        {
            m_CurrentPos += count;
            _ASSERT(m_CurrentPos <= m_DataEndPos);
        }
    void SkipChar(void)
        {
            SkipChars(1);
        }
    void SkipEndOfLine(char lastChar);

    void MarkPos(void)
        {
            m_MarkPos = m_CurrentPos;
        }
    const char* GetMarkPos(void) const
        {
            return m_Buffer + m_MarkPos;
        }

    size_t GetLine(void) const
        {
            return m_Line;
        }

    size_t ReadLine(char* buff, size_t size);

protected:
    size_t FillBuffer(size_t pos);

private:
    
    CNcbiIstream& m_Input;
    size_t m_BufferSize;
    char* m_Buffer;
    size_t m_CurrentPos;
    size_t m_DataEndPos;
    size_t m_MarkPos;
    size_t m_Line;
};

END_NCBI_SCOPE

#endif
