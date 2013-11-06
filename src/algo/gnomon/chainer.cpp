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
#include <algo/gnomon/annot.hpp>

#include <map>
#include <sstream>

#include <objects/general/Object_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include "gnomon_seq.hpp"


BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(gnomon)

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
    CChainerImpl(CRef<CHMMParameters>& hmm_params, auto_ptr<CGnomonEngine>& gnomon);
    void SetGenomicRange(const TAlignModelList& alignments);
    void SetConfirmedStartStopForProteinAlignments(TAlignModelList& alignments);
    void DropAlignmentInfo(TAlignModelList& alignments, TGeneModelList& models);

    void FilterOutChimeras(TGeneModelList& clust);
    void ScoreCDSes_FilterOutPoorAlignments(TGeneModelList& clust);
    void FilterOutInferiorProtAlignmentsWithIncompatibleFShifts(TGeneModelList& clust);
    void ReplicateFrameShifts(TGeneModelList& models);


    TGeneModelList MakeChains(TGeneModelList& models);
    void FilterOutBadScoreChainsHavingBetterCompatibles(TGeneModelList& chains);
    void CombineCompatibleChains(TChainList& chains);
    void SetFlagsForChains(TChainList& chains);
    SChainMember* FindOptimalChainForProtein(vector<SChainMember*>& pointers_all, vector<CGeneModel*>& parts, CGeneModel& palign);
    void CreateChainsForPartialProteins(TChainList& chains, vector<SChainMember*>& pointers);

    void CutParts(TGeneModelList& clust);
    void IncludeInContained(SChainMember& big, SChainMember& small);
    void FindContainedAlignments(vector<SChainMember*>& pointers);
    void DuplicateNotOriented(CChainMembers& pointers, TGeneModelList& clust);
    void Duplicate5pendsAndShortCDSes(CChainMembers& pointers, TGeneModelList& clust);
    void ScoreCdnas(CChainMembers& pointers);
    void DuplicateUTRs(CChainMembers& pointers);
    void CalculateSpliceWeights(CChainMembers& pointers);
    bool LRCanChainItoJ(int& delta_cds, double& delta_num, double& delta_splice_num, const SChainMember& mi, const SChainMember& mj, TContained& contained);
    void LRIinit(SChainMember& mi);
    void LeftRight(vector<SChainMember*>& pointers);
    void RightLeft(vector<SChainMember*>& pointers);
    double GoodCDNAScore(const CGeneModel& algn);
    void RemovePoorCds(CGeneModel& algn, double minscor);
    void SkipReason(CGeneModel* orig_align, const string& comment);
    bool AddIfCompatible(set<SFShiftsCluster>& fshift_clusters, const CGeneModel& algn);
    bool FsTouch(const TSignedSeqRange& lim, const CInDelInfo& fs);
    void AddFShifts(TGeneModelList& clust, const TInDels& fshifts);
    void CollectFShifts(const TGeneModelList& clust, TInDels& fshifts);
    void SplitAlignmentsByStrand(const TGeneModelList& clust, TGeneModelList& clust_plus, TGeneModelList& clust_minus);

    void FindGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet); 
    void FindAltsForGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet);
    void PlaceAllYouCan(list<CGene>& alts, TChainPointerList& not_placed_yet, TChainPointerList& rejected);
    enum ECompat { eNotCompatible, eAlternative, eNested, eExternal, eOtherGene };
    ECompat CheckCompatibility(const CGene& gene, const CChain& algn);
    list<CGene> FindGenes(TChainList& cls);
    void FilterOutSimilarsWithLowerScore(TChainPointerList& not_placed_yet, TChainPointerList& rejected);
    void FilterOutTandemOverlap(TChainPointerList& not_placed_yet, TChainPointerList& rejected, double fraction);
    void TrimAlignmentsIncludedInDifferentGenes(list<CGene>& genes);


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
    TUnmodAligns unmodified_aligns;

    map<TSignedSeqRange,int> mrna_count;
    map<TSignedSeqRange,int> est_count;
    map<TSignedSeqRange,int> rnaseq_count;
    bool has_rnaseq;
    set<TSignedSeqRange> oriented_introns_plus;
    set<TSignedSeqRange> oriented_introns_minus;

    double altfrac;
    int composite;
    bool allow_opposite_strand;
    bool allow_partialalts;
    int tolerance;

    int m_idnext;
    int m_idinc;

    friend class CChainer;
    friend class CChainerArgUtil;
};

CGnomonAnnotator_Base::CGnomonAnnotator_Base()
    : m_masking(false), m_edited_contig_map(0)
{
}

CGnomonAnnotator_Base::~CGnomonAnnotator_Base()
{
    delete m_edited_contig_map;
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
    :m_hmm_params(hmm_params), m_gnomon(gnomon), m_idnext(1), m_idinc(1)
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
        m_align(0), m_cds_info(0), m_align_map(0), m_left_member(0), m_right_member(0),
        m_copy(0), m_contained(0), m_identical_count(0), m_contained_weight(0),
        m_left_num(0), m_right_num(0), m_num(0),
        m_splice_weight(0), m_contained_splice_weight(0), m_left_splice_num(0), m_right_splice_num(0), m_splice_num(0),
        m_type(eCDS), m_left_cds(0), m_right_cds(0), m_cds(0), m_included(false),  m_postponed(false),
        m_marked_for_deletion(false), m_marked_for_retention(false), 
        m_gapped_connection(false), m_fully_connected_to_part(-1), m_not_for_chaining(false),
        m_rlimb(numeric_limits<int>::max()),  m_llimb(numeric_limits<int>::max()), m_orig_align(0), m_unmd_align(0), m_mem_id(0) {}

    TContained CollectContainedForChain();
    void MarkIncludedForChain();
    void MarkPostponedForChain();
    void MarkUnwantedCopiesForChain(const TSignedSeqRange& cds);
    TContained CollectContainedForMemeber();
    void AddToContained(TContained& contained, TMemberPtrSet& included_in_list);

    CGeneModel* m_align;
    const CCDSInfo* m_cds_info;
    CAlignMap* m_align_map;
    SChainMember* m_left_member;
    SChainMember* m_right_member;
    TContained* m_copy;      // is used to make sure that the copy of already incuded duplicated alignment is not included in contained and doesn't trigger a new chain genereation
    TContained* m_contained;
    int m_identical_count;
    double m_contained_weight;
    double m_left_num, m_right_num, m_num; 
    double m_splice_weight, m_contained_splice_weight;
    double m_left_splice_num, m_right_splice_num, m_splice_num; 
    int m_type, m_left_cds, m_right_cds, m_cds;
    bool m_included;
    bool m_postponed;
    bool m_marked_for_deletion;
    bool m_marked_for_retention;
    bool m_gapped_connection;          // used for gapped proteins
    int m_fully_connected_to_part;     // used for gapped proteins
    bool m_not_for_chaining;           // included in other alignmnet(s) or supressed and can't trigger a different chain
    int m_rlimb;                       // leftmost compatible rexon
    int m_llimb;                       // leftmost not compatible lexon

    CAlignModel* m_orig_align;
    CGeneModel* m_unmd_align;

    int m_mem_id;
};

class CChain : public CGeneModel
{
public:
    CChain(SChainMember& mbr, CGeneModel* gapped_helper = 0);

    void RestoreTrimmedEnds(int trim);
    void RemoveFshiftsFromUTRs();
    void RestoreReasonableConfirmedStart(const CGnomonEngine& gnomon, TOrigAligns& orig_aligns);
    void SetOpenForPartialyAlignedProteins(map<string, pair<bool,bool> >& prot_complet, TOrigAligns& orig_aligns);
    void ClipToCompleteAlignment(EStatus determinant); // determinant - cap or polya
    void ClipLowCoverageUTR(double utr_clip_threshold, const CGnomonEngine& gnomon);
    void CalculateDropLimits();
    void CalculateSupportAndWeightFromMembers();
    void ClipChain(TSignedSeqRange limits);

    void SetConfirmedStartStopForCompleteProteins(map<string, pair<bool,bool> >& prot_complet, TOrigAligns& orig_aligns, const SMinScor& minscor);
    void CollectTrustedmRNAsProts(TOrigAligns& orig_aligns, const SMinScor& minscor);
    void SetBestPlacement(TOrigAligns& orig_aligns);
    void SetConsistentCoverage();

    bool HarborsNested(const CChain& other_chain, bool check_in_holes) const;
    bool HarborsNested(const CGene& other_gene, bool check_in_holes) const;

    bool HasTrustedEvidence() const;

    TContained m_members;
    int m_polya_cap_right_hard_limit;
    int m_polya_cap_left_hard_limit;
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

    if(a.PStop() || !a.FrameShifts().empty())
        return false;
    if(front()->PStop() || !front()->FrameShifts().empty())
        return false;

    for(int i = 1; i < (int)a.Exons().size(); ++i) {   // check for gapfillers
        if(a.Exons()[i-1].m_ssplice_sig == "XX" || a.Exons()[i].m_fsplice_sig == "XX")
            return false;
    }

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
            if(a.Exons()[i-1].m_ssplice && a.Exons()[i].m_fsplice) {
                TSignedSeqRange intron(a.Exons()[i-1].GetTo()+1,a.Exons()[i].GetFrom()-1);
                if(b_introns.insert(intron).second) {
                    a_has_new_intron = true;
                    continue;
                }
            }
        }
 
        if(a_has_new_intron) {
            continue;
        } else if(a.RealCdsLimits().NotEmpty() && b.RealCdsLimits().NotEmpty() && !a.RealCdsLimits().IntersectingWith(b.RealCdsLimits()) && (!a.TrustedmRNA().empty() || !a.TrustedProt().empty())) {
#ifdef _DEBUG 
            const_cast<CGeneModel&>(a).AddComment("Secondary CDS");
#endif    
            continue;
        } else if(a.RealCdsLen() <= b.RealCdsLen()){
            return false;
        }
    }

    return true;
}

bool CGene::IsAlternative(const CChain& a) const
{
    _ASSERT( size()>0 );

    if (a.Strand() != front()->Strand())
        return false;

    bool has_common_splice = false;

    ITERATE(CGene, it, *this) {
        if(CModelCompare::CountCommonSplices(**it, a) > 0) {      // has common splice
            has_common_splice = true;
            break;
        }
    }

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
        }

        bool gene_has_trusted = false;
        ITERATE(CGene, it, *this) {
            if((*it)->HasTrustedEvidence()) {
                gene_has_trusted = true;
                break;
            }
        }

        if(has_common_cds || (has_common_splice && !gene_has_trusted && !a.HasTrustedEvidence()))
            return true;
        else
            return false;
    }

    return has_common_splice;
}

