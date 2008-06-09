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
 * Authors:  Alex Astashyn
 *
 * File Description:
 *
 */

#ifndef COMPARE_FEATS_HPP_
#define COMPARE_FEATS_HPP_

#include <vector>
#include <map>
#include <set>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/feature.hpp> 

#include "loc_mapper.hpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// CCompareSeq_locs is used for comparing locations of two features on the same coordinate system
/// It is agnostic to what type of feature it is and only compares the internal structure of the locs.
class CCompareSeq_locs : public CObject
{
public:
    typedef int TCompareLocsFlags;
    enum FCompareLocs {
        fCmp_Unknown                = 1 << 0,  ///< failed to compare
        fCmp_Incomplete             = 1 << 1,  ///< Some parts of the location could not be compared (e.g. on different sequence)
        fCmp_NoOverlap              = 1 << 2,  ///< seq_locs do not overlap at all
        fCmp_RegionOverlap          = 1 << 3,  ///< overlap of the extremes
        fCmp_Overlap                = 1 << 4,  ///< at least one interval overlaps
        fCmp_Subset                 = 1 << 5,  ///< comparison loc is a subset of the reference loc; some interval boundaries do not match
        fCmp_Superset               = 1 << 6,  ///< comparison loc is a superset of the reference loc; some interval boundaries do not match
        fCmp_intsMissing_internal   = 1 << 7,  ///< comparison loc is missing interval(s) internally
        fCmp_intsExtra_internal     = 1 << 8,  ///< comparinos loc has extra interval(s) internally
        fCmp_intsMissing_3p         = 1 << 9,  ///< comparison loc is missing interval(s) at 3' end
        fCmp_intsExtra_3p           = 1 << 10, ///< comparinos loc has extra interval(s) at 3' end
        fCmp_intsMissing_5p         = 1 << 11, ///< comparison loc is missing interval(s) at 5' end
        fCmp_intsExtra_5p           = 1 << 12, ///< comparinos loc has extra interval(s) at 5' end
        fCmp_3pExtension            = 1 << 13, ///< 3' terminal interval extended (other splice junction matches)
        fCmp_3pTruncation           = 1 << 14, ///< 3' terminal interval truncated (other splice junction matches)
        fCmp_5pExtension            = 1 << 15, ///< 5' terminal interval extended (other splice junction matches)
        fCmp_5pTruncation           = 1 << 16, ///< 5' terminal interval truncated (other splice junction matches)
        fCmp_StrandDifferent        = 1 << 17, ///< different strand
        fCmp_FuzzDifferent          = 1 << 18, ///< indicates fuzz mismatch if set
        fCmp_Match                  = 1 << 19  ///< all junctions match (fuzz-agnostic)
    };
    
    //This struct keeps the result of comparison of two exons
    struct SIntervalComparisonResult : CObject
    {
    public:
        SIntervalComparisonResult(unsigned pos1, unsigned pos2, FCompareLocs result, int pos_comparison = 0) 
            : m_exon_ordinal1(pos1), m_exon_ordinal2(pos2), m_result(result), m_position_comparison(pos_comparison) {}
        SIntervalComparisonResult() { SIntervalComparisonResult(0, 0, fCmp_Unknown, 0); } 


        inline bool missing_first() const {return m_exon_ordinal1 == 0;}
        inline bool missing_second() const {return m_exon_ordinal2 == 0;}
        
        unsigned m_exon_ordinal1;
        unsigned m_exon_ordinal2;
        FCompareLocs m_result;  
        int m_position_comparison; //we need to know which exon is "ahead" so the overlaps can be correctly reported,
                                   //e.g. 1:>1>  vs. 1:<1<
    };
    
    CCompareSeq_locs(const CSeq_loc& loc1, const CSeq_loc& loc2, CScope* scope2)
        : m_loc1(&loc1)
        , m_loc2(&loc2)
        , m_scope_t(scope2)
    {
        this->Reset();            
    } 

    /// Reset cached comparison results
    void Reset()
    {
        this->m_cachedOverlapValues = false;
        this->x_Compare();
    }
    
