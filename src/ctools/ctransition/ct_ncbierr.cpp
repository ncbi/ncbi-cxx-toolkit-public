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
 * Authors:  Vladimir Ivanov
 *
 * File Description:
 *   Wrapper for the C Toolkit ErrPostEx() macro.
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <ctools/ctransition/ncbierr.hpp>
#include <stdio.h>
#include <stdarg.h>

USING_NCBI_SCOPE;   
BEGIN_CTRANSITION_SCOPE


Severity ctransition_ErrSeverity(ErrSev c_severity)
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
    default:
        cxx_severity = eDiag_Fatal;
        break;
    }
    return cxx_severity;
}


string ctransition_ErrMessage(const char* format, ...)
{
    const int kMax = 8*1024;
    char buffer[kMax];
    va_list args;
    va_start(args, format);
    int n = vsprintf(buffer, format, args);
    if (n > kMax) {
        _TROUBLE;
    }
    va_end(args);
    return string(buffer);
}


END_CTRANSITION_SCOPE
