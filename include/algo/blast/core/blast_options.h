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
 * Author:  Tom Madden
 *
 */

/** @file blast_options.h
 * Options to be used for different stages of the BLAST search.
 */

#ifndef __BLASTOPTIONS__
#define __BLASTOPTIONS__

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_message.h>
#include <algo/blast/core/blast_stat.h>


#ifdef __cplusplus
extern "C" {
#endif

/** Some default values (used when creating blast options block and for
 * command-line program defaults. When changing these defaults, please
 * remember to update the defaults in the command-line programs 
 */

/** "window" between hits to trigger an extension. */
#define BLAST_WINDOW_SIZE_PROT 40  /**< default window (all protein searches) */
#define BLAST_WINDOW_SIZE_NUCL 0   /**< default window size (blastn) */
#define BLAST_WINDOW_SIZE_MEGABLAST 0   /**< default window size 
                                          (contiguous megablast) */
#define BLAST_WINDOW_SIZE_DISC 40  /**< default window size 
                                          (discontiguous megablast) */

/** length of word to trigger an extension. */
#define BLAST_WORDSIZE_PROT 3   /**< default word size (all protein searches) */
#define BLAST_WORDSIZE_NUCL 11   /**< default word size (blastn) */
#define BLAST_WORDSIZE_MEGABLAST 28   /**< default word size (contiguous 
                                          megablast; for discontig megablast
                                          the word size is explicitly 
                                          overridden) */
#define BLAST_VARWORD_NUCL 0  /**< blastn with variable wordsize */
#define BLAST_VARWORD_MEGABLAST 1 /**< megablast with variable wordsize */

/** Protein gap costs are the defaults for the BLOSUM62 scoring matrix.
 * More gap costs are listed in BLASTOptionSetGapParams 
 */

/** cost for the existence of a gap.*/
#define BLAST_GAP_OPEN_PROT 11 /**< default gap open penalty (all 
                                    protein searches) */
#define BLAST_GAP_OPEN_NUCL 5 /**< default gap open penalty (blastn) */
#define BLAST_GAP_OPEN_MEGABLAST 0 /**< default gap open penalty (megablast
                                        with greedy gapped alignment) */

/** cost to extend a gap. */
#define BLAST_GAP_EXTN_PROT 1 /**< default gap open penalty (all 
                                   protein searches) */
#define BLAST_GAP_EXTN_NUCL 2 /**< default gap open penalty (blastn) */
#define BLAST_GAP_EXTN_MEGABLAST 0 /**< default gap open penalty (megablast)
                                        with greedy gapped alignment) */

/** neighboring word score thresholds; a threshold of zero
 *  means that only query and subject words that match exactly
 *  will go into the BLAST lookup table when it is generated 
 */
#define BLAST_WORD_THRESHOLD_BLASTP 11 /**< default neighboring threshold
                                         (blastp/rpsblast) */
#define BLAST_WORD_THRESHOLD_BLASTN 0 /**< default threshold (blastn) */
#define BLAST_WORD_THRESHOLD_BLASTX 12 /**< default threshold (blastx) */
#define BLAST_WORD_THRESHOLD_TBLASTN 13 /**< default neighboring threshold 
                                          (tblastn/rpstblastn) */
#define BLAST_WORD_THRESHOLD_TBLASTX 13 /**< default threshold (tblastx) */
#define BLAST_WORD_THRESHOLD_MEGABLAST 0 /**< default threshold (megablast) */

/** default dropoff for ungapped extension; ungapped extensions
 *  will stop when the score for the extension has dropped from
 *  the current best score by at least this much 
 */
#define BLAST_UNGAPPED_X_DROPOFF_PROT 7 /**< ungapped dropoff score for all
                                             searches except blastn */
#define BLAST_UNGAPPED_X_DROPOFF_NUCL 20 /**< ungapped dropoff score for 
                                              blastn (and megablast) */

/** default dropoff for preliminary gapped extensions */
#define BLAST_GAP_X_DROPOFF_PROT 15 /**< default dropoff (all protein-
                                         based gapped extensions) */
#define BLAST_GAP_X_DROPOFF_NUCL 30 /**< default dropoff for non-greedy
                                         nucleotide gapped extensions */
#define BLAST_GAP_X_DROPOFF_GREEDY 30 /**< default dropoff for greedy
                                         nucleotide gapped extensions */
#define BLAST_GAP_X_DROPOFF_TBLASTX 0 /**< default dropoff for tblastx */

/** default bit score that will trigger gapped extension */
#define BLAST_GAP_TRIGGER_PROT 22.0 /**< default bit score that will trigger
                                         a gapped extension for all protein-
                                         based searches */
#define BLAST_GAP_TRIGGER_NUCL 25.0  /**< default bit score that will trigger
                                         a gapped extension for blastn */

