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

#include <ncbi_pch.hpp>
#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/gnomon.hpp>
#include "gnomon_seq.hpp"
#include <algo/gnomon/id_handler.hpp>
#include <set>
#include <functional>
#include <corelib/ncbiutil.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

USING_SCOPE(objects);

TInDels CGeneModel::GetInDels(TSignedSeqPos a, TSignedSeqPos b, bool fs_only) const {
    
    TInDels indels = GetInDels(fs_only);
    TInDels selected_indels;

    ITERATE(TInDels, i, indels) {
        if(i->IntersectingWith(a,b))
            selected_indels.push_back( *i );
    }
        
    return selected_indels;
}

TInDels CGeneModel::GetInDels(bool fs_only) const {

    TInDels selected_indels;
    if(!fs_only) {
        ITERATE(TInDels, i, FrameShifts()) {
            if(!i->IsMismatch())
                selected_indels.push_back(*i); 
        }
    } else {
        TExons::const_iterator e = Exons().begin();
        ITERATE(TInDels, i, FrameShifts()) {
            if(i->IsMismatch() || i->Len()%3 == 0)  // skip mismatches and full codons
                continue;

            for( ;e != Exons().end() && (e->Limits().Empty() || !i->IntersectingWith(e->GetFrom(),e->GetTo())); ++e); // harboring exon
            if(i->InDelEnd() > e->GetTo() && e != Exons().end()) {                                                   // skip full codons splitted by intron
                TInDels::const_iterator next = i;
                if(++next != FrameShifts().end() && (++e)->Limits().NotEmpty() && next->Loc() == e->GetFrom() && !next->IsMismatch()) {
                    int len = i->Len() + (i->GetType() == next->GetType() ? next->Len() : -next->Len());
                    if(len%3 == 0) {
                        i = next;
                        continue;
                    }
                }
            }

            selected_indels.push_back(*i);
        }
    }

    return selected_indels;
}


TSignedSeqRange CGeneModel::TranscriptExon(int i) const {
    CAlignMap amap = GetAlignMap();

    if(Exons()[i].Limits().NotEmpty()) {
        return amap.MapRangeOrigToEdited(Exons()[i].Limits(), true); 
    } else if(i > 0) {  // there is real exon on the left
        int p = amap.MapOrigToEdited(Exons()[i-1].GetTo());
        if(Orientation() == ePlus)
            return TSignedSeqRange(p+1,p+Exons()[i].m_seq.size());
        else
            return TSignedSeqRange(p-Exons()[i].m_seq.size(),p-1);
    } else {  // there is a real exon on the right
        _ASSERT(i < (int)Exons().size()-1);
        int p = amap.MapOrigToEdited(Exons()[i+1].GetFrom());
        if(Orientation() == ePlus)
            return TSignedSeqRange(p-Exons()[i].m_seq.size(),p-1);
        else
            return TSignedSeqRange(p+1,p+Exons()[i].m_seq.size());        
    }
}

TSignedSeqRange CGeneModel::TranscriptLimits() const {
    CAlignMap amap = GetAlignMap();

    int l,r;

    if(Orientation() == ePlus) {
        if(Exons().front().Limits().NotEmpty()) {
            l = amap.MapRangeOrigToEdited(Exons().front().Limits(),true).GetFrom();
        } else {
            _ASSERT(Exons().size() > 1 && Exons()[1].Limits().NotEmpty());
            l = amap.MapOrigToEdited(Exons()[1].GetFrom())-Exons().front().m_seq.length();
        }
        if(Exons().back().Limits().NotEmpty()) {
            r = amap.MapRangeOrigToEdited(Exons().back().Limits(),true).GetTo();
        } else {
            _ASSERT(Exons().size() > 1 && Exons()[Exons().size()-2].Limits().NotEmpty());
            r = amap.MapOrigToEdited(Exons()[Exons().size()-2].GetTo()) + Exons().back().m_seq.length();
        }
    } else {
        if(Exons().front().Limits().NotEmpty()) {
            r = amap.MapRangeOrigToEdited(Exons().front().Limits(),true).GetTo();
        } else {
            _ASSERT(Exons().size() > 1 && Exons()[1].Limits().NotEmpty());
            r = amap.MapOrigToEdited(Exons()[1].GetFrom())+Exons().front().m_seq.length();
        }
        if(Exons().back().Limits().NotEmpty()) {
            l = amap.MapRangeOrigToEdited(Exons().back().Limits(),true).GetFrom();
        } else {
            _ASSERT(Exons().size() > 1 && Exons()[Exons().size()-2].Limits().NotEmpty());
            l = amap.MapOrigToEdited(Exons()[Exons().size()-2].GetTo()) - Exons().back().m_seq.length();
        }
    }
        
    return TSignedSeqRange(l,r);
    }

bool CGeneModel::isNMD(int limit) const
{
    if (GetCdsInfo().ReadingFrame().Empty() || Exons().size() <= 1)
        return false;

    TSignedSeqRange cds = GetCdsInfo().Start()+GetCdsInfo().ReadingFrame()+GetCdsInfo().Stop();
    CAlignMap amap = GetAlignMap();
    if(GetCdsInfo().IsMappedToGenome()) {
        cds = amap.MapRangeOrigToEdited(cds);
    }
        
    int l = -1;
    if (Strand() == ePlus) {
        for(int i = (int)Exons().size()-1; i > 0; --i) {
            if(Exons()[i].m_fsplice && Exons()[i-1].m_ssplice && Exons()[i].m_fsplice_sig != "XX" && Exons()[i-1].m_ssplice_sig != "XX") {
                l = amap.MapRangeOrigToEdited(Exons()[i],true).GetFrom();
                break;
            }
        }
    } else {
        for(int i = 0; i < (int)Exons().size()-1; ++i) {
            if(Exons()[i].m_ssplice && Exons()[i+1].m_fsplice && Exons()[i].m_ssplice_sig != "XX" && Exons()[i+1].m_fsplice_sig != "XX") {
                l = amap.MapRangeOrigToEdited(Exons()[i],true).GetFrom();
                break;
            }                
        }
    }
    return l > cds.GetTo()+limit;
}


void CGeneModel::ReverseComplementModel() {
    SetStrand(Strand() == ePlus ? eMinus : ePlus);
    Status() ^= eReversed;
    NON_CONST_ITERATE(TExons, it, MyExons()) {
        if(it->m_fsplice_sig != "XX")
            ReverseComplement(it->m_fsplice_sig.begin(),it->m_fsplice_sig.end());
        if(it->m_ssplice_sig != "XX")
            ReverseComplement(it->m_ssplice_sig.begin(),it->m_ssplice_sig.end());
    }
}

CAlignMap CGeneModel::GetAlignMap() const { return CAlignMap(Exons(), FrameShifts(), Strand()); }

CAlignModel::CAlignModel(const CGeneModel& g, const CAlignMap& a)
    : CGeneModel(g), m_alignmap(a)
{

#ifdef _DEBUG
    int diff = 0;
    for(int ie = 0; ie < (int)Exons().size(); ++ie) {
        if(Exons()[ie].Limits().Empty())
            diff += Exons()[ie].m_seq.size();
        else
            diff += Exons()[ie].Limits().GetLength();

        diff -= TranscriptExon(ie).GetLength();
    }
    ITERATE(TInDels, i, FrameShifts()) {
        if(i->IsDeletion())
            diff += i->Len();
        else if(i->IsInsertion())
            diff -= i->Len();
    }
    _ASSERT(diff == 0);
#endif

    SetTargetId(*CIdHandler::GnomonMRNA(ID()));
    if(a.Orientation() != g.Strand()) 
        Status() |= CGeneModel::eReversed;
}

void CAlignModel::ResetAlignMap()
{
    Status() &= ~CGeneModel::eReversed;
    m_alignmap = CGeneModel::GetAlignMap();
}

void CAlignModel::RecalculateAlignMap(int left, int right) {
    if(Exons().empty()) {
        m_alignmap = CAlignMap();
        return;
    }

    vector<TSignedSeqRange> transcript_exons;
    for(int ie = 0; ie < (int)Exons().size(); ++ie) {
        const CModelExon& e = Exons()[ie];
        if(e.Limits().Empty()) {
            transcript_exons.push_back(TranscriptExon(ie));
        } else {
            CAlignMap::ERangeEnd l = e.GetFrom() == left ? CAlignMap::eSinglePoint : CAlignMap::eLeftEnd;
            CAlignMap::ERangeEnd r = e.GetTo() == right ? CAlignMap::eSinglePoint : CAlignMap::eRightEnd;
            TSignedSeqRange texon = m_alignmap.MapRangeOrigToEdited(e.Limits(), l, r);
            transcript_exons.push_back(texon);
        }
    }
    m_alignmap = CAlignMap(Exons(), transcript_exons, FrameShifts(), Orientation(), m_alignmap.TargetLen());
}

string CAlignModel::TargetAccession() const {
    _ASSERT( !GetTargetId().Empty() );
    if (GetTargetId().Empty())
        return "UnknownTarget";
    return CIdHandler::ToString(*GetTargetId());
}

void CGeneModel::Remap(const CRangeMapper& mapper)
{
    NON_CONST_ITERATE(TExons, e, MyExons()) {
        e->Remap(mapper);
    }
    RecalculateLimits();

    if(ReadingFrame().NotEmpty())
        m_cds_info.Remap(mapper);
}

bool CCDSInfo::operator== (const CCDSInfo& a) const
{
    return 
        m_start==a.m_start &&
        m_stop==a.m_stop && 
        m_reading_frame==a.m_reading_frame &&
        m_reading_frame_from_proteins==a.m_reading_frame_from_proteins &&
        m_max_cds_limits==a.m_max_cds_limits &&
        m_confirmed_start==a.m_confirmed_start &&
        m_confirmed_stop==a.m_confirmed_stop &&
        m_p_stops==a.m_p_stops &&
        m_open==a.m_open &&
        m_score==a.m_score &&
        m_genomic_coordinates == a.m_genomic_coordinates;
}

CCDSInfo CCDSInfo::MapFromEditedToOrig(const CAlignMap& amap) const
{
    CCDSInfo empty_cds;
    CCDSInfo new_cds(true);

    if(ProtReadingFrame().NotEmpty()) {
        TSignedSeqRange prot_rf = amap.MapRangeEditedToOrig(ProtReadingFrame(), true);
        if(prot_rf.NotEmpty())
            new_cds.SetReadingFrame(prot_rf, true);
        else
            return empty_cds;
    }
    if(ReadingFrame().NotEmpty()) {
        TSignedSeqRange rf = amap.MapRangeEditedToOrig(ReadingFrame(), true);
        if(rf.NotEmpty() && amap.FShiftedLen(rf, true)%3 == 0)
            new_cds.SetReadingFrame(rf, false);
        else
            return empty_cds;
    }
    if(HasStart()) {
        TSignedSeqRange start = amap.MapRangeEditedToOrig(Start(), false);
        if(start.NotEmpty())
            new_cds.SetStart(start, ConfirmedStart());
        else
            return empty_cds;
    }
    if(HasStop()) {
        TSignedSeqRange stop = amap.MapRangeEditedToOrig(Stop(), false);
        if(stop.NotEmpty())
            new_cds.SetStop(stop, ConfirmedStop());
        else
            return empty_cds;        
    }
    ITERATE(TPStops, i, PStops()) {
        TSignedSeqRange pstop = amap.MapRangeEditedToOrig(*i, false);
        if(pstop.NotEmpty())
            new_cds.AddPStop(pstop, i->m_status);
        else
            return empty_cds;        
    }
    if(HasStart() && MaxCdsLimits().GetFrom() == Start().GetFrom()) {
        new_cds.Set5PrimeCdsLimit(amap.MapEditedToOrig(Start().GetFrom()));
    } else if(HasStart() &&  MaxCdsLimits().GetTo() == Start().GetTo()) {
        new_cds.Set5PrimeCdsLimit(amap.MapEditedToOrig(Start().GetTo()));
    }
    new_cds.SetScore(Score(), OpenCds());

    return new_cds;    
}

