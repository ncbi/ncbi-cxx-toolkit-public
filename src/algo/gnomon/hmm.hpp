#ifndef ALGO_GNOMON___HMM__HPP
#define ALGO_GNOMON___HMM__HPP

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
#include "gnomon_seq.hpp"
#include "score.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

extern const double          kLnHalf;
extern const double          kLnThree;
       const int             kTooFarLen = 500;


template<int order> class CMarkovChain
{
    public:
        typedef CMarkovChain<order> Type;
        void InitScore(CNcbiIfstream& from);
        double& Score(const EResidue* seq) { return m_next[(int)*(seq-order)].Score(seq); }
        const double& Score(const EResidue* seq) const { return m_next[(int)*(seq-order)].Score(seq); }
        CMarkovChain<order-1>& SubChain(int i) { return m_next[i]; }
        void Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3);
        void toScore();
    
    private:
        friend class CMarkovChain<order+1>;
        void Init(CNcbiIfstream& from);
        
        CMarkovChain<order-1> m_next[5];
};

template<> class CMarkovChain<0>
{
    public:
        typedef CMarkovChain<0> Type;
        void InitScore(CNcbiIfstream& from);
        double& Score(const EResidue* seq) { return m_score[(int)*seq]; }
        const double& Score(const EResidue* seq) const { return m_score[(int)*seq]; }
        double& SubChain(int i) { return m_score[i]; }
        void Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3);
        void toScore();
    private:
        friend class CMarkovChain<1>;
        void Init(CNcbiIfstream& from);
        
        double m_score[5];
};

template<int order> class CMarkovChainArray
{
    public:
        void InitScore(int l, CNcbiIfstream& from);
        double Score(const EResidue* seq) const;
    private:
        int m_length;
        vector< CMarkovChain<order> > m_mc;
};

class CInputModel
{
    public:
        virtual ~CInputModel() = 0;
    
    protected:
        static void Error(const string& label) 
        { 
            NCBI_THROW(CGnomonException, eGenericError, label+" initialisation error");
        }
        static pair<int,int> FindContent(CNcbiIfstream& from, const string& label, int cgcontent);
};

//Terminal's score is located on the last position of the left state
class CTerminal : public CInputModel
{
    public:
        int InExon() const { return m_inexon; }
        int InIntron() const { return m_inintron; }
        int Left() const { return m_left; }
        int Right() const { return m_right; }
        virtual double Score(const CEResidueVec& seq, int i) const = 0;
        ~CTerminal() {}
    
    protected:
        int m_inexon, m_inintron, m_left, m_right;
};


class CMDD_Donor : public CTerminal
{
    public:
        CMDD_Donor(const string& file, int cgcontent);
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        TIVec m_position, m_consensus;
        vector< CMarkovChainArray<0> > m_matrix;
};

template<int order> class CWAM_Donor : public CTerminal
{
    public:
        CWAM_Donor(const string& file, int cgcontent);
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChainArray<order> m_matrix;
};

template<int order> class CWAM_Acceptor : public CTerminal
{
    public:
        CWAM_Acceptor(const string& file, int cgcontent);
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChainArray<order> m_matrix;
        
};

class CWMM_Start : public CTerminal
{
    public:
        CWMM_Start(const string& file, int cgcontent);
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChainArray<0> m_matrix;
};

class CWAM_Stop : public CTerminal
{
    public:
        CWAM_Stop(const string& file, int cgcontent);
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChainArray<1> m_matrix;
};

class CCodingRegion : public CInputModel
{
    public:
        virtual double Score(const CEResidueVec& seq, int i, int codonshift) const = 0;
        ~CCodingRegion() {}
};

template<int order> class CMC3_CodingRegion : public CCodingRegion
{
    public:
        CMC3_CodingRegion(const string& file, int cgcontent);
        double Score(const CEResidueVec& seq, int i, int codonshift) const;
    
    private:
        CMarkovChain<order> m_matrix[3];
};

class CNonCodingRegion : public CInputModel
{
    public:
        virtual double Score(const CEResidueVec& seq, int i) const = 0;
        ~CNonCodingRegion() {}
};

template<int order> class CMC_NonCodingRegion : public CNonCodingRegion
{
    public:
        CMC_NonCodingRegion(const string& file, int cgcontent);
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChain<order> m_matrix;
};

class CNullRegion : public CNonCodingRegion
{
    public:
        double Score(const CEResidueVec& seq, int i) const { return 0; };
};

struct SStateScores
{
    double m_score,m_branch,m_length,m_region,m_term;
};

template<class State> SStateScores CalcStateScores(const State& st)
{
    SStateScores sc;
    
    if(st.NoLeftEnd())
    {
        if(st.NoRightEnd()) sc.m_length = st.ThroughLengthScore();
        else sc.m_length = st.InitialLengthScore();
    }
    else
    {
        if(st.NoRightEnd()) sc.m_length = st.ClosingLengthScore();
        else sc.m_length = st.LengthScore();
    }
    
    sc.m_region = st.RgnScore();
    sc.m_term = st.TermScore();
    if(sc.m_term == BadScore()) sc.m_term = 0;
    sc.m_score = st.Score();
    if(st.LeftState()) sc.m_score -= st.LeftState()->Score();
    sc.m_branch = sc.m_score-sc.m_length-sc.m_region-sc.m_term;
    
    return sc;
}

