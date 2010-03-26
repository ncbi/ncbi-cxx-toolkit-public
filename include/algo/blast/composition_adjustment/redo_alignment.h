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
 * ==========================================================================*/
/**
 * @file redo_alignment.h
 * Definitions used to redo a set of alignments, using either
 * composition matrix adjustment or the Smith-Waterman algorithm (or
 * both.)
 *
 * Definitions with the prefix 'BlastCompo_' are primarily intended for use
 * by glue code that interfaces with this module, i.e. the definitions
 * need to be externally available so that glue code may be written, but
 * are not intended for general use.
 *
 * @author Alejandro Schaffer, E. Michael Gertz
 */
#ifndef __REDO_ALIGNMENT__
#define __REDO_ALIGNMENT__

#include <algo/blast/composition_adjustment/composition_adjustment.h>
#include <algo/blast/composition_adjustment/composition_constants.h>
#include <algo/blast/composition_adjustment/smith_waterman.h>
#include <algo/blast/composition_adjustment/compo_heap.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Within the composition adjustment module, an object of type
 * BlastCompo_Alignment represents a distinct alignment of the query
 * sequence to the current subject sequence.  These objects are
 * typically part of a singly linked list of distinct alignments,
 * stored in the reverse of the order in which they were computed.
 */
typedef struct BlastCompo_Alignment {
    int score;           /**< the score of this alignment */
    EMatrixAdjustRule matrix_adjust_rule; 
    /**< how the score matrix was computed */
    int queryIndex;      /**< index of the query in a concatenated query */
    int queryStart;      /**< the start of the alignment in the query */
    int queryEnd;        /**< one past the end of the alignment in the query */
    int matchStart;      /**< the start of the alignment in the subject */
    int matchEnd;        /**< one past the end of the alignment in the
                              subject */
    int frame;           /**< the subject frame */
    void * context;    /**< traceback info for a gapped alignment */
    struct BlastCompo_Alignment * next;  /**< the next alignment in the
                                              list */
} BlastCompo_Alignment;


/**
 * Create a new BlastCompo_Alignment; parameters to this function
 * correspond directly to fields of BlastCompo_Alignment */
NCBI_XBLAST_EXPORT
BlastCompo_Alignment *
BlastCompo_AlignmentNew(int score,
                        EMatrixAdjustRule whichRule,
                        int queryIndex, int queryStart, int queryEnd,
                        int matchStart, int matchEnd, int frame,
                        void * context);


/**
 * Recursively free all alignments in the singly linked list whose
 * head is *palign. Set *palign to NULL.
 *
 * @param palign            pointer to the head of a singly linked list
 *                          of alignments.
 * @param free_context      a function capable of freeing the context
 *                          field of an alignment, or NULL if the
 *                          context field should not be freed
 */
NCBI_XBLAST_EXPORT
void BlastCompo_AlignmentsFree(BlastCompo_Alignment ** palign,
                               void (*free_context)(void*));


/** Parameters used to compute gapped alignments */
typedef struct BlastCompo_GappingParams {
    int gap_open;        /**< penalty for opening a gap */
    int gap_extend;      /**< penalty for extending a gapped alignment by
                              one residue */
    int decline_align;   /**< penalty for declining to align two characters */
    int x_dropoff;       /**< for x-drop algorithms, once a path falls below
                              the best score by this (positive) amount, the
                              path is no longer searched */
    void * context;      /**< a pointer to any additional gapping parameters
                              that may be needed by the calling routine. */
} BlastCompo_GappingParams;


/**
 * BlastCompo_SequenceRange - a struct whose instances represent a range
 * of data in a sequence. */
typedef struct BlastCompo_SequenceRange
{
    int begin;    /**< the starting index of the range */
    int end;      /**< one beyond the last item in the range */
    int context;  /**< integer identifier for this window, can
                       indicate a translation frame or an index into a
                       set of sequences. */
} BlastCompo_SequenceRange;


/**
 * BlastCompo_SequenceData - represents a string of amino acids or nucleotides
 */
typedef struct BlastCompo_SequenceData {
    Uint1 * data;                /**< amino acid or nucleotide data */
    int length;                  /**< the length of data. For amino acid data
                                   &data[-1] is a valid address and
                                   data[-1] == 0. */
    Uint1 * buffer;               /**< if non-nil, points to memory that
                                    must be freed when this instance of
                                    BlastCompo_SequenceData is deleted. */
} BlastCompo_SequenceData;