CCDSInfo CCDSInfo::MapFromOrigToEdited(const CAlignMap& amap) const
{
    CCDSInfo new_cds(false);

    if(ProtReadingFrame().NotEmpty()) {
        new_cds.SetReadingFrame(amap.MapRangeOrigToEdited(ProtReadingFrame(), true), true);
        _ASSERT(new_cds.ProtReadingFrame().NotEmpty());
    }
    if(ReadingFrame().NotEmpty()) {
        new_cds.SetReadingFrame(amap.MapRangeOrigToEdited(ReadingFrame(), true), false);
        _ASSERT(new_cds.ReadingFrame().NotEmpty());
    }
    if(HasStart()) {
        new_cds.SetStart(amap.MapRangeOrigToEdited(Start(), false), ConfirmedStart());
        _ASSERT(new_cds.HasStart());
    }
    if(HasStop()) {
        new_cds.SetStop(amap.MapRangeOrigToEdited(Stop(), false), ConfirmedStop());
        _ASSERT(new_cds.HasStop());
    }
    ITERATE(TPStops, i, PStops()) {
        TSignedSeqRange p = amap.MapRangeOrigToEdited(*i, false);
        _ASSERT(p.NotEmpty());
        new_cds.AddPStop(p, i->m_status);
    }
    if(HasStart() && MaxCdsLimits().GetFrom() == Start().GetFrom()) {
        new_cds.Set5PrimeCdsLimit(amap.MapOrigToEdited(Start().GetFrom()));
    } else if(HasStart() &&  MaxCdsLimits().GetTo() == Start().GetTo()) {
        new_cds.Set5PrimeCdsLimit(amap.MapOrigToEdited(Start().GetTo()));
    }
    new_cds.SetScore(Score(), OpenCds());

    return new_cds;
}

void CCDSInfo::SetReadingFrame(TSignedSeqRange r, bool protein)
{
    if (r.Empty()) {
        if(protein)
            m_reading_frame_from_proteins = r;
        else
            Clear();
    } else {
        m_reading_frame = r;
        if (protein)
            m_reading_frame_from_proteins = r;
        if (m_max_cds_limits.Empty()) {
            m_max_cds_limits = TSignedSeqRange::GetWhole();
        }
    }
    _ASSERT( Invariant() );
}

void CCDSInfo::SetStart(TSignedSeqRange r, bool confirmed)
{
    if (confirmed) {
        m_confirmed_start = true;
        m_open = false;
    } else if (m_confirmed_start && r != m_start) {
        m_confirmed_start = false;
    }
    m_start = r;
    _ASSERT( Invariant() );
}

void CCDSInfo::SetStop(TSignedSeqRange r, bool confirmed)
{
    if(m_stop.NotEmpty()) {
        if(m_max_cds_limits.GetFrom() == m_stop.GetFrom())
            m_max_cds_limits.SetFrom( TSignedSeqRange::GetWholeFrom());
        if(m_max_cds_limits.GetTo() == m_stop.GetTo())
            m_max_cds_limits.SetTo( TSignedSeqRange::GetWholeTo());
    }

    if (confirmed)
        m_confirmed_stop = true;
    else if (m_confirmed_stop && r != m_stop) {
        m_confirmed_stop = false;
    }
    m_stop = r;
    if (r.NotEmpty()) {
        if (Precede(m_reading_frame,r))
            m_max_cds_limits.SetTo(r.GetTo());
        else
            m_max_cds_limits.SetFrom(r.GetFrom());
    }
    if(!m_p_stops.empty() && m_p_stops.back() == m_stop) 
        m_p_stops.pop_back();
    if(!m_p_stops.empty() && m_p_stops.front() == m_stop)
        m_p_stops.erase(m_p_stops.begin());

    _ASSERT( Invariant() );
}

void CCDSInfo::Clear5PrimeCdsLimit()
{
    if (HasStop()) {
        if (Precede(ReadingFrame(),Stop()))
            m_max_cds_limits.SetFrom( TSignedSeqRange::GetWholeFrom());
        else
            m_max_cds_limits.SetTo( TSignedSeqRange::GetWholeTo());
    } else {
        m_max_cds_limits = TSignedSeqRange::GetWhole();
    }
    _ASSERT( Invariant() );
}

void CCDSInfo::Set5PrimeCdsLimit(TSignedSeqPos p)
{
    if (p <= m_reading_frame.GetFrom())
        m_max_cds_limits.SetFrom(p);
    else
        m_max_cds_limits.SetTo(p);

    _ASSERT( Invariant() );
}


void CCDSInfo::AddPStop(TSignedSeqRange r, EStatus status)
{
    m_p_stops.push_back(SPStop(r, status));
    _ASSERT( Invariant() );
}


void CCDSInfo::SetScore(double score, bool open)
{
    m_score = score;
    _ASSERT( !(open && !HasStart()) );
    m_open = open;
    _ASSERT( Invariant() );
}

void CCDSInfo::Remap(const CRangeMapper& mapper)
{
    if(m_start.NotEmpty())
        m_start = mapper(m_start, false);
    if(m_stop.NotEmpty())
        m_stop = mapper(m_stop, false);
    if(m_reading_frame.NotEmpty())
        m_reading_frame = mapper(m_reading_frame, true);
    if(m_reading_frame_from_proteins.NotEmpty())
        m_reading_frame_from_proteins = mapper(m_reading_frame_from_proteins, true);
    if(m_max_cds_limits.NotEmpty())
        m_max_cds_limits = mapper(m_max_cds_limits, false);
    NON_CONST_ITERATE(TPStops, s, m_p_stops) {
        *s = SPStop(mapper(*s, false), s->m_status);
    }
    _ASSERT(Invariant());
}

void CCDSInfo::CombineWith(const CCDSInfo& another_cds_info)
{
    if (another_cds_info.m_reading_frame.Empty())
        return;

    CCDSInfo new_cds_info;

    if (m_reading_frame.Empty()) {
        new_cds_info = another_cds_info;
    } else {
        new_cds_info = *this;

        new_cds_info.m_p_stops.insert(new_cds_info.m_p_stops.end(),another_cds_info.m_p_stops.begin(),another_cds_info.m_p_stops.end());
        sort(new_cds_info.m_p_stops.begin(),new_cds_info.m_p_stops.end());
        new_cds_info.m_p_stops.erase( unique(new_cds_info.m_p_stops.begin(),new_cds_info.m_p_stops.end()), new_cds_info.m_p_stops.end() ) ;

        new_cds_info.m_reading_frame               += another_cds_info.m_reading_frame;
        new_cds_info.m_reading_frame_from_proteins += another_cds_info.m_reading_frame_from_proteins;
        new_cds_info.m_max_cds_limits        &= another_cds_info.m_max_cds_limits;

        if (new_cds_info.m_confirmed_start && Include(new_cds_info.m_reading_frame_from_proteins,new_cds_info.m_start)) {
            new_cds_info.m_start = TSignedSeqRange::GetEmpty();
            new_cds_info.m_confirmed_start = false;
        }
        if (another_cds_info.m_confirmed_start && !Include(new_cds_info.m_reading_frame_from_proteins,another_cds_info.m_start)) {
            new_cds_info.m_start = another_cds_info.m_start;
            new_cds_info.m_confirmed_start = true;
        }
        if (new_cds_info.m_confirmed_start) {
            if (Include(new_cds_info.m_reading_frame,new_cds_info.m_start)) {
                if (Precede(new_cds_info.m_start,new_cds_info.m_reading_frame_from_proteins))
                    new_cds_info.m_reading_frame.SetFrom( new_cds_info.m_reading_frame_from_proteins.GetFrom() );
                else
                    new_cds_info.m_reading_frame.SetTo( new_cds_info.m_reading_frame_from_proteins.GetTo() );
            }
        } else {
            if (new_cds_info.HasStart() && Include(new_cds_info.m_reading_frame,new_cds_info.m_start)) {
                new_cds_info.m_start = TSignedSeqRange::GetEmpty();
            }
            if (another_cds_info.HasStart() && !Include(new_cds_info.m_reading_frame,another_cds_info.m_start)) {
                new_cds_info.m_start = another_cds_info.m_start;
            }
        }

        _ASSERT( !new_cds_info.HasStop() || !another_cds_info.HasStop() || new_cds_info.Stop() == another_cds_info.Stop() );
        new_cds_info.m_stop += another_cds_info.m_stop;
        if (another_cds_info.m_confirmed_stop)
            new_cds_info.m_confirmed_stop = true;

        new_cds_info.SetScore( BadScore(), false );
    }

    _ASSERT( new_cds_info.Invariant() );
    *this = new_cds_info;
}

int CCDSInfo::Strand() const
{
    int strand = 0; //unknown
    if (HasStart())
        strand = Precede(Start(), ReadingFrame()) ? 1 : -1;
    else if (HasStop())
        strand = Precede(ReadingFrame(), Stop()) ? 1 : -1;
    else if (m_max_cds_limits.GetFrom() != TSignedSeqRange::GetWholeFrom())
        strand = 1;
    else if (m_max_cds_limits.GetTo() != TSignedSeqRange::GetWholeTo())
        strand = -1;
    return strand;
}

void CCDSInfo::Clip(TSignedSeqRange limits)
{
    if (ReadingFrame().Empty())
        return;

    m_reading_frame &= limits;

    if (m_reading_frame.Empty()) {
        Clear();
        return;
    }

    m_start  &= limits;
    m_confirmed_start &= HasStart();
    m_stop  &= limits;
    m_confirmed_stop &= HasStop();
    m_reading_frame_from_proteins &= limits;

    if (m_max_cds_limits.GetFrom() < limits.GetFrom())
        m_max_cds_limits.SetFrom(TSignedSeqRange::GetWholeFrom());
    if (limits.GetTo() < m_max_cds_limits.GetTo())
        m_max_cds_limits.SetTo(TSignedSeqRange::GetWholeTo());

    for(TPStops::iterator s = m_p_stops.begin(); s != m_p_stops.end(); ) {
        *s &= limits;
        if (s->NotEmpty())
            ++s;
        else
            s = m_p_stops.erase(s);
    }

    SetScore( BadScore(), false );

    _ASSERT( Invariant() );
}

void CCDSInfo::Cut(TSignedSeqRange hole) {
    if((Cds() & hole).Empty())
        return;

    if(Include(hole, Cds())) {
        Clear();
        return;
    }

    if(hole.IntersectingWith(m_start)) {
        _ASSERT(Include(hole, m_start));
        m_start = TSignedSeqRange::GetEmpty();
        m_confirmed_start = false;
    }
    if(hole.IntersectingWith(m_stop)) {
        _ASSERT(Include(hole, m_stop));
        m_stop = TSignedSeqRange::GetEmpty();
        m_confirmed_stop = false;
    }

    if(Include(hole, m_max_cds_limits.GetFrom()))
        m_max_cds_limits.SetFrom(TSignedSeqRange::GetWholeFrom());
    if(Include(hole, m_max_cds_limits.GetTo()))
        m_max_cds_limits.SetTo(TSignedSeqRange::GetWholeTo());

    if(hole.IntersectingWith(m_reading_frame_from_proteins)) {
        if(hole.GetFrom() <= m_reading_frame_from_proteins.GetFrom())
            m_reading_frame_from_proteins.SetFrom(hole.GetTo()+1);
        if(hole.GetTo() >= m_reading_frame_from_proteins.GetTo())
            m_reading_frame_from_proteins.SetTo(hole.GetFrom()-1); 
    }

    if(hole.IntersectingWith(m_reading_frame)) {
        if(hole.GetFrom() <= m_reading_frame.GetFrom())
            m_reading_frame.SetFrom(hole.GetTo()+1);
        if(hole.GetTo() >= m_reading_frame.GetTo())
            m_reading_frame.SetTo(hole.GetFrom()-1); 
    }

    ERASE_ITERATE(TPStops, s, m_p_stops) {
        if(hole.IntersectingWith(*s)) {
            _ASSERT(Include(hole, *s));
            VECTOR_ERASE(s, m_p_stops);
        }
    }

    SetScore( BadScore(), false );
    _ASSERT( Invariant() );
}

void CCDSInfo::Clear()
{
    m_start = m_stop = m_reading_frame = m_reading_frame_from_proteins = m_max_cds_limits = TSignedSeqRange::GetEmpty();
    m_confirmed_start = false;
    m_confirmed_stop = false;
    m_p_stops.clear();
    SetScore( BadScore(), false );
}

