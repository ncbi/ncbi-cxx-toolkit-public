/*  $Id$
  ===========================================================================
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
    void SetConfirmedStartStopForProteinAlignments(TAlignModelList& alignments);
    void DropAlignmentInfo(TAlignModelList& alignments, TGeneModelList& models);
    void CollapsSimilarESTandSR(TGeneModelList& clust);

    void FilterOutChimeras(TGeneModelList& clust);
    void ScoreCDSes_FilterOutPoorAlignments(TGeneModelList& clust);
    void FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(TGeneModelList& clust);
    void ReplicateFrameShifts(TGeneModelList& models);


    TGeneModelList MakeChains(TGeneModelList& models);
    void FilterOutBadScoreChainsHavingBetterCompatibles(TGeneModelList& chains);
    void CombineCompatibleChains(list<CChain>& chains);
    void CombineChainsForPartialProteins(list<CChain>& chains, const vector<SChainMember*>& pointers);
    void CreateChainsForPartialProteins(list<CChain>& chains, vector<SChainMember*>& pointers);

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
typedef set<CGeneModel*> TGeneModelSet;

struct SChainMember
{
    SChainMember() :
        m_align(0), m_align_map(0), m_left_member(0), m_right_member(0),
        m_copy(0), m_contained(0), m_identical_count(0), m_contained_weight(0),
        m_left_num(0), m_right_num(0), m_num(0),
        m_type(eCDS), m_left_cds(0), m_right_cds(0), m_cds(0), m_included(false),  m_postponed(false),
        m_marked_for_deletion(false), m_marked_for_retention(false), 
        m_gapped_connection(false), m_fully_connected_to_part(-1), m_not_for_chaining(false),
        m_rlimb(numeric_limits<int>::max()),  m_llimb(numeric_limits<int>::max()) {}

    void CollectContainedAlignments(TGeneModelSet& chain_alignments);
    void MarkIncludedContainedAlignments(const TSignedSeqRange& limits);
    void MarkPostponedContainedAlignments();
    void CollectAllContainedAlignments(TGeneModelSet& chain_alignments);
    void MarkIncludedAllContainedAlignments(const TSignedSeqRange& limits);
    void MarkPostponedAllContainedAlignments();
    void MarkExtraCopiesForDeletion(const TSignedSeqRange& cds);
    void MarkAllExtraCopiesForDeletion(const TSignedSeqRange& cds);
    TContained ExtractContainedFromNested();

    CGeneModel* m_align;
    CAlignMap* m_align_map;
    SChainMember* m_left_member;
    SChainMember* m_right_member;
    TContained* m_copy;      // is used to make sure that the copy of already incuded duplicated alignment is not included in contained and doesn't trigger a new chain genereation
    TContained* m_contained;
    int m_identical_count;
    double m_contained_weight;
    double m_left_num, m_right_num, m_num; 
    int m_type, m_left_cds, m_right_cds, m_cds;
    bool m_included;
    bool m_postponed;
    bool m_marked_for_deletion;
    bool m_marked_for_retention;
    bool m_gapped_connection;          // used for gapped proteins
    int m_fully_connected_to_part;     // used for gapped proteins
    bool m_not_for_chaining;           // included in other alignmnet(s) and can't trigger a different chain
    int m_rlimb;                       // leftmost compatible rexon
    int m_llimb;                       // leftmost not compatible lexon

    int m_mem_id;
};

class CChain : public CGeneModel
{
public:
    CChain(const TGeneModelSet& chain_alignments);
    //    void MergeWith(const CChain& another_chain, const CGnomonEngine&gnomon, const SMinScor& minscor);
    void ToEvidence(vector<TEvidence>& mrnas,
                    vector<TEvidence>& ests, 
                    vector<TEvidence>& prots) const ;

    void RestoreTrimmedEnds(int trim);
    void RemoveFshiftsFromUTRs();
    void RestoreReasonableConfirmedStart(const CGnomonEngine& gnomon, TOrigAligns& orig_aligns);
    void SetOpenForPartialyAlignedProteins(map<string, pair<bool,bool> >& prot_complet, TOrigAligns& orig_aligns);
    void ClipToCompleteAlignment(EStatus determinant); // determinant - cap or polya
    void ClipLowCoverageUTR(double utr_clip_threshold);
    void CalculatedSupportAndWeightFromMembers();
    void RecalculateAfterClip();

    void SetConfirmedStartStopForCompleteProteins(map<string, pair<bool,bool> >& prot_complet, TOrigAligns& orig_aligns, const SMinScor& minscor);
    void CollectTrustedmRNAsProts(TOrigAligns& orig_aligns, const SMinScor& minscor);

    vector<CGeneModel*> m_members;
    int m_polya_cap_right_hard_limit;
    int m_polya_cap_left_hard_limit;
};


TContained SChainMember::ExtractContainedFromNested() {
    TContained contained;
    set<SChainMember*> included_in_list;
    list<SChainMember*> not_visited(1,this);
    while(!not_visited.empty()) {
        SChainMember* mbr = not_visited.front();
        for(int c = 0; c < (int)mbr->m_contained->size(); ++c) {
            SChainMember* mi = (*mbr->m_contained)[c];
            if(c < mbr->m_identical_count) {
                contained.push_back(mi);                  //action
            } else if(included_in_list.insert(mi).second) {
                not_visited.push_back(mi);                //store for future
            }
        }
        not_visited.pop_front();
    }
    return contained;
}



void SChainMember::CollectContainedAlignments(TGeneModelSet& chain_alignments)
{
    TContained contained = ExtractContainedFromNested();
    NON_CONST_ITERATE (TContained, i, contained) {
        SChainMember* mi = *i;
        chain_alignments.insert(mi->m_align);
    }
}

void SChainMember::MarkIncludedContainedAlignments(const TSignedSeqRange& limits)
{
    TContained contained = ExtractContainedFromNested();
    NON_CONST_ITERATE (TContained, i, contained) {
        SChainMember* mi = *i;
        if(mi->m_align->Limits().IntersectingWith(limits)) {   // not clipped
            mi->m_included = true;
            if (mi->m_copy != 0) {
                ITERATE(TContained, j, *mi->m_copy) {
                    SChainMember* mj = *j;
                    mj->m_included = true;
                }
            }
        }
    }
}

void SChainMember::MarkPostponedContainedAlignments()
{
    TContained contained = ExtractContainedFromNested();
    NON_CONST_ITERATE (TContained, i, contained) {
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
    TContained contained = ExtractContainedFromNested();
    NON_CONST_ITERATE (TContained, i, contained) {
        SChainMember* mi = *i;
        CGeneModel& algni = *mi->m_align;
        if(Include(cds, algni.ReadingFrame())) {
            mi->m_marked_for_retention = true;
            mi->m_marked_for_deletion = false;
            if (mi->m_copy != 0) {
                ITERATE(TContained, j, *mi->m_copy) {
                    SChainMember* mj = *j;
                    if(mj->m_marked_for_retention)      // already included in cds
                        continue;
                    else if(algni.HasStart()) {         // don't delete copy which overrides the start
                        CGeneModel& algnj = *mj->m_align;
                        if((algni.Strand() == ePlus && algni.ReadingFrame().GetTo() == algnj.ReadingFrame().GetTo()) ||
                           (algni.Strand() == eMinus && algni.ReadingFrame().GetFrom() == algnj.ReadingFrame().GetFrom()))
                            continue;
                    }
                    mj->m_marked_for_deletion = true;
                }
            }
        }
    }
}



void SChainMember::CollectAllContainedAlignments(TGeneModelSet& chain_alignments)
{
    CollectContainedAlignments(chain_alignments);
    
    for (SChainMember* left = m_left_member; left != 0; left = left->m_left_member) {
        left->CollectContainedAlignments(chain_alignments);
    }
        
    for (SChainMember* right = m_right_member; right != 0; right = right->m_right_member) {
        right->CollectContainedAlignments(chain_alignments);
    }
}        

void SChainMember::MarkIncludedAllContainedAlignments(const TSignedSeqRange& limits)
{
    MarkIncludedContainedAlignments(limits);
    
    for (SChainMember* left = m_left_member; left != 0; left = left->m_left_member) {
        left->MarkIncludedContainedAlignments(limits);
    }
        
    for (SChainMember* right = m_right_member; right != 0; right = right->m_right_member) {
        right->MarkIncludedContainedAlignments(limits);
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

struct GenomeOrderD
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)    // left end increasing, long first if left end equal
    {
        const TSignedSeqRange& alimits = ap->m_align->Limits();
        const TSignedSeqRange& blimits = bp->m_align->Limits();
        if(ap->m_align->Limits() == bp->m_align->Limits()) 
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
        else if(alimits.GetFrom() == blimits.GetFrom()) 
            return (alimits.GetTo() > blimits.GetTo());
        else 
            return (alimits.GetFrom() < blimits.GetFrom());
    }
};        

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
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
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
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
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
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
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
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
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
    CChainMembers() {}
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
    list<TContained> m_containedlist;
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
    m.m_mem_id = size();
    m_members.push_back(m);
    push_back(&m_members.back());

    m_containedlist.push_back(TContained());
    m_members.back().m_contained = &m_containedlist.back();

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
    big.m_contained_weight += small.m_align->Weight();

    if(big.m_align->Limits() == small.m_align->Limits()) {  // identical
        ++big.m_identical_count;
    } else if((int)big.m_contained->size() > big.m_identical_count && small.m_align->Limits().GetTo() <= big.m_contained->back()->m_align->Limits().GetTo())  // contained in next level
            return;

    big.m_contained->push_back(&small);
}


void CChainer::CutParts(TGeneModelList& models)
{
    m_data->CutParts(models);
}

TGeneModelList GetParts(const CGeneModel& algn) {
    TGeneModelList parts;
    
    int left = algn.Limits().GetFrom();
    for(unsigned int i = 1; i < algn.Exons().size(); ++i) {
        if (!algn.Exons()[i-1].m_ssplice || !algn.Exons()[i].m_fsplice) {
            CGeneModel m = algn;
            m.Clip(TSignedSeqRange(left,algn.Exons()[i-1].GetTo()),CGeneModel::eRemoveExons);
            parts.push_back(m);
            left = algn.Exons()[i].GetFrom();
        }
    }
    if(!parts.empty()) {
        CGeneModel m = algn;
        m.Clip(TSignedSeqRange(left,algn.Limits().GetTo()),CGeneModel::eRemoveExons);
        parts.push_back(m);
    }

    return parts;
}

void CChainer::CChainerImpl::CutParts(TGeneModelList& clust) {
    for(TGeneModelList::iterator iloop = clust.begin(); iloop != clust.end(); ) {
        TGeneModelList::iterator im = iloop++;

        TGeneModelList parts = GetParts(*im);
        if(!parts.empty()) {
            clust.splice(clust.begin(),parts);
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
            //            algn.Status() &= (~CGeneModel::eUnknownOrientation);
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

        //        _ASSERT((algn.Status()&CGeneModel::eUnknownOrientation) == 0);

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
                CCDSInfo cdsinfo = algn.GetCdsInfo();

                CCDSInfo shortcdsinfo = cdsinfo;
                TSignedSeqPos start = (algn.Strand() == ePlus) ? algn.RealCdsLimits().GetFrom() : algn.RealCdsLimits().GetTo();            
                shortcdsinfo.Set5PrimeCdsLimit(start);
                algn.SetCdsInfo(shortcdsinfo);

                CGeneModel algn_with_longcds = algn;
                if(algn.Strand() == ePlus) {
                    int full_rf_left = algn.Limits().GetFrom()+(algn.FShiftedLen(algn.Limits().GetFrom(), cdsinfo.Start().GetFrom(), false)-1)%3;
                    cdsinfo.SetStart(TSignedSeqRange::GetEmpty());
                    cdsinfo.SetReadingFrame(TSignedSeqRange(full_rf_left,cdsinfo.ReadingFrame().GetTo()));
                } else {
                    int full_rf_right = algn.Limits().GetTo()-(algn.FShiftedLen(cdsinfo.Start().GetTo(),algn.Limits().GetTo(),false)-1)%3;
                    cdsinfo.SetStart(TSignedSeqRange::GetEmpty());
                    cdsinfo.SetReadingFrame(TSignedSeqRange(cdsinfo.ReadingFrame().GetFrom(),full_rf_right));
                }

                if(mbr.m_copy != 0) {
                    if(mbr.m_copy->front()->m_align->Strand() == algn.Strand()) {     // first copy is original alignment; for not oriented the second copy is reverse
                        if(mbr.m_copy->front()->m_align->ReadingFrame() == cdsinfo.ReadingFrame())
                            continue;
                    } else if((*mbr.m_copy)[1]->m_align->ReadingFrame() == cdsinfo.ReadingFrame()) {
                        continue;
                    }
                }

                algn_with_longcds.SetCdsInfo(cdsinfo);                        
                clust.push_back(algn_with_longcds);
                pointers.InsertMember(clust.back(), &mbr);
            }
        }
    }
}

void CChainer::CChainerImpl::FindContainedAlignments(vector<SChainMember*>& pointers) {

    set<int> left_exon_ends, right_exon_ends;
    ITERATE(vector<SChainMember*>, ip, pointers) {
        const CGeneModel& algn = *(*ip)->m_align;
        for(int i = 1; i < (int)algn.Exons().size(); ++i) {
            if(algn.Exons()[i-1].m_ssplice && algn.Exons()[i].m_fsplice) {
                left_exon_ends.insert(algn.Exons()[i].GetFrom());
                right_exon_ends.insert(algn.Exons()[i-1].GetTo());
            }
        }
    }
    NON_CONST_ITERATE(vector<SChainMember*>, ip, pointers) {
        SChainMember& mi = **ip;
        CGeneModel& ai = *mi.m_align;

        set<int>::iterator ri = right_exon_ends.lower_bound(ai.Limits().GetTo()); // leftmost compatible rexon
        mi.m_rlimb =  numeric_limits<int>::max();
        if(ri != right_exon_ends.end())
            mi.m_rlimb = *ri;
        set<int>::iterator li = left_exon_ends.upper_bound(ai.Limits().GetFrom()); // leftmost not compatible lexon
        mi.m_llimb = numeric_limits<int>::max() ;
        if(li != left_exon_ends.end())
            mi.m_llimb = *li;        
    }

//  finding contained subalignments (alignment is contained in itself) and selecting longer alignments for chaining

    sort(pointers.begin(),pointers.end(),GenomeOrderD());
    for(int i = 0; i < (int)pointers.size(); ++i) {
        SChainMember& mi = *pointers[i];
        CGeneModel& ai = *mi.m_align;
        const CCDSInfo& ai_cds_info = ai.GetCdsInfo();
        TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();

        int jfirst = i;
        while(jfirst > 0 && pointers[jfirst-1]->m_align->Limits() == ai.Limits())
            --jfirst;
        for(int j = jfirst; j < (int)pointers.size() && pointers[j]->m_align->Limits().GetFrom() < ai.Limits().GetTo(); ++j) {

            if(i == j) {
                IncludeInContained(mi, mi);          // include self
                continue;
            }

            SChainMember& mj = *pointers[j];
            CGeneModel& aj = *mj.m_align;

            if(aj.Strand() != ai.Strand())
                continue;

            if(!Include(ai.Limits(),aj.Limits()))
                continue;

            if(mi.m_type == eCDS && mj.m_type  == eRightUTR)
                continue;    // avoid including UTR copy

            if(mi.m_type != eCDS && mj.m_type  != mi.m_type)
                continue;    // avoid including UTR copy and avoid including CDS into UTR because that will change m_type

            if(mi.m_type == eCDS && mj.m_type != eCDS && (aj.Limits()&ai.MaxCdsLimits()).GetLength() >= min(6,aj.Limits().GetLength()))  // UTR in CDS
                continue;


            const CCDSInfo& aj_cds_info = aj.GetCdsInfo();
            TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();

            if(mi.m_type == eCDS && mj.m_type == eCDS) { // CDS in CDS
                TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();
                if (!Include(max_cds_limits, ExtendedMaxCdsLimits(ai) + ExtendedMaxCdsLimits(aj)))
                    continue;;
                if(!Include(ai_rf,aj_rf))
                    continue;

                if(ai_rf.GetFrom() != aj_rf.GetFrom()) {
                    TSignedSeqPos j_from = mi.m_align_map->MapOrigToEdited(aj_rf.GetFrom());
                    if(j_from < 0)
                        continue;
                    TSignedSeqPos i_from = mi.m_align_map->MapOrigToEdited(ai_rf.GetFrom()); 
                    if(abs(j_from-i_from)%3 != 0)
                        continue;
                }
            }

            int iex = ai.Exons().size();
            int jex = aj.Exons().size();
            if(jex > iex)
                continue;
            if(iex > 1) {                                               // big alignment is spliced
                int fex = 0;
                while(fex < iex && ai.Exons()[fex].GetTo() < aj.Limits().GetFrom()) {
                    ++fex;
                }
                if(ai.Exons()[fex].GetFrom() > aj.Limits().GetFrom())   // first aj exon is in ai intron
                    continue;

                if(iex-fex < jex)                                       // not enough exons left in ai
                    continue;

                if(ai.Exons()[fex+jex-1].GetTo() < aj.Limits().GetTo()) // last aj exon is in ai intron
                    continue;

                bool compatible = true;
                for(int j = 0; compatible && j < jex-1; ++j) {
                    if(aj.Exons()[j].GetTo() != ai.Exons()[fex+j].GetTo() || aj.Exons()[j+1].GetFrom() != ai.Exons()[fex+j+1].GetFrom())  // different intron
                        compatible = false;
                }
                if(!compatible)
                    continue;
            }

            IncludeInContained(mi, mj);

            if(mj.m_type  != mi.m_type)
                continue;
            if((aj.Status()&CGeneModel::ePolyA) != 0 || (aj.Status()&CGeneModel::eCap) != 0)
                continue;
            if((aj.Type()&CGeneModel::eProt) != 0)                               // proteins (actually only gapped) should be directly available
                continue;
            if(ai.Limits() == aj.Limits())
                continue;
            if(mj.m_rlimb < ai.Limits().GetTo() || mj.m_llimb != mi.m_llimb)      // bigger alignment may interfere with splices
                continue;
            if(mi.m_type == eCDS && mj.m_type == eCDS && !Include(ai_cds_info.MaxCdsLimits(),aj_cds_info.MaxCdsLimits()))  // bigger alignment restricts the cds
                continue;
                
            mj.m_not_for_chaining = true; 
        }
    }
}

#define START_BONUS 600

bool LRCanChainItoJ(int& delta_cds, double& delta_num, const SChainMember& mi, const SChainMember& mj, int oep, TContained& contained) {

    const CGeneModel& ai = *mi.m_align;
    const CGeneModel& aj = *mj.m_align;


    if(aj.Strand() != ai.Strand())
        return false;

    const CCDSInfo& ai_cds_info = ai.GetCdsInfo();
    TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();

    const CCDSInfo& aj_cds_info = aj.GetCdsInfo();
    TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();
 
    switch(mi.m_type) {
    case eCDS: 
        if(mj.m_type == eRightUTR) 
            return false;
        else if(mj.m_type == eLeftUTR && (!ai.LeftComplete() || (aj.Limits()&ai_rf).GetLength() > 5))
            return false;
        else 
            break;
    case eLeftUTR: 
        if(mj.m_type != eLeftUTR) 
            return false;
        else 
            break;
    case eRightUTR:
        if(mj.m_type == eLeftUTR) 
            return false;
        else if(mj.m_type == eCDS && (!aj.RightComplete() || (ai.Limits()&aj_rf).GetLength() > 5))
            return false;
        else 
            break;
    default:
        return false;    
    }

    switch(ai.MutualExtension(aj)) {
    case 0:              // not compatible 
        return false;
    case 1:              // no introns in intersection
        if(mi.m_type == eCDS && mj.m_type == eCDS)  // no intersecting limit for coding 
            break;
        if ((ai.Limits() & aj.Limits()).GetLength() < oep) 
            return false;
        break;
    default:             // one or more introns in intersection 
        break;
    }
            
    int cds_overlap = 0;

    if(mi.m_type == eCDS && mj.m_type == eCDS) {
        int genome_overlap =  ai_rf.GetLength()+aj_rf.GetLength()-(ai_rf+aj_rf).GetLength();
        if(genome_overlap < 0)
            return false;

        TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();

        if (!Include(max_cds_limits, ExtendedMaxCdsLimits(ai) + ExtendedMaxCdsLimits(aj)))
            return false;

        if((Include(ai_rf,aj_rf) || Include(aj_rf,ai_rf)) && ai_rf.GetFrom() != aj_rf.GetFrom() && ai_rf.GetTo() != aj_rf.GetTo())
            return false;
        
        cds_overlap = mi.m_align_map->FShiftedLen(ai_rf&aj_rf,false);
        if(cds_overlap%3 != 0)
            return false;

        if(ai.HasStart() && aj.HasStart())
            cds_overlap += START_BONUS; 
    }

    delta_cds = mi.m_cds-cds_overlap;

    TContained::const_iterator endsp = upper_bound(contained.begin(),contained.end(),&mj,LeftOrder()); // first alignmnet contained in ai and outside aj
    delta_num = 0;
    for(TContained::const_iterator ic = endsp; ic != contained.end(); ++ic) 
        delta_num += (*ic)->m_align->Weight();

    return true;
}

void LRIinit(SChainMember& mi) {
    CGeneModel& ai = *mi.m_align;
    const CCDSInfo& ai_cds_info = ai.GetCdsInfo();
    TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();

    mi.m_num = mi.m_contained_weight;

    mi.m_cds = mi.m_align_map->FShiftedLen(ai_rf,false);
    if(ai.HasStart()) {
        mi.m_cds += START_BONUS;
        _ASSERT((ai.Strand() == ePlus && ai_cds_info.Start().GetFrom() == ai_cds_info.MaxCdsLimits().GetFrom()) || 
                (ai.Strand() == eMinus && ai_cds_info.Start().GetTo() == ai_cds_info.MaxCdsLimits().GetTo()));
    }

    mi.m_left_member = 0;
    mi.m_left_num = mi.m_num;
    mi.m_left_cds =  mi.m_cds;

    mi.m_gapped_connection = false;
    mi.m_fully_connected_to_part = -1;
}

void CChainer::CChainerImpl::LeftRight(vector<SChainMember*>& pointers)
{
    sort(pointers.begin(),pointers.end(),LeftOrderD());
    TIVec right_ends(pointers.size());
    for(int k = 0; k < (int)pointers.size(); ++k)
        right_ends[k] = pointers[k]->m_align->Limits().GetTo();
    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        LRIinit(mi);
        TContained micontained = mi.ExtractContainedFromNested();
        sort(micontained.begin(),micontained.end(),LeftOrderD());
        
        TIVec::iterator lb = lower_bound(right_ends.begin(),right_ends.end(),ai.Limits().GetFrom());
        vector<SChainMember*>::iterator jfirst = pointers.begin();
        if(lb != right_ends.end())
            jfirst = pointers.begin()+(lb-right_ends.begin()); // skip all on the left side
        for(vector<SChainMember*>::iterator j = jfirst; j < i; ++j) {
            SChainMember& mj = **j;

            int delta_cds;
            double delta_num;
            if(LRCanChainItoJ(delta_cds, delta_num, mi, mj, intersect_limit, micontained)) {
                int newcds = mj.m_left_cds+delta_cds;
                double newnum = mj.m_left_num+delta_num;
                if (newcds > mi.m_left_cds || (newcds == mi.m_left_cds && newnum > mi.m_left_num)) {
                    mi.m_left_cds = newcds;
                    mi.m_left_num = newnum;
                    mi.m_left_member = &mj;
                    _ASSERT(mj.m_align->Limits().GetFrom() < ai.Limits().GetFrom() && mj.m_align->Limits().GetTo() < ai.Limits().GetTo());
                }
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
        TContained micontained = mi.ExtractContainedFromNested();
        sort(micontained.begin(),micontained.end(),RightOrderD());
        
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
            
            TContained::iterator endsp = upper_bound(micontained.begin(),micontained.end(),&mj,RightOrder()); // first alignment contained in ai and outside aj
            double delta = 0;
            for(TContained::iterator ic = endsp; ic != micontained.end(); ++ic) 
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
void CChainer::CChainerImpl::MakeChains(TGeneModelList& clust, list<CChain>& chains)
{
    if(clust.empty()) return;

    CChainMembers allpointers(clust);

    DuplicateNotOriented(allpointers, clust);
    ScoreCdnas(allpointers);
    Duplicate5pendsAndShortCDSes(allpointers, clust);
    DuplicateUTRs(allpointers);
    FindContainedAlignments(allpointers);
    vector<SChainMember*> pointers;
    ITERATE(vector<SChainMember*>, ip, allpointers) {
        if(!(*ip)->m_not_for_chaining)
            pointers.push_back(*ip);
    }

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

        TGeneModelSet chain_alignments;
        mi.CollectAllContainedAlignments(chain_alignments);
        CChain chain(chain_alignments);
        TSignedSeqRange i_rf = chain.ReadingFrame();
        m_gnomon->GetScore(chain);
        mi.MarkIncludedAllContainedAlignments(chain.Limits());

        if(chain.Score() == BadScore())
            continue;
        if((chain.Type() & CGeneModel::eProt) == 0 && !chain.ConfirmedStart() && 
           chain.Score() < 2*minscor.m_min && chain.FShiftedLen(chain.GetCdsInfo().Cds(),true) <  2*minscor.m_cds_len)
            continue;

        TSignedSeqRange n_rf = chain.ReadingFrame();
        if(!i_rf.IntersectingWith(n_rf))
            continue;
        int a,b;
        if(n_rf.GetFrom() <= i_rf.GetFrom()) {
            a = n_rf.GetFrom();
            b = i_rf.GetTo();
        } else {
            a = i_rf.GetFrom();
            b = n_rf.GetTo();
        }
        if(chain.FShiftedLen(a,b,true)%3 != 0)
            continue;

        mi.MarkAllExtraCopiesForDeletion(chain.RealCdsLimits());
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

        TGeneModelSet chain_alignments;
        mi.CollectAllContainedAlignments(chain_alignments);
        CChain chain(chain_alignments);

        m_gnomon->GetScore(chain);
        chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns);

        double ms = GoodCDNAScore(chain);
        if ((chain.Type() & CGeneModel::eProt)==0 && !chain.ConfirmedStart()) 
            RemovePoorCds(chain,ms);
        mi.MarkIncludedAllContainedAlignments(chain.Limits());   // alignments clipped below mighy not be in any chain
        chain.ClipToCompleteAlignment(CGeneModel::eCap);
        chain.ClipToCompleteAlignment(CGeneModel::ePolyA);
        chain.ClipLowCoverageUTR(minscor.m_utr_clip_threshold);
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


        /*
        vector<SChainMember*> mal;
        mal.push_back(&mi);
        for (SChainMember* left = mi.m_left_member; left != 0; left = left->m_left_member) {
            mal.push_back(left);
        }
        for (SChainMember* right = mi.m_right_member; right != 0; right = right->m_right_member) {
            mal.push_back(right);
        }
        sort(mal.begin(),mal.end(),GenomeOrderD());
        ITERATE(vector<SChainMember*>, imal, mal) {
            cout << mi.m_mem_id << '\t' << (*imal)->m_mem_id << '\t' << (*imal)->m_align->ID() << '\t' << (*imal)->m_align->Limits() << endl;
        }
        */

