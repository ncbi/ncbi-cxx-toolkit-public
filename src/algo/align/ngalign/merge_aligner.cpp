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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>
#include <math.h>

#include <algo/align/ngalign/merge_aligner.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/align/util/score_builder.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>


#include <algo/sequence/align_cleanup.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);


TAlignResultsRef CMergeAligner::GenerateAlignments(objects::CScope& Scope,
                                                ISequenceSet* QuerySet,
                                                ISequenceSet* SubjectSet,
                                                TAlignResultsRef AccumResults)
{
    TAlignResultsRef NewResults(new CAlignResultsSet);

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet,
                      QueryIter, AccumResults->Get()) {

        int BestRank = QueryIter->second->GetBestRank();
        if(BestRank > m_Threshold || BestRank == -1) {
            ERR_POST(Info << "Determined ID: "
                      << QueryIter->second->GetQueryId()->AsFastaString()
                      << " needs Merging.");

            CRef<CSeq_align_set> Results;
            Results = x_MergeAlignments(*QueryIter->second, Scope);

            if(!Results->Get().empty())
                NewResults->Insert(CRef<CQuerySet>(new CQuerySet(*Results)));
        }
    }

    return NewResults;
}


CRef<CSeq_align_set>
CMergeAligner::x_MergeAlignments(CQuerySet& QueryAligns, CScope& Scope)
{
    CRef<CSeq_align_set> Instances(new CSeq_align_set);
    CAlignCleanup Cleaner(Scope);
    Cleaner.FillUnaligned(true);

    NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter,
                      QueryAligns.Get()) {

        CRef<CSeq_align_set> Set = SubjectIter->second;
        list<CConstRef<CSeq_align> > In;
        ITERATE(CSeq_align_set::Tdata, AlignIter, Set->Get()) {
            CConstRef<CSeq_align> Align(*AlignIter);
            In.push_back(Align);
        }

        CRef<CSeq_align_set> Out(new CSeq_align_set);

        try {
            Cleaner.Cleanup(In, Out->Set());
        } catch(CException& e) {
            ERR_POST(Info << "Cleanup Error: " << e.ReportAll());
            continue;
        }

        NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, Out->Set()) {
            CRef<CSeq_align> Align(*AlignIter);
            CDense_seg& Denseg = Align->SetSegs().SetDenseg();

            if(!Denseg.CanGetStrands() || Denseg.GetStrands().empty()) {
                Denseg.SetStrands().resize(Denseg.GetDim()*Denseg.GetNumseg(), eNa_strand_plus);
            }

            if(Denseg.GetSeqStrand(1) != eNa_strand_plus) {
                Denseg.Reverse();
            }
            CRef<CDense_seg> Filled = Denseg.FillUnaligned();
            Denseg.Assign(*Filled);

            Align->SetNamedScore("merged_alignment", 1);
        }

        ITERATE(CSeq_align_set::Tdata, AlignIter, Out->Set()) {
            Instances->Set().push_back(*AlignIter);
        }
    }

    return Instances;
}



END_SCOPE(ncbi)
//end
