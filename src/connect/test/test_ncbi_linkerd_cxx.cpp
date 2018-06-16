/* $Id$
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
 * Authors:  David McElhany
 *
 * File Description:
 *   Test LINKERD via C++ classes
 *
 */

#include <ncbi_pch.hpp>     // This header must go first

#include <connect/ncbi_http_session.hpp>
#include <connect/ncbi_socket.h>
#include <corelib/ncbi_url.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbidiag.hpp>

#include <stdlib.h>

#include "../parson.h"

#include "test_assert.h"    // This header must go last


#ifdef _MSC_VER
#define unsetenv(n)     _putenv_s(n,"")
#define setenv(n,v,w)   _putenv_s(n,v)
#endif


USING_NCBI_SCOPE;


struct SServicePath
{
    string  service;
    string  path;
};


struct SSchemeServicePath
{
    string  scheme;
    string  service;
    string  path;
};


struct SMapper
{
    void Configure(CNcbiRegistry& config);

    string                  name;
    bool                    enabled;
    map<string, string>     env_vars;
};


class CTestNcbiLinkerdCxxApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);

private:
    void ReadData(void);

    void SetupRequest(CHttpRequest& request);
    int  PrintResponse(const CHttpSession* session, CHttpResponse& resp);

    void TestCaseStart(
        const string& test_case,
        const string* url = 0,
        const CUrl*   curl = 0,
        const string* scheme = 0,
        const string* service = 0,
        const string* path = 0);
    void TestCaseEnd(
        const string& test_case,
        int           result,
        const string* url = 0,
        const CUrl*   curl = 0,
        const string* scheme = 0,
        const string* service = 0,
        const string* path = 0);

    int TestGet_CUrl_Service(const string& service);
    int TestGet_CUrl_ServicePath(const string& service, const string& path);
    int TestGet_CUrl_SchemeServicePath(const string& scheme, const string& service, const string& path);
    int TestGet_Http(const string& url);
    int TestGet_HttpStream(const string& url);
    int TestGet_NewRequest(const CUrl& curl, bool nested = false);

    int TestPost_CUrl_Service(const string& service);
    int TestPost_CUrl_ServicePath(const string& service, const string& path);
    int TestPost_CUrl_SchemeServicePath(const string& scheme, const string& service, const string& path);
    int TestPost_Http(const string& url);
    int TestPost_HttpStream(const string& url);
    int TestPost_NewRequest(const CUrl& curl, bool nested = false);

    int RunTests(SMapper& mapper);

private:
    CTimeout            m_Timeout;
    unsigned short      m_Retries;
    char                m_Hostname[300];

    list<SMapper>       m_Mappers;
    string              m_MapperName;
    string              m_PostData;

    list<string>                m_GetCurlService;
    list<SServicePath>          m_GetCurlServicePath;
    list<SSchemeServicePath>    m_GetCurlSchemeServicePath;
    list<string>                m_GetHttp;
    list<string>                m_GetHttpStream;
    list<string>                m_GetNewRequest;

    list<string>                m_PostCurlService;
    list<SServicePath>          m_PostCurlServicePath;
    list<SSchemeServicePath>    m_PostCurlSchemeServicePath;
    list<string>                m_PostHttp;
    list<string>                m_PostHttpStream;
    list<string>                m_PostNewRequest;
};


void SMapper::Configure(CNcbiRegistry& config)
{
    for (auto env : this->env_vars) {
        setenv(env.first.c_str(), env.second.c_str(), 1);
    }
}


