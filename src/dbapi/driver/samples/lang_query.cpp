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
 * Author:  Vladimir Soussov
 *
 * This simple program illustrates how to execute SQL statements
 * using one of available DBAPI drivers
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include "dbapi_sample_base.hpp"
#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CLangQueryApp : public CDbapiSampleApp
{
public:
    CLangQueryApp(void);
    virtual ~CLangQueryApp(void);

protected:
    virtual void InitSample(CArgDescriptions& arg_desc);
    virtual int  RunSample(void);
};

CLangQueryApp::CLangQueryApp(void)
:   CDbapiSampleApp(eDoNotUseSampleDatabase)
{
}

CLangQueryApp::~CLangQueryApp(void)
{
}

void
CLangQueryApp::InitSample(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey(
        "Q", "query",
         "SQL statement to execute",
         CArgDescriptions::eString
    );
}

int
CLangQueryApp::RunSample(void)
{
    const CArgs& args = GetArgs();
    string query;

    // Read a query ...
    if (args["Q"].HasValue()) {
        query = args["Q"].AsString();
    } else {
        // query is on stdin
        char c;

        cout << "query is on ctdin" << endl;
        c = cin.get();
        while( !cin.eof() ) {
            cout << c;
            query += c;
            c = cin.get();
        }
        cout << endl;

        if(query.empty())
            query = "exec sp_who";
    }

    cout << "Run query: '" << query << "' on server " << GetServerName()
         << " using driver: " << GetDriverName() << endl;
    cout << "--------------------------------------------"
         << "----------------------------"
         << endl;

    try {
        ShowResults(query);
    }
    catch ( CDB_Exception& e ) {
        CDB_UserHandler::GetDefault().HandleIt(&e);
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    // Execute main application function
    return CLangQueryApp().AppMain(argc, argv);
}


