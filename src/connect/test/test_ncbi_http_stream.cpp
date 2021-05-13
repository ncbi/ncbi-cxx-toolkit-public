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
 *   Test NCBI HTTP stream
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_param.hpp>
#include <connect/ncbi_conn_stream.hpp>
#ifdef NCBI_OS_MSWIN
#  include <conio.h>
#endif // NCBI_OS_MSWIN
#ifdef NCBI_OS_UNIX
#  include <signal.h>
#endif

#include "test_assert.h"  // This header must go last


BEGIN_NCBI_SCOPE


static volatile bool s_Canceled = false;


class CCanceled : public CObject, public ICanceled
{
public:
    virtual bool IsCanceled(void) const { return s_Canceled; }
};


#if   defined(NCBI_OS_MSWIN)
static BOOL WINAPI s_Interrupt(DWORD type)
{
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        s_Canceled = true;
        return TRUE;  // handled
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    default:
        break;
    }
    return FALSE;  // unhandled
}
#elif defined(NCBI_OS_UNIX)
extern "C" {
static void s_Interrupt(int /*signo*/)
{
    s_Canceled = true;
}
}
#endif // NCBI_OS


NCBI_PARAM_DECL  (string, CONN, TEST_NCBI_HTTP_GET_CLIENT_CERT);
NCBI_PARAM_DEF_EX(string, CONN, TEST_NCBI_HTTP_GET_CLIENT_CERT, "",
                  eParam_Default, CONN_TEST_NCBI_HTTP_GET_CLIENT_CERT);
static NCBI_PARAM_TYPE(CONN, TEST_NCBI_HTTP_GET_CLIENT_CERT) s_CertFile;

NCBI_PARAM_DECL  (string, CONN, TEST_NCBI_HTTP_GET_CLIENT_PKEY);
NCBI_PARAM_DEF_EX(string, CONN, TEST_NCBI_HTTP_GET_CLIENT_PKEY, "",
                  eParam_Default, CONN_TEST_NCBI_HTTP_GET_CLIENT_PKEY);
static NCBI_PARAM_TYPE(CONN, TEST_NCBI_HTTP_GET_CLIENT_PKEY) s_PkeyFile;


class CNCBITestHttpStreamApp : public CNcbiApplication
{
public:
    CNCBITestHttpStreamApp(void);
    ~CNCBITestHttpStreamApp();

public:
    void Init(void);
    int  Run (void);

protected:
    string x_LoadFile(const string& filename);

    typedef enum {
        eX509_DER,
        eX509_PEM
    } EX509DataType;

    EX509DataType x_X509DataType(const string& filename,
                                 const string& contents);

private:
    NCBI_CRED m_Cred;
};


CNCBITestHttpStreamApp::~CNCBITestHttpStreamApp()
{
    if (m_Cred)
        NcbiDeleteTlsCertCredentials(m_Cred);
}


CNCBITestHttpStreamApp::CNCBITestHttpStreamApp(void)
    : m_Cred(0)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));
    DisableArgDescriptions(fDisableStdArgs);
    HideStdArgs(-1/*everything*/);
}


void CNCBITestHttpStreamApp::Init(void)
{
    // Usage setup
    unique_ptr<CArgDescriptions> args(new CArgDescriptions);
    if (args->Exist ("h"))
        args->Delete("h");
    if (args->Exist ("xmlhelp"))
        args->Delete("xmlhelp");
    args->AddExtra(1/*one mandatory*/, kMax_UInt/*unlimited optional*/,
                   "List of URL(s) to process", CArgDescriptions::eString);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "Test NCBI HTTP stream");
    SetupArgDescriptions(args.release());

    const string certfile = s_CertFile.Get();
    const string pkeyfile = s_PkeyFile.Get();
    if (certfile.empty()  ||  pkeyfile.empty())
        return;

    string cert = x_LoadFile(certfile);
    if (cert.empty()) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "Failed to load certificate from \"" + certfile + '"');
    }
    string pkey = x_LoadFile(pkeyfile);
    if (pkey.empty()) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "Failed to load private key from \"" + pkeyfile + '"');
    }

    // Explicitly init the CONNECT library (this also sets up the registry)
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        } conn_initer;  /*NCBI_FAKE_WARNING*/
    }
    // NB: Alternlatively, CONNECT_Init() can be used for applications

    size_t certsize = (x_X509DataType(certfile, cert) == eX509_PEM
                       ? 0 /*cert.size() + 1*/
                       : cert.size());
    size_t pkeysize = (x_X509DataType(pkeyfile, pkey) == eX509_PEM
                       ? 0 /*pkey.size() + 1*/
                       : pkey.size());
    if (!(m_Cred = NcbiCreateTlsCertCredentials(cert.c_str(), certsize,
                                                pkey.c_str(), pkeysize))) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "Failed to build NCBI_CRED from provided data");
    }
    LOG_POST(Info << "NCBI_CRED loaded from \""
             + certfile + "\" and \""
             + pkeyfile + '"');
}


