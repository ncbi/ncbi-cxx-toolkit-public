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
*     Misc. functions for test program that measures dbapi driver speed
*     (adapted from dbapi_bcp_util.cpp).
*
*============================================================================
*/

#include <ncbi_pch.hpp>
#include "dbapi_testspeed.hpp"
#include <string>


const char* prnType(EDB_Type t)
{
  switch(t) {
    case eDB_Int           : return "Int4";
    case eDB_SmallInt      : return "Int2";
    case eDB_TinyInt       : return "Uint1";
    case eDB_BigInt        : return "Int8";
    case eDB_VarChar       : return "VarChar";
    case eDB_Char          : return "Char";
    case eDB_VarBinary     : return "VarBinary";
    case eDB_Binary        : return "Binary";
    case eDB_Float         : return "Float";
    case eDB_Double        : return "Double";
    case eDB_DateTime      : return "DateTime";
    case eDB_SmallDateTime : return "SmallDateTime";
    case eDB_Text          : return "Text";
    case eDB_Image         : return "Image";
    case eDB_Bit           : return "Bit";
    case eDB_Numeric       : return "Numeric";
    default                : return "Unsupported";
  }
}

const char* prnSeverity(EDB_Severity s)
{
  switch(s) {
    case eDB_Info:    return "Info";
    case eDB_Warning: return "Warning";
    case eDB_Error:   return "Error";
    case eDB_Fatal:   return "Fatal Error";
    default:          return "Unknown severity";
  }
}

void prnDSEx(CDB_DSEx* ex)
{
  cerr << prnSeverity(ex->Severity()) << " Err.code=" << ex->ErrCode()
       << " Data source: " << ex->OriginatedFrom()
       << " \t| <" << ex->Message()<< ">" << endl;
}

static void prnRPCEx(CDB_RPCEx* ex)
{
  cerr << prnSeverity(ex->Severity()) << " Err code=" << ex->ErrCode()
       << " SQL/Open server: " << ex->OriginatedFrom()
       << " Procedure: " << ex->ProcName() << " Line: " << ex->ProcLine()
       << " \t| <" << ex->Message() << ">" << endl;
}

static void prnSQLEx(CDB_SQLEx* ex)
{
  cerr << prnSeverity(ex->Severity()) << " Err code=" << ex->ErrCode()
       << " SQL server: " << ex->OriginatedFrom() << " SQL: " << ex->SqlState()
       << " Line: " << ex->BatchLine()
       << " \t| <" << ex->Message() << ">" << endl;
}

static void prnDeadlockEx(CDB_DeadlockEx* ex)
{
  cerr << prnSeverity(ex->Severity()) << " SQL server: " << ex->OriginatedFrom()
       << " \t| <" << ex->Message() << ">" << endl;
}

static void prnTimeoutEx(CDB_TimeoutEx* ex)
{
  cerr << prnSeverity(ex->Severity()) << " SQL server: " << ex->OriginatedFrom()
       << " \t| <" << ex->Message() << ">" << endl;
}

static void prnCliEx(CDB_ClientEx* ex)
{
  cerr << prnSeverity(ex->Severity()) << " (Err.code=" << ex->ErrCode()
       << ") \tSource: " << ex->OriginatedFrom() << " \n <<<"
       << ex->Message() << ">>>" << endl;
}

bool HandleIt(const CDB_Exception* ex)
{
  switch(ex->Type()) {
    case CDB_Exception::eDS       : prnDSEx((CDB_DSEx*)ex);             break;
    case CDB_Exception::eRPC      : prnRPCEx((CDB_RPCEx*)ex);           break;
    case CDB_Exception::eSQL      : prnSQLEx((CDB_SQLEx*)ex);           break;
    case CDB_Exception::eDeadlock : prnDeadlockEx((CDB_DeadlockEx*)ex); break;
    case CDB_Exception::eTimeout  : prnTimeoutEx((CDB_TimeoutEx*)ex);   break;
    case CDB_Exception::eClient   : prnCliEx((CDB_ClientEx*)ex);        break;
    default:
      cerr << prnSeverity(ex->Severity())
           << " message: Error code=" << ex->ErrCode()
           << " \t| <" << ex->what() << ">" << endl;

  }
  return true;
}


// Read a command line argument
char* getParam(char tag, int argc, char* argv[], bool* flag)
{
  static int last_processed=0;
  if(tag=='\0') {
    // Return the next positional argument
    if(last_processed>=argc-1) return 0;
    return argv[++last_processed];
  }

  for(int i= 1; i < argc; i++) {
    if( (argv[i][0] == '-' || argv[i][0] == '/') && argv[i][1] == tag ) {
      // tag found
      if(flag) *flag= true;

      char* res=0;
      if(*(argv[i]+2) == '\0')  {
        // tag is a separate arg
        if( i <= argc - 1 && argv[i+1][0] != argv[i][0] ) res=argv[++i];
      }
      else {
        res=argv[i]+2;
      }

      if(last_processed<i) last_processed=i;
      return res;
    }
  }
  if(flag) *flag= false;
  return 0;
}

// Also drops any pre-existing table.
int CreateTable (CDB_Connection* con, const string& table_name)
{
  try {
    string s = "IF EXISTS(select * from sysobjects WHERE name = '";
    s+= table_name;
    s+= "' AND type = 'U') begin DROP TABLE ";
    s+= table_name;
    s+= " end";
    CDB_LangCmd* lcmd = con->LangCmd(s);
    lcmd->Send();

    while(lcmd->HasMoreResults()) {
      CDB_Result* r = lcmd->Result();
      if(r) delete r;
    }
    delete lcmd;

    s = "create table ";
    s+= table_name;
    s+= "(int_val  int      not null,"
        " fl_val   real         null,"
        " date_val datetime     null,"
        " str_val  varchar(255) null,"
        " txt_val  text         null)";
    lcmd = con->LangCmd(s);
    lcmd->Send();

    while (lcmd->HasMoreResults()) {
      CDB_Result* r = lcmd->Result();
      if(r) delete r;
    }
    delete lcmd;
  }
  catch (CDB_Exception& e) {
    HandleIt(&e);
    return 1;
  }
  return 0;
}

int FetchResults (CDB_Connection* con, const string& table_name, bool readItems)
{
  char* txt_buf = NULL ;
  long len_txt = 0;


  try {
    string query = "select int_val,fl_val,date_val,str_val,txt_val from ";
    query+=table_name;
    CDB_LangCmd* lcmd = con->LangCmd(query);
    lcmd->Send();

    while (lcmd->HasMoreResults()) {
      CDB_Result* r = lcmd->Result();
      if (!r) continue;

      if (r->ResultType() == eDB_RowResult) {
        while (r->Fetch()) {
          // cout << "<ROW>"<< endl;
          for (unsigned int j = 0;  j < r->NofItems(); j++) {
            //    Determination of data type:
            EDB_Type rt = r->ItemDataType(j);
            const char* iname= r->ItemName(j);

            //    Printing to stdout:
            if(iname == 0) iname= "";
            // cout << iname << '=';

            if( readItems && rt!=eDB_Numeric &&
                rt!=eDB_DateTime && rt!=eDB_SmallDateTime )
            {
              bool isNull;
              char buf[1024];
              int sz=0;
              while( j == r->CurrentItemNo() ) {
                sz+=r->ReadItem(buf, sizeof(buf), &isNull);
              }
              // cout << j << " " << sz << (j == r->NofItems()-1 ? "\n" : ", ");
              continue;
            }

            // Type-specific GetItem()
            if (rt == eDB_Char || rt == eDB_VarChar) {
              CDB_VarChar str_val;
              r->GetItem(&str_val);
              // cout << (str_val.IsNULL()? "{NULL}" : str_val.Value()) << endl << endl ;

            }
            else if (rt == eDB_Int || rt == eDB_SmallInt || rt == eDB_TinyInt) {
              CDB_Int int_val;
              r->GetItem(&int_val);
              if (int_val.IsNULL()) {
                // cout << "{NULL}";
              }
              else {
                // cout << int_val.Value() << endl << endl ;
              }
            }
            else if (rt == eDB_Float) {
              CDB_Float fl_val;
              r->GetItem(&fl_val);
              if(fl_val.IsNULL()) {
                // cout << "{NULL}";
              }
              else {
                // cout << fl_val.Value() << endl<< endl ;
              }
            }
            else if (rt == eDB_Double) {
              CDB_Double fl_val;
              r->GetItem(&fl_val);
              if(fl_val.IsNULL()) {
                // cout << "{NULL}";
              }
              else {
                // cout << fl_val.Value() << endl<< endl ;
              }
            }
            else if (rt == eDB_DateTime || rt == eDB_SmallDateTime) {
              CDB_DateTime date_val;
              r->GetItem(&date_val);
              if(date_val.IsNULL()) {
                // cout << "{NULL}";
              }
              else {
                // cout << date_val.Value().AsString() << endl<< endl ;
              }
            }
            else if(rt == eDB_Text) {
              CDB_Text text_val;
              r->GetItem(&text_val);

              if(text_val.IsNULL()) {
                // cout << "{NULL}";
              }
              else {
                txt_buf = ( char*) malloc (text_val.Size());
                len_txt = text_val.Read (( char*)txt_buf, text_val.Size());
                txt_buf[text_val.Size()] = '\0';
                // cout << txt_buf << endl<< endl ;
              }
            }
            else {
              r->SkipItem();
              // cout << "{unprintable}";
            }
          }
          // cout << "</ROW>" << endl << endl;
        }
        delete r;
      }
    }
    delete lcmd;
  }
  catch (CDB_Exception& e) {
     HandleIt(&e);
    return 1;
  }
  return 0;
}

int FetchFile(CDB_Connection* con, const string& table_name, bool readItems)
{
  CDB_VarChar str_val;
  CDB_DateTime date_val;

  try {
    string query = "select date_val,str_val,txt_val from ";
    query+=table_name;
    CDB_LangCmd* lcmd = con->LangCmd(query);
    lcmd->Send();

    //CTime fileTime;
    while (lcmd->HasMoreResults()) {
      CDB_Result* r = lcmd->Result();
      if (!r) continue;

      if (r->ResultType() == eDB_RowResult) {
        while (r->Fetch()) {
          CNcbiOfstream f("testspeed.out", IOS_BASE::trunc|IOS_BASE::out|IOS_BASE::binary);

          for (unsigned int j = 0;  j < r->NofItems(); j++) {
            EDB_Type rt = r->ItemDataType(j);
            const char* iname= r->ItemName(j);

            if( readItems && rt == eDB_Text )
            {
              bool isNull=false;
              char txt_buf[10240];
              // cout<< "j=" << j
              //    << " CurrentItemNo()=" << r->CurrentItemNo() << "\n";
              for(;;) {
                int len_txt = r->ReadItem(txt_buf, sizeof(txt_buf), &isNull);
                //cout << "len_txt=" << len_txt << " isNull=" << isNull << "\n";
                if(isNull || len_txt<=0) break;
                f.write(txt_buf, len_txt);
              }
              f.close();
              continue;
            }

            // Type-specific GetItem()
            if (rt == eDB_Char || rt == eDB_VarChar) {
              r->GetItem(&str_val);

            }
            else if (rt == eDB_DateTime || rt == eDB_SmallDateTime) {
              r->GetItem(&date_val);
            }
            else if(rt == eDB_Text) {
              CDB_Text text_val;
              r->GetItem(&text_val);

              if(text_val.IsNULL()) {
                // cout << "{NULL}";
              }
              else {
                char txt_buf[10240];
                //cout << "text_val.Size()=" << text_val.Size() << "\n";
                for(;;) {
                  int len_txt = text_val.Read( txt_buf, sizeof(txt_buf) );
                  if(len_txt<=0) break;
                  f.write(txt_buf, len_txt);
                }
              }
              f.close();
            }
            else {
              r->SkipItem();
              // cout << "{unprintable}";
            }
          }
          // cout << "</ROW>" << endl << endl;
        }
        delete r;
      }
    }
    delete lcmd;
  }
  catch (CDB_Exception& e) {
     HandleIt(&e);
    return 1;
  }

  cout<< "File " << str_val.Value() << " dated " << date_val.Value().AsString()
      << " was written to testspeed.out using "
      << (readItems?"ReadItem":"GetItem") << "\n";
  return 0;
}

int DeleteTable (CDB_Connection* con, const string& table_name)
{
  try {
    string s = "drop table ";
    s+= table_name;

    CDB_LangCmd* lcmd = con->LangCmd(s);
    lcmd->Send();

    while(lcmd->HasMoreResults()) {
      CDB_Result* r = lcmd->Result();
      if(r) delete r;
    }
    delete lcmd;
  }
  catch (CDB_Exception& e) {
    HandleIt(&e);
    return 1;
  }
  return 0;
}


