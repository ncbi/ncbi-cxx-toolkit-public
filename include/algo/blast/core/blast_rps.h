/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_rps.h

Author: Jason Papadopoulos

Contents: RPS BLAST structure definitions

*****************************************************************************/

#ifndef BLAST_RPS__H
#define BLAST_RPS__H

#include <algo/blast/core/blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPS_MAGIC_NUM 0x1e16    /* RPS data files contain this number */

typedef struct RPSLookupFileHeader {
    Int4 magic_number;
    Int4 num_lookup_tables;
    Int4 num_hits;
    Int4 num_filled_backbone_cells;
    Int4 overflow_hits;
    Int4 unused[3];
    Int4 start_of_backbone;
    Int4 end_of_overflow;
} RPSLookupFileHeader;

typedef struct RPSProfileHeader {
    Int4 magic_number;
    Int4 num_profiles;
    Int4 start_offsets[1]; /* variable number of Int4's beyond this point */
} RPSProfileHeader;

typedef struct RPSAuxInfo {
    Int1 orig_score_matrix[256];
    Int4 gap_open_penalty;
    Int4 gap_extend_penalty;
    double ungapped_k;
    double ungapped_h;
    Int4 max_db_seq_length;
    Int4 db_length;
    double scale_factor;
    double *karlin_k;
} RPSAuxInfo;

typedef struct RPSInfo {
    RPSLookupFileHeader *lookup_header;
    RPSProfileHeader *profile_header;
    RPSAuxInfo aux_info;
} RPSInfo;

#ifdef __cplusplus
}
#endif
#endif /* !BLAST_RPS__H */