class CLorentz
{
    public:
        bool Init(CNcbiIstream& from, const string& label);
        double Score(int l) const { return m_score[(l-1)/m_step]; }
        double ClosingScore(int l) const;
        int MinLen() const { return m_minl; }
        int MaxLen() const { return m_maxl; }
        double AvLen() const { return m_avlen; }
        double Through(int seqlen) const;
    
    private:
        int m_minl, m_maxl, m_step;
        double m_A, m_L, m_avlen, m_lnthrough;
        TDVec m_score, m_clscore;
};

class CSeqScores;

class CHMM_State : public CInputModel
{
    public:
        CHMM_State(EStrand strn, int point);
        const CHMM_State* LeftState() const { return m_leftstate; }
        const CTerminal* TerminalPtr() const { return m_terminal; }
        void UpdateLeftState(const CHMM_State& left) { m_leftstate = &left; }
        void ClearLeftState() { m_leftstate = 0; }
        void UpdateScore(double scr) { m_score = scr; }
        int MaxLen() const { return numeric_limits<int>::max(); };
        int MinLen() const;
        bool StopInside() const { return false; }
        EStrand Strand() const { return m_strand; }
        bool isPlus() const { return (m_strand == ePlus); }
        bool isMinus() const { return (m_strand == eMinus); }
        double Score() const { return m_score; }
        int Start() const { return m_leftstate ? m_leftstate->m_stop+1 : 0; }
        bool NoRightEnd() const { return m_stop < 0; }
        bool NoLeftEnd() const { return m_leftstate == 0; }
    int Stop() const { return NoRightEnd() ? sm_seqscr->SeqLen()-1 : m_stop; }
        int RegionStart() const;
        int RegionStop() const;
        virtual SStateScores GetStateScores() const = 0;
        virtual string GetStateName() const = 0;
        static void SetSeqScores(const CSeqScores& s) { sm_seqscr = &s; }
        static const CSeqScores* GetSeqScores() { return sm_seqscr; }
        
protected:
    int m_stop;
    EStrand m_strand;
    double m_score;
    const CHMM_State* m_leftstate;
    const CTerminal* m_terminal;
    static const CSeqScores* sm_seqscr;
};

class CIntron; 
class CIntergenic;

class CExon : public CHMM_State
{
    public:
        static void Init(const string& file, int cgcontent);
        CExon(EStrand strn, int point, int ph) : CHMM_State(strn,point), m_phase(ph), 
                                                   m_prevexon(0), m_mscore(BadScore()) {}
        int Phase() const { return m_phase; }  // frame of right exon end relatively start-codon
        bool StopInside() const;
        bool OpenRgn() const;
        double RgnScore() const;
        double DenScore() const { return 0; }
        double ThroughLengthScore() const { return BadScore(); } 
        double InitialLengthScore() const { return BadScore(); }
        double ClosingLengthScore() const { return BadScore(); }
        void UpdatePrevExon(const CExon& e);
        double MScore() const { return m_mscore; }
        
        
    protected:
        int m_phase;
        const CExon* m_prevexon;
        double m_mscore;

        static double sm_firstphase[3], sm_internalphase[3][3];
        static CLorentz sm_firstlen, sm_internallen, sm_lastlen, sm_singlelen; 
        static bool sm_initialised;
};

class CSingleExon : public CExon
{
    public:
        ~CSingleExon() {}
        CSingleExon(EStrand strn, int point);
        int MaxLen() const { return sm_singlelen.MaxLen(); }
        int MinLen() const { return sm_singlelen.MinLen(); }
        const CSingleExon* PrevExon() const { return static_cast<const CSingleExon*>(m_prevexon); }
        double LengthScore() const; 
        double TermScore() const;
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CIntergenic& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "SingleExon"; }
};

class CFirstExon : public CExon
{
    public:
        ~CFirstExon() {}
        CFirstExon(EStrand strn, int ph, int point);
        int MaxLen() const { return sm_firstlen.MaxLen(); }
        int MinLen() const { return sm_firstlen.MinLen(); }
        const CFirstExon* PrevExon() const { return static_cast<const CFirstExon*>(m_prevexon); }
        double LengthScore() const;
        double TermScore() const;
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CIntron& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "FirstExon"; }
};

class CInternalExon : public CExon
{
    public:
        ~CInternalExon() {}
        CInternalExon(EStrand strn, int ph, int point);
        int MaxLen() const { return sm_internallen.MaxLen(); }
        int MinLen() const { return sm_internallen.MinLen(); }
        const CInternalExon* PrevExon() const { return static_cast<const CInternalExon*>(m_prevexon); }
        double LengthScore() const; 
        double TermScore() const;
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CIntron& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "InternalExon"; }
};

