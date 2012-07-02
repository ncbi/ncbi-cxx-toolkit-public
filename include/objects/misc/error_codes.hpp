#ifndef OBJECTS___ERROR_CODES__HPP
#define OBJECTS___ERROR_CODES__HPP

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
/// Definition of all error codes used in objects libraries
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(Objects_UOConv,      1301,  4);
NCBI_DEFINE_ERRCODE_X(Objects_Taxonomy,    1302, 19);
NCBI_DEFINE_ERRCODE_X(Objects_SeqIdMap,    1303,  4);
NCBI_DEFINE_ERRCODE_X(Objects_SeqAlignMap, 1304, 20);
NCBI_DEFINE_ERRCODE_X(Objects_SeqLocMap,   1305, 30);
NCBI_DEFINE_ERRCODE_X(Objects_SeqId,       1306, 11);
NCBI_DEFINE_ERRCODE_X(Objects_SeqLoc,      1307,  3);
NCBI_DEFINE_ERRCODE_X(Objects_SeqAnnot,    1308,  1);
NCBI_DEFINE_ERRCODE_X(Objects_Bioseq,      1309,  1);
NCBI_DEFINE_ERRCODE_X(Objects_Omssa,       1310,  3);
NCBI_DEFINE_ERRCODE_X(Objects_ProtRef,     1311,  2);


END_NCBI_SCOPE


#endif  /* OBJECTS___ERROR_CODES__HPP */
