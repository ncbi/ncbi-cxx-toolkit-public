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

/// This class provides search capability of (integer) OIDs keyed to
/// (string) accessions, stored in a SQLite database.
class NCBI_XOBJREAD_EXPORT CSeqDBSqlite : public CObject
{
private:
    unique_ptr<CSQLITE_Connection> m_db;
    unique_ptr<CSQLITE_Statement> m_selectStmt;

public:
    typedef Int4 TOid;

    static const TOid kNotFound;     ///< accession not found in database

    /// Struct mirroring columns of "acc2oid" table in SQLite database
    struct SAccOid {
        string m_acc;   ///< "bare" accession w/out version suffix
        int    m_ver;   ///< version number
        TOid   m_oid;   ///< OID

        /// Default constructor
        SAccOid() :
            m_acc(""), m_ver(0), m_oid((TOid) -1) {}
        /// Explicit constructor
        /// @param acc accession string
        /// @param ver version number
        /// @param oid OID
        SAccOid(const string& acc, const int ver, const int oid) :
            m_acc(acc), m_ver(ver), m_oid((TOid) oid) {}
    };

    /// Struct mirroring columes of "volinfo" table in SQLite database
    /// The "volume file" is a database volume's ".nin" or ".pin" file.
    /// The modification time is the number of seconds since the UNIX epoch,
    /// as would be returned by C library function "time()".
    struct SVolInfo {
        const string m_path;        ///< full path to volume file
        const time_t m_modTime;     ///< modification time of volume file
        const int    m_vol;         ///< volume number
        const int    m_oids;        ///< number of OIDs in volume

        /// Explicit constructor
        /// @param path full path to volume file
        /// @param modtime modification time of volume file
        /// @param vol volume number, 0 on up
        /// @param oid number of OIDs in volume
        SVolInfo(
                const string& path,
                const time_t modTime,
                const int vol,
                const int oids
        ) : m_path(path), m_modTime(modTime), m_vol(vol), m_oids(oids) {}
    };

    /// Constructor
    /// @param dbname Database file name
    CSeqDBSqlite(const string& dbname);

    /// Destructor
    ~CSeqDBSqlite();

    /// Set SQLite cache size.
    /// @param cache_size Cache size in bytes
    void SetCacheSize(const size_t cache_size);

    /// Get OIDs for single string accession and integer version.
    /// String accession should NOT include ".version".
    /// If there are no matches, oidv will be returned empty.
    /// If there are multiple matches, all matches will be returned.
    /// In actuality, if there are multiple matches that share the
    /// version number, this should be considered an error in the database.
    /// @param oidv Reference to vector of TOid to receive found OIDs
    /// @param accession String accession (without version suffix)
    /// @param version Version number
    /// @see GetOids
    /// @see kNotFound
    void GetOid(
            vector<TOid>&      oidv,
            const string&      accession,
            const unsigned int version
    );

    /// Get OIDs for single string accession.
    /// String accession may have ".version" appended.
    /// If there are no matches, oidv will be returned empty.
    /// If there are multiple matches, and allow_dup is true,
    /// all matches will be returned even if they have the same version.
    /// If there are multiple matches, and allow_dup is false,
    /// the ones with the highest version number will be returned
    /// (assuming a version was not specified).
    /// In actuality, if there are multiple matches that share the
    /// highest or provided version number, this should be considered an error
    /// in the database.
    /// @param oidv Reference to vector of TOid to receive found OIDs
    /// @param accession String accession (with or without version suffix)
    /// @param allow_dup If true, return all OIDs which match (default false)
    /// @see GetOids
    /// @see kNotFound
    void GetOid(
            vector<TOid>& oidv,
            const string& accession,
            const bool    allow_dup = false
    );

    /// Get OIDs for a vector of string accessions.
    /// Accessions may have ".version" appended.
    /// Returned vector of OIDs will have the same length as vector
    /// of accessions in one-to-one correspondence.
    /// Each accession will be searched in a similar manner to
    /// calls to GetOid (above) with allow_dup set to false.
    /// Any accessions which are not found will be assigned OIDs
    /// of kNotFound (-1).
    /// @param oidv Reference to vector of TOid to receive found OIDs
    /// @param accessions Vector of string accessions
    /// @see GetOid
    void GetOids(
            vector<TOid>&         oidv,
            const vector<string>& accessions
    );

    /// Get accessions for a single OID.
    /// OID to accession is one-to-many, so a single OID can match
    /// zero, one, or more than one accession.
    /// NOTE: At this time, the SQLite databases only have an index on the
    /// accessions, NOT on the OIDs, so this method will run MUCH slower than
    /// GetOid/GetOids.
    /// @param accessions Reference to vector of accession strings,
    ///   which will have ".version" appended.
    /// @param oid OID on which to search.
    void GetAccessions(
            vector<string>& accessions,
            const TOid      oid
    );

    /// Step through all accession-to-OID rows.
    /// Will lazily execute "SELECT * ..." upon first call.
    /// If any argument is NULL, that value will not be returned.
    /// If all arguments are NULL, only 'found' (true) or 'done' (false)
    /// will be returned.
    /// When stepping is done, this method will finalize the SELECT statement.
    ///
    /// NOTE: Stepping should be repeated until this method returns false,
    /// so that the SELECT statement can be finalized and deleted.
    ///
    /// @param acc pointer to string for accession, or NULL
    /// @param ver pointer to int for version, or NULL
    /// @param oid pointer to int for OID, or NULL
    /// @return true if row is found, or false if all rows have been returned
    bool StepAccessions(string* acc, int* ver, TOid* oid);

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
    ///
    /// NOTE: Stepping should be repeated until this method returns false,
    /// so that the SELECT statement can be finalized and deleted.
    ///
    /// @param path pointer to string for path, or NULL
    /// @param modtime pointer to int for modification time, or NULL
    /// @param volume pointer to int for volume number, or NULL
    /// @param numoids pointer to int for number of OIDS in volume, or NULL
    /// @return true if row is found, or false if all rows have been returned
    bool StepVolumes(string* path, int* modtime, int* volume, int* numoids);
};

END_NCBI_SCOPE

#endif