#ifdef _DEBUG
            chain.AddComment("Member "+NStr::IntToString(mi.m_mem_id));
#endif
            chains.push_back(chain);
    }

    CreateChainsForPartialProteins(chains, pointers);
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

struct AntiAlignSeqOrder
{
    bool operator()(const CGeneModel* ap, const CGeneModel* bp)
    {
        if (ap->Limits().GetFrom() != bp->Limits().GetFrom()) return ap->Limits().GetFrom() > bp->Limits().GetFrom();
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

void CChainer::CChainerImpl::CreateChainsForPartialProteins(list<CChain>& chains, vector<SChainMember*>& pointers) {

    sort(pointers.begin(),pointers.end(),LeftOrderD());

    typedef map<Int8, vector<CGeneModel*> > TIdChainMembermap;
    TIdChainMembermap protein_parts;
    TIVec right_ends(pointers.size());
    vector<SChainMember> no_gap_members(pointers.size());   // helper chain members; will be used for gap filling optimisation
    for(int k = 0; k < (int)pointers.size(); ++k) {
        SChainMember& mi = *pointers[k];
        right_ends[k] = mi.m_align->Limits().GetTo();
        no_gap_members[k] = mi;
        if((mi.m_align->Type() & CGeneModel::eProt) && (mi.m_copy == 0 || mi.m_align->HasStart())) {  // only prots with start can have copies
            protein_parts[mi.m_align->ID()].push_back(mi.m_align);
        }
    }

    typedef multimap< int,vector<CGeneModel*>*,greater<int> > TLenChainMemberPmap;
    TLenChainMemberPmap gapped_sorted_protein_parts;
    NON_CONST_ITERATE(TIdChainMembermap, ip, protein_parts) {
        vector<CGeneModel*>& parts = ip->second;
        if(parts.size() > 1) {
            sort(parts.begin(),parts.end(),AlignSeqOrder());
            int align_len = 0;
            ITERATE(vector<CGeneModel*>, k, parts)
                align_len += (*k)->AlignLen();
            gapped_sorted_protein_parts.insert(TLenChainMemberPmap::value_type(align_len,&parts));
        }
    }

    NON_CONST_ITERATE(TLenChainMemberPmap, ip, gapped_sorted_protein_parts) {  // make chains starting from long proteins
        vector<CGeneModel*>& parts = *ip->second;
        Int8 id = parts.front()->ID();

        CGeneModel palign(parts.front()->Strand(), id, CGeneModel::eProt);
        ITERATE(vector<CGeneModel*>, k, parts) {
            CGeneModel part = **k;
            CCDSInfo cds = part.GetCdsInfo();
            cds.Clear5PrimeCdsLimit();
            part.SetCdsInfo(cds);
            palign.Extend(part);
        }
        m_gnomon->GetScore(palign);

        bool connected = false;
        NON_CONST_ITERATE(list<CChain>, k, chains) {
            if(palign.Strand() == k->Strand() && palign.IsSubAlignOf(*k)) {
                connected = true;
#ifdef _DEBUG
                k->AddComment("Was connected "+orig_aligns[palign.ID()]->TargetAccession());
#endif
                break;
            }
        }
        if(connected)
            continue;


        SChainMember* best_right = 0;

        int first_member = pointers.size()-1;
        int leftpos = palign.Limits().GetFrom();
        for(int i = pointers.size()-1; i >= 0; --i) {
            TSignedSeqRange limi = pointers[i]->m_align->Limits();
            if(limi.GetTo() >= leftpos) {
                first_member = i;
                leftpos = min(leftpos,limi.GetFrom());
            } else {
                break;
            }
        }

        int last_member = 0;
        int rightpos = palign.Limits().GetTo();
        for(int i = 0; i < (int)pointers.size(); ++i) {
            TSignedSeqRange limi = pointers[i]->m_align->Limits();
            if(Include(limi,rightpos)) {
                last_member = i;
                rightpos = max(rightpos,limi.GetTo());
            }
        }

        for(int i = first_member; i <= last_member; ++i) {
            SChainMember& mi = *pointers[i];                   // best connection maybe gapped
            SChainMember& mi_no_gap = no_gap_members[i];       // best not gapped connection (if any)
            CGeneModel& ai = *mi.m_align;
            LRIinit(mi);
            LRIinit(mi_no_gap);
            TContained micontained = mi.ExtractContainedFromNested();
            sort(micontained.begin(),micontained.end(),LeftOrderD());

            if(ai.Strand() != palign.Strand())
                continue;

            int part_to_connect =  parts.size()-1;
            while(part_to_connect >= 0 && ai.Limits().GetFrom() <= parts[part_to_connect]->Limits().GetFrom())
                --part_to_connect;

            if(part_to_connect >=0 && ai.Limits().GetFrom() < parts[part_to_connect]->Limits().GetTo() && !parts[part_to_connect]->isCompatible(ai))  // overlaps with part but not compatible
                continue;

            bool compatible_with_included_parts = true;
            int last_included_part = -1;
            bool includes_first_part = false;
            for(int p = part_to_connect+1; p < (int)parts.size(); ++p) {
                if(Include(ai.Limits(),parts[p]->Limits())) {
                    bool found = false;
                    ITERATE(TContained, ic, micontained) {
                        if((*ic)->m_align->ID() == id && (*ic)->m_align->Limits() == parts[p]->Limits()) {
                            found = true;
                            last_included_part = p;
                            if(p == 0)
                                includes_first_part = true;
                            break;
                        }
                    }
                    if(!found) {
                        compatible_with_included_parts = false;
                        break;
                    }
                } else if(ai.Limits().IntersectingWith(parts[p]->Limits())) {
                    if(!parts[p]->isCompatible(ai)) {
                        compatible_with_included_parts = false;
                        break;
                    }
                } else {
                    break;
                }
            }

            if(!compatible_with_included_parts)
                continue;

            if(includes_first_part) {
                mi.m_fully_connected_to_part = last_included_part;
                mi_no_gap.m_fully_connected_to_part = last_included_part;
            }

            TIVec::iterator lb = lower_bound(right_ends.begin(),right_ends.end(),(part_to_connect >= 0 ? parts[part_to_connect]->Limits().GetTo() : ai.Limits().GetFrom()));
            int jfirst = 0;
            if(lb != right_ends.end())
                jfirst = lb-right_ends.begin(); // skip all on the left side

            for(int j = jfirst; j < i; ++j) {
                SChainMember& mj = *pointers[j];                   // best connection maybe gapped
                SChainMember& mj_no_gap = no_gap_members[j];       // best not gapped connection (if any)
                CGeneModel& aj = *mj.m_align;

                if( ai.Strand() != aj.Strand())
                    continue;

                if(part_to_connect >= 0 && mj.m_fully_connected_to_part < part_to_connect)   // alignmnet is not connected to all previous parts
                    continue;

                if(ai.Limits().GetFrom() > aj.Limits().GetTo() && part_to_connect >= 0 && part_to_connect < (int)parts.size()-1 &&       // gap is not closed
                   mj_no_gap.m_fully_connected_to_part == part_to_connect &&                                                             // no additional gap
                   mi.m_type == eCDS && mj.m_type == eCDS &&
                   aj.GetCdsInfo().MaxCdsLimits().GetTo() == TSignedSeqRange::GetWholeTo() && 
                   ai.GetCdsInfo().MaxCdsLimits().GetFrom() == TSignedSeqRange::GetWholeFrom()) {                                        // reading frame not interrupted 
                    
#define PGAP_PENALTY 120

                    int newcds = mj_no_gap.m_left_cds+mi.m_cds - PGAP_PENALTY;
                    double newnum = mj_no_gap.m_left_num+mi.m_num; 

                    if(mi.m_left_member == 0 || newcds > mi.m_left_cds || (newcds == mi.m_left_cds && newnum > mi.m_left_num)) {
                        mi.m_left_cds = newcds;
                        mi.m_left_num = newnum;
                        mi.m_left_member = &mj_no_gap;
                        mi.m_gapped_connection = true;
                        mi.m_fully_connected_to_part = part_to_connect;
                    }
                } else {
                    int delta_cds;
                    double delta_num;
                    if(LRCanChainItoJ(delta_cds, delta_num, mi, mj, intersect_limit, micontained)) {      // i and j connected continuosly
                        int newcds = mj.m_left_cds+delta_cds;
                        double newnum = mj.m_left_num+delta_num;
                        if (mi.m_left_member == 0 || newcds > mi.m_left_cds || (newcds == mi.m_left_cds && newnum > mi.m_left_num)) {
                            mi.m_left_cds = newcds;
                            mi.m_left_num = newnum;
                            mi.m_gapped_connection = mj.m_gapped_connection;
                            mi.m_left_member = &mj;
                            mi.m_fully_connected_to_part = part_to_connect;
                            if(!mi.m_gapped_connection)
                                mi_no_gap = mi; 
                        } else if(mj_no_gap.m_fully_connected_to_part == part_to_connect) {
                            newcds = mj_no_gap.m_left_cds+delta_cds;
                            newnum = mj_no_gap.m_left_num+delta_num;
                            if (mi_no_gap.m_left_member == 0 || newcds > mi_no_gap.m_left_cds || (newcds == mi_no_gap.m_left_cds && newnum > mi_no_gap.m_left_num)) {
                                mi_no_gap.m_left_cds = newcds;
                                mi_no_gap.m_left_num = newnum;
                                mi_no_gap.m_left_member = &mj_no_gap;
                                mi_no_gap.m_fully_connected_to_part = part_to_connect;
                            }
                        }
                    }
                }
            }

            if(mi.m_left_member != 0 && last_included_part >= 0) {
                mi.m_fully_connected_to_part = last_included_part;
                mi.m_gapped_connection = false;
                mi_no_gap = mi;
            }

            if(mi.m_fully_connected_to_part == (int)parts.size()-1) {   // includes all parts
                if(best_right == 0 || (mi.m_left_cds >  best_right->m_left_cds || (mi.m_left_cds ==  best_right->m_left_cds && mi.m_left_num >  best_right->m_left_num)) ) 
                    best_right = &mi;
            }
        }

        _ASSERT(best_right != 0);

        TGeneModelSet chain_alignments;
        best_right->CollectAllContainedAlignments(chain_alignments);
        chain_alignments.insert(&palign);
        CChain chain(chain_alignments);
        remove(chain.m_members.begin(),chain.m_members.end(),&palign);
        chain.CalculatedSupportAndWeightFromMembers();
        m_gnomon->GetScore(chain);
        chain.ClipToCompleteAlignment(CGeneModel::eCap);
        chain.ClipToCompleteAlignment(CGeneModel::ePolyA);
        chain.ClipLowCoverageUTR(minscor.m_utr_clip_threshold);

#ifdef _DEBUG
        chain.AddComment("Connected "+orig_aligns[palign.ID()]->TargetAccession());
#endif
        

        chains.push_back(chain);
    }
}

void CChainer::CChainerImpl::CombineChainsForPartialProteins(list<CChain>& chains, const vector<SChainMember*>& apointers) {

    typedef map<Int8, vector<CGeneModel*> > TIdChainMembermap;
    TIdChainMembermap protein_parts;

    ITERATE(CChainMembers, i, apointers) {
        SChainMember& mi = **i;
        if(mi.m_align->Type() & CGeneModel::eProt) {
            Int8 id = mi.m_align->ID();
            if(mi.m_copy == 0 || mi.m_align->HasStart())   // only prots with start have copies
                protein_parts[id].push_back(mi.m_align);
        }
    }

    ITERATE(TIdChainMembermap, ip, protein_parts) {
        Int8 id = ip->first;
        vector<CGeneModel*> parts = ip->second;
        if(parts.size() < 2)
            continue;

        sort(parts.begin(),parts.end(),AlignSeqOrder());

        CGeneModel align(parts.front()->Strand(), id, CGeneModel::eProt);
        ITERATE(vector<CGeneModel*>, k, parts)
            align.Extend(**k);
        m_gnomon->GetScore(align);
        

        bool connected = false;
        ITERATE(list<CChain>, k, chains) {
            if(align.Strand() == k->Strand() && align.IsSubAlignOf(*k)) {
                connected = true;
                break;
            }
        }
        if(connected)
            continue;

        list<CChain> partial_chains;
        ITERATE(vector<CGeneModel*>, k, parts) {
            TGeneModelSet ca;
            ca.insert(*k);
            partial_chains.push_back(CChain(ca));
        }

        ITERATE(list<CChain>, k, chains) {
            const CChain& chain = *k;

            if(!chain.Continuous())
                continue;

            CSupportInfoSet support = chain.Support();
            if(support.insert(CSupportInfo(id)))    // check that chain contains part(s) 
                continue;

            if(chain.Score() == BadScore() || chain.Strand() != align.Strand() || !chain.isCompatible(align) || !Include(chain.GetCdsInfo().MaxCdsLimits(),align.Limits()))
                continue;

            TSignedSeqRange cds = chain.GetCdsInfo().Cds();
            bool bad_overlap = false;
            ITERATE(vector<CGeneModel*>, k, parts) {
                TSignedSeqRange lim = (*k)->Limits();
                if(lim.IntersectingWith(chain.Limits())) {
                    if(!Include(cds,lim) || (chain.FShiftedLen(cds.GetFrom(),lim.GetFrom())-1)%3 != 0)
                        bad_overlap = true;
                }
            }
            if(bad_overlap)
                continue;
            
            partial_chains.push_back(chain);
        }

        CChainMembers chain_pointers;
        NON_CONST_ITERATE(list<CChain>, k, partial_chains) {
            SChainMember m;
            m.m_align = &(*k);
            chain_pointers.InsertMember(m);
        }
        sort(chain_pointers.begin(),chain_pointers.end(),LeftOrderD());
        NON_CONST_ITERATE(vector<SChainMember*>, i, chain_pointers) {
            SChainMember& mi = **i;
            CGeneModel& ai = *mi.m_align;
            const CCDSInfo& ai_cds_info = ai.GetCdsInfo();
            TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();
            TSignedSeqRange ai_limits = ai.Limits();
        
            mi.m_num = ai.Weight();
            mi.m_cds = mi.m_align_map->FShiftedLen(ai_rf,false);

            mi.m_left_member = 0;
            mi.m_left_num = mi.m_num;
            mi.m_left_cds =  mi.m_cds;

            TSignedSeqRange left_part;
            ITERATE(vector<CGeneModel*>, k, parts) {
                if(Include(ai.Limits(),(*k)->Limits()))
                    break;
                left_part = (*k)->Limits();
            }
            if(left_part.Empty())
                continue;

            for(vector<SChainMember*>::iterator j = chain_pointers.begin(); (*j)->m_align->Limits().GetTo() < ai.Limits().GetFrom(); ++j) {  // connect not overlapping chains
                SChainMember& mj = **j;
                CGeneModel& aj = *mj.m_align;
                if(!Include(aj.Limits(),left_part))
                    continue;

                int newcds = mj.m_left_cds+mi.m_cds;
                double newnum = mj.m_left_num+mi.m_num;

                if(newcds > mi.m_left_cds || (newcds == mi.m_left_cds && newnum > mi.m_left_num)) {
                    mi.m_left_cds = newcds;
                    mi.m_left_num = newnum;
                    mi.m_left_member = &mj;
                }                
            }
        }

        NON_CONST_ITERATE(vector<SChainMember*>, i, chain_pointers) {
            (*i)->m_cds = (*i)->m_left_cds;
            (*i)->m_num = (*i)->m_left_num;
        }   
        sort(chain_pointers.begin(),chain_pointers.end(),CdsNumOrder());
        SChainMember* right_end = 0;
        ITERATE(vector<SChainMember*>, i, chain_pointers) {
            SChainMember& mi = **i;
            if(mi.m_align->Limits().GetTo() >= align.Limits().GetTo()) {
                right_end = &mi;
                break;
            }
        }

        TGeneModelSet selected_chains;
        selected_chains.insert(&align);
        vector<CGeneModel*> members;
        for(SChainMember* p = right_end; p != 0; p = p->m_left_member) {
            selected_chains.insert(p->m_align);
            CChain* c = dynamic_cast<CChain*>(p->m_align);
            members.insert(members.end(),c->m_members.begin(),c->m_members.end());
        }

        CChain combined_chain(selected_chains);
        sort(members.begin(),members.end(),AlignSeqOrder());
        combined_chain.m_members = members;
        combined_chain.CalculatedSupportAndWeightFromMembers();
        m_gnomon->GetScore(combined_chain);
        combined_chain.AddComment("Connected protein "+orig_aligns[align.ID()]->TargetAccession());
        chains.push_front(combined_chain);
    }
}


void CChainer::CChainerImpl::CombineCompatibleChains(list<CChain>& chains) {
    for(list<CChain>::iterator itt = chains.begin(); itt != chains.end(); ++itt) {
        for(list<CChain>::iterator jt = chains.begin(); jt != chains.end();) {
            list<CChain>::iterator jtt = jt++;
            if(itt != jtt && itt->Strand() == jtt->Strand() && Include(itt->ReadingFrame(),jtt->ReadingFrame()) && jtt->IsSubAlignOf(*itt) 
               && (itt->FShiftedLen(itt->GetCdsInfo().Cds().GetFrom(),jtt->GetCdsInfo().Cds().GetFrom(),false)-1)%3 == 0 ) {
                
                TGeneModelSet chain_alignments(itt->m_members.begin(),itt->m_members.end());
                ITERATE(vector<CGeneModel*>, is, jtt->m_members) 
                    chain_alignments.insert(*is);
                itt->m_members.resize(chain_alignments.size(),0);
                copy(chain_alignments.begin(),chain_alignments.end(),itt->m_members.begin());
                sort(itt->m_members.begin(),itt->m_members.end(),AlignSeqOrder());
                itt->CalculatedSupportAndWeightFromMembers();
                chains.erase(jtt);
            }
        }
    }
}

double CChainer::CChainerImpl::GoodCDNAScore(const CGeneModel& algn)
{
    if(algn.FShiftedLen(algn.GetCdsInfo().Cds(),true) >  minscor.m_cds_len)
        return 0.99*BadScore();
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

CChain::CChain(const TGeneModelSet& chain_alignments)
{
    _ASSERT(chain_alignments.size()>0);

    SetType(eChain);

    ITERATE(TGeneModelSet, it, chain_alignments) {
        m_members.push_back(*it);
    }
    sort(m_members.begin(),m_members.end(),AlignSeqOrder());

    list<CGeneModel> extened_parts;
    vector<CGeneModel*> extened_parts_and_gapped;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        
        if(!align.Continuous()) {
            extened_parts_and_gapped.push_back(*it);
        } else if(extened_parts.empty() || !align.Limits().IntersectingWith(extened_parts.back().Limits())) {
            extened_parts.push_back(align);
            extened_parts_and_gapped.push_back(&extened_parts.back());
        } else {
            extened_parts.back().Extend(align, false);
        }
    }
    sort(extened_parts_and_gapped.begin(),extened_parts_and_gapped.end(),AlignSeqOrder());

    EStrand strand = extened_parts_and_gapped.front()->Strand();
    SetStrand(strand);

    ITERATE (vector<CGeneModel*>, it, extened_parts_and_gapped) {
        const CGeneModel& align = **it;
        Extend(align, false);
    }

    CalculatedSupportAndWeightFromMembers();

    m_polya_cap_left_hard_limit = Limits().GetTo()+1;
    m_polya_cap_right_hard_limit = Limits().GetFrom()-1;
}


void CChain::CalculatedSupportAndWeightFromMembers() {

    vector<CSupportInfo> support;
    support.reserve(m_members.size());
    const CGeneModel* last_important_align = 0;
    double weight = 0;
    set<Int8> sp;
    set<Int8> sp_core;

    map<Int8,int> intronnum;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        for(int i = 1; i < (int)align.Exons().size(); ++i) {
            if(align.Exons()[i-1].m_ssplice && align.Exons()[i].m_fsplice)
                ++intronnum[align.ID()];
        }
    }

    TSignedSeqRange lim;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        Int8 id = align.ID();

        if(sp.insert(id).second) {   // avoid counting parts of splitted aligns
            weight += (*it)->Weight();
            support.push_back(CSupportInfo(id,false));
        }

        if(lim.Empty() || !Include(lim,align.Limits())) {

            bool add_some_introns = false;
            for(int i = (int)align.Exons().size()-1; !add_some_introns && i > 0; --i) {
                add_some_introns = align.Exons()[i-1].m_ssplice && align.Exons()[i].m_fsplice && 
                    (lim.Empty() || align.Exons()[i-1].GetTo() >= lim.GetTo());
           }

            bool pass_some_introns = false;
            for(int i = 1; last_important_align != 0 && !pass_some_introns && i < (int)(last_important_align->Exons().size()); ++i) {
                pass_some_introns = last_important_align->Exons()[i-1].m_ssplice && last_important_align->Exons()[i].m_fsplice && 
                    align.Limits().GetFrom() >= last_important_align->Exons()[i].GetFrom();
            }

            bool miss_some_introns = false;
            if(last_important_align != 0) {
                for(int i = (int)last_important_align->Exons().size()-1; !miss_some_introns && i > 0; --i) {
                    miss_some_introns = last_important_align->Exons()[i-1].m_ssplice && last_important_align->Exons()[i].m_fsplice &&
                        align.Limits().GetTo() < last_important_align->Exons()[i].GetFrom();
                }
            }

            if(add_some_introns) {
                if(pass_some_introns) {
                    sp_core.insert(last_important_align->ID());
                }
                last_important_align = *it;
            } else if(last_important_align != 0 && !pass_some_introns && !miss_some_introns && 
                      (sp_core.find(id) != sp_core.end() || intronnum[id] > intronnum[last_important_align->ID()])) {
                last_important_align = *it;
            }
        }

        lim += align.Limits();            
    }
    SetWeight(weight);
    if(last_important_align != 0) {
        sp_core.insert(last_important_align->ID());
    }

    ReplaceSupport(CSupportInfoSet());
    NON_CONST_ITERATE(vector<CSupportInfo>, s, support) {
        if(sp_core.find(s->GetId()) != sp_core.end()) {
            s->SetCore(true);
        }
        AddSupport(*s);
    }
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

void CChain::SetOpenForPartialyAlignedProteins(map<string, pair<bool,bool> >& prot_complet, TOrigAligns& orig_aligns) {
    if(ConfirmedStart() || !HasStart() || !HasStop() || OpenCds() || !Open5primeEnd() || (Type()&CGeneModel::eProt) == 0)
        return;

    bool found_length_match = false;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        CAlignModel* orig_align = orig_aligns[(*it)->ID()];
        if((orig_align->Type() & CGeneModel::eProt) == 0 || orig_align->TargetLen() == 0)   // not a protein or not known length
            continue;
    
        string accession = orig_align->TargetAccession();
        map<string, pair<bool,bool> >::iterator iter = prot_complet.find(accession);
        if(iter == prot_complet.end() || !iter->second.first || !iter->second.second) // unknown or partial protein
            continue;

        if(orig_align->TargetLen()*0.8 < RealCdsLen()) {
            found_length_match = true;
            break;
        }
    }

    if(!found_length_match) {
        CCDSInfo cds_info = GetCdsInfo();
        cds_info.SetScore(Score(), true);
        SetCdsInfo(cds_info);
    }