/**
 * A BlastCompo_MatchingSequence represents a subject sequence to be aligned
 * with the query.  This abstract sequence is used to hide the
 * complexity associated with actually obtaining and releasing the
 * data for a matching sequence, e.g. reading the sequence from a DB
 * or translating it from a nucleotide sequence.
 *
 * We draw a distinction between a sequence itself, and strings of
 * data that may be obtained from the sequence.  The amino
 * acid/nucleotide data is represented by an object of type
 * BlastCompo_SequenceData.  There may be more than one instance of
 * BlastCompo_SequenceData per BlastCompo_MatchingSequence, each representing a
 * different range in the sequence, or a different translation frame.
 */
typedef struct BlastCompo_MatchingSequence {
  Int4          length;         /**< length of this matching sequence */
  Int4          index;          /**< index of this sequence in the database */
  void * local_data;            /**< holds any sort of data that is
                                     necessary for callbacks to access
                                     the sequence */
} BlastCompo_MatchingSequence;


/** Collected information about a query */
typedef struct BlastCompo_QueryInfo {
    int origin;               /**< origin of the query in a
                                   concatenated query */
    BlastCompo_SequenceData seq;   /**< sequence data for the query */
    Blast_AminoAcidComposition composition;   /**< the composition of
                                                   the query */
    double eff_search_space;  /**< effective search space of searches
                                   involving this query */
} BlastCompo_QueryInfo;


/** Callbacks **/

/** Function type: calculate the statistical parameter Lambda from a
 * set of score probabilities.
 *
 * @param probs            an array of score probabilities
 * @param min_score        the score corresponding to probs[0]
 * @param max_score        the largest score in the probs array
 * @param lambda0          an initial guess for Lambda
 * @return Lambda
 */
typedef double
calc_lambda_type(double * probs, int min_score, int max_score,
                 double lambda0);

/**
 * Function type: Get a range of data for a sequence.
 *
 * @param sequence        a sequence
 * @param range           the range to get
 * @param data            the matching sequence data obtained
 * @param queryData       the sequence data for the query
 * @param queryOffset     offset for align if there are multiple queries
 * @param align           information about the alignment between query and subject
 * @param shouldTestIdentical did alignment pass a preliminary test in
 *                       redo_alignment.c that indicates the sequence
 *                        pieces may be near identical
 */
typedef int
get_range_type(const BlastCompo_MatchingSequence * sequence,
               const BlastCompo_SequenceRange * range,
               BlastCompo_SequenceData * data,
	       const BlastCompo_SequenceData * queryData,
	       const int queryOffset,
	       const BlastCompo_Alignment *align,
	       const Boolean shouldTestIdentical);

/**
 * Function type: Calculate the traceback for one alignment by
 * performing an x-drop alignment in both directions
 *
 * @param in_align         the existing alignment, without traceback
 * @param matrix_adjust_rule   rule used to compute the scoring matrix
 * @param whichMode        which mode of composition adjustment has
 *                         been used to adjust the scoring matrix
 * @param query_data       query sequence data
 * @param query_range      range of this query in the concatenated
 *                         query
 * @param ccat_query_length   total length of the concatenated query
 * @param subject_data     subject sequence data
 * @param subject_range    range of subject_data in the translated
 *                         query, in amino acid coordinates
 * @param full_subject_length   length of the full subject sequence
 * @param gapping_params        parameters used to compute gapped
 *                              alignments
 */
typedef BlastCompo_Alignment *
redo_one_alignment_type(BlastCompo_Alignment * in_align,
                        EMatrixAdjustRule matrix_adjust_rule,
                        BlastCompo_SequenceData * query_data,
                        BlastCompo_SequenceRange * query_range,
                        int ccat_query_length,
                        BlastCompo_SequenceData * subject_data,
                        BlastCompo_SequenceRange * subject_range,
                        int full_subject_length,
                        BlastCompo_GappingParams * gapping_params);

