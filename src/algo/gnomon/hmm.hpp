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
        void InitScore(CNcbiIstream& from);
        double& Score(const EResidue* seq) { return m_next[(int)*(seq-order)].Score(seq); }
        const double& Score(const EResidue* seq) const { return m_next[(int)*(seq-order)].Score(seq); }
        CMarkovChain<order-1>& SubChain(int i) { return m_next[i]; }
        void Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3);
        void toScore();
    
    private:
        friend class CMarkovChain<order+1>;
        void Init(CNcbiIstream& from);
        
        CMarkovChain<order-1> m_next[5];
};

template<> class CMarkovChain<0>
{
    public:
        typedef CMarkovChain<0> Type;
        void InitScore(CNcbiIstream& from);
        double& Score(const EResidue* seq) { return m_score[(int)*seq]; }
        const double& Score(const EResidue* seq) const { return m_score[(int)*seq]; }
        double& SubChain(int i) { return m_score[i]; }
        void Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3);
        void toScore();
    private:
        friend class CMarkovChain<1>;
        void Init(CNcbiIstream& from);
        
        double m_score[5];
};

template<int order> class CMarkovChainArray
{
    public:
        void InitScore(int l, CNcbiIstream& from);
        double Score(const EResidue* seq) const;
    private:
        int m_length;
        vector< CMarkovChain<order> > m_mc;
};

class CInputModel
{
    public:
        virtual ~CInputModel() = 0;
    
        static void Error(const string& label) 
        { 
            NCBI_THROW(CGnomonException, eGenericError, label+" initialisation error");
        }
};

//Terminal's score is located on the last position of the left state
class CTerminal : public CInputModel
{
    public:
        ~CTerminal() {}
        int InExon() const { return m_inexon; }
        int InIntron() const { return m_inintron; }
        int Left() const { return m_left; }
        int Right() const { return m_right; }
        virtual double Score(const CEResidueVec& seq, int i) const = 0;
    
    protected:
        int m_inexon, m_inintron, m_left, m_right;
};


class CMDD_Donor : public CTerminal
{
public:
    static string class_id() { return "MDD_Donor"; }
    CMDD_Donor(CNcbiIstream& from);
    ~CMDD_Donor() {}
    double Score(const CEResidueVec& seq, int i) const;
    
private:
    TIVec m_position, m_consensus;
    vector< CMarkovChainArray<0> > m_matrix;
};

template<int order> class CWAM_Donor : public CTerminal
{
    public:
    ~CWAM_Donor() {}
    static string class_id() { return "WAM_Donor_"+NStr::IntToString(order); }
        CWAM_Donor(CNcbiIstream& from);
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChainArray<order> m_matrix;
};

template<int order> class CWAM_Acceptor : public CTerminal
{
    public:
    static string class_id() { return "WAM_Acceptor_"+NStr::IntToString(order); }
        CWAM_Acceptor(CNcbiIstream& from);
    ~CWAM_Acceptor() {}
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChainArray<order> m_matrix;
        
};

class CWMM_Start : public CTerminal
{
    public:
    static string class_id() { return "WMM_Start"; }
        CWMM_Start(CNcbiIstream& from);
    ~CWMM_Start() {}
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChainArray<0> m_matrix;
};

class CWAM_Stop : public CTerminal
{
    public:
    static string class_id() { return "WAM_Stop_1"; }
        CWAM_Stop(CNcbiIstream& from);
    ~CWAM_Stop() {}
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChainArray<1> m_matrix;
};

class CCodingRegion : public CInputModel
{
    public:
    static string class_id() { return "CodingRegion"; }
        virtual double Score(const CEResidueVec& seq, int i, int codonshift) const = 0;
        ~CCodingRegion() {}
};

template<int order> class CMC3_CodingRegion : public CCodingRegion
{
    public:
    static string class_id() { return "MC3_CodingRegion_"+NStr::IntToString(order); }
        CMC3_CodingRegion(CNcbiIstream& from);
    ~CMC3_CodingRegion() {}
        double Score(const CEResidueVec& seq, int i, int codonshift) const;
    
    private:
        CMarkovChain<order> m_matrix[3];
};

