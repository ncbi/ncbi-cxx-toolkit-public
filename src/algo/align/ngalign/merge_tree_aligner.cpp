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
 * Author:  Nathan Bouk
 *
 * File Description: NGAligner plugin for CTreeAlignMerger
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>
#include <math.h>
#include <algorithm>


#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <algo/align/util/score_builder.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objects/seq/seq_id_handle.hpp>

#include <algo/align/mergetree/merge_tree.hpp>
#include <algo/align/ngalign/merge_tree_aligner.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);


TAlignResultsRef CMergeTreeAligner::GenerateAlignments(objects::CScope& Scope,
                                                ISequenceSet* QuerySet,
                                                ISequenceSet* SubjectSet,
                                                TAlignResultsRef AccumResults)
{
    TAlignResultsRef NewResults(new CAlignResultsSet);

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet,
                      QueryIter, AccumResults->Get()) {
        
        CQuerySet& QueryAligns = *QueryIter->second;

        int BestRank = QueryAligns.GetBestRank();
        if(BestRank > m_Threshold || BestRank == -1) {
            _TRACE("Determined ID: "
                   << QueryAligns.GetQueryId()->AsFastaString()
                   << " needs Tree Merging.");
        
            
            CRef<CSeq_align_set> Results(new CSeq_align_set);;
            
            NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryAligns.Get()) {
                NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {	

                    CRef<CSeq_align_set> Set = SubjectIter->second;
                    
                    CTreeAlignMerger TreeMerger;
                    TreeMerger.SetScope(&Scope);
                    TreeMerger.SetScoring(m_Scoring);
                    TreeMerger.Merge(Set->Get(), Results->Set());
                }
            }

            NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, Results->Set()) {
                (*AlignIter)->SetNamedScore("merge_tree_aligner", 1.0);
            }
            
            if(!Results->Get().empty()) {
                ERR_POST(Info << " Got " << Results->Get().size() << " alignments.");
                NewResults->Insert(CRef<CQuerySet>(new CQuerySet(*Results)));
            }
        }
    }

    return NewResults;
}




END_SCOPE(ncbi)
//end
