#ifndef NCBIAPP__HPP
#define NCBIAPP__HPP

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
* Authors:  Vsevolod Sandomirskiy, Denis Vakatov
*
* File Description:
*   CNcbiApplication -- a generic NCBI application class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2001/05/17 14:50:13  lavr
* Typos corrected
*
* Revision 1.17  2001/04/13 02:58:43  vakatov
* Do not apply R1.16 for non-UNIX platforms where we cannot configure
* HAVE_NCBI_C yet
*
* Revision 1.16  2001/04/12 22:55:09  vakatov
* [HAVE_NCBI_C]  Handle #GetArgs to avoid name clash with the NCBI C Toolkit
*
* Revision 1.15  2000/11/24 23:33:10  vakatov
* CNcbiApplication::  added SetupArgDescriptions() and GetArgs() to
* setup cmd.-line argument description, and then to retrieve their
* values, respectively. Also implements internal error handling and
* printout of USAGE for the described arguments.
*
* Revision 1.14  2000/01/20 17:51:16  vakatov
* Major redesign and expansion of the "CNcbiApplication" class to
*  - embed application arguments   "CNcbiArguments"
*  - embed application environment "CNcbiEnvironment"
*  - allow auto-setup or "by choice" (see enum EAppDiagStream) of diagnostics
*  - allow memory-resided "per application" temp. diagnostic buffer
*  - allow one to specify exact name of the config.-file to load, or to
*    ignore the config.file (via constructor's "conf" arg)
*  - added detailed comments
*
* Revision 1.13  1999/12/29 21:20:16  vakatov
* More intelligent lookup for the default config.file. -- try to strip off
* file extensions if cannot find an exact match;  more comments and tracing
*
* Revision 1.12  1999/11/15 18:57:01  vakatov
* Added <memory> (for "auto_ptr<>" template)
*
* Revision 1.11  1999/11/15 15:53:27  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.10  1999/04/27 14:49:50  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.9  1998/12/28 17:56:25  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.8  1998/12/09 17:30:12  sandomir
* ncbicgi.hpp deleted from ncbiapp.hpp
*
* Revision 1.7  1998/12/09 16:49:56  sandomir
* CCgiApplication added
*
* Revision 1.6  1998/12/07 23:46:52  vakatov
* Merged with "cgiapp.hpp";  minor fixes
*
* Revision 1.4  1998/12/03 21:24:21  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.3  1998/12/01 19:12:36  lewisg
* added CCgiApplication
*
* Revision 1.2  1998/11/05 21:45:13  sandomir
* std:: deleted
*
* Revision 1.1  1998/11/02 22:10:12  sandomir
* CNcbiApplication added; netest sample updated
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <memory>

// Avoid name clash with the NCBI C Toolkit
#if !defined(NCBI_OS_UNIX)  ||  defined(HAVE_NCBI_C)
#  if defined(GetArgs)
#    undef GetArgs
#  endif
#  define GetArgs GetArgs
#endif


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
// CNcbiApplication
//


// Some forward declarations
class CNcbiEnvironment;
class CNcbiRegistry;
class CNcbiArguments;
class CArgDescriptions;
class CArgs;


// Where to write the application's diagnostics to
enum EAppDiagStream {
    eDS_ToStdout,    // to standard output stream
    eDS_ToStderr,    // to standard error  stream
    eDS_ToStdlog,    // add to standard log file (app.name + ".log")
    eDS_ToMemory,    // keep in a temp.memory buffer (see "FlushDiag()")
    eDS_Disable,     // dont write it anywhere
    eDS_User,        // leave as was previously set (or not set) by user
    eDS_AppSpecific, // depends on the application type
    eDS_Default      // "eDS_User" if set, else "eDS_AppSpecific"
};


// Basic (abstract) NCBI application class
class CNcbiApplication {
public:
    // Singleton method
    static CNcbiApplication* Instance(void);

    // 'ctors
    CNcbiApplication(void);
    virtual ~CNcbiApplication(void);

    // AppMain() -- call SetupDiag(), LoadConfig(), Init(), Run(), Exit(), etc.
    // You can specify where to write the diagnostics to (see EAppDiagStream),
    // and where to get the configuration file (see LoadConfig()) to load
    // to the application registry (accessible via GetConfig()).
    // Throw exception if:  (a) not-only instance;  (b) cannot load
    // explicitly specified config.file;  (c) SetupDiag throws an exception. 
    int AppMain
    (int                argc,  // as in a regular "main(argc, argv, envp)"
     const char* const* argv,
     const char* const* envp = 0,
     EAppDiagStream     diag = eDS_Default,     /* eDS_ToStderr            */
     const char*        conf = NcbiEmptyCStr,   /* see LoadConfig()        */
     const string&      name = NcbiEmptyString  /* app.name (dflt: "ncbi") */
     );

    virtual void Init(void);     // initialization
    virtual int  Run(void) = 0;  // main loop
    virtual void Exit(void);     // cleanup (on the application exit)

    // Get the application's cached command-line arguments
    const CNcbiArguments& GetArguments(void) const;

    // Get cmd.-line arguments parsed according to the arg descriptions set by
    // SetArgDescriptions(). Throw exception if no descriptions have been set.
    const CArgs& GetArgs(void) const;

    // Get the application's cached environment
    const CNcbiEnvironment& GetEnvironment(void) const;

    // Get the application's cached configuration parameters
    const CNcbiRegistry& GetConfig(void) const;
    CNcbiRegistry& GetConfig(void);

    // "eDS_ToMemory" case only:  diagnostics will be stored in the internal
    // application buffer (see "m_DiagStream") -- at least until it's
    // redirected using SetDiagHandler/SetDiagStream.
    // Then, this function to:
    //  - dump all the stored diagnostics to stream "os";
    //  - if "os" is NULL then just purge out the stored diagnostics;
    //  - if "close_diag" is TRUE, then destroy "m_DiagStream", and if global
    //    diag.stream was attached to "m_DiagStream" then close it.
    // Return total # of bytes written to "os".
    SIZE_TYPE FlushDiag(CNcbiOstream* os, bool close_diag = false);


protected:
    // Setup cmd.-line argument descriptions. Call it from Init(). The passed
    // "arg_desc" will be owned by this class, and it'll be deleted
    // by ~CNcbiApplication(), or if SetupArgDescriptions() is called again.
    void SetupArgDescriptions(CArgDescriptions* arg_desc);

    // Setup app. diagnostic stream
    bool SetupDiag(EAppDiagStream diag);

    // Special, adjustable:  to call if SetupDiag(eDS_AppSpecific)
    // Default:  eDS_ToStderr.
    virtual bool SetupDiag_AppSpecific(void);

    // Default method to try load (add to "reg") from the config.file.
    // If "conf" arg (as passed to the class constructor)
    //   NULL      -- dont even try to load registry from any file at all;
    //   non-empty -- try to load from the conf.file of name "conf" only(!)
    //                TIP:  if the path is not full-qualified then:
    //                     if it starts from "../" or "./" -- look in the
    //                     working dir, else look in the program dir.
    //   empty     -- compose the conf.file name from the application name
    //                plus ".ini". If it does not match an existing
    //                file then try to strip file extensions, e.g., for
    //                "my_app.cgi.exe" -- try subsequently:
    //                    "my_app.cgi.exe.ini", "my_app.cgi.ini", "my_app.ini".
    // Throw an exception if "conf" is non-empty, and cannot open file. 
    // Throw an exception if file exists, but contains some invalid entries.
    // Return TRUE only if the file was non-NULL, found and successfully read.
    virtual bool LoadConfig(CNcbiRegistry& reg, const string* conf);


private:
    static CNcbiApplication*   m_Instance;   // current app.instance
    auto_ptr<CNcbiEnvironment> m_Environ;    // cached application environment
    auto_ptr<CNcbiRegistry>    m_Config;     // (guaranteed to be non-NULL)
    auto_ptr<CNcbiOstream>     m_DiagStream; // opt., aux., see "eDS_ToMemory"
    auto_ptr<CNcbiArguments>   m_Arguments;  // command-line arguments
    auto_ptr<CArgDescriptions> m_ArgDesc;    // cmd.-line arg descriptions
    auto_ptr<CArgs>            m_Args;       // parsed cmd.-line args
};



// Inline (getters)

inline const CNcbiArguments& CNcbiApplication::GetArguments(void) const {
    return *m_Arguments;
}

inline const CArgs& CNcbiApplication::GetArgs(void) const {
    if ( !m_Args.get() ) {
        throw runtime_error("CNcbiApplication::GetArgs() -- unset args");
    }
    return *m_Args;
}

inline const CNcbiEnvironment& CNcbiApplication::GetEnvironment(void) const {
    return *m_Environ;
}

inline const CNcbiRegistry& CNcbiApplication::GetConfig(void) const {
    return *m_Config;
}

inline CNcbiRegistry& CNcbiApplication::GetConfig(void) {
    return *m_Config;
}


END_NCBI_SCOPE

#endif // NCBIAPP__HPP


