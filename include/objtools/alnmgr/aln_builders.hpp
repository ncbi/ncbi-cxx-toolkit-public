#ifndef OBJTOOLS_ALNMGR___ALN_BUILDERS__HPP
#define OBJTOOLS_ALNMGR___ALN_BUILDERS__HPP
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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment builders
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_stats.hpp>
#include <objtools/alnmgr/aln_user_options.hpp>
#include <objtools/alnmgr/aln_rng_coll_oper.hpp>


BEGIN_NCBI_SCOPE


void 
MergeAlnRngColl(CPairwiseAln& existing,
                const CPairwiseAln& addition,
                const CAlnUserOptions::TMergeFlags& flags)
{
    CPairwiseAln difference;
    SubtractAlnRngCollections(addition,
                              existing,
                              difference);
#define _TRACE_MergeAlnRngColl
#ifdef _TRACE_MergeAlnRngColl
    cerr << endl << "existing:" << endl;
    existing.Dump(cerr);
    cerr << endl<< "addition:" << endl;
    addition.Dump(cerr);
    cerr << endl << "difference = addition - existing:" << endl;
    difference.Dump(cerr);
#endif
    ITERATE(CPairwiseAln, rng_it, difference) {
        existing.insert(*rng_it);
    }
#ifdef _TRACE_MergeAlnRngColl
    cerr << endl << "result = existing + difference:" << endl;
    existing.Dump(cerr);
    cerr << endl;
#endif
}


template <class TAnchoredAlns,
          class TSeqIdPtrComp>
void 
BuildAln(TAnchoredAlns& in_alns,         ///< Input Alignments
         CAnchoredAln& out_aln,          ///< Output
         const CAlnUserOptions& options, ///< Input Options
         const TSeqIdPtrComp& comp)      ///< TSeqIdPtr comparison functor
{
    // Types
    typedef CAnchoredAln::TDim TDim;
    typedef CAnchoredAln::TPairwiseAlnVector TPairwiseAlnVector;
    typedef CAnchoredAln::TSeqIdPtr TSeqIdPtr;

    /// 1. Sort the anchored alns by score (best to worst)
    sort(in_alns.begin(),
         in_alns.end(), 
         PScoreGreater<CAnchoredAln>());
    

    /// 2. Build a single anchored_aln
    _ASSERT(out_aln.GetDim() == 0);
    switch (options.m_MergeAlgo) {
    case CAlnUserOptions::eQuerySeqMergeOnly:
        ITERATE(typename TAnchoredAlns, aln_it, in_alns) {
            const CAnchoredAln& aln = **aln_it;
            if (aln_it == in_alns.begin()) {
                out_aln = aln;
                continue;
            }
            // assumption is that anchor row is the last
            _ASSERT(aln.GetAnchorRow() == aln.GetDim()-1);
            for (TDim row = 0; row < aln.GetDim(); ++row) {
                if (row == aln.GetAnchorRow()) {
                    MergeAlnRngColl(*out_aln.SetPairwiseAlns().back(),
                                    *aln.GetPairwiseAlns()[row],
                                    CAlnUserOptions::fTruncateOverlaps);
                } else {
                    // swap the anchor row with the new one
                    CRef<CPairwiseAln> anchor_pairwise(out_aln.GetPairwiseAlns().back());
                    out_aln.SetPairwiseAlns().back().Reset
                        (new CPairwiseAln(*aln.GetPairwiseAlns()[row]));
                    out_aln.SetPairwiseAlns().push_back(anchor_pairwise);
                    TSeqIdPtr anchor_id(out_aln.GetSeqIds().back());
                    out_aln.SetSeqIds().back().Reset(aln.GetSeqIds()[row]);
                    out_aln.SetSeqIds().push_back(anchor_id);
                }
            }
        }
        break;
    case CAlnUserOptions::ePreserveRows:
        ITERATE(typename TAnchoredAlns, aln_it, in_alns) {
            CAnchoredAln aln = **aln_it;
            if (aln_it == in_alns.begin()) {
                out_aln = aln;
                continue;
            }
            _ASSERT(aln.GetDim() == out_aln.GetDim());
            _ASSERT(aln.GetAnchorRow() == out_aln.GetAnchorRow());
            for (TDim row = 0; row < aln.GetDim(); ++row) {
                MergeAlnRngColl(*out_aln.SetPairwiseAlns()[row],
                                *aln.GetPairwiseAlns()[row],
                                row == aln.GetAnchorRow() ?
                                CAlnUserOptions::fTruncateOverlaps :
                                options.m_MergeFlags);
            }
        }
        break;
    case CAlnUserOptions::eMergeAllSeqs:
    default: 
        {
            typedef map<TSeqIdPtr, CRef<CPairwiseAln>, TSeqIdPtrComp> TIdAlnMap;
            TIdAlnMap id_aln_map(comp); // store the id-aln

            typedef vector<pair<TSeqIdPtr, CRef<CPairwiseAln> > > TIdAlnVec;
            

            TSeqIdPtr anchor_id;
            CRef<CPairwiseAln> anchor_pairwise;
            ITERATE(typename TAnchoredAlns, aln_it, in_alns) {
                const CAnchoredAln& aln = **aln_it;
                for (TDim row = 0; row < aln.GetDim(); ++row) {

                    CRef<CPairwiseAln>& pairwise = id_aln_map[aln.GetSeqIds()[row]];
                    if (pairwise.Empty()) {
                        pairwise.Reset
                            (new CPairwiseAln(*aln.GetPairwiseAlns()[row]));
                    } else {
                        MergeAlnRngColl(*pairwise,
                                        *aln.GetPairwiseAlns()[row],
                                        row == aln.GetAnchorRow() ?
                                        CAlnUserOptions::fTruncateOverlaps :
                                        options.m_MergeFlags);
                    }

                    if (row == aln.GetAnchorRow()) {
                        // if anchor
                        if (aln_it == in_alns.begin()) {
                            anchor_id = aln.GetSeqIds()[row];
                            anchor_pairwise.Reset(pairwise);
                        }
                        _ASSERT( !(comp(anchor_id, aln.GetSeqIds()[row])  ||
                                   comp(aln.GetSeqIds()[row], anchor_id)) );
                    } else {
                        // else add to the out vectors
                        out_aln.SetSeqIds().push_back(aln.GetSeqIds()[row]);
                        out_aln.SetPairwiseAlns().push_back(pairwise);
                    }
                    
                }
            }

            // finally, add the anchor
            out_aln.SetSeqIds().push_back(anchor_id);
            out_aln.SetPairwiseAlns().push_back(anchor_pairwise);
        }
        break;
    }
    out_aln.SetAnchorRow(out_aln.GetSeqIds().size() - 1);
    /// 3. Sort the ids and alns according to score, how to collect score?
}


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2006/12/06 20:08:31  todorov
* 1) Preserve the order of seq-ids.
* 2) MergeOption -> MergeAlgo
*
* Revision 1.5  2006/12/01 21:21:02  todorov
* - NCBI_XALNMGR_EXPORT
*
* Revision 1.4  2006/12/01 17:53:00  todorov
* + NCBI_XALNMGR_EXPORT
*
* Revision 1.3  2006/11/27 19:37:57  todorov
* using comp.
*
* Revision 1.2  2006/11/22 00:45:10  todorov
* Added support for three merging modes.
*
* Revision 1.1  2006/11/17 05:35:11  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_BUILDERS__HPP
