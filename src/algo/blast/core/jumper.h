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
 * Author:  Greg Boratyn
 *
 * Jumper alignment
 *
 */

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/spliced_hits.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int dcp, dcq, lng, ok; } JUMP;

/* Simple edit script for jumper: represented as integer n:
   n > 0: n matches
   n == 0: a single mismatch
   n < 0: a single insertion or deletion */
#define JUMPER_MISMATCH (0)
#define JUMPER_INSERTION (-1)
#define JUMPER_DELETION (-2)

/** Jumper edit script operation */
typedef Int2 JumperOpType;

/** Internal alignment edit script */
typedef struct JumperPrelimEditBlock
{
    JumperOpType* edit_ops;
    Int4 num_ops;
    Int4 num_allocated;
} JumperPrelimEditBlock;


/** Gapped alignment data needed for jumper */
typedef struct JumperGapAlign
{
    JumperPrelimEditBlock* left_prelim_block;
    JumperPrelimEditBlock* right_prelim_block;
    Uint4* table; /**< Table used for matching 4 bases in compressed subject
                       to 4 bases in uncompressed query */
} JumperGapAlign;


JumperGapAlign* JumperGapAlignFree(JumperGapAlign* jumper_align);
JumperGapAlign* JumperGapAlignNew(Int4 size);
Int4 JumperPrelimEditBlockAdd(JumperPrelimEditBlock* block, JumperOpType op);


/** Single alignment error */
typedef struct JumperEdit
{
    Int4 query_pos;      /**< Query position*/
    Uint1 query_base;    /**< Query base at this position */
    Uint1 subject_base;  /**< Subject base at this position */
} JumperEdit;


/** Alignment edit script for gapped alignment */
typedef struct JumperEditsBlock
{
    JumperEdit* edits;
    Int4 num_edits;
} JumperEditsBlock;


JumperEditsBlock* JumperEditsBlockFree(JumperEditsBlock* block);
JumperEditsBlock* JumperEditsBlockDup(const JumperEditsBlock* block);
JumperEditsBlock* JumperFindEdits(const Uint1* query, const Uint1* subject,
                                  BlastGapAlignStruct* gap_align);

/** Convert Jumper's preliminary edit script to GapEditScript */
GapEditScript* JumperPrelimEditBlockToGapEditScript(
                                      JumperPrelimEditBlock* rev_prelim_block,
                                      JumperPrelimEditBlock* fwd_prelim_block);


/** Test whether an HSP should be saved */
Boolean JumperGoodAlign(const BlastGapAlignStruct* gap_align,
                        const BlastHitSavingParameters* hit_params,
                        Int4 num_identical,
                        BlastContextInfo* range_info);


/** Right extension with traceback */
Int4 JumperExtendRightWithTraceback(
                                 const Uint1* query, const Uint1* subject, 
                                 int query_length, int subject_length,
                                 Int4 match_score, Int4 mismatch_score,
                                 Int4 gap_open, Int4 gap_extend,
                                 int max_mismatches, int window,
                                 Int4* query_ext_len, Int4* subject_ext_len,
                                 JumperPrelimEditBlock* edit_script,
                                 Int4* num_identical,
                                 Boolean left_extension,
                                 Int4* ungapped_ext_len,
                                 JUMP* jumper);



/** Jumper gapped alignment with traceback; 1 base per byte in query, 4 bases
    per byte in subject */
int JumperGappedAlignmentCompressedWithTraceback(const Uint1* query,
                               const Uint1* subject,
                               Int4 query_length, Int4 subject_length,
                               Int4 query_start, Int4 subject_start,
                               BlastGapAlignStruct* gap_align,
                               const BlastScoringParameters* score_params,
                               Int4* num_identical,
                               Int4* right_ungapped_ext_len);


#define SUBJECT_INDEX_WORD_LENGTH (4)

/** Index for a chunk of a subject sequence */
typedef struct SubjectIndex
{
    /** Array of lookup tables */
    BlastNaLookupTable** lookups;
    /** Number of bases covered by each lookup table */
    Int4 width;  
    /** Number of lookup tables used */
    Int4 num_lookups;
} SubjectIndex;

/** Free subject index structure */
SubjectIndex* SubjectIndexFree(SubjectIndex* sindex);

/** Index a sequence, used for indexing compressed nucleotide subject
 *  sequence. The index consists of a list of lookup tables, each covering
 *  width number of bases.
 *    
 * @param subject Sequence to be indexed [in]
 * @param width Number of bases covered by a single lookup table [in]
 * @param word_size Word size to be used in the lookup table [in]
 * @return Pointer to the created index
 */
SubjectIndex* SubjectIndexNew(BLAST_SequenceBlk* subject, Int4 width,
                              Int4 word_size);


/** Iterator over word locations in subject index */
typedef struct SubjectIndexIterator
{
    SubjectIndex* subject_index;
    Int4 word;
    Int4 from;
    Int4 to;
    Int4 lookup_index;
    Int4* lookup_pos;
    Int4 num_words;
    Int4 word_index;
} SubjectIndexIterator;


