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


void CNSTDatabase::ExecSP_GetNextObjectID(Int8 &  object_id)
{
    CQuery      query = x_NewQuery();

    object_id = 0;
    query.SetParameter("@next_id", 0, eSDB_Int8, eSP_InOut);
    query.ExecuteSP("GetNextObjectID");
    query.VerifyDone();
    x_CheckStatus(query, "GetNextObjectID");

    object_id = query.GetParameter("@next_id").AsInt8();
}


void CNSTDatabase::ExecSP_CreateClient(
            const string &  client, Int8 &  client_id)
{
    CQuery      query = x_NewQuery();

    client_id = 0;
    query.SetParameter("@client_name", client);
    query.SetParameter("@client_id", 0, eSDB_Int8, eSP_InOut);

    query.ExecuteSP("CreateClient");
    query.VerifyDone();
    x_CheckStatus(query, "CreateClient");

    client_id = query.GetParameter("@client_id").AsInt8();
}


void CNSTDatabase::ExecSP_CreateObject(
            Int8  object_id, const string &  object_key,
            const string &  object_loc, Int8  size,
            const string &  client_name)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_id", object_id);
    query.SetParameter("@object_key", object_key);
    query.SetParameter("@object_loc", object_loc);
    query.SetParameter("@object_size", size);
    query.SetParameter("@client_name", client_name);

    query.ExecuteSP("CreateObject");
    query.VerifyDone();
    x_CheckStatus(query, "CreateObject");
}


void CNSTDatabase::ExecSP_CreateObjectWithClientID(
            Int8  object_id, const string &  object_key,
            const string &  object_loc, Int8  size,
            Int8  client_id)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_id", object_id);
    query.SetParameter("@object_key", object_key);
    query.SetParameter("@object_loc", object_loc);
    query.SetParameter("@object_size", size);
    query.SetParameter("@client_id", client_id);

    query.ExecuteSP("CreateObjectWithClientID");
    query.VerifyDone();
    x_CheckStatus(query, "CreateObjectWithClientID");
}


void CNSTDatabase::ExecSP_UpdateObjectOnWriteByKey(
            const string &  object_key, Int8  size)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_key", object_key);
    query.SetParameter("@object_size", size);

    query.ExecuteSP("UpdateObjectOnWriteByKey");
    query.VerifyDone();
    x_CheckStatus(query, "UpdateObjectOnWriteByKey");
}


void CNSTDatabase::ExecSP_UpdateObjectOnWriteByLoc(
            const string &  object_loc, Int8  size)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_loc", object_loc);
    query.SetParameter("@object_size", size);

    query.ExecuteSP("UpdateObjectOnWriteByLoc");
    query.VerifyDone();
    x_CheckStatus(query, "UpdateObjectOnWriteByLoc");
}


void CNSTDatabase::ExecSP_UpdateObjectOnReadByKey(
            const string &  object_key)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_key", object_key);

    query.ExecuteSP("UpdateObjectOnReadByKey");
    query.VerifyDone();
    x_CheckStatus(query, "UpdateObjectOnReadByKey");
}


void CNSTDatabase::ExecSP_UpdateObjectOnReadByLoc(
            const string &  object_loc)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_loc", object_loc);

    query.ExecuteSP("UpdateObjectOnReadByLoc");
    query.VerifyDone();
    x_CheckStatus(query, "UpdateObjectOnReadByLoc");
}


void CNSTDatabase::ExecSP_RemoveObjectByKey(const string &  object_key)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_key", object_key);

    query.ExecuteSP("RemoveObjectByKey");
    query.VerifyDone();
    x_CheckStatus(query, "RemoveObjectByKey");
}


void CNSTDatabase::ExecSP_RemoveObjectByLoc(const string &  object_loc)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_loc", object_loc);

    query.ExecuteSP("RemoveObjectByLoc");
    query.VerifyDone();
    x_CheckStatus(query, "RemoveObjectByLoc");
}


void CNSTDatabase::ExecSP_AddAttributeByLoc(const string &  object_loc,
                                            const string &  attr_name,
                                            const string &  attr_value)
{
    CQuery      query = x_NewQuery();
    query.SetParameter("@object_loc", object_loc);
    query.SetParameter("@attr_name", attr_name);
    query.SetParameter("@attr_value", attr_value);

    query.ExecuteSP("AddAttributeByLoc");
    query.VerifyDone();
    x_CheckStatus(query, "AddAttributeByLoc");
}


void CNSTDatabase::x_CheckStatus(CQuery &  query,
                                 const string &  procedure)
{
    int     status = query.GetStatus();
    if (status != 0)
        NCBI_THROW(CNetStorageServerException, eDatabaseError,
                   "Error executing " + procedure + " stored "
                   "procedure (return code " + NStr::NumericToString(status) +
                   "). See MS SQL log for details.");
}


CQuery CNSTDatabase::x_NewQuery(void)
{
//    if (!m_Connected)
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

