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
#include <set>
#include <functional>
#include <corelib/ncbiutil.hpp>
#include <objects/general/Object_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

USING_SCOPE(objects);


CRef<CSeq_id> CreateSeqid(const string& name, int score_func(const CRef<CSeq_id>&))
{
    int int_id = NStr::StringToInt(name, NStr::fConvErr_NoThrow);
    if (int_id) {
        return CreateSeqid(int_id);
    }

    CRef<CSeq_id> ps;
    
    try {
        CBioseq::TId ids;
        CSeq_id::ParseFastaIds(ids, name);
        ps = FindBestChoice(ids, score_func); 

        if(!ps) ps = new CSeq_id(CSeq_id::e_Local, name);
    } catch(CException) {
        ps.Reset (new CSeq_id(CSeq_id::e_Local, name)); 
    }
    return ps;
}

CRef<CSeq_id> CreateSeqid(int local_id)
{
    CRef<CSeq_id> ps(new CSeq_id(CSeq_id::e_Local, local_id)); 
    return ps;
}

string CGeneModel::SupportName() const {
    ITERATE(CSupportInfoSet, i, Support()) {
        if (!i->Seqid()->IsLocal() || !i->Seqid()->GetLocal().IsId())
            return i->Seqid()->GetSeqIdString(true);
    }
    return Support().begin()->Seqid()->GetSeqIdString(true);
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
        m_p_stops==a.m_p_stops &&
        m_open==a.m_open &&
        m_score==a.m_score;
}

