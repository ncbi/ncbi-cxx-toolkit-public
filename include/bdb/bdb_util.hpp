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
 * File Description: Berkeley DB utilities. 
 *                   
 *
 */

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_cursor.hpp>

BEGIN_NCBI_SCOPE

// Template algorithm function to iterate the BDB File.
// Func() is called for every dbf record.
template<class FL, class Func> 
void BDB_iterate_file(FL& dbf, Func& func)
{
    CBDB_FileCursor cur(dbf);

    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        func(dbf);
    }
}

END_NCBI_SCOPE
/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/06/06 16:34:57  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif
