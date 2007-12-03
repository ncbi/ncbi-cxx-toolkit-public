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
*   ESummary request
*
*/

#include <ncbi_pch.hpp>
#include <objtools/eutils/api/esummary.hpp>


BEGIN_NCBI_SCOPE


CESummary_Request::CESummary_Request(const string& db,
                                     CRef<CEUtils_ConnContext>& ctx)
    : CEUtils_Request(ctx),
      m_RetStart(0),
      m_RetMax(0)
{
    SetDatabase(db);
}


CESummary_Request::~CESummary_Request(void)
{
}


string CESummary_Request::GetScriptName(void) const
{
    return "esummary.fcgi";
}


string CESummary_Request::GetQueryString(void) const
{
    string args = TParent::GetQueryString();
    string ids = m_Id.AsQueryString();
    if ( !ids.empty() ) {
        args += "&" + ids;
    }
    if (m_RetStart > 0) {
        args += "&retstart=" + NStr::IntToString(m_RetStart);
    }
    if (m_RetMax > 0) {
        args += "&retmax=" + NStr::IntToString(m_RetMax);
    }
    return args;
}


ESerialDataFormat CESummary_Request::GetSerialDataFormat(void) const
{
    return eSerial_Xml;
}


CRef<esummary::CESummaryResult> CESummary_Request::GetESummaryResult(void)
{
    CObjectIStream* is = GetObjectIStream();
    _ASSERT(is);
    CRef<esummary::CESummaryResult> res(new esummary::CESummaryResult);
    *is >> *res;
    Disconnect();
    return res;
}


END_NCBI_SCOPE
