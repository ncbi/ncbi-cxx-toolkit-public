#ifndef __GENE_FINDER__HPP
#define __GENE_FINDER__HPP

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
#include <corelib/ncbi_limits.hpp>
#include <algo/gnomon/gnomon_exception.hpp>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <algorithm>
#include <math.h>


BEGIN_NCBI_SCOPE


typedef vector<char> CVec;
typedef vector<int> IVec;
typedef vector<double> DVec;

struct IPair : pair<int,int> 
{
    IPair(int f, int s) : pair<int,int>(f,s) {}
    bool operator<(const IPair& p) const { return second < p.first; }
    bool operator>(const IPair& p) const { return first > p.second; }
    bool Intersect(const IPair& p) const { return !(*this < p || *this > p); }
    bool Include(int i) const { return (i >= first && i <= second); }
};

enum Nucleotides { nA, nC, nG, nT, nN };
enum { Plus, Minus };

extern const double BadScore;
extern const double LnHalf;
extern const double LnThree;
extern const int toMinus[5];
extern const int TooFarLen;
extern const char* aa_table;

template<int order> class MarkovChain
{
public:
    void InitScore(CNcbiIfstream& from);
    double Score(const int* seq) const { return next[*(seq-order)].Score(seq); }
private:
    typedef MarkovChain<order> Type;
    friend class MarkovChain<order+1>;
    void Init(CNcbiIfstream& from);
    void Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3);
    void toScore();

    MarkovChain<order-1> next[5];
};

template<> class MarkovChain<0>
{
public:
    void InitScore(CNcbiIfstream& from);
    double Score(const int* seq) const { return score[*seq]; }
private:
    typedef MarkovChain<0> Type;
    friend class MarkovChain<1>;
    void Init(CNcbiIfstream& from);
    void Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3);
    void toScore();

    double score[5];
};

template<int order> class MarkovChainArray
{
public:
    void InitScore(int l, CNcbiIfstream& from);
    double Score(const int* seq) const;
private:
    int length;
    vector< MarkovChain<order> > mc;
};

class InputModel
{
public:
    virtual ~InputModel() = 0;

protected:
    static void Error(const string& label) {
        NCBI_THROW(CGnomonException, eGenericError, label);
    }
    static pair<int,int> FindContent(CNcbiIfstream& from, const string& label, int cgcontent);
};

//Terminal's score is located on the last position of the left state
class Terminal : public InputModel
{
public:
    int InExon() const { return inexon; }
    int InIntron() const { return inintron; }
    int Left() const { return left; }
    int Right() const { return right; }
    virtual double Score(const IVec& seq, int i) const = 0;
    ~Terminal() {}

protected:
    int inexon, inintron, left, right;
};

class MDD_Donor : public Terminal
{
public:
    MDD_Donor(const string& file, int cgcontent);
    double Score(const IVec& seq, int i) const;

private:
    IVec position, consensus;
    vector< MarkovChainArray<0> > matrix;
};

template<int order> class WAM_Donor : public Terminal
{
public:
    WAM_Donor(const string& file, int cgcontent);
    double Score(const IVec& seq, int i) const;

private:
    MarkovChainArray<order> matrix;
};

template<int order> class WAM_Acceptor : public Terminal
{
public:
    WAM_Acceptor(const string& file, int cgcontent);
    double Score(const IVec& seq, int i) const;

private:
    MarkovChainArray<order> matrix;

};

class WMM_Start : public Terminal
{
public:
    WMM_Start(const string& file, int cgcontent);
    double Score(const IVec& seq, int i) const;

private:
    MarkovChainArray<0> matrix;
};

class WAM_Stop : public Terminal
{
public:
    WAM_Stop(const string& file, int cgcontent);
    double Score(const IVec& seq, int i) const;

private:
    MarkovChainArray<1> matrix;
};

class CodingRegion : public InputModel
{
public:
    virtual double Score(const IVec& seq, int i, int codonshift) const = 0;
    ~CodingRegion() {}
};

template<int order> class MC3_CodingRegion : public CodingRegion
{
public:
    MC3_CodingRegion(const string& file, int cgcontent);
    double Score(const IVec& seq, int i, int codonshift) const;

private:
    MarkovChain<order> matrix[3];
};

class NonCodingRegion : public InputModel
{
public:
    virtual double Score(const IVec& seq, int i) const = 0;
    ~NonCodingRegion() {}
};

template<int order> class MC_NonCodingRegion : public NonCodingRegion
{
public:
    MC_NonCodingRegion(const string& file, int cgcontent);
    double Score(const IVec& seq, int i) const;

private:
    MarkovChain<order> matrix;
};

