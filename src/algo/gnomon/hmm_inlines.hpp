#ifndef ALGO_GNOMON___HMM_INLINES__HPP
#define ALGO_GNOMON___HMM_INLINES__HPP

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
#include <algo/gnomon/gnomon_exception.hpp>
#include "score.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

template<int order> void CMarkovChain<order>::InitScore(CNcbiIfstream& from)
{
    Init(from);
    if(from) toScore();
}

template<int order> void CMarkovChain<order>::Init(CNcbiIfstream& from)
{
    m_next[enA].Init(from);
    m_next[enC].Init(from);
    m_next[enG].Init(from);
    m_next[enT].Init(from);
    if(from) m_next[enN].Average(m_next[enA],m_next[enC],m_next[enG],m_next[enT]);
}

template<int order> void CMarkovChain<order>::Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3)
{
    m_next[enA].Average(mc0.m_next[enA],mc1.m_next[enA],mc2.m_next[enA],mc3.m_next[enA]);
    m_next[enC].Average(mc0.m_next[enC],mc1.m_next[enC],mc2.m_next[enC],mc3.m_next[enC]);
    m_next[enG].Average(mc0.m_next[enG],mc1.m_next[enG],mc2.m_next[enG],mc3.m_next[enG]);
    m_next[enT].Average(mc0.m_next[enT],mc1.m_next[enT],mc2.m_next[enT],mc3.m_next[enT]);
    m_next[enN].Average(m_next[enA],m_next[enC],m_next[enG],m_next[enT]);
}

template<int order> void CMarkovChain<order>::toScore()
{
    m_next[enA].toScore();
    m_next[enC].toScore();
    m_next[enG].toScore();
    m_next[enT].toScore();
    m_next[enN].toScore();
}

template<int order> inline double CMarkovChainArray<order>::Score(const EResidue* seq) const
{
    double score = 0;
    for(int i = 0; i < m_length; ++i)
    {
        double s = m_mc[i].Score(seq+i);
        if(s == kBadScore) return kBadScore;
        score += s;
    }
    return score;
}

template<int order> void CMarkovChainArray<order>::InitScore(int l, CNcbiIfstream& from)
{
    m_length = l;
    m_mc.resize(m_length);
    for(int i = 0; i < m_length; ++i) m_mc[i].InitScore(from);
}

template<int order>
double CWAM_Donor<order>::Score(const CEResidueVec& seq, int i) const
{
    int first = i-m_left+1;
    int last = i+m_right;
    if(first-order < 0 || last >= (int)seq.size()) return kBadScore;    // out of range
    if(seq[i+1] != enG || (seq[i+2] != enT && seq[i+2] != enC)) return kBadScore;   // no GT/GC
//    if(seq[i+1] != enG || seq[i+2] != enT) return kBadScore;   // no GT
    
    return m_matrix.Score(&seq[first]);
}

template<int order>
CWAM_Donor<order>::CWAM_Donor(const string& file, int cgcontent)
{
    CNcbiOstrstream ost;
    ost << "[WAM_Donor_" << order << "]";
    string label = CNcbiOstrstreamToString(ost);
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    string str;
    from >> str;
    if(str != "InExon:") Error(label);
    from >> m_inexon;
    if(!from) Error(label);
    from >> str;
    if(str != "InIntron:") Error(label);
    from >> m_inintron;
    if(!from) Error(label);
    
    m_left = m_inexon;
    m_right = m_inintron;
    
    m_matrix.InitScore(m_inexon+m_inintron,from);
    if(!from) Error(label);
}


template<int order>
double CWAM_Acceptor<order>::Score(const CEResidueVec& seq, int i) const
{
    int first = i-m_left+1;
    int last = i+m_right;
    if(first-order < 0 || last >= (int)seq.size()) return kBadScore;  // out of range
    if(seq[i-1] != enA || seq[i] != enG) return kBadScore;   // no AG
    
    return m_matrix.Score(&seq[first]);
}

template<int order>
CWAM_Acceptor<order>::CWAM_Acceptor(const string& file, int cgcontent)
{
    CNcbiOstrstream ost;
    ost << "[WAM_Acceptor_" << order << "]";
    string label = CNcbiOstrstreamToString(ost);
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    string str;
    from >> str;
    if(str != "InExon:") Error(label);
    from >> m_inexon;
    if(!from) Error(label);
    from >> str;
    if(str != "InIntron:") Error(label);
    from >> m_inintron;
    if(!from) Error(label);
    
    m_left = m_inintron;
    m_right = m_inexon;
    
    m_matrix.InitScore(m_inexon+m_inintron,from);
    if(!from) Error(label);
}


