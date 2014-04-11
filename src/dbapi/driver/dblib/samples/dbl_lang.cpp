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
 * This simple program illustrates how to use the language command
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


class CDemoApp : public CNcbiApplication
{
public:
    virtual ~CDemoApp(void) {}

    virtual int Run(void);
};

int
CDemoApp::Run(void)
{
    try {
        DBLB_INSTALL_DEFAULT();

        CDBLibContext my_context;

        auto_ptr<CDB_Connection> con(my_context.Connect("DBAPI_DEV3",
                                                        "DBAPI_test",
                                                        "allowed",
                                                        0));

        auto_ptr<CDB_LangCmd> lcmd
            (con->LangCmd("select name, crdate from sysdatabases"));
        lcmd->Send();

        while (lcmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(lcmd->Result());
            if (!r.get())
                continue;

            cout
                << r->ItemName(0) << " \t\t\t"
                << r->ItemName(1) << endl
                << "-----------------------------------------------------"
                << endl;

            while (r->Fetch()) {
                CDB_Char dbname(24);
                CDB_DateTime crdate;

                r->GetItem(&dbname);
                r->GetItem(&crdate);

                cout
                    << dbname.AsString() << ' '
                    << crdate.Value().AsString("M/D/Y h:m")
                    << endl;
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
    return CDemoApp().AppMain(argc, argv);
}


