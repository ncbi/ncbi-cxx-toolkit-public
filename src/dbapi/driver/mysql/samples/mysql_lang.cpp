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
 * Author:  Anton Butanayev
 *
 * This simple program illustrates how to use the language command
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


class CDemoApp : public CNcbiApplication
{
public:
    virtual ~CDemoApp(void) {}

    virtual void Init(void);
    virtual int Run(void);
};


void
CDemoApp::Init()
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "MySQL Sample Application");

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

    arg_desc->AddKey("D", "database",
                            "Name of a database",
                            CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int
CDemoApp::Run(void)
{
    const CArgs& args = GetArgs();

    // Get command-line arguments ...
    string ServerName = args["S"].AsString();
    string UserName   = args["U"].AsString();
    string Password   = args["P"].AsString();
    string Database   = args["D"].AsString();

    try {
        DBLB_INSTALL_DEFAULT();

        CMySQLContext my_context;
        auto_ptr<CDB_Connection> con(my_context.Connect(ServerName, UserName, Password, 0));

        // changing database
        con->SetDatabaseName(Database);
        cout << "Database changed" << endl;

        // creating table
        {
            auto_ptr<CDB_LangCmd>
                lcmd(con->LangCmd("create temporary table tmp_t1("
                                  "a int,"
                                  "b datetime,"
                                  "c varchar(100),"
                                  "d text,"
                                  "e double,"
                                  "bl BLOB)"
                                  ));
            lcmd->Send();
            cout << "Table created" << endl;
        }

        int nBlobSize = 0xffff;
        auto_ptr<char> buff( new char[nBlobSize]);

        // inserting data
        {
            char* p = buff.get();
            for( int i = 0; i < nBlobSize; i++)
                *(p++) = i;

            auto_ptr<CDB_LangCmd> tmp_cmd(con->LangCmd("tmp"));

            string sql = "insert into tmp_t1 values";
            sql += "(1, '2002-11-25 12:45:59', 'Hello, world', 'SOME TEXT', 3.1415, '";
            sql += reinterpret_cast<CMySQL_LangCmd*>(tmp_cmd.get())->EscapeString( buff.get(), nBlobSize);
            sql += ")";

            auto_ptr<CDB_LangCmd> lcmd(con->LangCmd(sql));
            lcmd->Send();
            cout << "Data inserted " << lcmd->RowCount() << " row(s) affected" << endl;
        }

        // selecting data
        {
            auto_ptr<CDB_LangCmd> lcmd(con->LangCmd("select * from tmp_t1"));
            lcmd->Send();
            while (lcmd->HasMoreResults()) {
                auto_ptr<CDB_Result> r(lcmd->Result());
                while (r->Fetch()) {
                    CDB_Int a;
                    CDB_DateTime b;
                    CDB_VarChar c;
                    CDB_VarChar d;
                    CDB_Double e;
                    CDB_Image blob;

                    r->GetItem(&a);
                    r->GetItem(&b);
                    r->GetItem(&c);
                    r->GetItem(&d);
                    r->GetItem(&e);
                    r->GetItem(&blob);

                    auto_ptr<char> buff2( new char[blob.Size()]);
                    blob.Read( buff2.get(), blob.Size());
                    int correct = memcmp( buff2.get(), buff.get(), nBlobSize);

                    cout
                        << "a=" << a.Value() << endl
                        << "b=" << b.Value().AsString() << endl
                        << "c=" << c.Value() << endl
                        << "d=" << d.Value() << endl
                        << "e=" << e.Value() << endl
                        << "blob size is " << nBlobSize << " blob data is "
                        << (!correct ? "correct" : "not correct") << endl;
                }
            }
        }

        // selecting data as strings
        {
            auto_ptr<CDB_LangCmd> lcmd(con->LangCmd("select * from tmp_t1"));
            lcmd->Send();
            while (lcmd->HasMoreResults()) {
                auto_ptr<CDB_Result> r(lcmd->Result());
                for(unsigned i = 0; i < r->NofItems(); ++i)
                    cout << "[" << r->ItemName(i) << "]";
                cout << endl;

                while (r->Fetch()) {
                    for(unsigned i = 0; i < r->NofItems(); ++i) {
                        CDB_VarChar field;
                        r->GetItem(&field);
                        if(! field.IsNULL())
                            cout << field.Value() << endl;
                                else
                                    cout << "NULL\n";

                    }
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
    return CDemoApp().AppMain(argc, argv);
}