template<int order>
CMC3_CodingRegion<order>::CMC3_CodingRegion(const string& file, int cgcontent)
{
    CNcbiOstrstream ost;
    ost << "[MC3_CodingRegion_" << order << "]";
    string label = CNcbiOstrstreamToString(ost);
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    m_matrix[0].InitScore(from);
    m_matrix[1].InitScore(from);
    m_matrix[2].InitScore(from);
    if(!from) Error(label);
}

template<int order>
double CMC3_CodingRegion<order>::Score(const CEResidueVec& seq, int i, int codonshift) const
{
    if(i-order < 0) return kBadScore;  // out of range
//    else if(seq[i] == enN) return -2.;  // discourage making exons of Ns
    else return m_matrix[codonshift].Score(&seq[i]);
}

template<int order>
CMC_NonCodingRegion<order>::CMC_NonCodingRegion(const string& file, int cgcontent)
{
    CNcbiOstrstream ost;
    ost << "[MC_NonCodingRegion_" << order << "]";
    string label = CNcbiOstrstreamToString(ost);
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    m_matrix.InitScore(from);
    if(!from) Error(label);
}

template<int order>
double CMC_NonCodingRegion<order>::Score(const CEResidueVec& seq, int i) const
{
    if(i-order < 0) return kBadScore;  // out of range
    return m_matrix.Score(&seq[i]);
}

inline bool CSeqScores::isStart(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+2 >= SeqLen()) return false;  //out of range
    else if(ss[ii] != enA || ss[ii+1] != enT || ss[ii+2] != enG) return false;
    else return true;
}

inline bool CSeqScores::isStop(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+2 >= SeqLen()) return false;  //out of range
    if((ss[ii] != enT || ss[ii+1] != enA || ss[ii+2] != enA) &&
        (ss[ii] != enT || ss[ii+1] != enA || ss[ii+2] != enG) &&
        (ss[ii] != enT || ss[ii+1] != enG || ss[ii+2] != enA)) return false;
    else return true;
}

inline bool CSeqScores::isAG(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii-1 < 0 || ii >= SeqLen()) return false;  //out of range
    if(ss[ii-1] != enA || ss[ii] != enG) return false;
    else return true;
}

inline bool CSeqScores::isGT(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+1 >= SeqLen()) return false;  //out of range
    if(ss[ii] != enG || ss[ii+1] != enT) return false;
    else return true;
}

inline bool CSeqScores::isConsensusIntron(int i, int j, int strand) const
{
    if(strand == ePlus) return (m_dscr[ePlus][i-1] != kBadScore) && (m_ascr[ePlus][j] != kBadScore);
    else                return (m_ascr[eMinus][i-1] != kBadScore) && (m_dscr[eMinus][j] != kBadScore);
//    if(strand == ePlus) return isGT(i,strand) && isAG(j,strand);
//    else return isAG(i,strand) && isGT(j,strand);
}

inline const EResidue* CSeqScores::SeqPtr(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    return &ss.front()+ii;
}

inline bool CSeqScores::StopInside(int a, int b, int strand, int frame) const
{
    return (a <= m_laststop[strand][frame][b]);
}

inline bool CSeqScores::OpenCodingRegion(int a, int b, int strand, int frame) const
{
    return (a > m_notinexon[strand][frame][b]);
}

inline double CSeqScores::CodingScore(int a, int b, int strand, int frame) const
{
    if(a > b) return 0; // for splitted start/stop
    double score = m_cdrscr[strand][frame][b];
    if(a > 0) score -= m_cdrscr[strand][frame][a-1];
    return score;
}

inline bool CSeqScores::OpenNonCodingRegion(int a, int b, int strand) const
{
    return (a > m_notinintron[strand][b]);
}

inline double CSeqScores::NonCodingScore(int a, int b, int strand) const
{
    double score = m_ncdrscr[strand][b];
    if(a > 0) score -= m_ncdrscr[strand][a-1];
    return score;
}

inline bool CSeqScores::OpenIntergenicRegion(int a, int b) const
{
    return (a > m_notining[b]);
} 

