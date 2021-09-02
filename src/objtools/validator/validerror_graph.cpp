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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of seq_graph
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objmgr/graph_ci.hpp>
#include <objtools/validator/validerror_graph.hpp>
#include <objtools/validator/utilities.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_graph::CValidError_graph(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_graph::~CValidError_graph(void)
{
}


void CValidError_graph::ValidateSeqGraph(const CSeq_graph& graph)
{
    if (graph.GetGraph().IsByte()) {
        const CByte_graph& bg = graph.GetGraph().GetByte();

        // Test that min / max values are in the 0 - 100 range.
        x_ValidateMinValues(bg, graph);
        x_ValidateMaxValues(bg, graph);
    }
}


void CValidError_graph::x_ValidateMinValues(const CByte_graph& bg, const CSeq_graph& graph)
{
    int min = bg.GetMin();
    if ( min < 0  ||  min > 100 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphMin,
            "Graph min (" + NStr::IntToString(min) + ") out of range",
            graph);
    }
}


void CValidError_graph::x_ValidateMaxValues(const CByte_graph& bg, const CSeq_graph& graph)
{
    int max = bg.GetMax();
    if ( max <= 0  ||  max > 100 ) {
        EDiagSev sev = (max <= 0) ? eDiag_Error : eDiag_Warning;
        PostErr(sev, eErr_SEQ_GRAPH_GraphMax,
            "Graph max (" + NStr::IntToString(max) + ") out of range",
            graph);
    }
}


void CValidError_graph::ValidateSeqGraphContext(const CSeq_graph& graph, const CBioseq_set& set)
{
    m_Imp.IncrementMisplacedGraphCount();
}


void CValidError_graph::ValidateSeqGraphContext(const CSeq_graph& graph, const CBioseq& seq)
{
    if (!graph.IsSetLoc()) {
        m_Imp.IncrementMisplacedGraphCount();
    } else {
        CBioseq_Handle bsh = GetCache().GetBioseqHandleFromLocation(
            m_Scope,
            graph.GetLoc(), m_Imp.GetTSE_Handle());
        if (m_Scope->GetBioseqHandle(seq) != bsh) {
            m_Imp.IncrementMisplacedGraphCount();
        }
    }

    if (graph.GetGraph().IsByte()) {
        const CByte_graph& bg = graph.GetGraph().GetByte();

        TSeqPos numval = graph.GetNumval();
        if (numval != bg.GetValues().size()) {
            PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphByteLen,
                "SeqGraph (" + NStr::SizetToString(numval) + ") " +
                "and ByteStore (" + NStr::SizetToString(bg.GetValues().size()) +
                ") length mismatch", seq, graph);
        }
    }
}


bool CValidError_graph::x_ValidateGraphLocation (const CSeq_graph& graph)
{
    if (!graph.IsSetLoc() || graph.GetLoc().Which() == CSeq_loc::e_not_set) {
        PostErr (eDiag_Error, eErr_SEQ_GRAPH_GraphLocInvalid, "SeqGraph location (Unknown) is invalid", graph);
        return false;
    } else {
        const CSeq_loc& loc = graph.GetLoc();
        const CBioseq_Handle & bsh =
            GetCache().GetBioseqHandleFromLocation(
                m_Scope, loc, m_Imp.GetTSE_Handle());
        if (!bsh) {
            string label;
            if (loc.GetId()) {
               loc.GetId()->GetLabel(&label, CSeq_id::eContent);
            }
            if (NStr::IsBlank(label)) {
                label = "unknown";
            }
            PostErr (eDiag_Warning, eErr_SEQ_GRAPH_GraphBioseqId,
                     "Bioseq not found for Graph location " + label, graph);
            return false;
        }
        TSeqPos start = loc.GetStart(eExtreme_Positional);
        TSeqPos stop = loc.GetStop(eExtreme_Positional);
        if (start >= bsh.GetBioseqLength() || stop >= bsh.GetBioseqLength()
            || !loc.IsInt() || loc.GetStrand() == eNa_strand_minus) {
            string label = GetValidatorLocationLabel (loc, *m_Scope);
            PostErr (eDiag_Error, eErr_SEQ_GRAPH_GraphLocInvalid,
                     "SeqGraph location (" + label + ") is invalid", graph);
            return false;
        }
    }
    return true;
}


