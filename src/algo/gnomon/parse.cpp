/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software / database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software / database is freely available
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
#include "gene_finder.hpp"

BEGIN_NCBI_SCOPE

template<class T> inline void PushState(ParseVec<T>* vecp, int strand, int point)
{
    for (int phase = 0;  phase < 3;  ++phase) {
        vecp[phase].push_back(T(strand,phase,point));
        ++vecp[phase].num;
    }
}


template<class T> inline void PushState(ParseVec<T>& vec, int strand, int point)
{
    vec.push_back(T(strand,point));
    ++vec.num;
}


inline void PushState(ParseVec<LastExon>& vec, int strand, int point)
{
    vec.push_back(LastExon(strand,2,point));
    ++vec.num;
}


inline void PushState(ParseVec<FirstExon>& vec, int strand, int point)
{
    vec.push_back(FirstExon(strand,0,point));
    ++vec.num;
}


template<class L, class R> inline 
bool TooFar(const L& left, const R& right, double score) 
{ 
    if (left.MScore() == BadScore) {
        return true;
    }
    int len = right.Stop() - left.Stop();
    return (len > TooFarLen  &&  score + left.MScore() < right.Score());
}


struct RState
{
    RState(const HMM_State& l, const HMM_State& r)
    {
        lsp = r.LeftState();
        rsp = const_cast<HMM_State*>(&r);
        rsp->UpdateLeftState(l);
    }
    ~RState() { rsp->UpdateLeftState(*lsp); } 

    const HMM_State* lsp;
    HMM_State* rsp;
};

template<class Left, class Right> inline
bool EvaluateNewScore(const Left& left, const Right& right, double& rscore, 
                      bool& openrgn, bool& inalign)
{

    rscore = BadScore;

    RState rst(left,right);

    int len = right.Stop() - left.Stop();
    if (len > right.MaxLen()) {
        return false;
    }
    if ( !right.NoRightEnd()  &&  len < right.MinLen() ) {
        return true;
    }

    double scr, score = 0;
    if (left.isPlus()) {
        scr = left.BranchScore(right);
        if (scr == BadScore) {
            return true;
        }
    } else {
        scr = right.BranchScore(left);
        if (scr == BadScore) {
            return true;
        }
        scr += right.DenScore() - left.DenScore();
    }
    score += scr;

    // this check is frame - dependent and MUST be after BranchScore
    if (right.StopInside()) {
        return false;
    }     

    if (right.NoRightEnd()) {
        scr = right.ClosingLengthScore();
    } else {
        scr = right.LengthScore();
    }
    if (scr == BadScore) {
        return true;
    }
    score += scr;

    scr = right.RgnScore();
    if (scr == BadScore) {
        return true;
    }
    score += scr;

    if ( !right.NoRightEnd() ) {
        scr = right.TermScore();
        if (scr == BadScore) {
            return true;
        }
        score += scr;
    }

    openrgn = right.OpenRgn();
    inalign = right.InAlignment();
    rscore = score;
    return true;
}


class RightIsExon {};
class RightIsNotExon {};

template<class L, class R> // template for all right exons
inline bool ForwardStep(const L& left, R& right, RightIsExon rie)
{
    double score;
    bool openrgn, inalign;
    if ( !EvaluateNewScore(left,right,score,openrgn,inalign) ) {
        return false;
    } else if (score == BadScore) {
        return true;
    }

    if (left.Score() != BadScore  &&  openrgn  &&  !inalign) {
        double scr = score + left.Score();
        if (scr > right.Score()) {
            right.UpdateLeftState(left);
            right.UpdateScore(scr);
        }
    }

    if ( !openrgn ) {
        return false;
    }

    return true;
}


template<class L, class R> // template for all right non - exons
inline bool ForwardStep(const L& left, R& right, RightIsNotExon rine)
{
    double score;
    bool openrgn, inalign;
    if ( !EvaluateNewScore(left,right,score,openrgn,inalign) ) {
        return false;
    } else if (score == BadScore) {
        return true;
    }

    if (left.Score() != BadScore  &&  openrgn  &&  !inalign) {
        double scr = score + left.Score();
        if (scr > right.Score()) {
            right.UpdateLeftState(left);
            right.UpdateScore(scr);
        }
    }

    if ( !openrgn  ||  TooFar(left, right, score) ) {
        return false;
    }

    return true;
}


template<class L, class R> // template for all right exons
inline void MakeStep(ParseVec<L>& lvec, ParseVec<R>& rvec, RightIsExon rie)
{
    if (lvec.num < 0) {
        return;
    }
    int i = lvec.num;
    if (lvec[i].Stop() == rvec[rvec.num].Stop()) {
        --i;
    }
    while (i >= 0  &&  ForwardStep(lvec[i--],rvec[rvec.num],rie)) {
    }

    if (rvec.num > 0) {
        rvec[rvec.num].UpdatePrevExon(rvec[rvec.num - 1]);
    }
}


template<class L, class R> // template for all right non - exons
inline void MakeStep(ParseVec<L>& lvec, ParseVec<R>& rvec, RightIsNotExon rine)
{
    if (lvec.num < 0) {
        return;
    }
    R& right = rvec[rvec.num];
    int i = lvec.num;
    int rlimit = right.Stop();
    if (lvec[i].Stop() == rlimit) {
        --i;
    }
    int nearlimit = max(0,rlimit - TooFarLen);
    while (i >= 0  &&  lvec[i].Stop() >= nearlimit) {
        if ( !ForwardStep(lvec[i--],right,rine) ) {
            return;
        }
    }
    if (i < 0) {
        return;
    }
    for (const L* p = &lvec[i];  p !=0;  p = p->PrevExon()) {
        if ( !ForwardStep(*p,right,rine) ) {
            return;
        }
    }
}

