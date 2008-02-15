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
#include <algo/gnomon/gnomon.hpp>
#include "hmm.hpp"
#include "hmm_inlines.hpp"
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

USING_SCOPE(objects);

const double kLnHalf = log(0.5);
const double kLnThree = log(3.);

double CLorentz::ClosingScore(int l) const
{
    if(l >= MaxLen()) return BadScore();
    int i = (l-1)/m_step;
    int delx = min((i+1)*m_step,MaxLen())-l;
    double dely = (i == 0 ? 1 : m_clscore[i-1])-m_clscore[i];
    return log(dely/m_step*delx+m_clscore[i]);
}

CHMM_State::CHMM_State(EStrand strn, int point, const CSeqScores& seqscr)
    : m_stop(point), m_strand(strn), 
      m_score(BadScore()), m_leftstate(0), m_terminal(0),
      m_seqscr(&seqscr)
{
}

/*
int CHMM_State::MinLen() const
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
*/

int CHMM_State::RegionStart() const
{
    if(NoLeftEnd())  return 0;
    else
    {
        int a = m_leftstate->m_stop+1;
        if(isPlus()) a += m_leftstate->m_terminal->Right();
        else a += m_leftstate->m_terminal->Left();
        return min(a,m_seqscr->SeqLen()-1);
    }
}

int CHMM_State::RegionStop() const
{
    if(NoRightEnd()) return m_seqscr->SeqLen()-1;
    else 
    {
        int b = m_stop;
        if(isPlus()) b -= m_terminal->Left();
        else  b -= m_terminal->Right();
        return max(0,b);
    }
}

CExon::CExon(EStrand strn, int point, int ph, const CSeqScores& seqscr, const CExonParameters& exon_params)
    : CHMM_State(strn,point,seqscr), m_phase(ph), 
      m_prevexon(0), m_mscore(BadScore()),
      m_param(&exon_params)
{    
    if(!m_param || !m_param->m_initialised)
        CInputModel::Error("CExon is not initialised\n");
}

bool CExon::StopInside() const
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
    
    return m_seqscr->StopInside(Start(),Stop(),Strand(),frame);
}

bool CExon::OpenRgn() const
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
    
    return m_seqscr->OpenCodingRegion(Start(),Stop(),Strand(),frame);
}

double CExon::RgnScore() const
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
    
    return m_seqscr->CodingScore(RegionStart(),RegionStop(),Strand(),frame);
}

void CExon::UpdatePrevExon(const CExon& e)
{
    m_mscore = max(e.Score(),e.MScore());
    m_prevexon = &e;
    while(m_prevexon != 0 && m_prevexon->Score() <= Score()) m_prevexon = m_prevexon->m_prevexon;
}

CSingleExon::CSingleExon(EStrand strn, int point, const CSeqScores& seqscr, const CExonParameters& exon_params)
    : CExon(strn,point,2,seqscr,exon_params)
{
    m_terminal = isPlus() ? &m_seqscr->Stop() : &m_seqscr->Start();
    if(isMinus()) m_phase = 0;
            
    EvaluateInitialScore(*this);
}
        
double CSingleExon::LengthScore() const 
{
    return m_param->m_singlelen.Score(Stop()-Start()+1)+kLnThree;
}

double CSingleExon::TermScore() const
{
    if(isPlus()) return m_seqscr->StopScore(Stop(),Strand());
    else return m_seqscr->StartScore(Stop(),Strand());
}

double CSingleExon::BranchScore(const CIntergenic& next) const 
{ 
    if(isPlus() || (Stop()-Start())%3 == 2) return kLnHalf;
    else return BadScore();
}

CFirstExon::CFirstExon(EStrand strn, int ph, int point, const CSeqScores& seqscr, const CExonParameters& exon_params)
    : CExon(strn,point,ph,seqscr,exon_params)
{
    if(isPlus())
    {
        m_terminal = &m_seqscr->Donor();
    }
    else
    {
        m_phase = 0;
        m_terminal = &m_seqscr->Start();
    }

    EvaluateInitialScore(*this);
}

double CFirstExon::LengthScore() const
{
    int last = Stop()-Start();
    return m_param->m_firstlen.Score(last+1)+kLnThree+m_param->m_firstphase[last%3];
} 