bool CCDSInfo::PStop(bool includeall) const
{ 
    if(includeall)
        return !m_p_stops.empty(); 

    ITERATE(TPStops, stp, m_p_stops) {
        if(stp->m_status != eGenomeNotCorrect && stp->m_status != eSelenocysteine)
            return true;
    }

    return false;
} 

void CGeneModel::RemoveShortHolesAndRescore(const CGnomonEngine& gnomon) {
    TExons new_exons;
    new_exons.push_back(Exons().front());
    bool modified = false;
    for(unsigned int i = 1; i < Exons().size(); ++i) {
        bool hole = !Exons()[i-1].m_ssplice || !Exons()[i].m_fsplice;
        int intron = Exons()[i].GetFrom()-Exons()[i-1].GetTo()-1;
        if (hole && intron < gnomon.GetMinIntronLen()) {
            modified = true;
            new_exons.back().m_ssplice = Exons()[i].m_ssplice;
            new_exons.back().AddTo(Exons()[i].GetTo()-Exons()[i-1].GetTo());
            if(intron%3 != 0) {
                int len = intron%3;
                int loc = Exons()[i-1].GetTo() + 1 + (intron-len)/2;
                FrameShifts().push_back(CInDelInfo(loc, len, CInDelInfo::eIns));
            }
        } else {
            new_exons.push_back(Exons()[i]);
        } 
    }

    if(modified) {
        MyExons() = new_exons;
        sort(FrameShifts().begin(),FrameShifts().end());
        gnomon.GetScore(*this);
    }
}

bool CGeneModel::CdsInvariant(bool check_start_stop) const
{
    if (ReadingFrame().Empty())
        return true;


#ifdef _DEBUG
    CAlignMap mrnamap(Exons(),FrameShifts(),Strand());
    CCDSInfo cds_info = GetCdsInfo();
    if(cds_info.IsMappedToGenome())
        cds_info = cds_info.MapFromOrigToEdited(mrnamap);
    
    _ASSERT( !(ConfirmedStart() && OpenCds()) );

    _ASSERT(Include(cds_info.MaxCdsLimits(), cds_info.Cds()));
    _ASSERT(cds_info.MaxCdsLimits().GetFrom() == TSignedSeqRange::GetWholeFrom() || cds_info.MaxCdsLimits().GetFrom() == cds_info.Cds().GetFrom());
    _ASSERT(cds_info.MaxCdsLimits().GetTo() == TSignedSeqRange::GetWholeTo() || cds_info.MaxCdsLimits().GetTo() == cds_info.Cds().GetTo());

    if (check_start_stop && Score() != BadScore()) {
        _ASSERT(cds_info.ReadingFrame().GetFrom() < 3 || (cds_info.HasStart() && cds_info.Start().GetTo()+1 == cds_info.ReadingFrame().GetFrom()));
        _ASSERT(cds_info.ReadingFrame().GetTo() >= mrnamap.TargetLen()-3 || (cds_info.HasStop() && cds_info.Stop().GetFrom()-1 == cds_info.ReadingFrame().GetTo()));
    }

    _ASSERT(!cds_info.HasStart() || cds_info.Start().GetLength()==3);
    _ASSERT(!cds_info.HasStop() || cds_info.Stop().GetLength()==3);
    _ASSERT(cds_info.Cds().GetLength()%3==0);
#endif
    
    return true;
}

void CGeneModel::SetCdsInfo(const CCDSInfo& cds_info)
{
    m_cds_info = cds_info;
    _ASSERT( CdsInvariant() );
}

void CGeneModel::SetCdsInfo(const CGeneModel& a)
{
    m_cds_info = a.m_cds_info;
    _ASSERT( CdsInvariant() );
}

void CGeneModel::CombineCdsInfo(const CGeneModel& a, bool ensure_cds_invariant)
{
    CombineCdsInfo(a.m_cds_info, ensure_cds_invariant);
}

void CGeneModel::CombineCdsInfo(const CCDSInfo& cds_info, bool ensure_cds_invariant)
{
    _ASSERT( cds_info.ReadingFrame().Empty() || Include(Limits(),cds_info.ReadingFrame()) );
    //This assert is problem when when cds_info.ReadingFrame() touches a fs  ????
    _ASSERT( cds_info.ReadingFrame().Empty() || FShiftedLen(cds_info.Start()+cds_info.ReadingFrame()+cds_info.Stop(), false)%3==0 );

    m_cds_info.CombineWith(cds_info);

    _ASSERT( !ensure_cds_invariant || CdsInvariant(false) );
}

void CGeneModel::CutExons(TSignedSeqRange hole)
{
    if (ReadingFrame().NotEmpty()) {
        TSignedSeqRange cds_hole = hole;
        for(int i = 0; i < (int)MyExons().size(); ++i) {
            if(i > 0 && hole.GetFrom() == MyExons()[i].GetFrom())
                cds_hole.SetFrom(MyExons()[i].GetTo()+1);
            if(i < (int)MyExons().size()-1 && hole.GetTo() ==  MyExons()[i].GetTo())
                cds_hole.SetTo(MyExons()[i].GetFrom()-1);
        }
        m_cds_info.Cut(cds_hole);
    }

    for(int i = 0; i < (int)MyExons().size(); ++i) {
        TSignedSeqRange intersection = MyExons()[i].Limits() & hole;
        if (intersection.Empty())
            continue;
        if (MyExons()[i].GetFrom()<hole.GetFrom()) {
            MyExons()[i].Limits().SetTo(hole.GetFrom()-1);
            MyExons()[i].m_ssplice = false;
            MyExons()[i].m_ssplice_sig.clear();
            if (i+1 < (int)MyExons().size()) {
                MyExons()[i+1].m_fsplice=false;
            }
        } else if (hole.GetTo()<MyExons()[i].GetTo()) {
            MyExons()[i].Limits().SetFrom(hole.GetTo()+1);
            MyExons()[i].m_fsplice = false;
            MyExons()[i].m_fsplice_sig.clear();
            if (0<i) {
                MyExons()[i-1].m_ssplice=false;
            }
        } else {
            if (0<i) {
                MyExons()[i-1].m_ssplice=false;
                //                MyExons()[i-1].m_ssplice_sig.clear();
           }
            if (i+1 < (int)MyExons().size()) {
                MyExons()[i+1].m_fsplice=false;
                //                MyExons()[i+1].m_fsplice_sig.clear();
            }
            MyExons().erase( MyExons().begin()+i);
            --i;
        }
    }
    RecalculateLimits();
    RemoveExtraFShifts(hole.GetTo()+1, hole.GetFrom()-1);
}

void CGeneModel::Clip(TSignedSeqRange clip_limits, EClipMode mode, bool ensure_cds_invariant)
{
    if (ReadingFrame().NotEmpty()) {
        TSignedSeqRange cds_clip_limits = Limits();
        if (mode == eRemoveExons)
            cds_clip_limits &= clip_limits;
        else {
            TSignedSeqRange intersection;
            intersection = Exons().front().Limits() & clip_limits;
            if (intersection.NotEmpty())
                cds_clip_limits.SetFrom(intersection.GetFrom());
            intersection = Exons().back().Limits() & clip_limits;
            if (intersection.NotEmpty())
                cds_clip_limits.SetTo(intersection.GetTo());
        }

        TSignedSeqRange precise_cds_clip_limits;
        ITERATE(TExons, e, Exons())
            precise_cds_clip_limits += e->Limits() & cds_clip_limits;
        cds_clip_limits = precise_cds_clip_limits;

        CAlignMap mrnamap(Exons(),FrameShifts(),Strand(),GetCdsInfo().Cds());

        if (RealCdsLimits().GetFrom() < cds_clip_limits.GetFrom() && cds_clip_limits.GetFrom() < ReadingFrame().GetFrom())
            cds_clip_limits.SetFrom(ReadingFrame().GetFrom());
        else if (ReadingFrame().GetFrom() < cds_clip_limits.GetFrom() && cds_clip_limits.GetFrom() <= RealCdsLimits().GetTo()) {
            TSignedSeqRange tmp = mrnamap.ShrinkToRealPoints(TSignedSeqRange(cds_clip_limits.GetFrom(), RealCdsLimits().GetTo()),true);
            if(tmp.Empty() || tmp.GetFrom() >= ReadingFrame().GetTo())
                cds_clip_limits = TSignedSeqRange::GetEmpty();
            else               
                cds_clip_limits.SetFrom(tmp.GetFrom());
        }

        if( cds_clip_limits.NotEmpty()) {
            if (RealCdsLimits().GetFrom() <= cds_clip_limits.GetTo() && cds_clip_limits.GetTo() < ReadingFrame().GetTo()) {
                TSignedSeqRange tmp = mrnamap.ShrinkToRealPoints(TSignedSeqRange(RealCdsLimits().GetFrom(),cds_clip_limits.GetTo()),true);
                if(tmp.Empty() || ReadingFrame().GetFrom() >= tmp.GetTo())
                    cds_clip_limits = TSignedSeqRange::GetEmpty();
                else
                    cds_clip_limits.SetTo(tmp.GetTo());
            } else if (ReadingFrame().GetTo() < cds_clip_limits.GetTo() && cds_clip_limits.GetTo() < RealCdsLimits().GetTo())
                cds_clip_limits.SetTo(ReadingFrame().GetTo());
        }

        m_cds_info.Clip(cds_clip_limits);
    }

    for (TExons::iterator e = MyExons().begin(); e != MyExons().end();) {
        TSignedSeqRange clip = e->Limits() & clip_limits;
        if (clip.NotEmpty()) {
            if(e->GetFrom() <= clip_limits.GetFrom())
                e->m_fsplice = false;
            if(e->GetFrom() < clip_limits.GetFrom())
                e->m_fsplice_sig.clear(); 	 
            if(e->GetTo() >= clip_limits.GetTo())
                e->m_ssplice = false;
            if(e->GetTo() > clip_limits.GetTo())
                e->m_ssplice_sig.clear(); 	 

            e++->Limits() = clip;
        } else if (mode == eRemoveExons)
            e = MyExons().erase(e);
        else
            ++e;
    }

    if (Exons().size()>0) {
        MyExons().front().m_fsplice = false;
        //        MyExons().front().m_fsplice_sig.clear();
        MyExons().back().m_ssplice = false;
        //        MyExons().back().m_ssplice_sig.clear();
    }

    RecalculateLimits();
    RemoveExtraFShifts(clip_limits.GetFrom(), clip_limits.GetTo());
    
    _ASSERT( CdsInvariant(ensure_cds_invariant) );
}

void CGeneModel::RemoveExtraFShifts(int left, int right)
{
    TInDels::const_iterator indl = m_fshifts.begin();
    TInDels indels;
    for(CGeneModel::TExons::const_iterator e = Exons().begin(); e != Exons().end() && indl != m_fshifts.end(); ++e) {
        if(e->Limits().Empty())
            continue;

        for( ;indl != m_fshifts.end() && indl->InDelEnd() < (indl->IsDeletion() ? e->GetFrom() : e->GetFrom()+1); ++indl);  // skip indels on the left
        for( ;indl != m_fshifts.end() && e->GetFrom() == left && indl->IsDeletion() && indl->Loc() == left; ++indl);        // skip flanking deletion if clipped

        if(indl != m_fshifts.end() && indl->Loc() < e->GetFrom()) {                                                         // clip long mismatch
            _ASSERT(indl->IsMismatch());
            int extra = e->GetFrom()-indl->Loc();
            indels.push_back(CInDelInfo(e->GetFrom(),indl->Len()-extra,CInDelInfo::eMism,indl->GetInDelV().substr(extra)));
            ++indl;
        }

        for( ; indl != m_fshifts.end() && indl->Loc() <= e->GetTo(); ++indl)                                                // include all internal indels
            indels.push_back(*indl);
        
        for( ; indl != m_fshifts.end() && e->GetTo() != right && indl->IsDeletion() && indl->Loc() == e->GetTo()+1; ++indl) // include flanking deletion if not cliped
             indels.push_back(*indl);    

        if(!indels.empty() && indels.back().InDelEnd() > e->GetTo()+1) {      // clip long mismatch
            _ASSERT(indels.back().IsMismatch());
            int extra = indels.back().InDelEnd()-e->GetTo()-1;
            indels.back() = CInDelInfo(indels.back().Loc(),indels.back().Len()-extra,CInDelInfo::eMism,indels.back().GetInDelV().substr(0,indels.back().Len()-extra));
        }
    }

    m_fshifts = indels;
}


