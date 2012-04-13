#ifndef SERIAL___ERROR_CODES__HPP
#define SERIAL___ERROR_CODES__HPP

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
/// Definition of all error codes used in serial libraries
/// (xser.lib, xcser.lib).
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(Serial_Core,       801,  5);
NCBI_DEFINE_ERRCODE_X(Serial_OStream,    802, 13);
NCBI_DEFINE_ERRCODE_X(Serial_IStream,    803,  9);
NCBI_DEFINE_ERRCODE_X(Serial_TypeInfo,   804,  3);
NCBI_DEFINE_ERRCODE_X(Serial_MemberInfo, 805,  2);
NCBI_DEFINE_ERRCODE_X(Serial_ASNTypes,   806,  3);
NCBI_DEFINE_ERRCODE_X(Serial_Parsers,    807, 10);
NCBI_DEFINE_ERRCODE_X(Serial_Modules,    808,  8);
NCBI_DEFINE_ERRCODE_X(Serial_MainGen,    809, 19);
NCBI_DEFINE_ERRCODE_X(Serial_RPCGen,     810,  3);
NCBI_DEFINE_ERRCODE_X(Serial_FileCode,   811,  7);
NCBI_DEFINE_ERRCODE_X(Serial_Util,       812,  3);
NCBI_DEFINE_ERRCODE_X(Serial_Lexer,      813, 14);
NCBI_DEFINE_ERRCODE_X(Serial_DataTool,   814,  4);
NCBI_DEFINE_ERRCODE_X(Serial_DTType,     815,  3);
NCBI_DEFINE_ERRCODE_X(Serial_DTValue,    816, 18);
NCBI_DEFINE_ERRCODE_X(Serial_RPCClient,  817,  1);


END_NCBI_SCOPE


#endif  /* SERIAL___ERROR_CODES__HPP */
