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
* File Description: Implementation of dbapi language call
* $Log$
* Revision 1.5  2004/05/17 21:17:29  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.4  2003/08/05 19:23:58  vakatov
* MSSQL2 --> MS_DEV1
*
* Revision 1.3  2002/12/09 16:25:22  starchen
* remove the text files from samples
*
* Revision 1.2  2002/09/04 22:20:41  vakatov
* Get rid of comp.warnings
*
* Revision 1.1  2002/07/18 15:49:39  starchen
* first entry
*
*
*============================================================================
*/

#include <ncbi_pch.hpp>
#include <dbapi/driver/driver_mgr.hpp>

char* getParam(char tag, int argc, char* argv[], bool* flag= 0);

USING_NCBI_SCOPE;

//The following function illustrates a dbapi language call

int main(int argc, char* argv[])
{
    string query;
    I_DriverContext* my_context;
    string server_name;
    string driver_name;
    const char* p = NULL;
    char* txt_buf = NULL ;
    long len_txt = 0;

    
    //    Read args from a command line:
 
    p= getParam('S', argc, argv);
    if(p) server_name = p;

    p= getParam('d', argc, argv);
    if(p) driver_name = p;

    if ( p == NULL) {
       cout << endl << "usage: for Sybase dblib: -S STRAUSS -d dblib" << endl 
            << "for Sybase ctlib: -S STRAUSS -d ctlib" << endl
            << "for MSSQL: -S MS_DEV1 -d ftds" << endl << endl;
    }
    
    //    Driver Manager allowes you to change a type of SQL server
    //    without rebuilding your program.
    //    Driver's types:
    //         ctlib - to work with Sybase SQL Server (ct_ library);
    //         dblib - to work with Sybase SQL Server (db library);
    //         ftds  - to work with MS SQL Server.
    

    try {
        C_DriverMgr drv_mgr;
        string err_msg;
        my_context = drv_mgr.GetDriverContext (driver_name, &err_msg);

        if(!my_context) {
            cout << "Can not load a driver " << driver_name << " [" 
                 << err_msg << "] " << endl;
            return 1;
        }

        //    Connect to the server:
        CDB_Connection* con = my_context->Connect (server_name, "anyone", "allowed", 0);
       
        //    Change default database:
        CDB_LangCmd* lcmd = con->LangCmd("use DBAPI_Sample");
        lcmd->Send();
        while (lcmd->HasMoreResults()) {
            CDB_Result* r = lcmd->Result();
            if (!r) {
                continue;
            }
            delete r;
        }

        //    Sending a query:

        query = "select int_val,fl_val,date_val,str_val,txt_val from SelectSample";
        lcmd = con->LangCmd(query);
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

                     //    Fetching of char or varchar data type:
                     //    note that object type of CDB_Char is for a 1 -byte char and
                     //    object type of CDB_VarChar can't be more then 255 bytes.

                     if (rt == eDB_Char || rt == eDB_VarChar) {
                         CDB_VarChar str_val;
                         r->GetItem(&str_val);
                         cout << (str_val.IsNULL()? "{NULL}" : str_val.Value()) << endl << endl ;

                     //    Fetching of integer data type:
                     //    note that object type of CDB_Int is for a 4- bytes int
                     //    object type of CDB_SmallInt is for the 2- bytes int
                     //    object type of CDB_TinyInt is for the 1- byte int.

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


                      //    Fetching of real data type:  
                      //    note that  
                      //    object type of CDB_Float is for the 4-bytes float data type

                      } else if (rt == eDB_Float) {

                            CDB_Float fl_val;
                            r->GetItem(&fl_val);
                            if(fl_val.IsNULL()) {
                                cout << "{NULL}";
                            } else {
                                cout << fl_val.Value() << endl<< endl ;
                            }

                      //    Fetching of float data type:  
                      //    note that  
                      //    object type of CDB_Double is for a 8-byte float data type.

                      } else if (rt == eDB_Double) {

                            CDB_Double fl_val;
                            r->GetItem(&fl_val);
                            if(fl_val.IsNULL()) {
                                cout << "{NULL}";
                            } else {
                                cout << fl_val.Value() << endl<< endl ;
                            }

                      //    Fetching of datetime data type:
                      //    note that object of type CDB_DateTime is for a datetime4 (8 bytes) data type
                      //    object type of CDB_SmallDateTime is for a datetime (4 bytes) data type.

                      } else if (rt == eDB_DateTime ||
                                 rt == eDB_SmallDateTime) {

                            CDB_DateTime date_val;
                            r->GetItem(&date_val);
                            if(date_val.IsNULL()) {
                                cout << "{NULL}";
                            } else {
                                cout << date_val.Value().AsString() << endl<< endl ;
                            }
 
                       //    Fetching of text data type:
                       //    if you need to fetch an image 
                       //    you have to use CDB_Image type of object

                       } else if (rt == eDB_Text){

                            CDB_Text text_val;
                            r->GetItem(&text_val);

                            if(text_val.IsNULL()) {
                                cout << "{NULL}";
                            } else {
                                txt_buf = ( char*) malloc (text_val.Size());
                                len_txt = text_val.Read (( char*)txt_buf, text_val.Size());
                                cout << txt_buf << endl<< endl ;
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
        delete con;
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);        
        myExHandler.HandleIt(&e);
        return 1;
    }
    return 0;
}

