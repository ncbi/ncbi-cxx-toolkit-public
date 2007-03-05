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
#include <algo/gnomon/gnomon_exception.hpp>
#include "hmm.hpp"
#include "hmm_inlines.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

const double kLnHalf = log(0.5);
const double kLnThree = log(3.);

/* inline */ double CLorentz::ClosingScore(int l) const
{
    if(l >= MaxLen()) return BadScore();
    int i = (l-1)/m_step;
    int delx = min((i+1)*m_step,MaxLen())-l;
    double dely = (i == 0 ? 1 : m_clscore[i-1])-m_clscore[i];
    return log(dely/m_step*delx+m_clscore[i]);
}

/* inline */ CHMM_State::CHMM_State(EStrand strn, int point) : m_stop(point), m_strand(strn), 
                                      m_score(BadScore()), m_leftstate(0), m_terminal(0) 
{
    if(sm_seqscr == 0) 
    {
        NCBI_THROW(CGnomonException, eGenericError, "sm_seqscr is not initialised in HMM_State");
    }
}

/* inline */ int CHMM_State::MinLen() const
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

/* inline */ int CHMM_State::RegionStart() const
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

/* inline */ int CHMM_State::RegionStop() const
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

/* inline */ bool CExon::StopInside() const
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

/* inline */ bool CExon::OpenRgn() const
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

/* inline */ double CExon::RgnScore() const
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

/* inline */ void CExon::UpdatePrevExon(const CExon& e)
{
    m_mscore = max(Score(),e.MScore());
    m_prevexon = &e;
    while(m_prevexon != 0 && m_prevexon->Score() <= Score()) m_prevexon = m_prevexon->m_prevexon;
}
    
/* inline */ CSingleExon::CSingleExon(EStrand strn, int point) : CExon(strn,point,2)
{
    if(!sm_initialised) Error("CExon is not initialised\n");
    m_terminal = isPlus() ? &sm_seqscr->Stop() : &sm_seqscr->Start();
    if(isMinus()) m_phase = 0;
            
    EvaluateInitialScore(*this);
}
        
/* inline */ double CSingleExon::LengthScore() const 
{
    return sm_singlelen.Score(Stop()-Start()+1)+kLnThree;
}

/* inline */ double CSingleExon::TermScore() const
{
    if(isPlus()) return sm_seqscr->StopScore(Stop(),Strand());
    else return sm_seqscr->StartScore(Stop(),Strand());
}

/* inline */ double CSingleExon::BranchScore(const CIntergenic& next) const 
{ 
    if(isPlus() || (Stop()-Start())%3 == 2) return kLnHalf;
    else return BadScore();
}

/* inline */ CFirstExon::CFirstExon(EStrand strn, int ph, int point) : CExon(strn,point,ph)
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

/* inline */ double CFirstExon::LengthScore() const
{
    int last = Stop()-Start();
    return sm_firstlen.Score(last+1)+kLnThree+sm_firstphase[last%3];
} 

/* inline */ double CFirstExon::TermScore() const
{
    if(isPlus()) return sm_seqscr->DonorScore(Stop(),Strand());
    else return sm_seqscr->StartScore(Stop(),Strand());
}

/* inline */ double CFirstExon::BranchScore(const CIntron& next) const
{
    if(Strand() != next.Strand()) return BadScore();

    int ph = isPlus() ? Phase() : Phase()+Stop()-Start();

    if((ph+1)%3 == next.Phase()) return 0;
    else return BadScore();
}

/* inline */ CInternalExon::CInternalExon(EStrand strn, int ph, int point) : CExon(strn,point,ph)
{
    if(!sm_initialised) Error("CExon is not initialised\n");
    m_terminal = isPlus() ? &sm_seqscr->Donor() : &sm_seqscr->Acceptor();
            
    EvaluateInitialScore(*this);
}

/* inline */ double CInternalExon::LengthScore() const 
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

/* inline */ double CInternalExon::TermScore() const
{
    if(isPlus()) return sm_seqscr->DonorScore(Stop(),Strand());
    else return sm_seqscr->AcceptorScore(Stop(),Strand());
}

/* inline */ CLastExon::CLastExon(EStrand strn, int ph, int point) : CExon(strn,point,ph)
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

/* inline */ double CLastExon::LengthScore() const 
{
    int last = Stop()-Start();
    return sm_lastlen.Score(last+1)+kLnThree;
}

/* inline */ double CLastExon::TermScore() const
{
    if(isPlus()) return sm_seqscr->StopScore(Stop(),Strand());
    else return sm_seqscr->AcceptorScore(Stop(),Strand());
}

