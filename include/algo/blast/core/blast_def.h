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
 * ===========================================================================
 * 
 * Author: Ilya Dondoshansky
 *
 */

/** @file blast_def.h
 * Definitions of major structures used throughout BLAST
 */

#ifndef __BLAST_DEF__
#define __BLAST_DEF__

#include <algo/blast/core/ncbi_std.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Safe free a pointer: belongs to a higher level header. */
#ifndef sfree
#define sfree(x) __sfree((void**)&(x))
#endif
void __sfree(void** x); /* implemented in lib/util.c */

/******************** Preprocessor definitions ******************************/

/** Program type: defines the engine's notion of the different
 * applications of the BLAST algorithm */
typedef enum {
    eBlastTypeBlastn,
    eBlastTypeBlastp,
    eBlastTypeBlastx,
    eBlastTypeTblastn,
    eBlastTypeTblastx,
    eBlastTypeRpsBlast,
    eBlastTypeRpsTblastn,
    eBlastTypeUndefined
} EBlastProgramType;

/** Codons are always of length 3 */
#ifndef CODON_LENGTH
#define CODON_LENGTH 3
#endif

/** Compression ratio of nucleotide bases (4 bases in 1 byte) */
#ifndef COMPRESSION_RATIO
#define COMPRESSION_RATIO 4
#endif

/** Number of frames to which we translate in translating searches */
#ifndef NUM_FRAMES
#define NUM_FRAMES 6
#endif
#ifndef NUM_STRANDS
#define NUM_STRANDS 2
#endif

/********************* Structure definitions ********************************/

/** Structure for keeping the query masking information */
typedef struct BlastMaskLoc {
   Int4 index; /**< Index of the query sequence this mask is applied to */
   ListNode* loc_list; /**< List of mask locations */
   struct BlastMaskLoc* next; /**< Pointer to the next query mask */
} BlastMaskLoc;


/** Encapsulates masking/filtering information. */
typedef struct BlastMaskInformation {
   BlastMaskLoc* filter_slp; /**< masking locations. */
   Boolean mask_at_hash; /**< if TRUE masking used only for building lookup table. */
} BlastMaskInformation; 


/** Structure to hold a sequence. */
typedef struct BLAST_SequenceBlk {
   Uint1* sequence; /**< Sequence used for search (could be translation). */
   Uint1* sequence_start; /**< Start of sequence, usually one byte before 
                               sequence as that byte is a NULL sentinel byte.*/
   Int4     length;         /**< Length of sequence. */
   Int4	context; /**< Context of the query, needed for multi-query searches */
   Int2	frame; /**< Frame of the query, needed for translated searches */
   Int4 oid; /**< The ordinal id of the current sequence */
   Boolean sequence_allocated; /**< TRUE if memory has been allocated for 
                                  sequence */
   Boolean sequence_start_allocated; /**< TRUE if memory has been allocated 
                                        for sequence_start */
   Uint1* oof_sequence; /**< Mixed-frame protein representation of a
                             nucleotide sequence for out-of-frame alignment */
   Boolean oof_sequence_allocated; /**< TRUE if memory has been allocated 
                                        for oof_sequence */
   BlastMaskLoc* lcase_mask; /**< Locations to be masked from operations on 
                                this sequence: lookup table for query; 
                                scanning for subject. */
   Boolean lcase_mask_allocated; /**< TRUE if memory has been allocated for 
                                    lcase_mask */
} BLAST_SequenceBlk;

/** The query related information 
 */
typedef struct BlastQueryInfo {
   Int4 first_context; /**< Index of the first element of the context array */
   Int4 last_context;  /**< Index of the last element of the context array */
   int num_queries;   /**< Number of query sequences */
   Int4* context_offsets; /**< Offsets of the individual queries in the
                               concatenated super-query */
   Int4* length_adjustments; /**< Length adjustments for boundary conditions */
   Int8* eff_searchsp_array; /**< Array of effective search spaces for
                                  multiple queries. Dimension = number of 
                                  query sequences. */
   Uint4 max_length; /**< Length of the longest among the concatenated 
                        queries */
} BlastQueryInfo;

/** A structure containing two integers, used e.g. for locations for the 
 * lookup table.
 */
typedef struct SSeqRange {
   Int4 left;
   Int4 right;
} SSeqRange;

/** BlastSeqLoc is a ListNode with choice equal to the sequence local id,
 * and data->ptrvalue pointing to a SSeqRange structure defining the 
 * location interval in the sequence.
 */
#define BlastSeqLoc ListNode

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_DEF__ */
