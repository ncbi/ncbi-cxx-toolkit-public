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
 * This simple program illustrates how to execute SQL statements
 * using one of available DBAPI drivers
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>

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
    string query;

    if(argc < 2) {
        cout << argv[0]
             << " [-d<driver_name>] [-S<server_name>]"
             << " [-U<user_name>] [-P<password>] [-Q<query>]"
             << endl;
        return 0;
    }

    const char* p= getParam('S', argc, argv);
    server_name= p? p : "MOZART";
    
    p= getParam('d', argc, argv);
    if (p) {
        driver_name= p;
    } else {
        driver_name= (server_name.find("MSSQL") != NPOS)? "ftds" : "ctlib";
    }

    p= getParam('U', argc, argv);
    user_name= p? p : "anyone";

    p= getParam('P', argc, argv);
    passwd= p? p : "allowed";

    bool f;
    p= getParam('Q', argc, argv, &f);
    if(p) {
        query= p;
    }
    else {
        if(f) { // query is on stdin
            cout << "query is on ctdin" << endl;
            char c;
            c= cin.get();
            while(!cin.eof()) {
                cout << c;
                query+= c;
                c= cin.get();
            }
            cout << endl;
        }
        if(query.empty())
            query= "exec sp_who";
    }

    
    cout << "Run query: '" << query << "' on server " << server_name 
         << " using driver: " << driver_name << endl;
    cout << "--------------------------------------------"
         << "----------------------------"
         << endl;
    
    try {
        C_DriverMgr drv_mgr;
        string err_msg;
        I_DriverContext* my_context= drv_mgr.GetDriverContext(driver_name,
                                                              &err_msg);
        if(!my_context) {
            cout << "Can not load a driver " << driver_name << " [" 
                 << err_msg << "] " << endl;
            return 1;
        }
        
        CDB_Connection* con =
            my_context->Connect(server_name, user_name, passwd, 0);

        CDB_LangCmd* lcmd = con->LangCmd(query);
        lcmd->Send();
    
        while (lcmd->HasMoreResults()) {
            CDB_Result* r = lcmd->Result();
            if (!r)
                continue;
            
            if (r->ResultType() == eDB_RowResult) {
                while (r->Fetch()) {
                    cout << "<ROW>";
                    for (unsigned int j = 0;  j < r->NofItems(); j++) {
                        EDB_Type rt = r->ItemDataType(j);
                        const char* iname= r->ItemName(j);
                        if(iname == 0) iname= "";
                        cout << '<' << iname << '>';
                        if (rt == eDB_Char || rt == eDB_VarChar) {
                            CDB_VarChar v;
                            r->GetItem(&v);
                            cout << (v.IsNULL()? "{NULL}" : v.Value());
                        } 
                        else if (rt == eDB_Int ||
                                 rt == eDB_SmallInt ||
                                 rt == eDB_TinyInt) {
                            CDB_Int v;
                            r->GetItem(&v);
                            if(v.IsNULL()) {
                                cout << "{NULL}";
                            }
                            else {
                                cout << v.Value();
                            }
                        } 
                        else if (rt == eDB_Float) {
                            CDB_Float v;
                            r->GetItem(&v);
                            if(v.IsNULL()) {
                                cout << "{NULL}";
                            }
                            else {
                                cout << v.Value();
                            }
                        }
                        else if(rt == eDB_Double) {
                            CDB_Double v;
                            r->GetItem(&v);
                            if(v.IsNULL()) {
                                cout << "{NULL}";
                            }
                            else {
                                cout << v.Value();
                            }
                        }
                        else if (rt == eDB_DateTime ||
                                 rt == eDB_SmallDateTime) {
                            CDB_DateTime v;
                            r->GetItem(&v);
                            if(v.IsNULL()) {
                                cout << "{NULL}";
                            }
                            else {
                                cout << v.Value().AsString();
                            }
                        }
                        else if (rt == eDB_Numeric) {
                            CDB_Numeric v;
                            r->GetItem(&v);
                            if(v.IsNULL()) {
                                cout << "{NULL}";
                            }
                            else {
                                cout << v.Value();
                            }
                        }
                        else if (rt == eDB_Text) {
                            CDB_Text v;
                            r->GetItem(&v);
                            cout << "{text (" << v.Size() << " bytes)}"; 
                        }
                        else if (rt == eDB_Image) {
                            CDB_Image v;
                            r->GetItem(&v);
                            cout << "{image (" << v.Size() << " bytes)}"; 
                        }
                        else {
                            r->SkipItem();
                            cout << "{unprintable}";
                        }
                        cout << "</" << iname << '>';
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



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/05/17 21:16:37  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2002/06/13 21:29:22  soussov
 * numeric added; double and float separated
 *
 * Revision 1.1  2002/06/13 19:43:07  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
