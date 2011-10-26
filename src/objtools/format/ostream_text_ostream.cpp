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
* Author:  Mati Shomrat
*          
*
* File Description:
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/format/ostream_text_ostream.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
// TextOStream

COStreamTextOStream::COStreamTextOStream(void) :
    m_Ostream(cout)
{
}


COStreamTextOStream::COStreamTextOStream(CNcbiOstream &os) :
    m_Ostream(os)
{
}


void COStreamTextOStream::AddParagraph
(const list<string>& text,
 const CSerialObject* obj)
{
    ITERATE(list<string>, line, text) {
        m_Ostream << *line << '\n';
    }

    // we don't care about the object
}

void COStreamTextOStream::AddLine(
    const CTempString& line,
    const CSerialObject* obj,
    EAddNewline add_newline )
{
    m_Ostream << line;
    if( add_newline == eAddNewline_Yes ) {
        m_Ostream << '\n';
    }
}

void COStreamTextOStream::AddCLine(
    const char *line,
    const CSerialObject* obj )
{
    m_Ostream << line << '\n';
}

void COStreamTextOStream::AddRawText(
    const char *line,
    const CSerialObject* obj )
{
    m_Ostream << line;
}

END_SCOPE(objects)
END_NCBI_SCOPE
