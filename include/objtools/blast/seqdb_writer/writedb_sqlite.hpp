#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_SQLITE_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_SQLITE_HPP

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

/// @file writedb_sqlite.hpp
/// Defines SQLite implementation of string-key database.
///
/// Defines classes:
///     CWriteDB_Sqlite
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbsqlite.hpp>

#include <memory>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_NCBI_SCOPE


/// This class supports creation of a string accession to integer OID
/// database, stored and maintained using the SQLite library.

class NCBI_XOBJREAD_EXPORT CWriteDB_Sqlite : public CObject
{
private:
    unique_ptr<CSQLITE_Connection> m_db;
    unique_ptr<CSQLITE_Statement> m_insertStmt;
    bool m_inTransaction = false;

public:
    typedef CSeqDBSqlite::TOid TOid;
    typedef CSeqDBSqlite::SVolInfo SVolInfo;
    typedef CSeqDBSqlite::SAccOid SAccOid;

    /// Constructor for a SQLite accession-to-OID database.
    /// @param dbname Database name (no default suffix)
    CWriteDB_Sqlite(const string& dbname);

    // Destructor, finalizes SQLite database upon completion.
    ~CWriteDB_Sqlite();

    /// Set SQLite cache size.
    /// @param cache_size Cache size in bytes
    void SetCacheSize(const size_t cache_size);

    /// Create volume table.
    void CreateVolumeTable(const list<SVolInfo>& vols);

    /// Begin SQLite transaction.
    /// Multiple database operations will be collected to be executed
    /// together when EndTransaction is called.
    /// @see EndTransaction
    /// @see CommitTransaction
    void BeginTransaction(void);

    /// End SQLite transaction, committing all changes since BeginTransaction
    /// was called.
    /// @see BeginTransaction
    /// @see CommitTransaction
    void EndTransaction(void);

    /// End SQLite transaction, identical to EndTransaction above.
    /// @see BeginTransaction
    /// @see EndTransaction
    void CommitTransaction(void);

    /// Delete one or more records which match the given accession.
    /// If the version number is zero, ALL entries matching the
    /// accession will be deleted.  If the version number is not zero,
    /// only the entry matching the accession and version will be
    /// deleted.  If no matching entries are found, no action will
    /// be performed.
    /// @param accession accession(s) to be deleted
    /// @param version version number to be deleted, 0 means all matching
    /// @see DeleteEntries
    void DeleteEntry(const string& accession, const int version = 0);

    /// Delete entries which match the provided list.
    /// Unlike DeleteEntry above, the version number is not checked; ALL
    /// records which match the list of accessions will be deleted.
    /// @param accessions list of string accessions
    /// @return number of rows deleted
    /// @see DeleteEntry
    int DeleteEntries(const list<string>& accessions);

    /// Add an accession-to-OID record WITH version.
    /// Database will accept non-unique accessions, but it's not
    /// recommended.
    /// @param accession accession string without ".N" version appended
    /// @param version accession's version number
    /// @param oid OID associated with accession
    /// @see InsertEntries
    void InsertEntry(const string& accession, const int version, const TOid oid);

    /// Add an accession-to-OID record WITHOUT version on accession.
    /// Database will accept non-unique accessions, but it's not
    /// recommended.  Version will be stored as zero.
    /// @param accession accession string without ".N" version appended
    /// @param oid OID associated with accession
    /// @see InsertEntries
    void InsertEntry(const string& accession, const TOid oid);

    /// Add entries in bulk as fetched from CSeqDB::GetSeqIDs.
    /// @param oid OID
    /// @param seqids list<CRef<CSeq_id> > from CSeqDB::GetSeqIDs
    /// @return number of rows added to database
    /// @see InsertEntry
    int InsertEntries(const TOid oid, const list<CRef<CSeq_id> >& seqids);

    /// Add entries in bulk from list of SAccOid structs.
    /// @param seqids list<SAccOid> from CSeqDB::GetSeqIDs
    /// @return number of rows added to database
    /// @see IsertEntry
    int InsertEntries(const list<SAccOid>& seqids);

    /// Create accession-to-OID index in database.
    /// Creation of the index is a necessary step before the SQLite database
    /// can be efficiently used by the CSqlDB_Reader class.  The index can be
    /// created at any time after the constructor is called, but for the
    /// fastest database writing this method should be called AFTER all
    /// records have been written.  Be warned, for a large database like
    /// nr or nt, this step can take MINUTES to complete.
    /// Once a database has an index created, all updates to the records
    /// will update the index as well (which is why saving the index creation
    /// for last is recommended).
    /// Calling this method on a database which already has the index
    /// will return without taking any action.
    void CreateIndex();
};

END_NCBI_SCOPE

#endif