    /// Symmetrical overlap is defined as length(intersection(loc1, loc2) / (length(loc1) + length(loc2))
    /// intra-loc overlaps are merged (otherwise non-sensical results are possible)
    double GetSymmetricalOverlap() const
    {
        if(!m_cachedOverlapValues) {
            x_ComputeOverlapValues();
        }

        TSeqPos denom = m_len_seqloc1 + m_len_seqloc2 - m_len_seqloc_overlap;
        
        return (denom == 0) ? 0.0 : (static_cast<double>(m_len_seqloc_overlap) / denom);  
    }
    
    /// Relative overlap is defined as ratio of the length of the overlap to the
    /// length of the first feature
    double GetRelativeOverlap() const
    {
        if(!m_cachedOverlapValues) {
            x_ComputeOverlapValues();
        }

        TSeqPos denom = static_cast<TSeqPos>(std::min(m_len_seqloc1, m_len_seqloc2));
        
        return (denom == 0) ? 0.0 : (static_cast<double>(m_len_seqloc_overlap) / denom);  
    }
    
    
    /// str_out will contain human-readable summary of the internal comparison
    TCompareLocsFlags GetResult(string* str_out = NULL) const;
    
    /// The evidence string is a whitespace-separated list of exon comparisons
    /// Each exon comparison is a pair of exon ordinals (on query and target features)
    /// separated by a colon; target exon ordinal may contain splice junction "operators"
    /// that establish relationship to the query exon.
    ///
    /// '>' and '<' denote the splice junction shifts in 3' and 5' direction respectively
    /// (relative to the master sequence). E.g.
    /// '4:4'   = 4th exon matches exactly on both sequneces
    /// '4:>4>' = 4th exon shifted in 3' direction relative to the overlapping query exon
    /// '4:<4>' = 4th exon extended in both directions relative to the overlapping query exon
    /// '4:<4'  = 4th exon has 5' junction extended relative to the overlapping query exon
    /// etc.
    ///
    /// '~' are sentinels for non-overlapping exons. e.g.
    /// '5:~' = 5th exon on query location is unmatched;
    /// '~:5' = 5th exon on target location is unmatched.
    ///
    /// neighboring exon comparisons of the same class are collapsed in groups:
    /// '5-20:4-19' = exons 5 to 20 match exons 4 to 19 on target 
    /// '21-23:~'   = exons 21 to 23 do not overlap the target.
    ///
    ///
    /// The exon ordinals are numbered by their position within the feature.
    string GetEvidenceString() const;
    
    /// Return the vector of individual exon comparisons
    const vector<SIntervalComparisonResult>& GetIndividualComparisons() const
    {
        return m_IntComparisons;
    }

private:    

    /// This helper struct is used to accumulate the neighboring comparisons 
    /// of the same class, such that the comparison [... 3:5  4:6 ... 20:22 ...]
    /// can be represented as [... 3-20:5-22 ...]
    struct SIntervalComparisonResultGroup
    {
    public: 
        SIntervalComparisonResultGroup(bool isReverse) 
            : m_first(0, 0, fCmp_Unknown, 0)
            , m_last(0, 0, fCmp_Unknown, 0)
            , m_isReverse(isReverse)
        {}
        
        string ToString();
        
        bool IsValid() {
            return !(m_first.m_exon_ordinal1 == 0 && m_first.m_exon_ordinal2 == 0 && m_first.m_exon_ordinal1 == 0 && m_last.m_exon_ordinal2 == 0);
        }
        
        void Reset(const SIntervalComparisonResult& r)
        {
            m_first = r;
            m_last = r;
        }
        
        /// if the comparison is neighboring and of the same class, set the terminal compariosn to it
        /// and return true; otherwise return false
        bool Add(const SIntervalComparisonResult& r)
        {
            if(r.m_position_comparison == m_last.m_position_comparison
               && r.m_result == m_last.m_result
               && ((!m_isReverse && r.m_exon_ordinal1 == m_last.m_exon_ordinal1 + 1) 
                  || (m_isReverse && r.m_exon_ordinal1 == m_last.m_exon_ordinal1 - 1) 
                  || (r.m_exon_ordinal1 == 0 && m_last.m_exon_ordinal1 == 0))
               && ((!m_isReverse && r.m_exon_ordinal2 == m_last.m_exon_ordinal2 + 1) 
                  || (m_isReverse && r.m_exon_ordinal2 == m_last.m_exon_ordinal2 - 1)
                  || (r.m_exon_ordinal2 == 0 && m_last.m_exon_ordinal2 == 0)))
            {
                m_last = r;
                return true;
            } else {
                return false;
            }   
        }