bool s_CompareTwoSeqGraphs(const CRef <CSeq_graph> g1,
                                    const CRef <CSeq_graph> g2)
{
    if (!g1->IsSetLoc()) {
        return true;
    } else if (!g2->IsSetLoc()) {
        return false;
    }

    TSeqPos start1 = g1->GetLoc().GetStart(eExtreme_Positional);
    TSeqPos stop1 = g1->GetLoc().GetStop(eExtreme_Positional);
    TSeqPos start2 = g2->GetLoc().GetStart(eExtreme_Positional);
    TSeqPos stop2 = g2->GetLoc().GetStop(eExtreme_Positional);

    if (start1 < start2) {
        return true;
    } else if (start1 == start2 && stop1 < stop2) {
        return true;
    } else {
        return false;
    }
}


void CValidError_graph::ValidateGraphsOnBioseq(const CBioseq& seq)
{
    if ( !seq.IsNa() || !seq.IsSetAnnot() ) {
        return;
    }

    vector <CRef <CSeq_graph> > graph_list;

    FOR_EACH_ANNOT_ON_BIOSEQ (it, seq) {
        if ((*it)->IsGraph()) {
            FOR_EACH_GRAPH_ON_ANNOT(git, **it) {
                if (IsSupportedGraphType(**git)) {
                    CRef <CSeq_graph> r(*git);
                    graph_list.push_back(r);
                }
            }
        }
    }

    if (graph_list.size() == 0) {
        return;
    }

    int     last_loc = -1;
    bool    overlaps = false;
    const CSeq_graph* overlap_graph = nullptr;
    SIZE_TYPE num_graphs = 0;
    SIZE_TYPE graphs_len = 0;

    const CSeq_inst& inst = seq.GetInst();

    x_ValidateGraphOrderOnBioseq (seq, graph_list);

    // now sort, so that we can look for coverage
    sort (graph_list.begin(), graph_list.end(), s_CompareTwoSeqGraphs);

    SIZE_TYPE Ns_with_score = 0,
        gaps_with_score = 0,
        ACGTs_without_score = 0,
        vals_below_min = 0,
        vals_above_max = 0,
        num_bases = 0;

    int first_N = -1,
        first_ACGT = -1;

    for (vector<CRef <CSeq_graph> >::iterator grp = graph_list.begin(); grp != graph_list.end(); ++grp) {
        const CSeq_graph& graph = **grp;

        // Currently we support only byte graphs
        x_ValidateGraphValues(graph, seq, first_N, first_ACGT, num_bases, Ns_with_score, gaps_with_score, ACGTs_without_score, vals_below_min, vals_above_max);

        if (graph.IsSetLoc() && graph.GetLoc().Which() != CSeq_loc::e_not_set) {
            // Test for overlapping graphs
            const CSeq_loc& loc = graph.GetLoc();
            if ( (int)loc.GetTotalRange().GetFrom() <= last_loc ) {
                overlaps = true;
                overlap_graph = &graph;
            }
            last_loc = loc.GetTotalRange().GetTo();
        }

        graphs_len += graph.GetNumval();
        ++num_graphs;
    }

    if ( ACGTs_without_score > 0 ) {
        if (ACGTs_without_score * 10 > num_bases) {
            double pct = (double) (ACGTs_without_score) * 100.0 / (double) num_bases;
            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphACGTScoreMany,
                    NStr::SizetToString (ACGTs_without_score) + " ACGT bases ("
                    + NStr::DoubleToString (pct, 2) + "%) have zero score value - first one at position "
                    + NStr::IntToString (first_ACGT + 1),
                    seq);
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphACGTScore,
                NStr::SizetToString(ACGTs_without_score) +
                " ACGT bases have zero score value - first one at position " +
                NStr::IntToString(first_ACGT + 1), seq);
        }
    }
    if ( Ns_with_score > 0 ) {
        if (Ns_with_score * 10 > num_bases) {
            double pct = (double) (Ns_with_score) * 100.0 / (double) num_bases;
            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphNScoreMany,
                    NStr::SizetToString(Ns_with_score) + " N bases ("
                    + NStr::DoubleToString(pct, 2) + "%) have positive score value - first one at position "
                    + NStr::IntToString(first_N + 1),
                    seq);
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphNScore,
                NStr::SizetToString(Ns_with_score) +
                " N bases have positive score value - first one at position " +
                NStr::IntToString(first_N + 1), seq);
        }
    }
    if ( gaps_with_score > 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphGapScore,
            NStr::SizetToString(gaps_with_score) +
            " gap bases have positive score value",
            seq);
    }
    if ( vals_below_min > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphBelow,
            NStr::SizetToString(vals_below_min) +
            " quality scores have values below the reported minimum or 0",
            seq);
    }
    if ( vals_above_max > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphAbove,
            NStr::SizetToString(vals_above_max) +
            " quality scores have values above the reported maximum or 100",
            seq);
    }

    if ( overlaps ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphOverlap,
            "Graph components overlap, with multiple scores for "
            "a single base", seq, *overlap_graph);
    }

    SIZE_TYPE seq_len = GetUngappedSeqLen(seq);
    if ( (seq_len != graphs_len)  &&  (inst.GetLength() != graphs_len) ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphBioseqLen,
            "SeqGraph (" + NStr::SizetToString(graphs_len) + ") and Bioseq (" +
            NStr::SizetToString(seq_len) + ") length mismatch", seq);
    }

    if ( inst.GetRepr() == CSeq_inst::eRepr_delta  &&  num_graphs > 1 ) {
        x_ValidateGraphOnDeltaBioseq(seq);
    }

}


