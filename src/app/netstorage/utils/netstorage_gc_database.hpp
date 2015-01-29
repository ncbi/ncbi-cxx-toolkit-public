#ifndef NETSTORAGE_GC_DATABASE__HPP
#define NETSTORAGE_GC_DATABASE__HPP

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


struct SDbAccessInfo
{
    string    m_Service;
    string    m_UserName;
    string    m_Password;
    string    m_Database;
};


class CNetStorageGCDatabase
{
public:
    CNetStorageGCDatabase(const CNcbiRegistry &  reg, bool  verbose);
    ~CNetStorageGCDatabase(void);

    vector<string>  GetGCCandidates(void);
    int             GetDBStructureVersion(void);
    void            RemoveObject(const string &  locator, bool  dryrun,
                                 const string &  hit_id);

private:
    void x_Connect(void);
    void x_ReadDbAccessInfo(const CNcbiRegistry &  reg);
    void x_CreateDatabase(const CNcbiRegistry &  reg);

private:
    bool                    m_Verbose;
    SDbAccessInfo           m_DbAccessInfo;
    CDatabase *             m_Db;

    CNetStorageGCDatabase(const CNetStorageGCDatabase &);
    CNetStorageGCDatabase & operator= (const CNetStorageGCDatabase &);
};


END_NCBI_SCOPE

#endif /* NETSTORAGE_GC_DATABASE__HPP */

