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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Special HTTP upload test for demonstration of streaming capabilities
 *   of HTTP/1.1.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/rwstream.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_gnutls.h>
#include <stdlib.h>
#include <time.h>
#include "../ncbi_priv.h"

#include "test_assert.h"  // This header must go last


#define MAX_FILE_SIZE  (100 * (1 << 20))
#define MAX_REC_SIZE    10000


BEGIN_NCBI_SCOPE


static const char kSubId[] = " SubmissionPortalSID=";


static string s_GetHttpCred(void)
{
    const char* credfile = getenv("TEST_NCBI_HTTP_UPLOAD");
    if (!credfile)
        credfile = "/am/ncbiapdata/test_data/http/test_ncbi_http_upload";
    ifstream ifs(credfile);
    if (!ifs)
        return kEmptyStr;
    string key;
    ifs >> key;
    return key;
}


extern "C" {

static EHTTP_HeaderParse x_ParseKeyHeader(const char* header,
                                          void* data, int server_error)
{
    string* subid = reinterpret_cast<string*>(data);
    _ASSERT(subid  &&  subid->empty());
    if (!server_error) {
        SIZE_TYPE keypos = NStr::FindNoCase(header, kSubId);
        if (keypos == NPOS)
            return eHTTP_HeaderError;
        keypos += sizeof(kSubId) - 1;
        CTempString tmp(NStr::GetField_Unsafe(header + keypos, 0, "; \t"));
        if (tmp.empty())
            return eHTTP_HeaderError;
        *subid = tmp;
    }
    return eHTTP_HeaderSuccess;
}


static EHTTP_HeaderParse x_ParseSubHeader(const char* header,
                                          void* unused, int server_error)
{
    if (!server_error) {
        int code;
        ::sscanf(header, "%*s%d", &code);
        if (code != 201)
            return eHTTP_HeaderError;
    }
    return eHTTP_HeaderSuccess;
}

}


class CNullCountingWriter : public IWriter
{
public:
    CNullCountingWriter(size_t& count) 
        : m_Count(count)
    { }

public:
    virtual ERW_Result Write(const void* /*buf*/,
                             size_t      count,
                             size_t*     bytes_written = 0)
    {
        m_Count += count;
        if ( bytes_written )
            *bytes_written = count;
        return eRW_Success;
    }

    virtual ERW_Result Flush(void) { return eRW_Success; }

private:
    size_t& m_Count;
};


class CNCBITestApp : public CNcbiApplication
{
public:
    CNCBITestApp(void);

public:
    void Init(void);
    int  Run (void);
};


CNCBITestApp::CNCBITestApp(void)
{
    // Setup error posting
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Trace);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));
}


void CNCBITestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    if (arg_desc->Exist ("h"))
        arg_desc->Delete("h");
    if (arg_desc->Exist ("xmlhelp"))
        arg_desc->Delete("xmlhelp");
    arg_desc->AddOptionalPositional("seed",
                                    "Random seed to use",
                                    CArgDescriptions::eInteger);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test HTTP/1.1 chunked upload");

    SetupArgDescriptions(arg_desc.release());
}


int CNCBITestApp::Run(void)
{
    static const char kAuthUrl[]
        = "https://dsubmit.ncbi.nlm.nih.gov/accounts/api_login";
    static const char kHttpUrl[]
        = "https://dsubmit.ncbi.nlm.nih.gov/api/2.0/uploads/binary/";
    static const char kDownUrl[]
        = "https://dsubmit.ncbi.nlm.nih.gov/ft/byid/";

    string key = s_GetHttpCred();
    if (key.empty())
        ERR_POST(Fatal << "Empty credentials");

    const CArgs& args = GetArgs();
    if (args["seed"].HasValue()) {
        g_NCBI_ConnectRandomSeed
            = NStr::StringToUInt8(args["seed"].AsString());
    } else
        g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    ERR_POST(Info << "Random SEED = "
             + NStr::NumericToString(g_NCBI_ConnectRandomSeed));
    srand(g_NCBI_ConnectRandomSeed);

    string subid;
    CConn_HttpStream auth(kAuthUrl + string("?key=") + key, 0/*net_info*/,
                          kEmptyStr/*user_header*/, x_ParseKeyHeader, &subid,
                          0/*adjust*/, 0/*cleanup*/,
                          fHTTP_Flushable | fHTTP_NoAutoRetry);
    auth.Close();

    // cout << "Got SubID = " << subid << endl;

    CTime  now(CTime::eCurrent, CTime::eLocal);

    size_t n = rand() % MAX_FILE_SIZE;
    if (n == 0)
        n = MAX_FILE_SIZE;

    string file = "test_ncbi_http_upload_"
        + CSocketAPI::gethostname()
        + "_" + NStr::NumericToString(CProcess::GetCurrentPid())
        + "_" + now.AsString("YMD_hms_S")
        + "_" + NStr::NumericToString(n);

    string user_header = string("Content-Type: application/octet-stream\r\n"
                                "Cookie: ") + kSubId + subid + "\r\n"
        "File-Editable: false\r\n"
        "File-ID: " + file + "\r\n"
        "File-Expires: "
        + now.AddMinute(5).AsString(CTimeFormat::GetPredefined
                                    (CTimeFormat::eISO8601_DateTimeSec));

    SConnNetInfo* net_info = ConnNetInfo_Create(0);
    net_info->version = 1;  /*HTTP/1.1*/

    CConn_HttpStream http(kHttpUrl,
                          net_info,
                          user_header,
                          x_ParseSubHeader, 0, 0, 0,
                          fHTTP_NoAutoRetry | fHTTP_WriteThru);

    char* buf = new char[MAX_REC_SIZE];
    size_t m = 0;
    for (size_t i = 0, k = 0;  i < n;  i += k) {
        k = rand() % (MAX_REC_SIZE << 1);
        if (k == 0)
            k = MAX_REC_SIZE;
        if (k > n - i)
            k = n - i;
        if (k > MAX_REC_SIZE)
            k = MAX_REC_SIZE;
        for (size_t j = 0;  j < k;  ++j) 
            buf[j] = (unsigned char) rand() & 0xFF;
        // cout << "Record size: " << NStr::NumericToString(k) << endl;
        if (!http.write(buf, k)) {
            ERR_POST(Fatal << "Write error, offset "
                     + NStr::NumericToString(i) + ", size "
                     + NStr::NumericToString(k) + ", iteration "
                     + NStr::NumericToString(m + 1));
        }
        ++m;
    }
    delete[] buf;

    ERR_POST(Info << file + " (size " + NStr::NumericToString(n) + ")"
             " sent in " + NStr::NumericToString(m) + " chunk(s)");

    string sub;
    if (!NcbiStreamToString(&sub, http))
        ERR_POST(Fatal << "Submission error");
    http.Close();

    ERR_POST(Info << "Submission info:\n" << sub);

    size_t s = NStr::Find(sub, "\"size\":");
    if (s == NPOS)
        ERR_POST(Fatal << "No file size info found");
    CTempString size(NStr::GetField_Unsafe(&sub[s + 7], 0, ','));
    if (size.empty()
        ||  !(s = NStr::StringToUInt8(size, NStr::fConvErr_NoThrow))) {
        ERR_POST(Fatal << "File size unparsable");
    }
    if (n != s) {
        ERR_POST(Fatal << "File size mismatch: "
                 + NStr::NumericToString(n) + " uploaded != "
                 + NStr::NumericToString(s) + " reported");
    }

    s = 0;
    CWStream null(new CNullCountingWriter(s), 0, 0, CRWStreambuf::fOwnWriter);
    CConn_HttpStream down(kDownUrl + file + "/contents", net_info);
    ConnNetInfo_Destroy(net_info);

    if (!NcbiStreamCopy(null, down))
        ERR_POST(Fatal << "Read error from server");

    if (n != s) {
        ERR_POST(Fatal << "File size mismatch: "
                 + NStr::NumericToString(n) + " uploaded != "
                 + NStr::NumericToString(s) + " received");
    }

    ERR_POST(Info << "TEST COMPLETED SUCCESSFULLY");
    return 0/*okay*/;
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;
    return CNCBITestApp().AppMain(argc, argv);
}