TInDels  CGeneModel::FrameShifts(TSignedSeqPos a, TSignedSeqPos b) const
{
    TInDels fshifts;

    ITERATE(TInDels, i, m_fshifts) {
        if(i->IntersectingWith(a,b))
            fshifts.push_back( *i );
    }
        
    return fshifts;
}


int CGeneModel::FShiftedLen(TSignedSeqRange ab, bool withextras) const
{
    if (ab.Empty())
        return 0;

    _ASSERT(Include(Limits(),ab));

    return GetAlignMap().FShiftedLen(ab, withextras);
}

TSignedSeqPos CGeneModel::FShiftedMove(TSignedSeqPos pos, int len) const
{
    return GetAlignMap().FShiftedMove(pos, len);
}


int CGeneModel::MutualExtension(const CGeneModel& a) const
{
    //    if(Strand() != a.Strand()) return 0;
    const TSignedSeqRange limits = Limits();
    const TSignedSeqRange alimits = a.Limits();

    const int intersection = (limits & alimits).GetLength();
    if (intersection==0 || intersection==limits.GetLength() || intersection==alimits.GetLength()) return 0;
    
    return isCompatible(a);
}

int CGeneModel::isCompatible(const CGeneModel& a) const
{
    const CGeneModel& b = *this;  // shortcut to this alignment

    _ASSERT( b.Strand() == a.Strand() || ((a.Status()&CGeneModel::eUnknownOrientation) != 0 && (b.Status()&CGeneModel::eUnknownOrientation) != 0));

    /*     this code creates clatter of similr chains 
    if((a.Status()&CGeneModel::ePolyA) != (b.Status()&CGeneModel::ePolyA)) {   // one has PolyA another doesn't
        if(a.Strand() == ePlus) {
            int polya = (a.Status()&CGeneModel::ePolyA) != 0 ? a.Limits().GetTo() : b.Limits().GetTo();
            if(polya < max(a.Limits().GetTo(),b.Limits().GetTo()))            // extension beyond PolyA
                return 0;
        } else {
            int polya = (a.Status()&CGeneModel::ePolyA) != 0 ? a.Limits().GetFrom() : b.Limits().GetFrom();
            if(polya > min(a.Limits().GetFrom(),b.Limits().GetFrom()))            // extension beyond PolyA
                return 0;
        }
    }
    */   

    TSignedSeqRange intersect(a.Limits() & b.Limits());
    if(intersect.GetLength() <= 1) return 0;     // intersection with 1 base is not legit
    
    int anum = a.Exons().size()-1;   // exon containing left point or first exon on the left 
    for(; intersect.GetFrom() < a.Exons()[anum].GetFrom() ; --anum);
    bool aexon = intersect.GetFrom() <= a.Exons()[anum].GetTo();
    if(!aexon && intersect.GetTo() < a.Exons()[anum+1].GetFrom()) return 0;    // b is in intron
    
    int bnum = b.Exons().size()-1;   // exon containing left point or first exon on the left 
    for(; intersect.GetFrom() < b.Exons()[bnum].GetFrom(); --bnum);
    bool bexon = intersect.GetFrom() <= b.Exons()[bnum].GetTo();
    if(!bexon && intersect.GetTo() < b.Exons()[bnum+1].GetFrom()) return 0;    // a is in intron
    
    TSignedSeqPos left = intersect.GetFrom();
    TSignedSeqPos afirst = -1;
    TSignedSeqPos bfirst = -1;
    int commonspl = 0;
    int firstcommonpoint = -1;

    auto_ptr<CAlignMap> pmapa(0), pmapb(0);

    while(left <= intersect.GetTo())
    {
        TSignedSeqPos aright = aexon ? a.Exons()[anum].GetTo() : a.Exons()[anum+1].GetFrom()-1;
        TSignedSeqPos bright = bexon ? b.Exons()[bnum].GetTo() : b.Exons()[bnum+1].GetFrom()-1;
        TSignedSeqPos right = min(aright,bright);
        
        if(!aexon && bexon)
        {
            if(a.Exons()[anum].m_ssplice && a.Exons()[anum+1].m_fsplice) return 0;        // intron has both splices
            if(left == a.Exons()[anum].GetTo()+1 && a.Exons()[anum].m_ssplice) return 0;   // intron has left splice and == left
            if(right == aright && a.Exons()[anum+1].m_fsplice) return 0;          // intron has right splice and == right
        }
        if(aexon && !bexon)
        {
            if(b.Exons()[bnum].m_ssplice && b.Exons()[bnum+1].m_fsplice) return 0;
            if(left == b.Exons()[bnum].GetTo()+1 && b.Exons()[bnum].m_ssplice) return 0;
            if(right == bright && b.Exons()[bnum+1].m_fsplice) return 0; 
        }
        
        if(aexon && bexon) {
            if(firstcommonpoint < 0) {
                firstcommonpoint = left; 
            } else if((anum > 0 && left == a.Exons()[anum].GetFrom() && !a.Exons()[anum].m_fsplice) ||   // end of a-hole
                      (bnum > 0 && left == b.Exons()[bnum].GetFrom() && !b.Exons()[bnum].m_fsplice))  {  // end of b-hole
                
                if(afirst < 0) {
                    pmapa.reset(new CAlignMap(a.Exons(),a.FrameShifts(),a.Strand()));
                    pmapb.reset(new CAlignMap(b.Exons(),b.FrameShifts(),b.Strand()));
                    afirst = pmapa->MapOrigToEdited(firstcommonpoint);
                    bfirst = pmapb->MapOrigToEdited(firstcommonpoint);
                    if(afirst < 0 || bfirst < 0) return 0;
                }

                TSignedSeqPos asecond = pmapa->MapOrigToEdited(left);
                TSignedSeqPos bsecond = pmapb->MapOrigToEdited(left);
                if(asecond < 0 || bsecond < 0 || (asecond-afirst)%3 != (bsecond-bfirst)%3) return 0;
            }
        }

        if(aexon && aright == right && a.Exons()[anum].m_ssplice &&
           bexon && bright == right && b.Exons()[bnum].m_ssplice) ++commonspl;
        if(!aexon && aright == right && a.Exons()[anum+1].m_fsplice &&
           !bexon && bright == right && b.Exons()[bnum+1].m_fsplice) ++commonspl;
        
        left = right+1;
        
        if(right == aright)
        {
            aexon = !aexon;
            if(aexon) ++anum;
        }
        
        if(right == bright)
        {
            bexon = !bexon;
            if(bexon) ++bnum;
        }
    }
    
    return firstcommonpoint >= 0 ? commonspl+1 : 0;
}

bool CGeneModel::IsSubAlignOf(const CGeneModel& a) const 
{ 
    if(!Include(a.Limits(),Limits()) || !isCompatible(a))
        return false;

    for(unsigned int i = 1; i < a.Exons().size(); ++i) {
        if (!a.Exons()[i-1].m_ssplice || !a.Exons()[i].m_fsplice){
            TSignedSeqRange hole(a.Exons()[i-1].GetTo()+1, a.Exons()[i].GetFrom()-1);
            ITERATE(CGeneModel::TExons, k, Exons()) {
                if(k->Limits().IntersectingWith(hole)) {
                    return false;
                }
            }
        }
    }

    return true;
}


void CGeneModel::AddExon(TSignedSeqRange exon_range, const string& fs, const string& ss, double ident, const string& seq, const CInDelInfo::SSource& src)
{
    _ASSERT( (m_range & exon_range).Empty() );
    m_range += exon_range;

    CModelExon e(exon_range.GetFrom(),exon_range.GetTo(),false,false,fs,ss,ident,seq,src);
    if (MyExons().empty())
        MyExons().push_back(e);
    else if ((exon_range.Empty() || MyExons().back().Limits().Empty()) || MyExons().back().GetTo() < exon_range.GetFrom()) {
        if (!m_expecting_hole) {
            MyExons().back().m_ssplice = true;
            e.m_fsplice = true;
        }
        MyExons().push_back(e);
    } else {
        if (!m_expecting_hole) {
            MyExons().front().m_fsplice = true;
            e.m_ssplice = true;
        }
        MyExons().insert(MyExons().begin(),e);
    }
    m_expecting_hole = false;
}

void CGeneModel::AddHole()
{
    m_expecting_hole = true;
}

void CGeneModel::AddGgapExon(double ident, const string& seq, const CInDelInfo::SSource& src, bool infront)
{
    _ASSERT(MyExons().empty() || !m_expecting_hole);

    TSignedSeqRange exon_range(TSignedSeqRange::GetEmpty());
    CModelExon e(exon_range.GetFrom(),exon_range.GetTo(),false,false,"","",ident,seq,src);
    if (MyExons().empty())
        MyExons().push_back(e);
    else if (!infront) {
        _ASSERT(MyExons().back().Limits().NotEmpty());
        MyExons().back().m_ssplice = true;
        e.m_fsplice = true;
        e.m_fsplice_sig = "XX";
        MyExons().push_back(e);
    } else {
        _ASSERT(MyExons().front().Limits().NotEmpty());
        MyExons().front().m_fsplice = true;
        e.m_ssplice = true;
        e.m_ssplice_sig = "XX";
        MyExons().insert(MyExons().begin(),e);
    }    
    m_expecting_hole = false;
}

void CGeneModel::AddNormalExon(TSignedSeqRange exon_range, const string& fs, const string& ss, double ident, bool infront)
{
    _ASSERT( (m_range & exon_range).Empty() );
    m_range += exon_range;

    CModelExon e(exon_range.GetFrom(),exon_range.GetTo(),false,false,fs,ss,ident);
    if (MyExons().empty())
        MyExons().push_back(e);
    else if (!infront) {
        if (!m_expecting_hole) {
            MyExons().back().m_ssplice = true;
            if(MyExons().back().Limits().Empty())
                MyExons().back().m_ssplice_sig = "XX";
            e.m_fsplice = true;
        }
        MyExons().push_back(e);
    } else {
        if (!m_expecting_hole) {
            MyExons().front().m_fsplice = true;
            if(MyExons().front().Limits().Empty())
                MyExons().front().m_fsplice_sig = "XX"; 
            e.m_ssplice = true;
        }
        MyExons().insert(MyExons().begin(),e);
    }
    m_expecting_hole = false;
}

void CModelExon::Extend(const CModelExon& e)
{
    // spliced should include the other
    _ASSERT(
            (!m_fsplice || GetFrom() <= e.GetFrom()) &&
            (!e.m_fsplice || GetFrom() >= e.GetFrom()) &&
            (!m_ssplice || e.GetTo() <= GetTo()) &&
            (!e.m_ssplice || e.GetTo() >= GetTo())
            );

    Limits().CombineWith(e.Limits());
    m_fsplice =m_fsplice || e.m_fsplice;
    m_ssplice =m_ssplice || e.m_ssplice;
    if(e.m_fsplice && !e.m_fsplice_sig.empty())
        m_fsplice_sig = e.m_fsplice_sig;
    if(e.m_ssplice && !e.m_ssplice_sig.empty())
        m_ssplice_sig = e.m_ssplice_sig;    
}

