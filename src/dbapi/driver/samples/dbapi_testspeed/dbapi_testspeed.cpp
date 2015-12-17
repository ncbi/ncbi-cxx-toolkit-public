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
* File Description:
*   Run a series of insert/update/select statement,
*   measure the time required for their execution.
*
*============================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbifile.hpp>
#include "dbapi_testspeed.hpp"
#include "../dbapi_sample_base.hpp"
#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  CDbapiTestBcpApp::
//

class CDbapiTestSpeedApp : public CDbapiSampleApp
{
public:
    CDbapiTestSpeedApp(void);
    virtual ~CDbapiTestSpeedApp(void);

protected:
    virtual void InitSample(CArgDescriptions& arg_desc);
    virtual int  RunSample(void);

protected:
    ///
    void FetchFile (const string& table_name, bool readItems);
    ///
    void FetchResults (const string& table_name, bool readItems);
    string GetTableName(void) const;

private:
    string m_TableName;
};

CDbapiTestSpeedApp::CDbapiTestSpeedApp(void)
{
}

CDbapiTestSpeedApp::~CDbapiTestSpeedApp(void)
{
}

inline
string
CDbapiTestSpeedApp::GetTableName(void) const
{
    return m_TableName;
}

// const char usage[] =
// "Run a series of BCP/insert/select commands,\n"
// "measure the time required for their execution. \n"
// "\n"
// "USAGE:   dbapi_testspeed -parameters [file]\n"
// "REQUIRED PARAMETERS:\n"
// "  -S server\n"
// "  -d driver (e.g. ctlib dblib ftds odbc gateway)\n"
// "  -g sss_server:port for gateway database driver\n"
// "     (only one of -d/-g is required to be present)\n"
// "OPTIONAL PARAMETERS:\n"
// "  -r row_count (default is 1)\n"
// "  -b text_blob_size in kilobytes (default is 1 kb)\n"
// "  -c column_count  1..5 (int_val fl_val date_val str_val txt_val)\n"
// "  -t table_name (default is 'TestSpeed')\n"
// "  -m mode_character:\n"
// "     r  write and read as usual,\n"
// "        use CDB_Result->ReadItem() instead of GetItem() whenever possible\n"
// "     w  write to the table, do not read, do not delete it\n"
// "     R  read using ReadItem() (can be done many times after '-m w' )\n"
// "     G  read using GetItem()  (can be done many times after '-m w' )\n"
// "FILE (optional):\n"
// "  Upload the specified file as a blob into the table,\n"
// "  then download it to \"./testspeed.out\".\n"
// "  \"diff\" can then be used to verify the data.\n"
// "  -r -b -c parameterss are ignored.\n"
// "\n"
// ;

void
CDbapiTestSpeedApp::InitSample(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey(
        "g", "sss_server",
        "Server:port for gateway database driver\n"
        "(only one of -d/-g is required to be present)",
         CArgDescriptions::eString
    );

    arg_desc.AddDefaultKey(
        "r", "row_count",
        "Row count",
        CArgDescriptions::eInteger,
        "1"
    );

    arg_desc.AddDefaultKey(
        "b", "text_blob_size",
        "Text blob size in kilobytes",
        CArgDescriptions::eInteger,
        "1"
    );

    arg_desc.AddOptionalKey(
        "c", "column_count",
        "Column count  1..5 (int_val fl_val date_val str_val txt_val)",
        CArgDescriptions::eInteger
    );

    arg_desc.AddDefaultKey(
        "t", "table_name",
        "Table name",
        CArgDescriptions::eString,
        "#spd"
    );

    arg_desc.AddOptionalKey(
        "m", "mode_character",
        "Mode character \n"
        "     r  write and read as usual,\n"
        "        use CDB_Result->ReadItem() instead of GetItem() whenever possible\n"
        "     w  write to the table, do not read, do not delete it\n"
        "     R  read using ReadItem() (can be done many times after '-m w' )\n"
        "     G  read using GetItem()  (can be done many times after '-m w' )",
        CArgDescriptions::eString
    );
    arg_desc.SetConstraint("m", new CArgAllow_Symbols("rwRG"));

    arg_desc.AddOptionalKey(
        "FILE", "mode_character",
        "Upload the specified file as a blob into the table,\n"
        "then download it to \"./testspeed.out\" \n"
        "\"diff\" can then be used to verify the data.\n"
        "-r -b -c parameterss are ignored.\n",
        CArgDescriptions::eInputFile
    );
}