// R / L indicates that left / right parameter is a 3 - element array

template<class L, class R, class D> 
inline void MakeStepR(L& lvec, R rvec, D d)
{
    for (int phr = 0;  phr < 3;  ++phr) {
        MakeStep(lvec, rvec[phr], d);
    }
}


template<class L, class R, class D> 
inline void MakeStepL(L lvec, R& rvec, D d)
{
    for (int phl = 0;  phl < 3;  ++phl) {
        MakeStep(lvec[phl], rvec, d);
    }
}


template<class L, class R, class D> 
inline void MakeStepLR(L lvec, R rvec, D d)
{
    for (int phl = 0;  phl < 3;  ++phl) {
        for (int phr = 0;  phr < 3;  ++phr)
        {
            MakeStep(lvec[phl], rvec[phr], d);
        }
    }
}


template<class L, class R, class D> 
inline void MakeStepLR(L lvec, R rvec, D d, int shift)
{
    for (int phl = 0;  phl < 3;  ++phl) {
        int phr = (shift + phl)%3;
        MakeStep(lvec[phl], rvec[phr], d);
    }
}


Parse::Parse(const SeqScores& ss) : seqscr(ss)
{
    try {
        igplus.reserve (seqscr.StartNumber(Plus) + 1);
        igminus.reserve(seqscr.StopNumber (Minus) + 1);
        feminus.reserve(seqscr.StartNumber(Minus) + 1);
        leplus.reserve (seqscr.StopNumber (Plus) + 1);
        seplus.reserve (seqscr.StopNumber (Plus) + 1);
        seminus.reserve(seqscr.StartNumber(Minus) + 1);
        for (int phase = 0;  phase < 3;  ++phase) {
            inplus[phase ].reserve(seqscr.AcceptorNumber(Plus) + 1);
            inminus[phase].reserve(seqscr.DonorNumber   (Minus) + 1);
            feplus[phase ].reserve(seqscr.DonorNumber   (Plus) + 1);
            ieplus[phase ].reserve(seqscr.DonorNumber   (Plus) + 1);
            ieminus[phase].reserve(seqscr.AcceptorNumber(Minus) + 1);
            leminus[phase].reserve(seqscr.AcceptorNumber(Minus) + 1);
        }
    }
    catch(bad_alloc&) {
        NCBI_THROW(CGnomonException, eGenericError,
                   "Not enough memory in Parse");
    }

    int len = seqscr.SeqLen();

    RightIsExon rie;
    RightIsNotExon rine;

    for (int i = 0;  i < len;  ++i) {
        if (seqscr.AcceptorScore(i,Plus) != BadScore) {
            PushState (inplus,Plus,i);
            MakeStepLR(ieplus,inplus,rine,1);
            MakeStepLR(feplus,inplus,rine,1);
        }

        if (seqscr.AcceptorScore(i,Minus) != BadScore) {
            PushState (ieminus,Minus,i);
            PushState (leminus,Minus,i);
            MakeStepLR(inminus,ieminus,rie);
            MakeStepR(igminus,leminus,rie);
        }

        if (seqscr.DonorScore(i,Plus) != BadScore) {
            PushState (feplus,Plus,i);
            PushState (ieplus,Plus,i);
            MakeStepLR(inplus,ieplus,rie);
            MakeStepR(igplus,feplus,rie);
        }

        if (seqscr.DonorScore(i,Minus) != BadScore) {
            PushState (inminus,Minus,i);
            MakeStepLR(leminus,inminus,rine,0);
            MakeStepLR(ieminus,inminus,rine,0);
        }

        if (seqscr.StartScore(i,Plus) != BadScore) {
            PushState(igplus,Plus,i);
            MakeStep (seplus,igplus,rine);
            MakeStep (seminus,igplus,rine);
            MakeStep (leplus,igplus,rine);
            MakeStep (feminus,igplus,rine);
        }

        if (seqscr.StartScore(i,Minus) != BadScore) {
            PushState(feminus,Minus,i);
            PushState(seminus,Minus,i);
            MakeStepL(inminus,feminus,rie);
            MakeStep (igminus,seminus,rie);
        }

        if (seqscr.StopScore(i,Plus) != BadScore) {
            PushState(leplus,Plus,i);
            PushState(seplus,Plus,i);
            MakeStepL(inplus,leplus,rie);
            MakeStep (igplus,seplus,rie);
        }

        if (seqscr.StopScore(i,Minus) != BadScore) {
            PushState(igminus,Minus,i);
            MakeStep (seplus,igminus,rine);
            MakeStep (seminus,igminus,rine);
            MakeStep (leplus,igminus,rine);
            MakeStep (feminus,igminus,rine);
        }
    }

    PushState (inplus,Plus,-1);
    MakeStepLR(ieplus,inplus,rine,1);
    MakeStepLR(feplus,inplus,rine,1);

    PushState (ieminus,Minus,-1);
    MakeStepLR(inminus,ieminus,rie);

    PushState(leminus,Minus,-1);
    MakeStepR(igminus,leminus,rie);

    PushState (ieplus,Plus,-1);
    MakeStepLR(inplus,ieplus,rie);

    PushState(feplus,Plus,-1);
    MakeStepR(igplus,feplus,rie);

    PushState (inminus,Minus,-1);
    MakeStepLR(leminus,inminus,rine,0);
    MakeStepLR(ieminus,inminus,rine,0);

    PushState(igplus,Plus,-1);
    MakeStep (seplus,igplus,rine);
    MakeStep (seminus,igplus,rine);
    MakeStep (leplus,igplus,rine);
    MakeStep (feminus,igplus,rine);

    PushState(feminus,Minus,-1);
    MakeStepL(inminus,feminus,rie);

    PushState(seminus,Minus,-1);
    MakeStep (igminus,seminus,rie);

    PushState(leplus,Plus,-1);
    MakeStepL(inplus,leplus,rie);

    PushState(seplus,Plus,-1);
    MakeStep(igplus,seplus,rie);

    PushState(igminus,Minus,-1);
    MakeStep (seplus,igminus,rine);
    MakeStep (seminus,igminus,rine);
    MakeStep (leplus,igminus,rine);
    MakeStep (feminus,igminus,rine);

    igplus.back().UpdateScore(AddProbabilities(igplus.back().Score(),igminus.back().Score()));

    const HMM_State* p = &igplus.back();
    if (feminus.back().Score() > p->Score()) {
        p = &feminus.back();
    }
    if (leplus.back().Score() > p->Score()) {
        p = &leplus.back();
    }
    if (seplus.back().Score() > p->Score()) {
        p = &seplus.back();
    }
    if (seminus.back().Score() > p->Score()) {
        p = &seminus.back();
    }
    for (int ph = 0;  ph < 3;  ++ph) {
        if (inplus[ph].back().Score() > p->Score()) {
            p = &inplus[ph].back();
        }
        if (inminus[ph].back().Score() > p->Score()) {
            p = &inminus[ph].back();
        }
        if (ieplus[ph].back().Score() > p->Score()) {
            p = &ieplus[ph].back();
        }
        if (ieminus[ph].back().Score() > p->Score()) {
            p = &ieminus[ph].back();
        }
        if (leminus[ph].back().Score() > p->Score()) {
            p = &leminus[ph].back();
        }
        if (feplus[ph].back().Score() > p->Score()) {
            p = &feplus[ph].back();
        }
    }

    path = p;
}