void CGeneModel::TrimEdgesToFrameInOtherAlignGaps(const TExons& exons_with_gaps, bool ensure_cds_invariant)
{
    if(Exons().empty())
        return;

    TSignedSeqPos left_edge = Limits().GetFrom();
    TSignedSeqPos right_edge = Limits().GetTo();
    CAlignMap mrnamap(Exons(), FrameShifts(), ePlus);   // 'positive' alignmap

    for (int i = 0; i<int(exons_with_gaps.size())-1; ++i) {
        if (exons_with_gaps[i].GetTo() < left_edge && left_edge < exons_with_gaps[i+1].GetFrom()) {
            _ASSERT( !exons_with_gaps[i].m_ssplice && !exons_with_gaps[i+1].m_fsplice );
            _ASSERT( right_edge >= exons_with_gaps[i+1].GetFrom() );
            TSignedSeqRange tlim = mrnamap.MapRangeOrigToEdited(TSignedSeqRange(left_edge,exons_with_gaps[i+1].GetFrom()), CAlignMap::eLeftEnd, CAlignMap::eSinglePoint);
            _ASSERT(tlim.NotEmpty());
            int del = (tlim.GetLength()-1)%3;
            if(del > 0) {
                left_edge = -1;
                for(int tpos = tlim.GetFrom()+del; left_edge < 0 && tpos <= tlim.GetTo(); tpos += 3) {
                    left_edge = mrnamap.MapEditedToOrig(tpos);
                }
                _ASSERT(left_edge > 0);
                CutExons(TSignedSeqRange(Limits().GetFrom(),left_edge-1));
            }
        }

        if (exons_with_gaps[i].GetTo() < right_edge && right_edge < exons_with_gaps[i+1].GetFrom()) {
            _ASSERT( !exons_with_gaps[i].m_ssplice && !exons_with_gaps[i+1].m_fsplice );
            _ASSERT( left_edge <= exons_with_gaps[i].GetTo() );
            TSignedSeqRange tlim = mrnamap.MapRangeOrigToEdited(TSignedSeqRange(exons_with_gaps[i].GetTo(),right_edge), CAlignMap::eSinglePoint, CAlignMap::eLeftEnd);
            _ASSERT(tlim.NotEmpty());
            int del = (tlim.GetLength()-1)%3;
            if(del > 0) {
                right_edge = -1;
                for(int tpos = tlim.GetTo()-del; right_edge < 0 && tpos >= tlim.GetFrom(); tpos -= 3) {
                    right_edge = mrnamap.MapEditedToOrig(tpos);
                }
                _ASSERT(right_edge > 0);
                CutExons(TSignedSeqRange(right_edge+1, Limits().GetTo()));
            }
        }
    }
}

void CGeneModel::Extend(const CGeneModel& align, bool ensure_cds_invariant)
{
    _ASSERT( align.Strand() == Strand() );

    CGeneModel other_align = align;

    if ( !other_align.Continuous() )
        this->TrimEdgesToFrameInOtherAlignGaps(other_align.Exons(),ensure_cds_invariant);
    if ( !this->Continuous() )
        other_align.TrimEdgesToFrameInOtherAlignGaps(this->Exons(),ensure_cds_invariant);

    TExons a = MyExons();
    TExons b = other_align.Exons();

    MyExons().clear();

    size_t i,j;
    for ( i=0,j=0; i<a.size() || j<b.size(); ) {
        if (i==a.size())
            MyExons().push_back(b[j++]);
        else if (j==b.size())
            MyExons().push_back(a[i++]);
        else if (a[i].GetTo()+1<b[j].GetFrom())
            MyExons().push_back(a[i++]);
        else if (b[j].GetTo()+1<a[i].GetFrom())
            MyExons().push_back(b[j++]);
        else {
            b[j].Extend(a[i++]);
            while (j+1<b.size() && b[j].GetTo()+1>=b[j+1].GetFrom()) {
                b[j+1].Extend(b[j]);
                ++j;
            }
        }
        if(!MyExons().empty())
            MyExons().back().m_ident = 0;
    }

    RecalculateLimits();
    
    m_fshifts.insert(m_fshifts.end(),align.m_fshifts.begin(),align.m_fshifts.end());
    if(!m_fshifts.empty()) {
        sort(m_fshifts.begin(),m_fshifts.end());
        m_fshifts.erase( unique(m_fshifts.begin(),m_fshifts.end()), m_fshifts.end() );
    }
#ifdef _DEBUG 
    ITERATE(TInDels, indl, m_fshifts) {
        if(indl->IsDeletion())
            continue;
        TInDels::const_iterator next = indl;
        if(++next !=  m_fshifts.end())
            _ASSERT(indl->InDelEnd() <= next->Loc());
    }
#endif                        

    SetType(Type() | ((CGeneModel::eProt|CGeneModel::eSR|CGeneModel::eEST|CGeneModel::emRNA|CGeneModel::eNotForChaining)&align.Type()));
    if(align.ReadingFrame().NotEmpty())
        CombineCdsInfo(align, ensure_cds_invariant);
}

void CGeneModel::RecalculateLimits()
{
    if (Exons().empty()) {
        m_range = TSignedSeqRange::GetEmpty(); 
    } else {
        int num = Exons().size();
        if(Exons()[0].Limits().NotEmpty()) {
            m_range.SetFrom(Exons()[0].GetFrom());
        } else {
            _ASSERT(num > 1 && Exons()[1].Limits().NotEmpty());
            m_range.SetFrom(Exons()[1].GetFrom());
        }
        if(Exons()[num-1].Limits().NotEmpty()) {
            m_range.SetTo(Exons()[num-1].GetTo());
        } else {
           _ASSERT(num > 1 && Exons()[num-2].Limits().NotEmpty()); 
            m_range.SetTo(Exons()[num-2].GetTo());
        }
    }
}


void CGeneModel::ExtendLeft(int amount)
{
    _ASSERT(amount>=0);
    MyExons().front().AddFrom(-amount);
    RecalculateLimits();
}

void CGeneModel::ExtendRight(int amount)
{
    _ASSERT(amount>=0);
    MyExons().back().AddTo(amount);
    RecalculateLimits();
}

int CGeneModel::AlignLen() const
{
    return FShiftedLen(Limits(), false);
}

TSignedSeqRange CGeneModel::RealCdsLimits() const
{
    TSignedSeqRange cds = MaxCdsLimits();
    if (HasStart()) {
        if (Strand()==ePlus)
            cds.SetFrom(GetCdsInfo().Start().GetFrom());
        else
            cds.SetTo(GetCdsInfo().Start().GetTo());
    }
    return cds;
}

int CGeneModel::RealCdsLen() const
{
    return FShiftedLen(RealCdsLimits(), false);
}

TSignedSeqRange CGeneModel::MaxCdsLimits() const
{
    if (ReadingFrame().Empty())
        return TSignedSeqRange::GetEmpty();

    return GetCdsInfo().MaxCdsLimits() & Limits();
}   

template< class T>
class CStreamState {
#ifdef HAVE_IOS_REGISTER_CALLBACK
public:
    CStreamState(const T& deflt) : m_deflt(deflt), m_index( ios_base::xalloc() ) {}

    T& slot(ios_base& iob)
    {
        void *&p = iob.pword(m_index);
        T *c = static_cast<T*>(p);
        if (c == NULL) {
            p = c = new T(m_deflt);
            iob.register_callback(ios_callback, m_index);
        }
        return *c;
    }

private:
    static void ios_callback(ios_base::event e, ios_base& iob, int index)
    {
        if (e == ios_base::erase_event) {
            delete static_cast<T*>(iob.pword(index));
        } else if (e == ios_base::copyfmt_event) {
            void *&p = iob.pword(index);
            try {
                T *old = static_cast<T*>(p);
                p = new T(*old);
            } catch (...) {
                p = NULL;
            }
        }
    }

    T   m_deflt;
    int m_index;
#else
    // Crude approximation for use by older compilers (notably GCC 2.95).
    // In particular, this implementation has no way to find out when
    // anything happens to the streams it tracks, so it leaks some
    // amount of memory and disregards the possibility that multiple
    // distinct tracked streams have the same address in sequence. :-/
public:
    CStreamState(const T& deflt) : m_deflt(deflt) {}

    T& slot(IOS_BASE& iob)
    {
        TMap::iterator it = m_map.find(&iob);
        if (it == m_map.end()) {
            it = m_map.insert(make_pair(&iob, m_deflt)).first;
        }
        return it->second;
    }

private:
    typedef map<IOS_BASE*, T> TMap;

    T    m_deflt;
    TMap m_map;
#endif
};

int CAlignModel::PolyALen() const {
    if((Status()&ePolyA) == 0)
        return 0;

    TSignedSeqRange lim = GetAlignMap().MapRangeOrigToEdited(GetAlignMap().ShrinkToRealPoints(Limits()),false);
    if((Status()&eReversed) == 0) {
        return TargetLen()-lim.GetTo()-1;
    } else {
        return lim.GetFrom();
    }
}

CStreamState<pair<string,string> > line_buffer(make_pair(kEmptyStr,kEmptyStr));

CNcbiIstream& Getline(CNcbiIstream& is, string& line)
{
    if (!line_buffer.slot(is).first.empty()) {
        line = line_buffer.slot(is).first;
        line_buffer.slot(is).first.erase();
    } else {
        NcbiGetlineEOL(is, line);
    }
    line_buffer.slot(is).second = line;
    return is;
}

void Ungetline(CNcbiIstream& is)
{
    line_buffer.slot(is).first = line_buffer.slot(is).second;
    is.clear();
}

CNcbiIstream& InputError(CNcbiIstream& is)
{
    is.clear();
    ERR_POST( Error << "Input error. Last line: " <<  line_buffer.slot(is).second );
    Ungetline(is);
    is.setstate(ios::failbit);
    return is;
}

CStreamState<string> contig_stream_state(kEmptyStr);

CNcbiOstream& operator<<(CNcbiOstream& s, const setcontig& c)
{
    contig_stream_state.slot(s) = c.m_contig; return s;
}
CNcbiIstream& operator>>(CNcbiIstream& is, const getcontig& c)
{
    c.m_contig = contig_stream_state.slot(is); return is;
}

enum EModelFileFormat { eGnomonFileFormat, eGFF3FileFormat, eASNFileFormat };
NCBI_XALGOGNOMON_EXPORT CNcbiOstream& operator<<(CNcbiOstream& s, EModelFileFormat f);

CStreamState<EModelFileFormat> model_file_format_state(eGFF3FileFormat);

CNcbiOstream& operator<<(CNcbiOstream& s, EModelFileFormat f)
{
    model_file_format_state.slot(s) = f; return s;
}

struct SGFFrec {
    SGFFrec() : start(-1), end(-1), score(BadScore()), strand('.'), phase(-1), model(0), tstart(-1), tend(-2), tstrand('+') {}
    string seqid;
    string source;
    string type;
    int start;
    int end;
    double score;
    char strand;
    int phase;
    Int8 model;
    int tstart;
    int tend;
    char tstrand;
    map<string,string> attributes;

    void print(CNcbiOstream& os) const;
};

CNcbiOstream& operator<<(CNcbiOstream& os, const SGFFrec& r)
{
    r.print(os); return os;
}

namespace {
const char* dot = ".";
}

void SGFFrec::print(CNcbiOstream& os) const
{

    os << (seqid.empty()?dot:seqid) << '\t';
    os << (source.empty()?dot:source) << '\t';
    os << (type.empty()?dot:type) << '\t';
    if(start >= 0)
        os << (start+1) << '\t';
    else
        os << "-\t";
    if(end >= 0)
        os << (end+1) << '\t';
    else
        os << "-\t";
    if (score == BadScore()) os << dot; else os << score; os << '\t';
    os << strand << '\t';
    if (phase < 0) os << dot; else os << phase; os << '\t';

    os << "model=" << model;
    for (map<string,string>::const_iterator i = attributes.begin(); i != attributes.end(); ++i ) {
        if (!i->second.empty())
            os << ';' << i->first << '=' << i->second;
    }

    os << '\n';
}

