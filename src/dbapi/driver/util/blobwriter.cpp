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
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/util/blobstore.hpp>

USING_NCBI_SCOPE;

static char* getParam(char tag, int argc, char* argv[], bool* flag= 0)
{
    for(int i= 1; i < argc; i++) {
        if(((*argv[i] == '-') || (*argv[i] == '/')) && 
           (*(argv[i]+1) == tag)) { // tag found
            if(flag)
                *flag= true;

            if(*(argv[i]+2) == '\0') { // tag is a separate arg
                if(i == argc - 1)
                    return 0;
                if(*argv[i+1] != *argv[i])
                    return argv[i+1];
                else
                    return 0;
            }
            else {
                return argv[i]+2;
            }
        }
    }

    if(flag)
        *flag= false;

    return 0;
}

int main(int argc, char* argv[])
{
    string driver_name;
    string server_name;
    string user_name;
    string passwd;
    string blob_key;
    string table_name;
    string key_col_name;
    string num_col_name;

    size_t imagesize= 0;

    if(argc < 2) {
        cerr << argv[0]
             << " -K<blob_id> [-d<driver_name>] [-S<server_name>]"
             << " [-U<user_name>] [-P<password>] [-L<image size>] [-Z<compress_method>]"
             << " [-T<table_name>]"
             << endl;
        return 0;
    }

    const char* p= getParam('K', argc, argv);
    if(p == 0) {
        cerr << argv[0] << "Blob ID is missing" << endl;
        return 1;
    }
    blob_key= p;

    p= getParam('S', argc, argv);
    if(p == 0) {
        p= getenv("SQL_SERVER");
    }
    server_name= p? p : "MS_DEV1";
    
    p= getParam('d', argc, argv);
    if (p) {
        driver_name= p;
    } else {
        p= getenv("DBAPI_DRIVER");
        if(p == 0) {
            driver_name= (server_name.find("MS") != NPOS)? "ftds" : "ctlib";
        }
        else driver_name= p;
    }

    p= getParam('U', argc, argv);
    if(p == 0) {
        p= getenv("SQL_USER");
    }
    user_name= p? p : "anyone";

    p= getParam('P', argc, argv);
    if(p == 0) {
        p= getenv("SQL_PASSWD");
    }
    passwd= p? p : "allowed";

    ECompressMethod cm= eNone;
    p= getParam('Z', argc, argv);
    if(p) {
        if(*p == 'z') cm= eZLib;
        else cm= eBZLib;
    }

    p= getParam('L', argc, argv);
    if(p) imagesize= atoi(p);

    p= getParam('T', argc, argv);
    if(p == 0) {
        p= getenv("DATA_TABLE");
    }
    table_name= p? p : "MyDataTable";

    string *blob_column;
    
    try {
        C_DriverMgr drv_mgr;
        string err_msg;
        map<string, string> packet;
        packet.insert (map<string, string>::value_type (string("packet"),
                                                        string("2048")));
        I_DriverContext* my_context= drv_mgr.GetDriverContext(driver_name,
                                                              &err_msg, &packet);
        if(!my_context) {
            cerr << "Cannot load a driver " << driver_name << " [" 
                 << err_msg << "] " << endl;
            return 1;
        }

        CDB_Connection* con= my_context->Connect(server_name, user_name, passwd, 0, true);

        string query= "select * from " + table_name + " where 0=1";
        CDB_LangCmd* lcmd = con->LangCmd(query);

        lcmd->Send();

        unsigned int n;
        int k= 0;
        bool is_text= false;

        while (lcmd->HasMoreResults()) {
            CDB_Result* r = lcmd->Result();
            if (!r)
                continue;
            
            if (r->ResultType() == eDB_RowResult) {
                n= r->NofItems();
                if(n < 2) {
                    delete r;
                    continue;
                }
                
                blob_column= new string[n];

                for(unsigned int j= 0; j < n; j++) {
                    switch (r->ItemDataType(j)) {
                    case eDB_VarChar:
                    case eDB_Char:
                    case eDB_LongChar: 
                        key_col_name= r->ItemName(j);
                        break;

                    case eDB_Int:
                    case eDB_SmallInt:
                    case eDB_TinyInt:
                    case eDB_BigInt:
                        num_col_name= r->ItemName(j);
                        break;

                    case eDB_Text: is_text= true;
                    case eDB_Image:
                        blob_column[k++]= r->ItemName(j);
                    }
                }
                blob_column[k]= kEmptyStr;
                while(r->Fetch());
            }
            delete r;
        }
        delete lcmd;
        delete con;

        CSimpleBlobStore sbs(table_name, key_col_name, num_col_name, blob_column, is_text);

        CBlobLoader bload(my_context, server_name, user_name, passwd, &sbs);

        sbs.SetKey(blob_key);
        bload.Load(cin, cm, imagesize);
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
 * Revision 1.5  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.4  2004/05/25 19:47:28  soussov
 * moves blobstore.hpp header to include
 *
 * Revision 1.3  2004/05/24 19:41:46  soussov
 * adjusts to new blobstore API
 *
 * Revision 1.2  2004/05/17 21:18:21  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/05/03 16:47:10  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */
