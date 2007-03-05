#ifndef LDS_HPP__
#define LDS_HPP__
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Main LDS include file.
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_expt.hpp>


#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CLDS_DataLoader;

//////////////////////////////////////////////////////////////////
///
/// LDS database.
///

class NCBI_LDS_EXPORT CLDS_Database
{
public:
    /// Object type string to database id map
    typedef map<string, int> TObjTypeMap;

public:
    
    NCBI_DEPRECATED_CTOR(CLDS_Database(const string& db_dir_name, 
                                       const string& db_name,
                                       const string& alias));

    CLDS_Database(const string& db_dir_name,
                  const string& alias);

    ~CLDS_Database();

    /// Create the database. If the LDS database already exists all data will
    /// be cleaned up.
    void Create();


    /// database open mode
    enum EOpenMode {
        eReadWrite,
        eReadOnly
    };

    /// Open LDS database (Read/Write mode by default)
    void Open(EOpenMode omode = eReadWrite);
    /// Reopen the LDS database
    void ReOpen();

    /// Flush all cache buffers to disk
    void Sync();

    /// Return reference on database tables
    SLDS_TablesCollection& GetTables() { return *m_db; }

    const TObjTypeMap& GetObjTypeMap() const { return m_ObjTypeMap; }

    const string& GetAlias() const { return m_Alias; }

    void SetAlias(const string& alias) { m_Alias = alias; }

    /// Loads types map from m_ObjectTypeDB to memory.
    void LoadTypeMap();

    const string& GetDirName(void) const { return m_LDS_DirName; }
    NCBI_DEPRECATED string GetDbName(void) const { return ""; }
    
    NCBI_DEPRECATED CRef<CLDS_DataLoader> GetLoader();
    NCBI_DEPRECATED void SetLoader( CRef<CLDS_DataLoader> aLoader );

private:
    CLDS_Database(const CLDS_Database&);
    CLDS_Database& operator=(const CLDS_Database&);
private:
    string                 m_LDS_DirName;
    //string                 m_LDS_DbName;
    string                 m_Alias;

    auto_ptr<SLDS_TablesCollection> m_db;

    TObjTypeMap            m_ObjTypeMap;
    
    CRef<CLDS_DataLoader>  m_Loader;
    EOpenMode              m_OpenMode;
};

//////////////////////////////////////////////////////////////////
///
/// LDS database incapsulated into CObject compatible container
///

class NCBI_LDS_EXPORT CLDS_DatabaseHolder : public CObject
{
public:
    CLDS_DatabaseHolder(CLDS_Database* db = 0);
    ~CLDS_DatabaseHolder();

    void AddDatabase(CLDS_Database* db) { m_DataBases.push_back(db); }
    void RemoveDatabase(CLDS_Database* db);
    void RemoveDatabase(int idx) { m_DataBases.erase( m_DataBases.begin() + idx ); }

    CLDS_Database* GetDefaultDatabase() 
    { 
        return m_DataBases.empty() ? 0 : *(m_DataBases.begin()); 
    }

    /// Find LDS database by the alias
    CLDS_Database* GetDatabase(const string& alias);

    /// Get LDS database by its index
    CLDS_Database* GetDatabase(int idx) { return m_DataBases[idx]; }


    // Get aliases of all registered databases
    void EnumerateAliases(vector<string>* aliases);

private:
    CLDS_DatabaseHolder(const CLDS_DatabaseHolder&);
    CLDS_DatabaseHolder& operator=(const CLDS_DatabaseHolder&);

private:

    vector<CLDS_Database*> m_DataBases;
};

/// Indexing base for sequence id, (accession or integer)
///
struct SLDS_SeqIdBase
{
    int      int_id;
    string   str_id;

    SLDS_SeqIdBase() : int_id(0) {}

    void Init() { int_id = 0; str_id.erase(); }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif
