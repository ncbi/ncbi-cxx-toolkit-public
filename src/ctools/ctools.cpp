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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   A bridge between C and C++ Toolkits
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>  // C++ Toolkit stuff, must go first!
#include <ctools/ctools.hpp>
#include <ncbimsg.h>            // C   Toolkit error and message posting


BEGIN_NCBI_SCOPE


EDiagSev CTOOLS_CToCxxSeverity(int c_severity)
{
    EDiagSev cxx_severity;
    switch (c_severity) {
    case SEV_NONE:
        cxx_severity = eDiag_Trace;
        break;
    case SEV_INFO:
        cxx_severity = eDiag_Info;
        break;
    case SEV_WARNING:
        cxx_severity = eDiag_Warning;
        break;
    case SEV_ERROR:
        cxx_severity = eDiag_Error;
        break;
    case SEV_REJECT:
        cxx_severity = eDiag_Critical;
        break;
    case SEV_FATAL:
        /*fallthru*/
    default:
        cxx_severity = eDiag_Fatal;
        break;
    }
    return cxx_severity;
}


int CTOOLS_CxxToCSeverity(EDiagSev cxx_severity)
{
    int c_severity;
    switch (cxx_severity) {
    case eDiag_Trace:
        c_severity = SEV_NONE;
        break;
    case eDiag_Info:
        c_severity = SEV_INFO;
        break;
    case eDiag_Warning:
        c_severity = SEV_WARNING;
        break;
    case eDiag_Error:
        c_severity = SEV_ERROR;
        break;
    case eDiag_Critical:
        c_severity = SEV_REJECT;
        break;
    case eDiag_Fatal:
        /*fallthru*/
    default:
        c_severity = SEV_FATAL;
        break;
    }
    return c_severity;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;


static int LIBCALLBACK s_c2cxxErrorHandler(const ErrDesc* err)
{
    try {
        CNcbiDiag diag(CTOOLS_CToCxxSeverity(err->severity), eDPF_Default);
        if (*err->srcfile)
            diag.SetFile(err->srcfile);
        if (err->srcline)
            diag.SetLine(err->srcline);
        diag.SetErrorCode(err->errcode, err->subcode);
        if (*err->module)
            diag << err->module << ' ';
        if (*err->codestr)
            diag << err->codestr << ' ';
        for (const ValNode* node = err->userstr; node; node = node->next) {
            if (node->data.ptrvalue)
                diag << (char*) node->data.ptrvalue << ' ';
        }
        if (*err->errtext)
            diag << err->errtext;
    } catch (...) {
        _ASSERT(0);
        return ANS_NONE;
    }

    return ANS_OK;
}


void SetupCToolkitErrPost(void)
{
    Nlm_CallErrHandlerOnly(TRUE);
    Nlm_ErrSetHandler(reinterpret_cast<ErrHookProc>(s_c2cxxErrorHandler));
}
