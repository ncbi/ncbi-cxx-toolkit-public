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


BEGIN_NCBI_SCOPE


void 
BuildAln(vector<CRef<CAnchoredAln> >& in_anchored_alns, ///< Input Alignments
         CAnchoredAln& out_anchored_aln,                ///< Output
         CAlnUserOptions& aln_user_options)             ///< Input Options
{
    /// Types
    typedef CAnchoredAln::TDim TDim;
    typedef vector<CRef<CAnchoredAln> > TAnchoredAlnVector;

    /// Sort the anchored alns by score (best to worst)
    sort(in_anchored_alns.begin(),
         in_anchored_alns.end(), 
         PScoreGreater<CAnchoredAln>());
    
    /// Build a single anchored_aln
    for (size_t aln_idx = 0; 
         aln_idx < in_anchored_alns.size();
         ++aln_idx) {
        
        const CAnchoredAln& anchored_aln = *in_anchored_alns[aln_idx];
        
        TDim dim = anchored_aln.GetDim();
        for (TDim row = 0; row < dim; ++row) {
            
            if (row < out_anchored_aln.GetDim()) {
                
                CPairwiseAln& out_pairwise_aln = *out_anchored_aln.SetPairwiseAlns()[row];
                
                CRef<CPairwiseAln> diff(new CPairwiseAln);
                SubtractAlnRngCollections(*anchored_aln.GetPairwiseAlns()[row],
                                          out_pairwise_aln,
                                          *diff);
                ITERATE(CPairwiseAln, rng_it, *diff) {
                    out_pairwise_aln.insert(*rng_it);
                }
                
            } else {
                _ASSERT(row == out_anchored_aln.GetDim());
                
                CRef<CPairwiseAln> pairwise_aln
                    (new CPairwiseAln(*anchored_aln.GetPairwiseAlns()[row]));
                out_anchored_aln.SetPairwiseAlns().push_back(pairwise_aln);
                out_anchored_aln.SetSeqIds().push_back(anchored_aln.GetSeqIds()[row]);
            }
        }
        cout << "Added alignment " << aln_idx << ":" << endl;
        out_anchored_aln.Dump(cout);
    }
}


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2006/11/17 05:35:11  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_BUILDERS__HPP
