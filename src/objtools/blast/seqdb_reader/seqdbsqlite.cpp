/*  $Id$
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
 * Author:  Thomas W Rackers
 *
 */

/// @file seqdbsqlite.cpp
/// Implementation for the CSeqDBSqlite class, which manages an SQLite
/// index of string accessions to OIDs.

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/blast/seqdb_reader/impl/seqdbsqlite.hpp>

#include <sstream>
using std::ostringstream;

BEGIN_NCBI_SCOPE

const int CSeqDBSqlite::kNotFound = -1;
const int CSeqDBSqlite::kAmbiguous = -2;

CSeqDBSqlite::CSeqDBSqlite(const string& dbname)
{
    // Open input SQLite database.
    m_db = new CSQLITE_Connection(
            dbname,
            CSQLITE_Connection::fInternalMT
            | CSQLITE_Connection::fVacuumManual
            | CSQLITE_Connection::fJournalMemory
            | CSQLITE_Connection::fSyncOff
            | CSQLITE_Connection::fTempToMemory
            | CSQLITE_Connection::fWritesSync
            | CSQLITE_Connection::fReadOnly
    );
}

CSeqDBSqlite::~CSeqDBSqlite()
{
    // Clean up.
    if (m_selectStmt != NULL) {
        delete m_selectStmt;
        m_selectStmt = NULL;
    }
    delete m_db;
}

void CSeqDBSqlite::SetCacheSize(const size_t cache_size)
{
    unsigned int pageSize = m_db->GetPageSize();
    m_db->SetCacheSize(cache_size / pageSize);
}

int CSeqDBSqlite::GetOid(const string& accession)
{
    if (m_selectStmt == NULL) {
        m_selectStmt = new CSQLITE_Statement(
                m_db,
                "SELECT version, oid FROM acc2oid WHERE accession = ?;"
        );
    }
    m_selectStmt->ClearBindings();
    m_selectStmt->Bind(1, accession);
    m_selectStmt->Execute();
    int bestVer = -1;
    int bestOid = -1;
    bool ambig = false;
    while (m_selectStmt->Step()) {
        int ver = m_selectStmt->GetInt(0);
        int oid = m_selectStmt->GetInt(1);
        if (ver > bestVer) {
            bestVer = ver;
            bestOid = oid;
            ambig = false;
        } else if (ver == bestVer  &&  oid != bestOid) {
            ambig = true;
        }
    }
    m_selectStmt->Reset();
    if (ambig) return kAmbiguous;
    if (bestOid == -1) return kNotFound;
    return bestOid;
}

vector<int> CSeqDBSqlite::GetOids(const list<string>& accessions)
{
    // Create in-memory temporary table to hold accessions.
    m_db->ExecuteSql("CREATE TEMP TABLE IF NOT EXISTS accs ( acc TEXT );");
    CSQLITE_Statement* ins =
            new CSQLITE_Statement(m_db, "INSERT INTO accs(acc) VALUES (?);");
    m_db->ExecuteSql("BEGIN TRANSACTION;");
    for (
            list<string>::const_iterator it = accessions.begin();
            it != accessions.end();
            ++it
    ) {
        ins->ClearBindings();
        ins->Bind(1, *it);
        ins->Execute();
        ins->Reset();
    }
    m_db->ExecuteSql("END TRANSACTION;");
    delete ins;

    // Select from main table based on accessions also being in
    // temp table.
    struct SVerOid {
        int m_ver;
        int m_oid;
        SVerOid(int v, int o) : m_ver(v), m_oid(o) {}
    };
    map<string, SVerOid> acc2oid;
    CSQLITE_Statement* sel = new CSQLITE_Statement(
            m_db,
            "SELECT * FROM acc2oid INNER JOIN accs "
            "ON accession = acc ORDER BY accession ASC, version DESC;"
    );
    sel->Execute();
    while (sel->Step()) {
        // If accession is not yet in map, it will be added.
        // If accession is already in the map, it will not be added again.
        string accession = sel->GetString(0);
        int version = sel->GetInt(1);
        int oid = sel->GetInt(2);
        acc2oid.insert(pair<string, SVerOid>(accession, SVerOid(version, oid)));
        // Read back map element.  Most of the time we'll get back what
        // we put in.
        // If we exactly matched an existing map element, they're redundant
        // which is no problem.
        // If the versions differ, the earlier (higher) one takes precedence.
        // If the versions match but the OIDs do not, we have ambiguity.
        // If ambiguity has already been declared, that takes precedence.
        SVerOid veroid = acc2oid.at(accession);
        if (veroid.m_oid != kAmbiguous) {
            if (veroid.m_ver == version  &&  veroid.m_oid != oid) {
                acc2oid.at(accession).m_oid = kAmbiguous;
            }
        }
    }
    delete sel;

    // Fetch found OIDs, write to output list.
    vector<int> oids;
    for (
            list<string>::const_iterator it = accessions.begin();
            it != accessions.end();
            ++it
    ) {
        try {
            SVerOid veroid = acc2oid.at(*it);   // may throw exception
            oids.push_back(veroid.m_oid);
        } catch (out_of_range& e) {
            // Accession was not found.
            oids.push_back(kNotFound);
        }
    }

    // Clear contents of temp table, we may reuse it later.
    m_db->ExecuteSql("DELETE FROM accs;");

    return oids;
}

list<string> CSeqDBSqlite::GetAccessions(const int oid)
{
    list<string> accs;
    ostringstream oss;
    oss << "SELECT accession FROM acc2oid WHERE oid = " << oid << ";";
    CSQLITE_Statement sel(m_db, oss.str());
    sel.Execute();
    while (sel.Step()) {
        accs.push_back(sel.GetString(0));
    }
    return accs;
}

bool CSeqDBSqlite::StepAccessions(string* acc, int* ver, int* oid)
{
    if (m_selectStmt == NULL) {
        m_selectStmt = new CSQLITE_Statement(
                m_db,
                "SELECT * FROM acc2oid;"
        );
    }
    if (m_selectStmt->Step()) {
        if (acc != NULL) {
            *acc = m_selectStmt->GetString(0);
        }
        if (ver != NULL) {
            *ver = m_selectStmt->GetInt(1);
        }
        if (oid != NULL) {
            *oid = m_selectStmt->GetInt(2);
        }
        return true;
    } else {
        delete m_selectStmt;
        m_selectStmt = NULL;
        return false;
    }
}

bool CSeqDBSqlite::StepVolumes(
        string* path,
        int* modtime,
        int* volume,
        int* numoids
)
{
    if (m_selectStmt == NULL) {
        m_selectStmt = new CSQLITE_Statement(
                m_db,
                "SELECT * FROM volinfo;"
        );
    }
    if (m_selectStmt->Step()) {
        if (path != NULL) {
            *path = m_selectStmt->GetString(0);
        }
        if (modtime != NULL) {
            *modtime = m_selectStmt->GetInt(1);
        }
        if (volume != NULL) {
            *volume = m_selectStmt->GetInt(2);
        }
        if (numoids != NULL) {
            *numoids = m_selectStmt->GetInt(3);
        }
        return true;
    } else {
        delete m_selectStmt;
        m_selectStmt = NULL;
        return false;
    }
}

END_NCBI_SCOPE
