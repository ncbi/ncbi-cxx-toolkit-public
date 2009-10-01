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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/gnomon/chainer.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/gnomon.hpp>

#include <map>
#include <sstream>

#include <objects/general/Object_id.hpp>


BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(gnomon)

CChainer::CChainer()
{
}


enum {
    eCDS,
    eLeftUTR,
    eRightUTR
};

struct SChainMember;

typedef vector<SChainMember*> TContained;

typedef set<const CGeneModel*> TEvidence;

struct SChainMember
{
    SChainMember() :
        m_align(0), m_left_member(0), m_right_member(0),
        m_copy(0), m_left_num(0), m_right_num(0), m_num(0),
        m_type(eCDS), m_included(false), m_isduplicate(false) {}

    void CollectContainedAlignments(set<SChainMember*>& chain_alignments);
    void CollectAllContainedAlignments(set<SChainMember*>& chain_alignments);

    CGeneModel* m_align;
    SChainMember* m_left_member;
    SChainMember* m_right_member;
    SChainMember* m_copy;      // is used to make sure that the copy of already incuded duplicated alignment doesn't trigger a new chain genereation
    TContained m_contained;
    TContained m_identical;
    int m_left_num, m_right_num, m_num, m_type;
    bool m_included, m_isduplicate;
};


class CChain : public CGeneModel
{
public:
    CChain(const set<SChainMember*>& chain_alignments,
           const CGnomonEngine&      gnomon,
           const SMinScor&           minscor,
           int trim);
    //    void MergeWith(const CChain& another_chain, const CGnomonEngine&gnomon, const SMinScor& minscor);
    void ToEvidence(vector<TEvidence>& mrnas,
                    vector<TEvidence>& ests, 
                    vector<TEvidence>& prots) const ;

    vector<CGeneModel*> m_members;
};



void SChainMember::CollectContainedAlignments(set<SChainMember*>& chain_alignments)
{
    NON_CONST_ITERATE (TContained, ic, m_contained) {
        SChainMember& mi = **ic;
        mi.m_included = true;
        if (mi.m_copy != 0)
            mi.m_copy->m_included = true;

        if(mi.m_copy != 0 && mi.m_align->ReadingFrame().NotEmpty() &&  mi.m_align->ReadingFrame() == mi.m_copy->m_align->ReadingFrame() &&
           Include( mi.m_copy->m_align->GetCdsInfo().MaxCdsLimits(), mi.m_align->GetCdsInfo().MaxCdsLimits() )) {
            chain_alignments.insert(mi.m_copy);        // if dublicated include the longer MaxCds   
        } else {
            chain_alignments.insert(&mi);
        }       
    }
}

void SChainMember::CollectAllContainedAlignments(set<SChainMember*>& chain_alignments)
{
    CollectContainedAlignments(chain_alignments);
    
    for (SChainMember* left = m_left_member; left != 0; left = left->m_left_member) {
        left->CollectContainedAlignments(chain_alignments);
    }
        
    for (SChainMember* right = m_right_member; right != 0; right = right->m_right_member) {
        right->CollectContainedAlignments(chain_alignments);
    }
}        

struct LeftOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)
    {
        const TSignedSeqRange alimits = ap->m_align->Limits();
        const TSignedSeqRange blimits = bp->m_align->Limits();
        if(alimits.GetTo() == blimits.GetTo()) {
            if(alimits.GetFrom() == blimits.GetFrom()) {
                if (ap->m_type == bp->m_type)
                    return ap->m_align->ID() < bp->m_align->ID(); // to make sort deterministic
                else
                    return ap->m_type < bp->m_type;                     // used for MergeIdenticalSingleExon
            } else {
                return (alimits.GetFrom() > blimits.GetFrom());
            }
        } else {
            return (alimits.GetTo() < blimits.GetTo());
        }
    }
};

struct RightOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)
    {
        if(ap->m_align->Limits().GetFrom() == bp->m_align->Limits().GetFrom())
        {
            if (ap->m_align->Limits().GetTo() == bp->m_align->Limits().GetTo())
                return ap->m_align->ID() < bp->m_align->ID(); // to make sort deterministic
            else
                return (ap->m_align->Limits().GetTo() < bp->m_align->Limits().GetTo());
        }
        else
        {
            return (ap->m_align->Limits().GetFrom() > bp->m_align->Limits().GetFrom());
        }
    }
};

struct NumOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)
    {
        if (ap->m_num == bp->m_num)
            return ap->m_align->ID() < bp->m_align->ID(); // to make sort deterministic
        else
            return (ap->m_num > bp->m_num);
    }
};

static bool s_BySizePosition(const CGeneModel& a, const CGeneModel& b)
    {
        if (a.Exons().size() != b.Exons().size()) return a.Exons().size() < b.Exons().size();
        if (a.Strand() != b.Strand()) return a.Strand() < b.Strand();
        if (a.Limits() != b.Limits()) return a.Limits() < b.Limits();
        return a.ID() < b.ID(); // to make sort deterministic
    }

struct ScoreOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)
    {
        if (ap->m_align->Score() == bp->m_align->Score())
            return ap->m_align->ID() < bp->m_align->ID(); // to make sort deterministic
        else
            return (ap->m_align->Score() > bp->m_align->Score());
    }
};

static bool s_ScoreOrder(const CGeneModel& a, const CGeneModel& b)
    {
        if (a.Score() == b.Score())
            return a.ID() < b.ID(); // to make sort deterministic
        else
            return a.Score() > b.Score();
    }

template <class C>
void uniq(C& container)
{
    sort(container.begin(),container.end());
    container.erase( unique(container.begin(),container.end()), container.end() );
}

bool FillGaps(CGeneModel& a, CGeneModel& filling)
{
    int type = a.Type();
    CGeneModel::TExons old_exons = a.Exons();
    TSignedSeqRange a_limits = a.Limits();
    CCDSInfo cdsinfo = a.GetCdsInfo();
    a.Extend(filling);
    a.Clip(a_limits, CGeneModel::eRemoveExons );
    cdsinfo.ClearPStops();
    ITERATE(CCDSInfo::TPStops, ps, a.GetCdsInfo().PStops()) {
        cdsinfo.AddPStop(*ps);
    }
    a.SetCdsInfo(cdsinfo);

    filling.TrimEdgesToFrameInOtherAlignGaps( a.Exons() );

    a.SetType(type);

    return (a.Exons() != old_exons);
}

TSignedSeqPos CommonReadingFramePoint(const CGeneModel& a, const CGeneModel& b)
{
    //finds a common reading frame point not touching any indels
    TSignedSeqRange reading_frame = a.ReadingFrame() + b.ReadingFrame();
    ITERATE(CGeneModel::TExons, ae, a.Exons()) {
        TSignedSeqRange a_cds_piece = ae->Limits() & reading_frame; 
        if (a_cds_piece.NotEmpty()) {
            ITERATE(CGeneModel::TExons, be, b.Exons()) {
                TSignedSeqRange common_piece = a_cds_piece & be->Limits(); 
                for(TSignedSeqPos p = common_piece.GetFrom(); p <= common_piece.GetTo(); ++p) {
                    bool good_point = true;
                    ITERATE(TInDels, i, a.FrameShifts()) {
                        if((i->IsDeletion() && (i->Loc() == p || i->Loc() == p+1)) ||
                           (i->IsInsertion() && i->Loc() <= p+1 && i->Loc()+i->Len() >= p))
                            good_point = false;
                    }
                    ITERATE(TInDels, i, b.FrameShifts()) {
                        if((i->IsDeletion() && (i->Loc() == p || i->Loc() == p+1)) ||
                           (i->IsInsertion() && i->Loc() <= p+1 && i->Loc()+i->Len() >= p))
                            good_point = false;
                    }
                    if(good_point)
                        return p;
                }
            }
        }
    }
    return -1;
}

TSignedSeqRange ExtendedMaxCdsLimits(const CGeneModel& a)
{
    TSignedSeqRange limits(a.Limits().GetFrom()-1,a.Limits().GetTo()+1);

    return limits & a.GetCdsInfo().MaxCdsLimits();
}

bool CodingCompatible(const CGeneModel& a, const CGeneModel& b, const CResidueVec& seq, bool check_outside = true)
{
    _ASSERT( a.isCompatible(b) );

    if(a.ReadingFrame().Empty() && b.ReadingFrame().Empty())                 // both are UTRs
        return true;

    const CGeneModel* pa = &a;
    const CGeneModel* pb = &b;

    if(a.ReadingFrame().NotEmpty() && b.ReadingFrame().NotEmpty()) {          // both have CDSes


        //we don't want confirmedstarts/stops in the holes of the other alignments
        if(a.ConfirmedStart() || a.HasStop()) {
            TSignedSeqRange start;
            if(a.ConfirmedStart())
                start = a.GetCdsInfo().Start();
            TSignedSeqRange stop = a.GetCdsInfo().Stop();
            for(unsigned int i = 1; i < b.Exons().size(); ++i) {
                bool hole = !b.Exons()[i-1].m_ssplice || !b.Exons()[i].m_fsplice;
                TSignedSeqRange intron(b.Exons()[i-1].GetTo()+1,b.Exons()[i].GetFrom()-1);
                if(hole && (start.IntersectingWith(intron) || stop.IntersectingWith(intron)))
                    return false;
            }
        }
        if(b.ConfirmedStart() || b.HasStop()) {
            TSignedSeqRange start;
            if(b.ConfirmedStart())
                start = b.GetCdsInfo().Start();
            TSignedSeqRange stop = b.GetCdsInfo().Stop();
            for(unsigned int i = 1; i < a.Exons().size(); ++i) {
                bool hole = !a.Exons()[i-1].m_ssplice || !a.Exons()[i].m_fsplice;
                TSignedSeqRange intron(a.Exons()[i-1].GetTo()+1,a.Exons()[i].GetFrom()-1);
                if(hole && (start.IntersectingWith(intron) || stop.IntersectingWith(intron)))
                    return false;
            }
        }

        TSignedSeqRange max_cds_limits = a.GetCdsInfo().MaxCdsLimits() & b.GetCdsInfo().MaxCdsLimits();
        
        if (!Include(max_cds_limits, ExtendedMaxCdsLimits(a) + ExtendedMaxCdsLimits(b)))
            return false;

        TSignedSeqRange reading_frame_limits(max_cds_limits.GetFrom()+1,max_cds_limits.GetTo()-1); // clip to exclude possible start/stop
        
        if (!Include(reading_frame_limits, a.ReadingFrame() + b.ReadingFrame()))
            return false;

        CAlignMap amap(pa->Exons(),pa->FrameShifts(),pa->Strand()), bmap(pb->Exons(),pb->FrameShifts(),pb->Strand());

        TSignedSeqPos common_point = CommonReadingFramePoint(a, b);
        if(common_point < 0)
            return false;

        if(common_point < pa->ReadingFrame().GetFrom() || pb->ReadingFrame().GetTo() < common_point) {
            swap(pa,pb);
            swap(amap,bmap);
        }

        _ASSERT( pa->ReadingFrame().GetFrom() <= common_point);
        _ASSERT( common_point <= pb->ReadingFrame().GetTo() );

        int a_start_to_b_end =
            amap.FShiftedLen(pa->ReadingFrame().GetFrom(),common_point, true)+
            bmap.FShiftedLen(common_point,pb->ReadingFrame().GetTo(), true)-1;

        return (a_start_to_b_end % 3 == 0);
    } else {         // only one has CDS
        if(pb->ReadingFrame().NotEmpty()) {
            swap(pa,pb);               // pa has CDS
        }

        CGeneModel combination(*pb);
        combination.Extend(*pa, false);
        
        CResidueVec mrna;
        CAlignMap mrnamap(combination.GetAlignMap());
        mrnamap.EditedSequence(seq, mrna);

        bool start_needed, stop_needed;
        if (pa->Strand()==ePlus) {
            start_needed = pa->GetCdsInfo().MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom() && pb->Limits().GetFrom() < pa->Limits().GetFrom();
            stop_needed = !pa->HasStop() && pa->Limits().GetTo() < pb->Limits().GetTo();
        } else {
            start_needed = pa->GetCdsInfo().MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo() && pa->Limits().GetTo() < pb->Limits().GetTo();
            stop_needed = !pa->HasStop() && pb->Limits().GetFrom() < pa->Limits().GetFrom();
        }

        int maxcds5p = mrnamap.MapRangeOrigToEdited(pa->MaxCdsLimits(),false).GetFrom();
        TSignedSeqRange mrna_cds = mrnamap.MapRangeOrigToEdited(pa->ReadingFrame(),true);
        int cds5p = mrna_cds.GetFrom();
        int cds3p = mrna_cds.GetTo();


        int framelimit5p = maxcds5p+(cds5p-maxcds5p)%3;

        const CCDSInfo::TPStops& apstops = pa->GetCdsInfo().PStops();
        for(int i = framelimit5p; i < cds3p; i += 3) {                    // check that there are no new stops incide old cds
            if(IsStopCodon(&mrna[i])) {
                TSignedSeqPos a = mrnamap.MapEditedToOrig(i);
                TSignedSeqPos b = mrnamap.MapEditedToOrig(i+2);
                if(b < a) swap(a,b);
                TSignedSeqRange stopcodon(a,b);
                if(find(apstops.begin(),apstops.end(),stopcodon) == apstops.end()) {
                    return false;
                }
            }
        }

        if(!check_outside)       // no start/stop check for patching protein holes
            return true;

        if(start_needed) {                                                 // 5p extension
            bool found = false;
            for(int i = framelimit5p-3; i >= 0; i -= 3) {
                if(IsStartCodon(&mrna[i])) {
                    found = true;                                         // found upstream start
                    break;
                }
                if(IsStopCodon(&mrna[i])) return false;                   // found upstream stop
            }
            if(!found) return false;                                      // no start found 
        }

        if(stop_needed) {                                          // 3p extension and possibly no stop
            for(int i = cds3p+1; i <= (int)mrna.size()-3; i += 3) {
                if(IsStopCodon(&mrna[i])) return true;                    // found downsream stop 
            }
            return false;
        }

        return true;
    }
}

void FindNextSingleExon(vector<SChainMember*>::iterator& iter, const vector<SChainMember*>::iterator& end)
{
    while (iter != end && (*iter)->m_align->Exons().size() != 1)
      ++iter;
}

void MergeIdenticalSingleExon(vector<SChainMember*>& pointers)
{
    sort(pointers.begin(),pointers.end(),LeftOrder());
    vector<SChainMember*>::iterator cur = pointers.begin();
    FindNextSingleExon(cur,pointers.end());
    if (cur == pointers.end()) return;
    vector<SChainMember*>::iterator prev = cur++;

    for(;;) {
        FindNextSingleExon(cur,pointers.end());
        if (cur == pointers.end()) break;

        if ((*prev)->m_type == (*cur)->m_type &&
            (*prev)->m_align->Limits() == (*cur)->m_align->Limits() &&
            (*prev)->m_align->GetCdsInfo() == (*cur)->m_align->GetCdsInfo() &&
            ((*prev)->m_align->Status()&CGeneModel::ePolyA) == ((*cur)->m_align->Status()&CGeneModel::ePolyA) &&
            ((*prev)->m_align->Status()&CGeneModel::eCap) == ((*cur)->m_align->Status()&CGeneModel::eCap) ) {
                (*prev)->m_identical.push_back(*cur);
                cur=pointers.erase(cur);
        } else {
            prev = cur++;
        }
    }
}

void IncludeWithIdenticals(SChainMember& big, SChainMember& small, CCDSInfo& cds) {
    big.m_contained.push_back(&small);

    ITERATE(TContained, k, small.m_identical) {
        big.m_contained.push_back(*k);
    }
    if(small.m_type == eCDS) {
        cds.CombineWith(small.m_align->GetCdsInfo());
    }
}

void FindContainedAlignments(vector<SChainMember*>& pointers, CGnomonEngine& gnomon) {

//  finding contained subalignments (alignment is contained in itself)

    sort(pointers.begin(),pointers.end(),LeftOrder());
    const CResidueVec& seq = gnomon.GetSeq();
    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        TSignedSeqRange ai_limits = ai.Limits();
        TSignedSeqPos   ai_to = ai_limits.GetTo();
        TSignedSeqRange initial_reading_frame(ai.ReadingFrame());

        CCDSInfo cds(ai.GetCdsInfo());
        CCDSInfo old_cds(ai.GetCdsInfo());

        NON_CONST_ITERATE(vector<SChainMember*>, j, pointers) {
            SChainMember& mj = **j;
            CGeneModel& aj = *mj.m_align;
            if(aj.Limits().GetTo() > ai_to) break;

            if(i == j)
                IncludeWithIdenticals(mi, mj, cds);          // include self no matter what
            else if (&mj == mi.m_copy)
                continue;
            else if(mj.m_isduplicate && mj.m_type != eCDS)
                continue;    // avoid including UTR copy
            else if(mi.m_type != eCDS && mj.m_type == eCDS)
                continue;   // avoid including CDS into UTR because that will change m_type
            //            else if(mi.m_type == eCDS && !ai.HasStop() && (aj.Status()&CGeneModel::ePolyA) != 0)
            //                continue;   // awoid including PolyA if there is no 3' UTR 
            else if(aj.IsSubAlignOf(ai) && CodingCompatible(aj, ai, seq))
                IncludeWithIdenticals(mi, mj, cds);
        }
        /*
        if(mi.m_type == eCDS && initial_reading_frame != cds.ReadingFrame())  {
            ai.CombineCdsInfo(cds, false);
            gnomon.GetScore(ai);
            _ASSERT(ai.Score() != BadScore());
        }
        */
    }
}

void LeftRight(vector<SChainMember*>& pointers, int intersect_limit, CGnomonEngine& gnomon)
{
    sort(pointers.begin(),pointers.end(),LeftOrder());
    const CResidueVec& seq = gnomon.GetSeq();
    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        TSignedSeqRange ai_limits = ai.Limits();

        mi.m_left_num = mi.m_contained.size();
       
        vector<TSignedSeqPos> ends;
        ends.reserve(mi.m_left_num);
        ITERATE(TContained, ic, mi.m_contained) {
            ends.push_back((*ic)->m_align->Limits().GetTo());
        }
            
        sort(ends.begin(),ends.end());
        
        vector<TSignedSeqPos>::iterator endsp = ends.begin();

        for(vector<SChainMember*>::iterator j = pointers.begin(); j !=i; ++j) {
            SChainMember& mj = **j;
            CGeneModel& aj = *mj.m_align;
            
            switch(mi.m_type)
            {
                case eCDS: 
                    if(mj.m_type == eRightUTR) continue;
                    else break;
                case eLeftUTR: 
                    if(mj.m_type != eLeftUTR) continue;
                    else break;
                case eRightUTR:
                    if(mj.m_type == eLeftUTR) continue;
                    else break;
                default:
                    continue;    
            }

            switch(ai.MutualExtension(aj))
            {
                case 0:              // not compatible 
                    continue;
                case 1:              // no introns in intersection
                {
                    /*
                    if(mi.m_type == eCDS && mj.m_type == eCDS && ai.MaxCdsLimits().IntersectingWith(aj.MaxCdsLimits()))
                        break;

                    if(mi.m_type != eCDS && mi.m_align->Exons().size() > 1)
                        continue;     // no connection of multiexon UTRs without common splices
                    if(mj.m_type != eCDS && mj.m_align->Exons().size() > 1)
                        continue;     // no connection of multiexon UTRs without common splices
                    */

                    int intersection_len = (ai_limits & aj.Limits()).GetLength(); 
                    if (intersection_len < intersect_limit) continue;
                    break;
                }
                default:             // one or more introns in intersection
                    break;
            }
            
            if(!CodingCompatible(ai, aj, seq)) continue;

            endsp = upper_bound(endsp,ends.end(),aj.Limits().GetTo());
            int delta = ends.end()-endsp;
            int newnum = mj.m_left_num+delta;

            SChainMember* pleft = mi.m_left_member;
            if (newnum > mi.m_left_num || (newnum == mi.m_left_num && pleft != 0 && mj.m_type == eCDS && pleft->m_type == eCDS && 
                 Include(aj.GetCdsInfo().MaxCdsLimits(), pleft->m_align->GetCdsInfo().MaxCdsLimits()))) {
                mi.m_left_num = newnum;
                mi.m_left_member = &mj;
            }    
        }
    }
}

