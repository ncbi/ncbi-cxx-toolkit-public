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
* File Description: Implementation of dbapi BCP 
* $Log$
* Revision 1.6  2004/05/17 21:17:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.5  2003/08/05 19:23:52  vakatov
* MSSQL2 --> MS_DEV1
*
* Revision 1.4  2003/01/30 16:08:11  soussov
* Adopt the new default DateTime constructor
*
* Revision 1.3  2002/12/09 16:25:19  starchen
* remove the text files from samples
*
* Revision 1.2  2002/09/04 22:20:39  vakatov
* Get rid of comp.warnings
*
* Revision 1.1  2002/07/18 15:48:21  starchen
* first entry
*
* Revision 1.2  2002/07/18 15:16:44  starchen
* first entry
*
*
*============================================================================
*/

#include <ncbi_pch.hpp>
#include "dbapi_bcp.hpp"
#include "dbapi_bcp_util.hpp"

USING_NCBI_SCOPE;

// This program will CREATE a table with 5 rows , make BCP,
// PRINT table on screen (each row will begin with <ROW> and ended by </ROW>)
// and DELETE table from the database.

//The following function illustrates a dbapi Bulk Copy (BCP) 

int main (int argc, char* argv[])
{
    int count=0;
    I_DriverContext* my_context;
    string server_name;
    string driver_name;
    const char* p = NULL;
    CDB_Connection* con;
	 CDB_BCPInCmd* bcp;

    // Read args from a command line:
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


    C_DriverMgr drv_mgr;
    string err_msg;

    if (driver_name == "dblib") {  

       // create attr for dblib:"version=100"
       // you have to set version if you need to work with BCP dblib Sybase 12.5

        dblib_version.insert (map<string, string>::value_type (string("version"),
                                                                         string("100")));
        my_context = drv_mgr.GetDriverContext (driver_name, &err_msg, &dblib_version);
    } else {
        my_context = drv_mgr.GetDriverContext (driver_name, &err_msg);
    }

    if (!my_context) {
        cout << "Can not load a driver " << driver_name << " [" 
             << err_msg << "] " << endl;
        return 1;
    }

    //  Connect to the server:
    try {
        con = my_context->Connect (server_name, "anyone",
                                              "allowed", I_DriverContext::fBcpIn);

        //  Change default database:
        CDB_LangCmd* set_cmd= con->LangCmd("use DBAPI_Sample");
        set_cmd->Send();
        CDB_Result* r;
        while(set_cmd->HasMoreResults()) {
	        r= set_cmd->Result();
	        if(r) delete r;
        }
        delete set_cmd;

        // Create table in database for the test
    
        CreateTable(con);

        //Set textsize to work with text and image
        set_cmd= con->LangCmd("set textsize 1000000");
        set_cmd->Send();

        while(set_cmd->HasMoreResults()) {
	        r= set_cmd->Result();
	        if(r) delete r;
        }
        delete set_cmd;

        //"Bulk copy in" command
 	     bcp = con->BCPIn("BcpSample", 5);

  	     CDB_Int int_val;
        CDB_Float fl_val;
        CDB_DateTime date_val(CTime::eCurrent);
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
            //Send row of data to the server
	         bcp->SendRow();
 
            //Save any preceding rows in server
	         if (count == 2) {
	        	    bcp->CompleteBatch();
                count = 0;
	         }

            count++;
	     }
        //End a bulk copy 
        bcp->CompleteBCP();

        // Print results on screen
        ShowResults(con);

        //Delete table from database
        DeleteTable(con); 
	     delete bcp;
	     delete con;    	
    } catch (CDB_Exception& e) {
	   HandleIt(&e);
      DeleteTable(con);
      return 1;
    }

    return 0;
}
    








