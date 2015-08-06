/*  $Id$
  ===========================================================================
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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/aligncollapser.hpp>
#include <algo/gnomon/id_handler.hpp>
#include <corelib/ncbiargs.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include "gnomon_seq.hpp"


BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(gnomon)
USING_SCOPE(sequence);

string GetTargetAcc(int shift, const deque<char>& id_pool) {
    string target;
    for(int i = shift; id_pool[i] != 0; ++i)
        target.push_back(id_pool[i]);

    return target;
}

CAlignModel CAlignCommon::GetAlignment(const SAlignIndividual& ali, const deque<char>& target_id_pool) const {

    CGeneModel a(isPlus() ? ePlus : eMinus, 0, isEST() ? CGeneModel::eEST : CGeneModel::eSR);
    if(isPolyA()) 
        a.Status() |= CGeneModel::ePolyA;
    if(isCap())
        a.Status() |= CGeneModel::eCap;
    if(isUnknown()) 
        a.Status()|= CGeneModel::eUnknownOrientation;
    a.SetID(ali.m_align_id >= 0 ? ali.m_align_id : -ali.m_align_id);
    a.SetWeight(ali.m_weight);
    if(ali.m_align_id < 0)
        a.Status() |= CGeneModel::eChangedByFilter;

    if(m_introns.empty()) {
        a.AddExon(ali.m_range);
    } else {
        string fs;
        string ss;
        if(!m_introns.front().m_sig.empty()) {
            if(a.Strand() == ePlus)
                ss = m_introns.front().m_sig.substr(0,2);
            else
                ss = m_introns.front().m_sig.substr(2,2);
        }
        a.AddExon(TSignedSeqRange(ali.m_range.GetFrom(), m_introns.front().m_range.GetFrom()), fs, ss);
        for(int i = 0; i < (int)m_introns.size()-1; ++i) {
            if(!m_introns[i].m_sig.empty() && !m_introns[i+1].m_sig.empty()) {
                if(a.Strand() == ePlus) {
                    fs = m_introns[i].m_sig.substr(2,2);
                    ss = m_introns[i+1].m_sig.substr(0,2);
                } else {
                    fs = m_introns[i].m_sig.substr(0,2);
                    ss = m_introns[i+1].m_sig.substr(2,2);
                }
            }
            a.AddExon(TSignedSeqRange(m_introns[i].m_range.GetTo(), m_introns[i+1].m_range.GetFrom()), fs, ss);
        }
        if(!m_introns.back().m_sig.empty()) {
            if(a.Strand() == ePlus)
                fs = m_introns.back().m_sig.substr(2,2);
            else
                fs = m_introns.back().m_sig.substr(0,2);
        }
        ss = "";
        a.AddExon(TSignedSeqRange(m_introns.back().m_range.GetTo(), ali.m_range.GetTo()), fs, ss);
    }

    CAlignMap amap(a.Exons(), a.FrameShifts(), a.Strand());
    CAlignModel align(a, amap);

    CRef<CSeq_id> target_id(CIdHandler::ToSeq_id(GetTargetAcc(ali.m_target_id, target_id_pool)));
    align.SetTargetId(*target_id);

    return align;
};

struct LeftAndLongFirstOrder {
    LeftAndLongFirstOrder(const deque<char>& idp) : id_pool(idp) {}
    const deque<char>& id_pool;

    bool operator() (const SAlignIndividual& a, const SAlignIndividual& b) {  // left and long first
        if(a.m_range == b.m_range)
            return GetTargetAcc(a.m_target_id,id_pool) < GetTargetAcc(b.m_target_id,id_pool);
        else if(a.m_range.GetFrom() != b.m_range.GetFrom())
            return a.m_range.GetFrom() < b.m_range.GetFrom();
        else
            return a.m_range.GetTo() > b.m_range.GetTo();
    }
};

bool OriginalOrder(const SAlignIndividual& a, const SAlignIndividual& b) {  // the order in which alignmnets were added
    return a.m_target_id < b.m_target_id;
}



CAlignCommon::CAlignCommon(const CGeneModel& align) {

    m_flags = 0;
    if(align.Type()&CGeneModel::eSR)
        m_flags |= esr;
    if(align.Type()&CGeneModel::eEST)
        m_flags |= eest;
    if(align.Status()&CGeneModel::ePolyA)
        m_flags |= epolya;
    if(align.Status()&CGeneModel::eCap)
        m_flags |= ecap;

    if(align.Status()&CGeneModel::eUnknownOrientation) {
        m_flags |= eunknownorientation;
        m_flags |= eplus;
    } else if(align.Strand() == ePlus){
        m_flags |= eplus;
    } else {
        m_flags |= eminus;
    }

    const CGeneModel::TExons& e = align.Exons();
    for(int i = 1; i < (int)e.size(); ++i) {
        if(e[i-1].m_ssplice && e[i].m_fsplice) {
            string sig;
            if(align.Strand() == ePlus)
                sig = e[i-1].m_ssplice_sig+e[i].m_fsplice_sig;
            else
                sig = e[i].m_fsplice_sig+e[i-1].m_ssplice_sig;
            SIntron intron(e[i-1].GetTo(), e[i].GetFrom(), align.Strand(), (align.Status()&CGeneModel::eUnknownOrientation) == 0, sig);
            m_introns.push_back(intron);
        }
    }
}

struct SAlignExtended {
    SAlignExtended(SAlignIndividual& ali, const set<int>& left_exon_ends, const set<int>& right_exon_ends) : m_ali(&ali), m_initial_right_end(ali.m_range.GetTo()) {

        set<int>::const_iterator ri =
            right_exon_ends.lower_bound(m_ali->m_range.GetTo());  // leftmost compatible rexon
        m_rlimb =  numeric_limits<int>::max();
        if(ri != right_exon_ends.end())
            m_rlimb = *ri;                                        // position of leftmost compatible rexon
        m_rlima = -1;
        if(ri != right_exon_ends.begin())
            m_rlima = *(--ri);                                    // position of the rightmost incompatible rexon
        set<int>::const_iterator li =
            left_exon_ends.upper_bound(m_ali->m_range.GetFrom()); // leftmost not compatible lexon
        m_llimb = numeric_limits<int>::max() ;
        if(li != left_exon_ends.end())
            m_llimb = *li;                                        // position of the leftmost not compatible lexon
    }

    SAlignIndividual* m_ali;
    int m_initial_right_end;
    int m_rlimb;
    int m_rlima;
    int m_llimb;
};

void CAlignCollapser::SetupArgDescriptions(CArgDescriptions* arg_desc) {
    arg_desc->SetCurrentGroup("Collapsing and filtering");

    arg_desc->AddFlag("filtersr","Filter SR");
    arg_desc->AddFlag("filterest","Filter EST");
    arg_desc->AddFlag("filtermrna","Filter mRNA");
    arg_desc->AddFlag("filterprots","Filter proteins");
    arg_desc->AddFlag("collapsest","Collaps EST");
    arg_desc->AddFlag("collapssr","Collaps SR");
    arg_desc->AddFlag("fillgenomicgaps","Use provided selfspecies cDNA for genomic gap filling");

    arg_desc->AddDefaultKey("max-extension", "MaxExtension",
                            "Maximal extension for one-exon collapsed alignments",
                            CArgDescriptions::eInteger, "20");

    arg_desc->AddDefaultKey("min-consensus-support", "MinConsensusSupport",
                            "Minimal number of support for consensus intron",
                            CArgDescriptions::eInteger, "2");

    arg_desc->AddDefaultKey("min-non-consensussupport", "MinNonconsensusSupport",
                            "Minimal number of support for non-consensus intron",
                            CArgDescriptions::eInteger, "10");

    arg_desc->AddDefaultKey("high-identity", "HighIdentity",
                            "Minimal exon identity threshold for accepted introns",
                            CArgDescriptions::eDouble, "0.98");

    arg_desc->AddDefaultKey("min-support-fraction", "MinSupportFraction",
                            "Minimal splice expression relative exon expression",
                            CArgDescriptions::eDouble, "0.03");

    arg_desc->AddDefaultKey("end-pair-support-cutoff", "EndOairSupportCutoff",
                            "Minimal expression relative to the mean for introns with the same splice",
                            CArgDescriptions::eDouble, "0.1");

    arg_desc->AddDefaultKey("minest", "minest",
                            "Minimal EST support to trump expression checks",
                            CArgDescriptions::eInteger, "3");

    arg_desc->AddDefaultKey("min-edge-coverage", "MinEdgeCoverage",
                            "Minimal absolute expression for accepted single-exon alignmnets without polyA/Cap",
                            CArgDescriptions::eInteger, "5");

    arg_desc->AddDefaultKey("sharp-boundary", "SharpBoundary",
                            "Minimal relative expression for crossing splice",
                            CArgDescriptions::eDouble, "0.2");

    arg_desc->SetCurrentGroup("");
}

CAlignCollapser::CAlignCollapser(string contig, CScope* scope) : m_count(0), m_scope(scope) {
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    m_filtersr = args["filtersr"];
    m_filterest = args["filterest"];
    m_filtermrna = args["filtermrna"];
    m_filterprots = args["filterprots"];
    m_collapsest = args["collapsest"];
    m_collapssr = args["collapssr"];
    m_fillgenomicgaps = args["fillgenomicgaps"];

    if(m_scope != 0 && contig != "") {

        m_contig_name = contig;

        CRef<CSeq_id> contigid(new CSeq_id);
        contigid->Assign(*CIdHandler::ToSeq_id(contig)); 
        if(!contigid) 
            contigid = new CSeq_id(CSeq_id::e_Local, contig);

        CBioseq_Handle bh (m_scope->GetBioseqHandle(*contigid));
        if (!bh) {
            NCBI_THROW(CException, eUnknown, "contig '"+contig+"' retrieval failed");
        }
        CSeqVector sv (bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
        int length (sv.size());

        sv.GetSeqData(0, length, m_contig);

        m_contigrv.resize(length);
        copy(m_contig.begin(),m_contig.end(),m_contigrv.begin());

        TIntMap::iterator current_gap = m_genomic_gaps_len.end();
        for(int i = 0; i < length; ++i) {
            if(sv.IsInGap(i)) {
                if(current_gap == m_genomic_gaps_len.end())
                    current_gap = m_genomic_gaps_len.insert(TIntMap::value_type(i,1)).first;
                else
                    ++current_gap->second;
            } else {
                current_gap = m_genomic_gaps_len.end();
            }
        }

        m_genomic_gaps_len[-1] = 1;     // fake gap at the beginning
        m_genomic_gaps_len[length] = 1; // fake gap at the end
    }
}


bool AlignmentMarkedForDeletion(const SAlignIndividual& ali) {
    return ali.m_weight < 0;
}

#define COVERED_FRACTION 0.75
bool AlignmentIsSupportedBySR(const CAlignModel& align, const vector<double>& coverage, int mincoverage, int left_end) {

    int align_len = align.AlignLen();

    int covered_length = 0;
    ITERATE(CGeneModel::TExons, i, align.Exons()) {
        for(int p = i->Limits().GetFrom(); p <= i->Limits().GetTo(); ++p) 
            if(coverage[p-left_end] >= mincoverage)
                ++covered_length;            
    }

    return (covered_length >= COVERED_FRACTION*align_len);
}

bool isGoodIntron(int a, int b, EStrand strand, const CAlignCollapser::TAlignIntrons& introns, bool check_introns_on_both_strands) {
    SIntron intron_oriented_nosig(a, b, strand, true, "");
    SIntron intron_notoriented_nosig(a, b, ePlus, false, "");
    bool good_intron = (introns.find(intron_oriented_nosig) != introns.end() || introns.find(intron_notoriented_nosig) != introns.end());
    if(!good_intron && check_introns_on_both_strands) {
        SIntron intron_otherstrand_nosig(a, b, OtherStrand(strand), true, "");
        good_intron = (introns.find(intron_otherstrand_nosig) != introns.end());
    }

    return good_intron;
}


#define END_PART_LENGTH 35

void CAlignCollapser::ClipNotSupportedFlanks(CAlignModel& align, double clip_threshold) {

    double cov = 0;
    int nt = 0;
    ITERATE(CGeneModel::TExons, e, align.Exons()) {
        for(int i = e->GetFrom(); i <= e->GetTo(); ++i) {
            cov += m_coverage[i-m_left_end];
            ++nt;
        }
    }
    cov /= nt;

    CAlignMap amap = align.GetAlignMap();
    TSignedSeqRange old_limits = align.Limits();

    if(align.Type()&CGeneModel::eNotForChaining) {
        TSignedSeqRange tlim = align.TranscriptLimits();
        int not_aligned_left = tlim.GetFrom();
        int not_aligned_right = align.TargetLen()-1-tlim.GetTo();
        if(align.Orientation() == eMinus)
            swap(not_aligned_left,not_aligned_right);

        if(not_aligned_left > 30) {
            int l = align.Limits().GetFrom();
            int ie = 0;
            while(l < align.Limits().GetTo() && m_coverage[l-m_left_end] < clip_threshold*cov) {
                if(l < align.Exons()[ie].GetTo())
                    ++l;
                else
                    l = align.Exons()[++ie].GetFrom();
            }
            if(l != align.Limits().GetFrom()) {
                TSignedSeqRange seg = amap.ShrinkToRealPoints(TSignedSeqRange(l,align.Limits().GetTo()), false);
                if(seg.Empty() || amap.FShiftedLen(seg,false) < END_PART_LENGTH) {
                    align.ClearExons();
                    return;
                } else {
                    align.Clip(seg,CGeneModel::eRemoveExons);
                }
            }
        }

        if(not_aligned_right > 30) {
            int r = align.Limits().GetTo();
            int ie = align.Exons().size()-1;
            while(r > align.Limits().GetFrom() && m_coverage[r-m_left_end] < clip_threshold*cov) {
                if(r > align.Exons()[ie].GetFrom())
                    --r;
                else
                    r = align.Exons()[--ie].GetTo();
            }
            if(r != align.Limits().GetTo()) {
                TSignedSeqRange seg = amap.ShrinkToRealPoints(TSignedSeqRange(align.Limits().GetFrom(),r), false);
                if(seg.Empty() || amap.FShiftedLen(seg,false) < END_PART_LENGTH) {
                    align.ClearExons();
                    return;
                } else {
                    align.Clip(seg,CGeneModel::eRemoveExons);
                }
            }
        }
    }

    bool snap_to_codons = align.Type()&CAlignModel::eProt; 
    bool keepdoing = true;
    while(keepdoing) {
        keepdoing = false;
        for (int k = 1; k < (int)align.Exons().size(); ++k) {
            CModelExon exonl = align.Exons()[k-1];
            CModelExon exonr = align.Exons()[k];
            if(!(exonl.m_ssplice && exonr.m_fsplice)) {
                int l = exonl.GetTo();
                TSignedSeqRange segl(align.Limits().GetFrom(),l);
                for( ; l >= exonl.GetFrom() && m_coverage[l-m_left_end] < clip_threshold*cov; --l);
                if(l != exonl.GetTo())
                    segl = amap.ShrinkToRealPoints(TSignedSeqRange(align.Limits().GetFrom(),max(align.Limits().GetFrom(),l)),snap_to_codons);

                int r = exonr.GetFrom();
                TSignedSeqRange segr(r,align.Limits().GetTo());
                for( ; r <= exonr.GetTo() && m_coverage[r-m_left_end] < clip_threshold*cov; ++r);
                if(r != exonr.GetFrom())
                    segr = amap.ShrinkToRealPoints(TSignedSeqRange(min(align.Limits().GetTo(),r),align.Limits().GetTo()), snap_to_codons);

                if(segl.Empty() || amap.FShiftedLen(segl,false) < END_PART_LENGTH) {
                    if(segr.Empty() || amap.FShiftedLen(segr,false) < END_PART_LENGTH) {
                        align.ClearExons();
                        return;
                    } else {
                        align.Clip(segr,CGeneModel::eRemoveExons);
                        keepdoing = true;
                        break;                    
                    }
                } else if(segr.Empty() || amap.FShiftedLen(segr,false) < END_PART_LENGTH) {
                    align.Clip(segl,CGeneModel::eRemoveExons);
                    keepdoing = true;
                    break;                    
                } else if(l != exonl.GetTo() || r != exonr.GetFrom()) {
                    align.CutExons(TSignedSeqRange(segl.GetTo()+1,segr.GetFrom()-1));
                    keepdoing = true;
                    break;
                }                
            }
        }
    }

    for(int prev_exon = -1; prev_exon < (int)align.Exons().size()-1; ++prev_exon) {
        int piece_begin = prev_exon+1;
        if(align.Exons()[piece_begin].m_fsplice)
            continue;
        int piece_end = piece_begin;
        for( ; piece_end < (int)align.Exons().size() && align.Exons()[piece_end].m_ssplice; ++piece_end);
        int a = align.Exons()[piece_begin].GetFrom();
        int b = align.Exons()[piece_end].GetTo();
        if(amap.FShiftedLen(a, b, false) < END_PART_LENGTH) {
            if(a == align.Limits().GetFrom() && b == align.Limits().GetTo()) {
                align.ClearExons();
                return;
            } else if(a == align.Limits().GetFrom()) {
                TSignedSeqRange seg(align.Exons()[piece_end+1].GetFrom(),align.Limits().GetTo());
                align.Clip(seg, CGeneModel::eRemoveExons);
            } else if(b == align.Limits().GetTo()) {
                TSignedSeqRange seg(align.Limits().GetFrom(),align.Exons()[piece_begin-1].GetTo());
                align.Clip(seg, CGeneModel::eRemoveExons);
            } else {
                TSignedSeqRange seg(a, b);
                align.CutExons(seg);
            }
        }
    }

    if((align.Status()&CGeneModel::ePolyA) && 
       ((align.Strand() == ePlus && align.Limits().GetTo() != old_limits.GetTo()) || 
        (align.Strand() == eMinus && align.Limits().GetFrom() != old_limits.GetFrom()))) {  // clipped polyA

        align.Status() ^= CGeneModel::ePolyA;
    }
    if((align.Status()&CGeneModel::eCap) && 
       ((align.Strand() == eMinus && align.Limits().GetTo() != old_limits.GetTo()) || 
        (align.Strand() == ePlus && align.Limits().GetFrom() != old_limits.GetFrom()))) {  // clipped cap

        align.Status() ^= CGeneModel::eCap;
    }
}


#define CUT_MARGIN 15

bool CAlignCollapser::RemoveNotSupportedIntronsFromProt(CAlignModel& align) {

    CAlignMap amap = align.GetAlignMap();

    bool keepdoing = true;
    while(keepdoing) {
        keepdoing = false;
        for (int k = 1; k < (int)align.Exons().size(); ++k) {
            CModelExon exonl = align.Exons()[k-1];
            CModelExon exonr = align.Exons()[k];
            if(!(exonl.m_ssplice && exonr.m_fsplice) || isGoodIntron(exonl.GetTo(), exonr.GetFrom(), align.Strand(), m_align_introns, false))
                continue;

            TSignedSeqRange segl;
            if(exonl.GetTo()-CUT_MARGIN > align.Limits().GetFrom())
                segl = amap.ShrinkToRealPoints(TSignedSeqRange(align.Limits().GetFrom(),exonl.GetTo()-CUT_MARGIN), true);

            TSignedSeqRange segr;
            if(exonr.GetFrom()+CUT_MARGIN < align.Limits().GetTo())
                segr = amap.ShrinkToRealPoints(TSignedSeqRange(exonr.GetFrom()+CUT_MARGIN,align.Limits().GetTo()), true);

            if(segl.Empty() || amap.FShiftedLen(segl,false) < END_PART_LENGTH) {
                if(segr.Empty() || amap.FShiftedLen(segr,false) < END_PART_LENGTH) {
                    align.ClearExons();
                    return false;
                } else {
                    align.Clip(segr,CGeneModel::eRemoveExons);
                    keepdoing = true;
                    break;                    
                }
            } else if(segr.Empty() || amap.FShiftedLen(segr,false) < END_PART_LENGTH) {
                align.Clip(segl,CGeneModel::eRemoveExons);
                keepdoing = true;
                break;                    
            } else {
                align.CutExons(TSignedSeqRange(segl.GetTo()+1,segr.GetFrom()-1));
                keepdoing = true;
                break;
            }
        }
    }

    return true;
}

bool CAlignCollapser::RemoveNotSupportedIntronsFromTranscript(CAlignModel& align, bool check_introns_on_both_strands) const {

    CAlignMap amap = align.GetAlignMap();

    CGeneModel editedmodel = align;

    if(!(editedmodel.Status()&CGeneModel::eGapFiller)) {  //remove flanking bad introns AND exons
        editedmodel.ClearExons();  // empty alignment with all atributes and remove indels
        for (CAlignModel::TExons::const_iterator piece_begin = align.Exons().begin(); piece_begin != align.Exons().end(); ++piece_begin) {
            _ASSERT( !piece_begin->m_fsplice );
            
            CAlignModel::TExons::const_iterator piece_end = piece_begin;
            for ( ; piece_end != align.Exons().end() && piece_end->m_ssplice; ++piece_end) ;
            _ASSERT( piece_end != align.Exons().end() );

            CAlignModel a = align;
            a.Clip(TSignedSeqRange(piece_begin->Limits().GetFrom(),piece_end->Limits().GetTo()),CGeneModel::eRemoveExons);  // only one piece   
            
            //remove flanking bad introns
            int new_left = a.Limits().GetFrom();
            for(int k = 1; k < (int)a.Exons().size(); ++k) {
                CModelExon exonl = a.Exons()[k-1];
                CModelExon exonr = a.Exons()[k];
                if(isGoodIntron(exonl.GetTo(), exonr.GetFrom(), a.Strand(), m_align_introns, check_introns_on_both_strands))
                    break;
                else
                    new_left = exonr.GetFrom();
            }
            int new_right = a.Limits().GetTo();
            for(int k = (int)a.Exons().size()-1; k > 0 && a.Exons()[k-1].GetTo() > new_left; --k) {
                CModelExon exonl = a.Exons()[k-1];
                CModelExon exonr = a.Exons()[k];
                if(isGoodIntron(exonl.GetTo(), exonr.GetFrom(), a.Strand(), m_align_introns, check_introns_on_both_strands))
                    break;
                else
                    new_right = exonl.GetTo();
            }

            TSignedSeqRange new_lim(new_left,new_right);
            if(new_lim != a.Limits()) {
                new_lim = amap.ShrinkToRealPoints(new_lim,false);
                a.Clip(new_lim,CGeneModel::eRemoveExons);
                _ASSERT(a.Limits().NotEmpty());
            }

            if(!editedmodel.Exons().empty())
                editedmodel.AddHole();

            ITERATE(CGeneModel::TExons, e, a.Exons()) {
                editedmodel.AddExon(e->Limits(), e->m_fsplice_sig, e->m_ssplice_sig, e->m_ident);
            }
            editedmodel.FrameShifts().insert(editedmodel.FrameShifts().end(),a.FrameShifts().begin(),a.FrameShifts().end());

            piece_begin = piece_end;
        }
    }


    bool good_alignment = true;
    if((align.Type()&CGeneModel::eEST) && (int)editedmodel.Exons().size() == 1 && editedmodel.Limits() != align.Limits())
        good_alignment = false;


    bool keepdoing = true;
    while(keepdoing) {
        keepdoing = false;
        for (int k = 1; k < (int)editedmodel.Exons().size() && good_alignment; ++k) {
            CModelExon exonl = editedmodel.Exons()[k-1];
            CModelExon exonr = editedmodel.Exons()[k];
            if(exonl.m_ssplice && exonr.m_fsplice && !isGoodIntron(exonl.GetTo(), exonr.GetFrom(), editedmodel.Strand(), m_align_introns, check_introns_on_both_strands)) {
                if(editedmodel.Status()&CGeneModel::eGapFiller) {
                    TSignedSeqRange segl =  amap.ShrinkToRealPoints(TSignedSeqRange(editedmodel.Limits().GetFrom(),exonl.GetTo()-1), false);
                    TSignedSeqRange segr = amap.ShrinkToRealPoints(TSignedSeqRange(exonr.GetFrom()+1,editedmodel.Limits().GetTo()), false);
                    if(segl.NotEmpty() && segr.NotEmpty()) {
                        editedmodel.CutExons(TSignedSeqRange(segl.GetTo()+1,segr.GetFrom()-1)); 
                        keepdoing = true;
                        break;                    
                    }
                } else {
                    good_alignment = false;
                }
            }
        }
    }

    vector<TSignedSeqRange> transcript_exons;
    ITERATE(CGeneModel::TExons, e, editedmodel.Exons()) {
        TSignedSeqRange te = amap.MapRangeOrigToEdited(e->Limits(),e->m_fsplice ? CAlignMap::eLeftEnd : CAlignMap::eSinglePoint, e->m_ssplice ? CAlignMap::eRightEnd : CAlignMap::eSinglePoint);
        _ASSERT(te.NotEmpty());
        transcript_exons.push_back(te);
    }

    TSignedSeqRange old_limits = align.Limits();

    CAlignMap editedamap(editedmodel.Exons(), transcript_exons, editedmodel.FrameShifts(), align.Orientation(), align.GetAlignMap().TargetLen());
    CAlignModel editedalign(editedmodel, editedamap);
    editedalign.SetTargetId(*align.GetTargetId());
    align = editedalign;

    if((align.Status()&CGeneModel::ePolyA) && 
       ((align.Strand() == ePlus && align.Limits().GetTo() != old_limits.GetTo()) || 
        (align.Strand() == eMinus && align.Limits().GetFrom() != old_limits.GetFrom()))) {  // clipped polyA

        align.Status() ^= CGeneModel::ePolyA;
    }
    if((align.Status()&CGeneModel::eCap) && 
       ((align.Strand() == eMinus && align.Limits().GetTo() != old_limits.GetTo()) || 
        (align.Strand() == ePlus && align.Limits().GetFrom() != old_limits.GetFrom()))) {  // clipped cap

        align.Status() ^= CGeneModel::eCap;
    }

    return good_alignment;
}

struct GenomicGapsOrder {
    bool operator()(const CInDelInfo& a, const CInDelInfo& b) const
    {
        if(a != b)
            return a < b;
        else
            return a.GetSource().m_acc < b.GetSource().m_acc;
    }
};

#define MISM_PENALTY 10
#define INDEL_PENALTY 20
#define EXTRA_CUT 5
#define BIG_NOT_ALIGNED 20
void CAlignCollapser::CleanSelfTranscript(CAlignModel& align, const string& trans) const {

    string transcript = trans;   // transcript as it appears on the genome
    if(align.Orientation() == eMinus)
        ReverseComplement(transcript.begin(),transcript.end());

    int tlen = align.TargetLen();
    _ASSERT(tlen == (int)transcript.size());

    //expand not splices exons if identical
    CGeneModel::TExons exons = align.Exons();
    vector<TSignedSeqRange> transcript_exons;
    transcript_exons.reserve(exons.size());
    for(int ie = 0; ie < (int)exons.size(); ++ie) {
        transcript_exons.push_back(align.TranscriptExon(ie));
    }
    if(align.Orientation() == eMinus) {
        for(int ie = 0; ie < (int)exons.size(); ++ie) {
            TSignedSeqRange& te = transcript_exons[ie];
            te = TSignedSeqRange(tlen-1-te.GetTo(),tlen-1-te.GetFrom());
        }
    }
    for(int ie = 0; ie < (int)exons.size(); ++ie) {
        if(!exons[ie].m_fsplice) {
            int glim = (ie > 0) ? exons[ie-1].GetTo() : -1;
            int tlim = (ie > 0) ? transcript_exons[ie-1].GetTo() : -1;
            int g = exons[ie].GetFrom();
            int t = transcript_exons[ie].GetFrom();
            while(g > glim+1 && t > tlim+1 && transcript[t-1] == m_contig[g-1]) {
                --t;
                --g;
            }
            if(g < exons[ie].GetFrom()) {
                exons[ie].AddFrom(g-exons[ie].GetFrom());
                exons[ie].m_fsplice_sig.clear();
                transcript_exons[ie].SetFrom(t);
            }
        }
        if(!exons[ie].m_ssplice) {
            int glim = (ie+1 < (int)exons.size()) ? exons[ie+1].GetFrom() : m_contig.size();
            int tlim = (ie+1 < (int)exons.size()) ? transcript_exons[ie+1].GetFrom() : transcript.size();
            int g = exons[ie].GetTo();
            int t = transcript_exons[ie].GetTo();
            while(g < glim-1 && t < tlim-1 && transcript[t+1] == m_contig[g+1]) {
                ++t;
                ++g;
            }
            if(g > exons[ie].GetTo()) {
                exons[ie].AddTo(g-exons[ie].GetTo());
                exons[ie].m_ssplice_sig.clear();
                transcript_exons[ie].SetTo(t);
            }
        }
    }

    CAlignMap amap(exons,transcript_exons, align.FrameShifts(), ePlus, tlen);

    CGeneModel::TExons edited_exons;
    vector<TSignedSeqRange> edited_transcript_exons;

    for (int piece_begin = 0; piece_begin < (int)exons.size(); ++piece_begin) {
        _ASSERT( !exons[piece_begin].m_fsplice );
        int piece_end = piece_begin;
        for( ; exons[piece_end].m_ssplice; ++piece_end);
        _ASSERT(piece_end < (int)exons.size());

        TInDels indels = align.GetInDels(exons[piece_begin].GetFrom(), exons[piece_end].GetTo(), false);
        TInDels::const_iterator indl = indels.begin();

        string tseq;
        string gseq;
        TIVec exons_to_align;
        int tp = transcript_exons[piece_begin].GetFrom();
        for(int ie = piece_begin; ie <= piece_end; ++ie) {
            int gp = exons[ie].GetFrom();
            while(gp <= exons[ie].GetTo()) {
                if(indl == indels.end() || indl->Loc() != gp) {
                    tseq.push_back(transcript[tp++]);
                    gseq.push_back(m_contig[gp++]);
                } else if(indl->IsDeletion()) {
                    tseq += transcript.substr(tp,indl->Len());
                    gseq.insert(gseq.end(),indl->Len(),'-');
                    tp += indl->Len();
                    ++indl;
                } else {
                    tseq.insert(tseq.end(),indl->Len(),'-');
                    gseq += m_contig.substr(gp,indl->Len());
                    gp += indl->Len(); 
                    ++indl;
                }
            }
            if(indl != indels.end() && indl->Loc() == gp) {   // deletion at the end of exon
                _ASSERT(indl->IsDeletion());
                tseq += transcript.substr(tp,indl->Len());
                gseq.insert(gseq.end(), indl->Len(), '-');
                tp += indl->Len();
                ++indl;
            }
            exons_to_align.push_back(gseq.size()-1);
        }
        _ASSERT(tseq.size() == gseq.size() && indl == indels.end());

        TIVec score(tseq.size());
        for(int i = 0; i < (int)score.size(); ++i) {
            if(tseq[i] == gseq[i] && tseq[i] != 'N')
                score[i] = 1;
            else if(tseq[i] == '-' || gseq[i] == '-')
                score[i] = -INDEL_PENALTY;
            else
                score[i] = -MISM_PENALTY;
            if(i > 0)
                score[i] += score[i-1];
            score[i] = max(0,score[i]);
        }

        int align_right = max_element(score.begin(),score.end())-score.begin();

        if(score[align_right] > 0) {  // there is at least one match
            int align_left = align_right;
            while(align_left > 0 && score[align_left-1] > 0)
                --align_left;

            int agaps = count(tseq.begin(), tseq.begin()+align_left, '-');
            int bgaps = count(tseq.begin(), tseq.begin()+align_right, '-');
            TSignedSeqRange trange(transcript_exons[piece_begin].GetFrom()+align_left-agaps, transcript_exons[piece_begin].GetFrom()+align_right-bgaps);

            TSignedSeqRange grange = amap.MapRangeEditedToOrig(trange, false);
            _ASSERT(grange.NotEmpty());
            
            int pb = piece_begin;
            while(exons[pb].GetTo() < grange.GetFrom())
                ++pb;
            int pe = piece_end;
            while(exons[pe].GetFrom() > grange.GetTo())
                --pe;
            _ASSERT(pe >= pb);

            double lident = 0; // left exon identity
            int len = 0;
            for(int i = align_left; i <= (pe > pb ? exons_to_align[pb-piece_begin] : align_right); ++i) {
                ++len;
                if(tseq[i] == gseq[i])
                    ++lident;
            }
            lident /= len;

            double rident = 0; // right exon identity
            len = 0;
            for(int i = align_right; i >= (pe > pb ? exons_to_align[pe-1-piece_begin]+1 : align_left); --i) {
                ++len;
                if(tseq[i] == gseq[i])
                    ++rident;
            }
            rident /= len;

            for( int ie = pb; ie <= pe; ++ie) {
                CModelExon e = exons[ie];
                TSignedSeqRange t = transcript_exons[ie];
                if(ie == pb) {
                    e.m_fsplice = false;
                    e.Limits().SetFrom(grange.GetFrom());
                    t.SetFrom(trange.GetFrom());
                    e.m_fsplice_sig.clear();
                    e.m_ident = lident;
                }
                if(ie == pe) {
                    e.m_ssplice = false;
                    e.Limits().SetTo(grange.GetTo());
                    t.SetTo(trange.GetTo());
                    e.m_ssplice_sig.clear();
                    e.m_ident = rident;
                }

                edited_exons.push_back(e);
                edited_transcript_exons.push_back(t);
            }
        }
        piece_begin = piece_end;
    }


    CGeneModel editedmodel = align;
    editedmodel.ClearExons();  // empty alignment with all atributes
    TInDels edited_indels;

    for (int piece_begin = 0; piece_begin < (int)edited_exons.size(); ++piece_begin) {
        _ASSERT( !edited_exons[piece_begin].m_fsplice );
        int piece_end = piece_begin;
        for( ; edited_exons[piece_end].m_ssplice; ++piece_end);
        _ASSERT(piece_end < (int)edited_exons.size());

        //find splices if possible
        if(!(align.Status()&CGeneModel::eUnknownOrientation)) {           
            TSignedSeqRange& elim = edited_exons[piece_begin].Limits();
            TSignedSeqRange& tlim = edited_transcript_exons[piece_begin];
            int distance_to_lgap = -1;
            TIntMap::const_iterator igap = m_genomic_gaps_len.lower_bound(elim.GetFrom());
            if(igap != m_genomic_gaps_len.begin()) {
                --igap;
                distance_to_lgap = elim.GetFrom()-(igap->first+igap->second);
            }
            if(distance_to_lgap == 0) { // ubutting gap         
                edited_exons[piece_begin].m_fsplice_sig = "NN";
            } else if(tlim.GetFrom() > BIG_NOT_ALIGNED && (piece_begin == 0 || tlim.GetFrom() > edited_transcript_exons[piece_begin-1].GetTo()+1)) {
                string splice = (align.Strand() == ePlus) ? "AG" : "AC";
                for(int p = max(0,elim.GetFrom()-2); p <= min(elim.GetFrom()+EXTRA_CUT, elim.GetTo()-MISM_PENALTY)-2; ++p) {
                    if(m_contig[p] == splice[0] && m_contig[p+1] == splice[1]) {
                        tlim.SetFrom(tlim.GetFrom()+p+2-elim.GetFrom());

                        int del_len = 0;
                        ITERATE(TInDels, indl, align.FrameShifts()) {
                            if(indl->IsDeletion() && Include(elim, indl->Loc()))
                                del_len += indl->Len();
                        }
                        double errors = (1.-edited_exons[piece_begin].m_ident)*(elim.GetLength()+del_len);
                        elim.SetFrom(p+2);
                        edited_exons[piece_begin].m_ident = 1.-errors/(elim.GetLength()+del_len);  // splices won't clip indels or mismatches     
                        if(align.Strand() == eMinus)
                            ReverseComplement(splice.begin(),splice.end());
                        edited_exons[piece_begin].m_fsplice_sig = splice;
                        _ASSERT(elim.NotEmpty());
                        
                        break;
                    }
                }
            }
        }
        if(!(align.Status()&CGeneModel::eUnknownOrientation)) {           
            TSignedSeqRange& elim = edited_exons[piece_end].Limits();
            TSignedSeqRange& tlim = edited_transcript_exons[piece_end];
            int distance_to_rgap = -1;
            TIntMap::const_iterator igap = m_genomic_gaps_len.lower_bound(elim.GetTo());
            if(igap != m_genomic_gaps_len.end())
                distance_to_rgap = igap->first-elim.GetTo()-1;
            if(distance_to_rgap == 0) { // ubutting gap      
                edited_exons[piece_end].m_ssplice_sig = "NN";
            } else if(tlen-tlim.GetTo()-1 > BIG_NOT_ALIGNED && (piece_end == (int)edited_exons.size()-1 || tlim.GetTo() < edited_transcript_exons[piece_end+1].GetFrom()-1)) {
                string splice = (align.Strand() == ePlus) ? "GT" : "CT";
                for(int p = min((int)m_contig.size()-1,elim.GetTo()+2); p >= max(elim.GetTo()-EXTRA_CUT, elim.GetFrom()+MISM_PENALTY)+2; --p) {
                    if(m_contig[p-1] == splice[0] && m_contig[p] == splice[1]) {
                        tlim.SetTo(tlim.GetTo()-elim.GetTo()+p-2);

                        int del_len = 0;
                        ITERATE(TInDels, indl, align.FrameShifts()) {
                            if(indl->IsDeletion() && Include(elim, indl->Loc()))
                                del_len += indl->Len();
                        }
                        double errors = (1.-edited_exons[piece_end].m_ident)*(elim.GetLength()+del_len);
                        elim.SetTo(p-2);
                        edited_exons[piece_end].m_ident = 1.-errors/(elim.GetLength()+del_len);  // splices won't clip indels
                        if(align.Strand() == eMinus)
                            ReverseComplement(splice.begin(),splice.end());
                        edited_exons[piece_end].m_ssplice_sig = splice;
                        _ASSERT(elim.NotEmpty());

                        break;
                    }
                }
            }
        }

        for(int ie = piece_begin; ie <= piece_end; ++ie) {
            CModelExon& e = edited_exons[ie];
            editedmodel.AddExon(e.Limits(), e.m_fsplice_sig, e.m_ssplice_sig, e.m_ident);
        }
        editedmodel.AddHole();
        ITERATE(TInDels, indl, align.FrameShifts()) {
            if(indl->Loc() > edited_exons[piece_begin].GetFrom() && indl->Loc() < edited_exons[piece_end].GetTo())
                edited_indels.push_back(*indl); 
        }

        piece_begin = piece_end;
    }
    
    if(align.Orientation() == eMinus) {
        for(int ie = 0; ie < (int)edited_transcript_exons.size(); ++ie) {
            TSignedSeqRange& te = edited_transcript_exons[ie];
            te = TSignedSeqRange(tlen-1-te.GetTo(),tlen-1-te.GetFrom());
        }
    }
    CAlignMap editedamap(editedmodel.Exons(),edited_transcript_exons, edited_indels, align.Orientation(), tlen);
    editedmodel.FrameShifts() = edited_indels;
    CAlignModel editedalign(editedmodel, editedamap);
    editedalign.SetTargetId(*align.GetTargetId());

    align = editedalign;
}

int TotalFrameShift(const TInDels& indels, int a, int b) {
    int fs = 0;
    ITERATE(TInDels, indl, indels) {
        if(indl->IsMismatch() || !indl->IntersectingWith(a, b))
            continue;
        if(indl->IsInsertion())
            fs += indl->Len();
        else
            fs -= indl->Len();
    }
    
    return fs%3;
}



int TotalFrameShift(const TInDels& indels, TSignedSeqRange range = TSignedSeqRange::GetWhole()) {

    return TotalFrameShift(indels, range.GetFrom(), range.GetTo());
}



void CAlignCollapser::FilterAlignments() {

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    m_left_end = numeric_limits<int>::max();
    int right_end = 0;

    size_t count_m_aligns = 0;
    ITERATE(Tdata, i, m_aligns) {
        ITERATE(deque<SAlignIndividual>, k, i->second) {
            m_left_end = min(m_left_end, k->m_range.GetFrom());
            right_end = max(right_end, k->m_range.GetTo());
            ++count_m_aligns;
        }
    }
    if (count_m_aligns == 0) {
        for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
            TAlignModelList::iterator i = it++;
            CAlignModel& align = *i;
            if(align.Type()&CGeneModel::eNotForChaining)
                m_aligns_for_filtering_only.erase(i);
        }

        return;
    }

    ITERATE(TAlignIntrons, it, m_align_introns) {
        const SIntron& intron = it->first;
        int a = intron.m_range.GetFrom();
        int b = intron.m_range.GetTo();
        m_left_end = min(m_left_end, a);
        right_end = max(right_end, b);
    }

    ITERATE(TAlignModelList, i, m_aligns_for_filtering_only) {
        m_left_end = min(m_left_end,i->Limits().GetFrom());
        right_end = max(right_end,i->Limits().GetTo());
    }

    int len = right_end-m_left_end+1;

    cerr << "Before filtering: " << m_align_introns.size() << " introns, " << m_count << " alignments" << endl;



#define COVERAGE_WINDOW 20    
    //coverage calculation
    m_coverage.resize(len,0.);
    ITERATE(Tdata, i, m_aligns) {
        ITERATE(deque<SAlignIndividual>, k, i->second) {
            if(i->first.isSR()) {
                float weight = k->m_weight;
                TSignedSeqRange range = k->m_range;
                for(int l = range.GetFrom(); l <= range.GetTo(); ++l)  // add coverage for all alignmnet range 
                    m_coverage[l-m_left_end] += weight;
                ITERATE(CAlignCommon::Tintrons, in, i->first.GetIntrons()) {   // substract intron ranges
                    for(int l = in->m_range.GetFrom()+1; l <= in->m_range.GetTo()-1; ++l) 
                        m_coverage[l-m_left_end] -= weight;
                }
            }
        }
    }
    vector<double> left_coverage(len,0.);   // average from the left side (including point)
    double wsum = 0;
    for(int i = 0; i < len; ++i) {
        wsum += m_coverage[i];
        int ipast = i - COVERAGE_WINDOW;
        if(ipast >= 0)
            wsum -= m_coverage[ipast];
        left_coverage[i] = wsum/COVERAGE_WINDOW;
    }
    vector<double> right_coverage(len,0.);   // average from the right side (including point)
    wsum = 0;
    for(int i = len-1; i >= 0; --i) {
        wsum += m_coverage[i];
        int ipast = i + COVERAGE_WINDOW;
        if(ipast < len)
            wsum -= m_coverage[ipast];
        right_coverage[i] = wsum/COVERAGE_WINDOW;
    }

    //initial intron filtering
    int minconsensussupport = args["min-consensus-support"].AsInteger();
    int minnonconsensussupport = args["min-non-consensussupport"].AsInteger();
    double minident = args["high-identity"].AsDouble();
    int minest = args["minest"].AsInteger();
    for(TAlignIntrons::iterator it = m_align_introns.begin(); it != m_align_introns.end(); ) {
        TAlignIntrons::iterator intron = it++;
        bool bad_intron = false;
        SIntronData& id = intron->second;

        if(id.m_selfsp_support) {
            if(id.m_est_support >= minest)
                id.m_keep_anyway = true;
        } else {
            bad_intron = true;
        }

        if(id.m_keep_anyway)
            continue;

        if(intron->first.m_sig == "CTAC" && intron->first.m_oriented)
            bad_intron = true;

        if(intron->first.m_sig == "GTAG") {
            if(id.m_weight < minconsensussupport)
                bad_intron = true;
        } else {
            if(id.m_weight < minnonconsensussupport)
                bad_intron = true;
        }

        if(id.m_ident < minident)
            bad_intron = true;

        if(bad_intron)
            m_align_introns.erase(intron);
    }

    //filter low expressed splices
    double minspliceexpression = args["min-support-fraction"].AsDouble();
    double minintronexpression = args["end-pair-support-cutoff"].AsDouble();

    typedef multimap<int,CAlignCollapser::TAlignIntrons::iterator> TIntronsBySplice;
    TIntronsBySplice introns_by_left_splice;
    NON_CONST_ITERATE(TAlignIntrons, intron, m_align_introns) {
        introns_by_left_splice.insert(TIntronsBySplice::value_type(intron->first.m_range.GetFrom(),intron));
    }
    for(TIntronsBySplice::iterator a = introns_by_left_splice.begin(); a != introns_by_left_splice.end(); ) {
        int splice = a->first;
        TIntronsBySplice::iterator b = introns_by_left_splice.upper_bound(splice);  // first with different splice

        double weight = 0;
        int number = 0;
        for(TIntronsBySplice::iterator i = a; i != b; ++i) {
            ++number;
            weight += i->second->second.m_weight;
        }
        double mean = weight/number;

        for(TIntronsBySplice::iterator it = a; it != b; ) {
            TIntronsBySplice::iterator i = it++;
            SIntronData& id = i->second->second;
            if(!id.m_keep_anyway && (id.m_weight < minintronexpression*mean || weight < minspliceexpression*left_coverage[splice-m_left_end])) {
                id.m_weight = -1;
                introns_by_left_splice.erase(i);
            }
        }

        a = b;
    }

    TIntronsBySplice introns_by_right_splice;
    NON_CONST_ITERATE(TAlignIntrons, intron, m_align_introns) {
        introns_by_right_splice.insert(TIntronsBySplice::value_type(intron->first.m_range.GetTo(),intron));
    }
    for(TIntronsBySplice::iterator a = introns_by_right_splice.begin(); a != introns_by_right_splice.end(); ) {
        int splice = a->first;
        TIntronsBySplice::iterator b = introns_by_right_splice.upper_bound(splice);  // first with different splice

        double weight = 0;
        int number = 0;
        for(TIntronsBySplice::iterator i = a; i != b; ++i) {
            ++number;
            weight += i->second->second.m_weight;
        }
        double mean = weight/number;

        for(TIntronsBySplice::iterator it = a; it != b; ) {
            TIntronsBySplice::iterator i = it++;
            SIntronData& id = i->second->second;
            if(!id.m_keep_anyway && (id.m_weight < minintronexpression*mean || weight < minspliceexpression*right_coverage[splice-m_left_end])) {
                id.m_weight = -1;
                introns_by_right_splice.erase(i);
            }
        }

        a = b;
    }
    
    for(TAlignIntrons::iterator it = m_align_introns.begin(); it != m_align_introns.end(); ) {
        TAlignIntrons::iterator intron = it++;
        if(intron->second.m_weight < 0)
            m_align_introns.erase(intron);
    }
    
    //remove/clip alignmnets with bad introns
    for(Tdata::iterator it = m_aligns.begin(); it != m_aligns.end(); ) {
        Tdata::iterator data = it++;
        const CAlignCommon& alc = data->first;

        if((alc.isEST() && !m_filterest) || (alc.isSR() && !m_filtersr))
            continue;

        CAlignCommon::Tintrons introns = alc.GetIntrons();
        if(introns.empty())
            continue;

        //remove flanking bad introns
        int new_right = right_end;
        while(!introns.empty() && m_align_introns.find(introns.back()) == m_align_introns.end()) {
            new_right = introns.back().m_range.GetFrom();
            introns.pop_back();
        }
        int new_left = m_left_end;
        while(!introns.empty() && m_align_introns.find(introns.front()) == m_align_introns.end()) {
            new_left = introns.front().m_range.GetTo();
            introns.erase(introns.begin());
        }

        bool all_good = true;
        for(int i = 0; all_good && i < (int)introns.size(); ++i) {
            all_good = (m_align_introns.find(introns[i]) != m_align_introns.end());
        }

        if(all_good && introns.size() == alc.GetIntrons().size())  // all initial introns good
            continue;

        if(all_good && introns.size() > 0) { // clipped some flanked introns but not all
            const deque<char>& id_pool = m_target_id_pool[alc];
            ITERATE(deque<SAlignIndividual>, i, data->second) {
                CAlignModel align(alc.GetAlignment(*i, id_pool));
                align.Clip(TSignedSeqRange(new_left,new_right),CGeneModel::eRemoveExons);
                if(alc.isEST())
                    align.Status() |= CGeneModel::eChangedByFilter;  
                _ASSERT(align.Limits().NotEmpty());
                _ASSERT(align.Exons().size() == introns.size()+1);
                CAlignCommon c(align);
                m_aligns[c].push_back(SAlignIndividual(align, m_target_id_pool[c]));
            }
        }

        // delete initial alignments and ids
        m_target_id_pool.erase(data->first);
        m_aligns.erase(data);
    }
  
    //splices which should not be crossed
    double mincrossexpression = args["sharp-boundary"].AsDouble();
    TIVec left_plus(len,right_end);       // closest left + strand splice 'on the right' from the current position
    TIVec left_minus(len,right_end);      // closest left - strand splice 'on the right' from the current position
    TIVec right_plus(len,m_left_end);       // closest right + strand splice 'on the left' from the current position
    TIVec right_minus(len,m_left_end);      // closest right - strand splice 'on the left' from the current position
    ITERATE(TAlignIntrons, it, m_align_introns) {
        const SIntron& intron = it->first;
        int a = intron.m_range.GetFrom();
        int b = intron.m_range.GetTo();

        double two_side_exon_coverage = max(left_coverage[a-m_left_end],right_coverage[b-m_left_end]);

        //        if(right_coverage[a+1-m_left_end] < mincrossexpression*left_coverage[a-m_left_end]) {
        if(right_coverage[a+1-m_left_end] < mincrossexpression*two_side_exon_coverage) {
            if(!intron.m_oriented || intron.m_strand == ePlus)
                left_plus[a-m_left_end] = a;
            if(!intron.m_oriented || intron.m_strand == eMinus)
                left_minus[a-m_left_end] = a;
        }

        //        if(left_coverage[b-1-m_left_end] < mincrossexpression*right_coverage[b-m_left_end]) {
        if(left_coverage[b-1-m_left_end] < mincrossexpression*two_side_exon_coverage) {
            if(!intron.m_oriented || intron.m_strand == ePlus)
                right_plus[b-m_left_end] = b;
            if(!intron.m_oriented || intron.m_strand == eMinus)
                right_minus[b-m_left_end] = b;
        }
    }

    for(int i = 1; i < len; ++i) {
        right_plus[i] = max(right_plus[i],right_plus[i-1]);
        right_minus[i] = max(right_minus[i],right_minus[i-1]);
    }
    for(int i = len-2; i >= 0; --i) {
        left_plus[i] = min(left_plus[i],left_plus[i+1]);
        left_minus[i] = min(left_minus[i],left_minus[i+1]);
    }

    //filter/cut low abandance one-exon and crossing splices
    int minsingleexpression = args["min-edge-coverage"].AsInteger();
    int trim = args["trim"].AsInteger();
    int total = 0;
    for(Tdata::iterator it = m_aligns.begin(); it != m_aligns.end(); ) {
        Tdata::iterator data = it++;
        const CAlignCommon& alc = data->first;
        deque<SAlignIndividual>& aligns = data->second;

        if((alc.isEST() && m_filterest) || (alc.isSR() && m_filtersr)) {
            if(alc.GetIntrons().empty()) { // not spliced
                NON_CONST_ITERATE(deque<SAlignIndividual>, i, aligns) {
                    int a = i->m_range.GetFrom()+trim;
                    int b = i->m_range.GetTo()-trim;
                    if(b > a) {
                        if((m_coverage[a-m_left_end] < minsingleexpression || m_coverage[b-m_left_end] < minsingleexpression) && !alc.isPolyA() && !alc.isCap())
                            i->m_weight = -1;
                        else if((alc.isUnknown() || alc.isPlus()) && ((right_plus[b-m_left_end] > a && !alc.isCap()) || (left_plus[a-m_left_end] < b && !alc.isPolyA())))
                            i->m_weight = -1;
                        else if((alc.isUnknown() || alc.isMinus()) && ((right_minus[b-m_left_end] > a && !alc.isPolyA()) || (left_minus[a-m_left_end] < b && !alc.isCap())))
                            i->m_weight = -1;
                    } else {
                        i->m_weight = -1;
                    }
                }
            } else {    // spliced  
                const deque<char>& id_pool = m_target_id_pool[alc];
                NON_CONST_ITERATE(deque<SAlignIndividual>, i, aligns) {
                    CAlignModel align(alc.GetAlignment(*i, id_pool));
                    TSignedSeqRange new_lim = align.Limits();
                    if(align.Exons().front().Limits().GetLength() > trim) {
                        int a = align.Exons().front().Limits().GetFrom()+trim;
                        int b = align.Exons().front().Limits().GetTo();
                        if((alc.isUnknown() || alc.isPlus()) && (right_plus[b-m_left_end] > a && !alc.isCap())) // crosses right plus splice      
                            new_lim.SetFrom(right_plus[b-m_left_end]);
                        if((alc.isUnknown() || alc.isMinus()) && (right_minus[b-m_left_end] > a && !alc.isPolyA())) // crosses right minus splice 
                            new_lim.SetFrom(right_minus[b-m_left_end]);
                        _ASSERT(new_lim.GetFrom() <= align.Exons().front().GetTo());
                    }
                    if(align.Exons().back().Limits().GetLength() > trim) {
                        int a = align.Exons().back().Limits().GetFrom();
                        int b = align.Exons().back().Limits().GetTo()-trim;
                        if((alc.isUnknown() || alc.isPlus()) && (left_plus[a-m_left_end] < b && !alc.isPolyA())) // crosses left plus splice  
                            new_lim.SetTo(left_plus[a-m_left_end]);
                        if((alc.isUnknown() || alc.isMinus()) && (left_minus[a-m_left_end] < b && !alc.isCap())) // crosses left minus splice 
                            new_lim.SetTo(left_minus[a-m_left_end]);
                        _ASSERT(new_lim.GetTo() >= align.Exons().back().GetFrom());
                    }
                    i->m_range = new_lim;

                    //delete if retained intron in internal exon    
                    for(int n = 1; n < (int)align.Exons().size()-1 && i->m_weight > 0; ++n) {
                        int a = align.Exons()[n].Limits().GetFrom();
                        int b = align.Exons()[n].Limits().GetTo();

                        pair<TIntronsBySplice::iterator,TIntronsBySplice::iterator> eqr(introns_by_right_splice.end(),introns_by_right_splice.end());
                        if((alc.isUnknown() || alc.isPlus()) && right_plus[b-m_left_end] > a)         // crosses right plus splice    
                            eqr = introns_by_right_splice.equal_range(right_plus[b-m_left_end]);
                        else if((alc.isUnknown() || alc.isMinus()) && right_minus[b-m_left_end] > a)  // crosses right minus splice   
                            eqr = introns_by_right_splice.equal_range(right_minus[b-m_left_end]);
                        for(TIntronsBySplice::iterator ip = eqr.first; ip != eqr.second; ++ip) {
                            if(ip->second->first.m_range.GetFrom() > a)
                                i->m_weight = -1;
                        }
                    }
                }
            }
        
            aligns.erase(remove_if(aligns.begin(),aligns.end(),AlignmentMarkedForDeletion),aligns.end());
        }

        total += aligns.size();
        if(aligns.empty())
            m_aligns.erase(data);
    }


    //filter other alignments

    //filter introns
    double clip_threshold = args["utrclipthreshold"].AsDouble();
    for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
        TAlignModelList::iterator i = it++;
        CAlignModel& align = *i;

        if(align.Type()&CAlignModel::eProt) {
            CAlignModel a = align;
            a.Status() |= CGeneModel::eUnmodifiedAlign;
            m_aligns_for_filtering_only.push_front(a);
        }

        int intronnum = 0;
        ITERATE(CGeneModel::TExons, e, align.Exons()) {
            if(e->m_fsplice)
                ++intronnum;
        }
            
        if((align.Type()&CGeneModel::eEST) && !m_filterest)
            continue;
        if((align.Type()&CGeneModel::emRNA) && !m_filtermrna)
            continue;
        if((align.Type()&CGeneModel::eProt) && !m_filterprots)
            continue;

        if(!AlignmentIsSupportedBySR(align, m_coverage, minsingleexpression, m_left_end)) {
            if(align.Type()&(CGeneModel::emRNA|CGeneModel::eProt)) {
                continue;
            } else if(align.Type()&CGeneModel::eNotForChaining) {
                m_aligns_for_filtering_only.erase(i);
                continue;
            }
        }

        bool good_alignment = true;

        //clip alignmnets with bad introns
        if(align.Type()&CAlignModel::eProt) {
            good_alignment = RemoveNotSupportedIntronsFromProt(align);
        } else if(align.Type()&CGeneModel::eNotForChaining) {
            good_alignment = RemoveNotSupportedIntronsFromTranscript(align, true);
        } else {
            CAlignModel reversed = align;
            good_alignment = RemoveNotSupportedIntronsFromTranscript(align, false);
            if(align.Status()&CGeneModel::eUnknownOrientation) {
                reversed.ReverseComplementModel();
                bool good_reversed_alignment = RemoveNotSupportedIntronsFromTranscript(reversed, false);
                if(reversed.Exons().size() > align.Exons().size()) {
                    align = reversed;
                    good_alignment = good_reversed_alignment;
                }
            }
        }

        if(!align.Exons().empty())
            ClipNotSupportedFlanks(align, clip_threshold);

        if(align.Exons().empty() || (!good_alignment && !(align.Type()&CGeneModel::eNotForChaining)) || !AlignmentIsSupportedBySR(align, m_coverage, minsingleexpression, m_left_end)) {
            m_aligns_for_filtering_only.erase(i);
            continue;
        }

        ITERATE(CGeneModel::TExons, e, align.Exons()) {
            if(e->m_fsplice)
                --intronnum;
        }

        if(intronnum > 0 && !(align.Type()&CGeneModel::eNotForChaining))
            align.Status() |= CGeneModel::eChangedByFilter;  
    }

    TIVec self_coverage(len,0);

    //modify contig near correction indels which will ensure their clipping near self species cDNA edges (as mismatches)
    ITERATE(TInDels, indl, m_correction_data.m_correction_indels) {
        if(indl->IsDeletion()) {
            m_contig[indl->Loc()] = tolower(m_contig[indl->Loc()]);
            m_contig[indl->Loc()-1] = tolower(m_contig[indl->Loc()]-1);
        } else {
            for(int p = indl->Loc(); p < indl->Loc()+indl->Len(); ++p)
                m_contig[p] = tolower(m_contig[p]);
        }
    }
    

    //clean self species cDNA edges and calculate self coverage
    for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
        TAlignModelList::iterator i = it++;
        CAlignModel& align = *i;

        if(align.Status()&CGeneModel::eGapFiller) {
            string transcript = GetDNASequence(align.GetTargetId(),*m_scope);

            CleanSelfTranscript(align, transcript);

            ITERATE(CGeneModel::TExons, ie, align.Exons()) {
                int a = max(m_left_end, ie->GetFrom()); // TSA could be slightly extended in the area without alignments
                int b = min(right_end, ie->GetTo());
                for(int p = a; p <= b; ++p) {
                    ++self_coverage[p-m_left_end];
                }
            }
        }
    } 

    //restore contig
    NON_CONST_ITERATE(string, ip, m_contig)
        *ip = toupper(*ip);

    typedef pair<TSignedSeqRange,TInDels> TGapEnd;
    set<TGapEnd> right_gends;   //rightmost exon befor gap
    set<TGapEnd> left_gends;    // leftmost exon before gap

#define MIN_EXON 10
#define DESIRED_CHUNK 100      
    //cut NotForChaining and fill gaps
    for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
        TAlignModelList::iterator i = it++;
        CAlignModel& align = *i;

        if(!(align.Status()&CGeneModel::eGapFiller)) {
            if(align.Type()&CGeneModel::eNotForChaining)
                m_aligns_for_filtering_only.erase(i);
            continue;
        } 

        //collect fshifts
        ITERATE(CGeneModel::TExons, ie, align.Exons()) {
            TInDels fs = align.GetInDels(ie->GetFrom(), ie->GetTo(), true);
            left_gends.insert(TGapEnd(ie->Limits(),fs));
            right_gends.insert(TGapEnd(ie->Limits(),fs));
        }
        
        if(!(align.Type()&CGeneModel::eNotForChaining)) {
            CAlignModel editedalign = FillGapsInAlignmentAndAddToGenomicGaps(align, efill_left|efill_right|efill_middle);
            if(editedalign.Exons().size() > align.Exons().size()) {
                m_aligns_for_filtering_only.push_front(editedalign);
                m_aligns_for_filtering_only.erase(i);
            }
        } else {
            align.Status() &= ~CGeneModel::ePolyA;
            align.Status() &= ~CGeneModel::eCap;
            if(align.Exons().front().Limits().GetLength() > MIN_EXON) {
                CAlignModel a = align;

                TSignedSeqRange l = a.Exons().front().Limits();
                int len = l.GetLength();
                if(!align.Exons().front().m_ssplice && len > DESIRED_CHUNK) {
                    l.SetTo(l.GetFrom()+DESIRED_CHUNK-1);
                    len = DESIRED_CHUNK;
                }
                for(int ie = 0; len < DESIRED_CHUNK-2*MIN_EXON && a.Exons()[ie].m_ssplice; ++ie) {
                    if(a.Exons()[ie+1].m_ssplice) {
                        l.SetTo(a.Exons()[ie+1].GetTo());
                        len += a.Exons()[ie+1].Limits().GetLength();
                    } else {
                        l.SetTo(min(a.Exons()[ie+1].GetTo(),a.Exons()[ie+1].GetFrom()+DESIRED_CHUNK-len-1));
                    }
                }
                if(l.NotEmpty())
                    l = a.GetAlignMap().ShrinkToRealPoints(l, false);
                if(l.NotEmpty()) {
                    a.Clip(l, CGeneModel::eRemoveExons);
                    CAlignModel editedalign = FillGapsInAlignmentAndAddToGenomicGaps(a, efill_left);
                    if(editedalign.Exons().size() > a.Exons().size()) {
                        m_aligns_for_filtering_only.push_front(editedalign);
                    }
                }
            }

            for(int ie = 0; ie < (int)align.Exons().size()-1; ++ie) {
                if((!align.Exons()[ie].m_ssplice || !align.Exons()[ie+1].m_fsplice) && 
                   align.Exons()[ie].Limits().GetLength() > MIN_EXON && align.Exons()[ie+1].Limits().GetLength() > MIN_EXON) {
                    CAlignModel a = align;

                    int left = a.Exons()[ie].GetFrom();
                    int len = a.Exons()[ie].Limits().GetLength();
                    if(!a.Exons()[ie].m_fsplice && len > DESIRED_CHUNK) {
                        left = a.Exons()[ie].GetTo()-DESIRED_CHUNK+1;
                        len = DESIRED_CHUNK;
                    }
                    for(int iie = ie; len < DESIRED_CHUNK-2*MIN_EXON && a.Exons()[iie].m_fsplice; --iie) {
                        if(a.Exons()[iie-1].m_fsplice) {
                            left = a.Exons()[iie-1].GetFrom();
                            len += a.Exons()[iie-1].Limits().GetLength();
                        } else {
                            left = max(a.Exons()[iie-1].GetFrom(),a.Exons()[iie-1].GetTo()-DESIRED_CHUNK+len+1);
                        }
                    }
                    int right = a.Exons()[ie+1].GetTo();
                    len = a.Exons()[ie+1].Limits().GetLength();
                    if(!a.Exons()[ie+1].m_ssplice && len > DESIRED_CHUNK) {
                        right = a.Exons()[ie+1].GetFrom()+DESIRED_CHUNK-1;
                        len = DESIRED_CHUNK;
                    }
                    for(int iie = ie+1; len < DESIRED_CHUNK-2*MIN_EXON && a.Exons()[iie].m_ssplice; ++iie) {
                        if(a.Exons()[iie+1].m_ssplice) {
                            right = a.Exons()[iie+1].GetTo();
                            len += a.Exons()[iie+1].Limits().GetLength();
                        } else {
                            right = min(a.Exons()[iie+1].GetTo(),a.Exons()[iie+1].GetFrom()+DESIRED_CHUNK-len-1);
                        }
                    }                    
                    if(left >= 0 && right >= 0) {
                        TSignedSeqRange l(left, right);
                        l = a.GetAlignMap().ShrinkToRealPoints(l, false);
                        if(l.NotEmpty()) {
                            a.Clip(l, CGeneModel::eRemoveExons);
                            CAlignModel editedalign = FillGapsInAlignmentAndAddToGenomicGaps(a, efill_middle);
                            if(editedalign.Exons().size() >  a.Exons().size()) {
                                m_aligns_for_filtering_only.push_front(editedalign);
                            }
                        }
                    }
                }
           }

            if(align.Exons().back().Limits().GetLength() > MIN_EXON) {
                CAlignModel a = align;

                TSignedSeqRange l = a.Exons().back().Limits();
                int len = l.GetLength();
                if(!align.Exons().back().m_fsplice && len > DESIRED_CHUNK) {
                    l.SetFrom(a.Exons().back().GetTo()-DESIRED_CHUNK+1);
                    len = DESIRED_CHUNK;          
                }
                for(int ie = (int)a.Exons().size()-1; len < DESIRED_CHUNK-2*MIN_EXON && a.Exons()[ie].m_fsplice; --ie) {
                    if(a.Exons()[ie-1].m_fsplice) {
                        l.SetFrom(a.Exons()[ie-1].GetFrom());
                        len += a.Exons()[ie-1].Limits().GetLength();
                    } else {
                        l.SetFrom(max(a.Exons()[ie-1].GetFrom(),a.Exons()[ie-1].GetTo()-DESIRED_CHUNK+len+1));
                    }
                }
                if(l.NotEmpty())
                    l = a.GetAlignMap().ShrinkToRealPoints(l, false);
                if(l.NotEmpty()) {
                    a.Clip(l, CGeneModel::eRemoveExons);
                    CAlignModel editedalign = FillGapsInAlignmentAndAddToGenomicGaps(a, efill_right);
                    if(editedalign.Exons().size() > a.Exons().size()) {
                        m_aligns_for_filtering_only.push_front(editedalign);
                    }
                }
            }

            m_aligns_for_filtering_only.erase(i);
        }
    }

    enum EnpPoint { eRightPlus = 1, eRightMinus = 2, eLeftPlus = 4, eLeftMinus = 8};
    vector<unsigned char> end_status(len, 0);

    //include gap's boundaries in no cross splices
    ITERATE(TAlignModelList, i, m_aligns_for_filtering_only) {
        const CAlignModel& align = *i;
        for(int ie = 0; ie < (int)align.Exons().size(); ++ie) {
            if(align.Exons()[ie].Limits().Empty()) {
                if(ie > 0) {
                    int a = align.Exons()[ie-1].GetTo();
                    int al = a-m_left_end;
                    //                    if(a < right_end && self_coverage[al+1] == 0) {   // TSA could be slightly extended in the area without alignments; include only at drop
                    if(a < right_end) {
                        if((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) {
                            left_plus[al] = a;
                            end_status[al] |= eLeftPlus;
                        }
                        if((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand()== eMinus) {
                            left_minus[al] = a;
                            end_status[al] |= eLeftMinus;
                        }
                    }
                }
                if(ie < (int)align.Exons().size()-1) {
                    int b = align.Exons()[ie+1].GetFrom();
                    int bl = b-m_left_end;
                    //                    if(b > m_left_end && self_coverage[bl-1] == 0) {   // TSA could be slightly extended in the area without alignments; include only at drop
                    if(b > m_left_end) {
                        if((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) {
                            right_plus[bl] = b;
                            end_status[bl] |= eRightPlus;
                        }
                        if((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand()== eMinus) {
                            right_minus[bl] = b;
                            end_status[bl] |= eRightMinus;
                        }
                    }
                }
            }
        }
    }

    for(int i = 1; i < len; ++i) {
        right_plus[i] = max(right_plus[i],right_plus[i-1]);
        right_minus[i] = max(right_minus[i],right_minus[i-1]);
    }
    for(int i = len-2; i >= 0; --i) {
        left_plus[i] = min(left_plus[i],left_plus[i+1]);
        left_minus[i] = min(left_minus[i],left_minus[i+1]);
    }


#define FS_FUZZ 10
#define MAX_CLIP 200
#define SMALL_CLIP 30

    //trim 3'/5' exons crossing splices (including hole boundaries)
    for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
        TAlignModelList::iterator i = it++;
        CAlignModel& align = *i;

        if(align.Status()&CGeneModel::eUnmodifiedAlign)
            continue;

        CAlignMap amap = align.GetAlignMap();

        if((align.Type()&CGeneModel::eEST) && !m_filterest)
            continue;
        if((align.Type()&CGeneModel::emRNA) && !m_filtermrna)
            continue;
        if((align.Type()&CGeneModel::eProt) && !m_filterprots)
            continue;

        bool snap_to_codons = align.Type()&CAlignModel::eProt; 
        bool good_alignment = true;

        bool keepdoing = true;
        while(keepdoing && good_alignment) {
            keepdoing = false;
            for(int ie = 0; ie < (int)align.Exons().size(); ++ie) {
                const CModelExon& e = align.Exons()[ie];
 
                if(!e.m_fsplice && e.Limits().GetLength() > trim && e.GetTo() <= right_end && 
                   (ie != 0 || (align.Strand() == ePlus && !(align.Status()&CGeneModel::eCap) && !align.HasStart()) || (align.Strand() == eMinus && !(align.Status()&CGeneModel::ePolyA) && !align.HasStop()))) {
                    int l = e.GetFrom();
                    int r = e.GetTo();
                    int new_l = l;

                    TIVec* rights = 0;
                    EnpPoint endp;
                    if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) && right_plus[r-m_left_end] > l+trim) { // crosses right plus splice            
                        new_l = right_plus[r-m_left_end];
                        rights = &right_plus;
                        endp = eRightPlus;
                    }
                    if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == eMinus) && right_minus[r-m_left_end] > l+trim) { // crosses right minus splice         
                        new_l = right_minus[r-m_left_end];
                        rights = &right_minus;
                        endp = eRightMinus;
                    }

                    if(new_l != l && (end_status[new_l-m_left_end]&endp) && (align.Type()&CAlignModel::eProt)) {
                        // try to extend
                        while(new_l-l > MAX_CLIP  && (end_status[new_l-m_left_end]&endp))
                            new_l = max(l,(*rights)[new_l-1-m_left_end]);
                        TInDels pindels = align.GetInDels(true);
                        int firstclip = new_l;
                        for(int putativel = new_l; (new_l > l+SMALL_CLIP || TotalFrameShift(pindels, l, new_l)) && (end_status[new_l-m_left_end]&endp) && new_l == putativel; ) {
                            putativel = max(l,(*rights)[new_l-1-m_left_end]);
                            for(set<TGapEnd>::iterator ig = left_gends.begin(); ig != left_gends.end(); ++ig) {
                                if(ig->first.GetFrom() <= putativel && ig->first.GetTo() >= firstclip) {
                                    int prot_fs = TotalFrameShift(pindels, putativel, firstclip+FS_FUZZ);
                                    int tsa_fs = TotalFrameShift(ig->second, putativel-FS_FUZZ, firstclip);
                                    if(prot_fs == tsa_fs)
                                        new_l = putativel;
                                }
                            }
                        }
                        //check if undertrimmed
                        if(end_status[new_l-m_left_end]&endp) {
                            for(int i = 0; i < (int)pindels.size() && pindels[i].Loc() <= new_l+FS_FUZZ; ++i) 
                                new_l = max(new_l,pindels[i].Loc());
                        }
                    }

                    if(new_l != l) {
                        _ASSERT(new_l <= r);
                        if((align.Type()&CGeneModel::eEST) && (int)align.Exons().size() == 1) {
                            good_alignment = false;
                            break;
                        }
                        
                        TSignedSeqRange seg = amap.ShrinkToRealPoints(TSignedSeqRange(new_l,align.Limits().GetTo()),snap_to_codons);
                        if(seg.Empty() || amap.FShiftedLen(seg,false) < END_PART_LENGTH) { // nothing left on right  
                            if(ie == 0 || amap.FShiftedLen(TSignedSeqRange(align.Limits().GetFrom(),align.Exons()[ie-1].GetTo())) < END_PART_LENGTH) { // no alignmnet left  
                                good_alignment = false;
                            } else {      // left side is kept  
                                align.Clip(TSignedSeqRange(align.Limits().GetFrom(),align.Exons()[ie-1].GetTo()),CGeneModel::eRemoveExons);
                            }
                        } else { // trim    
                            if(ie == 0) { // first exon 
                                if(align.Type()&CGeneModel::eProt) 
                                    align.Clip(seg,CGeneModel::eRemoveExons); 
                                else
                                    align.CutExons(TSignedSeqRange(align.Limits().GetFrom(),seg.GetFrom()-1));   // Clip() is not friendly to gapfillers    
                            } else {
                                align.CutExons(TSignedSeqRange(align.Exons()[ie-1].GetTo()+1,seg.GetFrom()-1));
                            }
                        }
                        keepdoing = true;
                        break;
                    }
                }

                if(!e.m_ssplice && e.Limits().GetLength() > trim && e.GetFrom() >= m_left_end &&
                   (ie != (int)align.Exons().size()-1 || (align.Strand() == ePlus && !(align.Status()&CGeneModel::ePolyA) && !align.HasStop()) || (align.Strand() == eMinus && !(align.Status()&CGeneModel::eCap) && !align.HasStart()))) {
                    int l = e.GetFrom();
                    int r = e.GetTo();
                    int new_r = r;

                    TIVec* lefts = 0;
                    EnpPoint endp;
                    if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) && left_plus[l-m_left_end] < r-trim) { // crosses left plus splice              
                        new_r = left_plus[l-m_left_end];
                        lefts = &left_plus;
                        endp = eLeftPlus;
                    }
                    if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == eMinus) && left_minus[l-m_left_end] < r-trim) { // crosses left minus splice               
                        new_r = left_minus[l-m_left_end];
                        lefts = &left_minus;
                        endp = eLeftMinus;
                    }

                    if(new_r != r && (end_status[new_r-m_left_end]&endp) && (align.Type()&CAlignModel::eProt)) {
                        // try to extend
                        while(r-new_r >  MAX_CLIP  && (end_status[new_r-m_left_end]&endp))
                            new_r = min(r,(*lefts)[new_r+1-m_left_end]);
                        TInDels pindels = align.GetInDels(true);
                        int firstclip = new_r;
                        for(int putativer = new_r; (new_r < r-SMALL_CLIP || TotalFrameShift(pindels, new_r, r))   && (end_status[new_r-m_left_end]&endp) && new_r == putativer; ) {
                            putativer = min(r,(*lefts)[new_r+1-m_left_end]);
                            for(set<TGapEnd>::iterator ig = right_gends.begin(); ig != right_gends.end(); ++ig) {
                                if(ig->first.GetFrom() <= firstclip && ig->first.GetTo() >= putativer) {
                                    int prot_fs = TotalFrameShift(pindels, firstclip-FS_FUZZ, putativer);
                                    int tsa_fs = TotalFrameShift(ig->second, firstclip, putativer+FS_FUZZ);
                                    if(prot_fs == tsa_fs)
                                        new_r = putativer;
                                }
                            }
                        }
                        //check if undertrimmed
                        if(end_status[new_r-m_left_end]&endp) {
                            for(int i = pindels.size()-1; i >= 0 && pindels[i].Loc() >= new_r-FS_FUZZ; --i) 
                                new_r = min(new_r,pindels[i].Loc()-1);
                        }
                    }

                    if(new_r != r) {
                        _ASSERT(new_r >= l);
                        if((align.Type()&CGeneModel::eEST) && (int)align.Exons().size() == 1) {
                            good_alignment = false;
                            break;
                        }
                        
                        TSignedSeqRange seg = amap.ShrinkToRealPoints(TSignedSeqRange(align.Limits().GetFrom(),new_r),snap_to_codons);
                        if(seg.Empty() || amap.FShiftedLen(seg,false) < END_PART_LENGTH) { // nothing left on left    
                            if(ie == (int)align.Exons().size()-1 || amap.FShiftedLen(TSignedSeqRange(align.Exons()[ie+1].GetFrom(),align.Limits().GetTo())) < END_PART_LENGTH) { // no alignmnet left 
                                good_alignment = false;
                            } else {      // right side is kept  
                                align.Clip(TSignedSeqRange(align.Exons()[ie+1].GetFrom(),align.Limits().GetTo()),CGeneModel::eRemoveExons);
                            }
                        } else { // trim    
                            if(ie == (int)align.Exons().size()-1) { // last exon 
                                if(align.Type()&CGeneModel::eProt)
                                    align.Clip(seg,CGeneModel::eRemoveExons); 
                                else
                                    align.CutExons(TSignedSeqRange(seg.GetTo()+1, align.Limits().GetTo()));   // Clip() is not friendly to gapfillers 
                            } else {
                                align.CutExons(TSignedSeqRange(seg.GetTo()+1,align.Exons()[ie+1].GetFrom()-1));
                            }
                        }
                        keepdoing = true;
                        break;
                    }
                }               
            }
        }

        if(!good_alignment)
            m_aligns_for_filtering_only.erase(i);
    }

    //clean genomic gaps
    sort(m_correction_data.m_correction_indels.begin(),m_correction_data.m_correction_indels.end(),GenomicGapsOrder());                            // accsession is used if the sequence is same
    m_correction_data.m_correction_indels.erase( unique(m_correction_data.m_correction_indels.begin(),m_correction_data.m_correction_indels.end()), m_correction_data.m_correction_indels.end() );   // uses == for CInDelInfo which ignores accession

    total += m_aligns_for_filtering_only.size();
    

    cerr << "After filtering: " << m_align_introns.size() << " introns, " << total << " alignments" << endl;
}

bool CAlignCollapser::CheckAndInsert(const CAlignModel& align, TAlignModelClusterSet& clsset) const {

    ITERATE(CGeneModel::TExons, i, align.Exons()) {
        if(i->Limits().NotEmpty()) {
            CInDelInfo p(i->GetFrom(), 1, CInDelInfo::eDel);
            TInDels::const_iterator ig = lower_bound(m_correction_data.m_correction_indels.begin(), m_correction_data.m_correction_indels.end(), p); // first equal or greater
            for( ; ig != m_correction_data.m_correction_indels.end() && ig->Loc() <= i->GetTo(); ++ig) {
                if(ig->GetSource().m_range.NotEmpty())      // exon overlaps with inserted gap
                    return false;
            }
        }
    }
   
    clsset.Insert(align);
    return true;
}

struct LeftAndLongFirstOrderForAligns
{
    bool operator() (TAlignModelList::const_iterator a, TAlignModelList::const_iterator b) {  // left and long first
        if(a->Limits() == b->Limits()) {
            if(a->Ident() != b->Ident())
                return a->Ident() > b->Ident();
            else
                return a->TargetAccession() < b->TargetAccession();
        } else if(a->Limits().GetFrom() !=  b->Limits().GetFrom()) {
            return a->Limits().GetFrom() <  b->Limits().GetFrom();
        } else {
            return a->Limits().GetTo() >  b->Limits().GetTo();
        }
    }
}; 

// one-exon alignments are equal
// gapfilled exons compared by seq; real exons compared by range and splices
// real exon < gapfilled exon; 
bool OneExonCompare(const CModelExon& a, const CModelExon& b) {
    if(!a.m_seq.empty() || !b.m_seq.empty()) {  // at least one is gapfilling
        return a.m_seq < b.m_seq;
    } else if(b.Limits().Empty()) {             // b is from one-exon alignment
        return false;
    } else if(a.Limits().Empty()) {             // a is from one-exon alignment and b is not
        return true;
    } else if(a.m_fsplice != b.m_fsplice) {
        return a.m_fsplice < b.m_fsplice;
    } else if(a.m_ssplice != b.m_ssplice) {
        return a.m_ssplice < b.m_ssplice;
    } else {
        return a.Limits() < b.Limits();
    }
}

struct MultiExonsCompare
{ 
    bool operator() (const CGeneModel::TExons& a, const CGeneModel::TExons& b) const {
        if(a.size() != b.size()) {
            return a.size() < b.size();
        } else {
            for(int i = 0; i < (int)a.size(); ++i) {
                if(OneExonCompare(a[i],b[i]))
                    return true;
                if(OneExonCompare(b[i],a[i]))
                    return false;
            }
            return false;
        }
    }
};


void CAlignCollapser::GetCollapsedAlgnments(TAlignModelClusterSet& clsset) {

    cerr << "Added " << m_count << " alignments to collapser for contig " << m_contig_name << endl;

    if(m_count == 0)
        return;

    FilterAlignments();

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    int oep = args["oep"].AsInteger();
    int max_extend = args["max-extension"].AsInteger();

    set<int> left_exon_ends, right_exon_ends;
    ITERATE(TAlignIntrons, it, m_align_introns) {
        const SIntron& intron = it->first;
        int a = intron.m_range.GetFrom();
        int b = intron.m_range.GetTo();
        left_exon_ends.insert(b);
        right_exon_ends.insert(a);
    }

    int total = 0;
    NON_CONST_ITERATE(Tdata, i, m_aligns) {
        const CAlignCommon& alc = i->first;
        const deque<char>& id_pool = m_target_id_pool[alc];
        deque<SAlignIndividual>& alideque = i->second;

        if(alc.isSR() && !m_collapssr) {   // don't collaps
            ITERATE(deque<SAlignIndividual>, k, alideque) {
                CAlignModel align(alc.GetAlignment(*k, id_pool));
                if(CheckAndInsert(align, clsset))
                    ++total;
            }
        } else {
            sort(alideque.begin(),alideque.end(),LeftAndLongFirstOrder(id_pool));

            bool leftisfixed = (alc.isCap() && alc.isPlus()) || (alc.isPolyA() && alc.isMinus());
            bool rightisfixed = (alc.isPolyA() && alc.isPlus()) || (alc.isCap() && alc.isMinus());
            bool notspliced = alc.GetIntrons().empty();

            typedef list<SAlignExtended> TEA_List;
            TEA_List extended_aligns;

            NON_CONST_ITERATE(deque<SAlignIndividual>, k, alideque) {
                SAlignIndividual& aj = *k;
                bool collapsed = false;
            
                for(TEA_List::iterator itloop = extended_aligns.begin(); itloop != extended_aligns.end(); ) {
                    TEA_List::iterator ita = itloop++;
                    SAlignIndividual& ai = *ita->m_ali;

                    if(aj.m_range.GetFrom() >= min((leftisfixed ? ai.m_range.GetFrom():ai.m_range.GetTo())+1,ita->m_llimb)) {  // extendent align is completed  
                        CAlignModel align(alc.GetAlignment(ai, id_pool));
                        if(CheckAndInsert(align, clsset))
                            ++total;
                        extended_aligns.erase(ita);
                    } else if(!collapsed) {    // even if collapsed must check extended_aligns to the end to purge finished
                        if(rightisfixed && ai.m_range.GetTo() != aj.m_range.GetTo())
                            continue;
                        if(notspliced && aj.m_range.GetTo() > ai.m_range.GetTo()) {
                            if(ai.m_range.GetTo()-aj.m_range.GetFrom()+1 < oep)
                                continue;
                            if(aj.m_range.GetTo()-ita->m_initial_right_end > max_extend)
                                continue;
                            if(aj.m_range.GetFrom()-ai.m_range.GetFrom() > max_extend)
                                continue;
                        }
                        if(aj.m_range.GetTo() > (alc.isEST() ? ita->m_initial_right_end : ita->m_rlimb) || aj.m_range.GetTo() <= ita->m_rlima)
                            continue;
 
                        ai.m_weight += aj.m_weight;
                        if(aj.m_range.GetTo() > ai.m_range.GetTo())
                            ai.m_range.SetTo(aj.m_range.GetTo());
                        collapsed = true;
                    }
                }

                if(!collapsed) 
                    extended_aligns.push_back(SAlignExtended(aj,left_exon_ends,right_exon_ends));
            }

            ITERATE(TEA_List, ita, extended_aligns) {
                CAlignModel align(alc.GetAlignment(*ita->m_ali, id_pool));
                if(CheckAndInsert(align, clsset))
                    ++total;
            }
        }
    }

    if(m_collapsest && m_fillgenomicgaps) {   // collaps ests used for gapfilling
        typedef map<CGeneModel::TExons, vector<TAlignModelList::iterator>, MultiExonsCompare> TEstHolder;
        TEstHolder est_for_collapsing;
        NON_CONST_ITERATE(TAlignModelList, i, m_aligns_for_filtering_only) {
            if(i->Type() == CGeneModel::eEST) {
                CGeneModel::TExons exons = i->Exons();
                if(exons.size() == 1) {
                    exons.front().Limits() = TSignedSeqRange::GetEmpty();
                    _ASSERT(exons.front().m_seq.empty());
                } else {
                    if(exons.front().m_ssplice_sig != "XX")
                        exons.front().Limits().SetFrom(exons.front().GetTo());
                    if(exons.back().m_fsplice_sig != "XX")
                        exons.back().Limits().SetTo(exons.back().GetFrom());
                }
                est_for_collapsing[exons].push_back(i);
            }
        }

        NON_CONST_ITERATE(TEstHolder, i, est_for_collapsing) {
            sort(i->second.begin(),i->second.end(),LeftAndLongFirstOrderForAligns());
            list<TAlignModelList::iterator> ests(i->second.begin(),i->second.end());
            for(list<TAlignModelList::iterator>::iterator ihost = ests.begin(); ihost != ests.end(); ++ihost) {
                CAlignModel& host = **ihost;
                set<int>::const_iterator ri = right_exon_ends.lower_bound(host.Limits().GetTo());  // leftmost compatible rexon
                int rlima = -1;
                if(ri != right_exon_ends.begin())
                    rlima = *(--ri);                                                               // position of the rightmost incompatible rexon
                set<int>::const_iterator li = left_exon_ends.upper_bound(host.Limits().GetFrom()); // leftmost not compatible lexon
                int llimb = numeric_limits<int>::max() ;
                if(li != left_exon_ends.end())
                    llimb = *li;                                                                    // position of the leftmost not compatible lexon
                
                list<TAlignModelList::iterator>::iterator iloop = ihost;
                for(++iloop; iloop != ests.end(); ) {
                    list<TAlignModelList::iterator>::iterator iguest = iloop++;
                    CAlignModel& guest = **iguest;

                    if(guest.Limits().GetFrom() >= min(host.Limits().GetTo()+1,llimb))    // host is completed
                        break;

                    if(guest.Limits().GetTo() > host.Limits().GetTo() || guest.Limits().GetTo() <= rlima)
                        continue;

                    if(host.Strand() != guest.Strand() || (host.Status()&CGeneModel::eUnknownOrientation) != (guest.Status()&CGeneModel::eUnknownOrientation))
                        continue;
                    if((guest.Status()&CGeneModel::ePolyA) || (host.Status()&CGeneModel::ePolyA)) {
                        if((guest.Status()&CGeneModel::ePolyA) != (host.Status()&CGeneModel::ePolyA) 
                           || (guest.Strand() == ePlus && guest.Limits().GetTo() != host.Limits().GetTo())
                           || (guest.Strand() == eMinus && guest.Limits().GetFrom() != host.Limits().GetFrom()))
                            continue;
                    }
                    if((guest.Status()&CGeneModel::eCap) || (host.Status()&CGeneModel::eCap)) {
                        if((guest.Status()&CGeneModel::eCap) != (host.Status()&CGeneModel::eCap) 
                           || (guest.Strand() == eMinus && guest.Limits().GetTo() != host.Limits().GetTo())
                           || (guest.Strand() == ePlus && guest.Limits().GetFrom() != host.Limits().GetFrom()))
                            continue;
                    }

                    host.SetWeight(host.Weight()+guest.Weight());
                    m_aligns_for_filtering_only.erase(*iguest);
                    ests.erase(iguest);
                }
            }
        }
    }

    NON_CONST_ITERATE(TAlignModelList, i, m_aligns_for_filtering_only) {
        if(i->Type() == CGeneModel::emRNA) {
            CBioseq_Handle bh (m_scope->GetBioseqHandle(*i->GetTargetId()));
            const CMolInfo* molinfo = GetMolInfo(bh);
            if(molinfo && molinfo->IsSetTech() && molinfo->GetTech() == CMolInfo::eTech_tsa)
                i->Status() |= CGeneModel::eTSA;   // used to exclude from CDS projection
        }
        if(CheckAndInsert(*i, clsset))
            ++total;
    }
    
    cerr << "After collapsing: " << total << " alignments" << endl;
}


#define MAX_DIST_TO_FLANK_GAP 10000
CAlignModel CAlignCollapser::FillGapsInAlignmentAndAddToGenomicGaps(const CAlignModel& align, int fill) {

    CGeneModel editedmodel = align;
    editedmodel.ClearExons();  // empty alignment with all atributes
    vector<TSignedSeqRange> transcript_exons;

    string left_seq, right_seq;
    CInDelInfo::SSource left_src;
    CInDelInfo::SSource right_src;
    TSignedSeqRange left_texon, right_texon;
    TSignedSeqRange tlim = align.TranscriptLimits();
    string transcript = GetDNASequence(align.GetTargetId(),*m_scope);
    if(tlim.GetFrom() > 30 && ((align.Status()&CGeneModel::ePolyA) == 0 || (align.Status()&CGeneModel::eReversed) == 0)) {
        left_seq = transcript.substr(0,tlim.GetFrom());
        left_texon = TSignedSeqRange(0,tlim.GetFrom()-1);
        left_src.m_acc = align.TargetAccession();
        left_src.m_strand = ePlus;
        left_src.m_range = left_texon;
    }
    if(tlim.GetTo() < align.TargetLen()-30 && ((align.Status()&CGeneModel::ePolyA) == 0 || (align.Status()&CGeneModel::eReversed) != 0)) {
        right_seq = transcript.substr(tlim.GetTo()+1);
        right_texon = TSignedSeqRange(tlim.GetTo()+1,align.TargetLen()-1);
        right_src.m_acc = align.TargetAccession();
        right_src.m_strand = ePlus;
        right_src.m_range = right_texon;
    }
    if(align.Orientation() == eMinus) {
        swap(left_seq, right_seq);
        swap(left_texon, right_texon);
        swap(left_src, right_src);
    }

    if(!left_seq.empty() && (fill&efill_left) != 0) {
        TIntMap::iterator ig = m_genomic_gaps_len.lower_bound(align.Limits().GetFrom());
        if(ig != m_genomic_gaps_len.begin() && (--ig)->first > align.Limits().GetFrom()-MAX_DIST_TO_FLANK_GAP) {  // there is gap on left
            transcript_exons.push_back(left_texon);
            editedmodel.AddExon(TSignedSeqRange::GetEmpty(), "XX", "XX", 1, left_seq, left_src);
 
            if(align.Orientation() == eMinus) {
                ReverseComplement(left_seq.begin(),left_seq.end());
                left_src.m_strand = eMinus;
            }
            m_correction_data.m_correction_indels.push_back(CInDelInfo(max(0,ig->first+2*ig->second/3), left_seq.length(), CInDelInfo::eDel, left_seq, left_src));   // 1/3 of gap length will separate genes abatting the same gap
        }
    }

    for(int i = 0; i < (int)align.Exons().size(); ++i) {
        transcript_exons.push_back(align.TranscriptExon(i));
        const CModelExon& e = align.Exons()[i];
        editedmodel.AddExon(e.Limits(),e.m_fsplice_sig, e.m_ssplice_sig, e.m_ident);

        if(i < (int)align.Exons().size()-1 && (!e.m_ssplice || !align.Exons()[i+1].m_fsplice)) {
            if((fill&efill_middle) != 0) {
                TSignedSeqRange texon = align.GetAlignMap().MapRangeOrigToEdited(TSignedSeqRange(e.GetTo(),align.Exons()[i+1].GetFrom()),false);
                TIntMap::iterator ig = m_genomic_gaps_len.lower_bound(e.GetTo());  // first gap on right
                if(ig != m_genomic_gaps_len.end() && ig->first < align.Exons()[i+1].GetFrom() && texon.GetLength() > 2) {   // there is a gap
                    texon.SetFrom(texon.GetFrom()+1);
                    texon.SetTo(texon.GetTo()-1);
                    transcript_exons.push_back(texon);
                    string seq = transcript.substr(texon.GetFrom(),texon.GetLength());
                    CInDelInfo::SSource src;
                    src.m_acc = align.TargetAccession();
                    src.m_strand = ePlus;
                    src.m_range = texon;
                    editedmodel.AddExon(TSignedSeqRange::GetEmpty(), "XX", "XX", 1, seq, src);
                        
                    if(align.Orientation() == eMinus) {
                        ReverseComplement(seq.begin(),seq.end());
                        src.m_strand = eMinus;
                    }
                    m_correction_data.m_correction_indels.push_back(CInDelInfo(ig->first+ig->second/2, seq.length(), CInDelInfo::eDel, seq, src));
                } else {
                    editedmodel.AddHole();
                }
            } else {
                editedmodel.AddHole();
            }
        }
    }

    if(!right_seq.empty() && (fill&efill_right) != 0) {
        TIntMap::iterator ig = m_genomic_gaps_len.upper_bound(align.Limits().GetTo());
        if(ig != m_genomic_gaps_len.end() && ig->first < align.Limits().GetTo()+MAX_DIST_TO_FLANK_GAP) {  // there is gap on right
            transcript_exons.push_back(right_texon);
            editedmodel.AddExon(TSignedSeqRange::GetEmpty(), "XX", "XX", 1, right_seq, right_src);
                    
            if(align.Orientation() == eMinus) {
                ReverseComplement(right_seq.begin(),right_seq.end());
                right_src.m_strand = eMinus;
            }
            m_correction_data.m_correction_indels.push_back(CInDelInfo(ig->first+ig->second/3, right_seq.length(), CInDelInfo::eDel, right_seq, right_src));   // 1/3 of gap length will separate genes abatting the same gap
        }
    }

    CAlignMap editedamap(editedmodel.Exons(), transcript_exons, align.FrameShifts(), align.Orientation(), align.GetAlignMap().TargetLen());
    editedmodel.FrameShifts() = align.FrameShifts();
    CAlignModel editedalign(editedmodel, editedamap);
    editedalign.SetTargetId(*align.GetTargetId());

    return editedalign;
}

#define COLLAPS_CHUNK 500000
void CAlignCollapser::AddAlignment(const CAlignModel& a) {

    string acc = a.TargetAccession();
    if(acc.find("CorrectionData") != string::npos) {
        if(!m_genomic_gaps_len.empty()) {
            TIntMap::iterator gap = m_genomic_gaps_len.upper_bound(a.Limits().GetTo());               // gap clearly on the right (could be end)
            if(gap != m_genomic_gaps_len.begin())
                --gap;                                                                                // existing gap (not end)
            if(gap->first <= a.Limits().GetTo() && gap->first+gap->second-1 >= a.Limits().GetFrom())  // overlap
                return;
        }

        m_correction_data.m_confirmed_intervals.push_back(a.Limits());

        TInDels corrections = a.FrameShifts();
        ITERATE(TInDels, i, corrections) {
            if(i->IsMismatch()) {
                string seq = i->GetInDelV();
                for(int l = 0; l < i->Len(); ++l)
                    m_correction_data.m_replacements[i->Loc()+l] = seq[l];
            } else {
               m_correction_data.m_correction_indels.push_back(*i);
               m_correction_data.m_correction_indels.back().SetStatus(CInDelInfo::eGenomeNotCorrect);
            }
        }                

        return;
    }

    if((a.Type()&CGeneModel::eSR) && !a.Continuous())   // ignore SR with internal gaps
        return;

    CAlignModel align(a);
    if(!m_fillgenomicgaps)
        align.Status() &= ~CGeneModel::eGapFiller;
        
    if((align.Type()&CGeneModel::eNotForChaining) && !(align.Status()&CGeneModel::eGapFiller))
        return;    

    if((align.Status()&CGeneModel::eUnknownOrientation) && align.Strand() == eMinus)
        align.ReverseComplementModel();

    const CGeneModel::TExons& e = align.Exons();
    for(unsigned int l = 1; l < e.size(); ++l) {
        if(e[l-1].m_ssplice && e[l].m_fsplice) {
            string sig;
            if(align.Strand() == ePlus)
                sig = e[l-1].m_ssplice_sig+e[l].m_fsplice_sig;
            else
                sig = e[l].m_fsplice_sig+e[l-1].m_ssplice_sig;
            SIntron intron(e[l-1].GetTo(),e[l].GetFrom(), align.Strand(), (align.Status()&CGeneModel::eUnknownOrientation) == 0, sig);
            SIntronData& id = m_align_introns[intron];

            if(((align.Type()&CGeneModel::eSR) && !m_filtersr) || 
               ((align.Type()&CGeneModel::eEST) && !m_filterest) ||
               ((align.Type()&CGeneModel::emRNA) && !m_filtermrna) ||
               ((align.Type()&CGeneModel::eProt) && !m_filterprots)) {

                id.m_keep_anyway = true;
            }

            if((align.Type()&CGeneModel::eSR) || 
               (align.Status()&CGeneModel::eGapFiller && sig == "GTAG" && 
                e[l-1].Limits().GetLength() > 15 && e[l-1].m_ident > 0.99 && 
                e[l].Limits().GetLength() > 15 && e[l].m_ident > 0.99)) {
                
                id.m_selfsp_support = true;
            }

            id.m_weight += align.Weight();

            if(align.Type()&(CGeneModel::eEST|CGeneModel::emRNA))
                id.m_est_support += align.Weight()+0.5;

            double ident = min(e[l-1].m_ident,e[l].m_ident);
            if(ident == 0.)
                ident = 1.;   // collapsed SRA and proteins don't have ident information
            id.m_ident = max(id.m_ident,ident);
        }
    }

    if((align.Type()&CGeneModel::eSR) || ((align.Type()&CGeneModel::eEST) && !(align.Status()&CGeneModel::eGapFiller) && m_collapsest)) {   // add alignments for collapsing
        if(align.Continuous()) {
            CAlignCommon c(align);
            m_aligns[c].push_back(SAlignIndividual(align, m_target_id_pool[c]));
        } else {
            TAlignModelList aligns = GetAlignParts(align, false);
            ITERATE(TAlignModelList, i, aligns) {
                CAlignCommon c(*i);
                m_aligns[c].push_back(SAlignIndividual(*i, m_target_id_pool[c]));                
            }
        }
    } else {
        m_aligns_for_filtering_only.push_back(align);
    }
        
    if(++m_count%COLLAPS_CHUNK == 0) {
        cerr << "Added " << m_count << " alignments to collapser" << endl;
        CollapsIdentical();
    }
}

void CAlignCollapser::CollapsIdentical() {
    NON_CONST_ITERATE(Tdata, i, m_aligns) {
        deque<SAlignIndividual>& alideque = i->second;
        deque<char>& id_pool = m_target_id_pool[i->first];
        if(!alideque.empty()) {

            //remove identicals
            sort(alideque.begin(),alideque.end(),LeftAndLongFirstOrder(id_pool));
            deque<SAlignIndividual>::iterator ali = alideque.begin();
            for(deque<SAlignIndividual>::iterator farp = ali+1; farp != alideque.end(); ++farp) {
                _ASSERT(farp > ali);
                if(farp->m_range == ali->m_range) {
                    ali->m_weight += farp->m_weight;
                    for(deque<char>::iterator p = id_pool.begin()+farp->m_target_id; *p != 0; ++p) {
                        _ASSERT(p < id_pool.end());
                        *p = 0;
                    }
                } else {
                    *(++ali) = *farp;
                }
            }
            _ASSERT(ali-alideque.begin()+1 <= (int)alideque.size());
            alideque.resize(ali-alideque.begin()+1);  // ali - last retained element

            
            
            //clean up id pool and reset shifts
            sort(alideque.begin(),alideque.end(),OriginalOrder);
            deque<char>::iterator id = id_pool.begin();
            int shift = 0;
            ali = alideque.begin();
            for(deque<char>::iterator farp = id; farp != id_pool.end(); ) {
                while(farp != id_pool.end() && *farp == 0) {
                    ++farp;
                    ++shift;
                }
                if(farp != id_pool.end()) {
                                                            
                    if(farp-id_pool.begin() == ali->m_target_id) {
                        ali->m_target_id -= shift;
                        _ASSERT(ali->m_target_id >= 0);
                        ++ali;
                    }
                    

                    _ASSERT(farp >= id);
                    while(*farp != 0) {
                        *id++ = *farp++;
                    }
                    *id++ = *farp++;
                }
            }
            id_pool.resize(id-id_pool.begin());  // id - next after last retained element

            _ASSERT(ali == alideque.end());
        }    
    }
}


END_SCOPE(gnomon)
END_SCOPE(ncbi)


