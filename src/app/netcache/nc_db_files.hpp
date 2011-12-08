#ifndef NETCACHE__NC_DB_FILES__HPP
#define NETCACHE__NC_DB_FILES__HPP
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
 * Authors:  Pavel Ivanov
 */

#include <corelib/ncbithr.hpp>
#include <db/sqlite/sqlitewrapp.hpp>

#include "nc_db_info.hpp"
#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


class CNCDBStat;


/// Connection to index database in NetCache storage. Database contains
/// information about storage parts containing actual blobs data.
class CNCDBIndexFile : public CSQLITE_Connection
{
public:
    /// Create connection to index database file
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBIndexFile(const string& file_name);
    virtual ~CNCDBIndexFile(void);

    /// Create entire database structure for Index file
    void CreateDatabase(void);

    /// Create new database part and save information about it.
    /// Creation time in given info structure is set to current time.
    void NewDBFile(Uint4 file_id, const string& file_name);
    /// Delete database part
    void DeleteDBFile(Uint4 file_id);
    /// Read information about all database parts in order of their creation
    void GetAllDBFiles(TNCDBFilesMap* files_map);
    /// Clean index database removing information about all database parts.
    void DeleteAllDBFiles(void);

    Uint8 GetMaxSyncLogRecNo(void);
    void SetMaxSyncLogRecNo(Uint8 rec_no);
private:
    CNCDBIndexFile(const CNCDBIndexFile&);
    CNCDBIndexFile& operator= (const CNCDBIndexFile&);
};



//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////

inline
CNCDBIndexFile::CNCDBIndexFile(const string& file_name)
    : CSQLITE_Connection(file_name,
                         fJournalDelete | fVacuumOff | fExternalMT
                         | fSyncFull | fWritesSync)
{
    SetPageSize(4096);
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_DB_FILES__HPP */