void RightLeft(vector<SChainMember*>& pointers, int intersect_limit, CGnomonEngine& gnomon)
{
    sort(pointers.begin(),pointers.end(),RightOrder());
    const CResidueVec& seq = gnomon.GetSeq();
    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
    
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        TSignedSeqRange ai_limits = ai.Limits();

        mi.m_right_num = mi.m_contained.size();

        vector<TSignedSeqPos> ends;
        ends.reserve(mi.m_right_num);
        ITERATE(TContained, ic , mi.m_contained) {
            ends.push_back((*ic)->m_align->Limits().GetFrom());
        }

        sort(ends.begin(),ends.end(),greater<int>());
        
        vector<TSignedSeqPos>::iterator endsp = ends.begin();
        
        for(vector<SChainMember*>::iterator j = pointers.begin(); j !=i; ++j) {
            SChainMember& mj = **j;
            CGeneModel& aj = *mj.m_align;
            
            switch(mi.m_type)
            {
                case eCDS: 
                    if(mj.m_type == eLeftUTR) continue;
                    else break;
                case eRightUTR: 
                    if(mj.m_type != eRightUTR) continue;
                    else break;
                case eLeftUTR:
                    if(mj.m_type == eRightUTR) continue;
                    else break;
                default:
                    continue;    
            }

            switch(ai.MutualExtension(aj))
            {
                case 0:              // not compatible 
                    continue;
                case 1:              // no introns in intersection
                {
                    /*
                    if(mi.m_type == eCDS && mj.m_type == eCDS && ai.MaxCdsLimits().IntersectingWith(aj.MaxCdsLimits()))
                        break;

                    if(mi.m_type != eCDS && mi.m_align->Exons().size() > 1)
                        continue;     // no connection of multiexon UTRs without common splices
                    if(mj.m_type != eCDS && mj.m_align->Exons().size() > 1)
                        continue;     // no connection of multiexon UTRs without common splices
                    */

                    int intersect = (ai_limits & aj.Limits()).GetLength(); 
                    if(intersect < intersect_limit) continue;
                    break;
                }
                default:             // one or more introns in intersection
                    break;
            }
            
            if(!CodingCompatible(ai, aj,  seq)) continue;
            
            endsp = upper_bound(endsp,ends.end(),aj.Limits().GetFrom(),greater<int>());
            int delta = ends.end()-endsp;
            int newnum = mj.m_right_num+delta;

            SChainMember* pright = mi.m_right_member;
            if (newnum > mi.m_right_num || (newnum == mi.m_right_num && pright != 0 && mj.m_type == eCDS && pright->m_type == eCDS &&
                 Include(aj.GetCdsInfo().MaxCdsLimits(), pright->m_align->GetCdsInfo().MaxCdsLimits()))) {
                mi.m_right_num = newnum;
                mi.m_right_member = &mj;
            }    
        }
    }
}


// genome coordinate of the first start (in frame with the annotated start) on the model (coordinate of 'A')
TSignedSeqPos GetFirstStart(CGeneModel& model, const CResidueVec& seq)
{

    TSignedSeqPos first_start = -1;
    
    if(model.HasStart()) {
        CResidueVec mrna;
        CAlignMap mrnamap(model.Exons(),model.FrameShifts(),model.Strand());
        mrnamap.EditedSequence(seq, mrna);

        int annotated_start;
        if(model.Strand() == ePlus) {
            first_start = model.GetCdsInfo().Start().GetFrom();
        } else {
            first_start = model.GetCdsInfo().Start().GetTo();
        }
        annotated_start = mrnamap.MapOrigToEdited(first_start);

        _ASSERT(annotated_start >= 0);

        for(int i = annotated_start-3; i >= 0; i -= 3) {
            if(IsStartCodon(&mrna[i])) {
                first_start = mrnamap.MapEditedToOrig(i);
            }
        }
    }

    return first_start;
}


class CChainMembers : public vector<SChainMember*> {
public:
    CChainMembers(TGeneModelList& clust);
    void FillGapsInAlignments(const CResidueVec& seq);
    void DuplicateUnrestricted5Prime(TGeneModelList& clust, const CResidueVec& seq);
private:
    vector<SChainMember> m_members;

};

CChainMembers::CChainMembers(TGeneModelList& clust)
{
    m_members.reserve(2*clust.size());
    NON_CONST_ITERATE(TGeneModelList, itcl, clust) {
        CGeneModel& algn = *itcl;
        m_members.push_back(SChainMember());
        m_members.back().m_align = &(algn);
        
        if(algn.Score() == BadScore()) {
            if((algn.Status()&CGeneModel::ePolyA) != 0) {
                m_members.back().m_type = (algn.Strand() == ePlus) ?  eRightUTR : eLeftUTR;
            } else if((algn.Status()&CGeneModel::eCap) != 0) {
                m_members.back().m_type = (algn.Strand() == ePlus) ?  eLeftUTR : eRightUTR;
            } else {
                SChainMember& lutr = m_members.back();
                lutr.m_type = eLeftUTR;
                SChainMember rutr = lutr;
                rutr.m_type = eRightUTR;
                rutr.m_copy = &lutr;
                rutr.m_isduplicate = true;
                m_members.push_back(rutr);
                lutr.m_copy = &m_members.back();
            }
        }
    }

    NON_CONST_ITERATE(vector<SChainMember>, i, m_members)
        push_back(&*i);
}

void CChainMembers::DuplicateUnrestricted5Prime(TGeneModelList& clust, const CResidueVec& seq)
{
    for(int i = (int)m_members.size()-1; i >= 0; --i) {
        CGeneModel& algn = *m_members[i].m_align;
        
        if(algn.HasStart()) {
            bool inf_5prime;
            if (algn.Strand()==ePlus) {
                inf_5prime = algn.GetCdsInfo().MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom();
            } else {
                inf_5prime = algn.GetCdsInfo().MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo();
            }
            
            if (inf_5prime) {
                CGeneModel algn_with_shortcds = algn;
                CCDSInfo cdsinfo = algn_with_shortcds.GetCdsInfo();

                TSignedSeqPos first_start = GetFirstStart(algn_with_shortcds, seq);
                _ASSERT(first_start >= 0);

                cdsinfo.Set5PrimeCdsLimit(first_start);
                algn_with_shortcds.SetCdsInfo(cdsinfo);

                _ASSERT( !CodingCompatible(algn, algn_with_shortcds,  seq) );

                clust.push_front(algn_with_shortcds);
                
                SChainMember& longcds = m_members[i];
                SChainMember shortcds = longcds;
                shortcds.m_align = &clust.front();
                shortcds.m_copy = &longcds;
                shortcds.m_isduplicate = true;
                m_members.push_back(shortcds);              // enough space is reserved in constructor
                longcds.m_copy = &m_members.back();
                push_back(&m_members.back());
            }
        }
    }        
}

void CChainMembers::FillGapsInAlignments(const CResidueVec& seq)
{
    sort(begin(),end(),ScoreOrder());

    for (unsigned i = 0; i < size(); ++i) {
        SChainMember& mi = *(*this)[i];
        CGeneModel& ai = *mi.m_align;
        if ((ai.Type()&CGeneModel::eProt)==0 && ai.Continuous())
            continue;

        bool was_filled = false;
        do {
            for (unsigned j = 0; j < size(); ++j) {
                if (i == j) continue;
                SChainMember& mj = *(*this)[j];
                CGeneModel& aj = *mj.m_align;
                
                if (ai.isCompatible(aj) && CodingCompatible(ai, aj, seq, false) && FillGaps(ai, aj)) {
                    was_filled = true;
                    _ASSERT( ai.isCompatible(aj) && CodingCompatible(ai, aj, seq, false) );
                    if(ai.Continuous())
                        break;
                }
            }
        } while (was_filled && !ai.Continuous());
    }
}


// input alignments (clust parameter) should be all of the same strand
void MakeOneStrandChains(TGeneModelList& clust, list<CChain>& chains,
                CGnomonEngine& gnomon, int intersect_limit, const SMinScor& minscor, int trim)
{
    
    if(clust.empty()) return;

    CChainMembers pointers(clust);
    pointers.FillGapsInAlignments(gnomon.GetSeq());
    pointers.DuplicateUnrestricted5Prime(clust, gnomon.GetSeq());
    MergeIdenticalSingleExon(pointers);
    FindContainedAlignments(pointers, gnomon);
    LeftRight(pointers, intersect_limit, gnomon);
    RightLeft(pointers, intersect_limit, gnomon);

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_contained.size();
    }

    sort(pointers.begin(),pointers.end(),NumOrder());

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        if(mi.m_included) continue;

        set<SChainMember*> chain_alignments;
        
        mi.CollectAllContainedAlignments(chain_alignments);
        chains.push_back( CChain(chain_alignments, gnomon, minscor, trim) );
    }
}


