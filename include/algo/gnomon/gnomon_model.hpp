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
class CGenetic_code;
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
inline EStrand OtherStrand(EStrand s) { return (s == ePlus ? eMinus : ePlus); }

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

class CAlignModel;

class NCBI_XALGOGNOMON_EXPORT CInDelInfo
{
public:

    struct SSource {
        SSource() : m_strand(ePlus) {}
        string m_acc;
        TSignedSeqRange m_range;
        EStrand m_strand;
    };

    CInDelInfo(TSignedSeqPos l = 0, int len = 0, bool is_i = true, const string& v = kEmptyStr, const SSource& s = SSource()) : m_loc(l), m_len(len), m_is_insert(is_i), m_indelv(v),  m_source(s)
    {
        _ASSERT(m_indelv.empty() || (int)m_indelv.length() == len);
        if(IsDeletion() && GetInDelV().empty())
            m_indelv.insert( m_indelv.end(), Len(),'N');
    }
    TSignedSeqPos Loc() const { return m_loc; }
    int Len() const { return m_len; }
    bool IsInsertion() const { return m_is_insert; }
    bool IsDeletion() const { return !m_is_insert; }
    int IsMismatch(const CInDelInfo& next) const 
    { 
        if(IsInsertion() && next.IsDeletion() && Loc()+Len() == next.Loc())
            return min(Len(),next.Len()); 
        else
            return 0;
    }
    bool IntersectingWith(TSignedSeqPos a, TSignedSeqPos b) const    // insertion at least partially inside, deletion inside or flanking
    {
        return (IsDeletion() && Loc() >= a && Loc() <= b+1) ||
            (IsInsertion() && Loc() <= b && a <= Loc()+Len()-1);
    }
    bool operator<(const CInDelInfo& fsi) const    // source is ignored!!!!!!!!!!!
    {
        if(m_loc != fsi.m_loc)
            return m_loc < fsi.m_loc;
        else if(IsDeletion() != fsi.IsDeletion())
            return IsDeletion();  // if location is same deletion first
        else if(m_len != fsi.m_len)
            return m_len < fsi.m_len;
        else
            return m_indelv < fsi.m_indelv;
    }
    bool operator!=(const CInDelInfo& fsi) const { return (*this < fsi || fsi < *this); }
    bool operator==(const CInDelInfo& fsi) const { return !(*this != fsi); }
    string GetInDelV() const { return m_indelv; }
    const SSource& GetSource() const { return m_source; }

private:
    TSignedSeqPos m_loc;  // left location for insertion, deletion is before m_loc
                          // insertion - when there are extra bases in the genome
    int m_len;
    bool m_is_insert;
    string m_indelv;
    SSource m_source;
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
    CModelExon(TSignedSeqPos f = 0, TSignedSeqPos s = 0, bool fs = false, bool ss = false, const string& fsig = "", const string& ssig = "", double ident = 0, const string& seq = "", const CInDelInfo::SSource& src = CInDelInfo::SSource()) : 
        m_fsplice(fs), m_ssplice(ss), m_fsplice_sig(fsig), m_ssplice_sig(ssig), m_ident(ident), m_seq(seq), m_source(src), m_range(f,s) 
    { 
        _ASSERT(m_seq.empty() || m_range.Empty()); 
    };

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
    string m_fsplice_sig, m_ssplice_sig;   // obeys strand
    double m_ident;
    string m_seq;   // exon sequence if in gap; obeys strand
    CInDelInfo::SSource m_source;

    void Remap(const CRangeMapper& mapper) { m_range = mapper(m_range); }
private:
    TSignedSeqRange m_range;
};

class CAlignMap;

class CCDSInfo {
public:
    CCDSInfo(bool gcoords = true): m_confirmed_start(false), m_confirmed_stop(false), m_open(false), m_score(BadScore()), m_genomic_coordinates(gcoords) {}

    bool operator== (const CCDSInfo& another) const;