template<class T> void Out(T t, int w, CNcbiOstream& to = cout)
{
    to.width(w);
    to.setf(IOS_BASE::right,IOS_BASE::adjustfield);
    to << t;
}


void Out(double t, int w, CNcbiOstream& to = cout, int prec = 1)
{
    to.width(w);
    to.setf(IOS_BASE::right,IOS_BASE::adjustfield);
    to.setf(IOS_BASE::fixed,IOS_BASE::floatfield);
    to.precision(prec);

    if (t > 1000000000) {
        to << "+Inf";
    } else if (t < -1000000000) {
        to << "-Inf";
    } else {
        to << t;
    }
}


inline char toACGT(int c)
{
    switch (c) {
    case nA: 
        return 'A';
    case nC: 
        return 'C';
    case nG: 
        return 'G';
    case nT: 
        return 'T';
    case nN: 
        return 'N';
    }
}


int Parse::PrintGenes(CNcbiOstream& to, CNcbiOstream& toprot, bool complete) const
{
    enum {DNA_Align = 1, Prot_Align = 2 };

    list<Gene> genes = GetGenes();

    int right = seqscr.SeqMap(seqscr.SeqLen() - 1,true);
    if (complete  &&  !genes.empty()  &&  !genes.back().RightComplete()) {
        int partial_start = genes.back().front().Start();
        genes.pop_back();

        if ( !genes.empty() ) // end of the last complete gene
        {
            right = genes.back().back().Stop();
        } else {
            if (partial_start > seqscr.SeqMap(0,true) + 1000) {
                right = partial_start - 100;
            } else {
                return -1;   // calling program MUST be aware of this!!!!!!!
            }
        }
    }

    to << "\n" << seqscr.Contig() << '_' << seqscr.SeqMap(0,true) << '_' << right << "\n\n";
    to << genes.size() << " genes found\n";

    set<int> chain_id, prot_id;
    for (list<Gene>::iterator it = genes.begin();  it != genes.end();  ++it) {
        const Gene& gene = *it;
        for (int i = 0;  i < gene.size();  ++i) {
            ITERATE(set<int>, iter, gene[i].ChainID()) {
                chain_id.insert(*iter);
            }

            ITERATE(set<int>, iter, gene[i].ProtID()) {
                prot_id.insert(*iter);
            }

            //chain_id.insert(gene[i].ChainID().begin(), gene[i].ChainID().end());
            //prot_id.insert(gene[i].ProtID().begin(),  gene[i].ProtID().end());
        }
    }
    to << (chain_id.size() + prot_id.size()) << " alignments used\n";

    int num = 0;
    for (list<Gene>::iterator it = genes.begin();  it != genes.end();  ++it) {
        ++num;
        to << "\n\n";
        Out(" ",11,to);
        Out("Start",10,to);
        Out("Stop",10,to);
        Out("Length",7,to);
        Out("Align",10,to);
        Out("Prot",12,to);
        Out("FShift",7,to);
        to << endl;

        const Gene& gene = *it;
        int gene_start = -1;
        int gene_stop;
        int align_type = 0;

        for (int i = 0;  i < gene.size();  ++i) {
            const ExonData& exon = gene[i];
            const TFrameShifts& fshifts = exon.ExonFrameShifts();
            int estart = exon.Start();
            if (gene_start < 0  &&  exon.Type() == ExonData::Cds) {
                gene_start = estart;
            }
            if ( !exon.ChainID().empty() ) {
                align_type = align_type|DNA_Align;
            }
            if ( !exon.ProtID().empty() ) {
                align_type = align_type|Prot_Align;
            }

            for (int k = 0;  k < fshifts.size();  ++k) {
                int estop = fshifts[k].Loc() - 1;

                Out(num,4,to);
                Out((exon.Type() == ExonData::Cds) ? "CDS" : "UTR",5,to); 
                Out((gene.Strand() == Plus) ? '+' : '-',2,to);
                Out(estart,10,to);
                Out(estop,10,to);
                Out(estop - estart + 1,7,to);
                if (exon.ChainID().empty()) {
                    Out("-",10,to);
                } else {
                    Out(*exon.ChainID().begin(),10,to);
                }
                if (exon.ProtID().empty()) {
                    Out("-",12,to);
                } else {
                    Out(*exon.ProtID().begin(),12,to);
                }
                if (fshifts[k].IsInsertion()) {
                    Out("+",7,to);
                    estart = estop;
                } else {
                    Out("-",7,to);
                    estart = estop + 2;
                }
                to << endl;
            }
            int estop = exon.Stop();
            Out(num,4,to);
            Out((exon.Type() == ExonData::Cds) ? "CDS" : "UTR",5,to); 
            Out((gene.Strand() == Plus) ? '+' : '-',2,to);
            Out(estart,10,to);
            Out(estop,10,to);
            Out(estop - estart + 1,7,to);
            if (exon.ChainID().empty()) {
                Out("-",10,to);
            } else {
                Out(*exon.ChainID().begin(),10,to);
            }
            if (exon.ProtID().empty()) {
                Out("-",12,to);
            } else {
                Out(*exon.ProtID().begin(),12,to);
            }
            Out("*",7,to);
            to << endl;
            if (exon.Type() == ExonData::Cds) {
                gene_stop = estop;
            }
        }   

        toprot << '>' << seqscr.Contig() << "_" 
            << gene_start << "_" 
            << gene_stop << "_"
            << ((gene.Strand() == Plus) ? "plus" : "minus") << "_"
            << align_type << endl;

        const IVec& cds = gene.CDS();
        int i;
        for (i = 0;  i < cds.size() - 2;  i +=3) {
            toprot << aa_table[cds[i]*25 + cds[i + 1]*5 + cds[i + 2]];
            if ((i + 3)%150 == 0) {
                toprot << endl;
            }
        }
        if (i%150 != 0) {
            toprot << endl;
        }

        //      for (i = 0;  i < cds.size();  ++i)
        //      {
        //          toprot << toACGT(cds[i]);
        //          if ((i + 3)%50 == 0) toprot << endl;
        //      }
        //      if (i%50 != 0) toprot << endl;
    }

    return right;
}