// Create a table with 5 columns, fill it using BCP or insert commands(1),
// fetch results(2), delete the table. Execution time is measured for steps 1, 2.
int
CDbapiTestSpeedApp::RunSample(void)
{
    string host_name;
    string port_num;
    int count = 0;
    int row_count = 1;
    int col_count = 5;
    int blob_size = 1;
    bool readItems = false;
    bool writeOnly = false;
    bool selectOnly = false;
    string fileParam;
    CStopWatch timer;
    double timeElapsed;

    const CArgs& args = GetArgs();

    // Process additional arguments ...

    //
    if (args["g"]) {
        host_name = args["g"].AsString();

        SIZE_TYPE i = NStr::Find(host_name, ":");
        if ( i > 0 ) {
            port_num = host_name.substr( i + 1 );
            host_name.resize(i);
        }

        if ( GetDriverName().empty() ) {
            SetDriverName("gateway");
        }
    }

    //
    row_count = args["r"].AsInteger();
    if ( row_count < 1  ||  row_count > 0x100000 ) {
        cerr << "Error -- invalid row count; valid values are 1 .. 1 Meg.\n";
        return 1;
    }

    //
    if (args["c"]) {
        col_count = args["g"].AsInteger();
    }

    //
    blob_size = args["b"].AsInteger();
    if ( blob_size < 1  ||  blob_size > 1024000 ) {
        cerr << "Error -- invalid blob size; valid values are 1 (kb) to 1000 (kb)\n";
        return 1;
    }
    if ( col_count < 5 ) {
        cerr << "Error -- blob size makes sense for '-c 5' only.\n";
        return 1;
    }

    //
    m_TableName = args["t"].AsString();

    //
    if (args["m"]) {
        string key_value = args["m"].AsString();

        switch ( key_value[0] ) {
        case 'r':
            readItems  = true;
            break;
        case 'w':
            writeOnly  = true;
            break;
        case 'G':
            selectOnly = true;
            break;
        case 'R':
            selectOnly = true;
            readItems  = true;
            break;
        default:
            cerr << "Error -- invalid mode character '" << key_value << "'\n";
            return 1;
        }
    }

    //
    if (args["FILE"]) {
        fileParam = args["FILE"].AsString();
    }

    // Modify connection atributes
    // Do it before establishing of a connection to a server ...
    if ( !host_name.empty() ) {
        SetDatabaseParameter("host", host_name);
    }
    if ( !port_num.empty() ) {
        SetDatabaseParameter("port", port_num);
    }

    try {
        auto_ptr<CDB_LangCmd> set_cmd;
        auto_ptr<CDB_LangCmd> ins_cmd;
        auto_ptr<CDB_BCPInCmd> bcp;

        string cmd_str("set textsize 1024000");
        set_cmd.reset(GetConnection().LangCmd(cmd_str));
        set_cmd->Send();
        while ( set_cmd->HasMoreResults() ) {
            auto_ptr<CDB_Result> r(set_cmd->Result());
        }

        // Create table, insert data
        if ( !selectOnly ) {
            // Deletes the pre-existing table, if present
            CreateTable(GetTableName());
            cout << ", rows " << row_count;
            cout << ", cols " << col_count;
            cout << ", blob size " << blob_size << "\n";

            if ( col_count > 4 ) {
                // "Bulk copy in" command
                bcp.reset(GetConnection().BCPIn(GetTableName()));
            } else {
                string s  = "insert into ";
                s += GetTableName();
                s += " (int_val";
                string sv = "@i";

                if ( col_count > 1 ) {
                    s += ", fl_val";
                    sv +=", @f";
                }
                if ( col_count > 2 ) {
                    s += ", date_val";
                    sv += ", @d";
                }
                if ( col_count > 3 ) {
                    s += ", str_val";
                    sv +=", @s";
                }

                s += ") values (";
                s += sv;
                s += ")";

                ins_cmd.reset(GetConnection().LangCmd(s));
            }

            CDB_Int int_val;
            CDB_Float fl_val;
            CDB_DateTime date_val(CTime::eCurrent);
            CDB_VarChar str_val;
            CDB_Text pTxt;
            int i;

            if ( !fileParam.empty() ) {
                CNcbiIfstream f(fileParam.c_str(), IOS_BASE::in |
                                IOS_BASE::binary);
                if ( !f.is_open() ) {
                    cerr << "Error -- cannot read '" << fileParam << "'\n";
                    return 1;
                }
                char buf[10240];
                size_t sz;
                while ( f.good() && !f.eof() ) {
                    f.read( buf, sizeof(buf) );
                    sz = (size_t)f.gcount();
                    if ( sz == 0 ) break;
                    pTxt.Append(buf, sz);
                    if ( sz != sizeof(buf) ) break;
                }
                f.close();

                col_count = 5;
                row_count = 1;
            } else if ( col_count > 4 ) {
                for ( i = 0; i < blob_size; ++i ) {
                    // Add 1024 chars
                    for ( int j = 0; j < 32; ++j ) {
                        pTxt.Append("If you want to know who we are--");
                    }
                }
            }

            timer.Start();

            // Bind program variables as data source
            if ( bcp.get() ) {
                bcp->Bind(0, &int_val);
                if ( col_count > 1 ) bcp->Bind(1, &fl_val  );
                if ( col_count > 2 ) bcp->Bind(2, &date_val);
                if ( col_count > 3 ) bcp->Bind(3, &str_val );
                if ( col_count > 4 ) bcp->Bind(4, &pTxt    );
            } else {
                if ( !ins_cmd->BindParam("@i", &int_val) ) {
                    cerr << "Error in BindParam()\n";
                    DeleteTable(GetTableName());
                    return 1;
                }

                if ( col_count > 1 ) ins_cmd->BindParam("@f", &fl_val  );
                if ( col_count > 2 ) ins_cmd->BindParam("@d", &date_val );
                if ( col_count > 3 ) ins_cmd->BindParam("@s", &str_val );
            }

            for ( i = 0; i < row_count; ++i ) {
                int_val  = i;
                fl_val   = static_cast<float>(i + 0.999);
                if ( !fileParam.empty() ) {
                    CDirEntry fileEntry(fileParam);
                    CTime fileTime;
                    fileEntry.GetTime(&fileTime);

                    date_val = fileTime;
                    str_val = fileParam;
                } else {
                    date_val = date_val.Value();
                    str_val  = string("Franz Joseph Haydn symphony # ") +
                        NStr::IntToString(i);
                }
                pTxt.MoveTo(0);

                if ( bcp.get() ) {
                    bcp->SendRow();

                    if ( count == 2 ) {
                        bcp->CompleteBatch();
                        count = 0;
                    }
                    ++count;
                } else {
                    ins_cmd->Send();
                    while ( ins_cmd->HasMoreResults() ) {
                        auto_ptr<CDB_Result> r(ins_cmd->Result());
                    }
                }
            }
            if ( bcp.get() ) {
                bcp->CompleteBCP();
            }
            timeElapsed = timer.Elapsed();
            cout << "inserting timeElapsed=" << NStr::DoubleToString(timeElapsed, 2) << "\n";
        } else {
            cout << "\n";
        }

        if ( !writeOnly ) {
            // Read data
            timer.Start();
            if ( !fileParam.empty() ) {
                FetchFile(GetTableName(), readItems);
            } else {
                FetchResults(GetTableName(), readItems);
            }
            timeElapsed = timer.Elapsed();
            cout << "fetching timeElapsed=" << NStr::DoubleToString(timeElapsed,2) << "\n";
            cout << "\n";
        }

        if ( !(selectOnly || writeOnly) ) {
            DeleteTable(GetTableName());
        }

        // Drop lost tables.
        DeleteLostTables();
    }
    catch ( CDB_Exception& e ) {
        CDB_UserHandler::GetDefault().HandleIt(&e);
        return 1;
    }

    return 0;
}

