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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Pairwise and query-anchored alignments
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Split rows with mixed dir into separate rows
/// returns true if the operation was performed
bool CAnchoredAln::SplitStrands()
{
    TDim dim = GetDim();
    TDim new_dim = dim;
    TDim row;
    TDim new_row;

    for (row = 0;  row < dim;  ++row) {
        if (m_PairwiseAlns[row]->IsSet(CPairwiseAln::fMixedDir)) {
            ++new_dim;
        }
    }
    _ASSERT(dim <= new_dim);
    if (new_dim > dim) {
        m_PairwiseAlns.resize(new_dim);
        row = dim - 1;
        new_row = new_dim - 1;
        while (row < new_row) {
            _ASSERT(row >= 0);
            _ASSERT(new_row > 0);
            if (row == m_AnchorRow) {
                m_AnchorRow = new_row;
            }
            const CPairwiseAln& aln = *m_PairwiseAlns[row];
            if (aln.IsSet(CPairwiseAln::fMixedDir)) {
                m_PairwiseAlns[new_row].Reset
                    (new CPairwiseAln(aln.GetFirstId(),
                                      aln.GetSecondId(),
                                      aln.GetPolicyFlags()));
                CPairwiseAln& reverse_aln = *m_PairwiseAlns[new_row--];
                m_PairwiseAlns[new_row].Reset
                    (new CPairwiseAln(aln.GetFirstId(),
                                      aln.GetSecondId(),
                                      aln.GetPolicyFlags()));
                CPairwiseAln& direct_aln = *m_PairwiseAlns[new_row--];
                ITERATE (CPairwiseAln, aln_rng_it, aln) {
                    if (aln_rng_it->IsDirect()) {
                        direct_aln.push_back(*aln_rng_it);
                    } else {
                        reverse_aln.push_back(*aln_rng_it);
                    }
                }
            } else {
                m_PairwiseAlns[new_row--].Reset
                    (new CPairwiseAln(aln));
            }
            --row;
            _ASSERT(row <= new_row);
        }
        return true;
    } else {
        _ASSERT(dim == new_dim);
        return false;
    }
}


END_NCBI_SCOPE