class CNonCodingRegion : public CInputModel
{
    public:
    static string class_id() { return "NonCodingRegion"; }
        virtual double Score(const CEResidueVec& seq, int i) const = 0;
        ~CNonCodingRegion() {}
};

template<int order> class CMC_NonCodingRegion : public CNonCodingRegion
{
    public:
    static string class_id() { return "MC_NonCodingRegion_"+NStr::IntToString(order); }
        CMC_NonCodingRegion(CNcbiIstream& from);
    ~CMC_NonCodingRegion() {}
        double Score(const CEResidueVec& seq, int i) const;
    
    private:
        CMarkovChain<order> m_matrix;
};

class CNullRegion : public CNonCodingRegion
{
    public:
    ~CNullRegion() {}
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

class CHMM_State {
public:
    CHMM_State(EStrand strn, int point);
    virtual ~CHMM_State() {}
    const CHMM_State* LeftState() const { return m_leftstate; }
    const CTerminal* TerminalPtr() const { return m_terminal; }
    void UpdateLeftState(const CHMM_State& left) { m_leftstate = &left; }
    void ClearLeftState() { m_leftstate = 0; }
    void UpdateScore(double scr) { m_score = scr; }
    int MaxLen() const { return numeric_limits<int>::max(); }
    int MinLen() const { return 1; };
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

    virtual bool isExon() const { return false; }
    virtual bool isGeneLeftEnd() const { return false; }
    virtual bool isGeneRightEnd() const { return false; }
    virtual double VirtTermScore() const = 0; 
        
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

class CExonParameters: public CInputModel {
public:
    static string class_id() { return "Exon"; }
    CExonParameters(CNcbiIstream& from);
    ~CExonParameters() {}

    double m_firstphase[3], m_internalphase[3][3];
    CLorentz m_firstlen, m_internallen, m_lastlen, m_singlelen; 
    bool m_initialised;
};

class CExon : public CHMM_State
{
    public:
    static const CExonParameters& SetParams(const CExonParameters& params) { sm_param = &params; return params; }
    CExon(EStrand strn, int point, int ph);
    virtual ~CExon() {}

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
        
    virtual bool isExon() const { return true; }
    double ExonScore() const { return LeftState()->VirtTermScore() + VirtTermScore(); }
        
    protected:
        int m_phase;
        const CExon* m_prevexon;
        double m_mscore;

        static const CExonParameters* sm_param;
};

class CSingleExon : public CExon
{
    public:
        ~CSingleExon() {}
        CSingleExon(EStrand strn, int point);
        int MaxLen() const { return sm_param->m_singlelen.MaxLen(); }
        int MinLen() const { return sm_param->m_singlelen.MinLen(); }
        const CSingleExon* PrevExon() const { return static_cast<const CSingleExon*>(m_prevexon); }
        double LengthScore() const; 
        double TermScore() const;
    virtual double VirtTermScore() const { return TermScore(); }
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CIntergenic& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "SingleExon"; }

    virtual bool isGeneLeftEnd() const { return true; }
    virtual bool isGeneRightEnd() const { return true; }
};

class CFirstExon : public CExon
{
    public:
        ~CFirstExon() {}
        CFirstExon(EStrand strn, int ph, int point);
        int MaxLen() const { return sm_param->m_firstlen.MaxLen(); }
        int MinLen() const { return sm_param->m_firstlen.MinLen(); }
        const CFirstExon* PrevExon() const { return static_cast<const CFirstExon*>(m_prevexon); }
        double LengthScore() const;
        double TermScore() const;
    virtual double VirtTermScore() const { return TermScore(); }
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CIntron& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "FirstExon"; }

    virtual bool isGeneLeftEnd() const { return isPlus(); }
    virtual bool isGeneRightEnd() const { return isMinus(); }
};

