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
 * Author:  Anton Butanayev
 *
 * File Description:
 *    Driver for MySQL server
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <dbapi/driver/mysql/interfaces.hpp>


BEGIN_NCBI_SCOPE


CMySQLContext::CMySQLContext()
{
}


CMySQLContext::~CMySQLContext()
{
}


bool CMySQLContext::IsAbleTo(ECapability /*cpb*/) const
{
    return false;
}


bool CMySQLContext::SetLoginTimeout(unsigned int /*nof_secs*/)
{
    return false;
}


bool CMySQLContext::SetTimeout(unsigned int /*nof_secs*/)
{
    return false;
}


bool CMySQLContext::SetMaxTextImageSize(size_t /*nof_bytes*/)
{
    return false;
}


CDB_Connection* CMySQLContext::Connect(const string&   srv_name,
                                       const string&   user_name,
                                       const string&   passwd,
                                       TConnectionMode /*mode*/,
                                       bool            /*reusable*/,
                                       const string&   /*pool_name*/)
{
    return Create_Connection
        (*new CMySQL_Connection(this, srv_name, user_name, passwd));
}


///////////////////////////////////////////////////////////////////////
// DriverManager related functions
//

static I_DriverContext* MYSQL_CreateContext(const map<string,string>* /*attr*/)
{
    return new CMySQLContext();
}

void DBAPI_RegisterDriver_MYSQL(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("mysql", MYSQL_CreateContext);
}

extern "C" {
    NCBI_DBAPIDRIVER_MYSQL_EXPORT
    void* DBAPI_E_mysql()
    {
        return (void*) DBAPI_RegisterDriver_MYSQL;
    }
}

///////////////////////////////////////////////////////////////////////////////
const string kDBAPI_MYSQL_DriverName("mysql");

class CDbapiMySqlCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CMySQLContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CMySQLContext> TParent;

public:
    CDbapiMySqlCF2(void);
    ~CDbapiMySqlCF2(void);

};

CDbapiMySqlCF2::CDbapiMySqlCF2(void)
    : TParent( kDBAPI_MYSQL_DriverName, 0 )
{
    return ;
}

CDbapiMySqlCF2::~CDbapiMySqlCF2(void)
{
    return ;
}

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_mysql(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiMySqlCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_MYSQL_EXPORT
void
DBAPI_RegisterDriver_MYSQL(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_mysql );
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/03/02 19:29:54  ssikorsk
 * Export new RegisterDriver function on Windows
 *
 * Revision 1.9  2005/03/01 15:22:55  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.8  2004/12/20 17:13:13  ucko
 * Const-correctness fix for latest RegisterDriver interface.
 *
 * Revision 1.7  2004/05/17 21:15:34  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.6  2004/04/07 13:41:47  gorelenk
 * Added export prefix to implementations of DBAPI_E_* functions.
 *
 * Revision 1.5  2004/03/24 19:46:53  vysokolo
 * addaed support of blob
 *
 * Revision 1.4  2003/07/17 20:51:37  soussov
 * connections pool improvements
 *
 * Revision 1.3  2003/02/19 03:38:13  vakatov
 * Added DriverMgr related entry point and registration function
 *
 * Revision 1.2  2003/01/06 20:30:26  vakatov
 * Get rid of some redundant header(s).
 * Formally reformatted to closer meet C++ Toolkit/DBAPI style.
 *
 * Revision 1.1  2002/08/13 20:23:14  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
