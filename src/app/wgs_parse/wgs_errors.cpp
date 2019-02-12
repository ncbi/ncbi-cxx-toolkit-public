/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>

#include "wgs_errors.hpp"
#include "wgs_utils.hpp"

USING_NCBI_SCOPE;

namespace wgsparse
{

static CWgsParseDiagHandler s_diag_handler;

void CWgsParseDiagHandler::Post(const SDiagMessage& mess)
{
    string sev = CNcbiDiag::SeverityName(mess.m_Severity);
    NStr::ToUpper(sev);

    CNcbiOstrstream str_os;
    str_os << "[wgsparse] " << sev << ": wgsparse [" << ToStringLeadZeroes(mess.m_ErrCode, 3) << '.' << ToStringLeadZeroes(mess.m_ErrSubCode, 3) << "] " << mess.m_Buffer << '\n';

    string str = CNcbiOstrstreamToString(str_os);
    cerr.write(str.data(), str.size());
    cerr << NcbiFlush;
}

CWgsParseDiagHandler& CWgsParseDiagHandler::GetWgsParseDiagHandler()
{
    return s_diag_handler;
}

}
