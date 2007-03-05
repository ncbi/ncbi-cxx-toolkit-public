#ifndef ALGO_GNOMON___GNOMON_MODEL__HPP
#define ALGO_GNOMON___GNOMON_MODEL__HPP

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

#include <set>
#include <algorithm>
#include <math.h>

#include <objmgr/seq_vector_ci.hpp> // CSeqVectorTypes::TResidue
#include <util/range.hpp>           // TSignedSeqRange

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

USING_SCOPE(objects);

class CGnomonEngine;

// Making this a constant declaration (kBadScore) would be preferable,
// but backfires on WorkShop, where it is implicitly static and hence
// unavailable for use in inline functions.
inline
double BadScore() { return -numeric_limits<double>::max(); }

enum EStrand { ePlus, eMinus };

typedef CSeqVectorTypes::TResidue TResidue; // unsigned char

class NCBI_XALGOGNOMON_EXPORT CResidueVec  : public vector<TResidue> {};

typedef vector<int> TIVec;


inline bool Precede(TSignedSeqRange l, TSignedSeqRange r) { return l.GetTo() < r.GetFrom(); }
struct Precedence : public binary_function<TSignedSeqRange, TSignedSeqRange, bool>
{
    bool operator()(const TSignedSeqRange& __x, const TSignedSeqRange& __y) const
    { return Precede( __x, __y ); }
};

inline bool Include(TSignedSeqRange big, TSignedSeqRange small) { return (big.GetFrom()<=small.GetFrom() && small.GetTo()<=big.GetTo()); }
inline bool Include(TSignedSeqRange r, TSignedSeqPos p) { return (r.GetFrom()<=p && p<=r.GetTo()); }

class NCBI_XALGOGNOMON_EXPORT CFrameShiftInfo
{
public:
    CFrameShiftInfo(TSignedSeqPos l = 0, int len = 0, bool is_i = true, const string& d_v = kEmptyStr) :
        m_loc(l), m_len(len), m_is_insert(is_i), m_delet_value(d_v)
    { if(!is_i && d_v == kEmptyStr) m_delet_value = string(len,'N'); }
    TSignedSeqPos Loc() const { return m_loc; }
    int Len() const { return m_len; }
    bool IsInsertion() const { return m_is_insert; }
    bool IsDeletion() const { return !m_is_insert; }
    const string& DeletedValue() const { return m_delet_value; }
    bool operator<(const CFrameShiftInfo& fsi) const
    { return ((m_loc == fsi.m_loc && IsDeletion() != fsi.IsDeletion()) ? IsDeletion() : m_loc < fsi.m_loc); } // if location is same deletion first
    bool operator==(const CFrameShiftInfo& fsi) const 
    { return (m_loc == fsi.m_loc) && (m_len == fsi.m_len) && (m_is_insert == fsi.m_is_insert) && (m_delet_value == fsi.m_delet_value); }
    bool operator!=(const CFrameShiftInfo& fsi) const
    { return !(*this == fsi); }
private:
    TSignedSeqPos m_loc;  // left location for insertion, deletion is before m_loc
                          // insertion - when there are extra bases in the genome
    int m_len;
    bool m_is_insert;
    string m_delet_value;
};

typedef vector<CFrameShiftInfo> TFrameShifts;

class NCBI_XALGOGNOMON_EXPORT CAlignExon {
public:
    CAlignExon(TSignedSeqPos f = 0, TSignedSeqPos s = 0, bool fs = false, bool ss = false) : m_fsplice(fs), m_ssplice(ss), m_limits(f,s) {};
    CAlignExon(const TSignedSeqRange& p, bool fs = false, bool ss = false) : m_fsplice(fs), m_ssplice(ss), m_limits(p) {};

    bool operator==(const CAlignExon& p) const 
    { 
        return (m_limits==p.m_limits && m_fsplice == p.m_fsplice && m_ssplice == p.m_ssplice); 
    }
    bool operator!=(const CAlignExon& p) const
    {
        return !(*this == p);
    }
    bool operator<(const CAlignExon& p) const { return Precede(m_limits,p.m_limits); }
    
