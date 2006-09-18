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
 * Author: Ilya Dondoshansky
 *
 * Initial Version Creation Date:  November 13, 2003
 *
 *
 * File Description:
 *          C++ version of repeats filtering
 *
 * */

/// @file repeats_filter_cxx.cpp
/// C++ version of repeats filtering

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/blast/api/blast_types.hpp>

#include <algo/blast/api/seqsrc_seqdb.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include "repeats_filter.hpp"
#include "blast_setup.hpp"

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_filter.h>

#include <algo/blast/api/blast_aux.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/** Convert a list of mask locations to a CSeq_loc object.
 * @param query Query sequence location [in]
 * @param loc_list List of mask locations [in]
 * @return List of mask locations in a CSeq_loc form or NULL if loc_list is
 * NULL
 */
static CSeq_loc* 
s_BlastSeqLoc2CSeqloc(const CSeq_loc & query,
                      CScope         * scope,
                      BlastSeqLoc    * loc_list)
{
    if ( !loc_list ) {
        return NULL;
    }

    CSeq_loc* seqloc = new CSeq_loc();
    BlastSeqLoc* loc;

    seqloc->SetNull();
    for (loc = loc_list; loc; loc = loc->next) {
       seqloc->SetPacked_int().AddInterval(
           sequence::GetId(query, scope),
           loc->ssr->left, loc->ssr->right);
    }
    
    return seqloc;
}

/** Convert a list of mask locations to a CSeq_loc object.
 * @param query Query sequence location [in]
 * @param loc_list List of mask locations [in]
 * @return List of mask locations in a CSeq_loc form.
 */
static CSeq_loc* 
s_BlastSeqLoc2CSeqloc(SSeqLoc& query, BlastSeqLoc* loc_list)
{
    return s_BlastSeqLoc2CSeqloc(*query.seqloc, &*query.scope, loc_list);
}

/** Convert a list of mask locations to TMaskedQueryRegions.
 * @param query Query sequence location [in]
 * @param scope Scope for use by object manager [in]
 * @param loc_list List of mask locations [in]
 * @param program type of blast search [in]
 * @return List of mask locations in TMaskedQueryRegions form.
 */
TMaskedQueryRegions
s_BlastSeqLoc2MaskedRegions(const CSeq_loc    & query,
                            CScope            * scope,
                            BlastSeqLoc       * loc_list,
                            EBlastProgramType   program)
{
    CConstRef<CSeq_loc> sloc(s_BlastSeqLoc2CSeqloc(query, scope, loc_list));
    
    return PackedSeqLocToMaskedQueryRegions(sloc, program);
}


/// Build a list of BlastSeqLoc's from a set of Dense-seg contained in a
/// Seq-align-set.
///
/// This function processes Dense-segs, and adds the range of each hit to
/// a list of BlastSeqLoc structures.  Frame information is used to
/// translate hit coordinates hits to the plus strand.  All of the
/// HSPs should refer to the same query; both the query and subject in
/// the HSP are ignored.  This is used to construct a set of filtered
/// areas from hits against a repeats database.
///
/// @param alignment    Seq-align-set containing Dense-segs which specify the
///                     ranges of hits. [in]
/// @param locs         Filtered areas for this query are added here. [out]

static void
s_SeqAlignToBlastSeqLoc(const CSeq_align_set& alignment, 
                        BlastSeqLoc ** locs)
{
    ITERATE(CSeq_align_set::Tdata, dense_seg, alignment.Get()) {
        _ASSERT((*dense_seg)->GetSegs().IsDenseg());
        const CDense_seg& seg = (*dense_seg)->GetSegs().GetDenseg();
        const int kNumSegments = seg.GetNumseg();
#if _DEBUG      /* to eliminate compiler warning in release mode */
        const int kNumDim = seg.GetDim();
#endif
        _ASSERT(kNumDim == 2);

        const CDense_seg::TStarts& starts = seg.GetStarts();
        const CDense_seg::TLens& lengths = seg.GetLens();
        const CDense_seg::TStrands& strands = seg.GetStrands();
        _ASSERT(kNumSegments*kNumDim == (int) starts.size());
        _ASSERT(kNumSegments == (int) lengths.size());
        _ASSERT(kNumSegments*kNumDim == (int) strands.size());

        int left(0), right(0);

        if (strands[0] == strands[1]) {
            left = starts.front();
            right = starts[(kNumSegments-1)*2] + lengths[kNumSegments-1] - 1;
        } else {
            left = starts[(kNumSegments-1)*2];
            right = starts.front() + lengths.front() - 1;
        }

        BlastSeqLocNew(locs, left, right);
    }
}

