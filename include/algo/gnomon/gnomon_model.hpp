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

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>
#include "set.hpp"

#include <set>
#include <vector>
#include <algorithm>
#include <math.h>

#include <objmgr/seq_vector_ci.hpp> // CSeqVectorTypes::TResidue
#include <util/range.hpp>           // TSignedSeqRange

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
class CSeq_align;
class CSeq_id;
END_SCOPE(objects)
USING_SCOPE(objects);
BEGIN_SCOPE(gnomon)

class CGnomonEngine;

// Making this a constant declaration (kBadScore) would be preferable,
// but backfires on WorkShop, where it is implicitly static and hence
// unavailable for use in inline functions.
inline
double BadScore() { return -numeric_limits<double>::max(); }

enum EStrand { ePlus, eMinus};

typedef objects::CSeqVectorTypes::TResidue TResidue; // unsigned char

typedef vector<TResidue> CResidueVec;

typedef vector<int> TIVec;

inline bool Precede(TSignedSeqRange l, TSignedSeqRange r) { return l.GetTo() < r.GetFrom(); }
inline bool Include(TSignedSeqRange big, TSignedSeqRange small) { return (big.GetFrom()<=small.GetFrom() && small.GetTo()<=big.GetTo()); }
inline bool Include(TSignedSeqRange r, TSignedSeqPos p) { return (r.GetFrom()<=p && p<=r.GetTo()); }
inline bool Enclosed(TSignedSeqRange big, TSignedSeqRange small) { return (big != small && Include(big, small)); }

class CSupportInfo
{
public:
    CSupportInfo(Int8 model_id, bool core=false);


    Int8 GetId() const;
    void SetCore(bool core);
    bool IsCore() const;
    bool operator==(const CSupportInfo& s) const;
    bool operator<(const CSupportInfo& s) const;

private:
    Int8 m_id;
    bool m_core_align;
}; 

typedef CVectorSet<CSupportInfo> CSupportInfoSet;


class NCBI_XALGOGNOMON_EXPORT CInDelInfo
{
public:
    CInDelInfo(TSignedSeqPos l = 0, int len = 0, bool is_i = true, const string& d_v = kEmptyStr) : m_loc(l), m_len(len), m_is_insert(is_i) {}
    TSignedSeqPos Loc() const { return m_loc; }
    int Len() const { return m_len; }
    bool IsInsertion() const { return m_is_insert; }
    bool IsDeletion() const { return !m_is_insert; }
    bool IntersectingWith(TSignedSeqPos a, TSignedSeqPos b) const    // insertion at least partially inside, deletion inside or flanking
    {
        return (IsDeletion() && Loc() >= a && Loc() <= b+1) ||
            (IsInsertion() && Loc() <= b && a <= Loc()+Len()-1);
    }
    bool operator<(const CInDelInfo& fsi) const
    { return ((m_loc == fsi.m_loc && IsDeletion() != fsi.IsDeletion()) ? IsDeletion() : m_loc < fsi.m_loc); } // if location is same deletion first
    bool operator==(const CInDelInfo& fsi) const 
    { return (m_loc == fsi.m_loc) && (m_len == fsi.m_len) && (m_is_insert == fsi.m_is_insert); }
    bool operator!=(const CInDelInfo& fsi) const
    { return !(*this == fsi); }

private:
    TSignedSeqPos m_loc;  // left location for insertion, deletion is before m_loc
                          // insertion - when there are extra bases in the genome
    int m_len;
    bool m_is_insert;
};

typedef vector<CInDelInfo> TInDels;

template <class Res>
bool IsStartCodon(const Res * seq, int strand = ePlus);   // seq points to the first base in biological order
template <class Res>
bool IsStopCodon(const Res * seq, int strand = ePlus);   // seq points to the first base in biological order


class CRangeMapper {
public:
    virtual ~CRangeMapper() {}
    virtual TSignedSeqRange operator()(TSignedSeqRange r, bool withextras = true) const = 0;
};

