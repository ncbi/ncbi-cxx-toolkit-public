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
 * Author:  Sergey Satskiy
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>
#include "nst_dbapp.hpp"


BEGIN_NCBI_SCOPE


CNSTDbApp::CNSTDbApp(CNcbiApplication &  app)
    : m_App(app), m_Connected(false)
{
}


CNSTDbApp::~CNSTDbApp(void)
{
    if (m_Connected)
        x_GetDatabase()->Close();
}


void CNSTDbApp::Connect(void)
{
    if (!m_Connected) {
        x_GetDatabase()->Connect();
        m_Connected = true;
    }
}


int CNSTDbApp::ExecSP_CreateClientOwnerGroup(
            const string &  client,
            const CJsonNode &  message,
            const SCommonRequestArguments &  common_args,
            Int8 &  client_id, Int8 &  owner_id, Int8 &  group_id)
{



    return 0;
}


CQuery CNSTDbApp::x_NewQuery(void)
{
    if (!m_Connected)
        Connect();
    return x_GetDatabase()->NewQuery();
}


CQuery CNSTDbApp::x_NewQuery(const string &  sql)
{
    if (!m_Connected)
        Connect();
    return x_GetDatabase()->NewQuery(sql);
}


const SDbAccessInfo &  CNSTDbApp::x_GetDbAccessInfo(void)
{
    if (m_DbAccessInfo.get())
        return *m_DbAccessInfo;

    m_DbAccessInfo.reset(new SDbAccessInfo);

    m_DbAccessInfo->m_Service =
        m_App.GetConfig().GetString("database", "service", "");
    m_DbAccessInfo->m_UserName =
        m_App.GetConfig().GetString("database", "user_name", "");
    m_DbAccessInfo->m_Password =
        m_App.GetConfig().GetString("database", "password", "");
    m_DbAccessInfo->m_Database =
        m_App.GetConfig().GetString("database", "database", "");

    return *m_DbAccessInfo;
}


CDatabase *  CNSTDbApp::x_GetDatabase(void)
{
    if (m_Db.get() == NULL) {
        const SDbAccessInfo &   access_info = x_GetDbAccessInfo();
        string                  uri = "dbapi://" + access_info.m_UserName +
                                      ":" + access_info.m_Password +
                                      "@" + access_info.m_Service +
                                      "/" + access_info.m_Database;
        CSDB_ConnectionParam    db_params(uri);

        m_Db.reset(new CDatabase(db_params));
    }

    return m_Db.get();
}

END_NCBI_SCOPE