CNcbiIstream& operator>>(CNcbiIstream& is, SGFFrec& res)
{
    string line;
    do {
        Getline(is, line);
    } while (is && (line.empty() || NStr::StartsWith(line, "#")));
    if (!is) {
        return is;
    }

    vector<string> v;
    NStr::Tokenize(line, "\t", v, NStr::eNoMergeDelims);
    if (v.size()!=9)
        return InputError(is);
    SGFFrec rec;
    try {
        rec.seqid = v[0];
        rec.source = v[1];
        rec.type = v[2];
        rec.start = -1;
        if(v[3] != "-")
            rec.start = NStr::StringToNumeric<int>(v[3])-1;
        rec.end = -1;
        if(v[4] != "-")
            rec.end = NStr::StringToNumeric<int>(v[4])-1;
        rec.score = v[5]==dot?BadScore():NStr::StringToNumeric<double>(v[5]);
        rec.strand = v[6][0];
        rec.phase = v[7]==dot?-1:NStr::StringToNumeric<int>(v[7]);
    } catch (...) {
        return InputError(is);
    }
    bool model_id_present = false;
    vector<string> attributes;
    NStr::Tokenize(v[8], ";", attributes, NStr::eMergeDelims);
    for (size_t i = 0; i < attributes.size(); ++i) {
        string key, value;
        if (NStr::SplitInTwo(attributes[i], "=", key, value) && !value.empty()) {
            if (key == "model") {
                rec.model = NStr::StringToNumeric<Int8>(value);
                model_id_present = true;
            } else if(key == "Target") {
                vector<string> tt;
                NStr::Tokenize(value, " \t", tt);
                rec.tstart = NStr::StringToNumeric<int>(tt[1])-1;
                rec.tend = NStr::StringToNumeric<int>(tt[2])-1;
                if(tt.size() > 3 && tt[3] == "-")
                    rec.tstrand = '-';
                rec.attributes[key] = tt[0];
            } else
                rec.attributes[key] = value;
        }
    }
    if (!model_id_present)
        return InputError(is);
    res = rec;
    return is;
}

string BuildGFF3Gap(int& prev_pos, const CInDelInfo& indel) {
    string gap;
    string status;
    if(indel.GetStatus() == CInDelInfo::eGenomeCorrect)
        status = "c";
    else if(indel.GetStatus() == CInDelInfo::eGenomeNotCorrect)
        status = "n";
    if (prev_pos < indel.Loc())
        gap += " M"+NStr::NumericToString(indel.Loc()-prev_pos);
    if(indel.IsInsertion()) {
        gap += string(" ")+status+"D"+NStr::NumericToString(indel.Len());
    } else if(indel.IsDeletion()) {
        gap += string(" ")+status+"I"+indel.GetInDelV();
    } else {
        gap += string(" ")+status+"R"+indel.GetInDelV();
    }

    prev_pos = indel.InDelEnd();

    return gap;
}

string BuildGFF3Gap(int start, int end, const TInDels& indels)
{
    string gap;

    int prev_pos = start;
    ITERATE (TInDels, indelp, indels) {
        const CInDelInfo& indel = *indelp;
        if(indel.Loc() < start)
            continue;
        if(indel.InDelEnd() > end+1)
            break;

        _ASSERT( indel.IsDeletion() || (start <=indel.Loc() && indel.Loc()+indel.Len()-1 <= end));
        gap += BuildGFF3Gap(prev_pos, indel);        
    }
    if (!gap.empty()) {
        gap.erase(0,1);
        if (prev_pos < end+1)
            gap += " M"+NStr::NumericToString(end+1-prev_pos);
    }

    return gap;
}

void readGFF3Gap(const string& gap, int start, int end, TInDels& indels)
{
    if (gap.empty())
        return;
    vector<string> operations;
    NStr::Tokenize(gap, " ", operations);
    TSignedSeqPos loc = start;
    NON_CONST_ITERATE(vector<string>, o, operations) {
        CInDelInfo::EStatus status = CInDelInfo::eUnknown;
        if((*o)[0] == 'c') {
            status = CInDelInfo::eGenomeCorrect;
            o->erase(o->begin());
        } else if((*o)[0] == 'n') {
            status = CInDelInfo::eGenomeNotCorrect;
            o->erase(o->begin());
        }
        int len = NStr::StringToNumeric<int>(*o,NStr::fConvErr_NoThrow|NStr::fAllowLeadingSymbols);
        if ((*o)[0] == 'M') {
            loc += len;
        } else if ((*o)[0] == 'D') {
            indels.push_back(CInDelInfo(loc,len,CInDelInfo::eIns));
            indels.back().SetStatus(status);
            loc += len;
        } else if ((*o)[0] == 'I') {
            string seq;
            len = o->length()-1;
            seq = o->substr(1);
            indels.push_back(CInDelInfo(loc,len,CInDelInfo::eDel,seq));
            indels.back().SetStatus(status);
        } else {
            _ASSERT((*o)[0] == 'R');
            string seq;
            len = o->length()-1;
            seq = o->substr(1);
            indels.push_back(CInDelInfo(loc,len,CInDelInfo::eMism,seq));
            indels.back().SetStatus(status);
            loc += len;
        }
    }
    _ASSERT( loc == end+1 );
}

string CGeneModel::TypeToString(int type)
{
    if ((type & eGnomon)!=0) return "Gnomon";
    if ((type & eChain)!=0) return  "Chainer";
    if ((type & eProt)!=0) return  "ProSplign";
    if ((type & (eSR|eEST|emRNA|eNotForChaining))!=0) return  "Splign";
    return "Unknown";
}

void CollectAttributes(const CAlignModel& a, map<string,string>& attributes)
{
    attributes["ID"] = NStr::NumericToString(a.ID());
    if (a.GeneID()!=0)
        attributes["Parent"] = "gene"+NStr::NumericToString(a.GeneID());
    if (a.RankInGene()!=0)
        attributes["rankInGene"] = NStr::NumericToString(a.RankInGene());

    ITERATE(CSupportInfoSet, i, a.Support()) {
        attributes["support"] += ",";
        if(i->IsCore()) 
            attributes["support"] += "*";
        
        attributes["support"] += NStr::NumericToString(i->GetId());
    }
    attributes["support"].erase(0,1);

    list<string> tp;
    ITERATE(list< CRef<CSeq_id> >, i, a.TrustedProt()) {
        tp.push_back(CIdHandler::ToString(**i));      
    }
    tp.sort();
    attributes["TrustedProt"] = NStr::Join(tp,",");

    list<string> tm;
    ITERATE(list< CRef<CSeq_id> >, i, a.TrustedmRNA()) {
        tm.push_back(CIdHandler::ToString(**i));      
    }
    tm.sort();
    attributes["TrustedmRNA"] = NStr::Join(tm,",");

    if(a.TargetLen() > 0)
        attributes["TargetLen"] = NStr::NumericToString(a.TargetLen());  

    if(a.Ident() > 0)
        attributes["Ident"] = NStr::DoubleToString(a.Ident());  

    if(a.Weight() > 1)
        attributes["Weight"] = NStr::DoubleToString(a.Weight());  

    if (!a.ProteinHit().empty())
        attributes["protein_hit"] = a.ProteinHit();

    if ((a.Type()  &CGeneModel::eWall)!=0)       attributes["flags"] += ",Wall";
    if ((a.Type()  &CGeneModel::eNested)!=0)     attributes["flags"] += ",Nested";
    if ((a.Type()  &CGeneModel::eNotForChaining)!=0)         attributes["flags"] += ",NotForChaining";
    if ((a.Type()  &CGeneModel::eSR)!=0)         attributes["flags"] += ",SR";
    if ((a.Type()  &CGeneModel::eEST)!=0)        attributes["flags"] += ",EST";
    if ((a.Type()  &CGeneModel::emRNA)!=0)       attributes["flags"] += ",mRNA";
    if ((a.Type()  &CGeneModel::eProt)!=0)       attributes["flags"] += ",Prot";

    if ((a.Status()&CGeneModel::eCap)!=0)      attributes["flags"] += ",Cap";
    if ((a.Status()&CGeneModel::ePolyA)!=0)      attributes["flags"] += ",PolyA";
    if ((a.Status()&CGeneModel::eSkipped)!=0)    attributes["flags"] += ",Skip";
    if ((a.Status()&CGeneModel::eBestPlacement)!=0)    attributes["flags"] += ",BestPlacement";
    if ((a.Status()&CGeneModel::eConsistentCoverage)!=0)    attributes["flags"] += ",ConsistentCoverage";
    if ((a.Status()&CGeneModel::eUnknownOrientation)!=0)    attributes["flags"] += ",UnknownOrientation";
    if ((a.Status()&CGeneModel::eGapFiller)!=0)    attributes["flags"] += ",GapFiller";
    if ((a.Status()&CGeneModel::ecDNAIntrons)!=0)    attributes["flags"] += ",cDNAIntrons";
    if ((a.Status()&CGeneModel::eChangedByFilter)!=0)    attributes["flags"] += ",ChangedByFilter";
    if ((a.Status()&CGeneModel::eTSA)!=0)    attributes["flags"] += ",TSA";

    if (a.GetCdsInfo().ReadingFrame().NotEmpty()) {

        CCDSInfo cds_info = a.GetCdsInfo();
        if(cds_info.IsMappedToGenome()) {    // convert local copy to tcoords
            cds_info = cds_info.MapFromOrigToEdited(a.GetAlignMap());
        }
        
        TSignedSeqRange tlim = a.TranscriptLimits();
        TSignedSeqRange cds = cds_info.Cds();  // cdsinfo already in tcoords
        TSignedSeqRange start = cds_info.Start();
        TSignedSeqRange maxcdslim = tlim & cds_info.MaxCdsLimits();
        TSignedSeqRange protcds = cds_info.ProtReadingFrame();

        TSignedSeqRange rf = cds;
        if(cds_info.HasStart())
            rf.SetFrom(rf.GetFrom()+3);
        if(cds_info.HasStop())
            rf.SetTo(rf.GetTo()-3);

        bool tCoords = false;

        if(start.NotEmpty() && (start.GetFrom() != maxcdslim.GetFrom() && start.GetTo() != maxcdslim.GetTo())) {
            attributes["maxCDS"] = NStr::NumericToString(maxcdslim.GetFrom()+1)+" "+NStr::NumericToString(maxcdslim.GetTo()+1);
            tCoords = true;
        }
        if(protcds.NotEmpty() && protcds != rf) {
            attributes["protCDS"] = NStr::NumericToString(protcds.GetFrom()+1)+" "+NStr::NumericToString(protcds.GetTo()  +1);
            tCoords = true;
        }
        if(cds_info.HasStart()) {
            _ASSERT( !(cds_info.ConfirmedStart() && cds_info.OpenCds()) );
            string adj;
            if (cds_info.ConfirmedStart())
                adj = "Confirmed";
            else if (cds_info.OpenCds())
                adj = "Putative";
            attributes["flags"] += ","+adj+"Start";
        }
        if (cds_info.HasStop()) {
            string adj;
            if (cds_info.ConfirmedStop())
                adj = "Confirmed";
            attributes["flags"] += ","+adj+"Stop";
        }

        if ((a.Status()&CGeneModel::eFullSupCDS)!=0) attributes["flags"] += ",FullSupCDS";
        if ((a.Status()&CGeneModel::ePseudo)!=0)     attributes["flags"] += ",Pseudo";
        TInDels fs = a.GetInDels(true);
        ITERATE(TInDels, indl, fs) {
            if(indl->GetStatus() != CInDelInfo::eGenomeNotCorrect) {
                attributes["flags"] += ",Frameshifts";
                break;
            }
        }
        if(a.isNMD(50))                              attributes["flags"] += ",NMD";

        ITERATE(CCDSInfo::TPStops, s, cds_info.PStops()) {
            TSignedSeqRange pstop = *s;             
            attributes["pstop"] += ","+NStr::NumericToString(pstop.GetFrom()+1)+" "+NStr::NumericToString(pstop.GetTo()+1);
            if(s->m_status == CCDSInfo::eGenomeCorrect)
                attributes["pstop"] += " C";
            else if(s->m_status == CCDSInfo::eGenomeNotCorrect)
                attributes["pstop"] += " N";
            else if(s->m_status == CCDSInfo::eSelenocysteine)
                attributes["pstop"] += " S";
            
            tCoords = true;
        }
        attributes["pstop"].erase(0,1);

        if(tCoords)
           attributes["flags"] += ",tCoords"; 
    }

    attributes["flags"].erase(0,1);

    attributes["note"] = a.GetComment();
}

TSignedSeqRange StringToRange(const string& s)
{
    string start, stop;
    NStr::SplitInTwo(s, " ", start, stop);
    return TSignedSeqRange(NStr::StringToNumeric<TSignedSeqPos>(start)-1,NStr::StringToNumeric<TSignedSeqPos>(stop)-1);
}

