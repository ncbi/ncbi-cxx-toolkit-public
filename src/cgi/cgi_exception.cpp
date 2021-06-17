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
 * Author:  Alexey Grichenko
 *
 * File Description:   CGI exception implementation
 *
 */

#include <ncbi_pch.hpp>
#include <cgi/cgi_exception.hpp>

BEGIN_NCBI_SCOPE


void CCgiException::x_Init(const CDiagCompileInfo& info,
                           const string& message,
                           const CException* prev_exception,
                           EDiagSev severity)
{
    CException::x_Init(info, message, prev_exception, severity);
    m_StatusCode = eStatusNotSet;
}


void CCgiException::x_Assign(const CException& src)
{
    CException::x_Assign(src);
    const CCgiException& cgi_src = dynamic_cast<const CCgiException&>(src);
    m_StatusCode = cgi_src.m_StatusCode;
    m_StatusMessage = cgi_src.m_StatusMessage;
}


string CCgiException::GetStdStatusMessage(EStatusCode code)
{
    return code == eStatusNotSet ? "Status not set" :
        CRequestStatus::GetStdStatusMessage(CRequestStatus::ECode(code));
}


END_NCBI_SCOPE
