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
 *   Test C++-only API for named network services in MT mode
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbidiag.hpp>
#include <corelib/test_mt.hpp>

#include <connect/ncbi_service.hpp>

#include "test_assert.h"  // This header must go last


BEGIN_NCBI_SCOPE


class CTestApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);

protected:
    virtual bool TestApp_Args(CArgDescriptions& args);
    virtual bool TestApp_Init(void);

private:
    bool x_ParseTypes(void);

    static string       sm_Service;
    static TSERV_Type   sm_Types;

    static string   sm_ThreadsPassed;
    static string   sm_ThreadsFailed;
    static int      sm_CompleteThreads;
};


string      CTestApp::sm_Service;
TSERV_Type  CTestApp::sm_Types = fSERV_Any;

string  CTestApp::sm_ThreadsPassed;
string  CTestApp::sm_ThreadsFailed;
int     CTestApp::sm_CompleteThreads;


bool CTestApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddKey("service", "Service", "Name of service",
        CArgDescriptions::eString);

    args.AddDefaultKey("types",
        "Types", "Server type(s), eg 'ANY', 'ALL', 'HTTP_GET', "
        "'DNS | HTTP', 'NCBID+STANDALONE', 'NCBID, HTTP', etc.  "
        "From: { NCBID, STANDALONE, HTTP_GET, HTTP_POST, HTTP, FIREWALL, DNS }",
        CArgDescriptions::eString,
        "ANY");

    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "SERVICE CXX test");

    return true;
}


bool CTestApp::TestApp_Init(void)
{
    // Get args as passed in.
    sm_Service = GetArgs()["service"].AsString();
    if (sm_Service.empty()) {
        ERR_POST(Critical << "Missing service.");
        return false;
    }
    if ( ! x_ParseTypes()) {
        return false;
    }

    ERR_POST(Info << "Service:  '" << sm_Service << "'");
    ERR_POST(Info << "Types:    0x" << hex << sm_Types);

    sm_CompleteThreads = 0;

    return true;
}


bool CTestApp::Thread_Run(int idx)
{
    bool    retval = false;
    string  id = NStr::IntToString(idx);

    PushDiagPostPrefix(("@" + id).c_str());

    vector<CSERV_Info>  hosts;
    hosts = SERV_GetServers(sm_Service, sm_Types);
    if (hosts.size() > 0) {
        ERR_POST(Info << "Hosts for service '" << sm_Service << "':");
        for (const auto& h : hosts) {
            ERR_POST(Info << "    " << h.GetHost() << ":" << h.GetPort() <<
                "  (type = " << SERV_TypeStr(h.GetType()) << "; " <<
                "rate = " << h.GetRate() << ")");
        }
        retval = true;
    } else {
        ERR_POST(Error << "Service '" << sm_Service
            << "' appears to have no hosts.");
    }

    PopDiagPostPrefix();

    if (retval) {
        if ( ! sm_ThreadsPassed.empty()) {
            sm_ThreadsPassed += ",";
        }
        sm_ThreadsPassed += id;
    } else {
        if ( ! sm_ThreadsFailed.empty()) {
            sm_ThreadsFailed += ",";
        }
        sm_ThreadsFailed += id;
    }
    ERR_POST(Info << "Progress:          " << ++sm_CompleteThreads
                  << " out of " << s_NumThreads << " threads complete.");
    ERR_POST(Info << "Passed thread IDs: "
                  << (sm_ThreadsPassed.empty() ? "(none)" : sm_ThreadsPassed));
    ERR_POST(Info << "Failed thread IDs: "
                  << (sm_ThreadsFailed.empty() ? "(none)" : sm_ThreadsFailed));

    return retval;
}


bool CTestApp::x_ParseTypes(void)
{
    string types_str = GetArgs()["types"].AsString();
    if (NStr::EqualNocase(types_str, "ALL")) {
        sm_Types = fSERV_All;
    } else if (NStr::EqualNocase(types_str, "ANY")) {
        sm_Types = fSERV_Any;
    } else {
        list<string> types_list;
        NStr::Split(types_str, " |+,;", types_list, NStr::fSplit_Tokenize);
        for (auto typ : types_list) {
            ESERV_Type etyp;
            const char* styp = SERV_ReadType(NStr::ToUpper(typ).c_str(), &etyp);
            if ( ! styp) {
                ERR_POST(Critical << "Invalid server type '" << typ << "'.");
                return false;
            }
            sm_Types |= etyp;
        }
    }
    return true;
}


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_Severity | eDPF_ErrorID | eDPF_Prefix);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    s_NumThreads = 2; // default is small

    return CTestApp().AppMain(argc, argv);
}
