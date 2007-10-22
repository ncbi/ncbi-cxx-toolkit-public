#ifndef BDB___ERROR_CODES__HPP
#define BDB___ERROR_CODES__HPP

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
 *
 */

/// @file error_codes.hpp
/// Definition of all error codes used in bdb library
/// (bdb.lib and ncbi_xcache_bdb.lib).
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(Bdb_Blob,       1001,  4);
NCBI_DEFINE_ERRCODE_X(Bdb_RangeMap,   1002,  2);
NCBI_DEFINE_ERRCODE_X(Bdb_Cursor,     1003,  2);
NCBI_DEFINE_ERRCODE_X(Bdb_Checkpoint, 1004,  7);
NCBI_DEFINE_ERRCODE_X(Bdb_Env,        1005,  9);
NCBI_DEFINE_ERRCODE_X(Bdb_File,       1006,  5);
NCBI_DEFINE_ERRCODE_X(Bdb_Util,       1007,  2);
NCBI_DEFINE_ERRCODE_X(Bdb_Volumes,    1008,  2);
NCBI_DEFINE_ERRCODE_X(Bdb_BlobCache,  1009,  31);


END_NCBI_SCOPE


#endif  /* BDB___ERROR_CODES__HPP */
