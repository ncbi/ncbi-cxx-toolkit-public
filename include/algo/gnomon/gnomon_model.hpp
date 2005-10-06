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
        CFrameShiftInfo(TSignedSeqPos l = 0, int len = 0, bool is_i = true, const string& d_v = kEmptyStr) : m_loc(l), m_len(len), 
                  m_is_insert(is_i), m_delet_value(d_v) { if(!is_i && d_v == kEmptyStr) m_delet_value = string(len,'N'); }
        TSignedSeqPos Loc() const { return m_loc; }
        int Len() const { return m_len; }
        bool IsInsertion() const { return m_is_insert; }
        bool IsDeletion() const { return !m_is_insert; }
        const string& DeletedValue() const { return m_delet_value; }
        bool operator<(const CFrameShiftInfo& fsi) const { return ((m_loc == fsi.m_loc && IsDeletion() != fsi.IsDeletion()) ? IsDeletion() : m_loc < fsi.m_loc); } // if location is same deletion first
        bool operator==(const CFrameShiftInfo& fsi) const 
        { return (m_loc == fsi.m_loc) && (m_len == fsi.m_len) && (m_is_insert == fsi.m_is_insert) && (m_delet_value == fsi.m_delet_value); }
        bool operator!=(const CFrameShiftInfo& fsi) const
        {
            return !(*this == fsi);
        }
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
          TSignedSeqRange& Limits()       { return m_limits; }
    TSignedSeqPos GetFrom() const { return m_limits.GetFrom(); }
    TSignedSeqPos GetTo() const { return m_limits.GetTo(); }
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
        eOK,
        eSkipped,
        eAcceptedChain,
        eRejectedChain
    };

    string StatusString()
    {
        switch (m_status) {
        case eOK:            return "OK";
        case eSkipped:       return "eSkipped";
        case eAcceptedChain: return "AcceptedChain";
        case eRejectedChain: return "RejectedChain";
        default:             return "unknown";
        }
    }

    CAlignVec(EStrand s = ePlus, int i = 0, int t = eEST, TSignedSeqRange cdl = TSignedSeqRange::GetEmpty()) :
        m_cds_limits(cdl),
        m_type(t), m_strand(s), m_id(i), m_status(eOK),
        m_score(BadScore()), m_open_cds(false), m_pstop(false) {}


        void Insert(const CAlignExon& p);
        TSignedSeqRange Limits() const { return m_limits; }
        void RecalculateLimits() { m_limits = empty()?TSignedSeqRange::GetEmpty():TSignedSeqRange(front().GetFrom(),back().GetTo()); }
        TSignedSeqRange CdsLimits() const { return m_cds_limits; }   // notincluding start/stop (shows frame)
        void SetCdsLimits(TSignedSeqRange p) { m_cds_limits = p; }
        TSignedSeqRange RealCdsLimits() const;     // including start/stop
        TSignedSeqRange MaxCdsLimits() const { return m_max_cds_limits; }   // longest cds including start/stop (doesn't show frame)
        void SetMaxCdsLimits(TSignedSeqRange p) { m_max_cds_limits = p; }
        bool Intersect(const CAlignVec& a) const 
        {
            return Limits().IntersectingWith(a.Limits()); 
        }
        void SetStrand(EStrand s) { m_strand = s; }
        EStrand Strand() const { return m_strand; }
        void SetType(int t) { m_type = t; }
        int Type() const { return m_type; }
        void SetID(int i) { m_id = i; }
        int ID() const { return m_id; }
        void SetName(const string& name) { m_name = name; }
        const string& Name() const { return m_name; }
        void ChangeStatus(EStatus status) { m_status = status; }
        EStatus Status() const { return m_status; }
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
        bool CompleteCds() const   // start, stop and upstream 5' stop present 
        {
            if(m_max_cds_limits.Empty() || !Continious()) return false;
            else if(Strand() == ePlus && m_max_cds_limits.GetFrom() > Limits().GetFrom() && m_cds_limits.GetTo() < Limits().GetTo()-2) return true;
            else if(Strand() == eMinus && m_cds_limits.GetFrom() > Limits().GetFrom()+2 && m_max_cds_limits.GetTo() < Limits().GetTo()) return true;
            else return false;  
        }
        bool OpenCds() const { return m_open_cds; }  // "optimal" CDS is not internal
        void SetOpenCds(bool op) { m_open_cds = op; }
        bool PStop() const { return m_pstop; }  // has premature stop(s)
        void SetPStop(bool ps) { m_pstop = ps; }

        int FShiftedLen(TSignedSeqPos a, TSignedSeqPos b) const;

        void GetScore(const CGnomonEngine& engine, bool uselims = false);
    template <class Vec>
        void GetSequence(const Vec& seq, Vec& mrna, TIVec* mrnamap = 0, bool cdsonly = false) const;                     
        
        // Below comparisons ignore CDS completely, first 3 assume that alignments are the same strand
        
        int isCompatible(const CAlignVec& a) const;  // returns 0 for notcompatible or (number of common splices)+1
        bool SubAlign(const CAlignVec& a) const { return (Include(a.Limits(),Limits()) && isCompatible(a) > 0); }
        int MutualExtension(const CAlignVec& a) const;  // returns 0 for notcompatible or (number of introns) + 1
        
        bool IdenticalAlign(const CAlignVec& a) const { return (Strand() == a.Strand() && *this == a); }
        bool Similar(const CAlignVec& a, int tolerance) const;
        
        TFrameShifts& FrameShifts() { return m_fshifts; }
        const TFrameShifts& FrameShifts() const { return m_fshifts; }

