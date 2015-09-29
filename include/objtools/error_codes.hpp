#ifndef OBJTOOLS___ERROR_CODES__HPP
#define OBJTOOLS___ERROR_CODES__HPP

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
/// Definition of all error codes used in objtools libraries
///


#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


NCBI_DEFINE_ERRCODE_X(Objtools_LDS,         1401, 12);
NCBI_DEFINE_ERRCODE_X(Objtools_LDS_Admin,   1402,  4);
NCBI_DEFINE_ERRCODE_X(Objtools_LDS_File,    1403,  5);
NCBI_DEFINE_ERRCODE_X(Objtools_LDS_Mgr,     1404,  3);
NCBI_DEFINE_ERRCODE_X(Objtools_LDS_Object,  1405, 13);
NCBI_DEFINE_ERRCODE_X(Objtools_LDS_Query,   1406,  1);
NCBI_DEFINE_ERRCODE_X(Objtools_LDS_Loader,  1407,  3);
NCBI_DEFINE_ERRCODE_X(Objtools_GBLoader,    1408,  2);
NCBI_DEFINE_ERRCODE_X(Objtools_GB_Util,     1409, 14);
NCBI_DEFINE_ERRCODE_X(Objtools_Reader,      1410,  0);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Id2Base,  1411,  0);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Split,    1412,  1);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Disp,     1413,  0);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Process,  1414,  7);
NCBI_DEFINE_ERRCODE_X(Objtools_Reader_Id1,  1415,  5);
NCBI_DEFINE_ERRCODE_X(Objtools_Reader_Id2,  1416,  1);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Pubseq,   1417,  7);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Pubseq2,  1418,  4);
NCBI_DEFINE_ERRCODE_X(Objtools_SeqDBTax,    1419,  1);
NCBI_DEFINE_ERRCODE_X(Objtools_SplitCache,  1420,  1);
NCBI_DEFINE_ERRCODE_X(Objtools_Aln_Sparse,  1421,  3);
NCBI_DEFINE_ERRCODE_X(Objtools_Aln_Conv,    1422,  2);
NCBI_DEFINE_ERRCODE_X(Objtools_CAV_Seqset,  1423,  6);
NCBI_DEFINE_ERRCODE_X(Objtools_CAV_Alnset,  1424, 15);
NCBI_DEFINE_ERRCODE_X(Objtools_CAV_Disp,    1425, 21);
NCBI_DEFINE_ERRCODE_X(Objtools_CAV_Func,    1426, 31);
NCBI_DEFINE_ERRCODE_X(Objtools_Fmt_GFF,     1427,  4);
NCBI_DEFINE_ERRCODE_X(Objtools_Fmt_Gather,  1428,  2);
NCBI_DEFINE_ERRCODE_X(Objtools_Edit,        1429,  1);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Align,    1430,  1);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Fasta,    1431,  2);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_GFF,      1432,  5);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Phrap,    1433,  2);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Feature,  1434,  6);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_RepMask,  1435,  4);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_Glimmer,  1436,  7);
NCBI_DEFINE_ERRCODE_X(Objtools_Validator,   1437,  6);
NCBI_DEFINE_ERRCODE_X(Objtools_Rd_GICache,  1438,  0);
NCBI_DEFINE_ERRCODE_X(Objtools_Fmt_CIGAR,   1439,  1);
NCBI_DEFINE_ERRCODE_X(Objtools_Fmt_SAM,     1440,  0);
NCBI_DEFINE_ERRCODE_X(Objtools_LDS2,        1441,  10);
NCBI_DEFINE_ERRCODE_X(Objtools_LDS2_Loader, 1442,  2);


END_NCBI_SCOPE


#endif  /* OBJTOOLS___ERROR_CODES__HPP */
