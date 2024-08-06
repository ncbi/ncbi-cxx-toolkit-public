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

    string         m_Service;
    TSERV_TypeOnly m_Types;

    string m_ThreadsPassed;
    string m_ThreadsFailed;
    int    m_CompleteThreads;
};


bool CTestApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddKey("service", "Service", "Name of service",
        CArgDescriptions::eString);

    args.AddDefaultKey("types",
        "Types", "Server type(s), e.g. 'ANY', 'ALL', 'HTTP_GET', 'DNS | HTTP',"
        " 'NCBID+STANDALONE', 'NCBID, HTTP', etc. from: "
        "{ NCBID, STANDALONE, HTTP_GET, HTTP_POST, HTTP, FIREWALL, DNS }",
        CArgDescriptions::eString,
        "ANY");

    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "SERVICE CXX test");

    return true;
}


bool CTestApp::TestApp_Init(void)
{
    // Get args as passed in.
    m_Service = GetArgs()["service"].AsString();
    if (m_Service.empty()) {
        ERR_POST(Critical << "Missing service.");
        return false;
    }
    if ( ! x_ParseTypes()) {
        return false;
    }

    ERR_POST(Info << "Service:  '" << m_Service << "'");
    ERR_POST(Info << "Types:    0x" << hex << m_Types);

    m_CompleteThreads = 0;

    return true;
}


bool CTestApp::Thread_Run(int idx)
{
    bool    retval = false;
    string  id = NStr::IntToString(idx);

    PushDiagPostPrefix(("@" + id).c_str());

    vector<CSERV_Info> servers = SERV_GetServers(m_Service, m_Types);

    if (servers.size() > 0) {
        ERR_POST(Info << "Server(s) for service '" << m_Service << "':");
        for (const auto& s : servers) {
            ERR_POST(Info << "    " << s.GetHost() << ':' << s.GetPort()
                     << "  (type = " << SERV_TypeStr(s.GetType())
                     << "; rate = " << s.GetRate() << ')');
        }
        retval = true;
    } else {
        ERR_POST(Error << "Service '" << m_Service
                 << "' appears to have no servers.");
    }

    PopDiagPostPrefix();

    if (retval) {
        if ( ! m_ThreadsPassed.empty()) {
            m_ThreadsPassed += ",";
        }
        m_ThreadsPassed += id;
    } else {
        if ( ! m_ThreadsFailed.empty()) {
            m_ThreadsFailed += ",";
        }
        m_ThreadsFailed += id;
    }
    ERR_POST(Info << "Progress:          " << ++m_CompleteThreads
                  << " out of " << s_NumThreads << " threads complete.");
    ERR_POST(Info << "Passed thread IDs: "
                  << (m_ThreadsPassed.empty() ? "(none)" : m_ThreadsPassed));
    ERR_POST(Info << "Failed thread IDs: "
                  << (m_ThreadsFailed.empty() ? "(none)" : m_ThreadsFailed));

    return retval;
}


bool CTestApp::x_ParseTypes(void)
{
    string types_str = GetArgs()["types"].AsString();
    if (NStr::EqualNocase(types_str, "ALL")) {
        m_Types = fSERV_All;
    } else if (NStr::EqualNocase(types_str, "ANY")) {
        m_Types = fSERV_Any;
    } else {
        list<string> types_list;
        NStr::Split(types_str, " \t|+,;", types_list, NStr::fSplit_Tokenize);
        for (auto typ : types_list) {
            ESERV_Type etyp;
            const char* styp = SERV_ReadType(typ.c_str(), &etyp);
            if ( ! styp  ||  *styp) {
                ERR_POST(Critical << "Invalid server type '" << typ << "'.");
                return false;
            }
            m_Types |= etyp;
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