class CLastExon : public CExon
{
    public:
        ~CLastExon() {}
        CLastExon(EStrand strn, int ph, int point);
        int MaxLen() const { return sm_lastlen.MaxLen(); }
        int MinLen() const { return sm_lastlen.MinLen(); }
        const CLastExon* PrevExon() const { return static_cast<const CLastExon*>(m_prevexon); }
        double LengthScore() const; 
        double TermScore() const;
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CIntergenic& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "LastExon"; }
};

class CIntron : public CHMM_State
{
public:
    static void Init(const string& file, int cgcontent, int seqlen);
    
    ~CIntron() {}
    CIntron(EStrand strn, int ph, int point);
    static int MinIntron() { return sm_intronlen.MinLen(); }   // used for introducing frameshifts  
    static int MaxIntron() { return sm_intronlen.MaxLen(); }
    static double ChanceOfIntronLongerThan(int l) { return exp(sm_intronlen.ClosingScore(l)); } // used to eliminate "holes" in protein alignments
    int MinLen() const { return sm_intronlen.MinLen(); }
    int MaxLen() const { return sm_intronlen.MaxLen(); }
    int Phase() const { return m_phase; }
    bool OpenRgn() const { return sm_seqscr->OpenNonCodingRegion(Start(),Stop(),Strand()); }
    double RgnScore() const { return 0; }   // Intron scores are substructed from all others    
    double TermScore() const
    {
        if(isPlus()) return sm_seqscr->AcceptorScore(Stop(),Strand());
        else return sm_seqscr->DonorScore(Stop(),Strand());
    }
    double DenScore() const { return sm_lnDen[Phase()]; }
    double LengthScore() const
    {
        if(SplittedStop()) return BadScore();
        else return sm_intronlen.Score(Stop()-Start()+1);
    }
    double ClosingLengthScore() const;
    double ThroughLengthScore() const  { return sm_lnThrough[Phase()]; }
    double InitialLengthScore() const { return sm_lnDen[Phase()]+ClosingLengthScore(); }  // theoretically we should substract log(AvLen) but it gives to much penalty to the first element 
    double BranchScore(const CHMM_State& next) const { return BadScore(); }
    double BranchScore(const CLastExon& next) const;
    double BranchScore(const CInternalExon& next) const;
    bool SplittedStop() const
    {
        if(Phase() == 0 || NoLeftEnd() || NoRightEnd())
            return false;
        else if (isPlus())
            return sm_seqscr->SplittedStop(LeftState()->Stop(),Stop(),Strand(),Phase()-1);
        else
            return sm_seqscr->SplittedStop(Stop(),LeftState()->Stop(),Strand(),Phase()-1);
    }
    
    
    SStateScores GetStateScores() const { return CalcStateScores(*this); }
    string GetStateName() const { return "Intron"; }
    
protected:
    int m_phase;
    static double sm_lnThrough[3], sm_lnDen[3];
    static double sm_lnTerminal, sm_lnInternal;
    static CLorentz sm_intronlen;
    static bool sm_initialised;
};

inline double CIntron::BranchScore(const CInternalExon& next) const
{
    if(Strand() != next.Strand()) return BadScore();

    if(isPlus())
    {
        int shift = next.Stop()-next.Start();
        if((Phase()+shift)%3 == next.Phase()) return sm_lnInternal;
    }
    else if(Phase() == next.Phase()) return sm_lnInternal;
            
    return BadScore();
}

inline double CInternalExon::BranchScore(const CIntron& next) const
{
    if(Strand() != next.Strand()) return BadScore();

    int ph = isPlus() ? Phase() : Phase()+Stop()-Start();

    if((ph+1)%3 == next.Phase()) return 0;
    else return BadScore();
}

class CIntergenic : public CHMM_State
{
    public:
        static void Init(const string& file, int cgcontent, int seqlen);
        
        ~CIntergenic() {}
        CIntergenic(EStrand strn, int point);
        static int MinIntergenic() { return sm_intergeniclen.MinLen(); }   // used for making walls
        bool OpenRgn() const;
        double RgnScore() const;
        double TermScore() const;
        double DenScore() const { return sm_lnDen; }
        double LengthScore() const { return sm_intergeniclen.Score(Stop()-Start()+1); }
        double ClosingLengthScore() const { return sm_intergeniclen.ClosingScore(Stop()-Start()+1); }
        double ThroughLengthScore() const { return sm_lnThrough; }
        double InitialLengthScore() const { return sm_lnDen+ClosingLengthScore(); }  // theoretically we should substract log(AvLen) but it gives to much penalty to the first element
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CFirstExon& next) const; 
        double BranchScore(const CSingleExon& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "Intergenic"; }

    protected:
        static double sm_lnThrough, sm_lnDen;
        static double sm_lnSingle, sm_lnMulti;
        static CLorentz sm_intergeniclen;
        static bool sm_initialised;
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___HMM__HPP
