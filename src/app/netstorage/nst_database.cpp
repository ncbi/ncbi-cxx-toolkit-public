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
#include <connect/services/json_over_uttp.hpp>
#include "nst_database.hpp"
#include "nst_exception.hpp"


BEGIN_NCBI_SCOPE


CNSTDatabase::CNSTDatabase(CNcbiApplication &  app)
    : m_App(app), m_Connected(false)
{
}


CNSTDatabase::~CNSTDatabase(void)
{
    if (m_Connected)
        x_GetDatabase()->Close();
}


void CNSTDatabase::Connect(void)
{
    if (!m_Connected) {
        x_GetDatabase()->Connect();
        m_Connected = true;
    }
}


void CNSTDatabase::ExecSP_CreateClientOwnerGroup(
            const string &  client,
            const CJsonNode &  message,
            Int8 &  client_id, Int8 &  owner_id, Int8 &  group_id)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@client_name", client);

    if (message.HasKey("Owner"))
        query.SetParameter("@owner_name", message.GetString("Owner"));
    else
        query.SetNullParameter("@owner_name", eSDB_String);
    if (message.HasKey("Group"))
        query.SetParameter("@group_name", message.GetString("Group"));
    else
        query.SetNullParameter("@group_name", eSDB_String);
    query.SetParameter("@client_id_", 0, eSDB_Int8, eSP_InOut);
    query.SetParameter("@owner_id_", 0, eSDB_Int8, eSP_InOut);
    query.SetParameter("@group_id_", 0, eSDB_Int8, eSP_InOut);

    query.ExecuteSP("CreateClientOwnerGroup");
    query.VerifyDone();

    int     status = query.GetStatus();
    if (status != 0)
        NCBI_THROW(CNetStorageServerException, eDatabaseError,
                   "Error executing CreateClientOwnerGroup stored "
                   "procedure (return code " + NStr::NumericToString(status) +
                   "). See MS SQL log for details.");

    client_id = query.GetParameter("@client_id_").AsInt8();
    owner_id = query.GetParameter("@owner_id_").AsInt8();
    group_id = query.GetParameter("@group_id_").AsInt8();
}


void CNSTDatabase::ExecSP_CreateObjectWithIDs(
            const string &  name, Int8  size,
            Int8  client_id, Int8  owner_id, Int8  group_id)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_name", name);
    query.SetParameter("@object_size", size);
    query.SetParameter("@client_id", client_id);

    if (owner_id == 0)
        query.SetNullParameter("@owner_id", eSDB_Int8);
    else
        query.SetParameter("@owner_id", owner_id);

    if (group_id == 0)
        query.SetNullParameter("@group_id", eSDB_Int8);
    else
        query.SetParameter("@group_id", group_id);

    query.ExecuteSP("CreateObjectWithIDs");
    query.VerifyDone();

    int     status = query.GetStatus();
    if (status != 0)
        NCBI_THROW(CNetStorageServerException, eDatabaseError,
                   "Error executing CreateObjectWithIDs stored "
                   "procedure (return code " + NStr::NumericToString(status) +
                   "). See MS SQL log for details.");
}


CQuery CNSTDatabase::x_NewQuery(void)
{
    if (!m_Connected)
        Connect();
    return x_GetDatabase()->NewQuery();
}


CQuery CNSTDatabase::x_NewQuery(const string &  sql)
{
    if (!m_Connected)
        Connect();
    return x_GetDatabase()->NewQuery(sql);
}


const SDbAccessInfo &  CNSTDatabase::x_GetDbAccessInfo(void)
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


CDatabase *  CNSTDatabase::x_GetDatabase(void)
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

