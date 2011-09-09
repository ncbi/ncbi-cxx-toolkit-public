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
#include <algo/gnomon/id_handler.hpp>
#include <algo/gnomon/gnomon_exception.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/gnomon.hpp>

#include <map>
#include <sstream>

#include <objects/general/Object_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(gnomon)

struct SChainMember;
class CChain;
typedef map<Int8,CAlignModel*> TOrigAligns;
struct SFShiftsCluster;
class CChainMembers;

class CChainer::CChainerImpl {
public:
    CChainerImpl(CRef<CHMMParameters>& hmm_params, auto_ptr<CGnomonEngine>& gnomon);
    void SetGenomicRange(const TAlignModelList& alignments);
    void DropAlignmentInfo(TAlignModelList& alignments, TGeneModelList& models);
    void CollapsSimilarESTandSR(TGeneModelList& clust);

    void FilterOutChimeras(TGeneModelList& clust);
    void ScoreCDSes_FilterOutPoorAlignments(TGeneModelList& clust);
    void FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(TGeneModelList& clust);
    void ReplicateFrameShifts(TGeneModelList& models);


    TGeneModelList MakeChains(TGeneModelList& models);
    void FilterOutBadScoreChainsHavingBetterCompatibles(TGeneModelList& chains);
    void CombineCompatibleChains(list<CChain>& chains);

private:
    void CutParts(TGeneModelList& clust);
    void IncludeInContained(SChainMember& big, SChainMember& small);
    void FindContainedAlignments(vector<SChainMember*>& pointers);
    void DuplicateNotOriented(CChainMembers& pointers, TGeneModelList& clust);
    void Duplicate5pendsAndShortCDSes(CChainMembers& pointers, TGeneModelList& clust);
    void ScoreCdnas(CChainMembers& pointers);
    void DuplicateUTRs(CChainMembers& pointers);
    void LeftRight(vector<SChainMember*>& pointers);
    void RightLeft(vector<SChainMember*>& pointers);
    //    void MakeOneStrandChains(TGeneModelList& clust, list<CChain>& chains);
    void MakeChains(TGeneModelList& clust, list<CChain>& chains);
    double GoodCDNAScore(const CGeneModel& algn);
    void RemovePoorCds(CGeneModel& algn, double minscor);
    void SkipReason(CGeneModel* orig_align, const string& comment);
    bool AddIfCompatible(set<SFShiftsCluster>& fshift_clusters, const CGeneModel& algn);
    bool FsTouch(const TSignedSeqRange& lim, const CInDelInfo& fs);
    void AddFShifts(TGeneModelList& clust, const TInDels& fshifts);
    void CollectFShifts(const TGeneModelList& clust, TInDels& fshifts);
    void SplitAlignmentsByStrand(const TGeneModelList& clust, TGeneModelList& clust_plus, TGeneModelList& clust_minus);

    CRef<CHMMParameters>& m_hmm_params;
    auto_ptr<CGnomonEngine>& m_gnomon;


    SMinScor minscor;
    int intersect_limit;
    int trim;
    map<string,TSignedSeqRange> mrnaCDS;
    map<string, pair<bool,bool> > prot_complet;
    double mininframefrac;
    int minpolya;

    TOrigAligns orig_aligns;

    friend class CChainer;
};

CGnomonAnnotator_Base::CGnomonAnnotator_Base()
    : m_masking(false)
{
}

CGnomonAnnotator_Base::~CGnomonAnnotator_Base()
{
}

void CGnomonAnnotator_Base::EnableSeqMasking()
{
    m_masking = true;
}

CChainer::CChainer()
{
    m_data.reset( new CChainerImpl(m_hmm_params, m_gnomon) );
}

CChainer::~CChainer()
{
}

CChainer::CChainerImpl::CChainerImpl(CRef<CHMMParameters>& hmm_params, auto_ptr<CGnomonEngine>& gnomon)
    :m_hmm_params(hmm_params), m_gnomon(gnomon)
{
}

TGeneModelList CChainer::MakeChains(TGeneModelList& models)
{
    return m_data->MakeChains(models);
}

enum {
    eCDS,
    eLeftUTR,
    eRightUTR
};

typedef vector<SChainMember*> TContained;

typedef set<const CGeneModel*> TEvidence;

struct SChainMember
{
    SChainMember() :
        m_align(0), m_align_map(0), m_left_member(0), m_right_member(0),
        m_copy(0), m_left_num(0), m_right_num(0), m_num(0),
        m_type(eCDS), m_left_cds(0), m_right_cds(0), m_cds(0), m_included(false),  m_postponed(false),
        m_marked_for_deletion(false), m_marked_for_retention(false) {}

    void CollectContainedAlignments(set<SChainMember*>& chain_alignments);
    void MarkIncludedContainedAlignments();
    void MarkPostponedContainedAlignments();
    void CollectAllContainedAlignments(set<SChainMember*>& chain_alignments);
    void MarkIncludedAllContainedAlignments();
    void MarkPostponedAllContainedAlignments();
    void MarkExtraCopiesForDeletion(const TSignedSeqRange& cds);
    void MarkAllExtraCopiesForDeletion(const TSignedSeqRange& cds);

    CGeneModel* m_align;
    CAlignMap* m_align_map;
    SChainMember* m_left_member;
    SChainMember* m_right_member;
    TContained* m_copy;      // is used to make sure that the copy of already incuded duplicated alignment is not included in contained and doesn't trigger a new chain genereation
    TContained m_contained;
    double m_left_num, m_right_num, m_num; 
    int m_type, m_left_cds, m_right_cds, m_cds;
    bool m_included;
    bool m_postponed;
    bool m_marked_for_deletion;
    bool m_marked_for_retention;
#ifdef _DEBUG
    int m_mem_id;
#endif
};

class CChain : public CGeneModel
{
public:
    CChain(const set<SChainMember*>& chain_alignments);
    //    void MergeWith(const CChain& another_chain, const CGnomonEngine&gnomon, const SMinScor& minscor);
    void ToEvidence(vector<TEvidence>& mrnas,
                    vector<TEvidence>& ests, 
                    vector<TEvidence>& prots) const ;

    void RestoreTrimmedEnds(int trim);
    void RemoveFshiftsFromUTRs();
    void RestoreReasonableConfirmedStart(const CGnomonEngine& gnomon);
    void ClipToCompleteAlignment(EStatus determinant); // determinant - cap or polya

    void SetConfirmedStartStopForCompleteProteins(map<string, pair<bool,bool> >& prot_complet, TOrigAligns& orig_aligns, const SMinScor& minscor);
    void CollectTrustedmRNAsProts(TOrigAligns& orig_aligns, const SMinScor& minscor);

    vector<CGeneModel*> m_members;
};



void SChainMember::CollectContainedAlignments(set<SChainMember*>& chain_alignments)
{
    NON_CONST_ITERATE (TContained, i, m_contained) {
        SChainMember* mi = *i;
        chain_alignments.insert(mi);
    }
}

void SChainMember::MarkIncludedContainedAlignments()
{
    NON_CONST_ITERATE (TContained, i, m_contained) {
        SChainMember* mi = *i;
        mi->m_included = true;
        if (mi->m_copy != 0) {
            ITERATE(TContained, j, *mi->m_copy) {
                SChainMember* mj = *j;
                mj->m_included = true;
            }
        }
    }
}

void SChainMember::MarkPostponedContainedAlignments()
{
    NON_CONST_ITERATE (TContained, i, m_contained) {
        SChainMember* mi = *i;
        mi->m_postponed = true;
        if (mi->m_copy != 0) {
            ITERATE(TContained, j, *mi->m_copy) {
                SChainMember* mj = *j;
                mj->m_postponed = true;
            }
        }
    }
}

void SChainMember::MarkExtraCopiesForDeletion(const TSignedSeqRange& cds)
{
    NON_CONST_ITERATE (TContained, i, m_contained) {
        SChainMember* mi = *i;
        CGeneModel& algn = *mi->m_align;
        CAlignMap& amap = *mi->m_align_map;

        bool retain = false;
        if(Include(cds,algn.Limits()))   // all alignment in cds
            retain = true;
        else if(algn.Exons().size() > 1 && Include(cds, TSignedSeqRange(algn.Exons().front().GetTo(),algn.Exons().back().GetFrom())))   // all introns in cds
            retain = true;
        else if(amap.FShiftedLen(algn.RealCdsLimits(), false) > 0.5*amap.FShiftedLen(algn.Limits(), false))  // most of the alignment in cds
            retain = true;

        if(retain) {
            mi->m_marked_for_retention = true;
            mi->m_marked_for_deletion = false;
            if (mi->m_copy != 0) {
                ITERATE(TContained, j, *mi->m_copy) {
                    SChainMember* mj = *j;
                    if(!mj->m_marked_for_retention)
                        mj->m_marked_for_deletion = true;
                }
            }
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

void SChainMember::MarkIncludedAllContainedAlignments()
{
    MarkIncludedContainedAlignments();
    
    for (SChainMember* left = m_left_member; left != 0; left = left->m_left_member) {
        left->MarkIncludedContainedAlignments();
    }
        
    for (SChainMember* right = m_right_member; right != 0; right = right->m_right_member) {
        right->MarkIncludedContainedAlignments();
    }
}        

void SChainMember::MarkPostponedAllContainedAlignments()
{
    MarkPostponedContainedAlignments();
    
    for (SChainMember* left = m_left_member; left != 0; left = left->m_left_member) {
        left->MarkPostponedContainedAlignments();
    }
        
    for (SChainMember* right = m_right_member; right != 0; right = right->m_right_member) {
        right->MarkPostponedContainedAlignments();
    }
}        

void SChainMember::MarkAllExtraCopiesForDeletion(const TSignedSeqRange& cds)
{
    MarkExtraCopiesForDeletion(cds);
    
    for (SChainMember* left = m_left_member; left != 0; left = left->m_left_member) {
        left->MarkExtraCopiesForDeletion(cds);
    }
        
    for (SChainMember* right = m_right_member; right != 0; right = right->m_right_member) {
        right->MarkExtraCopiesForDeletion(cds);
    }
}        

struct LeftOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)    // right end increasing, short first if right end equal
    {
        const TSignedSeqRange& alimits = ap->m_align->Limits();
        const TSignedSeqRange& blimits = bp->m_align->Limits();
        if(alimits.GetTo() == blimits.GetTo()) 
            return (alimits.GetFrom() > blimits.GetFrom());
        else 
            return (alimits.GetTo() < blimits.GetTo());
    }
};

struct LeftOrderD                                                      // use for sorting not for finding
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)    // right end increasing, short first if right end equal
    {
        if(ap->m_align->Limits() == bp->m_align->Limits()) 
            return ap->m_align->ID() < bp->m_align->ID(); // to make sort deterministic
        else 
            return LeftOrder()(ap,bp);
    }
};


struct RightOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)   // left end decreasing, short first if left end equal
    {
        const TSignedSeqRange& alimits = ap->m_align->Limits();
        const TSignedSeqRange& blimits = bp->m_align->Limits();
        if(alimits.GetFrom() == blimits.GetFrom())
            return (alimits.GetTo() < blimits.GetTo());
        else
            return (alimits.GetFrom() > blimits.GetFrom());
    }
};

struct RightOrderD
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)   // left end decreasing, short first if left end equal
    {
        if(ap->m_align->Limits() == bp->m_align->Limits()) 
            return ap->m_align->ID() < bp->m_align->ID(); // to make sort deterministic
        else 
            return RightOrder()(ap,bp);
    }
};


struct CdsNumOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)
    {
        if(max(ap->m_cds,bp->m_cds) >= 300 && ap->m_cds != bp->m_cds) // only long cdses count
            return (ap->m_cds > bp->m_cds);
        else if(ap->m_num == bp->m_num)
            return ap->m_align->ID() < bp->m_align->ID(); // to make sort deterministic
        else
            return (ap->m_num > bp->m_num);
    }
};