/* inline */ double CLastExon::BranchScore(const CIntergenic& next) const 
{ 
    if(isPlus() || (Phase()+Stop()-Start())%3 == 2) return kLnHalf;
    else return BadScore(); 
}

/* inline */ CIntron::CIntron(EStrand strn, int ph, int point) : CHMM_State(strn,point), m_phase(ph)
{
    if(!sm_initialised) Error("Intron is not initialised\n");
    m_terminal = isPlus() ? &sm_seqscr->Acceptor() : &sm_seqscr->Donor();
            
    EvaluateInitialScore(*this);
}

/* inline */ double CIntron::ClosingLengthScore() const 
{ 
    return sm_intronlen.ClosingScore(Stop()-Start()+1); 
}

/* inline */ double CIntron::BranchScore(const CLastExon& next) const
{
    if(Strand() != next.Strand()) return BadScore();

    if(isPlus())
    {
        int shift = next.Stop()-next.Start();
        if((Phase()+shift)%3 == next.Phase()) return sm_lnTerminal;
    }
    else if(Phase() == next.Phase()) return sm_lnTerminal;
            
    return BadScore();
}

/* inline */ CIntergenic::CIntergenic(EStrand strn, int point) : CHMM_State(strn,point)
{
    if(!sm_initialised) Error("Intergenic is not initialised\n");
    m_terminal = isPlus() ? &sm_seqscr->Start() : &sm_seqscr->Stop();
            
    EvaluateInitialScore(*this);
}

/* inline */ bool CIntergenic::OpenRgn() const
{
    return sm_seqscr->OpenIntergenicRegion(Start(),Stop());
}

/* inline */ double CIntergenic::RgnScore() const
{
    return sm_seqscr->IntergenicScore(RegionStart(),RegionStop(),Strand());
}

/* inline */ double CIntergenic::TermScore() const
{
    if(isPlus()) return sm_seqscr->StartScore(Stop(),Strand());
    else return sm_seqscr->StopScore(Stop(),Strand());
}

/* inline */ double CIntergenic::BranchScore(const CFirstExon& next) const 
{
    if(&next == m_leftstate)
    {
        if(next.isMinus()) return sm_lnMulti;
        else return BadScore();
    }
    else if(isPlus() && next.isPlus()) 
    {
        if((next.Stop()-next.Start())%3 == next.Phase()) return sm_lnMulti;
        else return BadScore();
    }
    else return BadScore();
}

/* inline */ double CIntergenic::BranchScore(const CSingleExon& next) const 
{
    if(&next == m_leftstate)
    {
        if(next.isMinus()) return sm_lnSingle;
        else return BadScore();
    }
    else if(isPlus() && next.isPlus() && 
              (next.Stop()-next.Start())%3 == 2) return sm_lnSingle;
    else return BadScore();
}

void CMarkovChain<0>::InitScore(CNcbiIfstream& from)
{
    Init(from);
    if(from) toScore();
}

void CMarkovChain<0>::Init(CNcbiIfstream& from)
{
    from >> m_score[enA];
    from >> m_score[enC];
    from >> m_score[enG];
    from >> m_score[enT];
    m_score[enN] = 0.25*(m_score[enA]+m_score[enC]+m_score[enG]+m_score[enT]);
}

void CMarkovChain<0>::Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3)
{
    m_score[enA] = 0.25*(mc0.m_score[enA]+mc1.m_score[enA]+mc2.m_score[enA]+mc3.m_score[enA]);
    m_score[enC] = 0.25*(mc0.m_score[enC]+mc1.m_score[enC]+mc2.m_score[enC]+mc3.m_score[enC]);
    m_score[enG] = 0.25*(mc0.m_score[enG]+mc1.m_score[enG]+mc2.m_score[enG]+mc3.m_score[enG]);
    m_score[enT] = 0.25*(mc0.m_score[enT]+mc1.m_score[enT]+mc2.m_score[enT]+mc3.m_score[enT]);
    m_score[enN] = 0.25*(m_score[enA]+m_score[enC]+m_score[enG]+m_score[enT]);
}

void CMarkovChain<0>::toScore()
{
    m_score[enA] = (m_score[enA] <= 0) ? BadScore() : log(4*m_score[enA]);
    m_score[enC] = (m_score[enC] <= 0) ? BadScore() : log(4*m_score[enC]);
    m_score[enG] = (m_score[enG] <= 0) ? BadScore() : log(4*m_score[enG]);
    m_score[enT] = (m_score[enT] <= 0) ? BadScore() : log(4*m_score[enT]);
    m_score[enN] = (m_score[enN] <= 0) ? BadScore() : log(4*m_score[enN]);
}

