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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *    Driver for SQLite v3 embedded database server
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/sqlite3/interfaces.hpp>


BEGIN_NCBI_SCOPE


CSL3Context::CSL3Context()
{
}


CSL3Context::~CSL3Context()
{
    DeleteAllConn();
}


bool CSL3Context::IsAbleTo(ECapability /*cpb*/) const
{
    return false;
}


impl::CConnection*
CSL3Context::MakeIConnection(const SConnAttr& conn_attr)
{
    return new CSL3_Connection(*this, conn_attr.srv_name, conn_attr.user_name,
                                 conn_attr.passwd);
}


///////////////////////////////////////////////////////////////////////
// DriverManager related functions
//
/*
static I_DriverContext* SQLite3_CreateContext(const map<string,string>* attr)
{
    return new CSL3Context();
}
*/

///////////////////////////////////////////////////////////////////////////////
class CDbapiSQLite3CF2 : public CSimpleClassFactoryImpl<I_DriverContext, CSL3Context>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CSL3Context> TParent;

public:
    CDbapiSQLite3CF2(void);
    ~CDbapiSQLite3CF2(void);

};

CDbapiSQLite3CF2::CDbapiSQLite3CF2(void)
    : TParent( "sqlite3", 0 )
{
    return ;
}

CDbapiSQLite3CF2::~CDbapiSQLite3CF2(void)
{
    return ;
}

///////////////////////////////////////////////////////////////////////////////
extern "C"
{
    NCBI_DBAPIDRIVER_SQLITE3_EXPORT
    void
    NCBI_EntryPoint_xdbapi_sqlite3(
        CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
        CPluginManager<I_DriverContext>::EEntryPointRequest method)
    {
        CHostEntryPointImpl<CDbapiSQLite3CF2>::NCBI_EntryPointImpl( info_list, method );
    }
}

NCBI_DBAPIDRIVER_SQLITE3_EXPORT
void
DBAPI_RegisterDriver_SQLITE3(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_sqlite3 );
}


END_NCBI_SCOPE