/**
 * Function type: Calculate the traceback for one alignment by
 * performing an x-drop alignment in the forward direction, possibly
 * increasing the x-drop parameter until the desired score is
 * attained.
 *
 * The start, end and score of the alignment should be obtained
 * using the Smith-Waterman algorithm before this routine is called.
 *
 * @param *palign          the new alignment
 * @param *pqueryEnd       on entry, the end of the alignment in the
 *                         query, as computed by the Smith-Waterman
 *                         algorithm.  On exit, the end as computed by
 *                         the x-drop algorithm
 * @param *pmatchEnd       like as *pqueryEnd, but for the subject
 *                         sequence
 * @param queryStart       the starting point in the query
 * @param matchStart       the starting point in the subject
 * @param score            the score of the alignment, as computed by
 *                         the Smith-Waterman algorithm
 * @param query            query sequence data
 * @param query_range      range of this query in the concatenated
 *                         query
 * @param ccat_query_length   total length of the concatenated query
 * @param subject          subject sequence data
 * @param subject_range    range of subject_data in the translated
 *                         query, in amino acid coordinates
 * @param full_subject_length   length of the full subject sequence
 * @param gapping_params        parameters used to compute gapped
 *                              alignments
 * @param matrix_adjust_rule   rule used to compute the scoring matrix
 * @return   0 on success, -1 for out-of-memory error
 */
typedef int
new_xdrop_align_type(BlastCompo_Alignment **palign,
                     Int4 * pqueryEnd, Int4 * pmatchEnd,
                     Int4 queryStart, Int4 matchStart, Int4 score,
                     BlastCompo_SequenceData * query,
                     BlastCompo_SequenceRange * query_range,
                     Int4 ccat_query_length,
                     BlastCompo_SequenceData * subject,
                     BlastCompo_SequenceRange * subject_range,
                     Int4 full_subject_length,
                     BlastCompo_GappingParams * gapping_params,
                     EMatrixAdjustRule matrix_adjust_rule);

/** Function type: free traceback data in a BlastCompo_Alignment.
 *
 *  @param traceback_data   data (of unknown type) to be freed
 */
typedef void free_align_traceback_type(void * traceback_data);


/** Callbacks used by Blast_RedoOneMatch and
 * Blast_RedoOneMatchSmithWaterman routines */
typedef struct Blast_RedoAlignCallbacks {
    /** @sa calc_lambda_type */
    calc_lambda_type * calc_lambda;
    /** @sa get_range_type */
    get_range_type * get_range;
    /** @sa redo_one_alignment_type */
    redo_one_alignment_type * redo_one_alignment;
    /** @sa new_xdrop_align_type */
    new_xdrop_align_type * new_xdrop_align;
    /** @sa free_align_traceback_type */
    free_align_traceback_type * free_align_traceback;
} Blast_RedoAlignCallbacks;


/** A parameter block for the Blast_RedoOneMatch and
 * Blast_RedoOneMatchSmithWaterman routines */
typedef struct Blast_RedoAlignParams {
    Blast_MatrixInfo * matrix_info;  /**< information about the scoring
                                          matrix used */
    BlastCompo_GappingParams *
        gapping_params;              /**< parameters for performing a
                                          gapped alignment */
    ECompoAdjustModes compo_adjust_mode;   /**< composition adjustment mode */
    int positionBased;      /**< true if the search is position-based */
    int RE_pseudocounts;    /**< number of pseudocounts to use in
                                 relative-entropy based composition
                                 adjustment. */
    int subject_is_translated; /**< true if the subject is translated */
    int ccat_query_length;  /**< length of the concatenated query, or
                                just the length of the query if not
                                concatenated */
    int cutoff_s;           /**< cutoff score for saving alignments when
                                 HSP linking is used */
    double cutoff_e;        /**< cutoff e-value for saving alignments */
    int do_link_hsps;       /**< if true, then HSP linking and sum
                               statistics are used to computed e-values */
    const Blast_RedoAlignCallbacks *
        callbacks;                     /**< callback functions used by
                                            the Blast_RedoAlign* functions */
} Blast_RedoAlignParams;


/** Create new Blast_RedoAlignParams object.  The parameters of this
 * function correspond directly to the fields of
 * Blast_RedoAlignParams.  The new Blast_RedoAlignParams object takes
 * possession of *pmatrix_info and *pgapping_params, so these values
 * are set to NULL on exit. */
NCBI_XBLAST_EXPORT
Blast_RedoAlignParams *
Blast_RedoAlignParamsNew(Blast_MatrixInfo ** pmatrix_info,
                         BlastCompo_GappingParams **pgapping_params,
                         ECompoAdjustModes compo_adjust_mode,
                         int positionBased,
                         int subject_is_translated,
                         int ccat_query_length, int cutoff_s,
                         double cutoff_e, int do_link_hsps,
                         const Blast_RedoAlignCallbacks * callbacks);


/** Free a set of Blast_RedoAlignParams */
NCBI_XBLAST_EXPORT
void Blast_RedoAlignParamsFree(Blast_RedoAlignParams ** pparams);


