/* sprot.h
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
 * File Name:  sprot.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Defines SP keywords. Build SWISS-PROT format entry block.
 *
 */

#ifndef _SPROT_
#define _SPROT_

#define ParFlat_COL_DATA_SP 5

/* datablk.type: for detecting which keyword in the datablk's chain
 */
#define ParFlatSP_ID        0
#define ParFlatSP_AC        1
#define ParFlatSP_DT        2
#define ParFlatSP_DE        3
#define ParFlatSP_GN        4
#define ParFlatSP_OS        5
#define ParFlatSP_RN        6
#define ParFlatSP_CC        7
#define ParFlatSP_PE        8
#define ParFlatSP_DR        9
#define ParFlatSP_KW        10
#define ParFlatSP_FT        11
#define ParFlatSP_SQ        12
#define ParFlatSP_END       13

/* define subkeyword
 */
#define ParFlatSP_OG        20          /* organelle */
#define ParFlatSP_OC        21          /* organism classification */
#define ParFlatSP_OX        22          /* NCBI tax ids */
#define ParFlatSP_OH        23          /* NCBI tax ids and organism names
                                           of viral hosts */
#define ParFlatSP_RP        24          /* reference positions */
#define ParFlatSP_RC        25          /* reference comment */
#define ParFlatSP_RM        26          /* old (not used) reference Medline */
#define ParFlatSP_RX        27          /* reference Medline */
#define ParFlatSP_RA        28          /* reference authors */
#define ParFlatSP_RG        29          /* reference consortium */
#define ParFlatSP_RL        30          /* reference location */
#define ParFlatSP_RT        31          /* reference Title */

#endif