/** Fills the mask locations in the query SSeqLoc structures, as if it was a 
 * lower case mask, given the results of a BLAST search against a database of 
 * repeats.
 * @param query Vector of query sequence locations structures [in] [out]
 * @param results alignments returned from a BLAST search against a repeats 
 *                database [in]
 */
static void
s_FillMaskLocFromBlastResults(TSeqLocVector& query, 
                              const CSearchResultSet& results)
{
    _ASSERT(results.GetNumResults() == query.size());
    
    for (size_t query_index = 0; query_index < query.size(); ++query_index) {
        const CSearchResults& result = results[query_index];

        if (result.GetSeqAlign().Empty() || result.GetSeqAlign()->IsEmpty()) {
            continue;
        }

        // Get the previous mask locations
        BlastSeqLoc* loc_list = CSeqLoc2BlastSeqLoc(query[query_index].mask);
        
        // Find all HSP intervals in query
        ITERATE(CSeq_align_set::Tdata, alignment, result.GetSeqAlign()->Get()) {
            _ASSERT((*alignment)->GetSegs().IsDisc());
            s_SeqAlignToBlastSeqLoc((*alignment)->GetSegs().GetDisc(), 
                                    &loc_list);
        }
        
        // Make the intervals unique
        BlastSeqLoc* ordered_loc_list =
            BlastSeqLocCombine(loc_list, REPEAT_MASK_LINK_VALUE);
        
        // Free the list of locations that's no longer needed.
        loc_list = BlastSeqLocFree(loc_list);

        /* Create a CSeq_loc with these locations and fill it for the 
           respective query */
        CRef<CSeq_loc> filter_seqloc(s_BlastSeqLoc2CSeqloc(query[query_index],
                                                           ordered_loc_list));

        // Free the combined mask list in the BlastSeqLoc form.
        ordered_loc_list = BlastSeqLocFree(ordered_loc_list);

        query[query_index].mask.Reset(filter_seqloc);
    }
}

/** Fills the mask locations in the BlastSearchQuery structures, as if it was a
 * lower case mask, given the results of a BLAST search against a database of 
 * repeats.
 * @param query Vector of queries [in] [out]
 * @param results alignments returned from a BLAST search against a repeats 
 *                database [in]
 * @param program type of blast search [in]
 */
static void
s_FillMaskLocFromBlastResults(CBlastQueryVector& query,
                              const CSearchResultSet& results,
                              EBlastProgramType program)
{
    _ASSERT(results.GetNumResults() == query.Size());
    
    for (size_t qindex = 0; qindex < query.Size(); ++qindex) {
        const CSearchResults& result = results[qindex];
        
        if (result.GetSeqAlign().Empty() || result.GetSeqAlign()->IsEmpty()) {
            continue;
        }
        
        // Get the previous mask locations
        TMaskedQueryRegions mqr = query.GetMaskedRegions(qindex);
        
        CRef<CBlastQueryFilteredFrames> frames
            (new CBlastQueryFilteredFrames(program, mqr));
        
        vector<CBlastQueryFilteredFrames::ETranslationFrame> used
            = frames->ListFrames();
        
        BlastSeqLoc* loc_list = 0;
        
        for (size_t frame_i = 0; frame_i < used.size(); frame_i++) {
            // Pick frame +1 for nucleotide, or 0 (the only one) for protein.
            int pframe = used[frame_i];
            
            BlastSeqLoc* locs1 = *(*frames)[pframe];
            frames->Release(pframe);
            
            BlastSeqLoc ** pplast = & loc_list;
            
            while(*pplast) {
                pplast = & (*pplast)->next;
            }
            
            *pplast = locs1;
        }
        
        // Find all HSP intervals in query
        ITERATE(CSeq_align_set::Tdata, alignment, result.GetSeqAlign()->Get()) {
            _ASSERT((*alignment)->GetSegs().IsDisc());
            s_SeqAlignToBlastSeqLoc((*alignment)->GetSegs().GetDisc(), 
                                    &loc_list);
        }
        
        // Make the intervals unique
        BlastSeqLoc* ordered_loc_list =
            BlastSeqLocCombine(loc_list, REPEAT_MASK_LINK_VALUE);
        
        // Free the list of locations that's no longer needed.
        loc_list = BlastSeqLocFree(loc_list);
        
        /* Create a CSeq_loc with these locations and fill it for the 
           respective query */
        
        TMaskedQueryRegions filter_seqloc =
            s_BlastSeqLoc2MaskedRegions(*query.GetQuerySeqLoc(qindex),
                                        query.GetScope(qindex),
                                        ordered_loc_list,
                                        program);
        
        // Free the combined mask list in the BlastSeqLoc form.
        ordered_loc_list = BlastSeqLocFree(ordered_loc_list);
        
        query.SetMaskedRegions(qindex, filter_seqloc);
    }
}

