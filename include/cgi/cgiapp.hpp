#ifndef CGI___CGIAPP__HPP
#define CGI___CGIAPP__HPP

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
* Authors:
*	Vsevolod Sandomirskiy, Aaron Ucko, Denis Vakatov, Anatoliy Kuznetsov
*
* File Description:
*   Base class for (Fast-)CGI applications
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbitime.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/ncbicgir.hpp>
#include <cgi/ncbires.hpp>
#include <cgi/caf.hpp>


/** @addtogroup CGIBase
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CCgiServerContext;
class CCgiStatistics;
class CCgiWatchFile;


/////////////////////////////////////////////////////////////////////////////
//  CCgiApplication::
//

class NCBI_XCGI_EXPORT CCgiApplication : public CNcbiApplication
{
    friend class CCgiStatistics;
    typedef CNcbiApplication CParent;

public:
    CCgiApplication(void);
    ~CCgiApplication(void);

    /// Singleton
    static CCgiApplication* Instance(void);

    /// Get current server context. Throw exception if the context is not set.
    const CCgiContext& GetContext(void) const  { return x_GetContext(); }
    /// Get current server context. Throw exception if the context is not set.
    CCgiContext&       GetContext(void)        { return x_GetContext(); }

    /// Get server 'resource'. Throw exception if the resource is not set.
    const CNcbiResource& GetResource(void) const { return x_GetResource(); }
    /// Get server 'resource'. Throw exception if the resource is not set.
    CNcbiResource&       GetResource(void)       { return x_GetResource(); }

    /// Get the # of currently processed HTTP request.
    ///
    /// 1-based for FastCGI (but 0 before the first iteration starts);
    /// always 0 for regular (i.e. not "fast") CGIs.
    unsigned int GetFCgiIteration(void) const { return m_Iteration; }

    /// Return TRUE if it is running as a "fast" CGI
    bool IsFastCGI(void) const;

    /// This method is called on the CGI application initialization -- before
    /// starting to process a HTTP request or even receiving one.
    ///
    /// No HTTP request (or context) is available at the time of call.
    ///
    /// If you decide to override it, remember to call CCgiApplication::Init().
    virtual void Init(void);

    /// This method is called on the CGI application exit.
    ///
    /// No HTTP request (or context) is available at the time of call.
    ///
    /// If you decide to override it, remember to call CCgiApplication::Exit().
    virtual void Exit(void);

    /// Do not override this method yourself! -- it includes all the CGI
    /// specific machinery. If you override it, do call CCgiApplication::Run()
    /// from inside your overriding method.
    /// @sa ProcessRequest
    virtual int Run(void);

    /// This is the method you should override. It is called whenever the CGI
    /// application gets a syntaxically valid HTTP request.
    /// @param context
    ///  Contains the parameters of the HTTP request
    /// @return
    ///  Exit code;  it must be zero on success
    virtual int ProcessRequest(CCgiContext& context) = 0;

    virtual CNcbiResource*     LoadResource(void);
    virtual CCgiServerContext* LoadServerContext(CCgiContext& context);

    /// Set cgi parsing flag
    /// @sa CCgiRequest::Flags
    void SetRequestFlags(int flags) { m_RequestFlags = flags; }

    virtual void SetupArgDescriptions(CArgDescriptions* arg_desc);

    /// Get parsed command line arguments extended with CGI parameters
    ///
    /// @return
    ///   The CArgs object containing parsed cmd.-line arguments and
    ///   CGI parameters
    ///
    virtual const CArgs& GetArgs(void) const;

protected:
    /// This method is called if an exception is thrown during the processing
    /// of HTTP request. OnEvent() will be called after this method.
    ///
    /// Context and Resource aren't valid at the time of this method call
    ///
    /// The default implementation sends out an HTTP response with "e.what()",
    /// and then returns zero if the printout has got through, -1 otherwise.
    /// @param e
    ///  The exception thrown
    /// @param os
    ///  Output stream to the client.
    /// @return
    ///  Value to use as the CGI's (or FCGI iteration's) exit code
    /// @sa OnEvent
    virtual int OnException(std::exception& e, CNcbiOstream& os);

    /// @sa OnEvent, ProcessRequest
    enum EEvent {
        eSuccess,    ///< The HTTP request was processed, with zero exit code
        eError,      ///< The HTTP request was processed, non-zero exit code
        eWaiting,    ///< Periodic awakening while waiting for the next request
        eException,  ///< An exception occured during the request processing
        eEndRequest, ///< HTTP request processed, all results sent to client
        eExit,       ///< No more iterations, exiting (called the very last)
        eExecutable, ///< FCGI forced to exit as its modif. time has changed
        eWatchFile,  ///< FCGI forced to exit as its "watch file" has changed
        eExitOnFail, ///< [FastCGI].StopIfFailed set, and the iteration failed
        eExitRequest ///< FCGI forced to exit by client's 'exitfastcgi' request
    };

    /// This method is called after each request, or when the CGI is forced to
    /// skip a request, or to finish altogether without processing a request.
    ///
    /// No HTTP request (or context) may be available at the time of call.
    ///
    /// The default implementation of this method does nothing.
    ///
    /// @param event
    ///  CGI framework event
    /// @param status
    ///  - eSuccess, eError:  the value returned by ProcessRequest()
    ///  - eException:        the value returned by OnException()
    ///  - eExit:             exit code of the CGI application
    ///  - others:            non-zero, and different for any one status
    virtual void OnEvent(EEvent event, int status);


    /// Factory method for the Context object construction
    virtual CCgiContext*   CreateContext(CNcbiArguments*   args = 0,
                                         CNcbiEnvironment* env  = 0,
                                         CNcbiIstream*     inp  = 0,
                                         CNcbiOstream*     out  = 0,
                                         int               ifd  = -1,
                                         int               ofd  = -1);

    void                   RegisterDiagFactory(const string& key,
                                               CDiagFactory* fact);
    CDiagFactory*          FindDiagFactory(const string& key);

    virtual void           ConfigureDiagnostics    (CCgiContext& context);
    virtual void           ConfigureDiagDestination(CCgiContext& context);
    virtual void           ConfigureDiagThreshold  (CCgiContext& context);
    virtual void           ConfigureDiagFormat     (CCgiContext& context);

    /// Analyze registry settings ([CGI] Log) and return current logging option
    enum ELogOpt {
        eNoLog,
        eLog,
        eLogOnError
    };
    ELogOpt GetLogOpt(void) const;

    /// Class factory for statistics class
    virtual CCgiStatistics* CreateStat();

    /// Attach cookie affinity service interface. Pointer ownership goes to
    /// the CCgiApplication.
    void SetCafService(CCookieAffinity* caf);

private:
    // If FastCGI-capable, and run as a Fast-CGI, then iterate through
    // the FastCGI loop (doing initialization and running ProcessRequest()
    // for each HTTP request);  then return TRUE.
    // Return FALSE overwise.
    // In the "result", return # of requests whose processing has failed
    // (exception was thrown or ProcessRequest() returned non-zero value)
    bool x_RunFastCGI(int* result, unsigned int def_iter = 10);

    // Logging
    enum ELogPostFlags {
        fBegin = 0x1,
        fEnd   = 0x2
    };
    typedef int TLogPostFlags;  // binary OR of "ELogPostFlags"

    void x_LogPost(const char*             msg_header,
                   unsigned int            iteration,
                   const CTime&            start_time,
                   const CNcbiEnvironment* env,
                   TLogPostFlags           flags)
        const;

    // Add cookie with load balancer information
    void x_AddLBCookie();

    CCgiContext&   x_GetContext (void) const;
    CNcbiResource& x_GetResource(void) const;

    auto_ptr<CNcbiResource>   m_Resource;
    auto_ptr<CCgiContext>     m_Context;

    typedef map<string, CDiagFactory*> TDiagFactoryMap;
    TDiagFactoryMap           m_DiagFactories;

    auto_ptr<CCookieAffinity> m_Caf;         // Cookie affinity service pointer
    char*                     m_HostIP;      // Cookie affinity host IP buffer

    unsigned int              m_Iteration;   // (always 0 for plain CGI)

    // Environment var. value to put to the diag.prefix;  [CGI].DiagPrefixEnv
    string                    m_DiagPrefixEnv;

    /// Bit flags for CCgiRequest
    int                       m_RequestFlags;
    /// Flag, indicates arguments are in sync with CGI context
    /// (becomes TRUE on first call of GetArgs())
    mutable bool              m_ArgContextSync;

    /// Parsed cmd.-line args (cmdline + CGI)
    mutable auto_ptr<CArgs>   m_CgiArgs;

    // forbidden
    CCgiApplication(const CCgiApplication&);
    CCgiApplication& operator=(const CCgiApplication&);
};



/////////////////////////////////////////////////////////////////////////////
//  CCgiStatistics::
//
//    CGI statistics information
//

class NCBI_XCGI_EXPORT CCgiStatistics
{
    friend class CCgiApplication;
public:
    virtual ~CCgiStatistics();

protected:
    CCgiStatistics(CCgiApplication& cgi_app);

    // Reset statistics class. Method called only ones for CGI
    // applications and every iteration if it is FastCGI.
    virtual void Reset(const CTime& start_time,
                       int          result,
                       const std::exception*  ex = 0);

    // Compose message for statistics logging.
    // This default implementation constructs the message from the fragments
    // composed with the help of "Compose_Xxx()" methods (see below).
    // NOTE:  It can return empty string (when time cut-off is engaged).
    virtual string Compose(void);

    // Log the message
    virtual void   Submit(const string& message);

protected:
    virtual string Compose_ProgramName (void);
    virtual string Compose_Timing      (const CTime& end_time);
    virtual string Compose_Entries     (void);
    virtual string Compose_Result      (void);
    virtual string Compose_ErrMessage  (void);

protected:
    CCgiApplication& m_CgiApp;     // Reference on the "mother app"
    string           m_LogDelim;   // Log delimiter
    CTime            m_StartTime;  // CGI start time
    int              m_Result;     // Return code
    string           m_ErrMsg;     // Error message
};


END_NCBI_SCOPE


/* @} */


