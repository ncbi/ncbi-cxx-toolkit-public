/* genbank.h
 *
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
 * File Name:  genbank.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 */

#ifndef _GENBANK_
#define _GENBANK_

#define ParFlat_COL_DATA   12

/* datablk.type: for detecting which keyword in the datablk's chain
 */
#define ParFlat_LOCUS      0
#define ParFlat_DEFINITION 1
#define ParFlat_ACCESSION  2
#define ParFlat_NCBI_GI    3
#define ParFlat_GSDB_ID    4
#define ParFlat_KEYWORDS   5
#define ParFlat_SEGMENT    6
#define ParFlat_SOURCE     7
#define ParFlat_REFERENCE  8
#define ParFlat_COMMENT    9
#define ParFlat_FEATURES   10
#define ParFlat_BCOUNT     11
#define ParFlat_ORIGIN     12
#define ParFlat_END        13
#define ParFlat_GSDBID     14
#define ParFlat_CONTIG     15
#define ParFlat_VERSION    16
#define ParFlat_USER       17
#define ParFlat_WGS        18
#define ParFlat_PRIMARY    19
#define ParFlat_MGA        20
#define ParFlat_PROJECT    21
#define ParFlat_DBLINK     22

/* define subkeyword
 */
#define ParFlat_ORGANISM   23
#define ParFlat_AUTHORS    24
#define ParFlat_CONSRTM    25
#define ParFlat_TITLE      26
#define ParFlat_JOURNAL    27
#define ParFlat_STANDARD   28
#define ParFlat_FEATBLOCK  29
#define ParFlat_MEDLINE    30
#define ParFlat_REMARK     31
#define ParFlat_PUBMED     32

#endif