double GoodCDNAScore(const CGeneModel& algn, const SMinScor& minscor)
{
    if(((algn.Type()&CGeneModel::eProt)!=0 || algn.ConfirmedStart()) && algn.FShiftedLen(algn.GetCdsInfo().ProtReadingFrame(),true) > minscor.m_prot_cds_len) return 0.99*BadScore();
    
    int intron_left = 0, intron_internal = 0, intron_total =0;
    for(int i = 1; i < (int)algn.Exons().size(); ++i) {
        if(!algn.Exons()[i-1].m_ssplice || !algn.Exons()[i].m_fsplice) continue;
        
        ++intron_total;
        if(algn.Exons()[i].GetFrom()-1 < algn.RealCdsLimits().GetFrom()) ++intron_left; 
        if(algn.Exons()[i-1].GetTo()+1 > algn.RealCdsLimits().GetFrom() && algn.Exons()[i].GetFrom()-1 < algn.RealCdsLimits().GetTo()) ++intron_internal; 
    }
    
    int intron_3p, intron_5p;
    if(algn.Strand() == ePlus) {
        intron_5p = intron_left;
        intron_3p = intron_total -intron_5p - intron_internal; 
    } else {
        intron_3p = intron_left;
        intron_5p = intron_total -intron_3p - intron_internal; 
    }

    int cdslen = algn.RealCdsLen();
    int len = algn.AlignLen();

    //    return  max(0.,25+7*intron_5p+14*intron_3p-0.05*cdslen+0.005*len);
    return  max(0.,minscor.m_min+minscor.m_i5p_penalty*intron_5p+minscor.m_i3p_penalty*intron_3p-minscor.m_cds_bonus*cdslen+minscor.m_length_penalty*len);
}       


void RemovePoorCds(CGeneModel& algn, double minscor)
{
    if (algn.Score() < minscor)
        algn.SetCdsInfo(CCDSInfo());
}

struct AlignSeqOrder
{
    bool operator()(const CGeneModel* ap, const CGeneModel* bp)
    {
        if (ap->Limits().GetFrom() != bp->Limits().GetFrom()) return ap->Limits().GetFrom() < bp->Limits().GetFrom();
        if (ap->Limits().GetTo() != bp->Limits().GetTo()) return ap->Limits().GetTo() > bp->Limits().GetTo();
        return ap->ID() < bp->ID(); // to make sort deterministic
    }
};


struct LeftEndOrder
{
    bool operator()(const CGeneModel* ap, const CGeneModel* bp)
    {
        if(ap->Limits().GetFrom() != bp->Limits().GetFrom()) return ap->Limits().GetFrom() < bp->Limits().GetFrom();
        return ap->ID() < bp->ID(); // to make sort deterministic
    }
};

struct RightEndOrder
{
    bool operator()(const CGeneModel* ap, const CGeneModel* bp)
    {
        if(ap->Limits().GetTo() != bp->Limits().GetTo()) return ap->Limits().GetTo() < bp->Limits().GetTo();
        return ap->ID() < bp->ID(); // to make sort deterministic
    }
};


CChain::CChain(const set<SChainMember*>& chain_alignments, const CGnomonEngine& gnomon, const SMinScor& minscor, int trim)
{
    _ASSERT(chain_alignments.size()>0);
    ITERATE (set<SChainMember*>, it, chain_alignments)
        m_members.push_back((*it)->m_align);
    sort(m_members.begin(),m_members.end(),AlignSeqOrder());

    EStrand strand = (*chain_alignments.begin())->m_align->Strand();
    SetStrand(strand);

    vector<CSupportInfo> support;
    support.reserve(m_members.size());
    int last_important_support;
    const CGeneModel* last_important_align = 0;
    
    TSignedSeqRange conf_start;
    TSignedSeqPos rf, fivep; 

    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;

        if(align.ConfirmedStart()) {
            if(Strand() == ePlus) {
                if(conf_start.Empty() || align.GetCdsInfo().Start().GetFrom() < conf_start.GetFrom()) {
                    conf_start = align.GetCdsInfo().Start();
                    rf = align.ReadingFrame().GetFrom();
                    fivep = conf_start.GetFrom();
                }
            } else {
                if(conf_start.Empty() || align.GetCdsInfo().Start().GetTo() > conf_start.GetTo()) {
                    conf_start = align.GetCdsInfo().Start();
                    rf = align.ReadingFrame().GetTo();
                    fivep = conf_start.GetTo();
                }
            }
        }

        support.push_back(CSupportInfo(align.ID(),false));
        
        if(Limits().Empty() || !Include(Limits(),align.Limits())) {

            bool add_some_introns = false;
            for(int i = (int)align.Exons().size()-1; !add_some_introns && i > 0; --i) {
                add_some_introns = align.Exons()[i-1].m_ssplice && align.Exons()[i].m_fsplice && 
                    (Limits().Empty() || align.Exons()[i-1].GetTo() >= Limits().GetTo());
           }

            bool pass_some_introns = false;
            for(int i = 1; last_important_align != 0 && !pass_some_introns && i < (int)(last_important_align->Exons().size()); ++i) {
                pass_some_introns = last_important_align->Exons()[i-1].m_ssplice && last_important_align->Exons()[i].m_fsplice && 
                    align.Limits().GetFrom() >= last_important_align->Exons()[i].GetFrom();
           }

            if(add_some_introns) {
                if(pass_some_introns) {
                    support[last_important_support].SetCore(true);
                }
                last_important_support = support.size()-1;
                last_important_align = *it;
            }
        }

        Extend(align, false);
    }
    if(last_important_align != 0) 
       support[last_important_support].SetCore(true);
    ITERATE(vector<CSupportInfo>, s, support)
        AddSupport(*s);


    // add back trim
    
    for(int ia = 0; ia < (int)m_members.size(); ++ia)  {
        if((m_members[ia]->Type() & eProt)==0 && (m_members[ia]->Status() & CGeneModel::eLeftTrimmed)!=0 &&
           Exons().front().Limits().GetFrom() == m_members[ia]->Limits().GetFrom()) {
            ExtendLeft( trim );
            break;
        }
    }
     
    for(int ia = 0; ia < (int)m_members.size(); ++ia)  {
        if((m_members[ia]->Type() & eProt)==0 && (m_members[ia]->Status() & CGeneModel::eRightTrimmed)!=0 &&
           Exons().back().Limits().GetTo() == m_members[ia]->Limits().GetTo()) {
            ExtendRight( trim );
            break;
        }
    }

    gnomon.GetScore(*this);

    if(ReadingFrame().NotEmpty() && !ConfirmedStart() && conf_start.NotEmpty()) {
        TSignedSeqRange extra_cds = (Strand() == ePlus) ? TSignedSeqRange(RealCdsLimits().GetFrom(),conf_start.GetFrom()) : TSignedSeqRange(conf_start.GetTo(),RealCdsLimits().GetTo());
        if(FShiftedLen(extra_cds, false) < 0.2*RealCdsLen() && Continuous()) {
            string comment;

            AddComment(comment+"Restored confirmed start");
            CCDSInfo cds = GetCdsInfo();
            TSignedSeqRange reading_frame = cds.ReadingFrame();
            if(Strand() == ePlus) 
                reading_frame.SetFrom(rf);
            else
                reading_frame.SetTo(rf);
            CCDSInfo::TPStops pstops = cds.PStops();
            cds.ClearPStops();
            ITERATE(vector<TSignedSeqRange>, s, pstops) {
                if(Include(reading_frame, *s))
                    cds.AddPStop(*s);
            }
            cds.SetReadingFrame(reading_frame,true);
            cds.SetStart(conf_start,true);
            cds.Set5PrimeCdsLimit(fivep);
            SetCdsInfo(cds);          
            gnomon.GetScore(*this);
        }
    }


    _ASSERT(ReadingFrame().Empty() || Score() != BadScore());

    double ms = GoodCDNAScore(*this, minscor);
    if ((Type() & eProt)==0 && !ConfirmedStart()) 
        RemovePoorCds(*this,ms);

    TInDels fs;
    ITERATE(TInDels, i, FrameShifts()) {   // removing fshifts in UTRs
        if (i->IntersectingWith(MaxCdsLimits().GetFrom(), MaxCdsLimits().GetTo()))
            fs.push_back(*i);
    }
    FrameShifts() = fs;

    int polya = Strand() == ePlus ? 0 : numeric_limits<int>::max();
    int cap = Strand() == eMinus ? 0 : numeric_limits<int>::max();
    bool foundpolya = false;
    bool foundcap = false;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        if((align.Status()&CGeneModel::ePolyA) != 0) {
            foundpolya = true;
            if(Strand() == ePlus) {
                polya = max(polya, align.Limits().GetTo());
            } else {
                polya = min(polya,align.Limits().GetFrom()); 
            }
        }
        if((align.Status()&CGeneModel::eCap) != 0) {
            foundcap = true;
            if(Strand() == eMinus) {
                cap = max(cap, align.Limits().GetTo());
            } else {
                cap = min(cap,align.Limits().GetFrom()); 
            }
        }
    }
  
    vector<CGeneModel*> left_end_order(m_members);
    sort(left_end_order.begin(),left_end_order.end(),LeftEndOrder());

    vector<CGeneModel*> right_end_order(m_members);
    sort(right_end_order.begin(),right_end_order.end(),RightEndOrder());
    
    int num = m_members.size();
    int polya_fuzz = 25;
    int cap_fuzz = 25;
    if(foundpolya) {
        if(Strand() == ePlus && Include(Exons().back().Limits(),polya) && (ReadingFrame().Empty() || (HasStop() && polya > RealCdsLimits().GetTo()))) {   // polya in the last exon and not in cds
            int i = num-1;
            for ( ; i >= 0 && right_end_order[i]->Exons().size() == 1 && polya < right_end_order[i]->Limits().GetTo(); --i) ;    // only single-exon alignments extend polya
            if(polya > right_end_order[i]->Limits().GetTo()-polya_fuzz){
                Status() |= CGeneModel::ePolyA;
                if(right_end_order[i]->Limits().GetTo() < Limits().GetTo()) {
                    AddComment("polyaclip");
                    CutExons(TSignedSeqRange(polya+1,Limits().GetTo()));
                    RecalculateLimits();
                }
            } else {
                AddComment("lostpolya");
            }
        } else if(Strand() == eMinus && Include(Exons().front().Limits(),polya) && (ReadingFrame().Empty() || (HasStop() && polya < RealCdsLimits().GetFrom()))) {   // polya in the last exon and not in cds
            int i = 0;
            for ( ; i < num && left_end_order[i]->Exons().size() == 1 && polya > left_end_order[i]->Limits().GetFrom(); ++i) ;    // only single-exon alignments extend polya
            if(polya < left_end_order[i]->Limits().GetFrom()+polya_fuzz){
                Status() |= CGeneModel::ePolyA;
                if(left_end_order[i]->Limits().GetFrom() > Limits().GetFrom()){
                    AddComment("polyaclip");
                    CutExons(TSignedSeqRange(Limits().GetFrom(),polya-1));
                    RecalculateLimits();
                }
            } else {
                AddComment("lostpolya");
            }
        }
    } 
    if(foundcap) {
        if(Strand() == ePlus && Include(Exons().front().Limits(),cap) && (ReadingFrame().Empty() || (HasStart() && cap < RealCdsLimits().GetFrom()))) {   // cap in the first exon and not in cds        
            int i = 0;
            for ( ; i < num && left_end_order[i]->Exons().size() == 1 && cap > left_end_order[i]->Limits().GetFrom(); ++i) ;    // only single-exon alignments extend cap
            if(cap < left_end_order[i]->Limits().GetFrom()+cap_fuzz){
                Status() |= CGeneModel::eCap;
                if(left_end_order[i]->Limits().GetFrom() > Limits().GetFrom()){
                    AddComment("capclip");
                    CutExons(TSignedSeqRange(Limits().GetFrom(),cap-1));
                    RecalculateLimits();
                }
            } else {
                AddComment("lostcap");
            }
        } else if(Strand() == eMinus && Include(Exons().back().Limits(),cap) && (ReadingFrame().Empty() || (HasStart() && cap > RealCdsLimits().GetTo()))) {   // cap in the first exon and not in cds   
            int i = num-1;
            for ( ; i >= 0 && right_end_order[i]->Exons().size() == 1 && cap < right_end_order[i]->Limits().GetTo(); --i) ;    // only single-exon alignments extend cap
            if(cap > right_end_order[i]->Limits().GetTo()-cap_fuzz){
                Status() |= CGeneModel::eCap;
                if(right_end_order[i]->Limits().GetTo() < Limits().GetTo()){
                    AddComment("capclip");
                    CutExons(TSignedSeqRange(cap+1,Limits().GetTo()));
                    RecalculateLimits();
                }
            } else {
                AddComment("lostcap");
            }
        }
    }
        

    _ASSERT( FShiftedLen(GetCdsInfo().Start()+ReadingFrame()+GetCdsInfo().Stop(), false)%3==0 );
}