class NullRegion : public NonCodingRegion
{
public:
    double Score(const IVec& seq, int i) const { return 0; };
};

class AlignVec : public vector<IPair>
{
public:
    typedef vector<IPair>::iterator It;
    typedef vector<IPair>::const_iterator ConstIt;

    enum { Prot, EST, mRNA, RefSeq, RefSeqBest, Wall};
    AlignVec(int s = Plus, int i = 0, int t = EST, IPair cdl = IPair(-1,-1)) : type(t), strand(s), id(i), 
    limits(numeric_limits<int>::max(),0), cds_limits(cdl), score(BadScore) {}
    void Insert(IPair p);
    void Erase(int i);
    IPair Limits() const { return limits; }
    IPair CdsLimits() const { return cds_limits; }
    void SetCdsLimits(IPair p) { cds_limits = p; }
    bool Intersect(const AlignVec& a) const 
    {
        return limits.Intersect(a.limits); 
    }
    void SetStrand(int s) { strand = s; }
    int Strand() const { return strand; }
    void SetType(int t) { type = t; }
    int Type() const { return type; }
    void SetID(int i) { id = i; }
    int ID() const { return id; }
    bool operator<(const AlignVec& a) const { return limits < a.limits; }
    void Init();
    void SetScore(double s) { score = s; }
    double Score() const { return score; }
    void Extend(const AlignVec& a); 

private:
    int type, strand, id;
    IPair limits, cds_limits;
    double score;
};

CNcbiIstream& operator>>(CNcbiIstream& s, AlignVec& a);
CNcbiOstream& operator<<(CNcbiOstream& s, const AlignVec& a);


class CCluster : public list<AlignVec>
{
public:
    typedef list<AlignVec>::iterator It;
    typedef list<AlignVec>::const_iterator ConstIt;
    CCluster(int f = numeric_limits<int>::max(), int s = 0, int t = AlignVec::Prot) : limits(f,s), type(t) {}
    void Insert(const AlignVec& a);
    void Insert(const CCluster& c);
    IPair Limits() const { return limits; }
    int Type() const { return type; }
    bool operator<(const CCluster& c) const { return limits < c.limits; }
    void Init(int first, int second, int t);

private:
    IPair limits;
    int type;
};

CNcbiIstream& operator>>(CNcbiIstream& s, CCluster& c);
CNcbiOstream& operator<<(CNcbiOstream& s, const CCluster& c);


class CClusterSet : public set<CCluster>
{
public:
    typedef set<CCluster>::iterator It;
    typedef set<CCluster>::const_iterator ConstIt;
    CClusterSet() {}
    CClusterSet(string c) : contig(c) {}
    void InsertAlignment(const AlignVec& a);
    void InsertCluster(CCluster c);
    string Contig() const { return contig; }
    void Init(string cnt);

private:
    string contig;
};

CNcbiIstream& operator>>(CNcbiIstream& s, CClusterSet& cls);
CNcbiOstream& operator<<(CNcbiOstream& s, const CClusterSet& cls);


class CFrameShiftInfo
{
public:
    CFrameShiftInfo(int l, bool is_i, char i_v = 0) : loc(l), is_insert(is_i), insert_value(i_v) {}
    int Loc() const { return loc; }
    bool IsInsertion() const { return is_insert; }
    bool IsDeletion() const { return !is_insert; }
    char InsertValue() const { return insert_value; }
    bool operator<(const CFrameShiftInfo& fsi) const { return loc < fsi.loc; }
private:
    int loc;  // location for deletion, insertion before location
    // incertion - when I INCERT a new base in sequence
    bool is_insert;
    char insert_value;
};

typedef vector<CFrameShiftInfo> TFrameShifts;


class SeqScores
{
public:
    SeqScores (Terminal& a, Terminal& d, Terminal& stt, Terminal& stp, 
               CodingRegion& cr, NonCodingRegion& ncr, NonCodingRegion& ing, 
               CVec& sequence, int from, int to, const CClusterSet& cls, 
               const TFrameShifts& initial_fshifts,	bool repeats, bool leftwall, 
               bool rightwall, string cntg);

