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
 * Author:  Vladimir Soussov
 *
 * Sample procedures to illustrate how to use language and RPC commands
 *
 */

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/interfaces.hpp>
#include <iostream>


BEGIN_NCBI_SCOPE


void SampleDBAPI_Lang(I_DriverContext& context, const string& server_name)
{
    CDB_Connection* con = context.Connect(server_name, "anyone", "allowed", 0);

    CDB_LangCmd* lcmd =
        con->LangCmd("select name, crdate from sysdatabases");
    lcmd->Send();

    while (lcmd->HasMoreResults()) {
        CDB_Result* r = lcmd->Result();
        if (!r)
            continue;
        cout
            << r->ItemName(0) << " \t\t\t"
            << r->ItemName(1) << endl
            << "-----------------------------------------------------" << endl;

        while (r->Fetch()) {
            CDB_Char dbname(24);
            CDB_DateTime crdate;

            r->GetItem(&dbname);
            r->GetItem(&crdate);

            cout
                << dbname.Value() << ' '
                << crdate.Value().AsString("M/D/Y h:m") << endl;
        }
        delete r;
    }
    delete lcmd;
    delete con;
}


void SampleDBAPI_SpWho(I_DriverContext& context, const string& server_name)
{
    CDB_Connection* con = context.Connect(server_name, "anyone", "allowed", 0);

    CDB_RPCCmd* rcmd = con->RPC("sp_who", 0);
    rcmd->Send();

    while (rcmd->HasMoreResults()) {
        CDB_Result* r = rcmd->Result();
        if (!r)
            continue;

        if (r->ResultType() == eDB_RowResult) {
            while (r->Fetch()) {
                for (unsigned int j = 0;  j < r->NofItems(); j++) {
                    EDB_Type rt = r->ItemDataType(j);
                    if (rt == eDB_Char || rt == eDB_VarChar) {
                        CDB_VarChar r_vc;
                        r->GetItem(&r_vc);
                        cout << r->ItemName(j) << ": "
                             << (r_vc.IsNULL()? "" : r_vc.Value()) << " \t";
                    } else if (rt == eDB_Int ||
                               rt == eDB_SmallInt ||
                               rt == eDB_TinyInt) {
                        CDB_Int r_in;
                        r->GetItem(&r_in);
                        cout << r->ItemName(j) << ": " << r_in.Value() << ' ';
                    } else
                        r->SkipItem();
                }
                cout << endl;
            }
            delete r;
        }
    }
    delete rcmd;
    delete con;
}

void SampleDBAPI_Blob(I_DriverContext& context, const string& server_name)
{
    // CDB_Connection* con = context.Connect(server_name, "anyone", "allowed", 0);
    CDB_Connection* con = context.Connect("NLMSQL", "sapojnikov", "OurPub", 0);

    // CDB_LangCmd* lcmd_use = con->LangCmd("use SrcTrack");
    CDB_LangCmd* lcmd_use = con->LangCmd("use ssstest");

    lcmd_use->Send();
    while (lcmd_use->HasMoreResults()) {
        CDB_Result* r = lcmd_use->Result();
        if (!r) continue;
    }
    delete lcmd_use;

    /*
    CDB_LangCmd* lcmd_opt = con->LangCmd("set textsize 100000");

    lcmd_opt->Send();
    while (lcmd_opt->HasMoreResults()) {
        CDB_Result* r = lcmd_opt->Result();
        if (!r) continue;
    }
    delete lcmd_opt;
    */

    CDB_LangCmd* lcmd_select = con->LangCmd(
        "select data from PackedAsn where oid=1"
        // "select data from PackedAsn where oid=1"
      );
    lcmd_select->Send();

    I_ITDescriptor* my_descr;

    // the result loop
    while(lcmd_select->HasMoreResults()) {
        CDB_Result* r= lcmd_select->Result();
        // skip all but row result
        if (r == 0  ||  r->ResultType() != eDB_RowResult) {
            delete r;
            continue;
        }
        // fetching the row
        while(r->Fetch()) {
            // read 0 bytes from the text (some clients need this trick)
            char buf[204800];
            memset(buf,0,sizeof(buf));
            int res = r->ReadItem(buf, sizeof(buf)-1);
            // cerr << "ReadItem() done!!!\n";

            cout << "data for oid=1 of size=" << res << "\n"
                    "-----------------------\n"
                 << buf << "\n";


            // The descriptor is needed only for SendData()
            // it is called here just to test if it works (may be commented out).
            // To test writing the data, substitute the login 'anyone' above
            // with the one that can write, and uncomment the code below the loop.
            my_descr = r->GetImageOrTextDescriptor();
        }
        delete r;
    }

    delete lcmd_select;

    /*
    // Writing the data to text/image (cannot be done under login 'anyone')

    CDB_Image my_image;
    for(int i=0; i<1000; i++) {
      my_image.Append("This is a text I want to insert!\n",32);
    }
    con->SendData(*my_descr, my_image);
    */

    delete my_descr;
    delete con;

    getchar();
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/06/28 14:42:36  sapojnik
 * *** empty log message ***
 *
 * Revision 1.5  2002/01/14 20:28:14  sapojnik
 * new SampleDBAPI_Blob
 *
 * Revision 1.4  2001/11/06 18:00:06  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/10/25 00:18:05  vakatov
 * SampleDBAPI_XXX() to accept yet another arg -- server name
 *
 * Revision 1.2  2001/10/24 16:37:43  lavr
 * Finish log with horizontal rule
 *
 * Revision 1.1  2001/10/23 20:51:26  lavr
 * Initial revision (derived from former sample programs)
 *
 * ===========================================================================
 */