class NCBI_XALGOGNOMON_EXPORT CModelExon {
public:
    CModelExon(TSignedSeqPos f = 0, TSignedSeqPos s = 0, bool fs = false, bool ss = false) : m_fsplice(fs), m_ssplice(ss), m_range(f,s) {};

    bool operator==(const CModelExon& p) const 
    { 
        return (m_range==p.m_range && m_fsplice == p.m_fsplice && m_ssplice == p.m_ssplice); 
    }
    bool operator!=(const CModelExon& p) const
    {
        return !(*this == p);
    }
    bool operator<(const CModelExon& p) const { return Precede(Limits(),p.Limits()); }
    
    operator TSignedSeqRange() const { return m_range; }
    const TSignedSeqRange& Limits() const { return m_range; }
          TSignedSeqRange& Limits()       { return m_range; }
    TSignedSeqPos GetFrom() const { return m_range.GetFrom(); }
    TSignedSeqPos GetTo() const { return m_range.GetTo(); }
    void Extend(const CModelExon& e);
    void AddFrom(int d) { m_range.SetFrom( m_range.GetFrom() +d ); }
    void AddTo(int d) { m_range.SetTo( m_range.GetTo() +d ); }

    bool m_fsplice, m_ssplice;

    void Remap(const CRangeMapper& mapper) { m_range = mapper(m_range); }
private:
    TSignedSeqRange m_range;
};

class CCDSInfo {
public:
    CCDSInfo(): m_confirmed_start(false),m_confirmed_stop(false),m_open(false),
                m_score(BadScore()) {}

    bool operator== (const CCDSInfo& another) const;

    TSignedSeqRange ReadingFrame() const { return m_reading_frame; }
    TSignedSeqRange ProtReadingFrame() const { return m_reading_frame_from_proteins; }
    TSignedSeqRange Cds() const { return Start()+ReadingFrame()+Stop(); }
    TSignedSeqRange MaxCdsLimits() const { return m_max_cds_limits; }

    TSignedSeqRange Start() const {return m_start; }
    TSignedSeqRange Stop() const {return m_stop; }
    bool HasStart() const { return Start().NotEmpty(); }
    bool HasStop () const { return Stop().NotEmpty();  }
    bool ConfirmedStart() const { return m_confirmed_start; }  // start is confirmed by protein alignment
    bool ConfirmedStop() const { return m_confirmed_stop; }  // stop is confirmed by protein alignment
    typedef vector<TSignedSeqRange> TPStops;
    const TPStops& PStops() const { return m_p_stops; }
    bool PStop() const { return !m_p_stops.empty(); }  // has premature stop(s)
    
    bool OpenCds() const { return m_open; }  // "optimal" CDS is not internal
    double Score() const { return m_score; }

    void SetReadingFrame(TSignedSeqRange r, bool protein = false);
    void SetStart(TSignedSeqRange r, bool confirmed = false);
    void SetStop(TSignedSeqRange r, bool confirmed = false);
    void Set5PrimeCdsLimit(TSignedSeqPos p);
    void Clear5PrimeCdsLimit();
    void AddPStop(TSignedSeqRange r);
    void SetScore(double score, bool open=false);

    void CombineWith(const CCDSInfo& another_cds_info);
    void Remap(const CRangeMapper& mapper);
    void Clip(TSignedSeqRange limits);
    void Clear();
    void ClearPStops() { m_p_stops.clear(); }

    int Strand() const; // -1 (minus), 0 (unknown), 1 (plus)

