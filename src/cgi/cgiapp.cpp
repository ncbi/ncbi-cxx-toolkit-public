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
* Author: Eugene Vasilchenko, Denis Vakatov, Anatoliy Kuznetsov
*
* File Description:
*   Definition CGI application class and its context class.
*/

#include <ncbi_pch.hpp>
#include <cgi/cgiapp.hpp>

#include <corelib/ncbienv.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/stream_utils.hpp>
#include <corelib/ncbi_system.hpp> // for SuppressSystemMessageBox
#include <corelib/rwstream.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_strings.h>

#include <util/multi_writer.hpp>
#include <util/cache/cache_ref.hpp>

#include <cgi/cgictx.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgi_serial.hpp>
#include <cgi/error_codes.hpp>
#include <signal.h>


#ifdef NCBI_OS_UNIX
#  include <unistd.h>
#endif


#define NCBI_USE_ERRCODE_X   Cgi_Application


BEGIN_NCBI_SCOPE


NCBI_PARAM_DECL(bool, CGI, Print_Http_Referer);
NCBI_PARAM_DEF_EX(bool, CGI, Print_Http_Referer, true, eParam_NoThread,
                  CGI_PRINT_HTTP_REFERER);
typedef NCBI_PARAM_TYPE(CGI, Print_Http_Referer) TPrintRefererParam;


NCBI_PARAM_DECL(bool, CGI, Print_User_Agent);
NCBI_PARAM_DEF_EX(bool, CGI, Print_User_Agent, true, eParam_NoThread,
                  CGI_PRINT_USER_AGENT);
typedef NCBI_PARAM_TYPE(CGI, Print_User_Agent) TPrintUserAgentParam;


NCBI_PARAM_DECL(bool, CGI, Print_Self_Url);
NCBI_PARAM_DEF_EX(bool, CGI, Print_Self_Url, true, eParam_NoThread,
                  CGI_PRINT_SELF_URL);
typedef NCBI_PARAM_TYPE(CGI, Print_Self_Url) TPrintSelfUrlParam;


NCBI_PARAM_DECL(bool, CGI, Print_Request_Method);
NCBI_PARAM_DEF_EX(bool, CGI, Print_Request_Method, true, eParam_NoThread,
    CGI_PRINT_REQUEST_METHOD);
typedef NCBI_PARAM_TYPE(CGI, Print_Request_Method) TPrintRequestMethodParam;


NCBI_PARAM_DECL(bool, CGI, Allow_Sigpipe);
NCBI_PARAM_DEF_EX(bool, CGI, Allow_Sigpipe, false, eParam_NoThread,
                  CGI_ALLOW_SIGPIPE);
typedef NCBI_PARAM_TYPE(CGI, Allow_Sigpipe) TParamAllowSigpipe;

// Log any broken connection with status 299 rather than 499.
NCBI_PARAM_DEF_EX(bool, CGI, Client_Connection_Interruption_Okay, false,
                  eParam_NoThread,
                  CGI_CLIENT_CONNECTION_INTERRUPTION_OKAY);

// Severity for logging broken connection errors.
NCBI_PARAM_ENUM_ARRAY(EDiagSev, CGI, Client_Connection_Interruption_Severity)
{
    {"Info", eDiag_Info},
    {"Warning", eDiag_Warning},
    {"Error", eDiag_Error},
    {"Critical", eDiag_Critical},
    {"Fatal", eDiag_Fatal},
    {"Trace", eDiag_Trace}
};
NCBI_PARAM_ENUM_DEF_EX(EDiagSev, CGI, Client_Connection_Interruption_Severity,
                       eDiag_Error, eParam_NoThread,
                       CGI_CLIENT_CONNECTION_INTERRUPTION_SEVERITY);


///////////////////////////////////////////////////////
// IO streams with byte counting for CGI applications
//


class CCGIStreamReader : public IReader
{
public:
    CCGIStreamReader(istream& is) : m_IStr(is) { }

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* /*count*/)
    { return eRW_NotImplemented; }

protected:
    istream& m_IStr;
};


ERW_Result CCGIStreamReader::Read(void*   buf,
                                  size_t  count,
                                  size_t* bytes_read)
{
    size_t x_read = (size_t)CStreamUtils::Readsome(m_IStr, (char*)buf, count);
    ERW_Result result;
    if (x_read > 0  ||  count == 0) {
        result = eRW_Success;
    }
    else {
        result = m_IStr.eof() ? eRW_Eof : eRW_Error;
    }
    if (bytes_read) {
        *bytes_read = x_read;
    }
    return result;
}


///////////////////////////////////////////////////////
// CCgiStreamWrapper, CCgiStreamWrapperWriter
//

NCBI_PARAM_DECL(bool, CGI, Count_Transfered);
NCBI_PARAM_DEF_EX(bool, CGI, Count_Transfered, true, eParam_NoThread,
                  CGI_COUNT_TRANSFERED);
typedef NCBI_PARAM_TYPE(CGI, Count_Transfered) TCGI_Count_Transfered;


// Helper writer used to:
// - count bytes read/written to the stream;
// - disable output stream during HEAD requests after sending headers
//   so that the application can not output more data or clear 'bad' bit;
// - send chunked data to the client;
// - copy data to cache stream when necessary.
class CCgiStreamWrapperWriter : public IWriter
{
public:
    CCgiStreamWrapperWriter(CNcbiOstream& out);
    virtual ~CCgiStreamWrapperWriter(void);

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void);

    CCgiStreamWrapper::EStreamMode GetMode(void) const { return m_Mode; }

    void SetMode(CCgiStreamWrapper::EStreamMode mode);
    void SetCacheStream(CNcbiOstream& stream);

    void FinishChunkedTransfer(const CCgiStreamWrapper::TTrailer* trailer);
    void AbortChunkedTransfer(void);

private:
    void x_SetChunkSize(size_t sz);
    void x_WriteChunk(const char* buf, size_t count);

    CCgiStreamWrapper::EStreamMode m_Mode;
    CNcbiOstream* m_Out;
    // block mode
    bool m_ErrorReported;
    // chunked mode
    size_t m_ChunkSize;
    char* m_Chunk;
    size_t m_Count;
    bool m_UsedChunkedTransfer; // remember if chunked transfer was enabled.
};


CCgiStreamWrapperWriter::CCgiStreamWrapperWriter(CNcbiOstream& out)
    : m_Mode(CCgiStreamWrapper::eNormal),
      m_Out(&out),
      m_ErrorReported(false),
      m_ChunkSize(0),
      m_Chunk(0),
      m_Count(0),
      m_UsedChunkedTransfer(false)
{
}


CCgiStreamWrapperWriter::~CCgiStreamWrapperWriter(void)
{
    if (m_Mode == CCgiStreamWrapper::eChunkedWrites) {
        x_SetChunkSize(0); // cleanup
    }
}


void CCgiStreamWrapperWriter::x_WriteChunk(const char* buf, size_t count)
{
    if (!buf || count == 0) return;
    *m_Out << NStr::NumericToString(count, 0, 16) << HTTP_EOL;
    m_Out->write(buf, count);
    *m_Out << HTTP_EOL;
}


ERW_Result CCgiStreamWrapperWriter::Write(const void* buf,
                                          size_t      count,
                                          size_t*     bytes_written)
{
    ERW_Result result = eRW_Success;
    size_t written = 0;

    switch (m_Mode) {
    case CCgiStreamWrapper::eNormal:
        {
            if (!m_Out->write((char*)buf, count)) {
                result = eRW_Error;
            }
            else {
                result = eRW_Success;
                written = count;
            }
            break;
        }
    case CCgiStreamWrapper::eBlockWrites:
        {
            if ( !m_ErrorReported ) {
                if ( m_UsedChunkedTransfer ) {
                    ERR_POST_X(16, "CCgiStreamWrapperWriter::Write() -- attempt to "
                        "write data after finishing chunked transfer.");
                }
                else {
                    ERR_POST_X(15, "CCgiStreamWrapperWriter::Write() -- attempt to "
                        "write data after sending headers on HEAD request.");
                }
                m_ErrorReported = true;
            }
            // Pretend the operation was successfull so that applications
            // which check I/O result do not fail.
            written = count;
            break;
        }
    case CCgiStreamWrapper::eChunkedWrites:
        {
            const char* cbuf = static_cast<const char*>(buf);
            if (m_Chunk  &&  m_ChunkSize > 0) {
                // Copy data to own buffer
                while (count && result == eRW_Success) {
                    size_t chunk_count = min(count, m_ChunkSize - m_Count);
                    memcpy(m_Chunk + m_Count, cbuf, chunk_count);
                    cbuf += chunk_count;
                    m_Count += chunk_count;
                    count -= chunk_count;
                    written += chunk_count;
                    if (m_Count >= m_ChunkSize) {
                        x_WriteChunk(m_Chunk, m_Count);
                        if (!m_Out->good()) {
                            result = eRW_Error;
                            written -= chunk_count;
                        }
                        m_Count = 0;
                    }
                }
            }
            else {
                // If chunk size is zero, use the whole original buffer.
                x_WriteChunk(cbuf, count);
                if (m_Out->good()) {
                    written = count;
                }
                else {
                    result = eRW_Error;
                }
            }
            break;
        }
    }

    if (bytes_written) {
        *bytes_written = written;
    }
    return result;
}


ERW_Result CCgiStreamWrapperWriter::Flush(void)
{
    switch (m_Mode) {
    case CCgiStreamWrapper::eNormal:
        break;
    case CCgiStreamWrapper::eBlockWrites:
        // Report success
        return eRW_Success;
    case CCgiStreamWrapper::eChunkedWrites:
        if (m_Count) {
            x_WriteChunk(m_Chunk, m_Count);
            m_Count = 0;
        }
        break;
    }
    return m_Out->flush() ? eRW_Success : eRW_Error;
}


void CCgiStreamWrapperWriter::SetMode(CCgiStreamWrapper::EStreamMode mode)
{
    switch (mode) {
    case CCgiStreamWrapper::eNormal:
        break;
    case CCgiStreamWrapper::eBlockWrites:
        _ASSERT(m_Mode == CCgiStreamWrapper::eNormal);
        m_Out->flush();
        // Prevent output from writing anything, disable exceptions -
        // an attemp to write will be reported by the wrapper.
        m_Out->exceptions(ios_base::goodbit);
        m_Out->setstate(ios_base::badbit);
        break;
    case CCgiStreamWrapper::eChunkedWrites:
        _ASSERT(m_Mode == CCgiStreamWrapper::eNormal);
        // Use default chunk size.
        x_SetChunkSize(CCgiResponse::GetChunkSize());
        m_UsedChunkedTransfer = true;
        break;
    }
    m_Mode = mode;
}


