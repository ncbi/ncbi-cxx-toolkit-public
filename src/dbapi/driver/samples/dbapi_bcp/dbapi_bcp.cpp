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
* Author:  Elena Starchenko
*  $Id$
*
* File Description:
*   Test the implementation of BCP by DBAPI driver(s):
*     1) CREATE a table with 5 rows
*     2) make BCP of this table
*     3) PRINT the table on screen (use <ROW> and </ROW> to separate the rows)
*     4) DELETE the table from the database
*
* File Description: Implementation of dbapi BCP
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <dbapi/driver/interfaces.hpp>

#include "dbapi_bcp.hpp"
#include "../dbapi_sample_base.hpp"
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CDbapiBcpApp::
//

class CDbapiBcpApp : public CDbapiSampleApp
{
public:
    CDbapiBcpApp(void);
    virtual ~CDbapiBcpApp(void);

protected:
    virtual int  RunSample(void);
    string GetTableName(void) const;
};

CDbapiBcpApp::CDbapiBcpApp(void)
{
}

CDbapiBcpApp::~CDbapiBcpApp(void)
{
}

inline
string
CDbapiBcpApp::GetTableName(void) const
{
    return "#bcp_test";
}

int
CDbapiBcpApp::RunSample(void)
{
    int count=0;

    try {
        auto_ptr<CDB_LangCmd> set_cmd;

        // Create table in database for the test

        CreateTable(GetTableName());

        //Set textsize to work with text and image
        set_cmd.reset(GetConnection().LangCmd("set textsize 1000000"));
        set_cmd->Send();

        while(set_cmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(set_cmd->Result());
        }

        //"Bulk copy in" command
        auto_ptr<CDB_BCPInCmd> bcp(GetConnection().BCPIn(GetTableName()));

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

        for ( i= 0; *file_name[i] != '\0'; ++i ) {
            int_val = i;
            fl_val = float(i + 0.999);
            date_val = date_val.Value();
            str_val= file_name[i];

            pTxt.MoveTo(0);
            //Send row of data to the server
            bcp->SendRow();

            //Save any preceding rows in server
            if ( count == 2 ) {
                bcp->CompleteBatch();
                count = 0;
            }

            count++;
        }

        //End a bulk copy
        bcp->CompleteBCP();

        // Print results on screen
        ShowResults("select int_val,fl_val,date_val,str_val,txt_val from " + GetTableName());

        //Delete table from database
        DeleteTable(GetTableName());

        // Drop lost tables.
        DeleteLostTables();
    }
    catch ( CDB_Exception& e ) {
        CDB_UserHandler::GetDefault().HandleIt(&e);
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CDbapiBcpApp().AppMain(argc, argv);
}