pair<string,int> GetAccVer(const string& ident)
{
    istringstream istr(ident);
    string accession;
    int i = 0;
    while(i < 4 && getline(istr,accession,'|')) 
    {
        if(!accession.empty()) ++i;
    }
    if(i < 4) accession = ident;
    
    string::size_type dot = accession.find('.');
    int version = 0;
    if(dot != string::npos) 
    {
        version = atoi(accession.substr(dot+1).c_str());
        accession = accession.substr(0,dot);
    }
    
    return make_pair(accession,version);
}

static int s_ExonLen(const CGeneModel& a);

static bool s_ByAccVerLen(const CAlignModel& a, const CAlignModel& b)
    {
        pair<string,int> a_acc = GetAccVer(a.TargetAccession());
        pair<string,int> b_acc = GetAccVer(b.TargetAccession());
        int acc_cmp = NStr::CompareCase(a_acc.first,b_acc.first);
        if (acc_cmp != 0)
            return acc_cmp<0;
        if (a_acc.second != b_acc.second)
            return a_acc.second > b_acc.second;
        
        int a_len = s_ExonLen(a);
        int b_len = s_ExonLen(b);
        if (a_len!=b_len)
            return a_len > b_len;
        if (a.ConfirmedStart() != b.ConfirmedStart())
            return a.ConfirmedStart();
        return a.ID() < b.ID(); // to make sort deterministic
    }

static int s_ExonLen(const CGeneModel& a)
    {
        int len = 0;
        ITERATE(CGeneModel::TExons, e, a.Exons())
            len += e->Limits().GetLength();
        return len;
    }

typedef map<int,CAlignModel*> TOrigAligns;

void SkipReason(CGeneModel* orig_align, const string& comment)
{
    orig_align->Status() |= CGeneModel::eSkipped;
    orig_align->AddComment(comment);
}

void FilterOverlappingSameAccessionAlignment(TGeneModelList& clust, TOrigAligns& orig_aligns)
{
    //    clust.sort(s_ByAccVerLen);  this is moved up

    TGeneModelList::iterator first = clust.begin();
    TGeneModelList::iterator current = first; ++current;
    pair<string,int> first_accver = GetAccVer(orig_aligns[first->ID()]->TargetAccession());
    while (current != clust.end()) {
        pair<string,int> current_accver = GetAccVer(orig_aligns[current->ID()]->TargetAccession());
        if (first_accver.first == current_accver.first) {
            if (current->Limits().IntersectingWith(first->Limits())) {
                SkipReason(orig_aligns[current->ID()],"Overlaps the same alignment");
                current = clust.erase(current);
            } else {
                ++current;
            }
        } else {
            ++first;
            if (first==current) {
                first_accver = current_accver;
            } else {
                first_accver = GetAccVer(orig_aligns[first->ID()]->TargetAccession()); 
                current = first;
            }
            current = first; ++current;
        }
    }
}

void FilterOutPoorAlignments(TGeneModelList& clust,
                            const CGnomonEngine& gnomon, const SMinScor& minscor,
                             TOrigAligns& orig_aligns)
{
    clust.sort(s_BySizePosition);

    static CGeneModel dummy_align;
    CGeneModel* prev_align = &dummy_align;
    
    for(TGeneModelList::iterator itcl = clust.begin(); itcl != clust.end(); ) {

        CGeneModel& algn = *itcl;
        CAlignModel* orig = orig_aligns[algn.ID()];

        bool same_as_prev = algn.IdenticalAlign(*prev_align);
        
        if (same_as_prev && (prev_align->Status() & CGeneModel::eSkipped)!=0) {
            const string& reason =  prev_align->GetComment();
            SkipReason(prev_align = orig, reason);
            itcl = clust.erase(itcl);
            continue;
        }

        if (!same_as_prev && algn.Exons().size() > 1) {
            bool skip = false;
            const char* skip_reason = ""; 
            bool found_holes = false;
            for(unsigned int i = 1; i < algn.Exons().size(); ++i) {
                bool hole = !algn.Exons()[i-1].m_ssplice || !algn.Exons()[i].m_fsplice;
                if (hole) found_holes = true;
                int intron = algn.Exons()[i].GetFrom()-algn.Exons()[i-1].GetTo()-1;
                if (!hole && intron < gnomon.GetMinIntronLen()) {
                    skip = true;
                    skip_reason = "short intron";
                    break;
                } 
            }
            if (skip) {
                SkipReason(prev_align = orig, skip_reason);
                itcl = clust.erase(itcl);
                continue;
            }
            
            if((algn.Type() & CGeneModel::eProt)!=0 && found_holes) {
                string found_multiple;
                set<string> compatible_evidence;
                int len = algn.AlignLen();

                const CGeneModel* prev_alignp = &dummy_align;
                bool prev_is_compatible = false;
                NON_CONST_ITERATE(TGeneModelList, jtcl, clust) {
                    if(jtcl == itcl) continue;
                    CGeneModel& algnj = *jtcl;
                    if(algnj.AlignLen() < len/4) continue;
                        
                    bool same_as_prev = algnj.IdenticalAlign(*prev_alignp);
                    if (!same_as_prev)
                        prev_alignp = &algnj;
                        
                    if (same_as_prev && prev_is_compatible || !same_as_prev && algn.Strand()==algnj.Strand() && algn.isCompatible(algnj)) {
                        prev_is_compatible = true;
                        if(!compatible_evidence.insert(orig_aligns[algnj.ID()]->TargetAccession()).second) {
                            found_multiple = orig_aligns[algnj.ID()]->TargetAccession();
                            break;
                        }
                    } else
                        prev_is_compatible = false;
                }
                if (!found_multiple.empty()) {
                    SkipReason(prev_align = orig, "Multiple inclusion "+found_multiple);
                    itcl = clust.erase(itcl);
                    continue;
                }
            }
        }

        if ((algn.Type() & CGeneModel::eProt)!=0 || algn.ConfirmedStart()) {   // this includes protein alignments and mRNA with confirmed CDSes

            if (algn.Score()==BadScore()) {
                if (same_as_prev && !algn.ConfirmedStart()) {
                    CCDSInfo cdsinfo = prev_align->GetCdsInfo();
                    algn.SetCdsInfo(cdsinfo);
                } else {
                    gnomon.GetScore(algn);
                }
            }
           
            double ms = GoodCDNAScore(algn, minscor);

            if(algn.Score() == BadScore() || (algn.Score() < ms && (algn.Type() & CGeneModel::eProt) != 0 && orig->TargetLen() != 0 && algn.AlignLen() < minscor.m_minprotfrac*orig->TargetLen())) { // all mRNA with confirmed CDS and reasonably aligned proteins with known length will get through with any finite score 
                CNcbiOstrstream ost;
                if(algn.AlignLen() <= 75)
                    ost << "Short alignment " << algn.AlignLen();
                else
                    ost << "Low score " << algn.Score();
                SkipReason(prev_align = orig, CNcbiOstrstreamToString(ost));
                itcl = clust.erase(itcl);
                continue;
            }

            /*
            if((algn.Type() & CAlignModel::eProt)==0) { // mRNA with confirmed start - convert to noncoding if poor score
                RemovePoorCds(algn,ms);
            } else if(algn.Score() < ms) {             // protein - delete if poor score
                CNcbiOstrstream ost;
                if(algn.AlignLen() <= 75)
                    ost << "Short alignment " << algn.AlignLen();
                else
                    ost << "Low score " << algn.Score();
                SkipReason(prev_align = orig_aligns[algn.ID()], CNcbiOstrstreamToString(ost));
                itcl = clust.erase(itcl);
                continue;
            }
            */
        }
        prev_align = &algn;
        ++itcl;
    }
}   


