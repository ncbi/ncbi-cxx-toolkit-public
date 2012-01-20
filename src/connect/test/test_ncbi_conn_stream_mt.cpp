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
 *   Test CConn_IOStream API in MT mode
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/test_mt.hpp>
#define NCBI_CONN_STREAM_EXPERIMENTAL_API
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_gnutls.h>
/* This header must go last */
#include <common/test_assert.h>

#ifdef NCBI_OS_MSWIN
#  define DEVNULL  "NUL"
#else
#  define DEVNULL  "/dev/null"
#endif /*NCBI_OS_MSWIN*/


BEGIN_NCBI_SCOPE


class CTestApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);

protected:
    virtual bool TestApp_Args(CArgDescriptions& args);

    virtual bool TestApp_Init(void);

private:
    static string sm_URL;
};


string CTestApp::sm_URL;


bool CTestApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddPositional("url", "URL to read from (discard all read data)",
                       CArgDescriptions::eString);
    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "CConn_IOStream API MT test");
    return true;
}


bool CTestApp::TestApp_Init(void)
{
#ifdef HAVE_LIBGNUTLS
    SOCK_SetupSSL(NcbiSetupGnuTls);
#else
    ERR_POST(Warning << "SSL is not supported on this platform");
#endif //HAVE_LIBGNUTLS

    sm_URL = GetArgs()["url"].AsString();
    return !sm_URL.empty();
}


bool CTestApp::Thread_Run(int idx)
{
    string id = NStr::IntToString(idx);

    PushDiagPostPrefix(("@" + id).c_str());

    auto_ptr<CConn_IOStream> inp(NcbiOpenURL(sm_URL));
    CNcbiOfstream out(DEVNULL, IOS_BASE::out | IOS_BASE::binary);

    bool retval = inp.get()  &&  out ? NcbiStreamCopy(out, *inp) : false;
        
    PopDiagPostPrefix();

    return retval;
}


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags(eDPF_DateTime    | eDPF_Severity |
                        eDPF_OmitInfoSev | eDPF_ErrorID  |
                        eDPF_Prefix);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    s_NumThreads = 2; // default is small

    return CTestApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