//look for Seq-graphs out of order
void CValidError_graph::x_ValidateGraphOrderOnBioseq (const CBioseq& seq, vector <CRef <CSeq_graph> > graph_list)
{
    if (graph_list.size() < 2) {
        return;
    }

    TSeqPos last_left = graph_list[0]->GetLoc().GetStart(eExtreme_Positional);
    TSeqPos last_right = graph_list[0]->GetLoc().GetStop(eExtreme_Positional);

    for (size_t i = 1; i < graph_list.size(); i++) {
        TSeqPos left = graph_list[i]->GetLoc().GetStart(eExtreme_Positional);
        TSeqPos right = graph_list[i]->GetLoc().GetStop(eExtreme_Positional);

        if (left < last_left
            || (left == last_left && right < last_right)) {
            PostErr (eDiag_Warning, eErr_SEQ_GRAPH_GraphOutOfOrder,
                     "Graph components are out of order - may be a software bug",
                     *graph_list[i]);
            return;
        }
        last_left = left;
        last_right = right;
    }
}


void CValidError_graph::x_ValidateGraphValues
(const CSeq_graph& graph,
 const CBioseq& seq,
 int& first_N,
 int& first_ACGT,
 size_t& num_bases,
 size_t& Ns_with_score,
 size_t& gaps_with_score,
 size_t& ACGTs_without_score,
 size_t& vals_below_min,
 size_t& vals_above_max)
{
    string label;
    seq.GetFirstId()->GetLabel(&label);

    if (!x_ValidateGraphLocation(graph)) {
        return;
    }

    try {
        const CByte_graph& bg = graph.GetGraph().GetByte();
        int min = bg.GetMin();
        int max = bg.GetMax();

        const CSeq_loc& gloc = graph.GetLoc();
        CRef<CSeq_loc> tmp(new CSeq_loc());
        tmp->Assign(gloc);
        tmp->SetStrand(eNa_strand_plus);

        CSeqVector vec(*tmp, *m_Scope,
            CBioseq_Handle::eCoding_Ncbi,
            sequence::GetStrand(gloc, m_Scope));
        vec.SetCoding(CSeq_data::e_Ncbi4na);

        CSeqVector::const_iterator seq_begin = vec.begin();
        CSeqVector::const_iterator seq_end = vec.end();
        CSeqVector::const_iterator seq_iter = seq_begin;

        const CByte_graph::TValues& values = bg.GetValues();
        CByte_graph::TValues::const_iterator val_iter = values.begin();
        CByte_graph::TValues::const_iterator val_end = values.end();

        size_t score_pos = 0;

        while (seq_iter != seq_end && score_pos < graph.GetNumval()) {
            CSeqVector::TResidue res = *seq_iter;
            if (IsResidue(res)) {
                short val;
                if (val_iter == val_end) {
                    val = 0;
                } else {
                    val = (short)(*val_iter);
                    ++val_iter;
                }
                // counting total number of bases, to look for percentage of bases with score of zero
                num_bases++;

                if ((val < min) || (val < 0)) {
                    vals_below_min++;
                }
                if ((val > max) || (val > 100)) {
                    vals_above_max++;
                }

                switch (res) {
                case 0:     // gap
                    if (val > 0) {
                        gaps_with_score++;
                    }
                    break;

                case 1:     // A
                case 2:     // C
                case 4:     // G
                case 8:     // T
                    if (val == 0) {
                        ACGTs_without_score++;
                        if (first_ACGT == -1) {
                            first_ACGT = seq_iter.GetPos() + gloc.GetStart(eExtreme_Positional);
                        }
                    }
                    break;

                case 15:    // N
                    if (val > 0) {
                        Ns_with_score++;
                        if (first_N == -1) {
                            first_N = seq_iter.GetPos() + gloc.GetStart(eExtreme_Positional);
                        }
                    }
                    break;
                }
            }
            ++seq_iter;
            ++score_pos;
        }
    } catch (CException& e) {
        PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
            string("Exception while validating graph values. EXCEPTION: ") +
            e.what(), seq);
    }

}


