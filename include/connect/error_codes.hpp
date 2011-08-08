#ifndef CONNECT___ERROR_CODES__HPP
#define CONNECT___ERROR_CODES__HPP

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
 * Author:  Pavel Ivanov
 *
 */

/// @file error_codes.hpp
/// Definition of all error codes used in connect library
/// (xconnect.lib, xconnext.lib etc).
///

#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


// Here are only error codes used in C++ sources. For error codes used in
// C sources see src/connect/ncbi_priv.h.
NCBI_DEFINE_ERRCODE_X(Connect_Stream,    315, 10);
NCBI_DEFINE_ERRCODE_X(Connect_Pipe,      316, 16);
NCBI_DEFINE_ERRCODE_X(Connect_ThrServer, 317,  8);
NCBI_DEFINE_ERRCODE_X(Connect_Core,      318,  8);
// Caution: src/connect/ncbi_priv.h contains greater error codes


END_NCBI_SCOPE


#endif  /* CONNECT___ERROR_CODES__HPP */