void CCgiStreamWrapperWriter::SetCacheStream(CNcbiOstream& stream)
{
    list<CNcbiOstream*> slist;
    slist.push_back(m_Out);
    slist.push_back(&stream);
    m_Out = new CWStream(new CMultiWriter(slist), 1, 0,
        CRWStreambuf::fOwnWriter);
}


void CCgiStreamWrapperWriter::FinishChunkedTransfer(
    const CCgiStreamWrapper::TTrailer* trailer)
{
    if (m_Mode == CCgiStreamWrapper::eChunkedWrites) {
        Flush();
        // Zero chunk indicates end of chunked data.
        *m_Out << "0" << HTTP_EOL;
        x_SetChunkSize(0);
        // Allow to write trailers after the last chunk.
        SetMode(CCgiStreamWrapper::eNormal);
        if ( trailer ) {
            ITERATE(CCgiStreamWrapper::TTrailer, it, *trailer) {
                *m_Out << it->first << ": " << it->second << HTTP_EOL;
            }
        }
        // Finish chunked data/trailer.
        *m_Out << HTTP_EOL;
    }
}


void CCgiStreamWrapperWriter::AbortChunkedTransfer(void)
{
    if (m_Mode == CCgiStreamWrapper::eChunkedWrites) {
        x_SetChunkSize(0);
    }
    // Disable any writes.
    SetMode(CCgiStreamWrapper::eBlockWrites);
}


void CCgiStreamWrapperWriter::x_SetChunkSize(size_t sz)
{
    if (m_Chunk) {
        x_WriteChunk(m_Chunk, m_Count);
        delete[](m_Chunk);
        m_Chunk = 0;
    }
    m_Count = 0;
    m_ChunkSize = sz;
    if (m_ChunkSize) {
        m_Chunk = new char[m_ChunkSize];
    }
}


CCgiStreamWrapper::CCgiStreamWrapper(CNcbiOstream& out)
    : CWStream(m_Writer = new CCgiStreamWrapperWriter(out),
    1, 0, CRWStreambuf::fOwnWriter) // '1, 0' disables buffering by the wrapper
{
}


CCgiStreamWrapper::EStreamMode CCgiStreamWrapper::GetWriterMode(void)
{
    return m_Writer->GetMode();
}


void CCgiStreamWrapper::SetWriterMode(CCgiStreamWrapper::EStreamMode mode)
{
    flush();
    m_Writer->SetMode(mode);
}


void CCgiStreamWrapper::SetCacheStream(CNcbiOstream& stream)
{
    m_Writer->SetCacheStream(stream);
}


void CCgiStreamWrapper::FinishChunkedTransfer(const TTrailer* trailer)
{
    m_Writer->FinishChunkedTransfer(trailer);
}


void CCgiStreamWrapper::AbortChunkedTransfer(void)
{
    m_Writer->AbortChunkedTransfer();
}


///////////////////////////////////////////////////////
// CCgiApplication
//


CCgiApplication* CCgiApplication::Instance(void)
{
    return dynamic_cast<CCgiApplication*> (CParent::Instance());
}


extern "C" void SigTermHandler(int)
{
}


int CCgiApplication::Run(void)
{
    // Value to return from this method Run()
    int result;

    // Try to run as a Fast-CGI loop
    if ( x_RunFastCGI(&result) ) {
        return result;
    }

    CCgiRequestProcessor& processor = x_CreateProcessor();

    /// Run as a plain CGI application

    // Make sure to restore old diagnostic state after the Run()
    CDiagRestorer diag_restorer;

#if defined(NCBI_OS_UNIX)
    // Disable SIGPIPE if not allowed.
    if ( !TParamAllowSigpipe::GetDefault() ) {
        signal(SIGPIPE, SIG_IGN);
        struct sigaction sigterm,  sigtermold;
        memset(&sigterm, 0, sizeof(sigterm));
        sigterm.sa_handler = SigTermHandler;
        sigterm.sa_flags = SA_RESETHAND;
        if (sigaction(SIGTERM, &sigterm, &sigtermold) == 0
            &&  sigtermold.sa_handler != SIG_DFL) {
            sigaction(SIGTERM, &sigtermold, 0);
        }
    }

    // Compose diagnostics prefix
    PushDiagPostPrefix(NStr::IntToString(getpid()).c_str());
#endif
    PushDiagPostPrefix(GetEnvironment().Get(m_DiagPrefixEnv).c_str());

    // Timing
    CTime start_time(CTime::eCurrent);

    // Logging for statistics
    bool is_stat_log = GetConfig().GetBool("CGI", "StatLog", false,
                                           0, CNcbiRegistry::eReturn);
    bool skip_stat_log = false;
    unique_ptr<CCgiStatistics> stat(is_stat_log ? CreateStat() : 0);

    CNcbiOstream* orig_stream = NULL;
    //int orig_fd = -1;
    CNcbiStrstream result_copy;
    unique_ptr<CNcbiOstream> new_stream;
    shared_ptr<CCgiContext> context;
    unique_ptr<ICache> cache;

    try {
        _TRACE("(CGI) CCgiApplication::Run: calling ProcessRequest");
        GetDiagContext().SetAppState(eDiagAppState_RequestBegin);

        context.reset(CreateContext());
        _ASSERT(context);
        processor.SetContext(context);

        ConfigureDiagnostics(*context);
        x_AddLBCookie();
        try {
            // Print request start message
            x_OnEvent(eStartRequest, 0);

            VerifyCgiContext(*context);
            ProcessHttpReferer();
            LogRequest(*context);

            context->CheckStatus();
            
            try {
                cache.reset( GetCacheStorage() );
            } catch (const exception& ex) {
                ERR_POST_X(1, "Couldn't create cache : " << ex.what());
            }
            bool skip_process_request = false;
            bool caching_needed = IsCachingNeeded(context->GetRequest());
            if (cache && caching_needed) {
                skip_process_request = GetResultFromCache(context->GetRequest(),
                                                          context->GetResponse().out(),
                                                          *cache);
            }
            if (!skip_process_request) {
                if( cache ) {
                    CCgiStreamWrapper* wrapper = dynamic_cast<CCgiStreamWrapper*>(
                        context->GetResponse().GetOutput());
                    if ( wrapper ) {
                        wrapper->SetCacheStream(result_copy);
                    }
                    else {
                        list<CNcbiOstream*> slist;
                        orig_stream = context->GetResponse().GetOutput();
                        slist.push_back(orig_stream);
                        slist.push_back(&result_copy);
                        new_stream.reset(new CWStream(new CMultiWriter(slist), 1, 0,
                                                      CRWStreambuf::fOwnWriter));
                        context->GetResponse().SetOutput(new_stream.get());
                    }
                }
                GetDiagContext().SetAppState(eDiagAppState_Request);
                if (x_ProcessHelpRequest(processor) ||
                    x_ProcessVersionRequest(processor) ||
                    CCgiContext::ProcessCORSRequest(context->GetRequest(), context->GetResponse()) ||
                    x_ProcessAdminRequest(processor)) {
                    result = 0;
                }
                else {
                    if (!processor.ValidateSynchronizationToken()) {
                        NCBI_CGI_THROW_WITH_STATUS(CCgiRequestException, eData,
                            "Invalid or missing CSRF token.", CCgiException::e403_Forbidden);
                    }
                    result = processor.ProcessRequest(*context);
                }
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                context->GetResponse().Finalize();
                if (result != 0) {
                    processor.SetHTTPStatus(500);
                    processor.SetErrorStatus(true);
                    context->GetResponse().AbortChunkedTransfer();
                } else {
                    context->GetResponse().FinishChunkedTransfer();
                    if ( cache ) {
                        context->GetResponse().Flush();
                        if (processor.GetResultReady()) {
                            if(caching_needed)
                                SaveResultToCache(context->GetRequest(), result_copy, *cache);
                            else {
                                unique_ptr<CCgiRequest> request(GetSavedRequest(processor.GetRID(), *cache));
                                if (request.get()) 
                                    SaveResultToCache(*request, result_copy, *cache);
                            }
                        } else if (caching_needed) {
                            SaveRequest(processor.GetRID(), context->GetRequest(), *cache);
                        }
                    }
                }
            }
        }
        catch (const CCgiException& e) {
            if ( x_DoneHeadRequest(*context) ) {
                // Ignore errors after HEAD request has been finished.
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
            }
            else {
                if ( e.GetStatusCode() <  CCgiException::e200_Ok  ||
                     e.GetStatusCode() >= CCgiException::e400_BadRequest ) {
                    throw;
                }
                context->GetResponse().FinishChunkedTransfer();
                GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
                // If for some reason exception with status 2xx was thrown,
                // set the result to 0, update HTTP status and continue.
                context->GetResponse().SetStatus(e.GetStatusCode(),
                                                 e.GetStatusMessage());
            }
            result = 0;
        }

#ifdef NCBI_OS_MSWIN
        // Logging - on MSWin this must be done before flushing the output.
        if ( is_stat_log  &&  !skip_stat_log ) {
            stat->Reset(start_time, result);
            stat->Submit(stat->Compose());
        }
        is_stat_log = false;
#endif

        _TRACE("CCgiApplication::Run: flushing");
        context->GetResponse().Flush();
        _TRACE("CCgiApplication::Run: return " << result);
        x_OnEvent(result == 0 ? eSuccess : eError, result);
        x_OnEvent(eExit, result);
    }
    catch (exception& e) {
        if ( context ) {
            context->GetResponse().AbortChunkedTransfer();
        }
        GetDiagContext().SetAppState(eDiagAppState_RequestEnd);
        if ( x_DoneHeadRequest(*context) ) {
            // Ignore errors after HEAD request has been finished.
            result = 0;
            x_OnEvent(eSuccess, result);
        }
        else {
            // Call the exception handler and set the CGI exit code
            result = processor.OnException(e, NcbiCout);
            x_OnEvent(eException, result);

            // Logging
            {{
                string msg = "(CGI) CCgiApplication::ProcessRequest() failed: ";
                msg += e.what();

                if ( is_stat_log ) {
                    stat->Reset(start_time, result, &e);
                    msg = stat->Compose();
                    stat->Submit(msg);
                    skip_stat_log = true; // Don't print the same message again
                }
            }}

            // Exception reporting. Use different severity for broken connection.
            ios_base::failure* fex = dynamic_cast<ios_base::failure*>(&e);
            CNcbiOstream* os = context ? context->GetResponse().GetOutput() : NULL;
            if ((fex  &&  os  &&  !os->good())  ||  processor.GetOutputBroken()) {
                if ( !TClientConnIntOk::GetDefault() ) {
                    ERR_POST_X(13, Severity(TClientConnIntSeverity::GetDefault()) <<
                        "Connection interrupted");
                }
            }
            else {
                NCBI_REPORT_EXCEPTION_X(13, "(CGI) CCgiApplication::Run", e);
            }
        }
    }

#ifndef NCBI_OS_MSWIN
    // Logging
    if ( is_stat_log  &&  !skip_stat_log ) {
        stat->Reset(start_time, result);
        stat->Submit(stat->Compose());
    }
#endif

    x_OnEvent(eEndRequest, 120);
    x_OnEvent(eExit, result);

    if ( context ) {
        context->GetResponse().SetOutput(NULL);
    }
    return result;
}


