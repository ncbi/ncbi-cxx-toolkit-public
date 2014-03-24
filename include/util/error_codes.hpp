#ifndef UTIL___ERROR_CODES__HPP
#define UTIL___ERROR_CODES__HPP

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
/// Definition of all error codes used in util (xutil.lib).
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(Util_Thread,       201, 17);
NCBI_DEFINE_ERRCODE_X(Util_Cache,        202,  3);
NCBI_DEFINE_ERRCODE_X(Util_LVector,      203,  2);
NCBI_DEFINE_ERRCODE_X(Util_DNS,          204,  4);
NCBI_DEFINE_ERRCODE_X(Util_Stream,       205,  2);
NCBI_DEFINE_ERRCODE_X(Util_ByteSrc,      206,  1);
NCBI_DEFINE_ERRCODE_X(Util_File,         207,  1);
NCBI_DEFINE_ERRCODE_X(Util_QParse,       208,  2);
NCBI_DEFINE_ERRCODE_X(Util_Image,        209, 29);
NCBI_DEFINE_ERRCODE_X(Util_Compress,     210, 94);
NCBI_DEFINE_ERRCODE_X(Util_BlobStore,    211,  2);
NCBI_DEFINE_ERRCODE_X(Util_StaticArray,  212,  3);
NCBI_DEFINE_ERRCODE_X(Util_Scheduler,    213,  1);
NCBI_DEFINE_ERRCODE_X(Util_Unicode,      214,  3);
NCBI_DEFINE_ERRCODE_X(Util_LineReader,   215,  1);
NCBI_DEFINE_ERRCODE_X(Util_TextJoiner,   216,  1);
NCBI_DEFINE_ERRCODE_X(Util_Diff,         217,  1);


END_NCBI_SCOPE


#endif  /* UTIL___ERROR_CODES__HPP */