void ParseAttributes(map<string,string>& attributes, CAlignModel& a)
{
    bool open_cds = false;
    bool confirmed_start = false;
    bool confirmed_stop = false;
    bool has_start = false;
    bool has_stop = false;
    bool tcoords = false;
    
    vector<string> flags;
    NStr::Tokenize(attributes["flags"], ",", flags);
    ITERATE(vector<string>, f, flags) {
        if (*f == "Wall")       a.SetType(a.Type()|  CGeneModel::eWall);
        else if (*f == "Nested")     a.SetType(a.Type()|  CGeneModel::eNested);
        else if (*f == "NotForChaining")         a.SetType(a.Type()|  CGeneModel::eNotForChaining);
        else if (*f == "SR")         a.SetType(a.Type()|  CGeneModel::eSR);
        else if (*f == "EST")        a.SetType(a.Type()|  CGeneModel::eEST);
        else if (*f == "mRNA")       a.SetType(a.Type()|  CGeneModel::emRNA);
        else if (*f == "Prot")       a.SetType(a.Type()|  CGeneModel::eProt);
        else if (*f == "Skip")       a.Status()        |= CGeneModel::eSkipped;
        else if (*f == "FullSupCDS") a.Status()        |= CGeneModel::eFullSupCDS;
        else if (*f == "Pseudo")     a.Status()        |= CGeneModel::ePseudo;
        else if (*f == "PolyA")      a.Status()        |= CGeneModel::ePolyA;
        else if (*f == "Cap")        a.Status()        |= CGeneModel::eCap;
        else if (*f == "BestPlacement") a.Status()     |= CGeneModel::eBestPlacement;
        else if (*f == "ConsistentCoverage") a.Status()     |= CGeneModel::eConsistentCoverage;
        else if (*f == "UnknownOrientation") a.Status()|= CGeneModel::eUnknownOrientation;
        else if (*f == "GapFiller") a.Status()|= CGeneModel::eGapFiller;
        else if (*f == "cDNAIntrons") a.Status()       |= CGeneModel::ecDNAIntrons;
        else if (*f == "TSA") a.Status()       |= CGeneModel::eTSA;
        else if (*f == "ChangedByFilter") a.Status()       |= CGeneModel::eChangedByFilter;
        else if (*f == "ConfirmedStart")   { confirmed_start = true; has_start = true; }
        else if (*f == "PutativeStart")   { open_cds = true; has_start = true; }
        else if (*f == "Start") has_start = true;
        else if (*f == "ConfirmedStop")   { confirmed_stop = true; has_stop = true; }
        else if (*f == "Stop")  has_stop = true;
        else if (*f == "tCoords") tcoords = true;
    }

    if (NStr::StartsWith(attributes["Parent"],"gene"))
        a.SetGeneID(NStr::StringToNumeric<Int8>(attributes["Parent"],NStr::fConvErr_NoThrow|NStr::fAllowLeadingSymbols));
    if (!attributes["rankInGene"].empty()) {
        a.SetRankInGene(NStr::StringToNumeric<int>(attributes["rankInGene"]));
    }

    if (!attributes["Target"].empty()) {
        string target = NStr::Replace(attributes["Target"], "%20", " ");

        CRef<CSeq_id> target_id;
        try {
            target_id = CIdHandler::ToSeq_id(target);
        } catch(CException) {
            if(((a.Type() & CGeneModel::eGnomon) != 0 || (a.Type() & CGeneModel::eChain) != 0) && target.substr(0,4) == "hmm.") { // handles legacy files
                target = "gnl|GNOMON|"+target.substr(4);
            } else if((a.Type() & CGeneModel::eProt) != 0 && target.length() == 6 && isalpha(target[0])) {    // probably PIR
                target = "pir||"+target;
            }
            target_id = CIdHandler::ToSeq_id(target);
        }
        a.SetTargetId(*target_id);
    }

    if((a.Type() & CGeneModel::eGnomon) != 0 || (a.Type() & CGeneModel::eChain) != 0) { // handles legacy files
        vector<string> support;
        NStr::Tokenize(attributes["support"], ",", support);
        ITERATE(vector<string>, s, support) {
            bool core = (*s)[0] == '*';
            Int8 id = NStr::StringToNumeric<Int8>(core ? s->substr(1) : *s);
            a.AddSupport(CSupportInfo(id, core));
        }
    }
    
    vector<string> trustedmrna;
    NStr::Tokenize(attributes["TrustedmRNA"], ",", trustedmrna);
    ITERATE(vector<string>, s, trustedmrna) {
        a.InsertTrustedmRNA(CIdHandler::ToSeq_id(*s));
    }

    vector<string> trustedprot;
    NStr::Tokenize(attributes["TrustedProt"], ",", trustedprot);
    ITERATE(vector<string>, s, trustedprot) {
        a.InsertTrustedProt(CIdHandler::ToSeq_id(*s));
    }
    
    a.SetComment(attributes["note"]);
    
    if (!attributes["Ident"].empty())
        a.SetIdent(NStr::StringToNumeric<double>(attributes["Ident"]));
    
    if (!attributes["Weight"].empty())
        a.SetWeight(NStr::StringToNumeric<double>(attributes["Weight"]));
    
    if (!attributes["protein_hit"].empty())
        a.ProteinHit() = attributes["protein_hit"];
    
    CCDSInfo cds_info = a.GetCdsInfo();
    TSignedSeqRange reading_frame = cds_info.ReadingFrame();

    if (reading_frame.NotEmpty()) {

        if (!attributes["protCDS"].empty()) {
            TSignedSeqRange protcds = StringToRange(attributes["protCDS"]);
            if(!tcoords)
                protcds = a.GetAlignMap().MapRangeOrigToEdited(protcds, false);
            cds_info.SetReadingFrame(protcds, true );
        }
    
        bool reversed = (a.Status()&CGeneModel::eReversed) != 0;

        if(reversed)
            swap(has_start, has_stop);
        
        TSignedSeqRange start, stop;

        if(has_start) {
            start = TSignedSeqRange(reading_frame.GetFrom(),reading_frame.GetFrom()+2);
            reading_frame.SetFrom(reading_frame.GetFrom()+3);
        }
        
        if(has_stop) {
            stop = TSignedSeqRange(reading_frame.GetTo()-2,reading_frame.GetTo());
            reading_frame.SetTo(reading_frame.GetTo()-3);
        }
        
        if(reversed) {
            swap(has_start, has_stop);
            swap(start, stop);
        }    
           
        cds_info.SetReadingFrame(reading_frame, (a.Type()&CGeneModel::eProt)!=0 && cds_info.ProtReadingFrame().Empty());
        cds_info.SetStart(start,confirmed_start);
        
        cds_info.SetStop(stop,confirmed_stop);
        
        if(!attributes["pstop"].empty()) {
            vector<string> stops;
            NStr::Tokenize(attributes["pstop"], ",", stops);
            ITERATE(vector<string>, s, stops) {
                vector<string> parts;
                NStr::Tokenize(*s, " ", parts);
                TSignedSeqRange pstop = StringToRange(parts[0]+" "+parts[1]);
                if(!tcoords)
                    pstop = a.GetAlignMap().MapRangeOrigToEdited(pstop, false);
                CCDSInfo::EStatus status = CCDSInfo::eUnknown;
                if(parts.size() > 2) {
                    if(parts[2] == "C")
                        status = CCDSInfo::eGenomeCorrect;
                    else if(parts[2] == "N")
                        status = CCDSInfo::eGenomeNotCorrect;
                    else if(parts[2] == "S")
                        status = CCDSInfo::eSelenocysteine;
                    else
                        _ASSERT(false);
                }
                cds_info.AddPStop(pstop, status);
            }
        }

        TSignedSeqRange max_cds_limits;
        TSignedSeqRange tlim = a.TranscriptLimits();
        if (attributes["maxCDS"].empty()) {
            max_cds_limits = tlim;
            if(reversed)
                swap(start, stop); 
            if(start.NotEmpty())
                max_cds_limits.SetFrom(start.GetFrom());
            if(stop.NotEmpty())
                max_cds_limits.SetTo(stop.GetTo());
            if(reversed)
                swap(start, stop); 
        } else {
            max_cds_limits = StringToRange(attributes["maxCDS"]);
            if(!tcoords)
                max_cds_limits = a.GetAlignMap().MapRangeOrigToEdited(max_cds_limits, false);
        }

        if(reversed) {
            if(max_cds_limits.GetTo() < tlim.GetTo())
                cds_info.Set5PrimeCdsLimit(max_cds_limits.GetTo());
        } else {
            if(max_cds_limits.GetFrom() > tlim.GetFrom())
                cds_info.Set5PrimeCdsLimit(max_cds_limits.GetFrom());
        }
    }
    
    cds_info.SetScore(cds_info.Score(), open_cds);
    
    a.SetCdsInfo(cds_info);
}

CNcbiOstream& printGFF3(CNcbiOstream& os, CAlignModel a)
{
    CCDSInfo cds_info = a.GetCdsInfo();
    if(cds_info.IsMappedToGenome()) {    // convert local copy to tcoords
        cds_info = cds_info.MapFromOrigToEdited(a.GetAlignMap());
        a.SetCdsInfo(cds_info);
    }

    SGFFrec templ;
    templ.model = a.ID();
    templ.seqid = contig_stream_state.slot(os);

    templ.source = CGeneModel::TypeToString(a.Type());

    if(a.Strand() == ePlus)
        templ.strand = '+';
    else if(a.Strand() == eMinus)
        templ.strand = '-';

    SGFFrec mrna = templ;
    mrna.type = "mRNA";
    mrna.start = a.Limits().GetFrom();
    mrna.end = a.Limits().GetTo();
    mrna.score = a.Score();

    CollectAttributes(a, mrna.attributes);

    os << mrna;

    int part = 0;
    vector<SGFFrec> exons,cdss;

    templ.attributes["Parent"] = NStr::NumericToString(a.ID());
    SGFFrec exon = templ;
    exon.type = "exon";
    SGFFrec cds = templ;
    cds.type = "CDS";

    TSignedSeqRange tlim = a.TranscriptLimits();
    TSignedSeqRange rcds = tlim & cds_info.MaxCdsLimits();   // realcdslimits in tcoords
    if(cds_info.HasStart()) {
        if(a.Status()&CGeneModel::eReversed)
            rcds.SetTo(cds_info.Start().GetTo());
        else
            rcds.SetFrom(cds_info.Start().GetFrom());
    }

    int phase = 0;
    if(cds_info.ReadingFrame().NotEmpty() && ((a.Strand() == ePlus && !cds_info.HasStart()) || (a.Strand() == eMinus && !cds_info.HasStop()))) {   // may have extra bases on the left end
        phase = (a.Orientation() == ePlus) ? cds_info.ReadingFrame().GetFrom() - tlim.GetFrom() : tlim.GetTo()-cds_info.ReadingFrame().GetTo();
        _ASSERT(phase < 3);
    }

    TInDels corrections = a.FrameShifts();
    for(unsigned int i = 0; i < a.Exons().size(); ++i) {
        const CModelExon *e = &a.Exons()[i]; 

        if(e->Limits().NotEmpty()) {
            exon.start = e->GetFrom();
            exon.end = e->GetTo();
            exon.attributes["Gap"] = BuildGFF3Gap(exon.start, exon.end, corrections);
        } else {
            exon.start = -1;
            exon.end = -1;
            exon.attributes["Seq"] = e->m_seq;
            if(e->m_source.m_range.NotEmpty()) {
                exon.attributes["Source"] = e->m_source.m_acc + ":"+NStr::NumericToString(e->m_source.m_range.GetFrom()+1) + ":"+NStr::NumericToString(e->m_source.m_range.GetTo()+1) + ":";
                if(e->m_source.m_strand == ePlus)
                    exon.attributes["Source"] += "+";
                else
                    exon.attributes["Source"] += "-";
            }
        }

        string targetid = NStr::Replace(CIdHandler::ToString(*a.GetTargetId()), " ", "%20");
        
        TSignedSeqRange transcript_exon = a.TranscriptExon(i);
        string target = " "+NStr::NumericToString(transcript_exon.GetFrom()+1)+" "+NStr::NumericToString(transcript_exon.GetTo()+1);
        if((a.Status() & CGeneModel::eReversed)!=0) {
            target += " -";
        } else {
            target += " +";
        }
        exon.attributes["Target"] = targetid+target;

        if(e->m_ident > 0) {
            exon.attributes["Ident"] = NStr::DoubleToString(e->m_ident);
        }

        if(!e->m_fsplice_sig.empty() || !e->m_ssplice_sig.empty()) {
            string fs;
            if(!e->m_fsplice_sig.empty())
                fs = e->m_fsplice_sig;
            string ss;
            if(!e->m_ssplice_sig.empty())
                ss = e->m_ssplice_sig;
            if(a.Strand() == ePlus) {
                exon.attributes["Splices"] = fs+".."+ss;
            } else {
                exon.attributes["Splices"] = ss+".."+fs;
            }
        }

        exons.push_back(exon);
        exon.attributes["Gap"].erase();
        exon.attributes["Ident"].erase();
        exon.attributes["Splices"].erase();
        exon.attributes["Seq"].erase();
        exon.attributes["Source"].erase();

        TSignedSeqRange tcds = transcript_exon & rcds;
        if(tcds.NotEmpty()) {
            cds.tstart = tcds.GetFrom();
            cds.tend = tcds.GetTo();
            cds.start = -1;
            cds.end = -1;
            TSignedSeqRange cds_limits = a.GetAlignMap().MapRangeEditedToOrig(tcds, true);
            if(cds_limits.NotEmpty()) {
                cds.start = cds_limits.GetFrom();
                cds.end = cds_limits.GetTo();
            }
            string ctarget = " "+NStr::NumericToString(tcds.GetFrom()+1)+" "+NStr::NumericToString(tcds.GetTo()+1);
            if((a.Status() & CGeneModel::eReversed)!=0) {
                ctarget += " -";
            } else {
                ctarget += " +";
            }
            cds.attributes["Target"] = targetid+ctarget;
            cdss.push_back(cds);
        }

        if (!e->m_ssplice) {
            string suffix;
            if (!a.Continuous()) {
                SGFFrec rec = templ;
                suffix= "."+NStr::NumericToString(++part);
                if(exons.front().start >= 0) {
                    rec.start = exons.front().start;
                } else {
                    _ASSERT(exons.size() > 1 && exons[1].start >= 0);
                    rec.start = exons[1].start;
                }
                if(exons.back().end >= 0) {
                    rec.end = exons.back().end;
                } else {
                    _ASSERT(exons.size() > 1 && exons[exons.size()-2].end >= 0);
                    rec.end = exons[exons.size()-2].end;
                }
                rec.type = "mRNA_part";
                rec.attributes["ID"] = rec.attributes["Parent"]+suffix;
                os << rec;
            }
            NON_CONST_ITERATE(vector<SGFFrec>, e, exons) {
                e->attributes["Parent"] += suffix;
                os << *e;
            }
            exons.clear();

            NON_CONST_ITERATE(vector<SGFFrec>, e, cdss) {
                if (a.Strand()==ePlus)
                    e->phase = phase;

                phase = (e->tend - e->tstart+1 - phase)%3;
                if (a.Strand()==eMinus)
                    e->phase = phase;
                phase = (3-phase)%3;

                e->attributes["Parent"] += suffix;
                os << *e;
            }
            cdss.clear();
        }
    }
    
    return os;
}

