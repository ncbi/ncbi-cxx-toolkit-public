#ifndef __STATES__HPP
#define __STATES__HPP

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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

inline double AddProbabilities(double scr1, double scr2)
{
    if(scr1 == BadScore) return scr2;
    else if(scr2 == BadScore) return scr1;
    else if(scr1 >= scr2)  return scr1+log(1+exp(scr2-scr1));
    else return scr2+log(1+exp(scr1-scr2));
}

inline double AddScores(double scr1, double scr2)
{
    if(scr1 == BadScore || scr2 == BadScore) return BadScore;
    else return scr1+scr2;
}

template<class State> inline void EvaluateInitialScore(State& r)
{
    int len = r.Stop()-r.Start()+1;
    if(len >= r.MaxLen() || r.StopInside()) return;   // >= makes sence here

    int minlen = 1;
    if(!r.NoRightEnd())
    {
        if(r.isPlus()) minlen += r.TerminalPtr()->Left();
        else  minlen += r.TerminalPtr()->Right();
    }
    if(len < minlen) return;   // states can go beyon the start
    // so we shouldn't enforce MinLen

    double score;
    if(r.NoRightEnd()) 
    {
        score = r.ThroughLengthScore();
    }
    else 
    {
        score = r.InitialLengthScore();
    }
    if(score == BadScore) return;

    double scr;
    scr = r.RgnScore();
    if(scr == BadScore) return;
    score += scr;

    if(!r.NoRightEnd())
    {
        scr = r.TermScore();
        if(scr == BadScore) return;
        score += scr;
    }

    if(r.OpenRgn()) r.UpdateScore(score);
}

inline double Lorentz::ClosingScore(int l) const
{
    if(l == MaxLen()) return BadScore;
    int i = (l-1)/step;
    int delx = min((i+1)*step,MaxLen())-l;
    double dely = (i == 0 ? 1 : clscore[i-1])-clscore[i];
    return log(dely/step*delx+clscore[i]);

    //	return log(clscore[(l-1)/step]);

}

inline HMM_State::HMM_State(int strn, int point) : stop(point), strand(strn), 
score(BadScore), leftstate(0), terminal(0) 
{
    if(seqscr == 0) { cerr << "seqscr is not initialised in HMM_State\n"; exit(1); }
}

inline int HMM_State::MinLen() const
{
    int minlen = 1;

    if(!NoLeftEnd())
    {
        if(isPlus()) minlen += leftstate->terminal->Right();
        else minlen += leftstate->terminal->Left();
    }

    if(!NoRightEnd())
    {
        if(isPlus()) minlen += terminal->Left();
        else  minlen += terminal->Right();
    }

    return minlen;
}

inline int HMM_State::RegionStart() const
{
    if(NoLeftEnd())  return 0;
    else
    {
        int a = leftstate->stop+1;
        if(isPlus()) a += leftstate->terminal->Right();
        else a += leftstate->terminal->Left();
        return a;
    }
}

inline int HMM_State::RegionStop() const
{
    if(NoRightEnd()) return seqscr->SeqLen()-1;
    else 
    {
        int b = stop;
        if(isPlus()) b -= terminal->Left();
        else  b -= terminal->Right();
        return b;
    }
}

inline bool Exon::StopInside() const
{
    int frame;
    if(isPlus())
    {
        frame = (Phase()-Stop())%3;    // Stop()-Phase() is left codon end
        if(frame < 0) frame += 3;
    }
    else
    {
        frame = (Phase()+Stop())%3;    // Stop()+Phase() is right codon end
    }

    return seqscr->StopInside(Start(),Stop(),Strand(),frame);
}

inline bool Exon::OpenRgn() const
{
    int frame;
    if(isPlus())
    {
        frame = (Phase()-Stop())%3;    // Stop()-Phase() is left codon end
        if(frame < 0) frame += 3;
    }
    else
    {
        frame = (Phase()+Stop())%3;    // Stop()+Phase() is right codon end
    }

    return seqscr->OpenCodingRegion(Start(),Stop(),Strand(),frame);
}

