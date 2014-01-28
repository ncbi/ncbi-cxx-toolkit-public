#ifndef NETSTORAGE_DBAPP__HPP
#define NETSTORAGE_DBAPP__HPP

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

#include <corelib/ncbiapp.hpp>
#include <dbapi/dbapi.hpp>


BEGIN_NCBI_SCOPE


struct SDbAccessInfo
{
    string    m_ServerName;
    string    m_UserName;
    string    m_Password;
    string    m_Database;
    string    m_Driver;
};


class CNSTDbApp
{
public:
    CNSTDbApp(CNcbiApplication &  app);
    ~CNSTDbApp(void);

    const SDbAccessInfo &  GetDbAccessInfo(void);
    IDataSource *          GetDataSource(void);
    IConnection *          GetDbConn(void);

private:

    CNcbiApplication &      m_App;
    auto_ptr<SDbAccessInfo> m_DbAccessInfo;
    IDataSource *           m_IDataSource;
    IConnection *           m_IDbConnection;

    CNSTDbApp(void);
    CNSTDbApp(const CNSTDbApp &  conn);
    CNSTDbApp & operator= (const CNSTDbApp &  conn);
};


END_NCBI_SCOPE

#endif /* NETSTORAGE_DBAPP__HPP */