static bool s_ByExonNumberAndLocation(const CGeneModel& a, const CGeneModel& b)
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

class CChainMembers : public vector<SChainMember*> {
public:
    CChainMembers(TGeneModelList& clust);
    void InsertMember(CGeneModel& algn, SChainMember* copy_ofp = 0);
    void InsertMember(SChainMember& m, SChainMember* copy_ofp = 0); 
    void DuplicateUTR(SChainMember* copy_ofp); 
private:
    CChainMembers(const CChainMembers& object) { }
    CChainMembers& operator=(const CChainMembers& object) { return *this; }    
    list<SChainMember> m_members;
    list<TContained> m_copylist;
    list<CAlignMap> m_align_maps;
};

void CChainMembers::InsertMember(CGeneModel& algn, SChainMember* copy_ofp)
{
    SChainMember mbr;
    mbr.m_align = &algn;
    mbr.m_type = eCDS;
    if(algn.Score() == BadScore()) 
        mbr.m_type = eLeftUTR;
    InsertMember(mbr, copy_ofp);

    /*
    if(algn.Score() == BadScore()) {
        mbr.m_type = eLeftUTR;
        if((algn.Status()&CGeneModel::ePolyA) != 0 && algn.Strand() == ePlus)
            mbr.m_type = eRightUTR;
        if((algn.Status()&CGeneModel::eCap) != 0 && algn.Strand() == eMinus)
            mbr.m_type = eRightUTR;
    }
    InsertMember(mbr, copy_ofp);

    if(algn.Score() == BadScore() && (algn.Status()&(CGeneModel::ePolyA|CGeneModel::eCap)) == 0) { // right UTR copy
        mbr.m_type = eRightUTR;
        InsertMember(mbr,&m_members.back());
    }
    */
}

void CChainMembers::InsertMember(SChainMember& m, SChainMember* copy_ofp)
{
#ifdef _DEBUG
    m.m_mem_id = size();
#endif

    m_members.push_back(m);
    push_back(&m_members.back());

    _ASSERT(copy_ofp == 0 || (m.m_align->Exons()==copy_ofp->m_align->Exons() && m.m_align->FrameShifts()==copy_ofp->m_align->FrameShifts()));

    if(copy_ofp == 0 || m.m_align->Strand() != copy_ofp->m_align->Strand()) {         // first time or reversed copy
        m_align_maps.push_back(CAlignMap(m.m_align->Exons(), m.m_align->FrameShifts(), m.m_align->Strand()));
        m_members.back().m_align_map = &m_align_maps.back();
    } else {
        m_members.back().m_align_map = copy_ofp->m_align_map;
    }

    if(copy_ofp != 0) {                                            // we are making a copy of member
        if(copy_ofp->m_copy == 0) {
            m_copylist.push_back(TContained(1,copy_ofp));
            copy_ofp->m_copy = &m_copylist.back();
        }
        m_members.back().m_copy = copy_ofp->m_copy;
        copy_ofp->m_copy->push_back(&m_members.back());
    }
}

void CChainMembers::DuplicateUTR(SChainMember* copy_ofp)
{
    _ASSERT(copy_ofp->m_type == eLeftUTR);
    SChainMember new_mbr = *copy_ofp;
    new_mbr.m_type = eRightUTR;
    InsertMember(new_mbr, copy_ofp);
}

CChainMembers::CChainMembers(TGeneModelList& clust)
{
    NON_CONST_ITERATE(TGeneModelList, itcl, clust) 
        InsertMember(*itcl);
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


bool CodingCompatible(const SChainMember& ma, const SChainMember& mb)
{
    const CGeneModel& a = *ma.m_align; 
    const CGeneModel& b = *mb.m_align;

    _ASSERT( a.ReadingFrame().NotEmpty() && b.ReadingFrame().NotEmpty() );
    
    TSignedSeqRange max_cds_limits = a.GetCdsInfo().MaxCdsLimits() & b.GetCdsInfo().MaxCdsLimits();

    if (!Include(max_cds_limits, ExtendedMaxCdsLimits(a) + ExtendedMaxCdsLimits(b)))
        return false;
    
    TSignedSeqRange reading_frame_limits(max_cds_limits.GetFrom()+1,max_cds_limits.GetTo()-1); // clip to exclude possible start/stop
    
    if (!Include(reading_frame_limits, a.ReadingFrame() + b.ReadingFrame()))
        return false;
        
    TSignedSeqPos common_point = CommonReadingFramePoint(a, b);
    if(common_point < 0)
        return false;

    const CGeneModel* pa = &a;
    const CGeneModel* pb = &b;

    const CAlignMap* amap = ma.m_align_map;
    const CAlignMap* bmap = mb.m_align_map;

    
    if(common_point < pa->ReadingFrame().GetFrom() || pb->ReadingFrame().GetTo() < common_point) {
        swap(pa,pb);
        swap(amap,bmap);
    }

    _ASSERT( pa->ReadingFrame().GetFrom() <= common_point);
    _ASSERT( common_point <= pb->ReadingFrame().GetTo() );
    
    int a_start_to_b_end =
        amap->FShiftedLen(pa->ReadingFrame().GetFrom(),common_point, true)+
        bmap->FShiftedLen(common_point,pb->ReadingFrame().GetTo(), true)-1;
    
    return (a_start_to_b_end % 3 == 0);
}



void CChainer::CChainerImpl::IncludeInContained(SChainMember& big, SChainMember& small)
{
    big.m_contained.push_back(&small);
}


void CChainer::CutParts(TGeneModelList& models)
{
    m_data->CutParts(models);
}

void CChainer::CChainerImpl::CutParts(TGeneModelList& clust) {
    for(TGeneModelList::iterator iloop = clust.begin(); iloop != clust.end(); ) {
        TGeneModelList::iterator im = iloop++;

        bool cut = false;
        int left = im->Limits().GetFrom();
        for(unsigned int i = 1; i < im->Exons().size(); ++i) {
            if (!im->Exons()[i-1].m_ssplice || !im->Exons()[i].m_fsplice) {
                CGeneModel m = *im;
                m.Clip(TSignedSeqRange(left,im->Exons()[i-1].GetTo()),CGeneModel::eRemoveExons);
                clust.push_front(m);

                cut = true;
                left = im->Exons()[i].GetFrom();
            }
        }

        if(cut) {
            CGeneModel m = *im;
            m.Clip(TSignedSeqRange(left,im->Limits().GetTo()),CGeneModel::eRemoveExons);
            clust.push_front(m);
            clust.erase(im);
        }
    }
}

void CChainer::CChainerImpl::DuplicateNotOriented(CChainMembers& pointers, TGeneModelList& clust)
{
    unsigned int initial_size = pointers.size();
    for(unsigned int i = 0; i < initial_size; ++i) {
        SChainMember& mbr = *pointers[i];
        CGeneModel& algn = *mbr.m_align;
        if((algn.Status()&CGeneModel::eUnknownOrientation) != 0) {
            algn.Status() &= (~CGeneModel::eUnknownOrientation);
            CGeneModel new_algn = algn;
            new_algn.SetStrand(algn.Strand() == ePlus ? eMinus : ePlus);
            clust.push_back(new_algn);
            pointers.InsertMember(clust.back(), &mbr);    //reversed copy     
        }
    }
}

void CChainer::CChainerImpl::DuplicateUTRs(CChainMembers& pointers)
{
    unsigned int initial_size = pointers.size();
    for(unsigned int i = 0; i < initial_size; ++i) {
        SChainMember& mbr = *pointers[i];
        CGeneModel& algn = *mbr.m_align;
        if(algn.Score() == BadScore()) 
            pointers.DuplicateUTR(&mbr);
    }
}


void CChainer::CChainerImpl::ScoreCdnas(CChainMembers& pointers)
{
    NON_CONST_ITERATE(CChainMembers, i, pointers) {
        SChainMember& mbr = **i;
        CGeneModel& algn = *mbr.m_align;

        _ASSERT((algn.Status()&CGeneModel::eUnknownOrientation) == 0);

        if((algn.Type() & CGeneModel::eProt)!=0 || algn.ConfirmedStart()) {
            continue;
        }

        m_gnomon->GetScore(algn);
        double ms = GoodCDNAScore(algn);
        RemovePoorCds(algn,ms);
        
        if(algn.Score() != BadScore())
            mbr.m_type = eCDS;
    }
}


void CChainer::CChainerImpl::Duplicate5pendsAndShortCDSes(CChainMembers& pointers, TGeneModelList& clust)
{
    unsigned int initial_size = pointers.size();
    for(unsigned int i = 0; i < initial_size; ++i) {
        SChainMember& mbr = *pointers[i];
        CGeneModel& algn = *mbr.m_align;

        if(mbr.m_type == eRightUTR)   // avoid copying UTR copies
            continue;

        if(algn.GetCdsInfo().ProtReadingFrame().Empty() && algn.Score() < 5*minscor.m_min) {
            for(int i = 0; i < (int)algn.GetEdgeReadingFrames()->size(); ++i) {
                CCDSInfo cds_info = (*algn.GetEdgeReadingFrames())[i];
                if(cds_info.ReadingFrame() != algn.ReadingFrame()) {
                    CGeneModel new_algn = algn;
                    new_algn.SetCdsInfo(cds_info);
                    clust.push_back(new_algn);
                    pointers.InsertMember(clust.back(), &mbr);    //copy with CDS       
                }
            }

            if(algn.Score() != BadScore()) {
                CGeneModel new_algn = algn;
                new_algn.SetCdsInfo(CCDSInfo());
                clust.push_back(new_algn);
                pointers.InsertMember(clust.back(), &mbr);    //UTR copy        
            }
        }
    }
    

    initial_size = pointers.size();
    for(unsigned int i = 0; i < initial_size; ++i) {
        SChainMember& mbr = *pointers[i];
        CGeneModel& algn = *mbr.m_align;
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
                    
                TSignedSeqPos start = (algn.Strand() == ePlus) ? algn.RealCdsLimits().GetFrom() : algn.RealCdsLimits().GetTo();            
                cdsinfo.Set5PrimeCdsLimit(start);
                algn_with_shortcds.SetCdsInfo(cdsinfo);

                clust.push_back(algn_with_shortcds);
                pointers.InsertMember(clust.back(), &mbr);

                cdsinfo = algn.GetCdsInfo();
                if(algn.Strand() == ePlus) {
                    int full_rf_left = algn.Limits().GetFrom()+(algn.FShiftedLen(algn.Limits().GetFrom(), cdsinfo.Start().GetFrom(), false)-1)%3;
                    cdsinfo.SetStart(TSignedSeqRange::GetEmpty());
                    cdsinfo.SetReadingFrame(TSignedSeqRange(full_rf_left,cdsinfo.ReadingFrame().GetTo()));
                    algn.SetCdsInfo(cdsinfo);                        
                } else {
                    int full_rf_right = algn.Limits().GetTo()-(algn.FShiftedLen(cdsinfo.Start().GetTo(),algn.Limits().GetTo(),false)-1)%3;
                    cdsinfo.SetStart(TSignedSeqRange::GetEmpty());
                    cdsinfo.SetReadingFrame(TSignedSeqRange(cdsinfo.ReadingFrame().GetFrom(),full_rf_right));
                    algn.SetCdsInfo(cdsinfo);                        
                }
            }
        }
    }
}

void CChainer::CChainerImpl::FindContainedAlignments(vector<SChainMember*>& pointers)
{

//  finding contained subalignments (alignment is contained in itself)

    sort(pointers.begin(),pointers.end(),LeftOrderD());

    TIVec right_ends(pointers.size());
    for(int k = 0; k < (int)pointers.size(); ++k)
        right_ends[k] = pointers[k]->m_align->Limits().GetTo();
    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        TSignedSeqRange ai_limits = ai.Limits();
        TSignedSeqPos   ai_to = ai_limits.GetTo();
        TSignedSeqRange initial_reading_frame(ai.ReadingFrame());
        _ASSERT((ai.Status()&CGeneModel::eUnknownOrientation) == 0);

        TIVec::iterator lb = lower_bound(right_ends.begin(),right_ends.end(),ai.Limits().GetFrom());
        vector<SChainMember*>::iterator jfirst = pointers.begin();
        if(lb != right_ends.end())
            jfirst = pointers.begin()+(lb-right_ends.begin()); // skip all on thge left side
        for(vector<SChainMember*>::iterator j = jfirst; j != pointers.end(); ++j) {
            SChainMember& mj = **j;
            CGeneModel& aj = *mj.m_align;
            if(aj.Strand() != ai.Strand())
                continue;

            if(aj.Limits().GetTo() > ai_to) break;

            if(i == j)
                IncludeInContained(mi, mj);          // include self no matter what
            else if (mi.m_copy != 0 && ai.Limits() == aj.Limits() && find(mi.m_copy->begin(),mi.m_copy->end(),&mj) != mi.m_copy->end()) 
                continue; // don't include any self copy
            else if(mj.m_type  == eRightUTR)
                continue;    // avoid including UTR copy
            else if(mi.m_type != eCDS && mj.m_type == eCDS)
                continue;   // avoid including CDS into UTR because that will change m_type
            //            else if(mi.m_type == eCDS && mj.m_type != eCDS && aj.Limits().IntersectingWith(ai.MaxCdsLimits()))  // UTR in CDS
            else if(mi.m_type == eCDS && mj.m_type != eCDS && (aj.Limits()&ai.MaxCdsLimits()).GetLength() > 5)  // UTR in CDS
                continue;
            else if(!aj.IsSubAlignOf(ai))
                continue;
            else if(mi.m_type == eCDS && mj.m_type == eCDS &&  !CodingCompatible(mj, mi))  // CDS in CDS
                continue;
            else 
                IncludeInContained(mi, mj);
        }
    }
}

#define START_BONUS 350

void CChainer::CChainerImpl::LeftRight(vector<SChainMember*>& pointers)
{
    sort(pointers.begin(),pointers.end(),LeftOrderD());
    TIVec right_ends(pointers.size());
    for(int k = 0; k < (int)pointers.size(); ++k)
        right_ends[k] = pointers[k]->m_align->Limits().GetTo();
    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        const CCDSInfo& ai_cds_info = ai.GetCdsInfo();
        TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();
        TSignedSeqRange ai_limits = ai.Limits();

        mi.m_num = 0;
        ITERATE(TContained, ic, mi.m_contained)
            mi.m_num += (*ic)->m_align->Weight();
        mi.m_cds = mi.m_align_map->FShiftedLen(ai_rf,false);
        if(ai.HasStart()) {
            mi.m_cds += START_BONUS;
            _ASSERT((ai.Strand() == ePlus && ai_cds_info.Start().GetFrom() == ai_cds_info.MaxCdsLimits().GetFrom()) || 
            (ai.Strand() == eMinus && ai_cds_info.Start().GetTo() == ai_cds_info.MaxCdsLimits().GetTo()));
        }

        mi.m_left_member = 0;
        mi.m_left_num = mi.m_num;
        mi.m_left_cds =  mi.m_cds;
        sort(mi.m_contained.begin(),mi.m_contained.end(),LeftOrderD());
       
        TIVec::iterator lb = lower_bound(right_ends.begin(),right_ends.end(),ai.Limits().GetFrom());
        vector<SChainMember*>::iterator jfirst = pointers.begin();
        if(lb != right_ends.end())
            jfirst = pointers.begin()+(lb-right_ends.begin()); // skip all on the left side
        for(vector<SChainMember*>::iterator j = jfirst; j < i; ++j) {
            SChainMember& mj = **j;
            CGeneModel& aj = *mj.m_align;

            if(aj.Strand() != ai.Strand())
                continue;

            const CCDSInfo& aj_cds_info = aj.GetCdsInfo();
            TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();
 
            switch(mi.m_type)
            {
                case eCDS: 
                    if(mj.m_type == eRightUTR) 
                        continue;
                    else if(mj.m_type == eLeftUTR && (!ai.LeftComplete() || (aj.Limits()&ai_rf).GetLength() > 5))
                        continue;
                    else 
                        break;
                case eLeftUTR: 
                    if(mj.m_type != eLeftUTR) 
                        continue;
                    else 
                        break;
                case eRightUTR:
                    if(mj.m_type == eLeftUTR) 
                        continue;
                    else if(mj.m_type == eCDS && (!aj.RightComplete() || (ai.Limits()&aj_rf).GetLength() > 5))
                        continue;
                    else 
                        break;
                default:
                    continue;    
            }

            switch(ai.MutualExtension(aj))
            {
                case 0:              // not compatible 
                    continue;
                case 1:              // no introns in intersection
                {
                    if(mi.m_type == eCDS && mj.m_type == eCDS)  // no intersecting limit for coding
                        break;

                    int intersection_len = (ai_limits & aj.Limits()).GetLength(); 
                    if (intersection_len < intersect_limit) continue;
                    break;
                }
                default:             // one or more introns in intersection
                    break;
            }
            
            int cds_overlap = 0;

            if(mi.m_type == eCDS && mj.m_type == eCDS) {
                int genome_overlap =  ai_rf.GetLength()+aj_rf.GetLength()-(ai_rf+aj_rf).GetLength();
                if(genome_overlap < 0)
                    continue;

                TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();

                if (!Include(max_cds_limits, ExtendedMaxCdsLimits(ai) + ExtendedMaxCdsLimits(aj)))
                    continue;

                if((Include(ai_rf,aj_rf) || Include(aj_rf,ai_rf)) && ai_rf.GetFrom() != aj_rf.GetFrom() && ai_rf.GetTo() != aj_rf.GetTo())
                    continue;

                cds_overlap = mi.m_align_map->FShiftedLen(ai_rf&aj_rf,false);
                if(cds_overlap%3 != 0)
                    continue;

                if(ai.HasStart() && aj.HasStart())
                    cds_overlap += START_BONUS; 
            }

            int delta_cds = mi.m_cds-cds_overlap;
            int newcds = mj.m_left_cds+delta_cds;

            TContained::iterator endsp = upper_bound(mi.m_contained.begin(),mi.m_contained.end(),&mj,LeftOrder()); // first alignmnet contained in ai and outside aj
            double delta = 0;
            for(TContained::iterator ic = endsp; ic != mi.m_contained.end(); ++ic) 
                delta += (*ic)->m_align->Weight();
            double newnum = mj.m_left_num+delta;

            if(newcds > mi.m_left_cds || (newcds == mi.m_left_cds && newnum > mi.m_left_num)) {
                mi.m_left_cds = newcds;
                mi.m_left_num = newnum;
                mi.m_left_member = &mj;
                _ASSERT(aj.Limits().GetFrom() < ai.Limits().GetFrom() && aj.Limits().GetTo() < ai.Limits().GetTo());
            }    
        }
    }
}

void CChainer::CChainerImpl::RightLeft(vector<SChainMember*>& pointers)
{
    sort(pointers.begin(),pointers.end(),RightOrderD());
    TIVec left_ends(pointers.size());
    for(int k = 0; k < (int)pointers.size(); ++k)
        left_ends[k] = pointers[k]->m_align->Limits().GetFrom();
    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
    
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        const CCDSInfo& ai_cds_info = ai.GetCdsInfo();
        TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();
        TSignedSeqRange ai_limits = ai.Limits();

        mi.m_right_member = 0;
        mi.m_right_num = mi.m_num;
        mi.m_right_cds =  mi.m_cds;
        sort(mi.m_contained.begin(),mi.m_contained.end(),RightOrderD());
        
        TIVec::iterator lb = lower_bound(left_ends.begin(),left_ends.end(),ai.Limits().GetTo(),greater<int>()); // first potentially intersecting
        vector<SChainMember*>::iterator jfirst = pointers.begin();
        if(lb != left_ends.end())
            jfirst = pointers.begin()+(lb-left_ends.begin()); // skip all on thge right side
        for(vector<SChainMember*>::iterator j = jfirst; j < i; ++j) {
            SChainMember& mj = **j;
            CGeneModel& aj = *mj.m_align;

            if(aj.Strand() != ai.Strand())
                continue;

            const CCDSInfo& aj_cds_info = aj.GetCdsInfo();
            TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();
            
            switch(mi.m_type)
            {
                case eCDS: 
                    if(mj.m_type == eLeftUTR) 
                        continue;
                    if(mj.m_type == eRightUTR && (!ai.RightComplete() || (aj.Limits()&ai_rf).GetLength() > 5))
                        continue;
                    else 
                        break;
                case eRightUTR: 
                    if(mj.m_type != eRightUTR) 
                        continue;
                    else 
                        break;
                case eLeftUTR:
                    if(mj.m_type == eRightUTR) 
                        continue;
                    if(mj.m_type == eCDS && (!aj.LeftComplete() || (ai.Limits()&aj_rf).GetLength() > 5))
                        continue;
                    else 
                        break;
                default:
                    continue;    
            }

            switch(ai.MutualExtension(aj))
            {
                case 0:              // not compatible 
                    continue;
                case 1:              // no introns in intersection
                {
                    if(mi.m_type == eCDS && mj.m_type == eCDS)  // no intersecting limit for coding
                        break;

                    int intersect = (ai_limits & aj.Limits()).GetLength(); 
                    if(intersect < intersect_limit) continue;
                    break;
                }
                default:             // one or more introns in intersection
                    break;
            }
            
            int cds_overlap = 0;

            if(mi.m_type == eCDS && mj.m_type == eCDS) {
                int genome_overlap =  ai_rf.GetLength()+aj_rf.GetLength()-(ai_rf+aj_rf).GetLength();
                if(genome_overlap < 0)
                    continue;

                TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();

                if (!Include(max_cds_limits, ExtendedMaxCdsLimits(ai) + ExtendedMaxCdsLimits(aj)))
                    continue;

                if((Include(ai_rf,aj_rf) || Include(aj_rf,ai_rf)) && ai_rf.GetFrom() != aj_rf.GetFrom() && ai_rf.GetTo() != aj_rf.GetTo())
                    continue;

                cds_overlap = mi.m_align_map->FShiftedLen(ai_rf&aj_rf,false);
                if(cds_overlap%3 != 0)
                    continue;

                if(ai.HasStart() && aj.HasStart())
                    cds_overlap += START_BONUS; 
            }


            int delta_cds = mi.m_cds-cds_overlap;
            int newcds = mj.m_right_cds+delta_cds;
            
            TContained::iterator endsp = upper_bound(mi.m_contained.begin(),mi.m_contained.end(),&mj,RightOrder()); // first alignmnet contained in ai and outside aj
            double delta = 0;
            for(TContained::iterator ic = endsp; ic != mi.m_contained.end(); ++ic) 
                delta += (*ic)->m_align->Weight();
            double newnum = mj.m_right_num+delta;

            if(newcds > mi.m_right_cds || (newcds == mi.m_right_cds && newnum > mi.m_right_num)) {
                mi.m_right_cds = newcds;
                mi.m_right_num = newnum;
                mi.m_right_member = &mj;
                _ASSERT(aj.Limits().GetFrom() > ai.Limits().GetFrom() && aj.Limits().GetTo() > ai.Limits().GetTo());
            }    
        }
    }
}



/*
#include <stdio.h>
#include <time.h>
    time_t seconds0   = time (NULL);
    time_t seconds1   = time (NULL);
    cerr << "Time1: " << (seconds1-seconds0)/60. << endl;
*/


bool MemberIsCoding(const SChainMember* mp) {
    return (mp->m_align->Score() != BadScore());
}

bool MemberIsMarkedForDeletion(const SChainMember* mp) {
    return mp->m_marked_for_deletion;
}