/** default dropoff for the final gapped extension with traceback */
#define BLAST_GAP_X_DROPOFF_FINAL_PROT 25 /**< default dropoff (all protein-
                                               based gapped extensions) */
#define BLAST_GAP_X_DROPOFF_FINAL_NUCL 50 /**< default dropoff for nucleotide
                                               gapped extensions) */
#define BLAST_GAP_X_DROPOFF_FINAL_TBLASTX 0 /**< default dropoff for tblastx */

/** default reward and penalty (only applies to blastn/megablast) */
#define BLAST_PENALTY -3        /**< default nucleotide mismatch score */
#define BLAST_REWARD 1          /**< default nucleotide match score */

#define BLAST_EXPECT_VALUE 10.0 /**< by default, alignments whose expect
                                     value exceeds this number are discarded */

/** Types of the lookup table */
#define MB_LOOKUP_TABLE 1  /**< megablast lookup table (includes both
                                contiguous and discontiguous megablast) */
#define NA_LOOKUP_TABLE 2  /**< blastn lookup table */
#define AA_LOOKUP_TABLE 3  /**< standard protein (blastp) lookup table */
#define PHI_AA_LOOKUP 4  /**< protein lookup table specialized for phi-blast */
#define PHI_NA_LOOKUP 5  /**< nucleotide lookup table for phi-blast */
#define RPS_LOOKUP_TABLE 6 /**< RPS lookup table (rpsblast and rpstblastn) */

/** Defaults for PSI-BLAST options */
#define PSI_INCLUSION_ETHRESH 0.002
#define PSI_PSEUDO_COUNT_CONST 9

/** Default genetic code for query and/or database */
#define BLAST_GENETIC_CODE 1  /**< Use the standard genetic code for converting
                                   groups of three nucleotide bases to protein
                                   letters */

/** Default parameters for linking HSPs */
#define BLAST_GAP_PROB 0.5
#define BLAST_GAP_PROB_GAPPED 1.0
#define BLAST_GAP_DECAY_RATE 0.5 
#define BLAST_GAP_DECAY_RATE_GAPPED 0.1
#define BLAST_GAP_SIZE 50

/** Options needed to construct a lookup table 
 * Also needed: query sequence and query length.
 */
typedef struct LookupTableOptions {
   Int4 threshold; /**< Score threshold for putting words in a lookup table */
   Int4 lut_type; /**< What kind of lookup table to construct? E.g. blastn 
                     allows for traditional and megablast style lookup table */
   Int2 word_size; /**< Determines the size of the lookup table */
   Int4 alphabet_size; /**< Size of the alphabet */
   Uint1 mb_template_length; /**< Length of the discontiguous words */
   Uint1 mb_template_type; /**< Type of a discontiguous word template */
   Int4 max_positions; /**< Max number of positions per word (MegaBlast only);
                         no restriction if 0 */
   Uint1 scan_step; /**< Step at which database sequence should be parsed */
   char* phi_pattern;  /**< PHI-BLAST pattern */
   Int4 max_num_patterns; /**< Maximal number of patterns allowed for 
                             PHI-BLAST */
   Boolean use_pssm; /**< Use a PSSM rather than a (protein) query to construct lookup table */
} LookupTableOptions;

/** Options required for setting up the query sequence */
typedef struct QuerySetUpOptions {
   char* filter_string; /**< Parseable string that determines the filtering
                             options */
   Uint1 strand_option; /**< In blastn: which strand to search: 1 = forward;
                           2 = reverse; 3 = both */
   Int4 genetic_code;     /**< Genetic code to use for translation, 
                             [t]blastx only */
} QuerySetUpOptions;

/** specifies the data structures used for bookkeeping
 *  during computation of ungapped extensions 
 */
typedef enum SeedContainerType {
    eDiagArray,         /**< use diagonal structures */
    eMbStacks,          /**< use stacks (megablast only) */
    eMaxContainerType   /**< maximum value for this enumeration */
} SeedContainerType;

/** when performing mini-extensions on hits from the
 *  blastn or megablast lookup table, this determines
 *  the direction in which the mini-extension is attempted 
 */
typedef enum SeedExtensionMethod {
    eRight,             /**< extend only to the right */
    eRightAndLeft,      /**< extend to left and right (used with AG method) */
    eMaxSeedExtensionMethod   /**< maximum value for this enumeration */
} SeedExtensionMethod;

/** Options needed for initial word finding and processing */
typedef struct BlastInitialWordOptions {
   Int4 window_size; /**< Maximal allowed distance between 2 hits in case 2 
                        hits are required to trigger the extension */
   SeedContainerType container_type; /**< How to store offset pairs for initial
                                        seeds? */
   SeedExtensionMethod extension_method; /**< How should exact matches be 
                                            extended? */
   Boolean variable_wordsize; /**< Should the partial bytes be examined for 
                             determining whether exact match is long enough? */
   Boolean ungapped_extension; /**< Should the ungapped extension be 
                                  performed? */
   double x_dropoff; /**< X-dropoff value (in bits) for the ungapped 
                         extension */
} BlastInitialWordOptions;