/*
int Parse::PrintGenes(CNcbiOstream& to, CNcbiOstream& toprot, bool complete) const
{
    const char* aa_table = "KNKNXTTTTTRSRSXIIMIXXXXXXQHQHXPPPPPRRRRRLLLLLXXXXXEDEDXAAAAAGGGGGVVVVVXXXXX * Y*YXSSSSS * CWCXLFLFXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

    const HMM_State* plast = Path();
    if (complete  &&  dynamic_cast<const Intron*>(plast)) {
        while (!dynamic_cast<const Intergenic*>(plast)) {
            plast = plast->LeftState();
        }

        if ( !plast->LeftState() ) {
            return plast->Start() + seqscr.SeqMap(0,true) - 1;
        } else {
            plast = plast->LeftState();
        }
    }

    const CClusterSet& algns = seqscr.Alignments();

    int right = seqscr.SeqMap(plast->Stop(),false);  // either end of chunk or and of last complete gene and its alignments
    for (CClusterSet::ConstIt it = algns.begin();  it != algns.end();  ++it) {
        if (it->Limits().Include(right)) {
            right = it->Limits().second;
        }
    }

    to << "\n" << seqscr.Contig() << '_' << seqscr.SeqMap(0,true) << '_' << right << "\n\n";

    vector<const Exon*> states;
    const Exon* pe;
    int gennum = 0;
    for (const HMM_State* p = plast;  p != 0;  p = p->LeftState()) {
        if (pe = dynamic_cast<const Exon*>(p)) {
            states.push_back(pe);
        }

        if ( !states.empty() ) {
            if ( !p->LeftState()  ||  dynamic_cast<const Intergenic*>(p) ) {
                ++gennum;
            }
        }
    }
    reverse(states.begin(),states.end());

    int algnum = 0, algused = 0;
    IPair chunk(seqscr.SeqMap(0,true),seqscr.SeqMap(seqscr.SeqLen() - 1,false));
    for (CClusterSet::ConstIt it = algns.begin();  it != algns.end();  ++it) {
        IPair lim = it->Limits();
        if (lim.Intersect(chunk)) {
            algnum += it->size();
        }
        for (int i = 0;  i < states.size();  ++i) {
            int a = seqscr.SeqMap(states[i]->Start(),true);
            int b = seqscr.SeqMap(states[i]->Stop(),false);
            IPair exon(a,b);
            if (exon.Intersect(lim)) {
                algused += it->size();
                break;
            }
        }
    }
    IVec ids(states.size(),0), prot_ids(states.size(),0);
    for (int i = 0;  i < states.size();  ++i) {
        int a = seqscr.SeqMap(states[i]->Start(),true);
        int b = seqscr.SeqMap(states[i]->Stop(),false);
        IPair exon(a,b);
        for (CClusterSet::ConstIt cls_it = algns.begin();  cls_it != algns.end();  ++cls_it) {
            for (CCluster::ConstIt it = cls_it->begin();  it != cls_it->end();  ++it)
            {
                const AlignVec& al = *it;
                for (int j = 0;  j < al.size();  ++j) {
                    if (exon.Intersect(al[j])) {
                        if (al.Type() == AlignVec::Prot) {
                            prot_ids[i] = al.ID();
                        } else {
                            ids[i] = al.ID();
                            if (exon.first != al[j].first  ||  exon.second != al[j].second) {
                                ids[i] = -ids[i];
                            }
                        }
                    }
                }
            }
        }
    }


    to << gennum << " genes found\n";
    to << algused << " alignments used out of " << algnum << endl;
    if (gennum == 0) {
        return seqscr.SeqMap(plast->Stop(),false);
    }

    IVec cds;
    int num = 0;
    double gene_fscore, left_score;
    int gene_start, gene_stop, align_type;
    enum {DNA_Align = 1, Prot_Align = 2 };
    for (int i = 0;  i < states.size();  ++i) {
        pe = states[i];
        int shift = 0;

        if (i == 0  ||  
            dynamic_cast<const SingleExon*>(pe)  ||  
            (pe->isPlus()  &&  dynamic_cast<const FirstExon*>(pe))  ||  
            (pe->isMinus()  &&  dynamic_cast<const LastExon*>(pe)))
        {
            gene_start = seqscr.SeqMap(pe->Start(),true);
            ++num;
            to << "\n\n";
            Out(" ",19,to);
            Out("Start",10,to);
            Out("Stop",10,to);
            Out("Length",7,to);
            Out("Align",10,to);
            Out("Prot",12,to);
            Out("FShift",7,to);
            to << endl;

            left_score = pe->LeftState()->Score();

            cds.clear();
            const Intron* pi = dynamic_cast<const Intron*>(pe->LeftState());
            if (pi != 0) {
                int ph = pi->Phase();
                if (pe->isPlus()) {
                    shift = (3 - ph)%3;
                } else {
                    shift = ph;
                }
            }

            align_type = 0;
        }

        //If first / last base is insertion, this information is lost
        vector<IPair> subexons;
        int a = seqscr.SeqMap(pe->Start(),true);
        int b = a;
        for (int k = pe->Start() + 1;  k <= pe->Stop();  ++k) {
            int c = seqscr.SeqMap(k,false);
            if (c == b) {
                subexons.push_back(IPair(a,b));
                a = b;
            }
            if (c == b + 2) {
                subexons.push_back(IPair(a,b));
                a = c;
            }
            b = c;
        }
        subexons.push_back(IPair(a,b));

        for (int k = 0;  k < subexons.size();  ++k) {
            int a = subexons[k].first;
            int b = subexons[k].second;
            Out(num,4,to);
            Out(pe->GetStateName(),13,to);
            Out(pe->isPlus() ? '+' : '-',2,to);

            Out(a,10,to);
            Out(b,10,to);
            Out(b - a+1,7,to);

            if (ids[i]) {
                align_type = align_type|DNA_Align;
                Out(abs(ids[i]),9,to);
                to << ((ids[i] > 0) ? ' ' : '*');
            } else {
                Out("-",9,to);
                to << ' ';
            }
            if (prot_ids[i]) {
                align_type = align_type|Prot_Align;
                Out(prot_ids[i],12,to);
            } else {
                Out("-",12,to);
            }

            if (k != 0) {
                if (a == subexons[k - 1].second) {
                    Out("+",7,to);
                } else if (a == subexons[k - 1].second + 2) {
                    Out("-",7,to);
                } else {
                    Out("*",7,to);
                }
            } else {
                Out("*",7,to);
            }

            to << endl;
        }

        bool last = false;

        if (i == states.size() - 1  ||  
            dynamic_cast<const SingleExon*>(pe)  ||  
            (pe->isPlus()  &&  dynamic_cast<const LastExon*>(pe))  ||  
            (pe->isMinus()  &&  dynamic_cast<const FirstExon*>(pe)))
        {
            last = true;
            gene_stop = seqscr.SeqMap(pe->Stop(),false);
        }

        copy(seqscr.SeqPtr(pe->Start() + shift,Plus),
             seqscr.SeqPtr(pe->Stop() + 1,Plus),back_inserter(cds));

        if (last) {
            to << endl;

            cds.resize(cds.size() - cds.size()%3);

            if (pe->isMinus()) {
                for (int i = 0;  i < cds.size();  ++i) cds[i] = toMinus[cds[i]];
                reverse(cds.begin(),cds.end());
            }

            toprot << '>' << seqscr.Contig() << "_" 
                << gene_start << "_" 
                << gene_stop << "_"
                << (pe->isPlus() ? "plus" : "minus") << "_"
                << align_type << endl;

            int i;
            for (i = 0;  i < cds.size() - 2;  i +=3) {
                toprot << aa_table[cds[i]*25 + cds[i + 1]*5 + cds[i + 2]];
                if ((i + 3)%150 == 0) {
                    toprot << endl;
                }
            }
            if (i%150 != 0) {
                toprot << endl;
            }
        }
    }

    return right;
}
*/


