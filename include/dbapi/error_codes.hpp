#ifndef DBAPI___ERROR_CODES__HPP
#define DBAPI___ERROR_CODES__HPP

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
/// Definition of all error codes used in dbapi libraries
/// (dbapi_driver.lib and others).
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrTypes,     1101,  8);
NCBI_DEFINE_ERRCODE_X(Dbapi_DataServer,    1102, 10);
NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrExcepts,   1103,  8);
NCBI_DEFINE_ERRCODE_X(Dbapi_ConnFactory,   1104,  3);
NCBI_DEFINE_ERRCODE_X(Dbapi_ICache,        1105,  2);
NCBI_DEFINE_ERRCODE_X(Dbapi_CacheAdmin,    1106,  3);
NCBI_DEFINE_ERRCODE_X(Dbapi_SampleBase,    1107,  5);
NCBI_DEFINE_ERRCODE_X(Dbapi_CTLib_Context, 1108, 11);
NCBI_DEFINE_ERRCODE_X(Dbapi_Dblib_Conn,    1113,  3);
NCBI_DEFINE_ERRCODE_X(Dbapi_Dblib_Context, 1114,  6);
NCBI_DEFINE_ERRCODE_X(Dbapi_Dblib_Cmds,    1115,  4);
NCBI_DEFINE_ERRCODE_X(Dbapi_Dblib_Results, 1116,  5);
NCBI_DEFINE_ERRCODE_X(Dbapi_SQLt3_Conn,    1117,  2);
NCBI_DEFINE_ERRCODE_X(Dbapi_SQLt3_Cmds,    1118,  1);
NCBI_DEFINE_ERRCODE_X(Dbapi_CTlib_Conn,    1119,  3);
NCBI_DEFINE_ERRCODE_X(Dbapi_CTlib_Cmds,    1120,  7);
NCBI_DEFINE_ERRCODE_X(Dbapi_CTlib_Results, 1121,  3);
NCBI_DEFINE_ERRCODE_X(Dbapi_Mysql_Conn,    1122,  2);
NCBI_DEFINE_ERRCODE_X(Dbapi_Mysql_Cmds,    1123,  1);
NCBI_DEFINE_ERRCODE_X(Dbapi_Odbc_Conn,     1124,  3);
NCBI_DEFINE_ERRCODE_X(Dbapi_Odbc_Context,  1125,  4);
NCBI_DEFINE_ERRCODE_X(Dbapi_Odbc_Cmds,     1126,  5);
NCBI_DEFINE_ERRCODE_X(Dbapi_Odbc_Results,  1127,  5);
NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrWinHook,   1128,  9);
NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrMemStore,  1129,  1);
NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrResult,    1130,  1);
NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrUtil,      1131,  2);
NCBI_DEFINE_ERRCODE_X(Dbapi_Python,        1132,  5);
NCBI_DEFINE_ERRCODE_X(Dbapi_Variant,       1133,  1);
NCBI_DEFINE_ERRCODE_X(Dbapi_BlobStream,    1134,  3);
NCBI_DEFINE_ERRCODE_X(Dbapi_ObjImpls,      1135, 10);
NCBI_DEFINE_ERRCODE_X(Dbapi_BulkInsert,    1136,  1);
NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrContext,   1137,  1);
NCBI_DEFINE_ERRCODE_X(Dbapi_Sdbapi,        1138, 15);


END_NCBI_SCOPE


#endif  /* DBAPI___ERROR_CODES__HPP */
