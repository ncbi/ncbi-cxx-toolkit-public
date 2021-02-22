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
 * Author:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <cgi/cgiapp_cached.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_serial.hpp>

#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <util/xregexp/regexp.hpp>
#include <util/checksum.hpp>
#include <util/retry_ctx.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/grid_app_version_info.hpp>
#include <connect/services/ns_output_parser.hpp>

#include <connect/email_diag_handler.hpp>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>

#include <array>
#include <vector>
#include <map>
#include <sstream>
#include <unordered_map>

#define GRID_APP_NAME "cgi2rcgi"

USING_NCBI_SCOPE;

#define HTTP_NCBI_JSID         "NCBI-JSID"

static const string kSinceTime = "ctg_time";

enum EEntryPulling {
    fUseQueryString    = 1 << 0,
    fUseRequestContent = 1 << 1,
    fMakePersistent    = 1 << 2,
    eDefaultPulling    = fUseQueryString | fUseRequestContent | fMakePersistent,

    // Old values, now synonyms
    eUseQueryString    = fUseQueryString,
    eUseRequestContent = fUseRequestContent,
};


/** @addtogroup NetScheduleClient
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////
//  Grid Cgi Context
//  Context in which a request is processed
//
class CGridCgiContext
{
public:
    CGridCgiContext(CHTMLPage& page,
        CHTMLPage& custom_http_header, CCgiContext& ctx);

    // Get the HTML page
    CHTMLPage&    GetHTMLPage()       { return m_Page; }

    // Get the self URL
    string        GetSelfURL() const;

    // Get current job progress message
    const string& GetJobProgressMessage() const
        { return m_ProgressMsg; }

    // Get a value from a CGI request. if there is no an entry with a
    // given name it returns an empty string.
    const string& GetPersistentEntryValue(const string& entry_name) const;

    void GetQueryStringEntryValue(const string& entry_name,
        string& value) const;
    void GetRequestEntryValue(const string& entry_name, string& value) const;

    typedef map<string,string>    TPersistentEntries;

    void PullUpPersistentEntry(const string& entry_name);
    void PullUpPersistentEntry(const string& entry_name, string& value, int pulling = eDefaultPulling);
    void DefinePersistentEntry(const string& entry_name, const string& value);
    const TPersistentEntries& GetPersistentEntries() const
        { return m_PersistentEntries; }
    bool HasCtgTime() const { return m_PersistentEntries.find(kSinceTime) != m_PersistentEntries.end(); }

    void LoadQueryStringTags(CHTMLPlainText::EEncodeMode encode_mode);

    // Get CGI Context
    CCgiContext& GetCGIContext() { return m_CgiContext; }

    void SelectView(const string& view_name);
    bool NeedRenderPage() const { return m_NeedRenderPage; }
    void NeedRenderPage(bool value) { m_NeedRenderPage = value; }
    bool NeedMetaRefresh() const { return m_NeedMetaRefresh; }

    string& GetJobKey() { return m_JobKey; }
    string& GetJqueryCallback() { return m_JqueryCallback; }

public:
    // Remove all persistent entries from cookie and self url.
    void Clear();

    void SetJobProgressMessage(const string& msg)
        { m_ProgressMsg = msg; }

    void AddTagMap(const string& n, const string& v, CHTMLPlainText::EEncodeMode m = CHTMLPlainText::eHTMLEncode)
    {
        m_Page.AddTagMap(n, new CHTMLPlainText(m, v));
        m_CustomHTTPHeader.AddTagMap(n, new CHTMLPlainText(m, NStr::Sanitize(v)));
    }

private:
    CHTMLPage&                    m_Page;
    CHTMLPage&                    m_CustomHTTPHeader;
    CCgiContext&                  m_CgiContext;
    TCgiEntries                   m_ParsedQueryString;
    TPersistentEntries            m_PersistentEntries;
    string                        m_ProgressMsg;
    string                        m_JobKey;
    string                        m_JqueryCallback;
    bool                          m_NeedRenderPage;
    bool                          m_NeedMetaRefresh;
};


/////////////////////////////////////////////////////////////////////////////


CGridCgiContext::CGridCgiContext(CHTMLPage& page,
        CHTMLPage& custom_http_header, CCgiContext& ctx) :
    m_Page(page),
    m_CustomHTTPHeader(custom_http_header),
    m_CgiContext(ctx),
    m_NeedRenderPage(true)
{
    const CCgiRequest& req = ctx.GetRequest();
    string query_string = req.GetProperty(eCgi_QueryString);
    CCgiRequest::ParseEntries(query_string, m_ParsedQueryString);

    const string kNoMetaRefreshHeader = "X_NCBI_RETRY_NOMETAREFRESH";
    const string& no_meta_refresh = req.GetRandomProperty(kNoMetaRefreshHeader);
    m_NeedMetaRefresh = no_meta_refresh.empty() || no_meta_refresh == "0";
}

string CGridCgiContext::GetSelfURL() const
{
    string url = m_CgiContext.GetSelfURL();
    bool first = true;
    TPersistentEntries::const_iterator it;
    for (it = m_PersistentEntries.begin();
         it != m_PersistentEntries.end(); ++it) {
        const string& name = it->first;
        const string& value = it->second;
        if (!name.empty() && !value.empty()) {
            if (first) {
                url += '?';
                first = false;
            }
            else
                url += '&';
            url += name + '=' + NStr::URLEncode(value);
        }
    }
    return url;
}

const string& CGridCgiContext::GetPersistentEntryValue(
    const string& entry_name) const
{
    TPersistentEntries::const_iterator it = m_PersistentEntries.find(entry_name);
    if (it != m_PersistentEntries.end())
        return it->second;
    return kEmptyStr;
}

void CGridCgiContext::GetQueryStringEntryValue(const string& entry_name,
    string& value) const
{
    ITERATE(TCgiEntries, eit, m_ParsedQueryString) {
        if (NStr::CompareNocase(entry_name, eit->first) == 0) {
            string v = eit->second;
            if (!v.empty())
                value = v;
        }
    }
}

void CGridCgiContext::GetRequestEntryValue(const string& entry_name,
    string& value) const
{
    const TCgiEntries entries = m_CgiContext.GetRequest().GetEntries();
    ITERATE(TCgiEntries, eit, entries) {
        if (NStr::CompareNocase(entry_name, eit->first) == 0) {
            string v = eit->second;
            if (!v.empty())
                value = v;
        }
    }
}

void CGridCgiContext::PullUpPersistentEntry(const string& entry_name)
{
    string value = kEmptyStr;
    PullUpPersistentEntry(entry_name, value);
}

void CGridCgiContext::PullUpPersistentEntry(
    const string& entry_name, string& value, int pulling)
{
    if (pulling & fUseQueryString) {
        GetQueryStringEntryValue(entry_name, value);
    }

    if (value.empty() && (pulling & fUseRequestContent)) {
        GetRequestEntryValue(entry_name, value);
    }

    if (pulling & fMakePersistent) {
        NStr::TruncateSpacesInPlace(value);
        DefinePersistentEntry(entry_name, value);
    }
}

void CGridCgiContext::DefinePersistentEntry(const string& entry_name,
    const string& value)
{
    if (value.empty()) {
        TPersistentEntries::iterator it =
              m_PersistentEntries.find(entry_name);
        if (it != m_PersistentEntries.end())
            m_PersistentEntries.erase(it);
    } else {
        m_PersistentEntries[entry_name] = value;
    }
}

void CGridCgiContext::LoadQueryStringTags(
        CHTMLPlainText::EEncodeMode encode_mode)
{
    ITERATE(TCgiEntries, eit, m_ParsedQueryString) {
        AddTagMap("QUERY_STRING:" + eit->first, eit->second, encode_mode);
    }
}

void CGridCgiContext::Clear()
{
    m_PersistentEntries.clear();
}

void CGridCgiContext::SelectView(const string& view_name)
{
    m_CustomHTTPHeader.AddTagMap("CUSTOM_HTTP_HEADER",
        new CHTMLText("<@HEADER_" + view_name + "@>"));
    m_Page.AddTagMap("STAT_VIEW", new CHTMLText("<@VIEW_" + view_name + "@>"));
}

/////////////////////////////////////////////////////////////////////////////
//
//  Grid CGI Front-end Application
//
//  Class for CGI applications starting background jobs using
//  NetSchedule. Implements job submission, status check,
//  error processing, etc.  All request processing is done on the back end.
//  CGI application is responsible for UI rendering.
//
class CCgi2RCgiApp : public CCgiApplicationCached
{
public:
    // This method is called on the CGI application initialization -- before
    // starting to process a HTTP request or even receiving one.
    virtual void Init();

    // Factory method for the Context object construction.
    virtual CCgiContext* CreateContextWithFlags(CNcbiArguments* args,
        CNcbiEnvironment* env, CNcbiIstream* inp, CNcbiOstream* out,
            int ifd, int ofd, int flags);

    // The main method of this CGI application.
    // HTTP requests are processed in this method.
    virtual int ProcessRequest(CCgiContext& ctx);

private:
    void DefineRefreshTags(CGridCgiContext& grid_ctx, const string& url, int delay);

private:
    void ListenJobs(const string& job_ids_value, const string& timeout_value);
    void CheckJob(CGridCgiContext& grid_ctx);
    void SubmitJob(CCgiRequest& request, CGridCgiContext& grid_ctx);
    void PopulatePage(CGridCgiContext& grid_ctx);
    int RenderPage();
    CNetScheduleAPI::EJobStatus GetStatus(CGridCgiContext&);
    CNetScheduleAPI::EJobStatus GetStatusAndCtgTime(CGridCgiContext& grid_ctx);
    bool CheckIfJobDone(CGridCgiContext&, CNetScheduleAPI::EJobStatus);

    int m_RefreshDelay;
    int m_RefreshWait;
    int m_FirstDelay;

    CNetScheduleAPIExt m_NetScheduleAPI;
    CNetCacheAPI m_NetCacheAPI;
    unique_ptr<CGridClient> m_GridClient;
    CCgiResponse* m_Response;

private:
    void ReadJob(istream&, CGridCgiContext&);

    // This method is called when result is available immediately
    void OnJobDone(CGridCgiContext&);

    // This method is called when the worker node reported a failure.
    void OnJobFailed(const string& msg, CGridCgiContext& ctx);

    string m_Title;
    string m_FallBackUrl;
    int    m_FallBackDelay;
    int    m_CancelGoBackDelay;
    string m_DateFormat;
    string m_ElapsedTimeFormat;
    bool m_InterceptJQueryCallback;
    bool m_AddJobIdToHeader;
    bool m_DisplayDonePage;

    unique_ptr<CHTMLPage> m_Page;
    unique_ptr<CHTMLPage> m_CustomHTTPHeader;

    string m_HtmlTemplate;
    vector<string> m_HtmlIncs;

    string m_AffinityName;
    int m_AffinitySource;
    int m_AffinitySetLimit;

    string m_ContentType;

    CHTMLPlainText::EEncodeMode m_TargetEncodeMode;
    bool m_HTMLPassThrough;
    bool m_PortAdded;
};

void CCgi2RCgiApp::Init()
{
    // Standard CGI framework initialization
    CCgiApplicationCached::Init();

    // Grid client initialization
    CNcbiRegistry& config = GetRWConfig();
    string grid_cgi_section("grid_cgi");

    // Must correspond to TEnableVersionRequest
    config.Set("CGI", "EnableVersionRequest", "false");

    // Default value must correspond to SRCgiWait value
    m_RefreshDelay = config.GetInt(grid_cgi_section,
        "refresh_delay", 5, IRegistry::eReturn);

    m_RefreshWait = config.GetInt(grid_cgi_section,
        "refresh_wait", 0, IRegistry::eReturn);
    if (m_RefreshWait < 0)  m_RefreshWait = 0;
    if (m_RefreshWait > 20) m_RefreshWait = 20;

    m_FirstDelay = config.GetInt(grid_cgi_section,
        "expect_complete", 5, IRegistry::eReturn);

    if (m_FirstDelay > 20)
        m_FirstDelay = 20;

    if (m_FirstDelay < 0)
        m_FirstDelay = 0;

    m_NetScheduleAPI = CNetScheduleAPI(config);
    m_NetCacheAPI = CNetCacheAPI(config, kEmptyStr, m_NetScheduleAPI);

    m_GridClient.reset(new CGridClient(
        m_NetScheduleAPI.GetSubmitter(),
        m_NetCacheAPI,
        config.GetBool(grid_cgi_section, "automatic_cleanup",
            true, IRegistry::eReturn) ?
                CGridClient::eAutomaticCleanup : CGridClient::eManualCleanup,
        config.GetBool(grid_cgi_section, "use_progress",
            true, IRegistry::eReturn) ?
                CGridClient::eProgressMsgOn : CGridClient::eProgressMsgOff));

    // Allows CGI client to put the diagnostics to:
    //   HTML body (as comments) -- using CGI arg "&diag-destination=comments"
    RegisterDiagFactory("comments", new CCommentDiagFactory);
    //   E-mail -- using CGI arg "&diag-destination=email:user@host"
    RegisterDiagFactory("email",    new CEmailDiagFactory);


    // Initialize processing of both cmd-line arguments and HTTP entries

    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Cgi2RCgi application");

    arg_desc->AddOptionalKey("Cancel",
                             "Cancel",
                             "Cancel Job",
                             CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    // Read configuration parameters
    string cgi2rcgi_section("cgi2rcgi");

    m_ContentType = config.GetString(cgi2rcgi_section,
        "content_type", kEmptyStr);
    if (m_ContentType.empty() || m_ContentType == "text/html") {
        m_TargetEncodeMode = CHTMLPlainText::eHTMLEncode;
        m_HTMLPassThrough = config.GetBool(cgi2rcgi_section,
                "html_pass_through", false);
    } else {
        m_TargetEncodeMode = m_ContentType == "application/json" ?
                CHTMLPlainText::eJSONEncode : CHTMLPlainText::eNoEncode;
        m_HTMLPassThrough = true;
    }

    m_Title = config.GetString(cgi2rcgi_section, "cgi_title",
                                    "Remote CGI Status Checker");

    m_HtmlTemplate = config.GetString(cgi2rcgi_section, "html_template",
                                           "cgi2rcgi.html");

    string incs = config.GetString(cgi2rcgi_section, "html_template_includes",
                                        "cgi2rcgi.inc.html");

    NStr::Split(incs, ",; ", m_HtmlIncs,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);


    m_FallBackUrl = config.GetString(cgi2rcgi_section,
        "fall_back_url", kEmptyStr);
    m_FallBackDelay = config.GetInt(cgi2rcgi_section,
        "error_fall_back_delay", -1, IRegistry::eReturn);

    m_CancelGoBackDelay = config.GetInt(cgi2rcgi_section,
        "cancel_fall_back_delay", 0, IRegistry::eReturn);

    if (m_FallBackUrl.empty()) {
        m_FallBackDelay = -1;
        m_CancelGoBackDelay = -1;
    }

    m_AffinitySource = 0;
    m_AffinitySetLimit = 0;
    m_AffinityName = config.GetString(cgi2rcgi_section,
        "affinity_name", kEmptyStr);

    if (!m_AffinityName.empty()) {
        vector<string> affinity_methods;
        NStr::Split(config.GetString(cgi2rcgi_section,
            "affinity_source", "GET"), ", ;&|", affinity_methods,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        for (vector<string>::const_iterator it = affinity_methods.begin();
                it != affinity_methods.end(); ++it) {
            if (*it == "GET")
                m_AffinitySource |= eUseQueryString;
            else if (*it == "POST")
                m_AffinitySource |= eUseRequestContent;
            else {
                NCBI_THROW_FMT(CArgException, eConstraint,
                    "Invalid affinity_source value '" << *it << '\'');
            }
        }
        m_AffinitySetLimit = config.GetInt(cgi2rcgi_section,
            "narrow_affinity_set_to", 0);
    }

    // Disregard the case of CGI arguments
    CCgiRequest::TFlags flags = CCgiRequest::fCaseInsensitiveArgs;

    if (config.GetBool(cgi2rcgi_section, "donot_parse_content",
            true, IRegistry::eReturn) &&
                !(m_AffinitySource & eUseRequestContent))
        flags |= CCgiRequest::fDoNotParseContent;

    SetRequestFlags(flags);

    m_DateFormat = config.GetString(cgi2rcgi_section,
        "date_format", "D B Y, h:m:s");

    m_ElapsedTimeFormat = config.GetString(cgi2rcgi_section,
        "elapsed_time_format", "S");

    m_InterceptJQueryCallback = config.GetBool("CGI", "CORS_JQuery_Callback_Enable",
        false, IRegistry::eReturn);

    m_AddJobIdToHeader = config.GetBool(cgi2rcgi_section, "add_job_id_to_response",
        false, IRegistry::eReturn);

    m_DisplayDonePage = config.GetValue(cgi2rcgi_section, "display_done_page", false);
    m_PortAdded = false;
}

CCgiContext* CCgi2RCgiApp::CreateContextWithFlags(CNcbiArguments* args,
    CNcbiEnvironment* env, CNcbiIstream* inp, CNcbiOstream* out,
        int ifd, int ofd, int flags)
{
    if (flags & CCgiRequest::fDoNotParseContent)
        return CCgiApplicationCached::CreateContextWithFlags(args, env,
            inp, out, ifd, ofd, flags);

    if (m_AffinitySource & eUseRequestContent)
        return CCgiApplicationCached::CreateContextWithFlags(args, env,
            inp, out, ifd, ofd, flags | CCgiRequest::fSaveRequestContent);

    // The 'env' argument is only valid in FastCGI mode.
    if (env == NULL)
        env = &SetEnvironment();

    size_t content_length = 0;

    try {
        content_length = (size_t) NStr::StringToUInt(
            env->Get(CCgiRequest::GetPropertyName(eCgi_ContentLength)));
    }
    catch (...) {
    }

    // Based on the CONTENT_LENGTH CGI parameter, decide whether to parse
    // the POST request in search of the job_key parameter.
    return CCgiApplicationCached::CreateContextWithFlags(args, env, inp,
        out, ifd, ofd, flags | (content_length > 0 &&
            content_length < 128 ? CCgiRequest::fSaveRequestContent :
                CCgiRequest::fDoNotParseContent));
}

static const string kGridCgiForm =
    "<FORM METHOD=\"GET\" ACTION=\"<@SELF_URL@>\">\n"
    "<@HIDDEN_FIELDS@>\n<@STAT_VIEW@>\n"
    "</FORM>";

static const string kPlainTextView = "<@STAT_VIEW@>";

class CRegexpTemplateFilter : public CHTMLPage::TTemplateLibFilter
{
public:
    CRegexpTemplateFilter(CHTMLPage* page) : m_Page(page) {}

    virtual bool TestAttribute(const string& attr_name,
        const string& test_pattern);

private:
    CHTMLPage* m_Page;
};

bool CRegexpTemplateFilter::TestAttribute(const string& attr_name,
    const string& test_pattern)
{
    CNCBINode* node = m_Page->MapTag(attr_name);

    if (node == NULL)
        return false;

    stringstream node_stream;

    node->Print(node_stream, CNCBINode::ePlainText);

    CRegexp regexp(test_pattern, CRegexp::fCompile_ignore_case);

    return regexp.IsMatch(node_stream.str());
}

#define CALLBACK_PARAM "callback="

static void s_RemoveCallbackParameter(string* query_string)
{
    SIZE_TYPE callback_pos = NStr::Find(*query_string, CALLBACK_PARAM);

    if (callback_pos == NPOS)
        return;

    // See if 'callback' is the last parameter in the query string.
    const char* callback_end = strchr(query_string->c_str() +
            callback_pos + sizeof(CALLBACK_PARAM) - 1, '&');
    if (callback_end != NULL)
        query_string->erase(callback_pos,
                callback_end - query_string->data() - callback_pos + 1);
    else if (callback_pos == 0)
        query_string->clear();
    else if (query_string->at(callback_pos - 1) == '&')
        query_string->erase(callback_pos - 1);
}

int CCgi2RCgiApp::ProcessRequest(CCgiContext& ctx)
{
    CNcbiEnvironment& env = SetEnvironment();

    // Add server port to client node name.
    if (!m_PortAdded) {
        m_PortAdded = true;
        const string port(env.Get(CCgiRequest::GetPropertyName(eCgi_ServerPort)));
        m_NetScheduleAPI.AddToClientNode(port);
    }

    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    CCgiRequest& request = ctx.GetRequest();
    m_Response = &ctx.GetResponse();
    m_Response->RequireWriteHeader(false);

    if (m_TargetEncodeMode != CHTMLPlainText::eHTMLEncode)
        m_Response->SetContentType(m_ContentType);

    // Create an HTML page (using the template HTML file)
    try {
        m_Page.reset(new CHTMLPage(m_Title, m_HtmlTemplate));
        CHTMLText* stat_view = new CHTMLText(!m_HTMLPassThrough ?
            kGridCgiForm : kPlainTextView);
        m_Page->AddTagMap("VIEW", stat_view);
    }
    catch (exception& e) {
        ERR_POST("Failed to create " << m_Title << " HTML page: " << e.what());
        return 2;
    }
    m_CustomHTTPHeader.reset(new CHTMLPage);
    m_CustomHTTPHeader->SetTemplateString("<@CUSTOM_HTTP_HEADER@>");
    CGridCgiContext grid_ctx(*m_Page, *m_CustomHTTPHeader, ctx);

    string listen_jobs;
    string timeout;
    grid_ctx.PullUpPersistentEntry("listen_jobs", listen_jobs);
    grid_ctx.PullUpPersistentEntry("timeout", timeout);

    grid_ctx.PullUpPersistentEntry("job_key", grid_ctx.GetJobKey());
    grid_ctx.PullUpPersistentEntry("Cancel");

    grid_ctx.LoadQueryStringTags(m_TargetEncodeMode);

    m_NetScheduleAPI.UpdateAuthString();

    try {
        if (m_InterceptJQueryCallback) {
            TCgiEntries& entries = request.GetEntries();
            TCgiEntries::iterator jquery_callback_it = entries.find("callback");
            if (jquery_callback_it != entries.end()) {
                grid_ctx.GetJqueryCallback() = jquery_callback_it->second;
                entries.erase(jquery_callback_it);
                string query_string_param(
                        CCgiRequest::GetPropertyName(eCgi_QueryString));
                string query_string = env.Get(query_string_param);
                if (!query_string.empty()) {
                    s_RemoveCallbackParameter(&query_string);
                    env.Set(query_string_param, query_string);
                }
            }
        }

        grid_ctx.PullUpPersistentEntry(kSinceTime);

        try {
            if (!listen_jobs.empty()) {
                ListenJobs(listen_jobs, timeout);
                grid_ctx.NeedRenderPage(false);
            } else
            if (!grid_ctx.GetJobKey().empty()) {
                CheckJob(grid_ctx);
            } else {
                SubmitJob(request, grid_ctx);
            }
        } // try
        catch (exception& ex) {
            ERR_POST("Job's reported as failed: " << ex.what());
            OnJobFailed(ex.what(), grid_ctx);
        }

        if (grid_ctx.NeedRenderPage()) PopulatePage(grid_ctx);
    } //try
    catch (exception& e) {
        ERR_POST("Failed to populate " << m_Title <<
            " HTML page: " << e.what());
        return 3;
    }

    return grid_ctx.NeedRenderPage() ? RenderPage() : 0;
}

inline bool s_IsPendingOrRunning(CNetScheduleAPI::EJobStatus job_status)
{
    switch (job_status) {
    case CNetScheduleAPI::ePending:
    case CNetScheduleAPI::eRunning:
        return true;

    default:
        return false;
    }
}

struct SJob : CNetScheduleJob
{
    CNetScheduleAPI::EJobStatus status = CNetScheduleAPI::ePending;
    bool progress_msg_truncated = false;

    SJob(const string& id) { job_id = id; }
};

struct SJobs : unordered_map<string, SJob>
{
    friend CNcbiOstream& operator<<(CNcbiOstream& out, SJobs jobs);
};

void CCgi2RCgiApp::ListenJobs(const string& job_ids_value, const string& timeout_value)
{
    CTimeout timeout(CTimeout::eZero);

    try {
        timeout.Set(NStr::StringToDouble(timeout_value));
    }
    catch (...) {
    }

    CDeadline deadline(timeout);

    vector<string> job_ids;
    NStr::Split(job_ids_value, ",", job_ids);

    if (job_ids.empty()) return;

    SJobs jobs;

    for (const auto& job_id : job_ids) {
        jobs.emplace(job_id, job_id);
    }


    // Request notifications unless there is a job that is already not pending/running

    CNetScheduleSubmitter submitter = m_GridClient->GetNetScheduleSubmitter();
    CNetScheduleNotificationHandler handler;

    bool wait_notifications = true;

    for (auto&& j : jobs) {
        const auto& job_id = j.first;
        auto& job = j.second;

        wait_notifications = wait_notifications && !deadline.IsExpired();

        try {
            if (wait_notifications) {
                tie(job.status, ignore, job.progress_msg) =
                    handler.RequestJobWatching(m_NetScheduleAPI, job_id, deadline);
            } else {
                job.status = m_NetScheduleAPI.GetJobDetails(job);
            }
        } catch (CNetScheduleException& ex) {
            if (ex.GetErrCode() != CNetScheduleException::eJobNotFound) throw;
            job.status = CNetScheduleAPI::eJobNotFound;
        }

        wait_notifications = wait_notifications && s_IsPendingOrRunning(job.status);
    }


    // If all jobs are still pending/running, wait for a notification

    if (wait_notifications) {
        while (handler.WaitForNotification(deadline)) {
            SNetScheduleOutputParser parser(handler.GetMessage());

            auto it = jobs.find(parser("job_key"));

            // If it's one of requested jobs
            if (it != jobs.end()) {
                auto& job = it->second;
                job.status = CNetScheduleAPI::StringToStatus(parser("job_status"));
                job.progress_msg = parser("msg");
                job.progress_msg_truncated = !parser("msg_truncated").empty();

                if (!s_IsPendingOrRunning(job.status)) break;
            }
        }

        // Recheck still pending/running jobs, just in case
        for (auto&& j : jobs) {
            auto& job = j.second;

            if (s_IsPendingOrRunning(job.status)) {
                job.progress_msg_truncated = false;

                try {
                    job.status = m_NetScheduleAPI.GetJobDetails(job);
                } catch (CNetScheduleException& ex) {
                    if (ex.GetErrCode() != CNetScheduleException::eJobNotFound) throw;
                    job.status = CNetScheduleAPI::eJobNotFound;
                    job.progress_msg.clear();
                }
            }
        }
    }


    // Output jobs and their current states

    CNcbiOstream& out = m_Response->out();

    try {
        out << jobs;
    }
    catch (exception& e) {
        if (out) throw;

        ERR_POST(Warning << "Failed to write jobs and their states to client: " << e.what());
    }
}

CNcbiOstream& operator<<(CNcbiOstream& out, SJobs jobs)
{
    char delimiter = '{';
    out << "Content-type: application/json\nStatus: 200 OK\n\n";

    for (const auto& j : jobs) {
        const auto& job_id = j.first;
        const auto& job = j.second;

        const auto status = CNetScheduleAPI::StatusToString(job.status);
        const auto message = NStr::JsonEncode(job.progress_msg, NStr::eJsonEnc_Quoted);
        out << delimiter << "\n  \"" << job_id << "\":\n  {\n    \"Status\": \"" << status << "\"";

        if (!job.progress_msg.empty()) {
            out << ",\n    \"Message\": " << message;
            if (job.progress_msg_truncated) out << ",\n    \"Truncated\": true";
        }

        out << "\n  }";
        delimiter = ',';
    }

    out << "\n}" << endl;
    return out;
}

void CCgi2RCgiApp::CheckJob(CGridCgiContext& grid_ctx)
{
    bool done = true;

    GetDiagContext().Extra().Print("ctg_poll", "true");
    m_GridClient->SetJobKey(grid_ctx.GetJobKey());

    CNetScheduleAPI::EJobStatus status = CNetScheduleAPI::eJobNotFound;

    if (m_RefreshWait) {
        CDeadline wait_deadline(m_RefreshWait);

        status =
            CNetScheduleNotificationHandler().WaitForJobEvent(
                    grid_ctx.GetJobKey(),
                    wait_deadline,
                    m_NetScheduleAPI,
                    ~(CNetScheduleNotificationHandler::fJSM_Pending |
                    CNetScheduleNotificationHandler::fJSM_Running));
    }

    if (!grid_ctx.HasCtgTime()) {
        status = GetStatusAndCtgTime(grid_ctx);

    } else if (!m_RefreshWait) {
        status = GetStatus(grid_ctx);
    }

    done = CheckIfJobDone(grid_ctx, status);

    if (done)
        grid_ctx.Clear();
    else {
        // Check if job cancellation has been requested
        // via the user interface(HTML).
        if (GetArgs()["Cancel"] ||
                !grid_ctx.GetPersistentEntryValue("Cancel").empty())
            m_GridClient->CancelJob(grid_ctx.GetJobKey());

        DefineRefreshTags(grid_ctx, grid_ctx.GetSelfURL(), m_RefreshDelay);
    }
}

void CCgi2RCgiApp::SubmitJob(CCgiRequest& request,
        CGridCgiContext& grid_ctx)
{
    bool done = true;

    if (!m_AffinityName.empty()) {
        string affinity;
        grid_ctx.PullUpPersistentEntry(m_AffinityName, affinity, m_AffinitySource);
        if (!affinity.empty()) {
            if (m_AffinitySetLimit > 0) {
                CChecksum crc32(CChecksum::eCRC32);
                crc32.AddChars(affinity.data(), affinity.length());
                affinity = NStr::UIntToString(
                    crc32.GetChecksum() % m_AffinitySetLimit);
            }
            m_GridClient->SetJobAffinity(affinity);
        }
    }
    try {
        // The job is ready to be sent to the queue.
        // Prepare the input data.
        CNcbiOstream& os = m_GridClient->GetOStream();
        // Send the input data.
        request.Serialize(os);
        string saved_content(kEmptyStr);
        try {
            saved_content = request.GetContent();
        }
        catch (...) {
            // An exception is normal when the content
            // is not saved, disregard the exception.
        }
        if (!saved_content.empty())
            os.write(saved_content.data(), saved_content.length());

        grid_ctx.DefinePersistentEntry(kSinceTime,
            NStr::NumericToString(GetFastLocalTime().GetTimeT()));

        CNetScheduleAPI::EJobStatus status = m_GridClient->SubmitAndWait(m_FirstDelay);

        CNetScheduleJob& job(m_GridClient->GetJob());

        grid_ctx.GetJobKey() = job.job_id;

        grid_ctx.DefinePersistentEntry("job_key", grid_ctx.GetJobKey());
        GetDiagContext().Extra().Print("job_key", grid_ctx.GetJobKey());

        done = !s_IsPendingOrRunning(status) && CheckIfJobDone(grid_ctx, status);

        if (!done) {
            // The job has just been submitted.
            // Render a report page
            grid_ctx.SelectView("JOB_SUBMITTED");
            DefineRefreshTags(grid_ctx, grid_ctx.GetSelfURL(),
                m_RefreshDelay);
        }
    }
    catch (CNetScheduleException& ex) {
        ERR_POST("Failed to submit a job: " << ex.what());
        OnJobFailed(ex.GetErrCode() ==
                CNetScheduleException::eTooManyPendingJobs ?
            "NetSchedule Queue is busy" : ex.what(), grid_ctx);
        done = true;
    }
    catch (exception& ex) {
        ERR_POST("Failed to submit a job: " << ex.what());
        OnJobFailed(ex.what(), grid_ctx);
        done = true;
    }

    if (done)
        grid_ctx.Clear();
}

void CCgi2RCgiApp::PopulatePage(CGridCgiContext& grid_ctx)
{
    grid_ctx.AddTagMap("SELF_URL", grid_ctx.GetSelfURL(), CHTMLPlainText::eNoEncode);

    if (!m_HTMLPassThrough) {
        // Preserve persistent entries as hidden fields
        string hidden_fields;
        for (CGridCgiContext::TPersistentEntries::const_iterator it =
                    grid_ctx.GetPersistentEntries().begin();
                it != grid_ctx.GetPersistentEntries().end(); ++it)
            hidden_fields += "<INPUT TYPE=\"HIDDEN\" NAME=\"" + it->first
                    + "\" VALUE=\"" + NStr::HtmlEncode(it->second) + "\">\n";
        m_Page->AddTagMap("HIDDEN_FIELDS",
            new CHTMLPlainText(hidden_fields, true));
    }

    CTime now(GetFastLocalTime());
    m_Page->AddTagMap("DATE",
        new CHTMLText(now.AsString(m_DateFormat)));
    string since_time = grid_ctx.GetPersistentEntryValue(kSinceTime);
    if (!since_time.empty()) {
        grid_ctx.AddTagMap("SINCE_TIME", since_time);
        time_t tt = NStr::StringToInt(since_time);
        CTime start;
        start.SetTimeT(tt);
        m_Page->AddTagMap("SINCE",
                        new CHTMLText(start.AsString(m_DateFormat)));
        CTimeSpan ts = now - start;
        m_Page->AddTagMap("ELAPSED_TIME_MSG_HERE",
                        new CHTMLText("<@ELAPSED_TIME_MSG@>"));
        m_Page->AddTagMap("ELAPSED_TIME",
                        new CHTMLText(ts.AsString(m_ElapsedTimeFormat)));
    }
    grid_ctx.AddTagMap("JOB_ID", grid_ctx.GetJobKey());
    if (m_AddJobIdToHeader) {
        m_Response->SetHeaderValue(HTTP_NCBI_JSID, grid_ctx.GetJobKey());
    }
    string progress_message;
    try {
        progress_message = m_GridClient->GetProgressMessage();
    }
    catch (CException& e) {
        ERR_POST("Could not retrieve progress message for " <<
                grid_ctx.GetJobKey() << ": " << e);
    }
    grid_ctx.SetJobProgressMessage(progress_message);
    grid_ctx.GetHTMLPage().AddTagMap("PROGERSS_MSG",
            new CHTMLPlainText(m_TargetEncodeMode, progress_message));
    grid_ctx.GetHTMLPage().AddTagMap("PROGRESS_MSG",
            new CHTMLPlainText(m_TargetEncodeMode, progress_message));
}

int CCgi2RCgiApp::RenderPage()
{
    CNcbiOstream& out = m_Response->out();

    // Compose and flush the resultant HTML page
    try {
        CRegexpTemplateFilter page_filter(m_Page.get());
        CRegexpTemplateFilter header_filter(m_CustomHTTPHeader.get());

        vector<string>::const_iterator it;
        for (it = m_HtmlIncs.begin(); it != m_HtmlIncs.end(); ++it) {
            string lib = NStr::TruncateSpaces(*it);
            m_Page->LoadTemplateLibFile(lib, &page_filter);
            m_CustomHTTPHeader->LoadTemplateLibFile(lib, &header_filter);
        }

        stringstream header_stream;
        m_CustomHTTPHeader->Print(header_stream, CNCBINode::ePlainText);

        string header_line;
        string status_line;

        enum {
            eNoStatusLine,
            eReadingStatusLine,
            eGotStatusLine
        } status_line_status = eNoStatusLine;

        while (header_stream.good()) {
            getline(header_stream, header_line);
            if (header_line.empty())
                continue;
            if (status_line_status == eReadingStatusLine) {
                if (isspace(header_line[0])) {
                    status_line += header_line;
                    continue;
                }
                status_line_status = eGotStatusLine;
            }
            if (NStr::StartsWith(header_line, "Status:", NStr::eNocase)) {
                status_line_status = eReadingStatusLine;
                status_line = header_line;
                continue;
            }
            out << header_line << "\r\n";
        }
        if (status_line_status != eNoStatusLine) {
            CTempString status_code_and_reason(
                    status_line.data() + (sizeof("Status:") - 1),
                    status_line.size() - (sizeof("Status:") - 1));
            NStr::TruncateSpacesInPlace(status_code_and_reason);
            CTempString status_code, reason;
            NStr::SplitInTwo(status_code_and_reason, CTempString(" \t", 2),
                    status_code, reason,
                    NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
            m_Response->SetStatus(NStr::StringToUInt(status_code), reason);
        }
        m_Response->WriteHeader();
        m_Page->Print(out, CNCBINode::eHTML);
    }
    catch (exception& e) {
        if (!out) {
            ERR_POST(Warning << "Failed to write " << m_Title << " HTML page to client: " << e.what());
            return 0;
        }

        ERR_POST("Failed to compose/send " << m_Title <<
            " HTML page: " << e.what());
        return 4;
    }

    return 0;
}

void CCgi2RCgiApp::DefineRefreshTags(CGridCgiContext& grid_ctx,
        const string& url, int idelay)
{
    const auto idelay_str = NStr::IntToString(idelay);

    if (!m_HTMLPassThrough && idelay >= 0 && grid_ctx.NeedMetaRefresh()) {
        CHTMLText* redirect = new CHTMLText(
                    "<META HTTP-EQUIV=Refresh "
                    "CONTENT=\"<@REDIRECT_DELAY@>; URL=<@REDIRECT_URL@>\">");
        m_Page->AddTagMap("REDIRECT", redirect);

        CHTMLPlainText* delay = new CHTMLPlainText(idelay_str);
        m_Page->AddTagMap("REDIRECT_DELAY", delay);
    }

    grid_ctx.AddTagMap("REDIRECT_URL", url, CHTMLPlainText::eNoEncode);
    m_Response->SetHeaderValue("Expires", "0");
    m_Response->SetHeaderValue("Pragma", "no-cache");
    m_Response->SetHeaderValue("Cache-Control",
        "no-cache, no-store, max-age=0, private, must-revalidate");

    if (idelay >= 0) {
        m_Response->SetHeaderValue("NCBI-RCGI-RetryURL", url);

        // Must correspond to SRCgiWait values
        m_Response->SetHeaderValue(CHttpRetryContext::kHeader_Url, url);
        m_Response->SetHeaderValue(CHttpRetryContext::kHeader_Delay, idelay_str);
    }
}


CNetScheduleAPI::EJobStatus CCgi2RCgiApp::GetStatus(
        CGridCgiContext& grid_ctx)
{
    CNetScheduleAPI::EJobStatus status;
    try {
        status = m_GridClient->GetStatus();
    }
    catch (CNetSrvConnException& e) {
        ERR_POST("Failed to retrieve job status for " <<
                grid_ctx.GetJobKey() << ": " << e);

        CNetService service(m_NetScheduleAPI.GetService());

        CNetScheduleKey key(grid_ctx.GetJobKey(), m_NetScheduleAPI.GetCompoundIDPool());

        CNetServer bad_server(service.GetServer(key.host, key.port));

        // Skip to the next available server in the service.
        // If the server that caused a connection exception
        // was the only server in the service, rethrow the
        // exception.
        CNetServiceIterator it(service.ExcludeServer(bad_server));

        if (!it)
            throw;

        CNetScheduleAdmin::TQueueInfo queue_info;

        m_NetScheduleAPI.GetAdmin().GetQueueInfo(it.GetServer(), queue_info);

        if ((Uint8) GetFastLocalTime().GetTimeT() > NStr::StringToUInt8(
                grid_ctx.GetPersistentEntryValue(kSinceTime)) +
                NStr::StringToUInt(queue_info["timeout"]))
            status = CNetScheduleAPI::eJobNotFound;
        else {
            status = CNetScheduleAPI::eRunning;
            grid_ctx.GetHTMLPage().AddTagMap("MSG",
                    new CHTMLPlainText(m_TargetEncodeMode,
                            "Failed to retrieve job status: " + e.GetMsg()));
        }
    }

    return status;
}

void s_GetCtgTime(CGridCgiContext& grid_ctx, string event)
{
    const CTempString kTimestamp = "timestamp";
    const string kFormat = "M/D/Y h:m:G";

    CAttrListParser parser;
    parser.Reset(event);
    CTempString name;
    string value;
    size_t column;

    do {
        if (parser.NextAttribute(&name, &value, &column) == CAttrListParser::eNoMoreAttributes) return;
    } while (name != kTimestamp);

    grid_ctx.DefinePersistentEntry(kSinceTime, NStr::NumericToString(CTime(value, kFormat).GetTimeT()));
}

CNetScheduleAPI::EJobStatus CCgi2RCgiApp::GetStatusAndCtgTime(CGridCgiContext& grid_ctx)
{
    const string kStatus = "status: ";
    const string kEvent1 = "event1: ";

    auto rv = CNetScheduleAPI::eJobNotFound;
    auto output = m_NetScheduleAPI.GetAdmin().DumpJob(grid_ctx.GetJobKey());
    string line;

    while (output.ReadLine(line)) {
        if (NStr::StartsWith(line, kStatus)) {
            rv = CNetScheduleAPI::StringToStatus(line.substr(kStatus.size()));

        } else if (NStr::StartsWith(line, kEvent1)) {
            s_GetCtgTime(grid_ctx, line.substr(kEvent1.size()));
        }
    }

    return rv;
}

bool CCgi2RCgiApp::CheckIfJobDone(
        CGridCgiContext& grid_ctx, CNetScheduleAPI::EJobStatus status)
{
    bool done = true;
    const string status_str = CNetScheduleAPI::StatusToString(status);
    m_Response->SetHeaderValue("NCBI-RCGI-JobStatus",
            status_str);
    grid_ctx.GetHTMLPage().AddTagMap("JOB_STATUS",
            new CHTMLPlainText(status_str, true));

    switch (status) {
    case CNetScheduleAPI::eDone:
        // The worker node has finished the job and the
        // result is ready to be retrieved.
        OnJobDone(grid_ctx);
        break;

    case CNetScheduleAPI::eFailed:
        // a job has failed
        OnJobFailed(m_GridClient->GetErrorMessage(), grid_ctx);
        break;

    case CNetScheduleAPI::eCanceled:
        // The job has been canceled
        grid_ctx.DefinePersistentEntry(kSinceTime, kEmptyStr);
        // Render a job cancellation page
        grid_ctx.SelectView("JOB_CANCELED");

        DefineRefreshTags(grid_ctx, m_FallBackUrl.empty() ?
            grid_ctx.GetCGIContext().GetSelfURL() : m_FallBackUrl,
                m_CancelGoBackDelay);
        break;

    case CNetScheduleAPI::eJobNotFound:
        // The job has expired
        OnJobFailed("Job is not found.", grid_ctx);
        break;

    case CNetScheduleAPI::ePending:
        // The job is in the NetSchedule queue and
        // is waiting for a worker node.
        // Render a status report page
        grid_ctx.SelectView("JOB_PENDING");
        done = false;
        break;

    case CNetScheduleAPI::eRunning:
        // The job is being processed by a worker node
        // Render a status report page
        grid_ctx.SelectView("JOB_RUNNING");
        done = false;
        break;

    default:
        LOG_POST(Note << "Unexpected job state");
    }
    SetRequestId(grid_ctx.GetJobKey(), status == CNetScheduleAPI::eDone);
    return done;
}

void CCgi2RCgiApp::ReadJob(istream& is, CGridCgiContext& ctx)
{
    CNcbiOstream& out = m_Response->out();

    string err_msg;

    try {
        bool no_jquery = ctx.GetJqueryCallback().empty();

        // No need to amend anything
        if (no_jquery && !m_AddJobIdToHeader) {
            NcbiStreamCopy(out, is);
            ctx.NeedRenderPage(false);
            return;
        }

        // Amending HTTP header
        string header_line;
        while (getline(is, header_line)) {
            NStr::TruncateSpacesInPlace(header_line, NStr::eTrunc_End);
            if (header_line.empty())
                break;

            if (no_jquery)
                out << header_line << "\r\n";
            else if (NStr::StartsWith(header_line, "Content-Type", NStr::eNocase))
                out << "Content-Type: text/javascript\r\n";
            else if (!NStr::StartsWith(header_line, "Content-Length", NStr::eNocase))
                out << header_line << "\r\n";
        }

        if (m_AddJobIdToHeader) {
            out << HTTP_NCBI_JSID << ": " << ctx.GetJobKey() << "\r\n";
        }

        out << "\r\n";

        if (no_jquery) {
            NcbiStreamCopy(out, is);
        } else {
            out << ctx.GetJqueryCallback() << '(';
            NcbiStreamCopy(out, is);
            out << ')';
        }
        ctx.NeedRenderPage(false);
        return;
    }
    catch (CException& ex) {
        err_msg = ex.ReportAll();
    }
    catch (exception& ex) {
        err_msg = ex.what();
    }

    if (!is) {
        ERR_POST("Failed to read job output: " << err_msg);
        OnJobFailed("Failed to read job output: " + err_msg, ctx);
    } else if (!out) {
        ERR_POST(Warning << "Failed to write job output to client: " << err_msg);
        ctx.NeedRenderPage(false); // Client will not get the message anyway
    } else {
        ERR_POST("Failed while relaying job output: " << err_msg);
        OnJobFailed("Failed while relaying job output: " + err_msg, ctx);
    }
}

void CCgi2RCgiApp::OnJobDone(CGridCgiContext& ctx)
{
    if (m_DisplayDonePage) {
        string get_results;
        ctx.PullUpPersistentEntry("get_results", get_results);

        if (get_results.empty()) {
            ctx.SelectView("JOB_DONE");
            DefineRefreshTags(ctx, ctx.GetSelfURL() + "&get_results=true", m_RefreshDelay);
            return;
        }
    }

    CNcbiIstream& is = m_GridClient->GetIStream();

    // This must be after m_GridClient->GetIStream(), otherwise size would be empty
    if (m_GridClient->GetBlobSize() > 0) {
        ReadJob(is, ctx);
    } else {
        const char* str_page;

        switch (m_TargetEncodeMode) {
        case CHTMLPlainText::eHTMLEncode:
            str_page = "<html><head><title>Empty Result</title>"
                "</head><body>Empty Result</body></html>";
            break;
        case CHTMLPlainText::eJSONEncode:
            str_page = "{}";
            break;
        default:
            str_page = "";
        }

        ctx.GetHTMLPage().SetTemplateString(str_page);
    }
}

void CCgi2RCgiApp::OnJobFailed(const string& msg,
                                  CGridCgiContext& ctx)
{
    ctx.DefinePersistentEntry(kSinceTime, kEmptyStr);
    // Render a error page
    ctx.SelectView("JOB_FAILED");

    string fall_back_url = m_FallBackUrl.empty() ?
        ctx.GetCGIContext().GetSelfURL() : m_FallBackUrl;
    DefineRefreshTags(ctx, fall_back_url, m_FallBackDelay);

    ctx.GetHTMLPage().AddTagMap("MSG",
            new CHTMLPlainText(m_TargetEncodeMode, msg));
}

/////////////////////////////////////////////////////////////////////////////
int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();

    GetDiagContext().SetOldPostFormat(false);
    CCgi2RCgiApp app;
    return app.AppMain(argc, argv);
}