void
CDbapiTestSpeedApp::FetchFile(const string& table_name, bool readItems)
{
    CDB_VarChar str_val;
    CDB_DateTime date_val;

    string query = "select date_val,str_val,txt_val from ";
    query += table_name;
    auto_ptr<CDB_LangCmd> lcmd(GetConnection().LangCmd(query));
    lcmd->Send();

    //CTime fileTime;
    while ( lcmd->HasMoreResults() ) {
        auto_ptr<CDB_Result> r(lcmd->Result());
        if ( !r.get() ) continue;

        if ( r->ResultType() == eDB_RowResult ) {
            while ( r->Fetch() ) {
                CNcbiOfstream f("testspeed.out", IOS_BASE::trunc|IOS_BASE::out|IOS_BASE::binary);

                for ( unsigned int j = 0;  j < r->NofItems(); j++ ) {
                    EDB_Type rt = r->ItemDataType(j);

                    if ( readItems && rt == eDB_Text ) {
                        bool isNull=false;
                        char txt_buf[10240];
                        // cout<< "j=" << j
                        //    << " CurrentItemNo()=" << r->CurrentItemNo() << "\n";
                        for ( ;; ) {
                            size_t len_txt = r->ReadItem
                                (txt_buf, sizeof(txt_buf), &isNull);
                            //cout << "len_txt=" << len_txt << " isNull=" << isNull << "\n";
                            if ( isNull || len_txt<=0 ) break;
                            f.write(txt_buf, len_txt);
                        }
                        f.close();
                        continue;
                    }

                    // Type-specific GetItem()
                    if ( rt == eDB_Char || rt == eDB_VarChar ) {
                        r->GetItem(&str_val);

                    } else if ( rt == eDB_DateTime || rt == eDB_SmallDateTime ) {
                        r->GetItem(&date_val);
                    } else if ( rt == eDB_Text ) {
                        CDB_Text text_val;
                        r->GetItem(&text_val);

                        if ( text_val.IsNULL() ) {
                            // cout << "{NULL}";
                        } else {
                            char txt_buf[10240];
                            //cout << "text_val.Size()=" << text_val.Size() << "\n";
                            for ( ;; ) {
                                size_t len_txt
                                    = text_val.Read(txt_buf, sizeof(txt_buf));
                                if (len_txt == 0) break;
                                f.write(txt_buf, len_txt);
                            }
                        }
                        f.close();
                    } else {
                        r->SkipItem();
                        // cout << "{unprintable}";
                    }
                }
                // cout << "</ROW>" << endl << endl;
            }
        }
    }

    cout << "File " << str_val.AsString() << " dated "
        << date_val.Value().AsString() << " was written to testspeed.out using "
        << (readItems?"ReadItem":"GetItem") << "\n";
}