inline double Exon::RgnScore() const
{
    int frame;
    if(isPlus())
    {
        frame = (Phase()-Stop())%3;
        if(frame < 0) frame += 3;
    }
    else
    {
        frame = (Phase()+Stop())%3;
    }

    return seqscr->CodingScore(RegionStart(),RegionStop(),Strand(),frame);
}

inline void Exon::UpdatePrevExon(const Exon& e)
{
    mscore = max(score,e.MScore());
    prevexon = &e;
    while(prevexon != 0 && prevexon->Score() <= Score()) prevexon = prevexon->prevexon;
}

inline SingleExon::SingleExon(int strn, int point) : Exon(strn,point,2)
{
    if(!initialised) Error("Exon is not initialised\n");
    terminal = isPlus() ? &seqscr->Stop() : &seqscr->Start();
    if(isMinus()) phase = 0;

    EvaluateInitialScore(*this);
}

inline double SingleExon::LengthScore() const 
{
    return singlelen.Score(Stop()-Start()+1)+LnThree;
}

inline double SingleExon::TermScore() const
{
    if(isPlus()) return seqscr->StopScore(Stop(),Strand());
    else return seqscr->StartScore(Stop(),Strand());
}

inline double SingleExon::BranchScore(const Intergenic& next) const 
{ 
    if(isPlus() || (Stop()-Start())%3 == 2) return LnHalf;
    else return BadScore;
}

inline FirstExon::FirstExon(int strn, int ph, int point) : Exon(strn,point,ph)
{
    if(!initialised) Error("Exon is not initialised\n");
    if(isPlus())
    {
        terminal = &seqscr->Donor();
    }
    else
    {
        phase = 0;
        terminal = &seqscr->Start();
    }

    EvaluateInitialScore(*this);
}

inline double FirstExon::LengthScore() const
{
    int last = Stop()-Start();
    return firstlen.Score(last+1)+LnThree+firstphase[last%3];
} 

inline double FirstExon::TermScore() const
{
    if(isPlus()) return seqscr->DonorScore(Stop(),Strand());
    else return seqscr->StartScore(Stop(),Strand());
}

inline double FirstExon::BranchScore(const Intron& next) const
{
    if(Strand() != next.Strand()) return BadScore;

    int ph = isPlus() ? Phase() : Phase()+Stop()-Start();

    if((ph+1)%3 == next.Phase()) return 0;
    else return BadScore;
}

inline InternalExon::InternalExon(int strn, int ph, int point) : Exon(strn,point,ph)
{
    if(!initialised) Error("Exon is not initialised\n");
    terminal = isPlus() ? &seqscr->Donor() : &seqscr->Acceptor();

    EvaluateInitialScore(*this);
}

inline double InternalExon::LengthScore() const 
{
    int ph0,ph1;
    int last = Stop()-Start();
    if(isPlus())
    {
        ph1 = Phase();
        ph0 = (ph1-last)%3;
        if(ph0 < 0) ph0 += 3;
    }
    else
    {
        ph0 = Phase();
        ph1 = (ph0+last)%3;
    }

    return internallen.Score(last+1)+LnThree+internalphase[ph0][ph1];
}

inline double InternalExon::TermScore() const
{
    if(isPlus()) return seqscr->DonorScore(Stop(),Strand());
    else return seqscr->AcceptorScore(Stop(),Strand());
}


inline double InternalExon::BranchScore(const Intron& next) const
{
    if(Strand() != next.Strand()) return BadScore;

    int ph = isPlus() ? Phase() : Phase()+Stop()-Start();

    if((ph+1)%3 == next.Phase()) return 0;
    else return BadScore;
}

inline LastExon::LastExon(int strn, int ph, int point) : Exon(strn,point,ph)
{
    if(!initialised) Error("Exon is not initialised\n");
    if(isPlus())
    {
        phase = 2;
        terminal = &seqscr->Stop();
    }
    else
    {
        terminal = &seqscr->Acceptor();
    }

    EvaluateInitialScore(*this);
}

inline double LastExon::LengthScore() const 
{
    int last = Stop()-Start();
    return lastlen.Score(last+1)+LnThree;
}

inline double LastExon::TermScore() const
{
    if(isPlus()) return seqscr->StopScore(Stop(),Strand());
    else return seqscr->AcceptorScore(Stop(),Strand());
}

inline double LastExon::BranchScore(const Intergenic& next) const 
{ 
    if(isPlus() || (Phase()+Stop()-Start())%3 == 2) return LnHalf;
    else return BadScore; 
}

inline Intron::Intron(int strn, int ph, int point) : HMM_State(strn,point), phase(ph)
{
    if(!initialised) Error("Intron is not initialised\n");
    terminal = isPlus() ? &seqscr->Acceptor() : &seqscr->Donor();

    EvaluateInitialScore(*this);
}

inline double Intron::LengthScore() const 
{ 
    if(SplittedStop()) return BadScore;
    else 	return intronlen.Score(Stop()-Start()+1); 
}

inline double Intron::ClosingLengthScore() const 
{ 
    return intronlen.ClosingScore(Stop()-Start()+1); 
}

inline double Intron::InitialLengthScore() const 
{ 
    return lnDen[Phase()]+ClosingLengthScore(); 
}


inline bool Intron::OpenRgn() const
{
    return seqscr->OpenNonCodingRegion(Start(),Stop(),Strand());
}

inline double Intron::TermScore() const
{
    if(isPlus()) return seqscr->AcceptorScore(Stop(),Strand());
    else return seqscr->DonorScore(Stop(),Strand());
}

inline double Intron::BranchScore(const LastExon& next) const
{
    if(Strand() != next.Strand()) return BadScore;

    if(isPlus())
    {
        int shift = next.Stop()-next.Start();
        if((Phase()+shift)%3 == next.Phase()) return lnTerminal;
    }
    else if(Phase() == next.Phase()) return lnTerminal;

    return BadScore;
}

inline double Intron::BranchScore(const InternalExon& next) const
{
    if(Strand() != next.Strand()) return BadScore;

    if(isPlus())
    {
        int shift = next.Stop()-next.Start();
        if((Phase()+shift)%3 == next.Phase()) return lnInternal;
    }
    else if(Phase() == next.Phase()) return lnInternal;

    return BadScore;
}

inline bool Intron::SplittedStop() const
{
    if(Phase() == 0 || NoLeftEnd() || NoRightEnd()) 
    {
        return false;
    }
    else if(isPlus()) 
    {
        return seqscr->SplittedStop(LeftState()->Stop(),Stop(),Strand(),Phase()-1);
    }
    else
    {
        return seqscr->SplittedStop(Stop(),LeftState()->Stop(),Strand(),Phase()-1);
    }
}

inline Intergenic::Intergenic(int strn, int point) : HMM_State(strn,point)
{
    if(!initialised) Error("Intergenic is not initialised\n");
    terminal = isPlus() ? &seqscr->Start() : &seqscr->Stop();

    EvaluateInitialScore(*this);
}

inline bool Intergenic::OpenRgn() const
{
    return seqscr->OpenIntergenicRegion(Start(),Stop());
}

inline double Intergenic::RgnScore() const
{
    return seqscr->IntergenicScore(RegionStart(),RegionStop(),Strand());
}

inline double Intergenic::TermScore() const
{
    if(isPlus()) return seqscr->StartScore(Stop(),Strand());
    else return seqscr->StopScore(Stop(),Strand());
}

inline double Intergenic::BranchScore(const FirstExon& next) const 
{
    if(&next == leftstate)
    {
        if(next.isMinus()) return lnMulti;
        else return BadScore;
    }
    else if(isPlus() && next.isPlus()) 
    {
        if((next.Stop()-next.Start())%3 == next.Phase()) return lnMulti;
        else return BadScore;
    }
    else return BadScore;
}

inline double Intergenic::BranchScore(const SingleExon& next) const 
{
    if(&next == leftstate)
    {
        if(next.isMinus()) return lnSingle;
        else return BadScore;
    }
    else if(isPlus() && next.isPlus() && 
            (next.Stop()-next.Start())%3 == 2) return lnSingle;
    else return BadScore;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/07/28 12:33:19  dicuccio
 * Sync with Sasha's working tree
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // __STATES__HPP