/// Create an options handle with the defaults set for a search for repeats.
static 
CRef<CBlastOptionsHandle> s_CreateRepeatsSearchOptions()
{
    CBlastNucleotideOptionsHandle* opts(new CBlastNucleotideOptionsHandle);
    opts->SetTraditionalBlastnDefaults();
    opts->SetMismatchPenalty(REPEATS_SEARCH_PENALTY);
    opts->SetEvalueThreshold(REPEATS_SEARCH_EVALUE);
    opts->SetGapXDropoffFinal(REPEATS_SEARCH_XDROP_FINAL);
    opts->SetXDropoff(REPEATS_SEARCH_XDROP_UNGAPPED);
    opts->SetGapOpeningCost(REPEATS_SEARCH_GAP_OPEN);
    opts->SetGapExtensionCost(REPEATS_SEARCH_GAP_EXTEND);
    opts->SetDustFiltering(false);  // FIXME, is this correct?
    opts->SetWordSize(REPEATS_SEARCH_WORD_SIZE);
    return CRef<CBlastOptionsHandle>(opts);
}

void
Blast_FindRepeatFilterLoc(TSeqLocVector& query, 
                          const CBlastOptionsHandle* opts_handle)
{
    const CBlastNucleotideOptionsHandle* nucl_handle = 
        dynamic_cast<const CBlastNucleotideOptionsHandle*>(opts_handle);

    // Either non-blastn search or repeat filtering not desired.
    if (nucl_handle == NULL || nucl_handle->GetRepeatFiltering() == false)
       return;

    Blast_FindRepeatFilterLoc(query, nucl_handle->GetRepeatFilteringDB());
}

void
Blast_FindRepeatFilterLoc(TSeqLocVector& query, const char* filter_db)
{
    const CSearchDatabase target_db(filter_db,
                                    CSearchDatabase::eBlastDbIsNucleotide);

    CRef<CBlastOptionsHandle> repeat_opts = s_CreateRepeatsSearchOptions();

    // Remove any lower case masks, because they should not be used for the 
    // repeat locations search.
    vector< CConstRef<CSeq_loc> > lcase_mask_v;
    lcase_mask_v.reserve(query.size());
    
    for (unsigned int index = 0; index < query.size(); ++index) {
        lcase_mask_v.push_back(query[index].mask);
        query[index].mask.Reset(NULL);
    }

    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));
    CLocalBlast blaster(query_factory, repeat_opts, target_db);
    CSearchResultSet results = blaster.Run();

    // Restore the lower case masks
    for (unsigned int index = 0; index < query.size(); ++index) {
        query[index].mask.Reset(lcase_mask_v[index]);
    }

    // Extract the repeat locations and combine them with the previously 
    // existing mask in queries.
    s_FillMaskLocFromBlastResults(query, results);
}