void CValidError_graph::x_ValidateGraphOnDeltaBioseq(const CBioseq& seq)
{
    const CDelta_ext& delta = seq.GetInst().GetExt().GetDelta();
    CDelta_ext::Tdata::const_iterator curr = delta.Get().begin(),
        next = curr,
        end = delta.Get().end();

    SIZE_TYPE   num_delta_seq = 0;
    TSeqPos offset = 0;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    CGraph_CI grp(bsh);
    while (grp && !IsSupportedGraphType(grp->GetOriginalGraph())) {
        ++grp;
    }
    while ( curr != end && grp ) {
        const CSeq_graph& graph = grp->GetOriginalGraph();
        ++next;
        switch ( (*curr)->Which() ) {
            case CDelta_seq::e_Loc:
                {
                    const CSeq_loc& loc = (*curr)->GetLoc();
                    if ( !loc.IsNull() ) {
                        TSeqPos loclen = sequence::GetLength(loc, m_Scope);
                        if ( graph.GetNumval() != loclen ) {
                            PostErr(eDiag_Warning, eErr_SEQ_GRAPH_GraphSeqLocLen,
                                "SeqGraph (" + NStr::IntToString(graph.GetNumval()) +
                                ") and SeqLoc (" + NStr::IntToString(loclen) +
                                ") length mismatch", graph);
                        }
                        offset += loclen;
                        ++num_delta_seq;
                    }
                    ++grp;
                    while (grp && !IsSupportedGraphType(grp->GetOriginalGraph())) {
                        ++grp;
                    }
                }
                break;

            case CDelta_seq::e_Literal:
                {
                    const CSeq_literal& lit = (*curr)->GetLiteral();
                    TSeqPos litlen = lit.GetLength(),
                        nextlen = 0;
                    if ( lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap() ) {
                        while (next != end  &&  x_GetLitLength(**next, nextlen)) {
                            litlen += nextlen;
                            ++next;
                        }
                        if ( graph.GetNumval() != litlen ) {
                            PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphSeqLitLen,
                                "SeqGraph (" + NStr::IntToString(graph.GetNumval()) +
                                ") and SeqLit (" + NStr::IntToString(litlen) +
                                ") length mismatch", graph);
                        }
                        const CSeq_loc& graph_loc = graph.GetLoc();
                        if ( graph_loc.IsInt() ) {
                            TSeqPos from = graph_loc.GetTotalRange().GetFrom();
                            TSeqPos to = graph_loc.GetTotalRange().GetTo();
                            if (  from != offset ) {
                                PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphStartPhase,
                                    "SeqGraph (" + NStr::IntToString(from) +
                                    ") and SeqLit (" + NStr::IntToString(offset) +
                                    ") start do not coincide",
                                    graph);
                            }

                            if ( to != offset + litlen - 1 ) {
                                PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphStopPhase,
                                    "SeqGraph (" + NStr::IntToString(to) +
                                    ") and SeqLit (" +
                                    NStr::IntToString(litlen + offset - 1) +
                                    ") stop do not coincide",
                                    graph);
                            }
                        }
                        ++grp;
                        while (grp && !IsSupportedGraphType(grp->GetOriginalGraph())) {
                            ++grp;
                        }
                        ++num_delta_seq;
                    }
                    offset += litlen;
                }
                break;

            default:
                break;
        }
        curr = next;
    }

    // if there are any left, count the remaining delta seqs that should have graphs
    while ( curr != end) {
        ++next;
        switch ( (*curr)->Which() ) {
            case CDelta_seq::e_Loc:
                {
                    const CSeq_loc& loc = (*curr)->GetLoc();
                    if ( !loc.IsNull() ) {
                        ++num_delta_seq;
                    }
                }
                break;

            case CDelta_seq::e_Literal:
                {
                    const CSeq_literal& lit = (*curr)->GetLiteral();
                    TSeqPos litlen = lit.GetLength(),
                        nextlen = 0;
                    if ( lit.IsSetSeq_data() ) {
                        while (next != end  &&  x_GetLitLength(**next, nextlen)) {
                            litlen += nextlen;
                            ++next;
                        }
                        ++num_delta_seq;
                    }
                }
                break;

            default:
                break;
        }
        curr = next;
    }

    SIZE_TYPE num_graphs = 0;
    grp.Rewind();
    while (grp) {
        if (IsSupportedGraphType(grp->GetOriginalGraph())) {
            ++num_graphs;
        }
        ++grp;
    }

    if ( num_delta_seq != num_graphs ) {
        PostErr(eDiag_Error, eErr_SEQ_GRAPH_GraphDiffNumber,
            "Different number of SeqGraph (" +
            NStr::SizetToString(num_graphs) + ") and SeqLit (" +
            NStr::SizetToString(num_delta_seq) + ") components",
            seq);
    }
}


