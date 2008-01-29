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
                    lcmd(con->LangCmd("DROP TABLE table01"));

                lcmd->Send();
            }
            catch(const CDB_Exception&) {
                // Just ignore it ...
            }
        }

        // create a table
        {
            auto_ptr<CDB_LangCmd>
                lcmd(con->LangCmd("CREATE TABLE table01 ("
                                  "int_field INTEGER,"
                                  "date_field INTEGER,"
                                  "varchar_field VARCHAR(100),"
                                  "double_field DOUBLE,"
                                  "text_field TEXT,"
                                  "blob_field BLOB)"
                                  ));
            lcmd->Send();
        }

        const unsigned int nBlobSize = 0xffff;
        auto_ptr<char> buff( new char[nBlobSize]);
        const string str_const("Ooops ....");

        // insert data
        {
            CDB_Int int_value;
            CDB_Float float_value;
            CDB_DateTime date_value(CTime::eCurrent);
            CDB_VarChar str_value;
            CDB_Text text_value;
            CDB_Image blob_value;

            char* p = buff.get();
            for( unsigned int i = 0; i < nBlobSize;) {
                for (unsigned char c = 0; c < 255 && i < nBlobSize; ++c, ++i) {
                    *(p++) = c;
                }
            }

            auto_ptr<CDB_LangCmd>
                lcmd(con->LangCmd("insert into table01 values("
                                  ":int_field, :date_field, :varchar_field,"
                                  ":double_field, :text_field, :blob_field "
                                  ")"
                                  ));

            lcmd->BindParam("int_field", &int_value);
            lcmd->BindParam("date_field", &date_value);
            lcmd->BindParam("varchar_field", &str_value);
            lcmd->BindParam("double_field", &float_value);
            lcmd->BindParam("text_field", &text_value);
            lcmd->BindParam("blob_field", &blob_value);

            int_value = 2;
            str_value = "Brand new value";
            float_value = float(3.4);
            text_value.Append(str_const);
            blob_value.Append(buff.get(), nBlobSize);

            lcmd->Send();

            cout << "Data inserted " << lcmd->RowCount() << " row(s) affected" << endl << endl;
        }

        // selecting data
        {
            auto_ptr<CDB_LangCmd> lcmd(con->LangCmd("select * from table01"));
            lcmd->Send();

            while (lcmd->HasMoreResults()) {
                auto_ptr<CDB_Result> r(lcmd->Result());
                while (r->Fetch()) {
                    CDB_Int int_value;
                    CDB_Float float_value;
                    CDB_DateTime date_value;
                    CDB_VarChar str_value;
                    CDB_Text text_value;
                    CDB_Image blob_value;

                    r->GetItem(&int_value);
                    r->GetItem(&date_value);
                    r->GetItem(&str_value);
                    r->GetItem(&float_value);
                    r->GetItem(&text_value);
                    r->GetItem(&blob_value);

                    auto_ptr<char> buff2( new char[blob_value.Size()]);
                    blob_value.Read( buff2.get(), blob_value.Size());
                    const int blob_correct = memcmp( buff2.get(), buff.get(), nBlobSize);

                    auto_ptr<char> buff3( new char[text_value.Size()]);
                    text_value.Read( buff3.get(), text_value.Size());
                    const int text_correct = memcmp( buff3.get(), str_const.c_str(), str_const.size());

                    cout
                        << "int_field = " << int_value.Value() << endl
                        << "date_field = " << date_value.Value().AsString() << endl
                        << "varchar_field = " << str_value.Value() << endl
                        << "double_field = " << float_value.Value() << endl
                        << "Original text had size " << str_const.size() << ", retrieved text data size is "
                        << (!text_correct ? "correct" : "not correct") << endl
                        << "Original blob had size " << nBlobSize << ", retrieved blob data size is "
                        << (!blob_correct ? "correct" : "not correct") << endl << endl;
                }
            }
        }

        // selecting data as strings
        {
            auto_ptr<CDB_LangCmd> lcmd(con->LangCmd("select * from table01"));
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
                        if(! field.IsNULL()) {
                            cout << field.Value() << endl;
                        } else {
                            cout << "NULL\n";
                        }
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
    return CDemoeApp().AppMain(argc, argv);
}


