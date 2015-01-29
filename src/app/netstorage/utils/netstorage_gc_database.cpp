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

#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbistd.hpp>
#include <connect/services/netstorage.hpp>
#include <connect/services/netstorage_impl.hpp>

#include "netstorage_gc_database.hpp"
#include "netstorage_gc_exception.hpp"


BEGIN_NCBI_SCOPE


CNetStorageGCDatabase::CNetStorageGCDatabase(const CNcbiRegistry &  reg,
                                             bool  verbose)
    : m_Verbose(verbose), m_Db(NULL)
{
    x_CreateDatabase(reg);
    try {
        x_Connect();
    } catch (...) {
        delete m_Db;
        throw;
    }
}


void CNetStorageGCDatabase::x_Connect(void)
{
    try {
        if (m_Verbose)
            cout << "Connecting to the NetStorage meta info database..."
                 << endl;
        m_Db->Connect();
        if (m_Verbose)
            cout << "Connected" << endl;
    } catch (...) {
        cerr << "Error connecting to the NetStorage meta info database" << endl;
        throw;
    }
}


CNetStorageGCDatabase::~CNetStorageGCDatabase(void)
{
    if (m_Verbose)
        cout << "Closing the NetStorage meta info database connection..."
             << endl;
    m_Db->Close();
    if (m_Verbose)
        cout << "Closed" << endl;
    delete m_Db;
}


void  CNetStorageGCDatabase::x_ReadDbAccessInfo(const CNcbiRegistry &  reg)
{
    if (m_Verbose)
        cout << "Reading the NetStorage meta info "
                "database connection parameters..." << endl;
    m_DbAccessInfo.m_Service = reg.GetString("database", "service", "");
    m_DbAccessInfo.m_UserName = reg.GetString("database", "user_name", "");
    m_DbAccessInfo.m_Password = reg.GetString("database", "password", "");
    m_DbAccessInfo.m_Database = reg.GetString("database", "database", "");
    if (m_Verbose)
        cout << "Read" << endl;
}


void  CNetStorageGCDatabase::x_CreateDatabase(const CNcbiRegistry &  reg)
{
    x_ReadDbAccessInfo(reg);

    string  uri = "dbapi://" + m_DbAccessInfo.m_UserName +
                  ":" + m_DbAccessInfo.m_Password +
                  "@" + m_DbAccessInfo.m_Service +
                  "/" + m_DbAccessInfo.m_Database;

    CSDB_ConnectionParam    db_params(uri);

    if (m_Verbose)
        cout << "Creating CDatabase (uri: " << uri << ")..." << endl;
    m_Db = new CDatabase(db_params);
    if (m_Verbose)
        cout << "Created" << endl;
}


vector<string>
CNetStorageGCDatabase::GetGCCandidates(void)
{
    vector<string>      candidates;
    string              stmt = "SELECT object_loc FROM Objects "
                               "WHERE tm_expiration IS NOT NULL AND "
                               "tm_expiration < GETDATE()";

    if (m_Verbose)
        cout << "Retrieving the NetStorage meta info "
                "database expired objects..." << endl;

    try {
        CQuery              query = m_Db->NewQuery(stmt);
        query.Execute();

        ITERATE(CQuery, row, query.SingleSet()) {
            candidates.push_back(row["object_loc"].AsString());
        }
    } catch (...) {
        cerr << "Error retrieving the NetStorage meta info "
                "database expired objects" << endl;
        throw;
    }

    if (m_Verbose)
        cout << "Retrieved " << candidates.size()
             << " expired object(s)" << endl;
    return candidates;
}


