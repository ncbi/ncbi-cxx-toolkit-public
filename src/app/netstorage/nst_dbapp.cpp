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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>

#include "nst_dbapp.hpp"


BEGIN_NCBI_SCOPE


CNSTDbApp::CNSTDbApp(CNcbiApplication &  app)
    : m_App(app),
      m_IDataSource(NULL),
      m_IDbConnection(NULL)
{
    DBLB_INSTALL_DEFAULT();
    DBAPI_RegisterDriver_FTDS();
}


CNSTDbApp::~CNSTDbApp(void)
{
}


const SDbAccessInfo &  CNSTDbApp::GetDbAccessInfo(void)
{
    if (m_DbAccessInfo.get())
        return *m_DbAccessInfo;

    m_DbAccessInfo.reset(new SDbAccessInfo);

    m_DbAccessInfo->m_ServerName =
        m_App.GetConfig().GetString("database", "server_name", "");
    m_DbAccessInfo->m_UserName =
        m_App.GetConfig().GetString("database", "user_name", "");
    m_DbAccessInfo->m_Password =
        m_App.GetConfig().GetString("database", "password", "");
    m_DbAccessInfo->m_Database =
        m_App.GetConfig().GetString("database", "database", "");
    m_DbAccessInfo->m_Driver = "ftds";

    return *m_DbAccessInfo;
}


IDataSource *  CNSTDbApp::GetDataSource(void)
{
    if (m_IDataSource)
        return m_IDataSource;

    CDB_UserHandler::SetDefault(new CDB_UserHandler_Exception());

    CDriverManager &    dm = CDriverManager::GetInstance();
    m_IDataSource = dm.CreateDs(GetDbAccessInfo().m_Driver);
    return m_IDataSource;
}


IConnection *  CNSTDbApp::GetDbConn(void)
{
    if (m_IDbConnection)
        return m_IDbConnection;

    m_IDbConnection = GetDataSource()->CreateConnection();

    const SDbAccessInfo &   access_info = GetDbAccessInfo();
    m_IDbConnection->Connect(access_info.m_UserName,
                             access_info.m_Password,
                             access_info.m_ServerName,
                             access_info.m_Database);
    m_IDbConnection->SetMode(IConnection::eBulkInsert);
    return m_IDbConnection;
}

END_NCBI_SCOPE

