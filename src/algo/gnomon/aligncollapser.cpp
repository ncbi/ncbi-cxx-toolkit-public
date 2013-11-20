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
    CArgs args = CNcbiApplication::Instance()->GetArgs();
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

bool isGoodIntron(int a, int b, int strand, const CAlignCollapser::TAlignIntrons& introns) {
    SIntron intron_oriented_nosig(a, b, strand, true, "");
    SIntron intron_notoriented_nosig(a, b, ePlus, false, "");
    return (introns.find(intron_oriented_nosig) != introns.end() || introns.find(intron_notoriented_nosig) != introns.end());
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
            if(!(exonl.m_ssplice && exonr.m_fsplice) || isGoodIntron(exonl.GetTo(), exonr.GetFrom(), align.Strand(), m_align_introns))
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

    if(!align.TrustedProt().empty() && (align.Status()&CGeneModel::eBestPlacement)) {

        double cov = 0;
        int nt = 0;
        ITERATE(CGeneModel::TExons, e, align.Exons()) {
            for(int i = e->GetFrom(); i <= e->GetTo(); ++i) {
                cov += m_coverage[i-m_left_end];
                ++nt;
            }
        }
        cov /= nt;

        CGeneModel am = align;
        string protseq = am.GetProtein(m_contigrv);
        protseq.resize(protseq.length()-1);
        int pstop = protseq.find("*");
        bool ps = false;
        if(pstop != (int)string::npos) {
            ps = true;
            CAlignMap amp = am.GetAlignMap();
            int tcds = amp.MapRangeOrigToEdited(align.GetCdsInfo().Cds()).GetFrom();
            while(pstop != (int)string::npos) {
                int p = amp.MapEditedToOrig(tcds+3*pstop);
                if(p >= 0) {
                    cerr << "Pstopinfo\t" << align.ID() << '\t' << align.TargetLen() << '\t' << 3*pstop << '\t' << p << '\t' << m_coverage[p-m_left_end] << '\t' << m_coverage[p-m_left_end]/cov*100 << '\t' << protseq << endl;
                }
        
                pstop = protseq.find("*",pstop+1);
            }
        }
        ITERATE(TInDels, i, align.FrameShifts()) {
            cerr << "Fshiftinfo\t" << align.ID() << '\t' << i->Loc() << '\t' << m_coverage[i->Loc()-m_left_end] << '\t' << m_coverage[i->Loc()-m_left_end]/cov*100 << '\t' << protseq << endl;
        }
    }

    return true;
}

bool CAlignCollapser::RemoveNotSupportedIntronsFromTranscript(CAlignModel& align) const {

    CAlignMap amap = align.GetAlignMap();

    CGeneModel editedmodel = align;
    editedmodel.ClearExons();  // empty alignment with all atributes
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
            if(isGoodIntron(exonl.GetTo(), exonr.GetFrom(), a.Strand(), m_align_introns))
                break;
            else
                new_left = exonr.GetFrom();
        }
        int new_right = a.Limits().GetTo();
        for(int k = (int)a.Exons().size()-1; k > 0 && a.Exons()[k-1].GetTo() > new_left; --k) {
            CModelExon exonl = a.Exons()[k-1];
            CModelExon exonr = a.Exons()[k];
            if(isGoodIntron(exonl.GetTo(), exonr.GetFrom(), a.Strand(), m_align_introns))
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

        piece_begin = piece_end;
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
            if(exonl.m_ssplice && exonr.m_fsplice && !isGoodIntron(exonl.GetTo(), exonr.GetFrom(), editedmodel.Strand(), m_align_introns)) {
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

    CAlignMap editedamap(editedmodel.Exons(), transcript_exons, amap.GetInDels(false), align.Orientation(), align.GetAlignMap().TargetLen());
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


void CAlignCollapser::FilterAlignments() {

    //    if(!m_filtersr && !m_filterest && !m_filtermrna && !m_filterprots)
    //        return;

    CArgs args = CNcbiApplication::Instance()->GetArgs();

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
    vector<int> left_plus(len,right_end);       // closest left + strand splice 'on the right' from the current position
    vector<int> left_minus(len,right_end);      // closest left - strand splice 'on the right' from the current position
    vector<int> right_plus(len,m_left_end);       // closest right + strand splice 'on the left' from the current position
    vector<int> right_minus(len,m_left_end);      // closest right - strand splice 'on the left' from the current position
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
            CAlignModel a = align;
            a.Status() |= CGeneModel::eUnmodifiedAlign;
            m_aligns_for_filtering_only.push_front(a);

            good_alignment = RemoveNotSupportedIntronsFromProt(align);
        } else {
            good_alignment = RemoveNotSupportedIntronsFromTranscript(align);
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

    //clean self species cDNA edges
    for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
        TAlignModelList::iterator i = it++;
        CAlignModel& align = *i;

        if(align.Status()&CGeneModel::eGapFiller) {
            string transcript = GetDNASequence(align.GetTargetId(),*m_scope);
            for(int ie = 0; ie < (int)align.Exons().size(); ++ie) {
                if(!align.Exons()[ie].m_fsplice) {
                    CleanExonEdge(ie, align, transcript, false);
                }
                if(!align.Exons()[ie].m_ssplice) {
                    CleanExonEdge(ie, align, transcript, true);
                }
            } 
        }
    }
    

#define MIN_EXON 30      
    //cut NotForChaining and fill gaps
    for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
        TAlignModelList::iterator i = it++;
        CAlignModel& align = *i;

        if(!(align.Status()&CGeneModel::eGapFiller)) {
            if(align.Type()&CGeneModel::eNotForChaining)
                m_aligns_for_filtering_only.erase(i);
            continue;
        } 

        if(!(align.Type()&CGeneModel::eNotForChaining)) {
            CAlignModel editedalign = FillGapsInAlignmentAndAddToGenomicGaps(align, efill_left|efill_right|efill_middle);
            if(editedalign.Exons().size() > align.Exons().size()) {
                m_aligns_for_filtering_only.push_front(editedalign);
                m_aligns_for_filtering_only.erase(i);
            }
        } else {
            if(align.Exons().front().Limits().GetLength() > MIN_EXON/2) {
                CAlignModel a = align;
                TSignedSeqRange l;
                if(align.Exons().front().m_ssplice)
                    l = a.Exons().front().Limits();
                else if(align.Exons().front().Limits().GetLength() > MIN_EXON)
                    l = TSignedSeqRange(a.Exons().front().GetFrom(),(a.Exons().front().GetFrom()+a.Exons().front().GetTo())/2-1);
                if(l.NotEmpty())
                    l = a.GetAlignMap().ShrinkToRealPoints(l, false);
                if(l.NotEmpty()) {
                    a.Clip(l, CGeneModel::eRemoveExons);
                    CAlignModel editedalign = FillGapsInAlignmentAndAddToGenomicGaps(a, efill_left);
                    if(editedalign.Exons().size() > 1)
                        m_aligns_for_filtering_only.push_front(editedalign);
                }
            }

            for(int ie = 0; ie < (int)align.Exons().size()-1; ++ie) {
                if((!align.Exons()[ie].m_ssplice || !align.Exons()[ie+1].m_fsplice) && 
                   align.Exons()[ie].Limits().GetLength() > MIN_EXON/2 && align.Exons()[ie+1].Limits().GetLength() > MIN_EXON/2) {
                    CAlignModel a = align;
                    int left = -1;
                    int right = -1;

                    if(a.Exons()[ie].m_fsplice)
                        left = a.Exons()[ie].GetFrom();
                    else if(align.Exons()[ie].Limits().GetLength() > MIN_EXON)
                        left = (a.Exons()[ie].GetFrom()+a.Exons()[ie].GetTo())/2+1;

                    if(a.Exons()[ie+1].m_ssplice)
                        right = a.Exons()[ie+1].GetTo();
                    else if(a.Exons()[ie+1].Limits().GetLength() > MIN_EXON)
                        right = (a.Exons()[ie+1].GetFrom()+a.Exons()[ie+1].GetTo())/2-1;

                    if(left >= 0 && right >= 0) {
                        TSignedSeqRange l(left, right);
                        l = a.GetAlignMap().ShrinkToRealPoints(l, false);
                        if(l.NotEmpty()) {
                            a.Clip(l, CGeneModel::eRemoveExons);
                            CAlignModel editedalign = FillGapsInAlignmentAndAddToGenomicGaps(a, efill_middle);
                            if(editedalign.Exons().size() > 2)
                                m_aligns_for_filtering_only.push_front(editedalign);
                        }
                    }
                }
           }

            if(align.Exons().back().Limits().GetLength() > MIN_EXON/2) {
                CAlignModel a = align;
                TSignedSeqRange l;
                if(align.Exons().back().m_fsplice)
                    l = align.Exons().back().Limits();
                else if(align.Exons().back().Limits().GetLength() > MIN_EXON)
                    l = TSignedSeqRange((a.Exons().back().GetFrom()+a.Exons().back().GetTo())/2+1, a.Exons().back().GetTo());

                if(l.NotEmpty())
                    l = a.GetAlignMap().ShrinkToRealPoints(l, false);
                if(l.NotEmpty()) {
                    a.Clip(l, CGeneModel::eRemoveExons);
                    CAlignModel editedalign = FillGapsInAlignmentAndAddToGenomicGaps(a, efill_right);
                    if(editedalign.Exons().size() > 1)
                        m_aligns_for_filtering_only.push_front(editedalign);
                }
            }

            m_aligns_for_filtering_only.erase(i);
        }
    }

    //include gap's boundaries in no cross splices
    ITERATE(TAlignModelList, i, m_aligns_for_filtering_only) {
        const CAlignModel& align = *i;
        for(int ie = 0; ie < (int)align.Exons().size(); ++ie) {
            if(align.Exons()[ie].Limits().Empty()) {
                if(ie > 0) {
                    int a = align.Exons()[ie-1].GetTo();
                    if(a <= right_end) {   // TSA could be slightly extended in the area without alignmnets
                        if((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus)
                            left_plus[a-m_left_end] = a;
                        if((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand()== eMinus)
                            left_minus[a-m_left_end] = a;
                    }
                }
                if(ie < (int)align.Exons().size()-1) {
                    int b = align.Exons()[ie+1].GetFrom();
                    if(b >= m_left_end) {   // TSA could be slightly extended in the area without alignmnets
                        if((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus)
                            right_plus[b-m_left_end] = b;
                        if((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand()== eMinus)
                            right_minus[b-m_left_end] = b;
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

    //trim 3'/5' exons crossing splices (including hole boundaries)
    for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
        TAlignModelList::iterator i = it++;
        CAlignModel& align = *i;

        if(align.Status()&CGeneModel::eUnmodifiedAlign)
            continue;

        const CAlignMap& amap = align.GetAlignMap();

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
                    if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) && right_plus[r-m_left_end] > l+trim) // crosses right plus splice            
                        new_l = right_plus[r-m_left_end];
                    if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == eMinus) && right_minus[r-m_left_end] > l+trim) // crosses right minus splice         
                        new_l = right_minus[r-m_left_end];
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
                    if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) && left_plus[l-m_left_end] < r-trim) // crosses left plus splice              
                        new_r = left_plus[l-m_left_end];
                    if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == eMinus) && left_minus[l-m_left_end] < r-trim) // crosses left minus splice               
                        new_r = left_minus[l-m_left_end];
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
    sort(m_align_gaps.begin(),m_align_gaps.end());
    m_align_gaps.erase( unique(m_align_gaps.begin(),m_align_gaps.end()), m_align_gaps.end() );

    total += m_aligns_for_filtering_only.size();
    

    cerr << "After filtering: " << m_align_introns.size() << " introns, " << total << " alignments" << endl;
}

bool CAlignCollapser::CheckAndInsert(const CAlignModel& align, TAlignModelClusterSet& clsset) const {

    ITERATE(CGeneModel::TExons, i, align.Exons()) {
        if(i->Limits().NotEmpty()) {
            CInDelInfo p(i->GetFrom(), 1, false);
            TInDels::const_iterator ig = lower_bound(m_align_gaps.begin(), m_align_gaps.end(), p); // first equal or greater
            if(ig != m_align_gaps.end() && ig->Loc() <= i->GetTo())      // exon overlaps with inserted gap
                return false;
        }
    }
   
    clsset.Insert(align);
    return true;
}

void CAlignCollapser::GetCollapsedAlgnments(TAlignModelClusterSet& clsset) {

    cerr << "Added " << m_count << " alignments to collapser for contig " << m_contig_name << endl;

    if(m_count == 0)
        return;

    FilterAlignments();

    CArgs args = CNcbiApplication::Instance()->GetArgs();
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
                    } else if(!collapsed) {
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

    NON_CONST_ITERATE(TAlignModelList, i, m_aligns_for_filtering_only) {
        if(i->Type() == CGeneModel::emRNA) {
            CBioseq_Handle bh (m_scope->GetBioseqHandle(*i->GetTargetId()));
            const CMolInfo* molinfo = GetMolInfo(bh);
            if(molinfo && molinfo->IsSetTech() && molinfo->GetTech() == CMolInfo::eTech_tsa)
                i->Status() |= CGeneModel::eTSA;
        }
        if(CheckAndInsert(*i, clsset))
            ++total;
    }
    
    cerr << "After collapsing: " << total << " alignments" << endl;
}

#define CHECK_LENGTH 10
#define CLOSE_GAP 5
#define BIG_NOT_ALIGNED 20
#define EXTRA_CUT 5

void CAlignCollapser::CleanExonEdge(int ie, CAlignModel& align, const string& transcript, bool right_edge) const {

    if(align.Exons()[ie].Limits().GetLength() < CHECK_LENGTH)
        return;
    if(align.Status()&CGeneModel::ePolyA) {
        if(ie == 0 && align.Strand() == eMinus && !right_edge)
            return;
        if(ie == (int)align.Exons().size()-1 && align.Strand() == ePlus && right_edge)
            return;
    }

    TSignedSeqRange texon = align.TranscriptExon(ie);
    TSignedSeqRange tgap;
    if((ie == 0 && !right_edge && align.Orientation() == ePlus) || (ie == (int)align.Exons().size()-1 && right_edge && align.Orientation() == eMinus)) {  // 5p not aligned
        tgap.SetFrom(0);
        tgap.SetTo(texon.GetFrom()-1);
    } else if((ie == 0 && !right_edge && align.Orientation() == eMinus) || (ie == (int)align.Exons().size()-1 && right_edge && align.Orientation() == ePlus)) { // 3p not aligned
        tgap.SetFrom(texon.GetTo()+1);
        tgap.SetTo(align.TargetLen()-1);
    } else {  // hole
        if(right_edge)
            tgap = align.GetAlignMap().MapRangeOrigToEdited(TSignedSeqRange(align.Exons()[ie].GetTo(),align.Exons()[ie+1].GetFrom()), false);
        else
            tgap = align.GetAlignMap().MapRangeOrigToEdited(TSignedSeqRange(align.Exons()[ie-1].GetTo(),align.Exons()[ie].GetFrom()), false);

        tgap.SetFrom(tgap.GetFrom()+1);
        tgap.SetTo(tgap.GetTo()-1);
    }


    //transcript sequence for exon+not aligned
    string tseq = transcript.substr((texon+tgap).GetFrom(),(texon+tgap).GetLength());
    if(align.Orientation() == eMinus)
        ReverseComplement(tseq.begin(),tseq.end());   // tseq in genome direction (gap is on 'right/left' side)

    int direction = right_edge ? 1 : -1;
    int gedge = right_edge ? align.Exons()[ie].GetTo() : align.Exons()[ie].GetFrom();  // last aligned base before gap in genome coordinates
    int tedge = right_edge ? texon.GetLength()-1 : tgap.GetLength();                   // last aligned base before gap in tseq coordinates

    //expand if identical
    int gleftlim = 0;
    if(!right_edge && ie > 0)
        gleftlim = align.Exons()[ie-1].GetTo()+1;
    int grightlim = m_contig.length()-1;
    if(right_edge && ie < (int)align.Exons().size()-1)
        grightlim = align.Exons()[ie+1].GetFrom()-1;
    while(gedge+direction >= gleftlim && gedge+direction <= grightlim && 
          tedge+direction >= 0 && tedge+direction < (int)tseq.length() && 
          tseq[tedge+direction] == m_contig[gedge+direction]) {
        gedge += direction;
        tedge += direction;
    }

    // cut not aligned transceipt sequence
    int not_aligned_length = tseq.length();
    if(right_edge)
        tseq = tseq.substr(0,tedge+1);
    else
        tseq = tseq.substr(tedge);
    not_aligned_length -= tseq.length();

    int gleft = min(gedge,align.Exons()[ie].GetFrom());
    int gright = max(gedge,align.Exons()[ie].GetTo());
    string gseq = m_contig.substr(gleft,gright-gleft+1);
    
    //insert indels into both sequences
    TInDels indels = align.GetAlignMap().GetInDels(false);
    TInDels::const_iterator indl = indels.end();
    ITERATE(TInDels, i, indels) {
        if(i->Loc() < align.Exons()[ie].GetFrom())
            continue;
        if(i->IsDeletion() && i->Loc() == align.Exons()[ie].GetFrom() && !align.Exons()[ie].m_fsplice)
            continue;

        indl = i;
        break;
    }
    int gp = 0;
    int tp = 0;
    for(int i = gleft; i <= gright && indl != indels.end(); ++i) {
        if(indl->Loc() == i) {
            if(indl->IsDeletion()) {
                gseq.insert(gp, indl->Len(), '#');
                gp += indl->Len();
                tp += indl->Len();
            } else {
                tseq.insert(tp, indl->Len(), '#');
            }
            ++indl;
        } 

        ++gp;
        ++tp;        
    }
    if(indl != indels.end() && indl->Loc() == gright+1 && indl->IsDeletion()) {   // deletion at the end of exon
        gseq.insert(gp, indl->Len(), '#');
    }
    _ASSERT(gseq.length() == tseq.length() && gseq.length() >= CHECK_LENGTH);


    //trim mismatches
    int mismatches = 0;
    if(right_edge) {  // reverse sequences for easy counting
        reverse(gseq.begin(),gseq.end());
        reverse(tseq.begin(),tseq.end());
    }
    for(int i = 0; i < CHECK_LENGTH; ++i) {
        if(gseq[i] != tseq[i])
            ++mismatches;
    }
    while(mismatches > 0 && gseq.length() > CHECK_LENGTH) {
        if(gseq[0] != '#')
            gedge -= direction;
        if(tseq[0] != '#')
            ++not_aligned_length;
        if(gseq[0] != tseq[0])
            --mismatches;
        if(gseq[CHECK_LENGTH] != tseq[CHECK_LENGTH])
            ++mismatches;
        gseq.erase(gseq.begin());
        tseq.erase(tseq.begin());
    }

    if(mismatches == 0) {  
        int distance_to_gap = -1;
        if(right_edge) {
            TIntMap::const_iterator igap = m_genomic_gaps_len.lower_bound(gedge);
            if(igap != m_genomic_gaps_len.end())
                distance_to_gap = igap->first-gedge-1;
        } else {
            TIntMap::const_iterator igap = m_genomic_gaps_len.lower_bound(gedge);
            if(igap != m_genomic_gaps_len.begin()) {
                --igap;
                distance_to_gap = gedge-(igap->first+igap->second);
            }
        }


        double ident = 0.;
        for(int i = 0; i < (int)gseq.length(); ++i) {
            if(gseq[i] == tseq[i])
                ++ident;
        }
        ident /= gseq.length();

        //find splice if possible
        string splice_sig;
        if(distance_to_gap == 0) { // ubutting gap      
            splice_sig = "NN";
        } else if(distance_to_gap > CLOSE_GAP && not_aligned_length > BIG_NOT_ALIGNED) {
            int extracut = 0; 
            string splice, splice2;
            if(right_edge) {
                splice = (align.Strand() == ePlus) ? "GT" : "CT";
                if(align.Status()&CGeneModel::eUnknownOrientation)
                    splice2 = (align.Strand() == ePlus) ? "CT" : "GT";
            } else {
                splice = (align.Strand() == ePlus) ? "AG" : "AC";
                if(align.Status()&CGeneModel::eUnknownOrientation)
                    splice2 = (align.Strand() == ePlus) ? "AC" : "AG";
            }

            string spl;
            while(gseq.length() > CHECK_LENGTH && extracut < EXTRA_CUT && gseq[CHECK_LENGTH] == tseq[CHECK_LENGTH]) {
                spl = m_contig.substr(min(gedge+1,gedge+2*direction),2);
                if(spl == splice || spl == splice2)
                    break;

                gedge -= direction;
                ++extracut;
                gseq.erase(gseq.begin());
                tseq.erase(tseq.begin());
            }

            if(spl == splice || spl == splice2) {
                if(align.Strand() == eMinus)
                    ReverseComplement(spl.begin(),spl.end());
                splice_sig = spl;

                ident = 0.;
                for(int i = 0; i < (int)gseq.length(); ++i) {
                    if(gseq[i] == tseq[i])
                        ++ident;
                }
                ident /= gseq.length();
            } else {                           // didn't find splice - reverse extracut
                gedge += extracut*direction;   // gseq and tseq are invalid after this point !!!!
            }
        }
        
        CGeneModel editedmodel = align;
        editedmodel.ClearExons();  // empty alignment with all atributes
        vector<TSignedSeqRange> transcript_exons;
        
        for(int i = 0; i < (int)align.Exons().size(); ++i) {
            TSignedSeqRange te = align.TranscriptExon(i);
            const CModelExon& e = align.Exons()[i];
            if(i == ie) {
                if(right_edge) {
                    editedmodel.AddExon(TSignedSeqRange(e.GetFrom(),gedge),e.m_fsplice_sig, splice_sig, ident);
                    if(gedge < e.GetTo()) { // clip
                        te = align.GetAlignMap().MapRangeOrigToEdited(editedmodel.Exons().back().Limits(), align.Exons()[i].m_fsplice ? CAlignMap::eLeftEnd : CAlignMap::eSinglePoint, CAlignMap::eSinglePoint);
                        _ASSERT(te.NotEmpty());
                    } else if(gedge > e.GetTo()) { // expansion
                        int delta = gedge-e.GetTo();
                        if(align.Orientation() == ePlus)
                            te.SetTo(te.GetTo()+delta);
                        else
                            te.SetFrom(te.GetFrom()-delta);
                    }
                } else {
                    editedmodel.AddExon(TSignedSeqRange(gedge,e.GetTo()), splice_sig, e.m_ssplice_sig, ident);
                    if(gedge > e.GetFrom()) { // clip
                        te = align.GetAlignMap().MapRangeOrigToEdited(editedmodel.Exons().back().Limits(), CAlignMap::eSinglePoint, align.Exons()[i].m_ssplice ? CAlignMap::eRightEnd : CAlignMap::eSinglePoint);
                        _ASSERT(te.NotEmpty());
                    } else if(gedge < e.GetFrom()) { // expansion
                        int delta = e.GetFrom()-gedge;
                        if(align.Orientation() == ePlus)
                            te.SetFrom(te.GetFrom()-delta);
                        else
                            te.SetTo(te.GetTo()+delta);
                    }
                }
            } else {
                editedmodel.AddExon(e.Limits(), e.m_fsplice_sig, e.m_ssplice_sig, e.m_ident);
            }
            transcript_exons.push_back(te);

            if(i < (int)align.Exons().size()-1 && (!align.Exons()[i].m_ssplice || !align.Exons()[i+1].m_fsplice))
                editedmodel.AddHole();
        }

        CAlignMap editedamap(editedmodel.Exons(), transcript_exons, indels, align.Orientation(), align.GetAlignMap().TargetLen());
        CAlignModel editedalign(editedmodel, editedamap);
        editedalign.SetTargetId(*align.GetTargetId());

        align = editedalign;
    }
}

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
        if(ig != m_genomic_gaps_len.begin() && (--ig)->first > align.Limits().GetFrom()-10000) {  // there is gap on left
            transcript_exons.push_back(left_texon);
            editedmodel.AddExon(TSignedSeqRange::GetEmpty(), "XX", "XX", 1, left_seq, left_src);
 
            if(align.Orientation() == eMinus) {
                ReverseComplement(left_seq.begin(),left_seq.end());
                left_src.m_strand = eMinus;
            }
            m_align_gaps.push_back(CInDelInfo(max(0,ig->first+2*ig->second/3), left_seq.length(), false, left_seq, left_src));   // 1/3 of gap length will separate genes abatting the same gap
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
                    m_align_gaps.push_back(CInDelInfo(ig->first+ig->second/2, seq.length(), false, seq, src));
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
        if(ig != m_genomic_gaps_len.end() && ig->first < align.Limits().GetTo()+10000) {  // there is gap on right
            transcript_exons.push_back(right_texon);
            editedmodel.AddExon(TSignedSeqRange::GetEmpty(), "XX", "XX", 1, right_seq, right_src);
                    
            if(align.Orientation() == eMinus) {
                ReverseComplement(right_seq.begin(),right_seq.end());
                right_src.m_strand = eMinus;
            }
            m_align_gaps.push_back(CInDelInfo(ig->first+ig->second/3, right_seq.length(), false, right_seq, right_src));   // 1/3 of gap length will separate genes abatting the same gap
        }
    }

    CAlignMap editedamap(editedmodel.Exons(), transcript_exons, align.GetAlignMap().GetInDels(false), align.Orientation(), align.GetAlignMap().TargetLen());
    CAlignModel editedalign(editedmodel, editedamap);
    editedalign.SetTargetId(*align.GetTargetId());

    return editedalign;
}

#define COLLAPS_CHUNK 500000
void CAlignCollapser::AddAlignment(const CAlignModel& a) {

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

    if((align.Type()&CGeneModel::eSR) || ((align.Type()&CGeneModel::eEST) && m_collapsest && align.Continuous())) {   // add alignments for collapsing
        CAlignCommon c(align);
        m_aligns[c].push_back(SAlignIndividual(align, m_target_id_pool[c]));
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