/** Expect values corresponding to the default cutoff
 *  scores for ungapped alignments 
 */
#define UNGAPPED_CUTOFF_E_BLASTN 0.05  /**< default ungapped evalue (blastn) */
#define UNGAPPED_CUTOFF_E_BLASTP 1e-300 /**< default ungapped evalue (blastp) */
#define UNGAPPED_CUTOFF_E_BLASTX 1.0  /**< default ungapped evalue (blastx) */
#define UNGAPPED_CUTOFF_E_TBLASTN 1.0  /**< default ungapped evalue (tblastn) */
#define UNGAPPED_CUTOFF_E_TBLASTX 1e-300  /**< default ungapped evalue (tblastx) */

/** Parameter block that contains a pointer to BlastInitialWordOptions
 * and parsed values for those options that require it 
 * (in this case x_dropoff).
 */
typedef struct BlastInitialWordParameters {
   BlastInitialWordOptions* options; /**< The original (unparsed) options. */
   Int4 x_dropoff; /**< Raw X-dropoff value for the ungapped extension */
   Int4 cutoff_score; /**< Cutoff score for saving ungapped hits. */
} BlastInitialWordParameters;
	
/** The algorithm to be used for preliminary
 *  gapped extensions
 */
typedef enum EBlastPrelimGapExt {
    eDynProgExt,                /**< standard affine gapping */
    eGreedyExt,                 /**< Greedy extension (megaBlast) */
    eGreedyWithTracebackExt     /**< Greedy extension with Traceback
                               calculated. */
} EBlastPrelimGapExt;

/** The algorithm to be used for final gapped
 *  extensions with traceback
 */
typedef enum EBlastTbackExt {
    eDynProgTbck,          /**< standard affine gapping */
    eGreedyTbck,           /**< Greedy extension (megaBlast) */
    eSmithWatermanTbck     /**< Smith-waterman finds optimal scores, then ALIGN_EX
                            to find alignment. */
} EBlastTbackExt;

/** Options used for gapped extension 
 *  These include:
 *  a. Penalties for various types of gapping;
 *  b. Drop-off values for the extension algorithms tree exploration;
 *  c. Parameters identifying what kind of extension algorithm(s) should 
 *     be used.
 */
typedef struct BlastExtensionOptions {
   double gap_x_dropoff; /**< X-dropoff value for gapped extension (in bits) */
   double gap_x_dropoff_final;/**< X-dropoff value for the final gapped 
                                  extension (in bits) */
   double gap_trigger; /**< Score in bits for starting gapped extension */
   EBlastPrelimGapExt ePrelimGapExt; /**< type of preliminary gapped extension (normally) for calculating
                              score. */
   EBlastTbackExt eTbackExt; /**< type of traceback extension. */
   Boolean compositionBasedStats; /**< if TRUE use composition-based stats. */
   Boolean skip_traceback; /**< @deprecated Is traceback information needed in results? */
} BlastExtensionOptions;

/** Computed values used as parameters for gapped alignments */
typedef struct BlastExtensionParameters {
   BlastExtensionOptions* options;
   Int4 gap_x_dropoff; /**< X-dropoff value for gapped extension (raw) */
   Int4 gap_x_dropoff_final;/**< X-dropoff value for the final gapped 
                               extension (raw) */
   Int4 gap_trigger; /**< Minimal raw score for starting gapped extension */
} BlastExtensionParameters;

/** Options used when evaluating and saving hits
 *  These include: 
 *  a. Restrictions on the number of hits to be saved;
 *  b. Restrictions on the quality and positions of hits to be saved;
 *  c. Parameters used to evaluate the quality of hits.
 */
typedef struct BlastHitSavingOptions {
   double expect_value; /**< The expect value cut-off threshold for an HSP, or
                            a combined hit if sum statistics is used */
   Int4 cutoff_score; /**< The (raw) score cut-off threshold */
   double percent_identity; /**< The percent identity cut-off threshold */

   Int4 hitlist_size;/**< Maximal number of database sequences to return
                        results for */
   Int4 prelim_hitlist_size; /**< Maximal number of database sequences to 
                               save hits after preliminary alignment */
   Int4 hsp_num_max; /**< Maximal number of HSPs to save for one database 
                        sequence */
   Int4 total_hsp_limit; /**< Maximal total number of HSPs to keep */
   Int4 hsp_range_max; /**< Maximal number of HSPs to save in a region: 
                          used for culling only */
   Boolean perform_culling;/**< Perform culling of hit lists by keeping at 
                              most a certain number of HSPs in a range
                              (not implemented) */
   /* PSI-BLAST Hit saving options */
   Int4 required_start;  /**< Start of the region required to be part of the
                            alignment */
   Int4 required_end;    /**< End of the region required to be part of the
                            alignment */
   double original_expect_value; /**< Needed for PSI-BLAST??? */

   /********************************************************************/
   /* Merge all these in a structure for clarity? */
   /* applicable to all, except blastn */
   Boolean do_sum_stats; /**< Force sum statistics to be used to combine 
                            HSPs */
   /* tblastn w/ sum statistics */
   Int4 longest_intron; /**< The longest distance between HSPs allowed for
                           combining via sum statistics with uneven gaps */
   /********************************************************************/

   Int4 min_hit_length;    /**< optional minimum alignment length; alignments
                                not at least this long are discarded */
   Boolean is_neighboring; /**< @todo FIXME: neighboring is specified by a percent 
                             identity and a minimum hit length */

   Boolean phi_align;   /**< Is this a PHI BLAST search? */
} BlastHitSavingOptions;