/*
* ===========================================================================
*
* $Log$
* Revision 1.46  2005/05/23 15:20:47  didenko
* Undo wrong commit
*
* Revision 1.45  2005/05/23 15:03:09  didenko
* Added Serialize/Deserialize methods to CCgiRequest class
*
* Revision 1.44  2005/03/10 18:03:58  vakatov
* Made SetupArgDescriptions() and GetArgs() virtual.
* Keep and use its own CArgs instance rather than the CNcbiApplication's one
*
* Revision 1.43  2004/12/27 20:31:38  vakatov
* + CCgiApplication::OnEvent() -- to allow one catch and handle a variety of
*   states and events happening in the CGI and Fast-CGI applications
*
* Doxygen'ized and updated comments (in the header only).
*
* Revision 1.42  2004/12/01 13:49:13  kuznets
* Changes to make CGI parameters available as arguments
*
* Revision 1.41  2004/05/11 12:43:29  kuznets
* Changes to control HTTP parsing (CCgiRequest flags)
*
* Revision 1.40  2003/11/05 18:25:32  dicuccio
* Added export specifiers.  Added private, unimplemented copy ctor/assignment
* operator
*
* Revision 1.39  2003/06/24 15:02:18  ucko
* Prefix exception with std:: in OnException's declaration to avoid
* possible clashes with the struct exception in Sun's <math.h>.
*
* Revision 1.38  2003/05/21 17:38:04  vakatov
*    CCgiApplication::x_RunFastCGI():  increased the default number of
* FastCGI iterations from 3 to 10; also changed the meaning of the exit code.
*    CCgiApplication::OnException():  new virtual method to handle
* exception(s) -- allows sending the exception messages back to the HTTP
* client and setting the exit code.
*    CCgiApplication::x_FCGI_ShouldRestart():  moved to "fcgi_run.cpp".
*
* Revision 1.37  2003/04/10 19:01:40  siyan
* Added doxygen support
*
* Revision 1.36  2003/03/24 16:14:11  ucko
* New accessors GetFCgiIteration and IsFastCGI; new data member m_Iteration.
*
* Revision 1.35  2003/03/03 16:36:33  kuznets
* explicit use of std namespace when reffering exception
*
* Revision 1.34  2003/02/27 17:55:05  ucko
* +x_FCGI_ShouldRestart
*
* Revision 1.33  2003/02/26 17:34:03  kuznets
* CCgiStatistics::Reset changed to take exception as a parameter
*
* Revision 1.32  2003/02/25 14:10:56  kuznets
* Added support of CCookieAffinity service interface, host IP address, cookie encoding
*
* Revision 1.31  2003/02/19 17:51:34  kuznets
* Added generation of load balancing cookie
*
* Revision 1.30  2003/02/05 01:21:43  ucko
* "friend" -> "friend class" for GCC 3; CVS log -> end.
*
* Revision 1.29  2003/02/04 21:27:13  kuznets
* + Implementation of statistics logging
*
* Revision 1.28  2003/01/23 19:58:40  kuznets
* CGI logging improvements
*
* Revision 1.27  2001/11/19 15:20:16  ucko
* Switch CGI stuff to new diagnostics interface.
*
* Revision 1.26  2001/10/31 15:30:19  golikov
* warning removed
*
* Revision 1.25  2001/10/17 14:18:04  ucko
* Add CCgiApplication::SetCgiDiagHandler for the benefit of derived
* classes that overload ConfigureDiagDestination.
*
* Revision 1.24  2001/10/05 14:56:20  ucko
* Minor interface tweaks for CCgiStreamDiagHandler and descendants.
*
* Revision 1.23  2001/10/04 18:17:51  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* Revision 1.22  2001/06/13 21:04:35  vakatov
* Formal improvements and general beautifications of the CGI lib sources.
*
* Revision 1.21  2001/01/12 21:58:25  golikov
* cgicontext available from cgiapp
*
* Revision 1.20  2000/01/20 17:54:55  vakatov
* CCgiApplication:: constructor to get CNcbiArguments, and not raw argc/argv.
* SetupDiag_AppSpecific() to override the one from CNcbiApplication:: -- lest
* to write the diagnostics to the standard output.
*
* Revision 1.19  1999/12/17 04:06:20  vakatov
* Added CCgiApplication::RunFastCGI()
*
* Revision 1.18  1999/11/15 15:53:19  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.17  1999/05/14 19:21:48  pubmed
* myncbi - initial version; minor changes in CgiContext, history, query
*
* Revision 1.15  1999/05/06 20:32:47  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.14  1999/04/30 16:38:08  vakatov
* #include <ncbireg.hpp> to provide CNcbiRegistry class definition(see R1.13)
*
* Revision 1.13  1999/04/27 17:01:23  vakatov
* #include <ncbires.hpp> to provide CNcbiResource class definition
* for the "auto_ptr<CNcbiResource>" (required by "delete" under MSVC++)
*
* Revision 1.12  1999/04/27 14:49:46  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.11  1999/02/22 21:12:37  sandomir
* MsgRequest -> NcbiContext
*
* Revision 1.10  1998/12/28 23:28:59  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.9  1998/12/28 15:43:09  sandomir
* minor fixed in CgiApp and Resource
*
* Revision 1.8  1998/12/10 17:36:54  sandomir
* ncbires.cpp added
*
* Revision 1.7  1998/12/09 22:59:05  lewisg
* use new cgiapp class
*
* Revision 1.6  1998/12/09 17:27:44  sandomir
* tool should be changed to work with the new CCgiApplication
*
* Revision 1.5  1998/12/09 16:49:55  sandomir
* CCgiApplication added
*
* Revision 1.1  1998/12/03 21:24:21  sandomir
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
*
* ===========================================================================
*/

#endif // CGI___CGIAPP__HPP