list<Gene> Parse::GetGenes() const
{
    list<Gene> genes;

    const CClusterSet& cls = seqscr.Alignments();

    if (dynamic_cast<const Intron*>(Path())) {
        genes.push_front(Gene(Path()->Strand(),false,false));
    }

    for (const HMM_State* p = Path();  p != 0;  p = p->LeftState()) {
        if (const Exon* pe = dynamic_cast<const Exon*>(p)) {
            bool stopcodon = false;
            bool startcodon = false;

            if (dynamic_cast<const SingleExon*>(pe)) {
                genes.push_front(Gene(pe->Strand(),true,true));
                startcodon = true;
                stopcodon = true;
            } else if (dynamic_cast<const LastExon*>(pe)) {
                if (pe->isPlus()) {
                    genes.push_front(Gene(pe->Strand(),false,true));
                } else {
                    genes.front().leftend = true;
                }
                stopcodon = true;
            } else if (dynamic_cast<const FirstExon*>(pe)) {
                if (pe->isMinus()) {
                    genes.push_front(Gene(pe->Strand(),false,true));
                } else {
                    genes.front().leftend = true;
                }
                startcodon = true;
            }

            Gene& curgen = genes.front();

            int estart = pe->Start();
            int estop = pe->Stop();

            if (pe->isPlus()) {
                int ph = (pe->Phase() - estop + estart)%3;
                if (ph < 0) {
                    ph += 3;
                }
                curgen.cds_shift = ph;
            } else if (curgen.empty()) {
                curgen.cds_shift = pe->Phase();
            }

            if (pe->isMinus()  &&  startcodon) {
                curgen.cds.push_back(nT);
                curgen.cds.push_back(nA);
                curgen.cds.push_back(nC);
            }
            for (int i = estop;  i >= estart;  --i) {
                curgen.cds.push_back(*seqscr.SeqPtr(i,Plus));
            }
            if (pe->isPlus()  &&  startcodon) {
                curgen.cds.push_back(nG);
                curgen.cds.push_back(nT);
                curgen.cds.push_back(nA);
            }

            int a = seqscr.SeqMap(estart,true);
            bool a_insertion = (estart != seqscr.RevSeqMap(a,true));
            int b = seqscr.SeqMap(estop,false);
            bool b_insertion = (estop != seqscr.RevSeqMap(b,true));

            pair<CClusterSet::ConstIt,CClusterSet::ConstIt> lim = cls.equal_range(CCluster(a,b));
            CClusterSet::ConstIt first = lim.first;
            CClusterSet::ConstIt last = lim.second;
            if (lim.first != lim.second) {
                --last;
            }

            int lastid = 0;
            int margin = numeric_limits<int>::min();   // difference between end of exon and end of alignment exon
            bool rightexon = false;
            if (((startcodon  &&  pe->isMinus())  ||  (stopcodon  &&  pe->isPlus()))  &&  lim.first != lim.second)   // rightside UTR
            {
                for (CCluster::ConstIt it = last->begin();  it != last->end();  ++it) {
                    const AlignVec& algn = *it;
                    if (algn.Type() == AlignVec::Prot) {
                        continue;
                    }

                    for (int i = algn.size() - 1;  i >= 0;  --i) {
                        if (b < algn[i].first) {
                            curgen.push_back(ExonData(algn[i].first,algn[i].second,ExonData::Utr));
                            curgen.back().chain_id.insert(algn.ID()); 
                            rightexon = true;
                        } else if (algn[i].second >= a  &&  algn[i].second - b > margin) {
                            margin = algn[i].second - b;
                            lastid = algn.ID();
                        }
                    }
                }
            }


            int remain = 0;
            if ((pe->isMinus()  &&  startcodon)  ||  (pe->isPlus()  &&  stopcodon))   // start / stop position correction
            {
                if (margin >= 3) {
                    b += 3;
                    margin -= 3;
                } else if (margin >= 0  &&  rightexon) {
                    b += margin;
                    remain = 3 - margin;
                    margin = 0;
                } else {
                    b += 3;
                    margin = 0;
                }
            }
            if (margin > 0) {
                curgen.push_back(ExonData(b + 1,b + margin,ExonData::Utr)); 
                curgen.back().chain_id.insert(lastid); 
            } else if (remain > 0) {
                ExonData& e = curgen.back();
                int aa = e.start;
                e.start += remain;
                int bb = aa + remain - 1;
                ExonData er(aa,bb,ExonData::Cds);
                er.chain_id = e.chain_id;
                curgen.push_back(er);
            }

            curgen.push_back(ExonData(a,b,ExonData::Cds));
            ExonData& exon = curgen.back();
            margin = numeric_limits<int>::min();
            bool leftexon = false;
            lastid = 0;
            for (CClusterSet::ConstIt cls_it = lim.first;  cls_it != lim.second;  ++cls_it) {
                for (CCluster::ConstIt it = cls_it->begin();  it != cls_it->end();  ++it)
                {
                    const AlignVec& algn = *it;

                    for (int i = 0;  i < algn.size();  ++i) {
                        if (exon.stop < algn[i].first) {
                            break;
                        } else if (algn[i].second < exon.start) {
                            leftexon = true;
                            continue;
                        } else if (algn.Type() == AlignVec::Prot) {
                            exon.prot_id.insert(algn.ID()); 
                        } else {
                            exon.chain_id.insert(algn.ID());
                            if (a - algn[i].first > margin) {
                                margin = a - algn[i].first;
                                lastid = algn.ID();
                            }
                        }
                    }
                }
            }

            const TFrameShifts& fs = seqscr.SeqFrameShifts();
            for (int i = 0;  i < fs.size();  ++i) {
                if ((fs[i].Loc() == a  &&  a_insertion)  ||  // exon starts from insertion
                    (fs[i].Loc() == b + 1  &&  b_insertion)  ||  // exon ends by insertion
                    (fs[i].Loc() <= b  &&  fs[i].Loc() > a)) {
                    exon.fshifts.push_back(fs[i]);
                }
            }

            remain = 0;
            if ((pe->isPlus()  &&  startcodon)  ||  (pe->isMinus()  &&  stopcodon))   // start / stop position correction
            {
                if (margin >= 3) {
                    exon.start -= 3;
                    margin -= 3;
                } else if (margin >= 0  &&  leftexon) {
                    exon.start -= margin;
                    remain = 3 - margin;
                    margin = 0;
                } else {
                    exon.start -= 3;
                    margin = 0;
                }
            }

            if (((startcodon  &&  pe->isPlus())  ||  (stopcodon  &&  pe->isMinus()))  &&  lim.first != lim.second)   // lefttside UTR
            {
                int estart = exon.start;

                if (margin > 0) {
                    curgen.push_back(ExonData(estart - margin,estart - 1,ExonData::Utr)); 
                    curgen.back().chain_id.insert(lastid); 
                }

                for (CCluster::ConstIt it = first->begin();  it != first->end();  ++it) {
                    const AlignVec& algn = *it;
                    if (algn.Type() == AlignVec::Prot) {
                        continue;
                    }

                    for (int i = algn.size() - 1;  i >= 0;  --i) {
                        if (estart > algn[i].second) {
                            int aa = algn[i].first;
                            int bb = algn[i].second;
                            if (remain > 0) {
                                curgen.push_back(ExonData(bb - remain + 1,bb,ExonData::Cds));
                                curgen.back().chain_id.insert(algn.ID()); 
                                bb -= remain;
                                remain = 0;
                            }
                            curgen.push_back(ExonData(aa,bb,ExonData::Utr)); 
                            curgen.back().chain_id.insert(algn.ID()); 
                        }
                    }
                }
            }
        }
    }

    for (list<Gene>::iterator it = genes.begin();  it != genes.end();  ++it) {
        Gene& g = *it;
        reverse(g.begin(),g.end());

        if (g.Strand() == Plus) {
            reverse(g.cds.begin(),g.cds.end());
        } else {
            for (int i = 0;  i < g.cds.size();  ++i) {
                g.cds[i] = toMinus[g.cds[i]];
            }
        }

    }

    return genes;
}


