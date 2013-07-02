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


BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(gnomon)

CAlignModel CAlignCommon::GetAlignment(const SAlignIndividual& ali, const deque<char>& target_id_pool) const {

    CGeneModel a(isPlus() ? ePlus : eMinus, 0, isEST() ? CGeneModel::eEST : CGeneModel::eSR);
    if(isPolyA()) 
        a.Status() |= CGeneModel::ePolyA;
    if(isCap())
        a.Status() |= CGeneModel::eCap;
    if(isUnknown()) 
        a.Status()|= CGeneModel::eUnknownOrientation;
    a.SetID(ali.m_align_id);
    a.SetWeight(ali.m_weight);

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

    string target;
    for(int i = ali.m_target_id; target_id_pool[i] != 0; ++i)
        target.push_back(target_id_pool[i]);

    CRef<CSeq_id> target_id(CIdHandler::ToSeq_id(target));
    align.SetTargetId(*target_id);

    return align;
};

bool LeftAndLongFirstOrder(const SAlignIndividual& a, const SAlignIndividual& b) {  // left and long first
        if(a.m_range == b.m_range)
            return a.m_target_id < b.m_target_id;
        else if(a.m_range.GetFrom() != b.m_range.GetFrom())
            return a.m_range.GetFrom() < b.m_range.GetFrom();
        else
            return a.m_range.GetTo() > b.m_range.GetTo();
}

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

