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
 *
 */

/// @file ncbiexec.hpp 
/// Defines a portable execute class.


#include <corelib/ncbi_process.hpp>


/** @addtogroup Exec
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Exit code type
typedef int TExitCode;


/////////////////////////////////////////////////////////////////////////////
///
/// CExec --
///
/// Define portable exec class.
///
/// Defines the different ways a process can be spawned.

class NCBI_XNCBI_EXPORT CExec
{
public:
    /// Modification flags for EMode.
    ///
    /// @note
    /// These flags works on UNIX only, and will be ignored on MS Windows.
    /// Also, they don't work for eOverlay/eDetach modes.
    enum EModeFlags {
        /// After fork() move a process to new group (assign new PGID).
        /// This can be useful if new created process also spawn child
        /// processes and you wish to control it using signals, or,
        /// for example, terminate the whole process group at once.
        fNewGroup  = (1 << 8),
        /// Mask for all master modes, all EModeFlags must be above it.
        fModeMask  = 0x0F  // eOverlay | eWait | eNoWait | eDetach
    };

    /// Which exec mode the spawned process is called with.
    enum EMode {
        /// Overlays calling process with new process, destroying calling
        /// process.
        eOverlay     = 0, 
        /// Suspends calling thread until execution of new process
        /// is complete (synchronous operation).
        eWait        = 1,
        /// The same as eWait, but on UNIX platforms new process group
        /// will be created and calling process become the leader of the new
        /// process group.
        eWaitGroup = eWait | fNewGroup,
        /// Continues to execute calling process concurrently with new
        /// process (asynchronous process).                      
        eNoWait      = 2, 
        /// The same as eNoWait, but on UNIX platforms new process group
        /// will be created and calling process become the leader of the new
        /// process group.
        eNoWaitGroup = eNoWait | fNewGroup, 
        /// Continues to execute calling process; new process is run in
        /// background with no access to console or keyboard.
        /// On UNIX new created process become the leader of the new session,
        /// the process group leader of the new process group.
        /// Calls to Wait() against new process will fail on MS Windows,
        /// but work on UNIX platforms. This is an asynchronous spawn.                      
        eDetach      = 3
    };
   
    /// The result type for Spawn methods.
    /// 
    /// In the eNoWait and eDetach modes for Spawn functions to return process
    /// handles.  On MS Windows it is a real process handle of type HANDLE.
    /// On UNIX it is a process identifier (pid).
    /// In the eWait mode, the spawn functions return exit code of a process.
    /// Throws an exception if you try to get exit code instead of 
    /// stored process handle, and otherwise.
    /// In some cases can store both - an exit code and a handle (see Wait()).
    class NCBI_XNCBI_EXPORT CResult
    {
    public:
        /// Default ctor -- zero everything
        CResult() : m_Flags(0) { memset(&m_Result, 0, sizeof(m_Result)); }
        /// Get exit code
        TExitCode      GetExitCode     (void);
        /// Get process handle/pid
        TProcessHandle GetProcessHandle(void); 
        // Deprecated operator for compatibility with previous
        // versions of Spawn methods which returns integer value.
        NCBI_DEPRECATED operator intptr_t(void) const;

    private:
        /// Flags defines what this class store
        enum EFlags {
            fExitCode  = (1<<1),
            fHandle    = (1<<2),
            fBoth      = fExitCode | fHandle
        };
        typedef int TFlags;  ///< Binary OR of "EFlags"
        struct {
            TExitCode      exitcode;
            TProcessHandle handle;
        } m_Result;          ///< Result of Spawn*() methods
        TFlags m_Flags;      ///< What m_Result stores

        friend class CExec;
    };

    /// Execute the specified command.
    ///
    /// Execute the command and return the executed command's exit code.
    /// Throw an exception if command failed to execute. If cmdline is a null
    /// pointer, System() checks if the shell (command interpreter) exists and
    /// is executable. If the shell is available, System() returns a non-zero
    /// value; otherwise, it returns 0.
    static TExitCode System(const char* cmdline);

    /// Spawn a new process with specified command-line arguments.
    ///
    /// In the SpawnL() version, the command-line arguments are passed
    /// individually. SpawnL() is typically used when number of parameters to
    /// the new process is known in advance.
    ///
    /// Meaning of the suffix "L" in method name:
    /// - The letter "L" as suffix refers to the fact that command-line
    ///   arguments are passed separately as arguments.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path to the process to spawn.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnLE(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnL(EMode mode, const char *cmdname, const char *argv, .../*, NULL */);

    /// Spawn a new process with specified command-line arguments and
    /// environment settings.
    ///
    /// In the SpawnLE() version, the command-line arguments and environment
    /// pointer are passed individually. SpawnLE() is typically used when
    /// number of parameters to the new process and individual environment 
    /// parameter settings are known in advance.
    ///
    /// Meaning of the suffix "LE" in method name:
    /// - The letter "L" as suffix refers to the fact that command-line
    ///   arguments are passed separately as arguments.
    /// - The letter "E" as suffix refers to the fact that environment pointer,
    ///   envp, is passed as an array of pointers to environment settings to 
    ///   the new process. The NULL environment pointer indicates that the new 
    ///   process will inherit the parents process's environment.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @param envp
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in vector must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnLE (EMode mode, const char *cmdname, 
             const char *argv, ... /*, NULL, const char *envp[] */);

    /// Spawn a new process with variable number of command-line arguments and
    /// find file to execute from the PATH environment variable.
    ///
    /// In the SpawnLP() version, the command-line arguments are passed
    /// individually and the PATH environment variable is used to find the
    /// file to execute. SpawnLP() is typically used when number
    /// of parameters to the new process is known in advance but the exact
    /// path to the executable is not known.
    ///
    /// Meaning of the suffix "LP" in method name:
    /// - The letter "L" as suffix refers to the fact that command-line
    ///   arguments are passed separately as arguments.
    /// - The letter "P" as suffix refers to the fact that the PATH
    ///   environment variable is used to find file to execute - on a Unix
    ///   platform this feature works in functions without letter "P" in
    ///   function name. 
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLPE(), SpawnV(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult 
    SpawnLP(EMode mode, const char *cmdname, const char *argv, .../*, NULL*/);

    /// Spawn a new process with specified command-line arguments, 
    /// environment settings and find file to execute from the PATH
    /// environment variable.
    ///
    /// In the SpawnLPE() version, the command-line arguments and environment
    /// pointer are passed individually, and the PATH environment variable
    /// is used to find the file to execute. SpawnLPE() is typically used when
    /// number of parameters to the new process and individual environment
    /// parameter settings are known in advance, but the exact path to the
    /// executable is not known.
    ///
    /// Meaning of the suffix "LPE" in method name:
    /// - The letter "L" as suffix refers to the fact that command-line
    ///   arguments are passed separately as arguments.
    /// - The letter "P" as suffix refers to the fact that the PATH
    ///   environment variable is used to find file to execute - on a Unix
    ///   platform this feature works in functions without letter "P" in
    ///   function name. 
    /// - The letter "E" as suffix refers to the fact that environment pointer,
    ///   envp, is passed as an array of pointers to environment settings to 
    ///   the new process. The NULL environment pointer indicates that the new 
    ///   process will inherit the parents process's environment.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @param envp
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in an array must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///    Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnV(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnLPE(EMode mode, const char *cmdname,
             const char *argv, ... /*, NULL, const char *envp[] */);

    /// Spawn a new process with variable number of command-line arguments. 
    ///
    /// In the SpawnV() version, the command-line arguments are a variable
    /// number. The array of pointers to arguments must have a length of 1 or
    /// more and you must assign parameters for the new process beginning
    /// from 1.
    ///
    /// Meaning of the suffix "V" in method name:
    /// - The letter "V" as suffix refers to the fact that the number of
    /// command-line arguments are variable.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdline
    ///   Path of file to be executed.
    /// @param argv
    ///   Pointer to argument vector. Last value in vector must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnLPE(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnV(EMode mode, const char *cmdname, const char *const *argv);

    /// Spawn a new process with variable number of command-line arguments
    /// and specified environment settings.
    ///
    /// In the SpawnVE() version, the command-line arguments are a variable
    /// number. The array of pointers to arguments must have a length of 1 or
    /// more and you must assign parameters for the new process beginning from
    /// 1.  The individual environment parameter settings are known in advance
    /// and passed explicitly.
    ///
    /// Meaning of the suffix "VE" in method name:
    /// - The letter "V" as suffix refers to the fact that the number of
    ///   command-line arguments are variable.
    /// - The letter "E" as suffix refers to the fact that environment pointer,
    ///   envp, is passed as an array of pointers to environment settings to 
    ///   the new process. The NULL environment pointer indicates that the new 
    ///   process will inherit the parents process's environment.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   Argument vector. Last value must be NULL.
    /// @param envp
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in an array must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnVE(EMode mode, const char *cmdname,
            const char *const *argv, const char *const *envp);

    /// Spawn a new process with variable number of command-line arguments and
    /// find file to execute from the PATH environment variable.
    ///
    /// In the SpawnVP() version, the command-line arguments are a variable
    /// number. The array of pointers to arguments must have a length of 1 or
    /// more and you must assign parameters for the new process beginning from
    /// 1. The PATH environment variable is used to find the file to execute.
    ///
    /// Meaning of the suffix "VP" in method name:
    /// - The letter "V" as suffix refers to the fact that the number of
    ///   command-line arguments are variable.
    /// - The letter "P" as suffix refers to the fact that the PATH
    ///   environment variable is used to find file to execute - on a Unix
    ///   platform this feature works in functions without letter "P" in
    ///   function name. 
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   Pointer to argument vector. Last value in vector must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVE(), 
    ///   SpawnVPE().
    static CResult
    SpawnVP(EMode mode, const char *cmdname, const char *const *argv);

    /// Spawn a new process with variable number of command-line arguments
    /// and specified environment settings, and find the file to execute
    /// from the PATH environment variable.
    ///
    /// In the SpawnVPE() version, the command-line arguments are a variable
    /// number. The array of pointers to arguments must have a length of 1 or
    /// more and you must assign parameters for the new process beginning from
    /// 1. The PATH environment variable is used to find the file to execute,
    /// and the environment is passed via an environment vector pointer.
    ///
    /// Meaning of the suffix "VPE" in method name:
    /// - The letter "V" as suffix refers to the fact that the number of
    ///   command-line arguments are variable.
    /// - The letter "P" as suffix refers to the fact that the PATH
    ///   environment variable is used to find file to execute - on a Unix
    ///   platform this feature works in functions without letter "P" in
    ///   function name. 
    /// - The letter "E" as suffix refers to the fact that environment pointer,
    ///   envp, is passed as an array of pointers to environment settings to 
    ///   the new process. The NULL environment pointer indicates that the new 
    ///   process will inherit the parents process's environment.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   Argument vector. Last value must be NULL.
    /// @param envp
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in an array must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVE(),
    ///   SpawnVP(), 
    static CResult
    SpawnVPE(EMode mode, const char *cmdname,
             const char *const *argv, const char *const *envp);

    /// Wait until specified process terminates.
    ///
    /// Wait until the process with "handle" terminates, and return
    /// immeditately if the specifed process has already terminated.
    /// @param handle
    ///   Wait on process with identifier "handle", returned by one 
    ///   of the Spawn* function in eNoWait and eDetach modes.
    /// @param timeout
    ///   Time-out interval. By default it is infinite.
    /// @return
    ///   - Exit code of the process, if no errors.
    ///   - (-1), if error has occurred.
    static TExitCode Wait(TProcessHandle handle,
                          unsigned long  timeout = kInfiniteTimeoutMs);

    /// Mode used to wait processes termination.
    enum EWaitMode {
        eWaitAny,     ///< Wait any process to terminate
        eWaitAll      ///< Wait all processes to terminate
    };

    /// Wait until any/all processes terminates.
    ///
    /// Wait until any/all processes from specified list terminates.
    /// Return immeditately if the specifed processes has already terminated.
    /// @param handles
    ///   List of process identifiers. Each identifier is a value returned
    ///   by one of the Spawn* function in eNoWait and eDetach modes.
    ///   Handles for terminated processes going to "result" list, and
    ///   has been removed from this one.
    /// @param mode
    ///   Wait termination for any or all possible processes within
    ///   specified timeout. 
    ///    eWaitAny - wait until at least one process terminates.
    ///    eWaitAll - wait until all processes terminates or timeout expires.
    /// @param result
    ///   List of process handles/exitcodes of terminated processes from
    ///   the list "handles". If this list have elements, that they will
    ///   be removed.
    /// @param timeout
    ///   Time-out interval. By default it is infinite.
    /// @return
    ///   - Number of terminated processes (size of the "result" list),
    ///     if no errors. Regardless of timeout status.
    ///   - (-1), if error has occurred.
    static int Wait(list<TProcessHandle>& handles, 
                    EWaitMode             mode,
                    list<CResult>&        result,
                    unsigned long         timeout = kInfiniteTimeoutMs);

    /// Run console application in invisible mode.
    ///
    /// MS Windows:
    ///    This function try to run a program in invisible mode, without
    ///    visible window. This can be used to run console programm from 
    ///    non-console application. If it runs from console application,
    ///    the parent's console window can be used by child process.
    ///    Executing non-console program can show theirs windows or not,
    ///    this depends. In eDetach mode the main window/console of
    ///    the running program can be visible, use eNoWait instead.
    /// @note
    ///    If the running program cannot self-terminate, that
    ///    it can be never terminated.
    /// Unix:
    ///    In current implementation equal to SpawnL().
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path to the process to spawn.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL()
    static CResult
    RunSilent(EMode mode, const char *cmdname,
              const char *argv, ... /*, NULL */);

    /// Quote argument.
    ///
    /// Enclose argument in quotes if necessary.
    /// Used for concatenation arguments into command line.
    static string QuoteArg(const string& arg);


    /// Check executable permissions for specified file.
    ///
    /// @note
    ///   This is no guarantee that the file is executable even if
    ///   the function returns TRUE. It try to get effective user
    ///   permissions for spefified file, but sometimes this
    ///   is not possible.
    /// @param path
    ///   Path to the file to check.
    /// @return 
    ///   TRUE if file is executable, FALSE otherwise.
    /// @sa
    ///   CFile::CheckAccess
    static bool IsExecutable(const string& path);

    /// Find executable file.
    ///
    /// If necessary, the PATH environment variable is used
    /// to find the file to execute
    /// @param filename
    ///   Name of the file to search.
    /// @return 
    ///   Path to the executable file. kEmptyStr if not found,
    ///   or the file do not have executable permissions.
    /// @sa
    ///   IsExecutable
    static string ResolvePath(const string& filename);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CExecException --
///
/// Define exceptions generated by CExec.
///
/// CExecException inherits its basic functionality from
/// CErrnoTemplException<CCoreException> and defines additional error codes
/// for errors generated by CExec.

class NCBI_XNCBI_EXPORT CExecException : public CErrnoTemplException<CCoreException>
{
public:
    /// Error types that CExec can generate.
    enum EErrCode {
        eSystem,      ///< System error
        eSpawn,       ///< Spawn error
        eResult       ///< Result interpretation error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CExecException,
                           CErrnoTemplException<CCoreException>);
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB__NCBIEXEC__HPP */
