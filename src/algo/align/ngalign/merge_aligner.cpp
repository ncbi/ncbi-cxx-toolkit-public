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
#include <algo/align/util/genomic_compart.hpp>

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
    CRef<CSeq_align_set> Merged(new CSeq_align_set);
    CAlignCleanup Cleaner(Scope);
    Cleaner.FillUnaligned(true);

    NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter,
                      QueryAligns.Get()) {
        NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter,
                          AssemIter->second) {  

            CRef<CSeq_align_set> Set = SubjectIter->second;

#if 1
           	int options[2] = { fCompart_AllowIntersections,
						fCompart_AllowIntersections | fCompart_SortByScore };
			for(int i = 0; i < 1; i++) {
				list< CRef<CSeq_align_set> > compartments;
				FindCompartments(Set->Get(), compartments,
								options[i]);

				ITERATE (list< CRef<CSeq_align_set> >, cit, compartments) {
					CRef<CSeq_align_set> sas = *cit;
					x_SortAlignSet(*sas, options[i]);
					CRef<CSeq_align_set> out = x_MergeSeqAlignSet(*sas, Scope);
					if( out  &&  !out->Set().empty() ) {
						ITERATE(CSeq_align_set::Tdata, AlignIter, out->Set()) {
							Merged->Set().push_back(*AlignIter);
						}
					}
				}
			}
#endif

#if 0
            CRef<CSeq_align_set> Pluses(new CSeq_align_set),
                Minuses(new CSeq_align_set);

            ITERATE(CSeq_align_set::Tdata, AlignIter, Set->Get()) {
                if( (*AlignIter)->GetSeqStrand(0) == eNa_strand_plus)
                    Pluses->Set().push_back(*AlignIter);
                else if( (*AlignIter)->GetSeqStrand(0) == eNa_strand_minus)
                    Minuses->Set().push_back(*AlignIter);
            }

            CRef<CSeq_align_set> PlusOut, MinusOut;

            if(!Pluses->Set().empty()) {
                x_SortAlignSet(*Pluses);
                PlusOut = x_MergeSeqAlignSet(*Pluses, Scope);
            }
            if(!Minuses->Set().empty()) {
                x_SortAlignSet(*Minuses);
                MinusOut = x_MergeSeqAlignSet(*Minuses, Scope);
            }

            if(!PlusOut.IsNull())
                ITERATE(CSeq_align_set::Tdata, AlignIter, PlusOut->Set()) {
                    Merged->Set().push_back(*AlignIter);
                }
            if(!MinusOut.IsNull())
                ITERATE(CSeq_align_set::Tdata, AlignIter, MinusOut->Set()) {
                    Merged->Set().push_back(*AlignIter);
                }
#endif
        }
    }

    return Merged;
}


CRef<objects::CSeq_align_set>
CMergeAligner::x_MergeSeqAlignSet(CSeq_align_set& InAligns, objects::CScope& Scope)
{
    list<CConstRef<CSeq_align> > In;
    ITERATE(CSeq_align_set::Tdata, AlignIter, InAligns.Get()) {
        CConstRef<CSeq_align> Align(*AlignIter);
        In.push_back(Align);
    }

    CRef<CSeq_align_set> Out(new CSeq_align_set);

    try {
        CAlignCleanup Cleaner(Scope);
        Cleaner.FillUnaligned(true);
        Cleaner.Cleanup(In, Out->Set());
    } catch(CException& e) {
        ERR_POST(Info << "Cleanup Error: " << e.ReportAll());
        return CRef<CSeq_align_set>();
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

        Align->SetNamedScore(GetName(), 1);
    }

    if(Out->Set().empty())
        return CRef<CSeq_align_set>();
    return Out;
}



static bool s_SortByAlignedLength(const CRef<objects::CSeq_align>& A,
                                  const CRef<objects::CSeq_align>& B)
{
    CScoreBuilder Scorer;
    TSeqPos Lengths[2];
    Lengths[0] = Scorer.GetAlignLength(*A);
    Lengths[1] = Scorer.GetAlignLength(*B);
    return (Lengths[0] > Lengths[1]);
}


static bool s_SortByScore(const CRef<objects::CSeq_align>& A,
                          const CRef<objects::CSeq_align>& B)
{
	int Scores[2] = {0, 0};
	A->GetNamedScore(CSeq_align::eScore_Score, Scores[0]);
	B->GetNamedScore(CSeq_align::eScore_Score, Scores[1]);
    return (Scores[0] > Scores[1]);
}


void CMergeAligner::x_SortAlignSet(CSeq_align_set& AlignSet, int CompartFlags)
{
    vector<CRef<CSeq_align> > TempVec;
    TempVec.reserve(AlignSet.Set().size());
    copy(AlignSet.Set().begin(), AlignSet.Set().end(),
            insert_iterator<vector<CRef<CSeq_align> > >(TempVec, TempVec.end()));
    
	if(CompartFlags & fCompart_SortByScore)
		sort(TempVec.begin(), TempVec.end(), s_SortByScore);
	else
		sort(TempVec.begin(), TempVec.end(), s_SortByAlignedLength);

	AlignSet.Set().clear();
    copy(TempVec.begin(), TempVec.end(),
        insert_iterator<CSeq_align_set::Tdata>(AlignSet.Set(), AlignSet.Set().end()));
}


END_SCOPE(ncbi)
//end
