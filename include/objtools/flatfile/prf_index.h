/* prf_index.h
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
 * File Name:  prf_index.h
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      Defines PRF keywords. Build PRF format entry block.
 *
 */

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
#ifndef _PRF_INDEX_
#define _PRF_INDEX_

#define ParFlat_COL_DATA_PRF 12

/* datablk.type: for detecting which keyword in the datablk's chain
 */
#define ParFlatPRF_ref       0
#define ParFlatPRF_NAME      1
#define ParFlatPRF_SOURCE    2
#define ParFlatPRF_JOURNAL   3
#define ParFlatPRF_KEYWORD   4
#define ParFlatPRF_COMMENT   5
#define ParFlatPRF_CROSSREF  6
#define ParFlatPRF_SEQUENCE  7
#define ParFlatPRF_END       8

/* define subkeyword
 */
#define ParFlatPRF_subunit   9
#define ParFlatPRF_isotype   10
#define ParFlatPRF_class     11
#define ParFlatPRF_gene_name 12
#define ParFlatPRF_linkage   13
#define ParFlatPRF_determine 14
#define ParFlatPRF_strain    15
#define ParFlatPRF_org_part  16
#define ParFlatPRF_org_state 17
#define ParFlatPRF_host      18
#define ParFlatPRF_cname     19
#define ParFlatPRF_taxon     20
#define ParFlatPRF_AUTHOR    21
#define ParFlatPRF_TITLE     22

#endif
// LCOV_EXCL_STOP
