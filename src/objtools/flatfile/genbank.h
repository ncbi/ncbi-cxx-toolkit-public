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
 * File Name: genbank.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *
 */

#ifndef _GENBANK_
#define _GENBANK_

#define ParFlat_COL_DATA 12

/* datablk.type: for detecting which keyword in the datablk's chain */
enum EGenbankBlockType {
    ParFlat_LOCUS      = 0,
    ParFlat_DEFINITION = 1,
    ParFlat_ACCESSION  = 2,
    ParFlat_NCBI_GI    = 3,
    ParFlat_GSDB_ID    = 4,
    ParFlat_KEYWORDS   = 5,
    ParFlat_SEGMENT    = 6,
    ParFlat_SOURCE     = 7,
    ParFlat_REFERENCE  = 8,
    ParFlat_COMMENT    = 9,
    ParFlat_FEATURES   = 10,
    ParFlat_BCOUNT     = 11,
    ParFlat_ORIGIN     = 12,
    ParFlat_END        = 13,
    ParFlat_GSDBID     = 14,
    ParFlat_CONTIG     = 15,
    ParFlat_VERSION    = 16,
    ParFlat_USER       = 17,
    ParFlat_WGS        = 18,
    ParFlat_PRIMARY    = 19,
    ParFlat_MGA        = 20,
    ParFlat_PROJECT    = 21,
    ParFlat_DBLINK     = 22,

    /* define subkeyword */
    ParFlat_ORGANISM  = 23,
    ParFlat_AUTHORS   = 24,
    ParFlat_CONSRTM   = 25,
    ParFlat_TITLE     = 26,
    ParFlat_JOURNAL   = 27,
    ParFlat_STANDARD  = 28,
    ParFlat_FEATBLOCK = 29,
    ParFlat_MEDLINE   = 30,
    ParFlat_REMARK    = 31,
    ParFlat_PUBMED    = 32,
};

#endif