void CTestNcbiLinkerdCxxApp::ReadData(void)
{
    // Get config params as passed in.
    m_Timeout.Set(GetArgs()["timeout"].AsDouble());
    m_Retries = static_cast<unsigned short>(GetArgs()["retries"].AsInteger());

    ERR_POST(Info << "Timeout:  " << m_Timeout.GetAsDouble());
    ERR_POST(Info << "Retries:  " << m_Retries);

    x_JSON_Value    *root_value = NULL;
    x_JSON_Object   *root_obj;
    x_JSON_Object   *obj;
    x_JSON_Array    *arr;
    size_t          num;
    const char      *url;

    string test_file(GetArgs()["test_file"].AsString());
    ERR_POST(Info << "Test file: " << test_file);

    // Overall file stuff
    root_value = x_json_parse_file(test_file.c_str());
    if ( ! root_value) {
        NCBI_USER_THROW(string("Cannot open test file '") + test_file + "'.");
    }
    root_obj = x_json_value_get_object(root_value);
    if ( ! root_obj) {
        NCBI_USER_THROW(string("Cannot read test file '") + test_file + "'.");
    }

    // Mapper specs
    obj = x_json_object_get_object(root_obj, "mappers");
    size_t n_mappers = x_json_object_get_count(obj);
    for (size_t map_idx = 0; map_idx < n_mappers; ++map_idx) {
        SMapper                 mapper;
        map<string, string>     env_vars;

        const char *map_name = x_json_object_get_name(obj, map_idx);
        mapper.name = map_name;

        x_JSON_Object *map_obj = x_json_object_get_object(obj, map_name);
        mapper.enabled = (x_json_object_get_boolean(map_obj, "enabled") == 1);

        x_JSON_Object *envs_obj = x_json_object_get_object(map_obj, "env_set");
        size_t n_envs = x_json_object_get_count(envs_obj);
        for (size_t env_idx = 0; env_idx < n_envs; ++env_idx) {
            const char *env_name = x_json_object_get_name(envs_obj, env_idx);
            const char *env_val = x_json_object_get_string(envs_obj, env_name);
            mapper.env_vars[env_name] = env_val;
        }

        m_Mappers.push_back(mapper);
    }

    // GET specs
    obj = x_json_object_get_object(root_obj, "get");
    arr = x_json_object_get_array(obj, "curl_service");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        const char *svc = x_json_array_get_string(arr, idx);
        m_GetCurlService.push_back(svc);
    }
    arr = x_json_object_get_array(obj, "curl_service_path");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        SServicePath    svc_path;
        x_JSON_Object *svc_path_obj = x_json_array_get_object(arr, idx);
        const char *svc = x_json_object_get_string(svc_path_obj, "service");
        const char *path = x_json_object_get_string(svc_path_obj, "path");
        svc_path.service = svc;
        svc_path.path    = path;
        m_GetCurlServicePath.push_back(svc_path);
    }
    arr = x_json_object_get_array(obj, "curl_scheme_service_path");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        SSchemeServicePath  sch_svc_path;
        x_JSON_Object *sch_svc_path_obj = x_json_array_get_object(arr, idx);
        const char *sch = x_json_object_get_string(sch_svc_path_obj, "scheme");
        const char *svc = x_json_object_get_string(sch_svc_path_obj, "service");
        const char *path = x_json_object_get_string(sch_svc_path_obj, "path");
        sch_svc_path.scheme  = sch;
        sch_svc_path.service = svc;
        sch_svc_path.path    = path;
        m_GetCurlSchemeServicePath.push_back(sch_svc_path);
    }
    arr = x_json_object_get_array(obj, "http");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        url = x_json_array_get_string(arr, idx);
        m_GetHttp.push_back(url);
    }
    arr = x_json_object_get_array(obj, "http_stream");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        url = x_json_array_get_string(arr, idx);
        m_GetHttpStream.push_back(url);
    }
    arr = x_json_object_get_array(obj, "new_request");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        url = x_json_array_get_string(arr, idx);
        m_GetNewRequest.push_back(url);
    }

    // POST specs
    obj = x_json_object_get_object(root_obj, "post");
    const char *post_data = x_json_object_get_string(obj, "data");
    m_PostData = post_data;
    arr = x_json_object_get_array(obj, "curl_service");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        const char *svc = x_json_array_get_string(arr, idx);
        m_PostCurlService.push_back(svc);
    }
    arr = x_json_object_get_array(obj, "curl_service_path");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        SServicePath    svc_path;
        x_JSON_Object *svc_path_obj = x_json_array_get_object(arr, idx);
        const char *svc = x_json_object_get_string(svc_path_obj, "service");
        const char *path = x_json_object_get_string(svc_path_obj, "path");
        svc_path.service = svc;
        svc_path.path    = path;
        m_PostCurlServicePath.push_back(svc_path);
    }
    arr = x_json_object_get_array(obj, "curl_scheme_service_path");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        SSchemeServicePath  sch_svc_path;
        x_JSON_Object *sch_svc_path_obj = x_json_array_get_object(arr, idx);
        const char *sch = x_json_object_get_string(sch_svc_path_obj, "scheme");
        const char *svc = x_json_object_get_string(sch_svc_path_obj, "service");
        const char *path = x_json_object_get_string(sch_svc_path_obj, "path");
        sch_svc_path.scheme  = sch;
        sch_svc_path.service = svc;
        sch_svc_path.path    = path;
        m_PostCurlSchemeServicePath.push_back(sch_svc_path);
    }
    arr = x_json_object_get_array(obj, "http");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        url = x_json_array_get_string(arr, idx);
        m_PostHttp.push_back(url);
    }
    arr = x_json_object_get_array(obj, "http_stream");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        url = x_json_array_get_string(arr, idx);
        m_PostHttpStream.push_back(url);
    }
    arr = x_json_object_get_array(obj, "new_request");
    num = x_json_array_get_count(arr);
    for (size_t idx = 0; idx < num; ++idx) {
        url = x_json_array_get_string(arr, idx);
        m_PostNewRequest.push_back(url);
    }
}