    int Shift() const { return shift; }
    int AcceptorNumber(int strand) const { return anum[strand]; }
    int DonorNumber(int strand) const { return dnum[strand]; }
    int StartNumber(int strand) const { return sttnum[strand]; }
    int StopNumber(int strand) const { return stpnum[strand]; }
    double AcceptorScore(int i, int strand) const { return ascr[strand][i]; }
    double DonorScore(int i, int strand) const { return dscr[strand][i]; }
    double StartScore(int i, int strand) const { return sttscr[strand][i]; }
    double StopScore(int i, int strand) const { return stpscr[strand][i]; }
    Terminal& Acceptor() const { return acceptor; }
    Terminal& Donor() const { return donor; }
    Terminal& Start() const { return start; }
    Terminal& Stop() const { return stop; }
    const CClusterSet& Alignments() const { return cluster_set; }
    const TFrameShifts& SeqFrameShifts() const { return fshifts; }
    string Contig() const { return contig; }
    bool StopInside(int a, int b, int strand, int frame) const;
    bool OpenCodingRegion(int a, int b, int strand, int frame) const;
    double CodingScore(int a, int b, int strand, int frame) const;
    bool OpenNonCodingRegion(int a, int b, int strand) const;
    double NonCodingScore(int a, int b, int strand) const;
    bool OpenIntergenicRegion(int a, int b) const;
    bool InAlignment(int a, int b) const;
    double IntergenicScore(int a, int b, int strand) const;
    int SeqLen() const { return seq[0].size(); }
    bool SplittedStop(int id, int ia, int strand, int ph) const 
    { return (dsplit[strand][ph][id]&asplit[strand][ph][ia]) ? true : false; }
    bool isStart(int i, int strand) const;
    bool isStop(int i, int strand) const;
    bool isAG(int i, int strand) const;
    bool isGT(int i, int strand) const;
    bool isConsensusIntron(int i, int j, int strand) const;
    const int* SeqPtr(int i, int strand) const;
    int SeqMap(int i, bool forwrd) const;  // maps new coordinates to old coordinates,
    // if insertion gives next or previous point
    // depending on forwrd
    int RevSeqMap(int i, bool forwrd) const; // maps old coordinates to new coordinates, 
    // if deletion gives next or previous point
    // depending on forwrd

private:
    Terminal &acceptor, &donor, &start, &stop;
    CodingRegion &cdr;
    NonCodingRegion &ncdr, &intrg;
    const CClusterSet& cluster_set;
    TFrameShifts fshifts;
    IVec seq[2], laststop[2][3], notinexon[2][3], notinintron[2], notining;
    IVec seq_map, rev_seq_map;
    DVec ascr[2], dscr[2], sttscr[2], stpscr[2], ncdrscr[2], ingscr[2], cdrscr[2][3];
    IVec asplit[2][2], dsplit[2][2];
    IVec inalign;
    int anum[2], dnum[2], sttnum[2], stpnum[2];
    int shift;
    string contig;
};

struct StateScores
{
    double score,branch,length,region,term;
};

template<class State> StateScores CalcStateScores(const State& st)
{
    StateScores sc;

    if(st.NoLeftEnd())
    {
        if(st.NoRightEnd()) sc.length = st.ThroughLengthScore();
        else sc.length = st.InitialLengthScore();
    }
    else
    {
        if(st.NoRightEnd()) sc.length = st.ClosingLengthScore();
        else sc.length = st.LengthScore();
    }

    sc.region = st.RgnScore();
    sc.term = st.TermScore();
    if(sc.term == BadScore) sc.term = 0;
    sc.score = st.Score();
    if(st.LeftState()) sc.score -= st.LeftState()->Score();
    sc.branch = sc.score-sc.length-sc.region-sc.term;

    return sc;
}

class Lorentz
{
public:
    bool Init(CNcbiIstream& from, const string& label);
    double Score(int l) const { return score[(l-1)/step]; }
    double ClosingScore(int l) const;
    int MinLen() const { return minl; }
    int MaxLen() const { return maxl; }
    double AvLen() const { return avlen; }
    double Through(int seqlen) const;

private:
    int minl, maxl, step;
    double A, L, avlen, lnthrough;
    DVec score, clscore;
};

class HMM_State : public InputModel
{
public:
    HMM_State(int strn, int point);
    const HMM_State* LeftState() const { return leftstate; }
    const Terminal* TerminalPtr() const { return terminal; }
    void UpdateLeftState(const HMM_State& left) { leftstate = &left; }
    void UpdateScore(double scr) { score = scr; }
    int MaxLen() const { return numeric_limits<int>::max(); };
    int MinLen() const;
    bool StopInside() const { return false; }
    bool InAlignment() const { return false; }
    int Strand() const { return strand; }
    bool isPlus() const { return (strand == Plus); }
    bool isMinus() const { return (strand == Minus); }
    double Score() const { return score; }
    int Start() const { return leftstate ? leftstate->stop+1 : 0; }
    bool NoRightEnd() const { return stop < 0; }
    bool NoLeftEnd() const { return leftstate == 0; }
    int Stop() const  { return NoRightEnd() ? seqscr->SeqLen()-1 : stop; }
    int RegionStart() const;
    int RegionStop() const;
    virtual StateScores GetStateScores() const = 0;
    virtual string GetStateName() const = 0;
    static void SetSeqScores(const SeqScores& s) { seqscr = &s; }

protected:
    int stop, strand;
    double score;
    const HMM_State* leftstate;
    const Terminal* terminal;