void CCgiApplication::ProcessHttpReferer(void)
{
    // Set HTTP_REFERER
    string ref = x_GetProcessor().GetSelfReferer();
    if ( !ref.empty() ) {
        GetRWConfig().Set("CONN", "HTTP_REFERER", ref);
        CDiagContext::GetRequestContext().SetProperty("SELF_URL", ref);
    }
}


void CCgiApplication::LogRequest(void) const
{
    LogRequest(GetContext());
}

void CCgiApplication::LogRequest(const CCgiContext& ctx) const
{
    const auto& req = ctx.GetRequest();
    const auto& diag = GetDiagContext();

    string str;
    if (TPrintRequestMethodParam::GetDefault()) {
        // Print request method
        string method = req.GetRequestMethodName();
        if (!method.empty()) {
            diag.Extra().Print("REQUEST_METHOD", method);
        }
    }
    if ( TPrintSelfUrlParam::GetDefault() ) {
        // Print script URL
        string self_url = ctx.GetSelfURL();
        if ( !self_url.empty() ) {
            string args =
                req.GetRandomProperty("REDIRECT_QUERY_STRING", false);
            if ( args.empty() ) {
                args = req.GetProperty(eCgi_QueryString);
            }
            if ( !args.empty() ) {
                self_url += "?" + args;
            }
        }
        // Add target url
        string target_url = req.GetProperty(eCgi_ScriptName);
        if ( !target_url.empty() ) {
            bool secure = AStrEquiv(req.GetRandomProperty("HTTPS",
                false), "on", PNocase());
            string host = (secure ? "https://" : "http://") + diag.GetHost();
            string port = req.GetProperty(eCgi_ServerPort);
            if (!port.empty()  &&  port != (secure ? "443" : "80")) {
                host += ":" + port;
            }
            target_url = host + target_url;
        }
        if ( !self_url.empty()  ||  !target_url.empty() ) {
            diag.Extra().Print("SELF_URL", self_url).
                Print("TARGET_URL", target_url);
        }
    }
    // Print HTTP_REFERER
    if ( TPrintRefererParam::GetDefault() ) {
        str = req.GetProperty(eCgi_HttpReferer);
        if ( !str.empty() ) {
            diag.Extra().Print("HTTP_REFERER", str);
        }
    }
    // Print USER_AGENT
    if ( TPrintUserAgentParam::GetDefault() ) {
        str = req.GetProperty(eCgi_HttpUserAgent);
        if ( !str.empty() ) {
            diag.Extra().Print("USER_AGENT", str);
        }
    }
    // Print NCBI_LOG_FIELDS
    CNcbiLogFields f("http");
    map<string, string> env;
    list<string> names;
    const CNcbiEnvironment& rq_env = req.GetEnvironment();
    rq_env.Enumerate(names);
    ITERATE(list<string>, it, names) {
        if (!NStr::StartsWith(*it, "HTTP_")) continue;
        string name = it->substr(5);
        NStr::ToLower(name);
        NStr::ReplaceInPlace(name, "_", "-");
        env[name] = rq_env.Get(*it);
    }
    f.LogFields(env);
}


void CCgiApplication::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->SetArgsType(CArgDescriptions::eCgiArgs);

    CParent::SetupArgDescriptions(arg_desc);
}


CCgiContext& CCgiApplication::x_GetContext( void ) const
{
    if ( !x_IsSetProcessor() || !x_GetProcessor().IsSetContext() ) {
        ERR_POST_X(2, "CCgiApplication::GetContext: no context set");
        throw runtime_error("no context set");
    }
    return x_GetProcessor().GetContext();
}


CNcbiResource& CCgiApplication::x_GetResource( void ) const
{
    if ( !m_Resource.get() ) {
        ERR_POST_X(3, "CCgiApplication::GetResource: no resource set");
        throw runtime_error("no resource set");
    }
    return *m_Resource;
}


bool CCgiApplication::x_IsSetProcessor(void) const
{
    return m_Processor->GetValue();
}


CCgiRequestProcessor& CCgiApplication::x_GetProcessor(void) const
{
    auto proc = m_Processor->GetValue();
    if (!proc) {
        ERR_POST_X(17, "CCgiApplication::GetResource: no processor set");
        throw runtime_error("no request processor set");
    }
    return *proc;
}


CCgiRequestProcessor* CCgiApplication::x_GetProcessorOrNull(void) const
{
    return m_Processor->GetValue();
}


CCgiRequestProcessor& CCgiApplication::x_CreateProcessor(void)
{
    m_Processor->SetValue(CreateRequestProcessor());
    return x_GetProcessor();
}


NCBI_PARAM_DECL(bool, CGI, Merge_Log_Lines);
NCBI_PARAM_DEF_EX(bool, CGI, Merge_Log_Lines, true, eParam_NoThread,
                  CGI_MERGE_LOG_LINES);
typedef NCBI_PARAM_TYPE(CGI, Merge_Log_Lines) TMergeLogLines;


void CCgiApplication::Init(void)
{
    if ( TMergeLogLines::GetDefault() ) {
        // Convert multi-line diagnostic messages into one-line ones by default.
        SetDiagPostFlag(eDPF_MergeLines);
    }

    CParent::Init();

    m_Resource.reset(LoadResource());

    m_DiagPrefixEnv = GetConfig().Get("CGI", "DiagPrefixEnv");
}


void CCgiApplication::Exit(void)
{
    m_Processor->Reset();
    m_Resource.reset();
    CParent::Exit();
}


CNcbiResource* CCgiApplication::LoadResource(void)
{
    return 0;
}


CCgiServerContext* CCgiApplication::LoadServerContext(CCgiContext& /*context*/)
{
    return 0;
}


CCgiContext* CCgiApplication::CreateContext
(CNcbiArguments*   args,
 CNcbiEnvironment* env,
 CNcbiIstream*     inp,
 CNcbiOstream*     out,
 int               ifd,
 int               ofd)
{
    return CreateContextWithFlags(args, env,
        inp, out, ifd, ofd, m_RequestFlags);
}

CCgiContext* CCgiApplication::CreateContextWithFlags
(CNcbiArguments*   args,
 CNcbiEnvironment* env,
 CNcbiIstream*     inp,
 CNcbiOstream*     out,
 int               ifd,
 int               ofd,
 int               flags)
{
    return CreateContextWithFlags_Default(x_GetProcessor(),
        args, env, inp, out, ifd, ofd, flags);
}


CCgiContext* CCgiApplication::CreateContextWithFlags_Default(
    CCgiRequestProcessor& processor,
    CNcbiArguments* args,
    CNcbiEnvironment* env,
    CNcbiIstream* inp,
    CNcbiOstream* out,
    int ifd,
    int ofd,
    int flags)
{
    int errbuf_size =
        GetConfig().GetInt("CGI", "RequestErrBufSize", 256, 0,
                           CNcbiRegistry::eReturn);

    bool need_output_wrapper =
        TCGI_Count_Transfered::GetDefault()  ||
        (env && CCgiResponse::x_ClientSupportsChunkedTransfer(*env))  ||
        (env &&
        NStr::EqualNocase("HEAD",
        env->Get(CCgiRequest::GetPropertyName(eCgi_RequestMethod))));

    if ( TCGI_Count_Transfered::GetDefault() ) {
        if ( !inp ) {
            if ( !processor.IsSetInputStream() ) {
                processor.SetInputStream(
                    new CRStream(new CCGIStreamReader(std::cin), 0, 0,
                                CRWStreambuf::fOwnReader));
            }
            inp = &processor.GetInputStream();
            ifd = 0;
        }
    }
    if ( need_output_wrapper ) {
        if ( !out ) {
            if ( !processor.IsSetOutputStream() ) {
                processor.SetOutputStream(new CCgiStreamWrapper(std::cout));
            }
            out = &processor.GetOutputStream();
            ofd = 1;
            if ( processor.IsSetInputStream() ) {
                // If both streams are created by the application, tie them.
                inp->tie(out);
            }
        }
        else {
            processor.SetOutputStream(new CCgiStreamWrapper(*out));
            out = &processor.GetOutputStream();
        }
    }
    return
        new CCgiContext(*this, args, env, inp, out, ifd, ofd,
                        (errbuf_size >= 0) ? (size_t) errbuf_size : 256,
                        flags);
}


CCgiRequestProcessor* CCgiApplication::CreateRequestProcessor(void)
{
    return new CCgiRequestProcessor(*this);
}


void CCgiApplication::SetCafService(CCookieAffinity* caf)
{
    m_Caf.reset(caf);
}



// Flexible diagnostics support
//

class CStderrDiagFactory : public CDiagFactory
{
public:
    virtual CDiagHandler* New(const string&) {
        return new CStreamDiagHandler(&NcbiCerr);
    }
};


class CAsBodyDiagFactory : public CDiagFactory
{
public:
    CAsBodyDiagFactory(CCgiApplication* app) : m_App(app) {}
    virtual CDiagHandler* New(const string&) {
        CCgiResponse& response = m_App->GetContext().GetResponse();
        CDiagHandler* result   = new CStreamDiagHandler(&response.out());
        if (!response.IsHeaderWritten()) {
            response.SetContentType("text/plain");
            response.WriteHeader();
        }
        response.SetOutput(0); // suppress normal output
        return result;
    }

private:
    CCgiApplication* m_App;
};


