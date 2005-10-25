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
#include <objmgr/util/sequence.hpp>
#include <algo/blast/api/blast_types.hpp>

#include <algo/blast/api/seqsrc_seqdb.hpp>

#include <algo/blast/api/db_blast.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include "repeats_filter.hpp"
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_filter.h>

#include <string.h>

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
 * @return List of mask locations in a CSeq_loc form.
 */
static CSeq_loc* 
s_BlastSeqLoc2CSeqloc(SSeqLoc& query, BlastSeqLoc* loc_list)
{
   CSeq_loc* seqloc = new CSeq_loc();
   BlastSeqLoc* loc;

   seqloc->SetNull();
   for (loc = loc_list; loc; loc = loc->next) {
      seqloc->SetPacked_int().AddInterval(
          sequence::GetId(*query.seqloc, query.scope),
          loc->ssr->left, loc->ssr->right);
   }
   
   return seqloc;
}

/** Fills the mask locations in the query SSeqLoc structures, as if it was a 
 * lower case mask, given the results of a BLAST search against a database of 
 * repeats.
 * @param query Vector of query sequence locations structures [in] [out]
 * @param results Internal results structure, returned from a BLAST search
 *                against a repeats database [in]
 */
static void
s_FillMaskLocFromBlastHSPResults(TSeqLocVector& query, BlastHSPResults* results)
{
    ASSERT(results->num_queries == (Int4)query.size());
    
    for (Int4 query_index = 0; query_index < (Int4)query.size(); ++query_index) {
        BlastSeqLoc* loc_list = NULL;
        BlastSeqLoc* ordered_loc_list = NULL;
        BlastHitList* hit_list = results->hitlist_array[query_index];
       
        if (!hit_list) {
            continue;
        }
        Int4 query_length = sequence::GetLength(*query[query_index].seqloc, 
                                                query[query_index].scope);
        Int4 query_start = sequence::GetStart(*query[query_index].seqloc,
                                              query[query_index].scope);
        
        // Get the previous mask locations
        loc_list = CSeqLoc2BlastSeqLoc(query[query_index].mask);
        
        /* Find all HSP intervals in query */
        for (Int4 hit_index = 0; hit_index < hit_list->hsplist_count; 
             ++hit_index) {
            BlastHSPList* hsp_list = hit_list->hsplist_array[hit_index];
            /* HSP lists cannot be NULL! */
            ASSERT(hsp_list);
            for (Int4 hsp_index = 0; hsp_index < hsp_list->hspcnt; ++hsp_index) {
                BlastHSP* hsp = hsp_list->hsp_array[hsp_index];
                /* HSP cannot be NULL! */
                ASSERT(hsp);

                Int4 left, right;
                if (hsp->query.frame == hsp->subject.frame) {
                    left = hsp->query.offset;
                    right = hsp->query.end - 1;
                } else {
                    left = query_length - hsp->query.end;
                    right = query_length - hsp->query.offset - 1;
                }

                // Shift the coordinates so they correspond to the full 
                // sequence.
                left += query_start;
                right += query_start;

                BlastSeqLocNew(&loc_list, left, right);
            }
        }
        // Make the intervals unique
        ordered_loc_list = BlastSeqLocCombine(loc_list, REPEAT_MASK_LINK_VALUE);

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

/// Sets the default options for a search for repeats.
/// @param opts Nucleotide BLAST options handle [in] [out]
static void 
s_SetRepeatsSearchDefaults(CBlastNucleotideOptionsHandle& opts)
{
    opts.SetTraditionalBlastnDefaults();
    opts.SetMismatchPenalty(REPEATS_SEARCH_PENALTY);
    opts.SetEvalueThreshold(REPEATS_SEARCH_EVALUE);
    opts.SetGapXDropoffFinal(REPEATS_SEARCH_XDROP_FINAL);
    opts.SetXDropoff(REPEATS_SEARCH_XDROP_UNGAPPED);
    opts.SetGapOpeningCost(REPEATS_SEARCH_GAP_OPEN);
    opts.SetGapExtensionCost(REPEATS_SEARCH_GAP_EXTEND);
    opts.SetDustFiltering(false);  // FIXME, is this correct?
    opts.SetWordSize(REPEATS_SEARCH_WORD_SIZE);
}

void
Blast_FindRepeatFilterLoc(TSeqLocVector& query, const CBlastOptionsHandle* opts_handle)
{

    const CBlastNucleotideOptionsHandle* nucl_handle = dynamic_cast<const CBlastNucleotideOptionsHandle*>(opts_handle);

    // Either non-blastn search or repeat filtering not desired.
    if (nucl_handle == NULL || nucl_handle->GetRepeatFiltering() == false)
       return;

    bool kIsProt = false;
    BlastSeqSrc* seq_src = SeqDbBlastSeqSrcInit(nucl_handle->GetRepeatFilteringDB(), kIsProt);
    char* error_str = BlastSeqSrcGetInitError(seq_src);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }

    // Options for repeat filtering search
    CBlastNucleotideOptionsHandle repeat_opts;

    s_SetRepeatsSearchDefaults(repeat_opts);

    // Remove any lower case masks, because they should not be used for the 
    // repeat locations search.
    vector< CConstRef<CSeq_loc> > lcase_mask_v;
    lcase_mask_v.reserve(query.size());
    
    for (unsigned int index = 0; index < query.size(); ++index) {
        lcase_mask_v.push_back(query[index].mask);
        query[index].mask.Reset(NULL);
    }

    CDbBlast blaster(query, seq_src, repeat_opts);
    blaster.PartialRun();

    seq_src = BlastSeqSrcFree(seq_src);

    // Restore the lower case masks
    for (unsigned int index = 0; index < query.size(); ++index) {
        query[index].mask.Reset(lcase_mask_v[index]);
    }

    // Extract the repeat locations and combine them with the previously 
    // existing mask in queries.
    s_FillMaskLocFromBlastHSPResults(query, blaster.GetResults());
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
 *  $Log$
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
