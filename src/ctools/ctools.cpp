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
#include <ctools/ctools.h>
#include <ncbimsg.h>            // C   Toolkit error and message posting


USING_NCBI_SCOPE;


static int LIBCALLBACK s_c2cxxErrorHandler(const ErrDesc* err)
{
    EDiagSev level;
    switch (err->severity) {
    case SEV_NONE:
        level = eDiag_Trace;
        break;
    case SEV_INFO:
        level = eDiag_Info;
        break;
    case SEV_WARNING:
        level = eDiag_Warning;
        break;
    case SEV_ERROR:
        level = eDiag_Error;
        break;
    case SEV_REJECT:
        level = eDiag_Critical;
        break;
    case SEV_FATAL:
        /*fallthru*/
    default:
        level = eDiag_Fatal;
        break;
    }

    try {
        CNcbiDiag diag(level, eDPF_Default);
        if (*err->srcfile)
            diag.SetFile(err->srcfile);
        if (err->srcline)
            diag.SetLine(err->srcline);
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


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.7  2004/05/17 20:59:00  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.6  2003/11/13 16:01:48  lavr
 * Log moved to end
 *
 * Revision 1.5  2001/02/12 19:27:42  lavr
 * Now unnecessary intermediate function call eliminated
 *
 * Revision 1.4  2001/02/11 22:45:04  thiessen
 * put outside NCBI namespace, since it requires C linkage
 *
 * Revision 1.3  2001/02/11 02:15:39  lavr
 * extern "C" replaced with reinterpret_cast to accepted by MSVC
 *
 * Revision 1.2  2001/02/10 04:13:35  lavr
 * Extra semicolon removed
 *
 * Revision 1.1  2001/02/09 17:39:12  lavr
 * Initial revision
 *
 * ==========================================================================
 */
