#ifndef NETSTORAGE_DATABASE__HPP
#define NETSTORAGE_DATABASE__HPP

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
 * Author: Sergey Satskiy
 *
 */

#include <corelib/ncbiapp.hpp>
#include <dbapi/simple/sdbapi.hpp>


BEGIN_NCBI_SCOPE

// Forward declarations
struct SCommonRequestArguments;
class CJsonNode;
class CNetStorageServer;


struct SDbAccessInfo
{
    string    m_Service;
    string    m_UserName;
    string    m_Password;
    string    m_Database;
};


class CNSTDatabase
{
public:
    CNSTDatabase(CNetStorageServer *  server);
    ~CNSTDatabase(void);

    void Connect(void);

    int  ExecSP_GetNextObjectID(Int8 &  object_id);
    int  ExecSP_CreateClient(const string &  client,
                             Int8 &  client_id);
    int  ExecSP_CreateObject(
            Int8  object_id, const string &  object_key,
            const string &  object_loc, Int8  size,
            const string &  client_name);
    int  ExecSP_CreateObjectWithClientID(
            Int8  object_id, const string &  object_key,
            const string &  object_loc, Int8  size,
            Int8  client_id);
    int  ExecSP_UpdateObjectOnWrite(
            const string &  object_key,
            const string &  object_loc, Int8  size, Int8  client_id);
    int  ExecSP_UpdateObjectOnRead(
            const string &  object_key,
            const string &  object_loc, Int8  size, Int8  client_id);
    int  ExecSP_UpdateObjectOnRelocate(
            const string &  object_key,
            const string &  object_loc, Int8  client_id);
    int  ExecSP_RemoveObject(const string &  object_key);
    int  ExecSP_AddAttribute(const string &  object_key,
                             const string &  object_loc,
                             const string &  attr_name,
                             const string &  attr_value,
                             Int8  client_id);
    int  ExecSP_GetAttribute(const string &  object_key,
                             const string &  attr_name,
                             string &        value);
    int  ExecSP_DelAttribute(const string &  object_key,
                             const string &  attr_name);

private:
    const SDbAccessInfo &  x_GetDbAccessInfo(void);
    CDatabase *            x_GetDatabase(void);
    CQuery x_NewQuery(void);
    CQuery x_NewQuery(const string &  sql);
    int  x_CheckStatus(CQuery &  query,
                       const string &  procedure);

private:
    CNetStorageServer *         m_Server;
    auto_ptr<SDbAccessInfo>     m_DbAccessInfo;
    auto_ptr<CDatabase>         m_Db;
    bool                        m_Connected;

    CNSTDatabase(const CNSTDatabase &  conn);
    CNSTDatabase & operator= (const CNSTDatabase &  conn);
};


END_NCBI_SCOPE

#endif /* NETSTORAGE_DBAPP__HPP */

