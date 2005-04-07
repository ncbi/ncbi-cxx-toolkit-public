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


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/04/07 18:25:01  shomrat
* Use \n instead of endl
*
* Revision 1.4  2005/03/31 21:15:38  vasilche
* Do not flush after every line.
*
* Revision 1.3  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2004/04/22 15:59:35  shomrat
* Removed unused code
*
* Revision 1.1  2003/12/17 20:23:45  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
