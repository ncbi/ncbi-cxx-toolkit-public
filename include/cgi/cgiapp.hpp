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

#include <corelib/ncbireg.hpp>
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
class ICgiSessionStorage;
class CCgiSessionParameters;
class ICache;


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

    /// Get instance of CGI session storage interface. 
    /// If the CGI application needs to use CGI session it should overwrite 
    /// this metod and return an instance of an implementation of 
    /// ICgiSessionStorage interface. 
    /// @param params
    ///  Optional parameters
    virtual ICgiSessionStorage* GetSessionStorage(CCgiSessionParameters& params) const;



private:
    virtual ICache* GetCacheStorage() const;
    virtual bool IsCachingNeeded(const CCgiRequest& request) const;
    bool GetResultFromCache(const CCgiRequest& request, CNcbiOstream& os);
    void SaveResultToCache(const CCgiRequest& request, CNcbiIstream& is);
    void SaveRequest(const string& rid, const CCgiRequest& request);
    CCgiRequest* GetSavedRequest(const string& rid);

protected:
    /// Check the command line arguments before parsing them.
    /// If '-version' or '-version-full' is the only argument,
    /// print the version and exit (return ePreparse_Exit). Otherwise
    /// return ePreparse_Continue for normal execution.
    virtual EPreparseArgs PreparseArgs(int                argc,
                                       const char* const* argv);

    void SetRequestId(const string& rid, bool is_done);

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
        eStartRequest,
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

    /// Schedule Fast-CGI loop to end as soon as possible, after
    /// safely finishing the currently processed request, if any.
    /// @note
    ///  Calling it from inside OnEvent(eWaiting) will end the Fast-CGI
    ///  loop immediately.
    /// @note
    ///  It is a no-op for the regular CGI.
    void FASTCGI_ScheduleExit(void) { m_ShouldExit = true; }


    /// Factory method for the Context object construction
    virtual CCgiContext*   CreateContext(CNcbiArguments*   args = 0,
                                         CNcbiEnvironment* env  = 0,
                                         CNcbiIstream*     inp  = 0,
                                         CNcbiOstream*     out  = 0,
                                         int               ifd  = -1,
                                         int               ofd  = -1);

    /// The same as CreateContext(), but allows for a custom set of flags
    /// to be specified in the CCgiRequest constructor.
    virtual CCgiContext* CreateContextWithFlags(CNcbiArguments* args,
        CNcbiEnvironment* env, CNcbiIstream* inp, CNcbiOstream* out,
            int ifd, int ofd, int flags);

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

    /// Check CGI context for possible problems, throw exception with
    /// HTTP status set if something is wrong.
    void VerifyCgiContext(CCgiContext& context);

    /// Get default path for the log files.
    virtual string GetDefaultLogPath(void) const;

    /// Prepare properties and print the application start message
    virtual void AppStart(void);
    /// Prepare properties for application stop message
    virtual void AppStop(int exit_code);

    /// Set HTTP status code in the current request context
    void SetHTTPStatus(int status);

protected:
    /// Set CONN_HTTP_REFERER, print self-URL and referer to log.
    void ProcessHttpReferer(void);

    /// Write the required values to log (user-agent, self-url, referer etc.)
    void LogRequest(void) const;

    /// Bit flags for CCgiRequest
    int m_RequestFlags;

private:

    // If FastCGI-capable, and run as a Fast-CGI, then iterate through
    // the FastCGI loop (doing initialization and running ProcessRequest()
    // for each HTTP request);  then return TRUE.
    // Return FALSE overwise.
    // In the "result", return # of requests whose processing has failed
    // (exception was thrown or ProcessRequest() returned non-zero value)
    bool x_RunFastCGI(int* result, unsigned int def_iter = 10);

    // Write message to the application log, call OnEvent()
    void x_OnEvent(EEvent event, int status);

    // Add cookie with load balancer information
    void x_AddLBCookie();

    CCgiContext&   x_GetContext (void) const;
    CNcbiResource& x_GetResource(void) const;    

    auto_ptr<CNcbiResource>   m_Resource;
    auto_ptr<CCgiContext>     m_Context;
    auto_ptr<ICache>          m_Cache;

    typedef map<string, CDiagFactory*> TDiagFactoryMap;
    TDiagFactoryMap           m_DiagFactories;

    auto_ptr<CCookieAffinity> m_Caf;         // Cookie affinity service pointer
    char*                     m_HostIP;      // Cookie affinity host IP buffer

    unsigned int              m_Iteration;   // (always 0 for plain CGI)

    // Environment var. value to put to the diag.prefix;  [CGI].DiagPrefixEnv
    string                    m_DiagPrefixEnv;

    /// Flag, indicates arguments are in sync with CGI context
    /// (becomes TRUE on first call of GetArgs())
    mutable bool              m_ArgContextSync;

    /// Parsed cmd.-line args (cmdline + CGI)
    mutable auto_ptr<CArgs>   m_CgiArgs;

    /// Wrappers for cin and cout
    auto_ptr<CNcbiIstream>    m_InputStream;
    auto_ptr<CNcbiOstream>    m_OutputStream;

    string m_RID;
    bool m_IsResultReady;

    /// @sa FASTCGI_ScheduleExit()
    bool m_ShouldExit;

    /// Remember if request-start was printed, don't print request-stop
    /// without request-start.
    bool m_RequestStartPrinted;

    bool m_ErrorStatus; // True if HTTP status was set to a value >=400

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



/////////////////////////////////////////////////////////////////////////////
//  Tracking Environment

NCBI_PARAM_DECL(bool, CGI, DisableTrackingCookie); 
typedef NCBI_PARAM_TYPE(CGI, DisableTrackingCookie) TCGI_DisableTrackingCookie;
NCBI_PARAM_DECL(string, CGI, TrackingCookieName); 
typedef NCBI_PARAM_TYPE(CGI, TrackingCookieName) TCGI_TrackingCookieName;
NCBI_PARAM_DECL(string, CGI, TrackingTagName);
typedef NCBI_PARAM_TYPE(CGI, TrackingTagName) TCGI_TrackingTagName;
NCBI_PARAM_DECL(string, CGI, TrackingCookieDomain); 
typedef NCBI_PARAM_TYPE(CGI, TrackingCookieDomain) TCGI_TrackingCookieDomain;
NCBI_PARAM_DECL(string, CGI, TrackingCookiePath); 
typedef NCBI_PARAM_TYPE(CGI, TrackingCookiePath) TCGI_TrackingCookiePath;
NCBI_PARAM_DECL(bool, CGI, Client_Connection_Interruption_Okay);
typedef NCBI_PARAM_TYPE(CGI, Client_Connection_Interruption_Okay)
    TClientConnIntOk;
NCBI_PARAM_ENUM_DECL(EDiagSev, CGI, Client_Connection_Interruption_Severity);
typedef NCBI_PARAM_TYPE(CGI, Client_Connection_Interruption_Severity)
    TClientConnIntSeverity;

END_NCBI_SCOPE


/* @} */


#endif // CGI___CGIAPP__HPP