struct Precedence : public binary_function<TSignedSeqRange, TSignedSeqRange, bool>
{
    bool operator()(const TSignedSeqRange& __x, const TSignedSeqRange& __y) const
    { return Precede( __x, __y ); }
};

TSignedSeqRange operator- (TSignedSeqRange a, TSignedSeqRange b)
{
    b &= a;
    if (b.Empty())
        return a;
    if (a.GetFrom()==b.GetFrom())
        a.SetFrom(b.GetTo()+1);
    else if (a.GetTo()==b.GetTo())
        a.SetTo(b.GetFrom()-1);
    return a;
}


CNcbiIstream& readGFF3(CNcbiIstream& is, CAlignModel& align)
{
    SGFFrec rec;
    is >> rec;
    if (!is) {
         return is;
    }

    Int8 id = rec.model;

    if (id==0)
        return InputError(is);

    vector<SGFFrec> recs;

    do {
        recs.push_back(rec);
    } while (is >> rec && rec.seqid == recs.front().seqid && rec.model == id);

    Ungetline(is);
    
    CGeneModel a;

    a.SetID(id);
#ifdef _DEBUG
    a.oid = id;
#endif

    TSignedSeqRange cds;
    int phase = 0;
    bool tcoords = false;

    vector<CModelExon> exons;
    vector<TSignedSeqRange> transcript_exons;
    TInDels indels;
    set<TSignedSeqRange,Precedence> mrna_parts;

    rec = recs.front();

    if (rec.source == "Gnomon") a.SetType(a.Type() | CGeneModel::eGnomon);
    else if (rec.source == "Chainer") a.SetType(a.Type() | CGeneModel::eChain);
    
    if(rec.strand=='+')
        a.SetStrand(ePlus);
    else if(rec.strand=='-')
        a.SetStrand(eMinus);

    double score = BadScore();
    map<string, string> attributes;

    NON_CONST_ITERATE(vector<SGFFrec>, r, recs) {
        if (r->type == "mRNA") {
            attributes = r->attributes;
            score = r->score;
        } else if (r->type == "exon") {
            if(r->tstart >= 0 && r->tstrand == '-') {
                a.Status() |= CGeneModel::eReversed;
            }

            string fs, ss;
            if(!r->attributes["Splices"].empty()) {
                string splices = r->attributes["Splices"];
                unsigned int ispl = splices.find("..");
                if(ispl != string::npos) {
                    if(ispl == 2)
                        fs = splices.substr(0,2);
                    if(ispl+4 == splices.length())
                        ss = splices.substr(ispl+2,2);
                }
                if(a.Strand() == eMinus)
                    swap(fs,ss);
            }

            double eident = 0;
            if(!r->attributes["Ident"].empty()) {
                eident = NStr::StringToNumeric<double>(r->attributes["Ident"]);
            }
            
            if(r->start >= 0 && r->end >= 0)
                exons.push_back(CModelExon(r->start,r->end,false,false,fs,ss,eident));
            else {
                CInDelInfo::SSource src;
                if(!r->attributes["Source"].empty()) {
                    vector<string> v;
                    NStr::Tokenize(r->attributes["Source"], ":", v);
                    _ASSERT((int)v.size() == 4);
                    src.m_acc = v[0];
                    src.m_range.SetFrom(NStr::StringToNumeric<TSignedSeqPos>(v[1])-1);
                    src.m_range.SetTo(NStr::StringToNumeric<TSignedSeqPos>(v[2])-1);
                    if(v[3] == "+")
                        src.m_strand = ePlus;
                    else
                        src.m_strand = eMinus;
               }
                exons.push_back(CModelExon(1,-1,false,false,fs,ss,eident,r->attributes["Seq"],src));
            }
            TSignedSeqRange texon(r->tstart,r->tend);
            if(texon.NotEmpty())
                transcript_exons.push_back(texon);
            readGFF3Gap(r->attributes["Gap"],r->start,r->end, indels);
            if(!r->attributes["Target"].empty())
                attributes["Target"] = r->attributes["Target"];
        } else if (r->type == "CDS") {
            _ASSERT((a.Status()&CGeneModel::eUnknownOrientation) == 0);

            if (r->strand=='+') {
                if (cds.Empty())
                    phase = r->phase;
            } else {
                phase = r->phase;
            }
            if(r->tstart < 0) {   // old format before ggaps
                TSignedSeqRange cds_exon(r->start,r->end);
                cds.CombineWith(cds_exon);
                tcoords = false;
            } else {
                TSignedSeqRange cds_exon(r->tstart,r->tend);
                cds.CombineWith(cds_exon);
                tcoords = true;                
            }
        } else if (r->type == "mRNA_part") {
            mrna_parts.insert(TSignedSeqRange(r->start,r->end));
        }
    }
    
    for(int i = 0; i < (int)exons.size(); ++i) {
        const CModelExon& e = exons[i];
        if (a.Limits().IntersectingWith(e.Limits())) {
            return  InputError(is);
        }
        if(i > 0 && exons[i-1].Limits().NotEmpty()) {    // there can't be ggap adjacent to 'hole'
            set<TSignedSeqRange,Precedence>::iterator p = mrna_parts.lower_bound(e);
            if (p != mrna_parts.end() && p->GetFrom()==e.Limits().GetFrom())
                a.AddHole();
        }
        a.AddExon(e.Limits(),e.m_fsplice_sig,e.m_ssplice_sig,e.m_ident,e.m_seq,e.m_source);
    }

    sort(indels.begin(),indels.end());
    CAlignMap amap;
    if(!transcript_exons.empty())
        amap = CAlignMap(a.Exons(), transcript_exons, indels, a.Orientation(), NStr::StringToNumeric<int>(attributes["TargetLen"]));
    else
        amap = CAlignMap(a.Exons(), indels, a.Strand());

    a.FrameShifts() = indels;
    align = CAlignModel(a, amap);

    TSignedSeqRange reading_frame;
    CCDSInfo cds_info;

    if (cds.NotEmpty()) {
        cds_info = CCDSInfo(false);

        //        align.FrameShifts() = amap.GetInDels(true);

        TSignedSeqRange rf = cds;
        if(!tcoords)
            rf = amap.MapRangeOrigToEdited(cds, false);

        if(!(a.Status() & CGeneModel::eReversed)) {
            rf.SetFrom(rf.GetFrom()+phase);
            rf.SetTo(rf.GetTo()-rf.GetLength()%3);
        } else {
            rf.SetTo(rf.GetTo()-phase);
            rf.SetFrom(rf.GetFrom()+rf.GetLength()%3);
        }

        cds_info.SetReadingFrame(rf, false);
    }
        
    align.SetCdsInfo(cds_info);

    ParseAttributes(attributes, align);

    if(cds.NotEmpty()) {
        CCDSInfo cds_info_t = align.GetCdsInfo();
        cds_info_t.SetScore(score, cds_info_t.OpenCds());
        CCDSInfo cds_info_g = cds_info_t.MapFromEditedToOrig(amap);
        if(cds_info_g.ReadingFrame().NotEmpty())   // successful projection
            align.SetCdsInfo(cds_info_g);
        else
            align.SetCdsInfo(cds_info_t);
    }

    contig_stream_state.slot(is) = rec.seqid;
    return is;
}

CNcbiIstream& readGnomon(CNcbiIstream& is, CGeneModel& align)
{
    is.setstate(ios::failbit);
    return is;
}

CNcbiOstream& printASN(CNcbiOstream& os, const CGeneModel& align)
{
    os.setstate(ios::failbit);
    return os;
}

CNcbiIstream& readASN(CNcbiIstream& is, CGeneModel& align)
{
    is.setstate(ios::failbit);
    return is;
}

CNcbiOstream& operator<<(CNcbiOstream& s, const CGeneModel& a)
{
    return operator<<(s, CAlignModel(a, a.GetAlignMap()));
}

CNcbiOstream& operator<<(CNcbiOstream& os, const CAlignModel& a)
{
    switch (model_file_format_state.slot(os)) {
    case eGnomonFileFormat:
        break;
    case eGFF3FileFormat:
        return printGFF3(os,a);
    case eASNFileFormat:
        break;
    default:
        break;
    }

    os.setstate(ios::failbit);
    return os;
}

CNcbiIstream& operator>>(CNcbiIstream& is, CAlignModel& align)
{
    switch (model_file_format_state.slot(is)) {
    case eGFF3FileFormat:
        return readGFF3(is, align);
    case eGnomonFileFormat:
        return readGnomon(is, align);
    case eASNFileFormat:
        return readASN(is, align);
    default:
        is.setstate(ios::failbit);
        break;
    }
    return is;
}


CSupportInfo::CSupportInfo(Int8 model_id, bool core)
    : m_id( model_id), m_core_align(core)
{
}

Int8 CSupportInfo::GetId() const
{
    return m_id;
}

void CSupportInfo::SetCore(bool core)
{
    m_core_align = core;
}

bool CSupportInfo::IsCore() const
{
    return m_core_align;
}

bool CSupportInfo::operator==(const CSupportInfo& s) const
{
    return IsCore() == s.IsCore() && GetId() == s.GetId();
}

bool CSupportInfo::operator<(const CSupportInfo& s) const
{
    return GetId() == s.GetId() ? IsCore() < s.IsCore() : GetId() < s.GetId();
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
