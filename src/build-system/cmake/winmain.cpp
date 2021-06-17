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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *    WinMain() - necessary to launch a Win32 app in as a non-console app
 */

//
// WinMain provided here for Windows compatibility
// This allows us to run our application without a console
//

#include <ncbi_pch.hpp>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>

#ifdef _DEBUG
#  include <crtdbg.h>
#endif

USING_SCOPE(ncbi);

extern int NcbiSys_main(int argc, const ncbi::TXChar* const argv[]);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
#ifdef _UNICODE
    TXChar** argv = __wargv;
#else
    TXChar** argv = __argv;
#endif

#ifdef _DEBUG

    ///
    /// comment this section out if you want verbose leak tracking
    ///

    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags &= ~_CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(flags);

    ///
    ///
    ///

    // standard memory leak reporting infrastructure
    _CrtMemState start_state;
    _CrtMemState stop_state;
    _CrtMemCheckpoint(&start_state);

#ifndef _DEBUG_NO_CONSOLE
    //
    // In debug mode, a console will be created to display stdout/stderr
    //
    AllocConsole();
    freopen("conin$",  "r", stdin);
    freopen("conout$", "w", stdout);
    freopen("conout$", "w", stderr);

    // LOG_EER can be called from static initializers
    // this puts stream in error state
    // after that no stream output can be performed
    // so after console initialization we clear stream state
    cerr.clear();
    cout.clear();

    // call the app-supplied main()
    int ret_val = NcbiSys_main(__argc, argv);

    // close our console
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

#else

    // call the app-supplied main()
    int ret_val = NcbiSys_main(__argc, argv);

#endif

    // finalize our leak tracking
    _CrtMemCheckpoint(&stop_state);
    _CrtMemState diff_state;
    if (_CrtMemDifference(&diff_state, &start_state, &stop_state)) {
        _CrtMemDumpStatistics(&diff_state);
        //_CrtMemDumpAllObjectsSince(&diff_state);
    }

    return ret_val;

#else

    return NcbiSys_main(__argc, argv);

#endif
}