    private:
        
        SIntervalComparisonResult m_first;
        SIntervalComparisonResult m_last;
        bool m_isReverse;
    };
    
    
    //this struct is jusst a wrapper to keep the counts together
    struct ResultCounts
    {
        ResultCounts() :
            loc1_int(0),
            loc2_int(0),
            matched(0),
            partially_matched(0),
            unknown(0),    
            extra(0),
            missing(0),
            missing_3p(0),
            extra_3p(0),
            missing_5p(0),
            extra_5p(0) {}
            
        inline unsigned missing_internal() const {return missing - (missing_3p + missing_5p); }
        inline unsigned extra_internal() const {return extra - (extra_3p + extra_5p); }
        
        unsigned loc1_int;
        unsigned loc2_int;
        unsigned matched;
        unsigned partially_matched; //ext|trunc|overlap|subset|superset
        unsigned unknown;    
        unsigned extra;             //extra exons (5'+internal+3')
        unsigned missing;           //missing exons (5'+internal+3')
        unsigned missing_3p;
        unsigned extra_3p;
        unsigned missing_5p;
        unsigned extra_5p;   
    };  
    
    /// Process the seq_locs and generate the m_IntComparisons vector; Recompute the counts
    void x_Compare();
    
    /// Recompute m_len_seqloc_overlap, m_len_seqloc1, and m_len_seqloc2
    void x_ComputeOverlapValues() const; 
    
    /// Compare two exons
    FCompareLocs x_CompareInts(const CSeq_loc& loc1, const CSeq_loc& loc2) const;
    
    
    
    
    ResultCounts m_counts;
    bool m_sameStrand;
    bool m_sameBioseq;
    
    mutable bool m_cachedOverlapValues;
    mutable TSeqPos m_len_seqloc_overlap;
    mutable TSeqPos m_len_seqloc1;
    mutable TSeqPos m_len_seqloc2;
    
    
    vector<SIntervalComparisonResult> m_IntComparisons;
    const CConstRef<CSeq_loc> m_loc1;
    const CConstRef<CSeq_loc> m_loc2;
    CScope* m_scope_t;
    
};





/// CCompareFeats represens a result of comparison of two features. 
/// (CCompareFeats::m_compare stores the actual result)
/// These comparisons will be produces by CCompare_Regions
class CCompareFeats : public CObject
{
public:
    CCompareFeats(const CSeq_feat& feat1
                , const CSeq_loc& feat1_mapped_loc 
                , double mapped_identity
                , const CSeq_loc& feat1_self_loc
                , CScope* scope1
                , const CSeq_feat& feat2
                , const CSeq_loc& feat2_self_loc
                , CScope* scope2) 
        : m_feat1(&feat1)
        , m_feat1_mapped_loc(&feat1_mapped_loc)
        , m_feat1_self_loc(&feat1_self_loc)
        , m_scope_q(scope1)
        , m_feat2(&feat2)
        , m_feat2_self_loc(&feat2_self_loc)
        , m_scope_t(scope2)
        , m_compare(new CCompareSeq_locs(feat1_mapped_loc, feat2_self_loc, scope2)) // feat1_mapped_loc lives in scope2
        , m_irrelevance(0)
        , m_mapped_identity(mapped_identity)
    {}
    
    
    /// No matching feat2
    CCompareFeats(const CSeq_feat& feat1
                , const CSeq_loc& feat1_mapped_loc ///mapped to feat2's coordinate system
                , double mapped_identity
                , const CSeq_loc& feat1_self_loc 
                , CScope* scope1)
        : m_feat1(&feat1)
        , m_feat1_mapped_loc(&feat1_mapped_loc)
        , m_feat1_self_loc(&feat1_self_loc)
        , m_scope_q(scope1)
        , m_irrelevance(1) //Forward
        , m_mapped_identity(mapped_identity)
    {}
    
