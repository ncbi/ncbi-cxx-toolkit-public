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

#include <objects/util/lds/lds_db.hpp>
#include <objects/util/lds/lds_expt.hpp>
#include <objects/util/lds/lds_files.hpp>

#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//////////////////////////////////////////////////////////////////
//
// LDS database.
//

class CLDS_Database
{
public:
    // Object type string to database id map
    typedef map<string, int> TObjTypeMap;

public:

    CLDS_Database(string db_name)
    : m_LDS_DbName(db_name)
    {}

    // Create the database. If the LDS database already exists all data will
    // be cleaned up.
    void Create();

    // Open LDS database (Read/Write mode)
    void Open();

    // Syncronize LDS database content with directory. 
    // Function will do format guessing, files parsing, etc
    void SyncWithDir(const string& dir_name);
private:
    // Loads types map from m_ObjectTypeDB to memory.
    void x_LoadTypeMap();
private:
    CLDS_Database(const CLDS_Database&);
    CLDS_Database& operator=(const CLDS_Database&);
private:
    string                 m_LDS_DbName;

    SLDS_TablesCollection  m_db;

    TObjTypeMap            m_ObjTypeMap;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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