struct SFShiftsCluster {
    SFShiftsCluster(TSignedSeqRange limits = TSignedSeqRange::GetEmpty()) : m_limits(limits) {}
    TSignedSeqRange m_limits;
    TInDels    m_fshifts;
    bool operator<(const SFShiftsCluster& c) const { return m_limits.GetTo() < c.m_limits.GetFrom(); }
};

bool AddIfCompatible(set<SFShiftsCluster>& fshift_clusters, const CGeneModel& algn)
{
    typedef vector<SFShiftsCluster> TFShiftsClusterVec;
    typedef set<SFShiftsCluster>::iterator TIt;

    TFShiftsClusterVec algn_fclusters;
    algn_fclusters.reserve(algn.Exons().size());

    {
        const TInDels& fs = algn.FrameShifts();
        TInDels::const_iterator fi = fs.begin();
        ITERATE (CGeneModel::TExons, e, algn.Exons()) {
            algn_fclusters.push_back(SFShiftsCluster(e->Limits()));
            while(fi != fs.end() && fi->IntersectingWith(e->GetFrom(),e->GetTo())) {
                algn_fclusters.back().m_fshifts.push_back(*fi++);
            }
        }
    }

    ITERATE(TFShiftsClusterVec, exon_cluster, algn_fclusters) {
        pair<TIt,TIt> eq_rng = fshift_clusters.equal_range(*exon_cluster);
        for(TIt glob_cluster = eq_rng.first; glob_cluster != eq_rng.second; ++glob_cluster) {
            ITERATE(TInDels, fi, glob_cluster->m_fshifts)
                if (find(exon_cluster->m_fshifts.begin(),exon_cluster->m_fshifts.end(),*fi) == exon_cluster->m_fshifts.end())
                    if (fi->IntersectingWith(exon_cluster->m_limits.GetFrom(),exon_cluster->m_limits.GetTo()))
                        return false;
            ITERATE(TInDels, fi, exon_cluster->m_fshifts)
                if (find(glob_cluster->m_fshifts.begin(),glob_cluster->m_fshifts.end(),*fi) == glob_cluster->m_fshifts.end())
                    if (fi->IntersectingWith(glob_cluster->m_limits.GetFrom(),glob_cluster->m_limits.GetTo()))
                        return false;
        }
    }
    NON_CONST_ITERATE(TFShiftsClusterVec, exon_cluster, algn_fclusters) {
        pair<TIt,TIt> eq_rng = fshift_clusters.equal_range(*exon_cluster);
        for(TIt glob_cluster = eq_rng.first; glob_cluster != eq_rng.second;) {
            exon_cluster->m_limits += glob_cluster->m_limits;
            exon_cluster->m_fshifts.insert(exon_cluster->m_fshifts.end(),glob_cluster->m_fshifts.begin(),glob_cluster->m_fshifts.end());
            fshift_clusters.erase(glob_cluster++);
        }
        uniq(exon_cluster->m_fshifts);
        fshift_clusters.insert(eq_rng.second, *exon_cluster);
    }
    return true;
}

bool FsTouch(const TSignedSeqRange& lim, const CInDelInfo& fs) {
    if(fs.IsInsertion() && fs.Loc()+fs.Len() == lim.GetFrom())
        return true;
    if(fs.IsDeletion() && fs.Loc() == lim.GetFrom())
        return true;
    if(fs.Loc() == lim.GetTo()+1)
        return true;

    return false;
}

void FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(TGeneModelList& clust, TOrigAligns& orig_aligns)
{
    clust.sort(s_ScoreOrder);
    set<SFShiftsCluster> fshift_clusters;

    for (TGeneModelList::iterator itcl=clust.begin(); itcl != clust.end(); ) {
        CGeneModel& algn = *itcl;
        bool skip = false;
        
        if(((algn.Type() & CGeneModel::eProt)!=0 || algn.ConfirmedStart())) {
            skip = !AddIfCompatible(fshift_clusters,algn);
            
            for (TGeneModelList::iterator it = clust.begin(); !skip && it != itcl; ++it) {
                if((it->Type()&CGeneModel::eProt) == 0)
                    continue;
                ITERATE(TInDels, fi, it->FrameShifts()) {
                    if(FsTouch(algn.ReadingFrame(), *fi))
                       skip = true;
                }
                ITERATE(TInDels, fi, algn.FrameShifts()) {
                    if(FsTouch(it->ReadingFrame(), *fi))
                       skip = true;
                }
            }
        }

        if(skip) {
            SkipReason(orig_aligns[itcl->ID()], "incompatible indels3");
            itcl=clust.erase(itcl);
        } else
            ++itcl;
    }
}


void AddFShiftsAndScoreCdnas(TGeneModelList& clust, const TInDels& fshifts, const CGnomonEngine& gnomon, const SMinScor& minscor, TOrigAligns& orig_aligns)    
{      
    for (TGeneModelList::iterator itcl=clust.begin(); itcl != clust.end(); ) {
        CGeneModel& algn = *itcl;
        if((algn.Type() & CGeneModel::eProt)!=0 || algn.ConfirmedStart()) {
            ++itcl;
            continue;
        }

        TInDels algn_fs;
        bool incompatible_indels = false;
        TSignedSeqPos la = algn.Limits().GetFrom();
        TSignedSeqPos lb = algn.Limits().GetTo();

        // trim ends with fshifts (we want at least 3 bases without fs and no touching)
        ITERATE(TInDels, fs, fshifts) {
            if(fs->IsInsertion() && fs->Loc()<=la+2 && fs->Loc()+fs->Len()>=la)  
                la = fs->Loc()+fs->Len()+1;
            if(fs->IsInsertion() && fs->Loc()<=lb+1 && fs->Loc()+fs->Len()>=lb-1)
                lb = fs->Loc()-2;
            if(fs->IsDeletion() && fs->Loc()<=la+2 && fs->Loc()>=la-1)
                la = fs->Loc()+1;
            if(fs->IsDeletion() && fs->Loc()>=lb-2 && fs->Loc()<=lb+1)
                lb = fs->Loc()-2;
        }
        if(la != algn.Limits().GetFrom() || lb != algn.Limits().GetTo()) {
            algn.Clip(TSignedSeqRange(la,lb), CGeneModel::eRemoveExons);
            la = algn.Limits().GetFrom();
            lb = algn.Limits().GetTo();
            if(algn.Exons().empty())
                incompatible_indels = true;
        }
        
        ITERATE(TInDels, fs, fshifts) {
            if(!fs->IntersectingWith(la,lb))
                continue; 
            bool found = false;
            for(unsigned int k = 0; k < algn.Exons().size() && !found; ++k) {
                int a = algn.Exons()[k].GetFrom();
                int b = algn.Exons()[k].GetTo();
                if (fs->IntersectingWith(a,b)) {
                    found = true;
                    if (algn.Exons()[k].m_fsplice && fs->Loc()<a || algn.Exons()[k].m_ssplice && fs->IsInsertion() && b<fs->Loc()+fs->Len()-1) {
                        incompatible_indels = true;
                    }
                } 
            } 
            if (found)
                algn_fs.push_back(*fs);
        }

        if (incompatible_indels) {
            SkipReason(orig_aligns[algn.ID()], "incompatible indels1");
            itcl=clust.erase(itcl);
            continue;
        }

        algn.FrameShifts() = algn_fs;

        gnomon.GetScore(algn);
        double ms = GoodCDNAScore(algn, minscor);
        RemovePoorCds(algn,ms);

        if((algn.Status()&CGeneModel::ePolyA) != 0 && algn.ReadingFrame().NotEmpty() && !algn.HasStop())      // if there is no stop kill PolyA tail
            algn.Status() ^= CGeneModel::ePolyA;           

        ++itcl;
    }
}


void CollectFShifts(const TGeneModelList& clust, TInDels& fshifts)
{
    ITERATE (TGeneModelList, itcl, clust) {
        const TInDels& fs = itcl->FrameShifts();
        fshifts.insert(fshifts.end(),fs.begin(),fs.end());
    }
    sort(fshifts.begin(),fshifts.end());
            
    if (!fshifts.empty())
        uniq(fshifts);
}


void SplitAlignmentsByStrand(const TGeneModelList& clust, TGeneModelList& clust_plus, TGeneModelList& clust_minus)
{            
    ITERATE (TGeneModelList, itcl, clust) {
        const CGeneModel& algn = *itcl;

        if (algn.Strand() == ePlus)
            clust_plus.push_back(algn);
        else
            clust_minus.push_back(algn);
    }
}

// void MergeCompatibleChains(list<CChain>& chains, const CGnomonEngine& gnomon, const SMinScor& minscor, int trim)
// {
//     bool keep_doing = true;
//     const CResidueVec& seq = gnomon.GetSeq();
//     while(keep_doing) {
//         keep_doing = false;
//         for(list<CChain>::iterator it_chain = chains.begin(); it_chain != chains.end(); ) {
//             CChain& isorted_chain(*it_chain);

//             list<CChain>::iterator jt_chain = it_chain;
//             bool compatible = false;
//             for(++jt_chain; jt_chain != chains.end(); ++jt_chain) {
//                 CChain& jsorted_chain(*jt_chain);

//                 int intersect = (isorted_chain.Limits() & jsorted_chain.Limits()).GetLength();
//                 if(intersect < 2*trim+1) continue;
                
//                 if(isorted_chain.isCompatible(jsorted_chain)>1 && CodingCompatible(isorted_chain, jsorted_chain, seq)) {
//                     jsorted_chain.MergeWith(isorted_chain,gnomon,minscor);
//                     compatible = true;
//                     keep_doing = true;
//                 }
//             }
//             if(compatible) it_chain = chains.erase(it_chain);
//             else ++it_chain;
//         }
//     }
// } 


void ProjectCDS_ConvertToGeneModel(const map<string,TSignedSeqRange>& mrnaCDS, const CResidueVec& seq, CAlignModel& align, double mininframefrac) {
    if ((align.Type()&CAlignModel::eProt)!=0)
        return;
        
    if ((align.Type()&CAlignModel::emRNA)!=0 && (align.Status()&CAlignModel::eReversed)==0) {
        string accession = align.TargetAccession();
        map<string,TSignedSeqRange>::const_iterator pos = mrnaCDS.find(accession);
        if(pos != mrnaCDS.end()) {
            TSignedSeqRange cds_on_mrna = pos->second;
            CAlignMap alignmap(align.GetAlignMap());
            TSignedSeqPos left = alignmap.MapEditedToOrig(cds_on_mrna.GetFrom());
            TSignedSeqPos right = alignmap.MapEditedToOrig(cds_on_mrna.GetTo());
            if(align.Strand() == eMinus) {
                swap(left,right);
            }
            
            CGeneModel a = align;
            align.FrameShifts().clear();
            
            if(left < 0 || right < 0)     // start or stop cannot be projected  
                return;
            if(left < align.Limits().GetFrom() || right >  align.Limits().GetTo())    // cds is clipped
                return;
            
            a.Clip(TSignedSeqRange(left,right),CGeneModel::eRemoveExons);

            //            ITERATE(TInDels, fs, a.FrameShifts()) {
            //                if(fs->Len()%3 != 0) return;          // there is a frameshift    
            //            }

            if(!a.FrameShifts().empty()) {
                CAlignMap cdsmap(a.GetAlignMap());
                int inframelength = 0;
                int outframelength = 0;
                int frame = 0;
                TSignedSeqPos prev = left;
                ITERATE(TInDels, fs, a.FrameShifts()) {
                    int len = cdsmap.FShiftedLen(cdsmap.ShrinkToRealPoints(TSignedSeqRange(prev,fs->Loc()-1)),false);
                    if(frame == 0) {
                        inframelength += len;
                    } else {
                        outframelength += len;
                    }

                    if(fs->IsDeletion()) {
                        frame = (frame+fs->Len())%3;
                    } else {
                        frame = (3+frame-fs->Len()%3)%3;
                    }
                    prev = fs->Loc();    //  ShrinkToRealPoints will take care if it in insertion or intron  
                }
                int len = cdsmap.FShiftedLen(cdsmap.ShrinkToRealPoints(TSignedSeqRange(prev,right)),false);
                if(frame == 0) {
                    inframelength += len;
                } else {
                    outframelength += len;
                }
                if(outframelength > (1.- mininframefrac)*(inframelength + outframelength))
                    return;
            }
            
            a.FrameShifts().clear();                       // clear notshifted indels   
            CAlignMap cdsmap(a.GetAlignMap());
            CResidueVec cds;
            cdsmap.EditedSequence(seq,cds);
            unsigned int length = cds.size();
            
            if(length%3 != 0)
                return;
            
            if(!IsStartCodon(&cds[0]) || !IsStopCodon(&cds[length-3]) )   // start or stop on genome is not right
                return;

            for(unsigned int i = 0; i < length-3; i += 3) {
                if(IsStopCodon(&cds[i])) return;                // premature stop on genome
            }
            
            TSignedSeqRange reading_frame = cdsmap.MapRangeEditedToOrig(TSignedSeqRange(3,length-4));
            TSignedSeqRange start = cdsmap.MapRangeEditedToOrig(TSignedSeqRange(0,2));
            TSignedSeqRange stop = cdsmap.MapRangeEditedToOrig(TSignedSeqRange(length-3,length-1));

            CCDSInfo cdsinfo;
            cdsinfo.SetReadingFrame(reading_frame,true);
            cdsinfo.SetStart(start,true);
            cdsinfo.SetStop(stop,true);
            align.SetCdsInfo(cdsinfo);
            
            return;
        }
                
    }

    align.FrameShifts().clear();
}

void FilterOutBadScoreChainsHavingBetterCompatibles(TGeneModelList& chains)
{
            for(TGeneModelList::iterator it = chains.begin(); it != chains.end();) {
                TGeneModelList::iterator itt = it++;
                for(TGeneModelList::iterator jt = chains.begin(); jt != itt;) {
                    TGeneModelList::iterator jtt = jt++;
                    if(itt->Strand() != jtt->Strand() || (itt->Score() != BadScore() && jtt->Score() != BadScore())) continue;

                    // at least one score is BadScore
                    if(itt->Score() != BadScore()) {
                        if(itt->isCompatible(*jtt) > 1) chains.erase(jtt);
                    } else if(jtt->Score() != BadScore()) {
                        if(itt->isCompatible(*jtt) > 1) {
                            chains.erase(itt);
                            break;
                        }
                        
                    } else if(itt->AlignLen() > jtt->AlignLen()) {
                        if(itt->isCompatible(*jtt) > 0) chains.erase(jtt);
                    } else {
                        if(itt->isCompatible(*jtt) > 0) {
                            chains.erase(itt);
                            break;
                        }
                    }
                }
            }
}
          

struct TrimAlignment : public unary_function<CAlignModel&, CAlignModel&> {
public:
    TrimAlignment(int a_trim, const CResidueVec& a_seq, const map<string,TSignedSeqRange>& a_mrnaCDS) : trim(a_trim), seq(a_seq), mrnaCDS(a_mrnaCDS)  {}
    int trim;
    const CResidueVec& seq;
    const map<string,TSignedSeqRange>& mrnaCDS;

    TSignedSeqPos TrimCodingExonLeft(const CAlignModel& align, const CModelExon& e, int trim)
    {
        TSignedSeqPos old_from = e.GetFrom();
        TSignedSeqPos new_from = align.FShiftedMove(old_from, trim);
        _ASSERT( new_from-old_from >= trim && new_from <= e.GetTo() );

        return new_from;
    }

    TSignedSeqPos TrimCodingExonRight(const CAlignModel& align, const CModelExon& e, int trim)
    {
        TSignedSeqPos old_to = e.GetTo();
        TSignedSeqPos new_to = align.FShiftedMove(old_to, -trim);
        _ASSERT( old_to-new_to >= trim && new_to >= e.GetFrom() );

        return new_to;
    }