    const TSignedSeqRange& Limits() const { return m_limits; }
    TSignedSeqPos GetFrom() const { return m_limits.GetFrom(); }
    TSignedSeqPos GetTo() const { return m_limits.GetTo(); }
    void Extend(const CAlignExon& e);
    void AddFrom(int d) { m_limits.SetFrom( m_limits.GetFrom() +d ); }
    void AddTo(int d) { m_limits.SetTo( m_limits.GetTo() +d ); }

    bool m_fsplice, m_ssplice;

private:
    TSignedSeqRange m_limits;
};

class NCBI_XALGOGNOMON_EXPORT CAlignVec : public vector<CAlignExon>
{
public:
    typedef vector<CAlignExon>::iterator TIt;
    typedef vector<CAlignExon>::const_iterator TConstIt;
    
    enum {eWall, eNested, eEST, emRNA, eProt};

    enum EStatus {
        eOk = 0,
        eOpen = 1,
        ePStop = 2,
        eSkipped = 4,
        eLeftTrimmed = 8,
        eRightTrimmed = 16,
        eRejectedChain = 32,
        eConfirmedStart = 64
    };

    CAlignVec(EStrand s = ePlus, int i = 0, int t = eEST, TSignedSeqRange cdl = TSignedSeqRange::GetEmpty()) :
        m_cds_limits(cdl), m_type(t), m_strand(s), m_geneid(0), m_id(i), m_status(eOk), m_score(BadScore()) {}


    void Insert(const CAlignExon& p);
    TSignedSeqRange Limits() const { return m_limits; }
    void RecalculateLimits()
    {
        if (empty()) {
            m_limits = TSignedSeqRange::GetEmpty(); 
        } else {
            m_limits = TSignedSeqRange(front().GetFrom(),back().GetTo());
        }
    }
    TSignedSeqRange CdsLimits() const { return m_cds_limits; }   // notincluding start/stop (shows frame)
    void SetCdsLimits(TSignedSeqRange p) { m_cds_limits = p; }
    TSignedSeqRange RealCdsLimits() const;     // including start/stop
    TSignedSeqRange MaxCdsLimits() const { return m_max_cds_limits; }   // longest cds including start/stop (doesn't show frame)
    void SetMaxCdsLimits(TSignedSeqRange p) { m_max_cds_limits = p; }
    void SetCdsInfo(const CAlignVec& a)
    {
        SetCdsLimits(a.CdsLimits()); SetMaxCdsLimits(a.MaxCdsLimits());
        SetScore(a.Score()); SetPStop(a.PStop()); SetOpenCds(OpenCds());
    }
    bool Intersect(const CAlignVec& a) const 
    {
        return Limits().IntersectingWith(a.Limits()); 
    }
    void SetStrand(EStrand s) { m_strand = s; }
    EStrand Strand() const { return m_strand; }
    void SetType(int t) { m_type = t; }
    int Type() const { return m_type; }
    int GeneID() const { return m_geneid; }
    void SetGeneID(int id) { m_geneid = id; }
    void SetID(int i) { m_id = i; }
    int ID() const { return m_id; }
    void SetComposite(int i) { m_composite = i; }
    int Composite() const { return m_composite; }
    void SetName(const string& name) { m_name = name; }
    const string& Name() const { return m_name; }
    unsigned int& Status() { return m_status; }
    const unsigned int& Status() const { return m_status; }
    bool operator<(const CAlignVec& a) const { return Precede(Limits(),a.Limits()); }
    void Init();
    void SetScore(double s) { m_score = s; }
    double Score() const { return m_score; }
    void Extend(const CAlignVec& a);
    int AlignLen() const ;
    int CdsLen() const ;              // with start/stop
    int MaxCdsLen() const ;
    bool Continious() const           //  no "holes" in alignment
    {
        for(unsigned int i = 1; i < size(); ++i)
            {
                if(!(*this)[i-1].m_ssplice || !(*this)[i].m_fsplice) return false;
            }
        return true;
    }
    bool FullFiveP() const // start present
    {
        return m_cds_limits.NotEmpty() && (Strand() == ePlus ? m_cds_limits.GetFrom() > Limits().GetFrom()+2 : m_cds_limits.GetTo() < Limits().GetTo()-2);
    }
    bool FullThreeP() const // stop present
    {
        return m_cds_limits.NotEmpty() && (Strand() == ePlus ? m_cds_limits.GetTo() < Limits().GetTo()-2 : m_cds_limits.GetFrom() > Limits().GetFrom()+2);
    }
    bool FullCds() const   // both start and stop present and no "holes" 
    { 
        return (m_cds_limits.NotEmpty() && m_cds_limits.GetFrom() > Limits().GetFrom()+2 && 
                m_cds_limits.GetTo() < Limits().GetTo()-2 && Continious()); 
    }
    bool CompleteCds() const   // start, stop and upstream 5' stop present or start confirmed by protein alignment 
    {
        if(!FullCds()) return false;
        else if(ConfirmedStart()) return true; 
        else if(Strand() == ePlus && m_max_cds_limits.GetFrom() > Limits().GetFrom()) return true;
        else if(Strand() == eMinus && m_max_cds_limits.GetTo() < Limits().GetTo()) return true;
        else return false;  
    }