    //CDS mapped to transcript should be used only for for final models (not alignments)
    //Change in indels or 5' UTR will invalidate the cooordinates (in particular conversion from CAlignModel to CGeneModel);
    bool IsMappedToGenome() const { return m_genomic_coordinates; }
    CCDSInfo MapFromOrigToEdited(const CAlignMap& amap) const;
    CCDSInfo MapFromEditedToOrig(const CAlignMap& amap) const;  // returns 'empty' CDS if can't map

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
#ifdef _DEBUG
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
            _ASSERT(MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom() && MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo());
        } else if (HasStart() && !HasStop()) {
            if (Precede(Start(), ReadingFrame())) {
                _ASSERT( MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo() );
            } else {
                _ASSERT( MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom() );
            }
        } else if (HasStart() && HasStop()) {
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
#endif

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

    bool m_genomic_coordinates;
};


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
        eNotForChaining = 64,
        eChain = 128,
        eGnomon = 256
    };
    static string TypeToString(int type);

    enum EStatus {
        ecDNAIntrons = 1,
        eReversed = 2,
        eSkipped = 4,
        eLeftTrimmed = 8,
        eRightTrimmed = 16,
        eFullSupCDS = 32,
        ePseudo = 64,
        ePolyA = 128,
        eCap = 256,
        eBestPlacement = 512,
        eUnknownOrientation = 1024,
        eConsistentCoverage = 2048,
        eGapFiller = 4096,
        eUnmodifiedAlign = 8192,
        eChangedByFilter = 16384,
        eTSA = 32768
    };

    CGeneModel(EStrand s = ePlus, Int8 id = 0, int type = 0) :
        m_type(type), m_id(id), m_status(0), m_ident(0), m_weight(1), m_expecting_hole(false), m_strand(s), m_geneid(0), m_rank_in_gene(0) {}
    virtual ~CGeneModel() {}

    void AddExon(TSignedSeqRange exon, const string& fs = "", const string& ss = "", double ident = 0, const string& seq = "", const CInDelInfo::SSource& src = CInDelInfo::SSource());
    void AddHole(); // between model and next exons
    void AddGgapExon(double ident, const string& seq, const CInDelInfo::SSource& src, bool infront);
    void AddNormalExon(TSignedSeqRange exon, const string& fs, const string& ss, double ident, bool infront);

    typedef vector<CModelExon> TExons;
    const TExons& Exons() const { return m_exons; }
    void ClearExons() {
        m_exons.clear();
        m_fshifts.clear();
        m_range = TSignedSeqRange::GetEmpty();
        m_cds_info = CCDSInfo();
        m_edge_reading_frames.clear();
    }

    void ReverseComplementModel();

    void Remap(const CRangeMapper& mapper);
    enum EClipMode { eRemoveExons, eDontRemoveExons };
    virtual void Clip(TSignedSeqRange limits, EClipMode mode, bool ensure_cds_invariant = true);  // drops the score!!!!!!!!!
    virtual void CutExons(TSignedSeqRange hole); // clip or remove exons, dangerous, should be completely in or outside the cds, should not cut an exon in two 
    void ExtendLeft(int amount);
    void ExtendRight(int amount);
    void Extend(const CGeneModel& a, bool ensure_cds_invariant = true);
    void TrimEdgesToFrameInOtherAlignGaps(const TExons& exons_with_gaps, bool ensure_cds_invariant = true);
    void RemoveShortHolesAndRescore(const CGnomonEngine& gnomon);   // removes holes shorter than min intron (may add frameshifts/stops)
   
    TSignedSeqRange TranscriptExon(int i) const; 

    TSignedSeqRange Limits() const { return m_range; }
    TSignedSeqRange TranscriptLimits() const;
    int AlignLen() const ;
    void RecalculateLimits();

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
    EStrand Orientation() const {
        bool notreversed = (Status()&CGeneModel::eReversed) == 0;
        bool plusstrand = Strand() == ePlus;
        return (notreversed == plusstrand) ? ePlus : eMinus;
    }

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

    bool isNMD(int limit = 50) const;

    TInDels& FrameShifts() { return m_fshifts; }
    const TInDels& FrameShifts() const { return m_fshifts; }
    TInDels FrameShifts(TSignedSeqPos a, TSignedSeqPos b) const;
    void RemoveExtraFShifts();

    int FShiftedLen(TSignedSeqRange ab, bool withextras = true) const;    // won't work if a/b is insertion
    int FShiftedLen(TSignedSeqPos a, TSignedSeqPos b, bool withextras = true) const  { return FShiftedLen(TSignedSeqRange(a,b),withextras); }

    // move along mrna skipping introns
    TSignedSeqPos FShiftedMove(TSignedSeqPos pos, int len) const;  // may retun <0 if hits a deletion at the end of move

    virtual CAlignMap GetAlignMap() const;

    string GetCdsDnaSequence (const CResidueVec& contig_sequence) const;
    string GetProtein (const CResidueVec& contig_sequence) const;
    string GetProtein (const CResidueVec& contig_sequence, const CGenetic_code* gencode) const;
    
    // Below comparisons ignore CDS completely, first 3 assume that alignments are the same strand
    
    int isCompatible(const CGeneModel& a) const;  // returns 0 for notcompatible or (number of common splices)+1
    bool IsSubAlignOf(const CGeneModel& a) const;
    int MutualExtension(const CGeneModel& a) const;  // returns 0 for notcompatible or (number of introns) + 1
    
    bool IdenticalAlign(const CGeneModel& a) const
    { return Strand()==a.Strand() && Limits()==a.Limits() && Exons() == a.Exons() && FrameShifts()==a.FrameShifts() && 
            GetCdsInfo().PStops() == a.GetCdsInfo().PStops() && Type() == a.Type() && Status() == a.Status(); }
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

    friend class CChain;
};


