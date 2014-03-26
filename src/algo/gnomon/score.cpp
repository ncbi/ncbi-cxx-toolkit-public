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
#include <algo/gnomon/gnomon.hpp>
#include <algo/gnomon/chainer.hpp>
#include <algo/gnomon/gnomon_exception.hpp>
#include "score.hpp"
#include "hmm.hpp"
#include "hmm_inlines.hpp"
#include "gnomon_engine.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

bool CSeqScores::isStart(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+2 >= SeqLen()) return false;  //out of range
    else if(ss[ii] != enA || ss[ii+1] != enT || ss[ii+2] != enG) return false;
    else return true;
}

bool CSeqScores::isStop(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+2 >= SeqLen()) return false;  //out of range
    if((ss[ii] != enT || ss[ii+1] != enA || ss[ii+2] != enA) &&
        (ss[ii] != enT || ss[ii+1] != enA || ss[ii+2] != enG) &&
        (ss[ii] != enT || ss[ii+1] != enG || ss[ii+2] != enA)) return false;
    else return true;
}

bool CSeqScores::isReadingFrameLeftEnd(int i, int strand) const
{
    if(strand == ePlus) {
        return isStart(i-3, strand);
    } else {
        return isStop(i-1,strand);
    }
}

bool CSeqScores::isReadingFrameRightEnd(int i, int strand) const
{
    if(strand == ePlus) {
        return isStop(i+1, strand);
    } else {
        return isStart(i+3,strand);
    }
}

bool CSeqScores::isAG(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii-1 < 0 || ii >= SeqLen()) return false;  //out of range
    if(ss[ii-1] != enA || ss[ii] != enG) return false;
    else return true;
}

bool CSeqScores::isGT(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+1 >= SeqLen()) return false;  //out of range
    if(ss[ii] != enG || ss[ii+1] != enT) return false;
    else return true;
}

bool CSeqScores::isConsensusIntron(int i, int j, int strand) const
{
    if(strand == ePlus) return (m_dscr[ePlus][i-1] != BadScore()) && (m_ascr[ePlus][j] != BadScore());
    else                return (m_ascr[eMinus][i-1] != BadScore()) && (m_dscr[eMinus][j] != BadScore());
//    if(strand == ePlus) return isGT(i,strand) && isAG(j,strand);
//    else return isAG(i,strand) && isGT(j,strand);
}

const EResidue* CSeqScores::SeqPtr(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    return &ss.front()+ii;
}

bool CSeqScores::StopInside(int a, int b, int strand, int frame) const
{
    return (a <= m_laststop[strand][frame][b]);
}

bool CSeqScores::OpenCodingRegion(int a, int b, int strand, int frame) const
{
    return (a > m_notinexon[strand][frame][b]);
}

bool CSeqScores::OpenNonCodingRegion(int a, int b, int strand) const 
{
    return (a > m_notinintron[strand][b]);
}

double CSeqScores::CodingScore(int a, int b, int strand, int frame) const
{
    if(a > b) return 0; // for splitted start/stop
    double score = m_cdrscr[strand][frame][b];
    if(a > 0) score -= m_cdrscr[strand][frame][a-1];
    return score;
}

double CSeqScores::NonCodingScore(int a, int b, int strand) const
{
    double score = m_ncdrscr[strand][b];
    if(a > 0) score -= m_ncdrscr[strand][a-1];
    return score;
}

bool CSeqScores::OpenIntergenicRegion(int a, int b) const
{
    return (a > m_notining[b]);
} 

double CSeqScores::IntergenicScore(int a, int b, int strand) const
{
    _ASSERT(strand == 0 || strand == 1);
    _ASSERT(b>= 0 && b < (int)m_ingscr[strand].size());
    double score = m_ingscr[strand][b];
    if(a > 0) {
        _ASSERT(a-1 < (int)m_ingscr[strand].size()); 
        score -= m_ingscr[strand][a-1];
    }
    return score;
}

CResidueVec CSeqScores::ConstructSequenceAndMaps(const TGeneModelList& aligns, const CResidueVec& original_sequence)
{
    TSignedSeqRange chunk(m_chunk_start, m_chunk_stop);
    ITERATE(TGeneModelList, it, aligns) {
        const CGeneModel& align = *it;
        if (Include(chunk, align.MaxCdsLimits()))
            m_fshifts.insert(m_fshifts.end(),align.FrameShifts().begin(),align.FrameShifts().end());
    }
    sort(m_fshifts.begin(),m_fshifts.end());
    m_map = CAlignMap(m_chunk_start, m_chunk_stop, m_fshifts.begin(), m_fshifts.end());
    CResidueVec sequence;
    m_map.EditedSequence(original_sequence, sequence);

    _ASSERT( m_map.MapEditedToOrig(0) == From() && m_map.MapOrigToEdited(From()) == 0 );
    _ASSERT( m_map.MapEditedToOrig((int)sequence.size()-1) == To() && m_map.MapOrigToEdited(To()) == (int)sequence.size()-1 );

    return sequence;
}

struct SAlignOrder
{
    bool operator() (const CGeneModel& a, const CGeneModel& b) const
    {
        TSignedSeqPos x = a.ReadingFrame().NotEmpty() ? a.ReadingFrame().GetFrom() : a.Limits().GetFrom();
        TSignedSeqPos y = b.ReadingFrame().NotEmpty() ? b.ReadingFrame().GetFrom() : b.Limits().GetFrom();

        if (x==y)
            return a.ID()<b.ID();
        return x < y;
    }
};

typedef set<CGeneModel,SAlignOrder> TAlignSet;

struct CIndelMapper: public CRangeMapper {
    CIndelMapper(const CAlignMap& seq_map):
        m_seq_map(seq_map) {}

    const CAlignMap& m_seq_map;

    TSignedSeqRange operator() (TSignedSeqRange r, bool withextras = true) const
    {
        //        if(withextras)
            //            r = m_seq_map.ShrinkToRealPoints(r); // if exon starts/ends with insertion move to projectable points 
        return m_seq_map.MapRangeOrigToEdited(r, withextras);
    }
};

struct Is_ID_equal {
    Int8 m_id;
    Is_ID_equal(Int8 id): m_id(id) {}
    bool operator()(const CGeneModel& a)
    {
        return a.ID()==m_id;
    }
};

TSignedSeqPos AlignLeftLimit(const CGeneModel& a)
{
    return ( a.MaxCdsLimits().Empty() ? a.Limits().GetFrom() : a.MaxCdsLimits().GetFrom() );
}

static bool s_AlignLeftLimitOrder(const CGeneModel& ap, const CGeneModel& bp)
{
    return (AlignLeftLimit(ap) < AlignLeftLimit(bp));
}


CSeqScores::CSeqScores (const CTerminal& a, const CTerminal& d, const CTerminal& stt, const CTerminal& stp, 
const CCodingRegion& cr, const CNonCodingRegion& ncr, const CNonCodingRegion& ing,
               const CIntronParameters&     intron_params,
                        TSignedSeqPos from, TSignedSeqPos to, const TGeneModelList& cls, const TInDels& initial_fshifts, double mpp, const CGnomonEngine& gnomon)
