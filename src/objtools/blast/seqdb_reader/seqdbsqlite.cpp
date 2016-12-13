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

const CSeqDBSqlite::TOid CSeqDBSqlite::kNotFound = -1;

/// Try to extract accession and version from a string that may be of the
/// form "accession.version" where version can be parsed as a positive integer.
/// If successful, accession part will be returned in acc, and version number
///   will be returned in ver.  Function will return true.
/// If unsuccessful, acc_ver will be copied to acc, and ver will be set to -1.
///   Function will return false;
/// @param acc_ver string from which accession and version are to be extracted
/// @param acc reference to string which will receive accession
/// @param ver reference to int which will receive version
/// @return true if valid version extracted from string, otherwise false
static bool x_ExtractVersion(
        const string& acc_ver,
        string&       acc,
        unsigned int& ver
)
{
    // Where is the last '.' character?
    size_t dot = acc_ver.find_last_of('.');
    // If found...
    if (dot != acc_ver.npos) {
        // Try to parse non-negative integer after last '.' character.
        try {
            // Parse integer from portion after '.' (may throw).
            string sfx = acc_ver.substr(dot + 1);
            size_t rem;     // offset of any non-numerics after numeric
            ver = stoi(sfx, &rem);
            // Are there any non-numerics after the numeric portion?
            // (Shouldn't be.)
            if (rem >= sfx.length()) {
                // Version cannot be negative.
                if (ver >= 0) {
                    // Set acc to the portion before the '.'
                    // and return true indicating success
                    acc = acc_ver.substr(0, dot);
                    return true;
                }
            }
        } catch (...) {
        }
    }
    // If any condition above is not met, return unaltered string.
    acc = acc_ver;
    ver = -1;
    return false;
}

CSeqDBSqlite::CSeqDBSqlite(const string& dbname)
{
    // Open input SQLite database.
    m_db.reset(new CSQLITE_Connection(
            dbname,
            CSQLITE_Connection::fInternalMT
            | CSQLITE_Connection::fVacuumManual
            | CSQLITE_Connection::fJournalMemory
            | CSQLITE_Connection::fSyncOff
            | CSQLITE_Connection::fTempToMemory
            | CSQLITE_Connection::fWritesSync
            | CSQLITE_Connection::fReadOnly
    ));
}

CSeqDBSqlite::~CSeqDBSqlite()
{
    // Clean up.
    if (m_selectStmt) {
        m_selectStmt.reset();
    }
    m_db.reset();
}

void CSeqDBSqlite::SetCacheSize(const size_t cache_size)
{
    // This method accepts a value in bytes, but SQLite wants a number
    // of pages.  The page size is determined when an SQLite file is created.
    unsigned int pageSize = m_db->GetPageSize();
    m_db->SetCacheSize(cache_size / pageSize);
}

void CSeqDBSqlite::GetOid(
        vector<TOid>&      oidv,
        const string&      accession,
        const unsigned int version
)
{
    static CSQLITE_Statement* selectStmt = nullptr;

    // Create the SELECT statement.
    if (selectStmt == nullptr) {
        selectStmt = new CSQLITE_Statement(
                m_db.get(),
                "SELECT oid FROM acc2oid WHERE accession = ? AND version = ? "
                "ORDER BY oid ASC;"
        );
    }
    // Bind the accession string to the statement.
    selectStmt->ClearBindings();
    selectStmt->Bind(1, accession);
    selectStmt->Bind(2, version);
    selectStmt->Execute();
    // Erase current contents of result vector.
    oidv.clear();
    // Step through all selected rows.
    while (selectStmt->Step()) {
        // Fetch the version and OID.
        // Only add the OID to the vector if the vector is empty,
        // or if the OID differs from the last OID in the vector.
        TOid oid = static_cast<TOid>(selectStmt->GetInt(0));
        if (oidv.empty()  ||  oidv.back() != oid) {
            oidv.push_back(oid);
        }
    }
    selectStmt->Reset();
}