    return;
}

void CChain::RestoreReasonableConfirmedStart(const CGnomonEngine& gnomon, TOrigAligns& orig_aligns)
{
    if(HasStart())
        return;

    TSignedSeqRange conf_start;
    TSignedSeqPos rf=0; 
    
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        CAlignModel* orig_align = orig_aligns[(*it)->ID()];
        
        if(orig_align->ConfirmedStart() && Include(align.Limits(),orig_align->GetCdsInfo().Start())) {    // right part of orig is included
            if(Strand() == ePlus) {
                if(conf_start.Empty() || orig_align->GetCdsInfo().Start().GetFrom() < conf_start.GetFrom()) {
                    conf_start = orig_align->GetCdsInfo().Start();
                    rf = orig_align->ReadingFrame().GetFrom();
                }
            } else {
                if(conf_start.Empty() || orig_align->GetCdsInfo().Start().GetTo() > conf_start.GetTo()) {
                    conf_start = orig_align->GetCdsInfo().Start();
                    rf = orig_align->ReadingFrame().GetTo();
                }
            }
        }
    }

    if(ReadingFrame().NotEmpty() && conf_start.NotEmpty()) {
        TSignedSeqRange extra_cds = (Strand() == ePlus) ? TSignedSeqRange(RealCdsLimits().GetFrom(),conf_start.GetFrom()) : TSignedSeqRange(conf_start.GetTo(),RealCdsLimits().GetTo());
        if(FShiftedLen(extra_cds, false) < 0.2*RealCdsLen() && Continuous()) {
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
            SetCdsInfo(cds);          
            TInDels fs;
            ITERATE(TInDels, i, FrameShifts()) {
                TSignedSeqRange fullcds = cds.Start()+cds.ReadingFrame()+cds.Stop();
                if(Include(fullcds,i->Loc()))
                    fs.push_back(*i);
            }
            FrameShifts() = fs;
            gnomon.GetScore(*this);
            AddComment("Restored confirmed start");
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

void CChain::RecalculateAfterClip() {
    RecalculateLimits();

    vector<CGeneModel*>new_members;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        if(Limits().IntersectingWith(align.Limits())) {  // not clipped
            new_members.push_back(*it);
        }
    }
    m_members = new_members;
    CalculatedSupportAndWeightFromMembers();
}

void CChain::ClipToCompleteAlignment(EStatus determinant)
{
    string  name;
    EStrand right_end_strand = ePlus;
    bool complete = false;
    bool coding = ReadingFrame().NotEmpty();

    if (determinant == CGeneModel::ePolyA) {
        name  = "polya";
        right_end_strand = ePlus;
        complete = HasStop() || !coding;
    } else if (determinant == CGeneModel::eCap) {
        name = "cap";
        right_end_strand = eMinus;
        complete = HasStart() || !coding;
    } else {
        _ASSERT( false );
    }

    if(!complete)
        return;

    vector< pair<int,double> > position_weight;
    double weight = 0;
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        if((align.Status()&determinant) != 0) {
            if(Strand() == right_end_strand) {
                int rlimit = (coding ? RealCdsLimits().GetTo() : Exons().back().Limits().GetFrom()); // look in the last exon of notcoding or right UTR of coding
                if(rlimit < align.Limits().GetTo() && Include(Limits(),align.Limits().GetTo())) {
                    position_weight.push_back(make_pair(align.Limits().GetTo(),align.Weight()));
                    weight += align.Weight();
                }
            } else {
                int llimit = (coding ? RealCdsLimits().GetFrom() : Exons().front().Limits().GetTo()); // look in the first exon of notcoding or left UTR of coding
                if(llimit > align.Limits().GetFrom() && Include(Limits(),align.Limits().GetFrom())) {
                    position_weight.push_back(make_pair(align.Limits().GetFrom(),align.Weight()));
                    weight += align.Weight();
                }
            }
        }
    }

    if(position_weight.empty())
        return;

    if(Strand() == right_end_strand) {
        sort(position_weight.begin(),position_weight.end(),greater< pair<int,double> >());
    } else {
        sort(position_weight.begin(),position_weight.end(),less< pair<int,double> >());
    }

    double w = 0;
    int pos = -1;
    for(int i = 0; i < (int)position_weight.size() && w < 0.1*weight; ++i) {   // 10% cutoff
        pos = position_weight[i].first;
        w += position_weight[i].second;
    }

    w = 0;
    for(int i = (int)position_weight.size()-1; i >= 0 && w < 3; --i) {
        if(Strand() == right_end_strand) 
           m_polya_cap_right_hard_limit = position_weight[i].first;
        else
           m_polya_cap_left_hard_limit = position_weight[i].first;
        w += position_weight[i].second;
    }

    /*    keep mRNA without polya?????
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
    */


    TSignedSeqRange clip_range;
    Status() |= determinant;
    if (Strand() == right_end_strand && pos < Limits().GetTo()) 
        clip_range.Set(pos+1, Limits().GetTo());
    else if (Strand() != right_end_strand && pos > Limits().GetFrom())
        clip_range.Set(Limits().GetFrom(), pos-1);

    if (clip_range.NotEmpty()) {
        AddComment(name+"clip");
        CutExons(clip_range);
        RecalculateAfterClip();
    } 

    /*
    if((Status()&determinant) == 0) {
        AddComment("lost"+name);
    }
    */
}

