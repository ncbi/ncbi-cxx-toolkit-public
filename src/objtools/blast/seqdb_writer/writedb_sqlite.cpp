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

/// @file writedb_sqlite.cpp
/// Implementation for the CWriteDB_Sqlite and related classes.

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/blast/seqdb_writer/writedb_sqlite.hpp>

#include <sstream>
using std::ostringstream;

BEGIN_NCBI_SCOPE

CWriteDB_Sqlite::CWriteDB_Sqlite(const string& dbname)
{
    // Open output SQLite database, and create the acc2oid table
    // if it doesn't exist.
    m_db = new CSQLITE_Connection(
            dbname,
            CSQLITE_Connection::fExternalMT
            | CSQLITE_Connection::fVacuumOn
            | CSQLITE_Connection::fJournalMemory
            | CSQLITE_Connection::fSyncOff
            | CSQLITE_Connection::fTempToMemory
            | CSQLITE_Connection::fWritesSync
    );
    m_db->ExecuteSql(
            "CREATE TABLE IF NOT EXISTS acc2oid ( "
            "accession TEXT, "
            "version INTEGER, "
            "oid INTEGER"
            " );"
    );

    // Also create volume info table.
    m_db->ExecuteSql(
            "CREATE TABLE IF NOT EXISTS volinfo ( "
            "path TEXT, "
            "modtime INTEGER, "
            "volume INTEGER, "
            "numoids INTEGER"
            " );"
    );
}

CWriteDB_Sqlite::~CWriteDB_Sqlite()
{
    // Clean up.
    if (m_inTransaction) {
        EndTransaction();
    }
    if (m_insertStmt != NULL) {
        delete m_insertStmt;
        m_insertStmt = NULL;
    }
    delete m_db;
}

void CWriteDB_Sqlite::SetCacheSize(const size_t cache_size)
{
    unsigned int pageSize = m_db->GetPageSize();
    m_db->SetCacheSize(cache_size / pageSize);
}

void CWriteDB_Sqlite::CreateVolumeTable(const list<SVolInfo>& vols)
{
    // Delete all existing rows from volinfo table.
    m_db->ExecuteSql("DELETE FROM volinfo;");

    CSQLITE_Statement* ins = new CSQLITE_Statement(
            m_db,
            "INSERT INTO volinfo ( path, modtime, volume, numoids ) "
            "VALUES ( ?, ?, ?, ? );"
    );
    BeginTransaction();
    for (
            list<SVolInfo>::const_iterator it = vols.begin();
            it != vols.end();
            ++it
    ) {
        ins->ClearBindings();
        ins->Bind(1, it->m_path);
        ins->Bind(2, it->m_modTime);
        ins->Bind(3, it->m_vol);
        ins->Bind(4, it->m_oids);
        ins->Execute();
        ins->Reset();
    }
    EndTransaction();
    delete ins;
}

void CWriteDB_Sqlite::BeginTransaction(void)
{
    if (!m_inTransaction) {
        m_db->ExecuteSql("BEGIN TRANSACTION;");
        m_inTransaction = true;
    }
}

void CWriteDB_Sqlite::EndTransaction(void)
{
    if (m_inTransaction) {
        m_db->ExecuteSql("END TRANSACTION;");
        m_inTransaction = false;
    }
}

void CWriteDB_Sqlite::CommitTransaction(void)
{
    EndTransaction();
}

void CWriteDB_Sqlite::DeleteEntry(const string& accession, const int version)
{
    ostringstream oss;
    oss << "DELETE FROM acc2oid WHERE accession = '" << accession << "'";
    if (version > 0) {
        oss << " AND version = " << version;
    }
    oss << ";";
    m_db->ExecuteSql(oss.str());
}

int CWriteDB_Sqlite::DeleteEntries(const list<string>& accessions)
{
    // Create in-memory temporary table to hold accessions.
    m_db->ExecuteSql("CREATE TEMP TABLE IF NOT EXISTS accs ( acc TEXT );");
    m_db->ExecuteSql("BEGIN TRANSACTION;");
    CSQLITE_Statement ins(m_db, "INSERT INTO accs(acc) VALUES (?);");
    for (
            list<string>::const_iterator it = accessions.begin();
            it != accessions.end();
            ++it
    ) {
        ins.ClearBindings();
        ins.Bind(1, *it);
        ins.Execute();
        ins.Reset();
    }
    m_db->ExecuteSql("END TRANSACTION;");

    // Delete from main table based on accessions also being in temp table.
    CSQLITE_Statement del(m_db, "DELETE FROM acc2oid WHERE accession IN accs;");
    del.Execute();
    int nrows = del.GetChangedRowsCount();

    // Clear temp table for possible reuse.
    m_db->ExecuteSql("DELETE FROM accs;");

    return nrows;
}

void CWriteDB_Sqlite::InsertEntry(
        const string& accession,
        const int version,
        const int oid
)
{
    if (m_insertStmt == NULL) {
        // Prepare insert statement.
        m_insertStmt = new CSQLITE_Statement(
                m_db,
                "INSERT INTO acc2oid ( accession, version, oid ) "
                "VALUES ( ?, ?, ? );"
        );
    }
    m_insertStmt->ClearBindings();
    m_insertStmt->Bind(1, accession);
    m_insertStmt->Bind(2, version);
    m_insertStmt->Bind(3, oid);
    m_insertStmt->Execute();
    m_insertStmt->Reset();
}

void CWriteDB_Sqlite::InsertEntry(const string& accession, const int oid)
{
    InsertEntry(accession, 0, oid);
}

int CWriteDB_Sqlite::InsertEntries(
        const int oid,
        const list<CRef<CSeq_id> >& seqids
)
{
    int count = 0;
    list<CRef<CSeq_id> >::const_iterator it = seqids.begin();
    while (it != seqids.end()) {
        if (!(*it)->IsGi()) {
            int version;
            string seqid = (*it)->GetSeqIdString(&version);
            InsertEntry(seqid, version, oid);
            ++count;
        }
        ++it;
    }
    return count;
}

int CWriteDB_Sqlite::InsertEntries(const list<SAccOid>& seqids)
{
    int count = 0;
    list<SAccOid>::const_iterator it = seqids.begin();
    while (it != seqids.end()) {
        InsertEntry(it->m_acc, it->m_ver, it->m_oid);
        ++count;
        ++it;
    }
    return count;
}

void CWriteDB_Sqlite::CreateIndex()
{
    CSQLITE_Statement indexStmt(
            m_db,
            "CREATE INDEX IF NOT EXISTS acc ON acc2oid ( accession );"
    );
    indexStmt.Execute();
}

END_NCBI_SCOPE