    bool Invariant() const
    {
        if (ReadingFrame().Empty()) {
            _ASSERT( !HasStop() && !HasStart() );
            _ASSERT( ProtReadingFrame().Empty() && MaxCdsLimits().Empty() );
            _ASSERT( !ConfirmedStart() );
            _ASSERT( !ConfirmedStop() );
            _ASSERT( !PStop() );
            _ASSERT( !OpenCds() );
            _ASSERT( Score()==BadScore() );
            return true;
        }

        _ASSERT( !Start().IntersectingWith(ReadingFrame()) );
        _ASSERT( !Stop().IntersectingWith(ReadingFrame()) );
        _ASSERT( ProtReadingFrame().Empty() || Include(ReadingFrame(), ProtReadingFrame()) );
        _ASSERT( Include( MaxCdsLimits(), Cds() ) );

        if (!HasStop() && !HasStart()) {
            _ASSERT( MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom() ||
                     MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo() );
        } else if (HasStart() && !HasStop()) {
            if (Precede(Start(), ReadingFrame())) {
                _ASSERT( MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo() );
            } else {
                _ASSERT( MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom() );
            }
        }
        else if (HasStart() && HasStop()) {
            _ASSERT( Include(Start()+Stop(),ReadingFrame()) );
        }
        if (HasStop()) {
            if (Precede(ReadingFrame(),Stop())) {
                _ASSERT( MaxCdsLimits().GetTo()==Stop().GetTo() );
            } else {
                _ASSERT( MaxCdsLimits().GetFrom()==Stop().GetFrom() );
            } 
        }

        if (ConfirmedStart()) {
            _ASSERT( HasStart() );
        }

        if (ConfirmedStop()) {
            _ASSERT( HasStop() );
        }

        ITERATE( vector<TSignedSeqRange>, s, PStops())
            _ASSERT( Include(ReadingFrame(), *s) );

        return true;
    }

private:
    TSignedSeqRange m_start, m_stop;
    TSignedSeqRange m_reading_frame;
    TSignedSeqRange m_reading_frame_from_proteins;

    TSignedSeqRange m_max_cds_limits;

    bool m_confirmed_start, m_confirmed_stop;
    TPStops m_p_stops;

    bool m_open;
    double m_score;
};

class CAlignMap;



class NCBI_XALGOGNOMON_EXPORT CGeneModel
{
public:
    enum EType {
        eWall = 1,
        eNested = 2,
        eSR = 4,
        eEST = 8,
        emRNA = 16,
        eProt = 32,
        eChain = 64,
        eGnomon = 128
    };
    static string TypeToString(int type);

    enum EStatus {
        eReversed = 2,
        eSkipped = 4,
        eLeftTrimmed = 8,
        eRightTrimmed = 16,
        eFullSupCDS = 32,
        ePseudo = 64,
        ePolyA = 128,
        eCap = 256,
        eBestPlacement = 512,
        eUnknownOrientation = 1024 
    };

    CGeneModel(EStrand s = ePlus, int id = 0, int type = 0) :
        m_type(type), m_id(id), m_status(0), m_ident(0), m_weight(1), m_expecting_hole(false), m_strand(s), m_geneid(0), m_rank_in_gene(0) {}
    virtual ~CGeneModel() {}

    void AddExon(TSignedSeqRange exon);
    void AddHole(); // between model and next exons

    typedef vector<CModelExon> TExons;
    const TExons& Exons() const { return m_exons; }

    void Remap(const CRangeMapper& mapper);
    enum EClipMode { eRemoveExons, eDontRemoveExons };
    void Clip(TSignedSeqRange limits, EClipMode mode, bool ensure_cds_invariant = true);
    void CutExons(TSignedSeqRange hole); // clip or remove exons, dangerous, should be completely in or outside the cds, should not cut an exon in two 
    void ExtendLeft(int amount);
    void ExtendRight(int amount);
    void Extend(const CGeneModel& a, bool ensure_cds_invariant = true);
    void TrimEdgesToFrameInOtherAlignGaps(const TExons& exons_with_gaps, bool ensure_cds_invariant = true);
    void RemoveShortHolesAndRescore(const CGnomonEngine& gnomon);   // removes holes shorter than min intron (may add frameshifts/stops)
   