pair<int,int> CInputModel::FindContent(CNcbiIfstream& from, const string& label, int gccontent)
{
    string str;
    while(from >> str)
    {
        int low,high;
        if(str == label) 
        {
            from >> str;
            if(str != "CG:") return make_pair(-1,-1);
            from >> low >> high;
            if(!from) return make_pair(-1,-1);
            if( gccontent >= low && gccontent < high) return make_pair(low,high);
        }
    }
    return make_pair(-1,-1);
}

CInputModel::~CInputModel() {}

double CMDD_Donor::Score(const CEResidueVec& seq, int i) const
{
    int first = i-m_left+1;
    int last = i+m_right;
    if(first < 0 || last >= (int)seq.size()) return BadScore();    // out of range
    if(seq[i+1] != enG || seq[i+2] != enT) return BadScore();   // no GT
    
    int mat = 0;
    for(int j = 0; j < (int)m_position.size(); ++j)
    {
        if(seq[first+m_position[j]] != m_consensus[j]) break;
        ++mat;
    }
    
    return m_matrix[mat].Score(&seq[first]);
}

CMDD_Donor::CMDD_Donor(const string& file, int gccontent)
{
    string label = "[MDD_Donor]";
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,gccontent);
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
    
    do
    {
        from >> str;
        if(!from) Error(label);
        if(str == "Position:")
        {
            int i;
            from >> i;
            if(!from) Error(label);
            m_position.push_back(i);
            from >> str;
            if(!from) Error(label);
            if(str != "Consensus:") Error(label);
            char c;
            from >> c;
            if(!from) Error(label);
            i = fromACGT(c);
            if(i == enN) Error(label); 
            m_consensus.push_back(i);
        }
        m_matrix.push_back(CMarkovChainArray<0>());
        m_matrix.back().InitScore(m_inexon+m_inintron,from);
        if(!from) Error(label);
    }
    while(str != "End");
}

CWMM_Start::CWMM_Start(const string& file, int gccontent)
{
    string label = "[WMM_Start]";
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,gccontent);
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

double CWMM_Start::Score(const CEResidueVec& seq, int i) const
{
    int first = i-m_left+1;
    int last = i+m_right;
    if(first < 0 || last >= (int)seq.size()) return BadScore();  // out of range
    if(seq[i-2] != enA || seq[i-1] != enT || seq[i] != enG) return BadScore(); // no ATG
    
    return m_matrix.Score(&seq[first]);
}

CWAM_Stop::CWAM_Stop(const string& file, int gccontent)
{
    string label = "[WAM_Stop_1]";
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,gccontent);
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

double CWAM_Stop::Score(const CEResidueVec& seq, int i) const
{
    int first = i-m_left+1;
    int last = i+m_right;
    if(first-1 < 0 || last >= (int)seq.size()) return BadScore();  // out of range
    if((seq[i+1] != enT || seq[i+2] != enA || seq[i+3] != enA) &&
        (seq[i+1] != enT || seq[i+2] != enA || seq[i+3] != enG) &&
        (seq[i+1] != enT || seq[i+2] != enG || seq[i+3] != enA)) return BadScore(); // no stop-codon
    
    return m_matrix.Score(&seq[first]);
}

const CSeqScores* CHMM_State::sm_seqscr = 0;

bool CLorentz::Init(CNcbiIstream& from, const string& label)
{
    string s;
    from >> s;
    if(s != label) return false;
    
    from >> s;
    if(s != "A:") return false;
    if(!(from >> m_A)) return false;
    
    from >> s;
    if(s != "L:") return false;
    if(!(from >> m_L)) return false;
    
    from >> s;
    if(s != "MinL:") return false;
    if(!(from >> m_minl)) return false;
    
    from >> s;
    if(s != "MaxL:") return false;
    if(!(from >> m_maxl)) return false;
    
    from >> s;
    if(s != "Step:") return false;
    if(!(from >> m_step)) return false;
    
    int num = (m_maxl-1)/m_step+1;
    try
    {
        m_score.resize(num,0);
        m_clscore.resize(num,0);
    }
    catch(bad_alloc)
    {
        NCBI_THROW(CGnomonException, eMemoryLimit, "Not enough memory in CLorentz");
    }
    
    int i = 0;
    while(from >> m_score[i]) 
    {
        if((i+1)*m_step < m_minl) m_score[i] = 0;
        i++;
    }
    from.clear();
    while(i < num) 
    {
        m_score[i] = m_A/(m_L*m_L+pow((i+0.5)*m_step,2));
        i++;
    }

    double sum = 0;
    m_avlen = 0;
    for(int l = m_minl; l <= m_maxl; ++l) 
    {
        double scr = m_score[(l-1)/m_step];
        sum += scr;
        m_avlen += l*scr;
    }
        
    m_avlen /= sum;
    for(int i = 0; i < num; ++i) m_score[i] /= sum;

    m_clscore[num-1] = 0;
    for(int i = num-2; i >= 0; --i) m_clscore[i] = m_clscore[i+1]+m_score[i+1]*m_step;

    for(int i = 0; i < num; ++i) m_score[i] = (m_score[i] == 0) ? BadScore() : log(m_score[i]);
    
    return true;
}

