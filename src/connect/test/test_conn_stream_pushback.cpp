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
 *   Test CStreamUtils::Pushback() interface.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_util.h>
#include "../ncbi_priv.h"
#include "../../corelib/test/pbacktest.hpp"

#include "test_assert.h"  // This header must go last

USING_NCBI_SCOPE;


class CNCBITestApp : public CNcbiApplication
{
public:
    CNCBITestApp(void);

public:
    int Run(void);

private:
    unsigned int m_Seed;
};


CNCBITestApp::CNCBITestApp(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));
}


int CNCBITestApp::Run(void)
{
    const CNcbiArguments& app_args = GetArguments();
    m_Seed = app_args.Size() > 1
        ? (unsigned int) atoi(app_args[1].c_str())
        : (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    string host = "www.ncbi.nlm.nih.gov";
    string path = "/Service/bounce.cgi";
    string args = kEmptyStr;
    string uhdr = kEmptyStr;

    ERR_POST(Info << "Seed = " << m_Seed);
    srand(m_Seed);

    string url = "https://" + host + path + &"?"[args.empty() ? 1 : 0] + args;
    ERR_POST(Info << "Creating HTTP connection to " + url);
    CConn_HttpStream ios(url, eReqMethod_Any, uhdr);

    return TEST_StreamPushback(ios, false/*no rewind*/);
}


int main(int argc, char* argv[])
{
    return CNCBITestApp().AppMain(argc, argv);
}
