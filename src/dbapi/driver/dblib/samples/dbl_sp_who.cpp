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
 * This simple program illustrates how to use the RPC command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/dblib/interfaces.hpp>

USING_NCBI_SCOPE;


int main()
{
    try {
        CDBLibContext my_context;

#ifdef NCBI_OS_MSWIN
        CDB_Connection* con = my_context.Connect("MS_DEV1", "anyone", "allowed", 0);
#else
        CDB_Connection* con = my_context.Connect("MOZART", "anyone", "allowed", 0);
#endif

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
 * Revision 1.10  2004/05/17 21:13:08  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.9  2003/08/05 19:23:43  vakatov
 * MSSQL2 --> MS_DEV1
 *
 * Revision 1.8  2002/04/25 20:43:25  soussov
 * removes needless include
 *
 * Revision 1.7  2002/04/25 20:36:42  soussov
 * makes it plain
 *
 * Revision 1.6  2002/01/03 17:01:57  sapojnik
 * fixing CR/LF mixup
 *
 * Revision 1.5  2002/01/03 15:46:23  sapojnik
 * ported to MS SQL (about 12 'ifdef NCBI_OS_MSWIN' in 6 files)
 *
 * Revision 1.4  2001/11/06 18:00:00  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/10/25 00:18:04  vakatov
 * SampleDBAPI_XXX() to accept yet another arg -- server name
 *
 * Revision 1.2  2001/10/24 16:37:32  lavr
 * Finish log with horizontal rule
 *
 * Revision 1.1  2001/10/23 20:52:14  lavr
 * Initial revision (derived from former sample programs)
 *
 * Revision 1.1  2001/10/22 15:23:04  lavr
 * Initial revision derived from corresponding CTLib version
 *
 * ===========================================================================
 */
