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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _DEBUG
#  include <crtdbg.h>
#endif

extern int main(int, char**);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG

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

    // call the app-supplied main()
    int ret_val = main(__argc, __argv);

    // close our console
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

#else

    // call the app-supplied main()
    int ret_val = main(__argc, __argv);

#endif

    // finalize our leak tracking
    _CrtMemCheckpoint(&stop_state);
    _CrtMemState diff_state;
    if (_CrtMemDifference(&diff_state, &start_state, &stop_state)) {
        _CrtMemDumpStatistics(&diff_state);
        _CrtMemDumpAllObjectsSince(&diff_state);
    }

    return ret_val;

#else

    return main(__argc, __argv);

#endif
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/05/07 15:46:23  dicuccio
 * Copied over from compilers/msvc_prj
 *
 * Revision 1.2  2003/08/25 14:18:01  dicuccio
 * Added nested #ifdef to remove the default console in _DEBUG mode if
 * _DEBUG_NO_CONSOLE is #defined
 *
 * Revision 1.1  2003/08/21 12:34:06  dicuccio
 * Added additional gui projects
 *
 * Revision 1.2  2003/04/24 13:39:53  dicuccio
 * COnditionally include <crtdbg.h> if _DEBUG is defined
 *
 * Revision 1.1  2003/01/09 21:01:30  dicuccio
 * Moved WinMain into its own .cpp file.  Added winmain.cpp to build
 *
 * ===========================================================================
 */
