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
* Author: Robert Hubley ( boiler plate - Ilya Dondoshansky )
*
* File Description:
*   Unit test module to test hit saving procedures in RMBlast
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include "blast_setup.hpp"
#include "blast_objmgr_priv.hpp"
#include "test_objmgr.hpp"

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/gencode_singleton.h>
#include "blast_hits_priv.h"

extern "C" int h_score_compare_hsps(const void* v1, const void* v2)
{
    BlastHSP* h1,* h2;

    h1 = *((BlastHSP**) v1);
    h2 = *((BlastHSP**) v2);

    if (h1->score > h2->score)
        return -1;
    else if (h1->score < h2->score)
        return 1;
    return 0;
}

using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(blasthits)

    static void
    s_SetupScoringOptionsForReevaluateHSP(BlastScoringOptions* * options_ptr,
                                          bool gapped, bool is_prot)
    {
        BlastScoringOptions* options =
            (BlastScoringOptions*) calloc(1, sizeof(BlastScoringOptions));
        if (gapped) {
           options->gapped_calculation = TRUE;
           options->gap_open = 1;
           options->gap_extend = 1;
        }
        if (is_prot) {
           options->matrix = strdup("BLOSUM62");
        } else {
           options->reward = 1;
           options->penalty = -2;
        }
        *options_ptr = options;
    }

   static void
   s_SetupHSPListTransl(BlastHSPList* * hsplist_ptr)
   {
       *hsplist_ptr = Blast_HSPListNew(4);
       for(unsigned int i = 0; i < 4; i++ ){
    	   unsigned int factor = i+1;
    	   BlastHSP* hsp = Blast_HSPNew();

       		hsp->query.offset = 0;
       		hsp->query.end = 12/factor;
       		hsp->subject.offset = 0;
       		hsp->subject.frame = 1;
       		hsp->subject.end = 12/factor;
       		hsp->score = 45/factor;
       		(*hsplist_ptr)->hsp_array[i] = hsp;
       		(*hsplist_ptr)->hspcnt ++;
   		}
   }

    /** RMBlast function to filter HSPs with a raw score < HitSavingOptions->cutoff_score
     *  -RMH-
     */
    BOOST_AUTO_TEST_CASE(testHSPListReapByRawScore)
    {
        BlastHSPList* hsp_list = NULL;
        EBlastProgramType program_number = eBlastTypeBlastn;
        BlastScoringOptions* scoring_options = NULL;
        BlastHitSavingOptions* hit_options = NULL;

        s_SetupScoringOptionsForReevaluateHSP(&scoring_options, false, true);
        BlastHitSavingOptionsNew(program_number, &hit_options,
                                 scoring_options->gapped_calculation);

        s_SetupHSPListTransl(&hsp_list);

        BOOST_REQUIRE_EQUAL(4, (int) hsp_list->hspcnt);
        hit_options->cutoff_score = 15;
        Blast_HSPListReapByRawScore(hsp_list, hit_options);
        BOOST_REQUIRE_EQUAL(3, (int) hsp_list->hspcnt);
        hit_options->cutoff_score = 20;
        Blast_HSPListReapByRawScore(hsp_list, hit_options);
        BOOST_REQUIRE_EQUAL(2, (int) hsp_list->hspcnt);

        Blast_HSPListFree(hsp_list);
        BlastHitSavingOptionsFree(hit_options);
        BlastScoringOptionsFree(scoring_options);
    }

    /** RMBlast function to filter HSPs based on the percentage over overlap
     *  along the query ranges.
     *  -RMH-
     */
    BOOST_AUTO_TEST_CASE(testHSPResultsApplyMasklevel)
    {
        EBlastProgramType program_number = eBlastTypeBlastn;
        BlastHSPList* hsp_list = Blast_HSPListNew(0);

        const int kMaxHspCount = 26;
        const int kSubjectOffsets[kMaxHspCount] =
            { 10,382,14,83,4,382,1000,203,54,32,64,382,89,183,813,132,14,1344,321,224,34,8341,1344,254,861,834 };
        const int kQueryOffsets[kMaxHspCount] =
            { 1,4,14,21,24,34,41,44,54,61,64,74,81,84,94,101,104,114,121,124,134,141,144,154,161,164 };
        const int kLengths[kMaxHspCount] =
            { 17,9,9,17,9,9,17,9,9,17,9,9,17,9,9,17,9,9,17,9,9,17,9,9,17,9 };
        const int kScores[kMaxHspCount] =
            { 93,85,85,93,85,85,93,85,85,93,85,85,93,85,85,93,85,85,93,85,85,93,85,85,93,85 };

        int index;
        hsp_list->hspcnt = kMaxHspCount;
        for (index = 0; index < kMaxHspCount; ++index) {
            hsp_list->hsp_array[index] =
                (BlastHSP*) calloc(1, sizeof(BlastHSP));
            hsp_list->hsp_array[index]->query.offset = kQueryOffsets[index];
            hsp_list->hsp_array[index]->subject.offset = kSubjectOffsets[index];
            hsp_list->hsp_array[index]->query.end =
                kQueryOffsets[index] + kLengths[index];
            hsp_list->hsp_array[index]->subject.end =
                kSubjectOffsets[index] + kLengths[index];
            hsp_list->hsp_array[index]->score = kScores[index];
            hsp_list->hsp_array[index]->context = 0;
        }

        BlastHSPResults* results = Blast_HSPResultsNew(1);
        Blast_HSPResultsInsertHSPList( results, hsp_list, 1 );

        BlastQueryInfo* query_info = BlastQueryInfoNew(program_number, 1);
        query_info->contexts[0].query_length = 750;
        query_info->contexts[0].frame = 1;
        query_info->contexts[0].is_valid = true;
        for (int i=1; i<=query_info->last_context; i++)
            query_info->contexts[i].is_valid = false;

        // masklevel 80  [ 17 remaining ]
        //   1,14,21,34,41,54,61,74,81,94,101,114,121,134,141,154,161
        Blast_HSPResultsApplyMasklevel( results, query_info, 80, 750 );
        BOOST_REQUIRE_EQUAL(17, (int) results->hitlist_array[0]->hsplist_array[0]->hspcnt);
        // masklevel 25  [ 9 remaining ]
        //   1,21,41,61,81,101,121,141,161
        Blast_HSPResultsApplyMasklevel( results, query_info, 25, 750 );
        BOOST_REQUIRE_EQUAL(9, (int) results->hitlist_array[0]->hsplist_array[0]->hspcnt);

        BlastQueryInfoFree(query_info);
        Blast_HSPResultsFree(results);
    }

BOOST_AUTO_TEST_SUITE_END()