CCgiApplication::CCgiApplication(const SBuildInfo& build_info) 
 : CNcbiApplication(build_info),
   m_RequestFlags(0),
   m_CaughtSigterm(false),
   m_Processor(new CTls<CCgiRequestProcessor>()),
   m_HostIP(0)
{
    m_Iteration.Set(0);
    // Disable system popup messages
    SuppressSystemMessageBox();

    // Turn on iteration number
    SetDiagPostFlag(eDPF_RequestId);
    SetDiagTraceFlag(eDPF_RequestId);

    SetStdioFlags(fBinaryCin | fBinaryCout);
    DisableArgDescriptions();
    RegisterDiagFactory("stderr", new CStderrDiagFactory);
    RegisterDiagFactory("asbody", new CAsBodyDiagFactory(this));
    cerr.tie(0);
}


CCgiApplication::~CCgiApplication(void)
{
    ITERATE (TDiagFactoryMap, it, m_DiagFactories) {
        delete it->second;
    }
    if ( m_HostIP )
        free(m_HostIP);
}


int CCgiApplication::OnException(exception& e, CNcbiOstream& os)
{
    return x_IsSetProcessor() ? x_GetProcessor().OnException(e, os) : -1;
}


const CArgs& CCgiApplication::GetArgs(void) const
{
    // Are there no argument descriptions or no CGI context (yet?)
    if (!GetArgDescriptions()  ||  !x_IsSetProcessor()) return CParent::GetArgs();
    return x_GetProcessor().GetArgs();
}


void CCgiApplication::x_OnEvent(CCgiRequestProcessor* pprocessor, EEvent event, int status)
{
    switch ( event ) {
    case eStartRequest:
        {
            // Set context properties
            if (!pprocessor) break;
            CCgiRequestProcessor& processor = *pprocessor;
            const CCgiRequest& req = processor.GetContext().GetRequest();

            // Print request start message
            if ( !CDiagContext::IsSetOldPostFormat() ) {
                CExtraEntryCollector collector;
                req.GetCGIEntries(collector);
                GetDiagContext().PrintRequestStart()
                    .AllowBadSymbolsInArgNames()
                    .Print(collector.GetArgs());
                processor.SetRequestStartPrinted(true);
            }

            // Set default HTTP status code (reset above by PrintRequestStart())
            processor.SetHTTPStatus(200);
            processor.SetErrorStatus(false);

            // This will log ncbi_phid as a separate 'extra' message
            // if not yet logged.
            CDiagContext::GetRequestContext().GetHitID();

            // Check if ncbi_st cookie is set
            const CCgiCookie* st = req.GetCookies().Find(g_GetNcbiString(eNcbiStrings_Stat));
            if ( st ) {
                CUrlArgs pg_info(st->GetValue());
                // Log ncbi_st values
                CDiagContext_Extra extra = GetDiagContext().Extra();
                // extra.SetType("NCBICGI");
                ITERATE(CUrlArgs::TArgs, it, pg_info.GetArgs()) {
                    extra.Print(it->name, it->value);
                }
                extra.Flush();
            }
            processor.OnEvent(event, status);
            break;
        }
    case eSuccess:
    case eError:
    case eException:
        {
            if (!pprocessor) break;
            CCgiRequestProcessor& processor = *pprocessor;
            CRequestContext& rctx = GetDiagContext().GetRequestContext();
            try {
                if ( processor.IsSetInputStream() ) {
                    auto& in = processor.GetInputStream();
                    if ( !in.good() ) {
                        in.clear();
                    }
                    rctx.SetBytesRd(NcbiStreamposToInt8(in.tellg()));
                }
            }
            catch (const exception&) {
            }
            try {
                if ( processor.IsSetOutputStream() ) {
                    auto& out = processor.GetOutputStream();
                    if ( !out.good() ) {
                        processor.SetOutputBroken(true); // set flag to indicate broken output
                        out.clear();
                    }
                    rctx.SetBytesWr(NcbiStreamposToInt8(out.tellp()));
                }
            }
            catch (const exception&) {
            }
            processor.OnEvent(event, status);
            break;
        }
    case eEndRequest:
        {
            if (!pprocessor) break;
            CCgiRequestProcessor& processor = *pprocessor;
            CCgiContext& cgi_ctx = processor.GetContext();
            CDiagContext& diag_ctx = GetDiagContext();
            CRequestContext& rctx = CDiagContext::GetRequestContext();
            // If an error status has been set by ProcessRequest, don't try
            // to check the output stream and change the status to 299/499.
            if ( !processor.GetErrorStatus() ) {
                // Log broken connection as 299/499 status
                CNcbiOstream* os = processor.IsSetContext() ?
                    cgi_ctx.GetResponse().GetOutput() : NULL;
                if ((os  &&  !os->good())  ||  processor.GetOutputBroken()) {
                    // 'Accept-Ranges: bytes' indicates a request for
                    // content length, broken connection is OK.
                    // If Content-Range is also set, the client was downloading
                    // partial data. Broken connection is not OK in this case.
                    if (TClientConnIntOk::GetDefault()  ||
                        (cgi_ctx.GetResponse().AcceptRangesBytes()  &&
                        !cgi_ctx.GetResponse().HaveContentRange())) {
                        rctx.SetRequestStatus(
                            CRequestStatus::e299_PartialContentBrokenConnection);
                    }
                    else {
                        rctx.SetRequestStatus(
                            CRequestStatus::e499_BrokenConnection);
                    }
                }
            }
            if (!CDiagContext::IsSetOldPostFormat()) {
                if (processor.GetRequestStartPrinted()) {
                    // This will also reset request context
                    diag_ctx.PrintRequestStop();
                    processor.SetRequestStartPrinted(false);
                }
                rctx.Reset();
            }
            processor.OnEvent(event, status);
            break;
        }
    case eExit:
    case eExecutable:
    case eWatchFile:
    case eExitOnFail:
    case eExitRequest:
    case eWaiting:
        {
            break;
        }
    }

    OnEvent(event, status);
}


void CCgiApplication::OnEvent(EEvent event,
                              int    exit_code)
{
    if (x_IsSetProcessor()) x_GetProcessor().OnEvent(event, exit_code);
}


void CCgiApplication::RegisterDiagFactory(const string& key,
                                          CDiagFactory* fact)
{
    m_DiagFactories[key] = fact;
}


CDiagFactory* CCgiApplication::FindDiagFactory(const string& key)
{
    TDiagFactoryMap::const_iterator it = m_DiagFactories.find(key);
    if (it == m_DiagFactories.end())
        return 0;
    return it->second;
}


void CCgiApplication::ConfigureDiagnostics(CCgiContext& context)
{
    // Disable for production servers?
    ConfigureDiagDestination(context);
    ConfigureDiagThreshold(context);
    ConfigureDiagFormat(context);
}


void CCgiApplication::ConfigureDiagDestination(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    bool   is_set;
    string dest   = request.GetEntry("diag-destination", &is_set);
    if ( !is_set )
        return;

    SIZE_TYPE colon = dest.find(':');
    CDiagFactory* factory = FindDiagFactory(dest.substr(0, colon));
    if ( factory ) {
        SetDiagHandler(factory->New(dest.substr(colon + 1)));
    }
}


void CCgiApplication::ConfigureDiagThreshold(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    bool   is_set;
    string threshold = request.GetEntry("diag-threshold", &is_set);
    if ( !is_set )
        return;

    if (threshold == "fatal") {
        SetDiagPostLevel(eDiag_Fatal);
    } else if (threshold == "critical") {
        SetDiagPostLevel(eDiag_Critical);
    } else if (threshold == "error") {
        SetDiagPostLevel(eDiag_Error);
    } else if (threshold == "warning") {
        SetDiagPostLevel(eDiag_Warning);
    } else if (threshold == "info") {
        SetDiagPostLevel(eDiag_Info);
    } else if (threshold == "trace") {
        SetDiagPostLevel(eDiag_Info);
        SetDiagTrace(eDT_Enable);
    }
}


