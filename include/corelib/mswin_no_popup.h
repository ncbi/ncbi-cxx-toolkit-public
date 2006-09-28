#ifndef CORELIB___MSWIN_NO_POPUP__H
#define CORELIB___MSWIN_NO_POPUP__H

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
 * Author:  Denis Vakatov
 *
 * File Description:   Suppress popup messages on execution errors.
 *                     MS Windows specific.
 *
 * Include this header only to applications, not libraries.
 * 
 */

#include <ncbiconf.h>

#if defined(NCBI_OS_MSWIN)

#  include <crtdbg.h>
#  include <windows.h>

/* Suppress popup messages on execution errors.
 * NOTE: Windows-specific, suppresses all error message boxes in both runtime
 * and in debug libraries, as well as all General Protection Fault messages.
 * Environment variable DIAG_SILENT_ABORT must be set to "Y" or "y".
 */

/* Handler for "Unhandled" exceptions */
static LONG CALLBACK _SEH_Handler(EXCEPTION_POINTERS* ep)
{
    /* Always terminate a program */
    return EXCEPTION_EXECUTE_HANDLER;
}

static int _SuppressDiagPopupMessages(void)
{
    /* Windows GPF errors */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);

    /* Runtime library */
    _set_error_mode(_OUT_TO_STDERR);

    /* Debug library */
    _CrtSetReportFile(_CRT_WARN,   _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);

    /* Exceptions */
    SetUnhandledExceptionFilter(_SEH_Handler);

    return 0;
}

/* Put this function at startup init level 'V', far enough not to mess up with
 * base RTL init, which happens at preceding levels in alphabetical order.
 */
#  if _MSC_VER >= 1400
#    pragma section( ".CRT$XIV", read)
#  endif
#  pragma data_seg(".CRT$XIV")
static int (*_SDPM)(void) = _SuppressDiagPopupMessages;
#  pragma data_seg()

#endif /*defined(NCBI_OS_...)*/


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/09/28 15:51:50  ivanov
 * Initial revision. Copied from test/test_assert.h.
 *
 * ===========================================================================
 */

#endif  /* CORELIB___MSWIN_NO_POPUP__H */
