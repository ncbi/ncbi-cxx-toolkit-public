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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*           
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/format_item_ostream.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatTextOStream;


CFormatItemOStream::CFormatItemOStream
(IFlatTextOStream* text_os,
 IFormatter* formatter)
    : CFlatItemOStream(formatter), m_TextOS(text_os)
{
}


void CFormatItemOStream::AddItem(CConstRef<IFlatItem> i)
{
    m_Formatter->Format(*i, *m_TextOS);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2004/02/12 20:26:24  shomrat
* forward delaration instead of include
*
* Revision 1.1  2003/12/17 20:21:37  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