void CSeqDBSqlite::GetOid(
        vector<TOid>& oidv,
        const string& accession,
        const bool    allow_dup
)
{
    string acc;
    unsigned int ver;
    // If accession has valid version suffix, it will be parsed into ver,
    // and acc will be accession with version stripped off, and
    // x_ExtractVersion will return true.
    // Otherwise x_ExtractVersion will return false.
    oidv.clear();
    if (x_ExtractVersion(accession, acc, ver)) {
        // Looks like an accession.version string, try searching for that.
        GetOid(oidv, acc, ver);
        if (!oidv.empty()) return;
        // If we got no match, continue down to search for the whole string
        // as presented.
    }

    // Create the SELECT statement.
    static CSQLITE_Statement* selectStmt = nullptr;
    if (selectStmt == nullptr) {
        selectStmt = new CSQLITE_Statement(
                m_db.get(),
                "SELECT version, oid FROM acc2oid WHERE accession = ? "
                "ORDER BY version DESC, oid ASC;"
        );
    }
    // Bind the accession string to the statement.
    selectStmt->ClearBindings();
    selectStmt->Bind(1, accession);
    selectStmt->Execute();

    // We may need to keep track of the highest version if there are
    // multiple rows that match the accession.
    int bestVer = -1;       // the highest version
    // Step through all selected rows.
    while (selectStmt->Step()) {
        // Fetch the version and OID.
        int ver = selectStmt->GetInt(0);
        TOid oid = static_cast<TOid>(selectStmt->GetInt(1));
        if (allow_dup) {
            if (ver != bestVer  ||  oid != oidv.back()) {
                oidv.push_back(oid);
                bestVer = ver;
            }
        } else {
            if (ver > bestVer) {
                oidv.clear();
                oidv.push_back(oid);
                bestVer = ver;
            } else if (ver == bestVer) {
                if (oid != oidv.back()) {
                    oidv.push_back(oid);
                }
            } /* else ignore this OID */
        }
    }
    selectStmt->Reset();
}

void CSeqDBSqlite::GetOids(
        vector<TOid>&         oidv,
        const vector<string>& accessions
)
{
    // We need to separate those accessions which APPEAR to have version
    // numbers from those which obviously do not.
    // If there are any in the first group which don't have matches
    // in the database, they will be appended onto the second group prior to
    // the second search.
    vector<tuple<string, string, unsigned int> > acc_w_ver;
    vector<string> acc_wo_ver;
    for (auto acc : accessions) {
        string a;
        unsigned int v;
        if (x_ExtractVersion(acc, a, v)) {
            acc_w_ver.push_back(make_tuple(acc, a, v));
        } else {
            acc_wo_ver.push_back(acc);
        }
    }

    // Create in-memory temporary table to hold list of accessions
    // with versions.
    m_db->ExecuteSql(
            "CREATE TEMP TABLE IF NOT EXISTS accvers ( acc TEXT, ver INTEGER );"
    );
    unique_ptr<CSQLITE_Statement> ins(
            new CSQLITE_Statement(
                    m_db.get(),
                    "INSERT INTO accvers(acc, ver) VALUES (?, ?);"
            )
    );
    m_db->ExecuteSql("BEGIN TRANSACTION;");
    for (auto accver : acc_w_ver) {
        ins->ClearBindings();
        ins->Bind(1, get<1>(accver));
        ins->Bind(2, get<2>(accver));
        ins->Execute();
        ins->Reset();
    }
    m_db->ExecuteSql("END TRANSACTION;");
    ins.reset();

    // Create in-memory temporary table to hold list of accessions
    // without versions.
    m_db->ExecuteSql("CREATE TEMP TABLE IF NOT EXISTS accs ( acc TEXT );");
    ins.reset(
            new CSQLITE_Statement(
                    m_db.get(),
                    "INSERT INTO accs(acc) VALUES (?);"
            )
    );
    m_db->ExecuteSql("BEGIN TRANSACTION;");
    for (auto acc : acc_wo_ver) {
        ins->ClearBindings();
        ins->Bind(1, acc);
        ins->Execute();
        ins->Reset();
    }
    m_db->ExecuteSql("END TRANSACTION;");
    ins.reset();

    // First search

    // Select from main table based on accession/version pairs also being in
    // temp table "accvers".
    map<string, TOid> acc2oid;
    unique_ptr<CSQLITE_Statement> sel(new CSQLITE_Statement(
            m_db.get(),
            "SELECT * FROM acc2oid INNER JOIN accvers "
            "ON accession = acc AND version = ver "
            "ORDER BY accession ASC, version DESC, oid ASC;"
    ));
    sel->Execute();
    while (sel->Step()) {
        // Get all columns from current row.
        string accession = sel->GetString(0);
        unsigned int version = (unsigned int) sel->GetInt(1);
        TOid oid = (TOid) sel->GetInt(2);
        // If accession is not yet in map, it will be added.
        // If accession is already in the map, it will not be added again.
        // Here, "thing.1" and "thing.2" are considered distinct, and
        // both will be added to the map.
        string accver = accession + "." + to_string(version);
        acc2oid.insert(make_pair(accver, oid));
    }
    sel.reset();

    // Scan collection of accession/version pairs for any which did not
    // match any rows in the database.  We'll try searching for them again,
    // using the accession string in its original form.
    for (auto accver : acc_w_ver) {
        // If accession isn't in found list, append its original form
        // to the collection of accessions without version numbers
        // for another try.
        if (acc2oid.find(get<1>(accver)) == acc2oid.end()) {
            acc_wo_ver.push_back(get<0>(accver));
        }
    }

    // Second search

    // Select from main table based on accessions (without versions)
    // also being in temp table.
    sel.reset(new CSQLITE_Statement(
            m_db.get(),
            "SELECT * FROM acc2oid INNER JOIN accs "
            "ON accession = acc "
            "ORDER BY accession ASC, version DESC, oid ASC;"
    ));
    sel->Execute();
    while (sel->Step()) {
        // If accession is not yet in map, it will be added.
        // If accession is already in the map, it will not be added again.
        // Because of the order of the returned results, higher version
        // will take precedence.  If more than one accession has the highest
        // version, the lowest OID will take precedence.
        string accession = sel->GetString(0);
        TOid oid = (TOid) sel->GetInt(2);
        acc2oid.insert(make_pair(accession, oid));
    }
    sel.reset();

    // Fetch found OIDs, write to output list.
    oidv.clear();
    for (auto acc : accessions) {
        try {
            TOid oid = acc2oid.at(acc);   // may throw exception
            oidv.push_back(oid);
        } catch (out_of_range& e) {
            // Accession was not found.
            oidv.push_back(kNotFound);
        }
    }

    // Clear contents of temp tables, we may reuse them later.
    m_db->ExecuteSql("DELETE FROM accs;");
    m_db->ExecuteSql("DELETE FROM accvers;");
}