int
CNetStorageGCDatabase::GetDBStructureVersion(void)
{
    string              stmt = "SELECT version FROM Versions "
                               "WHERE name = 'db_structure'";
    int                 db_ver = -1;

    if (m_Verbose)
        cout << "Retrieving the NetStorage meta info "
                "database structure version..." << endl;

    try {
        CQuery              query = m_Db->NewQuery(stmt);
        query.Execute();

        ITERATE(CQuery, row, query.SingleSet()) {
            if (db_ver != -1) {
                string      msg = "Too many NetStorage meta info "
                                  "database structure versions";
                cerr << msg << endl;
                NCBI_THROW(CNetStorageGCException, eTooManyDBStructureVersions,
                           msg);
            }
            db_ver = row["version"].AsInt4();
        }
    } catch (const CNetStorageGCException &  ex) {
        throw;
    } catch (...) {
        cerr << "Error retrieving the NetStorage meta info "
                "database structure version..." << endl;
        throw;
    }

    if (db_ver == -1) {
        string      msg = "Cannot find NetStorage meta info "
                          "database structure version";
        cerr << msg << endl;
        NCBI_THROW(CNetStorageGCException, eDBStructureVersionNotFound, msg);
    }

    if (m_Verbose)
        cout << "Retrieved: " << db_ver << endl;
    return db_ver;
}


void
CNetStorageGCDatabase::RemoveObject(const string &  locator, bool  dryrun)
{
    CRef<CRequestContext>   ctx;

    // It involves removing attributes and removing the object record
    if (m_Verbose)
        cout << "Removing attributes of object " << locator << endl;

    string      stmt = "DELETE FROM AttrValues WHERE object_id IN ("
                       "SELECT object_id FROM Objects WHERE object_loc='" +
                       locator + "')";

    try {
        CQuery              query = m_Db->NewQuery(stmt);

        ctx.Reset(new CRequestContext());
        ctx->SetRequestID();
        GetDiagContext().SetRequestContext(ctx);
        GetDiagContext().PrintRequestStart()
                        .Print("action", "meta_attributes_remove")
                        .Print("locator", locator);

        if (!dryrun)
            query.Execute();

        ctx->SetRequestStatus(200);
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);
    } catch (const CException &  ex) {
        ERR_POST(ex);

        ctx->SetRequestStatus(500);
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);

        cerr << "Exception while removing attributes of object "
             << locator << endl;
        NCBI_THROW(CNetStorageGCException, eStopGC, k_StopGC);
    } catch (const std::exception &  ex ) {
        ERR_POST(ex.what());

        ctx->SetRequestStatus(500);
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);

        cerr << "std::exception while removing attributes of object "
             << locator << endl;
        NCBI_THROW(CNetStorageGCException, eStopGC, k_StopGC);
    } catch (...) {
        ERR_POST(k_UnknownException);

        ctx->SetRequestStatus(500);
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);

        cerr << "Unknown exception while removing attributes of object "
             << locator << endl;
        NCBI_THROW(CNetStorageGCException, eStopGC, k_StopGC);
    }

    // Second part: removing the object record
    if (m_Verbose)
        cout << "Removing object record of " << locator << endl;

    stmt = "DELETE FROM Objects WHERE object_loc='" + locator + "'";
    try {
        CQuery              query = m_Db->NewQuery(stmt);

        ctx.Reset(new CRequestContext());
        ctx->SetRequestID();
        GetDiagContext().SetRequestContext(ctx);
        GetDiagContext().PrintRequestStart()
                        .Print("action", "meta_object_remove")
                        .Print("locator", locator);

        if (!dryrun)
            query.Execute();

        ctx->SetRequestStatus(200);
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);
    } catch (const CException &  ex) {
        ERR_POST(ex);

        ctx->SetRequestStatus(500);
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);

        cerr << "Exception while removing object record of " << locator << endl;
        NCBI_THROW(CNetStorageGCException, eStopGC, k_StopGC);
    } catch (const std::exception &  ex ) {
        ERR_POST(ex.what());

        ctx->SetRequestStatus(500);
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);

        cerr << "std::exception while removing object record of "
             << locator << endl;
        NCBI_THROW(CNetStorageGCException, eStopGC, k_StopGC);
    } catch (...) {
        ERR_POST(k_UnknownException);

        ctx->SetRequestStatus(500);
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);

        cerr << "Unknown exception while removing object record of "
             << locator << endl;
        NCBI_THROW(CNetStorageGCException, eStopGC, k_StopGC);
    }
}


END_NCBI_SCOPE

