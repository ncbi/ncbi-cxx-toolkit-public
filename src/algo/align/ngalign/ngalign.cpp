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
#include <corelib/ncbi_signal.hpp>
#include <math.h>
#include <algo/align/ngalign/ngalign.hpp>

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

#include <objects/genomecoll/genome_collection__.hpp>


#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>


BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);


CNgAligner::CNgAligner(objects::CScope& Scope,
                       CGC_Assembly* GenColl,
                       bool AllowDupes)
    : m_Scope(&Scope), m_AllowDupes(AllowDupes), m_GenColl(GenColl)
{
}

CNgAligner::~CNgAligner()
{
}

void CNgAligner::SetQuery(ISequenceSet* Set)
{
    m_Query = Set;
}


void CNgAligner::SetSubject(ISequenceSet* Set)
{
    m_Subject = Set;
}


void CNgAligner::AddFilter(IAlignmentFilter* Filter)
{
    m_Filters.push_back(CIRef<IAlignmentFilter>(Filter));
}


void CNgAligner::AddAligner(IAlignmentFactory* Aligner)
{
    m_Aligners.push_back(CIRef<IAlignmentFactory>(Aligner));
}


void CNgAligner::AddScorer(IAlignmentScorer* Scorer)
{
    m_Scorers.push_back(CIRef<IAlignmentScorer>(Scorer));
}




const char* IAlignmentFilter::KFILTER_SCORE = "filter_score";

TAlignSetRef CNgAligner::Align()
{
    return x_Align_Impl();
}


TAlignSetRef CNgAligner::x_Align_Impl()
{

    TAlignResultsRef FilterResults(new CAlignResultsSet),
                     AccumResults(new CAlignResultsSet(m_GenColl, m_AllowDupes));

    NON_CONST_ITERATE(TFactories, AlignIter, m_Aligners) {
        if (CSignal::IsSignaled()) {
            NCBI_THROW(CException, eUnknown, "trapped signal");
        }

        TAlignResultsRef CurrResults;
        CurrResults = (*AlignIter)->GenerateAlignments(*m_Scope, m_Query, m_Subject,
                                                       AccumResults);

        NON_CONST_ITERATE(TScorers, ScorerIter, m_Scorers) {
            (*ScorerIter)->ScoreAlignments(CurrResults, *m_Scope);
        }

//cerr << MSerial_AsnText << *CurrResults->ToSeqAlignSet();
//        AccumResults->Insert(CurrResults);
        NON_CONST_ITERATE(TFilters, FilterIter, m_Filters) {
            (*FilterIter)->FilterAlignments(CurrResults, FilterResults, *m_Scope);
        }
        AccumResults->Insert(CurrResults);
    }

    TAlignSetRef Results;
    if(m_AllowDupes) {
        Results = AccumResults->ToSeqAlignSet();
    } else {
        Results = AccumResults->ToBestSeqAlignSet();
    }

    if(!Results.IsNull()) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        ITERATE(CSeq_align_set::Tdata, AlignIter, Results->Get()) {
            const CSeq_align& Align = **AlignIter;
            string FastaId = Align.GetSeq_id(0).AsFastaString();
            ITERATE(TFactories, FactoryIter, m_Aligners) {
                int Value;
                if(Align.GetNamedScore((*FactoryIter)->GetName(), Value)) {
                    extra.Print((*FactoryIter)->GetName(), FastaId);
                }
            }
        }
    }

    return Results;
}


END_SCOPE(ncbi)