    void ClearStatus() { m_status = 0; }

    bool OpenCds() const { return m_status & eOpen; }  // "optimal" CDS is not internal
    void SetOpenCds(bool op) { if(op) m_status |= eOpen; else m_status &= ~eOpen; }
    bool PStop() const { return (m_status & ePStop) != 0; }  // has premature stop(s)
    void SetPStop(bool ps) { if(ps) m_status |= ePStop; else m_status &= ~ePStop; }
    
    bool ConfirmedStart() const { return (m_status & eConfirmedStart) != 0; }  // start is confirmed by protein alignment
    void SetConfirmedStart(bool cs) { 
        if(cs) {
            m_status |= eConfirmedStart;
            SetOpenCds(false); 
        } else {
            m_status &= ~eConfirmedStart; 
        }
    }

    bool isNMD() const {
        if(CdsLimits().Empty() || size() <= 1) return false;
        if(Strand() == ePlus) {
            return RealCdsLimits().GetTo() < back().GetFrom();
        } else {
            return RealCdsLimits().GetFrom() > front().GetTo();
        }
    }
    
    int FShiftedLen(TSignedSeqPos a, TSignedSeqPos b) const;
    
    template <class Vec>
    void GetSequence(const Vec& seq, Vec& mrna, TIVec* mrnamap = 0, bool cdsonly = false) const;                     
    
    // Below comparisons ignore CDS completely, first 3 assume that alignments are the same strand
    
    int isCompatible(const CAlignVec& a) const;  // returns 0 for notcompatible or (number of common splices)+1
    bool SubAlign(const CAlignVec& a) const { return (Include(a.Limits(),Limits()) && isCompatible(a) > 0); }
    int MutualExtension(const CAlignVec& a) const;  // returns 0 for notcompatible or (number of introns) + 1
    
    bool IdenticalAlign(const CAlignVec& a) const
    { return Strand()==a.Strand() && Limits()==a.Limits() && Type()==a.Type() &&
            static_cast<const vector<CAlignExon>& >(*this) == a && FrameShifts()==a.FrameShifts(); }
    bool operator==(const CAlignVec& a) const
    {
        return IdenticalAlign(a) && ID()==a.ID() && Name()==a.Name();
    }
    bool Similar(const CAlignVec& a, int tolerance) const;
    
    TFrameShifts& FrameShifts() { return m_fshifts; }
    const TFrameShifts& FrameShifts() const { return m_fshifts; }

protected:
    TSignedSeqRange m_cds_limits, m_max_cds_limits;

private:
    int m_type;
    EStrand m_strand;
    TSignedSeqRange m_limits;
    int m_geneid;
    int m_id;
    string m_name;
    unsigned int m_status;
    int m_composite;
    double m_score;
    TFrameShifts m_fshifts;
};

typedef list<CAlignVec> TAlignList;
typedef TAlignList::iterator TAlignListIt;     
typedef TAlignList::const_iterator TAlignListConstIt;     

NCBI_XALGOGNOMON_EXPORT CNcbiIstream& operator>>(CNcbiIstream& s, CAlignVec& a);
NCBI_XALGOGNOMON_EXPORT CNcbiOstream& operator<<(CNcbiOstream& s, const CAlignVec& a);