protected:
    TSignedSeqRange m_cds_limits, m_max_cds_limits;

private:
    int m_type;
    EStrand m_strand;
    TSignedSeqRange m_limits;
    int m_id;
    string m_name;
    EStatus m_status;
    double m_score;
    bool m_open_cds, m_pstop;
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
        void Insert(const CAlignVec& a);
        void Insert(const CCluster& c);
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
        void InsertCluster(CCluster c);
        void Init();
};

class CGene;

class NCBI_XALGOGNOMON_EXPORT CExonData
{
    //    friend list<CGene> CParse::GetGenes() const;
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
    //    friend list<CGene> CParse::GetGenes() const;
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/10/06 15:49:21  chetvern
 * added precomputed limits to CAlignVec
 *
 * Revision 1.5  2005/10/06 14:36:03  souvorov
 * Editorial corrections
 *
 * Revision 1.4  2005/09/30 18:57:53  chetvern
 * added m_status and m_name to CAlignVec
 * removed m_contig from CClusterSet
 * moved frameshifts from CExonData to CGene
 *
 * Revision 1.3  2005/09/16 20:22:28  ucko
 * Whoops, MIPSpro also needs operator!= for CFrameShiftInfo.
 *
 * Revision 1.2  2005/09/16 18:02:31  ucko
 * Formal portability fixes:
 * - Replace kBadScore with an inline BadScore function that always
 *   returns the same value to avoid lossage in optimized WorkShop builds.
 * - Use <corelib/ncbi_limits.hpp> rather than <limits> for GCC 2.95.
 * - Supply CAlignExon::operator!= to satisfy SGI's MIPSpro compiler.
 * Don't bother explicitly including STL headers already pulled in by
 * ncbistd.hpp.
 *
 * Revision 1.1  2005/09/15 21:16:01  chetvern
 * redesigned API
 *
 * Revision 1.6  2005/06/03 18:17:03  souvorov
 * Change to indels mapping
 *
 * Revision 1.5  2005/04/13 19:02:37  souvorov
 * Score output for exons
 *
 * Revision 1.4  2005/04/07 15:25:30  souvorov
 * Output for supported length
 *
 * Revision 1.3  2005/03/23 20:50:02  souvorov
 * Mulpiprot penalty introduction
 *
 * Revision 1.2  2005/03/21 16:27:39  souvorov
 * Added check for holes in alignments
 *
 * Revision 1.1  2005/02/18 15:18:31  souvorov
 * First commit
 *
 * Revision 1.3  2004/07/28 12:33:18  dicuccio
 * Sync with Sasha's working tree
 *
 * Revision 1.2  2003/11/06 15:02:21  ucko
 * Use iostream interface from ncbistre.hpp for GCC 2.95 compatibility.
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // ALGO_GNOMON___GNOMON_MODEL__HPP
