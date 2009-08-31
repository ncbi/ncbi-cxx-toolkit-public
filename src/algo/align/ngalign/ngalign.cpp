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
#include <hash_map.h>
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

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>



using namespace ncbi;
using namespace objects;
using namespace blast;
using namespace std;

const string IAlignmentFilter::KFILTER_SCORE = "filter_score";

TAlignSetRef CUberAlign::Align()
{
    return x_Align_Impl();
}


TAlignSetRef CUberAlign::x_Align_Impl()
{

    TAlignResultsRef FilterResults(new CAlignResultsSet),
                     AccumResults(new CAlignResultsSet);

    NON_CONST_ITERATE(list<CRef<IAlignmentFactory> >, AlignIter, m_Aligners) {

        TAlignResultsRef CurrResults;
        CurrResults = (*AlignIter)->GenerateAlignments(m_Scope, m_Query, m_Subject,
                                                       AccumResults);

        NON_CONST_ITERATE(list<CRef<IAlignmentScorer> >, ScorerIter, m_Scorers) {
            (*ScorerIter)->ScoreAlignments(CurrResults, *m_Scope);
        }

//cerr << MSerial_AsnText << *CurrResults->ToSeqAlignSet();
//        AccumResults->Insert(CurrResults);
        NON_CONST_ITERATE(list<CRef<IAlignmentFilter> >, FilterIter, m_Filters) {
            (*FilterIter)->FilterAlignments(CurrResults, FilterResults);
        }
        AccumResults->Insert(CurrResults);
    }

    return AccumResults->ToBestSeqAlignSet();
}












//end