void
CDbapiTestSpeedApp::FetchResults (const string& table_name, bool readItems)
{
    // char* txt_buf = NULL ; // Temporary disabled ...
    // long len_txt = 0;


    string query = "select int_val,fl_val,date_val,str_val,txt_val from ";
    query += table_name;

    auto_ptr<CDB_LangCmd> lcmd(GetConnection().LangCmd(query));
    lcmd->Send();

    while ( lcmd->HasMoreResults() ) {
        auto_ptr<CDB_Result> r(lcmd->Result());
        if ( !r.get() ) continue;

        if ( r->ResultType() == eDB_RowResult ) {
            while ( r->Fetch() ) {
                // cout << "<ROW>"<< endl;
                for ( unsigned int j = 0;  j < r->NofItems(); ++j ) {
                    //    Determination of data type:
                    EDB_Type rt = r->ItemDataType(j);
                    const string iname = r->ItemName(j);

                    // cout << iname << '=';

                    if ( readItems && rt!=eDB_Numeric &&
                         rt != eDB_DateTime && rt != eDB_SmallDateTime ) {
                        bool isNull;
                        char buf[1024];
                        size_t sz=0;
                        while ( j == (unsigned int) r->CurrentItemNo() ) {
                            sz += r->ReadItem(buf, sizeof(buf), &isNull);
                        }
                        continue;
                    }

                    // Type-specific GetItem()
                    if ( rt == eDB_Char || rt == eDB_VarChar ) {
                        CDB_VarChar str_val;
                        r->GetItem(&str_val);
                        // cout << (str_val.IsNULL()? "{NULL}" : str_val.Value()) << endl << endl ;

                    } else if ( rt == eDB_Int || rt == eDB_SmallInt || rt == eDB_TinyInt ) {
                        CDB_Int int_val;
                        r->GetItem(&int_val);
                        if ( int_val.IsNULL() ) {
                            // cout << "{NULL}";
                        } else {
                            // cout << int_val.Value() << endl << endl ;
                        }
                    } else if ( rt == eDB_Float ) {
                        CDB_Float fl_val;
                        r->GetItem(&fl_val);
                        if ( fl_val.IsNULL() ) {
                            // cout << "{NULL}";
                        } else {
                            // cout << fl_val.Value() << endl<< endl ;
                        }
                    } else if ( rt == eDB_Double ) {
                        CDB_Double fl_val;
                        r->GetItem(&fl_val);
                        if ( fl_val.IsNULL() ) {
                            // cout << "{NULL}";
                        } else {
                            // cout << fl_val.Value() << endl<< endl ;
                        }
                    } else if ( rt == eDB_DateTime || rt == eDB_SmallDateTime ) {
                        CDB_DateTime date_val;
                        r->GetItem(&date_val);
                        if ( date_val.IsNULL() ) {
                            // cout << "{NULL}";
                        } else {
                            // cout << date_val.Value().AsString() << endl<< endl ;
                        }
                    } else if ( rt == eDB_Text ) {
                        CDB_Text text_val;
                        r->GetItem(&text_val);

                        if ( text_val.IsNULL() ) {
                            // cout << "{NULL}";
                        } else {
                            /* Do not do this at the moment ...
                            txt_buf = ( char*) malloc (text_val.Size() + 1);
                            len_txt = text_val.Read (( char*)txt_buf, text_val.Size());
                            txt_buf[text_val.Size()] = '\0';
                            */
                            // cout << txt_buf << endl<< endl ;
                        }
                    } else {
                        r->SkipItem();
                        // cout << "{unprintable}";
                    }
                }
                // cout << "</ROW>" << endl << endl;
            }
        }
    }
}

int main(int argc, char* argv[])
{
    // Execute main application function
    return CDbapiTestSpeedApp().AppMain(argc, argv);
}


