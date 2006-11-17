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
* Author:  Aaron Ucko
*
* File Description:
*   Lightweight interface for getting lines of data with minimal
*   memory copying.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <util/line_reader.hpp>

BEGIN_NCBI_SCOPE

bool CStreamLineReader::AtEOF(void) const
{
    return m_Stream.eof()  ||  CT_EQ_INT_TYPE(m_Stream.peek(), CT_EOF);
}


char CStreamLineReader::PeekChar(void) const
{
    return m_Stream.peek();
}


CStreamLineReader& CStreamLineReader::operator++(void)
{
    NcbiGetlineEOL(m_Stream, m_Line);
    return *this;
}


CTempString CStreamLineReader::operator*(void) const
{
    return CTempString(m_Line);
}

CT_POS_TYPE CStreamLineReader::GetPosition(void) const
{
    return m_Stream.tellg();
}


CMemoryLineReader::CMemoryLineReader(CMemoryFile* mem_file,
                                     EOwnership ownership)
    : m_Start(static_cast<char*>(mem_file->GetPtr())),
      m_End(m_Start + mem_file->GetSize()),
      m_Pos(m_Start)
{
    mem_file->MemMapAdvise(CMemoryFile::eMMA_Sequential);
    if (ownership == eTakeOwnership) {
        m_MemFile.reset(mem_file);
    }
}

bool CMemoryLineReader::AtEOF(void) const
{
    return m_Pos >= m_End;
}

char CMemoryLineReader::PeekChar(void) const
{
    return *m_Pos;
}

CMemoryLineReader& CMemoryLineReader::operator++(void)
{
    const char* p;
    for (p = m_Pos;  p < m_End  &&  *p != '\r'  && *p != '\n';  ++p)
        ;
    m_Line = CTempString(m_Pos, p - m_Pos);
    // skip over delimiters
    if (p + 1 < m_End  &&  *p == '\r'  &&  p[1] == '\n') {
        m_Pos = p + 2;
    } else if (p < m_End) {
        m_Pos = p + 1;
    } else { // no final line break
        m_Pos = p;
    }
    return *this;
}

CTempString CMemoryLineReader::operator*(void) const
{
    return m_Line;
}

CT_POS_TYPE CMemoryLineReader::GetPosition(void) const
{
    return NcbiInt8ToStreampos(m_Pos - m_Start);
}


END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2006/11/17 19:16:29  ucko
* CMemoryLineReader::operator++: make sure to advance m_Pos (to m_End)
* in the absence of a final delimiter.
*
* Revision 1.1  2006/04/13 14:42:47  ucko
* Add a lightweight interface for getting lines of data with minimal
* memory copying, along with two implementations -- one for input
* streams and one for memory regions (strings or mapped files).
*
*
* ===========================================================================
*/
