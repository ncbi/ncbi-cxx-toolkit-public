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


NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrTypes,   1101,  2);
NCBI_DEFINE_ERRCODE_X(Dbapi_DataServer,  1102,  1);
NCBI_DEFINE_ERRCODE_X(Dbapi_DrvrExcepts, 1103,  4);
NCBI_DEFINE_ERRCODE_X(Dbapi_ConnFactory, 1104,  2);
NCBI_DEFINE_ERRCODE_X(Dbapi_ICache,      1105,  2);
NCBI_DEFINE_ERRCODE_X(Dbapi_CacheAdmin,  1106,  3);
NCBI_DEFINE_ERRCODE_X(Dbapi_SampleBase,  1107,  5);
NCBI_DEFINE_ERRCODE_X(Dbapi_CTLib,       1108,  5);


END_NCBI_SCOPE


#endif  /* DBAPI___ERROR_CODES__HPP */
