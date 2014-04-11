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
 * This simple program illustrates how to use the RPC command
 *
 */

#include <ncbi_pch.hpp>

#include "../../dbapi_driver_sample_base.hpp"
#include <dbapi/driver/exception.hpp>
#ifdef FTDS_IN_USE
# include <dbapi/driver/ftds64/interfaces.hpp>
#else
# include <dbapi/driver/ctlib/interfaces.hpp>
#endif
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


class CDemoApp : public CDbapiDriverSampleApp
{
public:
    CDemoApp(const string& server_name,
             int tds_version = GetCtlibTdsVersion());
    virtual ~CDemoApp(void);

    virtual int RunSample(void);
};

CDemoApp::CDemoApp(const string& server_name, int tds_version) :
    CDbapiDriverSampleApp(server_name, tds_version)
{
}


CDemoApp::~CDemoApp(void)
{
    return;
}


int
CDemoApp::RunSample(void)
{
    try {
        DBLB_INSTALL_DEFAULT();

#ifdef FTDS_IN_USE
        CTDSContext my_context(true, GetTDSVersion());
#else
        CTLibContext my_context(true, GetTDSVersion());
#endif

        auto_ptr<CDB_Connection> con(my_context.Connect(GetServerName(),
                                                        GetUserName(),
                                                        GetPassword(),
                                                        0));

        auto_ptr<CDB_RPCCmd> rcmd(con->RPC("sp_who"));
        rcmd->Send();

        while (rcmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(rcmd->Result());
            if (!r.get())
                continue;

            if (r->ResultType() == eDB_RowResult) {
                while (r->Fetch()) {
                    for (unsigned int j = 0;  j < r->NofItems(); j++) {
                        EDB_Type rt = r->ItemDataType(j);
                        if (rt == eDB_Char || rt == eDB_VarChar) {
                            CDB_VarChar r_vc;
                            r->GetItem(&r_vc);
                            cout << r->ItemName(j) << ": " << r_vc.AsString()
                                 << " \t";
                        } else if (rt == eDB_Int ||
                                   rt == eDB_SmallInt ||
                                   rt == eDB_TinyInt) {
                            CDB_Int r_in;
                            r->GetItem(&r_in);
                            cout << r->ItemName(j) << ": " << r_in.Value()
                                 << ' ';
                        } else
                            r->SkipItem();
                    }
                    cout << endl;
                }
            }
        }
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
        return 1;
    } catch (const CException&) {
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CDemoApp("DBAPI_DEV3").AppMain(argc, argv);
}


