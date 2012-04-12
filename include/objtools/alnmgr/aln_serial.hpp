#ifndef OBJTOOLS_ALNMGR___ALN_SERIAL__HPP
#define OBJTOOLS_ALNMGR___ALN_SERIAL__HPP
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
*   Alignments Serialization
*
* ===========================================================================
*/


#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_explorer.hpp>
#include <objtools/alnmgr/aln_stats.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CPairwiseAln::TRng& rng);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const IAlnSegment::ESegTypeFlags& flags);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const IAlnSegment& aln_seg);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CPairwiseAln::TAlnRng& aln_rng);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CPairwiseAln::EFlags& flags);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const TAlnSeqIdIRef& aln_seq_id_iref);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CPairwiseAln& pairwise_aln);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CAnchoredAln& anchored_aln);

template<class _TAlnIdVec>
ostream& operator<<(ostream& out, const CAlnStats<_TAlnIdVec>& aln_stats)
{
    out << "Number of alignments: " << aln_stats.GetAlnCount() << endl;
    out << "IsCanonicalQueryAnchored: " << aln_stats.IsCanonicalQueryAnchored() << endl;
    out << "IsCanonicalMultiple: " << aln_stats.IsCanonicalMultiple() << endl;
    out << "CanBeAnchored: " << aln_stats.CanBeAnchored() << endl;
    out << endl;
    out << "IdVec (" << aln_stats.GetIdVec().size() << "):" << endl;
    ITERATE(TAlnStats::TIdVec, it, aln_stats.GetIdVec()) {
        out << (*it)->AsString() << " (base_width=" << (*it)->GetBaseWidth() << ")" << endl;
    }
    out << endl;
    out << "IdMap (" << aln_stats.GetIdMap().size() << "):" << endl;
    ITERATE(TAlnStats::TIdMap, it, aln_stats.GetIdMap()) {
        out << it->first->AsString() << " (base_width=" << it->first->GetBaseWidth() << ")" << endl;
    }
    out << endl;
    out << "AnchorIdVec (" << aln_stats.GetAnchorIdVec().size() << "):" << endl;
    ITERATE(TAlnStats::TIdVec, it, aln_stats.GetAnchorIdVec()) {
        out << (*it)->AsString() << " (base_width=" << (*it)->GetBaseWidth() << ")" << endl;
    }
    out << endl;
    out << "AnchorIdMap (" << aln_stats.GetAnchorIdMap().size() << "):" << endl;
    ITERATE(TAlnStats::TIdMap, it, aln_stats.GetAnchorIdMap()) {
        out << it->first->AsString() << " (base_width=" << it->first->GetBaseWidth() << ")" << endl;
    }
    out << endl;
    out << "AnchorIdxVec (" << aln_stats.GetAnchorIdxVec().size() << "):" << endl;
    ITERATE(TAlnStats::TIdxVec, it, aln_stats.GetAnchorIdxVec()) {
        out << *it << endl;
    }
    out << endl;
    for (size_t aln_idx = 0;  aln_idx < aln_stats.GetAlnCount();  ++aln_idx) {
        TAlnStats::TDim dim = aln_stats.GetDimForAln(aln_idx);
        out << "Alignment " << aln_idx << " has " 
            << dim << " rows:" << endl;
        for (TAlnStats::TDim row = 0;  row < dim;  ++row) {
            out << aln_stats.GetSeqIdsForAln(aln_idx)[row]->AsString();
            out << endl;
        }
        out << endl;
    }
    return out;
}


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_SERIAL__HPP