/**
 * Recompute all alignments for one query/subject pair using the
 * Smith-Waterman algorithm and possibly also composition-based
 * statistics or composition-based matrix adjustment.
 *
 * @param alignments       an array of lists containing the newly
 *                         computed alignments.  There is one array
 *                         element for each query in the original
 *                         search
 * @param params           parameters used to redo the alignments
 * @param incoming_aligns  a list of existing alignments
 * @param hspcnt           length of incoming_aligns
 * @param Lambda           statistical parameter
 * @param logK             statistical parameter
 * @param matchingSeq      the database sequence
 * @param query_info       information about all queries
 * @param numQueries       the number of queries
 * @param matrix           the scoring matrix
 * @param alphsize         the size of the alphabet
 * @param NRrecord         a workspace used to adjust the composition.
 * @param forbidden        a workspace used to hold forbidden ranges
 *                         for the Smith-Waterman algorithm.
 * @param significantMatches   an array of heaps of alignments for
 *                             query-subject pairs that have already
 *                             been redone; used to terminate the
 *                             Smith-Waterman algorithm early if it is
 *                             clear that the current match is not
 *                             significant enough to be saved.
 * @param pvalueThisPair   the compositional p-value for this pair of sequences
 * @param compositionTestIndex   index of the test function used to decide
 *                               whether to use a compositional p-value
 * @param LambdaRatio      the ratio of the actual value of Lambda to the
 *                         ideal value.
 *
 * @return 0 on success, -1 on out-of-memory
 */
NCBI_XBLAST_EXPORT
int Blast_RedoOneMatchSmithWaterman(BlastCompo_Alignment ** alignments,
                                    Blast_RedoAlignParams * params,
                                    BlastCompo_Alignment * incoming_aligns,
                                    int hspcnt,
                                    double Lambda, double logK,
                                    BlastCompo_MatchingSequence * matchingSeq,
                                    BlastCompo_QueryInfo query_info[],
                                    int numQueries,
                                    int ** matrix, int alphsize,
                                    Blast_CompositionWorkspace * NRrecord,
                                    Blast_ForbiddenRanges * forbidden,
                                    BlastCompo_Heap * significantMatches,
                                    double *pvalueThisPair,
                                    int compositionTestIndex,
                                    double *LambdaRatio);


/**
 * Recompute all alignments for one query/subject pair using
 * composition-based statistics or composition-based matrix adjustment.
 *
 * @param alignments       an array of lists containing the newly
 *                         computed alignments.  There is one array
 *                         element for each query in the original
 *                         search
 * @param params           parameters used to redo the alignments
 * @param incoming_aligns  a list of existing alignments
 * @param hspcnt           length of incoming_aligns
 * @param Lambda           statistical parameter
 * @param matchingSeq      the database sequence
 * @param ccat_query_length  the length of the concatenated query
 * @param query_info       information about all queries
 * @param numQueries       the number of queries
 * @param matrix           the scoring matrix
 * @param alphsize         the size of the alphabet
 * @param NRrecord         a workspace used to adjust the composition.
 * @param pvalueThisPair   the compositional p-value for this pair of sequences
 * @param compositionTestIndex   index of the test function used to decide
 *                               whether to use a compositional p-value
 * @param LambdaRatio      the ratio of the actual value of Lambda to the
 *                         ideal value.
 *
 * @return 0 on success, -1 on out-of-memory
 */
NCBI_XBLAST_EXPORT
int Blast_RedoOneMatch(BlastCompo_Alignment ** alignments,
                       Blast_RedoAlignParams * params,
                       BlastCompo_Alignment * incoming_aligns,
                       int hspcnt,
                       double Lambda,
                       BlastCompo_MatchingSequence * matchingSeq,
                       int ccat_query_length,
                       BlastCompo_QueryInfo query_info[],
                       int numQueries,
                       int ** matrix, int alphsize,
                       Blast_CompositionWorkspace * NRrecord,
                       double *pvalueThisPair,
                       int compositionTestIndex,
                       double *LambdaRatio);


/** Return true if a heuristic determines that it is unlikely to be
 * worthwhile to redo a query-subject pair with the given e-value; used
 * to terminate the main loop for redoing all alignments early. */
NCBI_XBLAST_EXPORT
int BlastCompo_EarlyTermination(double evalue,
                                BlastCompo_Heap significantMatches[],
                                int numQueries);

#ifdef __cplusplus
}
#endif

#endif