string CNCBITestHttpStreamApp::x_LoadFile(const string& filename)
{
    ifstream ifs(filename.c_str(), ios::binary);
    if (!ifs)
        return kEmptyStr;
    string cont;
    size_t size = NcbiStreamToString(&cont, ifs);
    return ifs.eof()  &&  size ? cont : kEmptyStr;
}


CNCBITestHttpStreamApp::EX509DataType CNCBITestHttpStreamApp::x_X509DataType
(const string& /*filename*/, const string& contents)
{
    /* NB: the real case scenario should know what type of credentials *
     *     there is, and not use the heuristics like below...          */
    return NStr::Find(contents, "-----BEGIN ") != NPOS ? eX509_PEM : eX509_DER;
}


int CNCBITestHttpStreamApp::Run(void)
{
    const CArgs& args = GetArgs();
    size_t n = args.GetNExtra();
    _ASSERT(n > 0);

    CCanceled canceled;
    unique_ptr<CConn_HttpStream> http;
    if (m_Cred) {
        unique_ptr<SConnNetInfo, void(*)(SConnNetInfo*)> net_info
            (ConnNetInfo_Create(0), ConnNetInfo_Destroy);
        net_info->req_method  = eReqMethod_Any11;
        net_info->credentials = m_Cred;
        http.reset(new CConn_HttpStream(args[1].AsString(), net_info.get(),
                                        kEmptyStr/*user-header*/,
                                        0, 0, 0, 0,
                                        fHTTP_NoAutoRetry));
    } else {
        http.reset(new CConn_HttpStream(args[1].AsString(),
                                        eReqMethod_Any11,
                                        kEmptyStr/*user-header*/,
                                        fHTTP_NoAutoRetry));
    }
    http->SetCanceledCallback(&canceled);

    int retval = -1;
    for (size_t i = 1; ;) {
        string response;
        bool copied = false;
        try {
            if (::rand() & 1)
                http->Fetch();
            CNcbiOstrstream temp;
            copied = NcbiStreamCopy(temp, *http);
            NcbiCerr << "\tTEMP   good:\t" << int(temp.good())  << NcbiEndl
                     << "\tTEMP    eof:\t" << int(temp.eof())   << NcbiEndl
                     << "\tTEMP   fail:\t" << int(temp.fail())  << NcbiEndl
                     << "\tTEMP    bad:\t" << int(temp.bad())   << NcbiEndl;
            string(CNcbiOstrstreamToString(temp)).swap(response);
        }
        NCBI_CATCH_ALL("NcbiStreamCopy() failed");
        EIO_Status status = http->Status();
        NcbiCerr << "\tStatus code:\t" << http->GetStatusCode() << NcbiEndl
                 << "\tStatus text:\t" << http->GetStatusText() << NcbiEndl
                 << "\tI/O  status:\t" << IO_StatusStr(status)
                 << '(' << int(status) << ')'                   << NcbiEndl
                 << "\tHTTP   good:\t" << int(http->good())     << NcbiEndl
                 << "\tHTTP    eof:\t" << int(http->eof())      << NcbiEndl
                 << "\tHTTP   fail:\t" << int(http->fail())     << NcbiEndl
                 << "\tHTTP    bad:\t" << int(http->bad())      << NcbiEndl
                 << "\tCopy result:\t" << (copied ? 'Y' : 'N')  << NcbiEndl
                 << "\tBody   size:\t" << response.size()       << NcbiEndl
                 << NcbiEndl;
        if (!copied) {
            retval = EIO_N_STATUS + 1;
            break;
        }
        NcbiCout << response << NcbiEndl;
        retval = int(http->Status());
        if (retval == eIO_Success)
            retval  = EIO_N_STATUS;
        if (retval != eIO_Closed)
            break;
        retval = 0/*eIO_Success*/;
        if (i >= n)
            break;
        http->clear();
        http->SetURL(args[++i].AsString());
    }
    if (retval)
        ERR_POST(Error << "Test failed");
    else
        LOG_POST(Info << "TEST COMPLETED SUCCESSFULLY");
    return retval;
}


END_NCBI_SCOPE



/* To test chunked input, run in the console:
 *
 * nc -l -p 8812 -c 'echo -e -n "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n8\r\n-World\r\n\r\n0\r\n\r\n"'
 *
 * Start the test with this URL:
 *
 * "http://localhost:8812"
 */

int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;

    // Ctrl-C handling
#if   defined(NCBI_OS_MSWIN)
    SetConsoleCtrlHandler(s_Interrupt, TRUE);
    static char buf[4096];
    cerr.rdbuf()->pubsetbuf(buf, sizeof(buf));
    cerr.unsetf(ios_base::unitbuf);
#elif defined(NCBI_OS_UNIX)
    signal(SIGINT,  s_Interrupt);
    signal(SIGQUIT, s_Interrupt);
#endif // NCBI_OS
    SOCK_SetInterruptOnSignalAPI(eOn);

    return CNCBITestHttpStreamApp().AppMain(argc, argv, 0, eDS_User);
}
