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
 * Authors: Wratko Hlavina, Aaron Ucko
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <dbapi/simple/sdbapi.hpp>

USING_NCBI_SCOPE;

class CSDBAPITestMTPoolingApp : public CThreadedApp
{
public:
    bool Thread_Run(int idx) override;

protected:
    bool TestApp_Args(CArgDescriptions& argdesc) override;
    bool TestApp_Init(void) override;

private:
    CSDB_ConnectionParam m_Params;
};


bool CSDBAPITestMTPoolingApp::Thread_Run(int idx)
{
    auto n = GetArgs()["queries-per-thread"].AsInteger();
    const string& query_string = GetArgs()["query"].AsString();
    for (auto i = n;  i > 0;  --i) {
        CDatabase db(m_Params);
        db.Connect();
        CQuery query = db.NewQuery(query_string);
        query.Execute();
        query.PurgeResults();
        ERR_POST(Note << idx << '\t' << (n-i));
    }
    return true;
}


bool CSDBAPITestMTPoolingApp::TestApp_Args(CArgDescriptions& argdesc)
{
    argdesc.SetUsageContext(GetArguments().GetProgramBasename(),
                             "Test (S)DBAPI connection pool thread safety");

    argdesc.AddDefaultKey("queries-per-thread", "N", "Queries per thread",
                          CArgDescriptions::eInteger, "1000");
    argdesc.SetConstraint("queries-per-thread",
                          new CArgAllow_Integers(1, kMax_Int));

    argdesc.AddDefaultKey("pool-size", "N", "Connection pool size",
                          CArgDescriptions::eInteger, "10");
    argdesc.SetConstraint("pool-size", new CArgAllow_Integers(1, kMax_Int));

    argdesc.AddDefaultKey("wait-time", "N",
                          "Wait time (in seconds) for a connection",
                          CArgDescriptions::eInteger, "10");
    argdesc.SetConstraint("wait-time", new CArgAllow_Integers(0, kMax_Int));
    
    argdesc.AddDefaultKey("service", "service", "Server or service name",
                           CArgDescriptions::eString, "MSDEV1");
    argdesc.AddDefaultKey("username", "username", "User name",
                           CArgDescriptions::eString, "anyone");
    argdesc.AddDefaultKey("password", "password", "Password",
                           CArgDescriptions::eString, "allowed",
                           CArgDescriptions::fConfidential);
    argdesc.AddDefaultKey("query", "query", "Query string",
                          CArgDescriptions::eString, "SELECT 1");

    return true;
}


bool CSDBAPITestMTPoolingApp::TestApp_Init(void)
{
    const CArgs& args = GetArgs();
    m_Params
        .Set(CSDB_ConnectionParam::eUsername,    args["username"].AsString())
        .Set(CSDB_ConnectionParam::ePassword,    args["password"].AsString())
        .Set(CSDB_ConnectionParam::eService,     args["service" ].AsString())
        .Set(CSDB_ConnectionParam::eUseConnPool, "true")
        .Set(CSDB_ConnectionParam::eConnPoolMaxSize,
             args["pool-size"].AsString())
        .Set(CSDB_ConnectionParam::eConnPoolWaitTime,
             args["wait-time"].AsString())
        .Set(CSDB_ConnectionParam::eConnPoolAllowTempOverflow, "false");

    return true;
}


int main(int argc, const char* argv[])
{
    return CSDBAPITestMTPoolingApp().AppMain(argc, argv);
}