// Currently we support only phrap, phred or gap4 types with byte values.
bool CValidError_graph::IsSupportedGraphType(const CSeq_graph& graph)
{
    string title;
    if ( graph.IsSetTitle() ) {
        title = graph.GetTitle();
    }
    if ( NStr::CompareNocase(title, "Phrap Quality") == 0  ||
         NStr::CompareNocase(title, "Phred Quality") == 0  ||
         NStr::CompareNocase(title, "Gap4") == 0 ) {
        if ( graph.GetGraph().IsByte() ) {
            return true;
        }
    }
    return false;
}


// NOTE - this returns the length of the non-gap portions of the sequence
SIZE_TYPE CValidError_graph::GetUngappedSeqLen(const CBioseq& seq)
{
    SIZE_TYPE seq_len = 0;
    const CSeq_inst & inst = seq.GetInst();

    if ( inst.GetRepr() == CSeq_inst::eRepr_raw ) {
        seq_len = inst.GetLength();
    } else if ( inst.GetRepr() == CSeq_inst::eRepr_delta ) {
        const CDelta_ext& delta = inst.GetExt().GetDelta();
        ITERATE( CDelta_ext::Tdata, dseq, delta.Get() ) {
            switch( (*dseq)->Which() ) {
            case CDelta_seq::e_Loc:
                seq_len += sequence::GetLength((*dseq)->GetLoc(), m_Scope);
                break;
            case CDelta_seq::e_Literal:
                if ( (*dseq)->GetLiteral().IsSetSeq_data() && !(*dseq)->GetLiteral().GetSeq_data().IsGap() ) {
                    seq_len += (*dseq)->GetLiteral().GetLength();
                }
                break;
            default:
                break;
            }
        }
    }
    return seq_len;
}


bool CValidError_graph::x_GetLitLength(const CDelta_seq& delta, TSeqPos& len)
{
    len = 0;
    if ( delta.IsLiteral() ) {
        const CSeq_literal& lit = delta.GetLiteral();
        if ( lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap()) {
            len = lit.GetLength();
            return true;
        }
    }
    return false;
}





END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
