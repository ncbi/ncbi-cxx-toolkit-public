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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include "dbapi_conn_policy.hpp"
#include <dbapi/driver/public.hpp>
#include <dbapi/driver/dbapi_conn_factory.hpp>
#include <connect/ext/ncbi_dblb_svcmapper.hpp>


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////////////
CConnectPolicyApp::CConnectPolicyApp(void)
: CDbapiSampleApp( eDoNotUseSampleDatabase )
{
}

CConnectPolicyApp::~CConnectPolicyApp(void)
{
}


string 
CConnectPolicyApp::GetCurrentDatabase(void)
{
    string sql("select @@servername");
    auto_ptr<CDB_Connection> conn(CreateConnection());

    auto_ptr<CDB_LangCmd> stmt(conn->LangCmd(sql));
    stmt->Send();

    while ( stmt->HasMoreResults() ) {
        auto_ptr<CDB_Result> result(stmt->Result());
        if ( !result.get() )
            continue;

        if ( result->ResultType() == eDB_RowResult ) {
            if ( result->Fetch() ) {
                CDB_LongChar v;
                result->GetItem(&v);
                return v.Value();
            }
        }
    }
    
    return string("Connection wasn't found");
}

int
CConnectPolicyApp::RunSample(void)
{
    try {
        DBLB_INSTALL_DEFAULT();
                
        for (int i = 0; i < 20; ++i) {
            cout << "Connected to the server: '" << GetCurrentDatabase() << "'" << endl;
        }
        
    }
    catch ( CDB_Exception& e ) {
        CDB_UserHandler::GetDefault().HandleIt(&e);
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CConnectPolicyApp().AppMain(argc, argv);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/01/04 13:11:42  ssikorsk
 * An initial version of a sample application.
 *
 * ===========================================================================
 */