void
Blast_FindRepeatFilterLoc(CBlastQueryVector& queries, const char* filter_db)
{
    const CSearchDatabase target_db(filter_db,
                                    CSearchDatabase::eBlastDbIsNucleotide);

    CRef<CBlastOptionsHandle> repeat_opts = s_CreateRepeatsSearchOptions();

    // Remove any lower case masks, because they should not be used for the 
    // repeat locations search.
    CBlastQueryVector temp_queries;
    for (size_t i = 0; i < queries.Size(); ++i) {
        TMaskedQueryRegions no_masks;
        CRef<CBlastSearchQuery> query
            (new CBlastSearchQuery(*queries.GetQuerySeqLoc(i),
                                   *queries.GetScope(i), no_masks));
        temp_queries.AddQuery(query);
    }

    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(temp_queries));
    CLocalBlast blaster(query_factory, repeat_opts, target_db);
    CSearchResultSet results = blaster.Run();

    // Extract the repeat locations and combine them with the previously 
    // existing mask in queries.
    s_FillMaskLocFromBlastResults(queries, results, eBlastTypeBlastn);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
 *  $Log$
 *  Revision 1.37  2006/09/18 14:39:14  camacho
 *  Fix compiler warning
 *
 *  Revision 1.36  2006/09/01 16:56:37  camacho
 *  Remove unused variable
 *
 *  Revision 1.35  2006/09/01 16:45:53  camacho
 *  Use size_type whenever possible
 *
 *  Revision 1.34  2006/09/01 14:22:06  camacho
 *  Pacify solaris compiler
 *
 *  Revision 1.33  2006/08/29 18:13:30  madden
 *  Doxygen fixes
 *
 *  Revision 1.32  2006/08/02 23:37:32  camacho
 *  Remove unneeded shifting when processing Seq-align results
 *
 *  Revision 1.31  2006/08/02 16:12:46  camacho
 *  Correct handling of no repeat filtering results
 *
 *  Revision 1.30  2006/07/31 13:20:39  camacho
 *  Deprecate CDbBlast
 *
 *  Revision 1.29  2006/06/15 17:40:21  papadopo
 *  PartialRun -> RunWithoutSeqalignGeneration
 *
 *  Revision 1.28  2006/06/14 15:58:54  camacho
 *  Replace ASSERT (defined in CORE) for _ASSERT (defined by C++ toolkit)
 *
 *  Revision 1.27  2006/03/22 20:07:40  bealer
 *  - Check in fix for BlastSeqLoc double free.
 *
 *  Revision 1.26  2006/03/13 16:43:51  bealer
 *  - Fix protein masking issue.
 *
 *  Revision 1.25  2006/02/22 18:34:17  bealer
 *  - Blastx filtering support, CBlastQueryVector class.
 *
 *  Revision 1.24  2006/01/24 15:35:08  camacho
 *  Overload Blast_FindRepeatFilterLoc with repeats database filtering arguments
 *
 *  Revision 1.23  2005/10/25 14:19:32  camacho
 *  repeats_filter.hpp is now a private header
 *
 *  Revision 1.22  2005/09/16 17:02:36  camacho
 *  Refactoring of filtering locations code.
 *
 *  Revision 1.21  2005/07/07 16:32:12  camacho
 *  Revamping of BLAST exception classes and error codes
 *
 *  Revision 1.20  2005/06/06 14:56:35  camacho
 *  Remove premature optimizations when using BlastSeqLocNew
 *
 *  Revision 1.19  2005/03/31 20:43:38  madden
 *  Blast_FindRepeatFilterLoc now takes CBlastOptionsHandle rather than char*
 *
 *  Revision 1.18  2005/03/29 15:59:21  dondosha
 *  Added blast scope
 *
 *  Revision 1.17  2005/03/02 13:56:25  madden
 *  Rename Filtering option funcitons to standard naming convention
 *
 *  Revision 1.16  2005/02/24 13:46:54  madden
 *  Changes to use structured filteing options instead of string
 *
 *  Revision 1.15  2005/02/08 20:35:37  dondosha
 *  Moved auxiliary functions and definitions for repeats filtering from C++ api into core; renamed FindRepeatFilterLoc into Blast_FindRepeatFilterLoc
 *
 *  Revision 1.14  2005/01/26 18:38:40  camacho
 *  Fix compiler error
 *
 *  Revision 1.13  2005/01/24 15:45:30  dondosha
 *  If lower case mask present, remove it for the repeats search, then combine repeats and lower case locations
 *
 *  Revision 1.12  2005/01/10 13:37:14  madden
 *  Call to SetSeedExtensionMethod removed as it has been removed
 *
 *  Revision 1.11  2004/12/20 15:13:58  dondosha
 *  Two small memory leak fixes
 *
 *  Revision 1.10  2004/11/24 16:06:47  dondosha
 *  Added and/or fixed doxygen comments
 *
 *  Revision 1.9  2004/11/17 20:26:33  camacho
 *  Use new BlastSeqSrc initialization function names and check for initialization errors
 *
 *  Revision 1.8  2004/11/02 17:58:27  camacho
 *  Add DOXYGEN_SKIP_PROCESSING to guard rcsid string
 *
 *  Revision 1.7  2004/09/13 15:55:04  madden
 *  Remove unused parameter from CSeqLoc2BlastSeqLoc
 *
 *  Revision 1.6  2004/09/13 12:47:06  madden
 *  Changes for redefinition of BlastSeqLoc and BlastMaskLoc
 *
 *  Revision 1.5  2004/07/19 14:58:47  dondosha
 *  Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
 *
 *  Revision 1.4  2004/06/23 14:07:19  dondosha
 *  Changed CSeq_loc argument in CSeqLoc2BlastMaskLoc to pointer
 *
 *  Revision 1.3  2004/06/15 22:51:54  dondosha
 *  Added const qualifiers to variables assigned returns from strchr and strstr - needed for SunOS compiler
 *
 *  Revision 1.2  2004/06/15 20:08:03  ucko
 *  Drop inclusion of unused ctools header.
 *
 *  Revision 1.1  2004/06/15 19:09:22  dondosha
 *  Repeats filtering code, moved from internal/blast/SplitDB/blastd
 *
 *  Revision 1.15  2004/05/18 16:00:24  dondosha
 *  Hide the use of CSeqDB API behind an #ifdef; use readdb by default
 *
 *  Revision 1.14  2004/05/14 17:41:51  dondosha
 *  Use CSeqDb API instead of readdb
 *
 *  Revision 1.13  2004/04/23 22:13:25  dondosha
 *  Changed DoubleInt to SSeqRange
 *
 *  Revision 1.12  2004/02/23 19:57:21  coulouri
 *  Add rcsid
 *
 *  Revision 1.11  2004/02/18 23:46:17  dondosha
 *  Use ReaddbBlastSeqSrcInit function to initialize sequence source structure
 *
 *  Revision 1.10  2004/01/07 21:18:37  dondosha
 *  Link together repeat locations with a gap smaller than 5 between them
 *
 *  Revision 1.9  2003/12/23 16:03:02  dondosha
 *  Set mask CSeq_loc to Null if there are no locations
 *
 *  Revision 1.8  2003/12/22 21:03:09  dondosha
 *  Added extra check for NULL string
 *
 *  Revision 1.7  2003/12/18 15:34:08  dondosha
 *  Correction in parsing repeats filter string
 *
 *  Revision 1.6  2003/12/17 21:26:16  dondosha
 *  Allow non-human repeats databases
 *
 *  Revision 1.5  2003/12/17 19:25:18  dondosha
 *  Perform repeats locations search for all query sequences together
 *
 *  Revision 1.4  2003/12/16 16:53:16  dondosha
 *  Correct intervals of repeat filter locations on reverse strand
 *
 *  Revision 1.3  2003/12/15 23:43:33  dondosha
 *  Set filter string to F for repeats search
 *
 *  Revision 1.2  2003/12/15 19:49:59  dondosha
 *  Minor fixes
 *
 *  Revision 1.1  2003/12/12 21:26:01  dondosha
 *  C++ implementation of repeats filtering
 *
 *  Revision 1.5  2003/12/11 23:55:44  dondosha
 *  Added human repeats filtering
 *
 *  Revision 1.4  2003/12/10 20:22:41  dondosha
 *  Fill the search space in the ASN.1 results structure
 *
 *  Revision 1.3  2003/12/10 18:59:22  madden
 *  Fix path for seqsrc_readdb.h
 *
 *  Revision 1.2  2003/12/08 22:46:32  dondosha
 *  Added conversion of other returns to ASN.1 generated structures
 *
 *  Revision 1.1  2003/12/03 18:17:07  dondosha
 *  Conversion between ASN.1 spec and C++ toolkit BLAST structures/classes
 *
 * ===========================================================================
 */