class NCBI_XALGOGNOMON_EXPORT CCluster : public list<CAlignVec>
{
    public:
        typedef list<CAlignVec>::iterator TIt;
        typedef list<CAlignVec>::const_iterator TConstIt;
        CCluster(int f = numeric_limits<int>::max(), int s = 0, int t = CAlignVec::eWall) : m_limits(f,s), m_type(t) {}
        CCluster(TSignedSeqRange limits) : m_limits(limits), m_type(CAlignVec::eWall) {}
        void Insert(const CAlignVec& a);
        void Insert(const CCluster& c);
        void Splice(CCluster& c); // elements removed from c and inserted into *this
        TSignedSeqRange Limits() const { return m_limits; }
        int Type() const { return m_type; }
        bool operator<(const CCluster& c) const { return Precede(m_limits, c.m_limits); }
        void Init(TSignedSeqPos first, TSignedSeqPos second, int t);

    private:
        TSignedSeqRange m_limits;
        int m_type;
};

NCBI_XALGOGNOMON_EXPORT CNcbiIstream& operator>>(CNcbiIstream& s, CCluster& c);
NCBI_XALGOGNOMON_EXPORT CNcbiOstream& operator<<(CNcbiOstream& s, const CCluster& c);


class NCBI_XALGOGNOMON_EXPORT CClusterSet : public set<CCluster>
{
public:
    typedef set<CCluster>::iterator TIt;
    typedef set<CCluster>::const_iterator TConstIt;
    
    CClusterSet() {}
    void InsertAlignment(const CAlignVec& a);
};

class CGene;

class NCBI_XALGOGNOMON_EXPORT CExonData
{
public:
    CExonData(TSignedSeqPos stt = 0, TSignedSeqPos stp = 0, int lf = 0, int rf = 0, string tp = kEmptyStr, int sl = 0, double sc = BadScore()) :
	m_limits(stt,stp), 
	m_lframe(lf), m_rframe(rf), m_supported_len(sl), m_type(tp), m_score(sc) {} 

    const TSignedSeqRange& Limits() const { return m_limits; }
    TSignedSeqPos GetFrom() const { return m_limits.GetFrom(); }
    void SetFrom(TSignedSeqPos start) { m_limits.SetFrom( start ); }
    TSignedSeqPos GetTo() const { return m_limits.GetTo(); }
    double Score() const { return m_score; }
    void SetScore(double score) { m_score = score; }
    string Type() const { return m_type; }
    void SetType( const string& type) { m_type= type; }
    int lFrame() const { return m_lframe; }
    void SetlFrame(int lframe) {m_lframe = lframe; }
    int rFrame() const { return m_rframe; }
    int SupportedLen() const { return m_supported_len; }
    void AddSupportedLen(int len) { m_supported_len += len; }
    set<int>& ChainID() { return m_chain_id; }
    const set<int>& ChainID() const { return m_chain_id; }
    bool Identical(const CExonData& ed) const { return (ed.m_limits == m_limits); }
    bool operator<(const CExonData& ed) const { return Precede(m_limits, ed.m_limits); }
private:
    TSignedSeqRange m_limits;
    int m_lframe, m_rframe;
    int m_supported_len;
    string m_type;
    set<int> m_chain_id;
    double m_score;
};

class NCBI_XALGOGNOMON_EXPORT CGene : public vector<CExonData>
{
public:
    CGene(EStrand s, bool l = true, bool r = true, int csf = 0, string cntg = "") : 
	m_strand(s), m_cds_shift(csf), m_leftend(l), m_rightend(r), m_contig(cntg)
    {}

    EStrand Strand() const { return m_strand; }
    int CDS_Shift() const { return m_cds_shift; }   // first complete codon for partial CDS
          CResidueVec& CDS()       { return m_cds; }
    const CResidueVec& CDS() const { return m_cds; }
    bool LeftComplete() const { return m_leftend; }
    void SetLeftComplete(bool f) { m_leftend = f; }
    bool RightComplete() const { return m_rightend; }
    bool Complete() const { return (m_leftend && m_rightend); }

    void SetContigName(const string& contig) { m_contig = contig; }
    void SetCdsShift(int s) { m_cds_shift = s; }

    void Print(int gnum, int mnum, CNcbiOstream& to = cout, CNcbiOstream& toprot = cout) const;

          TFrameShifts& FrameShifts()       { return m_fshifts; }
    const TFrameShifts& FrameShifts() const { return m_fshifts; }
private:
    EStrand m_strand;
    int m_cds_shift;
    bool m_leftend, m_rightend;
    CResidueVec m_cds;
    string m_contig;
    TFrameShifts m_fshifts;
};


END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___GNOMON_MODEL__HPP