void CTestNcbiLinkerdCxxApp::SetupRequest(CHttpRequest& request)
{
    if (m_Timeout.GetAsDouble() > 0.0) {
        request.SetTimeout(CTimeout(m_Timeout.GetAsDouble()));
    }
    request.SetRetries(m_Retries);
}


int CTestNcbiLinkerdCxxApp::PrintResponse(const CHttpSession* session, CHttpResponse& resp)
{
    ERR_POST(Info << "Status: " << resp.GetStatusCode() << " " << resp.GetStatusText());
    ERR_POST(Info << "--- Body ---");

    if (resp.CanGetContentStream()) {
        CNcbiIstream& in = resp.ContentStream();
        if ( in.good() ) {
            NcbiStreamCopy(cerr, in);
        }
        else {
            ERR_POST(Error << "Bad content stream.");
        }
    }
    else {
        CNcbiIstream& in = resp.ErrorStream();
        if (in.good()) {
            NcbiStreamCopy(cerr, in);
        }
        else {
            ERR_POST(Error << "Bad error stream.");
        }
    }
    return resp.GetStatusCode() == 200 ? 0 : 1;
}


void CTestNcbiLinkerdCxxApp::TestCaseStart(
    const string& test_case,
    const string* url,
    const CUrl*   curl,
    const string* scheme,
    const string* service,
    const string* path)
{
    cout << "\n\n" << string(80, '=') << endl;
    cout << "TestCaseStart  " << m_MapperName << "  " << test_case;
    if (url) {
        cout << "  " << *url << "  string";
    } else if (curl) {
        CUrl curl2(*curl);
        cout << "  " << curl2.ComposeUrl(CUrlArgs::eAmp_Char) << "  CUrl";
    } else {
        cout << "  _na_  _na_";
    }
    cout << "  " << (scheme ? *scheme : "_na_");
    cout << "  " << (service ? *service : "_na_");
    cout << "  " << (path ? *path : "_na_") << endl;
}


// Create a record that can be (a) easily found in the large log file, and (b)
// easily transformed into a CSV.  For example:
//      ./test_ncbi_linkerd_cxx >& testout.txt
//      cat testout.txt | grep -P '^nTestCaseEnd\t' | tr '\t' , > results.csv
void CTestNcbiLinkerdCxxApp::TestCaseEnd(
    const string& test_case,
    int           result,
    const string* url,
    const CUrl*   curl,
    const string* scheme,
    const string* service,
    const string* path)
{
    cout << "\nTestCaseEnd\t" << m_Hostname << "\t" << (result == 0 ? "PASS" : "FAIL");
    cout << "\t" << test_case << "\t";
    if (url) {
        cout << "string_url(" << *url << ")";
    } else if (curl) {
        CUrl curl2(*curl);
        cout << "CUrl=" << curl2.ComposeUrl(CUrlArgs::eAmp_Char);
    } else {
        CUrl curl;
        cout << "CUrl.Set(";
        if (scheme) {
            curl.SetScheme(*scheme);
            cout << "scheme=" << *scheme << ",";
        }
        curl.SetService(*service);
        cout << "service=" << *service;
        if (path) {
            curl.SetPath(*path);
            cout << ",path=" << *path;
        }
        cout << ")=" << curl.ComposeUrl(CUrlArgs::eAmp_Char);
    }
    cout << "\t" << m_MapperName;
    cout << "\n" << string(80, '-') << "\n" << endl;
}