void CSeqDBSqlite::GetAccessions(
        vector<string>& accessions,
        const TOid      oid
)
{
    // Perform reverse-lookup of accession for a given OID.
    // There are likely to be multiple matches.
    // Reverse lookups are slower than accession-to-OID lookups
    // because there's no index on the OIDs.
    ostringstream oss;
    oss << "SELECT accession FROM acc2oid WHERE oid = " << oid << ";";
    CSQLITE_Statement sel(m_db.get(), oss.str());
    sel.Execute();
    // Collect each accession selected.
    accessions.clear();
    while (sel.Step()) {
        accessions.push_back(sel.GetString(0));
    }
}

bool CSeqDBSqlite::StepAccessions(string* acc, int* ver, TOid* oid)
{
    // If this is the first step, create the SELECT statement.
    if (!m_selectStmt) {
        m_selectStmt.reset(new CSQLITE_Statement(
                m_db.get(),
                "SELECT * FROM acc2oid;"
        ));
    }
    // If there's at least one row remaining, fetch it.
    // Return only those columns for which non-null targets are given.
    if (m_selectStmt->Step()) {
        if (acc != NULL) {
            *acc = m_selectStmt->GetString(0);
        }
        if (ver != NULL) {
            *ver = m_selectStmt->GetInt(1);
        }
        if (oid != NULL) {
            *oid = (TOid) m_selectStmt->GetInt(2);
        }
        // We're returning a row.
        return true;
    } else {
        // Rows all used up, finalize SELECT statement and return false,
        // i.e. now row returned.
        m_selectStmt.reset();
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
    // If this is the first step, create the SELECT statement.
    if (!m_selectStmt) {
        m_selectStmt.reset(new CSQLITE_Statement(
                m_db.get(),
                "SELECT * FROM volinfo;"
        ));
    }
    // If there's at least one row remaining, fetch it.
    // Return only those columns for which non-null targets are given.
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
        // We're returning a row.
        return true;
    } else {
        // Rows all used up, finalize SELECT statement and return false,
        // i.e. now row returned.
        m_selectStmt.reset();
        return false;
    }
}

END_NCBI_SCOPE