// input alignments (clust parameter) should be all of the same strand
//void CChainer::CChainerImpl::MakeOneStrandChains(TGeneModelList& clust, list<CChain>& chains)
void CChainer::CChainerImpl::MakeChains(TGeneModelList& clust, list<CChain>& chains)
{
    if(clust.empty()) return;

    CChainMembers pointers(clust);

    DuplicateNotOriented(pointers, clust);
    ScoreCdnas(pointers);
    Duplicate5pendsAndShortCDSes(pointers, clust);
    DuplicateUTRs(pointers);
    FindContainedAlignments(pointers);

    vector<SChainMember*> coding_pointers;
    ITERATE(CChainMembers, i, pointers) {
        if(MemberIsCoding(*i)) 
            coding_pointers.push_back(*i); 
    }
    LeftRight(coding_pointers);
    RightLeft(coding_pointers);
    NON_CONST_ITERATE(vector<SChainMember*>, i, coding_pointers) {
        SChainMember& mi = **i;
        mi.m_cds = mi.m_left_cds+mi.m_right_cds-mi.m_cds;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
    }
    sort(coding_pointers.begin(),coding_pointers.end(),CdsNumOrder());
    NON_CONST_ITERATE(vector<SChainMember*>, i, coding_pointers) {
        SChainMember& mi = **i;
        if(mi.m_included) continue;

        set<SChainMember*> chain_alignments;
        mi.CollectAllContainedAlignments(chain_alignments);
        mi.MarkIncludedAllContainedAlignments();
        CChain chain(chain_alignments);
        m_gnomon->GetScore(chain);

        if(chain.GetCdsInfo().ProtReadingFrame().NotEmpty() || chain.Score() > 2*minscor.m_min) {
            mi.MarkAllExtraCopiesForDeletion(chain.RealCdsLimits());
            //            chains.push_back(chain);
        }
    }
   
    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsMarkedForDeletion),pointers.end());  // wrong orientaition/UTR/frames are removed


    LeftRight(pointers);
    RightLeft(pointers);

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        mi.m_included = false;
        mi.m_cds = mi.m_left_cds+mi.m_right_cds-mi.m_cds;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
    }

    sort(pointers.begin(),pointers.end(),CdsNumOrder());

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        if(mi.m_included || mi.m_postponed) continue;

        set<SChainMember*> chain_alignments;
        mi.CollectAllContainedAlignments(chain_alignments);
        CChain chain(chain_alignments);
        _ASSERT(chain.Weight() == mi.m_num);

        m_gnomon->GetScore(chain);
        chain.RestoreReasonableConfirmedStart(*m_gnomon);
        double ms = GoodCDNAScore(chain);
        if ((chain.Type() & CGeneModel::eProt)==0 && !chain.ConfirmedStart()) 
            RemovePoorCds(chain,ms);

        if(chain.Score() != BadScore()) {
            mi.MarkIncludedAllContainedAlignments();
            NON_CONST_ITERATE(vector<SChainMember*>, j, pointers) {
                SChainMember& mj = **j;
                const CGeneModel& align = *mj.m_align;
                
                if(mj.m_included || mj.m_type == eCDS || align.Strand() != chain.Strand()) continue;

                if(align.IsSubAlignOf(chain)) {
                    chain.AddSupport(CSupportInfo(align.ID()));
                    chain.SetWeight(chain.Weight()+align.Weight());
                    if (mj.m_copy != 0) {
                        ITERATE(TContained, k, *mj.m_copy) {
                            (*k)->m_included = true;
                        }
                    }                
                }
            }  
            chains.push_back(chain);
        } else {
            mi.MarkPostponedAllContainedAlignments();
        }
    }

    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsCoding),pointers.end());  // only noncoding left

    LeftRight(pointers);
    RightLeft(pointers);

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
        _ASSERT(mi.m_cds == 0);
    }

    sort(pointers.begin(),pointers.end(),CdsNumOrder());

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        if(mi.m_included) continue;

        set<SChainMember*> chain_alignments;
        mi.CollectAllContainedAlignments(chain_alignments);
        CChain chain(chain_alignments);
        _ASSERT(chain.Weight() == mi.m_num);

        m_gnomon->GetScore(chain);
        double ms = GoodCDNAScore(chain);
        RemovePoorCds(chain,ms);

        chains.push_back(chain);
        mi.MarkIncludedAllContainedAlignments();
    }

    CombineCompatibleChains(chains);
}


void CChainer::CChainerImpl::CombineCompatibleChains(list<CChain>& chains) {
    for(list<CChain>::iterator itt = chains.begin(); itt != chains.end(); ++itt) {
        for(list<CChain>::iterator jt = chains.begin(); jt != chains.end();) {
            list<CChain>::iterator jtt = jt++;
            if(itt != jtt && itt->Strand() == jtt->Strand() && itt->ReadingFrame() == jtt->ReadingFrame() && jtt->IsSubAlignOf(*itt)) {

                for(vector<CGeneModel*>::iterator is = jtt->m_members.begin(); is != jtt->m_members.end(); ++is) {
                    if(itt->AddSupport(CSupportInfo((*is)->ID())))
                        itt->SetWeight(itt->Weight()+(*is)->Weight());
                }

                chains.erase(jtt);
            }
        }
    }
}

double CChainer::CChainerImpl::GoodCDNAScore(const CGeneModel& algn)
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


void CChainer::CChainerImpl::RemovePoorCds(CGeneModel& algn, double minscor)
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


CChain::CChain(const set<SChainMember*>& chain_alignments)
{
    SetType(eChain);

    _ASSERT(chain_alignments.size()>0);
    ITERATE (set<SChainMember*>, it, chain_alignments)
        m_members.push_back((*it)->m_align);
    sort(m_members.begin(),m_members.end(),AlignSeqOrder());

    EStrand strand = (*chain_alignments.begin())->m_align->Strand();
    SetStrand(strand);

    vector<CSupportInfo> support;
    support.reserve(m_members.size());
    int last_important_support = -1;
    const CGeneModel* last_important_align = 0;
    
    double weight = 0;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;

        weight += align.Weight();

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
    SetWeight(weight);
    if(last_important_align != 0) 
       support[last_important_support].SetCore(true);
    ITERATE(vector<CSupportInfo>, s, support)
        AddSupport(*s);
}

void CChain::RestoreTrimmedEnds(int trim)
{
    // add back trimmed off UTRs    
    
    if(!OpenLeftEnd() || ReadingFrame().Empty()) {
        for(int ia = 0; ia < (int)m_members.size(); ++ia)  {
            if((m_members[ia]->Type() & eProt)==0 && (m_members[ia]->Status() & CGeneModel::eLeftTrimmed)!=0 &&
               Exons().front().Limits().GetFrom() == m_members[ia]->Limits().GetFrom()) {
                ExtendLeft( trim );
                break;
            }
        }
    }
     
    if(!OpenRightEnd() || ReadingFrame().Empty()) {
        for(int ia = 0; ia < (int)m_members.size(); ++ia)  {
            if((m_members[ia]->Type() & eProt)==0 && (m_members[ia]->Status() & CGeneModel::eRightTrimmed)!=0 &&
               Exons().back().Limits().GetTo() == m_members[ia]->Limits().GetTo()) {
                ExtendRight( trim );
                break;
            }
        }
    }
}

void CChain::RestoreReasonableConfirmedStart(const CGnomonEngine& gnomon)
{
    TSignedSeqRange conf_start;
    TSignedSeqPos rf=0, fivep=0; 
    
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
    }

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
}

void CChain::RemoveFshiftsFromUTRs()
{
    TInDels fs;
    ITERATE(TInDels, i, FrameShifts()) {   // removing fshifts in UTRs
        if (i->IntersectingWith(MaxCdsLimits().GetFrom(), MaxCdsLimits().GetTo()))
            fs.push_back(*i);
    }
    FrameShifts() = fs;
}

void CChain::ClipToCompleteAlignment(EStatus determinant)
{
    string  name;
    EStrand right_end_strand = ePlus;
    bool complete = false;

    if (determinant == CGeneModel::ePolyA) {
        name  = "polya";
        right_end_strand = ePlus;
        complete = HasStop();
    } else if (determinant == CGeneModel::eCap) {
        name = "cap";
        right_end_strand = eMinus;
        complete = HasStart();
    } else {
        _ASSERT( false );
    }

    int pos = Strand() == right_end_strand ? -1 : numeric_limits<int>::max();
    bool found = false;

    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        if((align.Status()&determinant) != 0) {
            found = true;
            if(Strand() == right_end_strand) {
                pos = max(pos, align.Limits().GetTo());
            } else {
                pos = min(pos,align.Limits().GetFrom()); 
            }
        }
    }

    if (!found)
        return;

    const int fuzz = 25;

    int last_not_oneexon_est_sr = Strand() == right_end_strand ? -1 : numeric_limits<int>::max()-fuzz;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        if((align.Type() != CGeneModel::eEST && align.Type() != CGeneModel::eSR) || align.Exons().size() > 1) {
            if(Strand() == right_end_strand) {
                last_not_oneexon_est_sr = max(pos, align.Limits().GetTo());
            } else {
                last_not_oneexon_est_sr = min(pos,align.Limits().GetFrom()); 
            }
        }
    }


    TSignedSeqRange clip_range;

    if (Strand() == right_end_strand && last_not_oneexon_est_sr-fuzz < pos &&
        Include(Exons().back().Limits(), pos) && (ReadingFrame().Empty() || (complete && pos > RealCdsLimits().GetTo())) && 
        (Exons().size() > 1 || RealCdsLimits().NotEmpty())) {   // complete in the right exon and not in cds

        Status() |= determinant;
        if(pos < Limits().GetTo()) {
            clip_range.Set(pos+1, Limits().GetTo());
        }
    } else if (Strand() != right_end_strand && pos < last_not_oneexon_est_sr+fuzz &&
               Include(Exons().front().Limits(), pos) && (ReadingFrame().Empty() || (complete && pos < RealCdsLimits().GetFrom())) && 
               (Exons().size() > 1 || RealCdsLimits().NotEmpty())) {   // complete in the right exon and not in cds    

        Status() |= determinant;
        if (pos > Limits().GetFrom()) {
            clip_range.Set(Limits().GetFrom(), pos-1);
        }
    }

    if (clip_range.NotEmpty()) {
        AddComment(name+"clip");
        CutExons(clip_range);
        RecalculateLimits();
    } 
    if((Status()&determinant) == 0) {
        AddComment("lost"+name);
    }
}

pair<bool,bool> ProteinPartialness(map<string, pair<bool,bool> >& prot_complet, const CAlignModel& align, CScope& scope)
{
    string accession = align.TargetAccession();
    map<string, pair<bool,bool> >::iterator iter = prot_complet.find(accession);
    if (iter == prot_complet.end()) {
        iter = prot_complet.insert(make_pair(accession, make_pair(true, true))).first;
        CSeqVector protein_seqvec(scope.GetBioseqHandle(*align.GetTargetId()), CBioseq_Handle::eCoding_Iupac);
        CSeqVector_CI protein_ci(protein_seqvec);
        iter->second.first = *protein_ci == 'M';
    }
    return iter->second;
}

bool LeftPartialProtein(map<string, pair<bool,bool> >& prot_complet, const CAlignModel& align, CScope& scope)
{
    return !ProteinPartialness(prot_complet, align, scope).first;
}

bool RightPartialProtein(map<string, pair<bool,bool> >& prot_complet, const CAlignModel& align, CScope& scope)
{
    return !ProteinPartialness(prot_complet, align, scope).second;
}