CAlignCollapser::CAlignCollapser() : m_count(0) {
    CArgs args = CNcbiApplication::Instance()->GetArgs();
    m_filtersr = args["filtersr"];
    m_filterest = args["filterest"];
    m_filtermrna = args["filtermrna"];
    m_filterprots = args["filterprots"];
    m_collapsest = args["collapsest"];
    m_collapssr = args["collapssr"];
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

bool isGoodIntron(int a, int b, int strand, CAlignCollapser::TAlignIntrons& introns) {
    SIntron intron_oriented_nosig(a, b, strand, true, "");
    SIntron intron_notoriented_nosig(a, b, ePlus, false, "");
    return (introns.find(intron_oriented_nosig) != introns.end() || introns.find(intron_notoriented_nosig) != introns.end());
}


#define CUT_MARGIN 15
bool RemoveNotSupportedIntronsFromProts(CAlignModel& align, CAlignCollapser::TAlignIntrons& introns) {

    CAlignMap amap = align.GetAlignMap();

    bool keepdoing = true;
    while(keepdoing) {
        keepdoing = false;
        for (int k = 1; k < (int)align.Exons().size(); ++k) {
            CModelExon exonl = align.Exons()[k-1];
            CModelExon exonr = align.Exons()[k];
            if(!(exonl.m_ssplice && exonr.m_fsplice) || isGoodIntron(exonl.GetTo(), exonr.GetFrom(), align.Strand(), introns))
                continue;

            TSignedSeqRange segl;
            if(exonl.GetTo()-CUT_MARGIN > align.Limits().GetFrom())
                segl = amap.ShrinkToRealPoints(TSignedSeqRange(align.Limits().GetFrom(),exonl.GetTo()-CUT_MARGIN), true);

            TSignedSeqRange segr;
            if(exonr.GetFrom()+CUT_MARGIN < align.Limits().GetTo())
                segr = amap.ShrinkToRealPoints(TSignedSeqRange(exonr.GetFrom()+CUT_MARGIN,align.Limits().GetTo()), true);

            if(segl.Empty()) {
                if(segr.Empty()) {
                    return false;
                } else {
                    align.Clip(segr,CGeneModel::eRemoveExons);
                    keepdoing = true;
                    break;                    
                }
            } else if(segr.Empty()) {
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

bool RemoveNotSupportedIntronsFromTranscripts(CAlignModel& align, CAlignCollapser::TAlignIntrons& introns) {

    CAlignMap amap = align.GetAlignMap();

    CAlignModel newalign;
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
            CModelExon exonl = align.Exons()[k-1];
            CModelExon exonr = align.Exons()[k];
            if(isGoodIntron(exonl.GetTo(), exonr.GetFrom(), a.Strand(), introns))
                break;
            else
                new_left = exonr.GetFrom();
        }
        int new_right = a.Limits().GetTo();
        for(int k = (int)a.Exons().size()-1; k > 0 && a.Exons()[k-1].GetTo() > new_left; --k) {
            CModelExon exonl = a.Exons()[k-1];
            CModelExon exonr = a.Exons()[k];
            if(isGoodIntron(exonl.GetTo(), exonr.GetFrom(), a.Strand(), introns))
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

        if(newalign.Limits().Empty()) {
            newalign = a;
        } else {
            newalign.AddHole();
            ITERATE(CGeneModel::TExons, e, a.Exons())
                newalign.AddExon(e->Limits(), e->m_fsplice_sig, e->m_ssplice_sig, e->m_ident);

            copy(a.FrameShifts().begin(),a.FrameShifts().end(), back_inserter(newalign.FrameShifts()));
        }

        piece_begin = piece_end;
    }

    bool good_alignment = true;
    if((align.Type()&CGeneModel::eEST) && (int)newalign.Exons().size() == 1 && newalign.Limits() != align.Limits())
        good_alignment = false;

    align = newalign;

    for (int k = 1; k < (int)align.Exons().size() && good_alignment; ++k) {
        CModelExon exonl = align.Exons()[k-1];
        CModelExon exonr = align.Exons()[k];
        if(exonl.m_ssplice && exonr.m_fsplice && !isGoodIntron(exonl.GetTo(), exonr.GetFrom(), align.Strand(), introns))
            good_alignment = false;
    }

    return good_alignment;
}


void CAlignCollapser::FilterAlignments() {

    if(!m_filtersr && !m_filterest && !m_filtermrna && !m_filterprots)
        return;

    CArgs args = CNcbiApplication::Instance()->GetArgs();

    int left_end = numeric_limits<int>::max();
    int right_end = 0;
    ITERATE(TAlignIntrons, it, m_align_introns) {
        const SIntron& intron = it->first;
        int a = intron.m_range.GetFrom();
        int b = intron.m_range.GetTo();
        left_end = min(left_end, a);
        right_end = max(right_end, b);
    }
    ITERATE(Tdata, i, m_aligns) {
        ITERATE(deque<SAlignIndividual>, k, i->second) {
            left_end = min(left_end, k->m_range.GetFrom());
            right_end = max(right_end, k->m_range.GetTo());
        }
    }
    int len = right_end-left_end+1;

    cerr << "Before filtering: " << m_align_introns.size() << " introns, " << m_count << " alignments" << endl;



#define COVERAGE_WINDOW 20    

    //coverage calculation
    vector<double> coverage(len,0.);
    ITERATE(Tdata, i, m_aligns) {
        ITERATE(deque<SAlignIndividual>, k, i->second) {
            if(i->first.isSR()) {
                float weight = k->m_weight;
                TSignedSeqRange range = k->m_range;
                for(int l = range.GetFrom(); l <= range.GetTo(); ++l)  // add coverage for all alignmnet range 
                    coverage[l-left_end] += weight;
                ITERATE(CAlignCommon::Tintrons, in, i->first.GetIntrons()) {   // substract intron ranges
                    for(int l = in->m_range.GetFrom()+1; l <= in->m_range.GetTo()-1; ++l) 
                        coverage[l-left_end] -= weight;
                }
            }
        }
    }
    vector<double> left_coverage(len,0.);   // average from the left side (including point)
    double wsum = 0;
    for(int i = 0; i < len; ++i) {
        wsum += coverage[i];
        int ipast = i - COVERAGE_WINDOW;
        if(ipast >= 0)
            wsum -= coverage[ipast];
        left_coverage[i] = wsum/COVERAGE_WINDOW;
    }
    vector<double> right_coverage(len,0.);   // average from the right side (including point)
    wsum = 0;
    for(int i = len-1; i >= 0; --i) {
        wsum += coverage[i];
        int ipast = i + COVERAGE_WINDOW;
        if(ipast < len)
            wsum -= coverage[ipast];
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

        if(id.m_est_support >= minest)
            id.m_keep_anyway = true;

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
            if(!id.m_keep_anyway && (id.m_weight < minintronexpression*mean || weight < minspliceexpression*left_coverage[splice-left_end])) {
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
            if(!id.m_keep_anyway && (id.m_weight < minintronexpression*mean || weight < minspliceexpression*right_coverage[splice-left_end])) {
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
        int new_left = left_end;
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
    vector<int> right_plus(len,left_end);       // closest right + strand splice 'on the left' from the current position
    vector<int> right_minus(len,left_end);      // closest right - strand splice 'on the left' from the current position
    ITERATE(TAlignIntrons, it, m_align_introns) {
        const SIntron& intron = it->first;
        int a = intron.m_range.GetFrom();
        int b = intron.m_range.GetTo();

        double two_side_exon_coverage = max(left_coverage[a-left_end],right_coverage[b-left_end]);

        //        if(right_coverage[a+1-left_end] < mincrossexpression*left_coverage[a-left_end]) {
        if(right_coverage[a+1-left_end] < mincrossexpression*two_side_exon_coverage) {
            if(!intron.m_oriented || intron.m_strand == ePlus)
                left_plus[a-left_end] = a;
            if(!intron.m_oriented || intron.m_strand == eMinus)
                left_minus[a-left_end] = a;
        }

        //        if(left_coverage[b-1-left_end] < mincrossexpression*right_coverage[b-left_end]) {
        if(left_coverage[b-1-left_end] < mincrossexpression*two_side_exon_coverage) {
            if(!intron.m_oriented || intron.m_strand == ePlus)
                right_plus[b-left_end] = b;
            if(!intron.m_oriented || intron.m_strand == eMinus)
                right_minus[b-left_end] = b;
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
                        if((coverage[a-left_end] < minsingleexpression || coverage[b-left_end] < minsingleexpression) && !alc.isPolyA() && !alc.isCap())
                            i->m_weight = -1;
                        else if((alc.isUnknown() || alc.isPlus()) && ((right_plus[b-left_end] > a && !alc.isCap()) || (left_plus[a-left_end] < b && !alc.isPolyA())))
                            i->m_weight = -1;
                        else if((alc.isUnknown() || alc.isMinus()) && ((right_minus[b-left_end] > a && !alc.isPolyA()) || (left_minus[a-left_end] < b && !alc.isCap())))
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
                        if((alc.isUnknown() || alc.isPlus()) && (right_plus[b-left_end] > a && !alc.isCap())) // crosses right plus splice      
                            new_lim.SetFrom(right_plus[b-left_end]);
                        if((alc.isUnknown() || alc.isMinus()) && (right_minus[b-left_end] > a && !alc.isPolyA())) // crosses right minus splice 
                            new_lim.SetFrom(right_minus[b-left_end]);
                        _ASSERT(new_lim.GetFrom() <= align.Exons().front().GetTo());
                    }
                    if(align.Exons().back().Limits().GetLength() > trim) {
                        int a = align.Exons().back().Limits().GetFrom();
                        int b = align.Exons().back().Limits().GetTo()-trim;
                        if((alc.isUnknown() || alc.isPlus()) && (left_plus[a-left_end] < b && !alc.isPolyA())) // crosses left plus splice  
                            new_lim.SetTo(left_plus[a-left_end]);
                        if((alc.isUnknown() || alc.isMinus()) && (left_minus[a-left_end] < b && !alc.isCap())) // crosses left minus splice 
                            new_lim.SetTo(left_minus[a-left_end]);
                        _ASSERT(new_lim.GetTo() >= align.Exons().back().GetFrom());
                    }
                    i->m_range = new_lim;

                    //delete if retained intron in internal exon    
                    for(int n = 1; n < (int)align.Exons().size()-1 && i->m_weight > 0; ++n) {
                        int a = align.Exons()[n].Limits().GetFrom();
                        int b = align.Exons()[n].Limits().GetTo();

                        pair<TIntronsBySplice::iterator,TIntronsBySplice::iterator> eqr(introns_by_right_splice.end(),introns_by_right_splice.end());
                        if((alc.isUnknown() || alc.isPlus()) && right_plus[b-left_end] > a)         // crosses right plus splice    
                            eqr = introns_by_right_splice.equal_range(right_plus[b-left_end]);
                        else if((alc.isUnknown() || alc.isMinus()) && right_minus[b-left_end] > a)  // crosses right minus splice   
                            eqr = introns_by_right_splice.equal_range(right_minus[b-left_end]);
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
    for(TAlignModelList::iterator it = m_aligns_for_filtering_only.begin(); it != m_aligns_for_filtering_only.end(); ) {
        TAlignModelList::iterator i = it++;
        CAlignModel& align = *i;
        const CAlignMap& amap = align.GetAlignMap();

        if((align.Type()&CGeneModel::eEST) && !m_filterest)
            continue;
        if((align.Type()&CGeneModel::emRNA) && !m_filtermrna)
            continue;
        if((align.Type()&CGeneModel::eProt) && !m_filterprots)
            continue;

        if((align.Type()&(CGeneModel::emRNA|CGeneModel::eProt)) && !AlignmentIsSupportedBySR(align, coverage, minsingleexpression, left_end)) {
            cerr << "Not supported " << align.Type() << ' ' << align.TargetAccession() << ' ' << align.ID() << endl;
            continue;
        }

        //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        for(int k = 1; k < (int)align.Exons().size(); ++k) {
            if(!align.Exons()[k-1].m_ssplice || !align.Exons()[k].m_fsplice)
                continue;

            if(!isGoodIntron(align.Exons()[k-1].GetTo(), align.Exons()[k].GetFrom(), align.Strand(), m_align_introns)) {
                cerr << "Bad intron " << align.Type() << ' ' << align.TargetAccession() << ' ' << align.ID() << ' ' << align.Exons()[k-1].GetTo()+1 << ':' << align.Exons()[k].GetFrom()+1 << endl;
            }
        }


        bool good_alignment = true;

        //clip alignmnets with bad introns
        if(align.Type()&CAlignModel::eProt) 
            good_alignment = RemoveNotSupportedIntronsFromProts(align, m_align_introns);
        else
            good_alignment = RemoveNotSupportedIntronsFromTranscripts(align, m_align_introns);

        if(!good_alignment) {
            m_aligns_for_filtering_only.erase(i);
            continue;
        }

        //delete if retained intron in internal exon    
        for(int n = 1; n < (int)align.Exons().size()-1; ++n) {
            if(!align.Exons()[n].m_fsplice || !align.Exons()[n].m_ssplice)
                continue;

            int l = align.Exons()[n].Limits().GetFrom();
            int r = align.Exons()[n].Limits().GetTo();

            pair<TIntronsBySplice::iterator,TIntronsBySplice::iterator> eqr(introns_by_right_splice.end(),introns_by_right_splice.end());
            if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) && right_plus[r-left_end] > l) // crosses right plus splice    
                eqr = introns_by_right_splice.equal_range(right_plus[r-left_end]);
            if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == eMinus) && right_minus[r-left_end] > l) // crosses right minus splice 
                eqr = introns_by_right_splice.equal_range(right_minus[r-left_end]);
            for(TIntronsBySplice::iterator ip = eqr.first; ip != eqr.second; ++ip) {
                if(ip->second->first.m_range.GetFrom() > l)
                    good_alignment = false;
            }
        }

        if(!good_alignment) {
            m_aligns_for_filtering_only.erase(i);
            continue;
        }

        //trim 3'/5' exons crossing splices (including hole boundaries)
        bool snap_to_codons = align.Type()&CAlignModel::eProt;      
        ITERATE(CGeneModel::TExons, e, align.Exons()) {
            if(!e->m_fsplice && e->Limits().GetLength() > trim && 
               (e != align.Exons().begin() || (align.Strand() == ePlus && !(align.Status()&CGeneModel::eCap)) || (align.Strand() == eMinus && !(align.Status()&CGeneModel::ePolyA)))) {
                int l = e->GetFrom();
                int r = e->GetTo();
                int new_l = l;
                if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) && right_plus[r-left_end] > l+trim) // crosses right plus splice            
                    new_l = right_plus[r-left_end];
                if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == eMinus) && right_minus[r-left_end] > l+trim) // crosses right minus splice         
                    new_l = right_minus[r-left_end];
                if(new_l != l) {
                    _ASSERT(new_l <= r);
                    if((align.Type()&CGeneModel::eEST) && (int)align.Exons().size() == 1)
                        good_alignment = false;

                    TSignedSeqRange seg = amap.ShrinkToRealPoints(TSignedSeqRange(new_l,r),snap_to_codons);
                    if(seg.NotEmpty()) 
                        align.Clip(TSignedSeqRange(seg.GetFrom(),align.Limits().GetTo()), CAlignModel::eDontRemoveExons);
                }
            }

            if(!e->m_ssplice && e->Limits().GetLength() > trim && 
               (e != align.Exons().end()-1 || (align.Strand() == ePlus && !(align.Status()&CGeneModel::ePolyA)) || (align.Strand() == eMinus && !(align.Status()&CGeneModel::eCap)))) {
                int l = e->GetFrom();
                int r = e->GetTo();
                int new_r = r;
                if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == ePlus) && left_plus[l-left_end] < r-trim) // crosses left plus splice          
                    new_r = left_plus[l-left_end];
                if(((align.Status()&CGeneModel::eUnknownOrientation) || align.Strand() == eMinus) && left_minus[l-left_end] < r-trim) // crosses left minus splice           
                    new_r = left_minus[l-left_end];
                if(new_r != r) {
                    _ASSERT(new_r >= l);
                    if((align.Type()&CGeneModel::eEST) && (int)align.Exons().size() == 1)
                        good_alignment = false;

                    TSignedSeqRange seg = amap.ShrinkToRealPoints(TSignedSeqRange(l,new_r),snap_to_codons);
                    if(seg.NotEmpty()) 
                        align.Clip(TSignedSeqRange(align.Limits().GetFrom(),seg.GetTo()), CAlignModel::eDontRemoveExons);
                }
            }
        }
    }

    total += m_aligns_for_filtering_only.size();
    

    cerr << "After filtering: " << m_align_introns.size() << " introns, " << total << " alignments" << endl;
}

void CAlignCollapser::GetCollapsedAlgnments(TAlignModelClusterSet& clsset) {

    cerr << "Added " << m_count << " alignments to collapser" << endl;

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
                clsset.Insert(align);
                ++total;
            }
        } else {
            sort(alideque.begin(),alideque.end(),LeftAndLongFirstOrder);

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
                        clsset.Insert(align);
                        extended_aligns.erase(ita);
                        ++total;
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
                clsset.Insert(align);
                ++total;
            }
        }
    }

    ITERATE(TAlignModelList, i, m_aligns_for_filtering_only) {
        clsset.Insert(*i);
        ++total;
    }
    
    cerr << "After collapsing: " << total << " alignments" << endl;
}

#define COLLAPS_CHUNK 500000
void CAlignCollapser::AddAlignment(const CAlignModel& a) {

    if((a.Type()&CGeneModel::eSR) && !a.Continuous())   // ignore SR with internal gaps
        return;

    CAlignModel align(a);
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
            sort(alideque.begin(),alideque.end(),LeftAndLongFirstOrder);
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


