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
*   Global query request
*
*/

#include <ncbi_pch.hpp>
#include <objtools/eutils/api/egquery.hpp>
#include <cgi/cgi_util.hpp>


BEGIN_NCBI_SCOPE


CEGQuery_Request::CEGQuery_Request(CRef<CEUtils_ConnContext>& ctx)
    : CEUtils_Request(ctx)
{
}


CEGQuery_Request::~CEGQuery_Request(void)
{
}


string CEGQuery_Request::GetScriptName(void) const
{
    return "egquery.fcgi";
}


string CEGQuery_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    if ( !m_Term.empty() ) {
        args += "&term=" +
            URL_EncodeString(m_Term, eUrlEncode_ProcessMarkChars);
    }
    return args;
}


ESerialDataFormat CEGQuery_Request::GetSerialDataFormat(void) const
{
    return eSerial_Xml;
}


CRef<egquery::CResult> CEGQuery_Request::GetResult(void)
{
    CObjectIStream* is = GetObjectIStream();
    _ASSERT(is);
    CRef<egquery::CResult> res(new egquery::CResult);
    *is >> *res;
    Disconnect();
    return res;
}


END_NCBI_SCOPE