class CAlignMap {
public:
    enum EEdgeType { eBoundary, eSplice, eInDel, eGgap };
    enum ERangeEnd{ eLeftEnd, eRightEnd, eSinglePoint };

    CAlignMap() {};
    CAlignMap(TSignedSeqPos orig_a, TSignedSeqPos orig_b) : m_orientation(ePlus) {
        m_orig_ranges.push_back(SMapRange(SMapRangeEdge(orig_a), SMapRangeEdge(orig_b)));
        m_edited_ranges = m_orig_ranges;
        m_target_len = FShiftedLen(orig_a, orig_b); 
    }
    CAlignMap(TSignedSeqPos orig_a, TSignedSeqPos orig_b, TInDels::const_iterator fsi_begin, const TInDels::const_iterator fsi_end) : m_orientation(ePlus) {
        EEdgeType atype = eBoundary;
        EEdgeType btype = eBoundary;
        if(fsi_begin != fsi_end) {
            if(fsi_begin->Loc() == orig_a) {
                _ASSERT(fsi_begin->IsDeletion());   // no reason to have insertion
                atype = eInDel;
            }
            TInDels::const_iterator fs = fsi_end-1;
            if(fs->Loc() == orig_b+1 && fs->IsDeletion())
                btype = eInDel;
        }
        InsertIndelRangesForInterval(orig_a, orig_b, 0, fsi_begin, fsi_end, atype, btype, "", ""); 
        m_target_len = FShiftedLen(orig_a, orig_b);
    }
    CAlignMap(const CGeneModel::TExons& exons, const vector<TSignedSeqRange>& transcript_exons, const TInDels& indels, EStrand orientation, int targetlen );     //orientation == strand if not Reversed
    CAlignMap(const CGeneModel::TExons& exons, const TInDels& frameshifts, EStrand strand, TSignedSeqRange lim = TSignedSeqRange::GetWhole(), int holelen = 0, int polyalen = 0);
    TSignedSeqPos MapOrigToEdited(TSignedSeqPos orig_pos) const;
    TSignedSeqPos MapEditedToOrig(TSignedSeqPos edited_pos) const;
    TSignedSeqRange MapRangeOrigToEdited(TSignedSeqRange orig_range, ERangeEnd lend, ERangeEnd rend) const;
    TSignedSeqRange MapRangeOrigToEdited(TSignedSeqRange orig_range, bool withextras = true) const { return MapRangeOrigToEdited(orig_range, withextras?eLeftEnd:eSinglePoint, withextras?eRightEnd:eSinglePoint); }
    TSignedSeqRange MapRangeEditedToOrig(TSignedSeqRange edited_range, bool withextras = true) const;
    template <class Vec>
    void EditedSequence(const Vec& original_sequence, Vec& edited_sequence, bool includeholes = false) const;
    int FShiftedLen(TSignedSeqRange ab, ERangeEnd lend, ERangeEnd rend) const;
    int FShiftedLen(TSignedSeqRange ab, bool withextras = true) const;
    int FShiftedLen(TSignedSeqPos a, TSignedSeqPos b, bool withextras = true) const { return FShiftedLen(TSignedSeqRange(a,b), withextras); }
    //snap to codons works by analising transcript coordinates (MUST be a protein or reading frame cutout)
    TSignedSeqRange ShrinkToRealPoints(TSignedSeqRange orig_range, bool snap_to_codons = false) const;
    TSignedSeqPos FShiftedMove(TSignedSeqPos orig_pos, int len) const;   // may reurn < 0 if hits a gap
    TInDels GetInDels(bool fs_only) const;
    int TargetLen() const { return m_target_len; }

// private: // breaks SMapRange on WorkShop. :-/
    struct SMapRangeEdge {
        SMapRangeEdge(TSignedSeqPos p, TSignedSeqPos e = 0, EEdgeType t = eBoundary, const string& seq = kEmptyStr) : m_pos(p), m_extra(e), m_edge_type(t), m_extra_seq(seq) {}
        bool operator<(const SMapRangeEdge& mre) const { return m_pos < mre.m_pos; }
        bool operator==(const SMapRangeEdge& mre) const { return m_pos == mre.m_pos; }

        TSignedSeqPos m_pos, m_extra;
        EEdgeType m_edge_type;
        string m_extra_seq;
    };
    
