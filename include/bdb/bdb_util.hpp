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

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_cursor.hpp>

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
CBDB_File::TUnifiedFieldIndex NCBI_BDB_EXPORT 
BDB_find_field(const CBDB_File&          dbf, 
               const CBoyerMooreMatcher& matcher,
               string*                   tmp_str_buffer = 0);

/// Return record id (integer key)
///
/// @return 
///    record integer key or 0 if it cannot be retrived
int NCBI_BDB_EXPORT BDB_get_rowid(const CBDB_File& dbf);

/* @} */


END_NCBI_SCOPE
/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/06/28 12:14:05  kuznets
 * BDB_find_field improved to search in non text fields too
 *
 * Revision 1.4  2004/03/10 14:02:55  kuznets
 * + BDB_get_rowid
 *
 * Revision 1.3  2004/03/08 13:33:19  kuznets
 * + BDB_find_field
 *
 * Revision 1.2  2003/06/10 20:07:27  kuznets
 * Fixed header files not to repeat information from the README file
 *
 * Revision 1.1  2003/06/06 16:34:57  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif
