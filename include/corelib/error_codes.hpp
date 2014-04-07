#ifndef CORELIB___ERROR_CODES__HPP
#define CORELIB___ERROR_CODES__HPP

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
/// Definition of all error codes used in corelib (xncbi.lib).
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(Corelib_Env,        101,  5);
NCBI_DEFINE_ERRCODE_X(Corelib_Config,     102, 23);
NCBI_DEFINE_ERRCODE_X(Corelib_Blob,       103,  1);
NCBI_DEFINE_ERRCODE_X(Corelib_Static,     104,  1);
NCBI_DEFINE_ERRCODE_X(Corelib_System,     105, 13);
NCBI_DEFINE_ERRCODE_X(Corelib_App,        106, 21);
NCBI_DEFINE_ERRCODE_X(Corelib_Diag,       107, 27);
NCBI_DEFINE_ERRCODE_X(Corelib_File,       108,  4);
NCBI_DEFINE_ERRCODE_X(Corelib_Object,     109, 15);
NCBI_DEFINE_ERRCODE_X(Corelib_Reg,        110,  8);
NCBI_DEFINE_ERRCODE_X(Corelib_Util,       111,  6);
NCBI_DEFINE_ERRCODE_X(Corelib_StreamBuf,  112, 12);
NCBI_DEFINE_ERRCODE_X(Corelib_PluginMgr,  113,  4);
NCBI_DEFINE_ERRCODE_X(Corelib_Stack,      114, 12);
NCBI_DEFINE_ERRCODE_X(Corelib_Unix,       115,  2);
NCBI_DEFINE_ERRCODE_X(Corelib_StreamUtil, 116,  1);
NCBI_DEFINE_ERRCODE_X(Corelib_Threads,    117,  4);
NCBI_DEFINE_ERRCODE_X(Corelib_Dll,        118,  1);
NCBI_DEFINE_ERRCODE_X(Corelib_TestBoost,  119,  6);
NCBI_DEFINE_ERRCODE_X(Corelib_Process,    120,  2);
NCBI_DEFINE_ERRCODE_X(Corelib_Mutex,      121,  2);


END_NCBI_SCOPE


#endif  /* CORELIB___ERROR_CODES__HPP */
