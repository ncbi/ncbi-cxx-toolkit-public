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
*/

#include <ncbi_pch.hpp>
#include "dbapi_send_data.hpp"
#include <map> 

map<string, string> dblib_version;


USING_NCBI_SCOPE;

// This program will CREATE a table with 5 rows , UPDATE text field,
// PRINT table on screen (each row will begin with <ROW> and ended by </ROW>)
// and DELETE table from the database.

// The following function illustrates an update text in the table by using SendData();

int main(int argc, char* argv[])
{
   
    I_DriverContext* my_context;
    string server_name;
    string driver_name;
    const char* p = NULL;
    CDB_Connection* con;
    C_DriverMgr drv_mgr;
    string err_msg;

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


    if (driver_name == "dblib") {    
      
       // create attr for dblib:"version=100"
       // you have to set version if you need to work with BCP dblib Sybase 12.5
       // in this program BCP used to populate a table

       dblib_version.insert (map<string, string>::value_type (string("version"), 
                                                                     string("100")));
       my_context = drv_mgr.GetDriverContext (driver_name, &err_msg, &dblib_version);
    } else {
       my_context = drv_mgr.GetDriverContext (driver_name, &err_msg);
    }
    // Change a default size of text(image)
    my_context->SetMaxTextImageSize(1000000);
    if (!my_context) {
            cout << "Can not load a driver " << driver_name << " [" 
                 << err_msg << "] " << endl;
            return 1;
    }

    try {
        //    Connect to the server:
        con = my_context->Connect (server_name, "anyone", "allowed",
                                                I_DriverContext::fBcpIn);

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

        CDB_Text txt;

        txt.Append ("This text will replace a text in the table.");

        // Example: update text field in the table CursorSample where int_val=4,
        // by using function SendData() 
       
        // Get a descriptor
        CDB_ITDescriptor d ("CursorSample", "text_val", "int_val = 4");

        // Update text field
        con->SendData(d, txt);

        // Print resutls on the screen
        ShowResults(con);
      
        // Delete table from database
        DeleteTable(con);
	     delete con;
    	
    } catch (CDB_Exception& e) {
	     HandleIt(&e);
        DeleteTable(con);
        return 1;
    }

    return 0;
}
