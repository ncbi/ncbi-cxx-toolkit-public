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
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////////////

CConnectPolicyApp::CConnectPolicyApp(void)
: CDbapiSampleApp( eDoNotUseSampleDatabase )
{
}


CConnectPolicyApp::~CConnectPolicyApp(void)
{
}


int
CConnectPolicyApp::RunSample(void)
{
    try {
        DBLB_INSTALL_DEFAULT();

        // CConnValidatorCoR is developed to combine other validators into a chain.
        CConnValidatorCoR conn_validator;

        // Combine validators.
        conn_validator.Push(CRef<IConnValidator>(new CTrivialConnValidator("tempdb")));
        conn_validator.Push(CRef<IConnValidator>(new CTrivialConnValidator("DBAPI_Sample")));

        for (int i = 0; i < 20; ++i) {
            auto_ptr<CDB_Connection> conn(CreateConnection(&conn_validator));
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