    static const SeqScores* seqscr;
};

class Intron; 
class Intergenic;

class Exon : public HMM_State
{
public:
    static void Init(const string& file, int cgcontent);
    Exon(int strn, int point, int ph) : HMM_State(strn,point), phase(ph), 
    prevexon(0), mscore(BadScore) {}
    int Phase() const { return phase; }
    bool StopInside() const;
    bool OpenRgn() const;
    double RgnScore() const;
    double DenScore() const { return 0; }
    double ThroughLengthScore() const { return BadScore; } 
    double InitialLengthScore() const { return BadScore; }
    double ClosingLengthScore() const { return BadScore; }
    void UpdatePrevExon(const Exon& e);
    double MScore() const { return mscore; }


protected:
    int phase;
    const Exon* prevexon;
    double mscore;

    static double firstphase[3], internalphase[3][3];
    static Lorentz firstlen, internallen, lastlen, singlelen; 
    static bool initialised;
};

class SingleExon : public Exon
{
public:
    ~SingleExon() {}
    SingleExon(int strn, int point);
    int MaxLen() const { return singlelen.MaxLen(); }
    int MinLen() const { return singlelen.MinLen(); }
    const SingleExon* PrevExon() const { return static_cast<const SingleExon*>(prevexon); }
    double LengthScore() const; 
    double TermScore() const;
    double BranchScore(const HMM_State& next) const { return BadScore; }
    double BranchScore(const Intergenic& next) const;
    StateScores GetStateScores() const { return CalcStateScores(*this); }
    string GetStateName() const { return "SingleExon"; }
};

class FirstExon : public Exon
{
public:
    ~FirstExon() {}
    FirstExon(int strn, int ph, int point);
    int MaxLen() const { return firstlen.MaxLen(); }
    int MinLen() const { return firstlen.MinLen(); }
    const FirstExon* PrevExon() const { return static_cast<const FirstExon*>(prevexon); }
    double LengthScore() const;
    double TermScore() const;
    double BranchScore(const HMM_State& next) const { return BadScore; }
    double BranchScore(const Intron& next) const;
    StateScores GetStateScores() const { return CalcStateScores(*this); }
    string GetStateName() const { return "FirstExon"; }
};

class InternalExon : public Exon
{
public:
    ~InternalExon() {}
    InternalExon(int strn, int ph, int point);
    int MaxLen() const { return internallen.MaxLen(); }
    int MinLen() const { return internallen.MinLen(); }
    const InternalExon* PrevExon() const { return static_cast<const InternalExon*>(prevexon); }
    double LengthScore() const; 
    double TermScore() const;
    double BranchScore(const HMM_State& next) const { return BadScore; }
    double BranchScore(const Intron& next) const;
    StateScores GetStateScores() const { return CalcStateScores(*this); }
    string GetStateName() const { return "InternalExon"; }
};

class LastExon : public Exon
{
public:
    ~LastExon() {}
    LastExon(int strn, int ph, int point);
    int MaxLen() const { return lastlen.MaxLen(); }
    int MinLen() const { return lastlen.MinLen(); }
    const LastExon* PrevExon() const { return static_cast<const LastExon*>(prevexon); }
    double LengthScore() const; 
    double TermScore() const;
    double BranchScore(const HMM_State& next) const { return BadScore; }
    double BranchScore(const Intergenic& next) const;
    StateScores GetStateScores() const { return CalcStateScores(*this); }
    string GetStateName() const { return "LastExon"; }
};

class Intron : public HMM_State
{
public:
    static void Init(const string& file, int cgcontent, int seqlen);

