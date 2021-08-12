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
#include <corelib/ncbiapp.hpp>
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
class CCgiRequestProcessor;

/////////////////////////////////////////////////////////////////////////////
//  CCgiApplication::
//

class NCBI_XCGI_EXPORT CCgiApplication : public CNcbiApplication
{
    friend class CCgiStatistics;
    friend class CCgiRequestProcessor;
    friend void s_ScheduleFastCGIExit(void);

public:
    typedef CNcbiApplication CParent;

    CCgiApplication(const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT());
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
    unsigned int GetFCgiIteration(void) const { return (unsigned int)(m_Iteration.Get()); }

    /// Return TRUE if it is running as a "fast" CGI
    virtual bool IsFastCGI(void) const;

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

    /// Validate synchronization token (cross-site request forgery prevention).
    /// CSRF prevention is controlled by CGI_VALIDATE_CSRF_TOKEN variable (bool).
    /// The default implementation assumes the token is passed in Ncbi-CSRF-Token
    /// HTTP header and must be equal to the current session id.
    /// @return
    ///   - true if the token passes validation or if CSRF prevention is disabled.
    ///   - false if the token does not pass validation; the CGI will return
    ///     status "403 Forbidden".
    /// @sa CCgiSession
    virtual bool ValidateSynchronizationToken(void);

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
    virtual void FASTCGI_ScheduleExit(void) { m_ShouldExit = true; }

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

