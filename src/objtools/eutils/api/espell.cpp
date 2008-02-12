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
* Author: Aleksey Grichenko
*
* File Description:
*   ESpell request
*
*/

#include <ncbi_pch.hpp>
#include <objtools/eutils/api/espell.hpp>
#include <cgi/cgi_util.hpp>


BEGIN_NCBI_SCOPE


CESpell_Request::CESpell_Request(const string& db,
                                 CRef<CEUtils_ConnContext>& ctx)
    : CEUtils_Request(ctx, "espell.fcgi")
{
    SetDatabase(db);
}


CESpell_Request::~CESpell_Request(void)
{
}


string CESpell_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    if ( !m_Term.empty() ) {
        args += "&term=" +
            URL_EncodeString(m_Term, eUrlEncode_ProcessMarkChars);
    }
    return args;
}


ESerialDataFormat CESpell_Request::GetSerialDataFormat(void) const
{
    return eSerial_Xml;
}


CRef<espell::CESpellResult> CESpell_Request::GetESpellResult(void)
{
    CObjectIStream* is = GetObjectIStream();
    _ASSERT(is);
    CRef<espell::CESpellResult> res(new espell::CESpellResult);
    *is >> *res;
    Disconnect();
    return res;
}


END_NCBI_SCOPE
