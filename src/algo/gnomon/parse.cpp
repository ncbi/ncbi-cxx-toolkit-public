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
    for(int phase = 0; phase < 3; ++phase)
    {
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
    if(left.MScore() == BadScore) return true;
    int len = right.Stop()-left.Stop();
    return (len > TooFarLen && score+left.MScore() < right.Score());
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
bool EvaluateNewScore(const Left& left, const Right& right, double& rscore, bool& openrgn)
{

    rscore = BadScore;

    RState rst(left,right);

    int len = right.Stop()-left.Stop();
    if(len > right.MaxLen()) return false;
    if(!right.NoRightEnd() && len < right.MinLen()) return true;

    double scr, score = 0;
    if(left.isPlus())
    {
        scr = left.BranchScore(right);
        if(scr == BadScore) return true;
    }
    else
    {
        scr = right.BranchScore(left);
        if(scr == BadScore) return true;
        scr += right.DenScore()-left.DenScore();
    }
    score += scr;

    // this check is frame-dependent and MUST be after BranchScore
    if(right.StopInside()) return false;     

    if(right.NoRightEnd()) scr = right.ClosingLengthScore();
    else scr = right.LengthScore();
    if(scr == BadScore) return true;
    score += scr;

    scr = right.RgnScore();
    if(scr == BadScore) return true;
    score += scr;

    if(!right.NoRightEnd())
    {
        scr = right.TermScore();
        if(scr == BadScore) return true;
        score += scr;
    }

    openrgn = right.OpenRgn();
    rscore = score;
    return true;
}

template<class L, class R> // template for all exons
inline bool ForwardStep(const L& left, R& right)
{
    double score;
    bool openrgn;
    if(!EvaluateNewScore(left,right,score,openrgn)) return false;
    else if(score == BadScore) return true;

    if(left.Score() != BadScore && openrgn)
    {
        double scr = score+left.Score();
        if(scr > right.Score())
        {
            right.UpdateLeftState(left);
            right.UpdateScore(scr);
        }
    }

    if(!openrgn)  return false;

    return true;
}

template<class L> 
inline bool ForwardStep(const L& left, Intron& right)
{
    double score;
    bool openrgn;
    if(!EvaluateNewScore(left,right,score,openrgn)) return false;
    else if(score == BadScore) return true;

    if(left.Score() != BadScore && openrgn)
    {
        double scr = score+left.Score();
        if(scr > right.Score())
        {
            right.UpdateLeftState(left);
            right.UpdateScore(scr);
        }
    }

    if(!openrgn || TooFar(left, right, score))  return false;

    return true;
}

template<class L> 
inline bool ForwardStep(const L& left, Intergenic& right)
{
    double score;
    bool openrgn;
    if(!EvaluateNewScore(left,right,score,openrgn)) return false;
    else if(score == BadScore) return true;

    if(left.Score() != BadScore && openrgn)
    {
        double scr = score+left.Score();
        if(scr > right.Score())
        {
            right.UpdateLeftState(left);
            right.UpdateScore(scr);
        }
    }

    if(!openrgn || TooFar(left, right, score))  return false;

    return true;
}


template<class L, class R> // template for all exons
inline void MakeStep(ParseVec<L>& lvec, ParseVec<R>& rvec)
{
    if(lvec.num < 0) return;
    int i = lvec.num;
    if(lvec[i].Stop() == rvec[rvec.num].Stop()) --i;
    while(i >= 0 && ForwardStep(lvec[i--],rvec[rvec.num]));

    if(rvec.num > 0) rvec[rvec.num].UpdatePrevExon(rvec[rvec.num-1]);
}

template<class L> 
inline void MakeStep(ParseVec<L>& lvec, ParseVec<Intron>& rvec)
{
    if(lvec.num < 0) return;
    Intron& right = rvec[rvec.num];
    int i = lvec.num;
    int rlimit = right.Stop();
    if(lvec[i].Stop() == rlimit) --i;
    int nearlimit = max(0,rlimit-TooFarLen);
    while(i >= 0 && lvec[i].Stop() >= nearlimit) 
    {
        if(!ForwardStep(lvec[i--],right)) return;
    }
    if(i < 0) return;
    for(const L* p = &lvec[i]; p !=0; p = p->PrevExon())
    {
        if(!ForwardStep(*p,right)) return;
    }
}


template<class L> 
inline void MakeStep(ParseVec<L>& lvec, ParseVec<Intergenic>& rvec)
{
    if(lvec.num < 0) return;
    Intergenic& right = rvec[rvec.num];
    int i = lvec.num;
    int rlimit = right.Stop();
    if(lvec[i].Stop() == rlimit) --i;
    while(i >= 0 && lvec[i].Stop() >= HMM_State::GetSeqScores()->LeftAlignmentBoundary(right.Stop())) --i;  // no intergenics in alignment
    int nearlimit = max(0,rlimit-TooFarLen);
    while(i >= 0 && lvec[i].Stop() >= nearlimit) 
    {
        if(!ForwardStep(lvec[i--],right)) return;
    }
    if(i < 0) return;
    for(const L* p = &lvec[i]; p !=0; p = p->PrevExon())
    {
        if(!ForwardStep(*p,right)) return;
    }
}

template<class L, class R> 
inline void MakeStep(ParseVec<L>& lvec, ParseVec<R>* rvec)
{
    for(int phr = 0; phr < 3; ++phr)
    {
        MakeStep(lvec, rvec[phr]);
    }
}

template<class L, class R> 
inline void MakeStep(ParseVec<L>* lvec, ParseVec<R>& rvec)
{
    for(int phl = 0; phl < 3; ++phl)
    {
        MakeStep(lvec[phl], rvec);
    }
}

template<class L, class R> 
inline void MakeStep(ParseVec<L>* lvec, ParseVec<R>* rvec)
{
    for(int phl = 0; phl < 3; ++phl)
    {
        for(int phr = 0; phr < 3; ++phr)
        {
            MakeStep(lvec[phl], rvec[phr]);
        }
    }
}

template<class L, class R> 
inline void MakeStep(ParseVec<L>* lvec, ParseVec<R>* rvec, int shift)
{
    for(int phl = 0; phl < 3; ++phl)
    {
        int phr = (shift+phl)%3;
        MakeStep(lvec[phl], rvec[phr]);
    }
}

Parse::Parse(const SeqScores& ss) : seqscr(ss)
{
    try
    {
        igplus.reserve(seqscr.StartNumber(Plus)+1);
        igminus.reserve(seqscr.StopNumber(Minus)+1);
        feminus.reserve(seqscr.StartNumber(Minus)+1);
        leplus.reserve(seqscr.StopNumber(Plus)+1);
        seplus.reserve(seqscr.StopNumber(Plus)+1);
        seminus.reserve(seqscr.StartNumber(Minus)+1);
        for(int phase = 0; phase < 3; ++phase)
        {
            inplus[phase].reserve(seqscr.AcceptorNumber(Plus)+1);
            inminus[phase].reserve(seqscr.DonorNumber(Minus)+1);
            feplus[phase].reserve(seqscr.DonorNumber(Plus)+1);
            ieplus[phase].reserve(seqscr.DonorNumber(Plus)+1);
            ieminus[phase].reserve(seqscr.AcceptorNumber(Minus)+1);
            leminus[phase].reserve(seqscr.AcceptorNumber(Minus)+1);
        }
    }
    catch(bad_alloc)
    {
        cerr << "Not enough memory in Parse\n";
        exit(1);
    }

    int len = seqscr.SeqLen();

    for(int i = 0; i < len; ++i)
    {
        if(seqscr.AcceptorScore(i,Plus) != BadScore)
        {
            PushState(inplus,Plus,i);
            MakeStep(ieplus,inplus,1);
            MakeStep(feplus,inplus,1);
        }

        if(seqscr.AcceptorScore(i,Minus) != BadScore)
        {
            PushState(ieminus,Minus,i);
            PushState(leminus,Minus,i);
            MakeStep(inminus,ieminus);
            MakeStep(igminus,leminus);
        }

        if(seqscr.DonorScore(i,Plus) != BadScore)
        {
            PushState(feplus,Plus,i);
            PushState(ieplus,Plus,i);
            MakeStep(inplus,ieplus);
            MakeStep(igplus,feplus);
        }

        if(seqscr.DonorScore(i,Minus) != BadScore)
        {
            PushState(inminus,Minus,i);
            MakeStep(leminus,inminus,0);
            MakeStep(ieminus,inminus,0);
        }

        if(seqscr.StartScore(i,Plus) != BadScore)
        {
            PushState(igplus,Plus,i);
            MakeStep(seplus,igplus);
            MakeStep(seminus,igplus);
            MakeStep(leplus,igplus);
            MakeStep(feminus,igplus);
        }

        if(seqscr.StartScore(i,Minus) != BadScore)
        {
            PushState(feminus,Minus,i);
            PushState(seminus,Minus,i);
            MakeStep(inminus,feminus);
            MakeStep(igminus,seminus);
        }

        if(seqscr.StopScore(i,Plus) != BadScore)
        {
            PushState(leplus,Plus,i);
            PushState(seplus,Plus,i);
            MakeStep(inplus,leplus);
            MakeStep(igplus,seplus);
        }

        if(seqscr.StopScore(i,Minus) != BadScore)
        {
            PushState(igminus,Minus,i);
            MakeStep(seplus,igminus);
            MakeStep(seminus,igminus);
            MakeStep(leplus,igminus);
            MakeStep(feminus,igminus);
        }
    }

    PushState(inplus,Plus,-1);
    MakeStep(ieplus,inplus,1);
    MakeStep(feplus,inplus,1);

    PushState(ieminus,Minus,-1);
    MakeStep(inminus,ieminus);

    PushState(leminus,Minus,-1);
    MakeStep(igminus,leminus);

    PushState(ieplus,Plus,-1);
    MakeStep(inplus,ieplus);

    PushState(feplus,Plus,-1);
    MakeStep(igplus,feplus);

    PushState(inminus,Minus,-1);
    MakeStep(leminus,inminus,0);
    MakeStep(ieminus,inminus,0);

    PushState(igplus,Plus,-1);
    MakeStep(seplus,igplus);
    MakeStep(seminus,igplus);
    MakeStep(leplus,igplus);
    MakeStep(feminus,igplus);

    PushState(feminus,Minus,-1);
    MakeStep(inminus,feminus);

    PushState(seminus,Minus,-1);
    MakeStep(igminus,seminus);

    PushState(leplus,Plus,-1);
    MakeStep(inplus,leplus);

    PushState(seplus,Plus,-1);
    MakeStep(igplus,seplus);

    PushState(igminus,Minus,-1);
    MakeStep(seplus,igminus);
    MakeStep(seminus,igminus);
    MakeStep(leplus,igminus);
    MakeStep(feminus,igminus);

    igplus.back().UpdateScore(AddProbabilities(igplus.back().Score(),igminus.back().Score()));

    const HMM_State* p = &igplus.back();
    if(feminus.back().Score() > p->Score()) p = &feminus.back();
    if(leplus.back().Score() > p->Score()) p = &leplus.back();
    if(seplus.back().Score() > p->Score()) p = &seplus.back();
    if(seminus.back().Score() > p->Score()) p = &seminus.back();
    for(int ph = 0; ph < 3; ++ph)
    {
        if(inplus[ph].back().Score() > p->Score()) p = &inplus[ph].back();
        if(inminus[ph].back().Score() > p->Score()) p = &inminus[ph].back();
        if(ieplus[ph].back().Score() > p->Score()) p = &ieplus[ph].back();
        if(ieminus[ph].back().Score() > p->Score()) p = &ieminus[ph].back();
        if(leminus[ph].back().Score() > p->Score()) p = &leminus[ph].back();
        if(feplus[ph].back().Score() > p->Score()) p = &feplus[ph].back();
    }

    path = p;
}

template<class T> void Out(T t, int w, ostream& to = cout)
{
    to.width(w);
    to.setf(IOS_BASE::right,IOS_BASE::adjustfield);
    to << t;
}

void Out(double t, int w, ostream& to = cout, int prec = 1)
{
    to.width(w);
    to.setf(IOS_BASE::right,IOS_BASE::adjustfield);
    to.setf(IOS_BASE::fixed,IOS_BASE::floatfield);
    to.precision(prec);

    if(t > 1000000000) to << "+Inf";
    else if(t < -1000000000) to << "-Inf";
    else to << t;
}

inline char toACGT(int c)
{
    switch(c)
    {
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

int Parse::PrintGenes(ostream& to, ostream& toprot, bool complete) const
{
    enum {DNA_Align = 1, Prot_Align = 2 };

    list<Gene> genes = GetGenes();
    for(list<Gene>::iterator it = genes.begin(); it != genes.end();)
    {
        if(it->CDS().size() < 6) it = genes.erase(it);
        else ++it;
    }

    int right = seqscr.SeqMap(seqscr.SeqLen()-1,true);
    if(complete && !genes.empty() && !genes.back().RightComplete())
    {
        int partial_start = genes.back().front().Start();
        genes.pop_back();

        if(!genes.empty()) // end of the last complete gene
        {
            right = genes.back().back().Stop();
        }
        else
        {
            if(partial_start > seqscr.SeqMap(0,true)+1000)
            {
                right = partial_start-100;
            }
            else
            {
                return -1;   // calling program MUST be aware of this!!!!!!!
            }
        }
    }

    to << "\n" << seqscr.Contig() << '_' << seqscr.SeqMap(0,true) << '_' << right << "\n\n";
    to << (int)genes.size() << " genes found\n";

    set<int> chain_id, prot_id;
    for(list<Gene>::iterator it = genes.begin(); it != genes.end(); ++it)
    {
        const Gene& gene(*it);
        for(int i = 0; i < (int)gene.size(); ++i)
        {
            chain_id.insert(gene[i].ChainID().begin(),gene[i].ChainID().end());
            prot_id.insert(gene[i].ProtID().begin(),gene[i].ProtID().end());
        }
    }
    to << (int)(chain_id.size()+prot_id.size()) << " alignments used\n";

    int num = 0;
    for(list<Gene>::iterator it = genes.begin(); it != genes.end(); ++it)
    {
        ++num;

        to << "\n\n";
        Out(" ",19,to);
        Out("Start",10,to);
        Out("Stop",10,to);
        Out("Length",7,to);
        Out("Frame",6,to);
        Out("Align",10,to);
        Out("Prot",12,to);
        Out("FShift",7,to);
        to << '\n';

        const Gene& gene(*it);
        int gene_start = -1;
        int gene_stop = 0;
        int align_type = 0;

        for(int i = 0; i < (int)gene.size(); ++i)
        {
            const ExonData& exon(gene[i]);

            const FrameShifts& fshifts(exon.ExonFrameShifts());
            int estart = exon.Start();
            int lframe = exon.lFrame();
            int rframe = exon.rFrame();

            if(gene_start < 0 && lframe >= 0) gene_start = estart;
            if(!exon.ChainID().empty()) align_type = align_type|DNA_Align;
            if(!exon.ProtID().empty()) align_type = align_type|Prot_Align;

            for(int k = 0; k < (int)fshifts.size(); ++k)
            {
                int estop = fshifts[k].Loc()-1;
                int rf = (gene.Strand() == Plus) ? (lframe+estop-estart)%3 : (lframe-estop+estart)%3;
                if(rf < 0) rf += 3; 

                Out(num,4,to);
                Out(exon.Type(),13,to); 
                Out((gene.Strand() == Plus) ? '+' : '-',2,to);
                Out(estart,10,to);
                Out(estop,10,to);
                Out(estop-estart+1,7,to);
                to << "  " << lframe << '/' << rframe << " ";
                if(exon.ChainID().empty()) Out("-",10,to);
                else Out(*exon.ChainID().begin(),10,to);
                if(exon.ProtID().empty()) Out("-",12,to);
                else Out(*exon.ProtID().begin(),12,to);
                if(fshifts[k].IsInsertion()) 
                {
                    Out("+",7,to);
                    estart = estop;
                }
                else 
                {
                    Out("-",7,to);
                    estart = estop+2;
                }
                to << '\n';

                lframe = (gene.Strand() == Plus) ? (rf+1)%3 : (rf+2)%3;
            }
            int estop = exon.Stop();

            Out(num,4,to);
            Out(exon.Type(),13,to); 
            Out((gene.Strand() == Plus) ? '+' : '-',2,to);
            Out(estart,10,to);
            Out(estop,10,to);
            Out(estop-estart+1,7,to);
            if(lframe < 0)
            {
                to << "   -  ";
            }
            else
            {
                to << "  " << lframe << '/' << rframe << " ";
            }
            if(exon.ChainID().empty()) Out("-",10,to);
            else Out(*exon.ChainID().begin(),10,to);
            if(exon.ProtID().empty()) Out("-",12,to);
            else Out(*exon.ProtID().begin(),12,to);
            Out("*",7,to);
            to << '\n';
            if(lframe >= 0) gene_stop = estop;
        }	

        int prot_start = gene.LeftComplete() ? gene_start : seqscr.SeqMap(0,true);
        int prot_stop = gene.RightComplete() ? gene_stop : seqscr.SeqMap(seqscr.SeqLen()-1,false);
        toprot << '>' << seqscr.Contig() << "_" << gene_start << "_" << gene_stop << "_"
            << ((gene.Strand() == Plus) ? "plus" : "minus") << "_" << align_type 
            << ' ' << prot_start << ' ' << prot_stop << '\n';

        const CVec& cds(gene.CDS());

        int ii;
        for(ii = gene.CDS_Shift(); ii < (int)cds.size()-2; ii +=3)
        {
            toprot << aa_table[cds[ii]*25+cds[ii+1]*5+cds[ii+2]];
            if((ii+3-gene.CDS_Shift())%150 == 0) toprot << '\n';
        }
        if((ii-gene.CDS_Shift())%150 != 0) toprot << '\n';
    }

    return right;
}


list<Gene> Parse::GetGenes() const
{
    list<Gene> genes;

    const CClusterSet& cls(seqscr.Alignments());

    if(dynamic_cast<const Intron*>(Path()) && Path()->LeftState()) genes.push_front(Gene(Path()->Strand(),false,false));

    for(const HMM_State* p = Path(); p != 0; p = p->LeftState()) 
    {
        if(const Exon* pe = dynamic_cast<const Exon*>(p))
        {
            bool stopcodon = false;
            bool startcodon = false;

            if(dynamic_cast<const SingleExon*>(pe))
            {
                genes.push_front(Gene(pe->Strand(),true,true));
                startcodon = true;
                stopcodon = true;
            }
            else if(dynamic_cast<const LastExon*>(pe))
            {
                if(pe->isPlus()) 
                {
                    genes.push_front(Gene(pe->Strand(),false,true));
                }
                else 
                {
                    genes.front().leftend = true;
                }
                stopcodon = true;
            }
            else if(dynamic_cast<const FirstExon*>(pe))
            {
                if(pe->isMinus()) 
                {
                    genes.push_front(Gene(pe->Strand(),false,true));
                }
                else 
                {
                    genes.front().leftend = true;
                }
                startcodon = true;
            }

            Gene& curgen = genes.front();

            int estart = pe->Start();
            int estop = pe->Stop();

            int rframe = pe->Phase();
            int lframe = pe->isPlus() ? (rframe-(estop-estart))%3 : (rframe+(estop-estart))%3;
            if(lframe < 0) lframe += 3;

            if(pe->isPlus())
            {
                curgen.cds_shift = (3-lframe)%3;  // first complete codon for partial CDS
            }
            else if(curgen.empty())
            {
                curgen.cds_shift = (3-rframe)%3;
            }

            if(pe->isMinus() && startcodon)
            {
                curgen.cds.push_back(nT);
                curgen.cds.push_back(nA);
                curgen.cds.push_back(nC);
            }
            for(int i = estop; i >= estart; --i) curgen.cds.push_back(*seqscr.SeqPtr(i,Plus));
            if(pe->isPlus() && startcodon)
            {
                curgen.cds.push_back(nG);
                curgen.cds.push_back(nT);
                curgen.cds.push_back(nA);
            }

            int a = seqscr.SeqMap(estart,true);
            bool a_insertion = (estart != seqscr.RevSeqMap(a,true));
            int b = seqscr.SeqMap(estop,false);
            bool b_insertion = (estop != seqscr.RevSeqMap(b,true));
            IPair ab_limits(a,b);

            //			if(a_insertion) lframe = pe->isPlus() ? (lframe+1)%3 : (lframe+2)%3;
            //			if(b_insertion) rframe = pe->isPlus() ? (rframe+2)%3 : (rframe+1)%3;

            pair<CClusterSet::ConstIt,CClusterSet::ConstIt> lim = cls.equal_range(Cluster(a,b));
            CClusterSet::ConstIt first = lim.first;
            CClusterSet::ConstIt last = lim.second;
            if(lim.first != lim.second) --last;

            int lastid = 0;
            int margin = numeric_limits<int>::min();   // difference between end of exon and end of alignment exon
            bool rightexon = false;
            string right_utr = pe->isPlus() ? "3'_Utr" : "5'_Utr";
            if(((startcodon && pe->isMinus()) || (stopcodon && pe->isPlus())) && lim.first != lim.second)   // rightside UTR
            {
                for(Cluster::ConstIt it = last->begin(); it != last->end(); ++it)
                {
                    const AlignVec& algn(*it);
                    if(algn.Type() == AlignVec::Prot || !algn.Limits().Intersect(ab_limits)) continue;

                    for(int i = (int)algn.size()-1; i >= 0; --i)
                    {
                        if(b < algn[i].first) 
                        {
                            curgen.push_back(ExonData(algn[i].first,algn[i].second,-1,-1,right_utr));
                            curgen.back().chain_id.insert(algn.ID()); 
                            rightexon = true;
                        }
                        else if(algn[i].second >= a && algn[i].second-b > margin)
                        {
                            margin = algn[i].second-b;
                            lastid = algn.ID();
                        }
                    }
                }
            }


            int remaina = 0;
            if((pe->isMinus() && startcodon) || (pe->isPlus() && stopcodon))   // start/stop position correction
            {
                if(margin >= 3)
                {
                    b += 3;
                    margin -= 3;
                }
                else if(margin >= 0 && rightexon)
                {
                    b += margin;
                    rframe = pe->isPlus() ? (rframe+margin)%3 : (rframe-margin+3)%3;
                    remaina = 3-margin;
                    margin = 0;
                }
                else
                {
                    b += 3;
                    margin = 0;
                }
            }
            if(margin > 0)
            {
                curgen.push_back(ExonData(b+1,b+margin,-1,-1,right_utr)); 
                curgen.back().chain_id.insert(lastid); 
            }
            else if(remaina > 0)
            {
                ExonData& e = curgen.back();
                int aa = e.start;
                e.start += remaina;
                int bb = aa+remaina-1;
                int lf, rf;
                string etype;
                if(pe->isMinus())    // splitted start/stop
                {
                    lf = remaina-1;
                    rf = 0;
                    etype = "First_Cds";
                }
                else
                {
                    lf = 3-remaina;
                    rf = 2;
                    etype = "Last_Cds";
                }
                ExonData er(aa,bb,lf,rf,etype);
                er.chain_id = e.chain_id;

                if(e.start <= e.stop)
                {
                    curgen.push_back(er);
                }
                else
                {
                    e = er;     // nothing left of e
                }
            }

            curgen.push_back(ExonData(a,b,lframe,rframe,"Internal_Cds"));
            ExonData& exon = curgen.back();
            margin = numeric_limits<int>::min();
            bool leftexon = false;
            lastid = 0;
            for(CClusterSet::ConstIt cls_it = lim.first; cls_it != lim.second; ++cls_it)
            {
                for(Cluster::ConstIt it = cls_it->begin(); it != cls_it->end(); ++it)
                {
                    const AlignVec& algn(*it);
                    if(!algn.Limits().Intersect(ab_limits)) continue;

                    for(int i = 0; i < (int)algn.size(); ++i)
                    {
                        if(ab_limits.second < algn[i].first) 
                        {
                            break;
                        }
                        else if(algn[i].second < ab_limits.first) 
                        {
                            leftexon = true;
                            continue;
                        }
                        else if(algn.Type() == AlignVec::Prot) 
                        {
                            exon.prot_id.insert(algn.ID()); 
                        }
                        else 
                        {
                            exon.chain_id.insert(algn.ID());
                            if(a-algn[i].first > margin)
                            {
                                margin = a-algn[i].first;
                                lastid = algn.ID();
                            }
                        }
                    }
                }
            }

            const FrameShifts& fs = seqscr.SeqFrameShifts();
            for(int i = 0; i < (int)fs.size(); ++i)
            {
                if((fs[i].Loc() == a && a_insertion) ||    // exon starts from insertion
                   (fs[i].Loc() == b+1 && b_insertion) ||  // exon ends by insertion
                   (fs[i].Loc() <= b && fs[i].Loc() > a)) exon.fshifts.push_back(fs[i]);
            }

            int remainb = 0;
            if((pe->isPlus() && startcodon) || (pe->isMinus() && stopcodon))   // start/stop position correction
            {
                if(margin >= 3)
                {
                    exon.start -= 3;
                    margin -= 3;
                }
                else if(margin >= 0 && leftexon)
                {
                    exon.start -= margin;
                    exon.lframe = pe->isPlus() ? (exon.lframe-margin+3)%3 : (exon.lframe+margin)%3;
                    remainb = 3-margin;
                    margin = 0;
                }
                else
                {
                    exon.start -= 3;
                    margin = 0;
                }
            }

            if(startcodon && stopcodon)   // maybe Single
            {
                if(remaina == 0 && remainb == 0)
                {
                    exon.type = "Single_Cds";
                }
                else if(remainb == 0)
                {
                    exon.type = pe->isPlus() ? "First_Cds" : "Last_Cds";
                }
                else if(remaina == 0)
                {
                    exon.type = pe->isPlus() ? "Last_Cds" : "First_Cds";
                }
            }
            else if(startcodon)          // maybe First
            {
                if((pe->isPlus() && remainb == 0) || (pe->isMinus() && remaina == 0)) exon.type = "First_Cds";
            }
            else if(stopcodon)          // maybe Last
            {
                if((pe->isMinus() && remainb == 0) || (pe->isPlus() && remaina == 0)) exon.type = "Last_Cds";
            }


            string left_utr = pe->isPlus() ? "5'_Utr" : "3'_Utr";
            if(((startcodon && pe->isPlus()) || (stopcodon && pe->isMinus())) && lim.first != lim.second)   // leftside UTR
            {
                int estt = exon.start;

                if(margin > 0)
                {
                    curgen.push_back(ExonData(estt-margin,estt-1,-1,-1,left_utr)); 
                    curgen.back().chain_id.insert(lastid); 
                }

                for(Cluster::ConstIt it = first->begin(); it != first->end(); ++it)
                {
                    const AlignVec& algn(*it);

                    if(algn.Type() == AlignVec::Prot || !algn.Limits().Intersect(ab_limits)) continue;

                    for(int i = (int)algn.size()-1; i >= 0; --i)
                    {
                        if(estt > algn[i].second) 
                        {
                            int aa = algn[i].first;
                            int bb = algn[i].second;
                            if(remainb > 0)
                            {
                                string etype;
                                int lf, rf;
                                if(pe->isPlus())     // splitted start/stop
                                {
                                    lf = 0;
                                    rf = remainb-1;
                                    etype = "First_Cds";
                                }
                                else
                                {
                                    lf = 2;
                                    rf = 3-remainb;
                                    etype = "Last_Cds";
                                }
                                curgen.push_back(ExonData(bb-remainb+1,bb,lf,rf,etype));
                                curgen.back().chain_id.insert(algn.ID()); 
                                bb -= remainb;
                                remainb = 0;
                            }
                            if(aa <= bb)
                            {
                                curgen.push_back(ExonData(aa,bb,-1,-1,left_utr)); 
                                curgen.back().chain_id.insert(algn.ID());
                            } 
                        }
                    }
                }
            }
        }
    }

    for(list<Gene>::iterator it = genes.begin(); it != genes.end(); ++it)
    {
        Gene& g(*it);
        reverse(g.begin(),g.end());

        if(g.Strand() == Plus) 
        {
            reverse(g.cds.begin(),g.cds.end());
        }
        else
        {
            for(int i = 0; i < (int)g.cds.size(); ++i)
            {
                g.cds[i] = toMinus[g.cds[i]];
            }
        }

    }

    return genes;
}

void Parse::PrintInfo() const
{
    vector<const HMM_State*> states;
    for(const HMM_State* p = Path(); p != 0; p = p->LeftState()) states.push_back(p);
    reverse(states.begin(),states.end());

    Out(" ",15);
    Out("Start",11);
    Out("Stop",11);
    Out("Score",8);
    Out("BrScr",8);
    Out("LnScr",8);
    Out("RgnScr",8);
    Out("TrmScr",8);
    Out("AccScr",8);
    cout << endl;

    for(int i = 0; i < (int)states.size(); ++i)
    {
        const HMM_State* p = states[i];
        if(dynamic_cast<const Intergenic*>(p)) cout << endl;

        Out(p->GetStateName(),13);
        Out(p->isPlus() ? '+' : '-',2);
        int a = seqscr.SeqMap(p->Start(),true);
        int b = seqscr.SeqMap(p->Stop(),false);
        Out(a,11);
        Out(b,11);
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

    bool out = false;
    int phase = 0;
    int strand = 0;
    vector< pair<int,int> > exons;
    for(const HMM_State* state = &st; state != 0; state = state->LeftState())
    {
        const HMM_State* left = state->LeftState();
        if(left == 0)
        {
            if(!dynamic_cast<Ig>(state) && !dynamic_cast<I>(state)) 
            { cout << "Logical error1\n"; exit(1); }

            out = true;
        }
        else if(dynamic_cast<Ig>(state))
        {
            if(!dynamic_cast<SE>(left) &&
               !(dynamic_cast<LE>(left) && left->isPlus()) &&
               !(dynamic_cast<FE>(left) && left->isMinus()))
            { cout << "Logical error2\n"; exit(1); }

            out = true;
        }
        else if(I ps = dynamic_cast<I>(state))
        {
            E p = 0;
            int ph = -1;
            if(((p = dynamic_cast<FE>(left)) && left->isPlus()) ||
               ((p = dynamic_cast<LE>(left)) && left->isMinus()) ||
               (p = dynamic_cast<IE>(left))) ph = p->Phase();

            if((ph < 0) || (ps->Strand() != left->Strand()) ||
               (ps->isPlus() && (ph+1)%3 != ps->Phase()) ||
               (ps->isMinus() && ph != ps->Phase()))
            { cout << "Logical error3\n"; exit(1); }

            out = false;
        }
        else if(SE ps = dynamic_cast<SE>(state))
        {
            if(!(dynamic_cast<Ig>(left) && ps->Strand() == left->Strand()))
            { cout << "Logical error4\n"; exit(1); }

            int a = ps->Start(), b = ps->Stop();
            if((b-a+1)%3 ||
               (ps->isPlus() && (!ss.isStart(a,Plus) || !ss.isStop(b+1,Plus))) ||
               (ps->isMinus() && (!ss.isStart(b,Minus) || !ss.isStop(a-1,Minus)))) 
            { cout << "Logical error5\n"; exit(1); }
        }
        else if(FE ps = dynamic_cast<FE>(state))
        {
            I p = 0;
            if((ps->Strand() != left->Strand()) ||
               (!(dynamic_cast<Ig>(left) && ps->isPlus()) &&
                !((p = dynamic_cast<I>(left)) && ps->isMinus())))
            { cout << "Logical error6\n"; exit(1); }

            int a = ps->Start(), b = ps->Stop();
            if(ps->isPlus())
            {
                if(ps->Phase() != (b-a)%3)
                { cout << "Logical error7\n"; exit(1); }
            }
            else
            {
                if(p->Phase() != (b+1-a)%3)
                { cout << "Logical error8\n"; exit(1); }
            }

            //			if((ps->isPlus() && (!ss.isStart(a,Plus) || !ss.isGT(b+1,Plus))) ||
            //			   (ps->isMinus() && (!ss.isStart(b,Minus) || !ss.isGT(a-1,Minus)))) 
            //			{ cout << "Logical error9\n"; exit(1); }
        }
        else if(IE ps = dynamic_cast<IE>(state))
        {
            I p = 0;
            if((ps->Strand() != left->Strand()) || 
               !(p = dynamic_cast<I>(left)))
            { cout << "Logical error10\n"; exit(1); }

            int a = ps->Start(), b = ps->Stop();
            if(ps->isPlus())
            {
                if(ps->Phase() != (p->Phase()+b-a)%3)
                { cout << "Logical error11\n"; exit(1); }
            }
            else
            {
                if(p->Phase() != (ps->Phase()+b+1-a)%3)
                { cout << "Logical error12\n"; exit(1); }
            }

            //			if((ps->isPlus() && (!ss.isAG(a-1,Plus) || !ss.isGT(b+1,Plus))) ||
            //			   (ps->isMinus() && (!ss.isAG(b+1,Minus) || !ss.isGT(a-1,Minus)))) 
            //			{ cout << "Logical error13\n"; exit(1); }
        }
        else if(LE ps = dynamic_cast<LE>(state))
        {
            I p = 0;
            if((ps->Strand() != left->Strand()) ||
               (!(dynamic_cast<Ig>(left) && ps->isMinus()) && 
                !((p = dynamic_cast<I>(left)) && ps->isPlus())))
            { cout << "Logical error14\n"; exit(1); }

            int a = ps->Start(), b = ps->Stop();
            if(ps->isPlus())
            {
                if((p->Phase()+b-a)%3 != 2)
                { cout << "Logical15 error\n"; exit(1); }
            }
            else
            {
                if((ps->Phase()+b-a)%3 != 2)
                { cout << "Logical error16\n"; exit(1); }
            }

            //			if((ps->isPlus() && (!ss.isAG(a-1,Plus) || !ss.isStop(b+1,Plus))) ||
            //			   (ps->isMinus() && (!ss.isStop(a-1,Minus) || !ss.isAG(b+1,Minus)))) 
            //			{ cout << "Logical error17\n"; exit(1); }
        }

        if(E ps = dynamic_cast<E>(state))
        {
            phase = ps->Phase();
            strand = ps->Strand();
            exons.push_back(make_pair(ps->Start(),ps->Stop()));
            out = false;
        }

        if(out && !exons.empty())
        {
            CVec seq;

            if(strand == Plus)
            {
                for(int i = (int)exons.size()-1; i >= 0; --i)
                {
                    int a = exons[i].first;
                    int b = exons[i].second;
                    seq.insert(seq.end(),ss.SeqPtr(a,Plus),ss.SeqPtr(b,Plus)+1);
                }
                phase = (phase-exons.back().second+exons.back().first)%3;
                if(phase < 0) phase += 3;
            }
            else
            {
                for(int i = 0; i < (int)exons.size(); ++i)
                {
                    int b = exons[i].first;
                    int a = exons[i].second;
                    seq.insert(seq.end(),ss.SeqPtr(a,Minus),ss.SeqPtr(b,Minus)+1);
                }
                int l = (int)seq.size()-1;
                phase = (phase+exons.back().second-exons.back().first-l)%3;
                if(phase < 0) phase += 3;
            }

            for(int i = (3-phase)%3; i+2 < (int)seq.size(); i += 3)
            {
                if((seq[i] == nT && seq[i+1] == nA && seq[i+2] == nA) ||
                   (seq[i] == nT && seq[i+1] == nA && seq[i+2] == nG) ||
                   (seq[i] == nT && seq[i+1] == nG && seq[i+2] == nA)) 
                { cout << "Logical error18\n"; exit(1); }
            }

            exons.clear();
            //			cout << "Gene is cleared\n";
        }

    }
}

END_NCBI_SCOPE

/*
 * ==========================================================================
 * $Log$
 * Revision 1.5  2004/07/28 17:28:53  ucko
 * Use macros from ncbistre.hpp for portability.
 * Use CVec rather than IVec in LogicalCheck to deal with WorkShop,
 * which doesn't have full templatized insert.
 *
 * Revision 1.4  2004/07/28 12:33:19  dicuccio
 * Sync with Sasha's working tree
 *
 * ==========================================================================
 */