    /// No matching feat1
    CCompareFeats(const CSeq_feat& feat2, const CSeq_loc& feat2_self_loc, double mapped_identity, CScope* scope2) 
        : m_feat2(&feat2)
        , m_feat2_self_loc(&feat2_self_loc)
        , m_scope_t(scope2)
        , m_irrelevance(2) //Reverse
        , m_mapped_identity(mapped_identity)
    {}
    

    friend CNcbiOstream& operator<<(CNcbiOstream& out, const CCompareFeats& cf);


    
    
    double GetMappedIdentity() const 
    {
        return m_mapped_identity;
    }

    // Return true iff features being compared are of the same subtype
    bool IsSameSubtype() const
    {
        return IsMatch()
            && m_feat1->CanGetData() 
            && m_feat2->CanGetData()
            && (m_feat1->GetData().GetSubtype() == m_feat2->GetData().GetSubtype());    
    }
    
    bool IsSameType() const
    {
        return IsMatch()
            && m_feat1->CanGetData() 
            && m_feat2->CanGetData()
            && (m_feat1->GetData().Which() == m_feat2->GetData().Which());    
    }
    
    // Return true iff labels are the same and fCmp_Match flag is set in the comparison result
    bool IsIdentical() const
    {
        return IsMatch() 
            && (CCompareFeats::s_GetFeatLabel(*m_feat1) == CCompareFeats::s_GetFeatLabel(*m_feat2)) 
            && (m_compare->GetResult() & CCompareSeq_locs::fCmp_Match);
    }
    
    static string s_GetLocLabel(const CSeq_loc& loc, bool merged = false)
    {
        string s = "";
        
        if(!merged) {
            loc.GetLabel(&s);
        } else {
            CRef<CSeq_loc> merged = sequence::Seq_loc_Merge(loc, CSeq_loc::fMerge_SingleRange, NULL);
            merged->GetLabel(&s);
        }
        return s;   
    }

    static string s_GetFeatLabel(const CSeq_feat& gene_feat, feature::ELabelType type = feature::eBoth)
    {
        string gene_label = "";
        feature::GetLabel(gene_feat, &gene_label, type, NULL);  
        return gene_label; 
    }

    
    CConstRef<CSeq_feat> GetFeatQ() const {return m_feat1;}
    CConstRef<CSeq_feat> GetFeatT() const {return m_feat2;}
    
    CConstRef<CSeq_loc> GetMappedLocQ() const {return m_feat1_mapped_loc;}
    CConstRef<CSeq_loc> GetSelfLocQ() const {return m_feat1_self_loc;} 
    CConstRef<CSeq_loc> GetSelfLocT() const {return m_feat2_self_loc;}
    
    bool IsMatch() const {return !m_feat1.IsNull() && !m_feat2.IsNull();}
    CConstRef<CCompareSeq_locs> GetComparison() const {return m_compare;}
    
    
    int GetIrrelevance() const {return m_irrelevance; }
    void SetIrrelevance(int  val) {m_irrelevance =val;}
private:
    CConstRef<CSeq_feat> m_feat1;   
    CConstRef<CSeq_loc> m_feat1_mapped_loc;
    CConstRef<CSeq_loc> m_feat1_self_loc;
    CScope* m_scope_q;
    
    
    CConstRef<CSeq_feat> m_feat2;
    CConstRef<CSeq_loc> m_feat2_self_loc;
    CScope* m_scope_t;
    
    
    CRef<CCompareSeq_locs> m_compare;
    int m_irrelevance;
    bool m_unmatched;
    double m_mapped_identity;
};

///////////////////////////////////////////////////////////////////////////////

/// Compare multiple feature annotations on the specified seq_locs.
class CCompareSeqRegions : public CObject
{
public:
    enum EScoreMethod {
        eScore_SymmetricPctOverlap      ///< length of overlap / (sum of lengths - overlaps)
        , eScore_Feat1PctOverlap        ///< length of overlap / (length of 1st feat)
        , eScore_Feat2PctOverlap        ///< length of overlap / (length of 2nd feat)
    };
   
