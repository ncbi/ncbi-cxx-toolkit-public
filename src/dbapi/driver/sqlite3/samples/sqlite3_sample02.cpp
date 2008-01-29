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
 * Author:  Sergey Sikorskiy
 *
 * This program illustrates how to use SQLite3 DBAPI driver
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/sqlite3/interfaces.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


class CDemoeApp : public CNcbiApplication
{
public:
    virtual ~CDemoeApp(void) {}

    virtual void Init(void);
    virtual int Run(void);
};


void
CDemoeApp::Init()
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "SQLite3 Sample Application");

    // Describe the expected command-line arguments
    arg_desc->AddKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString);

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString,
                            "anyone");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString,
                            "allowed");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int
CDemoeApp::Run(void)
{
    const CArgs& args = GetArgs();

    // Get command-line arguments ...
    string ServerName = args["S"].AsString();
    string UserName   = args["U"].AsString();
    string Password   = args["P"].AsString();

    try {
        CSL3Context my_context;
        auto_ptr<CDB_Connection> con(my_context.Connect(ServerName, UserName, Password, 0));

        // Drop a table if any ...
        {
            try {
                auto_ptr<CDB_LangCmd>
                    lcmd(con->LangCmd("DROP TABLE BulkSample"));

                lcmd->Send();
            }
            catch(const CDB_Exception&) {
                // Just ignore it ...
            }
        }

        // create a table
        {
            auto_ptr<CDB_LangCmd>
                lcmd(con->LangCmd("CREATE TABLE BulkSample ("
                                  "id INT NOT NULL,"
                                  "ord INT NOT NULL,"
                                  "mode TINYINT NOT NULL,"
                                  "date DATETIME NOT NULL )"
                                  ));
            lcmd->Send();
        }

        const unsigned int nBlobSize = 0xffff;
        auto_ptr<char> buff( new char[nBlobSize]);
        const string str_const("Ooops ....");

        // insert data
        {
            CDB_Int value1;
            CDB_Int value2;
            CDB_TinyInt value3;
            CDB_DateTime value4;

            auto_ptr<CDB_BCPInCmd> cmd(con->BCPIn("BulkSample"));

            cmd->Bind(0, &value1);
            cmd->Bind(1, &value2);
            cmd->Bind(2, &value3);
            cmd->Bind(3, &value4);

            for(int i = 0; i < 10; ++i ) {
                value1 = i;
                value2 = i * 2;
                value3 = Uint1(i + 33);
                value4 = CTime(CTime::eCurrent);
                cmd->SendRow();
            }

            cmd->CompleteBCP();
        }

        // get data
        {
            auto_ptr<CDB_LangCmd> lcmd(con->LangCmd("select * from BulkSample"));
            lcmd->Send();

            while (lcmd->HasMoreResults()) {
                auto_ptr<CDB_Result> r(lcmd->Result());
                while (r->Fetch()) {
                    CDB_Int value1;
                    CDB_Int value2;
                    CDB_TinyInt value3;
                    CDB_DateTime value4;

                    r->GetItem(&value1);
                    r->GetItem(&value2);
                    r->GetItem(&value3);
                    r->GetItem(&value4);

                    cout << value1.Value() << " "
                         << value2.Value() << " "
                         << value3.Value() << " "
                         << value4.Value().AsString() << endl;
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
    return CDemoeApp().AppMain(argc, argv);
}


