#ifndef INTERNAL_SRA___ERROR_CODES__HPP
#define INTERNAL_SRA___ERROR_CODES__HPP

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
 * Authors:  Eugene Vasilchenko
 *
 */

/// @file error_codes.hpp
/// Definition of all error codes used in SRA C++ support libraries
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(SRAReader,            2101,  0);
NCBI_DEFINE_ERRCODE_X(BAMReader,            2102,  0);
NCBI_DEFINE_ERRCODE_X(SRALoader,            2103,  0);
NCBI_DEFINE_ERRCODE_X(BAMLoader,            2104,  0);
NCBI_DEFINE_ERRCODE_X(BAM2Graph,            2105,  0);
NCBI_DEFINE_ERRCODE_X(VDBReader,            2108,  0);
NCBI_DEFINE_ERRCODE_X(cSRAReader,           2109,  0);
NCBI_DEFINE_ERRCODE_X(cSRALoader,           2110,  0);
NCBI_DEFINE_ERRCODE_X(WGSReader,            2111,  0);
NCBI_DEFINE_ERRCODE_X(WGSLoader,            2112,  0);
NCBI_DEFINE_ERRCODE_X(VDBGraphReader,       2113,  0);
NCBI_DEFINE_ERRCODE_X(VDBGraphLoader,       2114,  0);
NCBI_DEFINE_ERRCODE_X(SNPReader,            2115,  0);
NCBI_DEFINE_ERRCODE_X(SNPLoader,            2116,  0);
NCBI_DEFINE_ERRCODE_X(ID2WGSProcessor,      2117,  0);
NCBI_DEFINE_ERRCODE_X(ID2SNPProcessor,      2118,  0);
NCBI_DEFINE_ERRCODE_X(WGSResolver,          2119,  0);


END_NCBI_SCOPE


#endif  /* INTERNAL_SRA___ERROR_CODES__HPP */