void CCgiApplication::ConfigureDiagFormat(CCgiContext& context)
{
    const CCgiRequest& request = context.GetRequest();

    typedef map<string, TDiagPostFlags> TFlagMap;
    static CSafeStatic<TFlagMap> s_FlagMap;
    TFlagMap& flagmap = s_FlagMap.Get();

    TDiagPostFlags defaults = (eDPF_Prefix | eDPF_Severity
                               | eDPF_ErrCode | eDPF_ErrSubCode);

    if ( !CDiagContext::IsSetOldPostFormat() ) {
        defaults |= (eDPF_UID | eDPF_PID | eDPF_RequestId |
            eDPF_SerialNo | eDPF_ErrorID);
    }

    TDiagPostFlags new_flags = 0;

    bool   is_set;
    string format = request.GetEntry("diag-format", &is_set);
    if ( !is_set )
        return;

    if (flagmap.empty()) {
        flagmap["file"]        = eDPF_File;
        flagmap["path"]        = eDPF_LongFilename;
        flagmap["line"]        = eDPF_Line;
        flagmap["prefix"]      = eDPF_Prefix;
        flagmap["severity"]    = eDPF_Severity;
        flagmap["code"]        = eDPF_ErrCode;
        flagmap["subcode"]     = eDPF_ErrSubCode;
        flagmap["time"]        = eDPF_DateTime;
        flagmap["omitinfosev"] = eDPF_OmitInfoSev;
        flagmap["all"]         = eDPF_All;
        flagmap["trace"]       = eDPF_Trace;
        flagmap["log"]         = eDPF_Log;
        flagmap["errorid"]     = eDPF_ErrorID;
        flagmap["location"]    = eDPF_Location;
        flagmap["pid"]         = eDPF_PID;
        flagmap["tid"]         = eDPF_TID;
        flagmap["serial"]      = eDPF_SerialNo;
        flagmap["serial_thr"]  = eDPF_SerialNo_Thread;
        flagmap["iteration"]   = eDPF_RequestId;
        flagmap["uid"]         = eDPF_UID;
    }
    list<string> flags;
    NStr::Split(format, " ", flags,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    ITERATE(list<string>, flag, flags) {
        TFlagMap::const_iterator it;
        if ((it = flagmap.find(*flag)) != flagmap.end()) {
            new_flags |= it->second;
        } else if ((*flag)[0] == '!'
                   &&  ((it = flagmap.find(flag->substr(1)))
                        != flagmap.end())) {
            new_flags &= ~(it->second);
        } else if (*flag == "default") {
            new_flags |= defaults;
        }
    }
    SetDiagPostAllFlags(new_flags);
}


CCgiApplication::ELogOpt CCgiApplication::GetLogOpt() const
{
    string log = GetConfig().Get("CGI", "Log");

    CCgiApplication::ELogOpt logopt = eNoLog;
    if ((NStr::CompareNocase(log, "On") == 0) ||
        (NStr::CompareNocase(log, "true") == 0)) {
        logopt = eLog;
    } else if (NStr::CompareNocase(log, "OnError") == 0) {
        logopt = eLogOnError;
    }
#ifdef _DEBUG
    else if (NStr::CompareNocase(log, "OnDebug") == 0) {
        logopt = eLog;
    }
#endif

    return logopt;
}


CCgiStatistics* CCgiApplication::CreateStat()
{
    return new CCgiStatistics(*this);
}


ICgiSessionStorage* 
CCgiApplication::GetSessionStorage(CCgiSessionParameters&) const 
{
    return 0;
}


bool CCgiApplication::IsCachingNeeded(const CCgiRequest& /*request*/) const
{
    return true;
}


ICache* CCgiApplication::GetCacheStorage(void) const
{ 
    return NULL;
}


CCgiApplication::EPreparseArgs
CCgiApplication::PreparseArgs(int                argc,
                              const char* const* argv)
{
    static const char* s_ArgVersion = "-version";
    static const char* s_ArgFullVersion = "-version-full";

    if (argc != 2  ||  !argv[1]) {
        return ePreparse_Continue;
    }
    if ( NStr::strcmp(argv[1], s_ArgVersion) == 0 ) {
        // Print VERSION
        cout << GetFullVersion().Print(GetProgramDisplayName(),
            CVersion::fVersionInfo | CVersion::fPackageShort);
        return ePreparse_Exit;
    }
    else if ( NStr::strcmp(argv[1], s_ArgFullVersion) == 0 ) {
        // Print full VERSION
        cout << GetFullVersion().Print(GetProgramDisplayName());
        return ePreparse_Exit;
    }
    return ePreparse_Continue;
}


void CCgiApplication::SetRequestId(const string& rid, bool is_done)
{
    x_GetProcessor().SetRequestId(rid, is_done);
}


bool CCgiApplication::GetResultFromCache(const CCgiRequest& request, CNcbiOstream& os, ICache& cache)
{
    string checksum, content;
    if (!request.CalcChecksum(checksum, content))
        return false;

    try {
        CCacheHashedContent helper(cache);
        unique_ptr<IReader> reader( helper.GetHashedContent(checksum, content));
        if (reader.get()) {
            //cout << "(Read) " << checksum << " --- " << content << endl;
            CRStream cache_reader(reader.get());
            return NcbiStreamCopy(os, cache_reader);
        }
    } catch (const exception& ex) {
        ERR_POST_X(5, "Couldn't read cached request : " << ex.what());
    }
    return false;
}


void CCgiApplication::SaveResultToCache(const CCgiRequest& request, CNcbiIstream& is, ICache& cache)
{
    string checksum, content;
    if ( !request.CalcChecksum(checksum, content) )
        return;
    try {
        CCacheHashedContent helper(cache);
        unique_ptr<IWriter> writer( helper.StoreHashedContent(checksum, content) );
        if (writer.get()) {
            //        cout << "(Write) : " << checksum << " --- " << content << endl;
            CWStream cache_writer(writer.get());
            NcbiStreamCopy(cache_writer, is);
        }
    } catch (const exception& ex) {
        ERR_POST_X(6, "Couldn't cache request : " << ex.what());
    } 
}


void CCgiApplication::SaveRequest(const string& rid, const CCgiRequest& request, ICache& cache)
{    
    if (rid.empty())
        return;
    try {
        unique_ptr<IWriter> writer( cache.GetWriteStream(rid, 0, "NS_JID") );
        if (writer.get()) {
            CWStream cache_stream(writer.get());            
            request.Serialize(cache_stream);
        }
    } catch (const exception& ex) {
        ERR_POST_X(7, "Couldn't save request : " << ex.what());
    } 
}


CCgiRequest* CCgiApplication::GetSavedRequest(const string& rid, ICache& cache)
{
    if (rid.empty())
        return NULL;
    try {
        unique_ptr<IReader> reader(cache.GetReadStream(rid, 0, "NS_JID"));
        if (reader.get()) {
            CRStream cache_stream(reader.get());
            unique_ptr<CCgiRequest> request(new CCgiRequest);
            request->Deserialize(cache_stream, 0);
            return request.release();
        }
    } catch (const exception& ex) {
        ERR_POST_X(8, "Couldn't read saved request : " << ex.what());
    } 
    return NULL;
}


void CCgiApplication::AddLBCookie(CCgiCookies& cookies)
{
    const CNcbiRegistry& reg = GetConfig();

    string cookie_name = GetConfig().Get("CGI-LB", "Name");
    if ( cookie_name.empty() )
        return;

    int life_span = reg.GetInt("CGI-LB", "LifeSpan", 0, 0,
                               CNcbiRegistry::eReturn);

    string domain = reg.GetString("CGI-LB", "Domain", ".ncbi.nlm.nih.gov");

    if ( domain.empty() ) {
        ERR_POST_X(9, "CGI-LB: 'Domain' not specified.");
    } else {
        if (domain[0] != '.') {     // domain must start with dot
            domain.insert(0, ".");
        }
    }

    string path = reg.Get("CGI-LB", "Path");

    bool secure = reg.GetBool("CGI-LB", "Secure", false,
                              0, CNcbiRegistry::eErrPost);

    string host;

    // Getting host configuration can take some time
    // for fast CGIs we try to avoid overhead and call it only once
    // m_HostIP variable keeps the cached value

    if ( m_HostIP ) {     // repeated call
        host = m_HostIP;
    }
    else {               // first time call
        host = reg.Get("CGI-LB", "Host");
        if ( host.empty() ) {
            if ( m_Caf.get() ) {
                char  host_ip[64] = {0,};
                m_Caf->GetHostIP(host_ip, sizeof(host_ip));
                m_HostIP = m_Caf->Encode(host_ip, 0);
                host = m_HostIP;
            }
            else {
                ERR_POST_X(10, "CGI-LB: 'Host' not specified.");
            }
        }
    }


    CCgiCookie cookie(cookie_name, host, domain, path);
    if (life_span > 0) {
        CTime exp_time(CTime::eCurrent, CTime::eGmt);
        exp_time.AddSecond(life_span);
        cookie.SetExpTime(exp_time);
    }
    cookie.SetSecure(secure);
    cookies.Add(cookie);
}


void CCgiApplication::x_AddLBCookie()
{
    AddLBCookie(GetContext().GetResponse().Cookies());
}


void CCgiApplication::VerifyCgiContext(CCgiContext& context)
{
    string x_moz = context.GetRequest().GetRandomProperty("X_MOZ");
    if ( NStr::EqualNocase(x_moz, "prefetch") ) {
        NCBI_EXCEPTION_VAR(ex, CCgiRequestException, eData,
            "Prefetch is not allowed for CGIs");
        ex.SetStatus(CCgiException::e403_Forbidden);
        ex.SetSeverity(eDiag_Info);
        NCBI_EXCEPTION_THROW(ex);
    }
}


void CCgiApplication::AppStart(void)
{
    // Print application start message
    if ( !CDiagContext::IsSetOldPostFormat() ) {
        GetDiagContext().PrintStart(kEmptyStr);
    }
}


void CCgiApplication::AppStop(int exit_code)
{
    GetDiagContext().SetExitCode(exit_code);
}


const char* kToolkitRcPath = "/etc/toolkitrc";
const char* kWebDirToPort = "Web_dir_to_port";

string CCgiApplication::GetDefaultLogPath(void) const
{
    string log_path = "/log/";

    string exe_path = GetProgramExecutablePath();
    CNcbiIfstream is(kToolkitRcPath, ios::binary);
    CNcbiRegistry reg(is);
    list<string> entries;
    reg.EnumerateEntries(kWebDirToPort, &entries);
    size_t min_pos = exe_path.length();
    string web_dir;
    // Find the first dir name corresponding to one of the entries
    ITERATE(list<string>, it, entries) {
        if (!it->empty()  &&  (*it)[0] != '/') {
            // not an absolute path
            string mask = "/" + *it;
            if (mask[mask.length() - 1] != '/') {
                mask += "/";
            }
            size_t pos = exe_path.find(mask);
            if (pos < min_pos) {
                min_pos = pos;
                web_dir = *it;
            }
        }
        else {
            // absolute path
            if (exe_path.substr(0, it->length()) == *it) {
                web_dir = *it;
                break;
            }
        }
    }
    if ( !web_dir.empty() ) {
        return log_path + reg.GetString(kWebDirToPort, web_dir, kEmptyStr);
    }
    // Could not find a valid web-dir entry, use port or 'srv'
    const char* port = ::getenv("SERVER_PORT");
    return port ? log_path + string(port) : log_path + "srv";
}


void CCgiApplication::SetHTTPStatus(unsigned int status, const string& reason)
{
    if ( x_IsSetProcessor() ) {
        x_GetProcessor().SetHTTPStatus(status, reason);
    }
    else {
        CDiagContext::GetRequestContext().SetRequestStatus(status);
    }
}


bool CCgiApplication::x_DoneHeadRequest(CCgiContext& context) const
{
    const CCgiRequest& req = context.GetRequest();
    const CCgiResponse& res = context.GetResponse();
    if (req.GetRequestMethod() != CCgiRequest::eMethod_HEAD  ||
        !res.IsHeaderWritten() ) {
        return false;
    }
    return true;
}


inline
bool CCgiApplication::SAcceptEntry::operator<(const SAcceptEntry& entry) const
{
    // Prefer specific type over wildcard.
    bool this_wc = m_Type == "*";
    bool other_wc = entry.m_Type == "*";
    if (this_wc != other_wc) return !this_wc;
    // Prefer specific subtype over wildcard.
    this_wc = m_Subtype == "*";
    other_wc = entry.m_Subtype == "*";
    if (this_wc != other_wc) return !this_wc;
    // Prefer more specific media range params.
    if (m_MediaRangeParams.empty() != entry.m_MediaRangeParams.empty()) {
        return !m_MediaRangeParams.empty();
    }
    // Prefer higher quality factor.
    if (m_Quality != entry.m_Quality) return m_Quality > entry.m_Quality;
    // Otherwise sort by type/subtype values.
    if (m_Type != entry.m_Type) return m_Type < entry.m_Type;
    if (m_Subtype != entry.m_Subtype) return m_Subtype < entry.m_Subtype;
    // Otherwise (same type/subtype, quality factor and number of params)
    // consider entries equal.
    return false;
}


void CCgiApplication::ParseAcceptHeader(TAcceptEntries& entries) const
{
    x_GetProcessor().ParseAcceptHeader(entries);
}


NCBI_PARAM_DECL(bool, CGI, EnableHelpRequest);
NCBI_PARAM_DEF_EX(bool, CGI, EnableHelpRequest, true,
                  eParam_NoThread, CGI_ENABLE_HELP_REQUEST);
typedef NCBI_PARAM_TYPE(CGI, EnableHelpRequest) TEnableHelpRequest;


bool CCgiApplication::x_ProcessHelpRequest(CCgiRequestProcessor& processor)
{
    if (!TEnableHelpRequest::GetDefault()) return false;
    CCgiRequest& request = processor.GetContext().GetRequest();
    if (request.GetRequestMethod() != CCgiRequest::eMethod_GET) return false;
    bool found = false;
    string format = request.GetEntry("ncbi_help", &found);
    if ( !found ) return false;
    processor.ProcessHelpRequest(format);
    return true;
}


static const char* kStdFormats[] = { "html", "xml", "json" };
static const char* kStdContentTypes[] = { "text/html", "text/xml", "application/json" };

inline string FindContentType(CTempString format) {
    for (size_t i = 0; i < sizeof(kStdFormats) / sizeof(kStdFormats[0]); ++i) {
        if (format == kStdFormats[i]) return kStdContentTypes[i];
    }
    return kEmptyStr;
}


void CCgiApplication::ProcessHelpRequest(const string& format)
{
    x_GetProcessor().ProcessHelpRequest(format);
}


NCBI_PARAM_DECL(string, CGI, EnableVersionRequest);
NCBI_PARAM_DEF_EX(string, CGI, EnableVersionRequest, "t",
                  eParam_NoThread, CGI_ENABLE_VERSION_REQUEST);
typedef NCBI_PARAM_TYPE(CGI, EnableVersionRequest) TEnableVersionRequest;


bool CCgiApplication::x_ProcessVersionRequest(CCgiRequestProcessor& processor)
{
    CCgiRequest& request = processor.GetContext().GetRequest();
    if (request.GetRequestMethod() != CCgiRequest::eMethod_GET) return false;

    // If param value is a bool, enable the default ncbi_version CGI arg.
    // Otherwise try to use the value as the arg's name, then fallback to
    // ncbi_version.
    bool use_alt_name = false;
    string vparam = TEnableVersionRequest::GetDefault();
    if ( vparam.empty() ) return false;
    try {
        bool is_enabled = NStr::StringToBool(vparam);
        if (!is_enabled) return false;
    }
    catch (const CStringException&) {
        use_alt_name = true;
    }

    string ver_type;
    bool found = false;
    if ( use_alt_name ) {
        ver_type = request.GetEntry(vparam, &found);
    }
    if ( !found ) {
        ver_type = request.GetEntry("ncbi_version", &found);
    }
    if ( !found ) return false;

    EVersionType vt;
    if (ver_type.empty() || ver_type == "short") {
        vt = eVersion_Short;
    }
    else if (ver_type == "full") {
        vt = eVersion_Full;
    }
    else {
        NCBI_THROW(CCgiRequestException, eEntry,
            "Unsupported ncbi_version argument value");
    }
    processor.ProcessVersionRequest(vt);
    return true;
}


void CCgiApplication::ProcessVersionRequest(EVersionType ver_type)
{
    x_GetProcessor().ProcessVersionRequest(ver_type);
}

bool CCgiApplication::x_ProcessAdminRequest(CCgiRequestProcessor& processor)
{
    CCgiRequest& request = processor.GetContext().GetRequest();
    if (request.GetRequestMethod() != CCgiRequest::eMethod_GET) return false;

    bool found = false;
    string cmd_name = request.GetEntry("ncbi_admin_cmd", &found);
    if ( !found ) {
        // Check if PATH_INFO contains a command.
        string path_info = request.GetProperty(eCgi_PathInfo);
        NStr::TrimSuffixInPlace(path_info, "/");
        NStr::TrimPrefixInPlace(path_info, "/");
        if ( path_info.empty() ) return false;
        cmd_name = path_info;
    }
    EAdminCommand cmd = eAdmin_Unknown;
    if ( NStr::EqualNocase(cmd_name, "health") ) {
        cmd = eAdmin_Health;
    }
    else if ( NStr::EqualNocase(cmd_name, "deep-health") ) {
        cmd = eAdmin_HealthDeep;
    }

    // If the overriden method failed or refused to process a command,
    // fallback to the default processing which returns true for all known commands.
    return processor.ProcessAdminRequest(cmd) || processor.ProcessAdminRequest_Base(cmd);
}


bool CCgiApplication::ProcessAdminRequest(EAdminCommand cmd)
{
    return x_GetProcessor().ProcessAdminRequest(cmd);
}


NCBI_PARAM_DECL(bool, CGI, ValidateCSRFToken);
NCBI_PARAM_DEF_EX(bool, CGI, ValidateCSRFToken, false, eParam_NoThread,
    CGI_VALIDATE_CSRF_TOKEN);
typedef NCBI_PARAM_TYPE(CGI, ValidateCSRFToken) TParamValidateCSRFToken;

static const char* kCSRFTokenName = "NCBI_CSRF_TOKEN";

bool CCgiApplication::ValidateSynchronizationToken(void)
{
    return x_GetProcessor().ValidateSynchronizationToken();
}


string CCgiApplication::GetFastCGIStandaloneServer(void) const
{
    string path;
    const char* p = getenv("FCGI_STANDALONE_SERVER");
    if (p  &&  *p) {
        path = p;
    } else {
        path = GetConfig().Get("FastCGI", "StandaloneServer");
    }
    return path;
}


bool CCgiApplication::GetFastCGIStatLog(void) const
{
    return GetConfig().GetBool("CGI", "StatLog", false, 0, CNcbiRegistry::eReturn);
}


unsigned int CCgiApplication::GetFastCGIIterations(unsigned int def_iter) const
{
    int ret = def_iter;
    int x_iterations = GetConfig().GetInt("FastCGI", "Iterations", (int) def_iter, 0, CNcbiRegistry::eErrPost);
    if (x_iterations > 0) {
        ret = (unsigned int) x_iterations;
    } else {
        ERR_POST_X(6, "CCgiApplication::x_RunFastCGI:  invalid "
                      "[FastCGI].Iterations config.parameter value: "
                      << x_iterations);
        _ASSERT(def_iter);
        ret = def_iter;
    }

    int iterations_rnd_inc = GetConfig().
        GetInt("FastCGI", "Iterations_Random_Increase", 0, 0, CNcbiRegistry::eErrPost);
    if (iterations_rnd_inc > 0) {
        ret += rand() % iterations_rnd_inc;
    }

    _TRACE("CCgiApplication::Run: FastCGI limited to "
            << ret << " iterations");
    return ret;
}


bool CCgiApplication::GetFastCGIComplete_Request_On_Sigterm(void) const
{
    return GetConfig().GetBool("FastCGI", "Complete_Request_On_Sigterm", false);
}


CCgiWatchFile* CCgiApplication::CreateFastCGIWatchFile(void) const
{
    const string& orig_filename = GetConfig().Get("FastCGI", "WatchFile.Name");
    if ( !orig_filename.empty() ) {
        string filename = CDirEntry::CreateAbsolutePath
            (orig_filename, CDirEntry::eRelativeToExe);
        if (filename != orig_filename) {
            _TRACE("Adjusted relative CGI watch file name " << orig_filename
                    << " to " << filename);
        }
        int limit = GetConfig().GetInt("FastCGI", "WatchFile.Limit", -1, 0,
                                CNcbiRegistry::eErrPost);
        if (limit <= 0) {
            limit = 1024; // set a reasonable default
        }
        return new CCgiWatchFile(filename, limit);
    }
    return nullptr;
}


unsigned int CCgiApplication::GetFastCGIWatchFileTimeout(bool have_watcher) const
{
    int ret = GetConfig().GetInt("FastCGI", "WatchFile.Timeout", 0, 0, CNcbiRegistry::eErrPost);
    if (ret <= 0) {
        if ( have_watcher ) {
            ERR_POST_X(7, "CCgiApplication::x_RunFastCGI:  non-positive "
                "[FastCGI].WatchFile.Timeout conf.param. value ignored: " << ret);
        }
        return 0;
    }
    return (unsigned int) ret;
}


int CCgiApplication::GetFastCGIWatchFileRestartDelay(void) const
{
    int ret = GetConfig().GetInt("FastCGI", "WatchFile.RestartDelay", 0, 0, CNcbiRegistry::eErrPost);
    if (ret <= 0) return 0;
    // CRandom is higher-quality, but would introduce an extra
    // dependency on libxutil; rand() should be good enough here.
    srand(CCurrentProcess::GetPid());
    double r = rand() / (RAND_MAX + 1.0);
    return 1 + (int)(ret * r);
}


bool CCgiApplication::GetFastCGIChannelErrors(void) const
{
    return GetConfig().GetBool("FastCGI", "ChannelErrors", false, 0, CNcbiRegistry::eReturn);
}


bool CCgiApplication::GetFastCGIHonorExitRequest(void) const
{
    return GetConfig().GetBool("FastCGI", "HonorExitRequest", false, 0, CNcbiRegistry::eErrPost);
}

bool CCgiApplication::GetFastCGIDebug(void) const
{
    return GetConfig().GetBool("FastCGI", "Debug", false, 0, CNcbiRegistry::eErrPost);
}


bool CCgiApplication::GetFastCGIStopIfFailed(void) const
{
    return GetConfig().GetBool("FastCGI","StopIfFailed", false, 0, CNcbiRegistry::eErrPost);
}


CTime CCgiApplication::GetFileModificationTime(const string& filename)
{
    CTime mtime;
    if ( !CDirEntry(filename).GetTime(&mtime) ) {
        NCBI_THROW(CCgiErrnoException, eModTime,
                   "Cannot get modification time of the CGI executable "
                   + filename);
    }
    return mtime;
}


bool CCgiApplication::CheckMemoryLimit(void)
{
    Uint8 limit = NStr::StringToUInt8_DataSize(
        GetConfig().GetString("FastCGI", "TotalMemoryLimit", "0", CNcbiRegistry::eReturn),
        NStr::fConvErr_NoThrow);
    if ( limit ) {
        CCurrentProcess::SMemoryUsage memory_usage;
        if ( !CCurrentProcess::GetMemoryUsage(memory_usage) ) {
            ERR_POST("Could not check self memory usage" );
        }
        else if (memory_usage.total > limit) {
            ERR_POST(Warning << "Memory usage (" << memory_usage.total <<
                ") is above the configured limit (" <<
                limit << ")");
            return true;
        }
    }
    return false;
}


DEFINE_STATIC_FAST_MUTEX(s_RestartReasonMutex);

CCgiApplication::ERestartReason CCgiApplication::ShouldRestart(CTime& mtime, CCgiWatchFile* watcher, int delay)
{
    static CSafeStatic<CTime> restart_time;
    static ERestartReason restart_reason = eSR_None;

    CFastMutexGuard guard(s_RestartReasonMutex);
    if (restart_reason != eSR_None) return restart_reason;

    // Check if this CGI executable has been changed
    CTime mtimeNew = CCgiApplication::GetFileModificationTime(
        CCgiApplication::Instance()->GetArguments().GetProgramName());
    if ( mtimeNew != mtime) {
        _TRACE("CCgiApplication::x_RunFastCGI: "
               "the program modification date has changed");
        restart_reason = eSR_Executable;
    } else if ( watcher  &&  watcher->HasChanged()) {
        // Check if the file we're watching (if any) has changed
        // (based on contents, not timestamp!)
        ERR_POST_X(3, Warning <<
            "Scheduling restart of Fast-CGI, as its watch file has changed");
        restart_reason = eSR_WatchFile;
    }

    if (restart_reason != eSR_None) {
        if (restart_time->IsEmpty()) {
            restart_time->SetTimeZone(CTime::eGmt);
            restart_time->SetCurrent();
            restart_time->AddSecond(delay);
            _TRACE("Will restart Fast-CGI in " << delay << " seconds, at "
                   << restart_time->GetLocalTime().AsString("h:m:s"));
        }
        if (CurrentTime(CTime::eGmt) >= *restart_time) {
            return restart_reason;
        }
    }

    return eSR_None;
}


void CCgiApplication::InitArgs(CArgs& args, CCgiContext& context) const
{
    // Copy cmd-line arg values to CGI args
    args.Assign(CParent::GetArgs());
    // Add CGI parameters to the CGI version of args
    GetArgDescriptions()->ConvertKeys(&args, context.GetRequest().GetEntries(), true /*update=yes*/);
}


///////////////////////////////////////////////////////
// CCgiStatistics
//


CCgiStatistics::CCgiStatistics(CCgiApplication& cgi_app)
    : m_CgiApp(cgi_app), m_LogDelim(";")
{
}


CCgiStatistics::~CCgiStatistics()
{
}


void CCgiStatistics::Reset(const CTime& start_time,
                           int          result,
                           const std::exception*  ex)
{
    m_StartTime = start_time;
    m_Result    = result;
    m_ErrMsg    = ex ? ex->what() : kEmptyStr;
}


string CCgiStatistics::Compose(void)
{
    const CNcbiRegistry& reg = m_CgiApp.GetConfig();
    CTime end_time(CTime::eCurrent);

    // Check if it is assigned NOT to log the requests took less than
    // cut off time threshold
    TSeconds time_cutoff = reg.GetInt("CGI", "TimeStatCutOff", 0, 0,
                                      CNcbiRegistry::eReturn);
    if (time_cutoff > 0) {
        TSeconds diff = end_time.DiffSecond(m_StartTime);
        if (diff < time_cutoff) {
            return kEmptyStr;  // do nothing if it is a light weight request
        }
    }

    string msg, tmp_str;

    tmp_str = Compose_ProgramName();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    tmp_str = Compose_Result();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    bool is_timing =
        reg.GetBool("CGI", "TimeStamp", false, 0, CNcbiRegistry::eErrPost);
    if ( is_timing ) {
        tmp_str = Compose_Timing(end_time);
        if ( !tmp_str.empty() ) {
            msg.append(tmp_str);
            msg.append(m_LogDelim);
        }
    }

    tmp_str = Compose_Entries();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
    }

    tmp_str = Compose_ErrMessage();
    if ( !tmp_str.empty() ) {
        msg.append(tmp_str);
        msg.append(m_LogDelim);
    }

    return msg;
}


