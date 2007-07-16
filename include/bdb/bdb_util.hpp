#ifndef BDB_UTIL_HPP__
#define BDB_UTIL_HPP__

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: BDB library utilities. 
 *                   
 *
 */

#include <corelib/ncbireg.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_trans.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_Util
 *
 * @{
 */


/// Template algorithm function to iterate the BDB File.
/// Func() is called for every dbf record.
template<class FL, class Func> 
void BDB_iterate_file(FL& dbf, Func& func)
{
    CBDB_FileCursor cur(dbf);

    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        func(dbf);
    }
}

/// Iterate file following the sequence of ids
/// BDB file should have an integer key
template<class FL, class IT, class Func>
void BDB_iterate_file(FL& dbf, IT start, IT finish, Func& func)
{
    CBDB_BufferManager& kbuf = *dbf.GetKeyBuffer();
    _ASSERT(kbuf.FieldCount() == 1);
    CBDB_Field& fld = kbuf.GetField(0);

    for( ;start != finish; ++start) {
        unsigned id = *start;
        fld.SetInt((int)id);
        if (dbf.Fetch() == eBDB_Ok) {
            func(dbf);
        }
    } // for
}


/// Delete file records based on sequence of ids
///
/// Records are getting deleted in batches (one transaction per batch)
/// We need batches not to overflow transaction log and not to lock
/// the file for a long time (to allow concurrent access)
///
template<class IT, class Lock>
void BDB_batch_delete_recs(CBDB_File& dbf, 
                           IT start,  IT finish, 
                           unsigned   batch_size,
                           Lock*      locker)
{
    if (batch_size == 0) 
        ++batch_size;
    vector<unsigned> batch(batch_size);
    CBDB_BufferManager& kbuf = *dbf.GetKeyBuffer();
    _ASSERT(kbuf.FieldCount() == 1);
    CBDB_Field& fld = kbuf.GetField(0);

    while (start != finish) {
        batch.resize(0);
        for (unsigned i = 0; 
             i < batch_size && start != finish; 
             ++i, ++start) {

             unsigned id = (unsigned) *start;
             batch.push_back(id);
        }
        // delete the batch

        if (batch.size() == 0)
            continue;

        if (locker) locker->Lock();
        try {
            CBDB_Transaction trans(*dbf.GetEnv(), 
                                CBDB_Transaction::eTransASync);
            dbf.SetTransaction(&trans);

            ITERATE(vector<unsigned>, it, batch) {
                unsigned id = *it;
                fld.SetInt((int)id);
                dbf.Delete(CBDB_RawFile::eIgnoreError);
            }

            trans.Commit();
        } catch (...) {
            if (locker) locker->Unlock();
            throw;
        }

        if (locker) locker->Unlock();

    } // while
}


class CBoyerMooreMatcher;


/// Find index of field containing the specified value
/// Search condition is specified by CBoyerMooreMatcher
/// By default search is performed in string fields, but if 
/// tmp_str_buffer is specified function will try to convert 
/// non-strings(int, float) and search in there.
///
/// @param dbf
///    Database to search in. Should be positioned on some
///    record by Fetch or another method
/// @param matcher
///    Search condition substring matcher
/// @param tmp_str_buffer
///    Temporary string used for search in non-text fields
/// @return 
///    0 if value not found
NCBI_BDB_EXPORT
CBDB_File::TUnifiedFieldIndex 
BDB_find_field(const CBDB_File&          dbf, 
               const CBoyerMooreMatcher& matcher,
               string*                   tmp_str_buffer = 0);

/// Return record id (integer key)
///
/// @return 
///    record integer key or 0 if it cannot be retrived
int NCBI_BDB_EXPORT BDB_get_rowid(const CBDB_File& dbf);

/* @} */


/// Create and configure BDB environment using CNcbiRegistry 
/// as a parameter container
///
NCBI_BDB_EXPORT 
CBDB_Env* BDB_CreateEnv(const CNcbiRegistry& reg, 
                        const string& section_name);


END_NCBI_SCOPE

#endif