void Parse::PrintInfo() const
{
    vector<const HMM_State*> states;
    for (const HMM_State* p = Path();  p != 0;  p = p->LeftState()) states.push_back(p);
    reverse(states.begin(),states.end());

    Out(" ",15);
    Out("Start",8);
    Out("Stop",8);
    Out("Score",8);
    Out("BrScr",8);
    Out("LnScr",8);
    Out("RgnScr",8);
    Out("TrmScr",8);
    Out("AccScr",8);
    cout << endl;

    for (int i = 0;  i < states.size();  ++i) {
        const HMM_State* p = states[i];
        if (dynamic_cast<const Intergenic*>(p)) {
            cout << endl;
        }

        Out(p->GetStateName(),13);
        Out(p->isPlus() ? '+' : '-',2);
        int a = seqscr.SeqMap(p->Start(),true);
        int b = seqscr.SeqMap(p->Stop(),false);
        Out(a,8);
        Out(b,8);
        StateScores sc = p->GetStateScores();
        Out(sc.score,8);
        Out(sc.branch,8);
        Out(sc.length,8);
        Out(sc.region,8);
        Out(sc.term,8);
        Out(p->Score(),8);
        cout << endl;
    }
}


void LogicalCheck(const HMM_State& st, const SeqScores& ss)
{
    typedef const Intergenic* Ig;
    typedef const Intron* I;
    typedef const FirstExon* FE;
    typedef const InternalExon* IE;
    typedef const LastExon* LE;
    typedef const SingleExon* SE;
    typedef const Exon* E;

    bool out;
    int phase,strand;
    vector< pair<int,int> > exons;
    for (const HMM_State* state = &st;  state != 0;  state = state->LeftState()) {
        const HMM_State* left = state->LeftState();
        if (left == 0) {
            if ( !dynamic_cast<Ig>(state)  &&  !dynamic_cast<I>(state) ) {
                NCBI_THROW(CGnomonException, eGenericError,
                           "GNOMON: Logical error 1");
            }

            out = true;
        } else if (dynamic_cast<Ig>(state)) {
            if ( !dynamic_cast<SE>(left)  &&  
                 !(dynamic_cast<LE>(left)  &&  left->isPlus())  &&  
                 !(dynamic_cast<FE>(left)  &&  left->isMinus()) ) {
                NCBI_THROW(CGnomonException, eGenericError,
                           "GNOMON: Logical error 2");
            }

            out = true;
        } else if (I ps = dynamic_cast<I>(state)) {
            E p;
            int ph = -1;
            if (((p = dynamic_cast<FE>(left))  &&  left->isPlus())  ||  
                ((p = dynamic_cast<LE>(left))  &&  left->isMinus())  ||  
                (p = dynamic_cast<IE>(left))) {
                ph = p->Phase();
            }

            if ((ph < 0)  ||  (ps->Strand() != left->Strand())  ||  
                (ps->isPlus()  &&  (ph + 1)%3 != ps->Phase())  ||  
                (ps->isMinus()  &&  ph != ps->Phase())) {
                NCBI_THROW(CGnomonException, eGenericError,
                           "GNOMON: Logical error 3");
            }

            out = false;
        } else if (SE ps = dynamic_cast<SE>(state)) {
            if ( !(dynamic_cast<Ig>(left)  &&  ps->Strand() == left->Strand()) ) {
                NCBI_THROW(CGnomonException, eGenericError,
                           "GNOMON: Logical error 4");
            }

            int a = ps->Start(), b = ps->Stop();
            if ((b - a+1)%3  ||  
                (ps->isPlus()  &&  (!ss.isStart(a,Plus)  ||  !ss.isStop(b + 1,Plus)))  ||  
                (ps->isMinus()  &&  (!ss.isStart(b,Minus)  ||  !ss.isStop(a - 1,Minus))))  {
                NCBI_THROW(CGnomonException, eGenericError,
                           "GNOMON: Logical error 5");
            }
        } else if (FE ps = dynamic_cast<FE>(state)) {
            I p;
            if ((ps->Strand() != left->Strand())  ||  
                (!(dynamic_cast<Ig>(left)  &&  ps->isPlus())  &&  
                 !((p = dynamic_cast<I>(left))  &&  ps->isMinus()))) {
                NCBI_THROW(CGnomonException, eGenericError,
                           "GNOMON: Logical error 6");
            }

            int a = ps->Start(), b = ps->Stop();
            if (ps->isPlus()) {
                if (ps->Phase() != (b - a)%3) {
                    NCBI_THROW(CGnomonException, eGenericError,
                               "GNOMON: Logical error 7");
                }
            } else {
                if (p->Phase() != (b + 1-a)%3) {
                    NCBI_THROW(CGnomonException, eGenericError,
                               "GNOMON: Logical error 8");
                }
            }

            //          if ((ps->isPlus()  &&  (!ss.isStart(a,Plus)  ||  !ss.isGT(b + 1,Plus)))  ||  
            //             (ps->isMinus()  &&  (!ss.isStart(b,Minus)  ||  !ss.isGT(a - 1,Minus)))) 
            //          { cout << "Logical error9\n"; exit(1); }
        } else if (IE ps = dynamic_cast<IE>(state)) {
            I p;
            if ((ps->Strand() != left->Strand())  ||  
                !(p = dynamic_cast<I>(left))) {
                NCBI_THROW(CGnomonException, eGenericError,
                           "GNOMON: Logical error 10");
            }

            int a = ps->Start(), b = ps->Stop();
            if (ps->isPlus()) {
                if (ps->Phase() != (p->Phase() + b-a)%3) {
                    NCBI_THROW(CGnomonException, eGenericError,
                               "GNOMON: Logical error 11");
                }
            } else {
                if (p->Phase() != (ps->Phase() + b+1 - a)%3) {
                    NCBI_THROW(CGnomonException, eGenericError,
                               "GNOMON: Logical error 12");
                }
            }

            //          if ((ps->isPlus()  &&  (!ss.isAG(a - 1,Plus)  ||  !ss.isGT(b + 1,Plus)))  ||  
            //             (ps->isMinus()  &&  (!ss.isAG(b + 1,Minus)  ||  !ss.isGT(a - 1,Minus)))) 
            //          { cout << "Logical error13\n"; exit(1); }
        } else if (LE ps = dynamic_cast<LE>(state)) {
            I p;
            if ((ps->Strand() != left->Strand())  ||  
                (!(dynamic_cast<Ig>(left)  &&  ps->isMinus())  &&  
                 !((p = dynamic_cast<I>(left))  &&  ps->isPlus()))) {
                NCBI_THROW(CGnomonException, eGenericError,
                           "GNOMON: Logical error 14");
            }

            int a = ps->Start(), b = ps->Stop();
            if (ps->isPlus()) {
                if ((p->Phase() + b - a) % 3 != 2) {
                    NCBI_THROW(CGnomonException, eGenericError,
                               "GNOMON: Logical error 15");
                }
            } else {
                if ((ps->Phase() + b-a)%3 != 2) {
                    NCBI_THROW(CGnomonException, eGenericError,
                               "GNOMON: Logical error 16");
                }
            }

            //          if ((ps->isPlus()  &&  (!ss.isAG(a - 1,Plus)  ||  !ss.isStop(b + 1,Plus)))  ||  
            //             (ps->isMinus()  &&  (!ss.isStop(a - 1,Minus)  ||  !ss.isAG(b + 1,Minus)))) 
            //          { cout << "Logical error17\n"; exit(1); }
        }

        if (E ps = dynamic_cast<E>(state)) {
            phase = ps->Phase();
            strand = ps->Strand();
            exons.push_back(make_pair(ps->Start(),ps->Stop()));
            out = false;
        }

        if (out  &&  !exons.empty()) {
            IVec seq;

            if (strand == Plus) {
                for (int i = exons.size() - 1;  i >= 0;  --i) {
                    int a = exons[i].first;
                    int b = exons[i].second;
                    seq.insert(seq.end(),ss.SeqPtr(a,Plus),ss.SeqPtr(b,Plus) + 1);
                }
                phase = (phase - exons.back().second + exons.back().first)%3;
                if (phase < 0) {
                    phase += 3;
                }
            } else {
                for (int i = 0;  i < exons.size();  ++i) {
                    int b = exons[i].first;
                    int a = exons[i].second;
                    seq.insert(seq.end(),ss.SeqPtr(a,Minus),ss.SeqPtr(b,Minus) + 1);
                }
                int l = seq.size() - 1;
                phase = (phase + exons.back().second - exons.back().first - l)%3;
                if (phase < 0) {
                    phase += 3;
                }
            }

            for (int i = (3 - phase) % 3;  i + 2 < seq.size();  i += 3) {
                if ((seq[i] == nT  &&  seq[i + 1] == nA  &&  seq[i + 2] == nA)  ||  
                    (seq[i] == nT  &&  seq[i + 1] == nA  &&  seq[i + 2] == nG)  ||  
                    (seq[i] == nT  &&  seq[i + 1] == nG  &&  seq[i + 2] == nA))  {
                    NCBI_THROW(CGnomonException, eGenericError,
                               "GNOMON: Logical error 18");
                }
            }

            exons.clear();
            //          cout << "Gene is cleared\n";
        }

    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/11/06 15:02:21  ucko
 * Use iostream interface from ncbistre.hpp for GCC 2.95 compatibility.
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