void CCgiStatistics::Submit(const string& message)
{
    LOG_POST_X(11, message);
}


string CCgiStatistics::Compose_ProgramName(void)
{
    return m_CgiApp.GetArguments().GetProgramName();
}


string CCgiStatistics::Compose_Timing(const CTime& end_time)
{
    CTimeSpan elapsed = end_time.DiffTimeSpan(m_StartTime);
    return m_StartTime.AsString() + m_LogDelim + elapsed.AsString();
}


string CCgiStatistics::Compose_Entries(void)
{
    if (!m_CgiApp.x_GetProcessor().IsSetContext()) return kEmptyStr;
    const CCgiRequest& cgi_req = m_CgiApp.x_GetProcessor().GetContext().GetRequest();

    // LogArgs - list of CGI arguments to log.
    // Can come as list of arguments (LogArgs = param1;param2;param3),
    // or be supplemented with aliases (LogArgs = param1=1;param2=2;param3).
    // When alias is provided we use it for logging purposes (this feature
    // can be used to save logging space or reduce the net traffic).
    const CNcbiRegistry& reg = m_CgiApp.GetConfig();
    string log_args = reg.Get("CGI", "LogArgs");
    if ( log_args.empty() )
        return kEmptyStr;

    list<string> vars;
    NStr::Split(log_args, ",; \t", vars,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    string msg;
    ITERATE (list<string>, i, vars) {
        bool is_entry_found;
        const string& arg = *i;

        size_t pos = arg.find_last_of('=');
        if (pos == 0) {
            return "<misconf>" + m_LogDelim;
        } else if (pos != string::npos) {   // alias assigned
            string key = arg.substr(0, pos);
            const CCgiEntry& entry = cgi_req.GetEntry(key, &is_entry_found);
            if ( is_entry_found ) {
                string alias = arg.substr(pos+1, arg.length());
                msg.append(alias);
                msg.append("='");
                msg.append(entry.GetValue());
                msg.append("'");
                msg.append(m_LogDelim);
            }
        } else {
            const CCgiEntry& entry = cgi_req.GetEntry(arg, &is_entry_found);
            if ( is_entry_found ) {
                msg.append(arg);
                msg.append("='");
                msg.append(entry.GetValue());
                msg.append("'");
                msg.append(m_LogDelim);
            }
        }
    }

    return msg;
}


string CCgiStatistics::Compose_Result(void)
{
    return NStr::IntToString(m_Result);
}


string CCgiStatistics::Compose_ErrMessage(void)
{
    return m_ErrMsg;
}


/////////////////////////////////////////////////////////////////////////////
//  CCgiWatchFile

int CCgiWatchFile::x_Read(char* buf)
{
    CNcbiIfstream in(m_Filename.c_str());
    if (in) {
        in.read(buf, m_Limit);
        return (int) in.gcount();
    } else {
        return -1;
    }
}

CCgiWatchFile::CCgiWatchFile(const string& filename, int limit)
        : m_Filename(filename), m_Limit(limit), m_Buf(new char[limit])
{
    m_Count = x_Read(m_Buf.get());
    if (m_Count < 0) {
        ERR_POST_X(2, "Failed to open CGI watch file " << filename);
    }
}

bool CCgiWatchFile::HasChanged(void)
{
    TBuf buf(new char[m_Limit]);
    if (x_Read(buf.get()) != m_Count) {
        return true;
    } else if (m_Count == -1) { // couldn't be opened
        return false;
    } else {
        return memcmp(buf.get(), m_Buf.get(), m_Count) != 0;
    }
    // no need to update m_Count or m_Buf, since the CGI will restart
    // if there are any discrepancies.
}


///////////////////////////////////////////////////////
// CCgiRequestProcessor
//


CCgiRequestProcessor::CCgiRequestProcessor(CCgiApplication& app)
    : m_App(app)
{
}


CCgiRequestProcessor::~CCgiRequestProcessor(void)
{
}


int CCgiRequestProcessor::ProcessRequest(CCgiContext& context)
{
    _ASSERT(m_Context.get() == &context);
    return m_App.ProcessRequest(context);
}


void CCgiRequestProcessor::ProcessHelpRequest(const string& format)
{
    string base_name = m_App.GetProgramExecutablePath();
    string fname;
    string content_type;
    // If 'format' is set, try to find <basename>.help.<format> or help.<format> file.
    if ( !format.empty() ) {
        string fname_fmt = base_name + ".help." + format;
        if ( CFile(fname_fmt).Exists() ) {
            fname = fname_fmt;
            content_type = FindContentType(format);
        }
        else {
            fname_fmt = "help." + format;
            if ( CFile(fname_fmt).Exists() ) {
                fname = fname_fmt;
                content_type = FindContentType(format);
            }
        }
    }

    // If 'format' is not set or there's no file of the specified format, check
    // 'Accept:' header.
    if ( fname.empty() ) {
        TAcceptEntries entries;
        ParseAcceptHeader(entries);
        ITERATE(TAcceptEntries, it, entries) {
            string fname_accept = base_name + ".help." + it->m_Subtype + it->m_MediaRangeParams;
            if ( CFile(fname_accept).Exists() ) {
                fname = fname_accept;
                content_type = it->m_Type + "/" + it->m_Subtype;
                break;
            }
        }
        if ( fname.empty() ) {
            ITERATE(TAcceptEntries, it, entries) {
                string fname_accept = "help." + it->m_Subtype + it->m_MediaRangeParams;
                if ( CFile(fname_accept).Exists() ) {
                    fname = fname_accept;
                    content_type = it->m_Type + "/" + it->m_Subtype;
                    break;
                }
            }
        }
    }

    // Finally, check standard formats: html/xml/json
    if ( fname.empty() ) {
        for (size_t i = 0; i < sizeof(kStdFormats) / sizeof(kStdFormats[0]); ++i) {
            string fname_std = base_name + ".help." + kStdFormats[i];
            if ( CFile(fname_std).Exists() ) {
                fname = fname_std;
                content_type = kStdContentTypes[i];
                break;
            }
        }
    }
    if ( fname.empty() ) {
        for (size_t i = 0; i < sizeof(kStdFormats) / sizeof(kStdFormats[0]); ++i) {
            string fname_std = string("help.") + kStdFormats[i];
            if ( CFile(fname_std).Exists() ) {
                fname = fname_std;
                content_type = kStdContentTypes[i];
                break;
            }
        }
    }

    CCgiResponse& response = GetContext().GetResponse();
    if ( !fname.empty() ) {
        CNcbiIfstream in(fname.c_str());

        // Check if the file starts with "Content-type:" header followed by double-eol.
        bool ct_found = false;
        string ct;
        getline(in, ct);
        if ( NStr::StartsWith(ct, "content-type:", NStr::eNocase) ) {
            string eol;
            getline(in, eol);
            if ( eol.empty() ) {
                ct_found = true;
                content_type = NStr::TruncateSpaces(ct.substr(13));
            }
        }
        if ( !ct_found ) {
            in.seekg(0);
        }

        if ( !content_type.empty()) {
            response.SetContentType(content_type);
        }
        response.WriteHeader();
        NcbiStreamCopy(*response.GetOutput(), in);
    }
    else {
        // Could not find help file, use arg descriptions instead.
        const CArgDescriptions* args = m_App.GetArgDescriptions();
        if ( args ) {
            response.SetContentType("text/xml");
            response.WriteHeader();
            args->PrintUsageXml(*response.GetOutput());
        }
        else {
            NCBI_THROW(CCgiRequestException, eData,
                "Can not find help for CGI application");
        }
    }
}


void CCgiRequestProcessor::ProcessVersionRequest(CCgiApplication::EVersionType ver_type)
{
    string format = "plain";
    string content_type = "text/plain";
    TAcceptEntries entries;
    ParseAcceptHeader(entries);
    ITERATE(TAcceptEntries, it, entries) {
        if (it->m_Subtype == "xml" || it->m_Subtype == "json" ||
            (it->m_Type == "text" && it->m_Subtype == "plain")) {
            format = it->m_Subtype;
            content_type = it->m_Type + "/" + it->m_Subtype;
            break;
        }
    }

    CCgiResponse& response = GetContext().GetResponse();
    response.SetContentType(content_type);
    response.WriteHeader();
    CNcbiOstream& out = *response.GetOutput();
    if (format == "plain") {
        switch (ver_type) {
        case CCgiApplication::eVersion_Short:
            out << m_App.GetVersion().Print();
            break;
        case CCgiApplication::eVersion_Full:
            out << m_App.GetFullVersion().Print(m_App.GetAppName());
            break;
        }
    }
    else if (format == "xml") {
        switch (ver_type) {
        case CCgiApplication::eVersion_Short:
            out << m_App.GetFullVersion().PrintXml(kEmptyStr, CVersion::fVersionInfo);
            break;
        case CCgiApplication::eVersion_Full:
            out << m_App.GetFullVersion().PrintXml(m_App.GetAppName());
            break;
        }
    }
    else if (format == "json") {
        switch (ver_type) {
        case CCgiApplication::eVersion_Short:
            out << m_App.GetFullVersion().PrintJson(kEmptyStr, CVersion::fVersionInfo);
            break;
        case CCgiApplication::eVersion_Full:
            out << m_App.GetFullVersion().PrintJson(m_App.GetAppName());
            break;
        }
    }
    else {
        NCBI_THROW(CCgiRequestException, eData,
            "Unsupported version format");
    }
}


bool CCgiRequestProcessor::ProcessAdminRequest(CCgiApplication::EAdminCommand cmd)
{
    return ProcessAdminRequest_Base(cmd);
}


bool CCgiRequestProcessor::ProcessAdminRequest_Base(CCgiApplication::EAdminCommand cmd)
{
    if (cmd == CCgiApplication::eAdmin_Unknown) return false;

    // By default report status 200 and write headers for any command.
    CCgiResponse& response = GetContext().GetResponse();
    response.SetContentType("text/plain");
    SetHTTPStatus(CCgiException::e200_Ok,
        CCgiException::GetStdStatusMessage(CCgiException::e200_Ok));
    response.WriteHeader();
    return true;
}


bool CCgiRequestProcessor::ValidateSynchronizationToken(void)
{
    if (!TParamValidateCSRFToken::GetDefault()) return true;
    const CCgiRequest& req = GetContext().GetRequest();
    const string& token = req.GetRandomProperty(kCSRFTokenName, false);
    return !token.empty() && token == req.GetTrackingCookie();
}


string CCgiRequestProcessor::GetSelfReferer(void) const
{
    string ref = m_Context->GetSelfURL();
    if ( !ref.empty() ) {
        string args = m_Context->GetRequest().GetProperty(eCgi_QueryString);
        if ( !args.empty() ) ref += "?" + args;
    }
    return ref;
}


int CCgiRequestProcessor::OnException(std::exception& e, CNcbiOstream& os)
{
    // Discriminate between different types of error
    string status_str = "500 Server Error";
    string message = "";

    // Save current HTTP status. Later it may be changed to 299 or 499
    // depending on this value.
    SetErrorStatus(CDiagContext::GetRequestContext().GetRequestStatus() >= 400);
    SetHTTPStatus(500);

    CException* ce = dynamic_cast<CException*> (&e);
    if ( ce ) {
        message = ce->GetMsg();
        CCgiException* cgi_e = dynamic_cast<CCgiException*>(&e);
        if ( cgi_e ) {
            if ( cgi_e->GetStatusCode() != CCgiException::eStatusNotSet ) {
                SetHTTPStatus(cgi_e->GetStatusCode());
                status_str = NStr::IntToString(cgi_e->GetStatusCode()) +
                    " " + cgi_e->GetStatusMessage();
            }
            else {
                // Convert CgiRequestException and CCgiArgsException
                // to error 400
                if (dynamic_cast<CCgiRequestException*> (&e)  ||
                    dynamic_cast<CUrlException*> (&e)) {
                    SetHTTPStatus(400);
                    status_str = "400 Malformed HTTP Request";
                }
            }
        }
    }
    else {
        message = e.what();
    }

    // Don't try to write to a broken output
    if (!os.good()  ||  GetOutputBroken()) {
        return -1;
    }

    try {
        // HTTP header
        os << "Status: " << status_str << HTTP_EOL;
        os << "Content-Type: text/plain" HTTP_EOL HTTP_EOL;

        // Message
        os << "ERROR:  " << status_str << " " HTTP_EOL HTTP_EOL;
        os << NStr::HtmlEncode(message);

        if ( dynamic_cast<CArgException*> (&e) ) {
            string ustr;
            const CArgDescriptions* descr = m_App.GetArgDescriptions();
            if (descr) {
                os << descr->PrintUsage(ustr) << HTTP_EOL HTTP_EOL;
            }
        }

        // Check for problems in sending the response
        if ( !os.good() ) {
            ERR_POST_X(4, "CCgiApplication::OnException() failed to send error page"
                          " back to the client");
            return -1;
        }
    }
    catch (const exception& ex) {
        NCBI_REPORT_EXCEPTION_X(14, "(CGI) CCgiApplication::Run", ex);
    }
    return 0;
}


void CCgiRequestProcessor::OnEvent(CCgiApplication::EEvent /*event*/, int /*status*/)
{
    return;
}


void CCgiRequestProcessor::SetHTTPStatus(unsigned int status, const string& reason)
{
    if ( m_Context.get() ) {
        m_Context->GetResponse().SetStatus(status, reason);
    }
    else {
        CDiagContext::GetRequestContext().SetRequestStatus(status);
    }
}


void CCgiRequestProcessor::SetRequestId(const string& rid, bool is_done)
{
    m_RID = rid;
    m_IsResultReady = is_done;
}


void CCgiRequestProcessor::ParseAcceptHeader(TAcceptEntries& entries) const
{
    string accept = GetContext().GetRequest().GetProperty(eCgi_HttpAccept);
    if (accept.empty()) return;
    list<string> types;
    NStr::Split(accept, ",", types, NStr::fSplit_MergeDelimiters);
    ITERATE(list<string>, type_it, types) {
        list<string> parts;
        NStr::Split(NStr::TruncateSpaces(*type_it), ";", parts, NStr::fSplit_MergeDelimiters);
        if ( parts.empty() ) continue;
        entries.push_back(TAcceptEntry());
        TAcceptEntry& entry = entries.back();
        NStr::SplitInTwo(NStr::TruncateSpaces(parts.front()), "/", entry.m_Type, entry.m_Subtype);
        NStr::TruncateSpacesInPlace(entry.m_Type);
        NStr::TruncateSpacesInPlace(entry.m_Subtype);
        list<string>::const_iterator ext_it = parts.begin();
        ++ext_it; // skip type/subtype
        bool aparams = false;
        while (ext_it != parts.end()) {
            string name, value;
            NStr::SplitInTwo(NStr::TruncateSpaces(*ext_it), "=", name, value);
            NStr::TruncateSpacesInPlace(name);
            NStr::TruncateSpacesInPlace(value);
            if (name == "q") {
                entry.m_Quality = NStr::StringToNumeric<float>(value, NStr::fConvErr_NoThrow);
                if (entry.m_Quality == 0 && errno != 0) {
                    entry.m_Quality = 1;
                }
                aparams = true;
                ++ext_it;
                continue;
            }
            if (aparams) {
                entry.m_AcceptParams[name] = value;
            }
            else {
                 entry.m_MediaRangeParams += ";" + name + "=" + value;
            }
            ++ext_it;
        }
    }
    entries.sort();
}


void CCgiRequestProcessor::x_InitArgs(void) const
{
    m_CgiArgs.reset(new CArgs());
    m_App.InitArgs(*m_CgiArgs, *m_Context);
}

/////////////////////////////////////////////////////////////////////////////
//  Tracking Environment

NCBI_PARAM_DEF(bool, CGI, DisableTrackingCookie, false);
NCBI_PARAM_DEF(string, CGI, TrackingCookieName, "ncbi_sid");
NCBI_PARAM_DEF(string, CGI, TrackingTagName, "NCBI-SID");
NCBI_PARAM_DEF(string, CGI, TrackingCookieDomain, ".nih.gov");
NCBI_PARAM_DEF(string, CGI, TrackingCookiePath, "/");


END_NCBI_SCOPE