/** Parameter block that contains a pointer to BlastHitSavingOptions
 * and parsed values for those options that require it
 * (in this case expect value).
 */
typedef struct BlastHitSavingParameters {
   BlastHitSavingOptions* options; /**< The original (unparsed) options. */
   Int4 cutoff_score; /**< Raw cutoff score corresponding to the e-value 
                         provided by the user */
   Boolean do_sum_stats; /**< Is sum statistics used to combine HSPs? */
   double gap_prob;       /**< Probability of decay for linking HSPs */
   double gap_decay_rate; /**< Decay rate for linking HSPs */
   Int4 gap_size;          /**< Small gap size for linking HSPs */
   Int4 cutoff_small_gap; /**< Cutoff sum score for linked HSPs with small 
                             gaps */
   Int4 cutoff_big_gap; /**< Cutoff sum score for linked HSPs with big gaps */
   Boolean ignore_small_gaps; /**< Should small gaps be ignored? */
} BlastHitSavingParameters;
	

/** Scoring options block 
 *  Used to produce the BlastScoreBlk structure
 *  This structure may be needed for lookup table construction (proteins only),
 *  and for evaluating alignments. 
 */
typedef struct BlastScoringOptions {
   char* matrix;   /**< Name of the matrix containing all scores: needed for
                        finding neighboring words */
   char* matrix_path; /**< Directory path to where matrices are stored. */
   Int2 reward;      /**< Reward for a match */
   Int2 penalty;     /**< Penalty for a mismatch */
   Boolean gapped_calculation; /**< identical to the one in hit saving opts */
   Int4 gap_open;    /**< Extra penalty for starting a gap */
   Int4 gap_extend;  /**< Penalty for each gap residue */
   Int4 decline_align; /**< Cost for declining alignment (PSI-BLAST) */

   /* only blastx and tblastn (When query & subj are diff) */
   Boolean is_ooframe; /**< Should out-of-frame gapping be used in a translated
                          search? */
   Int4 shift_pen;   /**< Penalty for shifting a frame in out-of-frame 
                        gapping */
} BlastScoringOptions;

/** Scoring parameters block
 *  Contains scoring-related information that is actually used
 *  for the blast search
 */
typedef struct BlastScoringParameters {
   BlastScoringOptions *options; /**< User-provided values for these params */
   Int2 reward;      /**< Reward for a match */
   Int2 penalty;     /**< Penalty for a mismatch */
   Int4 gap_open;    /**< Extra penalty for starting a gap (scaled version) */
   Int4 gap_extend;  /**< Penalty for each gap residue  (scaled version) */
   Int4 decline_align; /**< Cost for declining alignment  (scaled version) */
   Int4 shift_pen;   /**< Penalty for shifting a frame in out-of-frame 
                        gapping (scaled version) */
   double scale_factor; /**< multiplier for all cutoff scores */
} BlastScoringParameters;

/** Options for setting up effective lengths and search spaces.  
 * The values are those the user has specified to override the real sizes.
 */
typedef struct BlastEffectiveLengthsOptions {
   Int8 db_length;    /**< Database length to be used for statistical
                         calculations */
   Int4 dbseq_num;    /**< Number of database sequences to be used for
                           statistical calculations */
   Int8 searchsp_eff; /**< Search space to be used for statistical
                           calculations */
   Boolean use_real_db_size; /**< Use real database size instead of virtual
                                database size for statistical calculations */
} BlastEffectiveLengthsOptions;

/** Parameters for setting up effective lengths and search spaces.  
 * The real database size values to be used for statistical calculations, if
 * there are no overriding values in options.
 */
typedef struct BlastEffectiveLengthsParameters {
   BlastEffectiveLengthsOptions* options; /**< User provided values for these 
                                             parameters */
   Int8 real_db_length; /**< Total database length to use in search space
                           calculations. */
   Int4 real_num_seqs;  /**< Number of subject sequences to use for search
                           space calculations */
} BlastEffectiveLengthsParameters;