    /// Default implementation of CreateContextWithFlags.
    CCgiContext* CreateContextWithFlags_Default(CCgiRequestProcessor& processor,
        CNcbiArguments* args, CNcbiEnvironment* env, CNcbiIstream* inp, CNcbiOstream* out,
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

    /// Set HTTP status code in the current request context and in the
    /// current CHttpResponse if one exists.
    /// @sa CHttpResponse::SetStatus()
    void SetHTTPStatus(unsigned int status, const string& reason = kEmptyStr);

    /// Process help request: set content type, print usage informations etc.
    /// For automatic handling of help request all of the following conditions
    /// must be met:
    /// - CGI_ENABLE_HELP_REQUEST=t must be set in the environment or
    ///   EnableHelpRequest=t in [CGI] section of the INI file.
    /// - REQUEST_METHOD must be GET
    /// - query string must include ncbi_help[=<format>] argument (all other
    ///   arguments are ignored).
    /// The default implementation looks for <appname>.help.<format> files in
    /// the app directory, then for help.<format>. The formats are checked in
    /// the following order:
    /// 1. If format argument is present, try to open <appname>.help.<format>,
    ///    then help.<format>.
    /// 2. Check 'Accept:' http header; for each 'type/subtype' entry try to open
    ///    <appname>.help.<subtype> and help.<subtype>.
    /// 3. Check availability of help files for html, xml, and json formats.
    /// 4. Use CArgDescriptions to print help in XML format.
    /// If a help file starts with 'Content-type: ...' followed by double-
    /// newline, the specified content type is sent in the response regardless
    /// of the actual format selected.
    virtual void ProcessHelpRequest(const string& format);

    enum EVersionType {
        eVersion_Short,
        eVersion_Full
    };

    /// Process version request: set content type, print version informations etc.
    /// For automatic handling of version request all of the following conditions
    /// must be met:
    /// - CGI_ENABLE_VERSION_REQUEST=t must be set in the environment or
    ///   EnableVersionRequest=t in [CGI] section of the INI file.
    /// - REQUEST_METHOD must be GET
    /// - query string must include ncbi_version=[short|full] argument (all other
    ///   arguments are ignored).
    /// The default implementation prints GetVersion/GetFullVersion as plain text
    /// (default), XML or JSON depending on the 'Accept:' HTTP header, if any.
    virtual void ProcessVersionRequest(EVersionType ver_type);

    /// Admin commands passed through ncbi_admin_cmd argument.
    enum EAdminCommand {
        eAdmin_Health,       ///< Report health for this CGI only
        eAdmin_HealthDeep,   ///< Report health for this CGI and any services used by it.
        eAdmin_Unknown       ///< Unrecognized command. Overriden ProcessAdminRequest()
                             ///< can use GetEntry() to fetch command name if necessary.
    };

    /// Process admin command passed through ncbi_admin_cmd argument.
    /// Return true on success, false if the command was not processed
    /// (in this case the default processing will be used).
    virtual bool ProcessAdminRequest(EAdminCommand cmd);

    /// "Accept:" header entry.
    struct SAcceptEntry {
        SAcceptEntry(void) : m_Quality(1) {}

        typedef map<string, string> TParams;

        string m_Type;
        string m_Subtype;
        float m_Quality; ///< Quality factor or "1" if not set (or not numeric).
        string m_MediaRangeParams; ///< Media range parameters
        TParams m_AcceptParams; ///< Accept parameters

        bool operator<(const SAcceptEntry& entry) const;
    };

    typedef list<SAcceptEntry> TAcceptEntries;

    /// Parse "Accept:" header, put entries to the list, more specific first.
    void ParseAcceptHeader(TAcceptEntries& entries) const;

    /// Create request processor to process the request. If the method returns null,
    /// the application's ProcessRequest() method is used for the request. Otherwise
    /// request is passed to the processor.
    /// @sa CCgiRequestProcessor
    virtual CCgiRequestProcessor* CreateRequestProcessor(void);

protected:
    /// Set CONN_HTTP_REFERER, print self-URL and referer to log.
    void ProcessHttpReferer(void);

    /// @deprecated Use LogRequest(const CCgiContext&) instead.
    NCBI_DEPRECATED void LogRequest(void) const;
    /// Write the required values to log (user-agent, self-url, referer etc.)
    void LogRequest(const CCgiContext& ctx) const;

    /// Bit flags for CCgiRequest
    int m_RequestFlags;

    // If FastCGI-capable, and run as a Fast-CGI, then iterate through
    // the FastCGI loop (doing initialization and running ProcessRequest()
    // for each HTTP request);  then return TRUE.
    // Return FALSE overwise.
    // In the "result", return # of requests whose processing has failed
    // (exception was thrown or ProcessRequest() returned non-zero value)
    virtual bool x_RunFastCGI(int* result, unsigned int def_iter = 10);

    string GetFastCGIStandaloneServer(void) const;
    bool GetFastCGIStatLog(void) const;
    unsigned int GetFastCGIIterations(unsigned int def_iter) const;
    bool GetFastCGIComplete_Request_On_Sigterm(void) const;
    CCgiWatchFile* CreateFastCGIWatchFile(void) const;
    unsigned int GetFastCGIWatchFileTimeout(bool have_watcher) const;
    int GetFastCGIWatchFileRestartDelay(void) const;
    bool GetFastCGIChannelErrors(void) const;
    bool GetFastCGIHonorExitRequest(void) const;
    bool GetFastCGIDebug(void) const;
    bool GetFastCGIStopIfFailed(void) const;
    static CTime GetFileModificationTime(const string& filename);
    // Return true if current memory usage is above the limit.
    bool CheckMemoryLimit(void);
    void InitArgs(CArgs& args, CCgiContext& context) const;
    void AddLBCookie(CCgiCookies& cookies);
    virtual ICache* GetCacheStorage(void) const;
    virtual bool IsCachingNeeded(const CCgiRequest& request) const;
    bool GetResultFromCache(const CCgiRequest& request, CNcbiOstream& os, ICache& cache);
    void SaveResultToCache(const CCgiRequest& request, CNcbiIstream& is, ICache& cache);
    void SaveRequest(const string& rid, const CCgiRequest& request, ICache& cache);
    CCgiRequest* GetSavedRequest(const string& rid, ICache& cache);
    bool x_ProcessHelpRequest(CCgiRequestProcessor& processor);
    bool x_ProcessVersionRequest(CCgiRequestProcessor& processor);
    bool x_ProcessAdminRequest(CCgiRequestProcessor& processor);

    enum ERestartReason {
        eSR_None       = 0,
        eSR_Executable = 111,
        eSR_WatchFile  = 112
    };
    // Decide if this FastCGI process should be finished prematurely, right now
    // (the criterion being whether the executable or a special watched file
    // has changed since the last iteration)
    // NOTE: The method is not MT-safe.
    static ERestartReason ShouldRestart(CTime& mtime, CCgiWatchFile* watcher, int delay);

    // Write message to the application log, call OnEvent()
    void x_OnEvent(CCgiRequestProcessor* pprocessor, EEvent event, int status);
    // Backward compatibility
    void x_OnEvent(EEvent event, int status) { x_OnEvent(x_GetProcessorOrNull(), event, status); }

    // Create processor and store it in TLS.
    CCgiRequestProcessor& x_CreateProcessor(void);

    bool m_CaughtSigterm;
    CRef<CTls<CCgiRequestProcessor>> m_Processor;
    CAtomicCounter            m_Iteration;   // (always 0 for plain CGI)

private:

    // Add cookie with load balancer information
    void x_AddLBCookie();

    CCgiContext&   x_GetContext (void) const;
    CNcbiResource& x_GetResource(void) const;    
    bool x_IsSetProcessor(void) const;
    CCgiRequestProcessor& x_GetProcessor(void) const;
    CCgiRequestProcessor* x_GetProcessorOrNull(void) const;

    // Check if HEAD request has been served.
    bool x_DoneHeadRequest(CCgiContext& context) const;

    unique_ptr<CNcbiResource>        m_Resource;

    typedef map<string, CDiagFactory*> TDiagFactoryMap;
    TDiagFactoryMap           m_DiagFactories;

    unique_ptr<CCookieAffinity> m_Caf;         // Cookie affinity service pointer
    char*                     m_HostIP;      // Cookie affinity host IP buffer

    // Environment var. value to put to the diag.prefix;  [CGI].DiagPrefixEnv
    string                    m_DiagPrefixEnv;

    /// @sa FASTCGI_ScheduleExit()
    bool m_ShouldExit;

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
    friend class CFastCgiApplicationMT;
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
//  CCgiWatchFile::
//
//    Aux. class for noticing changes to a file
//

class CCgiWatchFile
{
public:
    // ignores changes after the first LIMIT bytes
    CCgiWatchFile(const string& filename, int limit = 1024);
    bool HasChanged(void);

private:
    typedef AutoPtr<char, ArrayDeleter<char> > TBuf;

    string m_Filename;
    int    m_Limit;
    int    m_Count;
    TBuf   m_Buf;

    // returns count of bytes read (up to m_Limit), or -1 if opening failed.
    int x_Read(char* buf);
};


/////////////////////////////////////////////////////////////////////////////
//  CCgiStreamWrapper::
//
//    CGI stream with special processing.
//

class CCgiStreamWrapperWriter;

class CCgiStreamWrapper : public CWStream
{
public:
    enum EStreamMode {
        eNormal,         // normal output - write all data as-is
        eBlockWrites,    // block all writes after HEAD request
        eChunkedWrites,  // chunked output mode
    };

    CCgiStreamWrapper(CNcbiOstream& out);

    typedef map<string, string, PNocase> TTrailer;

    // Access to writer's methods.
    EStreamMode GetWriterMode(void);
    void SetWriterMode(EStreamMode mode);
    void SetCacheStream(CNcbiOstream& stream);
    void FinishChunkedTransfer(const TTrailer* trailer);
    void AbortChunkedTransfer(void);

private:
    CCgiStreamWrapperWriter* m_Writer;
};


/// Base class for request processors.
/// @sa CCgiApplication::CreateRequestProcessor()
class NCBI_XCGI_EXPORT CCgiRequestProcessor
{
    friend class CCgiApplication;

public:
    CCgiRequestProcessor(CCgiApplication& app);
    virtual ~CCgiRequestProcessor(void);

    /// Process request provided by the context. By default calls application's ProcessRequest.
    virtual int ProcessRequest(CCgiContext& context);

    const CNcbiResource& GetResource(void) const { return m_App.GetResource(); }
    CNcbiResource&       GetResource(void)       { return m_App.GetResource(); }

    virtual bool ValidateSynchronizationToken(void);

    /// Get self-URL to be used as referer.
    string GetSelfReferer(void) const;

protected:
    virtual void ProcessHelpRequest(const string& format);
    virtual void ProcessVersionRequest(CCgiApplication::EVersionType ver_type);
    virtual bool ProcessAdminRequest(CCgiApplication::EAdminCommand cmd);
    virtual int OnException(std::exception& e, CNcbiOstream& os);
    virtual void OnEvent(CCgiApplication::EEvent event, int status);

    void SetHTTPStatus(unsigned int status, const string& reason = kEmptyStr);
    void SetRequestId(const string& rid, bool is_done);

    typedef CCgiApplication::TAcceptEntries TAcceptEntries;
    typedef CCgiApplication::SAcceptEntry TAcceptEntry;
    void ParseAcceptHeader(TAcceptEntries& entries) const;

private:
    void x_InitArgs(void) const;
    // If ProcessAdminRequest is overridden, the base method may still be called.
    bool ProcessAdminRequest_Base(CCgiApplication::EAdminCommand cmd);

    CCgiApplication&                m_App;
    shared_ptr<CCgiContext>         m_Context;
    // Parsed cmd.-line args (cmdline + CGI).
    mutable unique_ptr<CArgs>       m_CgiArgs;
    // Wrappers for cin and cout
    unique_ptr<CNcbiIstream>        m_InputStream;
    unique_ptr<CNcbiOstream>        m_OutputStream;
    bool                            m_OutputBroken = false;
    // Remember if request-start was printed, don't print request-stop
    // without request-start.
    bool                            m_RequestStartPrinted = false;
    bool                            m_ErrorStatus = false; // True if HTTP status was set to a value >=400
    string                          m_RID;
    bool                            m_IsResultReady = true;

public:
    CCgiApplication& GetApp(void) { return m_App; }
    const CCgiApplication& GetApp(void) const { return m_App; }

    CCgiContext& GetContext(void) { return *m_Context; }
    const CCgiContext& GetContext(void) const { return *m_Context; }
    void SetContext(shared_ptr<CCgiContext> context) { m_Context = context; }
    bool IsSetContext(void) const { return bool(m_Context); }

    CArgs& GetArgs(void) { if (!m_CgiArgs) x_InitArgs(); return *m_CgiArgs; }
    const CArgs& GetArgs(void) const { if (!m_CgiArgs) x_InitArgs(); return *m_CgiArgs; }
    bool IsSetArgs(void) const { return bool(m_CgiArgs); }

    CNcbiIstream& GetInputStream(void) { return *m_InputStream; }
    const CNcbiIstream& GetInputStream(void) const { return *m_InputStream; }
    void SetInputStream(CNcbiIstream* in) { m_InputStream.reset(in); }
    bool IsSetInputStream(void) const { return bool(m_InputStream); }

    CNcbiOstream& GetOutputStream(void) { return *m_OutputStream; }
    const CNcbiOstream& GetOutputStream(void) const { return *m_OutputStream; }
    void SetOutputStream(CNcbiOstream* out) { m_OutputStream.reset(out); }
    bool IsSetOutputStream(void) const { return bool(m_OutputStream); }

    bool GetOutputBroken(void) const { return m_OutputBroken; }
    void SetOutputBroken(bool val) { m_OutputBroken = val; }

    bool GetRequestStartPrinted(void) const { return m_RequestStartPrinted; }
    void SetRequestStartPrinted(bool val) { m_RequestStartPrinted = val; }

    bool GetErrorStatus(void) const { return m_ErrorStatus; }
    void SetErrorStatus(bool val) { m_ErrorStatus = val; }

    string GetRID(void) const { return m_RID; }
    void SetRID(const string& val) { m_RID = val; }

    bool GetResultReady(void) const { return m_IsResultReady; }
    void SetResultReady(bool val) { m_IsResultReady = val; }
};


// Aux. class to provide timely reset of "m_Processor" in RunFastCGI()
class CCgiProcessorGuard
{
public:
    CCgiProcessorGuard(CTls<CCgiRequestProcessor>& proc) : m_Proc(&proc) {}
    ~CCgiProcessorGuard(void) { if (m_Proc) m_Proc->Reset(); }
private:
    CTls<CCgiRequestProcessor>* m_Proc;
};


/////////////////////////////////////////////////////////////////////////////
//  Tracking Environment

NCBI_PARAM_DECL_EXPORT(NCBI_XCGI_EXPORT, bool, CGI, DisableTrackingCookie);
typedef NCBI_PARAM_TYPE(CGI, DisableTrackingCookie) TCGI_DisableTrackingCookie;
NCBI_PARAM_DECL_EXPORT(NCBI_XCGI_EXPORT, string, CGI, TrackingCookieName);
typedef NCBI_PARAM_TYPE(CGI, TrackingCookieName) TCGI_TrackingCookieName;
NCBI_PARAM_DECL_EXPORT(NCBI_XCGI_EXPORT, string, CGI, TrackingTagName);
typedef NCBI_PARAM_TYPE(CGI, TrackingTagName) TCGI_TrackingTagName;
NCBI_PARAM_DECL_EXPORT(NCBI_XCGI_EXPORT, string, CGI, TrackingCookieDomain);
typedef NCBI_PARAM_TYPE(CGI, TrackingCookieDomain) TCGI_TrackingCookieDomain;
NCBI_PARAM_DECL_EXPORT(NCBI_XCGI_EXPORT, string, CGI, TrackingCookiePath);
typedef NCBI_PARAM_TYPE(CGI, TrackingCookiePath) TCGI_TrackingCookiePath;
NCBI_PARAM_DECL_EXPORT(NCBI_XCGI_EXPORT, bool, CGI,
    Client_Connection_Interruption_Okay);
typedef NCBI_PARAM_TYPE(CGI, Client_Connection_Interruption_Okay)
    TClientConnIntOk;
NCBI_PARAM_ENUM_DECL_EXPORT(NCBI_XCGI_EXPORT, EDiagSev, CGI,
    Client_Connection_Interruption_Severity);
typedef NCBI_PARAM_TYPE(CGI, Client_Connection_Interruption_Severity)
    TClientConnIntSeverity;

END_NCBI_SCOPE


/* @} */


#endif // CGI___CGIAPP__HPP