    TSignedSeqRange Limits() const { return m_range; }
    int AlignLen() const ;
    void RecalculateLimits()
    {
        if (Exons().empty()) {
            m_range = TSignedSeqRange::GetEmpty(); 
        } else {
            m_range = TSignedSeqRange(Exons().front().GetFrom(),Exons().back().GetTo());
        }
    }

    // ReadingFrame doesn't include start/stop. It's always on codon boundaries
    TSignedSeqRange ReadingFrame() const { return m_cds_info.ReadingFrame(); }
    // CdsLimits include start/stop if any, goes to model limit if no start/stop
    TSignedSeqRange RealCdsLimits() const;
    int RealCdsLen() const ;              // %3!=0 is possible 
    // MaxCdsLimits - longest cds. include start/stop if any,
    // goes to 5' limit if no upstream stop, goes to 3' limit if no stop
    TSignedSeqRange MaxCdsLimits() const;

    const CCDSInfo& GetCdsInfo() const { return m_cds_info; }
    void SetCdsInfo(const CCDSInfo& cds_info);
    void SetCdsInfo(const CGeneModel& a);
    void CombineCdsInfo(const CGeneModel& a, bool ensure_cds_invariant = true);
    void CombineCdsInfo(const CCDSInfo& cds_info, bool ensure_cds_invariant = true);

    bool IntersectingWith(const CGeneModel& a) const 
    {
        return Limits().IntersectingWith(a.Limits()); 
    }

    double Ident() const { return m_ident; }
    void SetIdent(double i) { m_ident = i; }

    double Weight() const { return m_weight; }
    void SetWeight(double w) { m_weight = w; }

    void SetStrand(EStrand s) { m_strand = s; }
    EStrand Strand() const { return m_strand; }
    void SetType(int t) { m_type = t; }
    int Type() const { return m_type; }
    int GeneID() const { return m_geneid; }
    void SetGeneID(int id) { m_geneid = id; }
    int RankInGene() const { return m_rank_in_gene; }
    void SetRankInGene(int rank) { m_rank_in_gene = rank; }
    Int8 ID() const { return m_id; }
    void SetID(Int8 id) { m_id = id; }
    const CSupportInfoSet& Support() const { return m_support; }
    bool AddSupport(const CSupportInfo& support) { return m_support.insert(support); }
    void ReplaceSupport(const CSupportInfoSet& support_set) {  m_support = support_set; }
    const string& ProteinHit() const { return  m_protein_hit; }
          string& ProteinHit()       { return  m_protein_hit; }

    unsigned int& Status() { return m_status; }
    const unsigned int& Status() const { return m_status; }
    void ClearStatus() { m_status = 0; }

    const string& GetComment() const { return m_comment; }
    void SetComment(const string& comment) { m_comment = comment; }
    void AddComment(const string& comment) { m_comment += " " + comment; }

    bool operator<(const CGeneModel& a) const { return Precede(Limits(),a.Limits()); }

    double Score() const { return m_cds_info.Score(); }

    bool Continuous() const           //  no "holes" in alignment
    {
        for(unsigned int i = 1; i < Exons().size(); ++i)
            if (!Exons()[i-1].m_ssplice || !Exons()[i].m_fsplice)
                return false;
        return true;
    }
    bool HasStart() const { return m_cds_info.HasStart(); }
    bool HasStop () const { return m_cds_info.HasStop ();  }
    bool LeftComplete()  const { return Strand() == ePlus ? HasStart() : HasStop();  }
    bool RightComplete() const { return Strand() == ePlus ? HasStop()  : HasStart(); }
    bool FullCds() const { return HasStart() && HasStop() && Continuous(); }
    bool CompleteCds() const { return FullCds() && (!Open5primeEnd() || ConfirmedStart()); }

    bool GoodEnoughToBeAnnotation() const
    {
        _ASSERT( !(OpenCds()&&ConfirmedStart()) );
        return (ReadingFrame().Empty() || (!OpenCds() && FullCds()));
    }

