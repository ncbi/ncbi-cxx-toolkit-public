#ifndef CORELIB__NCBIEXEC__HPP
#define CORELIB__NCBIEXEC__HPP

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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Portable exec class
 *
 */

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// Portable exec class
//

class CExec
{
public:
    // The exception specific for this class:
    class CException : public runtime_error {
    public:
        CException(const string& message) : runtime_error(message) {}
    };

    // Exec mode
    enum EMode {
        eOverlay = 0, // Overlays calling process with new process, destroying
                      // calling process 
        eWait    = 1, // Suspends calling thread until execution of new 
                      // process is complete (synchronous)
        eNoWait  = 2, // Continues to execute calling process concurrently 
                      // with new process (asynchronous)
        eDetach  = 3  // Continues to execute calling process; new process is 
                      // run in background with no access to console or 
                      // keyboard. Calls to Wait() against new process will
                      // fail. This is an asynchronous spawn.
    };

    // Execute a command. Return command's exit code.
    // Throw an exception if failed to execute.
    // If cmdline is a null pointer, System() checks if the shell (command 
    // interpreter) exists and is executable. If the shell is available, 
    // System() returns a non-zero value; otherwise, it returns 0.

    static int System(const char* cmdline);

    // Spawn new process. All functions return -1 on error
    // (and then "errno" will contain the error code). On success, they return:
    //   exit code   - in eWait mode.
    //   process pid - in eNoWait and eDetach modes.
    //   nothing     - in eOverlay mode.   

    // The letter(s) at the end of the function name determine the variation.
    //
    //   E - envp, array of pointers to environment settings, is passed to 
    //       new process. The NULL environment pointer indicates that the new 
    //       process will inherit the parents process's environment.
    //   L - command-line arguments are passed individually to function. 
    //       This suffix is typically used when number of parameters to new 
    //       process is known in advance.
    //   P - PATH environment variable is used to find file to execute 
    //       NOTE: On Unix platform this feature work also and in functions 
    //         without P letter in function name. 
    //   V - argv, array of pointers to command-line arguments, is passed to 
    //       function. This suffix is typically used when number of parameters
    //       to new process is variable.

    // NOTE: At least one argument must be present. This argument is always, 
    //       by convention, the name of the file being spawned (argument with 
    //       number 0). You can do not set this parameter, SpawnV* functions 
    //       will doing this self. But array of pointers must have length 1 or
    //       more. And You must assign parameters for new process beginning 
    //       from 1.

    static int SpawnL  (const EMode mode, const char *cmdname, 
                        const char *argv, ... /*, NULL */);
    static int SpawnLE (const EMode mode, const char *cmdname, 
                        const char *argv, ... /*, NULL, const char *envp[] */);
    static int SpawnLP (const EMode mode, const char *cmdname,
                        const char *argv, ... /*, NULL */);
    static int SpawnLPE(const EMode mode, const char *cmdname,
                        const char *argv, ... /*, NULL, const char *envp[] */);
    static int SpawnV  (const EMode mode, const char *cmdname,
                        const char *const *argv);
    static int SpawnVE (const EMode mode, const char *cmdname,
                        const char *const *argv, const char *const *envp);
    static int SpawnVP (const EMode mode, const char *cmdname,
                        const char *const *argv);
    static int SpawnVPE(const EMode mode, const char *cmdname,
                        const char *const *argv, const char *const *envp);

    // Waits until the child process with "pid" terminates (return right away
    // if the specifed child process has already terminated).
    // Return exit code of child process; return -1 if error has occured.
    static int Wait(const int pid);
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/05/31 20:48:39  ivanov
 * Clean up code
 *
 * Revision 1.1  2002/05/30 16:30:45  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CORELIB__NCBIEXEC__HPP */