void CChain::SetConfirmedStartStopForCompleteProteins(map<string, pair<bool,bool> >& prot_complet, TOrigAligns& orig_aligns, const SMinScor& minscor)
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    CAlignMap mrnamap = GetAlignMap();
    ITERATE(vector<CGeneModel*>, i, m_members) {
        CAlignModel* orig_align = orig_aligns[(*i)->ID()];
        
        if((orig_align->Type() & CGeneModel::eProt) == 0 || orig_align->TargetLen() == 0)   // not a protein or not known length
            continue;

        TSignedSeqRange fivep_exon = orig_align->Exons().front().Limits();
        TSignedSeqRange threep_exon = orig_align->Exons().back().Limits();
        if((*i)->Strand() == eMinus)
            swap(fivep_exon,threep_exon);

        if(!ConfirmedStart() && HasStart() && fivep_exon.IntersectingWith((*i)->Limits()) && 
           !LeftPartialProtein(prot_complet, *orig_align, scope) && Include(Limits(),(*i)->Limits())) {  // protein has start

            TSignedSeqPos not_aligned =  orig_align->GetAlignMap().MapRangeOrigToEdited((*i)->Limits(),false).GetFrom()-1;
            if(not_aligned <= (1.-minscor.m_minprotfrac)*orig_align->TargetLen()) {                                                         // well aligned
                TSignedSeqPos extra_length = mrnamap.MapRangeOrigToEdited((*i)->Limits(),false).GetFrom()-
                    mrnamap.MapRangeOrigToEdited(GetCdsInfo().Start(),false).GetFrom()-1;
                
                if(extra_length > not_aligned-minscor.m_endprotfrac*orig_align->TargetLen()) {
                    CCDSInfo cds_info = GetCdsInfo();
                    cds_info.SetScore(cds_info.Score(), false);   // not open
                    cds_info.SetStart(cds_info.Start(), true);    // confirmed start
                    SetCdsInfo(cds_info);
                }
            }
        }

        if(!ConfirmedStop() && HasStop() && threep_exon.IntersectingWith((*i)->Limits()) && 
           !RightPartialProtein(prot_complet, *orig_align, scope) && Include(Limits(),(*i)->Limits())) {  // protein has stop

            TSignedSeqPos not_aligned = orig_align->TargetLen()-orig_align->GetAlignMap().MapRangeOrigToEdited((*i)->Limits(),false).GetTo();
            if(not_aligned <= (1.-minscor.m_minprotfrac)*orig_align->TargetLen()) {                                                         // well aligned
                TSignedSeqPos extra_length = mrnamap.MapRangeOrigToEdited(GetCdsInfo().Stop(),false).GetTo()-
                    mrnamap.MapRangeOrigToEdited((*i)->Limits(),false).GetTo();
                
                if(extra_length > not_aligned-minscor.m_endprotfrac*orig_align->TargetLen()) {
                    CCDSInfo cds_info = GetCdsInfo();
                    cds_info.SetStop(cds_info.Stop(), true);    // confirmed stop   
                    SetCdsInfo(cds_info);
                }
            }
        }
    }
}

void CChain::CollectTrustedmRNAsProts(TOrigAligns& orig_aligns, const SMinScor& minscor)
{
    ClearTrustedmRNA();
    ClearTrustedProt();
    if(Continuous() && ConfirmedStart() && ConfirmedStop()) {
        ITERATE(vector<CGeneModel*>, i, m_members) {
            if(IntersectingWith(**i)) {                  // just in case we clipped this alignment
                CAlignModel* orig_align = orig_aligns[(*i)->ID()];
                if(orig_align->Continuous() && (((*i)->ConfirmedStart() && (*i)->ConfirmedStop()) || (orig_align->AlignLen() > minscor.m_minprotfrac*orig_align->TargetLen()))) {
                    if(!(*i)->TrustedmRNA().empty())
                        InsertTrustedmRNA(*(*i)->TrustedmRNA().begin());
                    else if(!(*i)->TrustedProt().empty())
                        InsertTrustedProt(*(*i)->TrustedProt().begin());
                }
            }
        }
    }
}

pair<string,int> GetAccVer(const CAlignModel& a, CScope& scope)
{
    string accession = sequence::GetAccessionForId(*a.GetTargetId(), scope);
    if (accession.empty())
        return make_pair(a.TargetAccession(), 0);

    string::size_type dot = accession.find('.');
    int version = 0;
    if(dot != string::npos) {
        version = atoi(accession.substr(dot+1).c_str());
        accession = accession.substr(0,dot);
    }
    
    return make_pair(accession,version);
}

static int s_ExonLen(const CGeneModel& a);

struct s_ByAccVerLen {
    s_ByAccVerLen(CScope& scope_) : scope(scope_) {}
    CScope& scope;
    bool operator()(const CAlignModel* a, const CAlignModel* b)
    {
        pair<string,int> a_acc = GetAccVer(*a, scope);
        pair<string,int> b_acc = GetAccVer(*b, scope);
        int acc_cmp = NStr::CompareCase(a_acc.first,b_acc.first);
        if (acc_cmp != 0)
            return acc_cmp<0;
        if (a_acc.second != b_acc.second)
            return a_acc.second > b_acc.second;
        
        int a_len = s_ExonLen(*a);
        int b_len = s_ExonLen(*b);
        if (a_len!=b_len)
            return a_len > b_len;
        if (a->ConfirmedStart() != b->ConfirmedStart())
            return a->ConfirmedStart();
        return a->ID() < b->ID(); // to make sort deterministic
    }
};
static int s_ExonLen(const CGeneModel& a)
    {
        int len = 0;
        ITERATE(CGeneModel::TExons, e, a.Exons())
            len += e->Limits().GetLength();
        return len;
    }

void CChainer::CChainerImpl::SkipReason(CGeneModel* orig_align, const string& comment)
{
    orig_align->Status() |= CGeneModel::eSkipped;
    orig_align->AddComment(comment);
}

void CChainer::FilterOutChimeras(TGeneModelList& clust)
{
    m_data->FilterOutChimeras(clust);
}

void CChainer::CChainerImpl::FilterOutChimeras(TGeneModelList& clust)
{
    typedef map<int,TGeneModelClusterSet> TClustersByStrand;
    TClustersByStrand trusted_aligns;
    ITERATE(TGeneModelList, it, clust) {
        CAlignModel* orig_align = orig_aligns[it->ID()];
        if(orig_align->Continuous() && (!it->TrustedmRNA().empty() || !it->TrustedProt().empty())
                            && it->AlignLen() > minscor.m_minprotfrac*orig_aligns[it->ID()]->TargetLen()) {
            trusted_aligns[it->Strand()].Insert(*it); 
        }           
    }

    if(trusted_aligns[ePlus].size() < 2 && trusted_aligns[eMinus].size() < 2)
        return;

    typedef set<int> TSplices;
    typedef list<TSplices> TSplicesList;
    typedef map<int,TSplicesList> TSplicesByStrand;
    TSplicesByStrand trusted_splices;
    
    ITERATE(TClustersByStrand, it, trusted_aligns) {
        int strand = it->first;
        const TGeneModelClusterSet& clset = it->second;
        ITERATE(TGeneModelClusterSet, jt, clset) {
            const TGeneModelCluster& cls = *jt;
            trusted_splices[strand].push_back(set<int>());
            TSplices& splices = trusted_splices[strand].back();
            ITERATE(TGeneModelCluster, lt, cls) {
                const CGeneModel& align = *lt;
                ITERATE(CGeneModel::TExons, e, align.Exons()) {
                    if(e->m_fsplice)
                        splices.insert(e->GetFrom());
                    if(e->m_ssplice)
                        splices.insert(e->GetTo());                       
                }
            }
        }
    }

    for(TGeneModelList::iterator it_loop = clust.begin(); it_loop != clust.end(); ) {
        TGeneModelList::iterator it = it_loop++;
        
        const CGeneModel& align = *it;
        int strand = align.Strand();
        const TSplicesList& spl = trusted_splices[strand];

        int count = 0;
        ITERATE(TSplicesList, jt, spl) {
            const TSplices& splices = *jt;
            for(unsigned int i = 0; i < align.Exons().size(); ++i) {
                const CModelExon& e = align.Exons()[i];
                if(splices.find(e.GetFrom()) != splices.end() || splices.find(e.GetTo()) != splices.end()) {
                    ++count;
                    break;
                }
            }        
        }

        if(count > 1) {
            SkipReason(orig_aligns[align.ID()],"Chimera");
            clust.erase(it);
        }
    }
}

struct OverlapsSameAccessionAlignment : public Predicate {
    OverlapsSameAccessionAlignment(TAlignModelList& alignments);
    virtual bool align_predicate(CAlignModel& align);
    virtual string GetComment() { return "Overlaps the same alignment";}
};

OverlapsSameAccessionAlignment::OverlapsSameAccessionAlignment(TAlignModelList& alignments)
{
    if (alignments.empty())
        return;

    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    vector<CAlignModel*> alignment_ptrs;
    NON_CONST_ITERATE(TAlignModelList, a, alignments) {
        alignment_ptrs.push_back(&*a);
    }
    sort(alignment_ptrs.begin(), alignment_ptrs.end(), s_ByAccVerLen(scope));

    vector<CAlignModel*>::iterator first = alignment_ptrs.begin();
    pair<string,int> first_accver = GetAccVer(**first, scope);
    vector<CAlignModel*> ::iterator current = first; ++current;
    for (; current != alignment_ptrs.end(); ++current) {
        pair<string,int> current_accver = GetAccVer(**current, scope);
        if (first_accver.first == current_accver.first) {
            if ((*current)->Strand() == (*first)->Strand() && (*current)->Limits().IntersectingWith((*first)->Limits())) {
                (*current)->Status() |= CGeneModel::eSkipped;
            }
        } else {
            first=current;
            first_accver = current_accver;
        }
    }
}

bool OverlapsSameAccessionAlignment::align_predicate(CAlignModel& align)
{
    return align.Status() & CGeneModel::eSkipped;
}

Predicate* CChainer::OverlapsSameAccessionAlignment(TAlignModelList& alignments)
{
    return new gnomon::OverlapsSameAccessionAlignment(alignments);
}

string FindMultiplyIncluded(CAlignModel& algn, TAlignModelList& clust)
{
    if ((algn.Type() & CGeneModel::eProt)!=0 && !algn.Continuous()) {
        set<string> compatible_evidence;
        int len = algn.AlignLen();
        
        static CGeneModel dummy_align;
        const CGeneModel* prev_alignp = &dummy_align;

        bool prev_is_compatible = false;
        NON_CONST_ITERATE(TAlignModelList, jtcl, clust) {
            CAlignModel& algnj = *jtcl;
            if (algn == algnj)
                continue;
            if (algnj.AlignLen() < len/4)
                continue;
            
            bool same_as_prev = algnj.IdenticalAlign(*prev_alignp);
            if (!same_as_prev)
                prev_alignp = &algnj;
                        
            if ((same_as_prev && prev_is_compatible) || (!same_as_prev && algn.Strand()==algnj.Strand() && algn.isCompatible(algnj))) {
                prev_is_compatible = true;
                if (!compatible_evidence.insert(algnj.TargetAccession()).second) {
                    return algnj.TargetAccession();
                }
            } else {
                prev_is_compatible = false;
            }
        }
    }
    return kEmptyStr;
}

struct ConnectsParalogs : public Predicate {
    ConnectsParalogs(TAlignModelList& _alignments)
        : alignments(_alignments)
    {}
    TAlignModelList& alignments;
    string paralog;

    virtual bool align_predicate(CAlignModel& align)
    {
        paralog = FindMultiplyIncluded(align, alignments);
        return !paralog.empty();
    }
    virtual string GetComment() { return "Connects two "+paralog+" alignments"; }
};

Predicate* CChainer::ConnectsParalogs(TAlignModelList& alignments)
{
    return new gnomon::ConnectsParalogs(alignments);
}

void CChainer::ScoreCDSes_FilterOutPoorAlignments(TGeneModelList& clust)
{
    m_data->ScoreCDSes_FilterOutPoorAlignments(clust);
}

