/* embl.h
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
 * File Name:  embl.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Build EMBL format entry block.
 */

#ifndef _EMBL_
#define _EMBL_

#define ParFlat_COL_DATA_EMBL 5

/* datablk.type: for detecting which keyword in the datablk's chain
 */
#define ParFlat_ID            0         /* locus */
#define ParFlat_AC            1         /* accession # */
#define ParFlat_NI            2         /* date */
#define ParFlat_DT            3         /* date */
#define ParFlat_DE            4         /* description */
#define ParFlat_KW            5         /* keywords */
#define ParFlat_OS            6         /* organism species */
#define ParFlat_RN            7         /* reference number */
#define ParFlat_DR            8         /* database cross-reference */
#define ParFlat_CC            9         /* comments or notes */
#define ParFlat_FH            10        /* feature table header */
#define ParFlat_SQ            11        /* sequence header */
#define ParFlat_SV            12        /* version */
#define ParFlat_CO            13        /* contig */
#define ParFlat_AH            14
#define ParFlat_PR            15        /* Genome Project ID */
#define ParFlatEM_END         16

/* define subkeyword
 */
#define ParFlat_OC            20        /* organism classification */
#define ParFlat_OG            21        /* organelle */
#define ParFlat_RC            22        /* reference comment */
#define ParFlat_RP            23        /* reference positions */
#define ParFlat_RX            24        /* Medline ID */
#define ParFlat_RG            25        /* consortium */
#define ParFlat_RA            26        /* reference authors */
#define ParFlat_RT            27        /* reference title */
#define ParFlat_RL            28        /* reference location */
#define ParFlat_FT            29        /* feature table data */

#endif