void CCDSInfo::SetReadingFrame(TSignedSeqRange r, bool protein)
{
    if (r.Empty())
        Clear();
    else {
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
    if (confirmed)
        m_confirmed_start = true;
    else if (m_confirmed_start && r != m_start) {
        m_confirmed_start = false;
    }
    m_start = r;
    _ASSERT( Invariant() );
}

void CCDSInfo::SetStop(TSignedSeqRange r)
{
    m_stop = r;
    if (r.NotEmpty()) {
        if (Precede(m_reading_frame,r))
            m_max_cds_limits.SetTo(r.GetTo());
        else
            m_max_cds_limits.SetFrom(r.GetFrom());
    }
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

void CCDSInfo::AddPStop(TSignedSeqRange r)
{
    m_p_stops.push_back(r);
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
        m_reading_frame = mapper(m_reading_frame, false);
    if(m_reading_frame_from_proteins.NotEmpty())
        m_reading_frame_from_proteins = mapper(m_reading_frame_from_proteins, false);
    if(m_max_cds_limits.NotEmpty())
        m_max_cds_limits = mapper(m_max_cds_limits, false);
    NON_CONST_ITERATE( vector<TSignedSeqRange>, s, m_p_stops) {
        *s = mapper(*s, false);
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

void CCDSInfo::Clear()
{
    m_start = m_stop = m_reading_frame = m_reading_frame_from_proteins = m_max_cds_limits = TSignedSeqRange::GetEmpty();
    m_confirmed_start = false;
    m_p_stops.clear();
    SetScore( BadScore(), false );
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
                FrameShifts().push_back(CFrameShiftInfo(loc, len, true));
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

bool CGeneModel::GoodEnoughToBeAlternative(int minCdsLen, int maxcomposite) const
{
    int composite = 0;
    ITERATE(CSupportInfoSet, s, Support()) {
        if(s->CoreAlignment() && ++composite > maxcomposite) return false;
    }
    
    return
        GoodEnoughToBeAnnotation(minCdsLen) &&
        !PStop() && FrameShifts().empty() &&
        !isNMD();
}


bool CGeneModel::CdsInvariant(bool check_start_stop) const
{
    _ASSERT( !(ConfirmedStart() && OpenCds()) );

    if (ReadingFrame().Empty())
        return true;

    _ASSERT( Include(MaxCdsLimits(),RealCdsLimits()) );

#ifdef _DEBUG
    CFrameShiftedSeqMap mrnamap(*this);
#endif


    if (check_start_stop && Score() != BadScore()) {
        if (Strand()==ePlus) {
            _ASSERT( mrnamap.FShiftedLen(Limits().GetFrom(),ReadingFrame().GetFrom(), false) < 4 ^ HasStart() );
            _ASSERT( mrnamap.FShiftedLen(ReadingFrame().GetTo(),Limits().GetTo(), false) < 4 ^ HasStop() );
        } else {
            _ASSERT( mrnamap.FShiftedLen(ReadingFrame().GetTo(),Limits().GetTo(), false) < 4 ^ HasStart() );
            _ASSERT( mrnamap.FShiftedLen(Limits().GetFrom(),ReadingFrame().GetFrom(), false) < 4 ^ HasStop() );
        }
    }

    _ASSERT( !HasStart() || mrnamap.FShiftedLen(GetCdsInfo().Start(), false)==3 );
    _ASSERT( mrnamap.FShiftedLen(ReadingFrame(), false)%3==0 );
    _ASSERT( !HasStop() || mrnamap.FShiftedLen(GetCdsInfo().Stop(), false)==3 );

    _ASSERT( GetCdsInfo().ProtReadingFrame().Empty() ||
             mrnamap.FShiftedLen(GetCdsInfo().ProtReadingFrame(), false)%3==0 );

    if (GetCdsInfo().MaxCdsLimits().GetFrom() < Limits().GetFrom())
        _ASSERT( GetCdsInfo().MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom() );
    if (Limits().GetTo() < GetCdsInfo().MaxCdsLimits().GetTo())
        _ASSERT( GetCdsInfo().MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo() );
    
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
    _ASSERT( cds_info.ReadingFrame().Empty() || CFrameShiftedSeqMap(*this).FShiftedLen(cds_info.ReadingFrame().GetFrom(), cds_info.ReadingFrame().GetTo(), false)%3==0 );

    m_cds_info.CombineWith(cds_info);

    _ASSERT( !ensure_cds_invariant || CdsInvariant(false) );
}

void CGeneModel::CutExons(TSignedSeqRange hole)
{
    for(size_t i = 0; i < MyExons().size(); ++i) {
        TSignedSeqRange intersection = MyExons()[i].Limits() & hole;
        if (intersection.Empty())
            continue;
        if (MyExons()[i].GetFrom()<hole.GetFrom()) {
            MyExons()[i].Limits().SetTo(hole.GetFrom()-1);
            if (i+1<MyExons().size())
                MyExons()[i+1].m_fsplice=false;
        } else if (hole.GetTo()<MyExons()[i].GetTo()) {
            MyExons()[i].Limits().SetFrom(hole.GetTo()+1);
            if (0<i)
                MyExons()[i-1].m_ssplice=false;
        } else {
            if (0<i)
                MyExons()[i-1].m_ssplice=false;
            if (i+1<MyExons().size())
                MyExons()[i+1].m_fsplice=false;
            MyExons().erase( MyExons().begin()+i);
            --i;
        }
    }
    RemoveExtraFShifts();
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

        CFrameShiftedSeqMap mrnamap(*this);

        if (RealCdsLimits().GetFrom() < cds_clip_limits.GetFrom() && cds_clip_limits.GetFrom() < ReadingFrame().GetFrom())
            cds_clip_limits.SetFrom(ReadingFrame().GetFrom());
        else if (ReadingFrame().GetFrom() < cds_clip_limits.GetFrom() && cds_clip_limits.GetFrom() <= RealCdsLimits().GetTo()) {
            TSignedSeqRange tmp = mrnamap.ShrinkToRealPoints(TSignedSeqRange(cds_clip_limits.GetFrom(), ReadingFrame().GetTo()));
            _ASSERT(tmp.GetTo() == ReadingFrame().GetTo());
            int del = mrnamap.FShiftedLen(tmp, false)%3;
            cds_clip_limits.SetFrom(mrnamap.FShiftedMove(tmp.GetFrom(), del));
            _ASSERT(mrnamap.FShiftedLen(cds_clip_limits.GetFrom(), ReadingFrame().GetTo(), false)%3==0 );
            if (cds_clip_limits.GetFrom() >= ReadingFrame().GetTo())
                cds_clip_limits = TSignedSeqRange::GetEmpty();
        }

        if (RealCdsLimits().GetFrom() <= cds_clip_limits.GetTo() && cds_clip_limits.GetTo() < ReadingFrame().GetTo()) {
            TSignedSeqRange tmp = mrnamap.ShrinkToRealPoints(TSignedSeqRange(ReadingFrame().GetFrom(),cds_clip_limits.GetTo()));
            _ASSERT(tmp.GetFrom() == ReadingFrame().GetFrom());
            int del = mrnamap.FShiftedLen(tmp, false)%3;
            cds_clip_limits.SetTo(mrnamap.FShiftedMove(tmp.GetTo(), -del));
            _ASSERT(mrnamap.FShiftedLen(ReadingFrame().GetFrom(),cds_clip_limits.GetTo(), false)%3==0 );
            if (ReadingFrame().GetFrom() >= cds_clip_limits.GetTo())
                cds_clip_limits = TSignedSeqRange::GetEmpty();
        } else if (ReadingFrame().GetTo() < cds_clip_limits.GetTo() && cds_clip_limits.GetTo() < RealCdsLimits().GetTo())
            cds_clip_limits.SetTo(ReadingFrame().GetTo());

        m_cds_info.Clip(cds_clip_limits);
    }

    for (TExons::iterator e = MyExons().begin(); e != MyExons().end();) {
        TSignedSeqRange clip = e->Limits() & clip_limits;
        if (clip.NotEmpty())
            e++->Limits() = clip;
        else if (mode == eRemoveExons)
            e = MyExons().erase(e);
        else
            ++e;
    }

    if (Exons().size()>0) {
        MyExons().front().m_fsplice = false;
        MyExons().back().m_ssplice = false;
    }

    RecalculateLimits();
    RemoveExtraFShifts();
    
    _ASSERT( CdsInvariant(ensure_cds_invariant) );
}

void CGeneModel::RemoveExtraFShifts()
{
    for(TFrameShifts::iterator i = FrameShifts().begin(); i != FrameShifts().end();) {
        bool belongs = false;
        ITERATE(TExons, e, Exons()) {
            CModelExon exon = *e;
            if(!exon.m_fsplice)     // no flanking deletions
                exon.AddFrom(1);
            if(!exon.m_ssplice)
                exon.AddTo(-1);
            if (i->IntersectingWith(exon.GetFrom(),exon.GetTo())) {
                belongs = true;
                break;
            }
        }
        if (belongs)
            ++i;
        else
            i = FrameShifts().erase(i);
    }
}


TFrameShifts  CGeneModel::FrameShifts(TSignedSeqPos a, TSignedSeqPos b) const
{
    TFrameShifts fshifts;

    ITERATE(TFrameShifts, i, m_fshifts) {
        if(i->IntersectingWith(a,b))
            fshifts.push_back( *i );
    }
        
    return fshifts;
}

int CGeneModel::FShiftedLen(TSignedSeqPos a, TSignedSeqPos b, bool withextras) const
{
    return FShiftedLen(TSignedSeqRange(a,b),withextras);
}


int CGeneModel::FShiftedLen(TSignedSeqRange ab, bool withextras) const
{
    if (ab.Empty())
        return 0;

    _ASSERT(Include(Limits(),ab));

    CFrameShiftedSeqMap mrnamap(*this);
    return mrnamap.FShiftedLen(ab, withextras);
}

TSignedSeqPos CGeneModel::FShiftedMove(TSignedSeqPos pos, int len) const
{
    CFrameShiftedSeqMap mrnamap(*this);

    return mrnamap.FShiftedMove(pos, len);
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

    _ASSERT( b.Strand() == a.Strand() );

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

    auto_ptr<CFrameShiftedSeqMap> pmapa(0), pmapb(0);

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
                    pmapa.reset(new CFrameShiftedSeqMap(a));
                    pmapb.reset(new CFrameShiftedSeqMap(b));
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

void CGeneModel::AddExon(TSignedSeqRange exon_range)
{
    _ASSERT( (m_range & exon_range).Empty() );
    m_range += exon_range;

    CModelExon e(exon_range.GetFrom(),exon_range.GetTo());
    if (MyExons().empty())
        MyExons().push_back(e);
    else if (MyExons().back().GetTo()<exon_range.GetFrom()) {
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

void CAlignModel::CutTranscriptExons(TSignedSeqRange hole) {

    _ASSERT(!MyTranscriptExons().empty());

    bool notreversed = (Status()&CGeneModel::eReversed) == 0;
    bool plusstrand = Strand() == ePlus;
    EStrand orientation = (notreversed == plusstrand) ? ePlus : eMinus;

    CAlignModel::TTranscriptExons new_transcript_exons;
    ITERATE(CAlignModel::TTranscriptExons, e, MyTranscriptExons()) {
        if(e->GetTo() < hole.GetFrom() || e->GetFrom() > hole.GetTo()) {  //not overlapping
            new_transcript_exons.push_back(*e);
        } else if(orientation == ePlus) {
            if(e->GetFrom() < hole.GetFrom())
                new_transcript_exons.push_back(CTranscriptExon(e->GetFrom(),hole.GetFrom()-1));
            if(e->GetTo() > hole.GetTo())
                new_transcript_exons.push_back(CTranscriptExon(hole.GetTo()+1,e->GetTo()));
        } else {
            if(e->GetTo() > hole.GetTo())
                new_transcript_exons.push_back(CTranscriptExon(hole.GetTo()+1,e->GetTo()));
            if(e->GetFrom() < hole.GetFrom())
                new_transcript_exons.push_back(CTranscriptExon(e->GetFrom(),hole.GetFrom()-1));
        }
    }
    MyTranscriptExons() = new_transcript_exons;
}


void CAlignModel::TrimTranscriptExons(int left_trim, int right_trim) {

#ifdef _DEBUG
    _ASSERT(!MyTranscriptExons().empty());
    int totallen = 0;
    ITERATE(CAlignModel::TTranscriptExons, e, MyTranscriptExons()) {
        totallen += e->GetTo()-e->GetFrom()+1;
    }
    _ASSERT(totallen > left_trim+right_trim);
#endif

    bool reversed = (Status()&CGeneModel::eReversed)!=0;

    if(left_trim > 0) {
        reverse(MyTranscriptExons().begin(),MyTranscriptExons().end());
        int llen = 0;
        for(int i = (int)MyTranscriptExons().size()-1; i >= 0; --i) {
            int exonlen = MyTranscriptExons()[i].GetTo()-MyTranscriptExons()[i].GetFrom()+1;
            if(llen+exonlen <= left_trim) {
                MyTranscriptExons().pop_back();
                llen += exonlen;
            } else {
                int extra = left_trim-llen;          // this ammount should be trimmed  
                if((Strand() == ePlus && !reversed) || (Strand() == eMinus && reversed)) {
                    MyTranscriptExons()[i].AddFrom(extra);
                } else {
                    MyTranscriptExons()[i].AddTo(-extra);
                }
                break;
            }
        }
        reverse(MyTranscriptExons().begin(),MyTranscriptExons().end());
    }

    if(right_trim > 0) {
        int rlen = 0;
        for(int i = (int)MyTranscriptExons().size()-1; i >= 0; --i) {
            int exonlen = MyTranscriptExons()[i].GetTo()-MyTranscriptExons()[i].GetFrom()+1;
            if(rlen+exonlen <= right_trim) {
                MyTranscriptExons().pop_back();
                rlen += exonlen;
            } else {
                int extra = right_trim-rlen;          // this ammount should be trimmed     
                if((Strand() == ePlus && !reversed) || (Strand() == eMinus && reversed)) {
                    MyTranscriptExons()[i].AddTo(-extra);
                } else {
                    MyTranscriptExons()[i].AddFrom(extra);
                }
                break;
            }
        }
    }
}

void CAlignModel::AddExon(TSignedSeqRange exon_range, TSignedSeqRange transcript_exon_range) {
    
    if(transcript_exon_range.NotEmpty()) {
        CTranscriptExon e(transcript_exon_range.GetFrom(),transcript_exon_range.GetTo());
        if (Exons().empty() || Exons().back().GetTo()<exon_range.GetFrom()) {
            MyTranscriptExons().push_back(e);
        } else {
            MyTranscriptExons().insert(MyTranscriptExons().begin(),e);
        }
    }

    CGeneModel::AddExon(exon_range);
}

void CGeneModel::AddHole()
{
    m_expecting_hole = true;
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
}

void CGeneModel::TrimEdgesToFrameInOtherAlignGaps(const TExons& exons_with_gaps, bool ensure_cds_invariant)
{
    TSignedSeqPos left_edge = Limits().GetFrom();
    TSignedSeqPos right_edge = Limits().GetTo();
    CFrameShiftedSeqMap mrnamap(*this);

    for (int i = 0; i<int(exons_with_gaps.size())-1; ++i) {
        if (exons_with_gaps[i].GetTo() < left_edge && left_edge < exons_with_gaps[i+1].GetFrom()) {
            _ASSERT( !exons_with_gaps[i].m_ssplice && !exons_with_gaps[i+1].m_fsplice );
            TSignedSeqPos rightmost_base_inside_gap = left_edge;
            ITERATE(TExons, e, Exons()) {
                if (e->GetTo() < exons_with_gaps[i+1].GetFrom()) {
                    rightmost_base_inside_gap = e->GetTo();
                } else if (e->GetFrom() < exons_with_gaps[i+1].GetFrom()) {
                    rightmost_base_inside_gap = exons_with_gaps[i+1].GetFrom()-1;
                    break;
                } else {
                    break;
                }
            }
            TSignedSeqRange tmp = mrnamap.ShrinkToRealPoints(TSignedSeqRange(left_edge, rightmost_base_inside_gap));
            int del = mrnamap.FShiftedLen(tmp, false)%3;
            left_edge = mrnamap.FShiftedMove(tmp.GetFrom(), del);
        }

        if (exons_with_gaps[i].GetTo() < right_edge && right_edge < exons_with_gaps[i+1].GetFrom()) {
            _ASSERT( !exons_with_gaps[i].m_ssplice && !exons_with_gaps[i+1].m_fsplice );
            TSignedSeqPos leftmost_base_inside_gap = right_edge;
            for (TExons::const_reverse_iterator e = Exons().rbegin(); e != Exons().rend(); ++e) {
                if (exons_with_gaps[i].GetTo() < e->GetFrom()) {
                    leftmost_base_inside_gap = e->GetFrom();
                } else if (exons_with_gaps[i].GetTo() < e->GetTo()) {
                    leftmost_base_inside_gap = exons_with_gaps[i].GetTo()+1;
                    break;
                } else {
                    break;
                }
            }
            TSignedSeqRange tmp = mrnamap.ShrinkToRealPoints(TSignedSeqRange(leftmost_base_inside_gap, right_edge));
            int del = mrnamap.FShiftedLen(tmp, false)%3;
            right_edge = mrnamap.FShiftedMove(tmp.GetTo(), -del);
        }
    }

    TSignedSeqRange clip(left_edge,right_edge);

    if (clip != Limits())
        Clip(clip, eRemoveExons, ensure_cds_invariant );
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
    }

    RecalculateLimits();
    
    m_fshifts.insert(m_fshifts.end(),align.m_fshifts.begin(),align.m_fshifts.end());
    if(!m_fshifts.empty()) {
        sort(m_fshifts.begin(),m_fshifts.end());
        m_fshifts.erase( unique(m_fshifts.begin(),m_fshifts.end()), m_fshifts.end() );
    }
    RemoveExtraFShifts();

    SetType(Type() | (CGeneModel::eProt|CGeneModel::eEST|CGeneModel::emRNA)&align.Type());
    CombineCdsInfo(align, ensure_cds_invariant);
}

void CGeneModel::ExtendLeft(int amount)
{
    _ASSERT(amount>0);
    MyExons().front().AddFrom(-amount);
    RecalculateLimits();
}

void CGeneModel::ExtendRight(int amount)
{
    _ASSERT(amount>0);
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

TConstSeqidRef CGeneModel::Seqid() const
{
    return TConstSeqidRef(new CSeq_id(CSeq_id::e_Local,ID()));
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
    int model;
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
const string dot = ".";
}

void SGFFrec::print(CNcbiOstream& os) const
{

    os << (seqid.empty()?dot:seqid) << '\t';
    os << (source.empty()?dot:source) << '\t';
    os << (type.empty()?dot:type) << '\t';
    os << (start+1) << '\t';
    os << (end+1) << '\t';
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
        rec.start = NStr::StringToUInt(v[3])-1;
        rec.end = NStr::StringToUInt(v[4])-1;
        rec.score = v[5]==dot?BadScore():NStr::StringToDouble(v[5]);
        rec.strand = v[6][0];
        rec.phase = v[7]==dot?-1:NStr::StringToInt(v[7]);
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
                rec.model = NStr::StringToInt(value);
                model_id_present = true;
            } else if(key == "Target") {
                vector<string> tt;
                NStr::Tokenize(value, " \t", tt);
                rec.tstart = NStr::StringToUInt(tt[1])-1;
                rec.tend = NStr::StringToUInt(tt[2])-1;
                if(tt.size() > 3 && tt[3] == "-")
                    rec.tstrand = '-';
            } else
                rec.attributes[key] = value;
        }
    }
    if (!model_id_present)
        return InputError(is);
    res = rec;
    return is;
}

string BuildGFF3Gap(int& prev_pos, int pos, bool is_ins, int len)
{
    string gap;
    if (prev_pos < pos)
        gap += " M"+NStr::IntToString(pos-prev_pos);
    gap += string(" ")+(is_ins?"D":"I")+NStr::IntToString(len);
    prev_pos = pos+(is_ins?len:0);

    return gap;
}

string BuildGFF3Gap(int start, int end, const TFrameShifts& indels)
{
    string gap;
    string deletion_replacement;

    int prev_pos = start;
    ITERATE (TFrameShifts, indelp, indels) {
        CFrameShiftInfo indel = *indelp;
        indel.RestoreIfReplaced();
        if (indel.IsInsertion()) {
            _ASSERT( start <=indel.Loc() && indel.Loc()+indel.Len()-1 <= end );
            gap += BuildGFF3Gap(prev_pos, indel.Loc(), true, indel.Len());
        } else {
            gap += BuildGFF3Gap(prev_pos, indel.Loc(), false, indel.Len());
            deletion_replacement += " "+NStr::IntToString(indel.ReplacementLoc()+1);
        }
    }
    if (!gap.empty()) {
        gap.erase(0,1);
        if (prev_pos < end+1)
            gap += " M"+NStr::IntToString(end+1-prev_pos);
        if (!deletion_replacement.empty()) {
            deletion_replacement.erase(0,1);
            gap += ";deletion_replacement="+deletion_replacement;
        }
    }

    return gap;
}

void readGFF3Gap(const string& gap, const string& deletion_replacement, int start, int end, insert_iterator<TFrameShifts> iter)
{
    if (gap.empty())
        return;
    vector<string> operations;
    NStr::Tokenize(gap, " ", operations);
    vector<string> deletion_replacements;
    NStr::Tokenize(deletion_replacement, " ", deletion_replacements);
    vector<string>::const_iterator deletion_replacement_i = deletion_replacements.begin();
    TSignedSeqPos loc = start;
    ITERATE(vector<string>, o, operations) {
        int len = NStr::StringToInt(*o,NStr::fConvErr_NoThrow|NStr::fAllowLeadingSymbols);
        if ((*o)[0] == 'M') {
            loc += len;
        } else if ((*o)[0] == 'D') {
            *iter++ = CFrameShiftInfo(loc,len);
            loc += len;
        } else if ((*o)[0] == 'I') {
            CFrameShiftInfo deletion(loc,len,false);
            
            if (deletion_replacement_i != deletion_replacements.end()) {
                int replacement_location = NStr::StringToInt(*deletion_replacement_i,NStr::fConvErr_NoThrow)-1;
                if (replacement_location>-1) {
                    deletion.ReplaceWithInsertion(replacement_location, 3 - len);
                }
                ++deletion_replacement_i;
            }
            *iter++ = deletion;
        }
    }
    _ASSERT( loc == end+1 );
}

string CGeneModel::TypeToString(int type)
{
    if ((type & eGnomon)!=0) return "Gnomon";
    if ((type & eChain)!=0) return  "Chainer";
    if ((type & eProt)!=0) return  "ProSplign";
    if ((type & (eEST|emRNA))!=0) return  "Splign";
    return "Unknown";
}

CNcbiOstream& printGFF3(CNcbiOstream& os, const CAlignModel& a)
{
#ifdef _DEBUG
    if(!a.TranscriptExons().empty()) {
        _ASSERT(a.TranscriptExons().size() == a.Exons().size());
        int diff = 0;
        for(unsigned int i = 0; i < a.Exons().size(); ++i) {
            diff += a.Exons()[i].GetTo()-a.Exons()[i].GetFrom()-a.TranscriptExons()[i].GetTo()+a.TranscriptExons()[i].GetFrom();
        }
        ITERATE(TFrameShifts, f, a.FrameShifts()) {
            diff += (f->IsDeletion()) ? f->Len() : -f->Len();
        }
        _ASSERT(diff == 0);
    }
#endif

    CFrameShiftedSeqMap amap(a, CFrameShiftedSeqMap::eRealCdsOnly);
    
    SGFFrec templ;
    templ.model = a.ID();
    templ.seqid = contig_stream_state.slot(os);

    templ.source = CGeneModel::TypeToString(a.Type());
    templ.strand = a.Strand() == eMinus ? '-' : '+';

    SGFFrec mrna = templ;
    templ.attributes["Parent"] = mrna.attributes["ID"] = NStr::IntToString(a.ID());

    mrna.type = "mRNA";
    mrna.start = a.Limits().GetFrom();
    mrna.end = a.Limits().GetTo();
    mrna.score = a.Score();

    if (a.GeneID()!=0)
        mrna.attributes["Parent"] = "gene"+NStr::IntToString(a.GeneID());

    ITERATE(CSupportInfoSet, i, a.Support()) {
        mrna.attributes["support"] += ",";
        if(i->CoreAlignment()) 
            mrna.attributes["support"] += "*";
        mrna.attributes["support"] += i->Seqid()->IsGi()? i->Seqid()->AsFastaString() : i->Seqid()->GetSeqIdString(true);
    }

    mrna.attributes["support"].erase(0,1);

    if (!a.ProteinHit().empty())
        mrna.attributes["protein_hit"] = a.ProteinHit();

    if ((a.Type()&CGeneModel::eWall)!=0) mrna.attributes["flags"] += ",Wall";
    if ((a.Type()&CGeneModel::eNested)!=0) mrna.attributes["flags"] += ",Nested";
    if ((a.Type()&CGeneModel::eEST)!=0) mrna.attributes["flags"] += ",EST";
    if ((a.Type()&CGeneModel::emRNA)!=0) mrna.attributes["flags"] += ",mRNA";
    if ((a.Type()&CGeneModel::eProt)!=0) {
        mrna.attributes["flags"] += ",Prot";
    }

    if ((a.Status()&CGeneModel::eSkipped)!=0) mrna.attributes["flags"] += ",Skip";

    if (a.ReadingFrame().NotEmpty()) {
        _ASSERT( amap.FShiftedLen(a.ReadingFrame(), false)%3==0 );

        if (a.MaxCdsLimits()!=a.RealCdsLimits()) {
            mrna.attributes["maxCDS"] = NStr::IntToString(a.MaxCdsLimits().GetFrom()+1)+" "+NStr::IntToString(a.MaxCdsLimits().GetTo()+1);
        }
        if (a.GetCdsInfo().ProtReadingFrame().NotEmpty() && a.GetCdsInfo().ProtReadingFrame()!=a.ReadingFrame()) {
            mrna.attributes["protCDS"] =
                NStr::IntToString(a.GetCdsInfo().ProtReadingFrame().GetFrom()+1)+" "+
                NStr::IntToString(a.GetCdsInfo().ProtReadingFrame().GetTo()  +1);
        }
        if (a.HasStart()) {
            _ASSERT( !(a.ConfirmedStart() && a.OpenCds()) );
            string adj;
            if (a.ConfirmedStart())
                adj = "Confirmed";
            else if (a.OpenCds())
                adj = "Putative";
            mrna.attributes["flags"] += ","+adj+"Start";
        }
        if (a.HasStop()) {
            mrna.attributes["flags"] += ",Stop";
        }
        if ((a.Status()&CGeneModel::eFullSupCDS)!=0) mrna.attributes["flags"] += ",FullSupCDS";
        if ((a.Status()&CGeneModel::ePseudo)!=0) mrna.attributes["flags"] += ",Pseudo";

        if (a.FrameShifts().size()>0)
            mrna.attributes["flags"] += ",Frameshifts";

        ITERATE(vector<TSignedSeqRange>, s, a.GetCdsInfo().PStops()) {
            mrna.attributes["pstop"] += ","+NStr::IntToString(s->GetFrom()+1)+" "+NStr::IntToString(s->GetTo()+1);
        }
        mrna.attributes["pstop"].erase(0,1);
    }

    mrna.attributes["flags"].erase(0,1);

    mrna.attributes["note"] = a.GetComment();

    os << mrna;

    int part = 0;
    vector<SGFFrec> exons,cdss;

    SGFFrec exon = templ;
    exon.type = "exon";
    SGFFrec cds = templ;
    cds.type = "CDS";

    int phase = 0;
    if(a.ReadingFrame().NotEmpty()) {
        TSignedSeqRange tmp = amap.ShrinkToRealPoints(TSignedSeqRange(a.RealCdsLimits().GetFrom(),a.ReadingFrame().GetFrom()));
        _ASSERT(tmp.GetTo() == a.ReadingFrame().GetFrom());
        phase = (amap.FShiftedLen(tmp, false)-1)%3;
    }

    for(unsigned int i = 0; i < a.Exons().size(); ++i) {
        const CModelExon *e = &a.Exons()[i]; 

        exon.start = e->GetFrom();
        exon.end = e->GetTo();

        if(!a.TranscriptExons().empty()) {
            string target = a.SupportName();
            target += " "+NStr::IntToString(a.TranscriptExons()[i].GetFrom()+1)+" "+NStr::IntToString(a.TranscriptExons()[i].GetTo()+1);
            if((a.Status() & CGeneModel::eReversed)!=0) {
                target += " -";
            } else {
                target += " +";
            }
            exon.attributes["Target"] = target;
        }

        exon.attributes["Gap"] = BuildGFF3Gap(exon.start, exon.end, a.FrameShifts(exon.start, exon.end));
        exons.push_back(exon);
        exon.attributes["Gap"].erase();

        TSignedSeqRange cds_limits = e->Limits() & a.RealCdsLimits();
        if (cds_limits.NotEmpty()) {
            cds.start = cds_limits.GetFrom();
            cds.end = cds_limits.GetTo();
            cdss.push_back(cds);
        }

        if (!e->m_ssplice) {
            string suffix;
            if (!a.Continuous()) {
                SGFFrec rec = templ;
                suffix= "."+NStr::IntToString(++part);
                rec.start = exons.front().start;
                rec.end = exons.back().end;
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
                phase = (amap.FShiftedLen(e->start,e->end)-phase)%3;
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

TSignedSeqRange StringToRange(const string& s)
{
    string start, stop;
    NStr::SplitInTwo(s, " ", start, stop);
    return TSignedSeqRange(NStr::StringToInt(start)-1,NStr::StringToInt(stop)-1);
}


CNcbiIstream& readGFF3(CNcbiIstream& is, CAlignModel& align)
{
    SGFFrec rec;
    is >> rec;
    if (!is) {
         return is;
    }

    int id = rec.model;
    if (id==0)
        return InputError(is);

    vector<SGFFrec> recs;

    do {
        recs.push_back(rec);
    } while (is >> rec && rec.seqid == recs.front().seqid && rec.model == id);

    Ungetline(is);

    CAlignModel a;

    a.SetID(id);
#ifdef _DEBUG
    a.oid = id;
#endif

    TSignedSeqRange cds;
    int phase = 0;

    typedef pair<TSignedSeqRange,TSignedSeqRange> TSignedSeqRangePair;
    vector<TSignedSeqRangePair> exons;
    set<TSignedSeqRange,Precedence> mrna_parts;

    rec = recs.front();

    if (rec.source == "Gnomon") a.SetType(a.Type() | CGeneModel::eGnomon);
    else if (rec.source == "Chainer") a.SetType(a.Type() | CGeneModel::eChain);
    a.SetStrand(rec.strand=='-'?eMinus:ePlus);

    CCDSInfo cds_info;
    cds_info.SetReadingFrame(TSignedSeqRange::GetWhole());

    TSignedSeqRange max_cds_limits;
    bool open_cds = false;
    double score = BadScore();
    bool confirmed_start = false;
    bool has_start = false;
    bool has_stop = false;
    string pstops, altstarts;

    NON_CONST_ITERATE(vector<SGFFrec>, r, recs) {
        if (r->type == "mRNA") {
            if (NStr::StartsWith(r->attributes["Parent"],"gene"))
                a.SetGeneID(NStr::StringToInt(r->attributes["Parent"],NStr::fConvErr_NoThrow|NStr::fAllowLeadingSymbols));

            vector<string> support;
            NStr::Tokenize(r->attributes["support"], ",", support);
            ITERATE(vector<string>, s, support) {
                bool core = (*s)[0] == '*';
                string id = core ? s->substr(1) : *s;
                a.Support().insert(CSupportInfo(CreateSeqid(id), core));
            }

            if (!r->attributes["protein_hit"].empty())
                a.ProteinHit()=r->attributes["protein_hit"];

            vector<string> flags;
            NStr::Tokenize(r->attributes["flags"], ",", flags);
            ITERATE(vector<string>, f, flags) {
                     if (*f == "Wall")       a.SetType(a.Type()|  CGeneModel::eWall);
                else if (*f == "Nested")     a.SetType(a.Type()|  CGeneModel::eNested);
                else if (*f == "EST")        a.SetType(a.Type()|  CGeneModel::eEST);
                else if (*f == "mRNA")       a.SetType(a.Type()|  CGeneModel::emRNA);
                else if (*f == "Prot")       a.SetType(a.Type()|  CGeneModel::eProt);
                else if (*f == "Skip")       a.Status()        |= CGeneModel::eSkipped;
                else if (*f == "FullSupCDS") a.Status()        |= CGeneModel::eFullSupCDS;
                else if (*f == "Pseudo")     a.Status()        |= CGeneModel::ePseudo;
                else if (*f == "ConfirmedStart")   { confirmed_start = true; has_start = true; }
                else if (*f == "PutativeStart")   { open_cds = true; has_start = true; }
                else if (*f == "Start") has_start = true;
                else if (*f == "Stop")  has_stop = true;
           }
           score = r->score;

            a.SetComment(r->attributes["note"]);

            if (!r->attributes["protein_hit"].empty())
                a.ProteinHit() = r->attributes["protein_hit"];

            if (!r->attributes["maxCDS"].empty()) {
                max_cds_limits = StringToRange(r->attributes["maxCDS"]);
            }
            if (!r->attributes["protCDS"].empty()) {
                cds_info.SetReadingFrame( StringToRange(r->attributes["protCDS"]), true );
            }
            pstops = r->attributes["pstop"];
            altstarts = r->attributes["altstart"];
        } else if (r->type == "exon") {
            if(r->tstart >= 0 && r->tstrand == '-')
                a.Status() |= CGeneModel::eReversed;
            exons.push_back(make_pair(TSignedSeqRange(r->start,r->end),TSignedSeqRange(r->tstart,r->tend)));
            readGFF3Gap(r->attributes["Gap"],r->attributes["deletion_replacement"],r->start,r->end,inserter(a.FrameShifts(),a.FrameShifts().end()));
        } else if (r->type == "CDS") {
            TSignedSeqRange cds_exon(r->start,r->end);
            if (r->strand=='+') {
                if (cds.Empty() || r->start < cds.GetFrom())
                    phase = r->phase;
            } else {
                if (cds.Empty() || cds.GetTo() < r->end)
                    phase = r->phase;
            }
            cds.CombineWith(cds_exon);
        } else if (r->type == "mRNA_part") {
            mrna_parts.insert(TSignedSeqRange(r->start,r->end));
        }
    }

    sort(exons.begin(),exons.end());
    ITERATE(vector<TSignedSeqRangePair>, e, exons) {
        if (a.Limits().IntersectingWith(e->first)) {
            return  InputError(is);
        }
        set<TSignedSeqRange,Precedence>::iterator p = mrna_parts.lower_bound(e->first);
        if (p != mrna_parts.end() && p->GetFrom()==e->first.GetFrom())
            a.AddHole();
        a.AddExon(e->first,e->second);
    }

    sort(a.FrameShifts().begin(),a.FrameShifts().end());
    
    CFrameShiftedSeqMap amap(a);

    if (max_cds_limits.Empty())
        max_cds_limits = cds;

    if (cds.NotEmpty()) {
        if (a.Strand()==ePlus) {
            if (has_start && phase!=0)
                return  InputError(is);
            cds.SetFrom(amap.FShiftedMove(cds.GetFrom(),phase));
            if (has_stop && amap.FShiftedLen(cds, false)%3!=0)
                return  InputError(is);
            cds.SetTo(amap.FShiftedMove(cds.GetTo(), -amap.FShiftedLen(cds, false)%3));
        } else {
            if (has_start && phase!=0)
                return  InputError(is);
            cds.SetTo(amap.FShiftedMove(cds.GetTo(),-phase));
            if (has_stop && amap.FShiftedLen(cds, false)%3!=0)
                return  InputError(is);
            cds.SetFrom(amap.FShiftedMove(cds.GetFrom(), +amap.FShiftedLen(cds, false)%3));
        }
    }

    if (cds.Empty() && (has_start || has_stop || max_cds_limits.NotEmpty()) ||
        max_cds_limits.NotEmpty() && !Include(max_cds_limits, cds))
        return  InputError(is);

    TSignedSeqRange start, stop;

    if (a.Strand()==eMinus) swap(has_start, has_stop);
    if (has_start) {
        start = TSignedSeqRange(cds.GetFrom(),amap.FShiftedMove(cds.GetFrom(), +2));

        if (amap.FShiftedLen(start, false) != 3)
            return InputError(is);
    }
    if (has_stop) {
        stop = TSignedSeqRange(amap.FShiftedMove(cds.GetTo(), -2), cds.GetTo());

        if (amap.FShiftedLen(stop, false) != 3)
            return InputError(is);
    }

    TSignedSeqRange reading_frame = cds;
    if (start.NotEmpty())
        reading_frame.SetFrom(amap.FShiftedMove(start.GetTo(),+1));
    if (stop.NotEmpty())
        reading_frame.SetTo(amap.FShiftedMove(stop.GetFrom(),-1));

    if (a.Strand()==eMinus) swap(start, stop);

    cds_info.SetReadingFrame(reading_frame, (a.Type()&CGeneModel::eProt)!=0 && cds_info.ProtReadingFrame().Empty());
    cds_info.SetStart(start,confirmed_start);

    if (max_cds_limits.NotEmpty()) {
        if (a.Strand()==ePlus && a.Limits().GetFrom()<max_cds_limits.GetFrom())
            cds_info.Set5PrimeCdsLimit(max_cds_limits.GetFrom());
        else if (a.Strand()==eMinus && a.Limits().GetTo()>max_cds_limits.GetTo())
            cds_info.Set5PrimeCdsLimit(max_cds_limits.GetTo());
    }
    cds_info.SetStop(stop);

    {
        vector<string> stops;
        NStr::Tokenize(pstops, ",", stops);
        ITERATE(vector<string>, s, stops)
            cds_info.AddPStop(StringToRange(*s));
    }

    cds_info.SetScore(score, open_cds);

    a.SetCdsInfo(cds_info);

    align = a;
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

CSupportInfo::CSupportInfo(const TConstSeqidRef& s, bool core)
    : m_core_align(core), m_type(0)
{
    SetSeqid(s);
}
TConstSeqidRef CSupportInfo::Seqid() const
{
    return m_local_id==-1?m_seq_id:CreateSeqid(m_local_id);
}
void CSupportInfo::SetSeqid(const TConstSeqidRef sid)
{
    if (sid->IsLocal() && sid->GetLocal().IsId()) {
        m_local_id = sid->GetLocal().GetId();
        m_seq_id.Reset();
    } else {
        m_local_id = -1;
        m_seq_id = sid;
    }
    _ASSERT( !(m_local_id == -1 && m_seq_id.Empty()) );
}
bool CSupportInfo::operator==(const CSupportInfo& s) const
{
    return m_local_id == s.m_local_id && CoreAlignment() == s.CoreAlignment() && Type() == s.Type() && (m_local_id != -1 || m_seq_id->Match(*s.m_seq_id));
}
bool CSupportInfo::operator<(const CSupportInfo& s) const
{
    return (m_local_id == -1 && s.m_local_id == -1) ? *m_seq_id < *s.m_seq_id : m_local_id < s.m_local_id;
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
