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
#include <math.h>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/scope.hpp>

#include <algo/blast/api/blast_results.hpp>


#include <algo/align/ngalign/alignment_filterer.hpp>


BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);


CQueryFilter::CQueryFilter(int Rank, const string& Query)
        : m_Rank(Rank), m_Query(Query), m_Filter(new CAlignFilter(Query))
{
    m_Filter.Reset(new CAlignFilter);
    m_Filter->SetFilter(m_Query);
}



void CQueryFilter::FilterAlignments(TAlignResultsRef In, TAlignResultsRef Out)
{
    m_Filter->SetRemoveDuplicates(true);

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter, In->Get()) {
        
        NON_CONST_ITERATE(CQuerySet::TAssemblyToSubjectSet, AssemIter, QueryIter->second->Get()) {

        NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, AssemIter->second) {
        //NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryIter->second->Get()) {
    
            TAlignSetRef Filtered(new CSeq_align_set);
            m_Filter->Filter(*SubjectIter->second, *Filtered);

            if(Filtered->Get().empty())
                continue;

            NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, Filtered->Set()) {
                int OldFilterRank;
                if((*AlignIter)->GetNamedScore(KFILTER_SCORE, OldFilterRank)) {
                    if(m_Rank < OldFilterRank) {
                        (*AlignIter)->SetNamedScore(KFILTER_SCORE, m_Rank);
                    }
                } else {
                    (*AlignIter)->SetNamedScore(KFILTER_SCORE, m_Rank);
                }
                //if((*AlignIter)->GetType() == CSeq_align::eType_disc) {
                //    cerr << MSerial_AsnText << (**AlignIter);
                //}
            }

            CRef<CQuerySet> FilteredResults(new CQuerySet(*Filtered));

            Out->Insert(FilteredResults);
        }

        }
    }
}

END_SCOPE(ncbi)