    bool Open5primeEnd() const
    {
        return (Strand() == ePlus ? OpenLeftEnd() : OpenRightEnd());
    }
    bool OpenLeftEnd() const
    {
        return ReadingFrame().NotEmpty() && GetCdsInfo().MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom();
    }
    bool OpenRightEnd() const
    {
        return ReadingFrame().NotEmpty() && GetCdsInfo().MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo();
    }

    bool OpenCds() const { return m_cds_info.OpenCds(); }  // "optimal" CDS is not internal
    bool PStop() const { return m_cds_info.PStop(); }  // has premature stop(s)
    
    bool ConfirmedStart() const { return m_cds_info.ConfirmedStart(); }  // start is confirmed by protein alignment
    bool ConfirmedStop() const { return m_cds_info.ConfirmedStop(); }  // stop is confirmed by protein alignment

    bool isNMD(int limit = 50) const
    {
        if (ReadingFrame().Empty() || Exons().size() <= 1)
            return false;
        if (Strand() == ePlus) {
            return RealCdsLimits().GetTo() < Exons().back().GetFrom() && FShiftedLen(RealCdsLimits().GetTo(),Exons()[Exons().size()-2].GetTo()) > limit;
        } else {
            return RealCdsLimits().GetFrom() > Exons().front().GetTo() && FShiftedLen(Exons()[1].GetFrom(),RealCdsLimits().GetFrom()) > limit ;
        }
    }

    TInDels& FrameShifts() { return m_fshifts; }
    const TInDels& FrameShifts() const { return m_fshifts; }
    TInDels FrameShifts(TSignedSeqPos a, TSignedSeqPos b) const;
    void RemoveExtraFShifts();

    int FShiftedLen(TSignedSeqRange ab, bool withextras = true) const;    // won't work if a/b is insertion
    int FShiftedLen(TSignedSeqPos a, TSignedSeqPos b, bool withextras = true) const  { return FShiftedLen(TSignedSeqRange(a,b),withextras); }

    // move along mrna skipping introns
    TSignedSeqPos FShiftedMove(TSignedSeqPos pos, int len) const;  // may retun <0 if hits a deletion at the end of move

    virtual CAlignMap GetAlignMap() const;

    string GetProtein (const CResidueVec& contig_sequence) const;
    
    // Below comparisons ignore CDS completely, first 3 assume that alignments are the same strand
    
    int isCompatible(const CGeneModel& a) const;  // returns 0 for notcompatible or (number of common splices)+1
    bool IsSubAlignOf(const CGeneModel& a) const { return Include(a.Limits(),Limits()) && isCompatible(a); }
    int MutualExtension(const CGeneModel& a) const;  // returns 0 for notcompatible or (number of introns) + 1
    
    bool IdenticalAlign(const CGeneModel& a) const
    { return Strand()==a.Strand() && Limits()==a.Limits() && Exons() == a.Exons() && FrameShifts()==a.FrameShifts() && 
            GetCdsInfo().PStops() == a.GetCdsInfo().PStops() && Type() == a.Type(); }
    bool operator==(const CGeneModel& a) const
    {
        return IdenticalAlign(a) && Type()==a.Type() && m_id==a.m_id && m_support==a.m_support;
    }

    const list< CRef<CSeq_id> >& TrustedmRNA() const { return m_trusted_mrna; }
    void InsertTrustedmRNA(CRef<CSeq_id> g) { m_trusted_mrna.push_back(g); };
    void ClearTrustedmRNA() { m_trusted_mrna.clear(); };
   
    const list< CRef<CSeq_id> >& TrustedProt() const { return m_trusted_prot; }
    void InsertTrustedProt(CRef<CSeq_id> g) { m_trusted_prot.push_back(g); };
    void ClearTrustedProt() { m_trusted_prot.clear(); };

    const vector<CCDSInfo>* GetEdgeReadingFrames() const { return &m_edge_reading_frames; }
    vector<CCDSInfo>* SetEdgeReadingFrames() { return &m_edge_reading_frames; }


#ifdef _DEBUG
    int oid;
#endif

private:
    int m_type;
    Int8 m_id;
    unsigned int m_status;

    double m_ident;
    double m_weight;

    TExons m_exons;
    TExons& MyExons() { return m_exons; }
    bool m_expecting_hole;

    TSignedSeqRange m_range;
    EStrand m_strand;
    TInDels m_fshifts;

    CCDSInfo m_cds_info;
    bool CdsInvariant(bool check_start_stop = true) const;

    int m_geneid;
    int m_rank_in_gene;
    CSupportInfoSet m_support;
    string m_protein_hit;
    string m_comment;
    list< CRef<CSeq_id> > m_trusted_prot;
    list< CRef<CSeq_id> > m_trusted_mrna;


    vector<CCDSInfo> m_edge_reading_frames;
};


class CAlignMap {
public:
    enum EEdgeType { eBoundary, eSplice, eInDel };
    enum ERangeEnd{ eLeftEnd, eRightEnd, eSinglePoint };

    CAlignMap() {};
    CAlignMap(TSignedSeqPos orig_a, TSignedSeqPos orig_b) : m_orientation(ePlus) {
        m_orig_ranges.push_back(SMapRange(SMapRangeEdge(orig_a), SMapRangeEdge(orig_b)));
        m_edited_ranges = m_orig_ranges;
        m_target_len = FShiftedLen(orig_a, orig_b); 
    }
    CAlignMap(TSignedSeqPos orig_a, TSignedSeqPos orig_b, TInDels::const_iterator fsi_begin, const TInDels::const_iterator fsi_end) : m_orientation(ePlus) { 
        InsertIndelRangesForInterval(orig_a, orig_b, 0, fsi_begin, fsi_end, eBoundary, eBoundary); 
        m_target_len = FShiftedLen(orig_a, orig_b);
    }
    CAlignMap(const CGeneModel::TExons& exons, const vector<TSignedSeqRange>& transcript_exons, const TInDels& indels, EStrand orientation, int targetlen );     //orientation == strand if not Reversed
    CAlignMap(const CGeneModel::TExons& exons, const TInDels& frameshifts, EStrand strand, TSignedSeqRange lim = TSignedSeqRange::GetWhole(), int holelen = 0);
    TSignedSeqPos MapOrigToEdited(TSignedSeqPos orig_pos) const;
    TSignedSeqPos MapEditedToOrig(TSignedSeqPos edited_pos) const;
    TSignedSeqRange MapRangeOrigToEdited(TSignedSeqRange orig_range, ERangeEnd lend, ERangeEnd rend) const;
    TSignedSeqRange MapRangeOrigToEdited(TSignedSeqRange orig_range, bool withextras = true) const { return MapRangeOrigToEdited(orig_range, withextras?eLeftEnd:eSinglePoint, withextras?eRightEnd:eSinglePoint); }
    TSignedSeqRange MapRangeEditedToOrig(TSignedSeqRange edited_range, bool withextras = true) const;
    template <class Vec>
    void EditedSequence(const Vec& original_sequence, Vec& edited_sequence, bool includeholes = false) const;
    int FShiftedLen(TSignedSeqRange ab, ERangeEnd lend, ERangeEnd rend) const { return MapRangeOrigToEdited(ab, lend, rend).GetLength(); }
    int FShiftedLen(TSignedSeqRange ab, bool withextras = true) const { return MapRangeOrigToEdited(ab, withextras).GetLength(); }
    int FShiftedLen(TSignedSeqPos a, TSignedSeqPos b, bool withextras = true) const { return FShiftedLen(TSignedSeqRange(a,b), withextras); }
    //snap to codons works by analising transcript coordinates (MUST be a protein or reading frame cutout)
    TSignedSeqRange ShrinkToRealPoints(TSignedSeqRange orig_range, bool snap_to_codons = false) const;
    TSignedSeqPos FShiftedMove(TSignedSeqPos orig_pos, int len) const;
    TInDels GetInDels(bool fs_only) const;
    int TargetLen() const { return m_target_len; }

// private: // breaks SMapRange on WorkShop. :-/
    struct SMapRangeEdge {
        SMapRangeEdge(TSignedSeqPos p, TSignedSeqPos e = 0, bool d = false, EEdgeType t = eBoundary) : m_pos(p), m_extra(e), m_edge_type(t) {}
        bool operator<(const SMapRangeEdge& mre) const { return m_pos < mre.m_pos; }
        bool operator==(const SMapRangeEdge& mre) const { return m_pos == mre.m_pos; }