class CInternalExon : public CExon
{
    public:
        ~CInternalExon() {}
        CInternalExon(EStrand strn, int ph, int point);
        int MaxLen() const { return sm_param->m_internallen.MaxLen(); }
        int MinLen() const { return sm_param->m_internallen.MinLen(); }
        const CInternalExon* PrevExon() const { return static_cast<const CInternalExon*>(m_prevexon); }
        double LengthScore() const; 
        double TermScore() const;
    virtual double VirtTermScore() const { return TermScore(); }
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
        int MaxLen() const { return sm_param->m_lastlen.MaxLen(); }
        int MinLen() const { return sm_param->m_lastlen.MinLen(); }
        const CLastExon* PrevExon() const { return static_cast<const CLastExon*>(m_prevexon); }
        double LengthScore() const; 
        double TermScore() const;
    virtual double VirtTermScore() const { return TermScore(); }
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CIntergenic& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "LastExon"; }

    virtual bool isGeneLeftEnd() const { return isMinus(); }
    virtual bool isGeneRightEnd() const { return isPlus(); }
};

class CIntronParameters : public CInputModel {
public:
    static string class_id() { return "Intron"; }
    CIntronParameters(CNcbiIstream& from);
    ~CIntronParameters() {}

    void SetSeqLen(int seqlen) const;

    mutable double m_lnThrough[3];
    mutable double m_lnDen[3];
    double m_lnTerminal, m_lnInternal;
    CLorentz m_intronlen;
private:
    double m_initp, m_phasep[3];
    mutable bool m_initialised;

    friend class CIntron;
};

class CIntron : public CHMM_State
{
public:
    static const CIntronParameters& SetParams(const CIntronParameters& params) { sm_param = &params; return params; }
        
    virtual ~CIntron() {}
    CIntron(EStrand strn, int ph, int point);
    static int MinIntron() { return sm_param->m_intronlen.MinLen(); }   // used for introducing frameshifts
    static int MaxIntron() { return sm_param->m_intronlen.MaxLen(); }
    static double ChanceOfIntronLongerThan(int l) { return exp(sm_param->m_intronlen.ClosingScore(l)); } // used to eliminate "holes" in protein alignments
    int MinLen() const { return sm_param->m_intronlen.MinLen(); }
    int MaxLen() const { return sm_param->m_intronlen.MaxLen(); }
    int Phase() const { return m_phase; }
    bool OpenRgn() const { return sm_seqscr->OpenNonCodingRegion(Start(),Stop(),Strand()); }
    double RgnScore() const { return 0; }   // Intron scores are substructed from all others
    double TermScore() const
    {
        if(isPlus()) return sm_seqscr->AcceptorScore(Stop(),Strand());
        else return sm_seqscr->DonorScore(Stop(),Strand());
    }
    virtual double VirtTermScore() const { return TermScore(); }
    double DenScore() const { return sm_param->m_lnDen[Phase()]; }
    double LengthScore() const
    {
        if(SplittedStop()) return BadScore();
        else return sm_param->m_intronlen.Score(Stop()-Start()+1);
    }
    double ClosingLengthScore() const;
    double ThroughLengthScore() const  { return sm_param->m_lnThrough[Phase()]; }
    double InitialLengthScore() const { return sm_param->m_lnDen[Phase()]+ClosingLengthScore(); }  // theoretically we should substract log(AvLen) but it gives to much penalty to the first element
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
    static const CIntronParameters* sm_param;
};

inline double CIntron::BranchScore(const CInternalExon& next) const
{
    if(Strand() != next.Strand()) return BadScore();

    if(isPlus()) {
        int shift = next.Stop()-next.Start();
        if((Phase()+shift)%3 == next.Phase())
            return sm_param->m_lnInternal;
    } else if(Phase() == next.Phase())
        return sm_param->m_lnInternal;
            
    return BadScore();
}

inline double CInternalExon::BranchScore(const CIntron& next) const
{
    if(Strand() != next.Strand()) return BadScore();

    int ph = isPlus() ? Phase() : Phase()+Stop()-Start();

    if((ph+1)%3 == next.Phase()) return 0;
    else return BadScore();
}

class CIntergenicParameters : public CInputModel {
public:
    static string class_id() { return "Intergenic"; }
    CIntergenicParameters(CNcbiIstream& from);
    ~CIntergenicParameters() {}