int NumOfRetainedIntrons(const CGeneModel& under_test, const CGeneModel& control_model)
{
    int num = 0;
    for(int i = 1; i < (int)control_model.Exons().size(); ++i) {
        if(control_model.Exons()[i-1].m_ssplice && control_model.Exons()[i].m_fsplice) {
            TSignedSeqRange intron(control_model.Exons()[i-1].GetTo()+1,control_model.Exons()[i].GetFrom()-1);
            ITERATE(CGeneModel::TExons, test_exon, under_test.Exons()) {
                if(Include(test_exon->Limits(), intron))
                    ++num;
            }
        }
    } 
    return num;
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

        double ds = 0.025*(fabs(b.Score())+fabs(a.Score()));
        
        double as = a.Score();
        if((a.Status()&CGeneModel::ecDNAIntrons) != 0)
            as += 2*ds;
        if((a.Status()&CGeneModel::ePolyA) != 0)
            as += ds; 
        if((a.Status()&CGeneModel::eCap) != 0)
            as += ds; 
        if(a.isNMD())
            as -= ds;
        
        double bs = b.Score();
        if((b.Status()&CGeneModel::ecDNAIntrons) != 0)
            bs += 2*ds;
        if((b.Status()&CGeneModel::ePolyA) != 0)
            bs += ds; 
        if((b.Status()&CGeneModel::eCap) != 0)
            bs += ds; 
        if(b.isNMD())
            bs -= ds;
        
        if(as > bs + 0.001*ds)    // better score
            return true;
        else if(bs > as + 0.001*ds)
            return false;
        else if(a.m_splice_weight > b.m_splice_weight) // more splice support
            return true;
        else if(a.m_splice_weight < b.m_splice_weight)
            return false;
        else if(a.Weight() > b.Weight())       // more alignments is better
            return true;
        else if(a.Weight() < b.Weight()) 
            return false;
        else 
            return (a.Limits().GetLength() < b.Limits().GetLength());   // everything else equal prefer compact model
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
        else 
            return (a.Limits().GetLength() < b.Limits().GetLength());   // everything else equal prefer compact model
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

    if(algn.HarborsNested(gene, gene_good_enough_to_be_annotation)) {    // gene is nested in align's intron (could be partial)
        if(gene_good_enough_to_be_annotation || algn.HasTrustedEvidence())
            return eExternal;
        else
            return eNotCompatible;
    }

    if(gene.HarborsNested(algn, algn_good_enough_to_be_annotation)) {   // algn is nested in gene (could be partial)
        if(algn_good_enough_to_be_annotation || algn.HasTrustedEvidence())
            return eNested;
        else
            return eNotCompatible;
    }

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
        else if(algn.Score() < 2*minscor.m_min && algn.GetCdsInfo().ProtReadingFrame().Empty())  // postpone not so good models
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

void CChainer::CChainerImpl::FindAltsForGeneSeeds(list<CGene>& alts, TChainPointerList& not_placed_yet) {

    not_placed_yet.sort(DescendingModelOrderPConsistentCoverage);

    for(TChainPointerList::iterator itloop = not_placed_yet.begin(); itloop != not_placed_yet.end(); ) {
        TChainPointerList::iterator it = itloop++;
        CChain& algn(**it);

        list<list<CGene>::iterator> included_in;
        list<CGene*> possibly_nested;

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
            case eNotCompatible:
                rejected.push_back(&algn);
                rejected.back()->Status() |= CGeneModel::eSkipped;
                ost << "Trumped by another model " << itl->front()->ID()-algn.ID();
                rejected.back()->AddComment(CNcbiOstrstreamToString(ost));
                good_model = false;
                break;
            case eAlternative:
                if(!allow_partialalts && !itl->front()->GoodEnoughToBeAnnotation()) {
                    rejected.push_back(&algn);
                    rejected.back()->Status() |= CGeneModel::eSkipped;
                    ost << "    Trumped by another model " << itl->front()->ID()-algn.ID();
                    rejected.back()->AddComment(CNcbiOstrstreamToString(ost));
                    good_model = false;
                } else if(included_in == alts.end()) {
                    included_in = itl;
                } else {  // tries to connect two different genes
                    good_model = false;
                    rejected.push_back(&algn);
                    rejected.back()->Status() |= CGeneModel::eSkipped;
                    ost << "Connects two genes " << itl->front()->ID()-algn.ID() << " " << included_in->front()->ID()-algn.ID();
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
                ost << "Trumped by similar chain " << ai.ID()-aj.ID();
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

    NON_CONST_ITERATE(TChainPointerList, l, bad_aligns)
        (*l)->Status() |= CGeneModel::eSkipped;

    return alts;
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


typedef vector< pair<SChainMember*,CGene*> > TMemeberGeneVec;

struct AlignIdOrder
{
    bool operator()(const TMemeberGeneVec::value_type& a, const TMemeberGeneVec::value_type& b) 
    {
        return a.first->m_align->ID() < b.first->m_align->ID();
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
                if(orig_aligns[m.m_align->ID()]->Continuous())
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
    typedef map<Int8,TGeneToMembers> TMembersInDiffGenes;
    TMembersInDiffGenes members_in_different_genes;
    members_in_different_genes[members_genes[0].first->m_align->ID()][members_genes[0].second].push_back(members_genes[0].first);
    for(int i = 1; i < (int)members_genes.size(); ++i) {
        if(members_genes[i-1].first->m_align->ID() != members_genes[i].first->m_align->ID()) {
            TMembersInDiffGenes::iterator it = members_in_different_genes.find(members_genes[i-1].first->m_align->ID());
            if(it->second.size() < 2) // alignment in only one gene
                members_in_different_genes.erase(it); 
        }
         members_in_different_genes[members_genes[i].first->m_align->ID()][members_genes[i].second].push_back(members_genes[i].first);
    }
    TMembersInDiffGenes::iterator it = members_in_different_genes.find(members_genes.back().first->m_align->ID());
    if(it->second.size() < 2) // alignment in only one gene
        members_in_different_genes.erase(it); 

    ITERATE(TMembersInDiffGenes, imdg, members_in_different_genes) {
        ITERATE(TGeneToMembers, ig1, imdg->second) {
            CGene& gene1 = *ig1->first;
            ITERATE(CGene, ic1, gene1) { 
                CChain& chain1 = **ic1;
                sort(chain1.m_members.begin(),chain1.m_members.end());
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
                    if(binary_search(chain1p_orig->m_members.begin(),chain1p_orig->m_members.end(),*im))
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
                            if(binary_search(chain2p->m_members.begin(),chain2p->m_members.end(),*im))
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

    ITERATE(TMembersInDiffGenes, imdg, members_in_different_genes) {
        ITERATE(TGeneToMembers, ig1, imdg->second) {
            CGene& gene1 = *ig1->first;
            ITERATE(CGene, ic1, gene1) { 
                CChain& chain1 = **ic1;
                sort(chain1.m_members.begin(),chain1.m_members.end(),GenomeOrderD());
            }
        }
    }


    ITERATE(TConflictMemebersInChains, it, conflict_members_in_chains) {
        CChain& chain = *it->first;
        const TMemberPtrSet& conflict_members = it->second;

        CAlignMap amap = chain.GetAlignMap();

        TSignedSeqRange hard_limits(chain.Exons().front().Limits().GetTo()-15,chain.Exons().back().Limits().GetFrom()+15);
        hard_limits = (hard_limits & chain.Limits());
        if(chain.ReadingFrame().NotEmpty())
            hard_limits = (chain.OpenCds() ? chain.MaxCdsLimits() : chain.RealCdsLimits());

        int hard_limits_len = amap.FShiftedLen(hard_limits);

        TSignedSeqRange noclip_limits = hard_limits;
        ITERATE(TContained, i, chain.m_members) {
            const CGeneModel& a = *(*i)->m_align;
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

        if(chain.Status()&CGeneModel::ePolyA) {
            if(chain.Strand() == ePlus) {
                if(chain.m_coverage_drop_right < 0)
                    noclip_limits.SetTo(chain.m_polya_cap_right_soft_limit);
                else
                    noclip_limits.SetTo(chain.m_coverage_drop_right);
            } else {
                if(chain.m_coverage_drop_left < 0)
                    noclip_limits.SetFrom(chain.m_polya_cap_left_soft_limit);
                else
                    noclip_limits.SetFrom(chain.m_coverage_drop_left);
            }
        }
        if(chain.Status()&CGeneModel::eCap) {
            if(chain.Strand() == ePlus) {
                if(chain.m_coverage_drop_left < 0)
                    noclip_limits.SetFrom(chain.m_polya_cap_left_soft_limit);
                else
                    noclip_limits.SetFrom(chain.m_coverage_drop_left);
            } else {
                if(chain.m_coverage_drop_right < 0)
                    noclip_limits.SetTo(chain.m_polya_cap_right_soft_limit);
                else
                    noclip_limits.SetTo(chain.m_coverage_drop_right);
            }
        }

        TSignedSeqRange new_limits = chain.Limits();
        ITERATE(TMemberPtrSet, im, conflict_members) {
            const CGeneModel& a = *(*im)->m_align;
            if(a.Limits().GetFrom() < noclip_limits.GetFrom()) {
                int to = min(noclip_limits.GetFrom(),a.Limits().GetTo());
                if(chain.m_coverage_drop_left > 0 && Include(a.Limits(),chain.m_coverage_drop_left)) {
                    to = min(noclip_limits.GetFrom(),chain.m_coverage_drop_left);
                }
                new_limits.SetFrom(max(new_limits.GetFrom(),to));
            } else if(a.Limits().GetTo() > noclip_limits.GetTo()) {
                int from = max(noclip_limits.GetTo(),a.Limits().GetFrom());
                if(chain.m_coverage_drop_right > 0 && Include(a.Limits(),chain.m_coverage_drop_right)) {
                    from = max(noclip_limits.GetTo(),chain.m_coverage_drop_right);
                }
                new_limits.SetTo(min(new_limits.GetTo(),from));
            }
        }

        if(new_limits != chain.Limits()) {

            chain.AddComment("Overlap UTR clip");

            _ASSERT(new_limits.NotEmpty());

            bool wasopen = chain.OpenCds();
            chain.ClipChain(new_limits);
            chain.ClipToCompleteAlignment(CGeneModel::eCap);
            chain.ClipToCompleteAlignment(CGeneModel::ePolyA);
            if(chain.Type()&CGeneModel::eNested)
                chain.ClipLowCoverageUTR(0.1, *m_gnomon);
            _ASSERT(chain.Limits().NotEmpty());
            if(chain.ReadingFrame().NotEmpty()) {
                m_gnomon->GetScore(chain);
                CCDSInfo cds = chain.GetCdsInfo();
                if(wasopen != chain.OpenCds() && (wasopen == false || cds.HasStart())) {
                    cds.SetScore(cds.Score(),wasopen);
                    chain.SetCdsInfo(cds);
                }
            }
        }
    }
}



//visits all levels of nested and adds uniquely to contained
void SChainMember::AddToContained(TContained& contained, TMemberPtrSet& included_in_list) {

    list<const SChainMember*> not_visited(1,this);
    while(!not_visited.empty()) {
        const SChainMember* mbr = not_visited.front();
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

void SChainMember::MarkIncludedForChain()
{
    TContained contained = CollectContainedForChain();
    NON_CONST_ITERATE (TContained, i, contained) {
        SChainMember* mi = *i;
        mi->m_included = true;
        if (mi->m_copy != 0) {
            ITERATE(TContained, j, *mi->m_copy) {
                SChainMember* mj = *j;
                if(mj->m_type != eCDS || mj->m_cds < START_BONUS+25 || 
                   (mi->m_align->Strand() == mj->m_align->Strand() &&
                    (mi->m_cds_info->ReadingFrame().GetFrom() == mj->m_cds_info->ReadingFrame().GetFrom() ||   // same copy or supressed start
                     mi->m_cds_info->ReadingFrame().GetTo() == mj->m_cds_info->ReadingFrame().GetTo())))       // same copy or supressed start
                    mj->m_included = true;
            }
        }
    }
}

void SChainMember::MarkPostponedForChain()
{
    TContained contained = CollectContainedForChain();
    NON_CONST_ITERATE (TContained, i, contained) {
        SChainMember* mi = *i;
        mi->m_postponed = true;
        if (mi->m_copy != 0) {
            ITERATE(TContained, j, *mi->m_copy) {
                SChainMember* mj = *j;
                if(mj->m_type != eCDS || mj->m_cds < START_BONUS+25 || 
                   (mi->m_align->Strand() == mj->m_align->Strand() &&
                    (mi->m_cds_info->ReadingFrame().GetFrom() == mj->m_cds_info->ReadingFrame().GetFrom() ||   // same copy or supressed start
                     mi->m_cds_info->ReadingFrame().GetTo() == mj->m_cds_info->ReadingFrame().GetTo())))       // same copy or supressed start
                    mj->m_postponed = true;
            }
        }
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
        else if(fabs(ap->m_splice_num - bp->m_splice_num) > 0.001)
            return (ap->m_splice_num > bp->m_splice_num);
        else if(fabs(ap->m_num - bp->m_num) > 0.001)
            return (ap->m_num > bp->m_num);
        else
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
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
        if (ap->m_cds_info->Score() == bp->m_cds_info->Score())
            return ap->m_mem_id < bp->m_mem_id; // to make sort deterministic
        else
            return (ap->m_cds_info->Score() > bp->m_cds_info->Score());
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
    CChainMembers() { m_extra_cds.push_back(CCDSInfo()); }   // empty cds for utrs; first in the list
    CChainMembers(TGeneModelList& clust, TOrigAligns& orig_aligns, TUnmodAligns& unmodified_aligns);
    void InsertMember(CGeneModel& algn, SChainMember* copy_ofp = 0);
    void InsertMemberCopyWithCds(const CCDSInfo& cds, SChainMember* copy_ofp);
    void InsertMemberCopyAndStoreCds(const CCDSInfo& cds, SChainMember* copy_ofp);
    void InsertMemberCopyWithoutCds(SChainMember* copy_ofp);
    void InsertMember(SChainMember& m, SChainMember* copy_ofp = 0); 
    void DuplicateUTR(SChainMember* copy_ofp); 
private:
    CChainMembers(const CChainMembers& object) { }
    CChainMembers& operator=(const CChainMembers& object) { return *this; }    
    list<SChainMember> m_members;
    list<TContained> m_copylist;
    list<CAlignMap> m_align_maps;
    list<TContained> m_containedlist;
    list<CCDSInfo> m_extra_cds;
};

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
    big.m_contained_weight += small.m_align->Weight();
    big.m_contained_splice_weight += small.m_splice_weight;

    //all identical members are contained in each other; only one of them (with smaller m_mem_id) is contained in other members
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
            CGeneModel new_algn = algn;
            new_algn.ReverseComplementModel();
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
        if(mbr.m_cds_info->Score() == BadScore()) 
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
        set<int>& ospl = oriented_splices[algn.Strand()];
        ITERATE(CGeneModel::TExons, ie, algn.Exons()) {
            TSignedSeqRange exon = *ie;
            for(set<int>::iterator spli = ospl.lower_bound(exon.GetFrom()); spli != ospl.end() && *spli <= exon.GetTo(); ++spli)
                mbr.m_splice_weight += algn.Weight();
        }
    }
}


void CChainer::CChainerImpl::ScoreCdnas(CChainMembers& pointers)
{
    NON_CONST_ITERATE(CChainMembers, i, pointers) {
        SChainMember& mbr = **i;
        CGeneModel& algn = *mbr.m_align;

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
        const CCDSInfo& ai_cds_info = *mi.m_cds_info;
        TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();

        // knockdown spliced notconsensus UTRs in reads
        if(mi.m_type != eCDS && (ai.Status()&CGeneModel::eUnknownOrientation) && ai.Exons().size() > 1)
            mi.m_not_for_chaining = true; 

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

            if(mi.m_type != eCDS && mj.m_type  != mi.m_type)
                continue;    // avoid including UTR copy and avoid including CDS into UTR because that will change m_type

            TSignedSeqRange ai_max_cds = ai_cds_info.MaxCdsLimits()&ai.Limits();
            if(mi.m_type == eCDS && mj.m_type == eLeftUTR && aj.Limits().GetTo()-ai_max_cds.GetFrom()+1 >= min(6,aj.Limits().GetLength()))  // UTR in CDS
                continue;
            if(mi.m_type == eCDS && mj.m_type == eRightUTR && ai_max_cds.GetTo()-aj.Limits().GetFrom()+1 >= min(6,aj.Limits().GetLength()))  // UTR in CDS
                continue;

            const CCDSInfo& aj_cds_info = *mj.m_cds_info;
            TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();

            if(mi.m_type == eCDS && mj.m_type == eCDS) { // CDS in CDS
                TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();
                if (!Include(max_cds_limits, ExtendedMaxCdsLimits(ai, ai_cds_info) + ExtendedMaxCdsLimits(aj, aj_cds_info)))
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

            if(mi.m_not_for_chaining)
                continue;

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

#define NON_CDNA_INTRON_PENALTY 20

bool CChainer::CChainerImpl::LRCanChainItoJ(int& delta_cds, double& delta_num, double& delta_splice_num, const SChainMember& mi, const SChainMember& mj, TContained& contained) {

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
 
    switch(mi.m_type) {
    case eCDS: 
        if(mj.m_type == eRightUTR) 
            return false;
        else if(mj.m_type == eLeftUTR && (!ai_left_complete || (aj.Limits()&ai_rf).GetLength() > 5))
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
        else if(mj.m_type == eCDS && (!aj_right_complete || (ai.Limits()&aj_rf).GetLength() > 5))
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
        if ((ai.Limits() & aj.Limits()).GetLength() < intersect_limit) 
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

    TContained::const_iterator endsp = upper_bound(contained.begin(),contained.end(),&mj,LeftOrder()); // first alignmnet contained in ai and outside aj
    delta_num = 0;
    delta_splice_num = 0;
    for(TContained::const_iterator ic = endsp; ic != contained.end(); ++ic) {
        delta_num += (*ic)->m_align->Weight();
        delta_splice_num += (*ic)->m_splice_weight;
    }

    return true;
}


void CChainer::CChainerImpl::LRIinit(SChainMember& mi) {
    const CCDSInfo& ai_cds_info = *mi.m_cds_info;
    TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();

    mi.m_num = mi.m_contained_weight;
    mi.m_splice_num = mi.m_contained_splice_weight;

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
        TContained micontained = mi.CollectContainedForMemeber();
        sort(micontained.begin(),micontained.end(),LeftOrderD());
        
        TIVec::iterator lb = lower_bound(right_ends.begin(),right_ends.end(),ai.Limits().GetFrom());
        vector<SChainMember*>::iterator jfirst = pointers.begin();
        if(lb != right_ends.end())
            jfirst = pointers.begin()+(lb-right_ends.begin()); // skip all on the left side
        for(vector<SChainMember*>::iterator j = jfirst; j < i; ++j) {
            SChainMember& mj = **j;

            int delta_cds;
            double delta_num;
            double delta_splice_num;
            if(LRCanChainItoJ(delta_cds, delta_num, delta_splice_num, mi, mj, micontained)) {
                int newcds = mj.m_left_cds+delta_cds;
                double newnum = mj.m_left_num+delta_num;
                double newsplicenum = mj.m_left_splice_num+delta_splice_num;

                bool better_connection = false;
                if(newcds != mi.m_left_cds) {
                    better_connection = (newcds > mi.m_left_cds);
                } else if(fabs(newsplicenum - mi.m_left_splice_num) > 0.001) {
                    better_connection = (newsplicenum > mi.m_left_splice_num);
                } else if(newnum > mi.m_left_num) {
                    better_connection = true;
                }

                if(better_connection) {                
                    mi.m_left_cds = newcds;
                    mi.m_left_splice_num = newsplicenum;
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
        const CCDSInfo& ai_cds_info = *mi.m_cds_info;
        TSignedSeqRange ai_rf = ai_cds_info.Start()+ai_cds_info.ReadingFrame()+ai_cds_info.Stop();
        TSignedSeqRange ai_limits = ai.Limits();
        bool ai_right_complete = ai.Strand() == ePlus ? ai_cds_info.HasStop() : ai_cds_info.HasStart();

        mi.m_right_member = 0;
        mi.m_right_num = mi.m_num;
        mi.m_right_splice_num = mi.m_splice_num;
        mi.m_right_cds =  mi.m_cds;
        TContained micontained = mi.CollectContainedForMemeber();
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

            const CCDSInfo& aj_cds_info = *mj.m_cds_info;
            TSignedSeqRange aj_rf = aj_cds_info.Start()+aj_cds_info.ReadingFrame()+aj_cds_info.Stop();
            bool aj_left_complete = aj.Strand() == ePlus ? aj_cds_info.HasStart() : aj_cds_info.HasStop();
            
            switch(mi.m_type)
            {
                case eCDS: 
                    if(mj.m_type == eLeftUTR) 
                        continue;
                    if(mj.m_type == eRightUTR && (!ai_right_complete || (aj.Limits()&ai_rf).GetLength() > 5))
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
                    if(mj.m_type == eCDS && (!aj_left_complete || (ai.Limits()&aj_rf).GetLength() > 5))
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
            
            TContained::iterator endsp = upper_bound(micontained.begin(),micontained.end(),&mj,RightOrder()); // first alignment contained in ai and outside aj
            double delta_num = 0;
            double delta_splice_num = 0;
            for(TContained::iterator ic = endsp; ic != micontained.end(); ++ic) {
                delta_num += (*ic)->m_align->Weight();
                delta_splice_num += (*ic)->m_splice_weight;
            }
            double newnum = mj.m_right_num+delta_num;
            double newsplicenum = mj.m_right_splice_num+delta_splice_num;

            bool better_connection = false;
            if(newcds != mi.m_right_cds) {
                better_connection = (newcds > mi.m_right_cds);
            } else if(fabs(newsplicenum - mi.m_right_splice_num) > 0.001) {
                better_connection = (newsplicenum > mi.m_right_splice_num);
            } else if(newnum > mi.m_right_num) {
                better_connection = true;
            }

            if(better_connection) {
                mi.m_right_cds = newcds;
                mi.m_right_splice_num = newsplicenum;
                mi.m_right_num = newnum;
                mi.m_right_member = &mj;
                _ASSERT(aj.Limits().GetFrom() > ai.Limits().GetFrom() && aj.Limits().GetTo() > ai.Limits().GetTo());
            }    
        }
    }
}




#include <stdio.h>
#include <time.h>
/*
    time_t seconds0   = time (NULL);
    time_t seconds1   = time (NULL);
    cerr << "Time1: " << (seconds1-seconds0)/60. << endl;
*/


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
    string note = string(NStr::Int8ToString(mi.m_align->ID()));
    ITERATE(vector<const SChainMember*>, imal, mal) {
        note = note+" "+NStr::Int8ToString((*imal)->m_align->ID());
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

void MarkUnwantedLowSupportIntrons(vector<SChainMember*>& pointers, const SMinScor& minscor, 
                                   map<TSignedSeqRange,int>& mrna_count, map<TSignedSeqRange,int>& est_count, map<TSignedSeqRange,int>& rnaseq_count) {

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) 
        (*i)->m_marked_for_deletion = !GoodSupportForIntrons(*(*i)->m_align, minscor, mrna_count, est_count, rnaseq_count); 
}

TGeneModelList CChainer::CChainerImpl::MakeChains(TGeneModelList& clust)
{
    if(clust.empty()) return TGeneModelList();

    ITERATE (TGeneModelList, it, clust) {
        const CGeneModel& align = *it;
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

    NON_CONST_ITERATE (TGeneModelList, it, clust) {
        CGeneModel& align = *it;
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
        }
    }
    

    CChainMembers allpointers(clust, orig_aligns, unmodified_aligns);
    DuplicateNotOriented(allpointers, clust);
    ScoreCdnas(allpointers);
    Duplicate5pendsAndShortCDSes(allpointers, clust);
    DuplicateUTRs(allpointers);
    CalculateSpliceWeights(allpointers);
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
        mi.m_splice_num = mi.m_left_splice_num+mi.m_right_splice_num-mi.m_splice_num;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
    }
    sort(coding_pointers.begin(),coding_pointers.end(),CdsNumOrder());
    NON_CONST_ITERATE(vector<SChainMember*>, i, coding_pointers) {
        SChainMember& mi = **i;

        if(mi.m_included) continue;

        CChain chain(mi);
        TSignedSeqRange i_rf = chain.ReadingFrame();

        m_gnomon->GetScore(chain);
        mi.MarkIncludedForChain();

        if(chain.Score() == BadScore())
            continue;

        int cdslen = chain.FShiftedLen(chain.GetCdsInfo().Cds(),true);
        if(chain.GetCdsInfo().ProtReadingFrame().Empty() && 
           (cdslen < minscor.m_minlen || (chain.Score() < 2*minscor.m_min && cdslen <  2*minscor.m_cds_len)))
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

        mi.MarkUnwantedCopiesForChain(chain.RealCdsLimits());
    }
   
    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsMarkedForDeletion),pointers.end());  // wrong orientaition/UTR/frames are removed

    LeftRight(pointers);
    RightLeft(pointers);
    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        mi.m_included = false;
        mi.m_cds = mi.m_left_cds+mi.m_right_cds-mi.m_cds;
        mi.m_splice_num = mi.m_left_splice_num+mi.m_right_splice_num-mi.m_splice_num;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
    }

    sort(pointers.begin(),pointers.end(),CdsNumOrder());

    TChainList tmp_chains;

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        if(mi.m_included || mi.m_postponed) continue;

        CChain chain(mi);
        mi.MarkPostponedForChain();

        m_gnomon->GetScore(chain);

        chain.RemoveFshiftsFromUTRs();
        chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns);
        chain.ClipToCompleteAlignment(CGeneModel::eCap);   // alignments clipped below might not be in any chain; clipping may produce redundant chains
        chain.ClipToCompleteAlignment(CGeneModel::ePolyA);
        chain.ClipLowCoverageUTR(minscor.m_utr_clip_threshold, *m_gnomon);

        double ms = GoodCDNAScore(chain);

        bool has_trusted = chain.HasTrustedEvidence();

        if(!has_trusted)
            RemovePoorCds(chain,ms);
        if(chain.Score() != BadScore() && (has_trusted || chain.RealCdsLen() >= minscor.m_minlen)) {
            mi.MarkIncludedForChain();

#ifdef _DEBUG 
            chain.AddComment("Link1 "+GetLinkedIdsForMember(mi));
#endif    
            tmp_chains.push_back(chain);

            _ASSERT( chain.FShiftedLen(chain.GetCdsInfo().Start()+chain.ReadingFrame()+chain.GetCdsInfo().Stop(), false)%3==0 );
        }
    }

    CreateChainsForPartialProteins(tmp_chains, pointers);


    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsCoding),pointers.end());  // only noncoding left

    MarkUnwantedLowSupportIntrons(pointers, minscor, mrna_count, est_count, rnaseq_count);
    pointers.erase(std::remove_if(pointers.begin(),pointers.end(),MemberIsMarkedForDeletion),pointers.end());  // low support introns removed

    LeftRight(pointers);
    RightLeft(pointers);

    ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        mi.m_splice_num = mi.m_left_splice_num+mi.m_right_splice_num-mi.m_splice_num;
        mi.m_num = mi.m_left_num+mi.m_right_num-mi.m_num;
        _ASSERT(mi.m_cds == 0);
    }

    sort(pointers.begin(),pointers.end(),CdsNumOrder());

    NON_CONST_ITERATE(vector<SChainMember*>, i, pointers) {
        SChainMember& mi = **i;
        if(mi.m_included) continue;

        CChain chain(mi);
        chain.RemoveFshiftsFromUTRs();

        mi.MarkIncludedForChain();
        chain.ClipToCompleteAlignment(CGeneModel::eCap);
        chain.ClipToCompleteAlignment(CGeneModel::ePolyA);
        chain.ClipLowCoverageUTR(minscor.m_utr_clip_threshold, *m_gnomon);

#ifdef _DEBUG 
        chain.AddComment("Link2  "+GetLinkedIdsForMember(mi));
#endif    

        if(chain.Continuous() && chain.Exons().size() > 1)
            tmp_chains.push_back(chain);
    }

    NON_CONST_ITERATE(TChainList, it, tmp_chains) {
        CChain& chain = *it;
        chain.SetID(m_idnext);
        chain.SetGeneID(m_idnext);
        m_idnext += m_idinc;
    }

    CombineCompatibleChains(tmp_chains);
    SetFlagsForChains(tmp_chains);

    list<CGene> genes = FindGenes(tmp_chains);  // assigns geneid, rank, skip, nested

    if(genes.size() > 1) {
        TrimAlignmentsIncludedInDifferentGenes(genes);
        CombineCompatibleChains(tmp_chains);
        SetFlagsForChains(tmp_chains);
        FindGenes(tmp_chains);                      // redo genes after trim    
    }
    
    TGeneModelList chains;
    NON_CONST_ITERATE(TChainList, it, tmp_chains) {
        it->RestoreTrimmedEnds(trim);
        chains.push_back(*it);
    }

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

SChainMember* CChainer::CChainerImpl::FindOptimalChainForProtein(vector<SChainMember*>& pointers, vector<CGeneModel*>& parts, CGeneModel& palign) {
    //    Int8 id = parts.front()->ID();
            
    TIVec right_ends(pointers.size());
    vector<SChainMember> no_gap_members(pointers.size());   // temporary helper chain members; will be used for gap filling optimisation    
    for(int k = 0; k < (int)pointers.size(); ++k) {
        SChainMember& mi = *pointers[k];
        right_ends[k] = mi.m_align->Limits().GetTo();
        no_gap_members[k] = mi;
    }

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

    int fully_connected_right = 0;     // rightmost point already connected to all parts    

    for(int i = first_member; i <= last_member; ++i) {
        SChainMember& mi = *pointers[i];                   // best connection maybe gapped
        SChainMember& mi_no_gap = no_gap_members[i];       // best not gapped connection (if any)
        CGeneModel& ai = *mi.m_align;
        LRIinit(mi);
        LRIinit(mi_no_gap);

        if(ai.Strand() != palign.Strand())
            continue;

        int part_to_connect =  parts.size()-1;
        while(part_to_connect >= 0 && ai.Limits().GetFrom() <= parts[part_to_connect]->Limits().GetFrom())
            --part_to_connect;

        if(part_to_connect >=0 && ai.Limits().GetFrom() < parts[part_to_connect]->Limits().GetTo() && !parts[part_to_connect]->isCompatible(ai))  // overlaps with part but not compatible
            continue;

        if(fully_connected_right > 0 && ai.Limits().GetFrom() > fully_connected_right)    // can't possibly be connected
            continue;
               
        TContained micontained = mi.CollectContainedForMemeber();
        sort(micontained.begin(),micontained.end(),LeftOrderD());

        bool compatible_with_included_parts = true;
        int last_included_part = -1;
        bool includes_first_part = false;
        for(int p = part_to_connect+1; p < (int)parts.size(); ++p) {
            /*            
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
                
                if(found) {
                    TSignedSeqRange ai_rf = mi.m_cds_info->Cds();
                    _ASSERT(parts[p]->isCompatible(ai));
                    _ASSERT(Include(ai_rf,parts[p]->Limits()));
                    _ASSERT(mi.m_align_map->FShiftedLen(ai_rf.GetFrom(),parts[p]->Limits().GetFrom(),false)%3==0);
                }

                if(!found) {
                    compatible_with_included_parts = false;
                    break;
                }
            */
               
            if(Include(ai.Limits(),parts[p]->Limits())) {
                TSignedSeqRange ai_rf = mi.m_cds_info->ReadingFrame();
                TSignedSeqRange aj_rf = parts[p]->ReadingFrame();
                if(parts[p]->isCompatible(ai) && Include(ai_rf,aj_rf) && mi.m_align_map->FShiftedLen(ai_rf.GetFrom(),aj_rf.GetFrom(),false)%3==1 &&
                   parts[p]->GetCdsInfo().HasStop() ==  mi.m_cds_info->HasStop() && (!parts[p]->GetCdsInfo().HasStop() || parts[p]->GetCdsInfo().Stop() == mi.m_cds_info->Stop())) {
                    last_included_part = p;
                    if(p == 0)
                        includes_first_part = true;
                } else {
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

        _ASSERT(part_to_connect < 0 || part_to_connect == (int)parts.size()-1 || mi.m_type == eCDS);   // coding if between parts

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
               mi.m_cds_info->MaxCdsLimits().GetFrom() == TSignedSeqRange::GetWholeFrom()) {                                        // reading frame not interrupted 
                    
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
            } else if(ai.Limits().IntersectingWith(aj.Limits())) {
                int delta_cds;
                double delta_num;
                double delta_splice_num;
                if(LRCanChainItoJ(delta_cds, delta_num, delta_splice_num, mi, mj, micontained)) {      // i and j connected continuosly
                    int newcds = mj.m_left_cds+delta_cds;
                    double newnum = mj.m_left_num+delta_num;
                    double newsplicenum = mj.m_left_splice_num+delta_splice_num;

                    bool better_connection = false;
                    if(newcds != mi.m_left_cds) {
                        better_connection = (newcds > mi.m_left_cds);
                    } else if(fabs(newsplicenum - mi.m_left_splice_num) > 0.001) {
                        better_connection = (newsplicenum > mi.m_left_splice_num);
                    } else if(newnum > mi.m_left_num) {
                        better_connection = true;
                    }

                    if (mi.m_left_member == 0 || better_connection) {
                        mi.m_left_cds = newcds;
                        mi.m_left_splice_num = newsplicenum;
                        mi.m_left_num = newnum;
                        mi.m_gapped_connection = mj.m_gapped_connection;
                        mi.m_left_member = &mj;
                        mi.m_fully_connected_to_part = part_to_connect;
                        if(!mi.m_gapped_connection)
                            mi_no_gap = mi; 
                    } else if(mj_no_gap.m_fully_connected_to_part == part_to_connect) {
                        newcds = mj_no_gap.m_left_cds+delta_cds;
                        newnum = mj_no_gap.m_left_num+delta_num;
                        newsplicenum = mj_no_gap.m_left_splice_num+delta_splice_num;

                        better_connection = false;
                        if(newcds != mi_no_gap.m_left_cds) {
                            better_connection = (newcds > mi_no_gap.m_left_cds);
                        } else if(fabs(newsplicenum - mi_no_gap.m_left_splice_num) > 0.001) {
                            better_connection = (newsplicenum > mi_no_gap.m_left_splice_num);
                        } else if(newnum > mi_no_gap.m_left_num) {
                            better_connection = true;
                        }

                        if (mi_no_gap.m_left_member == 0 || better_connection) {
                            mi_no_gap.m_left_cds = newcds;
                            mi_no_gap.m_left_splice_num = newsplicenum;
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
            fully_connected_right = max(fully_connected_right,mi.m_align->Limits().GetTo());

            if(best_right == 0 || (mi.m_left_cds >  best_right->m_left_cds || (mi.m_left_cds ==  best_right->m_left_cds && mi.m_left_num >  best_right->m_left_num)) ) 
                best_right = &mi;
        }
    }

    _ASSERT(best_right != 0);

    _ASSERT(best_right < &no_gap_members.front() || best_right > &no_gap_members.back());   // don't point to temporary vector  
    for (SChainMember* mp = best_right; mp != 0; mp = mp->m_left_member) {
        if(mp->m_left_member >= &no_gap_members.front() && mp->m_left_member <= &no_gap_members.back()) { // points to temporary vector 
            SChainMember* p = pointers[mp->m_left_member-&no_gap_members.front()];
            *p = *mp->m_left_member;
            mp->m_left_member = p;
        }
    }

    return best_right;
}

void CChainer::CChainerImpl::CreateChainsForPartialProteins(TChainList& chains, vector<SChainMember*>& pointers_all) {

    sort(pointers_all.begin(),pointers_all.end(),LeftOrderD());

    typedef map<Int8, vector<CGeneModel*> > TIdChainMembermap;
    TIdChainMembermap protein_parts;
    for(int k = 0; k < (int)pointers_all.size(); ++k) {
        SChainMember& mi = *pointers_all[k];

        if((mi.m_align->Type() & CGeneModel::eProt) && (mi.m_copy == 0 || mi.m_cds_info->HasStart())) {  // only prots with start can have copies
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


        vector<SChainMember*> pointers;
        for(int k = 0; k < (int)pointers_all.size(); ++k) {
            SChainMember* mip = pointers_all[k];

            if((mip->m_type != eCDS || !Include(mip->m_cds_info->MaxCdsLimits(),mip->m_align->Limits())) && Include(palign.Limits(),mip->m_align->Limits())) // skip all not entirely coding inside protein alignment   
                continue;

            if(mip->m_align->Exons().front().m_ssplice_sig == "XX" && Include(palign.Limits(),mip->m_align->Exons().front().Limits())) // skip 3'/5' cdna gapfillers inside protein alignment
                continue;
        
            if(mip->m_align->Exons().back().m_fsplice_sig == "XX" && Include(palign.Limits(),mip->m_align->Exons().back().Limits())) // skip 3'/5' cdna gapfillers inside protein alignment     
                continue;

            pointers.push_back(mip);
        }

        SChainMember* best_right = FindOptimalChainForProtein(pointers, parts, palign);

        best_right->m_right_member = 0;
        CChain chain(*best_right,&palign);
       
        if(!chain.Continuous() && unmodified_aligns.find(id) != unmodified_aligns.end()) {  // failed to connect all parts - try unsupported introns if existed
            CGeneModel unma = unmodified_aligns[id];
            if(unma.Limits() != palign.Limits())
                unma.Clip(palign.Limits(),CGeneModel::eRemoveExons);

            vector<TSignedSeqRange> remaining_holes;
            for(int k = 1; k < (int)chain.Exons().size(); ++k) {
                CModelExon exonl = chain.Exons()[k-1];
                CModelExon exonr = chain.Exons()[k];
                if(!(exonl.m_ssplice && exonr.m_fsplice))
                    remaining_holes.push_back(TSignedSeqRange(exonl.GetTo()+1,exonr.GetFrom()-1));
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

                    if(connected || existed) 
                        unma.CutExons(hole);
                }
            }
            m_gnomon->GetScore(unma);

            if(unma.Exons() != palign.Exons() && unma.FrameShifts() == palign.FrameShifts()) {
                TGeneModelList unmacl;
                unmacl.push_back(unma);
                CutParts(unmacl);

                vector<CGeneModel*> unmaparts;
                NON_CONST_ITERATE(TGeneModelList, im, unmacl) {
                    m_gnomon->GetScore(*im);
                    unmaparts.push_back(&(*im));
                }

                CChainMembers unmapointers(unmacl, orig_aligns, unmodified_aligns);
                Duplicate5pendsAndShortCDSes(unmapointers, unmacl);
                FindContainedAlignments(unmapointers);
                
                ITERATE(vector<SChainMember*>, ip, unmapointers)
                    pointers.push_back(*ip);
                
                sort(pointers.begin(),pointers.end(),LeftOrderD());

                best_right = FindOptimalChainForProtein(pointers, unmaparts, unma);
                CChain tmp_chain(*best_right, &unma);

                // remove connected protein parts which are probably local and unstable  
                while(best_right != 0 && best_right->m_align->ID() == id) {
                    best_right = best_right->m_left_member;
                }
                for (SChainMember* mp = best_right; mp != 0; ) {
                    SChainMember* leftm = mp->m_left_member;
                    if(leftm != 0 && leftm->m_align->ID() == id)  { // remove from chain
                        mp->m_left_member = leftm->m_left_member;
                    } else {
                        mp = leftm;
                    }
                }
                
                // add protein trimmed parts with stable pointers 
                ITERATE(vector<SChainMember*>, ip, pointers) {
                    if((*ip)->m_align->ID() == id && !((*ip)->m_align->Status()&CGeneModel::eUnmodifiedAlign) ) {
                        const CCDSInfo& ai_cds_info = tmp_chain.GetCdsInfo();
                        TSignedSeqRange ai_rf = ai_cds_info.Cds();
                        const CCDSInfo& aj_cds_info = *(*ip)->m_cds_info;
                        TSignedSeqRange aj_rf = aj_cds_info.Cds();
                        TSignedSeqRange max_cds_limits = ai_cds_info.MaxCdsLimits() & aj_cds_info.MaxCdsLimits();
                        if(Include(max_cds_limits, ExtendedMaxCdsLimits(tmp_chain, ai_cds_info) + ExtendedMaxCdsLimits(*(*ip)->m_align, aj_cds_info)) && Include(ai_rf,aj_rf)) {
                            (*ip)->m_left_member = best_right;
                            best_right = *ip;
                        }
                    }
                }

                _ASSERT(best_right != 0 && best_right->m_align->ID() == id && !(best_right->m_align->Status()&CGeneModel::eUnmodifiedAlign));

                best_right->m_right_member = 0;
                CChain new_chain(*best_right, &unma);
                chain = new_chain;
            }
        }
        

        m_gnomon->GetScore(chain);
        chain.RemoveFshiftsFromUTRs();
        chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns);
        chain.ClipToCompleteAlignment(CGeneModel::eCap);
        chain.ClipToCompleteAlignment(CGeneModel::ePolyA);
        chain.ClipLowCoverageUTR(minscor.m_utr_clip_threshold, *m_gnomon);
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

    NON_CONST_ITERATE(TChainList, it, chains) {
        CChain& chain = *it;
        chain.RestoreReasonableConfirmedStart(*m_gnomon, orig_aligns);
        chain.SetOpenForPartialyAlignedProteins(prot_complet, orig_aligns);
        chain.SetConfirmedStartStopForCompleteProteins(prot_complet, orig_aligns, minscor);
        chain.CollectTrustedmRNAsProts(orig_aligns, minscor);
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
        for(TChainList::iterator jt = chains.begin(); jt != chains.end();) {
            TChainList::iterator jtt = jt++;
            if(jtt->Status()&CGeneModel::eSkipped)
                continue;

            if(itt != jtt && itt->Strand() == jtt->Strand() && jtt->IsSubAlignOf(*itt)) {
                if(itt->ReadingFrame().Empty()) {
                    if(jtt->ReadingFrame().NotEmpty())
                        continue;
                } else if(jtt->ReadingFrame().NotEmpty()) {
                    if(!Include(itt->ReadingFrame(),jtt->ReadingFrame()) ||
                       (itt->FShiftedLen(itt->GetCdsInfo().Cds().GetFrom(),jtt->GetCdsInfo().Cds().GetFrom(),false)-1)%3 != 0)
                        continue;
                }

                set<CGeneModel*> support;
                ITERATE(TContained, i, itt->m_members) {
                    support.insert((*i)->m_align);
                }
                ITERATE(TContained, i, jtt->m_members) {
                    if(support.insert((*i)->m_align).second)
                        itt->m_members.push_back(*i);
                }
                sort(itt->m_members.begin(),itt->m_members.end(),GenomeOrderD());
                itt->CalculateSupportAndWeightFromMembers();
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

#define SCAN_WINDOW 49            // odd number!!!

CChain::CChain(SChainMember& mbr, CGeneModel* gapped_helper) : m_coverage_drop_left(-1), m_coverage_drop_right(-1), m_coverage_bump_left(-1), m_coverage_bump_right(-1), m_core_coverage(0), m_splice_weight(0)
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
    ITERATE(TContained, i, m_members) {
        SChainMember* mi = *i;
        CGeneModel align = *mi->m_align;
        align.SetCdsInfo(*mi->m_cds_info);
        if(extened_parts.empty() || !align.Limits().IntersectingWith(extened_parts.back().Limits())) {
            extened_parts.push_back(align);
            extened_parts_and_gapped.push_back(&extened_parts.back());
        } else {
            extened_parts.back().Extend(align, false);
        }
    }

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

    CalculateSupportAndWeightFromMembers();

    m_polya_cap_left_hard_limit = Limits().GetTo()+1;
    m_polya_cap_right_hard_limit = Limits().GetFrom()-1;
    m_polya_cap_left_soft_limit = Limits().GetTo()+1;
    m_polya_cap_right_soft_limit = Limits().GetFrom()-1;

    CAlignMap amap = GetAlignMap();
    int mrna_len = amap.FShiftedLen(Limits());
    vector<double> coverage_raw(mrna_len+SCAN_WINDOW);
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        TSignedSeqRange overlap = Limits()&align.Limits();  // theoretically some ends could be outside (partially trimmed from other chain and combined)

        if(align.Type() == CGeneModel::eSR && overlap.NotEmpty()) {
            TSignedSeqRange on_mrna = amap.MapRangeOrigToEdited(overlap);        // for align partially in a hole will give the hole boundary
            for(int i = on_mrna.GetFrom(); i <= on_mrna.GetTo(); ++i)
                coverage_raw[i+SCAN_WINDOW/2] += align.Weight();
        }
    }
   
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
        if(!(*i)->m_align->TrustedProt().empty() || (!(*i)->m_align->TrustedmRNA().empty() && (*i)->m_cds_info->ProtReadingFrame().NotEmpty()))
            return true;
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
        if(it->second == (int)orig_aligns[it->first]->Exons().size()) {   // all exons are included in the chain
            Status() |= eBestPlacement;
            break;
        }
    }
}

struct SLinker
{
    SLinker() : m_member(0), m_value(0), m_left(0), m_not_wanted(false), m_count(0), m_not_wanted_count(0), m_connected(false) {}
    bool operator<(const SLinker& sl) const { return m_range.GetFrom() < sl.m_range.GetFrom(); }

    SChainMember* m_member;
    TSignedSeqRange m_range;
    TSignedSeqRange m_reading_frame;
    int m_value;
    SLinker* m_left;
    bool m_not_wanted;
    int m_count;
    int m_not_wanted_count;
    bool m_connected;
};

typedef vector<SLinker> TLinkers;

void CChain::CalculateSupportAndWeightFromMembers() {

    TLinkers linkers;
    ITERATE(TContained, i, m_members) {
        SChainMember* mi = *i;
        _ASSERT(mi->m_orig_align);

        SLinker sl;
        sl.m_member = mi;
        sl.m_value = 1;
        ITERATE(CGeneModel::TExons, e, Exons()) {
            sl.m_range += (mi->m_align->Limits()&e->Limits());
            sl.m_reading_frame += (mi->m_cds_info->ReadingFrame()&e->Limits()&ReadingFrame());
        }

        bool pstop_outside_rf = false;
        for(int is = 0; is < (int)GetCdsInfo().PStops().size() && !pstop_outside_rf; ++is) {
            const TSignedSeqRange& s = GetCdsInfo().PStops()[is];
            pstop_outside_rf = Include(sl.m_range,s) && !Include(sl.m_reading_frame,s);  // these alignments were not in the 'original' chain - they were added during deleting compatible chains
        }

        if(!pstop_outside_rf)
            linkers.push_back(sl);
    }

    set<TSignedSeqRange> chain_introns;
    for(int i = 1; i < (int)Exons().size(); ++i) {
        if(Exons()[i-1].m_ssplice && Exons()[i].m_fsplice)
            chain_introns.insert(TSignedSeqRange(Exons()[i-1].GetTo(),Exons()[i].GetFrom()));
    }
        
    Status() &= ~CGeneModel::eChangedByFilter;

    NON_CONST_ITERATE(TLinkers, l, linkers) {
        SLinker& sl = *l;
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
        } else if(align.Status()&CGeneModel::eChangedByFilter) {
            sl.m_not_wanted = true;
        } else {
            CAlignModel& orig_align = *mi->m_orig_align;
            bool all_introns_included = true;
            for(int i = 1; all_introns_included && i < (int)orig_align.Exons().size(); ++i) {
                if(orig_align.Exons()[i-1].m_ssplice && orig_align.Exons()[i].m_fsplice)
                    all_introns_included = chain_introns.count(TSignedSeqRange(orig_align.Exons()[i-1].GetTo(),orig_align.Exons()[i].GetFrom()));
            }
            if(!all_introns_included)   // intron was clipped by UTR clip or part is not in chain
                sl.m_not_wanted = true; 
        }
    }

    if(m_gapped_helper_align.ID()) {
        int left = m_gapped_helper_align.Limits().GetFrom();
        for(int i = 0; i < (int)m_gapped_helper_align.Exons().size(); ++i) {
            if(!m_gapped_helper_align.Exons()[i].m_ssplice) {
                SLinker sl;
                sl.m_range = TSignedSeqRange(left,m_gapped_helper_align.Exons()[i].GetTo())&m_range;
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

    sort(linkers.begin(), linkers.end());
    for(int i = 0; i < (int)linkers.size(); ++i) {
        SLinker& sli = linkers[i];
        if(sli.m_range.GetFrom() == Limits().GetFrom()) {
            sli.m_count = sli.m_value;
            if(sli.m_not_wanted)
                sli.m_not_wanted_count = sli.m_value;
            sli.m_connected = true;
        } else {
            for(int j = i-1; j >= 0; --j) {
                SLinker& slj = linkers[j];
                if(slj.m_connected && 
                   slj.m_range.GetFrom() < sli.m_range.GetFrom() && 
                   slj.m_range.GetTo() < sli.m_range.GetTo() && 
                   slj.m_range.GetTo() >= sli.m_range.GetFrom()-1) {   //overlaps and extends and connected to the left end

                    bool divided_pstop = false;
                    for(int is = 0; is < (int)GetCdsInfo().PStops().size() && !divided_pstop; ++is) {
                        const TSignedSeqRange& s = GetCdsInfo().PStops()[is];
                        divided_pstop = (Include(s,slj.m_range.GetTo()) || Include(s,sli.m_range.GetFrom())) && !Include(slj.m_reading_frame,s) && !Include(sli.m_reading_frame,s);
                    }
                    if(divided_pstop)  // both alignmnets just touch the pstop without actually crossing it
                        continue;

                    int new_count = slj.m_count + sli.m_value;
                    int new_not_wanted_count = slj.m_not_wanted_count;
                    if(sli.m_not_wanted)
                        new_not_wanted_count += sli.m_value;
                    if(!sli.m_connected || new_count < sli.m_count || (new_count == sli.m_count && new_not_wanted_count < sli.m_not_wanted_count)) {
                        sli.m_count = new_count;
                        sli.m_not_wanted_count = new_not_wanted_count;
                        sli.m_connected = true;
                        sli.m_left = &slj;
                    }
                } 
            }
        }
    }
    SLinker* best_right = 0;
    for(int i = 0; i < (int)linkers.size(); ++i) {
        SLinker& sli = linkers[i];
        if(sli.m_connected && sli.m_range.GetTo() == Limits().GetTo()) {
            if(best_right == 0 || sli.m_count < best_right->m_count || (sli.m_count == best_right->m_count && sli.m_not_wanted_count < best_right->m_not_wanted_count))
                best_right = &sli;
        }
    }
    _ASSERT(best_right != 0);

    set<Int8> sp_core;
    for(SLinker* l = best_right; l != 0; l = l->m_left) {
        if(l->m_member)
            sp_core.insert(l->m_member->m_align->ID());
    }
    if(m_gapped_helper_align.ID())
        sp_core.insert(m_gapped_helper_align.ID());

    set<Int8> sp_not_wanted;
    for(int i = 0; i < (int)linkers.size(); ++i) {
        SLinker& sli = linkers[i];
        if(sli.m_member && sli.m_not_wanted) {
            if(!sp_core.count(sli.m_member->m_align->ID()))
                sp_not_wanted.insert(sli.m_member->m_align->ID());
            else
                Status() |= CGeneModel::eChangedByFilter;
        }
    }    

    double weight = 0;
    m_splice_weight = 0;
    set<Int8> sp;
    TSignedSeqRange protreadingframe;
    ReplaceSupport(CSupportInfoSet());

    SetType(Type() & (~(eSR | eEST | emRNA | eProt)));
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        Int8 id = align.ID();
        if(!sp_not_wanted.count(id)) {
            SetType(Type() | (align.Type() & (eSR | eEST | emRNA | eProt)));
            protreadingframe += align.GetCdsInfo().ProtReadingFrame();       
            m_splice_weight += (*it)->m_splice_num;            
            if(sp.insert(id).second) {   // avoid counting parts of splitted aligns
                weight += align.Weight();
                AddSupport(CSupportInfo(id,sp_core.count(id)));
            }
        }
    }

    if(protreadingframe.NotEmpty()) {
        CCDSInfo cds = GetCdsInfo();
        TSignedSeqRange reading_frame = cds.ReadingFrame();
        cds.SetReadingFrame(reading_frame&protreadingframe, true);
        cds.SetReadingFrame(reading_frame);
        SetCdsInfo(cds);
    }

    SetWeight(weight);
}

void CChain::RestoreTrimmedEnds(int trim)
{
    // add back trimmed off UTRs    
    
    if((!OpenLeftEnd() || ReadingFrame().Empty()) && (Strand() == ePlus || (Status()&ePolyA) == 0) && (Strand() == eMinus || (Status()&eCap) == 0)) {
        for(int ia = 0; ia < (int)m_members.size(); ++ia)  {
            const CGeneModel a = *m_members[ia]->m_align;
            if((a.Type() & eProt)==0 && (a.Status() & CGeneModel::eLeftTrimmed)!=0 &&
               a.Exons().size() > 1 && Exons().front().Limits().GetFrom() == a.Limits().GetFrom()) {
                ExtendLeft( trim );
                break;
            }
        }
    }
     
    if((!OpenRightEnd() || ReadingFrame().Empty()) && (Strand() == eMinus || (Status()&ePolyA) == 0) && (Strand() == ePlus || (Status()&eCap) == 0)) {
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

void CChain::SetOpenForPartialyAlignedProteins(map<string, pair<bool,bool> >& prot_complet, TOrigAligns& orig_aligns) {
    if(ConfirmedStart() || !HasStart() || !HasStop() || OpenCds() || !Open5primeEnd() || (Type()&CGeneModel::eProt) == 0)
        return;

    bool found_length_match = false;
    ITERATE (TContained, it, m_members) {
        CAlignModel* orig_align = orig_aligns[(*it)->m_align->ID()];
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
    if(ReadingFrame().Empty() || ConfirmedStart())
        return;

    TSignedSeqRange conf_start;
    TSignedSeqPos rf=0; 
    
    ITERATE (TContained, it, m_members) {
        CAlignModel* orig_align = orig_aligns[(*it)->m_align->ID()];
        
        if(orig_align->ConfirmedStart() && Include((*it)->m_align->Limits(),orig_align->GetCdsInfo().Start())) {    // right part of orig is included
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

    if(conf_start.Empty()) {
        CAlignMap amap = GetAlignMap();
        ITERATE(TOrigAligns, it, orig_aligns) {
            const CAlignModel& align = *it->second;
            if(align.Strand() != Strand() || !align.ConfirmedStart() || (align.TrustedProt().empty() && align.TrustedmRNA().empty()))
                continue;
            
            TSignedSeqRange start = align.GetCdsInfo().Start();

            int a = amap.MapOrigToEdited(start.GetFrom());
            int b = amap.MapOrigToEdited(start.GetTo());
            if(a < 0 || b < 0 || abs(a-b) != 2)
                continue;
            
            if(!Include(GetCdsInfo().Cds(),start) || amap.FShiftedLen(GetCdsInfo().Cds().GetFrom(),start.GetFrom())%3 != 1) 
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
                    
                    if(found && amap.FShiftedLen(rf,GetCdsInfo().Cds().GetTo()) > 75)
                        conf_start = start;
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

                    if(found && amap.FShiftedLen(GetCdsInfo().Cds().GetFrom(),rf) > 75)
                        conf_start = start;
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
        if(FShiftedLen(extra_cds) < 0.2*RealCdsLen()) {
            TSignedSeqRange reading_frame = cds.ReadingFrame();
            TSignedSeqRange protreadingframe = cds.ProtReadingFrame();
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
            if(protreadingframe.NotEmpty()) 
                cds.SetReadingFrame(reading_frame&protreadingframe, true);
            cds.SetReadingFrame(reading_frame);
            cds.SetStart(conf_start,true);
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
                ClipChain(new_lim);   // remove holes from new UTRs            
          
            gnomon.GetScore(*this);
            RemoveFshiftsFromUTRs();
            AddComment("Restored confirmed start");
        }
    }
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
        int mrna_len = AlignLen();
        m_coverage.resize(mrna_len, m_coverage.back());   // this will slightly shift values compared to recalculation from scratch but will keep better ends
    }
}


void CChain::ClipChain(TSignedSeqRange limits) {

    _ASSERT(Include(Limits(),limits) && (RealCdsLimits().Empty() || Include(limits,RealCdsLimits())));

    TSignedSeqRange limits_on_mrna = GetAlignMap().MapRangeOrigToEdited(limits,false);
    _ASSERT(limits_on_mrna.NotEmpty());

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

    if(limits_on_mrna.GetFrom() > 0)
        m_coverage.erase(m_coverage.begin(),m_coverage.begin()+limits_on_mrna.GetFrom());
    m_coverage.resize(limits_on_mrna.GetLength());

    TContained new_members;
    ITERATE (TContained, it, m_members) {
        if(Limits().IntersectingWith((*it)->m_align->Limits())) {  // not clipped
            new_members.push_back(*it);
        }
    }
    m_members = new_members;

    if(RealCdsLimits().NotEmpty()) {
        CCDSInfo cds = GetCdsInfo();
        if((Strand() == ePlus && cds.MaxCdsLimits().GetFrom() < Limits().GetFrom()) ||
           (Strand() == eMinus && cds.MaxCdsLimits().GetTo() > Limits().GetTo())) {
            
            cds.Clear5PrimeCdsLimit();
            SetCdsInfo(cds);
        }
    }

    CalculateSupportAndWeightFromMembers();
}

bool BelongToExon(const CGeneModel::TExons& exons, int pos) {
    ITERATE(CGeneModel::TExons, i, exons) {
        if(Include(i->Limits(),pos))
            return true;
    }
    return false;
}

#define MINIMAL_TERMINALS 1

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
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
        if((align.Status()&determinant) != 0) {
            if(Strand() == right_end_strand) {
                int rlimit = (coding ? RealCdsLimits().GetTo() : Exons().back().Limits().GetFrom()); // look in the last exon of notcoding or right UTR of coding
                if(rlimit < align.Limits().GetTo() && BelongToExon(Exons(),align.Limits().GetTo())) {
                    position_weight.push_back(make_pair(align.Limits().GetTo(),align.Weight()));
                    weight += align.Weight();
                }
            } else {
                int llimit = (coding ? RealCdsLimits().GetFrom() : Exons().front().Limits().GetTo()); // look in the first exon of notcoding or left UTR of coding
                if(llimit > align.Limits().GetFrom() && BelongToExon(Exons(),align.Limits().GetFrom())) {
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

    TSignedSeqRange limits = Limits();
    Status() |= determinant;
    if (Strand() == right_end_strand && pos < Limits().GetTo())
        limits.SetTo(pos);
    else if (Strand() != right_end_strand && pos > Limits().GetFrom())
        limits.SetFrom(pos);

    if (limits != Limits()) {
        AddComment(name+"clip");
        ClipChain(limits);
    } 

    w = 0;
    for(int i = (int)position_weight.size()-1; i >= 0 && w < MINIMAL_TERMINALS; --i) {
        if(!Include(Limits(),position_weight[i].first))
            break;
        if(Strand() == right_end_strand) 
            m_polya_cap_right_hard_limit = position_weight[i].first;
        else
            m_polya_cap_left_hard_limit = position_weight[i].first;
        w += position_weight[i].second;
    }
    w = 0;
    for(int i = 0; i < (int)position_weight.size() && w < 0.5*weight; ++i) {   // 50% cutoff
        if(Strand() == right_end_strand) {
            if(Include(Exons().back().Limits(),position_weight[i].first) && position_weight[i].first <= pos) {
                m_polya_cap_right_soft_limit = position_weight[i].first;
                w += position_weight[i].second;
            }
        } else {
            if(Include(Exons().front().Limits(),position_weight[i].first) && position_weight[i].first >= pos) { 
                m_polya_cap_left_soft_limit = position_weight[i].first;
                w += position_weight[i].second;
            }
        }
    }
}

#define MIN_UTR_EXON 15
#define COVERAGE_DROP 0.1
#define COVERAGE_BUMP 3

void CChain::ClipLowCoverageUTR(double utr_clip_threshold, const CGnomonEngine& gnomon)
{
    if((Type()&CGeneModel::eSR) == 0)   // don't have SR coverage
        return;

    CAlignMap amap = GetAlignMap();

    int mrna_len = amap.FShiftedLen(Limits());
    
    TSignedSeqRange core_lim;
    if(ReadingFrame().NotEmpty()) {
        if(OpenCds())
            core_lim = MaxCdsLimits();
        else
            core_lim = RealCdsLimits();
    } else {
        core_lim = Limits();
        if(Exons().size() > 1) {
            if(Exons().front().Limits().GetLength() >= MIN_UTR_EXON)
                core_lim.SetFrom(Exons().front().Limits().GetTo()-MIN_UTR_EXON+1);
            if(Exons().back().Limits().GetLength() >= MIN_UTR_EXON)
                core_lim.SetTo(Exons().back().Limits().GetFrom()+MIN_UTR_EXON-1);
        }
    }

    TSignedSeqRange genome_hard_limit;
    genome_hard_limit.SetFrom(min(m_polya_cap_left_hard_limit,core_lim.GetFrom()));
    genome_hard_limit.SetTo(max(m_polya_cap_right_hard_limit,core_lim.GetTo()));

    core_lim = amap.MapRangeOrigToEdited(core_lim);
    TSignedSeqRange hard_limit = amap.MapRangeOrigToEdited(genome_hard_limit);

    vector<double> coverage = m_coverage;
    _ASSERT((int)coverage.size() == mrna_len && core_lim.GetFrom() >= 0 && core_lim.GetTo() < mrna_len);

    double core_coverage = 0;
    for (int i = core_lim.GetFrom(); i <= core_lim.GetTo(); ++i) {
        core_coverage += coverage[i];
    }
    core_coverage /= core_lim.GetLength();
    m_core_coverage = core_coverage;

    if(hard_limit.GetFrom() <= 0 &&  hard_limit.GetTo() >= mrna_len-1)   //nothing to clip
        return;

    if(hard_limit.GetTo()-hard_limit.GetFrom() < SCAN_WINDOW)      // too short
        return;

    map<int,double> intron_coverage;   // in transcript space
    vector<double> longseq_coverage(mrna_len);
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
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

    TSignedSeqRange old_limits = Limits();

    // 5' UTR
    if(hard_limit.GetFrom() > SCAN_WINDOW/2) {
        int left_limit = hard_limit.GetFrom(); // cap or cds/splice
        int right_limit = core_lim.GetTo();    // cds/splice
        int len = right_limit-left_limit+1;
        double wlen = 0;
        for(int i = left_limit; i <= right_limit; ++i)
            wlen += coverage[i];

        while(left_limit > 0 && (longseq_coverage[left_limit] > 0 || 
                                 (coverage[left_limit] > max(core_coverage,wlen/len)*utr_clip_threshold &&
                                  (intron_coverage.find(left_limit-1) == intron_coverage.end() || intron_coverage[left_limit-1] > core_inron_coverage*utr_clip_threshold)
                                  )
                                 )
              ) {

            ++len;
            --left_limit;
            wlen += coverage[left_limit];
        }

        if(left_limit > 0) {
            AddComment("5putrclip");
            ClipChain(amap.MapRangeEditedToOrig(TSignedSeqRange(left_limit,mrna_len-1)));
            if(Strand() == ePlus && Exons().front().Limits().GetLength() < MIN_UTR_EXON && Exons().front().Limits().GetTo() < genome_hard_limit.GetFrom())
                ClipChain(TSignedSeqRange(Exons()[1].Limits().GetFrom(),Limits().GetTo()));
            else if(Strand() == eMinus && Exons().back().Limits().GetLength() < MIN_UTR_EXON && Exons().back().Limits().GetFrom() > genome_hard_limit.GetTo())
                ClipChain(TSignedSeqRange(Limits().GetFrom(),Exons()[Exons().size()-2].GetTo()));
            if((Status()&CGeneModel::eCap) != 0)           // cap was further clipped
                ClipToCompleteAlignment(CGeneModel::eCap);
        }
    }
    

    // 3' UTR
    if(hard_limit.GetTo() < mrna_len-1-SCAN_WINDOW/2) {
        int right_limit = hard_limit.GetTo();     // polya or cds/splice
        int left_limit = core_lim.GetFrom();      // cds/splice
        int len = right_limit-left_limit+1;
        double wlen = 0;
        for(int i = left_limit; i <= right_limit; ++i)
            wlen += coverage[i];

        double window_wlen = 0;
        for(int i = right_limit-SCAN_WINDOW/2; i <= right_limit+SCAN_WINDOW/2; ++i)
            window_wlen += coverage[i];
            
        while(right_limit < mrna_len-1 && (longseq_coverage[right_limit] > 0 || 
                                           (coverage[right_limit] > wlen/len*utr_clip_threshold &&
                                            (intron_coverage.find(right_limit) == intron_coverage.end() || intron_coverage[right_limit] > core_inron_coverage*utr_clip_threshold)
                                            )
                                           )
              ) {

            ++len;
            ++right_limit;
            wlen += coverage[right_limit];
        }

        if(right_limit < mrna_len-1) {
            AddComment("3putrclip");
            int new_5p = amap.MapRangeOrigToEdited(Limits()).GetFrom();
            ClipChain(amap.MapRangeEditedToOrig(TSignedSeqRange(new_5p,right_limit)));
            if(Strand() == ePlus && Exons().back().Limits().GetLength() < MIN_UTR_EXON && Exons().back().Limits().GetFrom() > genome_hard_limit.GetTo())
                ClipChain(TSignedSeqRange(Limits().GetFrom(),Exons()[Exons().size()-2].GetTo()));
            else if(Strand() == eMinus && Exons().front().Limits().GetLength() < MIN_UTR_EXON && Exons().front().Limits().GetTo() < genome_hard_limit.GetFrom())
                ClipChain(TSignedSeqRange(Exons()[1].Limits().GetFrom(),Limits().GetTo()));
            if((Status()&CGeneModel::ePolyA) != 0)           // polya was further clipped
                ClipToCompleteAlignment(CGeneModel::ePolyA);
        }
    }

    if(old_limits != Limits() && ReadingFrame().NotEmpty()) {
        bool wasopen = OpenCds();
        gnomon.GetScore(*this);
        CCDSInfo cds = GetCdsInfo();
        if(wasopen != OpenCds() && (wasopen == false || cds.HasStart())) {
            cds.SetScore(cds.Score(),wasopen);
            SetCdsInfo(cds);
        }
        RemoveFshiftsFromUTRs();
    }

    CalculateDropLimits();
}

void CChain::CalculateDropLimits() {

    m_coverage_drop_left = -1;
    m_coverage_drop_right = -1;
    m_coverage_bump_left = -1;
    m_coverage_bump_right = -1;

    CAlignMap amap = GetAlignMap();

    int mrna_len = amap.FShiftedLen(Limits());

    vector<double> longseq_coverage(mrna_len);
    ITERATE (TContained, it, m_members) {
        const CGeneModel& align = *(*it)->m_align;
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

    // 3' UTR
    int right_limit = soft_limit.GetTo();
    first_bump = -1;
    max_cov = 0;
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
            CAlignModel* orig_align = orig_aligns[(*i)->m_align->ID()];        
            if(orig_align->TargetLen() == 0)   // protein of not known length
                continue;

            TSignedSeqRange fivep_exon = orig_align->Exons().front().Limits();
            TSignedSeqRange threep_exon = orig_align->Exons().back().Limits();
            if((*i)->m_align->Strand() == eMinus)
                swap(fivep_exon,threep_exon);

            if(!ConfirmedStart() && HasStart() && fivep_exon.IntersectingWith((*i)->m_align->Limits()) && 
               !LeftPartialProtein(prot_complet, *orig_align, scope) && Include(Limits(),(*i)->m_align->Limits())) {  // protein has start

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
               !RightPartialProtein(prot_complet, *orig_align, scope) && Include(Limits(),(*i)->m_align->Limits())) {  // protein has stop  

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

    if(setconfstart) {
        CCDSInfo cds_info = GetCdsInfo();
        double score = cds_info.Score();
        score += max(1.,0.3*score);
        cds_info.SetScore(score, false);   // not open           
        cds_info.SetStart(cds_info.Start(), true);    // confirmed start            
        SetCdsInfo(cds_info);
    }
    if(setconfstop) {
        CCDSInfo cds_info = GetCdsInfo();
        cds_info.SetStop(cds_info.Stop(), true);    // confirmed stop           
        SetCdsInfo(cds_info);
    }
}

void CChain::CollectTrustedmRNAsProts(TOrigAligns& orig_aligns, const SMinScor& minscor)
{
    ClearTrustedmRNA();
    ClearTrustedProt();

    if(ConfirmedStart() && ConfirmedStop()) {
        typedef map<Int8, int> Tint8int;
        Tint8int palignedlen;
        ITERATE(TContained, i, m_members) {
            if(IntersectingWith(*(*i)->m_align)) {                                                                   // just in case we clipped this alignment
                if(!(*i)->m_align->TrustedProt().empty())
                    palignedlen[(*i)->m_align->ID()] += (*i)->m_align->AlignLen();
                else if(!(*i)->m_align->TrustedmRNA().empty() && (*i)->m_align->ConfirmedStart() && (*i)->m_align->ConfirmedStop())  // trusted mRNA with aligned CDS (correctly checks not duplicated cds)
                    InsertTrustedmRNA(*(*i)->m_align->TrustedmRNA().begin());                                      // could be only one 'part'
            }
        }
        ITERATE(Tint8int, i, palignedlen) {
            CAlignModel* orig_align = orig_aligns[i->first];
            if((Continuous() && i->second > 0.8*orig_align->TargetLen()) || i->second > minscor.m_minprotfrac*orig_align->TargetLen())                                 // well aligned trusted protein
            //            if(i->second > minscor.m_minprotfrac*orig_align->TargetLen())                                 // well aligned trusted protein
                InsertTrustedProt(*orig_align->TrustedProt().begin());
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
                }
            }
           
            double ms = GoodCDNAScore(algn);

            if (algn.Score() == BadScore() || (algn.Score() < ms && (algn.Type()&CGeneModel::eProt) && !(algn.Status()&CGeneModel::eBestPlacement) && orig->AlignLen() < minscor.m_minprotfrac*orig->TargetLen())) { // all mRNA with confirmed CDS and best placed or reasonably aligned proteins with known length will get through with any finite score 
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
            for(int k = 0; k < (int)algn.Exons().size() && !found; ++k) {
                int a = algn.Exons()[k].GetFrom();
                int b = algn.Exons()[k].GetTo();
                if (fs->IntersectingWith(a,b)) {
                    found = true;
                    if ((algn.Exons()[k].m_fsplice && fs->Loc()<a) ||
                        (algn.Exons()[k].m_ssplice && fs->IsInsertion() && b<fs->Loc()+fs->Len()-1)) {
                        incompatible_indels = true;
                    }
                }
                // AlignMap can't handle ggap and frameshift at rthe same point
                if(k > 0 && algn.Exons()[k-1].m_ssplice_sig == "XX" && fs->IntersectingWith(a,a))
                    incompatible_indels = true;
                if(k < (int)algn.Exons().size()-1 && algn.Exons()[k+1].m_fsplice_sig == "XX" && fs->IntersectingWith(b,b))
                    incompatible_indels = true;
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

        if(aa.Status()&CGeneModel::eUnmodifiedAlign) {
            unmodified_aligns[aa.ID()] = aa;
        } else {        
            models.push_back(aa);
            orig_aligns[aa.ID()]=&(*i);
        }
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
                            CArgDescriptions::eInteger, "100");
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
                            "Chains with thorter CDS should be supported by protein or satisfy noncoding intron reguirements",
                            CArgDescriptions::eInteger, "100");
    arg_desc->AddDefaultKey("altfrac","altfrac","The CDS length of the principal model in the gene is multiplied by this fraction. Alt variants with the CDS length above "
                            "this are included in gene",CArgDescriptions::eDouble,"80.0");
    arg_desc->AddDefaultKey("composite","composite","Maximal composite number in alts",CArgDescriptions::eInteger,"1");
    arg_desc->AddFlag("opposite","Allow overlap of complete multiexon genes with opposite strands");
    arg_desc->AddFlag("partialalts","Allows partial alternative variants. In combination with -nognomon will allow partial genes");
    arg_desc->AddDefaultKey("tolerance","tolerance","if models exon boundary differ only this much only one model will survive",CArgDescriptions::eInteger,"5");

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
    minscor.m_minsupport = args["minsupport"].AsInteger();
    minscor.m_minsupport_mrna = args["minsupport_mrna"].AsInteger();
    minscor.m_minsupport_rnaseq = args["minsupport_rnaseq"].AsInteger();
    minscor.m_minlen = args["minlen"].AsInteger();

    chainer->SetMinInframeFrac(args["mininframefrac"].AsDouble());

    chainer->m_data->altfrac = args["altfrac"].AsDouble();
    chainer->m_data->composite = args["composite"].AsInteger();
    chainer->m_data->allow_opposite_strand = args["opposite"];
    chainer->m_data->allow_partialalts = args["partialalts"];
    chainer->m_data->tolerance = args["tolerance"].AsInteger();

    
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

void CGnomonAnnotator_Base::MapModelsToOrigContig(TGeneModelList& models) const {
    if(m_edited_contig_map) {
        for(TGeneModelList::iterator im_loop = models.begin(); im_loop != models.end(); ) {
            TGeneModelList::iterator im = im_loop++;

            CGeneModel model = *im;
            model.SetCdsInfo(CCDSInfo());
            model.CutExons(model.Limits());  // empty model with all atributes

            bool bad_model = false;

            for(int ie = 0; ie < (int)im->Exons().size(); ++ie) {
                const CModelExon& e = im->Exons()[ie];

                string seq;
                CInDelInfo::SSource src;
                TGgapInfo::const_iterator i = m_inserted_seqs.find(e.GetFrom());
                if(i != m_inserted_seqs.end()) {
                    seq = i->second->GetInDelV().substr(0,e.Limits().GetLength());
                    src = i->second->GetSource();
                    if(src.m_strand == ePlus)
                        src.m_range.SetTo(src.m_range.GetFrom()+e.Limits().GetLength()-1);
                    else
                        src.m_range.SetFrom(src.m_range.GetTo()-e.Limits().GetLength()+1);
                } else {
                    i = m_inserted_seqs.upper_bound(e.GetTo());
                    if(i != m_inserted_seqs.begin() && (--i)->first + (int)i->second->GetInDelV().length() - 1 == e.GetTo()) {
                        string s = i->second->GetInDelV();
                        seq = s.substr(s.length()-e.Limits().GetLength());
                        src = i->second->GetSource();
                        if(src.m_strand == eMinus)
                            src.m_range.SetTo(src.m_range.GetFrom()+e.Limits().GetLength()-1);
                        else
                            src.m_range.SetFrom(src.m_range.GetTo()-e.Limits().GetLength()+1);
                    }
                }

                if(seq.empty()) {  // normal exon
                    TSignedSeqRange exon = m_edited_contig_map->MapRangeEditedToOrig(e.Limits(),false);
                    if(exon.Empty()) { // all real alignment and some filling was clipped
                        _ASSERT(im->Exons().size() == 1);
                        bad_model = true;
                        break;
                    }
                    //                    model.AddExon(exon, e.m_fsplice_sig, e.m_ssplice_sig);
                    model.AddNormalExon(exon, e.m_fsplice_sig, e.m_ssplice_sig, 0, false);
                } else {
                    if((int)im->Exons().size() == 1){ // all real alignment was clipped
                        bad_model = true;
                        break;
                    }

                    if(model.Strand() == eMinus) {
                        ReverseComplement(seq.begin(), seq.end());
                        src.m_strand = (src.m_strand == ePlus ? eMinus : ePlus);
                    }
                    _ASSERT((int)seq.length() == src.m_range.GetLength());
                    //                    model.AddExon(TSignedSeqRange::GetEmpty(), "XX", "XX", e.m_ident, seq, src);
                    model.AddGgapExon(0, seq, src, false);
                }

                if(ie < (int)im->Exons().size()-1 && (!e.m_ssplice || !im->Exons()[ie+1].m_fsplice)) // hole
                    model.AddHole();
            }

            if(bad_model) {
                models.erase(im);
                continue;
            }

            TInDels editedframeshifts = im->FrameShifts();
            NON_CONST_ITERATE(TInDels, i, editedframeshifts) {
                int newloc = m_edited_contig_map->MapEditedToOrig(i->Loc());
                _ASSERT(newloc >= 0);
                CInDelInfo indel(newloc, i->Len(), i->IsInsertion(), i->GetInDelV());
                *i = indel;
            }
            model.FrameShifts() = editedframeshifts;

            model.SetCdsInfo(im->GetCdsInfo().MapFromOrigToEdited(im->GetAlignMap()));

            *im = model;
        }
    }
}

CAlignModel CGnomonAnnotator_Base::MapOneModelToEditedContig(const CGeneModel& align) const 
{
    CGeneModel editedmodel = align;
    editedmodel.ClearExons();  // empty alignment with all atributes

    vector<TSignedSeqRange> transcript_exons;
 
    for(int i = 0; i < (int)align.Exons().size(); ++i) {
        transcript_exons.push_back(align.TranscriptExon(i));
        const CModelExon& e = align.Exons()[i];
                
        if(e.Limits().NotEmpty()) {   // real exon
            editedmodel.AddExon(m_edited_contig_map->MapRangeOrigToEdited(e.Limits(),false),e.m_fsplice_sig, e.m_ssplice_sig, e.m_ident);
            if(i < (int)align.Exons().size()-1 && (!align.Exons()[i].m_ssplice || !align.Exons()[i+1].m_fsplice))  // hole
                editedmodel.AddHole();
        } else {                     // gap exon
            string gap_seq = e.m_seq;
            if(align.Orientation() == eMinus)
                ReverseComplement(gap_seq.begin(), gap_seq.end());

            TInDels::const_iterator gap = m_editing_indels.end();
            ITERATE(TInDels, ig, m_editing_indels) {
                if(ig->IsInsertion())
                    continue;
                if(i > 0 && ig->Loc() < align.Exons()[i-1].GetTo())
                    continue;
                if(i == 0 && ig->Loc() > align.Exons()[i+1].GetFrom())
                    break;
                if(ig->GetInDelV() == gap_seq) {
                    gap = ig;
                    if(i > 0) break;  //first available  for all exons except the first one
                }
            }
            _ASSERT(gap != m_editing_indels.end());

            int left_end = m_edited_contig_map->MapOrigToEdited(gap->Loc());
            if(left_end >= 0) {
                left_end -= gap->Len();
                for(TInDels::const_iterator ig = gap+1; ig != m_editing_indels.end() && ig->Loc() == gap->Loc(); ++ig)
                    left_end -= ig->Len();
            } else {
                left_end = m_edited_contig_map->MapOrigToEdited(gap->Loc()-1);
                _ASSERT(left_end >= 0);
                left_end += 1;
                for(TInDels::const_iterator ig = gap; ig != m_editing_indels.begin() && (ig-1)->Loc() == gap->Loc(); --ig) {
                    left_end += (ig-1)->Len();
                }                        
            }
                    
            editedmodel.AddExon(TSignedSeqRange(left_end,left_end+gap->Len()-1), "XX", "XX", 1);
        }
    }
                    
    TInDels editedindels = align.GetAlignMap().GetInDels(false);
    NON_CONST_ITERATE(TInDels, i, editedindels) {
        int newloc = m_edited_contig_map->MapOrigToEdited(i->Loc());
        _ASSERT(newloc >= 0);
        CInDelInfo indel(newloc, i->Len(), i->IsInsertion(), i->GetInDelV());
        *i = indel;
    }

    CAlignMap editedamap(editedmodel.Exons(), transcript_exons, editedindels, align.Orientation(), align.GetAlignMap().TargetLen());

    CAlignModel editedalign(editedmodel, editedamap);

    _ASSERT(align.GetEdgeReadingFrames()->empty());

    if(align.ReadingFrame().NotEmpty()) {
        CCDSInfo cds = align.GetCdsInfo();
        if(cds.IsMappedToGenome())
            cds = cds.MapFromOrigToEdited(align.GetAlignMap());
        editedalign.SetCdsInfo(cds.MapFromEditedToOrig(editedalign.GetAlignMap()));
    }

    return editedalign;
}

void CGnomonAnnotator_Base::MapAlignmentsToEditedContig(TAlignModelList& alignments) const
{
    if(m_edited_contig_map) {
        NON_CONST_ITERATE(TAlignModelList, ia, alignments) {
            CAlignModel a = MapOneModelToEditedContig(*ia);
            a.SetTargetId(*ia->GetTargetId());
            *ia = a;
        }
    }
}

void CGnomonAnnotator_Base::MapModelsToEditedContig(TGeneModelList& models) const
{
    if(m_edited_contig_map) {
        NON_CONST_ITERATE(TGeneModelList, ia, models) {
            *ia = MapOneModelToEditedContig(*ia);
        }
    }
}

TInDels CGnomonAnnotator_Base::GetGenomicGaps(const TGeneModelList& models){
    TInDels ggaps;

    if(models.empty())
        return ggaps;

    int left = numeric_limits<int>::max();
    int right = 0;
    ITERATE(TGeneModelList, i, models) {
        left = min(left,i->Limits().GetFrom());
        right = max(right,i->Limits().GetTo());
    }

    int len = right-left+1;
    TIVec exons(len,0);
    ITERATE(TGeneModelList, i, models) {
        ITERATE(CGeneModel::TExons, e, i->Exons()) {
            if(e->Limits().NotEmpty()) {
                int a = e->GetFrom();
                if(e->m_fsplice)
                    a = max(left,a-2);
                int b = e->GetTo();
                if(e->m_ssplice)
                    b = min(right,b+2);
                for(int p = a; p <= b; ++p) {  // block all exons and their splices 
                    _ASSERT(p-left >= 0 && p-left < len);
                    exons[p-left] = 1;
                }
            }
        }
    }
    
    ITERATE(TGeneModelList, i, models) {
        for(int ie = 0; ie < (int)i->Exons().size(); ++ie) {
            const CModelExon& e = i->Exons()[ie];
            if(e.Limits().Empty()) {
                int pos;
                if(ie > 0) {
                    _ASSERT(i->Exons()[ie-1].Limits().NotEmpty());
                    for(pos = i->Exons()[ie-1].GetTo()+1; pos-left < len && exons[pos-left] > 0; ++pos);
                } else {
                    _ASSERT((int)i->Exons().size() > 1 && i->Exons()[1].Limits().NotEmpty());
                    for(pos = i->Exons()[1].GetFrom()-1; pos-left > 0 && exons[pos-left-1] > 0; --pos);
                }
                string seq = e.m_seq;
                CInDelInfo::SSource source = e.m_source;
                if(i->Strand() == eMinus) {
                    ReverseComplement(seq.begin(),seq.end());
                    source.m_strand = OtherStrand(source.m_strand);
                }

                //could push out of contig - will need correction in SetGenomic
                if(pos == left)
                    pos = left-2;  
                if(pos == right+1)
                    pos = right+3;

                ggaps.push_back(CInDelInfo(pos, seq.length(), false, seq, source));
            }
        }    
    }

    sort(ggaps.begin(),ggaps.end());
    return ggaps;
}

void CGnomonAnnotator_Base::SetGenomic(const CResidueVec& seq)
{
    delete m_edited_contig_map;
    m_edited_contig_map = 0;
    m_editing_indels.clear();
    m_inserted_seqs.clear();
    m_gnomon.reset(new CGnomonEngine(m_hmm_params, seq, TSignedSeqRange::GetWhole()));
}

void CGnomonAnnotator_Base::SetGenomic(const CSeq_id& contig, CScope& scope, const string& mask_annots, const TInDels* contig_fix_indels)
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

    delete m_edited_contig_map;
    m_edited_contig_map = 0;
    m_editing_indels.clear();
    m_inserted_seqs.clear();

    if(contig_fix_indels && !contig_fix_indels->empty()) {

        cerr << "Genomic gaps for contig " << CIdHandler::ToString(contig) << endl;
        ITERATE(TInDels, ig, *contig_fix_indels)
            cerr << "Gaploc " << ig->Loc() << ' ' << seq.size() << ' ' << ig->GetInDelV() << endl;

#define BLOCK_OF_Ns 35
        //surround ggapw with Ns to satisfy MinIntron
        ITERATE(TInDels, ig, *contig_fix_indels) {
            int l = ig->Loc();
            l = max(0,l);
            l = min((int)seq_txt.size(),l);
            CInDelInfo g(l, ig->Len(), ig->IsInsertion(), ig->GetInDelV(), ig->GetSource());
            CInDelInfo Ns(l, BLOCK_OF_Ns, false, string(BLOCK_OF_Ns,'N'));
            m_editing_indels.push_back(Ns);
            m_editing_indels.push_back(g);
            m_editing_indels.push_back(Ns);
        }
                
        m_edited_contig_map = new CAlignMap(0, seq_txt.size()-1, m_editing_indels.begin(), m_editing_indels.end());
        CResidueVec editedseq;
        m_edited_contig_map->EditedSequence(seq,editedseq);
        seq = editedseq;

        ITERATE(TInDels, ig, m_editing_indels) {
            if(ig->IsDeletion()) {
                int left_end = m_edited_contig_map->MapOrigToEdited(ig->Loc());
                if(left_end >= 0) {
                    left_end -= ig->Len();
                    for(TInDels::const_iterator igg = ig+1; igg != m_editing_indels.end() && igg->Loc() == ig->Loc(); ++igg)
                        left_end -= igg->Len();
                } else {
                    left_end = m_edited_contig_map->MapOrigToEdited(ig->Loc()-1);
                    _ASSERT(left_end >= 0);
                    left_end += 1;
                    for(TInDels::const_iterator i = ig; i != m_editing_indels.begin() && (i-1)->Loc() == ig->Loc(); --i) {
                        left_end += (i-1)->Len();
                    }                        
                }

                m_inserted_seqs[left_end] = ig;
            }
        }

    }

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


