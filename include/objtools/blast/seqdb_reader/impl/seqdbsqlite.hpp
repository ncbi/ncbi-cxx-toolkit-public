#ifndef OBJTOOLS_READERS_SEQDB__SEQDBSQLITE_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBSQLITE_HPP

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

#include <objtools/blast/seqdb_reader/seqdb.hpp>

#ifdef HAVE_LIBSQLITE3
#include <db/sqlite/sqlitewrapp.hpp>
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_NCBI_SCOPE


struct SAccOid {
    string m_acc;
    int m_ver;
    int m_oid;

    SAccOid() :
        m_acc(""), m_ver(0), m_oid(-1) {}
    SAccOid(const string acc, const int ver, const int oid) :
        m_acc(acc), m_ver(ver), m_oid(oid) {}
};


struct SVolInfo {
    const string m_path;
    const time_t m_modTime;
    const int    m_vol;
    const int    m_oids;
    SVolInfo(
            const string& path,
            const time_t modTime,
            const int vol,
            const int oids
    ) : m_path(path), m_modTime(modTime), m_vol(vol), m_oids(oids) {}
};


/// This class provides search capability of (integer) OIDs keyed to
/// (string) accessions, stored in a SQLite database.

class NCBI_XOBJREAD_EXPORT CSeqDBSqlite
{
private:
    CSQLITE_Connection* m_db = NULL;
    CSQLITE_Statement* m_selectStmt = NULL;

public:
    static const int kNotFound;
    static const int kAmbiguous;

    /// Constructor
    /// @param dbname Database file name
    CSeqDBSqlite(const string& dbname);

    /// Destructor
    ~CSeqDBSqlite();

    /// Set SQLite cache size.
    /// @param cache_size Cache size in bytes
    void SetCacheSize(const size_t cache_size);

    /// Get OID for single string accession.
    /// If more than one match to the accession is found,
    /// the OID of the one with the highest version number will
    /// be returned.
    /// If there are multiple instances of the accession with the same
    /// version number but different OIDs, -2 will be returned instead
    /// of the OID.
    /// @param accession String accession (without version suffix)
    /// @return OID >= 0 if found, -1 if not found, -2 if ambiguous
    int GetOid(const string& accession);

    /// Get OIDs for a list of string accessions.
    /// Returned vector of OIDs will have the same length as accessions;
    /// returned OID values will be as defined for GetOid.
    /// @param accessions list of string accessions
    /// @return vector of OIDs, one per accession
    vector<int> GetOids(const list<string>& accessions);

    /// Get accessions for a single OID.
    /// OID to accession is one-to-many, so a single OID can match
    /// zero, one, or more than one accession.
    list<string> GetAccessions(const int oid);

    /// Step through all accession-to-OID rows.
    /// Will lazily execute "SELECT * ..." upon first call.
    /// If any argument is NULL, that value will not be returned.
    /// If all arguments are NULL, only 'found' (true) or 'done' (false)
    /// will be returned.
    /// When stepping is done, this method will finalize the SELECT statement.
    /// @param acc pointer to string for accession, or NULL
    /// @param ver pointer to int for version, or NULL
    /// @param oid pointer to int for OID, or NULL
    /// @return true if row is found, or false if all rows have been returned
    bool StepAccessions(string* acc, int* ver, int* oid);

    /// Step through all volumes.
    /// Will lazily execute "SELECT * ..." upon first call.
    /// If any argument is NULL, that value will not be returned.
    /// If all arguments are NULL, only 'found' (true) or 'done' (false)
    /// will be returned.
    /// When stepping is done, this method will finalize the SELECT statement.
    /// Modification time is identical to time_t in Standard C Library,
    /// which is the number of seconds since the UNIX Epoch.
    /// This can be converted to human-readable form with C functions
    /// such as ctime().
    /// @param path pointer to string for path, or NULL
    /// @param modtime pointer to int for modification time, or NULL
    /// @param volume pointer to int for volume number, or NULL
    /// @param numoids pointer to int for number of OIDS in volume, or NULL
    /// @return true if row is found, or false if all rows have been returned
    bool StepVolumes(string* path, int* modtime, int* volume, int* numoids);
};

END_NCBI_SCOPE

#endif