inline double CSeqScores::IntergenicScore(int a, int b, int strand) const
{
    double score = m_ingscr[strand][b];
    if(a > 0) score -= m_ingscr[strand][a-1];
    return score;
}

inline int CSeqScores::SeqMap(int i, EMove move, int* dellenp) const 
{
    if(dellenp != 0) *dellenp = 0;
    int l = m_seq_map[i];
    if(l >= 0) return l;
    
    if(i == 0)
    {
         NCBI_THROW(CGnomonException, eGenericError, "First base in sequence can not be a deletion");
    }
    if(i == (int)m_seq_map.size()-1)
    {
         NCBI_THROW(CGnomonException, eGenericError, "Last base in sequence can not be a deletion");
    }
    
    switch(move)
    {
        case eMoveLeft:
        {
            int dl = 0;
            while(i-dl > 0 && m_seq_map[i-dl] < 0) ++dl;
            if(dellenp != 0) *dellenp = dl;
            return m_seq_map[i-dl];
        }
        case eMoveRight:
        {
            int dl = 0;
            while(i+dl < (int)m_seq_map.size()-1 && m_seq_map[i+dl] < 0) ++dl;
            if(dellenp != 0) *dellenp = dl;
            return m_seq_map[i+dl];
        }
        default:  return l;
    }
}

inline TSignedSeqPos CSeqScores::RevSeqMap(TSignedSeqPos i) const 
{
    int l = m_rev_seq_map[i-From()];
    if(l >= 0) 
    {
        return l;
    }
    else
    {
         NCBI_THROW(CGnomonException, eGenericError, "Mapping of insertion");
    }
}

inline double AddProbabilities(double scr1, double scr2)
{
    if(scr1 == kBadScore) return scr2;
    else if(scr2 == kBadScore) return scr1;
    else if(scr1 >= scr2)  return scr1+log(1+exp(scr2-scr1));
    else return scr2+log(1+exp(scr1-scr2));
}

inline double AddScores(double scr1, double scr2)
{
    if(scr1 == kBadScore || scr2 == kBadScore) return kBadScore;
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
    if(score == kBadScore) return;

    double scr;
    scr = r.RgnScore();
    if(scr == kBadScore) return;
    score += scr;

    if(!r.NoRightEnd())
    {
        scr = r.TermScore();
        if(scr == kBadScore) return;
        score += scr;
    }

    if(r.OpenRgn()) r.UpdateScore(score);
}

inline double CLorentz::ClosingScore(int l) const
{
    if(l == MaxLen()) return kBadScore;
    int i = (l-1)/m_step;
    int delx = min((i+1)*m_step,MaxLen())-l;
    double dely = (i == 0 ? 1 : m_clscore[i-1])-m_clscore[i];
    return log(dely/m_step*delx+m_clscore[i]);
}

inline CHMM_State::CHMM_State(EStrand strn, int point) : m_stop(point), m_strand(strn), 
                                      m_score(kBadScore), m_leftstate(0), m_terminal(0) 
{
    if(sm_seqscr == 0) 
    {
        NCBI_THROW(CGnomonException, eGenericError, "sm_seqscr is not initialised in HMM_State");
    }
}

inline int CHMM_State::MinLen() const
{
    int minlen = 1;

    if(!NoLeftEnd())
    {
        if(isPlus()) minlen += m_leftstate->m_terminal->Right();
        else minlen += m_leftstate->m_terminal->Left();
    }

    if(!NoRightEnd())
    {
        if(isPlus()) minlen += m_terminal->Left();
        else  minlen += m_terminal->Right();
    }

    return minlen;
}

inline int CHMM_State::Stop() const
{
    return NoRightEnd() ? (int)sm_seqscr->SeqLen()-1 : m_stop;
}

inline int CHMM_State::RegionStart() const
{
    if(NoLeftEnd())  return 0;
    else
    {
        int a = m_leftstate->m_stop+1;
        if(isPlus()) a += m_leftstate->m_terminal->Right();
        else a += m_leftstate->m_terminal->Left();
        return a;
    }
}

inline int CHMM_State::RegionStop() const
{
    if(NoRightEnd()) return sm_seqscr->SeqLen()-1;
    else 
    {
        int b = m_stop;
        if(isPlus()) b -= m_terminal->Left();
        else  b -= m_terminal->Right();
        return b;
    }
}

inline bool CExon::StopInside() const
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
    
    return sm_seqscr->StopInside(Start(),Stop(),Strand(),frame);
}