: m_acceptor(a), m_donor(d), m_start(stt), m_stop(stp), m_cdr(cr), m_ncdr(ncr), m_intrg(ing), 
  m_align_list(cls), m_fshifts(initial_fshifts), m_map(from,to), m_chunk_start(from), m_chunk_stop(to), m_mpp(mpp)
{
    m_align_list.sort(s_AlignLeftLimitOrder);
    NON_CONST_ITERATE(TGeneModelList, it, m_align_list) {
        CGeneModel& align = *it;
        CCDSInfo cds_info = align.GetCdsInfo();
        bool fixed = false;
        for(unsigned int i = 1; i < align.Exons().size(); ++i) {
            if (!align.Exons()[i-1].m_ssplice || !align.Exons()[i].m_fsplice) {
                int hole_len = align.Exons()[i].GetFrom()-align.Exons()[i-1].GetTo()-1;
                if(hole_len <= intron_params.MinLen()) {
                    fixed = true;
                    TSignedSeqRange pstop(align.Exons()[i-1].GetTo(),align.Exons()[i].GetFrom());     // to make sure GetScore doesn't complain about "new" pstops
                    cds_info.AddPStop(pstop);
                    if(hole_len%3 != 0) {
                        align.FrameShifts().push_back(CInDelInfo((align.Exons()[i-1].GetTo()+align.Exons()[i].GetFrom())/2, hole_len%3, true));
                    }

                    CGeneModel a(align.Strand());
                    a.AddExon(TSignedSeqRange(align.Exons()[i-1].GetTo(),align.Exons()[i].GetFrom()));
                    align.Extend(a);

                    --i;
                }
            }
        }
        if(fixed) {
            align.SetCdsInfo(cds_info);
            sort(align.FrameShifts().begin(),align.FrameShifts().end());
            gnomon.GetScore(align);                                                        // calculate possible new pstops
        }
    }
}

