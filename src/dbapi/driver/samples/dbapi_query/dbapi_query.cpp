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
* Revision 1.7  2005/08/24 12:43:15  ssikorsk
* Use temporary table to store test data
*
* Revision 1.6  2004/12/20 16:20:29  ssikorsk
* Refactoring of dbapi/driver/samples
*
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
#include "../dbapi_sample_base.hpp"

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  CDbapiQueryApp::
//

class CDbapiQueryApp : public CDbapiSampleApp
{
public:
    CDbapiQueryApp(void);
    virtual ~CDbapiQueryApp(void);

protected:
    virtual int  RunSample(void);
    string GetTableName(void) const;
};

CDbapiQueryApp::CDbapiQueryApp(void)
{
}

CDbapiQueryApp::~CDbapiQueryApp(void)
{
}


inline
string
CDbapiQueryApp::GetTableName(void) const
{
    return "#query_test";
}

// The following function illustrates a dbapi language call
int
CDbapiQueryApp::RunSample(void)
{
    try {
        // Create table in database for the test
        CreateTable(GetTableName());

        //    Sending a query:
        ShowResults("select int_val,fl_val,date_val,str_val,txt_val from " +
            GetTableName());

        // Delete table from database
        DeleteTable(GetTableName());

        // Drop lost tables.
        DeleteLostTables();
    } catch ( CDB_Exception& e ) {
        CDB_UserHandler::GetDefault().HandleIt(&e);
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CDbapiQueryApp().AppMain(argc, argv);
}