inline bool CExon::OpenRgn() const
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
    
    return sm_seqscr->OpenCodingRegion(Start(),Stop(),Strand(),frame);
}

inline double CExon::RgnScore() const
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
    
    return sm_seqscr->CodingScore(RegionStart(),RegionStop(),Strand(),frame);
}

inline void CExon::UpdatePrevExon(const CExon& e)
{
    m_mscore = max(Score(),e.MScore());
    m_prevexon = &e;
    while(m_prevexon != 0 && m_prevexon->Score() <= Score()) m_prevexon = m_prevexon->m_prevexon;
}
    
inline CSingleExon::CSingleExon(EStrand strn, int point) : CExon(strn,point,2)
{
    if(!sm_initialised) Error("CExon is not initialised\n");
    m_terminal = isPlus() ? &sm_seqscr->Stop() : &sm_seqscr->Start();
    if(isMinus()) m_phase = 0;
            
    EvaluateInitialScore(*this);
}
        
inline double CSingleExon::LengthScore() const 
{
    return sm_singlelen.Score(Stop()-Start()+1)+kLnThree;
}

inline double CSingleExon::TermScore() const
{
    if(isPlus()) return sm_seqscr->StopScore(Stop(),Strand());
    else return sm_seqscr->StartScore(Stop(),Strand());
}

inline double CSingleExon::BranchScore(const CIntergenic& next) const 
{ 
    if(isPlus() || (Stop()-Start())%3 == 2) return kLnHalf;
    else return kBadScore;
}

inline CFirstExon::CFirstExon(EStrand strn, int ph, int point) : CExon(strn,point,ph)
{
    if(!sm_initialised) Error("CExon is not initialised\n");
    if(isPlus())
    {
        m_terminal = &sm_seqscr->Donor();
    }
    else
    {
        m_phase = 0;
        m_terminal = &sm_seqscr->Start();
    }

    EvaluateInitialScore(*this);
}

inline double CFirstExon::LengthScore() const
{
    int last = Stop()-Start();
    return sm_firstlen.Score(last+1)+kLnThree+sm_firstphase[last%3];
} 

inline double CFirstExon::TermScore() const
{
    if(isPlus()) return sm_seqscr->DonorScore(Stop(),Strand());
    else return sm_seqscr->StartScore(Stop(),Strand());
}

inline double CFirstExon::BranchScore(const CIntron& next) const
{
    if(Strand() != next.Strand()) return kBadScore;

    int ph = isPlus() ? Phase() : Phase()+Stop()-Start();

    if((ph+1)%3 == next.Phase()) return 0;
    else return kBadScore;
}

inline CInternalExon::CInternalExon(EStrand strn, int ph, int point) : CExon(strn,point,ph)
{
    if(!sm_initialised) Error("CExon is not initialised\n");
    m_terminal = isPlus() ? &sm_seqscr->Donor() : &sm_seqscr->Acceptor();
            
    EvaluateInitialScore(*this);
}

inline double CInternalExon::LengthScore() const 
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

    return sm_internallen.Score(last+1)+kLnThree+sm_internalphase[ph0][ph1];
}

inline double CInternalExon::TermScore() const
{
    if(isPlus()) return sm_seqscr->DonorScore(Stop(),Strand());
    else return sm_seqscr->AcceptorScore(Stop(),Strand());
}


inline double CInternalExon::BranchScore(const CIntron& next) const
{
    if(Strand() != next.Strand()) return kBadScore;

    int ph = isPlus() ? Phase() : Phase()+Stop()-Start();

    if((ph+1)%3 == next.Phase()) return 0;
    else return kBadScore;
}

inline CLastExon::CLastExon(EStrand strn, int ph, int point) : CExon(strn,point,ph)
{
    if(!sm_initialised) Error("CExon is not initialised\n");
    if(isPlus())
    {
        m_phase = 2;
        m_terminal = &sm_seqscr->Stop();
    }
    else
    {
        m_terminal = &sm_seqscr->Acceptor();
    }

    EvaluateInitialScore(*this);
}

inline double CLastExon::LengthScore() const 
{
    int last = Stop()-Start();
    return sm_lastlen.Score(last+1)+kLnThree;
}

inline double CLastExon::TermScore() const
{
    if(isPlus()) return sm_seqscr->StopScore(Stop(),Strand());
    else return sm_seqscr->AcceptorScore(Stop(),Strand());
}

inline double CLastExon::BranchScore(const CIntergenic& next) const 
{ 
    if(isPlus() || (Phase()+Stop()-Start())%3 == 2) return kLnHalf;
    else return kBadScore; 
}

inline CIntron::CIntron(EStrand strn, int ph, int point) : CHMM_State(strn,point), m_phase(ph)
{
    if(!sm_initialised) Error("Intron is not initialised\n");
    m_terminal = isPlus() ? &sm_seqscr->Acceptor() : &sm_seqscr->Donor();
            
    EvaluateInitialScore(*this);
}

inline double CIntron::LengthScore() const 
{ 
    if(SplittedStop()) return kBadScore;
    else return sm_intronlen.Score(Stop()-Start()+1); 
}

inline double CIntron::ClosingLengthScore() const 
{ 
    return sm_intronlen.ClosingScore(Stop()-Start()+1); 
}

inline bool CIntron::OpenRgn() const
{
    return sm_seqscr->OpenNonCodingRegion(Start(),Stop(),Strand());
}

inline double CIntron::TermScore() const
{
    if(isPlus()) return sm_seqscr->AcceptorScore(Stop(),Strand());
    else return sm_seqscr->DonorScore(Stop(),Strand());
}

inline double CIntron::BranchScore(const CLastExon& next) const
{
    if(Strand() != next.Strand()) return kBadScore;

    if(isPlus())
    {
        int shift = next.Stop()-next.Start();
        if((Phase()+shift)%3 == next.Phase()) return sm_lnTerminal;
    }
    else if(Phase() == next.Phase()) return sm_lnTerminal;
            
    return kBadScore;
}

inline double CIntron::BranchScore(const CInternalExon& next) const
{
    if(Strand() != next.Strand()) return kBadScore;

    if(isPlus())
    {
        int shift = next.Stop()-next.Start();
        if((Phase()+shift)%3 == next.Phase()) return sm_lnInternal;
    }
    else if(Phase() == next.Phase()) return sm_lnInternal;
            
    return kBadScore;
}

inline bool CIntron::SplittedStop() const
{
    if(Phase() == 0 || NoLeftEnd() || NoRightEnd()) 
    {
        return false;
    }
    else if(isPlus()) 
    {
        return sm_seqscr->SplittedStop(LeftState()->Stop(),Stop(),Strand(),Phase()-1);
    }
    else
    {
        return sm_seqscr->SplittedStop(Stop(),LeftState()->Stop(),Strand(),Phase()-1);
    }
}

inline CIntergenic::CIntergenic(EStrand strn, int point) : CHMM_State(strn,point)
{
    if(!sm_initialised) Error("Intergenic is not initialised\n");
    m_terminal = isPlus() ? &sm_seqscr->Start() : &sm_seqscr->Stop();
            
    EvaluateInitialScore(*this);
}

inline bool CIntergenic::OpenRgn() const
{
    return sm_seqscr->OpenIntergenicRegion(Start(),Stop());
}

inline double CIntergenic::RgnScore() const
{
    return sm_seqscr->IntergenicScore(RegionStart(),RegionStop(),Strand());
}

inline double CIntergenic::TermScore() const
{
    if(isPlus()) return sm_seqscr->StartScore(Stop(),Strand());
    else return sm_seqscr->StopScore(Stop(),Strand());
}

inline double CIntergenic::BranchScore(const CFirstExon& next) const 
{
    if(&next == m_leftstate)
    {
        if(next.isMinus()) return sm_lnMulti;
        else return kBadScore;
    }
    else if(isPlus() && next.isPlus()) 
    {
        if((next.Stop()-next.Start())%3 == next.Phase()) return sm_lnMulti;
        else return kBadScore;
    }
    else return kBadScore;
}

inline double CIntergenic::BranchScore(const CSingleExon& next) const 
{
    if(&next == m_leftstate)
    {
        if(next.isMinus()) return sm_lnSingle;
        else return kBadScore;
    }
    else if(isPlus() && next.isPlus() && 
              (next.Stop()-next.Start())%3 == 2) return sm_lnSingle;
    else return kBadScore;
}

END_SCOPE(gnomon)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/09/15 21:28:07  chetvern
 * Sync with Sasha's working tree
 *
 *
 * ===========================================================================
 */

#endif  // ALGO_GNOMON___HMM_INLINES__HPP
