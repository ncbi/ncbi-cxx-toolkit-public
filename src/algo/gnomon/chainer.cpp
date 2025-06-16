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
#include <algo/gnomon/glb_align.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/gnomon.hpp>
#include <algo/gnomon/annot.hpp>

#include <map>
#include <sstream>
#include <tuple>
#include <unordered_set>
#include <unordered_map>

#include <objects/general/Object_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include "gnomon_seq.hpp"

BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(gnomon)

bool BelongToExon(const CGeneModel::TExons& exons, int pos) {
    ITERATE(CGeneModel::TExons, i, exons) {
        if(Include(i->Limits(),pos))
            return true;
    }
    return false;
}

class CChain;
typedef list<CChain> TChainList;
typedef list<CChain*> TChainPointerList;


struct SChainMember;
typedef vector<SChainMember*> TContained;

typedef map<Int8,CAlignModel*> TOrigAligns;
typedef map<Int8,CGeneModel> TUnmodAligns;
struct SFShiftsCluster;
class CChainMembers;

class CGene;

class CChainer::CChainerImpl {

private:
    CChainerImpl(CRef<CHMMParameters>& hmm_params, unique_ptr<CGnomonEngine>& gnomon, const CAlignMap& edited_contig_map, const TSignedSeqRange& limits, const string& m_contig_acc);
    void SetGenomicRange(const TAlignModelList& alignments);
    void SetConfirmedStartStopForProteinAlignments(TAlignModelList& alignments);

    void FilterOutChimeras(TGeneModelList& clust);

    TGeneModelList MakeChains(TGeneModelList& models);
    void FilterOutBadScoreChainsHavingBetterCompatibles(TGeneModelList& chains);
    void CombineCompatibleChains(TChainList& chains);
    void SetFlagsForChains(TChainList& chains);
    SChainMember* FindOptimalChainForProtein(TContained& pointers_all, vector<CGeneModel*>& parts, CGeneModel& palign);
    void CreateChainsForPartialProteins(TChainList& chains, TContained& pointers, TGeneModelList& unma_aligns, CChainMembers& unma_members);
    void CutParts(TGeneModelList& clust);
    bool CanIncludeJinI(const SChainMember& mi, const SChainMember& mj);
    void IncludeInContained(SChainMember& big, SChainMember& small);
    void FindContainedAlignments(TContained& pointers);
    void DuplicateNotOriented(CChainMembers& pointers, TGeneModelList& clust);
    void Duplicate5pendsAndShortCDSes(CChainMembers& pointers);
    void ReplicatePStops(CChainMembers& pointers);
    void ScoreCdnas(CChainMembers& pointers);
    void DuplicateUTRs(CChainMembers& pointers);
    void CalculateSpliceWeights(CChainMembers& pointers);
    bool LRCanChainItoJ(int& delta_cds, double& delta_num, double& delta_splice_num, SChainMember& mi, SChainMember& mj, TContained& contained, bool& not_sorted);
    void LRIinit(SChainMember& mi, const TContained& micontained);
    void LeftRight(TContained& pointers);
    void RightLeft(TContained& pointers);
    double GoodCDNAScore(const CGeneModel& algn, bool simple = false);
    void RemovePoorCds(CGeneModel& algn, double minscor);
    void SkipReason(CGeneModel* orig_align, const string& comment);
    bool AddIfCompatible(set<SFShiftsCluster>& fshift_clusters, const CGeneModel& algn);
    bool FsTouch(const TSignedSeqRange& lim, const CInDelInfo& fs);
    void SplitAlignmentsByStrand(const TGeneModelList& clust, TGeneModelList& clust_plus, TGeneModelList& clust_minus);

    void FindGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet); 
    void ReplacePseudoGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet);
    void FindAltsForGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet);
    void PlaceAllYouCan(list<CGene>& alts, TChainPointerList& not_placed_yet, TChainPointerList& rejected);
    enum ECompat { eNotCompatible, eAlternative, eNested, eExternal, eOtherGene, eNotCompatibleNested};
    ECompat CheckCompatibility(const CGene& gene, const CChain& algn);
    list<CGene> FindGenes(TChainList& cls);
    void FilterOutSimilarsWithLowerScore(TChainPointerList& not_placed_yet, TChainPointerList& rejected);
    void FilterOutTandemOverlap(TChainPointerList& not_placed_yet, TChainPointerList& rejected, double fraction);
    void TrimAlignmentsIncludedInDifferentGenes(list<CGene>& genes);


    CRef<CHMMParameters>& m_hmm_params;
    unique_ptr<CGnomonEngine>& m_gnomon;
    const CAlignMap& m_edited_contig_map;
    const TSignedSeqRange& m_limits;
    const string& m_contig_acc;


    SMinScor minscor;
    int intersect_limit;
    int trim;
    map<string,TSignedSeqRange> mrnaCDS;
    map<string, pair<bool,bool> > prot_complet;
    double mininframefrac;
    bool no5pextension;

    int min_cap_weight;
    int min_cap_blob;
    int min_polya_weight;
    int min_polya_blob;
    int max_dist;
    double secondary_peak;
    double tertiary_peak;
    double tertiary_peak_coverage;
    int min_flank_exon;

    int minpolya;
    bool use_confirmed_ends;
    TIntMap confirmed_ends; // [splice], end    

    TOrigAligns orig_aligns;
    TUnmodAligns unmodified_aligns;
    map<int, TSignedSeqRange> trusted_group_cds;

    map<TSignedSeqRange,int> mrna_count;
    map<TSignedSeqRange,int> est_count;
    map<TSignedSeqRange,int> rnaseq_count;
    bool has_rnaseq;
    set<TSignedSeqRange> oriented_introns_plus;
    set<TSignedSeqRange> oriented_introns_minus;

    double altfrac;
    double longreadsthreshold;
    int composite;
    bool allow_opposite_strand;
    bool allow_partialalts;
    int tolerance;

    int m_idnext;
    int m_idinc;

    TInDels all_frameshifts;

    int flex_len;

    friend class CChainer;
    friend class CChainerArgUtil;
};

CGnomonAnnotator_Base::CGnomonAnnotator_Base() : m_masking(false) { }

CGnomonAnnotator_Base::~CGnomonAnnotator_Base(){ }

void CGnomonAnnotator_Base::EnableSeqMasking()
{
    m_masking = true;
}

CChainer::CChainer()
{
    m_data.reset( new CChainerImpl(m_hmm_params, m_gnomon, m_edited_contig_map, m_limits, m_contig_acc) );
}

CChainer::~CChainer()
{
}

CChainer::CChainerImpl::CChainerImpl(CRef<CHMMParameters>& hmm_params, unique_ptr<CGnomonEngine>& gnomon,  const CAlignMap& edited_contig_map, const TSignedSeqRange& limits, const string& contig_acc)
    :m_hmm_params(hmm_params), m_gnomon(gnomon), m_edited_contig_map(edited_contig_map), m_limits(limits), m_contig_acc(contig_acc), m_idnext(1), m_idinc(1)
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

typedef set<SChainMember*> TMemberPtrSet;

struct SChainMember
{
    SChainMember() :
        m_align(0), m_cds_info(0), m_align_map(0), m_left_member(0), m_right_member(0), m_sink_for_contained(0),
        m_copy(0), m_contained(0), m_identical_count(0),
        m_left_num(0), m_right_num(0), m_num(0),
        m_splice_weight(0), m_left_splice_num(0), m_right_splice_num(0), m_splice_num(0),
        m_type(eCDS), m_left_cds(0), m_right_cds(0), m_cds(0), m_included(false),  m_postponed(false), m_internal(false),
        m_marked_for_deletion(false), m_marked_for_retention(false), m_restricted_to_start(false),
        m_gapped_connection(false), m_fully_connected_to_part(-1), m_not_for_chaining(false),
        m_rlimb(numeric_limits<int>::max()),  m_llimb(numeric_limits<int>::max()), m_orig_align(0), m_unmd_align(0),
	m_mem_id(0), m_left_trusted_group(0),  m_right_trusted_group(0), m_cds_from_trusted(false), m_excluded_readthrough(false) {}

    TContained CollectContainedForChain();
    TContained CollectCodingContainedForChain();
    void MarkIncluded();
    void MarkIncludedForChain();
    void MarkPostponed();
    void MarkPostponedForChain();
    void MarkUnwantedCopiesForChain(const TSignedSeqRange& cds);
    TContained CollectContainedForMemeber();
    TContained CollectCodingContainedForMemeber();
    void AddToContained(TContained& contained, TMemberPtrSet& included_in_list);
    void AddCodingToContained(TContained& contained, TMemberPtrSet& included_in_list);

    CGeneModel* m_align;
    const CCDSInfo* m_cds_info;
    CAlignMap* m_align_map;
    SChainMember* m_left_member;
    SChainMember* m_right_member;
    SChainMember* m_sink_for_contained;
    TContained* m_copy;      // is used to make sure that the copy of already incuded duplicated alignment is not included in contained and doesn't trigger a new chain genereation
    TContained* m_contained;
    int m_identical_count;
    double m_left_num, m_right_num, m_num; 
    double m_splice_weight;
    double m_left_splice_num, m_right_splice_num, m_splice_num;
    double m_accumulated_splice_num, m_accumulated_num;
    int m_type, m_left_cds, m_right_cds, m_cds;
    bool m_included;
    bool m_postponed;
    bool m_internal;
    bool m_marked_for_deletion;
    bool m_marked_for_retention;
    bool m_restricted_to_start;
    bool m_gapped_connection;          // used for gapped proteins
    int m_fully_connected_to_part;     // used for gapped proteins
    bool m_not_for_chaining;           // included in other alignmnet(s) or supressed and can't trigger a different chain
    int m_rlimb;                       // leftmost compatible rexon
    int m_llimb;                       // leftmost not compatible lexon
    CAlignModel* m_orig_align;
    CGeneModel* m_unmd_align;
    int m_mem_id;
    int m_left_trusted_group;
    int m_right_trusted_group;
    bool m_cds_from_trusted;
    bool m_excluded_readthrough;
};

class CChain : public CGeneModel
{
private:
    typedef map<int, double> TIDMap;
    tuple<TIDMap, TSignedSeqRange> PeaksAndLimits(EStatus determinant, int min_blob_weight, int max_empty_dist, int min_splice_dist);
    tuple<TIVec, TSignedSeqRange> MainPeaks(TIDMap& peak_weights, double secondary_peak, double tertiary_peak, double tertiary_peak_coverage, bool right_end);
public:
    CChain(SChainMember& mbr, CGeneModel* gapped_helper = 0, bool keep_all_evidence = false, bool addallsupport = true);
    CChain(SChainMember& mbr, bool full_support);
    void AddAllMembersAndCoverage(SChainMember& mbr);

    void RestoreTrimmedEnds(int trim);
    void RemoveFshiftsFromUTRs();
    bool RestoreReasonableConfirmedStart(const CGnomonEngine& gnomon, TOrigAligns& orig_aligns);
    void SetOpenForPartialyAlignedProteins(map<string, pair<bool,bool> >& prot_complet);
    pair<bool,bool> ValidPolyA(int pos, const CResidueVec& contig);
    void ClipToCap(int min_cap_blob, int max_dist, int min_flank_exon, double secondary_peak, bool recalulate_support = true );
    void ClipToPolyA(const CResidueVec& contig, int min_polya_blob, int max_dist, int min_flank_exon, double secondary_peak, double tertiary_peak, double tertiary_peak_coverage, bool recalulate_support = true);
    void CheckSecondaryCapPolyAEnds();
    void ClipLowCoverageUTR(double utr_clip_threshold, bool recalulate_support = true);
    void CalculateDropLimits();
    void CalculateSupportAndWeightFromMembers(bool keep_all_evidence = false);
    void ClipChain(TSignedSeqRange limits, bool recalulate_support = true);
    bool SetConfirmedEnds(const CGnomonEngine& gnomon, CGnomonAnnotator_Base::TIntMap& confirmed_ends);

    void SetConfirmedStartStopForCompleteProteins(map<string, pair<bool,bool> >& prot_complet, const SMinScor& minscor);
    void CollectTrustedmRNAsProts(TOrigAligns& orig_aligns, const SMinScor& minscor, CScope& scope, SMatrix& matrix, const CResidueVec& contig);
    void SetBestPlacement(TOrigAligns& orig_aligns);
    void SetConsistentCoverage();

    bool HarborsNested(const CChain& other_chain, bool check_in_holes) const;
    bool HarborsNested(const CGene& other_gene, bool check_in_holes) const;

    bool HasTrustedEvidence() const;

    TContained m_members;
    int m_polya_cap_right_soft_limit;
    int m_polya_cap_left_soft_limit;
    int m_coverage_drop_left;
    int m_coverage_drop_right;
    int m_coverage_bump_left;
    int m_coverage_bump_right;
    double m_core_coverage;
    vector<double> m_coverage;
    double m_splice_weight;
    CGeneModel m_gapped_helper_align;
    TSignedSeqRange m_supported_range;
    TIVec m_cap_peaks;
    TIVec m_polya_peaks;
    bool m_was_combined;
};


class CGene : public TChainPointerList
{
public:
    CGene() : m_maxscore(BadScore()) {}
    typedef list<CGeneModel>::iterator TIt;
    typedef list<CGeneModel>::const_iterator TConstIt;
    TSignedSeqRange Limits() const { return m_limits; }
    TSignedSeqRange RealCdsLimits() const { return m_real_cds_limits; }
    bool IsAlternative(const CChain& a) const;
    bool IsAllowedAlternative(const ncbi::gnomon::CGeneModel&, int maxcomposite) const;
    void Insert(CChain& a);
    double MaxScore() const { return m_maxscore; }
    bool Nested() const { return !m_nested_in_genes.empty(); }
    bool LargeCdsOverlap(const CGeneModel& a) const;
    bool HarborsNested(const CChain& other_chain, bool check_in_holes) const;
    bool HarborsNested(const CGene& other_gene, bool check_in_holes) const;

    void AddToHarbored(CGene* p) { m_harbors_genes.insert(p); }
    void AddToNestedIn(CGene* p) {m_nested_in_genes.insert(p); };
    set<CGene*> RemoveGeneFromOtherGenesSets();
    

private:
    bool HarborsRange(TSignedSeqRange range, bool check_in_holes) const;
    void RemoveFromHarbored(CGene* p) { m_harbors_genes.erase(p); }
    void RemoveFromNestedIn(CGene* p) {m_nested_in_genes.erase(p); };

    TSignedSeqRange m_limits, m_real_cds_limits;
    double m_maxscore;
    set<CGene*> m_nested_in_genes;
    set<CGene*> m_harbors_genes;
};

set<CGene*> CGene::RemoveGeneFromOtherGenesSets() {
    NON_CONST_ITERATE(set<CGene*>, i, m_nested_in_genes)
        (*i)->RemoveFromHarbored(this);
    NON_CONST_ITERATE(set<CGene*>, i,m_harbors_genes)
        (*i)->RemoveFromNestedIn(this);

    return m_harbors_genes;
}

// if external model is 'open' all 5' introns can harbor
// gene with 'double' CDS can harbor in the interval between CDSes (intron or not)
// non coding models in external coding genes have no effect
bool CGene::HarborsRange(TSignedSeqRange range, bool check_in_holes) const {
    TSignedSeqRange gene_lim_for_nested = Limits();
    if(RealCdsLimits().NotEmpty())
        gene_lim_for_nested = front()->OpenCds() ? front()->MaxCdsLimits() : RealCdsLimits();  // 'open' could be only a single variant gene
    if(!Include(gene_lim_for_nested,range))
        return false;

    bool nested = true;
    ITERATE(CGene, it, *this) {
        if(RealCdsLimits().NotEmpty() && (*it)->ReadingFrame().Empty())    // non coding model in coding gene
            continue;
        TSignedSeqRange model_lim_for_nested = (*it)->Limits();
        if((*it)->ReadingFrame().NotEmpty())
            model_lim_for_nested = (*it)->OpenCds() ? (*it)->MaxCdsLimits() : (*it)->RealCdsLimits();   // 'open' could be only a single variant gene
        if(range.IntersectingWith(model_lim_for_nested) && !CModelCompare::RangeNestedInIntron(range, **it, check_in_holes)) {
            nested = false;
            break;
        }
    }
    
    return nested;
}

// if external model is 'open' all 5' introns can harbor
// gene with 'double' CDS can harbor in the interval between CDSes (intron or not)
// for nested model 'open' is ignored
// non coding models in external coding genes have no effect
bool CGene::HarborsNested(const CChain& other_chain, bool check_in_holes) const {
    TSignedSeqRange other_lim_for_nested = other_chain.Limits();
    if(!other_chain.ReadingFrame().Empty())
        other_lim_for_nested = other_chain.RealCdsLimits();

    return HarborsRange(other_lim_for_nested, check_in_holes);
}

// if external model is 'open' all 5' introns can harbor
// gene with 'double' CDS can harbor in the interval between CDSes (intron or not)
// for nested model 'open' is ignored
// non coding models in external coding genes have no effect
bool CGene::HarborsNested(const CGene& other_gene, bool check_in_holes) const {
    TSignedSeqRange other_lim_for_nested = other_gene.Limits();
    if(!other_gene.RealCdsLimits().Empty())
        other_lim_for_nested = other_gene.RealCdsLimits();

    return HarborsRange(other_lim_for_nested, check_in_holes);
}


bool CGene::LargeCdsOverlap(const CGeneModel& a) const {

    ITERATE(CGene, it, *this) {
        const CGeneModel& b = **it;
        int common_cds = 0;
        ITERATE(CGeneModel::TExons, ib, b.Exons()) {
            ITERATE(CGeneModel::TExons, ia, a.Exons()) {
                common_cds += (ib->Limits()&b.RealCdsLimits()&ia->Limits()&a.RealCdsLimits()).GetLength();
            }
        }
        if(common_cds > 50)
            return true;
    }

    return false;
}

void CGene::Insert(CChain& a)
{
    push_back(&a);
    m_limits += a.Limits();
    m_real_cds_limits += a.RealCdsLimits();
    m_maxscore = max(m_maxscore,a.Score());
}

bool CGene::IsAllowedAlternative(const CGeneModel& a, int maxcomposite) const
{
    if(a.Exons().size() > 1 && (a.Status()&CGeneModel::ecDNAIntrons) == 0 && a.TrustedmRNA().empty() && a.TrustedProt().empty()) {
        return false;
    }

    if (a.Support().empty()) {
        return false;
    }

    int composite = 0;
    ITERATE(CSupportInfoSet, s, a.Support()) {
        if(s->IsCore() && ++composite > maxcomposite) return false;
    }

    if(a.PStop(false) || !a.FrameShifts().empty())
        return false;
    if(front()->PStop(false) || !front()->FrameShifts().empty())
        return false;

    // check for gapfillers  

    vector<TSignedSeqRange> gene_gapfill_exons;
    ITERATE(CGeneModel::TExons, e, front()->Exons()) {
        if(e->m_fsplice_sig == "XX" || e->m_ssplice_sig == "XX")
            gene_gapfill_exons.push_back(e->Limits());
    }
    vector<TSignedSeqRange> a_gapfill_exons;
    ITERATE(CGeneModel::TExons, e, a.Exons()) {
        if(e->m_fsplice_sig == "XX" || e->m_ssplice_sig == "XX")
            a_gapfill_exons.push_back(e->Limits());
    }
    if(gene_gapfill_exons != a_gapfill_exons)
        return false;

    bool a_share_intron = false;
    ITERATE(CGene, it, *this) {
        const CGeneModel& b = **it;
        set<TSignedSeqRange> b_introns;
        for(int i = 1; i < (int)b.Exons().size(); ++i) {
            if(b.Exons()[i-1].m_ssplice && b.Exons()[i].m_fsplice) {
                TSignedSeqRange intron(b.Exons()[i-1].GetTo()+1,b.Exons()[i].GetFrom()-1);
                b_introns.insert(intron);
            }
        }

        bool a_has_new_intron = false;
        for(int i = 1; i < (int)a.Exons().size(); ++i) {
            if(a.Exons()[i-1].m_ssplice && a.Exons()[i].m_fsplice && a.Exons()[i-1].m_ssplice_sig != "XX" && a.Exons()[i].m_fsplice_sig != "XX") {
                TSignedSeqRange intron(a.Exons()[i-1].GetTo()+1,a.Exons()[i].GetFrom()-1);
                if(b_introns.insert(intron).second)
                    a_has_new_intron = true;
                else
                    a_share_intron = true; 
            }
        }
 
        if(a_has_new_intron) {
            continue;
        } else if(!gene_gapfill_exons.empty()) {
           return false; 
        } else if(a.RealCdsLimits().NotEmpty() && b.RealCdsLimits().NotEmpty() && !a.RealCdsLimits().IntersectingWith(b.RealCdsLimits()) && (!a.TrustedmRNA().empty() || !a.TrustedProt().empty())) {
#ifdef _DEBUG 
            const_cast<CGeneModel&>(a).AddComment("Secondary CDS");
#endif    
            continue;
        } else if(a.RealCdsLen() <= b.RealCdsLen()){
            return false;
        }
    }

    return (a_share_intron || gene_gapfill_exons.empty());
}

bool CGene::IsAlternative(const CChain& a) const
{
    _ASSERT( size()>0 );

    if (a.Strand() != front()->Strand())
        return false;

    bool gene_has_trusted = false;
    ITERATE(CGene, it, *this) {
        if((*it)->HasTrustedEvidence()) {
            gene_has_trusted = true;
            break;
        }
    }

    bool has_common_splice = false;

    ITERATE(CGene, it, *this) {
        if(CModelCompare::CountCommonSplices(**it, a) > 0) {      // has common splice
            has_common_splice = true;
            break;
        }
    }

    if(has_common_splice && (!gene_has_trusted || !a.HasTrustedEvidence())) // separate trusted genes with similar splices if they don't have common cds
        return true;

    if(a.ReadingFrame().NotEmpty() && RealCdsLimits().NotEmpty()) { 
        CAlignMap amap(a.Exons(), a.FrameShifts(), a.Strand(), a.GetCdsInfo().Cds());
        TIVec acds_map(amap.FShiftedLen(a.GetCdsInfo().Cds()),0);
        for(unsigned int j = 0; j < a.Exons().size(); ++j) {
            for(TSignedSeqPos k = max(a.Exons()[j].GetFrom(),a.GetCdsInfo().Cds().GetFrom()); k <= min(a.Exons()[j].GetTo(),a.GetCdsInfo().Cds().GetTo()); ++k) {
                TSignedSeqPos p =  amap.MapOrigToEdited(k);
                _ASSERT(p < (int)acds_map.size());
                if(p >= 0)
                    acds_map[p] = k;
            }
        }
    
        
        bool has_common_cds = false;

        ITERATE(CGene, it, *this) {
            if(!a.GetCdsInfo().Cds().IntersectingWith((*it)->GetCdsInfo().Cds()))
                continue;

            CAlignMap gmap((*it)->Exons(), (*it)->FrameShifts(), (*it)->Strand(), (*it)->GetCdsInfo().Cds());
            TIVec cds_map(gmap.FShiftedLen((*it)->GetCdsInfo().Cds()),0);
            for(unsigned int j = 0; j < (*it)->Exons().size(); ++j) {
                for(TSignedSeqPos k = max((*it)->Exons()[j].GetFrom(),(*it)->GetCdsInfo().Cds().GetFrom()); k <= min((*it)->Exons()[j].GetTo(),(*it)->GetCdsInfo().Cds().GetTo()); ++k) {
                    TSignedSeqPos p =  gmap.MapOrigToEdited(k);
                    _ASSERT(p < (int)cds_map.size());
                    if(p >= 0)
                        cds_map[p] = k;
                }
            }
        
            for(unsigned int i = 0; i < acds_map.size(); ) {
                unsigned int j = 0;
                for( ; j < cds_map.size() && (acds_map[i] != cds_map[j] || i%3 != j%3); ++j);
                if(j == cds_map.size()) {
                    ++i;
                    continue;
                }
            
                int count = 0;
                for( ; j < cds_map.size() && i < acds_map.size() && acds_map[i] == cds_map[j]; ++j, ++i, ++count);
            
                if(count > 30) {        // has common cds
                    has_common_cds = true;
                    break;
                }
            }

            if(has_common_cds)
                break;
        }

        return has_common_cds;
    }

    return has_common_splice;
}

static bool DescendingModelOrder(const CChain& a, const CChain& b)
{
    if (!a.Support().empty() && b.Support().empty())
        return true;
    else if (a.Support().empty() && !b.Support().empty())
        return false;


    bool atrusted = !a.TrustedmRNA().empty() || !a.TrustedProt().empty();
    bool btrusted = !b.TrustedmRNA().empty() || !b.TrustedProt().empty();
    if(atrusted && !btrusted) {                                     // trusted gene is always better
        return true;
    } else if(btrusted && !atrusted) {
        return false;
    } else if(a.ReadingFrame().NotEmpty() && b.ReadingFrame().Empty()) {       // coding is always better
        return true;
    } else if(b.ReadingFrame().NotEmpty() && a.ReadingFrame().Empty()) {
        return false;
    } else if(a.ReadingFrame().NotEmpty()) {     // both coding

        double ds = 0.05*fabs(a.Score());
        double as = a.Score();
        if((a.Status()&CGeneModel::ecDNAIntrons) != 0)
            as += 2*ds;
        if((a.Status()&CGeneModel::ePolyA) != 0)
            as += ds; 
        if((a.Status()&CGeneModel::eCap) != 0)
            as += ds; 
        if(a.isNMD())
            as -= ds;
        
        ds = 0.05*fabs(b.Score());
        double bs = b.Score();
        if((b.Status()&CGeneModel::ecDNAIntrons) != 0)
            bs += 2*ds;
        if((b.Status()&CGeneModel::ePolyA) != 0)
            bs += ds; 
        if((b.Status()&CGeneModel::eCap) != 0)
            bs += ds; 
        if(b.isNMD())
            bs -= ds;
        
        if(as > bs)    // better score
            return true;
        else if(bs > as)
            return false;
        else if(a.m_splice_weight > b.m_splice_weight) // more splice support
            return true;
        else if(a.m_splice_weight < b.m_splice_weight)
            return false;
        else if(a.Weight() > b.Weight())       // more alignments is better
            return true;
        else if(a.Weight() < b.Weight()) 
            return false;
        else if(a.Limits().GetLength() != b.Limits().GetLength())
            return (a.Limits().GetLength() < b.Limits().GetLength());   // everything else equal prefer compact model
        else
            return a.ID() < b.ID();
    } else {                       // both noncoding
        double asize = a.m_splice_weight;
        double bsize = b.m_splice_weight;
        double ds = 0.025*(asize+bsize);
        
        if((a.Status()&CGeneModel::ePolyA) != 0)
            asize += ds; 
        if((a.Status()&CGeneModel::eCap) != 0)
            asize += ds; 
        if(a.isNMD())
            asize -= ds;
        
        if((b.Status()&CGeneModel::ePolyA) != 0)
            bsize += ds; 
        if((b.Status()&CGeneModel::eCap) != 0)
            bsize += ds; 
        if(b.isNMD())
            bsize -= ds;
        
        if(asize > bsize)     
            return true;
        else if(bsize > asize)
            return false;
        else if(a.Limits().GetLength() != b.Limits().GetLength())
            return (a.Limits().GetLength() < b.Limits().GetLength());   // everything else equal prefer compact model
        else
            return a.ID() < b.ID();
    }
}

typedef CChain* TChainPtr;
static bool DescendingModelOrderP(const TChainPtr& a, const TChainPtr& b)
{
    return DescendingModelOrder(*a, *b);
}
static bool DescendingModelOrderPConsistentCoverage(const TChainPtr& a, const TChainPtr& b)
{
    if((a->Status()&CGeneModel::eConsistentCoverage) != (b->Status()&CGeneModel::eConsistentCoverage))
        return (a->Status()&CGeneModel::eConsistentCoverage) > (b->Status()&CGeneModel::eConsistentCoverage);
    else
        return DescendingModelOrder(*a, *b);
}

CChainer::CChainerImpl::ECompat CChainer::CChainerImpl::CheckCompatibility(const CGene& gene, const CChain& algn)
{
    bool gene_good_enough_to_be_annotation = allow_partialalts || gene.front()->GoodEnoughToBeAnnotation(); 
    bool algn_good_enough_to_be_annotation = allow_partialalts || algn.GoodEnoughToBeAnnotation();

    TSignedSeqRange gene_cds = (gene.size() > 1 || gene.front()->CompleteCds() || algn_good_enough_to_be_annotation) ? gene.RealCdsLimits() : gene.front()->MaxCdsLimits();
    TSignedSeqRange algn_cds = (algn.CompleteCds() || gene_good_enough_to_be_annotation) ? algn.RealCdsLimits() : algn.MaxCdsLimits();

    if(!gene_good_enough_to_be_annotation && !algn_good_enough_to_be_annotation) { // both need ab initio
        const CGeneModel& b = *gene.front();
        for(int i = 1; i < (int)b.Exons().size(); ++i) {
            if(b.Exons()[i].m_ssplice_sig == "XX" && b.Exons()[i].m_fsplice_sig == "XX" && b.Exons()[i].Limits().IntersectingWith(gene_cds)) { // if gap cds extend range to left exon
                gene_cds.SetFrom(min(gene_cds.GetFrom(), b.Exons()[i-1].GetTo()));
            }
        }

        for(int i = 1; i < (int)algn.Exons().size(); ++i) {
            if(algn.Exons()[i].m_ssplice_sig == "XX" && algn.Exons()[i].m_fsplice_sig == "XX" && algn.Exons()[i].Limits().IntersectingWith(algn_cds)) { // if gap cds extend range to left exon
                algn_cds.SetFrom(min(algn_cds.GetFrom(), algn.Exons()[i-1].GetTo()));
            }
        }
    }

    if(!gene.Limits().IntersectingWith(algn.Limits()))             // don't overlap
        return eOtherGene;
    
    if(gene.IsAlternative(algn)) {   // has common splice or common CDS

        if(gene.IsAllowedAlternative(algn, composite) && algn_good_enough_to_be_annotation) {
            if(!algn.TrustedmRNA().empty() || !algn.TrustedProt().empty()) {                   // trusted gene
                return eAlternative;
            } else if(algn.ReadingFrame().Empty() || gene.front()->ReadingFrame().Empty()) {   // one noncoding
                if(algn.m_splice_weight > altfrac/100*gene.front()->m_splice_weight)                     // long enough
                    return eAlternative;
                else
                    return eNotCompatible;
            } else if(algn.RealCdsLen() > altfrac/100*gene.front()->RealCdsLen() || algn.Score() > altfrac/100*gene.front()->Score()) {   // good score or long enough cds
                return eAlternative;
            }
        }

        return eNotCompatible;
    }

    // don't include overlapping gapfil 'introns' in different genes
    set<TSignedSeqRange> gene_gapfill_introns;
    set<TSignedSeqRange> align_gapfill_introns;
    ITERATE(CGene, it, gene) {
        const CGeneModel& b = **it;
        for(int i = 1; i < (int)b.Exons().size(); ++i) {
            if(b.Exons()[i-1].m_ssplice_sig == "XX" || b.Exons()[i].m_fsplice_sig == "XX") {
                TSignedSeqRange intron(b.Exons()[i-1].GetTo(),b.Exons()[i].GetFrom());
                gene_gapfill_introns.insert(intron);
            }
        }
    }
    for(int i = 1; i < (int)algn.Exons().size(); ++i) {
        if(algn.Exons()[i-1].m_ssplice_sig == "XX" || algn.Exons()[i].m_fsplice_sig == "XX") {
            TSignedSeqRange intron(algn.Exons()[i-1].GetTo(),algn.Exons()[i].GetFrom());
            align_gapfill_introns.insert(intron);
        }
    }
    ITERATE(set<TSignedSeqRange>, ig, gene_gapfill_introns) {
        ITERATE(set<TSignedSeqRange>, ia, align_gapfill_introns) {
            if(ig->IntersectingWith(*ia))
                return eNotCompatible;
        }
    }
    
    if(algn.HarborsNested(gene, gene_good_enough_to_be_annotation))    // gene is nested in align's intron (could be partial)
        return eExternal;

    if(gene.HarborsNested(algn, algn_good_enough_to_be_annotation))    // algn is nested in gene (could be partial)
        return eNested;

    if(!algn_cds.Empty() && !gene_cds.Empty()) {                          // both coding
        if (!gene_cds.IntersectingWith(algn_cds)) {          // don't overlap
#ifdef _DEBUG 
            if((gene_cds+algn_cds).GetLength() < gene_cds.GetLength()+algn_cds.GetLength()+20)
                const_cast<CChain&>(algn).AddComment("Close proximity");
#endif    
            return eOtherGene;
        } else if(gene.LargeCdsOverlap(algn)) {
            return eNotCompatible;
        }
    }

    if(gene_good_enough_to_be_annotation && algn_good_enough_to_be_annotation) {
        if(gene.front()->Strand() != algn.Strand() && allow_opposite_strand && 
           ((algn.Status()&CGeneModel::eBestPlacement) || (algn.Exons().size() > 1 && gene.front()->Exons().size() > 1)))
            return eOtherGene; 
        else if(algn.Status() & CGeneModel::eBestPlacement && (algn.Exons().size() == 1 || (algn.Status()&CGeneModel::ecDNAIntrons))) {
#ifdef _DEBUG 
            const_cast<CChain&>(algn).AddComment("Best placement overlap");
#endif    
            return eOtherGene;
        }
    }
        
    return eNotCompatible;
}

void CChainer::CChainerImpl::FindGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet) {

    not_placed_yet.sort(DescendingModelOrderP);

    for(TChainPointerList::iterator itloop = not_placed_yet.begin(); itloop != not_placed_yet.end(); ) {
        TChainPointerList::iterator it = itloop++;
        CChain& algn(**it);

        if(algn.Score() == BadScore())             // postpone noncoding models
            continue;
        else if(!(algn.Score() >= 2*minscor.m_min || algn.GetCdsInfo().ProtReadingFrame().NotEmpty() || (algn.Score() >= minscor.m_min && (algn.Status()&(CGeneModel::eCap|CGeneModel::ePolyA)))))
            continue;

        list<CGene*> possibly_nested;

        bool good_model = true;
        for(list<CGene>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);

            switch(cmp) {
            case eExternal:
                possibly_nested.push_back(&(*itl));  // already created gene is nested in this model
            case eOtherGene:
                break;
            default:
                good_model = false;
                break;
            }                
        }

        if(good_model) {
            alts.push_back(CGene());
#ifdef _DEBUG 
            algn.AddComment("Pass1");
#endif    
            alts.back().Insert(algn);
            not_placed_yet.erase(it);
        }

        ITERATE(list<CGene*>, itl, possibly_nested) {
            (*itl)->AddToNestedIn(&alts.back());
            alts.back().AddToHarbored(*itl);
        }
    }
}

void CChainer::CChainerImpl::ReplacePseudoGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet) {

    not_placed_yet.sort(DescendingModelOrderP);

    for(TChainPointerList::iterator itloop = not_placed_yet.begin(); itloop != not_placed_yet.end(); ) {
        TChainPointerList::iterator it = itloop++;
        CChain& algn(**it);

        list<list<CGene>::iterator> included_in;
        list<CGene*> possibly_nested;   // genes which 'could' become nested
        list<CGene*> nested_in;

        bool good_model = true;
        for(list<CGene>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);

            switch(cmp) {
            case eNested:
                nested_in.push_back(&(*itl));
                break;
            case eExternal:
                possibly_nested.push_back(&(*itl));  // already created gene is nested in this model
                break;
            case eOtherGene:
                break;
            case eAlternative:
                included_in.push_back(itl);
                break;
            case eNotCompatible:
            case eNotCompatibleNested:
                if(itl->IsAlternative(algn))
                    included_in.push_back(itl);
                else
                    good_model = false;                
                break;
            default:
                good_model = false;
                break;
            }
        }

        if(!good_model || included_in.size() != 1 || (!(algn.Status()&CGeneModel::ecDNAIntrons) && algn.TrustedmRNA().empty() && algn.TrustedProt().empty()))
            continue;
        
        CGene& gene = *included_in.front();
        CChain& model = *gene.front();
        //        if((!model.PStop(false) && model.FrameShifts().empty()) || algn.PStop(false) || !algn.FrameShifts().empty())
        if(!model.PStop(false) || algn.PStop(false) || !algn.FrameShifts().empty())  // use only for pstops
            continue;

        int algn_cds_len = algn.FShiftedLen(algn.GetCdsInfo().Cds(),false);
        int model_cds_len = model.FShiftedLen(model.GetCdsInfo().Cds(),false);
        if(algn_cds_len < 0.8*model_cds_len)
            continue;

#ifdef _DEBUG 
        algn.AddComment("Replacing pseudo "+NStr::NumericToString(model.ID()));
#endif 
        not_placed_yet.push_back(gene.front()); // position doesn't matter - will go to 'bad' models 
        gene.RemoveGeneFromOtherGenesSets();
        gene = CGene();
        gene.Insert(algn);
        ITERATE(list<CGene*>, itl, nested_in) {
            gene.AddToNestedIn(*itl); 
            (*itl)->AddToHarbored(&gene);
        }
        ITERATE(list<CGene*>, itl, possibly_nested) {
            (*itl)->AddToNestedIn(&gene);
            gene.AddToHarbored(*itl);
        }

        not_placed_yet.erase(it);
    }
}

void CChainer::CChainerImpl::FindAltsForGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet) {

    not_placed_yet.sort(DescendingModelOrderPConsistentCoverage);

    for(TChainPointerList::iterator itloop = not_placed_yet.begin(); itloop != not_placed_yet.end(); ) {
        TChainPointerList::iterator it = itloop++;
        CChain& algn(**it);

        list<list<CGene>::iterator> included_in;
        list<CGene*> possibly_nested;   // genes which 'could' become nested

        bool good_model = true;
        for(list<CGene>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);

            switch(cmp) {
            case eExternal:
                possibly_nested.push_back(&(*itl));  // already created gene is nested in this model
            case eOtherGene:
                break;
            case eAlternative:
                included_in.push_back(itl);
                break;
            default:
                good_model = false;
                break;
            }
        }

        if(good_model && !included_in.empty() && (allow_partialalts || included_in.front()->front()->GoodEnoughToBeAnnotation())) {
            if(included_in.size() == 1) {    // alternative to only one seed
#ifdef _DEBUG 
                algn.AddComment("Pass2a");
#endif 

                CGene& gene = *included_in.front();
                gene.Insert(algn);
                not_placed_yet.erase(it);

                ITERATE(list<CGene*>, itl, possibly_nested) {
                    if(gene.HarborsNested(**itl, true)) {
                        (*itl)->AddToNestedIn(&gene);
                        gene.AddToHarbored(*itl);
                    }
                }
            } else {  // connects seeds

                bool allow_connection = false;

                if(!algn.TrustedmRNA().empty() || !algn.TrustedProt().empty() || (algn.Status()&CGeneModel::eConsistentCoverage)) {   // connects seeds but trusted  
                    bool cds_overlap = true;
                    if(algn.ReadingFrame().Empty()) {
                        cds_overlap = false;
                    } else {
                        CChain a = algn;
                        a.Clip(a.RealCdsLimits(), CAlignModel::eRemoveExons);
                        ITERATE(list<list<CGene>::iterator>, k, included_in) {
                            if(!(*k)->IsAlternative(a)) {
                                cds_overlap = false;
                                break;
                            }
                        }
                    }

                    if(cds_overlap || (algn.Status()&CGeneModel::eConsistentCoverage)) {
#ifdef _DEBUG 
                        algn.AddComment("Gene overlap override");
#endif    
                        allow_connection = true;
                    }
                } 

                if(allow_connection) {
                    CGene& gene = *included_in.front();
                    gene.Insert(algn);

                    ITERATE(list<list<CGene>::iterator>, k, included_in) {
                        if(k != included_in.begin()) {
                            ITERATE(CGene, l, **k) {
                                if(itloop == not_placed_yet.end() || !DescendingModelOrder(**itloop, **l)) {  // next is not better 
                                    if(CheckCompatibility(*included_in.front(), **l) == eAlternative) {  // check that the thresholds are met   
#ifdef _DEBUG 
                                        (*l)->AddComment("Pass2b");
#endif    
                                        included_in.front()->Insert(**l);
                                    } else {
                                        not_placed_yet.push_back(*l); // position doesn't matter - will go to 'bad' models  
                                    }
                                } else {
                                    TChainPointerList::iterator idest = itloop;
                                    for( ;idest != not_placed_yet.end() && DescendingModelOrder(**idest, **l); ++idest);
                                    not_placed_yet.insert(idest, *l);
                                }
                            }
                            set<CGene*> nested_genes = (*k)->RemoveGeneFromOtherGenesSets();
                            ITERATE(set<CGene*>, i, nested_genes)
                                possibly_nested.push_back(*i);
                            alts.erase(*k);
                        }
                    }
                    not_placed_yet.erase(it);

                    ITERATE(list<CGene*>, itl, possibly_nested) {
                        if(gene.HarborsNested(**itl, true)) {
                            (*itl)->AddToNestedIn(&gene);
                            gene.AddToHarbored(*itl);
                        }
                    }
                }
            }
        }
    }
}

void CChainer::CChainerImpl::PlaceAllYouCan(list<CGene>& alts, TChainPointerList& not_placed_yet, TChainPointerList& rejected) {

    not_placed_yet.sort(DescendingModelOrderP);

    ITERATE(TChainPointerList, it, not_placed_yet) {
        CChain& algn(**it);
        list<CGene>::iterator included_in(alts.end());
        list<CGene*> possibly_nested;
        list<CGene*> nested_in;

        bool good_model = true;
        for(list<CGene>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);
            CNcbiOstrstream ost;
            switch(cmp) {
            case eNotCompatibleNested:
            case eNotCompatible:
                rejected.push_back(&algn);
                rejected.back()->Status() |= CGeneModel::eSkipped;
                ost << "Trumped by another model " << itl->front()->ID();
                rejected.back()->AddComment(CNcbiOstrstreamToString(ost));
                if(cmp == eNotCompatibleNested)
                    rejected.back()->SetType(rejected.back()->Type()|CGeneModel::eNested); 
                good_model = false;
                break;
            case eAlternative:
                if(!allow_partialalts && !itl->front()->GoodEnoughToBeAnnotation()) {
                    rejected.push_back(&algn);
                    rejected.back()->Status() |= CGeneModel::eSkipped;
                    ost << "Trumped by another model " << itl->front()->ID();
                    rejected.back()->AddComment(CNcbiOstrstreamToString(ost));
                    good_model = false;
                } else if(included_in == alts.end()) {
                    included_in = itl;
                } else {  // tries to connect two different genes
                    good_model = false;
                    rejected.push_back(&algn);
                    rejected.back()->Status() |= CGeneModel::eSkipped;
                    ost << "Connects two genes " << itl->front()->ID() << " " << included_in->front()->ID();
                    rejected.back()->AddComment(CNcbiOstrstreamToString(ost));
                }
                break;
            case eNested:
                nested_in.push_back(&(*itl));
                break;
            case eExternal:
                possibly_nested.push_back(&(*itl));  // already created gene is nested in this model
                break;
            case eOtherGene:
                break;
            }
        }
        if(good_model) {
            CGene* genep;
            if(included_in != alts.end()) {
#ifdef _DEBUG 
                algn.AddComment("Pass3a");
#endif    
                included_in->Insert(algn);
                genep = &(*included_in);
            } else {
                alts.push_back(CGene());
                genep = &alts.back();
#ifdef _DEBUG 
                algn.AddComment("Pass3b");
#endif    
                alts.back().Insert(algn);
            }
            ITERATE(list<CGene*>, itl, nested_in) {
                if((*itl)->HarborsNested(*genep, true)) {
                   genep->AddToNestedIn(*itl); 
                   (*itl)->AddToHarbored(genep);
                }
            }
            ITERATE(list<CGene*>, itl, possibly_nested) {
                if(genep->HarborsNested(**itl, true)) {
                    (*itl)->AddToNestedIn(genep);
                    genep->AddToHarbored(*itl);
                }
            }
        }
    }
}

void CChainer::CChainerImpl::FilterOutSimilarsWithLowerScore(TChainPointerList& not_placed_yet, TChainPointerList& rejected)
{
    not_placed_yet.sort(DescendingModelOrderP);

    NON_CONST_ITERATE(TChainPointerList, it, not_placed_yet) {
        CChain& ai(**it);
        TChainPointerList::iterator jt_loop = it;
        for(++jt_loop; jt_loop != not_placed_yet.end();) {
            TChainPointerList::iterator jt = jt_loop++;
            CChain& aj(**jt);
            if (CModelCompare::AreSimilar(ai,aj,tolerance)) {
                CNcbiOstrstream ost;
                ost << "Trumped by similar chain " << ai.ID();
                aj.AddComment(CNcbiOstrstreamToString(ost));
                rejected.push_back(&aj);
                not_placed_yet.erase(jt);
            }
        }
    }
}

void CChainer::CChainerImpl::FilterOutTandemOverlap(TChainPointerList& not_placed_yet, TChainPointerList& rejected, double fraction)
{
    for(TChainPointerList::iterator it_loop = not_placed_yet.begin(); it_loop != not_placed_yet.end();) {
        TChainPointerList::iterator it = it_loop++;
        CChain& ai(**it);

        if(!ai.TrustedmRNA().empty() || !ai.TrustedProt().empty() || ai.ReadingFrame().Empty())
            continue;
        int cds_len = ai.RealCdsLen();

        vector<const CChain*> candidates;
        ITERATE(TChainPointerList, jt, not_placed_yet) {
            const CChain& aj(**jt);
            if(!aj.HasStart() || !aj.HasStop() || aj.Score() < fraction/100*ai.Score() || aj.RealCdsLen() < fraction/100*cds_len || !CModelCompare::HaveCommonExonOrIntron(ai,aj)) 
                continue;
            candidates.push_back(&aj);
        }

        bool alive = true;
        for (size_t i = 0; alive && i < candidates.size(); ++i) {
            for (size_t j = i+1; alive && j < candidates.size(); ++j) {
                if(!candidates[i]->Limits().IntersectingWith(candidates[j]->Limits())) {
                    CNcbiOstrstream ost;
                    ost << "Overlapping tandem " << candidates[i]->ID() - ai.ID() << " " << candidates[j]->ID() - ai.ID();
                    ai.AddComment(CNcbiOstrstreamToString(ost));
                    rejected.push_back(*it);
                    not_placed_yet.erase(it);
                    alive = false;
                }
            }
        }
    }
}

list<CGene> CChainer::CChainerImpl::FindGenes(TChainList& cls)
{
    TChainPointerList not_placed_yet;
    NON_CONST_ITERATE(TChainList, it, cls) {
        if((it->Status()&CGeneModel::eSkipped) == 0) {
            if(it->Type()&CGeneModel::eNested)
                it->SetType(it->Type()^CGeneModel::eNested);
            it->SetGeneID(it->ID());
            it->SetRankInGene(0);
            not_placed_yet.push_back(&(*it));
        }
    }

    list<CGene> alts; 
    TChainPointerList bad_aligns;

    FilterOutSimilarsWithLowerScore(not_placed_yet, bad_aligns);
    FilterOutTandemOverlap(not_placed_yet, bad_aligns, 80);

    FindGeneSeeds(alts, not_placed_yet);
    ReplacePseudoGeneSeeds(alts, not_placed_yet);
    FindAltsForGeneSeeds(alts, not_placed_yet);
    PlaceAllYouCan(alts, not_placed_yet, bad_aligns);

    NON_CONST_ITERATE(list<CGene>, k, alts) {
        int rank = 0;
        NON_CONST_ITERATE(CGene, l, *k) {
            (*l)->SetGeneID(k->front()->ID());
            (*l)->SetRankInGene(++rank);
            if(k->Nested())
               (*l)->SetType((*l)->Type()|CGeneModel::eNested); 
        }
    }

    NON_CONST_ITERATE(TChainPointerList, l, bad_aligns) {
        (*l)->Status() |= CGeneModel::eSkipped;
	(*l)->AddComment("Not placed");
    }

    return alts;
}


struct GenomeOrderD
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)    // left end increasing, long first if left end equal
    {
        TSignedSeqRange alimits = ap->m_align->Limits();
        //ignore flexible ends for sorting
        if(ap->m_align->Status()&CGeneModel::eLeftFlexible)
            alimits.SetFrom(alimits.GetTo());
        if(ap->m_align->Status()&CGeneModel::eRightFlexible)
            alimits.SetTo(alimits.GetFrom());
        TSignedSeqRange blimits = bp->m_align->Limits();
        //ignore flexible ends for sorting
        if(bp->m_align->Status()&CGeneModel::eLeftFlexible)
            blimits.SetFrom(blimits.GetTo());
        if(bp->m_align->Status()&CGeneModel::eRightFlexible)
            blimits.SetTo(blimits.GetFrom());
        if(alimits == blimits) 
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
        else if(alimits.GetFrom() == blimits.GetFrom()) 
            return (alimits.GetTo() > blimits.GetTo());
        else 
            return (alimits.GetFrom() < blimits.GetFrom());
    }
};        


typedef vector< pair<SChainMember*,CGene*> > TMemeberGeneVec;

typedef tuple<Int8, TSignedSeqRange> TIdLim;
TIdLim AlignIdLimits(SChainMember* mp) {
    return make_tuple(mp->m_align->ID(), mp->m_align->Limits());
}
struct AlignIdOrder
{
    bool operator()(const TMemeberGeneVec::value_type& a, const TMemeberGeneVec::value_type& b) 
    {
        return AlignIdLimits(a.first) < AlignIdLimits(b.first);
    }
};


void CChainer::CChainerImpl::TrimAlignmentsIncludedInDifferentGenes(list<CGene>& genes) {
    TMemeberGeneVec members_genes;
    NON_CONST_ITERATE(list<CGene>, ig, genes) {
        CGene& gene = *ig;
        TMemberPtrSet gmembers;
        ITERATE(CGene, ic, gene) {
            CChain& chain = **ic;
            ITERATE(TContained, im, chain.m_members) {
                SChainMember& m = **im;
                _ASSERT(m.m_orig_align);
                if(m.m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
                    continue;
                if(m.m_orig_align->Continuous())
                    gmembers.insert(&m);
            }
        }
        ITERATE(TMemberPtrSet, im, gmembers) {
            SChainMember& m = **im;
            members_genes.push_back(TMemeberGeneVec::value_type(&m,&gene));
        }
    }

    if(members_genes.empty())
        return;

    sort(members_genes.begin(),members_genes.end(),AlignIdOrder());

    typedef map<CGene*,list<SChainMember*> > TGeneToMembers;
    typedef map<TIdLim, TGeneToMembers> TMembersInDiffGenes;
    TMembersInDiffGenes members_in_different_genes;
    {
        SChainMember* mp = members_genes.front().first;
        TIdLim idlim = AlignIdLimits(mp);
        CGene* genep = members_genes.front().second;
        members_in_different_genes[idlim][genep].push_back(mp); 
    }
    for(int i = 1; i < (int)members_genes.size(); ++i) {
        TIdLim idlim_prev = AlignIdLimits(members_genes[i-1].first);
        SChainMember* mp = members_genes[i].first;
        TIdLim idlim = AlignIdLimits(mp);
        CGene* genep = members_genes[i].second;
        if(idlim_prev != idlim) {
            TMembersInDiffGenes::iterator it = members_in_different_genes.find(idlim_prev);
            if(it->second.size() < 2) // alignment in only one gene
                members_in_different_genes.erase(it); 
        }
        members_in_different_genes[idlim][genep].push_back(mp); 
    }
    {
        SChainMember* mp = members_genes.back().first;
        TIdLim idlim = AlignIdLimits(mp);
        TMembersInDiffGenes::iterator it = members_in_different_genes.find(idlim);
        if(it->second.size() < 2) // alignment in only one gene
            members_in_different_genes.erase(it); 
    }        

    ITERATE(TMembersInDiffGenes, imdg, members_in_different_genes) {
        ITERATE(TGeneToMembers, ig1, imdg->second) {
            CGene& gene1 = *ig1->first;
            ITERATE(CGene, ic1, gene1) { 
                CChain& chain1 = **ic1;
                sort(chain1.m_members.begin(), chain1.m_members.end(), std::less<SChainMember*>());
            }
        }
    }    
    
    typedef map<CChain*,TMemberPtrSet> TConflictMemebersInChains;
    TConflictMemebersInChains conflict_members_in_chains;

    ITERATE(TMembersInDiffGenes, imdg, members_in_different_genes) {
        ITERATE(TGeneToMembers, ig1, imdg->second) {
            CGene& gene1 = *ig1->first;
            ITERATE(CGene, ic1, gene1) { 
                CChain* chain1p_orig = *ic1;
                SChainMember* mbr1p_orig = 0;
                for(list<SChainMember*>::const_iterator im = ig1->second.begin(); im != ig1->second.end() && mbr1p_orig == 0; ++im) {
                    if(binary_search(chain1p_orig->m_members.begin(),chain1p_orig->m_members.end(),*im, std::less<SChainMember*>()))
                       mbr1p_orig = *im;
                }
                for(TGeneToMembers::const_iterator ig2 = imdg->second.begin(); mbr1p_orig != 0 && ig2 != ig1; ++ig2) {
                    CGene& gene2 = *ig2->first;
                    ITERATE(CGene, ic2, gene2) { 
                        CChain* chain1p = chain1p_orig;
                        SChainMember* mbr1p = mbr1p_orig;
                        CChain* chain2p = *ic2;
                        SChainMember* mbr2p = 0;
                        for(list<SChainMember*>::const_iterator im = ig2->second.begin(); im != ig2->second.end() && mbr2p == 0; ++im) {
                            if(binary_search(chain2p->m_members.begin(),chain2p->m_members.end(),*im, std::less<SChainMember*>()))
                                mbr2p = *im;
                        }
                    
                        if(mbr2p != 0) {    // both chains have alignment 

                            TSignedSeqRange core1 = chain1p->RealCdsLimits();
                            if(chain1p->Exons().size() > 1)
                                core1 += TSignedSeqRange(chain1p->Exons().front().Limits().GetTo(),chain1p->Exons().back().Limits().GetFrom());
                            TSignedSeqRange core2 = chain2p->RealCdsLimits();
                            if(chain2p->Exons().size() > 1)
                                core2 += TSignedSeqRange(chain2p->Exons().front().Limits().GetTo(),chain2p->Exons().back().Limits().GetFrom());
                            _ASSERT(core1.NotEmpty() && core2.NotEmpty());

                            if(Precede(core2,core1)) {   // chain2 is on the left change them over to simplify coding below
                                swap(chain1p,chain2p);
                                swap(mbr1p,mbr2p);
                                swap(core1,core2);
                            }
                            
                            CChain& chain1 = *chain1p;
                            CChain& chain2 = *chain2p;
                            TSignedSeqRange align_lim = mbr1p->m_align->Limits();

                            if(CModelCompare::RangeNestedInIntron(core2, chain1)) {            // chain2 is nested 
                                conflict_members_in_chains[&chain2].insert(mbr2p);
                            } else if(CModelCompare::RangeNestedInIntron(core1, chain2)) {     // chain1 is nested 
                                conflict_members_in_chains[&chain1].insert(mbr1p);
                            }else if(Precede(core1,core2)) {                                   // chain1 on the left
                                if(Precede(align_lim,core1))                                       // alignment on the left of chain1
                                    conflict_members_in_chains[&chain2].insert(mbr2p);
                                else if(Precede(core2,align_lim))                                  // alignment on the right of chain2
                                    conflict_members_in_chains[&chain1].insert(mbr1p);
                                else {                                                             // alignmnet in between
                                    if(chain1.m_coverage_drop_right > 0 && chain2.m_coverage_drop_left > chain1.m_coverage_drop_right) {  // non overlapping drop limits
                                        if(align_lim.GetTo() > chain1.m_coverage_drop_right)
                                            conflict_members_in_chains[&chain1].insert(mbr1p);
                                        if(align_lim.GetFrom() < chain2.m_coverage_drop_left)
                                            conflict_members_in_chains[&chain2].insert(mbr2p);
                                    } else if(chain1.m_coverage_drop_right > 0 && chain2.m_coverage_drop_left < 0 && chain1.m_core_coverage > 2*chain2.m_core_coverage) {    // only chain1 has drop limit and is more expressed
                                        if(align_lim.GetTo() > chain1.m_coverage_drop_right)
                                            conflict_members_in_chains[&chain1].insert(mbr1p);
                                        if(align_lim.GetFrom() < max(chain2.m_coverage_bump_left,chain1.m_coverage_drop_right+50))
                                            conflict_members_in_chains[&chain2].insert(mbr2p);
                                    } else if(chain1.m_coverage_drop_right < 0 && chain2.m_coverage_drop_left > 0 && chain2.m_core_coverage > 2*chain1.m_core_coverage) {    // only chain2 has drop limit and is more expressed
                                        if(align_lim.GetFrom() < chain2.m_coverage_drop_left)
                                            conflict_members_in_chains[&chain2].insert(mbr2p);
                                        if(align_lim.GetTo() > chain2.m_coverage_drop_left-50 || (chain1.m_coverage_bump_right > 0 && align_lim.GetTo() > chain1.m_coverage_bump_right))
                                            conflict_members_in_chains[&chain1].insert(mbr1p);
                                    } else {
                                        conflict_members_in_chains[&chain1].insert(mbr1p);
                                        conflict_members_in_chains[&chain2].insert(mbr2p);
                                    }
                                }
                            } else {
                                conflict_members_in_chains[&chain1].insert(mbr1p);
                                conflict_members_in_chains[&chain2].insert(mbr2p);
                            }
                        }
                    }
                }
            }
        }
    }

    for(CGene& gene : genes) {
        for(CChain* chainp : gene)
           sort(chainp->m_members.begin(),chainp->m_members.end(),GenomeOrderD()); 
    }

    /*
    ITERATE(TMembersInDiffGenes, imdg, members_in_different_genes) {
        ITERATE(TGeneToMembers, ig1, imdg->second) {
            CGene& gene1 = *ig1->first;
            ITERATE(CGene, ic1, gene1) { 
                CChain& chain1 = **ic1;
                sort(chain1.m_members.begin(),chain1.m_members.end(),GenomeOrderD());
            }
        }
    }
    */

    ITERATE(TConflictMemebersInChains, it, conflict_members_in_chains) {
        CChain& chain = *it->first;
        const TMemberPtrSet& conflict_members = it->second;

        CAlignMap amap = chain.GetAlignMap();

        TSignedSeqRange hard_limits(chain.Exons().front().Limits().GetTo()-15,chain.Exons().back().Limits().GetFrom()+15);
        hard_limits = (hard_limits & chain.Limits());
        if(chain.ReadingFrame().NotEmpty())
            hard_limits = (chain.OpenCds() ? chain.MaxCdsLimits() : chain.RealCdsLimits());

        TSignedSeqRange noclip_limits = hard_limits;

        /*
        int hard_limits_len = amap.FShiftedLen(hard_limits);
        ITERATE(TContained, i, chain.m_members) {
            const CGeneModel& a = *(*i)->m_align;
            if(a.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
                continue;
            TSignedSeqRange alim(amap.ShrinkToRealPoints(a.Limits()&chain.Limits(),false));
            if(Include(alim,hard_limits.GetFrom()) ) {
                TSignedSeqRange l(hard_limits.GetFrom(),alim.GetTo());
                l = amap.ShrinkToRealPoints(l,false);
                int len = 0;
                if(l.NotEmpty())
                    len = amap.FShiftedLen(l);
                if(len > 0.75*a.AlignLen() || len > 0.75*hard_limits_len)
                    noclip_limits.SetFrom(min(noclip_limits.GetFrom(),alim.GetFrom()));
            }
            if(Include(alim,hard_limits.GetTo())) {
                TSignedSeqRange l(alim.GetFrom(),hard_limits.GetTo());
                l = amap.ShrinkToRealPoints(l,false);
                int len = 0;
                if(l.NotEmpty())
                    len = amap.FShiftedLen(l);
                if(len > 0.75*a.AlignLen() || len > 0.75*hard_limits_len)                
                    noclip_limits.SetTo(max(noclip_limits.GetTo(),alim.GetTo()));
            }
        }

        noclip_limits = (noclip_limits & chain.Limits());
        */

        if(chain.Status()&CGeneModel::ePolyA) {
            if(chain.Strand() == ePlus) {
                if(chain.m_coverage_drop_right < 0)
                    noclip_limits.SetTo(max(noclip_limits.GetTo(),chain.m_polya_cap_right_soft_limit));
                else
                    noclip_limits.SetTo(max(noclip_limits.GetTo(),chain.m_coverage_drop_right));
            } else {
                if(chain.m_coverage_drop_left < 0)
                    noclip_limits.SetFrom(min(noclip_limits.GetFrom(),chain.m_polya_cap_left_soft_limit));
                else
                    noclip_limits.SetFrom(min(noclip_limits.GetFrom(),chain.m_coverage_drop_left));
            }
        }
        if(chain.Status()&CGeneModel::eCap) {
            if(chain.Strand() == ePlus) {
                if(chain.m_coverage_drop_left < 0)
                    noclip_limits.SetFrom(min(noclip_limits.GetFrom(),chain.m_polya_cap_left_soft_limit));
                else
                    noclip_limits.SetFrom(min(noclip_limits.GetFrom(),chain.m_coverage_drop_left));
            } else {
                if(chain.m_coverage_drop_right < 0)
                    noclip_limits.SetTo(max(noclip_limits.GetTo(),chain.m_polya_cap_right_soft_limit));
                else
                    noclip_limits.SetTo(max(noclip_limits.GetTo(),chain.m_coverage_drop_right));
            }
        }        

        TSignedSeqRange new_limits = chain.Limits();
	
        ITERATE(TMemberPtrSet, im, conflict_members) {
            TSignedSeqRange alim = (*im)->m_align->Limits()&chain.Limits();
            if(alim.Empty())
                continue;
            alim = amap.ShrinkToRealPoints(alim);
            if(alim.Empty())
                continue;
            if(alim.GetFrom() < noclip_limits.GetFrom()) {
                int to = min(noclip_limits.GetFrom(),alim.GetTo());
                if(chain.m_coverage_drop_left > 0 && Include(alim,chain.m_coverage_drop_left)) {
                    to = min(noclip_limits.GetFrom(),chain.m_coverage_drop_left);
                }
                new_limits.SetFrom(max(new_limits.GetFrom(),to));
            } else if(alim.GetTo() > noclip_limits.GetTo()) {
                int from = max(noclip_limits.GetTo(),alim.GetFrom());
                if(chain.m_coverage_drop_right > 0 && Include(alim,chain.m_coverage_drop_right)) {
                    from = max(noclip_limits.GetTo(),chain.m_coverage_drop_right);
                }
                new_limits.SetTo(min(new_limits.GetTo(),from));
            }
        }

        int left_splice = -1;
        int right_splice = -1;
        for(int e = 1; e < (int)chain.Exons().size(); ++e) {
            if(left_splice < 0 && chain.Exons()[e-1].m_ssplice && Include(new_limits,chain.Exons()[e-1].GetTo()))
                left_splice = chain.Exons()[e-1].GetTo();
            if(chain.Exons()[e].m_fsplice && Include(new_limits,chain.Exons()[e].GetFrom()))
                right_splice = chain.Exons()[e].GetFrom();
        }
        map<int,double> left_weights;
        double left_weights_total = 0.;
        map<int,double> right_weights;
        double right_weights_total = 0.;
        ITERATE(TContained, i, chain.m_members) {
            const CGeneModel& a = *(*i)->m_align;
            if(a.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
                continue;
            TSignedSeqRange alim(amap.ShrinkToRealPoints(a.Limits()&chain.Limits(),false));
            for(int e = 1; e < (int)a.Exons().size(); ++e) {
                if(a.Exons()[e-1].m_ssplice && a.Exons()[e-1].GetTo() == left_splice) {
                    left_weights[alim.GetFrom()] += a.Weight();
                    left_weights_total += a.Weight();
                }
                if(a.Exons()[e].m_fsplice && a.Exons()[e].GetFrom() == right_splice) {
                    right_weights[alim.GetTo()] += a.Weight();
                    right_weights_total += a.Weight();
                }
            }
        }
        if(left_weights_total > 0.) {
            int left = numeric_limits<int>::max();
            double t = 0;
            for(map<int,double>::reverse_iterator it = left_weights.rbegin(); it != left_weights.rend(); ++it) {
                if(t < 0.9*left_weights_total)
                    left = it->first;
                t += it->second;
            }
            if(left < new_limits.GetFrom())
                new_limits.SetFrom(left);
        }
        if(right_weights_total > 0.) {
            int right = 0;
            double t = 0;
            for(map<int,double>::iterator it = right_weights.begin(); it != right_weights.end(); ++it) {
                if(t < 0.9*right_weights_total)
                    right = it->first;
                t += it->second;
            }
            if(right > new_limits.GetTo())
                new_limits.SetTo(right);
        }

        //if has to clip, clip to next cap/polya
        if(new_limits.GetFrom() != chain.Limits().GetFrom() && chain.m_polya_cap_left_soft_limit < chain.Limits().GetTo())
            new_limits.SetFrom(chain.m_polya_cap_left_soft_limit);
        if(new_limits.GetTo() != chain.Limits().GetTo() && chain.m_polya_cap_right_soft_limit > chain.Limits().GetFrom())
            new_limits.SetTo(chain.m_polya_cap_right_soft_limit);

        //don't clip confirmed ends
        if(chain.Status()&CGeneModel::eLeftConfirmed)
            new_limits.SetFrom(chain.Limits().GetFrom());
        if(chain.Status()&CGeneModel::eRightConfirmed)
            new_limits.SetTo(chain.Limits().GetTo());

        if(new_limits != chain.Limits()) {
            string note;
            if(new_limits.GetFrom() != chain.Limits().GetFrom())
                note += "Left";
            if(new_limits.GetTo() != chain.Limits().GetTo())
                note += "Right";
            note += " overlap UTR clip";
            chain.AddComment(note);
            _ASSERT(new_limits.NotEmpty());

            bool wasopen = chain.OpenCds();
            chain.ClipChain(new_limits);
            if(chain.Type()&CGeneModel::eNested)
                chain.ClipLowCoverageUTR(0.1);
            _ASSERT(chain.Limits().NotEmpty());
            if(chain.ReadingFrame().NotEmpty()) {
                m_gnomon->GetScore(chain, !no5pextension);
                CCDSInfo cds = chain.GetCdsInfo();
                if(wasopen != chain.OpenCds() && (wasopen == false || cds.HasStart())) {
                    cds.SetScore(cds.Score(),wasopen);
                    chain.SetCdsInfo(cds);
                }
            }
            chain.CalculateDropLimits();
        }
    }
}

void SChainMember::AddCodingToContained(TContained& contained, TMemberPtrSet& included_in_list) {
    if(m_type != eCDS)
        return;

    //    list<const SChainMember*> not_visited(1,this);
    deque<const SChainMember*> not_visited(1,this);
    while(!not_visited.empty()) {
        const SChainMember* mbr = not_visited.front();
        for(int c = 0; c < (int)mbr->m_contained->size(); ++c) {
            SChainMember* mi = (*mbr->m_contained)[c];
            if(mi->m_type != eCDS)
                continue;
            if(c < mbr->m_identical_count) {
                if(included_in_list.insert(mi).second) {
                    contained.push_back(mi);                  //action  
                    if(mi->m_copy != 0) 
                        included_in_list.insert(mi->m_copy->begin(),mi->m_copy->end());
                }
            } else if(included_in_list.find(mi) == included_in_list.end()) {
                not_visited.push_back(mi);                //store for future
            }            
        }
        not_visited.pop_front();
    }
}

TContained SChainMember::CollectCodingContainedForMemeber() {

    TContained contained;
    TMemberPtrSet included_in_list;
    AddCodingToContained(contained, included_in_list);

    return contained;
}

TContained SChainMember::CollectCodingContainedForChain()
{
    TContained contained;
    TMemberPtrSet included_in_list;

    AddCodingToContained(contained, included_in_list);

    for (SChainMember* left = m_left_member; left != 0; left = left->m_left_member) {
        left->AddCodingToContained(contained, included_in_list);
    }

    for (SChainMember* right = m_right_member; right != 0; right = right->m_right_member) {
        right->AddCodingToContained(contained, included_in_list);
    }

    return contained;
}
//visits all levels of nested and adds uniquely to contained
void SChainMember::AddToContained(TContained& contained, TMemberPtrSet& included_in_list) {

    //    list<const SChainMember*> not_visited(1,this);
    deque<const SChainMember*> not_visited(1,this);
    while(!not_visited.empty()) {
        const SChainMember* mbr = not_visited.front();
        for(int c = 0; c < (int)mbr->m_contained->size(); ++c) {
            SChainMember* mi = (*mbr->m_contained)[c];
            if(c < mbr->m_identical_count) {
                if(included_in_list.insert(mi).second) {
                    contained.push_back(mi);                  //action  
                    if(mi->m_copy != 0) 
                        included_in_list.insert(mi->m_copy->begin(),mi->m_copy->end());
                }
            } else if(included_in_list.find(mi) == included_in_list.end()) {
                not_visited.push_back(mi);                //store for future
            }            
        }
        not_visited.pop_front();
    }
}

TContained SChainMember::CollectContainedForMemeber() {

    TContained contained;
    TMemberPtrSet included_in_list;
    AddToContained(contained, included_in_list);

    return contained;
}

TContained SChainMember::CollectContainedForChain()
{
    TContained contained;
    TMemberPtrSet included_in_list;

    AddToContained(contained, included_in_list);

    for (SChainMember* left = m_left_member; left != 0; left = left->m_left_member) {
        left->AddToContained(contained, included_in_list);
    }

    for (SChainMember* right = m_right_member; right != 0; right = right->m_right_member) {
        right->AddToContained(contained, included_in_list);
    }

    return contained;
}  

#define START_BONUS 600 

void SChainMember::MarkIncluded() {
    m_included = true;
    if (m_copy != 0) {
        ITERATE(TContained, j, *m_copy) {
            SChainMember* mj = *j;
            if(mj->m_type != eCDS || mj->m_cds < START_BONUS+25 || 
               (m_align->Strand() == mj->m_align->Strand() &&
                (m_cds_info->ReadingFrame().GetFrom() == mj->m_cds_info->ReadingFrame().GetFrom() ||   // same copy or supressed start
                 m_cds_info->ReadingFrame().GetTo() == mj->m_cds_info->ReadingFrame().GetTo())))       // same copy or supressed start
                mj->m_included = true;
        }
    }
}

void SChainMember::MarkIncludedForChain()
{
    TContained contained = CollectContainedForChain();
    NON_CONST_ITERATE (TContained, i, contained) {
        SChainMember* mi = *i;
        mi->MarkIncluded(); 
    }
}

void SChainMember::MarkPostponed() 
{
    m_postponed = true;
    if (m_copy != 0) {
        ITERATE(TContained, j, *m_copy) {
            SChainMember* mj = *j;
            if(mj->m_type != eCDS || mj->m_cds < START_BONUS+25 || 
               (m_align->Strand() == mj->m_align->Strand() &&
                (m_cds_info->ReadingFrame().GetFrom() == mj->m_cds_info->ReadingFrame().GetFrom() ||   // same copy or supressed start
                 m_cds_info->ReadingFrame().GetTo() == mj->m_cds_info->ReadingFrame().GetTo())))       // same copy or supressed start
                mj->m_postponed = true;
        }
    }
}   

void SChainMember::MarkPostponedForChain()
{
    TContained contained = CollectContainedForChain();
    NON_CONST_ITERATE (TContained, i, contained) {
        SChainMember* mi = *i;
        mi->MarkPostponed();                
    }
}

void SChainMember::MarkUnwantedCopiesForChain(const TSignedSeqRange& cds)
{
    TContained contained = CollectContainedForChain();
    NON_CONST_ITERATE (TContained, i, contained) {
        SChainMember* mi = *i;
        CGeneModel& algni = *mi->m_align;
        const CCDSInfo& cinfoi = *mi->m_cds_info;
        if(Include(cds, cinfoi.ReadingFrame())) {
            mi->m_marked_for_retention = true;
            mi->m_marked_for_deletion = false;
            if (mi->m_copy != 0) {
                ITERATE(TContained, j, *mi->m_copy) {
                    SChainMember* mj = *j;
                    const CCDSInfo& cinfoj = *mj->m_cds_info;
                    if(mj->m_marked_for_retention)      // already included in cds
                        continue;
                    else if(cinfoi.HasStart() || cinfoj.HasStart()) {         // don't delete copy which overrides the start or has the start
                        if((algni.Strand() == ePlus && cinfoi.ReadingFrame().GetTo() == cinfoj.ReadingFrame().GetTo()) ||
                           (algni.Strand() == eMinus && cinfoi.ReadingFrame().GetFrom() == cinfoj.ReadingFrame().GetFrom()))
                            continue;
                    }
                    mj->m_marked_for_deletion = true;
                }
            }
        }
    }
}


struct LeftOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)    // right end increasing, short first if right end equal
    {
        TSignedSeqRange alimits = ap->m_align->Limits();
        //ignore flexible ends for sorting
        if(ap->m_align->Status()&CGeneModel::eLeftFlexible)
            alimits.SetFrom(alimits.GetTo());
        if(ap->m_align->Status()&CGeneModel::eRightFlexible)
            alimits.SetTo(alimits.GetFrom());
        TSignedSeqRange blimits = bp->m_align->Limits();
        //ignore flexible ends for sorting
        if(bp->m_align->Status()&CGeneModel::eLeftFlexible)
            blimits.SetFrom(blimits.GetTo());
        if(bp->m_align->Status()&CGeneModel::eRightFlexible)
            blimits.SetTo(blimits.GetFrom());

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
        TSignedSeqRange alimits = ap->m_align->Limits();
        //ignore flexible ends for sorting
        if(ap->m_align->Status()&CGeneModel::eLeftFlexible)
            alimits.SetFrom(alimits.GetTo());
        if(ap->m_align->Status()&CGeneModel::eRightFlexible)
            alimits.SetTo(alimits.GetFrom());
        TSignedSeqRange blimits = bp->m_align->Limits();
        //ignore flexible ends for sorting
        if(bp->m_align->Status()&CGeneModel::eLeftFlexible)
            blimits.SetFrom(blimits.GetTo());
        if(bp->m_align->Status()&CGeneModel::eRightFlexible)
            blimits.SetTo(blimits.GetFrom());

        if(alimits == blimits) 
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
        else if(alimits.GetTo() == blimits.GetTo()) 
            return (alimits.GetFrom() > blimits.GetFrom());
        else 
            return (alimits.GetTo() < blimits.GetTo());
    }
};


struct RightOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)   // left end decreasing, short first if left end equal
    {
        TSignedSeqRange alimits = ap->m_align->Limits();
        //ignore flexible ends for sorting
        if(ap->m_align->Status()&CGeneModel::eLeftFlexible)
            alimits.SetFrom(alimits.GetTo());
        if(ap->m_align->Status()&CGeneModel::eRightFlexible)
            alimits.SetTo(alimits.GetFrom());
        TSignedSeqRange blimits = bp->m_align->Limits();
        //ignore flexible ends for sorting
        if(bp->m_align->Status()&CGeneModel::eLeftFlexible)
            blimits.SetFrom(blimits.GetTo());
        if(bp->m_align->Status()&CGeneModel::eRightFlexible)
            blimits.SetTo(blimits.GetFrom());

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
        TSignedSeqRange alimits = ap->m_align->Limits();
        //ignore flexible ends for sorting
        if(ap->m_align->Status()&CGeneModel::eLeftFlexible)
            alimits.SetFrom(alimits.GetTo());
        if(ap->m_align->Status()&CGeneModel::eRightFlexible)
            alimits.SetTo(alimits.GetFrom());
        TSignedSeqRange blimits = bp->m_align->Limits();
        //ignore flexible ends for sorting
        if(bp->m_align->Status()&CGeneModel::eLeftFlexible)
            blimits.SetFrom(blimits.GetTo());
        if(bp->m_align->Status()&CGeneModel::eRightFlexible)
            blimits.SetTo(blimits.GetFrom());

        if(alimits == blimits) 
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
        else if(alimits.GetFrom() == blimits.GetFrom())
            return (alimits.GetTo() < blimits.GetTo());
        else
            return (alimits.GetFrom() > blimits.GetFrom());
    }
};


struct CdsNumOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)
    {
        if(max(ap->m_cds,bp->m_cds) >= 300 && ap->m_cds != bp->m_cds) // only long cdses count
            return (ap->m_cds > bp->m_cds);
        else if(fabs(ap->m_splice_num - bp->m_splice_num) > 0.001)
            return (ap->m_splice_num > bp->m_splice_num);
        else if(fabs(ap->m_num - bp->m_num) > 0.001)
            return (ap->m_num > bp->m_num);
        else
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
    }
};

struct ScoreOrder
{
    bool operator()(const SChainMember* ap, const SChainMember* bp)
    {
        if (ap->m_cds_info->Score() == bp->m_cds_info->Score())
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
        else
            return (ap->m_cds_info->Score() > bp->m_cds_info->Score());
    }
};

template <class C>
void uniq(C& container)
{
    sort(container.begin(),container.end());
    container.erase( unique(container.begin(),container.end()), container.end() );
}

class CChainMembers : public vector<SChainMember*> {
public:
    CChainMembers() { m_extra_cds.push_back(CCDSInfo()); }   // empty cds for utrs; first in the list
    CChainMembers(TGeneModelList& clust, TOrigAligns& orig_aligns, TUnmodAligns& unmodified_aligns);
    void InsertMember(CGeneModel& algn, SChainMember* copy_ofp = 0);
    void InsertMemberCopyWithCds(const CCDSInfo& cds, SChainMember* copy_ofp);
    void InsertMemberCopyAndStoreCds(const CCDSInfo& cds, SChainMember* copy_ofp);
    void InsertMemberCopyWithoutCds(SChainMember* copy_ofp);
    void InsertMember(SChainMember& m, SChainMember* copy_ofp = 0); 
    void DuplicateUTR(SChainMember* copy_ofp); 
    void SpliceFromOther(CChainMembers& other);
private:
    CChainMembers(const CChainMembers& object) = delete;
    CChainMembers& operator=(const CChainMembers& object) = delete;    
    list<SChainMember> m_members;
    list<TContained> m_copylist;
    list<CAlignMap> m_align_maps;
    list<TContained> m_containedlist;
    list<CCDSInfo> m_extra_cds;
};

void CChainMembers::SpliceFromOther(CChainMembers& other) {
    m_members.splice(m_members.end(),other.m_members);
    m_copylist.splice(m_copylist.end(),other.m_copylist);
    m_align_maps.splice(m_align_maps.end(),other.m_align_maps);
    m_containedlist.splice(m_containedlist.end(),other.m_containedlist);
    m_extra_cds.splice(m_extra_cds.end(),other.m_extra_cds);
    insert(end(),other.begin(),other.end());
}

void CChainMembers::InsertMemberCopyWithCds(const CCDSInfo& cds, SChainMember* copy_ofp) {

    SChainMember mbr = *copy_ofp;
    mbr.m_cds_info = &cds;
    mbr.m_type = eCDS;
    InsertMember(mbr, copy_ofp);
}

void CChainMembers::InsertMemberCopyAndStoreCds(const CCDSInfo& cds, SChainMember* copy_ofp) {

    m_extra_cds.push_back(cds);
    InsertMemberCopyWithCds(m_extra_cds.back(), copy_ofp);
}

void CChainMembers::InsertMemberCopyWithoutCds(SChainMember* copy_ofp) {

    SChainMember mbr = *copy_ofp;
    mbr.m_cds_info = &m_extra_cds.front(); // empty cds
    mbr.m_type = eLeftUTR;
    InsertMember(mbr, copy_ofp);
}


void CChainMembers::InsertMember(CGeneModel& algn, SChainMember* copy_ofp)
{
    SChainMember mbr;
    mbr.m_align = &algn;
    mbr.m_cds_info = &algn.GetCdsInfo();
    mbr.m_type = eCDS;
    if(algn.Score() == BadScore()) 
        mbr.m_type = eLeftUTR;
    if(copy_ofp) {
        mbr.m_orig_align = copy_ofp->m_orig_align;
        mbr.m_unmd_align = copy_ofp->m_unmd_align;
    }
    InsertMember(mbr, copy_ofp);
}

void CChainMembers::InsertMember(SChainMember& m, SChainMember* copy_ofp)
{
    m.m_mem_id = (int)size()+1;
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


CChainMembers::CChainMembers(TGeneModelList& clust, TOrigAligns& orig_aligns, TUnmodAligns& unmodified_aligns)
{
    m_extra_cds.push_back(CCDSInfo());      // empty cds for utrs; first in the list
    NON_CONST_ITERATE(TGeneModelList, itcl, clust) {
        InsertMember(*itcl);
        m_members.back().m_orig_align = orig_aligns[itcl->ID()];
        if(unmodified_aligns.count(itcl->ID()))
            m_members.back().m_unmd_align = &unmodified_aligns[itcl->ID()];        
    }
}


TSignedSeqRange ExtendedMaxCdsLimits(const CGeneModel& a, const CCDSInfo& cds)
{
    TSignedSeqRange limits(a.Limits().GetFrom()-1,a.Limits().GetTo()+1);

    return limits & cds.MaxCdsLimits();
}


void CChainer::CChainerImpl::IncludeInContained(SChainMember& big, SChainMember& small)
{
    //all identical members are contained in each other; only one of them (with smaller m_mem_id) is contained in other members
    TSignedSeqRange big_limits = big.m_align->Limits();
    if(big.m_align->Status()&CGeneModel::eLeftFlexible)
        big_limits.SetFrom(big_limits.GetTo());
    if(big.m_align->Status()&CGeneModel::eRightFlexible)
        big_limits.SetTo(big_limits.GetFrom());
    TSignedSeqRange small_limits = small.m_align->Limits();
    bool small_flex = false;
    if(small.m_align->Status()&CGeneModel::eLeftFlexible) {
        small_limits.SetFrom(small_limits.GetTo());
        small_flex = true;
    }
    if(small.m_align->Status()&CGeneModel::eRightFlexible) {
        small_limits.SetTo(small_limits.GetFrom());
        small_flex = true;
    }

    if(big_limits == small_limits) {  // identical
        ++big.m_identical_count;
        big.m_contained->push_back(&small);
        return;
    } else if(big.m_sink_for_contained != nullptr &&
              small_limits.GetTo() <= big.m_sink_for_contained->m_align->Limits().GetTo() &&
              CanIncludeJinI(*big.m_sink_for_contained, small)) {
        return;  // contained in next level
    } else {
        big.m_contained->push_back(&small);
        if(!small_flex && (big.m_sink_for_contained == nullptr || small_limits.GetTo() > big.m_sink_for_contained->m_align->Limits().GetTo()))
            big.m_sink_for_contained = &small;
    }
}


void CChainer::CutParts(TGeneModelList& models)
{
    m_data->CutParts(models);
}

void CChainer::CChainerImpl::CutParts(TGeneModelList& models) {
    ERASE_ITERATE(TGeneModelList, im, models) {
        TGeneModelList parts = GetAlignParts(*im, true);
        if(!parts.empty()) {
            models.splice(models.begin(),parts);
            models.erase(im);            
        }
    }
}

void CChainer::CChainerImpl::DuplicateNotOriented(CChainMembers& pointers, TGeneModelList& clust)
{
    size_t initial_size = pointers.size();
    for(size_t i = 0; i < initial_size; ++i) {
        SChainMember& mbr = *pointers[i];
        CGeneModel& algn = *mbr.m_align;
        if((algn.Status()&CGeneModel::eUnknownOrientation) != 0) {
            CGeneModel new_algn = algn;
            new_algn.ReverseComplementModel();
            new_algn.Status() &= ~CGeneModel::eReversed;
            clust.push_back(new_algn);
            pointers.InsertMember(clust.back(), &mbr);    //reversed copy     
        }
    }
}

void CChainer::CChainerImpl::DuplicateUTRs(CChainMembers& pointers)
{
    size_t initial_size = pointers.size();
    for(size_t i = 0; i < initial_size; ++i) {
        SChainMember& mbr = *pointers[i];
        if(mbr.m_align->Status()&CGeneModel::eLeftFlexible)
            mbr.m_type = eRightUTR;
        else if(mbr.m_align->Status()&CGeneModel::eRightFlexible)
            mbr.m_type = eLeftUTR;
        else if(mbr.m_cds_info->Score() == BadScore()) 
            pointers.DuplicateUTR(&mbr);
    }
}

void CChainer::CChainerImpl::CalculateSpliceWeights(CChainMembers& pointers)
{
    map<int, set<int> > oriented_splices;
    ITERATE(set<TSignedSeqRange>, i, oriented_introns_plus) {
        oriented_splices[ePlus].insert(i->GetFrom());
        oriented_splices[ePlus].insert(i->GetTo());
    }
    ITERATE(set<TSignedSeqRange>, i, oriented_introns_minus) {
        oriented_splices[eMinus].insert(i->GetFrom());
        oriented_splices[eMinus].insert(i->GetTo());
    }

    NON_CONST_ITERATE(CChainMembers, i, pointers) {
        SChainMember& mbr = **i;
        CGeneModel& algn = *mbr.m_align;
        if(algn.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;
        set<int>& ospl = oriented_splices[algn.Strand()];
        ITERATE(CGeneModel::TExons, ie, algn.Exons()) {
            TSignedSeqRange exon = *ie;
            for(set<int>::iterator spli = ospl.lower_bound(exon.GetFrom()); spli != ospl.end() && *spli <= exon.GetTo(); ++spli)
                mbr.m_splice_weight += algn.Weight();
        }
    }
}

void CChainer::CChainerImpl::ReplicatePStops(CChainMembers& pointers)
{
    int left = numeric_limits<int>::max();
    int right = 0;
    typedef vector<pair<CCDSInfo::SPStop,TSignedSeqRange> > TPstopIntron;
    TPstopIntron pstops_with_intron_plus;
    TPstopIntron pstops_with_intron_minus;
    ITERATE(CChainMembers, i, pointers) {
        SChainMember& mbr = **i;
        CGeneModel& algn = *mbr.m_align;
        TPstopIntron& pstops_with_intron = (algn.Strand() == ePlus) ? pstops_with_intron_plus : pstops_with_intron_minus;
        ITERATE(CCDSInfo::TPStops, s, algn.GetCdsInfo().PStops()) {
            if(s->m_status == CCDSInfo::eSelenocysteine || s->m_status == CCDSInfo::eGenomeNotCorrect) {
                left = min(left,s->GetFrom());
                right = max(right,s->GetTo());
                if(s->GetLength() == 3) {
                    pstops_with_intron.push_back(make_pair(*s,TSignedSeqRange(0,0)));
                } else {
                    for(int i = 1; i < (int)algn.Exons().size(); ++i) {
                        TSignedSeqRange intron(algn.Exons()[i-1].GetTo(),algn.Exons()[i].GetFrom());
                        pstops_with_intron.push_back(make_pair(*s,intron));
                    }
                }
            }
        }
    }
    uniq(pstops_with_intron_plus);
    uniq(pstops_with_intron_minus);

    ITERATE(CChainMembers, i, pointers) {
        SChainMember& mbr = **i;
        CGeneModel& algn = *mbr.m_align;
        if(algn.Limits().GetFrom() > right || algn.Limits().GetTo() < left)
            continue;
        if((algn.Type()&CGeneModel::eProt) && !algn.PStop())
            continue;
        if(algn.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;

        TPstopIntron& pstops_with_intron = (algn.Strand() == ePlus) ? pstops_with_intron_plus : pstops_with_intron_minus;
        if(pstops_with_intron.empty()) 
            continue;

        if(algn.Type()&CGeneModel::eProt) {
            CCDSInfo cds = algn.GetCdsInfo();
            CCDSInfo::TPStops pstops = cds.PStops();
            NON_CONST_ITERATE(CCDSInfo::TPStops, s, pstops) {
                ITERATE(TPstopIntron, si, pstops_with_intron) {
                    if(si->second.GetLength() == 1) {  // no split
                        if(si->first == *s)
                            *s = si->first;            // assigns status                         
                    } else {
                        for(int i = 1; i < (int)algn.Exons().size(); ++i) {
                            TSignedSeqRange intron(algn.Exons()[i-1].GetTo(),algn.Exons()[i].GetFrom());
                            if(si->second == intron && si->first == *s)
                                *s = si->first;        // assigns status
                        }
                    }
                }
            }
            cds.ClearPStops();
            ITERATE(CCDSInfo::TPStops, s, pstops)
                cds.AddPStop(*s);
            algn.SetCdsInfo(cds);
        } else if(algn.ReadingFrame().Empty()) {
            CCDSInfo cds;
            const CGeneModel::TExons& exons = algn.Exons(); 
            ITERATE(TPstopIntron, si, pstops_with_intron) {
                if(si->first.GetTo() < algn.Limits().GetFrom())
                    continue;
                if(si->first.GetFrom() > algn.Limits().GetTo())
                    break;
                for(int i = 0; i < (int)exons.size(); ++i) {
                    if(Include(exons[i].Limits(),si->first.GetFrom())) {
                        if(si->second.GetLength() == 1) {  // no split 
                            if(si->first.GetTo() <= exons[i].GetTo())
                                cds.AddPStop(si->first);
                        } else {
                            if(i < (int)exons.size()-1) {
                                TSignedSeqRange intron(exons[i].GetTo(),exons[i+1].GetFrom());
                                if(intron == si->second && si->first.GetTo() <= exons[i+1].GetTo())
                                    cds.AddPStop(si->first);
                            }
                        }
                    }
                }
            }
            if(cds.PStop())
                algn.SetCdsInfo(cds); 
        }            
    }
}

void CChainer::CChainerImpl::ScoreCdnas(CChainMembers& pointers)
{
    NON_CONST_ITERATE(CChainMembers, i, pointers) {
        SChainMember& mbr = **i;
        CGeneModel& algn = *mbr.m_align;

        if(algn.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;
        if((algn.Type() & CGeneModel::eProt)!=0 || algn.ConfirmedStart())
            continue;        

        m_gnomon->GetScore(algn);
        double ms = GoodCDNAScore(algn);
        RemovePoorCds(algn,ms);
        
        if(algn.Score() != BadScore())
            mbr.m_type = eCDS;
    }
}

void CChainer::CChainerImpl::Duplicate5pendsAndShortCDSes(CChainMembers& pointers)
{
    size_t initial_size = pointers.size();
    for(size_t i = 0; i < initial_size; ++i) {
        SChainMember& mbr = *pointers[i];
        CGeneModel& algn = *mbr.m_align;

        if(algn.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;

        if(mbr.m_type == eRightUTR)   // avoid copying UTR copies
            continue;

        if(algn.GetCdsInfo().ProtReadingFrame().Empty() && algn.Score() < 5*minscor.m_min) {
            for(int i = 0; i < (int)algn.GetEdgeReadingFrames()->size(); ++i) {
                const CCDSInfo& cds_info = (*algn.GetEdgeReadingFrames())[i];
                if(cds_info.ReadingFrame() != algn.ReadingFrame()) {
                    pointers.InsertMemberCopyWithCds(cds_info, &mbr);    //copy with CDS       
                }
            }

            if(algn.Score() != BadScore()) {
                pointers.InsertMemberCopyWithoutCds(&mbr);    //UTR copy        
            }
        }
    }
    

    initial_size = pointers.size();
    for(unsigned int i = 0; i < initial_size; ++i) {
        SChainMember& mbr = *pointers[i];
        CGeneModel& algn = *mbr.m_align;
        CCDSInfo& acdsinfo = const_cast<CCDSInfo&>(*mbr.m_cds_info);

        if(acdsinfo.HasStart()) {
            bool inf_5prime;
            if (algn.Strand()==ePlus) {
                inf_5prime = acdsinfo.MaxCdsLimits().GetFrom()==TSignedSeqRange::GetWholeFrom();
            } else {
                inf_5prime = acdsinfo.MaxCdsLimits().GetTo()==TSignedSeqRange::GetWholeTo();
            }
            if (inf_5prime) {
                CCDSInfo cdsinfo = acdsinfo;

                TSignedSeqPos start = (algn.Strand() == ePlus) ? acdsinfo.Start().GetFrom() : acdsinfo.Start().GetTo();            
                acdsinfo.Set5PrimeCdsLimit(start);
                mbr.m_restricted_to_start = true;

                if(algn.Strand() == ePlus) {
                    int full_rf_left = algn.FShiftedMove(algn.Limits().GetFrom(),(algn.FShiftedLen(algn.Limits().GetFrom(), cdsinfo.Start().GetFrom(), false)-1)%3);
                    cdsinfo.SetStart(TSignedSeqRange::GetEmpty());
                    cdsinfo.SetScore(cdsinfo.Score(),false);
                    cdsinfo.SetReadingFrame(TSignedSeqRange(full_rf_left,cdsinfo.ReadingFrame().GetTo()));
                } else {
                    int full_rf_right = algn.FShiftedMove(algn.Limits().GetTo(),-(algn.FShiftedLen(cdsinfo.Start().GetTo(),algn.Limits().GetTo(),false)-1)%3);
                    cdsinfo.SetStart(TSignedSeqRange::GetEmpty());
                    cdsinfo.SetScore(cdsinfo.Score(),false);
                    cdsinfo.SetReadingFrame(TSignedSeqRange(cdsinfo.ReadingFrame().GetFrom(),full_rf_right));
                }

                if(mbr.m_copy != 0) {
                    if(mbr.m_copy->front()->m_align->Strand() == algn.Strand()) {     // first copy is original alignment; for not oriented the second copy is reverse
                        if(mbr.m_copy->front()->m_cds_info->ReadingFrame() == cdsinfo.ReadingFrame())
                            continue;
                    } else if((*mbr.m_copy)[1]->m_cds_info->ReadingFrame() == cdsinfo.ReadingFrame()) {
                        continue;
                    }
                }

                pointers.InsertMemberCopyAndStoreCds(cdsinfo, &mbr);
            }
        }
    
    }
}

TInDels StrictlyContainedInDels(const TInDels& indels, const TSignedSeqRange& lim) {
    TInDels fs;
    ITERATE(TInDels, indl, indels) {
        if(indl->InDelEnd() > lim.GetFrom() && indl->Loc() <= lim.GetTo())
            fs.push_back(*indl);
    }
    return fs;
}

bool CChainer::CChainerImpl::CanIncludeJinI(const SChainMember& mi, const SChainMember& mj) {
    const CGeneModel& ai = *mi.m_align;
    const CGeneModel& aj = *mj.m_align;

    if(ai.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
        return false;
    
    bool jflex = false;
    TSignedSeqRange jlimits = aj.Limits();
    if(aj.Status()&CGeneModel::eLeftFlexible) {
        jlimits.SetFrom(jlimits.GetTo());
        jflex = true;
    }
    if(aj.Status()&CGeneModel::eRightFlexible) {
        jlimits.SetTo(jlimits.GetFrom());
        jflex = true;
    }

    if(aj.Strand() != ai.Strand() || !Include(ai.Limits(),jlimits))
        return false;

    if(mi.m_type != eCDS && mj.m_type  != mi.m_type)
        return false;    // avoid including UTR copy and avoid including CDS into UTR because that will change m_type

    const CCDSInfo& ai_cds_info = *mi.m_cds_info;
    TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();
    TSignedSeqRange ai_max_cds = ai_cds_info.MaxCdsLimits()&ai.Limits();

    const CCDSInfo& aj_cds_info = *mj.m_cds_info;
    TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();

    // UTR in CDS
    if(mi.m_type == eCDS && mj.m_type == eLeftUTR) {
        if(!jflex && jlimits.GetTo()-ai_max_cds.GetFrom() >= 5)                                                                     // normal UTR don't go into CDS > 5bp
            return false;
        else if(jflex && (aj.Status()&CGeneModel::ePolyA) && (!ai_cds_info.HasStop() || jlimits.GetTo()-ai_max_cds.GetFrom() >= 5)) // flex polyA needs stop and don't go into CDS > 5bp
            return false;
        else if(jflex && (aj.Status()&CGeneModel::eCap) && ai_cds_info.HasStop() && ai_max_cds.GetTo()-jlimits.GetTo() <= 5)         // flex cap is allowed almost up to 3' UTR to be awailable if start moves
            return false;
    }
    if(mi.m_type == eCDS && mj.m_type == eRightUTR) {
        if(!jflex && ai_max_cds.GetTo()-jlimits.GetFrom() >= 5)
            return false;
        else if(jflex && (aj.Status()&CGeneModel::ePolyA) && (!ai_cds_info.HasStop() || ai_max_cds.GetTo()-jlimits.GetFrom() >= 5))
            return false;
        else if(jflex && (aj.Status()&CGeneModel::eCap) && ai_cds_info.HasStop() && jlimits.GetFrom()-ai_max_cds.GetFrom() <= 5)
            return false;
    }

    if(aj.FrameShifts() != StrictlyContainedInDels(ai.FrameShifts(), aj.Limits()))    // not compatible frameshifts
        return false;

    if(mi.m_type == eCDS && mj.m_type == eCDS) { // CDS in CDS
        TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();
        if (!Include(max_cds_limits, ExtendedMaxCdsLimits(ai, ai_cds_info) + ExtendedMaxCdsLimits(aj, aj_cds_info)))
            return false;;
        if(!Include(ai_rf,aj_rf))
            return false;
        
        if(ai_rf.GetFrom() != aj_rf.GetFrom()) {
            TSignedSeqPos j_from = mi.m_align_map->MapOrigToEdited(aj_rf.GetFrom());
            if(j_from < 0)
                return false;
            TSignedSeqPos i_from = mi.m_align_map->MapOrigToEdited(ai_rf.GetFrom()); 
            if(abs(j_from-i_from)%3 != 0)
                return false;
        }
    }

    int iex = (int)ai.Exons().size();
    int jex = (int)aj.Exons().size();
    if(jex > iex)
        return false;
    if(iex > 1)                                                 // big alignment is spliced 
        return ai.HasCompatibleOverlap(aj, 1);                  // overlap 1 for flexible

    return true;
}

void CChainer::CChainerImpl::FindContainedAlignments(TContained& pointers) {

    set<int> left_exon_ends, right_exon_ends;
    ITERATE(TContained, ip, pointers) {
        const CGeneModel& algn = *(*ip)->m_align;
        for(int i = 1; i < (int)algn.Exons().size(); ++i) {
            if(algn.Exons()[i-1].m_ssplice && algn.Exons()[i].m_fsplice) {
                left_exon_ends.insert(algn.Exons()[i].GetFrom());
                right_exon_ends.insert(algn.Exons()[i-1].GetTo());
            }
        }
    }
    NON_CONST_ITERATE(TContained, ip, pointers) {
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

    int jfirst = 0;
    for(int i = 0; i < (int)pointers.size(); ++i) {
        SChainMember& mi = *pointers[i];
        CGeneModel& ai = *mi.m_align;
        const CCDSInfo& ai_cds_info = *mi.m_cds_info;

        // knockdown spliced notconsensus UTRs in reads
        if(mi.m_type != eCDS && ai.Exons().size() > 1) {
            if(ai.Status()&CGeneModel::eUnknownOrientation) {
                mi.m_not_for_chaining = true;
            } else {
                for(int i = 1; i < (int)ai.Exons().size() && !mi.m_not_for_chaining; ++i) {
                    
                    string ssplice = ai.Exons()[i-1].m_ssplice_sig;
                    string fsplice = ai.Exons()[i].m_fsplice_sig;
                    if(ssplice == "XX" || fsplice == "XX")
                        continue;
                    else if(ai.Strand() == ePlus && ((ssplice != "GT" && ssplice != "GC") || fsplice != "AG"))
                        mi.m_not_for_chaining = true;
                    else if(ai.Strand() == eMinus && (ssplice != "AG" || (fsplice != "GT" && fsplice != "GC")))
                        mi.m_not_for_chaining = true;
                    
                    /*
                    if(ai.Exons()[i-1].m_ssplice_sig == "XX" || ai.Exons()[i].m_fsplice_sig == "XX")
                        continue;
                    else if(ai.Strand() == ePlus && (ai.Exons()[i-1].m_ssplice_sig != "GT" || ai.Exons()[i].m_fsplice_sig != "AG"))
                        mi.m_not_for_chaining = true;
                    else if(ai.Strand() == eMinus && (ai.Exons()[i-1].m_ssplice_sig != "AG" || ai.Exons()[i].m_fsplice_sig != "GT"))
                        mi.m_not_for_chaining = true;
                    */
                }
            }
        }

        //don't use alignments intersection with frameshifts for hiding smaller alignments
        TSignedSeqRange intersect_with_fs;
        ITERATE(TInDels, indl, all_frameshifts) {
            if(indl->InDelEnd() < ai.Limits().GetFrom())
                continue;
            else if(indl->Loc() >  ai.Limits().GetTo()+1)
                break;
            else {
                ITERATE(CGeneModel::TExons, e, ai.Exons()) {
                    if(indl->IntersectingWith(e->GetFrom(), e->GetTo()))
                        intersect_with_fs += TSignedSeqRange(indl->Loc(), indl->InDelEnd());                    
                }
            }
        }

        if(pointers[jfirst]->m_align->Limits() != ai.Limits())
            jfirst = i;
        for(int j = jfirst; j < (int)pointers.size() && pointers[j]->m_align->Limits().GetFrom() <= ai.Limits().GetTo(); ++j) {
            if(i == j) {
                IncludeInContained(mi, mi);          // include self
                continue;
            }

            SChainMember& mj = *pointers[j];
            CGeneModel& aj = *mj.m_align;	 
            const CCDSInfo& aj_cds_info = *mj.m_cds_info;

            if(CanIncludeJinI(mi, mj)) 
                IncludeInContained(mi, mj);            
            else	 
                continue;	 
 	 
            if(mi.m_not_for_chaining	 
               || mj.m_not_for_chaining	 
               || mi.m_internal
               || mj.m_internal
               || mj.m_type != mi.m_type	 
               || ai.Limits() == aj.Limits()) {	 
                continue;	 
            }	 
 	 
             if(intersect_with_fs.NotEmpty() && !Include(aj.Limits(), intersect_with_fs))	 
                 continue;            	 
             if((aj.Status()&CGeneModel::ePolyA) != 0 || (aj.Status()&CGeneModel::eCap) != 0)	 
                 continue;                          	 
             if((aj.Type()&CGeneModel::eProt) != 0)                               // proteins (actually only gapped) should be directly available	 
                 continue;            	 
             if(mj.m_rlimb < ai.Limits().GetTo() || mj.m_llimb != mi.m_llimb)     // bigger alignment may interfere with splices	 
                 continue;            	 
             if(mi.m_type == eCDS && mj.m_type == eCDS && !Include(ai_cds_info.MaxCdsLimits(),aj_cds_info.MaxCdsLimits()))   // bigger alignment restricts the cds	 
                 continue;            	 
                 	 
             //             mj.m_not_for_chaining = true;
             mj.m_internal = true;              
        }
    }
}

#define NON_CDNA_INTRON_PENALTY 20

bool CChainer::CChainerImpl::LRCanChainItoJ(int& delta_cds, double& delta_num, double& delta_splice_num, SChainMember& mi, SChainMember& mj, TContained& contained, bool& not_sorted) {

    const CGeneModel& ai = *mi.m_align;
    const CGeneModel& aj = *mj.m_align;


    if(aj.Strand() != ai.Strand())
        return false;

    const CCDSInfo& ai_cds_info = *mi.m_cds_info;
    TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();
    bool ai_left_complete = ai.Strand() == ePlus ? ai_cds_info.HasStart() : ai_cds_info.HasStop();

    const CCDSInfo& aj_cds_info = *mj.m_cds_info;
    TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();
    bool aj_right_complete = aj.Strand() == ePlus ? aj_cds_info.HasStop() : aj_cds_info.HasStart();
 
    bool j_rflexible = aj.Status()&CGeneModel::eRightFlexible;
    bool i_lflexible = ai.Status()&CGeneModel::eLeftFlexible;
    switch(mi.m_type) {
    case eCDS: 
        if(mj.m_type == eRightUTR) 
            return false;
        else if(mj.m_type == eLeftUTR && (!ai_left_complete || (!j_rflexible && (aj.Limits()&ai_rf).GetLength() > 5)))
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
        else if(mj.m_type == eCDS && (!aj_right_complete || (!i_lflexible && (ai.Limits()&aj_rf).GetLength() > 5)))
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
        if(j_rflexible || i_lflexible)              // no intersecting limit for flexible
            break;
        if((ai.Limits() & aj.Limits()).GetLength() < intersect_limit) 
            return false;
        break;
    default:             // one or more introns in intersection 
        break;
    }

    TSignedSeqRange overlap = (ai.Limits() & aj.Limits());
    if(StrictlyContainedInDels(ai.FrameShifts(), overlap) !=  StrictlyContainedInDels(aj.FrameShifts(), overlap))   // incompatible frameshifts
        return false;
            
    int cds_overlap = 0;

    if(mi.m_type == eCDS && mj.m_type == eCDS) {
        int genome_overlap =  ai_rf.GetLength()+aj_rf.GetLength()-(ai_rf+aj_rf).GetLength();
        if(genome_overlap < 0)
            return false;

        TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();

        if (!Include(max_cds_limits, ExtendedMaxCdsLimits(ai, ai_cds_info) + ExtendedMaxCdsLimits(aj, aj_cds_info)))
            return false;

        if((Include(ai_rf,aj_rf) || Include(aj_rf,ai_rf)) && ai_rf.GetFrom() != aj_rf.GetFrom() && ai_rf.GetTo() != aj_rf.GetTo())
            return false;
        
        cds_overlap = mi.m_align_map->FShiftedLen(ai_rf&aj_rf,false);
        if(cds_overlap%3 != 0)
            return false;

        if(ai_cds_info.HasStart() && aj_cds_info.HasStart())
            cds_overlap += START_BONUS;

        if(has_rnaseq) {
            for(int i = 1; i < (int)ai.Exons().size(); ++i) {
                if(ai.Exons()[i-1].m_ssplice && ai.Exons()[i].m_fsplice) {
                    TSignedSeqRange intron(ai.Exons()[i-1].Limits().GetTo(),ai.Exons()[i].Limits().GetFrom());
                    if(Include(ai_rf,intron) && Include(aj_rf,intron) && mrna_count[intron]+est_count[intron]+rnaseq_count[intron] == 0) {
                        cds_overlap -= NON_CDNA_INTRON_PENALTY;
                    }
                }
            }
        }
    }

    delta_cds = mi.m_cds-cds_overlap;

    delta_num = 0;
    delta_splice_num = 0;
    if(delta_cds >= 0) {
        if(not_sorted)
            sort(contained.begin(),contained.end(),LeftOrderD());
        int first = 0;
        if(!j_rflexible && !i_lflexible)
            first = upper_bound(contained.begin(), contained.end(), &mj, LeftOrder())-contained.begin(); // first alignmnet contained in ai and outside aj
        if(not_sorted) {
            not_sorted = false;
            contained.back()->m_accumulated_num = contained.back()->m_align->Weight();
            contained.back()->m_accumulated_splice_num = contained.back()->m_splice_weight;
            for(int i = (int)contained.size()-2; i >= first; --i) {
                contained[i]->m_accumulated_num = contained[i]->m_align->Weight()+contained[i+1]->m_accumulated_num;
                contained[i]->m_accumulated_splice_num = contained[i]->m_splice_weight+contained[i+1]->m_accumulated_splice_num;
            }
        }

        delta_num = contained[first]->m_accumulated_num;
        delta_splice_num = contained[first]->m_accumulated_splice_num;
    }

    return true;
}


void CChainer::CChainerImpl::LRIinit(SChainMember& mi, const TContained& micontained) {
    const CCDSInfo& ai_cds_info = *mi.m_cds_info;
    TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();

    //    TContained micontained = mi.CollectContainedForMemeber();
    mi.m_num = 0;
    mi.m_splice_num = 0;
    for(auto p : micontained) {
        mi.m_num += p->m_align->Weight();
        mi.m_splice_num += p->m_splice_weight;
    }

    const CGeneModel& ai = *mi.m_align;
    mi.m_cds = mi.m_align_map->FShiftedLen(ai_rf,false);
    if(ai_cds_info.HasStart()) {
        mi.m_cds += START_BONUS;
        _ASSERT((ai.Strand() == ePlus && ai_cds_info.Start().GetFrom() == ai_cds_info.MaxCdsLimits().GetFrom()) || 
                (ai.Strand() == eMinus && ai_cds_info.Start().GetTo() == ai_cds_info.MaxCdsLimits().GetTo()));
    }

    if(has_rnaseq) {
        for(int i = 1; i < (int)ai.Exons().size(); ++i) {
            if(ai.Exons()[i-1].m_ssplice && ai.Exons()[i].m_fsplice) {
                TSignedSeqRange intron(ai.Exons()[i-1].Limits().GetTo(),ai.Exons()[i].Limits().GetFrom());
                if(Include(ai_rf,intron) && mrna_count[intron]+est_count[intron]+rnaseq_count[intron] == 0) {
                    mi.m_cds -= NON_CDNA_INTRON_PENALTY;
                }
            }
        }
    }
    
    mi.m_left_member = 0;
    mi.m_left_num = mi.m_num;
    mi.m_left_splice_num = mi.m_splice_num;
    mi.m_left_cds =  mi.m_cds;

    mi.m_gapped_connection = false;
    mi.m_fully_connected_to_part = -1;

    mi.m_excluded_readthrough = false;
    auto& align = *mi.m_align;
    int tgr = align.TrustedGroup();
    mi.m_left_trusted_group = tgr;	
    if(tgr != 0 && (mi.m_cds_info->Cds()&align.TrustedCds()).NotEmpty())
	mi.m_cds_from_trusted = true;
    else
	mi.m_cds_from_trusted = false;
    _ASSERT(tgr == 0 || (tgr > 0 && align.Strand() == ePlus) || (tgr < 0 && align.Strand() == eMinus));    
}

void CChainer::CChainerImpl::LeftRight(TContained& pointers)
{
    sort(pointers.begin(),pointers.end(),LeftOrderD());
    TIVec right_ends(pointers.size());
    for(int k = 0; k < (int)pointers.size(); ++k) {
        auto& kalign = *pointers[k]->m_align;
        int rend = kalign.Limits().GetTo();
        if(kalign.Status()&CGeneModel::eRightFlexible)
            rend = kalign.Limits().GetFrom();
        right_ends[k] = rend;
    }
    NON_CONST_ITERATE(TContained, i, pointers) {
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        TContained micontained = mi.CollectContainedForMemeber();
        LRIinit(mi, micontained);
        bool not_sorted = true;
        //        sort(micontained.begin(),micontained.end(),LeftOrderD());

        TIVec::iterator lb = lower_bound(right_ends.begin(),right_ends.end(),ai.Limits().GetFrom()-2*flex_len); // give some extra for flexible
        TContained::iterator jfirst = pointers.begin();
        if(lb != right_ends.end())
            jfirst = pointers.begin()+(lb-right_ends.begin()); // skip all on the left side
        for(TContained::iterator j = jfirst; j < i; ++j) {
            SChainMember& mj = **j;
            CGeneModel& aj = *mj.m_align;

            if(aj.Exons().back().m_fsplice_sig == "XX" || ai.Exons().front().m_ssplice_sig == "XX")  // don't extend first/last exon if gapfill
                continue;            

            if(aj.Limits().GetTo() < ai.Limits().GetFrom())    // skip not overlapping (may exist because of flex_len)
                continue;

            int delta_cds;
            double delta_num;
            double delta_splice_num;
            if(LRCanChainItoJ(delta_cds, delta_num, delta_splice_num, mi, mj, micontained, not_sorted)) {
                int newcds = mj.m_left_cds+delta_cds;
                double newnum = mj.m_left_num+delta_num;
                double newsplicenum = mj.m_left_splice_num+delta_splice_num;

		bool trust_compatible = true;
		if(mj.m_left_trusted_group != 0) {
		    int tgr = mj.m_left_trusted_group;
		    if(mi.m_left_trusted_group != 0 && mi.m_left_trusted_group != tgr) // direct connction of trusted
			trust_compatible = false;
		    else if(mj.m_type == eLeftUTR && mi.m_align->Limits().GetTo() > trusted_group_cds[tgr].GetTo()) // left trusted utr goes past cds
			trust_compatible = false;
		}
		bool better_connection = false;
                if(newcds != mi.m_left_cds) {
                    better_connection = (newcds > mi.m_left_cds);
                } else if(fabs(newsplicenum - mi.m_left_splice_num) > 0.001) {
                    better_connection = (newsplicenum > mi.m_left_splice_num);
                } else if(newnum > mi.m_left_num) {
                    better_connection = true;
                }

                if(better_connection) {
		    if(trust_compatible) {
			mi.m_left_cds = newcds;
			mi.m_left_splice_num = newsplicenum;
			mi.m_left_num = newnum;
			mi.m_left_member = &mj;
			_ASSERT(((ai.Status()&CGeneModel::eLeftFlexible) || aj.Limits().GetFrom() < ai.Limits().GetFrom()) 
				&& ((aj.Status()&CGeneModel::eRightFlexible) || aj.Limits().GetTo() < ai.Limits().GetTo()));
		    } else if (mi.m_type == eCDS && mj.m_cds_from_trusted) {
			mi.m_excluded_readthrough = true;
		    }
		}
            }    
        }
	if(mi.m_left_member != nullptr && mi.m_left_member->m_left_trusted_group != 0) { // propagate trusted group
	    mi.m_left_trusted_group = mi.m_left_member->m_left_trusted_group;
	    if(mi.m_type == eCDS)
		mi.m_cds_from_trusted = (mi.m_cds_from_trusted || mi.m_left_member->m_cds_from_trusted);
	}
#ifdef _DEBUG	
	int tgr = mi.m_left_trusted_group;
	int strand = ai.Strand();
	_ASSERT(tgr == 0 || (tgr > 0 && strand == ePlus) || (tgr < 0 && strand == eMinus));
#endif
    }
}

void CChainer::CChainerImpl::RightLeft(TContained& pointers)
{
    sort(pointers.begin(),pointers.end(),RightOrderD());
    TIVec left_ends(pointers.size());
    for(int k = 0; k < (int)pointers.size(); ++k) {
        auto& kalign = *pointers[k]->m_align;
        int lend = kalign.Limits().GetFrom();
        if(kalign.Status()&CGeneModel::eRightFlexible)
            lend = kalign.Limits().GetTo();
        left_ends[k] = lend;
    }
    NON_CONST_ITERATE(TContained, i, pointers) {
        SChainMember& mi = **i;
        CGeneModel& ai = *mi.m_align;
        const CCDSInfo& ai_cds_info = *mi.m_cds_info;
        TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();
        TSignedSeqRange ai_limits = ai.Limits();
        bool ai_right_complete = ai.Strand() == ePlus ? ai_cds_info.HasStop() : ai_cds_info.HasStart();
        mi.m_right_member = 0;
        mi.m_right_num = mi.m_num;
        mi.m_right_splice_num = mi.m_splice_num;
        mi.m_right_cds =  mi.m_cds;
	mi.m_right_trusted_group = ai.TrustedGroup();	
	if(ai.TrustedGroup() != 0 && (mi.m_cds_info->Cds()&ai.TrustedCds()).NotEmpty())
	    mi.m_cds_from_trusted = true;
	else
	    mi.m_cds_from_trusted = false;
        TContained micontained; //  = mi.CollectContainedForMemeber();
        bool not_sorted = true;
        //        sort(micontained.begin(),micontained.end(),RightOrderD());
        
        TIVec::iterator lb = lower_bound(left_ends.begin(),left_ends.end(),ai.Limits().GetTo()+2*flex_len,greater<int>()); // first potentially intersecting
        TContained::iterator jfirst = pointers.begin();
        if(lb != left_ends.end())
            jfirst = pointers.begin()+(lb-left_ends.begin()); // skip all on the right side
        for(TContained::iterator j = jfirst; j < i; ++j) {
            SChainMember& mj = **j;
            CGeneModel& aj = *mj.m_align;

            if(aj.Exons().front().m_ssplice_sig == "XX" || ai.Exons().back().m_fsplice_sig == "XX")  // don't extend first/last exon if gapfill
                continue;

            if(aj.Strand() != ai.Strand())
                continue;
            if(aj.Limits().GetFrom() > ai.Limits().GetTo())   // skip not overlapping (may exist because of flex_len)
                continue;

            const CCDSInfo& aj_cds_info = *mj.m_cds_info;
            TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();
            bool aj_left_complete = aj.Strand() == ePlus ? aj_cds_info.HasStart() : aj_cds_info.HasStop();

            bool j_lflexible = aj.Status()&CGeneModel::eLeftFlexible;
            bool i_rflexible = ai.Status()&CGeneModel::eRightFlexible;
            switch(mi.m_type)
            {
                case eCDS: 
                    if(mj.m_type == eLeftUTR) 
                        continue;
                    if(mj.m_type == eRightUTR && (!ai_right_complete || (!j_lflexible && (aj.Limits()&ai_rf).GetLength() > 5)))
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
                    if(mj.m_type == eCDS && (!aj_left_complete || (!i_rflexible && (ai.Limits()&aj_rf).GetLength() > 5)))
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
                    if(j_lflexible || i_rflexible)              // no intersecting limit for flexible
                        break;

                    int intersect = (ai_limits & aj.Limits()).GetLength(); 
                    if(intersect < intersect_limit) continue;
                    break;
                }
                default:             // one or more introns in intersection
                    break;
            }

            TSignedSeqRange overlap = (ai.Limits() & aj.Limits());
            if(StrictlyContainedInDels(ai.FrameShifts(), overlap) !=  StrictlyContainedInDels(aj.FrameShifts(), overlap))   // incompatible frameshifts
                continue;
            
            int cds_overlap = 0;

            if(mi.m_type == eCDS && mj.m_type == eCDS) {
                int genome_overlap =  ai_rf.GetLength()+aj_rf.GetLength()-(ai_rf+aj_rf).GetLength();
                if(genome_overlap < 0)
                    continue;

                TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();

                if (!Include(max_cds_limits, ExtendedMaxCdsLimits(ai, ai_cds_info) + ExtendedMaxCdsLimits(aj, aj_cds_info)))
                    continue;

                if((Include(ai_rf,aj_rf) || Include(aj_rf,ai_rf)) && ai_rf.GetFrom() != aj_rf.GetFrom() && ai_rf.GetTo() != aj_rf.GetTo())
                    continue;

                cds_overlap = mi.m_align_map->FShiftedLen(ai_rf&aj_rf,false);
                if(cds_overlap%3 != 0)
                    continue;

                if(ai_cds_info.HasStart() && aj_cds_info.HasStart())
                    cds_overlap += START_BONUS; 

                if(has_rnaseq) {
                    for(int i = 1; i < (int)ai.Exons().size(); ++i) {
                        if(ai.Exons()[i-1].m_ssplice && ai.Exons()[i].m_fsplice) {
                            TSignedSeqRange intron(ai.Exons()[i-1].Limits().GetTo(),ai.Exons()[i].Limits().GetFrom());
                            if(Include(ai_rf,intron) && Include(aj_rf,intron) && mrna_count[intron]+est_count[intron]+rnaseq_count[intron] == 0) {
                                cds_overlap -= NON_CDNA_INTRON_PENALTY;
                            }
                        }
                    }
                }
            }


            int delta_cds = mi.m_cds-cds_overlap;
            int newcds = mj.m_right_cds+delta_cds;
            if(newcds < mi.m_right_cds)
                continue;

            if(not_sorted) {
		micontained = mi.CollectContainedForMemeber();
                sort(micontained.begin(),micontained.end(),RightOrderD());
	    }
            int first = 0;
            if(!j_lflexible && !i_rflexible)
                first = upper_bound(micontained.begin(),micontained.end(),&mj,RightOrder())-micontained.begin(); // first alignment contained in ai and outside aj
            if(not_sorted) {
                not_sorted = false;
                micontained.back()->m_accumulated_num = micontained.back()->m_align->Weight();
                micontained.back()->m_accumulated_splice_num = micontained.back()->m_splice_weight;
                for(int i = (int)micontained.size()-2; i >= first; --i) {
                    micontained[i]->m_accumulated_num = micontained[i]->m_align->Weight()+micontained[i+1]->m_accumulated_num;
                    micontained[i]->m_accumulated_splice_num = micontained[i]->m_splice_weight+micontained[i+1]->m_accumulated_splice_num;
                }
            }

            double delta_num = micontained[first]->m_accumulated_num;
            double delta_splice_num = micontained[first]->m_accumulated_splice_num;

            double newnum = mj.m_right_num+delta_num;
            double newsplicenum = mj.m_right_splice_num+delta_splice_num;

	    bool trust_compatible = true;
	    if(mj.m_right_trusted_group != 0) {
		int tgr = mj.m_right_trusted_group;
		if(mi.m_right_trusted_group != 0 && mi.m_right_trusted_group != tgr) // direct connction of trusted
		    trust_compatible = false;
		else if(mj.m_type == eRightUTR && mi.m_align->Limits().GetFrom() < trusted_group_cds[tgr].GetFrom()) // right trusted utr goes past cds
		    trust_compatible = false;
	    }
            bool better_connection = false;
            if(newcds != mi.m_right_cds) {
                better_connection = (newcds > mi.m_right_cds);
            } else if(fabs(newsplicenum - mi.m_right_splice_num) > 0.001) {
                better_connection = (newsplicenum > mi.m_right_splice_num);
            } else if(newnum > mi.m_right_num) {
                better_connection = true;
            }

            if(better_connection) {
		if(trust_compatible) {
		    mi.m_right_cds = newcds;
		    mi.m_right_splice_num = newsplicenum;
		    mi.m_right_num = newnum;
		    mi.m_right_member = &mj;
		    _ASSERT(((aj.Status()&CGeneModel::eLeftFlexible) || aj.Limits().GetFrom() > ai.Limits().GetFrom()) 
			    && ((ai.Status()&CGeneModel::eRightFlexible) || aj.Limits().GetTo() > ai.Limits().GetTo()));
		} else if(mi.m_type == eCDS && mj.m_cds_from_trusted) {
		    mi.m_excluded_readthrough = true;
		}
            }    
        }
	if(mi.m_right_member != nullptr && mi.m_right_member->m_right_trusted_group != 0) { // propagate trusted group
	    mi.m_right_trusted_group = mi.m_right_member->m_right_trusted_group;
	    if(mi.m_type == eCDS)
		mi.m_cds_from_trusted = (mi.m_cds_from_trusted || mi.m_right_member->m_cds_from_trusted);
	}
#ifdef _DEBUG	
	int tgr = mi.m_right_trusted_group;
	int strand = ai.Strand();
	_ASSERT(tgr == 0 || (tgr > 0 && strand == ePlus) || (tgr < 0 && strand == eMinus));
#endif
    }
}

bool MemberIsCoding(const SChainMember* mp) {
    return (mp->m_cds_info->Score() != BadScore());
}

bool MemberIsMarkedForDeletion(const SChainMember* mp) {
    return mp->m_marked_for_deletion;
}

// returns essential members of the chain for debugging
string GetLinkedIdsForMember(const SChainMember& mi) {
    vector<const SChainMember*> mal;
    mal.push_back(&mi);
    for (SChainMember* left = mi.m_left_member; left != 0; left = left->m_left_member) {
        mal.push_back(left);
    }
    for (SChainMember* right = mi.m_right_member; right != 0; right = right->m_right_member) {
        mal.push_back(right);
    }
    sort(mal.begin(),mal.end(),GenomeOrderD());
    string note = to_string(mi.m_align->ID());  //+":"+to_string(mi.m_mem_id);;
    ITERATE(vector<const SChainMember*>, imal, mal) {
	string star;
	if((*imal)->m_excluded_readthrough)
	    star = "*";
        note = note+" "+star+to_string((*imal)->m_align->ID());  //+":"+to_string((*imal)->m_mem_id);
    }
    return note;
}

bool GoodSupportForIntrons(const CGeneModel& chain, const SMinScor& minscor, 
                                   map<TSignedSeqRange,int>& mrna_count, map<TSignedSeqRange,int>& est_count, map<TSignedSeqRange,int>& rnaseq_count) {
    bool good = true;
    for(int i = 1; i < (int)chain.Exons().size() && good; ++i) {
        if(chain.Exons()[i-1].m_ssplice && chain.Exons()[i].m_fsplice) {
            TSignedSeqRange intron(chain.Exons()[i-1].Limits().GetTo(),chain.Exons()[i].Limits().GetFrom());
            if(mrna_count[intron] < minscor.m_minsupport_mrna && mrna_count[intron]+est_count[intron] < minscor.m_minsupport && rnaseq_count[intron] < minscor.m_minsupport_rnaseq)
                good = false;
        }
    }
    
    return good;
}

void MarkUnwantedLowSupportIntrons(TContained& pointers, const SMinScor& minscor, 
                                   map<TSignedSeqRange,int>& mrna_count, map<TSignedSeqRange,int>& est_count, map<TSignedSeqRange,int>& rnaseq_count) {

    NON_CONST_ITERATE(TContained, i, pointers) 
        (*i)->m_marked_for_deletion = !GoodSupportForIntrons(*(*i)->m_align, minscor, mrna_count, est_count, rnaseq_count); 
}

struct GModelOrder
{
    GModelOrder(TOrigAligns& oa) : orig_aligns(oa) {}

    TOrigAligns& orig_aligns;

    bool operator()(const CGeneModel& a, const CGeneModel& b)
    {
        if(a.Limits() != b.Limits())
            return a.Limits() < b.Limits();
        // same limits
        unsigned aflex = a.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible);
        unsigned bflex = b.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible);
        if(aflex != bflex)
            return aflex < bflex;
        else
            return *orig_aligns[a.ID()]->GetTargetId() < *orig_aligns[ b.ID()]->GetTargetId(); // to make sort deterministic
    }
};

TGeneModelList CChainer::CChainerImpl::MakeChains(TGeneModelList& clust)
{
    CStopWatch sw;
    sw.Restart();

    if(clust.empty()) return TGeneModelList();

    clust.sort(GModelOrder(orig_aligns));

    {
        map<tuple<int, int>, TGeneModelList::iterator> special_aligns; // [left/right flex|cap/polya, position] 
        //all known flexible
        for(TGeneModelList::iterator it = clust.begin(); it != clust.end(); ++it) {
            if(it->Status()&CGeneModel::eLeftFlexible) {
                int status = it->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eCap|CGeneModel::ePolyA);
                special_aligns.emplace(make_tuple(status, it->Limits().GetTo()), it);
            }
            if(it->Status()&CGeneModel::eRightFlexible) {
                int status = it->Status()&(CGeneModel::eRightFlexible|CGeneModel::eCap|CGeneModel::ePolyA);
                special_aligns.emplace(make_tuple(status, it->Limits().GetFrom()), it);
            }
        }
        //make flexible from normal cap/polya   
        TSignedSeqPos contig_len = (TSignedSeqPos)m_gnomon->GetSeq().size();
        int spec_extend = SPECIAL_ALIGN_LEN-1;
        for(TGeneModelList::iterator it = clust.begin(); it != clust.end(); ++it) {
            if(it->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
                continue;

            if(it->Status()&CGeneModel::eCap) {
                it->Status() &= ~CGeneModel::eCap;
                CGeneModel galign(it->Strand(), it->ID(), CGeneModel::eSR);
                galign.SetWeight(it->Weight());

                int pos;
                int status = CGeneModel::eCap;
                if(it->Strand() == ePlus) {
                    pos = it->Limits().GetFrom();
                    galign.AddExon(TSignedSeqRange(pos, pos+spec_extend));
                    status |= CGeneModel::eRightFlexible;
                } else {
                    pos = it->Limits().GetTo();
                    galign.AddExon(TSignedSeqRange(pos-spec_extend, pos));
                    status |= CGeneModel::eLeftFlexible;
                }
                if(galign.Limits().GetFrom() >= 0 && galign.Limits().GetTo() < contig_len) {
                    galign.Status() |= status;
                    clust.push_front(galign);
                    auto rslt = special_aligns.emplace(make_tuple(status, pos), clust.begin());
                    if(!rslt.second) {  //this position already exists
                        auto ialign = rslt.first->second;
                        ialign->SetWeight(ialign->Weight()+galign.Weight());
                        clust.pop_front();
                    }
                }
            }
            if(it->Status()&CGeneModel::ePolyA) {
                it->Status() &= ~CGeneModel::ePolyA;
                CGeneModel galign(it->Strand(), it->ID(), CGeneModel::eSR);
                galign.SetWeight(it->Weight());

                int pos;
                int status = CGeneModel::ePolyA;
                if(it->Strand() == eMinus) {
                    pos = it->Limits().GetFrom();
                    galign.AddExon(TSignedSeqRange(pos, pos+spec_extend));
                    status |= CGeneModel::eRightFlexible;
                } else {
                    pos = it->Limits().GetTo();
                    galign.AddExon(TSignedSeqRange(pos-spec_extend, pos));
                    status |= CGeneModel::eLeftFlexible;
                }
                if(galign.Limits().GetFrom() >= 0 && galign.Limits().GetTo() < contig_len) {
                    galign.Status() |= status;
                    clust.push_front(galign);
                    auto rslt = special_aligns.emplace(make_tuple(status, pos), clust.begin());
                    if(!rslt.second) {  //this position already exists
                        auto ialign = rslt.first->second;
                        ialign->SetWeight(ialign->Weight()+galign.Weight());
                        clust.pop_front();
                    }
                }
            }
        }

        //remove below threshold and crossing contig boundaries
        for(auto& sa : special_aligns) {
            auto ialign = sa.second;
            double min_pos_weight = ((ialign->Status()&CGeneModel::eCap) ? min_cap_weight : min_polya_weight);
            if(ialign->Limits().GetFrom() < 0 || ialign->Limits().GetTo() >= contig_len || ialign->Weight() < min_pos_weight)
                clust.erase(ialign);            
        }

        clust.sort(GModelOrder(orig_aligns));
    }

    confirmed_ends.clear();
    all_frameshifts.clear();
    ITERATE (TGeneModelList, it, clust) {
        const CGeneModel& align = *it;
        if(use_confirmed_ends) {
            if(align.Status()&CGeneModel::eLeftConfirmed) {
                auto rslt = confirmed_ends.emplace(align.Exons().front().GetTo(), align.Exons().front().GetFrom());
                if(!rslt.second)
                    rslt.first->second = min(rslt.first->second, align.Exons().front().GetFrom());
            } 
            if(align.Status()&CGeneModel::eRightConfirmed) {
                auto rslt = confirmed_ends.emplace(align.Exons().back().GetFrom(), align.Exons().back().GetTo());
                if(!rslt.second)
                    rslt.first->second = max(rslt.first->second, align.Exons().back().GetTo());
            }
        }
        all_frameshifts.insert(all_frameshifts.end(), align.FrameShifts().begin(), align.FrameShifts().end());
        for(int i = 1; i < (int)align.Exons().size(); ++i) {
            if(align.Exons()[i-1].m_ssplice && align.Exons()[i].m_fsplice) {
                TSignedSeqRange intron(align.Exons()[i-1].Limits().GetTo(),align.Exons()[i].Limits().GetFrom());

                if((align.Status()&CGeneModel::eUnknownOrientation) == 0) {
                    if(align.Strand() == ePlus)
                        oriented_introns_plus.insert(intron);
                    else
                        oriented_introns_minus.insert(intron); 
                }

                if(align.Type() == CGeneModel::emRNA)
                    mrna_count[intron] += align.Weight();
                else if(align.Type() == CGeneModel::eEST)
                    est_count[intron] += align.Weight();
                else if(align.Type() == CGeneModel::eSR)
                    rnaseq_count[intron] += align.Weight();
            }
        }
    }

    has_rnaseq = !rnaseq_count.empty();
    sort(all_frameshifts.begin(),all_frameshifts.end());
    if(!all_frameshifts.empty())
        uniq(all_frameshifts);

    flex_len = 0;
    NON_CONST_ITERATE (TGeneModelList, it, clust) {
        CGeneModel& align = *it;
        if(align.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            flex_len = max(flex_len, align.Limits().GetLength()); 
               
        if(align.Status()&CGeneModel::eUnknownOrientation) {
            int pluses = 0;
            int minuses = 0;
            for(int i = 1; i < (int)align.Exons().size(); ++i) {
                if(align.Exons()[i-1].m_ssplice && align.Exons()[i].m_fsplice) {
                    TSignedSeqRange intron(align.Exons()[i-1].Limits().GetTo(),align.Exons()[i].Limits().GetFrom());
                    if(oriented_introns_plus.find(intron) != oriented_introns_plus.end())
                        ++pluses;
                    if(oriented_introns_minus.find(intron) != oriented_introns_minus.end())
                        ++minuses;
                }
            }
            if(pluses > 0 && minuses == 0) {
                align.Status() ^= CGeneModel::eUnknownOrientation;
                if(align.Strand() == eMinus)
                    align.ReverseComplementModel();
            } else if(minuses > 0 && pluses == 0) {
                align.Status() ^= CGeneModel::eUnknownOrientation;
                if(align.Strand() == ePlus)
                    align.ReverseComplementModel();
            }
            align.Status() &= ~CGeneModel::eReversed;
        }
    }

    // some alignmets were divided to parts
    for(CGeneModel& align : clust)
	align.SetTrustedCds(align.TrustedCds()&align.Limits());
    
    CChainMembers allpointers(clust, orig_aligns, unmodified_aligns);

    DuplicateNotOriented(allpointers, clust);
    ReplicatePStops(allpointers);
    ScoreCdnas(allpointers);
    Duplicate5pendsAndShortCDSes(allpointers);
    DuplicateUTRs(allpointers);
    CalculateSpliceWeights(allpointers);
    LOG_POST("Create pointers in " << sw.Elapsed());
    sw.Restart();

    FindContainedAlignments(allpointers);
    LOG_POST("Find contained in " << sw.Elapsed());
    sw.Restart();

    TContained pointers;
    ITERATE(TContained, ip, allpointers) {
        _ASSERT((*ip)->m_orig_align);
        if(!(*ip)->m_not_for_chaining)
            pointers.push_back(*ip);
    }

    TContained coding_pointers;
    ITERATE(CChainMembers, i, pointers) {
        if(MemberIsCoding(*i)) 
            coding_pointers.push_back(*i); 
    }

    LeftRight(coding_pointers);
    RightLeft(coding_pointers);

    TChainList tmp_chains;

    array<map<TSignedSeqPos,TSignedSeqRange>, 2> coding_right_splices;
    array<map<TSignedSeqPos,TSignedSeqRange>, 2> coding_left_splices;
    array<set<TSignedSeqRange>, 2> coding_introns;

    NON_CONST_ITERATE(TContained, i, coding_pointers) {
        SChainMember& mi = **i;
        mi.m_cds = mi.m_left_cds+mi.m_right_cds-mi.m_cds;
        mi.m_splice_num = mi.m_left_splice_num+mi.m_right_splice_num-mi.m_splice_num;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
    }
    sort(coding_pointers.begin(),coding_pointers.end(),CdsNumOrder());
    NON_CONST_ITERATE(TContained, i, coding_pointers) {
        SChainMember& mi = **i;
	if(mi.m_left_trusted_group != 0 && mi.m_right_trusted_group !=0 && mi.m_left_trusted_group != mi.m_right_trusted_group) // connects different trusted
	    continue;
        if(mi.m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;
        if(mi.m_included || mi.m_postponed || mi.m_internal)
            continue;

        CChain chain(mi, false);
        TSignedSeqRange i_rf = chain.ReadingFrame();

        m_gnomon->GetScore(chain, false, false, true);  // max_cds extended

        if(chain.Score() == BadScore() || chain.PStop(false))
            continue;

        int cdslen = chain.FShiftedLen(chain.GetCdsInfo().Cds(),true);
        if(chain.GetCdsInfo().ProtReadingFrame().Empty() && 
           (cdslen < minscor.m_minlen || (chain.Score() < 2*minscor.m_min && cdslen <  2*minscor.m_cds_len)))
            continue;
        
        TSignedSeqRange real_cds = chain.MaxCdsLimits();       //chain.RealCdsLimits();
        for(int i = 1; i < (int)chain.Exons().size(); ++ i) {
            TSignedSeqPos donor = chain.Exons()[i-1].GetTo();
            bool coding_donor = Include(real_cds, donor);
            TSignedSeqPos acceptor = chain.Exons()[i].GetFrom();
            bool coding_acceptor = Include(real_cds, acceptor);
            if(coding_donor)
                coding_right_splices[chain.Strand()][donor].CombineWith(real_cds);
            if(coding_acceptor)
                coding_left_splices[chain.Strand()][acceptor].CombineWith(real_cds);
            if(coding_donor && coding_acceptor)
                coding_introns[chain.Strand()].emplace(donor, acceptor);
        }        

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

        mi.MarkUnwantedCopiesForChain(chain.RealCdsLimits());
    } 

    for(auto ip : pointers) {        
        if(ip->m_align->Type()&CGeneModel::eSR || ip->m_marked_for_deletion)
            continue;
        
        TSignedSeqRange cds = ip->m_cds_info->Cds();
        int strand = ip->m_align->Strand();
        auto& crs = coding_right_splices[strand];
        auto& cls = coding_left_splices[strand];
        for(int i = 1; i < (int)ip->m_align->Exons().size(); ++ i) {
            TSignedSeqPos rsplice = ip->m_align->Exons()[i-1].GetTo();
            auto rslt = crs.find(rsplice);
            if(rslt != crs.end() && !cds.IntersectingWith(rslt->second)) {
                ip->m_marked_for_deletion = true;
                break;
            }
            TSignedSeqPos lsplice = ip->m_align->Exons()[i].GetFrom();
            rslt = cls.find(lsplice);
            if(rslt != cls.end() && !cds.IntersectingWith(rslt->second)) {
                ip->m_marked_for_deletion = true;
                break;
            }
        }

        if(!ip->m_marked_for_deletion) {
            auto& cdi = coding_introns[strand];
            for(auto& exon :ip->m_align->Exons()) {
                if(Include(cds, exon.Limits())) // coding exon  
                    continue;
                for(auto intronp = cdi.upper_bound(exon.Limits()); intronp != cdi.end() && intronp->GetFrom() < exon.GetTo(); ++intronp) {
                    if(Include(exon.Limits(), *intronp) && !Include(cds, *intronp)) {
                        ip->m_marked_for_deletion = true;
                        break;
                    }
                }
                if(ip->m_marked_for_deletion)
                    break;
            }        
        }
    }
   
    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsMarkedForDeletion),pointers.end());  // wrong orientaition/UTR/frames are removed
    LOG_POST("Find coding in " << sw.Elapsed());
    sw.Restart();

    set<TSignedSeqRange> introns;
    set<TSignedSeqRange> est_introns;
    for(auto p : pointers) {
        auto& exons = p->m_align->Exons();
        bool est = p->m_align->Type() == CGeneModel::eEST;
        for(unsigned i = 1; i < exons.size(); ++i) {
            if(!exons[i-1].m_ssplice || !exons[i].m_fsplice)
                continue;
            introns.emplace(exons[i-1].GetTo(), exons[i].GetFrom());
            if(est)
                est_introns.emplace(exons[i-1].GetTo(), exons[i].GetFrom());
        }
    }
    bool enough_est = !introns.empty() && est_introns.size() > longreadsthreshold/100*introns.size();

    cerr << "Introns: " << introns.size() << " " << est_introns.size() << " " << enough_est << endl;

    int old_oep = intersect_limit;
    if(enough_est) {
        intersect_limit = 10000;
        pointers.erase(std::remove_if(pointers.begin(),pointers.end(),[](SChainMember* p){ 
                                          if(p->m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
                                              return false;
                                          if(p->m_align->Exons().size() != 1 || p->m_type == eCDS)
                                              return false;
                                          if(p->m_copy != nullptr) {
                                              for(SChainMember* cp : *p->m_copy) {
                                                  if(cp->m_cds_info->MaxCdsLimits() == TSignedSeqRange::GetWhole()) // through CDS
                                                      //                                                  if(Include(cp->m_cds_info->MaxCdsLimits(), cp->m_align->Limits())) // through CDS
                                                      return false;
                                              }
                                          }
                                          return true; }), pointers.end());
    }

    LeftRight(pointers);
    RightLeft(pointers);
    NON_CONST_ITERATE(TContained, i, pointers) {
        SChainMember& mi = **i;
        mi.m_included = false;
        mi.m_postponed = false;
        mi.m_cds = mi.m_left_cds+mi.m_right_cds-mi.m_cds;
        mi.m_splice_num = mi.m_left_splice_num+mi.m_right_splice_num-mi.m_splice_num;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
    }

    sort(pointers.begin(),pointers.end(),CdsNumOrder());

    NON_CONST_ITERATE(TContained, i, pointers) {
        SChainMember& mi = **i;
	if(mi.m_left_trusted_group != 0 && mi.m_right_trusted_group && mi.m_left_trusted_group != mi.m_right_trusted_group) // connects different trusted
	    continue;
        if(mi.m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;
        if(mi.m_included || mi.m_postponed || mi.m_internal)
            continue;

        // simple chain and rough check
        CChain chain(mi, false);
        bool has_trusted = chain.HasTrustedEvidence();
        {
            if(!chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns))
                m_gnomon->GetScore(chain);

            if(!has_trusted)
                RemovePoorCds(chain, GoodCDNAScore(chain, true));
            if(chain.Score() == BadScore() || (!has_trusted && chain.RealCdsLen() < minscor.m_minlen)) {
                mi.MarkPostponedForChain();
                continue;
            }
        }
        chain.AddAllMembersAndCoverage(mi);
        if(!chain.SetConfirmedEnds(*m_gnomon, confirmed_ends))
            continue;

        chain.RemoveFshiftsFromUTRs();
        const CResidueVec& contig = m_gnomon->GetSeq();
        // alignments clipped below might not be in any chain; clipping may produce redundant chains
        chain.ClipToCap(min_cap_blob, max_dist, min_flank_exon, secondary_peak, false);
        chain.ClipToPolyA(contig, min_polya_blob, max_dist, min_flank_exon, secondary_peak, tertiary_peak, tertiary_peak_coverage, false);
        chain.ClipLowCoverageUTR(minscor.m_utr_clip_threshold, false);
        if(!chain.SetConfirmedEnds(*m_gnomon, confirmed_ends))
            continue;

        m_gnomon->GetScore(chain, !no5pextension); // this will return CDS to best/longest depending on no5pextension
        chain.CheckSecondaryCapPolyAEnds();
        if(!has_trusted)
            RemovePoorCds(chain, GoodCDNAScore(chain));
        if(chain.Score() != BadScore() && (has_trusted || chain.RealCdsLen() >= minscor.m_minlen)) {
            mi.MarkIncludedForChain();

#ifdef _DEBUG 
            chain.AddComment("Link1 "+GetLinkedIdsForMember(mi));
#endif    

            chain.CalculateSupportAndWeightFromMembers(false);
            chain.CalculateDropLimits();
            tmp_chains.push_back(chain);
            _ASSERT( chain.FShiftedLen(chain.GetCdsInfo().Start()+chain.ReadingFrame()+chain.GetCdsInfo().Stop(), false)%3==0 );
        }
    }
    LOG_POST("Link1 chains in " << sw.Elapsed());
    sw.Restart();

    TGeneModelList unma_aligns;
    CChainMembers unma_members;
    CreateChainsForPartialProteins(tmp_chains, pointers, unma_aligns, unma_members);
    LOG_POST("Chains for partial proteins in " << sw.Elapsed());
    sw.Restart();

    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsCoding),pointers.end());  // only noncoding left

    MarkUnwantedLowSupportIntrons(pointers, minscor, mrna_count, est_count, rnaseq_count);
    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsMarkedForDeletion),pointers.end());  // low support introns removed

    // convert all flexible to left UTRs; copy contained flexible from right UTRs to left UTRs; remove right UTRs 
    for(auto i : allpointers) {
        SChainMember& mi = *i;
        if(mi.m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible)) {
            mi.m_type = eLeftUTR;
        } else if(mi.m_type == eLeftUTR) {
            if(mi.m_copy != nullptr) {
                for(auto j : *mi.m_copy) {
                    if(j->m_type == eRightUTR && j->m_align->Strand() == mi.m_align->Strand()) {
                        for(auto jc : *j->m_contained) {
                            if(jc->m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
                                mi.m_contained->push_back(jc);
                        }
                    }
                }
            }
        }
    }
    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),[](SChainMember* p){ return p->m_type == eRightUTR; }), pointers.end());

    if(enough_est) {
        // remove notspliced alignments
        pointers.erase(std::remove_if(pointers.begin(),pointers.end(),[](SChainMember* p){ 
                  return p->m_align->Exons().size() == 1 && !(p->m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible)); }), pointers.end());

        // remove alignments with notspliced inrons
        array<set<TSignedSeqRange>, 2> non_coding_introns;
        for(auto p : pointers) {
            auto& exons = p->m_align->Exons();
            for(unsigned i = 1; i < exons.size(); ++i) {
                TSignedSeqRange intron(exons[i-1].GetTo(), exons[i].GetFrom());
                non_coding_introns[p->m_align->Strand()].insert(intron);
            }
        }
        for(auto p : pointers) {
            int strand = p->m_align->Strand();
            auto& ncdi = non_coding_introns[strand];
            for(auto& exon : p->m_align->Exons()) {
                for(auto intronp = ncdi.upper_bound(exon.Limits()); intronp != ncdi.end() && intronp->GetFrom() < exon.GetTo(); ++intronp) {
                    if(Include(exon.Limits(), *intronp)) {
                        p->m_marked_for_deletion = true;
                        break;
                    }
                }
                if(p->m_marked_for_deletion)
                    break;
            }
        }
        pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsMarkedForDeletion),pointers.end());
    }

    LeftRight(pointers);
    RightLeft(pointers);

    ITERATE(TContained, i, pointers) {
        SChainMember& mi = **i;
        mi.m_splice_num = mi.m_left_splice_num+mi.m_right_splice_num-mi.m_splice_num;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
        _ASSERT(mi.m_cds == 0);
    }

    sort(pointers.begin(),pointers.end(),CdsNumOrder());

    NON_CONST_ITERATE(TContained, i, pointers) {
        SChainMember& mi = **i;
	if(mi.m_left_trusted_group != 0 && mi.m_right_trusted_group && mi.m_left_trusted_group != mi.m_right_trusted_group) // connects different trusted
	    continue;
        if(mi.m_included || mi.m_internal)
            continue;

        if(mi.m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;

        CChain chain(mi, false);
        chain.AddAllMembersAndCoverage(mi);
        if(!chain.SetConfirmedEnds(*m_gnomon, confirmed_ends))
            continue;

        chain.RemoveFshiftsFromUTRs();
        mi.MarkIncludedForChain();
        const CResidueVec& contig = m_gnomon->GetSeq();
        chain.ClipToCap(min_cap_blob, max_dist, min_flank_exon, secondary_peak, false);
        chain.ClipToPolyA(contig, min_polya_blob, max_dist, min_flank_exon, secondary_peak, tertiary_peak, tertiary_peak_coverage, false);
        chain.ClipLowCoverageUTR(minscor.m_utr_clip_threshold, false);
        if(!chain.SetConfirmedEnds(*m_gnomon, confirmed_ends))
            continue;
        if(chain.Continuous() && chain.Exons().size() > 1) {
#ifdef _DEBUG 
            chain.AddComment("Link2  "+GetLinkedIdsForMember(mi));
#endif    
            chain.CalculateSupportAndWeightFromMembers(false);
            chain.CalculateDropLimits();
            tmp_chains.push_back(chain);
        }
    }
    LOG_POST("Link2 chains in " << sw.Elapsed());
    sw.Restart();

    NON_CONST_ITERATE(TChainList, it, tmp_chains) {
        CChain& chain = *it;
        chain.SetID(m_idnext);
        chain.SetGeneID(m_idnext);
        m_idnext += m_idinc;
    }

    CombineCompatibleChains(tmp_chains);
    SetFlagsForChains(tmp_chains);

    intersect_limit = old_oep;

    list<CGene> genes = FindGenes(tmp_chains);  // assigns geneid, rank, skip, nested

    if(genes.size() > 1) {
        TrimAlignmentsIncludedInDifferentGenes(genes);
        CombineCompatibleChains(tmp_chains);
        SetFlagsForChains(tmp_chains);
    }

    if(genes.size() > 1)
        FindGenes(tmp_chains);                      // redo genes after trim
    
    TGeneModelList chains;
    NON_CONST_ITERATE(TChainList, it, tmp_chains) {
        it->RestoreTrimmedEnds(trim);
        chains.push_back(*it);
    }
    LOG_POST("Find genes in " << sw.Elapsed());
    sw.Restart();

    enum { eFirstPeak = 1, eSecondPeak = 2, eThirdPeak = 4, eAs = 8};
    map<tuple<int, int, int>, int> cap_polya_info; // [cap/polya strand position]
    const CResidueVec& contig = m_gnomon->GetSeq();
    for(auto& chain : tmp_chains) {
        if(chain.Status()&CGeneModel::eSkipped)
            continue;
        if(chain.Status()&CGeneModel::eCap) {
            for(int i = 0; i < (int)chain.m_cap_peaks.size(); ++i) {
                int pos = chain.m_cap_peaks[i];
                if(pos >= 0) 
                    cap_polya_info[make_tuple(CGeneModel::eCap, chain.Strand(), pos)] |= (1 << i);
            }
        }
        if(chain.Status()&CGeneModel::ePolyA) {
            for(int i = 0; i < (int)chain.m_polya_peaks.size(); ++i) {
                int pos = chain.m_polya_peaks[i];
                if(pos >= 0) {
                    cap_polya_info[make_tuple(CGeneModel::ePolyA, chain.Strand(), pos)] |= (1 << i);
                    if(chain.ValidPolyA(pos, contig).second)
                        cap_polya_info[make_tuple(CGeneModel::ePolyA, chain.Strand(), pos)] |= eAs;
                }
            }
        }
    }
    for(auto& info : cap_polya_info) {
        string determinant = get<0>(info.first) == CGeneModel::eCap ? "Cap" : "PolyA";
        char strand = get<1>(info.first) == ePlus ? '+' : '-';
        int pos = m_edited_contig_map.MapEditedToOrig(get<2>(info.first))+m_limits.GetFrom()+1;
        cerr << m_contig_acc << ' ' << determinant << ' ' << strand << ' ' << pos << ' ';
        if(info.second&eFirstPeak)
            cerr << ":FirstPeak";
        if(info.second&eSecondPeak)
            cerr << ":SecondPeak";
        if(info.second&eThirdPeak)
            cerr << ":ThirdPeak";
        if(info.second&eAs)
            cerr << ":As";
        cerr << ":\n";
    }
    LOG_POST("Cap/polyA info in " << sw.Elapsed());
    sw.Restart();

    return chains;
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

// fills fully or partially protein gaps; will not create new gaps
SChainMember* CChainer::CChainerImpl::FindOptimalChainForProtein(TContained& pointers, vector<CGeneModel*>& parts, CGeneModel& palign) {
            
    TIVec right_ends(pointers.size());
    vector<SChainMember> no_gap_members(pointers.size());   // temporary helper chain members; connected to the LAST part without gap; will be used for gap filling optimisation    
    for(int k = 0; k < (int)pointers.size(); ++k) {
        SChainMember& mi = *pointers[k];
        right_ends[k] = mi.m_align->Limits().GetTo();
        no_gap_members[k] = mi;
    }

    SChainMember* best_right = 0;

    int first_member = (int)pointers.size()-1;
    int leftpos = palign.Limits().GetFrom();
    for(int i = (int)pointers.size()-1; i >= 0; --i) {
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

    int fully_connected_right = 0;     // rightmost point already connected to all parts    

    for(int i = first_member; i <= last_member; ++i) {
        SChainMember& mi = *pointers[i];                   // best connection maybe gapped
        SChainMember& mi_no_gap = no_gap_members[i];       // best not gapped connection (if any)
        CGeneModel& ai = *mi.m_align;
        TContained micontained = mi.CollectContainedForMemeber();
        LRIinit(mi, micontained);
        mi_no_gap = mi;

        if(ai.Strand() != palign.Strand())
            continue;

        int part_to_connect =  (int)parts.size()-1;
        while(part_to_connect >= 0 && ai.Limits().GetFrom() <= parts[part_to_connect]->Limits().GetFrom())
            --part_to_connect;

        if(fully_connected_right > 0 && ai.Limits().GetFrom() > fully_connected_right)    // can't possibly be connected
            continue;
               
        bool not_sorted = true;

        bool compatible_with_included_parts = true;
        int last_included_part = -1;
        bool includes_first_part = false;
        for(int p = part_to_connect+1; p < (int)parts.size(); ++p) {
            if(Include(ai.Limits(),parts[p]->Limits())) {
                TSignedSeqRange ai_rf = mi.m_cds_info->ReadingFrame();
                TSignedSeqRange aj_rf = parts[p]->GetCdsInfo().ReadingFrame();
                TSignedSeqRange ai_cds = mi.m_cds_info->Cds();
                TSignedSeqRange aj_cds = parts[p]->GetCdsInfo().Cds();
                bool compatible = (parts[p]->HasCompatibleOverlap(ai) && Include(ai_rf,aj_rf) && mi.m_align_map->FShiftedLen(ai_cds.GetFrom(),aj_cds.GetFrom(),false)%3==1);
                bool samestop = (parts[p]->GetCdsInfo().HasStop() ==  mi.m_cds_info->HasStop() && (!parts[p]->GetCdsInfo().HasStop() || parts[p]->GetCdsInfo().Stop() == mi.m_cds_info->Stop()));
                bool samefshifts = (parts[p]->FrameShifts() == StrictlyContainedInDels(ai.FrameShifts(), parts[p]->Limits()));
                if(compatible && samestop && samefshifts) {
                    last_included_part = p;
                    if(p == 0)
                        includes_first_part = true;
                } else {
                    compatible_with_included_parts = false;
                    break;
                }
            } else if(ai.Limits().IntersectingWith(parts[p]->Limits())) {
                TSignedSeqRange overlap = (ai.Limits() & parts[p]->Limits());
                if(!parts[p]->HasCompatibleOverlap(ai) || StrictlyContainedInDels(ai.FrameShifts(), overlap) !=  StrictlyContainedInDels(parts[p]->FrameShifts(), overlap)) {
                    compatible_with_included_parts = false;
                    break;
                }
            } else {
                break;
            }
        }

        if(!compatible_with_included_parts)
            continue;

        _ASSERT(part_to_connect < 0 || part_to_connect == (int)parts.size()-1 || mi.m_type == eCDS);   // coding if between parts

        if(includes_first_part) {
            mi.m_fully_connected_to_part = last_included_part;
            mi_no_gap.m_fully_connected_to_part = last_included_part;
        }

        TIVec::iterator lb = lower_bound(right_ends.begin(),right_ends.end(),(part_to_connect >= 0 ? parts[part_to_connect]->Limits().GetTo() : ai.Limits().GetFrom()));
        int jfirst = 0;
        if(lb != right_ends.end())
            jfirst = (int)(lb-right_ends.begin()); // skip all on the left side

        for(int j = jfirst; j < i; ++j) {
            SChainMember& mj = *pointers[j];                   // best connection maybe gapped
            if(part_to_connect >= 0 && mj.m_fully_connected_to_part < part_to_connect)   // alignmnet is not connected to all previous parts
                continue;
            CGeneModel& aj = *mj.m_align;
            if( ai.Strand() != aj.Strand())
                continue;

            SChainMember& mj_no_gap = no_gap_members[j];       // best not gapped connection (if any)

            if(ai.Limits().GetFrom() > aj.Limits().GetTo() && part_to_connect >= 0 && part_to_connect < (int)parts.size()-1 &&       // gap is not closed
               mj_no_gap.m_fully_connected_to_part == part_to_connect &&                                                             // no additional gap
               mi.m_type == eCDS && mj.m_type == eCDS &&
               mj.m_cds_info->MaxCdsLimits().GetTo() == TSignedSeqRange::GetWholeTo() && 
               mi.m_cds_info->MaxCdsLimits().GetFrom() == TSignedSeqRange::GetWholeFrom()) {                                         // reading frame not interrupted 
                    
#define PGAP_PENALTY 120
                
                int newcds = mj_no_gap.m_left_cds+mi.m_cds - PGAP_PENALTY;
                double newnum = mj_no_gap.m_left_num+mi.m_num;

		bool trust_compatible = mi.m_left_trusted_group == 0 || mj_no_gap.m_left_trusted_group == 0 || mi.m_left_trusted_group ==  mj_no_gap.m_left_trusted_group;
		bool better_connection = mi.m_left_member == 0 || newcds > mi.m_left_cds || (newcds == mi.m_left_cds && newnum > mi.m_left_num);

                if(better_connection) {
		    if(trust_compatible) {
			mi.m_left_cds = newcds;
			mi.m_left_num = newnum;
			mi.m_left_member = &mj_no_gap;
			mi.m_gapped_connection = true;
			mi.m_fully_connected_to_part = part_to_connect;
		    } else if(mi.m_type == eCDS && mj.m_cds_from_trusted) {
			mi.m_excluded_readthrough = true;
		    }
                }
            } else if(ai.Limits().IntersectingWith(aj.Limits())) {
                int delta_cds;
                double delta_num;
                double delta_splice_num;
                if(LRCanChainItoJ(delta_cds, delta_num, delta_splice_num, mi, mj, micontained, not_sorted)) {      // i and j connected continuosly
                    int newcds = mj.m_left_cds+delta_cds;
                    double newnum = mj.m_left_num+delta_num;
                    double newsplicenum = mj.m_left_splice_num+delta_splice_num;

		    bool trust_compatible = true;
		    if(mj.m_left_trusted_group != 0) {
			int tgr = mj.m_left_trusted_group;
			if(mi.m_left_trusted_group != 0 && mi.m_left_trusted_group != tgr) // direct connction of trusted
			    trust_compatible = false;
			else if(mj.m_type == eLeftUTR && mi.m_align->Limits().GetTo() > trusted_group_cds[tgr].GetTo()) // left trusted utr goes past cds
			    trust_compatible = false;
		    }
                    bool better_connection = false;
		    if(mi.m_left_member == 0) {
			better_connection = true;
		    } else if(newcds != mi.m_left_cds) {
			better_connection = (newcds > mi.m_left_cds);
		    } else if(fabs(newsplicenum - mi.m_left_splice_num) > 0.001) {
			better_connection = (newsplicenum > mi.m_left_splice_num);
		    } else if(newnum > mi.m_left_num) {
			better_connection = true;
		    }		    

                    if (better_connection) {
			if(trust_compatible) {
			    mi.m_left_cds = newcds;
			    mi.m_left_splice_num = newsplicenum;
			    mi.m_left_num = newnum;
			    mi.m_gapped_connection = mj.m_gapped_connection;
			    mi.m_left_member = &mj;
			    mi.m_fully_connected_to_part = part_to_connect;
			    if(!mi.m_gapped_connection)
				mi_no_gap = mi;
			} else if(mi.m_type == eCDS && mj.m_cds_from_trusted) {
			    mi.m_excluded_readthrough = true;
			}
                    } else if(mj_no_gap.m_fully_connected_to_part == part_to_connect) {
                        newcds = mj_no_gap.m_left_cds+delta_cds;
                        newnum = mj_no_gap.m_left_num+delta_num;
                        newsplicenum = mj_no_gap.m_left_splice_num+delta_splice_num;

			trust_compatible = mi_no_gap.m_left_trusted_group == 0 || mj_no_gap.m_left_trusted_group == 0 || mi_no_gap.m_left_trusted_group ==  mj_no_gap.m_left_trusted_group;
			better_connection = false;
			if(mi_no_gap.m_left_member == 0) {
			    better_connection = true;
			} else if(newcds != mi_no_gap.m_left_cds) {
			    better_connection = (newcds > mi_no_gap.m_left_cds);
			} else if(fabs(newsplicenum - mi_no_gap.m_left_splice_num) > 0.001) {
			    better_connection = (newsplicenum > mi_no_gap.m_left_splice_num);
			} else if(newnum > mi_no_gap.m_left_num) {
			    better_connection = true;
			}			

                        if (better_connection) {
			    if(trust_compatible) {
				mi_no_gap.m_left_cds = newcds;
				mi_no_gap.m_left_splice_num = newsplicenum;
				mi_no_gap.m_left_num = newnum;
				mi_no_gap.m_left_member = &mj_no_gap;
				mi_no_gap.m_fully_connected_to_part = part_to_connect;
			    } else if(mi_no_gap.m_type == eCDS && mj_no_gap.m_cds_from_trusted) {
				mi_no_gap.m_excluded_readthrough = true;
			    }
                        }
                    }
                }
            }
	    if(mi.m_left_member != nullptr && mi.m_left_member->m_left_trusted_group != 0) { // propagate trusted group
		mi.m_left_trusted_group = mi.m_left_member->m_left_trusted_group;
		if(mi.m_type == eCDS)
		    mi.m_cds_from_trusted = (mi.m_cds_from_trusted || mi.m_left_member->m_cds_from_trusted);
	    }
	    if(mi_no_gap.m_left_member != nullptr && mi_no_gap.m_left_member->m_left_trusted_group !=0 ) { // propagate trusted group
		mi_no_gap.m_left_trusted_group = mi_no_gap.m_left_member->m_left_trusted_group;
		if(mi_no_gap.m_type == eCDS)
		    mi_no_gap.m_cds_from_trusted = (mi_no_gap.m_cds_from_trusted || mi_no_gap.m_left_member->m_cds_from_trusted);
	    }
        }

	//reset gapped status when a part was included
        if(mi.m_left_member != 0 && last_included_part >= 0) {
            mi.m_fully_connected_to_part = last_included_part;
            mi.m_gapped_connection = false;
            mi_no_gap = mi;
        }

        if(mi.m_fully_connected_to_part == (int)parts.size()-1) {   // includes all parts
            fully_connected_right = max(fully_connected_right,mi.m_align->Limits().GetTo());

            if(best_right == 0 || (mi.m_left_cds >  best_right->m_left_cds || (mi.m_left_cds ==  best_right->m_left_cds && mi.m_left_num >  best_right->m_left_num)) ) 
                best_right = &mi;
        }
    }

    if(best_right == nullptr)
	return best_right;
	
    _ASSERT(std::less<SChainMember*>()(best_right, &no_gap_members.front()) ||  std::less<SChainMember*>()(&no_gap_members.back(), best_right));  // don't point to temporary vector 
    for (SChainMember* mp = best_right; mp != 0; mp = mp->m_left_member) {
        if(!std::less<SChainMember*>()(mp->m_left_member, &no_gap_members.front()) && !std::less<SChainMember*>()(&no_gap_members.back(), mp->m_left_member)) { // points to temporary vector 
            SChainMember* p = pointers[mp->m_left_member-&no_gap_members.front()];
            *p = *mp->m_left_member;
            mp->m_left_member = p;
        }
    }

    return best_right;
}

struct AlignLenOrder
{
    AlignLenOrder(TOrigAligns& oa) : orig_aligns(oa) {}
    TOrigAligns& orig_aligns;

    bool operator()(const vector<CGeneModel*>* ap, const vector<CGeneModel*>* bp)
    {
        const vector<CGeneModel*>& partsa = *ap;
        const vector<CGeneModel*>& partsb = *bp;
        
        int align_lena = 0;
        ITERATE(vector<CGeneModel*>, k, partsa)
            align_lena += (*k)->AlignLen();

        int align_lenb = 0;
        ITERATE(vector<CGeneModel*>, k, partsb)
            align_lenb += (*k)->AlignLen();

        if(align_lena != align_lenb) {
            return align_lena > align_lenb;
        } else {
            return *orig_aligns[partsa.front()->ID()]->GetTargetId() < *orig_aligns[partsb.front()->ID()]->GetTargetId(); // to make sort deterministic
        }
    }    
};

void CChainer::CChainerImpl::CreateChainsForPartialProteins(TChainList& chains, TContained& pointers_all, TGeneModelList& unma_aligns, CChainMembers& unma_members) {

    sort(pointers_all.begin(),pointers_all.end(),LeftOrderD());

    typedef map<Int8, vector<CGeneModel*> > TIdChainMembermap;
    TIdChainMembermap protein_parts;
    for(int k = 0; k < (int)pointers_all.size(); ++k) {
        SChainMember& mi = *pointers_all[k];

        if((mi.m_align->Type() & CGeneModel::eProt) && (mi.m_copy == 0 || mi.m_cds_info->HasStart())) {  // only prots with start can have copies
            protein_parts[mi.m_align->ID()].push_back(mi.m_align);
        }
    }

    vector<vector<CGeneModel*>*> gapped_sorted_protein_parts;
    NON_CONST_ITERATE(TIdChainMembermap, ip, protein_parts) {
        vector<CGeneModel*>& parts = ip->second;
        if(parts.size() > 1) {
            sort(parts.begin(),parts.end(),AlignSeqOrder());
            gapped_sorted_protein_parts.push_back(&parts);
        }
    }
    sort(gapped_sorted_protein_parts.begin(),gapped_sorted_protein_parts.end(),AlignLenOrder(orig_aligns));
    
    NON_CONST_ITERATE(vector<vector<CGeneModel*>*>, ip, gapped_sorted_protein_parts) {  // make chains starting from long proteins
        vector<CGeneModel*>& parts = **ip;
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
        NON_CONST_ITERATE(TChainList, k, chains) {
            if(k->Continuous() && palign.Strand() == k->Strand() && palign.IsSubAlignOf(*k)) {
                connected = true;
#ifdef _DEBUG
                k->AddComment("Was connected "+orig_aligns[palign.ID()]->TargetAccession());
#endif
                break;
            }
        }
        if(connected)
            continue;

        TContained pointers;
        for(int k = 0; k < (int)pointers_all.size(); ++k) {
            SChainMember* mip = pointers_all[k];
            TSignedSeqRange limits = mip->m_align->Limits();

            if(mip->m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible)) // skip flexible
                continue;

            if((mip->m_type != eCDS || !Include(mip->m_cds_info->MaxCdsLimits(),mip->m_align->Limits())) && Include(palign.Limits(),mip->m_align->Limits())) // skip all not entirely coding inside protein alignment   
                continue;

            if(mip->m_align->Exons().front().m_ssplice_sig == "XX" && Include(palign.Limits(),mip->m_align->Exons().front().Limits())) // skip 3'/5' cdna gapfillers inside protein alignment
                continue;
        
            if(mip->m_align->Exons().back().m_fsplice_sig == "XX" && Include(palign.Limits(),mip->m_align->Exons().back().Limits())) // skip 3'/5' cdna gapfillers inside protein alignment     
                continue;

            if(palign.Limits().IntersectingWith(limits)) { // skip not compatible alignments
                bool compatible = true;
                for(CGeneModel* partp : parts) {
                    if((mip->m_align->ID() != id && Include(partp->Limits(), limits)) || (partp->Limits().IntersectingWith(limits) && !partp->HasCompatibleOverlap(*mip->m_align))) {
                        compatible = false;
                        break;
                    }
                }
                if(!compatible)
                    continue;
            }

            pointers.push_back(mip);
        }

        SChainMember* best_right = FindOptimalChainForProtein(pointers, parts, palign);

	if(best_right == nullptr)
	    return;

        best_right->m_right_member = 0;
        //        CChain chain(*best_right, &palign, false, false);
        CChain chain(*best_right, false);
        chain.m_gapped_helper_align = palign;

        if(unmodified_aligns.count(id)) {  // some unmodifies are deleted if interfere with a gap or trusted genes
            CGeneModel unma = unmodified_aligns[id]; 
            vector<TSignedSeqRange> new_holes;
            vector<TSignedSeqRange> remaining_holes;
            for(int k = 1; k < (int)chain.Exons().size(); ++k) {
                CModelExon exonl = chain.Exons()[k-1];
                CModelExon exonr = chain.Exons()[k];
                if(!(exonl.m_ssplice && exonr.m_fsplice)) {
                    TSignedSeqRange h(exonl.GetTo()+1,exonr.GetFrom()-1);
                    remaining_holes.push_back(h);
                    for(int piece_begin = 0; piece_begin < (int)unma.Exons().size(); ++piece_begin) {
                        int piece_end = piece_begin;
                        for( ; piece_end < (int)unma.Exons().size() && unma.Exons()[piece_end].m_ssplice; ++piece_end);
                        if(unma.Exons()[piece_begin].GetFrom() < h.GetFrom() && unma.Exons()[piece_end].GetTo() > h.GetTo()) {
                            new_holes.push_back(h);
                            break;
                        }
                        piece_begin = piece_end;
                    }
                }
            }
       
            if(!new_holes.empty()) {  // failed to connect all parts - try unsupported introns
                CAlignMap umap = unma.GetAlignMap();
                if(unma.Limits() != palign.Limits()) {
                    TSignedSeqRange lim = umap.ShrinkToRealPoints(palign.Limits(), true);
                    unma.Clip(lim,CGeneModel::eRemoveExons);                
                }

                vector<TSignedSeqRange> existed_holes;
                for(int k = 1; k < (int)unma.Exons().size(); ++k) {
                    CModelExon exonl = unma.Exons()[k-1];
                    CModelExon exonr = unma.Exons()[k];
                    if(!(exonl.m_ssplice && exonr.m_fsplice))
                        existed_holes.push_back(TSignedSeqRange(exonl.GetTo()+1,exonr.GetFrom()-1));
                }

                for(int k = 1; k < (int)palign.Exons().size(); ++k) {   // cut holes which were connected or existed    
                    CModelExon exonl = palign.Exons()[k-1];
                    CModelExon exonr = palign.Exons()[k];
                    if(!(exonl.m_ssplice && exonr.m_fsplice)) {
                        TSignedSeqRange hole(exonl.GetTo()+1,exonr.GetFrom()-1);
                        bool connected = true;
                        ITERATE(vector<TSignedSeqRange>, h, remaining_holes) {
                            _ASSERT(Include(unma.Limits(), *h));
                            if(Include(hole, *h)) {
                                connected = false;
                                break;
                            }
                        }

                        bool existed = false;
                        ITERATE(vector<TSignedSeqRange>, h, existed_holes) {
                            if(Include(hole, *h)) {
                                existed = true;
                                break;
                            }
                        }

                        if(connected || existed) {
                            TSignedSeqRange left = umap.ShrinkToRealPoints(TSignedSeqRange(unma.Limits().GetFrom(),hole.GetFrom()-1), true);
                            TSignedSeqRange right = umap.ShrinkToRealPoints(TSignedSeqRange(hole.GetTo()+1,unma.Limits().GetTo()), true);
                            if(left.GetTo()+1 == hole.GetFrom() && right.GetFrom()-1 == hole.GetTo())
                                unma.CutExons(hole);
                        }
                    }
                }
                m_gnomon->GetScore(unma);
            
                TGeneModelList unmacl;
                unmacl.push_back(unma);
                CutParts(unmacl);

                vector<CGeneModel*> unmaparts;
                NON_CONST_ITERATE(TGeneModelList, im, unmacl) {
                    m_gnomon->GetScore(*im);
                    unmaparts.push_back(&(*im));
                }

                CChainMembers unmapointers(unmacl, orig_aligns, unmodified_aligns);
                Duplicate5pendsAndShortCDSes(unmapointers);
                sort(pointers.begin(),pointers.end(),GenomeOrderD());
                ITERATE(TContained, ip, unmapointers) {
                    SChainMember& mi = **ip;
                    IncludeInContained(mi, mi);          // include self                    
                    //                    ITERATE(TContained, jp, pointers) {
                    ITERATE(TContained, jp, pointers_all) {
                        SChainMember& mj = **jp;
                        if(CanIncludeJinI(mi, mj)) 
                            IncludeInContained(mi, mj);
                    }
                }
                
                ITERATE(TContained, ip, unmapointers) {
                    _ASSERT((*ip)->m_orig_align);
                    (*ip)->m_mem_id = -(*ip)->m_mem_id;   // unique m_mem_id    
                    pointers.push_back(*ip);
                }
                
                sort(pointers.begin(),pointers.end(),LeftOrderD());
                best_right = FindOptimalChainForProtein(pointers, unmaparts, unma);
                ITERATE(TContained, jp, unmapointers) {  // add parts in case they were 'shadowed' by longer or identical alignment 
                    SChainMember& mj = **jp;
                    bool present = false;
                    for(SChainMember* ip = best_right; ip != 0 && !present; ip = ip->m_left_member)
                        present = ip == &mj;
                    for(SChainMember* ip = best_right; ip != 0 && !present; ip = ip->m_left_member) {
                        SChainMember& mi = *ip;
                        if(CanIncludeJinI(mi, mj)) {
                            IncludeInContained(mi, mj);
                            break;
                        }
                    }
                }
                //                chain = CChain(*best_right, &unma, false, false);
                chain = CChain(*best_right, false);
                chain.m_gapped_helper_align = unma;
                unma_aligns.splice(unma_aligns.end(), unmacl);
                unma_members.SpliceFromOther(unmapointers);
            }
        }

        if(!chain.SetConfirmedEnds(*m_gnomon, confirmed_ends))
            continue;

        if(!chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns))
            m_gnomon->GetScore(chain);
        if(chain.Score() == BadScore())
            continue;
        chain.AddAllMembersAndCoverage(*best_right);
        chain.RemoveFshiftsFromUTRs();
        /*
        m_gnomon->GetScore(chain);
        if(chain.Score() == BadScore())
            continue;
        chain.RemoveFshiftsFromUTRs();
        chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns);
        */
        const CResidueVec& contig = m_gnomon->GetSeq();
        chain.ClipToCap(min_cap_blob, max_dist, min_flank_exon, secondary_peak, false);
        chain.ClipToPolyA(contig, min_polya_blob, max_dist, min_flank_exon, secondary_peak, tertiary_peak, tertiary_peak_coverage, false);
        chain.ClipLowCoverageUTR(minscor.m_utr_clip_threshold, false);
        if(!chain.SetConfirmedEnds(*m_gnomon, confirmed_ends))
            continue;
        m_gnomon->GetScore(chain, !no5pextension); // this will return CDS to best/longest depending on no5pextension
        chain.CheckSecondaryCapPolyAEnds();
        chain.CalculateSupportAndWeightFromMembers(false);
        chain.CalculateDropLimits();
        _ASSERT( chain.FShiftedLen(chain.GetCdsInfo().Start()+chain.ReadingFrame()+chain.GetCdsInfo().Stop(), false)%3==0 );

#ifdef _DEBUG
        chain.AddComment("Connected "+orig_aligns[palign.ID()]->TargetAccession());
        chain.AddComment("LinkForGapped  "+GetLinkedIdsForMember(*best_right));
#endif
        chains.push_back(chain);
    }
}

void CChainer::CChainerImpl::SetFlagsForChains(TChainList& chains) {

    int left = numeric_limits<int>::max();
    int right = 0;
    ITERATE(TOrigAligns, it, orig_aligns) {
        const CAlignModel& align = *it->second;
        left = min(left,align.Limits().GetFrom());
        right = max(right,align.Limits().GetTo());
    }

    int len = right-left+1;

    vector<int> prot_cov[2][3];
    prot_cov[0][0].resize(len,0);
    prot_cov[0][1].resize(len,0);
    prot_cov[0][2].resize(len,0);
    prot_cov[1][0].resize(len,0);
    prot_cov[1][1].resize(len,0);
    prot_cov[1][2].resize(len,0);
    ITERATE(TOrigAligns, it, orig_aligns) {
        const CAlignModel& align = *it->second;
        if(align.GetCdsInfo().ProtReadingFrame().NotEmpty()) {
            CAlignMap amap = align.GetAlignMap();
            int cdstr = amap.MapOrigToEdited(align.GetCdsInfo().Cds().GetFrom());
            for(int i = 0; i < (int)align.Exons().size(); ++i) {
                TSignedSeqRange rf = (align.Exons()[i].Limits() & align.ReadingFrame());
                if(rf.NotEmpty()) {
                    for(int j = rf.GetFrom(); j <= rf.GetTo(); ++j) {
                        int jtr = amap.MapOrigToEdited(j);
                        if(jtr >= 0)
                            ++prot_cov[align.Strand()][abs(cdstr-jtr)%3][j-left];
                    }
                }
            }
        }
    }    

    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    SMatrix matrix;

    const CResidueVec& contig = m_gnomon->GetSeq();

    NON_CONST_ITERATE(TChainList, it, chains) {
        CChain& chain = *it;
        //        chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns);
        chain.SetOpenForPartialyAlignedProteins(prot_complet);
        chain.SetConfirmedStartStopForCompleteProteins(prot_complet, minscor);
        chain.CollectTrustedmRNAsProts(orig_aligns, minscor, scope, matrix, contig);
        chain.SetBestPlacement(orig_aligns);
        chain.SetConsistentCoverage();
        if(chain.Continuous() && chain.Exons().size() > 1) {
            bool allcdnaintrons = true;
            int num = 0;
            for(int i = 1; i < (int)chain.Exons().size() && allcdnaintrons; ++i) {
                if(chain.Exons()[i-1].m_ssplice_sig != "XX" && chain.Exons()[i].m_fsplice_sig != "XX") {
                    TSignedSeqRange intron(TSignedSeqRange(chain.Exons()[i-1].GetTo(),chain.Exons()[i].GetFrom()));
                    allcdnaintrons = (mrna_count[intron]+est_count[intron]+rnaseq_count[intron] > 0);
                    ++num;
                }
            }
            if(allcdnaintrons && num >0)
                chain.Status() |= CGeneModel::ecDNAIntrons;
        }
        if (chain.FullCds()) {
            chain.Status() |= CGeneModel::eFullSupCDS;
        }

        if(chain.GetCdsInfo().ProtReadingFrame().Empty() && chain.ReadingFrame().NotEmpty()) {  // coding chain without protein support
            int protcds = 0;
            int lrf_from_proteins = numeric_limits<int>::max();
            int rrf_from_proteins = 0;
            CAlignMap amap = chain.GetAlignMap();
            int cdstr = amap.MapOrigToEdited(chain.GetCdsInfo().Cds().GetFrom());
            for(int i = 0; i < (int)chain.Exons().size(); ++i) {
                TSignedSeqRange rf = (chain.Exons()[i].Limits() & chain.ReadingFrame());
                if(rf.NotEmpty()) {
                    for(int j = rf.GetFrom(); j <= rf.GetTo(); ++j) {
                        if(j < left || j > right)
                            continue;

                        int jtr = amap.MapOrigToEdited(j);
                        int frame = abs(cdstr-jtr)%3;
                        if(jtr >= 0 && prot_cov[chain.Strand()][frame][j-left] > 0) {
                            if(frame == 0)
                                lrf_from_proteins = min(lrf_from_proteins,j);
                            if(frame == 2)
                                rrf_from_proteins = max(rrf_from_proteins,j);
                            ++protcds;
                        }
                    }
                }
            }
            if(protcds > 0.2*amap.FShiftedLen(chain.GetCdsInfo().Cds()) && rrf_from_proteins > lrf_from_proteins) {
                CCDSInfo cds = chain.GetCdsInfo();
                TSignedSeqRange reading_frame = cds.ReadingFrame();
                cds.SetReadingFrame(reading_frame&TSignedSeqRange(lrf_from_proteins,rrf_from_proteins), true);
                cds.SetReadingFrame(reading_frame);
                chain.SetCdsInfo(cds);
                chain.SetType(chain.Type()|CGeneModel::eProt);
                
#ifdef _DEBUG 
                chain.AddComment("Added protsupport");
#endif    
            }
        }        
    }
}


void CChainer::CChainerImpl::CombineCompatibleChains(TChainList& chains) {
    for(TChainList::iterator itt = chains.begin(); itt != chains.end(); ++itt) {
        if(itt->Status()&CGeneModel::eSkipped)
            continue;
        CCDSInfo::TPStops istops = itt->GetCdsInfo().PStops();
        for(TChainList::iterator jt = chains.begin(); jt != chains.end();) {
            TChainList::iterator jtt = jt++;
            if(jtt->Status()&CGeneModel::eSkipped)
                continue;

            if(itt != jtt && itt->Strand() == jtt->Strand() && jtt->IsSubAlignOf(*itt) && itt->ReadingFrame().Empty() == jtt->ReadingFrame().Empty()) {
                if(itt->ReadingFrame().NotEmpty()) {                    
                    TSignedSeqRange icds = itt->GetCdsInfo().Cds();
                    TSignedSeqRange jcds = jtt->GetCdsInfo().Cds(); 
                    TSignedSeqPos a = min(icds.GetFrom(),jcds.GetFrom());
                    TSignedSeqPos b = max(icds.GetFrom(),jcds.GetFrom());
                    bool same_frame = (itt->FShiftedLen(a, b, false)-1)%3 == 0;
                    /*
                    if(itt->Score() > jtt->Score() && (icds&jcds).Empty() && same_frame 
                       && ((itt->Strand() == ePlus && Precede(icds, jcds)) || (itt->Strand() == eMinus && Precede(jcds, icds)))) {
                        jtt->AddComment("Secondary CDS for "+to_string(itt->ID()));
                    }
                    */

                    if(!Include(jtt->MaxCdsLimits(), itt->MaxCdsLimits()))
                        continue;               
                
                    if(jtt->FrameShifts() != StrictlyContainedInDels(itt->FrameShifts(), jtt->Limits()))
                        continue;
                    
                    if(!same_frame)
                        continue;

                    CCDSInfo::TPStops jstops = jtt->GetCdsInfo().PStops();
                    bool same_stops = true;
                    ITERATE(CCDSInfo::TPStops, istp, istops) {
                        if(Include(jtt->Limits(),*istp) && find(jstops.begin(), jstops.end(), *istp) == jstops.end()) {
                            same_stops = false;
                            break;
                        }
                    }
                    if(!same_stops)
                        continue;
                    /* current algorithm prefers shorter CDS with less pstops (still includes longer protein as evidence)
                    ITERATE(CCDSInfo::TPStops, jstp, jstops) {
                        if(find(istops.begin(), istops.end(), *jstp) == istops.end()) {
                            same_stops = false;
                            break;
                        }
                    }
                    if(!same_stops)
                        continue;
                    */
                }

                TMemberPtrSet support;
                ITERATE(TContained, i, itt->m_members) {
                    support.insert(*i);
                    if((*i)->m_copy != 0)
                        support.insert((*i)->m_copy->begin(),(*i)->m_copy->end());
                }
                TSignedSeqRange jlimits = jtt->m_supported_range;
                ITERATE(TContained, i, jtt->m_members) {
                    TSignedSeqRange il = (*i)->m_align->Limits();
                    if(!jlimits.IntersectingWith(il))
                        continue;
                    if(((*i)->m_align->Status()&CGeneModel::eLeftFlexible) && il.GetTo() > jlimits.GetTo())
                        continue;
                    if(((*i)->m_align->Status()&CGeneModel::eRightFlexible) && il.GetFrom() < jlimits.GetFrom())
                        continue;

                    if(support.insert(*i).second && (Include(jlimits, il) || itt->HasCompatibleOverlap(*(*i)->m_align, 1))) {
                        itt->m_was_combined = true;
                        itt->m_members.push_back(*i);
                        if((*i)->m_copy != 0)
                            support.insert((*i)->m_copy->begin(),(*i)->m_copy->end());                        
                    }
                }
                if(itt->m_was_combined) {
                    sort(itt->m_members.begin(),itt->m_members.end(),GenomeOrderD());
                    itt->CalculateSupportAndWeightFromMembers();
                }

                jtt->Status() |= CGeneModel::eSkipped;
		jtt->AddComment("Combined with "+to_string(itt->ID()));
            }
        }
    }
}

double CChainer::CChainerImpl::GoodCDNAScore(const CGeneModel& algn, bool simple)
{
    if(algn.FShiftedLen(algn.GetCdsInfo().Cds(),true) >  minscor.m_cds_len)
        return 0.99*BadScore();

    if(((algn.Type()&CGeneModel::eProt)!=0 || algn.ConfirmedStart()) && algn.FShiftedLen(algn.GetCdsInfo().ProtReadingFrame(),true) > minscor.m_prot_cds_len) 
        return 0.99*BadScore();
               
    return minscor.m_min;    

    /*
    int cdslen = algn.RealCdsLen();
    int len = algn.AlignLen();

    if(simple)
        return  max(0.,minscor.m_min-minscor.m_cds_bonus*cdslen);
    
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

    return  max(0.,minscor.m_min+minscor.m_i5p_penalty*intron_5p+minscor.m_i3p_penalty*intron_3p-minscor.m_cds_bonus*cdslen+minscor.m_length_penalty*len);
    */
}       


void CChainer::CChainerImpl::RemovePoorCds(CGeneModel& algn, double minscor)
{
    if (algn.Score() < minscor)
        algn.SetCdsInfo(CCDSInfo());
}

#define SCAN_WINDOW 49            // odd number!!!

void CChain::AddAllMembersAndCoverage(SChainMember& mbr) {    
    m_members = mbr.CollectContainedForChain();
    _ASSERT(m_members.size()>0);
    sort(m_members.begin(),m_members.end(),GenomeOrderD());

    for(SChainMember* mi : m_members)
        mi->MarkPostponed();

    //    CalculateSupportAndWeightFromMembers(false);
    
    CAlignMap amap = GetAlignMap();
    int mrna_len = amap.FShiftedLen(Limits());
    vector<double> coverage_raw(mrna_len+SCAN_WINDOW);
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        if(align.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;

        TSignedSeqRange overlap = Limits()&align.Limits();  // theoretically some ends could be outside (partially trimmed from other chain and combined)
        if(align.Type() == CGeneModel::eSR && overlap.NotEmpty()) {
            TSignedSeqRange on_mrna = amap.MapRangeOrigToEdited(overlap);        // for align partially in a hole will give the hole boundary
            for(int i = on_mrna.GetFrom(); i <= on_mrna.GetTo(); ++i)
                coverage_raw[i+SCAN_WINDOW/2] += align.Weight();
        }
    }
   
    m_coverage.clear();
    m_coverage.resize(mrna_len);
    double cov = 0;
    for(int i = 0; i < SCAN_WINDOW; ++i)
        cov += coverage_raw[i]/SCAN_WINDOW;
    for(int i = 0; i < mrna_len; ++i) {            // will decrease coverage in SCAN_WINDOW/2 end intervals
        m_coverage[i] = cov;
        cov -= coverage_raw[i]/SCAN_WINDOW;
        cov += coverage_raw[i+SCAN_WINDOW]/SCAN_WINDOW;
    }         
}

CChain::CChain(SChainMember& mbr, bool full_support) : m_coverage_drop_left(-1), m_coverage_drop_right(-1), m_coverage_bump_left(-1), m_coverage_bump_right(-1), m_core_coverage(0), m_splice_weight(0), m_cap_peaks(3, -1), m_polya_peaks(3, -1), m_was_combined(false) {
    m_strand = mbr.m_align->Strand();
    m_fshifts = mbr.m_align->m_fshifts;
    int atype = eSR|eEST|emRNA|eProt|eNotForChaining;
    m_type = eChain|(mbr.m_align->m_type&atype);
    m_weight = mbr.m_num;
    mbr.MarkPostponed();
    if(mbr.m_excluded_readthrough)
	Status() |= CGeneModel::eExcludedReadthrough;

    deque<CModelExon> exons(mbr.m_align->Exons().begin(), mbr.m_align->Exons().end());
    for(SChainMember* p = mbr.m_right_member; p != nullptr; p = p->m_right_member) {
	if(p->m_excluded_readthrough)
	    Status() |= CGeneModel::eExcludedReadthrough;
        p->MarkPostponed();
        m_type |= (p->m_align->m_type&atype);
        if(p->m_align->Status()&CGeneModel::eLeftFlexible) {
            exons.back().AddTo(p->m_align->Limits().GetTo()-exons.back().GetTo());
        } else {        
            m_fshifts.insert(m_fshifts.end(), p->m_align->m_fshifts.begin(), p->m_align->m_fshifts.end());
            auto& other_exons = p->m_align->Exons();
            int num = other_exons.size();
            int first;
            if(num == 1)
                first = 0;
            else if(num == 2)
                first = other_exons.front().GetTo() >= exons.back().GetTo() ? 0 : 1;
            else
                first = std::lower_bound(other_exons.begin(), other_exons.end(), exons.back().GetTo(), [](const CModelExon& e, TSignedSeqPos a) { return e.GetTo() < a; })-other_exons.begin();
            exons.back().Extend(other_exons[first]);
            exons.insert(exons.end(), other_exons.begin()+first+1, other_exons.end());
        }
    }
    SChainMember* prev = &mbr;
    for(SChainMember* p = mbr.m_left_member; p != nullptr; prev = p, p = p->m_left_member) {
	if(p->m_excluded_readthrough)
	    Status() |= CGeneModel::eExcludedReadthrough;
        p->MarkPostponed();
        m_type |= (p->m_align->m_type&atype);
        if(p->m_align->Status()&CGeneModel::eRightFlexible) {
            exons.front().AddFrom(p->m_align->Limits().GetFrom()-exons.front().GetFrom()); // must be negative
        } else {        
            m_fshifts.insert(m_fshifts.end(), p->m_align->m_fshifts.begin(), p->m_align->m_fshifts.end());
            auto& other_exons = p->m_align->Exons();
            int num = other_exons.size();

            if(other_exons.back().GetTo() < exons.front().GetFrom()) { // hole from proteins
                exons.insert(exons.begin(), other_exons.begin(), other_exons.end());
                // set splices
                exons[num].m_fsplice = false;
                exons[num].m_fsplice_sig.clear();
                exons[num-1].m_ssplice = false;
                exons[num-1].m_ssplice_sig.clear();
                // cli hole to codons
                TSignedSeqRange cds = prev->m_cds_info->Cds();
                _ASSERT(cds.NotEmpty() && cds.IntersectingWith(exons[num].Limits()));
                if(cds.GetFrom() > exons[num].GetFrom())
                    exons[num].Limits().SetFrom(cds.GetFrom());
                TSignedSeqRange other_cds = p->m_cds_info->Cds();
                _ASSERT(other_cds.NotEmpty() && other_cds.IntersectingWith(exons[num-1].Limits()));
                if(other_cds.GetTo() < exons[num-1].GetTo())
                    exons[num-1].Limits().SetTo(other_cds.GetTo());
            } else {
                int first;
                if(num == 1)
                    first = 0;
                else if(num == 2)
                    first = other_exons.back().GetFrom() <= exons.front().GetFrom() ? 1 : 0;
                else 
                    first = std::lower_bound(other_exons.begin(), other_exons.end(), exons.front().GetFrom(), [](const CModelExon& e, TSignedSeqPos a) { return e.GetTo() < a; })-other_exons.begin();
                exons.front().Limits().SetFrom(other_exons[first].GetFrom());
                if(other_exons[first].m_fsplice) {
                    exons.front().m_fsplice = true;
                    exons.front().m_fsplice_sig = other_exons[first].m_fsplice_sig;
                }
                exons.insert(exons.begin(), other_exons.begin(), other_exons.begin()+first);
            }
        }
    }

    m_members = mbr.CollectCodingContainedForChain();      // only coding
    sort(m_members.begin(),m_members.end(),GenomeOrderD());
    for(SChainMember* mi : m_members) {
        mi->MarkPostponed();
        m_type |= (mi->m_align->m_type&atype);
        m_cds_info.CombineWith(*mi->m_cds_info);
    }

    m_exons.assign(exons.begin(), exons.end());
    m_range.SetFrom(m_exons.front().GetFrom());
    m_range.SetTo(m_exons.back().GetTo());
    m_exons.front().m_fsplice = false;
    m_exons.front().m_fsplice_sig.clear();
    m_exons.back().m_ssplice = false;
    m_exons.back().m_ssplice_sig.clear();
    m_supported_range = m_range;                           // could become != m_range in SetConfirmedEnds
    m_polya_cap_left_soft_limit = m_range.GetTo()+1;
    m_polya_cap_right_soft_limit = m_range.GetFrom()-1;
    if(!m_fshifts.empty()) {
        sort(m_fshifts.begin(),m_fshifts.end());
        m_fshifts.erase(unique(m_fshifts.begin(),m_fshifts.end()), m_fshifts.end());
    }

    if(full_support)
        AddAllMembersAndCoverage(mbr);
}

CChain::CChain(SChainMember& mbr, CGeneModel* gapped_helper, bool keep_all_evidence, bool addallsupport) : m_coverage_drop_left(-1), m_coverage_drop_right(-1), m_coverage_bump_left(-1), m_coverage_bump_right(-1), m_core_coverage(0), m_splice_weight(0), m_cap_peaks(3, -1), m_polya_peaks(3, -1), m_was_combined(false)
{
    m_members = mbr.CollectContainedForChain();
    _ASSERT(m_members.size()>0);
    sort(m_members.begin(),m_members.end(),GenomeOrderD());
    
    list<CGeneModel> extened_parts;
    vector<CGeneModel*> extened_parts_and_gapped;
    if(gapped_helper != 0) {
        extened_parts_and_gapped.push_back(gapped_helper);
        m_gapped_helper_align = *gapped_helper;
    }
    //limits extended by cap/polya info alignments without other support
    int left = numeric_limits<int>::max();
    int right = 0;
    ITERATE(TContained, i, m_members) {
        SChainMember* mi = *i;
        CGeneModel align = *mi->m_align;
        if(align.Status()&CGeneModel::eLeftFlexible) {
            right = max(right, align.Limits().GetTo());
            continue;
        } else if(align.Status()&CGeneModel::eRightFlexible) {
            left = min(left, align.Limits().GetFrom());
            continue;
        }
        align.SetCdsInfo(*mi->m_cds_info);
        if(extened_parts.empty() || !align.Limits().IntersectingWith(extened_parts.back().Limits())) {
            extened_parts.push_back(align);
            _ASSERT(extened_parts.back().Continuous());
            extened_parts_and_gapped.push_back(&extened_parts.back());
        } else {
            extened_parts.back().Extend(align, false);
            _ASSERT(extened_parts.back().Continuous());
        }
    }
    if(left < extened_parts.front().Limits().GetFrom())
        extened_parts.front().ExtendLeft(extened_parts.front().Limits().GetFrom()-left);
    if(right > extened_parts.back().Limits().GetTo())
        extened_parts.back().ExtendRight(right-extened_parts.back().Limits().GetTo());

    SetType(eChain);
    EStrand strand = extened_parts_and_gapped.front()->Strand();
    SetStrand(strand);

    sort(extened_parts_and_gapped.begin(),extened_parts_and_gapped.end(),AlignSeqOrder());
    ITERATE (vector<CGeneModel*>, it, extened_parts_and_gapped) {
        const CGeneModel& align = **it;
        Extend(align, false);
    }    

    NON_CONST_ITERATE(TExons, e, MyExons()) {
        if(!e->m_fsplice)
            e->m_fsplice_sig.clear();
        if(!e->m_ssplice)
            e->m_ssplice_sig.clear();
    }

    m_supported_range = Limits();
    m_weight = mbr.m_num;

    if(addallsupport)
        CalculateSupportAndWeightFromMembers(keep_all_evidence);

    m_polya_cap_left_soft_limit = Limits().GetTo()+1;
    m_polya_cap_right_soft_limit = Limits().GetFrom()-1;
    
    CAlignMap amap = GetAlignMap();
    int mrna_len = amap.FShiftedLen(Limits());
    vector<double> coverage_raw(mrna_len+SCAN_WINDOW);
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        if(align.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;

        TSignedSeqRange overlap = Limits()&align.Limits();  // theoretically some ends could be outside (partially trimmed from other chain and combined)
        if(align.Type() == CGeneModel::eSR && overlap.NotEmpty()) {
            TSignedSeqRange on_mrna = amap.MapRangeOrigToEdited(overlap);        // for align partially in a hole will give the hole boundary
            for(int i = on_mrna.GetFrom(); i <= on_mrna.GetTo(); ++i)
                coverage_raw[i+SCAN_WINDOW/2] += align.Weight();
        }
    }
   
    m_coverage.clear();
    m_coverage.resize(mrna_len);
    double cov = 0;
    for(int i = 0; i < SCAN_WINDOW; ++i)
        cov += coverage_raw[i]/SCAN_WINDOW;
    for(int i = 0; i < mrna_len; ++i) {            // will decrease coverage in SCAN_WINDOW/2 end intervals
        m_coverage[i] = cov;
        cov -= coverage_raw[i]/SCAN_WINDOW;
        cov += coverage_raw[i+SCAN_WINDOW]/SCAN_WINDOW;
    }     
}

bool CChain::HasTrustedEvidence() const {
    ITERATE (TContained, i, m_members) {
        const CGeneModel* align = (*i)->m_align;
        if(!align->TrustedProt().empty() || (!align->TrustedmRNA().empty() && (*i)->m_cds_info->ProtReadingFrame().NotEmpty())) {
            //            CAlignModel* orig_align = orig_aligns[align->ID()];
            if(align->AlignLen() > 0.5*(*i)->m_orig_align->TargetLen())
                return true;
        }
    }

    return false;
}

void CChain::SetBestPlacement(TOrigAligns& orig_aligns) {

    map<Int8,int> exonnum;
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;

        if(align.GetCdsInfo().ProtReadingFrame().NotEmpty() && (align.Status()&eBestPlacement) && ((*it)->m_copy == 0 || (*it)->m_cds_info->HasStart())) // best placed protein or projected mRNA
            exonnum[align.ID()] += align.Exons().size();       
    }

    for(map<Int8,int>::iterator it = exonnum.begin(); it != exonnum.end(); ++it) {
        if(it->second >= (int)orig_aligns[it->first]->Exons().size()) {   // all exons are included in the chain
            Status() |= eBestPlacement;
            break;
        }
    }
}

struct SLinker
{
    SLinker() : m_member(0), m_value(0), m_matches(0), m_left(0), m_not_wanted(false), m_count(0), m_not_wanted_count(0), m_matches_count(0), m_connected(false) {}
    bool operator<(const SLinker& sl) const {
        if(m_range != sl.m_range)
            return m_range < sl.m_range; 
        else if(!m_member)
            return true;
        else if (!sl.m_member)
            return false;
        else
            return m_member->m_mem_id < sl.m_member->m_mem_id;  // to make sort deterministic
    }
    bool NewIsBetter(int new_count, int new_not_wanted_count, int new_matches_count, int new_mem_id) const {
        if(m_count != new_count)
            return new_count < m_count;
        if(m_not_wanted_count != new_not_wanted_count)
            return new_not_wanted_count < m_not_wanted_count;
        if(m_matches_count != new_matches_count)
            return new_matches_count > m_matches_count;
        int mem_id = m_left->m_member != nullptr ? m_left->m_member->m_mem_id : 0;
        return new_mem_id < mem_id;      // to make deterministic
    }
    bool OtherIsBetter(const SLinker& a) const {
        if(m_count != a.m_count)
            return a.m_count < m_count;
        if(m_not_wanted_count != a.m_not_wanted_count)
            return a.m_not_wanted_count < m_not_wanted_count;
        if(m_matches_count != a.m_matches_count)
            return a.m_matches_count > m_matches_count;
        int mem_id = m_member != nullptr ? m_member->m_mem_id : 0;
        int amem_id = a.m_member != nullptr ? a.m_member->m_mem_id : 0;
        return amem_id < mem_id;     // to make deterministic
    }

    SChainMember* m_member;
    TSignedSeqRange m_range;
    TSignedSeqRange m_reading_frame;
    int m_value;
    int m_matches;
    SLinker* m_left;
    bool m_not_wanted;
    int m_count;
    int m_not_wanted_count;
    int m_matches_count;
    bool m_connected;
};

typedef vector<SLinker> TLinkers;

struct RangeOrder {
    bool operator()(const TSignedSeqRange& a, const TSignedSeqRange& b) const {
        return Precede(a, b);
    }
};
typedef set<TSignedSeqRange,RangeOrder> TRangePrecedeSet;


void CChain::CalculateSupportAndWeightFromMembers(bool keep_all_evidence) {
    TLinkers linkers;
    for(SChainMember* mi : m_members) {
        CGeneModel* ai = mi->m_align;
        TSignedSeqRange alimits = ai->Limits();
        if(!alimits.IntersectingWith(m_supported_range)) // clipped
            continue;
        _ASSERT(mi->m_orig_align);
        int matches = ai->AlignLen();
        if(ai->Ident() > 0.)
            matches = ai->Ident()*matches+0.5;
        bool not_wanted = false;

        if(ai->Status()&CGeneModel::eRightFlexible) {
            matches = 0;
            not_wanted = true; 
            auto exonp = std::lower_bound(Exons().begin(), Exons().end(), alimits.GetFrom(), [](const CModelExon& e, TSignedSeqPos p){ return e.GetTo() < p; });
            alimits.SetTo(min(alimits.GetTo(), exonp->Limits().GetTo()));
            matches = alimits.GetLength();
        } else if(ai->Status()&CGeneModel::eLeftFlexible) {
            matches = 0;
            not_wanted = true; 
            auto exonp = std::upper_bound(Exons().begin(), Exons().end(), alimits.GetTo(), [](TSignedSeqPos p, const CModelExon& e){ return p < e.GetFrom(); })-1;
            alimits.SetFrom(max(alimits.GetFrom(), exonp->Limits().GetFrom()));
            matches = alimits.GetLength();
        } else {
            if(Exons().front().GetTo() < m_supported_range.GetFrom() || Exons().back().GetFrom() > m_supported_range.GetTo())  // some exons clipped
                not_wanted = true;
        }

        SLinker sl;
        sl.m_not_wanted = not_wanted;
        sl.m_member = mi;
        sl.m_matches = matches;
        sl.m_value = 1;
        if(ai->Status()&CGeneModel::eLeftFlexible)
            sl.m_value = (ai->Limits().GetTo() == Limits().GetTo()) ? 1000 : 10000;     // remove from support if possible; keep exact end if needed
        if(ai->Status()&CGeneModel::eRightFlexible)
            sl.m_value = (ai->Limits().GetFrom() == Limits().GetFrom()) ? 1000 : 10000; // remove from support if possible; keep exact end if needed
        sl.m_range = alimits&Limits();
        sl.m_reading_frame = ReadingFrame()&sl.m_range;
        linkers.push_back(sl);        
    }
        
    Status() &= ~CGeneModel::eChangedByFilter;

    set<TSignedSeqRange> chain_introns;
    for(int i = 1; i < (int)Exons().size(); ++i) {
        if(Exons()[i-1].m_ssplice && Exons()[i].m_fsplice)
            chain_introns.insert(TSignedSeqRange(Exons()[i-1].GetTo(),Exons()[i].GetFrom()));
    }

    for(SLinker& sl : linkers) {
        SChainMember* mi = sl.m_member;
        CGeneModel& align = *mi->m_align;
        if(mi->m_unmd_align) {
            CGeneModel& unma = *mi->m_unmd_align;
            bool all_introns_included = true;
            for(int i = 1; all_introns_included && i < (int)unma.Exons().size(); ++i) {
                if(unma.Exons()[i-1].m_ssplice && unma.Exons()[i].m_fsplice)
                    all_introns_included = chain_introns.count(TSignedSeqRange(unma.Exons()[i-1].GetTo(),unma.Exons()[i].GetFrom()));
            }
            if(!all_introns_included) {   // protein intron was clipped and not restored or part is not in chain
                sl.m_not_wanted = true;
                if(align.ID() == m_gapped_helper_align.ID())
                    Status() |= CGeneModel::eChangedByFilter;
            } 
        } else if(align.Status()&CGeneModel::eChangedByFilter) {  // for proteins could be restored
            sl.m_not_wanted = true;
        } else {
            CAlignModel& orig_align = *mi->m_orig_align;
            bool all_introns_included = true;
            for(int i = 1; all_introns_included && i < (int)orig_align.Exons().size(); ++i) {
                if(orig_align.Exons()[i-1].m_ssplice && orig_align.Exons()[i].m_fsplice)
                    all_introns_included = chain_introns.count(TSignedSeqRange(orig_align.Exons()[i-1].GetTo(),orig_align.Exons()[i].GetFrom()));
            }
            if(!all_introns_included) {   // intron was clipped by UTR clip or part is not in chain
                sl.m_not_wanted = true;
                if(align.Type()&eNotForChaining) // if TSA was clipped remove from support if possible
                    sl.m_value = 10000;
            } 
        }
    }

    if(m_gapped_helper_align.ID()) { // these linkers have m_member == nullptr
        int left = m_gapped_helper_align.Limits().GetFrom();
        for(int i = 0; i < (int)m_gapped_helper_align.Exons().size(); ++i) {
            if(!m_gapped_helper_align.Exons()[i].m_ssplice) {
                SLinker sl;
                sl.m_range = TSignedSeqRange(left,m_gapped_helper_align.Exons()[i].GetTo())&m_supported_range;
                sl.m_reading_frame = sl.m_range&ReadingFrame();
                if(sl.m_range.NotEmpty())
                    linkers.push_back(sl);
                
                if(i+1 < (int)m_gapped_helper_align.Exons().size())
                    left = m_gapped_helper_align.Exons()[i+1].GetFrom();
            }
        }

        for(int i = 1; i < (int)Exons().size(); ++i) {
            if(!Exons()[i-1].m_ssplice || !Exons()[i].m_fsplice) {
                SLinker sl;
                sl.m_range = TSignedSeqRange(Exons()[i-1].GetTo(),Exons()[i].GetFrom());
                sl.m_reading_frame = sl.m_range&ReadingFrame();
                linkers.push_back(sl);                
            }
        }
    }

    //    sort(linkers.begin(), linkers.end());

    multimap<TSignedSeqPos, SLinker*> right_ends;
    for(auto& linker : linkers)
        right_ends.emplace(linker.m_range.GetTo(), &linker); 

    for(auto it = right_ends.begin(); it != right_ends.end(); ++it) {
        SLinker& sli = *it->second;
        if(sli.m_range.GetFrom() == m_supported_range.GetFrom()) {
            sli.m_count = sli.m_value;
            sli.m_matches_count = sli.m_matches;
            if(sli.m_not_wanted)
                sli.m_not_wanted_count = sli.m_value;
            sli.m_connected = true;
        } else if(it != right_ends.begin()) {
            for(auto jt = prev(it); jt->first >= sli.m_range.GetFrom()-1; --jt) { // right end touching or inside
                SLinker& slj = *jt->second;
                if(slj.m_connected && slj.m_range.GetFrom() < sli.m_range.GetFrom()  && slj.m_range.GetTo() < sli.m_range.GetTo()) {  //overlaps and extends and connected to the left end
                    bool divided_pstop = false;
                    for(int is = 0; is < (int)GetCdsInfo().PStops().size() && !divided_pstop; ++is) {
                        const TSignedSeqRange& s = GetCdsInfo().PStops()[is];
                        divided_pstop = (Include(s,slj.m_range.GetTo()) || Include(s,sli.m_range.GetFrom())) && !Include(slj.m_reading_frame,s) && !Include(sli.m_reading_frame,s);
                    }
                    if(divided_pstop)  // both alignmnets just touch the pstop without actually crossing it
                        continue;

                    int new_count = slj.m_count + sli.m_value;
                    int new_matches_count = slj.m_matches_count + sli.m_matches;
                    int new_not_wanted_count = slj.m_not_wanted_count;
                    if(sli.m_not_wanted)
                        new_not_wanted_count += sli.m_value;
                    int new_mem_id = slj.m_member != nullptr ? slj.m_member->m_mem_id : 0;

                    if(!sli.m_connected || sli.NewIsBetter(new_count, new_not_wanted_count, new_matches_count, new_mem_id)) {
                        sli.m_count = new_count;
                        sli.m_matches_count = new_matches_count;
                        sli.m_not_wanted_count = new_not_wanted_count;
                        sli.m_connected = true;
                        sli.m_left = &slj;
                    }
                }
                if(jt == right_ends.begin())
                    break;
            }
        }
    }

    SLinker* best_right = 0;
    for(int i = 0; i < (int)linkers.size(); ++i) {
        SLinker& sli = linkers[i];
        if(sli.m_connected && sli.m_range.GetTo() == m_supported_range.GetTo()) {
            if(best_right == 0 || best_right->OtherIsBetter(sli))
                best_right = &sli;
        }
    }
    _ASSERT(best_right != 0);

    set<Int8> sp_core;
    set<Int8> sp_not_wanted;
    for(SLinker* l = best_right; l != 0; l = l->m_left) {
        if(l->m_member)
            sp_core.insert(l->m_member->m_align->ID());
    }
    if(m_gapped_helper_align.ID())
        sp_core.insert(m_gapped_helper_align.ID());
    if(!keep_all_evidence) {
        for(int i = 0; i < (int)linkers.size(); ++i) {
            SLinker& sli = linkers[i];
            if(sli.m_member && sli.m_not_wanted) {
                if(!sp_core.count(sli.m_member->m_align->ID()))
                    sp_not_wanted.insert(sli.m_member->m_align->ID());
                else
                    Status() |= CGeneModel::eChangedByFilter;
            }
        }    
    }        

    double weight = 0;
    m_splice_weight = 0;
    set<Int8> sp;
    TSignedSeqRange protreadingframe;
    ReplaceSupport(CSupportInfoSet());

    SetType(Type() & (~(eSR | eEST | emRNA | eProt | eNotForChaining)));
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        Int8 id = align.ID();
        if(!sp_not_wanted.count(id)) {
            SetType(Type() | (align.Type() & (eSR | eEST | emRNA | eProt | eNotForChaining)));
            protreadingframe += align.GetCdsInfo().ProtReadingFrame();       
            m_splice_weight += (*it)->m_splice_weight;            
            if(sp.insert(id).second) {   // avoid counting parts of splitted aligns
                weight += align.Weight();
                AddSupport(CSupportInfo(id,sp_core.count(id)));
            }
        }
    }
    
    CCDSInfo cds = GetCdsInfo();
    TSignedSeqRange readingframe = cds.ReadingFrame();
    protreadingframe &= readingframe;
    cds.SetReadingFrame(protreadingframe, true);
    cds.SetReadingFrame(readingframe, false);
    SetCdsInfo(cds);    
    SetWeight(weight);
}

void CChain::RestoreTrimmedEnds(int trim)
{
    // add back trimmed off UTRs    
    
    if(((Status()&eLeftConfirmed) == 0) && (!OpenLeftEnd() || ReadingFrame().Empty()) && (Strand() == ePlus || (Status()&ePolyA) == 0) && (Strand() == eMinus || (Status()&eCap) == 0)) {
        for(int ia = 0; ia < (int)m_members.size(); ++ia)  {
            const CGeneModel a = *m_members[ia]->m_align;
            if((a.Type() & eProt)==0 && (a.Status() & CGeneModel::eLeftTrimmed)!=0 &&
               a.Exons().size() > 1 && Exons().front().Limits().GetFrom() == a.Limits().GetFrom()) {
                ExtendLeft( trim );
                break;
            }
        }
    }
     
    if(((Status()&eRightConfirmed) == 0) && (!OpenRightEnd() || ReadingFrame().Empty()) && (Strand() == eMinus || (Status()&ePolyA) == 0) && (Strand() == ePlus || (Status()&eCap) == 0)) {
        for(int ia = 0; ia < (int)m_members.size(); ++ia)  {
            const CGeneModel a = *m_members[ia]->m_align;
            if((a.Type() & eProt)==0 && (a.Status() & CGeneModel::eRightTrimmed)!=0 &&
               a.Exons().size() > 1 && Exons().back().Limits().GetTo() == a.Limits().GetTo()) {
                ExtendRight( trim );
                break;
            }
        }
    }
}

void CChain::SetOpenForPartialyAlignedProteins(map<string, pair<bool,bool> >& prot_complet) {
    if(ConfirmedStart() || !HasStart() || !HasStop() || OpenCds() || !Open5primeEnd() || (Type()&CGeneModel::eProt) == 0)
        return;

    bool found_length_match = false;
    ITERATE (TContained, it, m_members) {
        CAlignModel* orig_align = (*it)->m_orig_align;
        _ASSERT(orig_align);
        if((orig_align->Type() & CGeneModel::eProt) == 0 || orig_align->TargetLen() == 0)   // not a protein or not known length
            continue;
    
        string accession = orig_align->TargetAccession();
        map<string, pair<bool,bool> >::iterator iter = prot_complet.find(accession);
        _ASSERT(iter != prot_complet.end());
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

bool CChain::RestoreReasonableConfirmedStart(const CGnomonEngine& gnomon, TOrigAligns& orig_aligns)
{
    //    if(ReadingFrame().Empty() || ConfirmedStart())
    if(ReadingFrame().Empty())
        return false;

    TSignedSeqRange conf_start;
    TSignedSeqPos rf=0; 
    bool trusted = false;

    CAlignMap amap = GetAlignMap();
    ITERATE(TOrigAligns, it, orig_aligns) {
        const CAlignModel& align = *it->second;
        if(align.Strand() != Strand() || !align.ConfirmedStart() || (align.TrustedProt().empty() && align.TrustedmRNA().empty()) || !(align.Status()&CGeneModel::eBestPlacement))
            continue;
            
        TSignedSeqRange start = align.GetCdsInfo().Start();

        int a = amap.MapOrigToEdited(start.GetFrom());
        int b = amap.MapOrigToEdited(start.GetTo());
        if(a < 0 || b < 0 || abs(a-b) != 2)
            continue;
            
        int l = GetCdsInfo().Cds().GetFrom();
        int r = start.GetFrom();
        if(l > r)
            swap(l,r);
        if(!Include(GetCdsInfo().MaxCdsLimits(),start) || amap.FShiftedLen(l,r)%3 != 1) 
            continue;

        list<TSignedSeqRange> align_introns;
        for(int i = 1; i < (int)align.Exons().size(); ++i) {
            TSignedSeqRange intron(align.Exons()[i-1].Limits().GetTo(),align.Exons()[i].Limits().GetFrom());
            if(Include(start,intron))
                align_introns.push_back(intron);
        }

        list<TSignedSeqRange> introns;
        bool hole = false;
        int len = start.GetLength();
        for(int i = 1; i < (int)Exons().size(); ++i) {
            TSignedSeqRange intron(Exons()[i-1].Limits().GetTo(),Exons()[i].Limits().GetFrom());
            if(Include(start,intron)) {
                introns.push_back(intron);
                len -= intron.GetLength()+2;
                if(!Exons()[i-1].m_ssplice || !Exons()[i].m_fsplice)
                    hole = true;
            }
        }

        if(len !=3 || hole || align_introns != introns)
            continue;

        if(Strand() == ePlus) {
            if(conf_start.Empty() || start.GetFrom() < conf_start.GetFrom()) {
                bool found = false;
                for(int i = 0; i < (int)Exons().size() && !found; ++i) {
                    if(Include(Exons()[i].Limits(),start.GetTo())) {
                        if(Exons()[i].Limits().GetTo() > start.GetTo()) {
                            rf = start.GetTo()+1;
                            found = true;
                        } else if(i != (int)Exons().size()-1) {
                            rf = Exons()[i+1].Limits().GetFrom();
                            found = true;
                        }
                    }
                }
                    
                if(found && amap.FShiftedLen(rf,GetCdsInfo().Cds().GetTo()) > 75) {
                    conf_start = start;
                    trusted = true;
                }
            }
        } else {
            if(conf_start.Empty() || start.GetTo() > conf_start.GetTo()) {
                bool found = false;
                for(int i = 0; i < (int)Exons().size() && !found; ++i) {
                    if(Include(Exons()[i].Limits(),start.GetFrom())) {
                        if(Exons()[i].Limits().GetFrom() < start.GetFrom()) {
                            rf = start.GetFrom()-1;
                            found = true;
                        } else if(i != 0) {
                            rf = Exons()[i-1].Limits().GetTo();
                            found = true;
                        }
                    }
                }

                if(found && amap.FShiftedLen(GetCdsInfo().Cds().GetFrom(),rf) > 75) {
                    conf_start = start;
                    trusted = true;
                }
            }
        }            
    }

    
    if(conf_start.Empty()) {
        ITERATE (TContained, it, m_members) {
            CAlignModel* orig_align = (*it)->m_orig_align;
            _ASSERT(orig_align);
        
            if(orig_align->ConfirmedStart() && Include((*it)->m_align->Limits(),orig_align->GetCdsInfo().Start())) {    // right part of orig is included
                TSignedSeqRange start = orig_align->GetCdsInfo().Start();
                int l = GetCdsInfo().Cds().GetFrom();
                int r = start.GetFrom();
                if(l > r)
                    swap(l,r);
                if(!Include(GetCdsInfo().MaxCdsLimits(),start) || amap.FShiftedLen(l,r)%3 != 1) // orig_align could be dropped beacause it was modified and have frameshifts between its start and 'best' start
                    continue;
                
                if(Strand() == ePlus) {
                    if(conf_start.Empty() || start.GetFrom() < conf_start.GetFrom()) {
                        conf_start = start;
                        rf = orig_align->ReadingFrame().GetFrom();
                    }
                } else {
                    if(conf_start.Empty() || start.GetTo() > conf_start.GetTo()) {
                        conf_start = start;
                        rf = orig_align->ReadingFrame().GetTo();
                    }
                }
            }
        }
    }


    if(conf_start.NotEmpty()) {
        TSignedSeqRange extra_cds;
        CCDSInfo cds = GetCdsInfo();
        if(cds.ProtReadingFrame().NotEmpty()) {
            if(Strand() == ePlus && cds.ProtReadingFrame().GetFrom() < conf_start.GetFrom())
                extra_cds = TSignedSeqRange(cds.ProtReadingFrame().GetFrom(), conf_start.GetFrom());
            else if(Strand() == eMinus && cds.ProtReadingFrame().GetTo() > conf_start.GetTo())
                extra_cds = TSignedSeqRange(conf_start.GetTo(), cds.ProtReadingFrame().GetTo());
        }
        if(extra_cds.Empty() || FShiftedLen(extra_cds) < 0.2*RealCdsLen()) {
            TSignedSeqRange reading_frame = cds.ReadingFrame();
            if(Strand() == ePlus) 
                reading_frame.SetFrom(rf);
            else
                reading_frame.SetTo(rf);
            TSignedSeqRange protreadingframe = cds.ProtReadingFrame();
            TSignedSeqRange stop = cds.Stop();
            bool confirmed_stop = cds.ConfirmedStop();
            CCDSInfo::TPStops pstops = cds.PStops();
            cds.Clear();

            if(protreadingframe.NotEmpty()) 
                cds.SetReadingFrame(reading_frame&protreadingframe, true);
            cds.SetReadingFrame(reading_frame);
            cds.SetStart(conf_start,true);
            if(stop.NotEmpty())
                cds.SetStop(stop,confirmed_stop);
            ITERATE(CCDSInfo::TPStops, s, pstops) {
                if(Include(reading_frame, *s))
                    cds.AddPStop(*s);
            }
            SetCdsInfo(cds);          

            TSignedSeqRange new_lim = Limits();
            for(int i = 1; i < (int)Exons().size(); ++i) {
                if(!Exons()[i-1].m_ssplice || !Exons()[i].m_fsplice) {
                    TSignedSeqRange hole(Exons()[i-1].GetTo(),Exons()[i].GetFrom());
                    if(Precede(hole,reading_frame)) {
                        new_lim.SetFrom(hole.GetTo());
                    } else if(Precede(reading_frame,hole)) {
                        new_lim.SetTo(hole.GetFrom());
                        break;
                    }
                }
            }
            if(new_lim != Limits())
                ClipChain(new_lim, false);   // remove holes from new UTRs            
          
            AddComment("Restored confirmed start");
            gnomon.GetScore(*this, false, trusted);
            RemoveFshiftsFromUTRs();
            return true;
        }
    }

    return false;
}

void CChain::RemoveFshiftsFromUTRs()
{
    TInDels fs;
    ITERATE(TInDels, i, FrameShifts()) {   // removing fshifts in UTRs
        TSignedSeqRange cds = GetCdsInfo().Cds();
        if(OpenCds())
            cds = MaxCdsLimits();
        if(Include(cds,i->Loc()))
            fs.push_back(*i);
    }
    if(FrameShifts().size() != fs.size()) {
        FrameShifts() = fs;
        if(!m_coverage.empty()) {
            int mrna_len = AlignLen();
            m_coverage.resize(mrna_len, m_coverage.back());   // this will slightly shift values compared to recalculation from scratch but will keep better ends
        }
    }
}


void CChain::ClipChain(TSignedSeqRange limits, bool recalulate_support) {

    _ASSERT(Include(Limits(),limits) && (RealCdsLimits().Empty() || Include(limits,RealCdsLimits())));

    TSignedSeqRange limits_on_mrna = GetAlignMap().MapRangeOrigToEdited(limits,false);
    _ASSERT(limits_on_mrna.NotEmpty());

    TContained new_members;
    ITERATE (TContained, it, m_members) {
        auto ai = (*it)->m_align;
        TSignedSeqRange alimits = ai->Limits();
        if(limits.IntersectingWith(alimits))   // not clipped
            new_members.push_back(*it);        
    }
    m_members = new_members;

    if(limits.GetFrom() > Limits().GetFrom()) {
        TSignedSeqRange clip_range(Limits().GetFrom(),limits.GetFrom()-1);
        CutExons(clip_range);
        RecalculateLimits();
    }
    if(limits.GetTo() < Limits().GetTo()) {
        TSignedSeqRange clip_range(limits.GetTo()+1,Limits().GetTo());
        CutExons(clip_range);
        RecalculateLimits();
    }

    if(!m_coverage.empty()) {
        if(limits_on_mrna.GetFrom() > 0)
            m_coverage.erase(m_coverage.begin(),m_coverage.begin()+limits_on_mrna.GetFrom());
        m_coverage.resize(limits_on_mrna.GetLength());
    }

    if(RealCdsLimits().NotEmpty()) {
        CCDSInfo cds = GetCdsInfo();
        bool changed = false;
        if((Strand() == ePlus && cds.MaxCdsLimits().GetFrom() < Limits().GetFrom()) ||
           (Strand() == eMinus && cds.MaxCdsLimits().GetTo() > Limits().GetTo())) {
            cds.Clear5PrimeCdsLimit();
            changed = true;
        }
        if(cds.PStop()) {
            CCDSInfo::TPStops pstops;
            for(auto& pstop : cds.PStops()) {
                if(Include(limits, pstop))
                   pstops.push_back(pstop);
            }
            if(pstops.size() != cds.PStops().size()) {
                cds.ClearPStops();
                for(auto& pstop : pstops)
                    cds.AddPStop(pstop);
                changed = true;
            }            
        }

        if(changed)
            SetCdsInfo(cds);
    }

    if(limits.GetFrom() > m_supported_range.GetFrom())
        m_supported_range.SetFrom(limits.GetFrom());
    if(limits.GetTo() < m_supported_range.GetTo())
        m_supported_range.SetTo(limits.GetTo());

    if(recalulate_support)
        CalculateSupportAndWeightFromMembers();
}

bool CChain::SetConfirmedEnds(const CGnomonEngine& gnomon, CGnomonAnnotator_Base::TIntMap& confirmed_ends) {
    if(Exons().size() < 2)
        return true;

    auto old_limits = Limits();
    auto new_limits = old_limits;
    bool left_confirmed = false;
    bool right_confirmed = false;

    auto rslt = confirmed_ends.find(Exons().front().GetTo());
    if(rslt != confirmed_ends.end() && rslt->second < Exons().front().GetTo()) {
        left_confirmed = true;
        new_limits.SetFrom(rslt->second);
    }
    rslt = confirmed_ends.find(Exons().back().GetFrom());
    if(rslt != confirmed_ends.end() && rslt->second > Exons().back().GetFrom()) {
        right_confirmed = true;
        new_limits.SetTo(rslt->second);
    }

    if(!left_confirmed && !right_confirmed)
        return true;
    else if(!Continuous())
        return false;

    CCDSInfo cds_info = GetCdsInfo();
    bool left_complete = LeftComplete();   // has start/stop on left
    bool right_complete = RightComplete(); // has start/stop on right

    SetCdsInfo(CCDSInfo()); //we will deal with CDS separately

    //extend chain
    if(new_limits.GetFrom() < old_limits.GetFrom()) {
        int delta = old_limits.GetFrom()-new_limits.GetFrom();
        ExtendLeft(delta);
        if(!m_coverage.empty())
            m_coverage.insert(m_coverage.begin(), delta, 0);
    }
    if(new_limits.GetTo() > old_limits.GetTo()) {
        int delta = new_limits.GetTo()-old_limits.GetTo();
        ExtendRight(delta);
        if(!m_coverage.empty())
            m_coverage.insert(m_coverage.end(), delta, 0);
    }

    CAlignMap amap = GetAlignMap(); //includes extended ends and keeps clipped ends  

    {   // removing fshifts outside of clip
        TInDels fs;
        ITERATE(TInDels, i, FrameShifts()) {
            if(i->Loc() > new_limits.GetFrom() && i->InDelEnd() < new_limits.GetTo())
                fs.push_back(*i);
        }

        if(FrameShifts().size() != fs.size()) {
            FrameShifts() = fs;
            if(!m_coverage.empty()) {
                int mrna_len = AlignLen();
                m_coverage.resize(mrna_len, m_coverage.back());   // this will slightly shift values compared to recalculation from scratch but will keep better ends       
            }
        }
    }
    
    //clip chain    
    if(Limits() != new_limits)
        ClipChain(new_limits, false);

    //set limits
    m_polya_cap_left_soft_limit = max(m_polya_cap_left_soft_limit, new_limits.GetFrom());
    m_polya_cap_right_soft_limit = min(m_polya_cap_right_soft_limit, new_limits.GetTo());

    //set status
    if(left_confirmed) {
        Status() |= eLeftConfirmed;
        if(new_limits.GetFrom() < old_limits.GetFrom())
            AddComment("Extended to confirmed left");
        else if(new_limits.GetFrom() > old_limits.GetFrom())
            AddComment("Clipped to confirmed left");
    }
    if(right_confirmed) {
        Status() |= eRightConfirmed;
        if(new_limits.GetTo() > old_limits.GetTo()) 
            AddComment("Extended to confirmed right");
        else if(new_limits.GetTo() < old_limits.GetTo())
            AddComment("Clipped to confirmed right");
    }

    if(cds_info.ReadingFrame().Empty()) //non coding chain
        return true;

    if(!Include(new_limits, cds_info.Cds()) || (left_confirmed && !left_complete) || (right_confirmed && !right_complete)) { //CDS may need clipping to expose startstop
        auto cds_info_t = cds_info.MapFromOrigToEdited(amap);
        int frame = cds_info_t.ReadingFrame().GetFrom()%3;

        //project new_limits to transcript and align to frame
        auto cds_limits_t = amap.ShrinkToRealPoints(new_limits);
        cds_limits_t = amap.MapRangeOrigToEdited(cds_limits_t, CAlignMap::eSinglePoint, CAlignMap::eSinglePoint);
        for(int i = cds_limits_t.GetFrom(); i <= cds_limits_t.GetTo(); ++i) {
            cds_limits_t.SetFrom(i);
            if(i%3 == frame && amap.MapEditedToOrig(i) >= 0)
                break;
        }
        for(int i = cds_limits_t.GetTo(); i >= cds_limits_t.GetFrom(); --i) {
            cds_limits_t.SetTo(i);
            if((i+1)%3 == frame && amap.MapEditedToOrig(i) >= 0)
                break;
        }
        if(cds_limits_t.Empty())
            return false;
        cds_info_t.Clip(cds_limits_t); // remove extra CDS


        bool fivep_confirmed = (Strand() == ePlus) ? left_confirmed : right_confirmed;
        bool threep_confirmed = (Strand() == ePlus) ? right_confirmed : left_confirmed;
        bool has_start = cds_info_t.HasStart();
        bool has_stop = cds_info_t.HasStop();
        auto prot_rf = cds_info_t.ProtReadingFrame();
        if(prot_rf.NotEmpty() && ((fivep_confirmed && !has_start) || (threep_confirmed && !has_stop))) { //CDS may need some additional clipping to expose starts/stops
            const CResidueVec& contig = gnomon.GetSeq();
            CResidueVec mrna;
            amap.EditedSequence(contig, mrna);

            auto IndelInCodon = [this](int i, CAlignMap& map) {
                int a = map.MapEditedToOrig(i);
                int b = map.MapEditedToOrig(i+2);
                if(Strand() == eMinus)
                    swap(a, b);
                return (a < 0 || b < 0 || map.MapEditedToOrig(i+1) < 0 || !GetInDels(a, b, false).empty()); // genomic indels inside, if true
            };

            if(fivep_confirmed && !has_start) {
                for(int i = prot_rf.GetFrom(); !has_start && i >= cds_limits_t.GetFrom() && !IsStopCodon(&mrna[i]); i -= 3) { //find start outside protein (no clip will be needed)
                    has_start =  IsStartCodon(&mrna[i]) && !IndelInCodon(i, amap);
                }
                for(int i = prot_rf.GetFrom(); !has_start && i < cds_limits_t.GetTo(); i += 3) {                              //find start inside protein (clip will be needed)
                    if(i > prot_rf.GetTo() && IsStopCodon(&mrna[i]))
                        break;
                    has_start =  IsStartCodon(&mrna[i]) && !IndelInCodon(i, amap);
                    cds_limits_t.SetFrom(i);
                }
                if(!has_start)
                    return false;
            }
            if(threep_confirmed && !has_stop) {
                for(int i = prot_rf.GetTo()+1; !has_stop && i < cds_limits_t.GetTo(); i += 3) //find stop outside protein (no clip will be needed)
                    has_stop = IsStopCodon(&mrna[i]) && !IndelInCodon(i, amap);
                if(!has_stop && cds_info_t.PStop(false)) {                                    //find stop inside protein (clip will be needed)
                    CCDSInfo::TPStops pstops = cds_info_t.PStops();
                    sort(pstops.begin(), pstops.end());
                    for(auto& stp : pstops) {
                        if(stp.m_status != CCDSInfo::eGenomeNotCorrect && stp.m_status != CCDSInfo::eSelenocysteine && !IndelInCodon(stp.GetFrom(), amap)) {
                            has_stop = true;
                            cds_limits_t.SetTo(stp.GetFrom()-1);
                        }
                    }
                    if(!has_stop)
                        return false;
                }
            }

            if(cds_limits_t.Empty())
                return false;

            cds_info_t.Clip(cds_limits_t);
        }
        
        cds_info = cds_info_t.MapFromEditedToOrig(amap);
    }

    {   // removing fshifts in UTRs  
        auto cds = cds_info.Cds();
        TInDels fs;
        ITERATE(TInDels, i, FrameShifts()) {
            if(Include(cds, i->Loc()))
                fs.push_back(*i);
        }

        if(FrameShifts().size() != fs.size()) {
            FrameShifts() = fs;
            if(!m_coverage.empty()) {
                int mrna_len = AlignLen();
                m_coverage.resize(mrna_len, m_coverage.back());   // this will slightly shift values compared to recalculation from scratch but will keep better ends       
            }
        }
    }
    
    SetCdsInfo(cds_info);    
 
    return true;
}

// valid, found As
pair<bool, bool> CChain::ValidPolyA(int pos, const CResidueVec& contig) {
    string motif1 = "AATAAA";
    string motif2 = "ATTAAA";
    string motif3 = "AGTAAA";
    int block_of_As_len = 6;
    CResidueVec block_of_As;
    if(Strand() == ePlus)
        block_of_As.assign(block_of_As_len, 'A');
    else
        block_of_As.assign(block_of_As_len, 'T');

    int a = max(0, pos-block_of_As_len);
    int b = min((int)contig.size()-1, pos+block_of_As_len);
    if(b-a+1 < block_of_As_len)
        return make_pair(false, false);
    if(search(contig.begin()+a, contig.begin()+b+1, block_of_As.begin(), block_of_As.end()) != contig.begin()+b+1) {  // found As
        int left;
        int right;
        if(Strand() == ePlus) {
            left = pos-35;
            right = pos-18;
        } else {
            left = pos+18;
            right = pos+35;
        }
        if(left < 0 || right >= (int)contig.size())
            return make_pair(false, false);

        string segment(contig.begin()+left, contig.begin()+right+1);
        if(Strand() == eMinus)
            ReverseComplement(segment.begin(), segment.end());

        if(segment.find(motif1) != string::npos || segment.find(motif2) != string::npos || segment.find(motif3) != string::npos)
            return make_pair(true, true);
        else
            return make_pair(false, true);
    } else {
        return make_pair(true, false);
    }
}

#define MIN_UTR_EXON 15

tuple<CChain::TIDMap, TSignedSeqRange> CChain::PeaksAndLimits(EStatus determinant, int min_blob_weight, int max_empty_dist, int min_splice_dist) {
    bool right_end = (determinant == ePolyA && Strand() == ePlus) || (determinant == eCap && Strand() == eMinus); // determinant is on the right gene side
    bool coding = ReadingFrame().NotEmpty();

    TIDMap peak_weights;
    TSignedSeqRange real_limits;

    int flex_len = 0;
    TIDMap raw_weights;
    for(auto& mi : m_members) {        
        const CGeneModel& align = *mi->m_align;        
        if(align.Status()&determinant) {
            if(right_end) {
                int rlimit = (coding ? RealCdsLimits().GetTo() : Exons().back().Limits().GetFrom());      // look in the last exon of notcoding or right UTR of coding
                //                int rlimit = (coding ? RealCdsLimits().GetTo() : Limits().GetFrom());      // look everywhere of notcoding or right UTR of coding
                bool belong_to_exon = false;
                int pos = align.Limits().GetTo();
                for(auto& exon : Exons()) {
                    if(pos >= exon.Limits().GetFrom()+min_splice_dist && pos <= exon.Limits().GetTo()) {
                        belong_to_exon = true;
                        break;
                    }
                }
                if(rlimit < pos && belong_to_exon)
                    raw_weights[align.Limits().GetTo()] += align.Weight();
            } else {
                int llimit = (coding ? RealCdsLimits().GetFrom() : Exons().front().Limits().GetTo());     // look in the first exon of notcoding or left UTR of coding
                //                int llimit = (coding ? RealCdsLimits().GetFrom() : Limits().GetTo());     // look everywhere of notcoding or left UTR of coding
                bool belong_to_exon = false;
                int pos = align.Limits().GetFrom();
                for(auto& exon : Exons()) {
                    if(pos >= exon.Limits().GetFrom() && pos <= exon.Limits().GetTo()-min_splice_dist) {
                        belong_to_exon = true;
                        break;
                    }
                }
                if(llimit > pos && belong_to_exon)
                    raw_weights[-align.Limits().GetFrom()] += align.Weight();                             // negative position, so the map is in convinient order
            }
        }
        if(align.Status()&(eLeftFlexible|eRightFlexible))
            flex_len = max(flex_len, align.Limits().GetLength());
        else
            real_limits += (align.Limits()&Limits());
    }
    if(raw_weights.empty())
        return make_tuple(peak_weights,real_limits);

    int last_allowed = right_end ? real_limits.GetTo()+flex_len : -(real_limits.GetFrom()-flex_len);
    auto ipeak = raw_weights.begin();
    double w = ipeak->second;
    for(auto it = next(raw_weights.begin()); it != raw_weights.end(); ++it) {
        if(it->first > prev(it)->first+1+max_empty_dist) {           // next blob
            if(ipeak->first > last_allowed)
                break;
            if(w >= min_blob_weight) {
                auto still_good = ipeak;
                for(auto i = ipeak; i != it && i->first <= last_allowed; ++i) { // shift position to furthest 50% within blob
                    if(i->second >= 0.5*ipeak->second)
                        still_good = i;
                }
                peak_weights.emplace(still_good->first, w);      // peak position, blob weight
            }    
            ipeak = it;
            w = it->second;
        } else {
            w += it->second;
            if(it->second > ipeak->second)            // new peak position; first for equals
                ipeak = it;
        }
    }
    if(ipeak->first <= last_allowed && w >= min_blob_weight) { // last peak
        auto still_good = ipeak;
        for(auto i = ipeak; i != raw_weights.end() && i->first <= last_allowed; ++i) { // shift position to furthest 50% within blob
            if(i->second >= 0.5*ipeak->second)
                still_good = i;
        }
        peak_weights.emplace(still_good->first, w);      // peak position, blob weight
    }

    return make_tuple(peak_weights,real_limits);
}

tuple<TIVec, TSignedSeqRange> CChain::MainPeaks(TIDMap& peak_weights, double secondary_peak, double tertiary_peak, double tertiary_peak_coverage, bool right_end) {
    TIVec peaks(3, -1);
    auto limits = Limits();
    auto ifirst_peak = max_element(peak_weights.begin(), peak_weights.end(), [](const TIDMap::value_type& a, const TIDMap::value_type& b) { return a.second < b.second; });
    peaks[0] = abs(ifirst_peak->first);
    if(right_end) {
        int first_peak = ifirst_peak->first;
        limits.SetTo(first_peak);
        m_polya_cap_right_soft_limit = first_peak;
    } else {
        int first_peak = -ifirst_peak->first;
        limits.SetFrom(first_peak);
        m_polya_cap_left_soft_limit = first_peak;
    }

    auto isecond_peak = prev(peak_weights.end());
    for( ; isecond_peak != ifirst_peak && isecond_peak->second < secondary_peak*ifirst_peak->second; --isecond_peak);
    if(isecond_peak != ifirst_peak) 
        peaks[1] = abs(isecond_peak->first);

    if(tertiary_peak > 0) {
        CAlignMap amap = GetAlignMap();
        TSignedSeqRange genome_core_lim = RealCdsLimits();
        if(genome_core_lim.Empty()) {
            genome_core_lim = Limits();
            if(Exons().size() > 1) {
                if(Exons().front().Limits().GetLength() >= MIN_UTR_EXON)
                    genome_core_lim.SetFrom(Exons().front().Limits().GetTo()-MIN_UTR_EXON+1);
                if(Exons().back().Limits().GetLength() >= MIN_UTR_EXON)
                    genome_core_lim.SetTo(Exons().back().Limits().GetFrom()+MIN_UTR_EXON-1);
            }            
        }
        genome_core_lim = amap.ShrinkToRealPoints(genome_core_lim);
        TSignedSeqRange core_lim = amap.MapRangeOrigToEdited(genome_core_lim);
        double core_coverage = 0;
        for (int i = core_lim.GetFrom(); i <= core_lim.GetTo(); ++i) {
            core_coverage += m_coverage[i];
        }
        core_coverage /= core_lim.GetLength();

        TSignedSeqRange fpeak_exon;
        for(auto& exon : Exons()) {
            if(Include(exon.Limits(), abs(ifirst_peak->first))) {
                fpeak_exon = exon.Limits();
                break;
            }
        }

        auto ithird_peak = prev(peak_weights.end());
        for( ; ithird_peak != isecond_peak; --ithird_peak) {
            if(Include(fpeak_exon, abs(ithird_peak->first))) {
                int p = amap.MapOrigToEdited(abs(ithird_peak->first));
                if(p < 0)
                    continue;
                if(ithird_peak->second >= tertiary_peak*ifirst_peak->second && m_coverage[p] > tertiary_peak_coverage*core_coverage)
                    break;
            }
        }
        if(ithird_peak != isecond_peak) 
            peaks[2] = abs(ithird_peak->first);
        isecond_peak = ithird_peak;
    }

    if(isecond_peak != ifirst_peak) {
        if(right_end) {
            int second_peak = isecond_peak->first;
            limits.SetTo(second_peak);
        } else {
            int second_peak = -isecond_peak->first;
            limits.SetFrom(second_peak);
        }
    }

    return make_tuple(peaks, limits);
}

void CChain::ClipToCap(int min_cap_blob, int max_dist, int min_flank_exon, double secondary_peak, bool recalulate_support) {
    bool right_end = Strand() == eMinus; // cap is on the right gene side
    if((Status()&eLeftConfirmed) && !right_end)
        return;
    if((Status()&eRightConfirmed) && right_end)
        return;

    bool coding = ReadingFrame().NotEmpty();
    if(!HasStart() && coding)
        return;

    auto rslt = PeaksAndLimits(eCap, min_cap_blob, max_dist, min_flank_exon);
    TIDMap& peak_weights(get<0>(rslt));
    TSignedSeqRange real_limits(get<1>(rslt));
    if(peak_weights.empty()) {
        TSignedSeqRange limits = Limits();
        Status() &= ~eCap;
        if(right_end && real_limits.GetTo() < Limits().GetTo())
            limits.SetTo(real_limits.GetTo());
        else if(!right_end && real_limits.GetFrom() > Limits().GetFrom())
            limits.SetFrom(real_limits.GetFrom());

        if (limits != Limits()) {
            if(!coding || Include(limits,RealCdsLimits())) {
                AddComment("capsupressed");
                ClipChain(limits, recalulate_support);
            } else {
                AddComment("capoverlapcds");
            }
        } 

        if(right_end)
            m_polya_cap_right_soft_limit = Limits().GetFrom()-1;
        else
            m_polya_cap_left_soft_limit = Limits().GetTo()+1;
        
        return;
    }

    Status() |= eCap;
    auto rslt1 = MainPeaks(peak_weights, secondary_peak, 0., 0., right_end);
    m_cap_peaks = get<0>(rslt1);
    TSignedSeqRange limits = get<1>(rslt1);
    if (limits != Limits()) {
        AddComment("capclip");
        ClipChain(limits, recalulate_support);
    } 
}
void CChain::ClipToPolyA(const CResidueVec& contig, int min_polya_blob, int max_dist, int min_flank_exon, double secondary_peak, double tertiary_peak, double tertiary_peak_coverage, bool recalulate_support) {
    bool right_end = Strand() == ePlus; // polya is on the right gene side
    if((Status()&eLeftConfirmed) && !right_end)
        return;
    if((Status()&eRightConfirmed) && right_end)
        return;

    bool coding = ReadingFrame().NotEmpty();
    if(!HasStop() && coding)
        return;

    auto rslt = PeaksAndLimits(ePolyA, min_polya_blob, max_dist, min_flank_exon);
    TIDMap& peak_weights(get<0>(rslt));
    TSignedSeqRange real_limits(get<1>(rslt));
    //check for As
    for(auto ip_loop = peak_weights.begin(); ip_loop != peak_weights.end(); ) {
        auto ip = ip_loop++;
        if(!ValidPolyA(abs(ip->first), contig).first)
            peak_weights.erase(ip); 
    }

    if(peak_weights.empty()) {
        TSignedSeqRange limits = Limits();
        Status() &= ~ePolyA;
        if(right_end && real_limits.GetTo() < Limits().GetTo())
            limits.SetTo(real_limits.GetTo());
        else if(!right_end && real_limits.GetFrom() > Limits().GetFrom())
            limits.SetFrom(real_limits.GetFrom());

        if (limits != Limits()) {
            if(!coding || Include(limits,RealCdsLimits())) {
                AddComment("polyasupressed");
                ClipChain(limits, recalulate_support);
            } else {
                AddComment("polyaoverlapcds");
            }
        } 

        if(right_end)
            m_polya_cap_right_soft_limit = Limits().GetFrom()-1;
        else
            m_polya_cap_left_soft_limit = Limits().GetTo()+1;
        
        return;
    }

    Status() |= ePolyA;
    auto rslt1 = MainPeaks(peak_weights, secondary_peak, tertiary_peak, tertiary_peak_coverage, right_end);
    m_polya_peaks = get<0>(rslt1);
    TSignedSeqRange limits = get<1>(rslt1);

    if (limits != Limits()) {
        AddComment("polyaclip");
        ClipChain(limits, recalulate_support);
    } 
}

void CChain::CheckSecondaryCapPolyAEnds() {
    if(m_polya_cap_left_soft_limit < Limits().GetTo() && Include(RealCdsLimits(), m_polya_cap_left_soft_limit))
        m_polya_cap_left_soft_limit = Limits().GetFrom();
    
    if(m_polya_cap_right_soft_limit > Limits().GetFrom() && Include(RealCdsLimits(), m_polya_cap_right_soft_limit))
        m_polya_cap_right_soft_limit = Limits().GetTo();
}

#define COVERAGE_DROP 0.1
#define COVERAGE_BUMP 3
#define SMALL_GAP_UTR 100

void CChain::ClipLowCoverageUTR(double utr_clip_threshold, bool recalulate_support)
{
    if((Type()&CGeneModel::eSR) == 0)   // don't have SR coverage
        return;

    CAlignMap amap = GetAlignMap();

    int mrna_len = amap.FShiftedLen(Limits());
    
    TSignedSeqRange genome_core_lim;
    if(ReadingFrame().NotEmpty()) {
        if(OpenCds())
            genome_core_lim = MaxCdsLimits();
        else
            genome_core_lim = RealCdsLimits();
        ITERATE (CGeneModel::TExons, e, Exons()) {
            if(Include(e->Limits(),genome_core_lim.GetFrom()))
                genome_core_lim.SetFrom(max(genome_core_lim.GetFrom()-MIN_UTR_EXON,e->GetFrom()));
            if(Include(e->Limits(),genome_core_lim.GetTo()))
                genome_core_lim.SetTo(min(genome_core_lim.GetTo()+MIN_UTR_EXON,e->GetTo()));
        }
    } else {
        genome_core_lim = Limits();
        if(Exons().size() > 1) {
            if(Exons().front().Limits().GetLength() >= MIN_UTR_EXON)
                genome_core_lim.SetFrom(Exons().front().Limits().GetTo()-MIN_UTR_EXON+1);
            if(Exons().back().Limits().GetLength() >= MIN_UTR_EXON)
                genome_core_lim.SetTo(Exons().back().Limits().GetFrom()+MIN_UTR_EXON-1);
        }
    }

    TSignedSeqRange core_lim = amap.MapRangeOrigToEdited(genome_core_lim);

    vector<double> coverage = m_coverage;
    _ASSERT((int)coverage.size() == mrna_len && core_lim.GetFrom() >= 0 && core_lim.GetTo() < mrna_len);

    double core_coverage = 0;
    for (int i = core_lim.GetFrom(); i <= core_lim.GetTo(); ++i) {
        core_coverage += coverage[i];
    }
    core_coverage /= core_lim.GetLength();
    m_core_coverage = core_coverage;

    if(core_lim.GetFrom() <= 0 &&  core_lim.GetTo() >= mrna_len-1)   //nothing to clip
        return;

    if(core_lim.GetTo()-core_lim.GetFrom() < SCAN_WINDOW)      // too short
        return;

    map<int,double> intron_coverage;   // in transcript space
    vector<double> longseq_coverage(mrna_len);
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        if(align.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;
        TSignedSeqRange overlap = Limits()&align.Limits();
        if(overlap.Empty())   // some could be cut by polya clip
            continue;

        for(int i = 1; i < (int)align.Exons().size(); ++i) {
            if(align.Exons()[i-1].m_ssplice && align.Exons()[i].m_fsplice && align.Exons()[i-1].m_ssplice_sig != "XX" && align.Exons()[i].m_fsplice_sig != "XX") {
                TSignedSeqRange intr(align.Exons()[i-1].Limits().GetTo(),align.Exons()[i].Limits().GetFrom());
                bool valid_intron = false;                                     // some introns might be clipped by previous UTR clips but still be in members
                for(int j = 1; j < (int)Exons().size() && !valid_intron; ++j) {
                    if(Exons()[j-1].m_ssplice && Exons()[j].m_fsplice) {
                        TSignedSeqRange jntr(Exons()[j-1].Limits().GetTo(),Exons()[j].Limits().GetFrom());
                        valid_intron = (intr == jntr);
                    }
                }
                if(valid_intron) {
                    int intron = 0;   // donor in transcript space
                    if(Strand() == ePlus) {
                        intron = amap.MapRangeOrigToEdited(Limits()&align.Exons()[i-1].Limits()).GetTo();
                    } else {
                        intron = amap.MapRangeOrigToEdited(Limits()&align.Exons()[i].Limits()).GetTo();
                    }
                    intron_coverage[intron] += (align.Type() == CGeneModel::eSR) ? align.Weight() : 0;
                }
            }
        }
        

        TSignedSeqRange overlap_on_mrna = amap.MapRangeOrigToEdited(overlap);

        if(align.Type() == CGeneModel::emRNA || align.Type() == CGeneModel::eEST || align.Type() == CGeneModel::eNotForChaining) {   //OK to clip protein in UTR
            for(int i = overlap_on_mrna.GetFrom(); i <= overlap_on_mrna.GetTo(); ++i)
                longseq_coverage[i] += align.Weight();
        }
    }

    //don't save short gap utrs
    TSignedSeqRange cds = GetCdsInfo().Cds();
    if(Exons().front().m_ssplice_sig == "XX" && (cds&Exons().front().Limits()).Empty() && Exons().front().Limits().GetLength() < SMALL_GAP_UTR) {
        TSignedSeqRange texon = TranscriptExon(0);
        for(int i = texon.GetFrom(); i <= texon.GetTo(); ++i) {
            coverage[i] = 0;
            longseq_coverage[i] = 0;
        }
    }
    if(Exons().back().m_fsplice_sig == "XX" && (cds&Exons().back().Limits()).Empty() && Exons().back().Limits().GetLength() < SMALL_GAP_UTR) {
        TSignedSeqRange texon = TranscriptExon((int)Exons().size()-1);
        for(int i = texon.GetFrom(); i <= texon.GetTo(); ++i) {
            coverage[i] = 0;
            longseq_coverage[i] = 0;
        }
    }

    double core_inron_coverage = 0;
    int core_introns = 0;
    for(int i = 1; i < (int)Exons().size(); ++i) {
        if(Exons()[i-1].m_ssplice && Exons()[i].m_fsplice) {
            int intron;   // donor in transcript space
            if(Strand() == ePlus) 
                intron = amap.MapRangeOrigToEdited(Exons()[i-1].Limits(), true).GetTo();
            else
                intron = amap.MapRangeOrigToEdited(Exons()[i].Limits(), true).GetTo();
            if(Include(core_lim, intron)) {
                ++core_introns;
                core_inron_coverage += intron_coverage[intron];
            }
        }
    }
    if(core_introns > 0) 
        core_inron_coverage /= core_introns;
    else
        core_inron_coverage = 0.5*core_coverage;

    // 5' UTR
    bool fivep_confirmed = (Strand() == ePlus) ? (Status()&eLeftConfirmed) : (Status()&eRightConfirmed);
    if(!fivep_confirmed && !(Status()&eCap) && core_lim.GetFrom() > SCAN_WINDOW/2) {
        int left_limit = core_lim.GetFrom(); // cds/splice
        int right_limit = core_lim.GetTo();  // cds/splice
        int len = right_limit-left_limit+1;
        double wlen = 0;
        for(int i = left_limit; i <= right_limit; ++i)
            wlen += coverage[i];

        while(left_limit > 0 && (longseq_coverage[left_limit] > 0 || 
               (coverage[left_limit] > max(core_coverage,wlen/len)*utr_clip_threshold &&
               (intron_coverage.find(left_limit-1) == intron_coverage.end() || intron_coverage[left_limit-1] > core_inron_coverage*utr_clip_threshold)))) {

            ++len;
            --left_limit;
            wlen += coverage[left_limit];
        }

        if(left_limit > 0) {
            AddComment("5putrclip");
            ClipChain(amap.MapRangeEditedToOrig(TSignedSeqRange(left_limit,mrna_len-1)), recalulate_support);
            if(Strand() == ePlus && Exons().front().Limits().GetLength() < MIN_UTR_EXON && Exons().front().Limits().GetTo() < genome_core_lim.GetFrom())
                ClipChain(TSignedSeqRange(Exons()[1].Limits().GetFrom(),Limits().GetTo()), recalulate_support);
            else if(Strand() == eMinus && Exons().back().Limits().GetLength() < MIN_UTR_EXON && Exons().back().Limits().GetFrom() > genome_core_lim.GetTo())
                ClipChain(TSignedSeqRange(Limits().GetFrom(),Exons()[Exons().size()-2].GetTo()), recalulate_support);
        }
    }
    

    // 3' UTR
    bool threep_confirmed = (Strand() == ePlus) ? (Status()&eRightConfirmed) : (Status()&eLeftConfirmed);
    if(!threep_confirmed && !(Status()&ePolyA) && core_lim.GetTo() < mrna_len-1-SCAN_WINDOW/2) {
        int right_limit = core_lim.GetTo();     // cds/splice
        int left_limit = core_lim.GetFrom();    // cds/splice
        int len = right_limit-left_limit+1;
        double wlen = 0;
        for(int i = left_limit; i <= right_limit; ++i)
            wlen += coverage[i];
            
        while(right_limit < mrna_len-1 && (longseq_coverage[right_limit] > 0 || 
              (coverage[right_limit] > wlen/len*utr_clip_threshold &&
              (intron_coverage.find(right_limit) == intron_coverage.end() || intron_coverage[right_limit] > core_inron_coverage*utr_clip_threshold)))) {

            ++len;
            ++right_limit;
            wlen += coverage[right_limit];
        }

        if(right_limit < mrna_len-1) {
            AddComment("3putrclip");
            int new_5p = amap.MapRangeOrigToEdited(Limits()).GetFrom();
            ClipChain(amap.MapRangeEditedToOrig(TSignedSeqRange(new_5p,right_limit)), recalulate_support);
            if(Strand() == ePlus && Exons().back().Limits().GetLength() < MIN_UTR_EXON && Exons().back().Limits().GetFrom() > genome_core_lim.GetTo())
                ClipChain(TSignedSeqRange(Limits().GetFrom(),Exons()[Exons().size()-2].GetTo()), recalulate_support);
            else if(Strand() == eMinus && Exons().front().Limits().GetLength() < MIN_UTR_EXON && Exons().front().Limits().GetTo() < genome_core_lim.GetFrom())
                ClipChain(TSignedSeqRange(Exons()[1].Limits().GetFrom(),Limits().GetTo()), recalulate_support);
        }
    }
}

void CChain::CalculateDropLimits() {

    m_coverage_drop_left = -1;
    m_coverage_drop_right = -1;
    m_coverage_bump_left = -1;
    m_coverage_bump_right = -1;

    bool fivep_confirmed = (Strand() == ePlus) ? (Status()&eLeftConfirmed) : (Status()&eRightConfirmed);
    bool threep_confirmed = (Strand() == ePlus) ? (Status()&eRightConfirmed) : (Status()&eLeftConfirmed);

    if(fivep_confirmed && threep_confirmed)
        return;

    CAlignMap amap = GetAlignMap();

    int mrna_len = amap.FShiftedLen(Limits());

    vector<double> longseq_coverage(mrna_len);
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        if(align.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;
        TSignedSeqRange overlap = Limits()&align.Limits();
        if(overlap.Empty())
            continue;

        TSignedSeqRange overlap_on_mrna = amap.MapRangeOrigToEdited(overlap);

        if(align.Type() == CGeneModel::emRNA || align.Type() == CGeneModel::eEST) {   //OK to clip protein in UTR
            for(int i = overlap_on_mrna.GetFrom(); i <= overlap_on_mrna.GetTo(); ++i)
                longseq_coverage[i] += align.Weight();
        }
    }

    TSignedSeqRange sfl(Exons().front().Limits().GetTo(),Exons().back().Limits().GetFrom());
    if(ReadingFrame().NotEmpty()) {
        TSignedSeqRange cds = (OpenCds() ? MaxCdsLimits() : RealCdsLimits());
        sfl.SetFrom(min(sfl.GetFrom(),cds.GetFrom()));
        sfl.SetTo(max(sfl.GetTo(),cds.GetTo()));
    }
    TSignedSeqRange soft_limit = sfl;
    ITERATE(TContained, i, m_members) {
        if((*i)->m_align->Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;
        TSignedSeqRange overlap = ((*i)->m_align->Limits() & Limits());
        if(Include(overlap,sfl.GetFrom()+1))
            soft_limit.SetFrom(min(soft_limit.GetFrom(),overlap.GetFrom()));
        if(Include(overlap,sfl.GetTo()-1))
            soft_limit.SetTo(max(soft_limit.GetTo(),overlap.GetTo()));
    }
    soft_limit.SetFrom(min(soft_limit.GetFrom(),m_polya_cap_left_soft_limit));
    soft_limit.SetTo(max(soft_limit.GetTo(),m_polya_cap_right_soft_limit));

    soft_limit = amap.MapRangeOrigToEdited(soft_limit);

    // 5' UTR
    if(!fivep_confirmed) {
        int left_limit = soft_limit.GetFrom();
        int first_bump = -1;
        double max_cov = 0;
        while(left_limit > 0 && first_bump < 0 && (longseq_coverage[left_limit] > 0 || m_coverage[left_limit] > m_core_coverage*COVERAGE_DROP)) {
            max_cov = max(max_cov,m_coverage[left_limit]);
            if(max_cov > m_core_coverage*COVERAGE_BUMP)
                first_bump = left_limit;

            --left_limit;
        }

        if(first_bump > 0) {
            for( ; first_bump < soft_limit.GetFrom()-SCAN_WINDOW && m_coverage[first_bump+SCAN_WINDOW] < m_coverage[first_bump]; ++first_bump);
            if(Strand() == ePlus)
                m_coverage_bump_left = amap.MapEditedToOrig(first_bump);
            else
                m_coverage_bump_right = amap.MapEditedToOrig(first_bump);
        } else if(left_limit > 0 || m_coverage[left_limit] <= m_core_coverage*COVERAGE_DROP) {
            int first_drop = left_limit;
            if(first_drop+SCAN_WINDOW/2 < mrna_len) {
                for( ; first_drop-SCAN_WINDOW/2 > 0; --first_drop) {
                    if(m_coverage[first_drop-SCAN_WINDOW/2] >= m_coverage[first_drop+SCAN_WINDOW/2])  // check for negative gradient    
                        break;
                    if(m_coverage[first_drop-SCAN_WINDOW/2]+m_coverage[first_drop+SCAN_WINDOW/2]-2*m_coverage[first_drop] >= 0)  // check for decrease of gradient  
                        break;
                }
            }
            if(Strand() == ePlus)
                m_coverage_drop_left = amap.MapEditedToOrig(first_drop);
            else
                m_coverage_drop_right = amap.MapEditedToOrig(first_drop);
        }
    }

    // 3' UTR
    if(!threep_confirmed) {
        int right_limit = soft_limit.GetTo();
        int first_bump = -1;
        double max_cov = 0;
        while(right_limit < mrna_len-1 && first_bump < 0 && (longseq_coverage[right_limit] > 0 || m_coverage[right_limit] > m_core_coverage*COVERAGE_DROP)) {
            max_cov = max(max_cov,m_coverage[right_limit]);
            if(first_bump < 0 && max_cov > m_core_coverage*COVERAGE_BUMP)
                first_bump = right_limit;

            ++right_limit;
        }
        if(first_bump > 0) {
            for( ; first_bump > soft_limit.GetTo()+SCAN_WINDOW && m_coverage[first_bump-SCAN_WINDOW] < m_coverage[first_bump]; --first_bump);
            if(Strand() == ePlus)
                m_coverage_bump_right = amap.MapEditedToOrig(first_bump);
            else
                m_coverage_bump_left = amap.MapEditedToOrig(first_bump);
        } else if(right_limit < mrna_len-1 || m_coverage[right_limit] <= m_core_coverage*COVERAGE_DROP) {  // garanteed that right_limit <= mrna_len-1  
            int first_drop = right_limit;
            if(first_drop-SCAN_WINDOW/2 > 0) {
                for( ; first_drop < mrna_len-SCAN_WINDOW/2; ++first_drop) {
                    if(m_coverage[first_drop+SCAN_WINDOW/2] >= m_coverage[first_drop-SCAN_WINDOW/2])  // check for negative gradient    
                        break;
                    if(m_coverage[first_drop-SCAN_WINDOW/2]+m_coverage[first_drop+SCAN_WINDOW/2]-2*m_coverage[first_drop] >= 0)  // check for decrease of gradient  
                        break;                    
                }
            }
            if(Strand() == ePlus)
                m_coverage_drop_right = amap.MapEditedToOrig(first_drop);
            else
                m_coverage_drop_left = amap.MapEditedToOrig(first_drop);
        }
    }
}

void CChain::SetConsistentCoverage()
{
    if(!(Type()&CGeneModel::eSR))
        return;

    CAlignMap amap = GetAlignMap();
    int mrna_len = amap.FShiftedLen(Limits());
    map<TSignedSeqRange,double> intron_coverage;
    vector<double> coverage(mrna_len);
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        if(align.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            continue;
        TSignedSeqRange overlap = Limits()&align.Limits();
        if(overlap.Empty())   // some could be cut by polya clip
            continue;

        if(align.Type() == CGeneModel::eSR) {
            TSignedSeqRange overlap_on_mrna = amap.MapRangeOrigToEdited(overlap);
            for(int i = overlap_on_mrna.GetFrom(); i <= overlap_on_mrna.GetTo(); ++i)
                coverage[i] += align.Weight();
        }
        
        for(int i = 1; i < (int)align.Exons().size(); ++i) {
            if(align.Exons()[i-1].m_ssplice_sig != "XX" && align.Exons()[i].m_fsplice_sig != "XX") {
                TSignedSeqRange intron(align.Exons()[i-1].Limits().GetTo(),align.Exons()[i].Limits().GetFrom());
                if(Include(Limits(),intron))
                    intron_coverage[intron] += (align.Type() == CGeneModel::eSR) ? align.Weight() : 0;
            }
        }
    }

    double minintroncount = numeric_limits<double>::max();
    double maxintroncount = 0;
    for(map<TSignedSeqRange,double>::iterator it = intron_coverage.begin(); it != intron_coverage.end(); ++it) {
        minintroncount = min(minintroncount,it->second);
        maxintroncount = max(maxintroncount,it->second);
    }
    if(minintroncount < 0.1*maxintroncount)
        return;

    vector<int> dips(mrna_len,0);
    double maxsofar = 0;
    for(int i = 0; i < mrna_len; ++i) {
        if(coverage[i] < 0.1*maxsofar)
            dips[i] = 1;
        maxsofar = max(maxsofar,coverage[i]);
    }
    for(int i = 0; i < (int)Exons().size(); ++i) {
        if(Exons()[i].m_fsplice_sig == "XX" || Exons()[i].m_ssplice_sig == "XX") {
            TSignedSeqRange te = amap.MapRangeOrigToEdited(Exons()[i].Limits(),false);
            _ASSERT(te.NotEmpty());
            for(int p = max(0,te.GetFrom()-50); p <= min(mrna_len-1,te.GetTo()+50); ++p)
                dips[p] = 0;
        }
    }
    maxsofar = 0;
    for(int i = mrna_len-1; i >= 0; --i) {
        if(coverage[i] < 0.1*maxsofar && dips[i] > 0)
            return;
        maxsofar = max(maxsofar,coverage[i]);
    }

    if(intron_coverage.size() > 1)
        Status() |= eConsistentCoverage;
}

void CChain::SetConfirmedStartStopForCompleteProteins(map<string, pair<bool,bool> >& prot_complet, const SMinScor& minscor)
{
    if(ConfirmedStart() && ConfirmedStop())
        return;

    bool setconfstart = false;
    bool setconfstop = false;

    CAlignMap mrnamap = GetAlignMap();
    ITERATE(TContained, i, m_members) {

        if((*i)->m_align->GetCdsInfo().ProtReadingFrame().Empty())            // not known CDS
            continue;

        if((*i)->m_align->Type() & emRNA) {
            if(!ConfirmedStart() && HasStart())
                setconfstart = true;
            if(!ConfirmedStop() && HasStop())
                setconfstop = true; 
        } else {
            CAlignModel* orig_align = (*i)->m_orig_align;        
            if(orig_align->TargetLen() == 0)   // protein of not known length
                continue;

            string accession = orig_align->TargetAccession();
            map<string, pair<bool,bool> >::iterator iter = prot_complet.find(accession);
            _ASSERT(iter != prot_complet.end());
            if(iter == prot_complet.end())
                continue;

            TSignedSeqRange fivep_exon = orig_align->Exons().front().Limits();
            TSignedSeqRange threep_exon = orig_align->Exons().back().Limits();
            if((*i)->m_align->Strand() == eMinus)
                swap(fivep_exon,threep_exon);

            if(!ConfirmedStart() && HasStart() && fivep_exon.IntersectingWith((*i)->m_align->Limits()) && 
               iter->second.first && Include(Limits(),(*i)->m_align->Limits())) {  // protein has start

                TSignedSeqPos not_aligned =  orig_align->GetAlignMap().MapRangeOrigToEdited((*i)->m_align->Limits(),false).GetFrom()-1;
                if(not_aligned <= (1.-minscor.m_minprotfrac)*orig_align->TargetLen()) {                                                         // well aligned
                    TSignedSeqPos fivep = mrnamap.MapOrigToEdited(Strand() == ePlus ? (*i)->m_align->Limits().GetFrom() : (*i)->m_align->Limits().GetTo());
                    if(fivep > 0) {  // the end is still in chain
                        TSignedSeqPos extra_length = fivep-mrnamap.MapRangeOrigToEdited(GetCdsInfo().Start(),false).GetFrom()-1;
                        if(extra_length > not_aligned-minscor.m_endprotfrac*orig_align->TargetLen()) {
                            setconfstart = true;
                        }
                    }
                }
            }

            if(!ConfirmedStop() && HasStop() && threep_exon.IntersectingWith((*i)->m_align->Limits()) && 
               iter->second.second && Include(Limits(),(*i)->m_align->Limits())) {  // protein has stop  

                TSignedSeqPos not_aligned = orig_align->TargetLen()-orig_align->GetAlignMap().MapRangeOrigToEdited((*i)->m_align->Limits(),false).GetTo();
                if(not_aligned <= (1.-minscor.m_minprotfrac)*orig_align->TargetLen()) {                                                         // well aligned 
                    TSignedSeqPos threep = mrnamap.MapOrigToEdited(Strand() == ePlus ? (*i)->m_align->Limits().GetTo() : (*i)->m_align->Limits().GetFrom());
                    if(threep >= 0) {  // the end is still in chain
                        TSignedSeqPos extra_length = mrnamap.MapRangeOrigToEdited(GetCdsInfo().Stop(),false).GetTo()-threep;                
                        if(extra_length > not_aligned-minscor.m_endprotfrac*orig_align->TargetLen()) {
                            setconfstop = true; 
                        }
                    }
                }
            }
        }
    }

    CCDSInfo cds_info = GetCdsInfo();
    double score = cds_info.Score();
    if((setconfstart || ConfirmedStart()) && (setconfstop || ConfirmedStop()) && Continuous()) {
        score += max(1.,0.3*score);
        cds_info.SetScore(score, false);   // not open              
    }
   
    if(setconfstart) {
        cds_info.SetScore(score, false);   // not open           
        cds_info.SetStart(cds_info.Start(), true);    // confirmed start            
    }

    if(setconfstop) {
        cds_info.SetStop(cds_info.Stop(), true);    // confirmed stop           
    }

    SetCdsInfo(cds_info);
}

void CChain::CollectTrustedmRNAsProts(TOrigAligns& orig_aligns, const SMinScor& minscor, CScope& scope, SMatrix& delta, const CResidueVec& contig)
{
    ClearTrustedmRNA();
    ClearTrustedProt();

    if(HasStart() && HasStop()) {
        typedef map<Int8, set<TSignedSeqRange> > Tint8range;
        Tint8range aexons;
        Tint8range uexons;
        ITERATE(TContained, i, m_members) {
            if(IntersectingWith(*(*i)->m_align)) {                                                                   // just in case we clipped this alignment
                if(!(*i)->m_align->TrustedProt().empty()) {
                    ITERATE(TExons, e, (*i)->m_align->Exons()) {
                        if((*i)->m_mem_id > 0)
                            aexons[(*i)->m_align->ID()].insert(e->Limits());
                        else
                            uexons[(*i)->m_align->ID()].insert(e->Limits());
                    }
                }
                else if(!(*i)->m_align->TrustedmRNA().empty() && (*i)->m_align->ConfirmedStart() && (*i)->m_align->ConfirmedStop())  // trusted mRNA with aligned CDS (correctly checks not duplicated cds)
                    InsertTrustedmRNA(*(*i)->m_align->TrustedmRNA().begin());                                      // could be only one 'part'
            }
        }
        typedef map<Int8, int> Tint8int;
        Tint8int palignedlen;
        ITERATE(Tint8range, i, aexons) {
            int len = 0;
            ITERATE(set<TSignedSeqRange>, e, i->second)
                len += e->GetLength();
            palignedlen[i->first] = len;
        }
        ITERATE(Tint8range, i, uexons) {
            int len = 0;
            ITERATE(set<TSignedSeqRange>, e, i->second)
                len += e->GetLength();
            palignedlen[i->first] = max(len,palignedlen[i->first]);
        }

        if(ConfirmedStart() && ConfirmedStop()) {
            ITERATE(Tint8int, i, palignedlen) {
                CAlignModel* orig_align = orig_aligns[i->first];
                if((Continuous() && i->second > 0.8*orig_align->TargetLen()) || i->second > minscor.m_minprotfrac*orig_align->TargetLen())                                 // well aligned trusted protein
                    InsertTrustedProt(*orig_align->TrustedProt().begin());
            }
        }

        if(Continuous() && TrustedmRNA().empty() && TrustedProt().empty() && !palignedlen.empty()) {
            TSignedSeqRange cds = RealCdsLimits();
            int gap_cds = 0;
            ITERATE(CGeneModel::TExons, ie, Exons()) {
                if(ie->m_fsplice_sig == "XX" || ie->m_ssplice_sig == "XX")
                    gap_cds += (cds&ie->Limits()).GetLength();
            }

            if(gap_cds > 0) {            
                string mprotein = GetProtein(contig);
                ITERATE(Tint8int, i, palignedlen) {
                    CAlignModel* orig_align = orig_aligns[i->first];
                    if(i->second+gap_cds > 0.8*orig_align->TargetLen()) { //realign proteins if close enough
                        CSeqVector protein_seqvec(scope.GetBioseqHandle(*orig_align->GetTargetId()), CBioseq_Handle::eCoding_Iupac);
                        string tprotein(protein_seqvec.begin(),protein_seqvec.end());
                        CCigar cigar = LclAlign(mprotein.c_str(), (int)mprotein.size(), tprotein.c_str(), (int)tprotein.size(), 10, 1, delta.matrix);
                        if(cigar.SubjectRange().GetLength() > 0.8*tprotein.size()) {
                            InsertTrustedProt(*orig_align->TrustedProt().begin());
                            break;
                        }
                    }
                }
            }
        }
    }
}

// if external model is 'open' all 5' introns can harbor
// for nested model 'open' is ignored
bool CChain::HarborsNested(const CChain& other_chain, bool check_in_holes) const {
    TSignedSeqRange lim_for_nested = Limits();
    if(!ReadingFrame().Empty())
        lim_for_nested = OpenCds() ? MaxCdsLimits() : RealCdsLimits();

    TSignedSeqRange other_lim_for_nested = other_chain.Limits();
    if(!other_chain.ReadingFrame().Empty())
        other_lim_for_nested = other_chain.RealCdsLimits();

    if(lim_for_nested.IntersectingWith(other_lim_for_nested))
        return CModelCompare::RangeNestedInIntron(other_lim_for_nested, *this, check_in_holes);
    else
        return false;
}

// if external model is 'open' all 5' introns can harbor
// for nested model 'open' is ignored
bool CChain::HarborsNested(const CGene& other_gene, bool check_in_holes) const {
    TSignedSeqRange lim_for_nested = Limits();
    if(!ReadingFrame().Empty())
        lim_for_nested = OpenCds() ? MaxCdsLimits() : RealCdsLimits();

    TSignedSeqRange other_lim_for_nested = other_gene.Limits();
    if(!other_gene.RealCdsLimits().Empty())
        other_lim_for_nested = other_gene.RealCdsLimits();

    if(lim_for_nested.IntersectingWith(other_lim_for_nested)) 
        return CModelCompare::RangeNestedInIntron(other_lim_for_nested, *this, check_in_holes);
    else
        return false;
}

pair<string,int> GetAccVer(const CAlignModel& a, CScope& scope)
{
    if((a.Type()&CGeneModel::eProt) == 0)
        return make_pair(a.TargetAccession(), 0);

    try {
        CSeq_id_Handle idh = sequence::GetId(*a.GetTargetId(), scope,
                                             sequence::eGetId_ForceAcc);
        if (idh) {
            CConstRef<CSeq_id> acc = idh.GetSeqId();
            const CTextseq_id* txtid = acc->GetTextseq_Id();
            return (txtid  &&  txtid->IsSetAccession() && txtid->IsSetVersion()) ?
                make_pair(txtid->GetAccession(), txtid->GetVersion()) : make_pair(idh.AsString(), 0);
        }
    }
    catch (sequence::CSeqIdFromHandleException&) {
    }
    return make_pair(a.TargetAccession(), 0);
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

        if((a->Status()&CGeneModel::eBestPlacement) != (b->Status()&CGeneModel::eBestPlacement))
            return (a->Status()&CGeneModel::eBestPlacement) > (b->Status()&CGeneModel::eBestPlacement);

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
		if(it->Continuous() && (!it->TrustedmRNA().empty() || !it->TrustedProt().empty())
		   && orig_align != nullptr && it->AlignLen() > minscor.m_minprotfrac*orig_align->TargetLen()) {	
            trusted_aligns[it->Strand()].Insert(*it); 
        }           
    }

    if(trusted_aligns[ePlus].size() < 2 && trusted_aligns[eMinus].size() < 2)
        return;

    typedef set<string> TAccessions;
    typedef set<TSignedSeqPos> TSplices;
	typedef tuple<TSplices, TAccessions, const TGeneModelCluster*, TSignedSeqRange, list<TIVec>> TGRInfo;
    typedef vector<TGRInfo> TGRInfoVec;
    typedef map<int,TGRInfoVec> TGRInfoByStrand;
    TGRInfoByStrand trusted_splices;
    
    for(auto& strand_vec :  trusted_aligns) {
        int strand = strand_vec.first;
        TGeneModelClusterSet& clset = strand_vec.second;
		for(const TGeneModelCluster& cls : clset) {
            trusted_splices[strand].emplace_back();
			auto& last = trusted_splices[strand].back();
            TSplices& splices = get<0>(last);
            TAccessions& accessions = get<1>(last);
			get<2>(last) = &cls;
			TSignedSeqRange& cds = get<3>(last);
			list<TIVec>& cds_maps = get<4>(last);
			for(const CGeneModel& talign : cls) {
				//splices
				for(auto& e : talign.Exons()) {
                    if(e.m_fsplice)
                        splices.insert(e.GetFrom());
                    if(e.m_ssplice)
                        splices.insert(e.GetTo());                       
                }
				//accessions
                for(auto& cref : talign.TrustedProt())
                    accessions.insert(CIdHandler::ToString(*cref));
                for(auto& cref : talign.TrustedmRNA())
                    accessions.insert(CIdHandler::ToString(*cref));
				//cds and cds_maps
				TSignedSeqRange tcds = talign.GetCdsInfo().Cds();
				if(tcds.NotEmpty()) {
					cds += tcds;
					CAlignMap tmap(talign.Exons(), talign.FrameShifts(), talign.Strand(), tcds);
					cds_maps.emplace_back(tmap.FShiftedLen(tcds)/3,0);
					for(auto& e : talign.Exons()) {
						auto overlap = (e.Limits()&tcds);
						for(TSignedSeqPos k = overlap.GetFrom(); k <= overlap.GetTo(); ++k) {
							TSignedSeqPos p =  tmap.MapOrigToEdited(k);
							if(p >= 0 && p%3 == 0) {
								_ASSERT(p/3 < (int)cds_maps.back().size());
								cds_maps.back()[p/3] = k;
							}
						}
					}
				}
			}
        }
    }

    for(auto& ts : trusted_splices) {
		char strand = ts.first == ePlus ? '+' : '-';
		TGRInfoVec& spl = ts.second;
		for(int i = 0; i < (int)spl.size(); ++i) {
			auto& cls = *get<2>(spl[i]);
			int tgr = i+1;
			if(strand == '-')
				tgr = -tgr;
			trusted_group_cds[tgr] = get<3>(spl[i]);
			set<Int8> included_ids;
			for(const CGeneModel& align : cls) {
				if(!included_ids.count(align.ID())) // proteins doubled as unmodifieds
					cerr << "TGR: " << strand << " " << tgr << " " << align.ID() << endl;
			}
		}
    }

    for(TGeneModelList::iterator it_loop = clust.begin(); it_loop != clust.end(); ) {
        TGeneModelList::iterator ita = it_loop++;
        
        CGeneModel& align = *ita;
		if((align.Status()&CGeneModel::eUnknownOrientation) || align.Exons().size() == 1) //allow one-exons with CDS??
			continue;
	
        int strand = align.Strand();
		if(!trusted_splices.count(strand))
			continue;

        const TGRInfoVec& spl = trusted_splices[strand];
		auto first = lower_bound(spl.begin(), spl.end(), align.Limits().GetFrom(),
								 [](const TGRInfo& ti, TSignedSeqPos p) { return get<2>(ti)->Limits().GetTo() < p; }); 
		auto second = upper_bound(spl.begin(), spl.end(), align.Limits().GetTo(),
								  [](TSignedSeqPos p, const TGRInfo& ti) { return p < get<2>(ti)->Limits().GetFrom(); });
		if(first == second)
			continue;

		// align overlaps with at least one trusted group
		
		TSignedSeqRange acds = align.GetCdsInfo().Cds();
		TIVec acds_map;
		if(acds.NotEmpty()) {
			CAlignMap amap(align.Exons(), align.FrameShifts(), align.Strand(), acds);
			acds_map.resize(amap.FShiftedLen(acds)/3,0);
			for(auto& e : align.Exons()) {
				auto overlap = (e.Limits()&acds);
				for(TSignedSeqPos k = overlap.GetFrom(); k <= overlap.GetTo(); ++k) {
					TSignedSeqPos p =  amap.MapOrigToEdited(k);
					if(p >= 0 && p%3 == 0) {
						_ASSERT(p/3 < (int)acds_map.size());
						acds_map[p/3] = k;
					}
				}
			}
		}

        int count = 0;
        bool paralogs = false;
        TAccessions prev_accessions;
		for(auto it = first; it != second; ++it) {
			int tgr = it-spl.begin()+1;
			if(strand == eMinus)
				tgr = -tgr;

			int atgr = 0;
			const TSplices& splices = get<0>(*it);
            for(unsigned int i = 0; i < align.Exons().size() && atgr != tgr; ++i) {
                const CModelExon& e = align.Exons()[i];
				if((e.m_fsplice && splices.count(e.GetFrom())) || (e.m_ssplice && splices.count(e.GetTo())))
					atgr = tgr;
            }
			TSignedSeqRange cds = get<3>(*it);
			if(atgr != tgr && acds.IntersectingWith(cds)) {
				const list<TIVec>& cds_maps = get<4>(*it);
				for(const TIVec& tcds_map : cds_maps) {
					int long_enough = 10;
					int mci = MaxCommonInterval(acds_map, tcds_map, long_enough);
					if(mci >= long_enough) {
						atgr = tgr;
						break;
					}
				}
			}
			if(atgr == tgr) {
				const TAccessions& accessions = get<1>(*it);
				auto num = prev_accessions.size();
				prev_accessions.insert(accessions.begin(), accessions.end());
				if(num+accessions.size() != prev_accessions.size()) // overlapping ids
					paralogs = true;
				++count;
				align.SetTrustedGroup(tgr);
				align.SetTrustedCds(cds&align.Limits());
			}
        }

        if(count > 1) {
            string flag = "-";
            if(align.Type()&CGeneModel::eSR)
                flag = "SR";
            else if(align.Type()&CGeneModel::eEST)
                flag = "EST";
            else if(align.Type()&CGeneModel::emRNA)
                flag = "mRNA";
            else if(align.Type()&CGeneModel::eProt)
                flag = "Prot";
            cerr << "Chimeric alignment " << align.ID() << " " << (paralogs ? "paralogs" : "unrelated") << " " << flag << endl;
            SkipReason(orig_aligns[align.ID()],"Chimera");
            clust.erase(ita);
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
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    vector<CAlignModel*> alignment_ptrs;
    NON_CONST_ITERATE(TAlignModelList, a, alignments) {
        if(!(a->Status()&CGeneModel::eUnmodifiedAlign) && a->Type() != CGeneModel::eNotForChaining)
            alignment_ptrs.push_back(&*a);
    }

    if (alignment_ptrs.empty())
        return;

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
    ERASE_ITERATE(TGeneModelList, itcl, clust) {
        if(m_data->orig_aligns.find(itcl->ID()) == m_data->orig_aligns.end()) {
            clust.erase(itcl);
            continue;
        }

        CGeneModel& algn = *itcl;
        if ((algn.Type() & CGeneModel::eProt)!=0 || algn.ConfirmedStart()) {   // this includes protein alignments and mRNA with confirmed CDSes

            m_gnomon->GetScore(algn);
            double ms = m_data->GoodCDNAScore(algn);
            CAlignModel* orig = m_data->orig_aligns[algn.ID()];

            if (algn.Score() == BadScore() || (algn.Score() < ms && (algn.Type()&CGeneModel::eProt) && !(algn.Status()&CGeneModel::eBestPlacement) && orig->AlignLen() < m_data->minscor.m_minprotfrac*orig->TargetLen())) { // all mRNA with confirmed CDS and best placed or reasonably aligned proteins with known length will get through with any finite score 
                CNcbiOstrstream ost;
                if(algn.AlignLen() <= 75)
                    ost << "Short alignment " << algn.AlignLen();
                else
                    ost << "Low score " << algn.Score();
                m_data->SkipReason(orig, CNcbiOstrstreamToString(ost));
                clust.erase(itcl);
            }
        }
    }
} 

#define PROT_CLIP 120
#define PROT_CLIP_FRAC 0.20
#define MIN_PART 30

void CChainer::FindSelenoproteinsClipProteinsToStartStop(TGeneModelList& clust) {
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    const CResidueVec& contig = m_gnomon->GetSeq();
    
    ERASE_ITERATE(TGeneModelList, itcl, clust) {
        if(!(itcl->Type()&CGeneModel::eProt) || m_data->orig_aligns.find(itcl->ID()) == m_data->orig_aligns.end())  // skip cDNA and 'unmodified' without origaligns
            continue;

        CGeneModel& align = *itcl;
        m_gnomon->GetScore(align);
        if(align.Score() == BadScore()) {
            clust.erase(itcl);
            continue;
        }       

        CAlignModel* orig = m_data->orig_aligns[align.ID()];
        CSeqVector protein_seqvec(scope.GetBioseqHandle(*orig->GetTargetId()), CBioseq_Handle::eCoding_Iupac);

        CAlignMap amap = align.GetAlignMap();
        CAlignMap origmap = orig->GetAlignMap();

        //find selenoproteins and stops 'confirmed' on genome
        if(align.PStop()) {
            CCDSInfo::TPStops pstops = align.GetCdsInfo().PStops();
            NON_CONST_ITERATE(CCDSInfo::TPStops, stp, pstops) {
                TInDels fs = StrictlyContainedInDels(align.FrameShifts(), *stp);
                if(!fs.empty())
                    continue;
                TSignedSeqRange tstop = amap.MapRangeOrigToEdited(*stp,false);
                CResidueVec mrna;
                amap.EditedSequence(contig, mrna);
                if(tstop.GetLength() == 3 && mrna[tstop.GetFrom()] == 'T' && mrna[tstop.GetFrom()+1] == 'G' && mrna[tstop.GetFrom()+2] == 'A') {
                    TSignedSeqRange ostop = origmap.MapRangeOrigToEdited(*stp,false);
                    if(ostop.GetLength() == 3 && protein_seqvec[ostop.GetFrom()/3] == 'U') {
                        stp->m_status = CCDSInfo::eSelenocysteine;
                    }
                }
                if(stp->m_status != CCDSInfo::eSelenocysteine) {
                    TIntMap::iterator conf = m_confirmed_bases_len.upper_bound(stp->GetTo()); // confirmed on the right
                    if(conf != m_confirmed_bases_len.begin() && (--conf)->first <= stp->GetFrom() && conf->first+conf->second > stp->GetTo())
                        stp->m_status =  CCDSInfo::eGenomeCorrect;
                }
            }

            CCDSInfo cds = align.GetCdsInfo();
            cds.ClearPStops();
            ITERATE(CCDSInfo::TPStops, stp, pstops)
                cds.AddPStop(*stp);
            align.SetCdsInfo(cds);
        }

        if(itcl->Status()&CGeneModel::eUnmodifiedAlign) {
            m_data->unmodified_aligns[itcl->ID()] = *itcl;
            clust.erase(itcl);
            continue;
        }       

        if(align.Limits() == orig->Limits() && (!align.HasStart() || !align.FrameShifts().empty() || align.PStop(false))) {
            int maxclip = min(PROT_CLIP, (int)(align.AlignLen()*PROT_CLIP_FRAC+0.5));
            TSignedSeqRange tlim = orig->TranscriptLimits();
            int fivepclip = 0;
            if(protein_seqvec[0] == 'M')
                fivepclip = maxclip-tlim.GetFrom();
            int threepclip = maxclip-(orig->TargetLen()-tlim.GetTo()-1);

            bool skip = false;

            int fivepshift = 0;
            int threepshift = 0;
            int tlen = align.TranscriptLimits().GetTo()+1;
            for(TInDels::iterator indl = align.FrameShifts().begin(); !skip && indl != align.FrameShifts().end(); ++indl) {
                //project safely in case of tandem frameshifts or exon boundaries
                TSignedSeqRange left(align.Limits().GetFrom(),indl->Loc()-1);
                left = amap.ShrinkToRealPoints(left,false);
                _ASSERT(left.NotEmpty());
                TSignedSeqRange right(indl->InDelEnd(),align.Limits().GetTo());
                right = amap.ShrinkToRealPoints(right,false);
                _ASSERT(right.NotEmpty());

                TSignedSeqRange lim = amap.MapRangeOrigToEdited(TSignedSeqRange(left.GetTo(),right.GetFrom()), false);
                _ASSERT(lim.GetLength() >= 2);
                int tpa = lim.GetFrom()+1;
                int tpb = lim.GetTo()-1;
                // for deletion tpa,tpb are first and last tposition of the extra sequence on transcript
                // for insertion tpa is AFTER the missing sequence and tpb is BEFORE
                if(tpb < fivepclip) {                           // clipable 5' frameshift
                    if(indl->IsInsertion())
                        fivepshift += indl->Len();
                    else if(indl->IsDeletion())
                        fivepshift -= indl->Len();
                } else if(tpa >= tlen-threepclip) {             // clipable 3' frameshift
                    if(indl->IsInsertion())
                        threepshift += indl->Len();
                    else if(indl->IsDeletion())
                        threepshift -= indl->Len();
                } else {                                        // frameshift in main body
                    skip = true;   
                }            
            }
            if(skip)
                continue;

            if(fivepshift >= 0)                
                fivepshift %= 3;
            else
                fivepshift = 3-(-fivepshift)%3;
            if(threepshift >= 0)                
                threepshift %= 3;
            else
                threepshift = 3-(-threepshift)%3;

            CGeneModel editedm = align;
            editedm.FrameShifts().clear();
            editedm.SetCdsInfo(CCDSInfo());
            //CAlignMap edited_map = editedm.GetAlignMap();
            TSignedSeqRange edited_tlim = editedm.TranscriptLimits();
            edited_tlim.SetFrom(edited_tlim.GetFrom()+fivepshift);
            edited_tlim.SetTo(edited_tlim.GetTo()-threepshift);
            TSignedSeqRange edited_lim = editedm.GetAlignMap().MapRangeEditedToOrig(edited_tlim, false);
            _ASSERT(edited_lim.NotEmpty());
            editedm.Clip(edited_lim, CGeneModel::eRemoveExons);
            CCDSInfo ecds;
            ecds.SetReadingFrame(edited_lim, true);
            editedm.SetCdsInfo(ecds);            

            string protseq = editedm.GetProtein(contig);
            tlen = 3*(int)protseq.size();
            int fivep_problem = -1;
            int first_stop = tlen;
 
            for(int p = 0; !skip && p < (int)protseq.size(); ++p) {
                if(protseq[p] == '*') {                    
                    int tpa = p*3;
                    int tpb = tpa+2;
                    if(tpb < fivepclip)                                           // clipable 5' stop
                        fivep_problem = max(fivep_problem, tpb);
                    else if(tpa >= tlen-threepclip || p == (int)protseq.size()-1) // leftmost 3' stop
                        first_stop = min(first_stop, tpa);
                    else                                                          // stop in main body
                        skip = true;
                } 
            }
            if(skip)
                continue;

            int fivep_limit = 0;
            auto m = protseq.find("M", (fivep_problem+1)/3);   // first start after possible stop/frameshift
            skip = true;
            if(m != string::npos && (int)m*3 <= fivepclip) {
                fivep_limit = 3*(int)m;
                skip = false;                
            }
            if(skip)
                continue;
            
            int threep_limit = tlen-1;
            skip = true;
            if(first_stop+2 < threep_limit) {
                threep_limit =  first_stop+2;
                skip = false;               
            }
            if(skip)
                continue;

            TSignedSeqRange clip(fivep_limit, threep_limit);
            tlen = clip.GetLength();
            clip = editedm.GetAlignMap().MapRangeEditedToOrig(clip, false);
            _ASSERT(clip.NotEmpty());

            editedm.Clip(clip, CGeneModel::eRemoveExons);
            if(align.Limits().GetFrom() != editedm.Limits().GetFrom() && !editedm.Exons().front().m_ssplice && editedm.Exons().front().Limits().GetLength() < MIN_PART) // short 5' part
                continue;
            if(align.Limits().GetTo() != editedm.Limits().GetTo() && !editedm.Exons().back().m_fsplice && editedm.Exons().back().Limits().GetLength() < MIN_PART) // short 3' part
                continue;

            TSignedSeqRange start(0, 2);
            TSignedSeqRange stop(tlen-3, tlen-1);
            TSignedSeqRange rf(start.GetTo()+1,stop.GetFrom()-1);
            CCDSInfo edited_cds(false);
            edited_cds.SetReadingFrame(rf,true);
            edited_cds.SetStart(start,true);
            edited_cds.SetStop(stop,true);
            edited_cds.SetScore(align.Score());
            edited_cds = edited_cds.MapFromEditedToOrig(editedm.GetAlignMap());
            editedm.SetCdsInfo(edited_cds);

#ifdef _DEBUG 
            protseq = editedm.GetProtein(contig);
            _ASSERT(tlen == 3*(int)protseq.size());
            _ASSERT(protseq[0] == 'M');
            m = protseq.find("*");
            _ASSERT(m == protseq.size()-1);
#endif    

            align = editedm;
        }        
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

double InframeFraction(const CGeneModel& a, TSignedSeqPos left, TSignedSeqPos right)
{
    if(a.FrameShifts().empty())
        return 1.0;

    CAlignMap cdsmap(a.GetAlignMap());
    int inframelength = 0;
    int outframelength = 0;
    int frame = 0;
    TSignedSeqPos prev = left;
    TInDels indels = a.GetInDels(left, right, true);
    ITERATE(TInDels, fs, indels) {
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
    if ((align.Type()&CAlignModel::emRNA)==0 || (align.Status()&CGeneModel::eTSA)!=0 || (align.Status()&CGeneModel::eReversed)!=0 || (align.Status()&CGeneModel::eUnknownOrientation)!=0)
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

    CAlignMap alignmap_clipped(a.GetAlignMap());
    if(alignmap_clipped.MapOrigToEdited(left) < 0 || alignmap_clipped.MapOrigToEdited(right) < 0)     // cds is clipped
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
    int length = (int)cds.size();
    
    if(length%3 != 0 || length < 6)
        return;
    
    if(!IsStartCodon(&cds[0]) || !IsStopCodon(&cds[length-3]) )   // start or stop on genome is not right
        return;
    
    for(int i = 0; i < length-3; i += 3) {
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
        TSignedSeqRange flimits = align.Exons().front().Limits();
        TSignedSeqRange blimits = align.Exons().back().Limits();
        CAlignMap alignmap(align.GetAlignMap());

        if ((align.Type() & CAlignModel::eProt)!=0) {
            TrimProtein(align, alignmap);
        } else {
            TrimTranscript(align, alignmap);
        }

        // don't mark trimmed if trim was to the next exon
        if(align.Limits().GetFrom() > flimits.GetFrom() && align.Limits().GetFrom() <= flimits.GetTo()) align.Status() |= CAlignModel::eLeftTrimmed;
        if(align.Limits().GetTo() < blimits.GetTo() && align.Limits().GetTo() >= blimits.GetFrom()) align.Status() |= CAlignModel::eRightTrimmed;
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
        if(!align.TrustedmRNA().empty())
            return;
        if(align.Status()&(CGeneModel::eLeftFlexible|CGeneModel::eRightFlexible))
            return;

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

        //don't trim gapfillers
        if(align.Exons().front().m_ssplice_sig == "XX")
            a = align.Limits().GetFrom();
        if(align.Exons().back().m_fsplice_sig == "XX")
            b = align.Limits().GetTo();
        
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
        if ((align.Status()&CGeneModel::ePolyA) == 0)
            return;

        if ((align.Status()&CGeneModel::eUnknownOrientation) != 0 || align.PolyALen() < minpolya)
            align.Status() ^= CGeneModel::ePolyA;
    }
};

TransformFunction* CChainer::DoNotBelieveShortPolyATail()
{
    return new gnomon::DoNotBelieveShortPolyATail(m_data->minpolya);
}


void CChainer::SetNumbering(int idnext, int idinc)
{
    m_data->m_idnext = idnext;
    m_data->m_idinc = idinc;
}

void CChainer::SetGenomicRange(const TAlignModelList& alignments)
{
    m_data->SetGenomicRange(alignments);
}

void CChainer::CChainerImpl::SetGenomicRange(const TAlignModelList& alignments)
{
    TSignedSeqRange range = alignments.empty() ? TSignedSeqRange::GetWhole() : TSignedSeqRange::GetEmpty();

    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    ITERATE(TAlignModelList, i, alignments) {
        range += i->Limits();

        if(i->Type()&CGeneModel::eProt) {
            string accession = i->TargetAccession();
            if(!prot_complet.count(accession)) {
                CSeqVector protein_seqvec(scope.GetBioseqHandle(*i->GetTargetId()), CBioseq_Handle::eCoding_Iupac);
                CSeqVector_CI protein_ci(protein_seqvec);
                prot_complet[accession] = make_pair(*protein_ci == 'M', true);
            }
        }
    }

    _ASSERT(m_gnomon.get() != NULL);
    m_gnomon->ResetRange(range);

    confirmed_ends.clear();
    all_frameshifts.clear();
    orig_aligns.clear();
    unmodified_aligns.clear();
    trusted_group_cds.clear();
    mrna_count.clear();
    est_count.clear();
    rnaseq_count.clear();
    oriented_introns_plus.clear();
    oriented_introns_minus.clear();
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

void CChainer::SetConfirmedStartStopForProteinAlignments(TAlignModelList& alignments)
{
    m_data->SetConfirmedStartStopForProteinAlignments(alignments);
}

void CChainer::CChainerImpl::SetConfirmedStartStopForProteinAlignments(TAlignModelList& alignments)
{
    NON_CONST_ITERATE (TAlignModelCluster, i, alignments) {
        CAlignModel& algn = *i;
        if ((algn.Type() & CGeneModel::eProt)!=0) {
            CCDSInfo cds = algn.GetCdsInfo();
            TSignedSeqRange alignedlim = algn.GetAlignMap().MapRangeOrigToEdited(algn.Limits(),false);
            map<string, pair<bool,bool> >::iterator iter = prot_complet.find(algn.TargetAccession());
            _ASSERT(iter != prot_complet.end());
            if(iter == prot_complet.end())
                continue;
            
            if(cds.HasStart() && iter->second.first && alignedlim.GetFrom() == 0)
                cds.SetStart(cds.Start(),true);
            if(cds.HasStop() && iter->second.second && alignedlim.GetTo() == algn.TargetLen()-1)
                cds.SetStop(cds.Stop(),true);
            if(cds.ConfirmedStart() || cds.ConfirmedStop())
                algn.SetCdsInfo(cds);
        }
    }
}

void CChainer::DropAlignmentInfo(TAlignModelList& alignments, TGeneModelList& models)
{

    NON_CONST_ITERATE (TAlignModelCluster, i, alignments) {
        if(!(i->Status()&CGeneModel::eUnmodifiedAlign)) 
            m_data->orig_aligns[i->ID()]=&(*i);

        CGeneModel aa = *i;

        if(!i->TrustedmRNA().empty() && i->Exons().size() > 1) {
            auto tlim = i->TranscriptLimits();
            if(i->Exons().front().Limits().NotEmpty() && tlim.GetFrom() == 0)
                aa.Status() |= CGeneModel::eLeftConfirmed;
            if(i->Exons().back().Limits().NotEmpty() && (tlim.GetTo() == i->TargetLen()-1 || (i->Status()&CGeneModel::ePolyA)))
                aa.Status() |= CGeneModel::eRightConfirmed;
        }

        if(aa.Type() & CGeneModel::eProt) {
            TInDels alignfshifts = i->GetInDels(true);
            TInDels fshifts;
            ITERATE(CGeneModel::TExons, e, aa.Exons()) {
                TInDels efshifts;
                int len = 0;
                ITERATE(TInDels, fs, alignfshifts) {
                    if(fs->IntersectingWith(e->GetFrom(),e->GetTo())) {
                        efshifts.push_back(*fs);
                        len += (fs->IsInsertion() ? fs->Len() : -fs->Len());
                    }
                }
                if(efshifts.empty())
                    continue;

                int a = efshifts.front().Loc()-1;
                int b = efshifts.back().InDelEnd();
                TIntMap::iterator conf = m_confirmed_bases_len.upper_bound(b); // confirmed on the right    
                bool confirmed_region = (conf != m_confirmed_bases_len.begin() && (--conf)->first <= a && conf->first+conf->second > b);

                if(len%3 != 0 || !confirmed_region) {
                    ITERATE(TInDels, fs, efshifts) {
                        int l = fs->Len()%3;
                        if(fs->IsInsertion()) {
                            fshifts.push_back(CInDelInfo(fs->Loc(), l, CInDelInfo::eIns));
                        } else {
                            fshifts.push_back(CInDelInfo(fs->Loc(), l, CInDelInfo::eDel, fs->GetInDelV().substr(0,l)));
                        }                        
                    }
                    //                    fshifts.insert(fshifts.end(), efshifts.begin(), efshifts.end());
                }
            }
            aa.FrameShifts() = fshifts;
        } else {
            aa.FrameShifts().clear();
            aa.Status() &= ~CGeneModel::eReversed;
        }

        models.push_back(aa);
    }

    // remove unmodifieds for deleted aligns
    for(auto it_loop = models.begin(); it_loop != models.end(); ) {
	auto it = it_loop++;
	if(!m_data->orig_aligns.count(it->ID())) {
	    _ASSERT(it->Status()&CGeneModel::eUnmodifiedAlign);
	    models.erase(it);
	}
    }
}


void CChainerArgUtil::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->AddKey("param", "param",
                     "Organism specific parameters",
                     CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("pcsf_factor","pcsf_factor","Normalisation factor for phyloPCSF scores",CArgDescriptions::eDouble,"0.1");

    arg_desc->SetCurrentGroup("Alignment modification");
    arg_desc->AddDefaultKey("trim", "trim",
                            "If aligned sequence is partial and includes a small portion of an exon the alignment program "
                            "usually misses this exon and might erroneously place a few bases from this exon near the previous exon, "
                            "and this will mess up the chaining. To prevent this we trim small portions of the alignment before chaining. "
                            "If it is possible, the trimming will be reversed for the 5'/3' ends of the final chain. Must be < minex and "
                            "multiple of 3",
                            CArgDescriptions::eInteger, "6");

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
                            CArgDescriptions::eInteger, "900");
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
                            CArgDescriptions::eInteger, "10");
    arg_desc->AddDefaultKey("minsupport", "minsupport",
                            "Minimal number of mRNA/EST for valid noncoding models",
                            CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey("minsupport_mrna", "minsupport_mrna",
                            "Minimal number of mRNA for valid noncoding models",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("minsupport_rnaseq", "minsupport_rnaseq",
                            "Minimal number of RNA-Seq for valid noncoding models",
                            CArgDescriptions::eInteger, "5");
    arg_desc->AddDefaultKey("minlen", "minlen",
                            "Chains with thorter CDS should be supported by trusted protein",
                            CArgDescriptions::eInteger, "100");
    arg_desc->AddDefaultKey("altfrac","altfrac","The CDS length of the principal model in the gene is multiplied by this fraction. Alt variants with the CDS length above "
                            "this are included in gene",CArgDescriptions::eDouble,"80.0");
    arg_desc->AddDefaultKey("longreadsthreshold","longreadsthreshold","If long reads support that many introns in alignment cluster "
                            "noncoding imtronless overlaps are not used",CArgDescriptions::eDouble,"75.0");
    arg_desc->AddDefaultKey("composite","composite","Maximal composite number in alts",CArgDescriptions::eInteger,"1");
    arg_desc->AddFlag("opposite","Allow overlap of complete multiexon genes with opposite strands");
    arg_desc->AddFlag("partialalts","Allows partial alternative variants. In combination with -nognomon will allow partial genes");
    arg_desc->AddDefaultKey("tolerance","tolerance","if models exon boundary differ only this much only one model will survive",CArgDescriptions::eInteger,"5");
    arg_desc->AddFlag("no5pextension","Don't extend chain CDS to the leftmost start");

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

    arg_desc->SetCurrentGroup("CAGE/PolyA arguments");

    arg_desc->AddDefaultKey("min-cap-weight", "MinCapWeight",
                            "Minimal accepted weight for a capped alignment",
                            CArgDescriptions::eInteger, "5");
    arg_desc->AddDefaultKey("min-cap-blob", "MinCapBlob",
                            "Minimal cap blob weight for accepted peak",
                            CArgDescriptions::eInteger, "50");

    arg_desc->AddDefaultKey("min-polya-weight", "MinPolyaWeight",
                            "Minimal accepted weight for polya alignment",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("min-polya-blob", "MinPolyaBlob",
                            "Minimal polya blob weight for accepted peak",
                            CArgDescriptions::eInteger, "1");

    arg_desc->AddDefaultKey("max-dist", "MaxDist",
                            "Maximal distance between individual cap/polya positions in a blob",
                            CArgDescriptions::eInteger, "20");
    arg_desc->AddDefaultKey("secondary-peak", "SecondaryPeak",
                            "Minimal weight fraction for a secondary cap/polya peak",
                            CArgDescriptions::eDouble, "0.5");
    arg_desc->AddDefaultKey("tertiary-peak", "TertiaryPeak",
                            "Last 5' exon is extended to low weight polya peak if there is sufficient rnaseq coverage",
                            CArgDescriptions::eDouble, "0.2");
    arg_desc->AddDefaultKey("tertiary-peak-coverage", "TertiaryPeakCoverage",
                            "Minimal relative rnaseq coverage for tertiary peak",
                            CArgDescriptions::eDouble, "0.05");

    arg_desc->AddDefaultKey("min-flank-exon", "MinFlankExon",
                            "The minimal distance of cap/polya to a splice",
                            CArgDescriptions::eInteger, "25");


    arg_desc->AddDefaultKey("minpolya", "minpolya",
                            "Minimal accepted polyA tale length in transcript alignments",
                            CArgDescriptions::eInteger, "6");
    arg_desc->AddFlag("use_confirmed_ends","Use end exons of trusted transcripts for clippig/extension");

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

    SMinScor& minscor = chainer->SetMinScor();
    minscor.m_min = args["minscor"].AsDouble();
    // this parameters are not used
    minscor.m_i5p_penalty = args["i5p"].AsDouble();
    minscor.m_i3p_penalty = args["i3p"].AsDouble();
    minscor.m_cds_bonus = args["cdsbonus"].AsDouble();
    minscor.m_length_penalty = args["lenpen"].AsDouble();
    //
    minscor.m_minprotfrac = args["minprotfrac"].AsDouble();
    minscor.m_endprotfrac = args["endprotfrac"].AsDouble();
    minscor.m_prot_cds_len = args["protcdslen"].AsInteger();
    minscor.m_cds_len = args["longenoughcds"].AsInteger();
    minscor.m_utr_clip_threshold = args["utrclipthreshold"].AsDouble();
    minscor.m_minsupport = args["minsupport"].AsInteger();
    minscor.m_minsupport_mrna = args["minsupport_mrna"].AsInteger();
    minscor.m_minsupport_rnaseq = args["minsupport_rnaseq"].AsInteger();
    minscor.m_minlen = args["minlen"].AsInteger();

    chainer->SetMinInframeFrac(args["mininframefrac"].AsDouble());

    chainer->m_data->altfrac = args["altfrac"].AsDouble();
    chainer->m_data->longreadsthreshold = args["longreadsthreshold"].AsDouble();
    chainer->m_data->composite = args["composite"].AsInteger();
    chainer->m_data->allow_opposite_strand = args["opposite"];
    chainer->m_data->allow_partialalts = args["partialalts"];
    chainer->m_data->tolerance = args["tolerance"].AsInteger();
    chainer->m_data->no5pextension =  args["no5pextension"];
 
    chainer->m_data->min_cap_weight = args["min-cap-weight"].AsInteger();
    chainer->m_data->min_cap_blob = args["min-cap-blob"].AsInteger();
    chainer->m_data->min_polya_weight = args["min-polya-weight"].AsInteger();
    chainer->m_data->min_polya_blob = args["min-polya-blob"].AsInteger();
    chainer->m_data->max_dist = args["max-dist"].AsInteger();
    chainer->m_data->secondary_peak = args["secondary-peak"].AsDouble();
    chainer->m_data->tertiary_peak = args["tertiary-peak"].AsDouble();
    chainer->m_data->tertiary_peak_coverage = args["tertiary-peak-coverage"].AsDouble();
    chainer->m_data->min_flank_exon = args["min-flank-exon"].AsInteger();
    chainer->SetMinPolyA(args["minpolya"].AsInteger());
    chainer->m_data->use_confirmed_ends = args["use_confirmed_ends"];
    chainer->m_pcsf_factor = args["pcsf_factor"].AsDouble();    
    
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

bool OverlappingIndel(int pos, const CInDelInfo& indl) {
    if(indl.IsDeletion())
        return pos <= indl.InDelEnd();
    else
        return pos < indl.InDelEnd();
}

//this just copies exona_indels unless genome corrections are used
//extra_left/extra_right insertions at the ends of exon on Agenome
TInDels CombineCorrectionsAndIndels(const TSignedSeqRange exona, int extra_left, int extra_right, const TSignedSeqRange exonb, const TInDels& editing_indels_frombtoa, const TInDels& exona_indels) {
    TInDels combined_indels;

    TInDels::const_iterator ic = upper_bound(editing_indels_frombtoa.begin(), editing_indels_frombtoa.end(), exonb.GetFrom(), OverlappingIndel);  // skip all correction ending before exonb
    for( ;ic != editing_indels_frombtoa.end() && ic->GetStatus() != CInDelInfo::eGenomeNotCorrect; ++ic);   //skip ggaps and Ns
    if((ic == editing_indels_frombtoa.end() || ic->Loc() > exonb.GetTo()+1) && exona_indels.empty()) 
        return combined_indels;

    typedef list<char> TCharList;
    TCharList edit; // edit for Bgenome->transceipt calculated in two steps: Bgenome->Agenome->transceipt
    // M match/mismatch
    // - skip one base
    // everything else insert this letter 
    
    //edit from B genome to A genome 
    int pb = exonb.GetFrom();
    for( ;pb <= exonb.GetTo(); ++pb) {
        if(ic != editing_indels_frombtoa.end() && ic->Loc() <= pb) {
            if(ic->IsInsertion()) {
                int len = min(exonb.GetTo()+1,ic->InDelEnd())-max(exonb.GetFrom(),ic->Loc());
                edit.insert(edit.end(),len,'-');
                pb = ic->InDelEnd()-1;                                    
            } else {
                string s = ic->GetInDelV();
                if(pb == exonb.GetFrom())       // include extra_left part of deletion
                    s = s.substr(ic->Len()-extra_left);                                    
                edit.insert(edit.end(),s.begin(),s.end());
                edit.push_back('M');            // base before deletion
            }
            ++ic;
        } else {
            edit.push_back('M');
        }
    }
    if(ic != editing_indels_frombtoa.end() && ic->Loc() == pb && ic->GetStatus() == CInDelInfo::eGenomeNotCorrect && extra_right > 0) { // include extra_right part of deletion
        _ASSERT(ic->IsDeletion());
        string s = ic->GetInDelV().substr(0,extra_right);
        edit.insert(edit.end(),s.begin(),s.end());
    } 
    _ASSERT(exonb.GetLength() == count(edit.begin(),edit.end(),'M')+count(edit.begin(),edit.end(),'-'));
    _ASSERT(exona.GetLength() == (int)edit.size()-count(edit.begin(),edit.end(),'-'));

    // adding changes from A to transcript 
    if(!exona_indels.empty()) {
        TInDels::const_iterator jleft = exona_indels.begin();
        int pa = exona.GetFrom()-1;
        int skipsome = 0;
        ERASE_ITERATE(TCharList, ip, edit) {
            if(*ip == '-')
                continue;
            else
                ++pa;
        
            if(jleft != exona_indels.end() && jleft->Loc() == pa) {
                if(jleft->IsInsertion()) {  // skip extra bases on edited           
                    _ASSERT(skipsome == 0);
                    skipsome = jleft->Len();
                    // don't use reverse iterator for erasing           
                    for(TCharList::iterator ipp = ip; skipsome > 0 && ipp != edit.begin() && *(--ipp) != '-' && *ipp != 'M'; ) {  // skip previosly inserted            
                        --skipsome;                        
                        ipp = edit.erase(ipp);
                    }
                } else {                    // insert extra bases in transcript         
                    _ASSERT(skipsome == 0);
                    int insertsome = jleft->Len();                           
                    for(reverse_iterator<TCharList::iterator> ir(ip); insertsome > 0 && ir != edit.rend() && *ir == '-'; ++ir) { // reuse skipped positions         
                        *ir = 'M';
                        --insertsome;
                    }
                    if(insertsome > 0)
                        edit.insert(ip,insertsome,'N');
                }
                ++jleft;
            }

            if(skipsome > 0) {
                --skipsome;                        
                if(*ip == 'M')
                    *ip = '-';
                else if(*ip != '-') // looks like *ip is never '-'
                    edit.erase(ip);
            }
        }
        if(jleft != exona_indels.end()) {
            _ASSERT(jleft->IsDeletion() && jleft->Loc() == pa+1);
            int insertsome = jleft->Len();                           
            for(TCharList::reverse_iterator ir = edit.rbegin(); insertsome > 0 && ir != edit.rend() && *ir == '-'; ++ir) { // reuse skipped positions       
                *ir = 'M';
                --insertsome;
            }
            if(insertsome > 0)
                edit.insert(edit.end(),insertsome,'N');
        }
    }
    _ASSERT(exonb.GetLength() == count(edit.begin(),edit.end(),'M')+count(edit.begin(),edit.end(),'-'));
 
    //TODO: combine +- indels separated by short spans of Ms
    pb = exonb.GetFrom();
    for(TCharList::iterator ip = edit.begin(); ip != edit.end(); ) {
        if(*ip == 'M') {
            ++pb;
            ++ip;
        } else if(*ip == '-') {
            int len = 0;
            for( ;ip != edit.end() && *ip == '-'; ++ip, ++len);
            int pos = pb;
            pb += len;
            for( ;len > 0 && ip != edit.end() && *ip != 'M'; ++ip, --len);   // we may have ----+++M but not +++--- (can't really happen unless corrections had adjacent -+)
            if(len > 0)
                combined_indels.push_back(CInDelInfo(pos,len,CInDelInfo::eIns));
        } else {
            string s;
            for( ;ip != edit.end() && *ip != 'M' && *ip != '-'; ++ip)
                s.push_back(*ip);
            combined_indels.push_back(CInDelInfo(pb, (int)s.size(), CInDelInfo::eDel, s));
        }
    }
    _ASSERT(pb == exonb.GetTo()+1);

    return combined_indels;
}

CGeneModel CGnomonAnnotator_Base::MapOneModelToOrigContig(const CGeneModel& srcmodel) const {
    CGeneModel model = srcmodel;
    model.SetCdsInfo(CCDSInfo());
    model.CutExons(model.Limits());  // empty model with all atributes
    TInDels editedframeshifts;

    for(int ie = 0; ie < (int)srcmodel.Exons().size(); ++ie) {
        const CModelExon& e = srcmodel.Exons()[ie];

        string seq;
        CInDelInfo::SSource src;
        CGnomonAnnotator_Base::TGgapInfo::const_iterator i = m_inserted_seqs.upper_bound(e.GetTo());          // first ggap on right or end()
        if(i != m_inserted_seqs.begin()) {
            --i;                                                                       // first ggap left or equal GetTo()
            int ggapa = i->first;
            int ggapb = i->first+(int)i->second->GetInDelV().length()-1;
            if(ggapa == e.GetFrom()) {                                                 // exons starts with ggap
                seq = i->second->GetInDelV().substr(0,e.Limits().GetLength());
                src = i->second->GetSource();
                if(src.m_strand == ePlus)
                    src.m_range.SetTo(src.m_range.GetFrom()+e.Limits().GetLength()-1);
                else
                    src.m_range.SetFrom(src.m_range.GetTo()-e.Limits().GetLength()+1);
            } else if(ggapb == e.GetTo()) {                                            // exon ends by ggap
                string s = i->second->GetInDelV();
                seq = s.substr(s.length()-e.Limits().GetLength());
                src = i->second->GetSource();
                if(src.m_strand == eMinus)
                    src.m_range.SetTo(src.m_range.GetFrom()+e.Limits().GetLength()-1);
                else
                    src.m_range.SetFrom(src.m_range.GetTo()-e.Limits().GetLength()+1);
            } else if(ggapb >= e.GetFrom()) {                                          // all real alignment and some filling was clipped
                _ASSERT(srcmodel.Exons().size() == 1);
                return CGeneModel();
            }
        }
        
        if(!seq.empty()) {  // ggap
            if((int)srcmodel.Exons().size() == 1){ // all real alignment was clipped
                return CGeneModel();
            }
            if(model.Strand() == eMinus) {
                ReverseComplement(seq.begin(), seq.end());
                src.m_strand = (src.m_strand == ePlus ? eMinus : ePlus);
            }
            _ASSERT((int)seq.length() == src.m_range.GetLength());
            model.AddGgapExon(0, seq, src, false);
        } else {  // normal exon
            TSignedSeqRange exon = m_edited_contig_map.ShrinkToRealPointsOnEdited(e.Limits());
            if(exon.Empty()) {   // not projectable exon
                return CGeneModel();
            }
            int extra_left = exon.GetFrom()-e.GetFrom();
            int extra_right = e.GetTo()-exon.GetTo();

            exon = m_edited_contig_map.MapRangeEditedToOrig(exon,false);
            _ASSERT(exon.NotEmpty());

            TInDels exon_indels;
            ITERATE(TInDels, indl, srcmodel.FrameShifts()) {
                if(indl->IntersectingWith(e.GetFrom(),e.GetTo()))
                    exon_indels.push_back(*indl);
            }
            TInDels efs = CombineCorrectionsAndIndels(e.Limits(), extra_left, extra_right, exon, m_editing_indels, exon_indels);

            TInDels erepl;
            map<int,char>::const_iterator ir = m_replacements.lower_bound(exon.GetFrom());  // first exon replacement or end()
            for( ;ir != m_replacements.end() && ir->first <= exon.GetTo(); ++ir) {
                int loc = ir->first;
                char c = ir->second;
                TInDels::const_iterator ic = upper_bound(efs.begin(), efs.end(), loc, OverlappingIndel);  // skip all indels ending before mismatch
                if(ic != efs.end() && ic->IsInsertion() && ic->Loc() <= loc && ic->InDelEnd() > loc)   // overlapping insertion
                    continue;
                else if(ic != efs.end() && ic->IsDeletion() && ic->Loc() == loc)                       // deletion right before mismatch
                    erepl.push_back(CInDelInfo(loc, 1, CInDelInfo::eMism, string(1,c)));
                else if(erepl.empty() || erepl.back().InDelEnd() != loc)                               // not extention of previous
                    erepl.push_back(CInDelInfo(loc, 1, CInDelInfo::eMism, string(1,c)));
                else {
                    loc = erepl.back().Loc();
                    string s = erepl.back().GetInDelV()+string(1,c);
                    erepl.back() = CInDelInfo(loc, (int)s.size(), CInDelInfo::eMism, s);
                }
            }
            efs.insert(efs.end(), erepl.begin(), erepl.end());
            sort(efs.begin(), efs.end());
            for(auto& indl : efs) {
                indl.SetLoc(indl.Loc()+m_limits.GetFrom());
                editedframeshifts.push_back(indl);
            }

            exon.SetFrom(exon.GetFrom()+m_limits.GetFrom());
            exon.SetTo(exon.GetTo()+m_limits.GetFrom());
            model.AddNormalExon(exon, e.m_fsplice_sig, e.m_ssplice_sig, 0, false);
        }

        if(ie < (int)srcmodel.Exons().size()-1 && (!e.m_ssplice || !srcmodel.Exons()[ie+1].m_fsplice)) // hole
            model.AddHole();
    }

    model.FrameShifts() = editedframeshifts;
    model.SetCdsInfo(srcmodel.GetCdsInfo().MapFromOrigToEdited(srcmodel.GetAlignMap()));

    return model;
}


/*
//currently not used for anything; will need separation of indels and replacemnets inputs if used
void MapAlignsToOrigContig(TAlignModelList& aligns, const TInDels& corrections, int contig_size) {
    CGnomonAnnotator_Base::TGgapInfo inserted_seqs;  // not used
    TInDels editing_indels;
    map<int,char> replacements;

    ITERATE(TInDels, i, corrections) {
        if(i->IsMismatch()) {
            string seq = i->GetInDelV();
            for(int l = 0; l < i->Len(); ++l)
                replacements[i->Loc()+l] = seq[l];
        } else {
            editing_indels.push_back(*i);
            if(i->IsInsertion())
                contig_size += i->Len();
            else
                contig_size -= i->Len();
        }
    }                
    CAlignMap edited_contig_map(0, contig_size-1, editing_indels.begin(), editing_indels.end());
    
    ERASE_ITERATE(TAlignModelList, ia, aligns) {
        CAlignModel& align = *ia;
        CGeneModel model = MapOneModelToOrigContig(align, editing_indels, replacements, edited_contig_map, inserted_seqs);
        if(model.Limits().Empty()) {
            aligns.erase(ia);
        } else {
            _ASSERT(align.Exons().size() == model.Exons().size());
            if(align.Type()&CAlignModel::eProt)
                model.FrameShifts() = model.GetInDels(false);
            vector<TSignedSeqRange> transcript_exons;
            for(int i = 0; i < (int)align.Exons().size(); ++i)
                transcript_exons.push_back(align.TranscriptExon(i));
            CAlignMap amap(model.Exons(), transcript_exons, model.FrameShifts(), align.Orientation(), align.TargetLen());
            CConstRef<objects::CSeq_id> id = align.GetTargetId();
            *ia = CAlignModel(model,amap);
            ia->SetTargetId(*id);
        }
    }    
}
*/

void CGnomonAnnotator_Base::MapModelsToOrigContig(TGeneModelList& models) const {
    ERASE_ITERATE(TGeneModelList, im, models) {
        CGeneModel model = MapOneModelToOrigContig(*im);
        if(model.Limits().Empty()) {
            models.erase(im);
        } else {
            NON_CONST_ITERATE(TInDels, i, model.FrameShifts()) {
                if(i->IsMismatch()) {
                    i->SetStatus(CInDelInfo::eGenomeNotCorrect);
                } else {
                    TIntMap::const_iterator conf = m_confirmed_bases_orig_len.upper_bound(i->Loc()); // confirmed on the right  
                    bool included = (conf != m_confirmed_bases_orig_len.begin() && (--conf)->first < i->Loc() &&  conf->first+conf->second >= i->InDelEnd());

                    TInDels::const_iterator ic = upper_bound(m_editing_indels.begin(), m_editing_indels.end(), i->Loc(), OverlappingIndel);  // skip all correction ending before Loc() 
                    if(ic != m_editing_indels.end() && i->GetType() == ic->GetType() && i->Loc() >= ic->Loc() && i->InDelEnd() <= ic->InDelEnd()) {
                        i->SetStatus(CInDelInfo::eGenomeNotCorrect);
                        _ASSERT(included);
                    } else if(included && (ic == m_editing_indels.end() || ic->Loc() > i->InDelEnd())) {
                        i->SetStatus(CInDelInfo::eGenomeCorrect);                                        
                    }
                }
            }            
            *im = model;
        }
    }
}

CAlignModel CGnomonAnnotator_Base::MapOneModelToEditedContig(const CGeneModel& align) const 
{
    CAlignMap amap = align.GetAlignMap();
    CCDSInfo acds = align.GetCdsInfo();
    if(align.ReadingFrame().NotEmpty() && acds.IsMappedToGenome())
        acds = acds.MapFromOrigToEdited(amap);
    amap.MoveOrigin(m_limits.GetFrom());

    //mismatches are dropped at this point
    TInDels aindels = align.GetInDels(false);

    //recalculate limits to contig chunk
    for(auto& indel : aindels)
        indel.SetLoc(indel.Loc()-m_limits.GetFrom());

    CGeneModel::TExons aexons = align.Exons();
    for(auto& e : aexons) {
        if(e.Limits().NotEmpty()) {
            e.AddFrom(-m_limits.GetFrom());
            e.AddTo(-m_limits.GetFrom());
        }
    }

    CGeneModel editedmodel = align;
    editedmodel.ClearExons();  // empty alignment with all atributes

    vector<TSignedSeqRange> transcript_exons;
    TInDels editedindels;
    bool snap_to_codons = (align.Type() == CAlignModel::eProt);
 
    for(int i = 0; i < (int)aexons.size(); ++i) {
        const CModelExon& e = aexons[i];
                
        if(e.Limits().NotEmpty()) {   // real exon
            list<CInDelInfo> exon_indels;
            ITERATE(TInDels, indl, aindels) {
                if(indl->IntersectingWith(e.GetFrom(), e.GetTo()))
                    exon_indels.push_back(*indl);
            }

            int left = e.GetFrom();  //projectable boundary
            int left_shrink = 0;     //unprojectable touching insertion
            int right = e.GetTo();   //projectable boundary
            int right_shrink = 0;    //unprojectable touching insertion
            int left_extend = 0;     //both alignment and correction indicate deletion of left_extend bases
            int right_extend = 0;    //both alignment and correction indicate deletion of right_extend base
            CAlignMap::ERangeEnd lend = CAlignMap::eLeftEnd;
            CAlignMap::ERangeEnd rend = CAlignMap::eRightEnd;

            TSignedSeqRange left_codon;
            TSignedSeqRange right_codon;
            if(align.Type() == CAlignModel::eProt) {
                if(i == 0)
                    left_codon = (align.Strand() == ePlus ? acds.Start() :  acds.Stop());
                if(i == (int)aexons.size()-1)
                    right_codon = (align.Strand() == ePlus ? acds.Stop() :  acds.Start());

                left_codon = amap.MapRangeEditedToOrig(left_codon, false);
                right_codon = amap.MapRangeEditedToOrig(right_codon, false);            
            }

            TInDels::const_iterator ileft = upper_bound(m_editing_indels.begin(), m_editing_indels.end(), left, OverlappingIndel);  // skip all correction left of exon (doesn't skip touching deletion)
            for( ;ileft != m_editing_indels.end() && ileft->GetStatus() != CInDelInfo::eGenomeNotCorrect; ++ileft);   //skip ggaps and Ns

            if(ileft != m_editing_indels.end() && ileft->IsDeletion() && ileft->Loc() == left) {
                if(!exon_indels.empty() && exon_indels.front().IsDeletion() && exon_indels.front().Loc() == left) {// ileft is touching deletion and there is matching indel in alignmnet
                    _ASSERT(left_codon.Empty());
                    left_extend = min(ileft->Len(),exon_indels.front().Len());
                }
                ++ileft;
            }

            //adjust left end
            int ll = left;
            if(left_codon.GetLength() == 3)
                ll = left_codon.GetTo();
            if(ileft != m_editing_indels.end() && ileft->Loc() <= ll) {  // left end is involved
                if(e.m_fsplice) {  // move splice to projectable point, add indels to keep the texon length
                    _ASSERT(left_codon.Empty());
                    left = ileft->Loc()+ileft->Len();  //could be only touching insertion
                    if(left > right)
                        return CAlignModel();
                    left_shrink = left-e.GetFrom();
                } else {
                    // clip to commom projectable point
                    TSignedSeqRange lim = e.Limits();
                    if(left_codon.GetLength() == 3)
                        lim.SetFrom(left_codon.GetTo()+1);
                    while(ileft != m_editing_indels.end() && ileft->Loc() <= lim.GetFrom()) {
                        lim.SetFrom(ileft->InDelEnd());
                        if(lim.NotEmpty())
                            lim = amap.ShrinkToRealPoints(lim, snap_to_codons);  // skip alignment indels   
                        if(lim.Empty())
                            return CAlignModel();

                        for( ;ileft != m_editing_indels.end() && ileft->InDelEnd() <= lim.GetFrom(); ++ileft); // skip outside corrections  
                    }

                    left = lim.GetFrom();
                    while(!exon_indels.empty() && exon_indels.front().InDelEnd() <= left)
                        exon_indels.pop_front();
                    lend = CAlignMap::eSinglePoint;  // is used for transcript exon
                }
            }            
            
            TInDels::const_iterator first_outside = ileft;
            for( ; first_outside != m_editing_indels.end() && first_outside->Loc() <= (first_outside->IsInsertion() ? right : right+1); ++first_outside); // end() or first completely on right
            reverse_iterator<TInDels::const_iterator> iright(first_outside);  // previous correction (last which interferes with exon or rend())
            for( ;iright != m_editing_indels.rend() && iright->GetStatus() != CInDelInfo::eGenomeNotCorrect; ++iright);   //skip ggaps and Ns

            if(iright != m_editing_indels.rend() && iright->IsDeletion() && iright->Loc() == right+1) {
                if(!exon_indels.empty() && exon_indels.back().IsDeletion() && exon_indels.back().Loc() == right+1) { // touching deletion and there is matching indel in alignmnet
                    _ASSERT(right_codon.Empty());
                    right_extend = min(iright->Len(),exon_indels.back().Len());
                }
                ++iright;
            }

            //adjust right end
            int rr = right;
            if(right_codon.GetLength() == 3)
                rr = right_codon.GetFrom();
            if(iright != m_editing_indels.rend() && iright->InDelEnd() > rr) {  // right end is involved
                if(e.m_ssplice) { // move splice to projectable point, add indels to keep the texon length
                    _ASSERT(right_codon.Empty());
                    right = iright->Loc()-1;
                    if(right < left)
                        return CAlignModel();                     
                    right_shrink = e.GetTo()-right;
                } else {
                    // clip to commom projectable point
                    TSignedSeqRange lim = e.Limits();
                    if(right_codon.GetLength() == 3)
                        lim.SetTo(right_codon.GetFrom()-1);
                    while(iright != m_editing_indels.rend() && iright->InDelEnd() > lim.GetTo()) { // iright is insertion including right position
                        lim.SetTo(iright->Loc()-1);
                        if(lim.NotEmpty())
                            lim = amap.ShrinkToRealPoints(lim, snap_to_codons);  // skip alignment indels
                        if(lim.Empty())
                            return CAlignModel();
                            
                        for( ; iright != m_editing_indels.rend() && iright->Loc() > lim.GetTo(); ++iright);  // skip outside corrections    
                    }

                    right = lim.GetTo();
                    while(!exon_indels.empty() && exon_indels.back().Loc() > right)
                        exon_indels.pop_back();
                    rend = CAlignMap::eSinglePoint;  // is used for transcript exon
                }
            }
            
            TSignedSeqRange orig_exon(left-left_shrink, right+right_shrink);
            TSignedSeqRange texon = amap.MapRangeOrigToEdited(orig_exon, lend, rend);
            transcript_exons.push_back(texon);

            TSignedSeqRange corrected_exon = m_edited_contig_map.MapRangeOrigToEdited(TSignedSeqRange(left, right), false);
            _ASSERT(corrected_exon.NotEmpty());
            //TODO: account for left/right shrink? Whe projected back, this will move all isertion inside the exon
            corrected_exon.SetFrom(corrected_exon.GetFrom()-left_extend);
            corrected_exon.SetTo(corrected_exon.GetTo()+right_extend);
            editedmodel.AddExon(corrected_exon, e.m_fsplice_sig, e.m_ssplice_sig, e.m_ident);
            if(i < (int)aexons.size()-1 && (!aexons[i].m_ssplice || !aexons[i+1].m_fsplice))  // hole
                editedmodel.AddHole();


            TInDels efs = CombineCorrectionsAndIndels(orig_exon, left_shrink, right_shrink, corrected_exon, m_reversed_corrections, TInDels(exon_indels.begin(), exon_indels.end()));
            editedindels.insert(editedindels.end(), efs.begin(), efs.end());
        } else {                     // gap exon
            transcript_exons.push_back(align.TranscriptExon(i));
            string gap_seq = e.m_seq;
            if(align.Orientation() == eMinus)
                ReverseComplement(gap_seq.begin(), gap_seq.end());

            TInDels::const_iterator gap = m_editing_indels.end();
            ITERATE(TInDels, ig, m_editing_indels) {
                if(ig->GetSource().m_range.NotEmpty()) {  //ggap 
                    if(i > 0 && ig->Loc() < aexons[i-1].GetTo())
                        continue;
                    if(i == 0 && ig->Loc() > aexons[i+1].GetFrom())
                        break;
                    if(ig->GetInDelV() == gap_seq) {
                        gap = ig;
                        if(i > 0) break;  //first available  for all exons except the first one 
                    }
                }
            }
            if(gap == m_editing_indels.end()) // gap is beyond align cluster
                return CAlignModel();
            //            _ASSERT(gap != m_editing_indels.end());

            int left_end = m_edited_contig_map.MapOrigToEdited(gap->Loc());
            if(left_end >= 0) {
                left_end -= gap->Len();
                for(TInDels::const_iterator ig = gap+1; ig != m_editing_indels.end() && ig->Loc() == gap->Loc(); ++ig)
                    left_end -= ig->Len();
            } else {
                left_end = m_edited_contig_map.MapOrigToEdited(gap->Loc()-1);
                _ASSERT(left_end >= 0);
                left_end += 1;
                for(TInDels::const_iterator ig = gap; ig != m_editing_indels.begin() && (ig-1)->Loc() == gap->Loc(); --ig) {
                    left_end += (ig-1)->Len();
                }                        
            }
                    
            editedmodel.AddExon(TSignedSeqRange(left_end,left_end+gap->Len()-1), "XX", "XX", 1);
        }
    }

    CAlignMap editedamap(editedmodel.Exons(), transcript_exons, editedindels, align.Orientation(), amap.TargetLen());

    editedmodel.FrameShifts() = editedindels;
    CAlignModel editedalign(editedmodel, editedamap);

    _ASSERT(align.GetEdgeReadingFrames()->empty());

    if(align.ReadingFrame().NotEmpty()) {
        double score = acds.Score();
        bool open = acds.OpenCds();
        acds.Clip(editedalign.TranscriptLimits());
        acds.SetScore(score, open);
        editedalign.SetCdsInfo(acds.MapFromEditedToOrig(editedamap));
    }

    return editedalign;
}

void CGnomonAnnotator_Base::MapAlignmentsToEditedContig(TAlignModelList& alignments) const
{
    ERASE_ITERATE(TAlignModelList, ia, alignments) {
        CAlignModel a = MapOneModelToEditedContig(*ia);
        if(a.Limits().NotEmpty()) {
            a.SetTargetId(*ia->GetTargetId());
            *ia = a;
        } else {
            alignments.erase(ia);
        }
    }
}

void CGnomonAnnotator_Base::MapModelsToEditedContig(TGeneModelList& models) const
{
    NON_CONST_ITERATE(TGeneModelList, ia, models) {
        *ia = MapOneModelToEditedContig(*ia);
        _ASSERT(!ia->Exons().empty());
    }
}

void CGnomonAnnotator_Base::SetGenomic(const CResidueVec& seq)
{
    m_edited_contig_map = CAlignMap(0, (TSignedSeqPos)seq.size()-1);
    m_editing_indels.clear();
    m_reversed_corrections.clear();
    m_confirmed_bases_len.clear();
    m_confirmed_bases_orig_len.clear();
    m_replacements.clear();
    m_inserted_seqs.clear();
    m_notbridgeable_gaps_len.clear();
    m_contig_acc.clear();
    m_gnomon.reset(new CGnomonEngine(m_hmm_params, seq, TSignedSeqRange::GetWhole(), m_pcsf_slice.get()));
}

// SetGenomic for annot - models could be 0
void CGnomonAnnotator_Base::SetGenomic(const CSeq_id& contig, CScope& scope, const string& mask_annots, const TGeneModelList* models) {
    SCorrectionData correction_data;
    m_notbridgeable_gaps_len.clear();
    
    if(models) {
        CBioseq_Handle bh(scope.GetBioseqHandle(contig));
        CSeqVector sv (bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
        int length (sv.size());
        string seq_txt;
        sv.GetSeqData(0, length, seq_txt);

        TIVec exons(length,0);

        ITERATE(TGeneModelList, i, *models) {
            ITERATE(CGeneModel::TExons, e, i->Exons()) {
                if(e->Limits().NotEmpty()) {
                    int a = e->GetFrom();
                    //                    if(a > 0 && !sv.IsInGap(a-1)) --a;
                    //                    if(a > 0 && !sv.IsInGap(a-1)) --a;
                    int b = e->GetTo();
                    //                    if(b < length-1 && !sv.IsInGap(b+1)) ++b;
                    //                    if(b < length-1 && !sv.IsInGap(b+1)) ++b;
                    //                    for(int p = a; p <= b; ++p) {  // block all exons and splices
                    for(int p = a+1; p <= b; ++p) {  // block all exons except first base (can't keep splices after all)
                        exons[p] = 1;                // mark positions which cannot be used for deletions
                    }                                // !!!!!!!!it is still a problem if gapfilled models are exactly next to each other!!!!!!!!!!!!!!
                }
            }
        }

        TIVec model_ranges(length,0);

        ITERATE(TGeneModelList, i, *models) {
            for(int p = max(0,i->Limits().GetFrom()-2); p <= min(length-1,i->Limits().GetTo()+2); ++p)
                model_ranges[p] = 1;

            ITERATE(TInDels, indl, i->FrameShifts()) {
                if(indl->GetStatus() == CInDelInfo::eGenomeNotCorrect) {
                    if(indl->IsMismatch()) {
                        string s = indl->GetInDelV();
                        for(int l = 0; l < indl->Len(); ++l)
                            correction_data.m_replacements[indl->Loc()+l] = s[l];
                    } else {
                        correction_data.m_correction_indels.push_back(*indl);
                    }
                }
                if(indl->GetStatus() != CInDelInfo::eUnknown) {
                    correction_data.m_confirmed_intervals.push_back(TSignedSeqRange(indl->Loc()-1,indl->InDelEnd()));
                    _ASSERT(correction_data.m_confirmed_intervals.back().GetFrom() >= 0 && correction_data.m_confirmed_intervals.back().GetTo() < length);
                }
            }
            for(int ie = 0; ie < (int)i->Exons().size(); ++ie) {
                const CModelExon& e = i->Exons()[ie];
                if(e.Limits().Empty()) {
                    int pos;
                    if(ie > 0) {
                        _ASSERT(i->Exons()[ie-1].Limits().NotEmpty());
                        for(pos = i->Exons()[ie-1].GetTo()+1; pos < length && exons[pos] > 0; ++pos);
                    } else {
                        _ASSERT((int)i->Exons().size() > 1 && i->Exons()[1].Limits().NotEmpty());
                        //                        for(pos = i->Exons()[1].GetFrom(); pos > 0 && exons[pos-1] > 0; --pos);
                        for(pos = i->Exons()[1].GetFrom(); pos > 0 && exons[pos] > 0; --pos);
                    }
                    string seq = e.m_seq;
                    CInDelInfo::SSource source = e.m_source;
                    if(i->Strand() == eMinus) {
                        ReverseComplement(seq.begin(),seq.end());
                        source.m_strand = OtherStrand(source.m_strand);
                    }
                    correction_data.m_correction_indels.push_back(CInDelInfo(pos, (int)seq.length(), CInDelInfo::eDel, seq, source));
                }
            }    
        }

        uniq(correction_data.m_correction_indels);  //remove duplicates from altvariants
        ERASE_ITERATE(TInDels, indl, correction_data.m_correction_indels) {  // remove 'partial' indels
            TInDels::iterator next = indl;
            if(++next != correction_data.m_correction_indels.end() && indl->Loc() == next->Loc()) {
                if(indl->GetSource().m_range.Empty() && next->GetSource().m_range.Empty()) {
                    _ASSERT(indl->IsDeletion());
                    _ASSERT(next->IsDeletion());
                    VECTOR_ERASE(indl, correction_data.m_correction_indels);
                }
            }
        }

        TIntMap::iterator current_gap = m_notbridgeable_gaps_len.end();
        for(int i = 0; i < length; ++i) {
            if(model_ranges[i])
                continue;

            CConstRef<CSeq_literal> gsl = sv.GetGapSeq_literal(i);
            if(gsl && gsl->GetBridgeability() == CSeq_literal::e_NotBridgeable) {               
                if(current_gap == m_notbridgeable_gaps_len.end())                    
                    current_gap = m_notbridgeable_gaps_len.insert(TIntMap::value_type(i,1)).first;
                else
                    ++current_gap->second;
            } else {
                current_gap = m_notbridgeable_gaps_len.end();
            }
        }
    }

    SetGenomic(contig, scope, correction_data,  TSignedSeqRange::GetWhole(), mask_annots);
}

void CGnomonAnnotator_Base::SetGenomic(const CSeq_id& contig, CScope& scope, const SCorrectionData& correction_data, TSignedSeqRange limits, const string& mask_annots)
{
    m_contig_acc = CIdHandler::ToString(contig);

    CResidueVec seq;
    int length;

    CBioseq_Handle bh(scope.GetBioseqHandle(contig));
    {
        CSeqVector sv (bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
        length = sv.size(); 
        if(limits == TSignedSeqRange::GetWhole()) {
            limits.SetFrom(0);
            limits.SetTo(length-1);
        }
        int GC_RANGE = 200000;
        limits.SetFrom(max(0, limits.GetFrom()-GC_RANGE/2));
        limits.SetTo(min(length-1, limits.GetTo()+GC_RANGE/2));
        length = limits.GetLength();
        m_limits = limits;
        seq.reserve(length);
        for(int i = limits.GetFrom(); i <= limits.GetTo(); ++i)
            seq.push_back(sv[i]);
    }

    if (m_masking) {
        SAnnotSelector sel;
        {
            list<string> arr;
            NStr::Split(mask_annots, " ", arr, NStr::fSplit_MergeDelimiters|NStr::fSplit_Truncate);
            ITERATE(list<string>, annot, arr) {
                sel.AddNamedAnnots(*annot);
            }
        }
        sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_repeat_region)
            .SetResolveAll()
            .SetAdaptiveDepth(true);
        for (CFeat_CI it(bh, sel);  it;  ++it) {
            TSeqRange range = it->GetLocation().GetTotalRange();
            for(unsigned int i = range.GetFrom(); i <= range.GetTo(); ++i) {
                if(Include(limits, i))
                    seq[i-limits.GetFrom()] = tolower(seq[i-limits.GetFrom()]);
            }
        }
    }

    m_editing_indels.clear();
    m_reversed_corrections.clear();
    m_confirmed_bases_len.clear();
    m_confirmed_bases_orig_len.clear();
    m_replacements.clear();
    m_inserted_seqs.clear();

    m_replacements = correction_data.m_replacements;
    for(map<int,char>::iterator ir = m_replacements.begin(); ir != m_replacements.end(); ++ir) {
        if(Include(limits,ir->first)) {
            m_replaced_bases[ir->first-limits.GetFrom()] = seq[ir->first-limits.GetFrom()];
            seq[ir->first-limits.GetFrom()] = ir->second;
        }
    }


#define     BLOCK_OF_Ns 35
    for(auto cor :  correction_data.m_correction_indels) {
        if(cor.GetSource().m_range.Empty() && Include(limits, cor.Loc())) { // correction indel
            cor.SetLoc(cor.Loc()-limits.GetFrom());
            m_editing_indels.push_back(cor);
        } else if(cor.Loc() >= limits.GetFrom() && cor.Loc() <= limits.GetTo()+1) {     // ggap (1bp fake ggaps may be loctated right before or after contig)
            int l = cor.Loc()-limits.GetFrom();
            CInDelInfo g(l, cor.Len(), cor.GetType(), cor.GetInDelV(), cor.GetSource());
            //surround ggap with Ns to satisfy MinIntron
            CInDelInfo Ns(l, BLOCK_OF_Ns, CInDelInfo::eDel, string(BLOCK_OF_Ns,'N'));
            m_editing_indels.push_back(Ns);
            m_editing_indels.push_back(g);
            m_editing_indels.push_back(Ns);
        }
    }
                
    m_edited_contig_map = CAlignMap(0, length-1, m_editing_indels.begin(), m_editing_indels.end());
    {
        CResidueVec editedseq;
        m_edited_contig_map.EditedSequence(seq,editedseq);
        swap(seq, editedseq);
    }

    // init phyloCSF slice
    if(m_pcsf_data != nullptr) {
        m_pcsf_slice.reset(m_pcsf_data->CreateSliceForContig(m_contig_acc));
        if(m_pcsf_slice) {
            m_pcsf_slice->m_map = &m_edited_contig_map;
            m_pcsf_slice->m_shift = m_limits.GetFrom();
            m_pcsf_slice->m_factor = m_pcsf_factor;
        }
    }
           
    ITERATE(TInDels, ig, m_editing_indels) {
        TInDels::const_iterator nexti = next(ig);
        if(nexti != m_editing_indels.end() && nexti->GetSource().m_range.NotEmpty() && nexti->Loc() == ig->Loc())  // block of Ns
            continue;

        if(ig->GetSource().m_range.NotEmpty()) {  //ggap    
            int left_end = m_edited_contig_map.MapOrigToEdited(ig->Loc());
            if(left_end >= 0) {
                left_end -= ig->Len();
                for(TInDels::const_iterator igg = ig+1; igg != m_editing_indels.end() && igg->Loc() == ig->Loc(); ++igg)
                    left_end -= igg->Len();
            } else {
                left_end = m_edited_contig_map.MapOrigToEdited(ig->Loc()-1);
                _ASSERT(left_end >= 0);
                left_end += 1;
                for(TInDels::const_iterator i = ig; i != m_editing_indels.begin() && (i-1)->Loc() == ig->Loc(); --i) {
                    left_end += (i-1)->Len();
                }                        
            }
            m_inserted_seqs[left_end] = ig;
            ++ig;   // skip  block of Ns
        } else {
            int loc = m_edited_contig_map.MapOrigToEdited(ig->InDelEnd());
            _ASSERT(loc >= 0);
            if(ig->IsInsertion()) {
                string s(seq.begin()+ig->Loc(), seq.begin()+ig->Len());
                m_reversed_corrections.push_back(CInDelInfo(loc, ig->Len(), CInDelInfo::eDel, NStr::ToUpper(s)));
            } else {
                m_reversed_corrections.push_back(CInDelInfo(loc-ig->Len(), ig->Len(), CInDelInfo::eIns));
            }
            m_reversed_corrections.back().SetStatus(ig->GetStatus());
        }
    }

    set<int> confirmed_bases;
    for(list<TSignedSeqRange>::const_iterator it = correction_data.m_confirmed_intervals.begin(); it != correction_data.m_confirmed_intervals.end(); ++it) {
        TSignedSeqRange lim = *it;
        _ASSERT(lim.NotEmpty());
        for(int p = lim.GetFrom(); p <= lim.GetTo(); ++p)
            confirmed_bases.insert(p);
    }
    TIntMap::iterator cbase_len = m_confirmed_bases_orig_len.end();
    ITERATE(set<int>, ip, confirmed_bases) {
        if(cbase_len == m_confirmed_bases_orig_len.end() || *ip != cbase_len->first+cbase_len->second)
            cbase_len = m_confirmed_bases_orig_len.insert(TIntMap::value_type(*ip,1)).first;
        else
            ++cbase_len->second;
    }

    ITERATE(TIntMap, ic,  m_confirmed_bases_orig_len) {
        TSignedSeqRange lim(ic->first, ic->first+ic->second-1);
        lim = m_edited_contig_map.MapRangeOrigToEdited(lim, false);
        _ASSERT(lim.NotEmpty());
        m_confirmed_bases_len[lim.GetFrom()] = lim.GetLength();
    }

    TIntMap notbridgeable_gaps_len;
    ITERATE(TIntMap, ig, m_notbridgeable_gaps_len) {
        int pos = m_edited_contig_map.MapOrigToEdited(ig->first);
        _ASSERT(pos >= 0);
        notbridgeable_gaps_len[pos] = ig->second;
    }
    m_notbridgeable_gaps_len = notbridgeable_gaps_len;

    
    m_gnomon.reset(new CGnomonEngine(m_hmm_params, std::move(seq), TSignedSeqRange::GetWhole(), m_pcsf_slice.get()));
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
        fivep = align.TranscriptExon((int)align.Exons().size()-1).GetFrom();
    if((align.Status()&CGeneModel::eReversed) == 0 && caps.find(acc) != caps.end() && fivep < capgap)
        align.Status() |= CGeneModel::eCap;
}

MarkupTrustedGenes::MarkupTrustedGenes(const set<string>& _trusted_genes) : trusted_genes(_trusted_genes) {}

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
        if (!hole && m.Exons()[i].m_fsplice_sig != "XX" && m.Exons()[i-1].m_ssplice_sig != "XX" && intron < gnomon.GetMinIntronLen()) {
            return true;
        } 
    }
    return false;
}

HasLongIntron::HasLongIntron(CGnomonEngine& _gnomon)
    :gnomon(_gnomon) {}

bool HasLongIntron::model_predicate(CGeneModel& m)
{
    for(unsigned int i = 1; i < m.Exons().size(); ++i) {
        bool hole = !m.Exons()[i-1].m_ssplice || !m.Exons()[i].m_fsplice;
        int intron = m.Exons()[i].GetFrom()-m.Exons()[i-1].GetTo()-1;
        if (!hole && intron > gnomon.GetMaxIntronLen()) {
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
    if(a.Exons().size() == 1 && min(a.Limits().GetLength(),alignmap.FShiftedLen(alignmap.ShrinkToRealPoints(a.Limits()),false)) < 2*minex) {
        // one exon and it is short
        a.CutExons(a.Limits());
        return;
    }

    bool snap_to_codons = ((a.Type() & CAlignModel::eProt)!=0);      
    TSignedSeqPos left  = a.Limits().GetFrom();
    if ((a.Exons().size() > 1 && !a.Exons().front().m_ssplice) || (a.Type() & CAlignModel::eProt)==0 || !a.LeftComplete()) {
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
    if ((a.Exons().size() > 1 && !a.Exons().back().m_fsplice) || (a.Type() & CAlignModel::eProt)==0 || !a.RightComplete()) {
        for(auto i = a.Exons().size()-1; i > 0; --i) {
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
    if(newlimits.NotEmpty()) {
        newlimits = alignmap.ShrinkToRealPoints(newlimits,snap_to_codons);
        if(newlimits != a.Limits()) {
            if(newlimits.GetLength() < 2*minex || alignmap.FShiftedLen(newlimits,false) < 2*minex) {
                a.CutExons(a.Limits());
                return;
            }
            a.Clip(newlimits,CAlignModel::eRemoveExons);
        }
    } else {
        a.CutExons(a.Limits());
        return;
    }
    

    for (size_t i = 1; i < a.Exons().size()-1; ++i) {
        const CModelExon* e = &a.Exons()[i];
        
        while (!e->m_ssplice && EffectiveExonLength(*e, alignmap, snap_to_codons) < minex) {

            if(i == 0) { //first exon
                a.CutExons(*e);
                e = &a.Exons()[0];    // we still have at least one exon
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