    CAlignModel& operator() (CAlignModel& align)
    {
        TSignedSeqRange limits = align.Limits();
        CAlignMap alignmap(align.GetAlignMap());

        if ((align.Type() & CAlignModel::eProt)!=0) {
            for (CAlignModel::TExons::const_iterator piece_begin = align.Exons().begin(); piece_begin != align.Exons().end(); ++piece_begin) {
                _ASSERT( !piece_begin->m_fsplice );

                CAlignModel::TExons::const_iterator piece_end;
                for (piece_end = piece_begin; piece_end != align.Exons().end() && piece_end->m_ssplice; ++piece_end) ;
                _ASSERT( piece_end != align.Exons().end() );

                TSignedSeqPos a;
                if (piece_begin == align.Exons().begin() && align.LeftComplete())
                    a = align.Limits().GetFrom();
                else
                    a = piece_begin->GetFrom()+trim;

                TSignedSeqPos b;
                if (piece_end->GetTo() >= align.Limits().GetTo() && align.RightComplete())
                    b = align.Limits().GetTo();
                else
                    b = piece_end->GetTo()-trim;

                if(a != piece_begin->GetFrom() || b != piece_end->GetTo()) {
                    TSignedSeqRange newlimits = alignmap.ShrinkToRealPoints(TSignedSeqRange(a,b),true);
                    _ASSERT(newlimits.NotEmpty() && piece_begin->GetTo() >= newlimits.GetFrom() && piece_end->GetFrom() <= newlimits.GetTo());
                    align.Clip(newlimits, CAlignModel::eDontRemoveExons);
                }

                piece_begin = piece_end;
            }
        } else {
            int a = align.Limits().GetFrom();
            int b = align.Limits().GetTo();
            if(align.Strand() == ePlus) {
                if((align.Status()&CGeneModel::eCap) == 0)
                    a += trim;
                if((align.Status()&CGeneModel::ePolyA) == 0)
                    b -= trim;
            } else {
                if((align.Status()&CGeneModel::ePolyA) == 0)
                    a += trim;
                if((align.Status()&CGeneModel::eCap) == 0)
                    b -= trim;
            }

            TSignedSeqRange newlimits = alignmap.ShrinkToRealPoints(TSignedSeqRange(a,b),false);
            _ASSERT(newlimits.NotEmpty() && align.Exons().front().GetTo() >= newlimits.GetFrom() && align.Exons().back().GetFrom() <= newlimits.GetTo());

            string accession = align.TargetAccession();
            map<string,TSignedSeqRange>::const_iterator pos = mrnaCDS.find(accession);
            if(pos != mrnaCDS.end() && alignmap.MapEditedToOrig(pos->second.GetFrom()) >= 0 && alignmap.MapEditedToOrig(pos->second.GetTo()) >= 0) {  // avoid trimming confirmed CDSes
                TSignedSeqRange cds_on_mrna = pos->second;
                CResidueVec mrna;
                alignmap.EditedSequence(seq, mrna, true);
                if(IsStartCodon(&mrna[cds_on_mrna.GetFrom()]) && IsStopCodon(&mrna[cds_on_mrna.GetTo()-2]) )  {  // start and stop on genome are right
                    TSignedSeqRange cds_on_genome = alignmap.MapRangeEditedToOrig(cds_on_mrna, false);
                    if(cds_on_genome.GetFrom() < newlimits.GetFrom()) {
                        a = align.Limits().GetFrom();
                        newlimits = alignmap.ShrinkToRealPoints(TSignedSeqRange(a,b),false);
                        _ASSERT(newlimits.NotEmpty() && align.Exons().front().GetTo() >= newlimits.GetFrom() && align.Exons().back().GetFrom() <= newlimits.GetTo());
                    }
                    if(cds_on_genome.GetTo() > newlimits.GetTo()) {
                        b = align.Limits().GetTo();
                        newlimits = alignmap.ShrinkToRealPoints(TSignedSeqRange(a,b),false);
                        _ASSERT(newlimits.NotEmpty() && align.Exons().front().GetTo() >= newlimits.GetFrom() && align.Exons().back().GetFrom() <= newlimits.GetTo());
                    }
                }
            }

            if(newlimits != align.Limits()) {
                align.Clip(newlimits,CAlignModel::eDontRemoveExons);    // Clip doesn't change AlignMap
            }
        }

        if(align.Limits().GetFrom() > limits.GetFrom()) align.Status() |= CAlignModel::eLeftTrimmed;
        if(align.Limits().GetTo() < limits.GetTo()) align.Status() |= CAlignModel::eRightTrimmed;

        return align;
    }
};

TGeneModelList MakeChains(TAlignModelList& alignments, CGnomonEngine& gnomon, const SMinScor& minscor,
                          int intersect_limit, int trim, size_t alignlimit, const map<string,TSignedSeqRange>& mrnaCDS, 
                          map<string, pair<bool,bool> >& prot_complet, double mininframefrac)
{
    transform(alignments.begin(), alignments.end(), alignments.begin(),
              TrimAlignment(trim, gnomon.GetSeq(), mrnaCDS));

    TGeneModelList chains;
    TGeneModelList models;
    TOrigAligns orig_aligns;
    //    int align_counter = 0;
    bool skipest = (alignments.size() > alignlimit);

    alignments.sort(s_ByAccVerLen);    // used in FilterOverlappingSameAccessionAlignment

    NON_CONST_ITERATE (TAlignModelCluster, i, alignments) {
        if(skipest && (i->Type() & CGeneModel::eEST)!=0)
            continue;
        CAlignModel aa = *i;
        ProjectCDS_ConvertToGeneModel(mrnaCDS, gnomon.GetSeq(), aa, mininframefrac);
        models.push_back(aa);
        //        models.back().SetID(++align_counter);
        orig_aligns[models.back().ID()]=&(*i);
    }

    if(models.empty()) return chains;

    FilterOverlappingSameAccessionAlignment(models, orig_aligns);
    FilterOutPoorAlignments(models, gnomon, minscor, orig_aligns);
    FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(models, orig_aligns);

    if(models.empty()) return chains;

    TInDels fshifts;
    CollectFShifts(models, fshifts);

    AddFShiftsAndScoreCdnas(models, fshifts, gnomon, minscor, orig_aligns);


//     set<int> orig_ids;
//     ITERATE(TGeneModelList, ia, models) {
//         int oid = orig_aligns[ia->ID()]->ID();
//         if(orig_ids.insert(oid).second) {   // don't output duplicates
//             CGeneModel a = *ia;
//             a.SetID(oid);
//             processed_evidence_out << a;
//         }
//     }

 
    TGeneModelList clust_plus, clust_minus;

    SplitAlignmentsByStrand(models, clust_plus, clust_minus);

    list<CChain> tmp_chains;
    
    MakeOneStrandChains(clust_plus, tmp_chains, gnomon, intersect_limit, minscor, trim);
    MakeOneStrandChains(clust_minus, tmp_chains, gnomon, intersect_limit, minscor, trim);

    for(list<CChain>::iterator it_chain = tmp_chains.begin(); it_chain != tmp_chains.end(); ++it_chain) {
        CChain& chain(*it_chain);

        CAlignMap mrnamap = chain.GetAlignMap();
        ITERATE(vector<CGeneModel*>, i, chain.m_members) {
            CAlignModel* orig_align = orig_aligns[(*i)->ID()];

            if((orig_align->Type() & CGeneModel::eProt) == 0 || orig_align->TargetLen() == 0)   // not a protein or not known length
                continue;

            if(!chain.ConfirmedStart() && chain.HasStart() && prot_complet[orig_align->TargetAccession()].first && Include(chain.Limits(),(*i)->Limits())) {  // protein has start
                TSignedSeqPos not_aligned =  orig_align->GetAlignMap().MapRangeOrigToEdited(orig_align->Limits(),false).GetFrom()-1;
                if(not_aligned <= (1.-minscor.m_minprotfrac)*orig_align->TargetLen()) {                                                         // well aligned
                    TSignedSeqPos extra_length = mrnamap.MapRangeOrigToEdited(orig_align->Limits(),false).GetFrom()-
                        mrnamap.MapRangeOrigToEdited(chain.GetCdsInfo().Start(),false).GetFrom()-1;
            
                    if(extra_length > not_aligned-minscor.m_endprotfrac*orig_align->TargetLen()) {
                        CCDSInfo cds_info = chain.GetCdsInfo();
                        cds_info.SetScore(cds_info.Score(), false);   // not open
                        cds_info.SetStart(cds_info.Start(), true);    // confirmed start
                        chain.SetCdsInfo(cds_info);
                    }
                }
            }

            if(!chain.ConfirmedStop() && chain.HasStop() && prot_complet[orig_align->TargetAccession()].second && Include(chain.Limits(),(*i)->Limits())) {  // protein has stop
                TSignedSeqPos not_aligned = orig_align->TargetLen()-orig_align->GetAlignMap().MapRangeOrigToEdited(orig_align->Limits(),false).GetTo();
                if(not_aligned <= (1.-minscor.m_minprotfrac)*orig_align->TargetLen()) {                                                         // well aligned
                    TSignedSeqPos extra_length = mrnamap.MapRangeOrigToEdited(chain.GetCdsInfo().Stop(),false).GetTo()-
                        mrnamap.MapRangeOrigToEdited(orig_align->Limits(),false).GetTo();
            
                    if(extra_length > not_aligned-minscor.m_endprotfrac*orig_align->TargetLen()) {
                        CCDSInfo cds_info = chain.GetCdsInfo();
                        cds_info.SetStop(cds_info.Stop(), true);    // confirmed stop   
                        chain.SetCdsInfo(cds_info);
                    }
                }
            }
        }

        chain.ClearEntrezmRNA();
        chain.ClearEntrezProt();
        if(chain.Continuous() && chain.ConfirmedStart() && chain.ConfirmedStop()) {
            ITERATE(vector<CGeneModel*>, i, chain.m_members) {
                if(Include(chain.Limits(),(*i)->Limits())) {
                    CAlignModel* orig_align = orig_aligns[(*i)->ID()];
                    if(((*i)->Continuous() && (*i)->ConfirmedStart() && (*i)->ConfirmedStop()) || (orig_align->AlignLen() > minscor.m_minprotfrac*orig_align->TargetLen())) {
                        if(!(*i)->EntrezmRNA().empty())
                            chain.InsertEntrezmRNA(*(*i)->EntrezmRNA().begin());
                        else if(!(*i)->EntrezProt().empty())
                            chain.InsertEntrezProt(*(*i)->EntrezProt().begin());
                    }
                }
            }

        }
    }


    for(list<CChain>::iterator it_chain = tmp_chains.begin(); it_chain != tmp_chains.end(); ++it_chain) {
        CChain& chain(*it_chain);
        chain.SetType(chain.Type() | CGeneModel::eChain);

        CSupportInfoSet orig_support;
        ITERATE(CSupportInfoSet, i, chain.Support()) {
            int id = i->GetId();
            CGeneModel* orig_align = orig_aligns[id];
            orig_support.insert(CSupportInfo(orig_align->ID(), i->IsCore()));
        }
        chain.ReplaceSupport(orig_support);

        chains.push_back(chain);
    }

    FilterOutBadScoreChainsHavingBetterCompatibles(chains);

    return chains;
}



END_SCOPE(gnomon)
END_SCOPE(ncbi)