double CFirstExon::TermScore() const
{
    if(isPlus()) return m_seqscr->DonorScore(Stop(),Strand());
    else return m_seqscr->StartScore(Stop(),Strand());
}

double CFirstExon::BranchScore(const CIntron& next) const
{
    if(Strand() != next.Strand()) return BadScore();

    int ph = isPlus() ? Phase() : Phase()+Stop()-Start();

    if((ph+1)%3 == next.Phase()) return 0;
    else return BadScore();
}

CInternalExon::CInternalExon(EStrand strn, int ph, int point, const CSeqScores& seqscr, const CExonParameters& exon_params)
    : CExon(strn,point,ph,seqscr,exon_params)
{
    m_terminal = isPlus() ? &m_seqscr->Donor() : &m_seqscr->Acceptor();
            
    EvaluateInitialScore(*this);
}

double CInternalExon::LengthScore() const 
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

    return m_param->m_internallen.Score(last+1)+kLnThree+m_param->m_internalphase[ph0][ph1];
}

double CInternalExon::TermScore() const
{
    if(isPlus()) return m_seqscr->DonorScore(Stop(),Strand());
    else return m_seqscr->AcceptorScore(Stop(),Strand());
}

CLastExon::CLastExon(EStrand strn, int ph, int point, const CSeqScores& seqscr, const CExonParameters& exon_params)
    : CExon(strn,point,ph,seqscr,exon_params)
{
    if(isPlus())
    {
        m_phase = 2;
        m_terminal = &m_seqscr->Stop();
    }
    else
    {
        m_terminal = &m_seqscr->Acceptor();
    }

    EvaluateInitialScore(*this);
}

double CLastExon::LengthScore() const 
{
    int last = Stop()-Start();
    return m_param->m_lastlen.Score(last+1)+kLnThree;
}

double CLastExon::TermScore() const
{
    if(isPlus()) return m_seqscr->StopScore(Stop(),Strand());
    else return m_seqscr->AcceptorScore(Stop(),Strand());
}

double CLastExon::BranchScore(const CIntergenic& next) const 
{ 
    if(isPlus() || (Phase()+Stop()-Start())%3 == 2) return kLnHalf;
    else return BadScore(); 
}

CIntron::CIntron(EStrand strn, int ph, int point, const CSeqScores& seqscr, const CIntronParameters& intron_params)
    : CHMM_State(strn,point,seqscr), m_phase(ph),
      m_param(&intron_params)
{
    if(!m_param || !m_param->m_initialised) CInputModel::Error("Intron is not initialised\n");
    m_terminal = isPlus() ? &m_seqscr->Acceptor() : &m_seqscr->Donor();
            
    EvaluateInitialScore(*this);
}

double CIntron::ClosingLengthScore() const 
{ 
    return m_param->m_intronlen.ClosingScore(Stop()-Start()+1); 
}

double CIntron::BranchScore(const CLastExon& next) const
{
    if(Strand() != next.Strand()) return BadScore();

    if(isPlus())
    {
        int shift = next.Stop()-next.Start();
        if((Phase()+shift)%3 == next.Phase()) return m_param->m_lnTerminal;
    }
    else if(Phase() == next.Phase()) return m_param->m_lnTerminal;
            
    return BadScore();
}

CIntergenic::CIntergenic(EStrand strn, int point, const CSeqScores& seqscr, const CIntergenicParameters& intergenic_params)
    : CHMM_State(strn,point,seqscr),
      m_param(&intergenic_params)
{
    if(!m_param || !m_param->m_initialised) CInputModel::Error("Intergenic is not initialised\n");
    m_terminal = isPlus() ? &m_seqscr->Start() : &m_seqscr->Stop();
            
    EvaluateInitialScore(*this);
}

bool CIntergenic::OpenRgn() const
{
    return m_seqscr->OpenIntergenicRegion(Start(),Stop());
}

double CIntergenic::RgnScore() const
{
    return m_seqscr->IntergenicScore(RegionStart(),RegionStop(),Strand());
}

double CIntergenic::TermScore() const
{
    if(isPlus()) return m_seqscr->StartScore(Stop(),Strand());
    else return m_seqscr->StopScore(Stop(),Strand());
}