    enum EComparisonOptions {
        fSelectBest                 = (1<<0),
        fMergeExons                 = (1<<1),
        fDifferentGenesOnly         = (1<<2),
        fCreateSentinelGenes        = (1<<3),
        fSameTypeOnly               = (1<<4)
    };
    
    typedef int TComparisonOptions; 
    
    
    CCompareSeqRegions(const CSeq_loc& query_loc
                     , CScope* q_scope
                     , CScope* t_scope
                     , ILocMapper& mapper
                     , const SAnnotSelector& q_sel
                     , const SAnnotSelector& t_sel
                     , const CSeq_id& target_id
                     , TComparisonOptions options = fSelectBest|fMergeExons
                     , EScoreMethod score_method = eScore_SymmetricPctOverlap) 
        : m_loc_q(&query_loc) 
        , m_scope_q(q_scope)
        , m_scope_t(t_scope)
        , m_mapper(&mapper)
        , m_selector_q(q_sel)
        , m_selector_t(t_sel)
        , m_target_id(&target_id)
        , m_comp_options(options)
        , m_score_method(score_method)
        , m_loc_q_ci(*m_scope_q, *m_loc_q, q_sel)
        , m_already_processed_unmatched_targets(false)
    {
        
        //when initializing the whole_locs, we use maximally large intervals
        //instead of Whole seqloc type because Seq_loc_Mapper can't digest those
        //in the case of incomplete scopes, such as pre-locuslink LDS type
        CRef<CSeq_loc> t_whole_loc(new CSeq_loc);
        CRef<CSeq_id> t_id(new CSeq_id);
        t_id->Assign(target_id);
        t_whole_loc->SetInt().SetId(*t_id);
        t_whole_loc->SetInt().SetFrom(0);
        t_whole_loc->SetInt().SetTo(((TSeqPos) (-10)));
        
        
        CRef<CSeq_loc> q_whole_loc(new CSeq_loc);
        CRef<CSeq_id> q_id(new CSeq_id);
        q_id->Assign(sequence::GetId(query_loc, 0));
        q_whole_loc->SetInt().SetId(*q_id);
        q_whole_loc->SetInt().SetFrom(0);
        q_whole_loc->SetInt().SetTo(((TSeqPos) (-10)));
        

        m_self_mapper_q.Reset(new CSeq_loc_Mapper(*q_whole_loc, *q_whole_loc, m_scope_q));
        m_self_mapper_t.Reset(new CSeq_loc_Mapper(*t_whole_loc, *t_whole_loc, m_scope_t));
        
        m_seen_targets.clear();
    }
    
    
    void Rewind() {
        m_loc_q_ci.Rewind();
        m_seen_targets.clear();
    }
    
    TComparisonOptions& SetOptions() {return m_comp_options;}
    TComparisonOptions GetOptions() const {return m_comp_options;}
    
    const CSeq_loc& GetQueryLoc() const {return *m_loc_q;}
 
    bool NextComparisonGroup(vector<CRef<CCompareFeats> >& v);
    void SelectMatches(vector<CRef<CCompareFeats> >& v);
    static int CCompareSeqRegions::s_GetGeneId(const CSeq_feat& feat);
private:
    void x_GetPutativeMatches(vector<CRef<CCompareFeats> >& v, CConstRef<CSeq_feat> q_feat);
    CConstRef<CSeq_loc> x_GetSelfLoc(
            const CSeq_loc& loc, 
            CScope* scope,
            bool merge_single_range);
    
    
    
    CConstRef<CSeq_loc> m_loc_q;
    CScope* m_scope_q;
    CScope* m_scope_t; 
    CRef<ILocMapper> m_mapper;
    const SAnnotSelector& m_selector_q;
    const SAnnotSelector& m_selector_t;
    CConstRef<CSeq_id> m_target_id;
    CRef<CSeq_loc_Mapper> m_self_mapper_q;
    CRef<CSeq_loc_Mapper> m_self_mapper_t;
    TComparisonOptions m_comp_options;
    EScoreMethod m_score_method;
    CFeat_CI m_loc_q_ci;
    std::set<std::string> m_seen_targets;
        //loc-labels of all target features that have been compared
        //(we use it to collect the target features that are not comparable at the end)
    bool m_already_processed_unmatched_targets;
};
    

END_NCBI_SCOPE

#endif
