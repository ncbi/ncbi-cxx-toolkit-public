/* $Id $
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
 * This simple program illustrates how to use the RPC command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>

USING_NCBI_SCOPE;


int main()
{
    try {
        CODBCContext my_context;

        CDB_Connection* con = my_context.Connect("MS_DEV1", "anyone", "allowed", 0);

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
                                 << (r_vc.IsNULL()? "" : r_vc.Value()) 
                                 << " \t";
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
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
        return 1;
    }
    return 0;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/05/17 21:16:12  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/08/05 19:23:47  vakatov
 * MSSQL2 --> MS_DEV1
 *
 * Revision 1.2  2003/05/08 21:25:08  soussov
 * adopting to unix based odbc
 *
 * Revision 1.1  2002/06/20 20:08:35  ivanov
 * Initial revision -- built on base "dbl_sp_who" sample
 *
 * ===========================================================================
 */
