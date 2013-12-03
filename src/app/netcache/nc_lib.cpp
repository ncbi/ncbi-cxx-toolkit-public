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
 * Author: Pavel Ivanov
 *
 * File Description: Network cache daemon
 *
 */

#include "nc_pch.hpp"

// Additional sources to avoid linking with related libraries and to force
// everybody to use CTaskServer's infrastructure of threads, diagnostics and
// application-related stuff

// define dummy CThread::GetSelf() -
//        it is mentioned in random_gen.cpp, but not actually used in netcache
namespace CThread {
static unsigned int GetSelf() {return 1;}
};
#include "../../util/random_gen.cpp"

#include "../../util/checksum.cpp"
#include "../../util/md5.cpp"
#include "../../connect/services/netservice_protocol_parser.cpp"
#define NO_COMPOUND_ID_SUPPORT
#include "../../connect/services/netcache_key.cpp"
#undef HAVE_SQLITE3ASYNC_H
#undef HAVE_SQLITE3_UNLOCK_NOTIFY
#include "../../db/sqlite/sqlitewrapp.cpp"
#undef NCBI_USE_ERRCODE_X


BEGIN_NCBI_SCOPE


const char*
CUtilException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eNoInput:      return "eNoInput";
    case eWrongCommand: return "eWrongCommand";
    case eWrongData:    return "eWrongData";
    default:     return CException::GetErrCodeString();
    }
}


unsigned g_NumberOfUnderscoresPlusOne(const string& str)
{
    unsigned underscore_count = 1;
    const char* underscore = strchr(str.c_str(), '_');
    while (underscore != NULL) {
        ++underscore_count;
        underscore = strchr(underscore + 1, '_');
    }
    return underscore_count;
}


END_NCBI_SCOPE
