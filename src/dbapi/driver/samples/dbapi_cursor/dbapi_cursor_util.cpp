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
* File Name:  $Id$
*
* File Description: Implementation of dbapi bcp
* $Log$
* Revision 1.6  2004/05/17 21:17:16  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.5  2003/02/27 20:21:35  starchen
* correct a lang call
*
* Revision 1.4  2002/12/09 16:25:21  starchen
* remove the text files from samples
*
* Revision 1.3  2002/09/04 22:20:40  vakatov
* Get rid of comp.warnings
*
* Revision 1.2  2002/07/18 19:51:04  starchen
* fixed some error
*
* Revision 1.1  2002/07/18 15:47:42  starchen
* first entry
*
* Revision 1.1  2002/07/18 15:18:02  starchen
* first entry
*
*
*============================================================================
*/

#include <ncbi_pch.hpp>
#include "dbapi_cursor.hpp"

const char*  file_name[] = {
    "../test1.txt",
    "../test2.txt",
    "../test3.txt",
    "../test4.txt",
    "../test5.txt",
    ""
};



 const char* prnType(EDB_Type t)
{
    switch(t) {
    case eDB_Int: return "Int4";
    case eDB_SmallInt: return "Int2";
    case eDB_TinyInt:  return "Uint1";
    case eDB_BigInt:   return "Int8";
    case eDB_VarChar : return "VarChar";
    case eDB_Char:     return "Char";
    case eDB_VarBinary: return "VarBinary";
    case eDB_Binary :   return "Binary";
    case eDB_Float : return "Float";
    case eDB_Double: return "Double";
    case eDB_DateTime : return "DateTime";
    case eDB_SmallDateTime : return "SmallDateTime";
    case eDB_Text : return "Text";
    case eDB_Image : return "Image";
    case eDB_Bit : return "Bit";
    case eDB_Numeric : return "Numeric";
    default: return "Unsupported";
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
    cerr << "Sybase " << prnSeverity(ex->Severity()) << " Message: Err.code=" << ex->ErrCode() <<
	" Data source: " << ex->OriginatedFrom() << " \t| <" << ex->Message() << ">" << endl;
}

static void prnRPCEx(CDB_RPCEx* ex)
{
    cerr << "Sybase " << prnSeverity(ex->Severity()) << " Message: Err code=" << ex->ErrCode() <<
	" SQL/Open server: " << ex->OriginatedFrom() << " Procedure: " << ex->ProcName() << " Line: " << ex->ProcLine() <<
	" \t| <" << ex->Message() << ">" << endl;
}

static void prnSQLEx(CDB_SQLEx* ex)
{
    cerr << "Sybase " << prnSeverity(ex->Severity()) << " Message: Err code=" << ex->ErrCode() <<
	" SQL server: " << ex->OriginatedFrom() << " SQL: " << ex->SqlState() << " Line: " << ex->BatchLine() <<
	" \t| <" << ex->Message() << ">" << endl;
}

static void prnDeadlockEx(CDB_DeadlockEx* ex)
{
    cerr << "Sybase " << prnSeverity(ex->Severity()) << " Message: SQL server: " << ex->OriginatedFrom() << 
	" \t| <" << ex->Message() << ">" << endl;
}

static void prnTimeoutEx(CDB_TimeoutEx* ex)
{
    cerr << "Sybase " << prnSeverity(ex->Severity()) << " Message: SQL server: " << ex->OriginatedFrom() << 
	" \t| <" << ex->Message() << ">" << endl;
}

static void prnCliEx(CDB_ClientEx* ex)
{
    cerr << "Sybase " << prnSeverity(ex->Severity()) << " Message: (Err.code=" << ex->ErrCode() <<
	") \tSource: " << ex->OriginatedFrom() << " \n <<<" << ex->Message() << ">>>" << endl;
}

 bool HandleIt(const CDB_Exception* ex) 
{
    switch(ex->Type()) {
    case CDB_Exception::eDS :
	prnDSEx((CDB_DSEx*)ex);
	break;
    case CDB_Exception::eRPC :
	prnRPCEx((CDB_RPCEx*)ex);
	break;
    case CDB_Exception::eSQL :
	prnSQLEx((CDB_SQLEx*)ex);
	break;
    case CDB_Exception::eDeadlock :
	prnDeadlockEx((CDB_DeadlockEx*)ex);
	break;
    case CDB_Exception::eTimeout :
	prnTimeoutEx((CDB_TimeoutEx*)ex);
	break;
    case CDB_Exception::eClient :
	prnCliEx((CDB_ClientEx*)ex);
	break;
    default:
	cerr << "Sybase " << prnSeverity(ex->Severity()) << " message: Error code=" << ex->ErrCode() << " \t| <" <<
	    ex->what() << ">" << endl;
	
    }
    return true;
}



// getParam is reading arguments from the command line

 char* getParam(char tag, int argc, char* argv[], bool* flag)
{
    for(int i= 1; i < argc; i++) {
        if(((*argv[i] == '-') || (*argv[i] == '/')) && 
           (*(argv[i]+1) == tag)) { // tag found
            if(flag) *flag= true;
            if(*(argv[i]+2) == '\0')  { // tag is a separate arg
                if(i == argc - 1) return 0;
                if(*argv[i+1] != *argv[i]) return argv[i+1];
                else return 0;
            }
            else return argv[i]+2;
        }
    }
    if(flag) *flag= false;
    return 0;
}

// function CreateTable is creating table in the database
//   and populate it by using BCP

int CreateTable (CDB_Connection* con)
{
   CDB_BCPInCmd* bcp;

   try {
 
       CDB_LangCmd* lcmd = con->LangCmd ( 
                     " IF EXISTS(select * from sysobjects WHERE name = 'CursorSample'"
                       " AND   type = 'U') begin "
                       " DROP TABLE CursorSample end ");
       lcmd->Send();

       while (lcmd->HasMoreResults()) {
            CDB_Result* r = lcmd->Result();
            if (!r) {
                continue;
            }
            delete r;
        }
        delete lcmd;

        lcmd = con->LangCmd (  " create table CursorSample"
                     " (int_val int not null,fl_val real not null,"
                     " date_val datetime not null ,str_val varchar(255) null,"
                     " text_val text null, primary key clustered(int_val))");
        lcmd->Send();

        while (lcmd->HasMoreResults()) {
            CDB_Result* r = lcmd->Result();
            if (!r) {
                continue;
            }
            delete r;
        }
        delete lcmd;
	     bcp = con->BCPIn("CursorSample", 5);

  	     CDB_Int int_val;
        CDB_Float fl_val;
        CDB_DateTime date_val;
        CDB_VarChar str_val;
        CDB_Text pTxt;
	     int i;
        pTxt.Append("This is a test string.");

        // Bind data from a program variables  
        bcp->Bind(0, &int_val);
        bcp->Bind(1, &fl_val);
        bcp->Bind(2, &date_val);
        bcp->Bind(3, &str_val);
        bcp->Bind(4, &pTxt);

	     for(i= 0; *file_name[i] != '\0'; i++) {
            int_val = i;
            fl_val = i + 0.999;
            date_val = date_val.Value();
            str_val= file_name[i];
            pTxt.MoveTo(0);
	       
	         bcp->SendRow();
	         
	     }
        bcp->CompleteBCP();
	     delete bcp;  
   }  catch (CDB_Exception& e) {
	   HandleIt(&e);
      return 1;
   }
   return 0;
}

//ShowResults is printing resuts on screen

int ShowResults (CDB_Connection* con)
{
    char* txt_buf = NULL ;
    long len_txt = 0;


    try {   
        string query = "select int_val,fl_val,date_val,str_val,text_val from CursorSample";
        CDB_LangCmd* lcmd = con->LangCmd(query);
        lcmd->Send();

        //    Fetching  results:

        while (lcmd->HasMoreResults()) {
            CDB_Result* r = lcmd->Result();
            if (!r) continue;
            
            if (r->ResultType() == eDB_RowResult) {
                while (r->Fetch()) {
                   cout << "<ROW>"<< endl;
                   for (unsigned int j = 0;  j < r->NofItems(); j++) {

                     //    Determination of data type:
                     EDB_Type rt = r->ItemDataType(j);
                     const char* iname= r->ItemName(j);

                     //    Printing to stdout:
                     if(iname == 0) iname= "";
                     cout  << iname << '=';

                     if (rt == eDB_Char || rt == eDB_VarChar) {
                         CDB_VarChar str_val;
                         r->GetItem(&str_val);
                         cout << (str_val.IsNULL()? "{NULL}" : str_val.Value()) << endl << endl ;

                     } else if (rt == eDB_Int ||
                                rt == eDB_SmallInt ||
                                rt == eDB_TinyInt) {

                         CDB_Int int_val;
                         r->GetItem(&int_val);
                         if (int_val.IsNULL()) {
                             cout << "{NULL}";
                         } else {
                             cout << int_val.Value() << endl << endl ;
                         }
                      } else if (rt == eDB_Float) {

                            CDB_Float fl_val;
                            r->GetItem(&fl_val);
                            if(fl_val.IsNULL()) {
                                cout << "{NULL}";
                            } else {
                                cout << fl_val.Value() << endl<< endl ;
                            }
                      } else if (rt == eDB_Double) {

                            CDB_Double fl_val;
                            r->GetItem(&fl_val);
                            if(fl_val.IsNULL()) {
                                cout << "{NULL}";
                            } else {
                                cout << fl_val.Value() << endl<< endl ;
                            }
                       } else if (rt == eDB_DateTime ||
                                  rt == eDB_SmallDateTime) {

                            CDB_DateTime date_val;
                            r->GetItem(&date_val);
                            if(date_val.IsNULL()) {
                                cout << "{NULL}";
                            } else {
                                cout << date_val.Value().AsString() << endl<< endl ;
                            }
                        } else if (rt == eDB_Text){

                            CDB_Text text_val;
                            r->GetItem(&text_val);

                            if(text_val.IsNULL()) {
                                cout << "{NULL}";
                            } else {
                                txt_buf = ( char*) malloc (text_val.Size()+1);
                                len_txt = text_val.Read (( char*)txt_buf, text_val.Size());
                                txt_buf[text_val.Size()] = '\0'; 
                                cout << txt_buf << endl << endl ;
                            }
                        } else {
                            r->SkipItem();
                            cout << "{unprintable}";
                        }
                    }
                    cout << "</ROW>" << endl << endl;
                }
                delete r;
            }
        }
        delete lcmd;
    } catch (CDB_Exception& e) {
	   HandleIt(&e);
      return 1;
    }
    return 0;
}

//DeleteTable is destroying a table after program finished to work with it
int DeleteTable (CDB_Connection* con)
{
    try {
        CDB_LangCmd* lcmd = con->LangCmd("drop table CursorSample");
        lcmd->Send();

        while (lcmd->HasMoreResults()) {
            CDB_Result* r = lcmd->Result();
            if (!r) {
                continue;
            }
            delete r;
        }
        delete lcmd;
     } catch (CDB_Exception& e) {
	   HandleIt(&e);
      return 1;
     }
    return 0;
}