/** Options used in protein BLAST only (PSI, PHI, RPS and translated BLAST)
 *  Some of these possibly should be transfered elsewhere  
 */
typedef struct PSIBlastOptions {
    /** Minimum evalue for inclusion in PSSM calculation. Needed for the first
     * stage of the PSSM calculation algorithm */
    double inclusion_ethresh;

    /** Pseudocount constant. Needed for the computing the PSSM residue 
     * frequencies */
    Int4 pseudo_count;
} PSIBlastOptions;

/** Options used to create the ReadDBFILE structure 
 *  Include database name and various information for restricting the database
 *  to a subset.
 */
typedef struct BlastDatabaseOptions {
   Int4 genetic_code;  /**< Genetic code to use for translation, 
                             tblast[nx] only */
   Uint1* gen_code_string;  /**< Genetic code string in ncbistdaa encoding,
                                 tblast[nx] only */
} BlastDatabaseOptions;

/********************************************************************************

	Functions to create options blocks with default values
	and free them after use.

*********************************************************************************/

/** Deallocate memory for QuerySetUpOptions. 
 * @param options Structure to free [in]
 */
QuerySetUpOptions* BlastQuerySetUpOptionsFree(QuerySetUpOptions* options);


/** Allocate memory for QuerySetUpOptions and fill with default values.  
 * @param options The options that have are being returned [out]
 */
Int2 BlastQuerySetUpOptionsNew(QuerySetUpOptions* *options);

/** Fill non-default contents of the QuerySetUpOptions.
 * @param options The options structure [in] [out]  
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param filter_string Parsable string of filtering options [in] 
 * @param strand_option which strand to search [in]
*/
Int2 BLAST_FillQuerySetUpOptions(QuerySetUpOptions* options,
        Uint1 program, const char *filter_string, Uint1 strand_option);


/** Deallocate memory for BlastInitialWordOptions.
 * @param options Structure to free [in]
 */
BlastInitialWordOptions*
BlastInitialWordOptionsFree(BlastInitialWordOptions* options);

/** Allocate memory for BlastInitialWordOptions and fill with default values.
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param options The options that have are being returned [out] 
*/
Int2
BlastInitialWordOptionsNew(Uint1 program, 
   BlastInitialWordOptions* *options);

/** Fill non-default values in the BlastInitialWordOptions structure.
 * @param options The options structure [in] [out] 
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param greedy Settings should assume greedy alignments [in]
 * @param window_size Size of a largest window between 2 words for the two-hit
 *                    version [in]
 * @param variable_wordsize Will only full bytes of the compressed sequence be
 *        checked in initial word extension (blastn only)? [in]
 * @param ag_blast Is AG BLAST approach used for scanning the database 
 *                 (blastn only)? [in]
 * @param mb_lookup Is Mega BLAST (12-mer based) lookup table used? [in]
 * @param xdrop_ungapped The value of the X-dropoff for ungapped extensions [in]
*/
Int2
BLAST_FillInitialWordOptions(BlastInitialWordOptions* options, 
   Uint1 program, Boolean greedy, Int4 window_size, 
   Boolean variable_wordsize, Boolean ag_blast, Boolean mb_lookup,
   double xdrop_ungapped);


/** Deallocate memory for BlastInitialWordParameters.
 * @param parameters Structure to free [in]
 */
BlastInitialWordParameters*
BlastInitialWordParametersFree(BlastInitialWordParameters* parameters);

/** Allocate memory for BlastInitialWordParameters and set x_dropoff.
 * Calling BlastInitialWordParametersNew calculates the
 * raw x_dropoff from the bit x_dropoff and puts it into
 * the x_dropoff field of BlastInitialWordParameters*.
 *
 * @param program_number Type of BLAST program [in]
 * @param word_options The initial word options [in]
 * @param hit_params The hit saving options (needed to calculate cutoff score 
 *                    for ungapped extensions) [in]
 * @param ext_params Extension parameters (containing gap trigger value) [in]
 * @param sbp Statistical (Karlin-Altschul) information [in]
 * @param query_info Query information [in]
 * @param subject_length Average subject sequence length [in]
 * @param parameters Resulting parameters [out]
*/
Int2
BlastInitialWordParametersNew(Uint1 program_number, 
   const BlastInitialWordOptions* word_options, 
   const BlastHitSavingParameters* hit_params, 
   const BlastExtensionParameters* ext_params, BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, 
   Uint4 subject_length,
   BlastInitialWordParameters* *parameters);

/** Update cutoff scores in BlastInitialWordParameters structure.
 * @param program_number Type of BLAST program [in]
 * @param hit_params The hit saving options (needed to calculate cutoff score 
 *                    for ungapped extensions) [in]
 * @param ext_params Extension parameters (containing gap trigger value) [in]
 * @param sbp Statistical (Karlin-Altschul) information [in]
 * @param query_info Query information [in]
 * @param subject_length Average subject sequence length [in]
 * @param parameters Preallocated parameters [in] [out]
*/
Int2
BlastInitialWordParametersUpdate(Uint1 program_number, 
   const BlastHitSavingParameters* hit_params, 
   const BlastExtensionParameters* ext_params, BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, Uint4 subject_length,
   BlastInitialWordParameters* parameters);

/** Deallocate memory for BlastExtensionOptions.
 * @param options Structure to free [in]
 */
BlastExtensionOptions*
BlastExtensionOptionsFree(BlastExtensionOptions* options);

/** Allocate memory for BlastExtensionOptions and fill with default values.
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param options The options that are being returned [out]
*/
Int2
BlastExtensionOptionsNew(Uint1 program, BlastExtensionOptions* *options);

/** Fill non-default values in the BlastExtensionOptions structure.
 * @param options The options structure [in] [out]
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param greedy Settings should assume greedy alignments [in]
 * @param x_dropoff X-dropoff parameter value for preliminary gapped 
 *                  extensions [in]
 * @param x_dropoff_final X-dropoff parameter value for final gapped 
 *                        extensions with traceback [in]
*/
Int2
BLAST_FillExtensionOptions(BlastExtensionOptions* options, 
   Uint1 program, Boolean greedy, double x_dropoff, 
   double x_dropoff_final);


/** Validate contents of BlastExtensionOptions.
 * @param program_number Type of BLAST program [in]
 * @param options Options to be validated [in]
 * @param blast_msg Describes any validation problems found [out]
*/
Int2 BlastExtensionOptionsValidate(Uint1 program_number, 
        const BlastExtensionOptions* options, Blast_Message* *blast_msg);

/** Calculate the raw values for the X-dropoff parameters 
 * @param blast_program Program number [in]
 * @param options Already allocated extension options [in]
 * @param sbp Structure containing statistical information [in]
 * @param query_info Query information, needed only for determining the first 
 *                   context [in]
 * @param parameters Extension parameters [out]
 */
Int2 BlastExtensionParametersNew(Uint1 blast_program, 
        const BlastExtensionOptions* options, 
        BlastScoreBlk* sbp, BlastQueryInfo* query_info, 
        BlastExtensionParameters* *parameters);

/** Deallocate memory for BlastExtensionParameters. 
 * @param parameters Structure to free [in]
 */
BlastExtensionParameters*
BlastExtensionParametersFree(BlastExtensionParameters* parameters);


/**  Deallocate memory for BlastScoringOptions. 
 * @param options Structure to free [in]
 */
BlastScoringOptions* BlastScoringOptionsFree(BlastScoringOptions* options);

/** Allocate memory for BlastScoringOptions and fill with default values. 
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param options The options that are being returned [out]
*/
Int2 BlastScoringOptionsNew(Uint1 program, BlastScoringOptions* *options);

/** Fill non-default values in the BlastScoringOptions structure. 
 * @param options The options structure [in] [out]
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param greedy_extension Is greedy extension algorithm used? [in]
 * @param penalty Mismatch penalty score (blastn only) [in]
 * @param reward Match reward score (blastn only) [in]
 * @param matrix Name of the BLAST matrix (all except blastn) [in]
 * @param gap_open Extra cost for opening a gap [in]
 * @param gap_extend Cost of a gap [in]
*/
Int2 
BLAST_FillScoringOptions(BlastScoringOptions* options, Uint1 program, 
   Boolean greedy_extension, Int4 penalty, Int4 reward, const char *matrix, 
   Int4 gap_open, Int4 gap_extend);

/** Validate contents of BlastScoringOptions.
 * @param program_number Type of BLAST program [in]
 * @param options Options to be validated [in]
 * @param blast_msg Describes any validation problems found [out]
*/
Int2
BlastScoringOptionsValidate(Uint1 program_number, 
   const BlastScoringOptions* options, Blast_Message* *blast_msg);

/** Produces copy of "old" options, with new memory allocated.
 * @param new_opt Contains copied BlastScoringOptions upon return [out]
 * @param old_opt BlastScoringOptions to be copied [in]
*/
Int2 BlastScoringOptionsDup(BlastScoringOptions* *new_opt, const BlastScoringOptions* old_opt);

/**  Deallocate memory for BlastScoringParameters.
 * @param parameters Structure to free [in]
 */
BlastScoringParameters* BlastScoringParametersFree(
                                     BlastScoringParameters* parameters);

/** Calculate scaled cutoff scores and gap penalties
 * @param options Already allocated scoring options [in]
 * @param sbp Structure containing scale factor [in]
 * @param parameters Scoring parameters [out]
 */
Int2 BlastScoringParametersNew(const BlastScoringOptions *options,
                               BlastScoreBlk* sbp, 
                               BlastScoringParameters* *parameters);

/** Deallocate memory for BlastEffectiveLengthsOptions*. 
 * @param options Structure to free [in]
 */
