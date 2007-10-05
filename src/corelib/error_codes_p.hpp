#ifndef CORELIB___ERROR_CODES_P__HPP
#define CORELIB___ERROR_CODES_P__HPP

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

/// @file error_codes_p.hpp
/// Definition of all error codes used in xncbi.lib.
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(XNcbiLibEnv, 101, 5)
NCBI_DEFINE_ERRCODE_X(XNcbiLibConfig, 102, 23)
NCBI_DEFINE_ERRCODE_X(XNcbiLibBlob, 103, 1)
NCBI_DEFINE_ERRCODE_X(XNcbiLibStatic, 104, 1)
NCBI_DEFINE_ERRCODE_X(XNcbiLibSystem, 105, 10)
NCBI_DEFINE_ERRCODE_X(XNcbiLibApp, 106, 14)
NCBI_DEFINE_ERRCODE_X(XNcbiLibDiag, 107, 16)
NCBI_DEFINE_ERRCODE_X(XNcbiLibFile, 108, 3)
NCBI_DEFINE_ERRCODE_X(XNcbiLibObject, 109, 15)
NCBI_DEFINE_ERRCODE_X(XNcbiLibReg, 110, 7)
NCBI_DEFINE_ERRCODE_X(XNcbiLibUtil, 111, 5)
NCBI_DEFINE_ERRCODE_X(XNcbiLibStreamBuf, 112, 4)


END_NCBI_SCOPE


#endif  /* DIR_DIR_DIR___HEADER_TEMPLATE__HPP */