/** Free memory reserved for subject index word iterator */
SubjectIndexIterator* SubjectIndexIteratorFree(SubjectIndexIterator* it);

/** Create an iterator for locations of a given word
 * @param s_index Sequence index [in]
 * @param word Nucleotide word to be searched [in]
 * @param from Sequence location to begin search [in]
 * @param to Sequence location to end search [in]
 * @return Word location iterator
 */
SubjectIndexIterator* SubjectIndexIteratorNew(SubjectIndex* s_index, Int4 word,
                                              Int4 from, Int4 to);

/** Return the next location of a word in an indexed sequence
 * @param it Iterator [in|out]
 * @return Word location or value less than zero if no more words are found
 */
Int4 SubjectIndexIteratorNext(SubjectIndexIterator* it);

/** Return the previous location of a word in an indexed sequence
 * @param it Iterator [in|out]
 * @return Word location or value less than zero if no more words are found
 */
Int4 SubjectIndexIteratorPrev(SubjectIndexIterator* it);



/** Extend a list of word hits */
Int4 BlastNaExtendJumper(BlastOffsetPair* offset_pairs, Int4 num_hits,
                         const BlastInitialWordParameters* word_params,
                         const BlastScoringParameters* score_params,
                         const BlastHitSavingParameters* hit_params,
                         LookupTableWrap* lookup_wrap,
                         BLAST_SequenceBlk* query,
                         BLAST_SequenceBlk* subject,
                         BlastQueryInfo* query_info,
                         BlastGapAlignStruct* gap_align,
                         BlastHSPList* hsp_list,
                         Uint4 s_range,
                         SubjectIndex* s_index);


#define MAPPER_SPLICE_SIGNAL (0x80)
#define MAPPER_EXON (0x40)
#define MAPPER_POLY_A (0x20)
#define MAPPER_ADAPTER (0x10)

/** Find splice signals at the edges of an HSP and save them in the HSP
 * @param hsp HSP [in|out]
 * @param query_len Length of the query/read [in]
 * @param subject Subject sequence [in]
 * @param subject_len Subject length [in]
 * @return Zero on success
 */
int JumperFindSpliceSignals(BlastHSP* hsp, Int4 query_len,
                            const Uint1* subject, Int4 subject_len);


#define MAPPER_FLAGS_PAIR_CONVERGENT (1)
#define MAPPER_FLAGS_PAIR_DIVERGENT  (1 << 1)
#define MAPPER_FLAGS_PAIR_REARRANGED (1 << 2)

#define MAPPER_FLAGS_PAIR (MAPPER_FLAGS_PAIR_CONVERGENT | MAPPER_FLAGS_PAIR_DIVERGENT | MAPPER_FLAGS_PAIR_REARRANGED)


/* Combine edit scripts into one. Returns the combined edit script, deletes
   input scripts. */
GapEditScript* GapEditScriptCombine(GapEditScript** edit_script,
                                    GapEditScript** append);

/* Combine two edits blocks into one. Returns the combined block, deletes input
   blocks */
JumperEditsBlock* JumperEditsBlockCombine(JumperEditsBlock** block,
                                          JumperEditsBlock** append);

/** Structure to save short unaligned subsequences outside an HSP */
typedef struct SequenceOverhangs
{
    Int4 left_len;    /**< Length of the left subsequence */
    Int4 right_len;   /**< Length of the right subsequence */
    Uint1* left;      /**< Left subsequence */
    Uint1* right;     /**< Rught subsequence */
} SequenceOverhangs;

SequenceOverhangs* SequenceOverhangsFree(SequenceOverhangs* overhangs);


/** Do a search against a single subject with smaller word size and with no
 *  database word frequency filtering, for queries that have partial hits.
 *  The subject is scanned only where one expects to find an exon. This is to
 *  find smaller and low complexity exons.
 *  @param query Concatenated query [in]
 *  @param subject Subject sequence [in]
 *  @param word_size Minimum word size to use in scanning [in]
 *  @param query_info Query information structure [in]
 *  @param gap_align Data for gapped alignment [in|out]
 *  @param score_params Alignment scoring parameters [in]
 *  @param hit_params Hit saving parameters [in]
 *  @param hsp_stream Queries will be selected based on hits in hsp_stream and
 *  new hits will be written to it [in|out]
 *  @return Status
 */
Int2 DoAnchoredSearch(BLAST_SequenceBlk* query,
                      BLAST_SequenceBlk* subject,
                      Int4 word_size,
                      BlastQueryInfo* query_info,
                      BlastGapAlignStruct* gap_align,
                      const BlastScoringParameters* score_params,
                      const BlastHitSavingParameters* hit_params, 
                      BlastHSPStream* hsp_stream);


Int2 FilterQueriesForMapping(Uint1* sequence, Int4 length, Int4 offset,
                             const SReadQualityOptions* options,
                             BlastSeqLoc** seq_loc);


/** Get alignment cutoff score for a given query length. The function
    assumes that score for match is 1 */
Int4 GetCutoffScore(Int4 query_length);

#ifdef __cplusplus
}
#endif