BlastEffectiveLengthsOptions* 
BlastEffectiveLengthsOptionsFree(BlastEffectiveLengthsOptions* options);

/** Allocate memory for BlastEffectiveLengthsOptions* and fill with 
 * default values. 
 * @param options The options that are being returned [out]
 */
Int2 BlastEffectiveLengthsOptionsNew(BlastEffectiveLengthsOptions* *options);

/** Deallocate memory for BlastEffectiveLengthsParameters*. 
 * @param parameters Structure to free [in]
 */
BlastEffectiveLengthsParameters* 
BlastEffectiveLengthsParametersFree(BlastEffectiveLengthsParameters* parameters);

/** Allocate memory for BlastEffectiveLengthsParameters 
 * @param options The user provided options [in]
 * @param db_length The database length [in]
 * @param num_seqs Number of sequences in database [in]
 * @param parameters The parameters structure returned [out]
 */
Int2 
BlastEffectiveLengthsParametersNew(const BlastEffectiveLengthsOptions* options, 
                               Int8 db_length, Int4 num_seqs,
                               BlastEffectiveLengthsParameters* *parameters);

/** Fill the non-default values in the BlastEffectiveLengthsOptions structure.
 * @param options The options [in] [out]
 * @param dbseq_num Number of sequences in the database (if zero real value will be used) [in]
 * @param db_length Total length of the database (if zero real value will be used) [in]
 * @param searchsp_eff Effective search space (if zero real value will be used) [in]
 */
Int2 
BLAST_FillEffectiveLengthsOptions(BlastEffectiveLengthsOptions* options, 
   Int4 dbseq_num, Int8 db_length, Int8 searchsp_eff);


/** Allocate memory for lookup table options and fill with default values.
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param options The options that are being returned [out]
 */
Int2 LookupTableOptionsNew(Uint1 program, LookupTableOptions* *options);

/** Auxiliary function that calculates best database scanning stride for the
 * given parameters.
 * @param word_size Length of the exact match required to trigger 
 *                  extensions [in]
 * @param var_words If true, and word_size is divisible by 4, partial bytes 
 *                  need not be checked to test the length of the 
 *                  exact match [in]
 * @param lut_type  What kind of lookup table is used (based on 4-mers, 8-mers 
 *                  or 12-mers) [in]
 * @return          The stride necessary to find all exact matches of a given
 *                  word size.
 */
Int4 CalculateBestStride(Int4 word_size, Boolean var_words, Int4 lut_type);


/** Allocate memory for lookup table options and fill with default values.
 * @param options The options [in] [out]
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param is_megablast Megablast (instead of blastn) if TRUE [in]
 * @param threshold Threshold value for finding neighboring words [in]
 * @param word_size Number of matched residues in an initial word [in]
 * @param ag_blast Is AG BLAST approach to database scanning used? [in]
 * @param variable_wordsize Are only full bytes of a compressed sequence 
 *        checked to find initial words? [in]
 * @param use_pssm Use PSSM rather than (protein) query to build lookup table.
 */
Int2 
BLAST_FillLookupTableOptions(LookupTableOptions* options, 
   Uint1 program, Boolean is_megablast, Int4 threshold,
   Int2 word_size, Boolean ag_blast, Boolean variable_wordsize,
   Boolean use_pssm);


/** Deallocates memory for LookupTableOptions*.
 * @param options Structure to free [in]
 */
LookupTableOptions*
LookupTableOptionsFree(LookupTableOptions* options);

/** Validate LookupTableOptions.
 * @param program_number BLAST program [in]
 * @param options The options that have are being returned [in]
 * @param blast_msg The options that have are being returned [out]
*/
Int2
LookupTableOptionsValidate(Uint1 program_number, 
   const LookupTableOptions* options,  Blast_Message* *blast_msg);

/** Deallocate memory for BlastHitSavingOptions. 
 * @param options Structure to free [in]
 */
BlastHitSavingOptions*
BlastHitSavingOptionsFree(BlastHitSavingOptions* options);

/** Validate BlastHitSavingOptions
 * @param program_number BLAST program [in]
 * @param options The options that have are being returned [in]
 * @param blast_msg The options that have are being returned [out]
*/

Int2
BlastHitSavingOptionsValidate(Uint1 program_number,
   const BlastHitSavingOptions* options, Blast_Message* *blast_msg);

/** Allocate memory for BlastHitSavingOptions.
 * @param program Program number (blastn, blastp, etc.) [in]
 * @param options The options that are being returned [out]
*/
Int2 BlastHitSavingOptionsNew(Uint1 program, 
        BlastHitSavingOptions* *options);

/** Allocate memory for BlastHitSavingOptions.
 * @param options The options [in] [out]
 * @param evalue The expected value threshold [in]
 * @param hitlist_size How many database sequences to save per query? [in]
*/
Int2
BLAST_FillHitSavingOptions(BlastHitSavingOptions* options, 
                           double evalue, Int4 hitlist_size);

