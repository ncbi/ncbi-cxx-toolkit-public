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
#include <objtools/lds/lds_query.hpp>


#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//////////////////////////////////////////////////////////////////
//
// LDS database.
//

class NCBI_LDS_EXPORT CLDS_Database
{
public:
    // Object type string to database id map
    typedef map<string, int> TObjTypeMap;

public:

    CLDS_Database(const string& db_dir_name, 
                  const string& db_name,
                  const string& alias);

    CLDS_Database(const string& db_dir_name,
                  const string& alias);

    ~CLDS_Database();

    // Create the database. If the LDS database already exists all data will
    // be cleaned up.
    void Create();

    // Open LDS database (Read/Write mode)
    void Open();

    // Flush all cache buffers to disk
    void Sync();

    // Return reference on database tables
    SLDS_TablesCollection& GetTables() { return m_db; }

    const TObjTypeMap& GetObjTypeMap() const { return m_ObjTypeMap; }

    const string& GetAlias() const { return m_Alias; }

    void SetAlias(const string& alias) { m_Alias = alias; }

    // Loads types map from m_ObjectTypeDB to memory.
    void LoadTypeMap();

    const string& GetDirName(void) const { return m_LDS_DirName; }
    const string& GetDbName(void) const { return m_LDS_DbName; }
private:
    CLDS_Database(const CLDS_Database&);
    CLDS_Database& operator=(const CLDS_Database&);
private:
    string                 m_LDS_DirName;
    string                 m_LDS_DbName;
    string                 m_Alias;

    SLDS_TablesCollection  m_db;

    TObjTypeMap            m_ObjTypeMap;
};

//////////////////////////////////////////////////////////////////
//
// LDS database incapsulated into CObject compatible container
//

class NCBI_LDS_EXPORT CLDS_DatabaseHolder : public CObject
{
public:
    CLDS_DatabaseHolder(CLDS_Database* db = 0);
    ~CLDS_DatabaseHolder();

    void AddDatabase(CLDS_Database* db) { m_DataBases.push_back(db); }

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


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.21  2003/11/05 13:32:24  kuznets
 * Fixed a bug in GetDefaultDatabase() (resulted in core when there are no
 * databases attached)
 *
 * Revision 1.20  2003/10/29 16:22:49  kuznets
 * +CLDS_DatabaseHolder::EnumerateAliases
 *
 * Revision 1.19  2003/10/27 20:16:45  kuznets
 * +CLDS_DatabaseHolder::GetDatabase
 *
 * Revision 1.18  2003/10/27 19:18:08  kuznets
 * +CLDS_DatabaseHolder::AddDatabase
 *
 * Revision 1.17  2003/10/09 19:55:30  kuznets
 * +SetAlias function
 *
 * Revision 1.16  2003/10/09 18:10:50  kuznets
 * LoadTypeMap() made public
 *
 * Revision 1.15  2003/10/09 16:44:48  kuznets
 * Added Sync() method for LDS database to be able explicitly flush cache buffer
 * (when we at the DB update checkpoint)
 *
 * Revision 1.14  2003/10/08 18:19:14  kuznets
 * Database holder class changed to keep more than one database.
 *
 * Revision 1.13  2003/08/11 19:58:27  kuznets
 * Code clean-up: separated dir name and database name
 *
 * Revision 1.12  2003/07/31 20:00:08  kuznets
 * + CLDS_DatabaseHolder (CObject compatible vehicle for CLDs_Database)
 *
 * Revision 1.11  2003/06/23 18:41:19  kuznets
 * Added #include <lds_query.hpp>
 *
 * Revision 1.10  2003/06/18 18:46:46  kuznets
 * Minor fix.
 *
 * Revision 1.9  2003/06/16 15:39:03  kuznets
 * Removing dead code.
 *
 * Revision 1.8  2003/06/16 14:54:08  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 * Revision 1.7  2003/06/11 15:30:56  kuznets
 * Added GetObjTypeMap() member function.
 *
 * Revision 1.6  2003/06/06 16:35:51  kuznets
 * Added CLDS_Database::GetTables()
 *
 * Revision 1.5  2003/06/03 19:14:02  kuznets
 * Added lds dll export/import specifications
 *
 * Revision 1.4  2003/06/03 14:07:46  kuznets
 * Include paths changed to reflect the new directory structure
 *
 * Revision 1.3  2003/05/23 20:33:33  kuznets
 * Bulk changes in lds library, code reorganizations, implemented top level
 * objects read, metainformation persistance implemented for top level objects...
 *
 * Revision 1.2  2003/05/22 18:57:17  kuznets
 * Work in progress
 *
 * Revision 1.1  2003/05/22 13:24:45  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif

