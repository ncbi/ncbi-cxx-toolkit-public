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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  DBAPI BLOB cache administration tool
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <dbapi/cache/dbapi_blob_cache.hpp>
#include <dbapi/driver/drivers.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

/// DBAPI cache administration tool
///
/// @internal
///
class CDBAPI_CacheAdmin : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
protected:
    int Connect(const CArgs& args);

    unsigned GetTimeout(const CArgs& args);
private:
    IDataSource*            m_Ds;
    auto_ptr<IConnection>   m_Conn;
    CDBAPI_Cache            m_Cache;

};


void CDBAPI_CacheAdmin::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "DBAPI cache admin library");

    arg_desc->AddOptionalKey("s",
                             "server",
                             "Database server name (default: MSSQL10)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("d",
                             "database",
                             "Database name (default: NCBI_Cache)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("u",
                             "username",
                             "Login name (default: cwrite)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("p",
                             "password",
                             "Password",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("m",
                      "Run cache maintenance (Timeout based BLOB removal).");



    arg_desc->AddOptionalKey("st",
                      "stimeout",
                      "BLOB expiration timeout in seconds",
                      CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("mt",
                      "mtimeout",
                      "BLOB expiration timeout in minutes",
                      CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("ht",
                      "htimeout",
                      "BLOB expiration timeout in hours",
                      CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("dt",
                      "dtimeout",
                      "BLOB expiration timeout in days",
                      CArgDescriptions::eInteger);

    SetupArgDescriptions(arg_desc.release());
}

int CDBAPI_CacheAdmin::Connect(const CArgs& args)
{
    CDriverManager &db_drv_man = CDriverManager::GetInstance();
    string drv_name;

#ifdef NCBI_OS_MSWIN
    DBAPI_RegisterDriver_ODBC(db_drv_man);
    drv_name = "odbc";
#else
    DBAPI_RegisterDriver_FTDS(db_drv_man);
    drv_name = "ftds";
#endif
    IDataSource* ds = db_drv_man.CreateDs(drv_name);

    if (ds == 0) {
        NcbiCerr << "Cannot init driver: " << drv_name
                    << NcbiEndl;
        return 1;
    }

    m_Conn.reset(ds->CreateConnection());
    if (m_Conn.get() == 0) {
        NcbiCerr << "Cannot create connection. Driver: " << drv_name
                    << NcbiEndl;
        return 1;
    }

    string server = (args["s"]) ? args["s"].AsString() : "MSSQL10";
    string database = (args["d"]) ? args["d"].AsString() : "NCBI_Cache";
    string user = (args["u"]) ? args["u"].AsString() : "cwrite";
    string passwd = (args["p"]) ? args["p"].AsString() : "allowed";

    m_Conn->Connect(user, passwd, server, database);

    m_Cache.Open(&*m_Conn);

    return 0;
}

unsigned CDBAPI_CacheAdmin::GetTimeout(const CArgs& args)
{
    unsigned timeout = 24 * 60 * 60;
    unsigned sec  = (args["st"]) ? args["st"].AsInteger() : 0;
    unsigned min  = (args["mt"]) ? args["mt"].AsInteger() : 0;
    unsigned hr   = (args["ht"]) ? args["ht"].AsInteger() : 0;
    unsigned days = (args["dt"]) ? args["dt"].AsInteger() : 0;

    unsigned tout = sec + (min * 60) + (hr * 60 * 60) + (days * 24 * 60 * 60);

    if (tout == 0)
        return timeout;
    return tout;
}

int CDBAPI_CacheAdmin::Run(void)
{
    try {
        CArgs args = GetArgs();

        int rc = Connect(args);
        if (rc) {
            return rc;
        }

        if (args["m"]) {
            NcbiCout << "Running cache maintanance..." << NcbiEndl;

            unsigned time_out = GetTimeout(args);
            m_Cache.Purge(time_out);
        }

    } catch( CDB_Exception& dbe ) {
        NcbiCerr << dbe.what() << NcbiEndl
                 << dbe.GetMsg()
                 << NcbiEndl;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CDBAPI_CacheAdmin().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/04/04 13:03:56  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.2  2004/07/21 15:33:46  kuznets
 * Fixed auto-login name and application parameters
 *
 * Revision 1.1  2004/07/20 18:14:05  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