        TSignedSeqPos m_pos, m_extra;
        EEdgeType m_edge_type;
    };
    
    class SMapRange {
    public:
        SMapRange(SMapRangeEdge from, SMapRangeEdge to) : m_from(from), m_to(to) {}
        TSignedSeqPos GetFrom() const { return m_from.m_pos; }
        TSignedSeqPos GetTo() const { return m_to.m_pos; }
        TSignedSeqPos GetExtendedFrom() const { return m_from.m_pos - m_from.m_extra; }
        TSignedSeqPos GetExtendedTo() const { return m_to.m_pos+m_to.m_extra; }
        TSignedSeqPos GetExtraFrom() const { return m_from.m_extra; }
        TSignedSeqPos GetExtraTo() const { return m_to.m_extra; }
        EEdgeType GetTypeFrom() const { return m_from.m_edge_type; }
        EEdgeType GetTypeTo() const { return m_to.m_edge_type; }
        bool operator<(const SMapRange& mr) const {
            if(m_from.m_pos == mr.m_from.m_pos) return m_to.m_pos < mr.m_to.m_pos;
            else return m_from.m_pos < mr.m_from.m_pos;
        }
    
    private:
        SMapRangeEdge m_from, m_to;
    };


private:
    static TSignedSeqPos MapAtoB(const vector<CAlignMap::SMapRange>& a, const vector<CAlignMap::SMapRange>& b, TSignedSeqPos p, ERangeEnd move_mode);
    static TSignedSeqRange MapRangeAtoB(const vector<CAlignMap::SMapRange>& a, const vector<CAlignMap::SMapRange>& b, TSignedSeqRange r, ERangeEnd lend, ERangeEnd rend);
    static TSignedSeqRange MapRangeAtoB(const vector<CAlignMap::SMapRange>& a, const vector<CAlignMap::SMapRange>& b, TSignedSeqRange r, bool withextras ) {
        return MapRangeAtoB(a, b, r, withextras?eLeftEnd:eSinglePoint, withextras?eRightEnd:eSinglePoint);
    };
    static int FindLowerRange(const vector<CAlignMap::SMapRange>& a,  TSignedSeqPos p);

    void InsertOneToOneRange(TSignedSeqPos orig_start, TSignedSeqPos edited_start, int len, int left_orige, int left_edite, int right_orige, int right_edite, EEdgeType left_type, EEdgeType right_type);
    TSignedSeqPos InsertIndelRangesForInterval(TSignedSeqPos orig_a, TSignedSeqPos orig_b, TSignedSeqPos edit_a, TInDels::const_iterator fsi_begin, TInDels::const_iterator fsi_end, EEdgeType type_a, EEdgeType type_b);

    vector<SMapRange> m_orig_ranges, m_edited_ranges;
    EStrand m_orientation;
    int m_target_len;
};




class CAlignModel : public CGeneModel {
public:
    CAlignModel() {}
    CAlignModel(const objects::CSeq_align& seq_align);
    CAlignModel(const CGeneModel& g, const CAlignMap& a);
    TSignedSeqRange TranscriptExon(int i) const;
    virtual CAlignMap GetAlignMap() const { return m_alignmap; }
    void ResetAlignMap();