    ~Intron() {}
    Intron(int strn, int ph, int point);
    static int MinIntron() { return intronlen.MinLen(); }   // used for introducing frameshifts
    int MinLen() const { return intronlen.MinLen(); }
    int MaxLen() const { return intronlen.MaxLen(); }
    int Phase() const { return phase; }
    bool OpenRgn() const;
    double RgnScore() const { return 0; }   // Intron scores are substructed from all others
    double TermScore() const;
    double DenScore() const { return lnDen[Phase()]; }
    double LengthScore() const;
    double ClosingLengthScore() const;
    double ThroughLengthScore() const  { return lnThrough[Phase()]; }
    double InitialLengthScore() const;
    double BranchScore(const HMM_State& next) const { return BadScore; }
    double BranchScore(const LastExon& next) const;
    double BranchScore(const InternalExon& next) const;
    bool SplittedStop() const;

    StateScores GetStateScores() const { return CalcStateScores(*this); }
    string GetStateName() const { return "Intron"; }

protected:
    int phase;
    static double lnThrough[3], lnDen[3];
    static double lnTerminal, lnInternal;
    static Lorentz intronlen;
    static bool initialised;
};

class Intergenic : public HMM_State
{
public:
    static void Init(const string& file, int cgcontent, int seqlen);

    ~Intergenic() {}
    Intergenic(int strn, int point);
    bool OpenRgn() const;
    double RgnScore() const;
    double TermScore() const;
    double DenScore() const { return lnDen; }
    double LengthScore() const { return intergeniclen.Score(Stop()-Start()+1); }
    double ClosingLengthScore() const { return intergeniclen.ClosingScore(Stop()-Start()+1); }
    double ThroughLengthScore() const { return lnThrough; }
    double InitialLengthScore() const { return lnDen+ClosingLengthScore(); }
    double BranchScore(const HMM_State& next) const { return BadScore; }
    double BranchScore(const FirstExon& next) const; 
    double BranchScore(const SingleExon& next) const;
    bool InAlignment() const; 
    StateScores GetStateScores() const { return CalcStateScores(*this); }
    string GetStateName() const { return "Intergenic"; }

protected:
    static double lnThrough, lnDen;
    static double lnSingle, lnMulti;
    static Lorentz intergeniclen;
    static bool initialised;
};

template<class T> class ParseVec : public vector<T>
{
public:
    ParseVec() : num(-1) {}
    int num;
};

class Gene;

class Parse
{
public:
    Parse(const SeqScores& ss);
    const HMM_State* Path() const { return path; }
    int PrintGenes(CNcbiOstream& to = cout, CNcbiOstream& toprot = cout,
                   bool complete = false) const;
    void PrintInfo() const;
    list<Gene> GetGenes() const;
    //typedef list<Gene>::iterator GenIt;

private:
    const SeqScores& seqscr;
    const HMM_State* path;

    ParseVec<Intergenic> igplus, igminus;
    ParseVec<Intron> inplus[3], inminus[3];
    ParseVec<FirstExon> feplus[3], feminus;
    ParseVec<InternalExon> ieplus[3], ieminus[3];
    ParseVec<LastExon> leplus, leminus[3];
    ParseVec<SingleExon> seplus, seminus;
};

class ExonData
{
    friend list<Gene> Parse::GetGenes() const;
public:
    ExonData(int stt, int stp, int tp) : start(stt), stop(stp), type(tp) {} 
    int Start() const { return start; }
    int Stop() const { return stop; }
    int Type() const { return type; }
    const set<int>& ChainID() const { return chain_id; }
    const set<int>& ProtID() const { return prot_id; }
    const TFrameShifts& ExonFrameShifts() const { return fshifts; }
    bool Identical(const ExonData& ed) const { return (ed.start == start && ed.stop == stop); }
    bool operator<(const ExonData& ed) const { return (stop < ed.start); }
    enum {Cds, Utr};
private:
    int start, stop, type;
    set<int> chain_id, prot_id;
    TFrameShifts fshifts;
};

class Gene : public vector<ExonData>
{
    friend list<Gene> Parse::GetGenes() const;
public:
    Gene(int s, bool l = true, bool r = true, int csf = 0) : 
        strand(s), leftend(l), rightend(r), cds_shift(csf) {}
    int Strand() const { return strand; }
    int CDS_Shift() const { return cds_shift; }
    const IVec& CDS() const { return cds; }
    bool LeftComplete() const { return leftend; }
    bool RightComplete() const { return rightend; }
    bool Complete() const { return (leftend && rightend); }
private:
    int strand, cds_shift;
    bool leftend, rightend;
    IVec cds;
};

void LogicalCheck(const HMM_State& st, const SeqScores& ss);


END_NCBI_SCOPE

#include "models.hpp"
#include "states.hpp"

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/11/06 15:02:21  ucko
 * Use iostream interface from ncbistre.hpp for GCC 2.95 compatibility.
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
#endif  // __GENE_FINDER__HPP