void CChainer::CChainerImpl::ScoreCDSes_FilterOutPoorAlignments(TGeneModelList& clust)
{
    clust.sort(s_ByExonNumberAndLocation);

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

        if ((algn.Type() & CGeneModel::eProt)!=0 || algn.ConfirmedStart()) {   // this includes protein alignments and mRNA with confirmed CDSes

            if (algn.Score()==BadScore()) {
                if (same_as_prev && !algn.ConfirmedStart()) {
                    CCDSInfo cdsinfo = prev_align->GetCdsInfo();
                    algn.SetCdsInfo(cdsinfo);
                } else {
                    m_gnomon->GetScore(algn);
                }
            }
           
            double ms = GoodCDNAScore(algn);

            if (algn.Score() == BadScore() || (algn.Score() < ms && (algn.Type() & CGeneModel::eProt) != 0 && orig->AlignLen() < minscor.m_minprotfrac*orig->TargetLen())) { // all mRNA with confirmed CDS and reasonably aligned proteins with known length will get through with any finite score 
                CNcbiOstrstream ost;
                if(algn.AlignLen() <= 75)
                    ost << "Short alignment " << algn.AlignLen();
                else
                    ost << "Low score " << algn.Score();
                SkipReason(prev_align = orig, CNcbiOstrstreamToString(ost));
                itcl = clust.erase(itcl);
                continue;
            }
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

bool CChainer::CChainerImpl::AddIfCompatible(set<SFShiftsCluster>& fshift_clusters, const CGeneModel& algn)
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

bool CChainer::CChainerImpl::FsTouch(const TSignedSeqRange& lim, const CInDelInfo& fs) {
    if(fs.IsInsertion() && fs.Loc()+fs.Len() == lim.GetFrom())
        return true;
    if(fs.IsDeletion() && fs.Loc() == lim.GetFrom())
        return true;
    if(fs.Loc() == lim.GetTo()+1)
        return true;

    return false;
}

void CChainer::FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(TGeneModelList& clust)
{
    m_data->FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(clust);
}

void CChainer::CChainerImpl::FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(TGeneModelList& clust)
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


void CChainer::CChainerImpl::AddFShifts(TGeneModelList& clust, const TInDels& fshifts)    
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
                    if ((algn.Exons()[k].m_fsplice && fs->Loc()<a) ||
                        (algn.Exons()[k].m_ssplice && fs->IsInsertion() && b<fs->Loc()+fs->Len()-1)) {
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

        ++itcl;
    }
}


void CChainer::CChainerImpl::CollectFShifts(const TGeneModelList& clust, TInDels& fshifts)
{
    ITERATE (TGeneModelList, itcl, clust) {
        const TInDels& fs = itcl->FrameShifts();
        fshifts.insert(fshifts.end(),fs.begin(),fs.end());
    }
    sort(fshifts.begin(),fshifts.end());
            
    if (!fshifts.empty())
        uniq(fshifts);
}


void CChainer::CChainerImpl::SplitAlignmentsByStrand(const TGeneModelList& clust, TGeneModelList& clust_plus, TGeneModelList& clust_minus)
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

double InframeFraction(const CGeneModel& a, TSignedSeqPos left, TSignedSeqPos right)
{
    if(a.FrameShifts().empty())
        return 1.0;

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
    return double(inframelength)/(inframelength + outframelength);
}

struct ProjectCDS : public TransformFunction {
    ProjectCDS(double _mininframefrac, const CResidueVec& _seq, CScope* _scope, const map<string, TSignedSeqRange>& _mrnaCDS)
        : mininframefrac(_mininframefrac), seq(_seq), scope(_scope), mrnaCDS(_mrnaCDS) {}

    double mininframefrac;
    const CResidueVec& seq;
    CScope* scope;
    const map<string, TSignedSeqRange>& mrnaCDS;
    virtual void transform_align(CAlignModel& align);
};

void ProjectCDS::transform_align(CAlignModel& align)
{
    if ((align.Type()&CAlignModel::emRNA)==0 || (align.Status()&CAlignModel::eReversed)!=0)
        return;

    TSignedSeqRange cds_on_mrna;

    if (scope != NULL) {
        SAnnotSelector sel;
        sel.SetFeatSubtype(CSeqFeatData::eSubtype_cdregion);
        CSeq_loc mrna;
        CRef<CSeq_id> target_id(new CSeq_id);
        target_id->Assign(*align.GetTargetId());
        mrna.SetWhole(*target_id);
        CFeat_CI feat_ci(*scope, mrna, sel);
        if (feat_ci && !feat_ci->IsSetPartial()) {
            TSeqRange feat_range = feat_ci->GetRange();
            cds_on_mrna = TSignedSeqRange(feat_range.GetFrom(), feat_range.GetTo());
        }
    } else {
        string accession = align.TargetAccession();
        map<string,TSignedSeqRange>::const_iterator pos = mrnaCDS.find(accession);
        if(pos != mrnaCDS.end()) {
            cds_on_mrna = pos->second;
        }
    }

    if (cds_on_mrna.Empty())
        return;

    CAlignMap alignmap(align.GetAlignMap());
    TSignedSeqPos left = alignmap.MapEditedToOrig(cds_on_mrna.GetFrom());
    TSignedSeqPos right = alignmap.MapEditedToOrig(cds_on_mrna.GetTo());
    if(align.Strand() == eMinus) {
        swap(left,right);
    }
    
    CGeneModel a = align;
    
    if(left < 0 || right < 0)     // start or stop cannot be projected  
        return;
    if(left < align.Limits().GetFrom() || right >  align.Limits().GetTo())    // cds is clipped
        return;
    
    a.Clip(TSignedSeqRange(left,right),CGeneModel::eRemoveExons);
    
    //            ITERATE(TInDels, fs, a.FrameShifts()) {
    //                if(fs->Len()%3 != 0) return;          // there is a frameshift    
    //            }
    
    if (InframeFraction(a, left, right) < mininframefrac)
        return;
    
    a.FrameShifts().clear();                       // clear notshifted indels   
    CAlignMap cdsmap(a.GetAlignMap());
    CResidueVec cds;
    cdsmap.EditedSequence(seq, cds);
    unsigned int length = cds.size();
    
    if(length%3 != 0)
        return;
    
    if(!IsStartCodon(&cds[0]) || !IsStopCodon(&cds[length-3]) )   // start or stop on genome is not right
        return;
    
    for(unsigned int i = 0; i < length-3; i += 3) {
        if(IsStopCodon(&cds[i]))
            return;                // premature stop on genome
    }
    
    TSignedSeqRange reading_frame = cdsmap.MapRangeEditedToOrig(TSignedSeqRange(3,length-4));
    TSignedSeqRange start = cdsmap.MapRangeEditedToOrig(TSignedSeqRange(0,2));
    TSignedSeqRange stop = cdsmap.MapRangeEditedToOrig(TSignedSeqRange(length-3,length-1));
    
    CCDSInfo cdsinfo;
    cdsinfo.SetReadingFrame(reading_frame,true);
    cdsinfo.SetStart(start,true);
    cdsinfo.SetStop(stop,true);

    //    align.FrameShifts().clear();
    CGeneModel b = align;
    b.FrameShifts().clear();
    align = CAlignModel(b, b.GetAlignMap());
    align.SetCdsInfo(cdsinfo);
}

void CChainer::CChainerImpl::FilterOutBadScoreChainsHavingBetterCompatibles(TGeneModelList& chains)
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
          

struct TrimAlignment : public TransformFunction {
public:
    TrimAlignment(int a_trim) : trim(a_trim)  {}
    int trim;

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

    virtual void transform_align(CAlignModel& align)
    {
        TSignedSeqRange limits = align.Limits();
        CAlignMap alignmap(align.GetAlignMap());

        if ((align.Type() & CAlignModel::eProt)!=0) {
            TrimProtein(align, alignmap);
        } else {
            TrimTranscript(align, alignmap);
        }

        if(align.Limits().GetFrom() > limits.GetFrom()) align.Status() |= CAlignModel::eLeftTrimmed;
        if(align.Limits().GetTo() < limits.GetTo()) align.Status() |= CAlignModel::eRightTrimmed;
    }

    void TrimProtein(CAlignModel& align, CAlignMap& alignmap)
    {
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
    }

    void TrimTranscript(CAlignModel& align, CAlignMap& alignmap)
    {
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
        
        if(!align.ReadingFrame().Empty()) {  // avoid trimming confirmed CDSes
            TSignedSeqRange cds_on_genome = align.RealCdsLimits();
            if(cds_on_genome.GetFrom() < a) {
                a = align.Limits().GetFrom();
            }
            if(b < cds_on_genome.GetTo()) {
                b = align.Limits().GetTo();
            }
        }
        
        TSignedSeqRange newlimits = alignmap.ShrinkToRealPoints(TSignedSeqRange(a,b),false);
        _ASSERT(newlimits.NotEmpty() && align.Exons().front().GetTo() >= newlimits.GetFrom() && align.Exons().back().GetFrom() <= newlimits.GetTo());
        
        if(newlimits != align.Limits()) {
            align.Clip(newlimits,CAlignModel::eDontRemoveExons);    // Clip doesn't change AlignMap
        }
    }
};

TransformFunction* CChainer::TrimAlignment()
{
    return new gnomon::TrimAlignment(m_data->trim);
}

struct DoNotBelieveShortPolyATail : public TransformFunction {
    DoNotBelieveShortPolyATail(int _minpolya) : minpolya(_minpolya) {}

    int minpolya;
    virtual void transform_align(CAlignModel& align)
    {
        int polyalen = align.PolyALen();
        if ((align.Status() & CGeneModel::ePolyA) != 0  &&
            polyalen < minpolya) {
            align.Status() ^= CGeneModel::ePolyA;
        }
    }
};

TransformFunction* CChainer::DoNotBelieveShortPolyATail()
{
    return new gnomon::DoNotBelieveShortPolyATail(m_data->minpolya);
}

void CChainer::SetGenomicRange(const TAlignModelList& alignments)
{
    m_data->SetGenomicRange(alignments);
}

void CChainer::CChainerImpl::SetGenomicRange(const TAlignModelList& alignments)
{
    TSignedSeqRange range = alignments.empty() ? TSignedSeqRange::GetWhole() : TSignedSeqRange::GetEmpty();

    ITERATE(TAlignModelList, i, alignments) {
        range += i->Limits();
    }

    _ASSERT(m_gnomon.get() != NULL);
    m_gnomon->ResetRange(range);

    orig_aligns.clear();
}

TransformFunction* CChainer::ProjectCDS(CScope& scope)
{
    return new gnomon::ProjectCDS(m_data->mininframefrac, m_gnomon->GetSeq(),
                                  m_data->mrnaCDS.find("use_objmgr")!=m_data->mrnaCDS.end() ? &scope : NULL,
                                  m_data->mrnaCDS);
}

struct DoNotBelieveFrameShiftsWithoutCdsEvidence : public TransformFunction {
    virtual void transform_align(CAlignModel& align)
    {
        if (align.ReadingFrame().Empty())
            align.FrameShifts().clear();
    }
};

TransformFunction* CChainer::DoNotBelieveFrameShiftsWithoutCdsEvidence()
{
    return new gnomon::DoNotBelieveFrameShiftsWithoutCdsEvidence();
}

bool LeftAndLongFirst(const CGeneModel& a, const CGeneModel& b) {
    if(a.Limits() == b.Limits()) {
        if(a.Type() == b.Type())
            return a.ID() < b.ID();
        else 
            return a.Type() > b.Type();
    }
    else if(a.Limits().GetFrom() == b.Limits().GetFrom())
        return a.Limits().GetTo() > b.Limits().GetTo();
    else
        return a.Limits().GetFrom() < b.Limits().GetFrom();
}

void CChainer::CChainerImpl::CollapsSimilarESTandSR(TGeneModelList& clust)
{
    set<int> left_exon_ends, right_exon_ends;
    ITERATE(TGeneModelList, i, clust) {
        const CGeneModel::TExons& e = i->Exons();
        for(unsigned int l = 0; l < e.size(); ++l) {
            if(e[l].m_fsplice)
                left_exon_ends.insert(e[l].GetFrom());
            if(e[l].m_ssplice)
                right_exon_ends.insert(e[l].GetTo());
        }
    }

#define MAX_ETEND 200   // useful for separating overlapping genes
    clust.sort(LeftAndLongFirst); // left to right, if equal longer first

    NON_CONST_ITERATE(TGeneModelList, i, clust) {
        CGeneModel& ai = *i;

        if((ai.Type() != CGeneModel::eSR && ai.Type() != CGeneModel::eEST))
            continue;

        double weight = ai.Weight();
        int right_end = ai.Limits().GetTo();
        bool left_trim = (ai.Status()&CGeneModel::eLeftTrimmed)!=0;
        bool right_trim = (ai.Status()&CGeneModel::eRightTrimmed)!=0;
        int splice_numi = 2*(ai.Exons().size()-1);
 
        set<int>::iterator ri = right_exon_ends.lower_bound(ai.Limits().GetTo()); // leftmost compatible rexon
        int rlimb =  numeric_limits<int>::max();
        if(ri != right_exon_ends.end())
            rlimb = *ri;
        if(ai.Type() == CGeneModel::eEST)
            rlimb = ai.Limits().GetTo();   // don't extend EST
        int rlima = -1;
        if(ri != right_exon_ends.begin())
            rlima = *(--ri);
        
        set<int>::iterator li = left_exon_ends.upper_bound(ai.Limits().GetFrom()); // leftmost not compatible lexon
        int llimb = numeric_limits<int>::max() ;
        if(li != left_exon_ends.end())
            llimb = *li;
        
        
        TGeneModelList:: iterator jloop = i;
        for(++jloop; jloop != clust.end() && jloop->Limits().GetFrom() < min(right_end,llimb); ) {    // overlaps with (extended) EST 
            TGeneModelList:: iterator j = jloop++;
            CGeneModel& aj = *j;

            if(aj.Limits().GetTo() > rlimb || aj.Limits().GetTo() <= rlima)
                continue;
 
            int splice_numj = 2*(aj.Exons().size()-1);
            if( ai.Type() != aj.Type() || (ai.Status()&CGeneModel::eUnknownOrientation) != (aj.Status()&CGeneModel::eUnknownOrientation) ||
                ((ai.Status()&CGeneModel::eUnknownOrientation) == 0 && ai.Strand() != aj.Strand()) || 
                splice_numi != splice_numj  || (splice_numi > 0 && ai.isCompatible(aj)-1 != splice_numi) )
                        continue;

            if( ((ai.Status()&CGeneModel::ePolyA) && ai.Strand() == ePlus && aj.Limits().GetTo() > ai.Limits().GetTo()) || 
                ((aj.Status()&CGeneModel::ePolyA) && ((aj.Strand() == ePlus && ai.Limits().GetTo() > aj.Limits().GetTo()) || (aj.Strand() == eMinus && ai.Limits().GetFrom() < aj.Limits().GetFrom()))) ||
                ((ai.Status()&CGeneModel::eCap) && ai.Strand() == eMinus && aj.Limits().GetTo() > ai.Limits().GetTo()) ||
                ((aj.Status()&CGeneModel::eCap) && ((aj.Strand() == eMinus && ai.Limits().GetTo() > aj.Limits().GetTo()) || (aj.Strand() == ePlus && ai.Limits().GetFrom() < aj.Limits().GetFrom()))) )
                          continue;

            if(aj.Limits().GetTo() > right_end) {
                if(splice_numi == 0 && (right_end-aj.Limits().GetFrom()+1 < intersect_limit || aj.Limits().GetTo()-ai.Limits().GetTo() > MAX_ETEND))
                    continue;
                right_trim = aj.Status()&CGeneModel::eRightTrimmed;
                right_end = aj.Limits().GetTo();
            }

            weight += aj.Weight();

            if(ai.Limits().GetFrom() == aj.Limits().GetFrom() && aj.Status()&CGeneModel::eLeftTrimmed)
                left_trim = true;

            if(aj.Status()&CGeneModel::ePolyA)
                ai.Status() |= CGeneModel::ePolyA;
            if(aj.Status()&CGeneModel::eCap)
                ai.Status() |= CGeneModel::eCap;
            
            clust.erase(j);
        }


        ai.Status() &= (~CGeneModel::eRightTrimmed);
        if(right_trim)
            ai.Status() |= CGeneModel::eRightTrimmed;
        if(left_trim)
            ai.Status() |= CGeneModel::eLeftTrimmed;
        ai.SetWeight(weight);
        if(right_end > ai.Limits().GetTo())
            ai.ExtendRight(right_end-ai.Limits().GetTo());
    }

    cerr << "Collapsed to " << clust.size() << endl;
}

void CChainer::DropAlignmentInfo(TAlignModelList& alignments, TGeneModelList& models)
{
    m_data->DropAlignmentInfo(alignments, models);
}
void CChainer::CChainerImpl::DropAlignmentInfo(TAlignModelList& alignments, TGeneModelList& models)
{
    NON_CONST_ITERATE (TAlignModelCluster, i, alignments) {
        CGeneModel aa = *i;
        
        models.push_back(aa);
        orig_aligns[models.back().ID()]=&(*i);
    }
}

void CChainer::ReplicateFrameShifts(TGeneModelList& models)
{
    m_data->ReplicateFrameShifts(models);
}

void CChainer::CChainerImpl::ReplicateFrameShifts(TGeneModelList& models)
{
    TInDels fshifts;
    CollectFShifts(models, fshifts);
    AddFShifts(models, fshifts);
}

TGeneModelList CChainer::CChainerImpl::MakeChains(TGeneModelList& models)
{
    //    CollapsSimilarESTandSR(models);

    list<CChain> tmp_chains;
    MakeChains(models, tmp_chains);

    NON_CONST_ITERATE(list<CChain>, it, tmp_chains) {
        CChain& chain = *it;
        
        chain.RemoveFshiftsFromUTRs();

        chain.ClipToCompleteAlignment(CGeneModel::eCap);
        chain.ClipToCompleteAlignment(CGeneModel::ePolyA);

        _ASSERT( chain.FShiftedLen(chain.GetCdsInfo().Start()+chain.ReadingFrame()+chain.GetCdsInfo().Stop(), false)%3==0 );

        chain.SetConfirmedStartStopForCompleteProteins(prot_complet, orig_aligns, minscor);
        chain.CollectTrustedmRNAsProts(orig_aligns, minscor);
        if (chain.FullCds()) {
            chain.Status() |= CGeneModel::eFullSupCDS;
        }

        chain.RestoreTrimmedEnds(trim);
    }

    TGeneModelList chains;
    ITERATE(list<CChain>, it, tmp_chains) {
        chains.push_back(*it);
    }

    //    FilterOutBadScoreChainsHavingBetterCompatibles(chains);


    return chains;
}

void CChainerArgUtil::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->AddKey("param", "param",
                     "Organism specific parameters",
                     CArgDescriptions::eInputFile);

    arg_desc->SetCurrentGroup("Alignment modification");
    arg_desc->AddDefaultKey("trim", "trim",
                            "If aligned sequence is partial and includes a small portion of an exon the alignment program "
                            "usually misses this exon and might erroneously place a few bases from this exon near the previous exon, "
                            "and this will mess up the chaining. To prevent this we trim small portions of the alignment before chaining. "
                            "If it is possible, the trimming will be reversed for the 5'/3' ends of the final chain. Must be < minex and "
                            "multiple of 3",
                            CArgDescriptions::eInteger, "6");
    arg_desc->AddDefaultKey("minpolya", "minpolya",
                            "Minimal accepted polyA tale. The default (large) value forces chainer to ignore PolyA",
                            CArgDescriptions::eInteger, "10000");

    arg_desc->SetCurrentGroup("Additional information about sequences");
    arg_desc->AddOptionalKey("mrnaCDS", "mrnaCDS",
                             "CDSes annotated on mRNAs. If CDS could be projected on genome with intact "
                             "Start/Stop and frame the Stop will be accepted as is. The Start could/will "
                             "be moved further to make the longest possible complete CDS within the chain",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("mininframefrac", "mininframefrac",
                            "Some mRNA alignments have paired indels which throw a portion of CDS out of frame."
                            "This parameter regulates how much of the CDS could suffer from this before CDS is considered inaceptable",
                            CArgDescriptions::eDouble, "0.95");
    arg_desc->AddOptionalKey("pinfo", "pinfo",
                             "Information about protein 5' and 3' completeness",
                             CArgDescriptions::eInputFile);

    arg_desc->SetCurrentGroup("Thresholds");
    arg_desc->AddDefaultKey("minscor", "minscor",
                            "Minimal coding propensity score for valid CDS. cDNA which score less that are considered noncoding. "
                            "Proteins with smaller score are ignored. This threshold could be ignored depending on "
                            "-protcdslen and -minprotfrac",
                            CArgDescriptions::eDouble, "25.0");
    arg_desc->AddDefaultKey("protcdslen", "protcdslen",
                            "Minimal CDS supported by protein or annotated mRNA to ignore the score (bp)",
                            CArgDescriptions::eInteger, "300");
    arg_desc->AddDefaultKey("minprotfrac", "minprotfrac",
                            "Minimal fraction of protein aligned to ignore "
                            "the score and consider for confirmed start",
                            CArgDescriptions::eDouble, "0.9");
    arg_desc->AddDefaultKey("endprotfrac", "endprotfrac",
                            "Some proteins aligned with better than -minprotfrac coverage are missing Start/Stop. "
                            "If such an alignment was extended by EST(s) which provided a Start/Stop and we are not missing "
                            "more than (1-endprotfrac)*proteinlength on either side this chain will be considered to have a confirmed Start/Stop",
                            CArgDescriptions::eDouble, "0.05");
    arg_desc->AddDefaultKey("oep", "oep",
                            "Minimal overlap length for chaining alignments which don't have introns in the ovrlapping regions",
                            CArgDescriptions::eInteger, "100");

    arg_desc->SetCurrentGroup("Heuristic parameters for score evaluation");
    arg_desc->AddDefaultKey("i5p", "i5p",
                            "5p intron penalty",
                            CArgDescriptions::eDouble, "7.0");
    arg_desc->AddDefaultKey("i3p", "i3p",
                            "3p intron penalty",
                            CArgDescriptions::eDouble, "14.0");
    arg_desc->AddDefaultKey("cdsbonus", "cdsbonus",
                            "Bonus for CDS length",
                            CArgDescriptions::eDouble, "0.05");
    arg_desc->AddDefaultKey("lenpen", "lenpen",
                            "Penalty for total length",
                            CArgDescriptions::eDouble, "0.005");
}

void CGnomonAnnotator_Base::SetHMMParameters(CHMMParameters* params)
{
    m_hmm_params = params;
}

void CChainer::SetIntersectLimit(int value)
{
    m_data->intersect_limit = value;
}
void CChainer::SetTrim(int trim)
{
    trim = (trim/3)*3;
    m_data->trim = trim;
}
void CChainer::SetMinPolyA(int minpolya)
{
    m_data->minpolya = minpolya;
}
SMinScor& CChainer::SetMinScor()
{
    return m_data->minscor;
}
void CChainer::SetMinInframeFrac(double mininframefrac)
{
    m_data->mininframefrac = mininframefrac;
}
map<string, pair<bool,bool> >& CChainer::SetProtComplet()
{
    return m_data->prot_complet;
}
map<string,TSignedSeqRange>& CChainer::SetMrnaCDS()
{
    return m_data->mrnaCDS;
}

void CChainerArgUtil::ArgsToChainer(CChainer* chainer, const CArgs& args, CScope& scope)
{
    CNcbiIfstream param_file(args["param"].AsString().c_str());
    chainer->SetHMMParameters(new CHMMParameters(param_file));
    
    chainer->SetIntersectLimit(args["oep"].AsInteger());
    chainer->SetTrim(args["trim"].AsInteger());
    chainer->SetMinPolyA(args["minpolya"].AsInteger());

    SMinScor& minscor = chainer->SetMinScor();
    minscor.m_min = args["minscor"].AsDouble();
    minscor.m_i5p_penalty = args["i5p"].AsDouble();
    minscor.m_i3p_penalty = args["i3p"].AsDouble();
    minscor.m_cds_bonus = args["cdsbonus"].AsDouble();
    minscor.m_length_penalty = args["lenpen"].AsDouble();
    minscor.m_minprotfrac = args["minprotfrac"].AsDouble();
    minscor.m_endprotfrac = args["endprotfrac"].AsDouble();
    minscor.m_prot_cds_len = args["protcdslen"].AsInteger();

    chainer->SetMinInframeFrac(args["mininframefrac"].AsDouble());
    
    CIdHandler cidh(scope);

    map<string,TSignedSeqRange>& mrnaCDS = chainer->SetMrnaCDS();
    if(args["mrnaCDS"]) {
        if (args["mrnaCDS"].AsString()=="use_objmgr") {
            mrnaCDS[args["mrnaCDS"].AsString()] = TSignedSeqRange();
        } else {
            CNcbiIfstream cdsfile(args["mrnaCDS"].AsString().c_str());
            if (!cdsfile)
                NCBI_THROW(CGnomonException, eGenericError, "Cannot open file " + args["mrnaCDS"].AsString());
            string accession, tmp;
            int a, b;
            while(cdsfile >> accession >> a >> b) {
                _ASSERT(a > 0 && b > 0 && b > a);
                getline(cdsfile,tmp);
                accession = CIdHandler::ToString(*cidh.ToCanonical(*CIdHandler::ToSeq_id(accession)));
                mrnaCDS[accession] = TSignedSeqRange(a-1,b-1);
            }
        }
    }

    map<string, pair<bool,bool> >& prot_complet = chainer->SetProtComplet();
    if(args["pinfo"]) {
        CNcbiIfstream protfile(args["pinfo"].AsString().c_str());
            if (!protfile)
                NCBI_THROW(CGnomonException, eGenericError, "Cannot open file " + args["pinfo"].AsString());
        string seqid_str;
        bool fivep;
        bool threep; 
        while(protfile >> seqid_str >> fivep >> threep) {
            seqid_str = CIdHandler::ToString(*CIdHandler::ToSeq_id(seqid_str));
            prot_complet[seqid_str] = make_pair(fivep, threep);
        }
    }
}

void CGnomonAnnotator_Base::SetGenomic(const CSeq_id& contig, CScope& scope, const string& mask_annots)
{
    CBioseq_Handle bh(scope.GetBioseqHandle(contig));
    CSeqVector sv (bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
    const size_t length (sv.size());
    string seq_txt;
    sv.GetSeqData(0, length, seq_txt);

    if (m_masking) {
        SAnnotSelector sel;
        {
            list<string> arr;
            NStr::Split(mask_annots, " ", arr);
            ITERATE(list<string>, annot, arr) {
                sel.AddNamedAnnots(*annot);
            }
        }
        sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_repeat_region)
            .SetResolveAll()
            .SetAdaptiveDepth(true);
        for (CFeat_CI it(bh, sel);  it;  ++it) {
            TSeqRange range = it->GetLocation().GetTotalRange();
            for(unsigned int i = range.GetFrom(); i <= range.GetTo(); ++i)
                seq_txt[i] = tolower(seq_txt[i]);
        }
    }

    CResidueVec seq(seq_txt.size());
    copy(seq_txt.begin(), seq_txt.end(), seq.begin());
    m_gnomon.reset(new CGnomonEngine(m_hmm_params, seq, TSignedSeqRange::GetWhole()));
}

CGnomonEngine& CGnomonAnnotator_Base::GetGnomon()
{
    return *m_gnomon;
}

MarkupCappedEst::MarkupCappedEst(const set<string>& _caps, int _capgap)
    : caps(_caps)
    , capgap(_capgap)
{}

void MarkupCappedEst::transform_align(CAlignModel& align)
{
    string acc = CIdHandler::ToString(*align.GetTargetId());
    int fivep = align.TranscriptExon(0).GetFrom();
    if(align.Strand() == eMinus)
        fivep = align.TranscriptExon(align.Exons().size()-1).GetFrom();
    if((align.Status()&CGeneModel::eReversed) == 0 && caps.find(acc) != caps.end() && fivep < capgap)
        align.Status() |= CGeneModel::eCap;
}

MarkupTrustedGenes::MarkupTrustedGenes(set<string> _trusted_genes) : trusted_genes(_trusted_genes) {}

void MarkupTrustedGenes::transform_align(CAlignModel& align)
{
    string acc = CIdHandler::ToString(*align.GetTargetId());
    if(trusted_genes.find(acc) != trusted_genes.end()) {
        CRef<CSeq_id> target_id(new CSeq_id);
        target_id->Assign(*align.GetTargetId());
        if(align.Type() == CGeneModel::eProt)
            align.InsertTrustedProt(target_id);
        else
            align.InsertTrustedmRNA(target_id);
    }
}

ProteinWithBigHole::ProteinWithBigHole(double _hthresh, double _hmaxlen, CGnomonEngine& _gnomon)
    : hthresh(_hthresh), hmaxlen(_hmaxlen), gnomon(_gnomon) {}
bool ProteinWithBigHole::model_predicate(CGeneModel& m)
{
    if ((m.Type() & CGeneModel::eProt)==0)
        return false;
    int total_hole_len = 0;
    for(unsigned int i = 1; i < m.Exons().size(); ++i) {
        if(!m.Exons()[i-1].m_ssplice || !m.Exons()[i].m_fsplice)
            total_hole_len += m.Exons()[i].GetFrom()-m.Exons()[i-1].GetTo()-1;
    }
    if(total_hole_len < hmaxlen*m.Limits().GetLength())
        return false;

    for(unsigned int i = 1; i < m.Exons().size(); ++i) {
        bool hole = !m.Exons()[i-1].m_ssplice || !m.Exons()[i].m_fsplice;
        int intron = m.Exons()[i].GetFrom()-m.Exons()[i-1].GetTo()-1;
        if (hole && gnomon.GetChanceOfIntronLongerThan(intron) < hthresh) {
            return true;
        } 
    }
    return false;
}

bool CdnaWithHole::model_predicate(CGeneModel& m)
{
    if ((m.Type() & CGeneModel::eProt)!=0)
        return false;
    return !m.Continuous();
}

HasShortIntron::HasShortIntron(CGnomonEngine& _gnomon)
    :gnomon(_gnomon) {}

bool HasShortIntron::model_predicate(CGeneModel& m)
{
    for(unsigned int i = 1; i < m.Exons().size(); ++i) {
        bool hole = !m.Exons()[i-1].m_ssplice || !m.Exons()[i].m_fsplice;
        int intron = m.Exons()[i].GetFrom()-m.Exons()[i-1].GetTo()-1;
        if (!hole && intron < gnomon.GetMinIntronLen()) {
            return true;
        } 
    }
    return false;
}

CutShortPartialExons::CutShortPartialExons(int _minex)
    : minex(_minex) {}

int EffectiveExonLength(const CModelExon& e, const CAlignMap& alignmap, bool snap_to_codons) {
    TSignedSeqRange shrinkedexon = alignmap.ShrinkToRealPoints(e,snap_to_codons);
    int exonlen = alignmap.FShiftedLen(shrinkedexon,false);  // length of the projection on transcript
    return min(exonlen,shrinkedexon.GetLength());
}

void CutShortPartialExons::transform_align(CAlignModel& a)
{
    if (a.Exons().empty())
        return;

    CAlignMap alignmap(a.GetAlignMap());
    if(a.Exons().size() == 1 && min(a.Limits().GetLength(),alignmap.FShiftedLen(alignmap.ShrinkToRealPoints(a.Limits()),false)) < minex) {
        // one exon and it is short
        a.CutExons(a.Limits());
        return;
    }

    bool snap_to_codons = ((a.Type() & CAlignModel::eProt)!=0);      
    TSignedSeqPos left  = a.Limits().GetFrom();
    if ((a.Type() & CAlignModel::eProt)==0 || !a.LeftComplete()) {
        for(unsigned int i = 0; i < a.Exons().size()-1; ++i) {
            if(EffectiveExonLength(a.Exons()[i], alignmap, snap_to_codons) >= minex) {
                break;
            } else {
                left = a.Exons()[i+1].GetFrom();
                if(a.Strand() == ePlus && (a.Status()&CGeneModel::eCap) != 0)
                    a.Status() ^= CGeneModel::eCap;
                if(a.Strand() == eMinus && (a.Status()&CGeneModel::ePolyA) != 0)
                    a.Status() ^= CGeneModel::ePolyA;
            }
        }
    }

    TSignedSeqPos right = a.Limits().GetTo();
    if ((a.Type() & CAlignModel::eProt)==0 || !a.RightComplete()) {
        for(unsigned int i = a.Exons().size()-1; i > 0; --i) {
            if(EffectiveExonLength(a.Exons()[i], alignmap, snap_to_codons) >= minex) {
                break;
            } else {
                right = a.Exons()[i-1].GetTo();
                if(a.Strand() == eMinus && (a.Status()&CGeneModel::eCap) != 0)
                    a.Status() ^= CGeneModel::eCap;
                if(a.Strand() == ePlus && (a.Status()&CGeneModel::ePolyA) != 0)
                    a.Status() ^= CGeneModel::ePolyA;
            }
        }
    }

    TSignedSeqRange newlimits(left,right);
    newlimits = alignmap.ShrinkToRealPoints(newlimits,snap_to_codons);
    if(newlimits != a.Limits()) {
        if(newlimits.Empty() || newlimits.GetLength() < minex || alignmap.FShiftedLen(newlimits,false) < minex) {
            a.CutExons(a.Limits());
            return;
        }
        a.Clip(newlimits,CAlignModel::eRemoveExons);
    }
    

    for (size_t i = 1; i < a.Exons().size()-1; ++i) {
        const CModelExon* e = &a.Exons()[i];
        
        while (i > 0 && !e->m_ssplice && EffectiveExonLength(*e, alignmap, snap_to_codons) < minex) {  //i==0 exon is long enough
            
            //this point is not an indel and is a codon boundary for proteins
            TSignedSeqPos remainingpoint = alignmap.ShrinkToRealPoints(TSignedSeqRange(a.Exons().front().GetFrom(),a.Exons()[i-1].GetTo()),snap_to_codons).GetTo();
            TSignedSeqPos left = e->GetFrom();
            if(remainingpoint < a.Exons()[i-1].GetTo())
                left = remainingpoint+1;
            a.CutExons(TSignedSeqRange(left,e->GetTo()));
            --i;
            e = &a.Exons()[i];
        }
        
        while (!e->m_fsplice && EffectiveExonLength(*e, alignmap, snap_to_codons) < minex) { // i always < size()-1
            
            //this point is not an indel and is a codon boundary for proteins
            TSignedSeqPos remainingpoint = alignmap.ShrinkToRealPoints(TSignedSeqRange(a.Exons()[i+1].GetFrom(),a.Exons().back().GetTo()),snap_to_codons).GetFrom();
            TSignedSeqPos right = e->GetTo();
            if(remainingpoint > a.Exons()[i+1].GetFrom())
                right = remainingpoint-1;
            
            a.CutExons(TSignedSeqRange(e->GetFrom(),right));
            e = &a.Exons()[i];
        }
    }
    return;
}

bool HasNoExons::model_predicate(CGeneModel& m)
{
    return m.Exons().empty();
}

bool SingleExon_AllEst::model_predicate(CGeneModel& m)
{
    return m.Exons().size() <= 1 && (m.Type() & (CAlignModel::eProt|CAlignModel::emRNA))==0;
}

bool SingleExon_Noncoding::model_predicate(CGeneModel& m)
{
    return m.Exons().size() <= 1 && m.Score() == BadScore();
}

LowSupport_Noncoding::LowSupport_Noncoding(int _minsupport)
    : minsupport(_minsupport)
{}
bool LowSupport_Noncoding::model_predicate(CGeneModel& m)
{
    return m.Score() == BadScore() && int(m.Support().size()) < minsupport && (m.Type() & (CAlignModel::eProt|CAlignModel::emRNA))==0;
}

END_SCOPE(gnomon)
END_SCOPE(ncbi)