    string TargetAccession() const;
    void SetTargetId(const objects::CSeq_id& id) { m_target_id.Reset(&id); }
    CConstRef<objects::CSeq_id> GetTargetId() const { return m_target_id; }
    int TargetLen() const { return m_alignmap.TargetLen(); }
    TInDels GetInDels(bool fs_only) const;
    TInDels GetInDels(TSignedSeqPos a, TSignedSeqPos b, bool fs_only) const;
    int PolyALen() const;

private:
    CAlignMap m_alignmap;
    CConstRef<objects::CSeq_id> m_target_id;
};




struct NCBI_XALGOGNOMON_EXPORT setcontig {
    const string& m_contig;
    explicit setcontig(const string& cntg) : m_contig(cntg) {}
};
struct NCBI_XALGOGNOMON_EXPORT getcontig {
    string& m_contig;
    explicit getcontig(string& cntg) : m_contig(cntg) {}
};
NCBI_XALGOGNOMON_EXPORT CNcbiOstream& operator<<(CNcbiOstream& s, const setcontig& c);
NCBI_XALGOGNOMON_EXPORT CNcbiIstream& operator>>(CNcbiIstream& s, const getcontig& c);

NCBI_XALGOGNOMON_EXPORT CNcbiIstream& operator>>(CNcbiIstream& s, CAlignModel& a);
NCBI_XALGOGNOMON_EXPORT CNcbiOstream& operator<<(CNcbiOstream& s, const CAlignModel& a);
NCBI_XALGOGNOMON_EXPORT CNcbiOstream& operator<<(CNcbiOstream& s, const CGeneModel& a);


template<class Model> 
class NCBI_XALGOGNOMON_EXPORT CModelCluster : public list<Model> {
public:
    typedef Model TModel;
    CModelCluster(int f = numeric_limits<int>::max(), int s = 0) : m_limits(f,s) {}
    CModelCluster(TSignedSeqRange limits) : m_limits(limits) {}
    void Insert(const Model& a) {
        m_limits.CombineWith(a.Limits());
        push_back(a);
    }
    void Splice(CModelCluster& c) { // elements removed from c and inserted into *this
        m_limits.CombineWith(c.Limits());
        splice(list<Model>::end(),c);
    }
    TSignedSeqRange Limits() const { return m_limits; }
    bool operator<(const CModelCluster& c) const { return Precede(m_limits, c.m_limits); }
    void Init(TSignedSeqPos first, TSignedSeqPos second) {
        list<Model>::clear();
        m_limits.SetFrom( first );
        m_limits.SetTo( second );
    }

private:
    TSignedSeqRange m_limits;
};

typedef CModelCluster<CGeneModel> TGeneModelCluster;
typedef CModelCluster<CAlignModel> TAlignModelCluster;

typedef list<CGeneModel> TGeneModelList;
typedef list<CAlignModel> TAlignModelList;


template<class Cluster> 
class NCBI_XALGOGNOMON_EXPORT CModelClusterSet : public set<Cluster> {
 public:
    typedef typename set<Cluster>::iterator Titerator;
    CModelClusterSet() {}
    void Insert(const typename Cluster::TModel& a) {
        Cluster clust;
        clust.Insert(a);
        pair<Titerator,Titerator> lim = set<Cluster>::equal_range(clust);
        for(Titerator it = lim.first; it != lim.second;) {
            clust.Splice(const_cast<Cluster&>(*it));
            erase(it++);
        }
        const_cast<Cluster&>(*insert(lim.second,Cluster(clust.Limits()))).Splice(clust);
    }
};

typedef CModelClusterSet<TGeneModelCluster> TGeneModelClusterSet;
typedef CModelClusterSet<TAlignModelCluster> TAlignModelClusterSet;


END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___GNOMON_MODEL__HPP
