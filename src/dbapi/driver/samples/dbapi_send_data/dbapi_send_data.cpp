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
#include "../dbapi_sample_base.hpp"
#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

// This program will CREATE a table with 5 rows , UPDATE text field,
// PRINT table on screen (each row will begin with <ROW> and ended by </ROW>)
// and DELETE table from the database.

// The following function illustrates an update text in the table by using SendData();

/////////////////////////////////////////////////////////////////////////////
//  CDbapiTestBcpApp::
//

class CDbapiSendDataApp : public CDbapiSampleApp
{
public:
    CDbapiSendDataApp(void);
    virtual ~CDbapiSendDataApp(void);

protected:
    virtual int  RunSample(void);
    string GetTableName(void) const;
};

CDbapiSendDataApp::CDbapiSendDataApp(void)
{
}

CDbapiSendDataApp::~CDbapiSendDataApp(void)
{
}

inline
string
CDbapiSendDataApp::GetTableName(void) const
{
    return "#snd_test";
}

int
CDbapiSendDataApp::RunSample(void)
{
    try {
        auto_ptr<CDB_LangCmd> set_cmd;

        GetDriverContext().SetMaxBlobSize(1000000);

        // Create table in database for the test
        CreateTable(GetTableName());

        CDB_Text txt;

        txt.Append ("This text will replace a text in the table.");

        // Example: update text field in the table CursorSample where int_val=4,
        // by using function SendData()

        // Get a descriptor
        CDB_BlobDescriptor d(GetTableName(), "txt_val", "int_val = 4");

        // Update text field
        GetConnection().SendData(d, txt);

        // Print resutls on the screen
        ShowResults("select int_val,fl_val,date_val,str_val,txt_val from " + GetTableName());

        // Delete table from database
        DeleteTable(GetTableName());
    }
    catch ( CDB_Exception& e ) {
        CDB_UserHandler::GetDefault().HandleIt(&e);
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    // Execute main application function
    return CDbapiSendDataApp().AppMain(argc, argv);
}