int CTestNcbiLinkerdCxxApp::TestGet_CUrl_Service(const string& service)
{
    TestCaseStart("CUrl.Service.GET", 0, 0, 0, &service);
    CUrl curl;
    curl.SetService(service);
    int result = TestGet_NewRequest(curl, true);
    TestCaseEnd("CUrl.Service.GET", result, 0, 0, 0, &service);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestGet_CUrl_ServicePath(const string& service, const string& path)
{
    TestCaseStart("CUrl.Service.Path.GET", 0, 0, 0, &service, &path);
    CUrl curl;
    curl.SetService(service);
    curl.SetPath(path);
    int result = TestGet_NewRequest(curl, true);
    TestCaseEnd("CUrl.Service.Path.GET", result, 0, 0, 0, &service, &path);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestGet_CUrl_SchemeServicePath(const string& scheme, const string& service, const string& path)
{
    TestCaseStart("CUrl.Scheme.Service.Path.GET", 0, 0, &scheme, &service, &path);
    CUrl curl;
    curl.SetScheme(scheme);
    curl.SetService(service);
    curl.SetPath(path);
    int result = TestGet_NewRequest(curl, true);
    TestCaseEnd("CUrl.Scheme.Service.Path.GET", result, 0, 0, &scheme, &service, &path);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestGet_Http(const string& url)
{
    TestCaseStart("g_HttpGet", &url);
    CHttpResponse resp = g_HttpGet(url);
    int result = PrintResponse(0, resp);
    TestCaseEnd("g_HttpGet", result, &url);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestGet_HttpStream(const string& url)
{
    TestCaseStart("CConn_HttpStream.GET", &url);
    try {
        CConn_HttpStream httpstr(url);
        CConn_MemoryStream mem_str;
        NcbiStreamCopy(mem_str, httpstr);
        string resp;
        mem_str.ToString(&resp);
        cerr << resp;
    }
    catch (CException& ex) {
        ERR_POST(Error << "HttpStream exception: " << ex.what());
        TestCaseEnd("CConn_HttpStream.GET", 1, &url);
        return 1;
    }
    TestCaseEnd("CConn_HttpStream.GET", 0, &url);
    return 0;
}


int CTestNcbiLinkerdCxxApp::TestGet_NewRequest(const CUrl& curl, bool nested)
{
    if ( ! nested)
        TestCaseStart("CHttpSession::NewRequest.GET", 0, &curl);
    CHttpSession session;
    CHttpRequest req = session.NewRequest(curl);
    SetupRequest(req);
    CHttpResponse resp = req.Execute();
    int result = PrintResponse(&session, resp);
    if ( ! nested)
        TestCaseEnd("CHttpSession::NewRequest.GET", result, 0, &curl);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestPost_CUrl_Service(const string& service)
{
    TestCaseStart("CUrl.Service.POST", 0, 0, 0, &service);
    CUrl curl;
    curl.SetService(service);
    int result = TestPost_NewRequest(curl, true);
    TestCaseEnd("CUrl.Service.POST", result, 0, 0, 0, &service);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestPost_CUrl_ServicePath(const string& service, const string& path)
{
    TestCaseStart("CUrl.Service.Path.POST", 0, 0, 0, &service, &path);
    CUrl curl;
    curl.SetService(service);
    curl.SetPath(path);
    int result = TestPost_NewRequest(curl, true);
    TestCaseEnd("CUrl.Service.Path.POST", result, 0, 0, 0, &service, &path);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestPost_CUrl_SchemeServicePath(const string& scheme, const string& service, const string& path)
{
    TestCaseStart("CUrl.Scheme.Service.Path.POST", 0, 0, &scheme, &service, &path);
    CUrl curl;
    curl.SetScheme(scheme);
    curl.SetService(service);
    curl.SetPath(path);
    int result = TestPost_NewRequest(curl, true);
    TestCaseEnd("CUrl.Scheme.Service.Path.POST", result, 0, 0, &scheme, &service, &path);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestPost_Http(const string& url)
{
    TestCaseStart("g_HttpPost", &url);
    CHttpResponse resp = g_HttpPost(url, m_PostData);
    int result = PrintResponse(0, resp);
    TestCaseEnd("g_HttpPost", result, &url);
    return result;
}


int CTestNcbiLinkerdCxxApp::TestPost_HttpStream(const string& url)
{
    TestCaseStart("CConn_HttpStream.POST", &url);
    try {
        CConn_HttpStream httpstr(url, eReqMethod_Post);
        httpstr << m_PostData;
        CConn_MemoryStream mem_str;
        NcbiStreamCopy(mem_str, httpstr);
        string resp;
        mem_str.ToString(&resp);
        cerr << resp;
    }
    catch (CException& ex) {
        ERR_POST(Error << "HttpStream exception: " << ex.what());
        TestCaseEnd("CConn_HttpStream.POST", 1, &url);
        return 1;
    }
    TestCaseEnd("CConn_HttpStream.POST", 0, &url);
    return 0;
}


int CTestNcbiLinkerdCxxApp::TestPost_NewRequest(const CUrl& curl, bool nested)
{
    if ( ! nested)
        TestCaseStart("CHttpSession::NewRequest.POST", 0, &curl);
    CHttpSession session;
    CHttpRequest req = session.NewRequest(curl, CHttpSession::ePost);
    SetupRequest(req);
    CHttpResponse resp = req.Execute();
    int result = PrintResponse(&session, resp);
    if ( ! nested)
        TestCaseEnd("CHttpSession::NewRequest.POST", result, 0, &curl);
    return result;
}


int CTestNcbiLinkerdCxxApp::RunTests(SMapper& mapper)
{
    int num_errors = 0;

    if ( ! mapper.enabled) {
        ERR_POST(Info << string(30, '<') + "  Skipping disabled mapper: " << mapper.name << "  " << string(30, '>'));
        return 0;
    }
    ERR_POST(Info << string(30, '<') + "  Testing mapper: " << mapper.name << "  " << string(30, '>'));

    mapper.Configure(GetRWConfig());
    m_MapperName = mapper.name;

    for (string svc : m_GetCurlService) {
        num_errors += TestGet_CUrl_Service(svc);
    }
    for (SServicePath svc_path : m_GetCurlServicePath) {
        num_errors += TestGet_CUrl_ServicePath(svc_path.service, svc_path.path);
    }
    for (SSchemeServicePath sch_svc_path : m_GetCurlSchemeServicePath) {
        num_errors += TestGet_CUrl_SchemeServicePath(sch_svc_path.scheme, sch_svc_path.service, sch_svc_path.path);
    }
    for (string url : m_GetHttp) {
        num_errors += TestGet_Http(url);
    }
    for (string url : m_GetHttpStream) {
        num_errors += TestGet_HttpStream(url);
    }
    for (string url : m_GetNewRequest) {
        num_errors += TestGet_NewRequest(CUrl(url));
    }

    for (string svc : m_PostCurlService) {
        num_errors += TestPost_CUrl_Service(svc);
    }
    for (SServicePath svc_path : m_PostCurlServicePath) {
        num_errors += TestPost_CUrl_ServicePath(svc_path.service, svc_path.path);
    }
    for (SSchemeServicePath sch_svc_path : m_PostCurlSchemeServicePath) {
        num_errors += TestPost_CUrl_SchemeServicePath(sch_svc_path.scheme, sch_svc_path.service, sch_svc_path.path);
    }
    for (string url : m_PostHttp) {
        num_errors += TestPost_Http(url);
    }
    for (string url : m_PostHttpStream) {
        num_errors += TestPost_HttpStream(url);
    }
    for (string url : m_PostNewRequest) {
        num_errors += TestPost_NewRequest(CUrl(url));
    }

    return num_errors;
}


void CTestNcbiLinkerdCxxApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test Linkerd via C++ classes");

    arg_desc->AddDefaultKey("timeout", "Timeout", "Timeout",
       CArgDescriptions::eDouble, "60.0");

    arg_desc->AddDefaultKey("retries", "Retries", "Max HTTP retries",
           CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("test_file", "TestFile", "JSON test data file",
           CArgDescriptions::eString, "test_ncbi_linkerd_cxx.json");

    SetupArgDescriptions(arg_desc.release());

    SOCK_gethostname(m_Hostname, sizeof(m_Hostname));
}


int CTestNcbiLinkerdCxxApp::Run(void)
{
    int num_errors = 0;

    ReadData();

    for (SMapper mapper : m_Mappers)
        num_errors += RunTests(mapper);

    if (num_errors == 0)
        ERR_POST(Info << "All tests passed.");
    else
        ERR_POST(Info << num_errors << " tests failed.");

    return num_errors;
}


int main(int argc, char* argv[])
{
    // Set error posting and tracing on maximum
    //SetDiagTrace(eDT_Enable);
    //SetDiagPostLevel(eDiag_Info);
    //SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default) | eDPF_All);
    //SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
    //                    | eDPF_All | eDPF_OmitInfoSev);
    //UnsetDiagPostFlag(eDPF_Line);
    //UnsetDiagPostFlag(eDPF_File);
    //UnsetDiagPostFlag(eDPF_Location);
    //UnsetDiagPostFlag(eDPF_LongFilename);
    //SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    try {
        return CTestNcbiLinkerdCxxApp().AppMain(argc, argv);
    }
    catch (...) {
        cout << "\n\nunhandled exception" << endl;
    }
}