#define SCAN_WINDOW 50
#define MIN_UTR 20
#define MIN_UTR_EXON 15

void CChain::ClipLowCoverageUTR(double utr_clip_threshold)
{
    if(ReadingFrame().Empty() || (Type()&CGeneModel::eSR) == 0)   // not coding or don't have SR coverage
        return;

    CAlignMap amap = GetAlignMap();

    int mrna_len = amap.FShiftedLen(Limits());
    
    TSignedSeqRange cds_lim;
    if(OpenCds())
        cds_lim = MaxCdsLimits();
    else
        cds_lim = RealCdsLimits();

    TSignedSeqRange genome_hard_limit;
    genome_hard_limit.SetFrom(min(m_polya_cap_left_hard_limit,cds_lim.GetFrom()));
    genome_hard_limit.SetTo(max(m_polya_cap_right_hard_limit,cds_lim.GetTo()));

    cds_lim = amap.MapRangeOrigToEdited(cds_lim);
    TSignedSeqRange hard_limit = amap.MapRangeOrigToEdited(genome_hard_limit);

    TSignedSeqRange no_clip_lim;
    no_clip_lim.SetFrom(max(0,min(hard_limit.GetFrom(),cds_lim.GetFrom()-MIN_UTR)));
    no_clip_lim.SetTo(min(mrna_len-1,max(hard_limit.GetTo(),cds_lim.GetTo()+MIN_UTR)));

    if(no_clip_lim.GetFrom() <= 0 &&  no_clip_lim.GetTo() >= mrna_len-1)   //nothing to clip
        return;

    vector<double> coverage(mrna_len);
    ITERATE (vector<CGeneModel*>, it, m_members) {
        const CGeneModel& align = **it;
        TSignedSeqRange overlap = Limits()&align.Limits();
        if(overlap.Empty())   // some could be cut by polya clip
            continue;

        TSignedSeqRange overlap_on_mrna = amap.MapRangeOrigToEdited(overlap);

        if(align.Type() != CGeneModel::eSR) {
            no_clip_lim.SetFrom(min(no_clip_lim.GetFrom(),overlap_on_mrna.GetFrom()));
            no_clip_lim.SetTo(max(no_clip_lim.GetTo(),overlap_on_mrna.GetTo()));
        }

        for(int i = overlap_on_mrna.GetFrom(); i <= overlap_on_mrna.GetTo(); ++i)
            coverage[i] += align.Weight();
    }

    if(no_clip_lim.GetTo()-no_clip_lim.GetFrom() < SCAN_WINDOW)      // too short
        return;

    double cds_coverage = 0;
    for (int i = cds_lim.GetFrom(); i <= cds_lim.GetTo(); ++i)
        cds_coverage += coverage[i];
    cds_coverage /= cds_lim.GetLength();

    // 5' UTR
    if(no_clip_lim.GetFrom() > 0) {
        int left_limit = no_clip_lim.GetFrom();
        int right_limit = cds_lim.GetFrom()-1+SCAN_WINDOW-MIN_UTR;

        double wlen = 0;
        for(int i = left_limit; i <= right_limit; ++i)
            wlen += coverage[i];
        double window_wlen = 0;
        for(int i = left_limit; i <= left_limit+SCAN_WINDOW-1; ++i)
            window_wlen += coverage[i];

        int len = right_limit-left_limit+1;

        while(left_limit > 0 && window_wlen/SCAN_WINDOW > max(cds_coverage,wlen/len)*utr_clip_threshold) {

            //            cerr << amap.MapEditedToOrig(left_limit) << ' ' << coverage[left_limit] << ' ' << window_wlen/SCAN_WINDOW << ' ' << max(cds_coverage,wlen/len) << endl;

            ++len;
            --left_limit;
            wlen += coverage[left_limit];
            window_wlen += coverage[left_limit]-coverage[left_limit+SCAN_WINDOW];
        }

        if(left_limit > 0) {
            AddComment("5putrclip");
            CutExons(amap.MapRangeEditedToOrig(TSignedSeqRange(0,left_limit-1)));
            if(Strand() == ePlus && Exons().front().Limits().GetLength() < MIN_UTR_EXON && Exons().front().Limits().GetTo() < genome_hard_limit.GetFrom())
                CutExons(Exons().front().Limits());
            else if(Strand() == eMinus && Exons().back().Limits().GetLength() < MIN_UTR_EXON && Exons().back().Limits().GetFrom() > genome_hard_limit.GetTo())
                CutExons(Exons().back().Limits());
            RecalculateAfterClip();
            if((Status()&CGeneModel::eCap) != 0)           // cap was further clipped
                ClipToCompleteAlignment(CGeneModel::eCap);
        }
    }
    

    // 3' UTR
    if(no_clip_lim.GetTo() < mrna_len-1) {

        int left_limit = cds_lim.GetTo()+1-SCAN_WINDOW+MIN_UTR;
        int right_limit = no_clip_lim.GetTo();

        double wlen = 0;
        for(int i = left_limit; i <= right_limit; ++i)
            wlen += coverage[i];
        double window_wlen = 0;
        for(int i = right_limit-SCAN_WINDOW+1; i <= right_limit; ++i)
            window_wlen += coverage[i];

        int len = right_limit-left_limit+1;
            
        while(right_limit < mrna_len-1 && window_wlen/SCAN_WINDOW > wlen/len*utr_clip_threshold) {
            ++len;
            ++right_limit;
            wlen += coverage[right_limit];
            window_wlen += coverage[right_limit]-coverage[right_limit-SCAN_WINDOW];
        }

        if(right_limit < mrna_len-1) {
            AddComment("3putrclip");
            CutExons(amap.MapRangeEditedToOrig(TSignedSeqRange(right_limit+1,mrna_len-1)));
            if(Strand() == ePlus && Exons().back().Limits().GetLength() < MIN_UTR_EXON && Exons().back().Limits().GetFrom() > genome_hard_limit.GetTo())
                CutExons(Exons().back().Limits());
            else if(Strand() == eMinus && Exons().front().Limits().GetLength() < MIN_UTR_EXON && Exons().front().Limits().GetTo() < genome_hard_limit.GetFrom())
                CutExons(Exons().front().Limits());
            RecalculateAfterClip();
            if((Status()&CGeneModel::ePolyA) != 0)           // polya was further clipped
                ClipToCompleteAlignment(CGeneModel::ePolyA);
        }
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

    if(ConfirmedStart() && ConfirmedStop()) {
        typedef map<Int8, int> Tint8int;
        Tint8int palignedlen;
        ITERATE(vector<CGeneModel*>, i, m_members) {
            if(IntersectingWith(**i)) {                                                                   // just in case we clipped this alignment
                if(!(*i)->TrustedProt().empty())
                    palignedlen[(*i)->ID()] += (*i)->AlignLen();
                else if(!(*i)->TrustedmRNA().empty() && (*i)->ConfirmedStart() && (*i)->ConfirmedStop())  // trusted mRNA with aligned CDS
                    InsertTrustedmRNA(*(*i)->TrustedmRNA().begin());                                      // could be only one 'part'
            }
        }
        ITERATE(Tint8int, i, palignedlen) {
            CAlignModel* orig_align = orig_aligns[i->first];
            if(i->second > minscor.m_minprotfrac*orig_align->TargetLen())                                 // well aligned trusted protein
                InsertTrustedProt(*orig_align->TrustedProt().begin());
        }
    }
}

pair<string,int> GetAccVer(const CAlignModel& a, CScope& scope)
{
    if((a.Type()&CGeneModel::eProt) == 0)
        return make_pair(a.TargetAccession(), 0);

    try {
        CSeq_id_Handle idh = sequence::GetId(*a.GetTargetId(), scope); 
        const CTextseq_id* txtid = idh.GetSeqId()->GetTextseq_Id(); 
        return (txtid  &&  txtid->IsSetAccession() && txtid->IsSetVersion()) ? 
            make_pair(txtid->GetAccession(), txtid->GetVersion()) : make_pair(idh.AsString(), 0);
    } catch (sequence::CSeqIdFromHandleException& e) {
        return make_pair(a.TargetAccession(), 0);
    }
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

        int a_stt = a->HasStart()+a->HasStop();
        int b_stt = b->HasStart()+b->HasStop();
        if (a_stt != b_stt)
            return a_stt > b_stt;
        
        int a_len = s_ExonLen(*a);
        int b_len = s_ExonLen(*b);
        if (a_len!=b_len)
            return a_len > b_len;

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
    //    CScope scope(*CObjectManager::GetInstance());
    //    scope.AddDefaults();

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
                    /*
                    if((algn.Type() & CGeneModel::eProt)!=0 && algn.Score() != BadScore()) {
                        CCDSInfo cds = algn.GetCdsInfo();
                        TSignedSeqRange alignedlim = orig->GetAlignMap().MapRangeOrigToEdited(orig->Limits(),false);
                        if(cds.HasStart() && !LeftPartialProtein(prot_complet, *orig, scope) && alignedlim.GetFrom() == 0)
                            cds.SetStart(cds.Start(),true);
                        if(cds.HasStop() && !RightPartialProtein(prot_complet, *orig, scope) && alignedlim.GetTo() == orig->TargetLen()-1)
                            cds.SetStop(cds.Stop(),true);
                        if(cds.ConfirmedStart() || cds.ConfirmedStop())
                            algn.SetCdsInfo(cds);
                    }
                    */
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
            const CSeq_loc& cds_loc = feat_ci->GetMappedFeature().GetLocation();
            const CSeq_id* cds_loc_seq_id  = cds_loc.GetId();
            if (cds_loc_seq_id != NULL && sequence::IsSameBioseq(*cds_loc_seq_id, *target_id, scope)) {
                TSeqRange feat_range = cds_loc.GetTotalRange();
                cds_on_mrna = TSignedSeqRange(feat_range.GetFrom(), feat_range.GetTo());
            }
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

    if(!a.Continuous())
        return;
    
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
            
            if((a != piece_begin->GetFrom() || b != piece_end->GetTo()) && b > a) {
                TSignedSeqRange newlimits = alignmap.ShrinkToRealPoints(TSignedSeqRange(a,b),true);
                //                _ASSERT(newlimits.NotEmpty() && piece_begin->GetTo() >= newlimits.GetFrom() && piece_end->GetFrom() <= newlimits.GetTo());
                if(newlimits.NotEmpty() && piece_begin->GetTo() >= newlimits.GetFrom() && piece_end->GetFrom() <= newlimits.GetTo())
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

#define MAX_ETEND 20   // useful for separating overlapping genes
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

void CChainer::SetConfirmedStartStopForProteinAlignments(TAlignModelList& alignments)
{
    m_data->SetConfirmedStartStopForProteinAlignments(alignments);
}

void CChainer::CChainerImpl::SetConfirmedStartStopForProteinAlignments(TAlignModelList& alignments)
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    NON_CONST_ITERATE (TAlignModelCluster, i, alignments) {
        CAlignModel& algn = *i;
        if ((algn.Type() & CGeneModel::eProt)!=0) {
            CCDSInfo cds = algn.GetCdsInfo();
            TSignedSeqRange alignedlim = algn.GetAlignMap().MapRangeOrigToEdited(algn.Limits(),false);
            if(cds.HasStart() && !LeftPartialProtein(prot_complet, algn, scope) && alignedlim.GetFrom() == 0)
                cds.SetStart(cds.Start(),true);
            if(cds.HasStop() && !RightPartialProtein(prot_complet, algn, scope) && alignedlim.GetTo() == algn.TargetLen()-1)
                cds.SetStop(cds.Stop(),true);
            if(cds.ConfirmedStart() || cds.ConfirmedStop())
                algn.SetCdsInfo(cds);
        }
    }
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

        if(chain.Score() != BadScore()) {
            m_gnomon->GetScore(chain);                           // cds properties could change because of clipping
            chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns);
            chain.SetOpenForPartialyAlignedProteins(prot_complet, orig_aligns);
        }
        

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
                            "Minimal coding propensity score for valid CDS. This threshold could be ignored depending on "
                            "-longenoughcds or -protcdslen and -minprotfrac",
                            CArgDescriptions::eDouble, "25.0");
    arg_desc->AddDefaultKey("longenoughcds", "longenoughcds",
                            "Minimal CDS not supported by protein or annotated mRNA to ignore the score (bp)",
                            CArgDescriptions::eInteger, "600");
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
    arg_desc->AddDefaultKey("utrclipthreshold", "utrclipthreshold",
                            "Relative coverage for clipping low support UTRs",
                            CArgDescriptions::eDouble, "0.01");

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
    minscor.m_cds_len = args["longenoughcds"].AsInteger();
    minscor.m_utr_clip_threshold = args["utrclipthreshold"].AsDouble();

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

MarkupTrustedGenes::MarkupTrustedGenes(set<string>& _trusted_genes) : trusted_genes(_trusted_genes) {}

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
        
        while (i >= 0 && !e->m_ssplice && EffectiveExonLength(*e, alignmap, snap_to_codons) < minex) {

            if(i == 0) { //first exon
                a.CutExons(*e);
                --i;
                break;
            }
            
            //this point is not an indel and is a codon boundary for proteins
            TSignedSeqPos remainingpoint = alignmap.ShrinkToRealPoints(TSignedSeqRange(a.Exons().front().GetFrom(),a.Exons()[i-1].GetTo()),snap_to_codons).GetTo();
            TSignedSeqPos left = e->GetFrom();
            if(remainingpoint < a.Exons()[i-1].GetTo())
                left = remainingpoint+1;
            a.CutExons(TSignedSeqRange(left,e->GetTo()));
            --i;
            e = &a.Exons()[i];
        }
        
        while (!e->m_fsplice && EffectiveExonLength(*e, alignmap, snap_to_codons) < minex) { 

            if(i == a.Exons().size()-1) { //last exon
                a.CutExons(*e);
                break;
            }
            
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