double CIntergenic::BranchScore(const CFirstExon& next) const 
{
    if(&next == m_leftstate)
    {
        if(next.isMinus()) return m_param->m_lnMulti;
        else return BadScore();
    }
    else if(isPlus() && next.isPlus()) 
    {
        if((next.Stop()-next.Start())%3 == next.Phase()) return m_param->m_lnMulti;
        else return BadScore();
    }
    else return BadScore();
}

double CIntergenic::BranchScore(const CSingleExon& next) const 
{
    if(&next == m_leftstate)
    {
        if(next.isMinus()) return m_param->m_lnSingle;
        else return BadScore();
    }
    else if(isPlus() && next.isPlus() && 
              (next.Stop()-next.Start())%3 == 2) return m_param->m_lnSingle;
    else return BadScore();
}

void CMarkovChain<0>::InitScore(const CMarkov_chain_params& from)
{
    Init(from);
    toScore();
}

void CMarkovChain<0>::Init(const CMarkov_chain_params& from)
{
    if (from.GetOrder() != 0)
        CInputModel::Error("Wrong Markov Chain order");
    CMarkov_chain_params::TProbabilities::const_iterator i = from.GetProbabilities().begin();
    m_score[enA] = (*i++)->GetValue();
    m_score[enC] = (*i++)->GetValue();
    m_score[enG] = (*i++)->GetValue();
    m_score[enT] = (*i++)->GetValue();
    if (i != from.GetProbabilities().end())
        CInputModel::Error("Too many values in Markov Chain");
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

CInputModel::~CInputModel() {}

CWMM_Start::CWMM_Start(const CGnomon_param::C_Param& from)
{
    m_inexon = from.GetStart().GetIn_exon();
    m_inintron = from.GetStart().GetIn_intron();
    
    m_left = m_inintron;
    m_right = m_inexon;
    
    m_matrix.InitScore(m_inexon+m_inintron,from.GetStart());
}

double CWMM_Start::Score(const CEResidueVec& seq, int i) const
{
    int first = i-m_left+1;
    int last = i+m_right;
    if(first < 0 || last >= (int)seq.size()) return BadScore();  // out of range
    if(seq[i-2] != enA || seq[i-1] != enT || seq[i] != enG) return BadScore(); // no ATG
    
    return m_matrix.Score(&seq[first]);
}

CWAM_Stop::CWAM_Stop(const CGnomon_param::C_Param& from)
{
    m_inexon = from.GetStop().GetIn_exon();
    m_inintron = from.GetStop().GetIn_intron();
    
    m_left = m_inexon;
    m_right = m_inintron;
    
    m_matrix.InitScore(m_inexon+m_inintron,from.GetStop());
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


void CLorentz::Init(const CLength_distribution_params& from)
{
    m_A = from.GetA();
    m_L = from.GetL();
    m_minl = from.GetRange().GetMin();
    m_maxl = from.GetRange().GetMax();
    m_step = from.GetStep();
    
    int num = (m_maxl-1)/m_step+1;
    m_maxl = (num-1)*m_step+1;
    
    try
    {
        m_score.resize(num,0);
        m_clscore.resize(num,0);
    }
    catch(bad_alloc)
    {
        NCBI_THROW(CGnomonException, eMemoryLimit, "Not enough memory for CLorentz");
    }
    
    int i = 0;
    ITERATE(CLength_distribution_params::TP, s, from.GetP()) {
        m_score[i] = *s;
        if((i+1)*m_step < m_minl)
            m_score[i] = 0;
        ++i;
    }
    while(i < num) 
    {
        m_score[i] = m_A/(m_L*m_L+pow((i+0.5)*m_step,2));
        ++i;
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
    for(int i = num-2; i >= 0; --i)
        m_clscore[i] = m_clscore[i+1]+m_score[i+1]*m_step;

    for(int i = 0; i < num; ++i)
        m_score[i] = (m_score[i] == 0) ? BadScore() : log(m_score[i]);
}

double CLorentz::Through(int seqlen) const
{
    if(seqlen >= MaxLen()) return BadScore();

    double through = 0;

    if(seqlen >= MinLen()) {
        int ifirst = (MinLen()-1)/m_step;
        if(m_score[ifirst] != BadScore()) {
            int w = (ifirst+1)*m_step-MinLen()+1;
            int l2 = (MinLen()+(ifirst+1)*m_step);
            through = (l2-2*seqlen)*w/2*exp(m_score[ifirst]);
        }

        int ilast = (seqlen-1)/m_step;
        for(int i = 0; i < ilast; ++i) {
            if(m_score[i] == BadScore()) continue;
            int l2 = (i+1)*m_step+i*m_step+1;
            through += (l2-2*seqlen)*m_step/2*exp(m_score[i]);
        }

        if(m_score[ilast] != BadScore()) {
            int w = seqlen-ilast*m_step;
            int l2 = ilast*m_step+1+seqlen;
            through += (l2-2*seqlen)*w/2*exp(m_score[ilast]);
        }
    }

    through = (AvLen()-seqlen-through)/AvLen();
    through = (through <= 0) ? BadScore() : log(through);
    return through;
}

CExonParameters::CExonParameters(const CGnomon_param::C_Param& from)
{
    string label = class_id();

    int i = 0;
    ITERATE(CExon_params::TFirst_exon_phase_probabilities, p, from.GetExon().GetFirst_exon_phase_probabilities()) {
        if (i<3)
            m_firstphase[i] = log(*p);
        else
            Error(label+" Too long First_exon_phase_probabilities");
        ++i;
    }

    CExon_params::TInternal_exon_phase_probabilities::const_iterator p = from.GetExon().GetInternal_exon_phase_probabilities().begin();
    for(int i = 0; i < 3; ++i) {
        for(int j = 0; j < 3; ++j)  {
            m_internalphase[i][j] = log(*p++);
        }
    }
    if (p != from.GetExon().GetInternal_exon_phase_probabilities().end())
        Error(label+" Too long Internal_exon_phase_probabilities");
    
    m_firstlen.Init(from.GetExon().GetFirst_exon_length());
    m_internallen.Init(from.GetExon().GetInternal_exon_length());
    m_lastlen.Init(from.GetExon().GetLast_exon_length());
    m_singlelen.Init(from.GetExon().GetSingle_exon_length());
    
    m_initialised = true;
}


CIntronParameters::CIntronParameters(const CGnomon_param::C_Param& from)
{
    m_initp = from.GetIntron().GetInitp();
    m_initp = m_initp/2;      // two strands

    int i = 0;
    ITERATE(CIntron_params::TPhase_probabilities, p, from.GetIntron().GetPhase_probabilities()) {
        if (i<3)
            m_phasep[i] = *p;
        else
            Error(class_id()+" Too long Phase_probabilities");
        ++i;
    }

    double toterm =  from.GetIntron().GetTo_term();
    m_lnTerminal = log(toterm);
    m_lnInternal = log(1-toterm);

    m_intronlen.Init(from.GetIntron().GetLength());
}

void CIntronParameters::SetSeqLen(int seqlen) const
{
    double lnthrough = m_intronlen.Through(seqlen);
    for(int i = 0; i < 3; ++i)
    {
        m_lnDen[i] = log(m_initp*m_phasep[i]);
        m_lnThrough[i] = (lnthrough == BadScore()) ? BadScore() : m_lnDen[i]+lnthrough;
    }
    
    m_initialised = true;
}


CIntergenicParameters::CIntergenicParameters(const CGnomon_param::C_Param& from)
{
    m_initp = from.GetIntergenic().GetInitp();
    m_initp = m_initp/2;      // two strands

    double tosingle =  from.GetIntergenic().GetTo_single();

    m_lnSingle = log(tosingle);
    m_lnMulti = log(1-tosingle);

    m_intergeniclen.Init(from.GetIntergenic().GetLength());
}

void CIntergenicParameters::SetSeqLen(int seqlen) const
{
    double lnthrough = m_intergeniclen.Through(seqlen);
    m_lnDen = log(m_initp);
    m_lnThrough = (lnthrough == BadScore()) ? BadScore() : m_lnDen+lnthrough;
    
    m_initialised = true;
}

CHMMParameters::SDetails::~SDetails()
{
    DeleteAllCreatedModels();
}
void CHMMParameters::SDetails::DeleteAllCreatedModels()
{
    ITERATE(vector<CInputModel*>, i, all_created_models)
        delete *i;
    all_created_models.clear();
    params.clear();
}

CHMMParameters::SDetails::TCGContentList& CHMMParameters::SDetails::GetCGList(const string& type)
{
    TParamMap::iterator i = params.insert(make_pair(type,TCGContentList())).first;
    if (i->second.empty())
        i->second.push_back(make_pair<int,CInputModel*>(101,NULL));
    
    return i->second;
}

void CHMMParameters::SDetails::StoreParam( const string& type, CInputModel* input_model, int low, int high )
{
    TCGContentList& list = GetCGList(type);
    
    int lowest = 0;
    TCGContentList::iterator i = list.begin();
    while( i->first <= low ) {
        lowest = i->first;
        ++i;
    }
    
    if (lowest < low) {
        i = list.insert(i, *i );
        i->first = low;
        ++i;
    }
    
    if (high < i->first) {
        i = list.insert(i, *i );
        i->first = high;
    } else if (high > i->first) {
        CInputModel::Error(type);
    }
    
    i->second = input_model;
}

CHMMParameters::CHMMParameters(const CGnomon_params& hmm_params_asn) : m_details( new SDetails(hmm_params_asn) )
{
}

CHMMParameters::CHMMParameters(CNcbiIstream& hmm_params_istr, ESerialDataFormat format)
{
    auto_ptr<CObjectIStream> inp(CObjectIStream::Open(format,hmm_params_istr));
    CRef<CGnomon_params> params_asn(new CGnomon_params);
    *inp >> *params_asn;
    m_details.Reset( new SDetails(*params_asn) );
}

CHMMParameters::~CHMMParameters()
{
}

template <class CParam>
void CHMMParameters::SDetails::ReadParameters(const CGnomon_params& hmm_params_asn, CGnomon_param::C_Param::E_Choice choice)
{
    ITERATE(CGnomon_params::Tdata, p, hmm_params_asn.Get()) {
        if ((*p)->GetParam().Which() != choice)
            continue;

        int low = (*p)->GetGc_content_range().GetFrom();
        int high = (*p)->GetGc_content_range().GetTo();
        if (!( 0<=low && low < high && high <= 100) )
            CInputModel::Error(CParam::class_id());

        CInputModel* input_model( new CParam((*p)->GetParam()) );
        if (input_model==NULL)
            continue;
        
        all_created_models.push_back(input_model);

        StoreParam(CParam::class_id(), input_model, low, high);
    }
}

CHMMParameters::SDetails::SDetails(const CGnomon_params& hmm_params_asn)
{
    DeleteAllCreatedModels();

    ReadParameters<CWAM_Donor<2> >(hmm_params_asn, CGnomon_param::C_Param::e_Donor);
    ReadParameters<CWAM_Acceptor<2> >(hmm_params_asn, CGnomon_param::C_Param::e_Acceptor);
    ReadParameters<CWMM_Start>(hmm_params_asn, CGnomon_param::C_Param::e_Start);
    ReadParameters<CWAM_Stop>(hmm_params_asn, CGnomon_param::C_Param::e_Stop);
    ReadParameters<CMC3_CodingRegion<5> >(hmm_params_asn, CGnomon_param::C_Param::e_Coding_region);
    ReadParameters<CMC_NonCodingRegion<5> >(hmm_params_asn, CGnomon_param::C_Param::e_Non_coding_region);
    ReadParameters<CIntronParameters>(hmm_params_asn, CGnomon_param::C_Param::e_Intron);
    ReadParameters<CIntergenicParameters>(hmm_params_asn, CGnomon_param::C_Param::e_Intergenic);
    ReadParameters<CExonParameters>(hmm_params_asn, CGnomon_param::C_Param::e_Exon);

}

const CInputModel& CHMMParameters::GetParameter(const string& type, int cgcontent) const
{
    return m_details->GetParameter(type, cgcontent);
} 

const CInputModel& CHMMParameters::SDetails::GetParameter(const string& type, int cgcontent) const
{
    if (cgcontent < 0)
        cgcontent = 0;
    else if (cgcontent >= 100)
        cgcontent = 99;

    TParamMap::const_iterator i_param = params.find(type);
    if (i_param == params.end())
        CInputModel::Error( type );
    
    ITERATE( TCGContentList, i, i_param->second) {
        if (cgcontent < i->first) {
            if (i->second == NULL) {
                CInputModel::Error( type );
            }
            return *i->second;
        }
    }

    _ASSERT( false);
    CInputModel::Error( type );
    return *params.begin()->second.front().second;
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