/** Deallocate memory for BlastHitSavingOptions*. 
 * @param parameters Structure to free [in]
 */
BlastHitSavingParameters*
BlastHitSavingParametersFree(BlastHitSavingParameters* parameters);

/** Allocate memory and initialize the BlastHitSavingParameters structure. 
 * Calculates the (raw) score cutoff given an expect value and puts
 * it in the "cutoff_score" field of the returned BlastHitSavingParameters*
 *
 * @param program_number Number of the BLAST program [in]
 * @param options The given hit saving options [in]
 * @param ext_params Extension parameters containing the gap trigger value [in]
 * @param sbp Scoring block, needed for calculating score cutoff from 
 *            e-value [in]
 * @param query_info Query information, needed for calculating score cutoff 
 *                   from e-value [in]
 * @param parameters Resulting parameters [out]
 */
Int2 BlastHitSavingParametersNew(Uint1 program_number, 
        const BlastHitSavingOptions* options, 
        const BlastExtensionParameters* ext_params,
        BlastScoreBlk* sbp, BlastQueryInfo* query_info, 
        BlastHitSavingParameters* *parameters);

/** Updates cutoff scores in hit saving parameters. 
 * @param program_number Number of the BLAST program [in]
 * @param ext_params Extension parameters containing the gap trigger 
 *                   value [in]
 * @param sbp Scoring block, needed for calculating score cutoff from 
 *            e-value [in]
 * @param query_info Query information, needed for calculating score cutoff 
 *                   from e-value [in]
 * @param parameters Preallocated parameters [in] [out]
 */
Int2 BlastHitSavingParametersUpdate(Uint1 program_number, 
        const BlastExtensionParameters* ext_params,
        BlastScoreBlk* sbp, BlastQueryInfo* query_info, 
        BlastHitSavingParameters* parameters);

/** Initialize default options for PSI BLAST */
Int2 PSIBlastOptionsNew(PSIBlastOptions** psi_options);

/** Deallocate PSI BLAST options */
PSIBlastOptions* PSIBlastOptionsFree(PSIBlastOptions* psi_options);

/** Allocates the BlastDatabase options structure and sets the default
 * database genetic code value (BLAST_GENETIC_CODE). Genetic code string in
 * ncbistdaa must be populated by client code */
Int2 BlastDatabaseOptionsNew(BlastDatabaseOptions** db_options);

/** Deallocate database options */
BlastDatabaseOptions* 
BlastDatabaseOptionsFree(BlastDatabaseOptions* db_options);

/** Initialize all the BLAST search options structures with the default
 * values.
 * @param blast_program Type of blast program: blastn, blastp, blastx, 
 *                      tblastn, tblastx) [in]
 * @param lookup_options Lookup table options [out]
 * @param query_setup_options Query options [out]
 * @param word_options Initial word processing options [out]
 * @param ext_options Extension options [out]
 * @param hit_options Hit saving options [out]
 * @param score_options Scoring options [out]
 * @param eff_len_options Effective length options [out]
 * @param protein_options Protein BLAST options [out]
 * @param db_options BLAST database options [out]
 */
Int2 BLAST_InitDefaultOptions(Uint1 blast_program,
   LookupTableOptions** lookup_options,
   QuerySetUpOptions** query_setup_options, 
   BlastInitialWordOptions** word_options,
   BlastExtensionOptions** ext_options,
   BlastHitSavingOptions** hit_options,
   BlastScoringOptions** score_options,
   BlastEffectiveLengthsOptions** eff_len_options,
   PSIBlastOptions** protein_options,
   BlastDatabaseOptions** db_options);

/** Validate all options */
Int2 BLAST_ValidateOptions(Uint1 program_number,
                           const BlastExtensionOptions* ext_options,
                           const BlastScoringOptions* score_options, 
                           const LookupTableOptions* lookup_options, 
                           const BlastHitSavingOptions* hit_options,
                           Blast_Message* *blast_msg);

/** Calculates cutoff scores and returns them.
 *	Equations provided by Stephen Altschul.
 * @param program BLAST program type [in]
 * @param query_info Query(ies) information [in]
 * @param sbp Scoring statistical parameters [in]
 * @param hit_params Hit saving parameters, including all cutoff 
 *                   scores [in] [out]
 * @param ext_params Extension parameters (gap_trigger used) [in]
 * @param db_length Total length of database (non-database search if 0) [in]
 * @param subject_length Length of the subject sequence. [in]
 * 
*/
void
CalculateLinkHSPCutoffs(Uint1 program, BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, BlastHitSavingParameters* hit_params, 
   BlastExtensionParameters* ext_params,
   Int8 db_length, Int4 subject_length);

#ifdef __cplusplus
}
#endif
#endif /* !__BLASTOPTIONS__ */

