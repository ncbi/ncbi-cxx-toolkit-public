#ifndef LDS_MANAGER_HPP__
#define LDS_MANAGER_HPP__
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
 * Author: Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description: LDS database manager
 *
 */

#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_object.hpp>

#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////
//
// LDS database maintanance(modification).
//

class NCBI_LDS_EXPORT CLDS_Manager
{
public:
    ///
    /// @param source_path
    ///    path to the directory with data files
    /// @param db_path
    ///    path to the directory where LDS database will be create
    ///    if it is an empty string the path will be the same as source_path
    /// @param db_alias
    ///    alias for LDS database
    CLDS_Manager(const string& source_path,
                 const string& db_path  = kEmptyStr,
                 const string& db_alias = kEmptyStr);
   
    ~CLDS_Manager();

    /// Recursive scanning of directories in LDS source dir
    /// @sa Index
    enum ERecurse {
        eDontRecurse,      ///< Looking for files only in the given source directory
        eRecurseSubDirs    ///< Looking for files recursively in the given source directory
    };

    /// CRC32 calculation mode
    /// @sa Index
    enum EComputeControlSum {
        eNoControlSum,      ///< Don't calculate control sums for source files
        eComputeControlSum  ///< Calculate control sums for source files
    };

    /// Duplicate ids handling
    /// @sa Index
    enum EDuplicateId {
        eCheckDuplicates,   ///< if an entry with duplicated id is found throw an exception
        eIgnoreDuplicates   ///< ignore entries with duplicated ids
    };

    enum EFlags {
        fDontRecurse        = 0<<0,
        fRecurseSubDirs     = 1<<0,
        fRecurseMask        = 1<<0,

        fNoControlSum       = 0<<1,
        fComputeControlSum  = 1<<1,
        fControlSumMask     = 1<<1,

        fIgnoreDuplicates   = 0<<2,
        fCheckDuplicates    = 1<<2,
        fDupliucatesMask    = 1<<2,

        fNoGBRelease        = 0<<3,
        fGuessGBRelease     = 1<<3,
        fForceGBRelease     = 2<<3,
        fGBReleaseMask      = 3<<3,
        
        fDefaultFlags       = fRecurseSubDirs | fComputeControlSum |
                              fIgnoreDuplicates | fGuessGBRelease,
    };
    typedef int TFlags;

    /// Index(reindex) LDS database content.
    /// The main workhorse function.
    /// Function will do format guessing, files parsing, etc.
    /// Current implementation can work with only one directory.
    /// If a data file is removed all relevant objects will be cleaned up too.
    /// @param recurse
    ///   subdirectories recursion mode
    /// @param control_sum
    ///   control sum computation
    /// @param dup_control
    ///   control duplicated ids
    void Index(ERecurse           recurse     = eRecurseSubDirs,
               EComputeControlSum control_sum = eComputeControlSum,
               EDuplicateId       dup_control = eIgnoreDuplicates);
    void Index(TFlags             flags = fDefaultFlags);

    /// Delete LDS database
    void DeleteDB();

    /// Open an already existing database in read-only mode.
    /// Throw an exception on error (or, if the database does not exist).
    /// 
    /// @note Use Index() method to create or repair the database.
    CLDS_Database& GetDB();

    /// Open an already existing database in read-only mode.
    /// Throw an exception on error (or, if the database does not exist).
    /// Gives away the database object; caller will be responsible for destroying it.
    /// 
    /// @note Use Index() method to create or repair the database.
    CLDS_Database* ReleaseDB();
       
    void DumpTable(const string& table_name);

private:
    static void sx_CreateDB(CLDS_Database& lds);
    auto_ptr<CLDS_Database> x_OpenDB(CLDS_Database::EOpenMode omode);

    auto_ptr<CLDS_Database>   m_lds_db;        // LDS database object
    string m_SourcePath;
    string m_DbPath;
    string m_DbAlias;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