double CLorentz::Through(int seqlen) const
{
    double through = 0;
    for(int l = seqlen+1; l <= MaxLen(); ++l) 
    {
        int i = (l-1)/m_step;
        if(m_score[i] == BadScore()) continue;
        through += exp(m_score[i])*(l-seqlen);
    }
    through /= AvLen();
    through = (through == 0) ? BadScore() : log(through);
    return through;
}

double CExon::sm_firstphase[3], CExon::sm_internalphase[3][3];
CLorentz CExon::sm_firstlen, CExon::sm_internallen, CExon::sm_lastlen, CExon::sm_singlelen; 
bool CExon::sm_initialised = false;

void CExon::Init(const string& file, int cgcontent)
{
    string label = "[Exon]";
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label+" 1");
    
    string str;
    from >> str;
    if(str != "FirstExonPhase:") Error(label+" 2");
    for(int i = 0; i < 3; ++i) 
    {
        from >> sm_firstphase[i];
        if(!from) Error(label);
        sm_firstphase[i] = log(sm_firstphase[i]);
    }

    from >> str;
    if(str != "InternalExonPhase:") Error(label+" 3");
    for(int i = 0; i < 3; ++i)
    {
        for(int j = 0; j < 3; ++j) 
        {
            from >> sm_internalphase[i][j];
            if(!from) Error(label+" 4");
            sm_internalphase[i][j] = log(sm_internalphase[i][j]);
        }
    }

    if(!sm_firstlen.Init(from,"FirstExonDistribution:")) Error(label+" 5");
    if(!sm_internallen.Init(from,"InternalExonDistribution:")) Error(label+" 6");
    if(!sm_lastlen.Init(from,"LastExonDistribution:")) Error(label+" 7");
    if(!sm_singlelen.Init(from,"SingleExonDistribution:")) Error(label+" 8");
    
    sm_initialised = true;
    from.close();
}


double CIntron::sm_lnThrough[3], CIntron::sm_lnDen[3];
double CIntron::sm_lnTerminal, CIntron::sm_lnInternal;
CLorentz CIntron::sm_intronlen;
bool CIntron::sm_initialised = false;

void CIntron::Init(const string& file, int cgcontent, int seqlen)
{
    string label = "[Intron]";
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    string str;
    from >> str;
    if(str != "InitP:") Error(label);
    double initp, phasep[3];
    from >> initp >> phasep[0] >> phasep[1] >> phasep[2];
    if(!from) Error(label);
    initp = initp/2;      // two strands

    from >> str;
    if(str != "toTerm:") Error(label);
    double toterm;
    from >> toterm;
    if(!from) Error(label);
    sm_lnTerminal = log(toterm);
    sm_lnInternal = log(1-toterm);

    if(!sm_intronlen.Init(from,"IntronDistribution:")) Error(label);

    double lnthrough = sm_intronlen.Through(seqlen);
    for(int i = 0; i < 3; ++i)
    {
        sm_lnDen[i] = log(initp*phasep[i]);
        sm_lnThrough[i] = (lnthrough == BadScore()) ? BadScore() : sm_lnDen[i]+lnthrough;
    }
    
    sm_initialised = true;
    from.close();
}


double CIntergenic::sm_lnThrough;
double CIntergenic::sm_lnDen;
double CIntergenic::sm_lnSingle, CIntergenic::sm_lnMulti;
CLorentz CIntergenic::sm_intergeniclen;
bool CIntergenic::sm_initialised = false;

void CIntergenic::Init(const string& file, int cgcontent, int seqlen)
{
    string label = "[Intergenic]";
    CNcbiIfstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);
    
    string str;
    from >> str;
    if(str != "InitP:") Error(label);
    double initp;
    from >> initp;
    if(!from) Error(label);
    initp = initp/2;      // two strands

    from >> str;
    if(str != "toSingle:") Error(label);
    double tosingle;
    from >> tosingle;
    if(!from) Error(label);
    sm_lnSingle = log(tosingle);
    sm_lnMulti = log(1-tosingle);

    if(!sm_intergeniclen.Init(from,"IntergenicDistribution:")) Error(label);

    double lnthrough = sm_intergeniclen.Through(seqlen);
    sm_lnDen = log(initp);
    sm_lnThrough = (lnthrough == BadScore()) ? BadScore() : sm_lnDen+lnthrough;
    
    sm_initialised = true;
    from.close();
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