    void SetSeqLen(int seqlen) const;

    mutable double m_lnThrough, m_lnDen;
    double m_lnSingle, m_lnMulti;
    CLorentz m_intergeniclen;
private:
    double m_initp;
    mutable bool m_initialised;
    friend class CIntergenic;
};

class CIntergenic : public CHMM_State
{
    public:
    static const CIntergenicParameters& SetParams(const CIntergenicParameters& params) { sm_param = &params; return params; }
        
    virtual ~CIntergenic() {}
        CIntergenic(EStrand strn, int point);
        static int MinIntergenic() { return sm_param->m_intergeniclen.MinLen(); }   // used for making walls
        bool OpenRgn() const;
        double RgnScore() const;
        double TermScore() const;
    virtual double VirtTermScore() const { return TermScore(); }
        double DenScore() const { return sm_param->m_lnDen; }
        double LengthScore() const { return sm_param->m_intergeniclen.Score(Stop()-Start()+1); }
        double ClosingLengthScore() const { return sm_param->m_intergeniclen.ClosingScore(Stop()-Start()+1); }
        double ThroughLengthScore() const { return sm_param->m_lnThrough; }
        double InitialLengthScore() const { return sm_param->m_lnDen+ClosingLengthScore(); }  // theoretically we should substract log(AvLen) but it gives to much penalty to the first element
        double BranchScore(const CHMM_State& next) const { return BadScore(); }
        double BranchScore(const CFirstExon& next) const; 
        double BranchScore(const CSingleExon& next) const;
        SStateScores GetStateScores() const { return CalcStateScores(*this); }
        string GetStateName() const { return "Intergenic"; }

    protected:
        static const CIntergenicParameters* sm_param;
};

// Singleton Factory
class COrgParameters {
public:
    static COrgParameters& Instance();

    void Read(CNcbiIstream& from);
    const CInputModel& GetParameter(const string& type, int cgcontent);

    typedef CInputModel* (*TParameterCreator)(CNcbiIstream& from);
    bool RegisterParameter(const string& type, TParameterCreator creator);

private:
    COrgParameters();

    struct SDetails;
    auto_ptr<SDetails> m_details;
};

#define    REGISTER_ORG_PARAMETER(Class) \
CInputModel* Create##Class(CNcbiIstream& from) { return new Class(from); } \
const bool Class##_registered = COrgParameters::Instance().RegisterParameter(Class::class_id(),Create##Class)

#define    REGISTER_ORG_TMPL_PARAMETER(Class,Order) \
CInputModel* Create##Class##Order(CNcbiIstream& from) { return new Class<Order>(from); } \
const bool Class##Order##_registered = COrgParameters::Instance().RegisterParameter(Class<Order>::class_id(),Create##Class##Order)

#define  GET_ORG_PARAMETER(C,gc) dynamic_cast<const C*>( &COrgParameters::Instance().GetParameter(C::class_id(), (gc)))

END_SCOPE(gnomon)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3.2.2  2006/10/12 19:08:22  chetvern
 * Changed hmm parameters reading
 *
 * Revision 1.3.2.1  2006/10/06 14:19:36  chetvern
 * Major overhaul. Single format for intermediate files.
 *
 * Revision 1.5  2006/10/05 15:32:05  souvorov
 * Implementation of anchors for intergenics
 *
 * Revision 1.4  2006/03/06 15:52:53  souvorov
 * Changes needed for ChanceOfIntronLongerThan(int l)
 *
* Revision 1.3  2005/10/06 15:51:43  chetvern
 * moved TDVec definition from hmm.hpp to gnomon_seq.hpp
 * moved several most frequently called methods implementations into class definitions to make them inline
 *
 * Revision 1.2  2005/09/16 18:04:16  ucko
 * kBadScore has been replaced with an inline BadScore function that
 * always returns the same value to avoid lossage in optimized WorkShop
 * builds.
 *
 * Revision 1.1  2005/09/15 21:28:07  chetvern
 * Sync with Sasha's working tree
 *
 *
 * ===========================================================================
 */

#endif  // ALGO_GNOMON___HMM__HPP
