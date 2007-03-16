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
    
    CCompareSeq_locs(const CSeq_loc& loc1, CScope* scope1, const CSeq_loc& loc2, CScope* scope2)
        : m_loc1(&loc1)
        , m_scope1(scope1)
        , m_loc2(&loc2)
        , m_scope2(scope2)
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
    /// can be represented as [... 3-20:5:22 ...]
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
            
        string GetDump() {
            CNcbiOstrstream strm;  
            strm
                << "(loc1_int:" << loc1_int
                << ", loc2_int:" << loc2_int
                << ", matched:" << matched
                << ", partially_matched:" << partially_matched
                << ", unknown:" << unknown            
                << ", missing:" << missing
                << ", missing_5p:" << missing_5p
                << ", missing_3p:" << missing_3p
                << ", extra:" << extra
                << ", extra_5p:" << extra_5p
                << ", extra_3p:" << extra_3p
                << ")"; 
            return  CNcbiOstrstreamToString(strm); 
        }

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
    void x_ComputeOverlapValues() const 
    {
        CRef<CSeq_loc> merged_loc1 = sequence::Seq_loc_Merge(*m_loc1, CSeq_loc::fMerge_All, m_scope1);
        CRef<CSeq_loc> merged_loc2 = sequence::Seq_loc_Merge(*m_loc2, CSeq_loc::fMerge_All, m_scope2);
        
        CRef<CSeq_loc> subtr_loc1 = sequence::Seq_loc_Subtract(*merged_loc1, *merged_loc2, CSeq_loc::fMerge_All, m_scope1);
        
        TSeqPos subtr_len;
        try {
            subtr_len = sequence::GetLength(*subtr_loc1, m_scope1);
        } catch (...) {
            subtr_len = 0;
        }
        
        try {
            m_len_seqloc1 = sequence::GetLength(*merged_loc1, m_scope1);
        } catch(...) {
            m_len_seqloc1 = 0;
        }
        
        try {
            m_len_seqloc2 = sequence::GetLength(*merged_loc2, m_scope1);
        } catch(...) {
            m_len_seqloc2 = 0;
        }
        
        m_len_seqloc_overlap = m_len_seqloc1 - subtr_len;
  
        m_cachedOverlapValues = true;
    }
    
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
    CScope* m_scope1;
    const CConstRef<CSeq_loc> m_loc2;
    CScope* m_scope2;
};





/// CCompareFeats represens a result of comparison of two features. 
/// (CCompareFeats::m_compare stores the actual result)
/// These comparisons will be produces by CCompare_Regions
class CCompareFeats : public CObject
{
public:
    CCompareFeats(const CSeq_feat& feat1
                , const CSeq_loc& feat1_mapped_loc ///mapped to feat2's coordinate system
                , CScope* scope1
                , const CSeq_feat& feat2
                , CScope* scope2) 
        : m_feat1(&feat1)
        , m_feat1_mapped_loc(&feat1_mapped_loc)
        , m_scope1(scope1)
        , m_feat2(&feat2)
        , m_scope2(scope2)
        , m_compare(new CCompareSeq_locs(feat1_mapped_loc, scope1, feat2.GetLocation(), scope2))
        , m_unmatched(false)
    {}
    
    
    /// No matching feat2
    CCompareFeats(const CSeq_feat& feat1
                , const CSeq_loc& feat1_mapped_loc ///mapped to feat2's coordinate system
                , CScope* scope1)
        : m_feat1(&feat1)
        , m_feat1_mapped_loc(&feat1_mapped_loc)
        , m_scope1(scope1)
        , m_unmatched(true)
    {}
    
    /// No matching feat1
    CCompareFeats(const CSeq_feat& feat2, CScope* scope2) 
        : m_feat2(&feat2)
        , m_scope2(scope2)
        , m_unmatched(true)
    {}
    

    friend CNcbiOstream& operator<<(CNcbiOstream& out, const CCompareFeats& cf);


    /// If any of the locs is partial or they are of different subtypes - return relative overlap score.
    /// Otherwise return symmetrical overlap score.
    ///
    /// Rationale: in the former case, if one feature contains another, but the containee endpoints
    /// are fuzzy or are expected to be fully contained (e.g. mRNA fully contains comparison exon), 
    /// then we don't want to penalize for non-overlapping regions of the larger feature.
    double GetAutoOverlap() const
    {
        if (m_unmatched) return 0.0;
         
        bool isPartial1 = m_feat1_mapped_loc->IsPartialStart(eExtreme_Positional)
                       || m_feat1_mapped_loc->IsPartialStop(eExtreme_Positional)
                       || m_feat1_mapped_loc->IsTruncatedStart(eExtreme_Positional)
                       || m_feat1_mapped_loc->IsTruncatedStop(eExtreme_Positional);
                      
        bool isPartial2 = m_feat2->GetLocation().IsPartialStart(eExtreme_Positional)
                       || m_feat2->GetLocation().IsPartialStop(eExtreme_Positional)
                       || m_feat2->GetLocation().IsTruncatedStart(eExtreme_Positional)
                       || m_feat2->GetLocation().IsTruncatedStop(eExtreme_Positional);
        
        if(isPartial1 || isPartial2 || !IsSameSubtype())  {
            return m_compare->GetRelativeOverlap();
        } else {
            return m_compare->GetSymmetricalOverlap();
        } 
    }
    

    // Return true iff features being compared are of the same subtype
    bool IsSameSubtype() const
    {
        return !m_unmatched 
            && m_feat1->CanGetData() 
            && m_feat2->CanGetData()
            && (m_feat1->GetData().GetSubtype() == m_feat2->GetData().GetSubtype());    
    }
    
    // Return true iff labels are the same and fCmp_Match flag is set in the comparison result
    bool IsIdentical() const
    {
        return !m_unmatched 
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

    CConstRef<CSeq_feat> m_feat1;   
    CConstRef<CSeq_loc> m_feat1_mapped_loc;
    CScope* m_scope1;
    CConstRef<CSeq_feat> m_feat2;
    CScope* m_scope2;
    CRef<CCompareSeq_locs> m_compare;
    bool m_unmatched;
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
    
    CCompareSeqRegions(const CSeq_loc& query_loc
                     , CScope* q_scope
                     , CScope* t_scope
                     , ILocMapper& mapper
                     , EScoreMethod score_method = eScore_SymmetricPctOverlap) 
        : m_loc1(&query_loc) 
        , m_scope1(q_scope)
        //, m_loc2(&target_loc)
        , m_scope2(t_scope)
        , m_mapper(&mapper)
        , m_score_method(score_method)
        
    {
        //TODO: make sure the locs are on single bioseq each.
        //When processing feats on the region filter out the segments 
        //that point to different seqs
        
        SAnnotSelector sel;
        sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
        sel.SetResolveMethod(SAnnotSelector::eResolve_All);
        sel.SetAdaptiveDepth(true);
        CFeat_CI ci(*m_scope1, *m_loc1, sel);
        m_loc1_ci = ci;
    }
    
    void Rewind() {
        m_loc1_ci.Rewind();
    }

    bool NextComparisonGroup(vector<CRef<CCompareFeats> >& v);
private:
    CFeat_CI m_loc1_ci;
    CConstRef<CSeq_loc> m_loc1;
    CScope* m_scope1;
    //CConstRef<CSeq_loc> m_loc2;
    CScope* m_scope2; 
    CRef<ILocMapper> m_mapper;
    EScoreMethod m_score_method;
};
    

END_NCBI_SCOPE

#endif