    class SMapRange {
    public:
        SMapRange(SMapRangeEdge from, SMapRangeEdge to) : m_from(from), m_to(to) {}
        SMapRangeEdge GetEdgeFrom() const { return m_from; }
        SMapRangeEdge GetEdgeTo() const { return m_to; }
        void SetEdgeFrom(SMapRangeEdge from) { m_from = from; }
        void SetEdgeTo(SMapRangeEdge to) { m_to = to; }
        TSignedSeqPos GetFrom() const { return m_from.m_pos; }
        TSignedSeqPos GetTo() const { return m_to.m_pos; }
        TSignedSeqPos GetExtendedFrom() const { return m_from.m_pos - m_from.m_extra; }
        TSignedSeqPos GetExtendedTo() const { return m_to.m_pos+m_to.m_extra; }
        TSignedSeqPos GetExtraFrom() const { return m_from.m_extra; }
        string GetExtraSeqFrom() const { return m_from.m_extra_seq; }
        TSignedSeqPos GetExtraTo() const { return m_to.m_extra; }
        string GetExtraSeqTo() const { return m_to.m_extra_seq; }
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

    void InsertOneToOneRange(TSignedSeqPos orig_start, TSignedSeqPos edited_start, int len, int left_orige, int left_edite, int right_orige, int right_edite, EEdgeType left_type, EEdgeType right_type, const string& left_edit_extra_seq, const string& right_edit_extra_seq);
    TSignedSeqPos InsertIndelRangesForInterval(TSignedSeqPos orig_a, TSignedSeqPos orig_b, TSignedSeqPos edit_a, TInDels::const_iterator fsi_begin, TInDels::const_iterator fsi_end, EEdgeType type_a, EEdgeType type_b, const string& gseq_a, const string& gseq_b);

    vector<SMapRange> m_orig_ranges, m_edited_ranges;
    EStrand m_orientation;
    int m_target_len;
};




class NCBI_XALGOGNOMON_EXPORT CAlignModel : public CGeneModel {
public:
    CAlignModel() {}
    CAlignModel(const objects::CSeq_align& seq_align);
    CAlignModel(const CGeneModel& g, const CAlignMap& a);
    virtual CAlignMap GetAlignMap() const { return m_alignmap; }
    void ResetAlignMap();

    virtual void Clip(TSignedSeqRange limits, EClipMode mode, bool ensure_cds_invariant = true) { // drops the score!!!!!!!!!
        CGeneModel::Clip(limits,mode,ensure_cds_invariant);
        RecalculateAlignMap();
    }
    virtual void CutExons(TSignedSeqRange hole) { // clip or remove exons, dangerous, should be completely in or outside the cds, should not cut an exon in two 
        CGeneModel::CutExons(hole);
        RecalculateAlignMap();
    }

    string TargetAccession() const;
    void SetTargetId(const objects::CSeq_id& id) { m_target_id.Reset(&id); }
    CConstRef<objects::CSeq_id> GetTargetId() const { return m_target_id; }
    int TargetLen() const { return m_alignmap.TargetLen(); }
    TInDels GetInDels(bool fs_only) const;
    TInDels GetInDels(TSignedSeqPos a, TSignedSeqPos b, bool fs_only) const;
    int PolyALen() const;
    CRef<objects::CSeq_align> MakeSeqAlign(const string& contig) const;   // should be used for alignments only; for chains and models will produce a Splign alignment of mRNA

private:
    void RecalculateAlignMap();
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
        this->push_back(a);
    }
    void Splice(CModelCluster& c) { // elements removed from c and inserted into *this
        m_limits.CombineWith(c.Limits());
        this->splice(list<Model>::end(),c);
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
            this->erase(it++);
        }
        const_cast<Cluster&>(*this->insert(lim.second,Cluster(clust.Limits()))).Splice(clust);
    }
};

typedef CModelClusterSet<TGeneModelCluster> TGeneModelClusterSet;
typedef CModelClusterSet<TAlignModelCluster> TAlignModelClusterSet;


enum EResidueNames { enA, enC, enG, enT, enN };

class EResidue {
public :
    EResidue() : data(enN) {}
    EResidue(EResidueNames e) : data(e) {}

    operator int() const { return int(data); }

private:
    unsigned char data;
};

inline TResidue Complement(TResidue c)
{
    switch(c)
    {
        case 'A': 
            return 'T';
        case 'a': 
            return 't';
        case 'C':
            return 'G'; 
        case 'c': 
            return 'g';
        case 'G':
            return 'C'; 
        case 'g': 
            return 'c';
        case 'T':
            return 'A'; 
        case 't': 
            return 'a';
        default:
            return 'N';
    }
}

extern const EResidue    k_toMinus[5];
extern const char *const k_aa_table;

inline EResidue Complement(EResidue c)
{
    return k_toMinus[c];
}

template <class BidirectionalIterator>
void ReverseComplement(const BidirectionalIterator& first, const BidirectionalIterator& last)
{
    for (BidirectionalIterator i( first ); i != last; ++i)
        *i = Complement(*i);
    reverse(first, last);
}


END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___GNOMON_MODEL__HPP