void CSeqScores::Init( CResidueVec& original_sequence, bool repeats, bool leftwall, bool rightwall, double consensuspenalty, const CIntergenicParameters& intergenic_params, const CGnomonAnnotator_Base::TGgapInfo& ggapinfo)
{
    CResidueVec sequence = ConstructSequenceAndMaps(m_align_list,original_sequence);

    TSignedSeqPos len = sequence.size();
    
    try {
        m_notining.resize(len,-1);
        m_inalign.resize(len,numeric_limits<int>::max());
        m_protnum.resize(len,0);

        for(int strand = 0; strand < 2; ++strand) {
            m_seq[strand].resize(len);
            m_ascr[strand].resize(len,BadScore());
            m_dscr[strand].resize(len,BadScore());
            m_sttscr[strand].resize(len,BadScore());
            m_stpscr[strand].resize(len,BadScore());
            m_ncdrscr[strand].resize(len,BadScore());
            m_ingscr[strand].resize(len,BadScore());
            m_notinintron[strand].resize(len,-1);
            for(int frame = 0; frame < 3; ++frame) {
                m_cdrscr[strand][frame].resize(len,BadScore());
                m_laststop[strand][frame].resize(len,-1);
                m_notinexon[strand][frame].resize(len,-1);
            }
            for(int ph = 0; ph < 2; ++ph) {
                m_asplit[strand][ph].resize(len,0);
                m_dsplit[strand][ph].resize(len,0);
            }
        }
    } catch(bad_alloc) {
        NCBI_THROW(CGnomonException, eMemoryLimit, "Not enough memory in CSeqScores");
    }

    ITERATE(CGnomonAnnotator_Base::TGgapInfo, ig, ggapinfo) {      // block ab initio in ggaps
        for(int jj = ig->first; jj <= ig->first + (int)ig->second->GetInDelV().length() - 1; ++jj) {
            if(jj >= m_chunk_start && jj <= m_chunk_stop) {
                int j = m_map.MapOrigToEdited(jj);
                m_laststop[ePlus][0][j] = j;
                m_laststop[ePlus][1][j] = j;
                m_laststop[ePlus][2][j] = j;
                m_laststop[eMinus][0][j] = j;
                m_laststop[eMinus][1][j] = j;
                m_laststop[eMinus][2][j] = j;
            }
        }
    }

    const int RepeatMargin = 25;
    for(int rpta = 0; rpta < len; ) {
        while(rpta < len && isupper(sequence[rpta]))
            ++rpta;
        int rptb = rpta;
        while(rptb+1 < len && islower(sequence[rptb+1]))
            ++rptb;
        if(rptb-rpta+1 > 2*RepeatMargin) {
            for(TSignedSeqPos i = rpta+RepeatMargin; i <= rptb-RepeatMargin; ++i) {  // masking repeats
                m_laststop[ePlus][0][i] = i;
                m_laststop[ePlus][1][i] = i;
                m_laststop[ePlus][2][i] = i;
                m_laststop[eMinus][0][i] = i;
                m_laststop[eMinus][1][i] = i;
                m_laststop[eMinus][2][i] = i;
            }
        }
        rpta = rptb+1;
    }

    for(TSignedSeqPos i = 0; i < len; ++i) {
        char c = sequence[i];
        EResidue l = fromACGT(c);
        m_seq[ePlus][i] = l;
        m_seq[eMinus][len-1-i] = k_toMinus[l];
    }
    
    const int NLimit = 6;
    CEResidueVec::iterator it = search_n(m_seq[ePlus].begin(),m_seq[ePlus].end(),NLimit,enN);
    while(it != m_seq[ePlus].end())           // masking big chunks of Ns
    {
        for(; it != m_seq[ePlus].end() && *it == enN; ++it) 
        {
            int j = it-m_seq[ePlus].begin();
            m_laststop[ePlus][0][j] = j;
            m_laststop[ePlus][1][j] = j;
            m_laststop[ePlus][2][j] = j;
            m_laststop[eMinus][0][j] = j;
            m_laststop[eMinus][1][j] = j;
            m_laststop[eMinus][2][j] = j;
        }
        it = search_n(it,m_seq[ePlus].end(),NLimit,enN); 
    } 
    
    CIndelMapper mapper(m_map);
    
    TAlignSet allaligns; 

    TSignedSeqRange chunk(m_chunk_start, m_chunk_stop);

    ITERATE(TGeneModelList, it, m_align_list) {
        const CGeneModel& origalign = *it;
        TSignedSeqRange limits = origalign.Limits() & chunk;
        if (limits.Empty() || (origalign.MaxCdsLimits().NotEmpty() &&  !Include(limits, origalign.MaxCdsLimits())))
            continue;

        CGeneModel align = origalign;
        align.Clip(chunk, CGeneModel::eRemoveExons);

        bool opposite = false;
        if((align.Type() & CGeneModel::eNested)!=0 || (align.Type() & CGeneModel::eWall)!=0) {
            ITERATE(TGeneModelList, jt, m_align_list) {
                if(it == jt) continue;
                const CGeneModel& aa = *jt;
                if((aa.Type() & CGeneModel::eWall)==0 && (aa.Type() & CGeneModel::eNested)==0 && 
                   aa.MaxCdsLimits().NotEmpty() && aa.MaxCdsLimits().IntersectingWith(align.Limits())) {
                    opposite = true;
                    break;
                }
            }
        }

        align.Remap(mapper);
        align.FrameShifts().clear();

        EStrand strand = align.Strand();

        if((align.Type()&CGeneModel::eWall)==0 && (align.Type()&CGeneModel::eNested)==0) {
            CGeneModel al = align;
            int extrabases = m_start.Left()-3;   // bases before ATG needed for score;
            int extraNs5p = 0;                   // extra Ns if too close to the end of contig
            if(extrabases > al.Limits().GetFrom()) {
                extraNs5p = extrabases - al.Limits().GetFrom();
                if(al.Limits().GetFrom() > 0) al.ExtendLeft(al.Limits().GetFrom());
            } else {
                al.ExtendLeft(extrabases);
            }
            int extraNs3p = 0;                   // extra Ns if too close to the end of contig  
            if(al.Limits().GetTo()+extrabases > len-1) {
                extraNs3p = al.Limits().GetTo()+extrabases-(len-1);
                if(al.Limits().GetTo() < len-1) al.ExtendRight(len-1-al.Limits().GetTo());
            } else {
                al.ExtendRight(extrabases);
            }
            if(strand == eMinus) swap(extraNs5p,extraNs3p); 
            
            CEResidueVec mRNA;
            CAlignMap mrnamap(al.GetAlignMap());
            mrnamap.EditedSequence(m_seq[ePlus], mRNA);
            mRNA.insert(mRNA.begin(),extraNs5p,enN);
            mRNA.insert(mRNA.end(),extraNs3p,enN);

            // Here we are after splitted start/stops but this loop will score nonsplitted starts/stops as well 
            // "starts/stops" crossing a hole will get some score also but they will be ignored because a hole is alwais incide CDS 
            for(int k = extraNs5p; k < (int)mRNA.size()-extraNs3p; ++k) {
                int i;
                if(strand == ePlus) {
                    i = mrnamap.MapEditedToOrig(k-extraNs5p+1)-1;      // the position of G or right before the first coding exon if start is completely in another exon
                    if(align.MaxCdsLimits().Empty() || Include(align.MaxCdsLimits(),i))
                        m_sttscr[strand][i] = m_start.Score(mRNA,k);

                    i = mrnamap.MapEditedToOrig(k-extraNs5p);             // the last position in the last coding exon (usually right before T)
                    if(align.MaxCdsLimits().Empty() || Include(align.MaxCdsLimits(),i))
                        m_stpscr[strand][i] = m_stop.Score(mRNA,k);   
                } else {
                    i = mrnamap.MapEditedToOrig(k-extraNs5p+1);           // the last position in the last coding exon (usually right before G)
                    if(i >= 0 && (align.MaxCdsLimits().Empty() || Include(align.MaxCdsLimits(),i)))
                        m_sttscr[strand][i] = m_start.Score(mRNA,k);

                    i = mrnamap.MapEditedToOrig(k-extraNs5p)-1;        // the position of T or right before the first coding exon if stop is completely in another exon
                    if(i >= 0 && (align.MaxCdsLimits().Empty() || Include(align.MaxCdsLimits(),i)))
                        m_stpscr[strand][i] = m_stop.Score(mRNA,k);
                }
            }
        }
        
        _ASSERT(align.FShiftedLen(align.ReadingFrame(), false)%3==0);

        if(align.MaxCdsLimits().NotEmpty()) {
            limits = align.MaxCdsLimits();
            align.Clip(limits, CGeneModel::eRemoveExons);
        } else {
            limits = align.Limits();
        }

        _ASSERT( align.Exons().size() > 0 );

        if((align.Type() & CGeneModel::eNested)!=0) {
            //            int a = max(0,int(limits.GetFrom())-intergenic_params.MinLen()+1);
            //            int b = min(len-1,limits.GetTo()+intergenic_params.MinLen()-1);
            int a = limits.GetFrom();
            int b = limits.GetTo();
            
            if(opposite) {
                for(int pnt = a; pnt <= b; ++pnt) {           // allow prediction on the opposite strand of nested models   
                    m_notinexon[strand][0][pnt] = pnt;
                    m_notinexon[strand][1][pnt] = pnt;
                    m_notinexon[strand][2][pnt] = pnt;
                }
            } else {
                for(int pnt = a; pnt <= b; ++pnt) {
                    m_notinexon[ePlus][0][pnt] = pnt;
                    m_notinexon[ePlus][1][pnt] = pnt;
                    m_notinexon[ePlus][2][pnt] = pnt;
                    m_notinexon[eMinus][0][pnt] = pnt;
                    m_notinexon[eMinus][1][pnt] = pnt;
                    m_notinexon[eMinus][2][pnt] = pnt;
                }
            }
            continue;
        } else if((align.Type() & CGeneModel::eWall)!=0) {
        //            int a = max(0,int(limits.GetFrom())-intergenic_params.MinLen()+1);
        //            int b = min(len-1,limits.GetTo()+intergenic_params.MinLen()-1);
            int a = limits.GetFrom();
            int b = limits.GetTo();

            if(opposite) {
                for(int pnt = a; pnt <= b; ++pnt) {           // allow prediction on the opposite strand of complete models
                    m_notinexon[strand][0][pnt] = pnt;
                    m_notinexon[strand][1][pnt] = pnt;
                    m_notinexon[strand][2][pnt] = pnt;
                    
                    m_notinintron[strand][pnt] = pnt;
                }
            } else {
                for(int pnt = a; pnt <= b; ++pnt) {
                    m_notinexon[ePlus][0][pnt] = pnt;
                    m_notinexon[ePlus][1][pnt] = pnt;
                    m_notinexon[ePlus][2][pnt] = pnt;
                    m_notinexon[eMinus][0][pnt] = pnt;
                    m_notinexon[eMinus][1][pnt] = pnt;
                    m_notinexon[eMinus][2][pnt] = pnt;

                    m_notinintron[ePlus][pnt] = pnt;
                    m_notinintron[eMinus][pnt] = pnt;
                }
            }
            continue;
        }

        allaligns.insert(align);

        ITERATE(CGeneModel::TExons, e, align.Exons()) {
            for(TSignedSeqPos i = e->GetFrom(); i <= e->GetTo(); ++i) {  // ignoring repeats and allowing Ns in alignmnets
                m_laststop[strand][0][i] = -1;
                m_laststop[strand][1][i] = -1;
                m_laststop[strand][2][i] = -1;
            }
        }

        
        if((align.Type() & CGeneModel::eProt)!=0)
            m_protnum[align.ReadingFrame().GetFrom()] = 1;

        if(align.ReadingFrame().NotEmpty()) {  // enforcing frames of known CDSes and protein pieces
            TSignedSeqPos right = align.ReadingFrame().GetTo();
            int ff = (strand == ePlus) ? 2-right%3 : right%3;               // see cdrscr
            for(int frame = 0; frame < 3; ++frame) {
                if(frame != ff)
                    m_notinexon[strand][frame][right] = right;
            }

            ITERATE(CGeneModel::TExons, e, align.Exons()) {
                right = e->GetTo();
                if (!e->m_ssplice && right < align.Limits().GetTo()) {
                    ff = (strand == ePlus) ? 2-right%3 : right%3;
                    for(int frame = 0; frame < 3; ++frame) {
                        if(frame != ff)
                            m_notinexon[strand][frame][right] = right;
                    }
                }
            }
        }

        if(align.ConfirmedStart()) {        // enforcing confirmed start
            int pnt;
            if(align.Strand() == ePlus) {
                pnt = align.ReadingFrame().GetFrom()-1;
            } else {
                pnt = align.ReadingFrame().GetTo()+1;
            }
            m_notinexon[ePlus][0][pnt] = pnt;
            m_notinexon[ePlus][1][pnt] = pnt;
            m_notinexon[ePlus][2][pnt] = pnt;
            m_notinexon[eMinus][0][pnt] = pnt;
            m_notinexon[eMinus][1][pnt] = pnt;
            m_notinexon[eMinus][2][pnt] = pnt;
            
            m_notinintron[ePlus][pnt] = pnt;
            m_notinintron[eMinus][pnt] = pnt;
        }

        // restricting prediction to MaxCdsLimits if not infinite
        if(align.GetCdsInfo().MaxCdsLimits().NotEmpty()) {
            if(TSignedSeqRange::GetWholeFrom() < align.GetCdsInfo().MaxCdsLimits().GetFrom()) {
                m_notinexon[ePlus][0][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[ePlus][1][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[ePlus][2][limits.GetFrom()] = limits.GetFrom();
                m_notinintron[ePlus][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[eMinus][0][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[eMinus][1][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[eMinus][2][limits.GetFrom()] = limits.GetFrom();
                m_notinintron[eMinus][limits.GetFrom()] = limits.GetFrom();
            }
            if(align.GetCdsInfo().MaxCdsLimits().GetTo() < TSignedSeqRange::GetWholeTo()) {
                m_notinexon[ePlus][0][limits.GetTo()] = limits.GetTo();
                m_notinexon[ePlus][1][limits.GetTo()] = limits.GetTo();
                m_notinexon[ePlus][2][limits.GetTo()] = limits.GetTo();
                m_notinintron[ePlus][limits.GetTo()] = limits.GetTo();
                m_notinexon[eMinus][0][limits.GetTo()] = limits.GetTo();
                m_notinexon[eMinus][1][limits.GetTo()] = limits.GetTo();
                m_notinexon[eMinus][2][limits.GetTo()] = limits.GetTo();
                m_notinintron[eMinus][limits.GetTo()] = limits.GetTo();
            }
        }
    }
    
    for(TSignedSeqPos i = 1; i < len; ++i)
        m_protnum[i] += m_protnum[i-1];

    TAlignSet::iterator it_prev = allaligns.end();

    for(TAlignSet::iterator it = allaligns.begin(); it != allaligns.end(); ++it)
    {
        const CGeneModel& algn(*it);

        if (it_prev != allaligns.end()) {
            const CGeneModel& algn_prev(*it_prev);
            if(algn.Limits().IntersectingWith(algn_prev.Limits())) {
                //we can not handle overlapping alignments - at this point it is input error
                CNcbiOstrstream ost;
                ost << "MaxCDS of alignment " << algn.ID() << " intersects MaxCDS of alignment " << algn_prev.ID();
                NCBI_THROW(CGnomonException, eGenericError, CNcbiOstrstreamToString(ost));
            }
        }
        it_prev = it;

        int strand = algn.Strand();
        int otherstrand = (strand == ePlus) ? eMinus : ePlus;
                

        for(unsigned int k = 1; k < algn.Exons().size(); ++k)  // accept NONCONSENSUS alignment splices
        {
            if(algn.Exons()[k-1].m_ssplice)
            {
                int i = algn.Exons()[k-1].GetTo();
                if(strand == ePlus) m_dscr[strand][i] = 0;  // donor score on the last base of exon
                else m_ascr[strand][i] = 0;                 // acceptor score on the last base of exon
            }
            if(algn.Exons()[k].m_fsplice)
            {
                int i = algn.Exons()[k].GetFrom();
                if(strand == ePlus) m_ascr[strand][i-1] = 0; // acceptor score on the last base of intron
                else m_dscr[strand][i-1] = 0;                // donor score on the last base of intron 
            }
        }
            
        for(unsigned int k = 1; k < algn.Exons().size(); ++k) // enforsing introns
        {
            int introna = algn.Exons()[k-1].GetTo()+1;
            int intronb = algn.Exons()[k].GetFrom()-1;
            
            if(algn.Exons()[k-1].m_ssplice)
            {
                int pnt = introna;
                m_notinexon[strand][0][pnt] = pnt;
                m_notinexon[strand][1][pnt] = pnt;
                m_notinexon[strand][2][pnt] = pnt;
            }
            
            if(algn.Exons()[k].m_fsplice)
            {
                int pnt = intronb;
                m_notinexon[strand][0][pnt] = pnt;
                m_notinexon[strand][1][pnt] = pnt;
                m_notinexon[strand][2][pnt] = pnt;
            }
                    
            if(algn.Exons()[k-1].m_ssplice && algn.Exons()[k].m_fsplice)
            {            
                for(int pnt = introna; pnt <= intronb; ++pnt)
                {
                    m_notinexon[strand][0][pnt] = pnt;
                    m_notinexon[strand][1][pnt] = pnt;
                    m_notinexon[strand][2][pnt] = pnt;
                }
            }
        }

        for(unsigned int k = 0; k < algn.Exons().size(); ++k)  // enforcing exons
        {
            int exona = algn.Exons()[k].GetFrom();
            int exonb = algn.Exons()[k].GetTo();
            for(int pnt = exona; pnt <= exonb; ++pnt)
            {
                m_notinintron[strand][pnt] = pnt;
            }
        }
        
        TSignedSeqPos a = algn.Limits().GetFrom();
        TSignedSeqPos b = algn.Limits().GetTo();
        
        for(TSignedSeqPos i = a; i <= b && i < len-1; ++i)  // first and last points are conserved to deal with intergenics going outside
        {
            m_inalign[i] = max(TSignedSeqPos(1),a);         // one CDS maximum per alignment
            m_notinintron[otherstrand][i] = i;              // no other strand models
            m_notinexon[otherstrand][0][i] = i;
            m_notinexon[otherstrand][1][i] = i;
            m_notinexon[otherstrand][2][i] = i;
        }

        if(strand == ePlus) a += 3;    // start is a part of intergenic!!!!
        else b -= 3;
            
        m_notining[b] = a;                                  // at least one CDS per alignment
        if(algn.ReadingFrame().NotEmpty()) {
            for(TSignedSeqPos i = algn.ReadingFrame().GetFrom(); i <= algn.ReadingFrame().GetTo(); ++i)
                m_notining[i] = i;
        }
    }

    
    if(leftwall) m_notinintron[ePlus][0] = m_notinintron[eMinus][0] = 0;                  // no partials
    if(rightwall) m_notinintron[ePlus][len-1] = m_notinintron[eMinus][len-1] = len-1;

    for(int strand = 0; strand < 2; ++strand)
    {
        TIVec& nin = m_notinintron[strand];
        for(TSignedSeqPos i = 1; i < len; ++i) nin[i] = max(nin[i-1],nin[i]);

        for(int frame = 0; frame < 3; ++frame)
        {
            TIVec& nex = m_notinexon[strand][frame];
            for(TSignedSeqPos i = 1; i < len; ++i) nex[i] = max(nex[i-1],nex[i]); 
        }
    }
    for(TSignedSeqPos i = 1; i < len; ++i) m_notining[i] = max(m_notining[i-1],m_notining[i]);

    for(int strand = 0; strand < 2; ++strand)
    {
        CEResidueVec& s = m_seq[strand];
        
        m_anum[strand] = 0;
        m_dnum[strand] = 0;
        m_sttnum[strand] = 0;
        m_stpnum[strand] = 0;

        if(strand == ePlus)
        {
            for(TSignedSeqPos i = 0; i < len; ++i)
            {
                m_ascr[strand][i] = max(m_ascr[strand][i],m_acceptor.Score(s,i));
                m_dscr[strand][i] = max(m_dscr[strand][i],m_donor.Score(s,i));
                /*
                if(m_ascr[strand][i] != BadScore())
                    cout << "Acceptor\t" << i+1+chunk.GetFrom() << "\t" << m_ascr[strand][i] << "\t+" << endl;
                if(m_dscr[strand][i] != BadScore())
                    cout << "Donor\t" << i+2+chunk.GetFrom() << "\t" << m_dscr[strand][i] << "\t+" << endl;
                */
                m_sttscr[strand][i] = max(m_sttscr[strand][i],m_start.Score(s,i));
                m_stpscr[strand][i] = max(m_stpscr[strand][i],m_stop.Score(s,i));
                if(m_ascr[strand][i] != BadScore()) ++m_anum[strand];
                if(m_dscr[strand][i] != BadScore()) ++m_dnum[strand];
                if(m_sttscr[strand][i] != BadScore()) ++m_sttnum[strand];
            }
        }
        else
        {
            for(TSignedSeqPos i = 0; i < len; ++i)
            {
                int ii = len-2-i;   // extra -1 because ii is point on the "right"
                m_ascr[strand][i] = max(m_ascr[strand][i],m_acceptor.Score(s,ii));
                m_dscr[strand][i] = max(m_dscr[strand][i],m_donor.Score(s,ii));
                /*
                if(m_ascr[strand][i] != BadScore())
                    cout << "Acceptor\t" << i+2+chunk.GetFrom() << "\t" << m_ascr[strand][i] << "\t-" << endl;
                if(m_dscr[strand][i] != BadScore())
                    cout << "Donor\t" << i+1+chunk.GetFrom() << "\t" << m_dscr[strand][i] << "\t-" << endl;
                */
                m_sttscr[strand][i] = max(m_sttscr[strand][i],m_start.Score(s,ii));
                m_stpscr[strand][i] = max(m_stpscr[strand][i],m_stop.Score(s,ii));
                if(m_ascr[strand][i] != BadScore()) ++m_anum[strand];
                if(m_dscr[strand][i] != BadScore()) ++m_dnum[strand];
            }
        }
    }		

    
    for(TAlignSet::iterator it = allaligns.begin(); it != allaligns.end(); ++it)
    {
        const CGeneModel& algn(*it);
        int strand = algn.Strand();
        TSignedSeqRange cds_lim = algn.ReadingFrame();
        if(cds_lim.Empty()) continue;
        
        if(strand == ePlus)
        {
            for(unsigned int k = 0; k < algn.Exons().size(); ++k) {   // ignoring STOPS in CDS
                TSignedSeqPos a = max(algn.Exons()[k].GetFrom(),cds_lim.GetFrom());
                if(a > 0) --a;
                TSignedSeqPos b = min(algn.Exons()[k].GetTo(),cds_lim.GetTo())-1;   // -1 don't touch the real stop
                for(TSignedSeqPos i = a; i <= b; ++i)
                    m_stpscr[strand][i] = BadScore();  // score on last coding base
            }
        }
        else
        {
            for(unsigned int k = 0; k < algn.Exons().size(); ++k) {   // ignoring STOPS in CDS
                TSignedSeqPos a = max(algn.Exons()[k].GetFrom(),cds_lim.GetFrom());
                TSignedSeqPos b = min(algn.Exons()[k].GetTo(),cds_lim.GetTo());
                for(TSignedSeqPos i = a; i <= b; ++i)
                    m_stpscr[strand][i] = BadScore();  // score on last intergenic (first stop) base
            }
        }
    }

    for(int strand = 0; strand < 2; ++strand)
    {
        for(TSignedSeqPos i = 0; i < len; ++i)
        {
            if(m_sttscr[strand][i] != BadScore()) ++m_sttnum[strand];
            
            if(m_stpscr[strand][i] != BadScore())
            {
                if(strand == ePlus)
                {
                    int& lstp = m_laststop[strand][2-i%3][i+3];
                    lstp = max(int(i)+1,lstp);
                    ++m_stpnum[strand];
                }
                else
                {
                    int& lstp = m_laststop[strand][i%3][i];
                    lstp = max(int(i)-2,lstp);
                    ++m_stpnum[strand];
                }
            }
        }
    }


    for(int strand = 0; strand < 2; ++strand)
    {
        CEResidueVec& s = m_seq[strand];
        
        for(TSignedSeqPos i = 0; i < len; ++i)
        {
            TSignedSeqPos ii = strand == ePlus ? i : len-1-i;
            
            double score = m_ncdr.Score(s,ii);
            if(score == BadScore()) score = 0;
            m_ncdrscr[strand][i] = score;
            if(i > 0) m_ncdrscr[strand][i] += m_ncdrscr[strand][i-1];

            score = m_intrg.Score(s,ii);
            if(score == BadScore()) score = 0;
            m_ingscr[strand][i] = score;
            if(i > 0) m_ingscr[strand][i] += m_ingscr[strand][i-1];
        }

        for(int frame = 0; frame < 3; ++frame)
        {
            for(TSignedSeqPos i = 0; i < len; ++i)
            {
                int codonshift,ii;
                if(strand == ePlus)     // left end of codon is shifted by frame bases to left
                {
                    codonshift = (frame+i)%3;
                    ii = i;
                }
                else                  // right end of codone is shifted by frame bases to right
                {
                    codonshift = (frame-(int)i)%3;
                    if(codonshift < 0) codonshift += 3;
                    ii = len-1-i;
                }

                double score = m_cdr.Score(s,ii,codonshift);
                if(score == BadScore()) score = 0;

                m_cdrscr[strand][frame][i] = score;
                if(i > 0) 
                {
                    m_cdrscr[strand][frame][i] += m_cdrscr[strand][frame][i-1];
                    
                    TIVec& lstp = m_laststop[strand][frame];
                    lstp[i] = max(lstp[i-1],lstp[i]);
                }
            }
        }
    }

    for(int strand = 0; strand < 2; ++strand)
    {
        for(int frame = 0; frame < 3; ++frame)
        {
            for(TSignedSeqPos i = 0; i < len; ++i)
            {
                m_cdrscr[strand][frame][i] -= m_ncdrscr[strand][i];
            }
        }
        for(TSignedSeqPos i = 0; i < len; ++i)
        {
            m_ingscr[strand][i] -= m_ncdrscr[strand][i];

            int left, right;
            if(m_dscr[strand][i] != BadScore())
            {
                const CTerminal& t = m_donor;
                left = i+1-(strand==ePlus ? t.Left():t.Right());
                right = i+(strand==ePlus ? t.Right():t.Left());
                left = max(0,left);
                right = min(len-1,right);
                m_dscr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(m_ascr[strand][i] != BadScore())
            {
                const CTerminal& t = m_acceptor;
                left = i+1-(strand==ePlus ? t.Left():t.Right());
                right = i+(strand==ePlus ? t.Right():t.Left());
                left = max(0,left);
                right = min(len-1,right);
                m_ascr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(m_sttscr[strand][i] != BadScore())
            {
                const CTerminal& t = m_start;
                left = i+1-(strand==ePlus ? t.Left():t.Right());
                right = i+(strand==ePlus ? t.Right():t.Left());
                left = max(0,left);
                right = min(len-1,right);
                m_sttscr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(m_stpscr[strand][i] != BadScore())
            {
                const CTerminal& t = m_stop;
                left = i+1-(strand==ePlus ? t.Left():t.Right());
                right = i+(strand==ePlus ? t.Right():t.Left());
                left = max(0,left);
                right = min(len-1,right);
                m_stpscr[strand][i] -= NonCodingScore(left,right,strand);
            }
        }
    }

    ITERATE(TAlignSet, it, allaligns) {
        const CGeneModel& algn(*it);
        if(algn.ReadingFrame().Empty())
            continue;
        int strand = algn.Strand();

        if(algn.Exons().front().m_fsplice_sig == "XX" || algn.Exons().front().m_ssplice_sig == "XX") {
            int p = algn.ReadingFrame().GetFrom()-1;
            if(strand == ePlus &&  !algn.HasStart()) {
                m_sttscr[strand][p] = consensuspenalty;
                ++m_sttnum[strand];
            } else if(strand == eMinus && !algn.HasStop()) {
                m_stpscr[strand][p] = consensuspenalty;
                ++m_stpnum[strand];
            }
        }

        if(algn.Exons().back().m_fsplice_sig == "XX" || algn.Exons().back().m_ssplice_sig == "XX") {
            int p = algn.ReadingFrame().GetTo();
            if(strand == ePlus && !algn.HasStop()) {
                m_stpscr[strand][p] = consensuspenalty;
                ++m_stpnum[strand];
            } else if(strand == eMinus && !algn.HasStart()) {
                m_sttscr[strand][p] = consensuspenalty;
                ++m_sttnum[strand];
            }
        }
    }
    
    const int NonConsensusMargin = 50;
    if (consensuspenalty != BadScore())
    ITERATE(TAlignSet, it, allaligns) {
        const CGeneModel& algn(*it);
        if((algn.Type() & CGeneModel::eProt) == 0) continue;
        
        int strand = algn.Strand();
        TSignedSeqRange lim = algn.Limits();
        
        for(unsigned int k = 1; k < algn.Exons().size(); ++k)
        {
            if(algn.Exons()[k-1].m_ssplice && algn.Exons()[k].m_fsplice) continue;
            
            int a = algn.Exons()[k-1].GetTo();
            int b = algn.Exons()[k].GetFrom();
            for(TSignedSeqPos i = a; i <= b; ++i)
            {
                if(i <= a+NonConsensusMargin || i >= b-NonConsensusMargin)
                {
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
        }
        
        if(strand == ePlus)
        {
            if(!algn.HasStart() && algn.Exons().front().m_ssplice_sig != "XX")
            {
                int a = max(2,lim.GetFrom()-NonConsensusMargin);
                int b = lim.GetFrom()-1;
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(m_sttscr[strand][i] == BadScore())
                    {
                        m_sttscr[strand][i] = consensuspenalty;
                        ++m_sttnum[strand];
                    }
                    
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
            
            if(!algn.HasStop() && algn.Exons().back().m_fsplice_sig != "XX")
            {
                int a = lim.GetTo();
                int b = min(len-4,lim.GetTo()+NonConsensusMargin);
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(m_stpscr[strand][i] == BadScore())
                    {
                        m_stpscr[strand][i] = consensuspenalty;
                        ++m_stpnum[strand];
                    }
                    
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
        }
        else
        {
            if(!algn.HasStart() && algn.Exons().back().m_fsplice_sig != "XX")
            {
                int a = lim.GetTo();
                int b = min(len-4,lim.GetTo()+NonConsensusMargin);
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(m_sttscr[strand][i] == BadScore())
                    {
                        m_sttscr[strand][i] = consensuspenalty;
                        ++m_sttnum[strand];
                    }
                    
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
            
            if(!algn.HasStop() && algn.Exons().front().m_ssplice_sig != "XX")
            {
                int a = max(2,lim.GetFrom()-NonConsensusMargin);
                int b = lim.GetFrom()-1;
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(m_stpscr[strand][i] == BadScore())
                    {
                        m_stpscr[strand][i] = consensuspenalty;
                        ++m_stpnum[strand];
                    }
                    
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
        }
    }
    
    const int stpT = 1, stpTA = 2, stpTG = 4;

    for(int strand = 0; strand < 2; ++strand)
    {
        CEResidueVec& s = m_seq[strand];
        
        if(strand == ePlus) {
            for(TSignedSeqPos i = 0; i < len; ++i) {
                if(m_ascr[strand][i] != BadScore()) {
                    if(s[i+1] == enA && s[i+2] == enA) m_asplit[strand][0][i] |= stpT;
                    if(s[i+1] == enA && s[i+2] == enG) m_asplit[strand][0][i] |= stpT; 
                    if(s[i+1] == enG && s[i+2] == enA) m_asplit[strand][0][i] |= stpT;
                    if(s[i+1] == enA) m_asplit[strand][1][i] |= stpTA|stpTG;
                    if(s[i+1] == enG) m_asplit[strand][1][i] |= stpTA;
                }
                if(m_dscr[strand][i] != BadScore()) {
                    if(s[i] == enT) m_dsplit[strand][0][i] |= stpT;
                    if(s[i-1] == enT && s[i] == enA) m_dsplit[strand][1][i] |= stpTA;
                    if(s[i-1] == enT && s[i] == enG) m_dsplit[strand][1][i] |= stpTG;
                }
            }
        } else {
            for(TSignedSeqPos i = 0; i < len; ++i) {
                int ii = len-2-i;   // extra -1 because ii is point on the "right"
                if(m_ascr[strand][i] != BadScore()) {
                    if(s[ii+1] == enA && s[ii+2] == enA) m_asplit[strand][0][i] |= stpT;
                    if(s[ii+1] == enA && s[ii+2] == enG) m_asplit[strand][0][i] |= stpT; 
                    if(s[ii+1] == enG && s[ii+2] == enA) m_asplit[strand][0][i] |= stpT;
                    if(s[ii+1] == enA) m_asplit[strand][1][i] |= stpTA|stpTG;
                    if(s[ii+1] == enG) m_asplit[strand][1][i] |= stpTA;
                }
                if(m_dscr[strand][i] != BadScore()) {
                    if(s[ii] == enT) m_dsplit[strand][0][i] |= stpT;
                    if(s[ii-1] == enT && s[ii] == enA) m_dsplit[strand][1][i] |= stpTA;
                    if(s[ii-1] == enT && s[ii] == enG) m_dsplit[strand][1][i] |= stpTG;
                }
            }
        }
    }		
    
    for(TAlignSet::iterator it = allaligns.begin(); it != allaligns.end(); ++it)
    {
        const CGeneModel& algn(*it);
        int strand = algn.Strand();
        TSignedSeqRange cds_lim = algn.ReadingFrame();
        if(cds_lim.Empty()) continue;
        
        if(strand == ePlus)
        {
            for(unsigned int k = 0; k < algn.Exons().size()-1; ++k)   // ignoring splitted STOPS in CDS
            {
                TSignedSeqPos b = algn.Exons()[k].GetTo();
                if(algn.Exons()[k].m_ssplice && algn.Exons()[k+1].m_fsplice && b >= cds_lim.GetFrom() && b <= cds_lim.GetTo())
                {
                    m_dsplit[strand][0][b] = 0;
                    m_dsplit[strand][1][b] = 0;
                }
            }
        }
        else
        {
            for(unsigned int k = 1; k < algn.Exons().size(); ++k)   // ignoring splitted STOPS in CDS
            {
                TSignedSeqPos a = algn.Exons()[k].GetFrom();
                if(a > 0 && algn.Exons()[k-1].m_ssplice && algn.Exons()[k].m_fsplice && a >= cds_lim.GetFrom() && a <= cds_lim.GetTo())
                {
                    m_dsplit[strand][0][a-1] = 0;
                    m_dsplit[strand][1][a-1] = 0;
                }
            }
        }
    }
}

double CGnomonEngine::SelectBestReadingFrame(const CGeneModel& model, const CEResidueVec& mrna, const CAlignMap& mrnamap, TIVec starts[3],  TIVec stops[3], int& best_frame, int& best_start, int& best_stop, bool extend5p) const
{
    //    const CTerminal& acceptor    = *m_data->m_acceptor;
    //    const CTerminal& donor       = *m_data->m_donor;
    const CTerminal& stt         = *m_data->m_start;
    const CTerminal& stp         = *m_data->m_stop;
    const CCodingRegion& cdr     = *m_data->m_cdr;
    const CNonCodingRegion& ncdr = *m_data->m_ncdr;
    int contig_len = m_data->m_seq.size();
    EStrand strand = model.Strand();
    //    const vector<CModelExon>& exons = model.Exons();
    //    int num_exons = model.Exons().size();

    const CDoubleStrandSeq& ds = m_data->m_ds;
    TDVec splicescr(mrna.size(),0);

    /*
    if(strand == ePlus) {
        int shift = -1;
        for(int i = 1; i < num_exons; ++i) {
            shift += mrnamap.FShiftedLen(exons[i-1].GetFrom(),exons[i-1].GetTo());
            
            if(exons[i-1].m_ssplice && exons[i-1].m_ssplice_sig != "XX") {
                int l = exons[i-1].GetTo();
                double scr = donor.Score(ds[ePlus],l);
                if(scr == BadScore()) {
                    scr = 0;
                } else {
                    for(int k = l-donor.Left()+1; k <= l+donor.Right(); ++k) {
                        double s = ncdr.Score(ds[ePlus],k);
                        if(s == BadScore()) s = 0;
                        scr -= s;
                    }
                }
                splicescr[shift] = scr;
            }

            if(exons[i].m_fsplice && exons[i].m_fsplice_sig != "XX") {
                int l = exons[i].GetFrom()-1;
                double scr = acceptor.Score(ds[ePlus],l);
                if(scr == BadScore()) {
                    scr = 0;
                } else {
                    for(int k = l-acceptor.Left()+1; k <= l+acceptor.Right(); ++k) {
                        double s = ncdr.Score(ds[ePlus],k);
                        if(s == BadScore()) s = 0;
                        scr -= s;
                    }
                }
                splicescr[shift] += scr;
            }
        }
    } else {
        int shift = mrna.size()-1;
        for(int i = 1; i < num_exons; ++i) {
            shift -= mrnamap.FShiftedLen(exons[i-1].GetFrom(),exons[i-1].GetTo());
            
            if(exons[i-1].m_ssplice && exons[i-1].m_ssplice_sig != "XX") {
                int l = contig_len-2-exons[i-1].GetTo();
                double scr = acceptor.Score(ds[eMinus],l);
                if(scr == BadScore()) {
                    scr = 0;
                } else {
                    for(int k = l-acceptor.Left()+1; k <= l+acceptor.Right(); ++k) {
                        double s = ncdr.Score(ds[eMinus],k);
                        if(s == BadScore()) s = 0;
                        scr -= s;
                    }
                }
                splicescr[shift] = scr;
            }

            if(exons[i].m_fsplice && exons[i].m_fsplice_sig != "XX") {
                int l = contig_len-1-exons[i].GetFrom();
                double scr = donor.Score(ds[eMinus],l);
                if(scr == BadScore()) {
                    scr = 0;
                } else {
                    for(int k = l-donor.Left()+1; k <= l+donor.Right(); ++k) {
                        double s = ncdr.Score(ds[eMinus],k);
                        if(s == BadScore()) s = 0;
                        scr -= s;
                    }
                }
                splicescr[shift] += scr;
            }
        }
    }
    */


    TDVec cdrscr[3];
    for(int frame = 0; frame < 3; ++frame) {
        if (frame==best_frame || best_frame==-1) {
            cdrscr[frame].resize(mrna.size(),BadScore());
            for(int i = 0; i < (int)mrna.size(); ++i) {
                int codonshift = (i-frame)%3;
                if(codonshift < 0)
                    codonshift += 3;
                
                double scr = cdr.Score(mrna,i,codonshift);
                if(scr == BadScore())  {
                    scr = 0;
                } else {
                    double s = ncdr.Score(mrna,i);
                    if(s == BadScore()) s = 0;
                    scr -= s;
                }

                cdrscr[frame][i] = scr+splicescr[i];
                if(i > 0)
                    cdrscr[frame][i] += cdrscr[frame][i-1];
            }
        }
    }

    double best_score  = BadScore();
    best_start = -1;
    best_stop = -1;
    CCDSInfo cds_info = model.GetCdsInfo();

    int best_frame_initial = best_frame;

    for(int frame = 0; frame < 3; ++frame) {
        if (frame==best_frame_initial || best_frame_initial==-1) {
            for(int i = 0; i < (int)stops[frame].size(); i++) {
                int stop = stops[frame][i];
                if(stop < 0)        // bogus stop
                    continue;

                int prev_stop = -6;
                if(i > 0)
                    prev_stop = stops[frame][i-1];
                TIVec::iterator it_a = lower_bound(starts[frame].begin(),starts[frame].end(),prev_stop+3);
                if(it_a == starts[frame].end() || *it_a >= stop)    // no start 
                    continue;
                TIVec::iterator it_b = it_a+1;
                if(*it_a < 0 && it_b != starts[frame].end() && *it_b < stop)   // open rf and there is apropriate start at right    
                    ++it_b;
                if(!extend5p) {
                    for( ; it_b != starts[frame].end() && *it_b < stop; ++it_b);
                }

                for(TIVec::iterator it = it_a; it != it_b; it++) {             // if extend5p==true this loop includes only open rf (if exists) and one real start
                                                                               // otherwise the best start is selected    
                    int start = *it;
                    
                    if (stop-start-(start>=0?0:3) < 30)
                        continue;
            
                    double s = cdrscr[frame][stop-1]-cdrscr[frame][start+2+(start>=0?0:3)];
                    
                    double stt_score = BadScore();
                    if(start >= stt.Left()+2) {    // 5 extra bases for ncdr    
                        int pnt = start+2;
                        stt_score = stt.Score(mrna,pnt);
                        if(stt_score != BadScore()) {
                            for(int k = pnt-stt.Left()+1; k <= pnt+stt.Right(); ++k) {
                                double sn = ncdr.Score(mrna,k);
                                if(sn != BadScore())
                                    stt_score -= sn;
                            }
                        }
                    } else {
                        int pnt = mrnamap.MapEditedToOrig(start+(start>=0?0:3));
                        if(strand == eMinus) pnt = contig_len-1-pnt;
                        pnt += 2;
                        stt_score = stt.Score(ds[strand],pnt);
                        if(stt_score != BadScore()) {
                            for(int k = pnt-stt.Left()+1; k <= pnt+stt.Right(); ++k) {
                                double sn = ncdr.Score(ds[strand],k);
                                if(sn != BadScore())
                                    stt_score -= sn;
                            }
                        }
                    }
                    if(stt_score != BadScore())
                        s += stt_score;
            
                    double stp_score = stp.Score(mrna,stop-1);
                    if(stp_score != BadScore()) {
                        for(int k = stop-stp.Left(); k < stop+stp.Right(); ++k) {
                            double sn = ncdr.Score(mrna,k);
                            if(sn != BadScore())
                                stp_score -= sn;
                        }
                        s += stp_score;
                    }
            
                    if(s >= best_score) {
                        best_frame = frame;
                        best_score = s;
                        best_start = start;
                        best_stop = stop;
                    }
                }
            }
        }
    }

    if(cds_info.ConfirmedStart() && cds_info.ConfirmedStop() && model.Continuous()) {  // looks like a complete model, will get a permanent boost in score which will improve chanses for the first placement over notcomplete models
        best_score += max(1.,0.3*best_score);        // in case s negative
    }

    return best_score;
}

void CGnomonEngine::GetScore(CGeneModel& model, bool extend5p, bool obeystart) const
{
    CAlignMap mrnamap(model.Exons(),model.FrameShifts(),model.Strand());
    CEResidueVec mrna;
    mrnamap.EditedSequence(m_data->m_ds[ePlus], mrna);

    int frame = -1;
    TIVec starts[3], stops[3];

    FindStartsStops(model, m_data->m_ds[model.Strand()], mrna, mrnamap, starts, stops, frame, obeystart);

    if(model.GetCdsInfo().ReadingFrame().Empty()) {   // we didn't know the frame before
        int mrna_len = (int)mrna.size();

        for(int fr = 0; fr < 3; ++fr) {

            int first_stop = stops[fr][0];
            //            if(first_stop < 0)  // bogus stop for maxcds
            //                first_stop = stops[fr][1];

            if(first_stop >= 3) {   // we have 5' reading frame
                TSignedSeqRange rframe, stt, stp;                

                if(first_stop < mrna_len-2) {
                    stp = mrnamap.MapRangeEditedToOrig(TSignedSeqRange(first_stop,first_stop+2), false);
                }

                int first_start = -1;
                if(!starts[fr].empty()) {
                    first_start = starts[fr][0];
                    if(first_start < 0 && starts[fr].size() > 1)
                        first_start = starts[fr][1];
                }
                int fivep_rf = fr;
                if(first_start >= 0 && first_start <= first_stop-6) {
                    stt = mrnamap.MapRangeEditedToOrig(TSignedSeqRange(first_start,first_start+2), false);
                    fivep_rf = first_start+3;
                }

                if(stt.NotEmpty() || stops[fr][0] >= 0) {    // there is a start or no upstream stop
                    int threep_rf = first_stop-1;
                    rframe = mrnamap.MapRangeEditedToOrig(TSignedSeqRange(fivep_rf,threep_rf), true);

                    _ASSERT(rframe.NotEmpty());

                    CCDSInfo ci;
                    ci.SetReadingFrame(rframe);
                    if(stt.NotEmpty()) {
                        ci.SetStart(stt);
                        if(stops[fr][0] < 0) {
                            int bs  = mrnamap.MapEditedToOrig(first_start);
                            _ASSERT( bs >= 0 );
                            ci.Set5PrimeCdsLimit(bs);
                        }
                    }
                    if(stp.NotEmpty())
                        ci.SetStop(stp);
                    ci.SetScore(0, stt.NotEmpty());
                    model.SetEdgeReadingFrames()->push_back(ci);
                }
            }

            if(first_stop < mrna_len-2) {   //there is a stop and possibly 3' reading frame
                TSignedSeqRange rframe, stt;                
                int last_stop = stops[fr][(int)stops[fr].size()-1];
                if(last_stop >= mrna_len-2)
                    last_stop = stops[fr][(int)stops[fr].size()-2]; //garanteed to be present
                int start;
                ITERATE(TIVec, it, starts[fr]) {
                    start = *it;
                    if(start > last_stop && start <= mrna_len-6) {
                        stt = mrnamap.MapRangeEditedToOrig(TSignedSeqRange(start,start+2), false);
                        break;
                    }
                }
                if(stt.NotEmpty()) {    // there is 3' reading frame
                    int fivep_rf = start+3;
                    int threep_rf = mrna_len-(mrna_len-fr)%3-1;
                    rframe = mrnamap.MapRangeEditedToOrig(TSignedSeqRange(fivep_rf,threep_rf), true);

                    _ASSERT(rframe.NotEmpty());

                    CCDSInfo ci;
                    ci.SetReadingFrame(rframe);
                    ci.SetStart(stt);
                    ci.Set5PrimeCdsLimit(model.Strand() == ePlus ? stt.GetFrom():stt.GetTo());
                    ci.SetScore(0,false);
                    model.SetEdgeReadingFrames()->push_back(ci);
                }
            }
        }
    }

    int best_start, best_stop;
    double best_score = SelectBestReadingFrame(model, mrna, mrnamap, starts, stops, frame, best_start, best_stop, extend5p);

    CCDSInfo cds_info = model.GetCdsInfo();
    if(cds_info.MaxCdsLimits().NotEmpty())
        cds_info.Clear5PrimeCdsLimit();

    if (best_score == BadScore()) {
        cds_info.Clear();
        model.SetCdsInfo( cds_info );
        return;
    }

    bool is_open = false;
    if(best_start == 0) {
        is_open = !cds_info.ConfirmedStart();
    } else if(best_start<0 && starts[frame].size() > 1) {
        int new_start = starts[frame][1];
        int newlen = best_stop-new_start;
        int oldlen = best_stop-best_start;
        if(newlen >= max(6,oldlen/2) || cds_info.ConfirmedStart()) {
            is_open = !cds_info.ConfirmedStart();
            best_start = new_start;
        }
    }

    if (cds_info.ConfirmedStart() && best_start != starts[frame].back()) {
        model.AddComment("movedconfstart");
    }
                
    bool has_start = best_start>=0;
    bool confirmed_start = cds_info.ConfirmedStart();   //we wamnt to keep the status even if the actual start moved within alignment, the status will be used in gnomon and will prevent any furher extension
    bool confirmed_stop = cds_info.ConfirmedStop();

    TSignedSeqRange best_reading_frame = mrnamap.MapRangeEditedToOrig(TSignedSeqRange(best_start+3,best_stop-1), true);
    if (Include(best_reading_frame, cds_info.Start()))
        cds_info.SetStart(TSignedSeqRange::GetEmpty());

    cds_info.ClearPStops();

    if (cds_info.ProtReadingFrame().NotEmpty() && !Include(best_reading_frame, cds_info.ProtReadingFrame())) {
        _ASSERT( best_start==0 );
        cds_info.SetReadingFrame(best_reading_frame & cds_info.ProtReadingFrame(), true);
    }
    cds_info.SetReadingFrame(best_reading_frame);

    //    cds_info.Clear5PrimeCdsLimit();
    if(has_start) {
        int upstream_stop = -3;
        bool found_upstream_stop = FindUpstreamStop(stops[frame],best_start,upstream_stop);
        
        cds_info.SetStart(mrnamap.MapRangeEditedToOrig(TSignedSeqRange(best_start,best_start+2), false), confirmed_start);

        if(found_upstream_stop) {
            int bs  = mrnamap.MapEditedToOrig(best_start);
            _ASSERT( bs >= 0 );
            cds_info.Set5PrimeCdsLimit(bs);
        } else {
#ifdef _DEBUG
            for(int i = best_start; i >=0; i -= 3) {
                _ASSERT(!IsStopCodon(&mrna[i]));
            }
#endif
        }
    }

    if ((int)mrna.size() - best_stop >=3)
        cds_info.SetStop(mrnamap.MapRangeEditedToOrig(TSignedSeqRange(best_stop,best_stop+2), false),confirmed_stop);
    
    for(int i = best_start+3; i < best_stop; i += 3) {
        if(IsStopCodon(&mrna[i])) {
            TSignedSeqRange pstop = mrnamap.MapRangeEditedToOrig(TSignedSeqRange(i,i+2), false);
            cds_info.AddPStop(pstop);
        }
    }

    cds_info.SetScore(best_score, is_open);
    model.SetCdsInfo( cds_info );
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
