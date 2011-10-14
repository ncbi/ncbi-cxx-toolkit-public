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

#include <ncbi_pch.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Feat_id.hpp>


#include <algo/sequence/loc_mapper.hpp>
#include <algo/sequence/compare_feats.hpp>
#include <queue>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// Comparison functor for pqueue storing related comparisons
struct SCompareFeats_OpLess : public binary_function<CRef<CCompareFeats>&, CRef<CCompareFeats>&, bool>
{
public:
    bool operator()(const CRef<CCompareFeats>& c1, const CRef<CCompareFeats>& c2) const
    {    
#if 0
        string s1q = "";
        feature::GetLabel(*c1->GetFeatQ(), &s1q, feature::eContent);
        string s1t = "";
        feature::GetLabel(*c1->GetFeatT(), &s1t, feature::eContent);
        string s2q = "";
        feature::GetLabel(*c2->GetFeatQ(), &s2q, feature::eContent);
        string s2t = "";
        feature::GetLabel(*c2->GetFeatT(), &s2t, feature::eContent);
        
#endif
        
        
        //any match is better than no match
        if(!c1->IsMatch() && c2->IsMatch()) return true;
        if(c1->IsMatch() && !c2->IsMatch()) return false;
        if(!c1->IsMatch() && !c2->IsMatch()) return c1->GetFeatQ().IsNull();
        
        //Same type is better
        bool c1_sameType = c1->IsSameType();
        bool c2_sameType = c2->IsSameType();
        if(c1_sameType && !c2_sameType) return false;
        if(!c1_sameType && c2_sameType) return true;


        //same product is better
        try {
            bool c1_same_product = !c1->GetFeatQ().IsNull() && c1->GetFeatQ()->CanGetProduct() 
                                && !c1->GetFeatT().IsNull() && c1->GetFeatT()->CanGetProduct()
                                &&  sequence::IsSameBioseq(
                                        sequence::GetId(c1->GetFeatQ()->GetProduct(), NULL),
                                        sequence::GetId(c1->GetFeatT()->GetProduct(), NULL),
                                        NULL);
            bool c2_same_product = !c2->GetFeatQ().IsNull() && c2->GetFeatQ()->CanGetProduct() 
                                && !c2->GetFeatT().IsNull() && c2->GetFeatT()->CanGetProduct()
                                &&  sequence::IsSameBioseq(
                                        sequence::GetId(c2->GetFeatQ()->GetProduct(), NULL),
                                        sequence::GetId(c2->GetFeatT()->GetProduct(), NULL),
                                        NULL);
        
            if(c1_same_product && !c2_same_product) return false;
            if(!c1_same_product && c2_same_product) return true;
        } catch(CException&) {
            ;
        }


        //the similarity score is a composite of the score based on shared sites, and the symmetrical overlap.
        //(both are 0..1). The constant below is used to give more weight to shared sites score
        const float k = 0.8f;

        float score1(0.0f);
        float score2(0.0f);
        c1->GetComparison()->GetSplicingSimilarity(score1);
        c2->GetComparison()->GetSplicingSimilarity(score2);

        score1 = k * score1 + (1.0 - k) * c1->GetComparison()->GetSymmetricalOverlap();
        score2 = k * score2 + (1.0 - k) * c2->GetComparison()->GetSymmetricalOverlap();

        if(score1 < score2) return true;
        if(score2 > score1) return false;

        //same subtype is better than different subtypes (I bet we NEVER get to this point)
        bool c1_sameSubtype = c1->IsSameSubtype();
        bool c2_sameSubtype = c2->IsSameSubtype();
        if(c1_sameSubtype && !c2_sameSubtype) return false;
        if(!c1_sameSubtype && c2_sameSubtype) return true;
        
        return false;    
    }
private:

};






CNcbiOstream& operator<<(CNcbiOstream& out, const CCompareFeats& cf)
{
    if(!cf.m_feat1.IsNull()) {
        out << CCompareFeats::s_GetFeatLabel(*cf.m_feat1) << "\t";
        out << CCompareFeats::s_GetLocLabel(cf.m_feat1->GetLocation(), true) << "\t";
        out << CCompareFeats::s_GetLocLabel(*cf.m_feat1_mapped_loc, true) << "\t";
    } else {
        out << "\t\t\t";
    }
    
    if(!cf.m_feat2.IsNull()) {
        out << CCompareFeats::s_GetFeatLabel(*cf.m_feat2) << "\t";
        out << CCompareFeats::s_GetLocLabel(cf.m_feat2->GetLocation(), true) << "\t";
    } else {
        out << "\t\t";
    }
    
    if(!cf.m_unmatched) {
        out.setf(ios::fixed);
        out.setf(ios::showpoint);
        out.precision(2);

        string sResult;
        cf.m_compare->GetResult(&sResult);
        out << cf.m_compare->GetEvidenceString() << "\t";
        cf.m_compare->GetResult(&sResult);
        out << sResult << "\t";
        
        out << cf.GetMappedIdentity() << "\t";
        out << cf.m_compare->GetRelativeOverlap() << "\t";
        out << cf.m_compare->GetSymmetricalOverlap();
    } else {
        out << "\t\t\t\t";
    }
    return out;
}


        
string CCompareSeq_locs::SIntervalComparisonResultGroup::ToString()
{
    CNcbiOstrstream strm; 
    string s_pos1;
    string s_pos2;
    if(m_first.m_exon_ordinal1 == m_last.m_exon_ordinal1 && m_first.m_exon_ordinal2 == m_last.m_exon_ordinal2) {
        s_pos1 = (m_first.m_exon_ordinal1 == 0 ? (m_first.m_result == fCmp_Unknown ? "?" : "~") : NStr::IntToString(m_first.m_exon_ordinal1));
        s_pos2 = (m_first.m_exon_ordinal2 == 0 ? (m_first.m_result == fCmp_Unknown ? "?" : "~") : NStr::IntToString(m_first.m_exon_ordinal2));
    } else {
        if(m_first.m_exon_ordinal1 == 0) {
            s_pos1 = (m_first.m_result == fCmp_Unknown ? "?" : "~");
        } else {
            s_pos1 =  NStr::IntToString(m_first.m_exon_ordinal1) + "-" +  NStr::IntToString(m_last.m_exon_ordinal1);
        }
        
        if(m_first.m_exon_ordinal2 == 0) {
            s_pos2 = (m_first.m_result == fCmp_Unknown ? "?" : "~");
        } else {
            s_pos2 = NStr::IntToString(m_first.m_exon_ordinal2) + "-" +  NStr::IntToString(m_last.m_exon_ordinal2);
        }
    }
    
    strm << s_pos1 << ":";
    
    if(m_first.m_result & fCmp_StrandDifferent) {
        strm << "strand-mismatch(" << s_pos2 << ")";
    } else { 
        bool overlap_5p = (m_first.m_result & fCmp_Overlap) && (m_first.m_position_comparison < 0);
        bool overlap_3p = (m_first.m_result & fCmp_Overlap) && (m_first.m_position_comparison > 0);

        strm << ((m_first.m_result & (fCmp_5pExtension  | fCmp_Superset) || overlap_5p) ? ">" :
                ((m_first.m_result & (fCmp_5pTruncation | fCmp_Subset)   || overlap_3p) ? "<" : ""));
        strm << s_pos2;        
        strm << ((m_first.m_result & (fCmp_3pTruncation | fCmp_Subset)   || overlap_5p) ? ">" :
                ((m_first.m_result & (fCmp_3pExtension  | fCmp_Superset) || overlap_3p) ? "<" : ""));        
    }
  
   
    return CNcbiOstrstreamToString(strm);
}

string CCompareSeq_locs::GetEvidenceString() const
{
    CNcbiOstrstream strm;   
    strm << "[";
    string sep = "";
    
    SIntervalComparisonResultGroup grp(false);

    int ii(0);
    ITERATE(vector<SIntervalComparisonResult>, it, m_IntComparisons) {
        SIntervalComparisonResult comp = *it;
        if(!grp.Add(comp)) {
            if(grp.IsValid()) {
                strm << sep << grp.ToString();
                sep = "  ";
            }
            grp.Reset(comp);   
        }  
        ii++;
    }

    strm << sep << grp.ToString() << "](" << ii << ")";
    return  CNcbiOstrstreamToString(strm); 
}   



/// Compare two loc primitives
CCompareSeq_locs::FCompareLocs CCompareSeq_locs::x_CompareInts(const CSeq_loc& loc1, const CSeq_loc& loc2) const
{
    bool is5p_match = loc1.GetStart(eExtreme_Biological) == loc2.GetStart(eExtreme_Biological);
    bool is3p_match = loc1.GetStop(eExtreme_Biological) == loc2.GetStop(eExtreme_Biological);

    sequence::ECompare cmp = sequence::Compare(loc1, loc2, m_scope_t); //we are comparing mapped query loc vs
                                                                       //the target, hence the target scope
    
    switch(cmp) {
        case sequence::eSame:       return fCmp_Match;
        case sequence::eNoOverlap:  return fCmp_NoOverlap;
        case sequence::eOverlap:    return fCmp_Overlap;
        case sequence::eContains:   return is5p_match ? fCmp_3pExtension : (is3p_match ? fCmp_5pExtension : fCmp_Superset);
        case sequence::eContained:  return is5p_match ? fCmp_3pTruncation : (is3p_match ? fCmp_5pTruncation : fCmp_Subset);
        default:                    return fCmp_Unknown;      
    }
}



/// Cross-compare the intervals on both locs
void CCompareSeq_locs::x_Compare()
{
    m_IntComparisons.clear(); //our job here is to refill this
    

    // loc1 and loc2 will be the locations that we will compare.
    // If all intervals on all locations are on the same sequence we sort the intervals.
    // (because we need to know about 3'|5' terminality) Hence, the exon comparisons will be
    // in sorted order regardless of their positional order in the loc-mix.
    // 
    // If some intervals refer to other bioseqs we assume that our locs have them in order.
    // The fact is stored in this->m_sameBioseq
    CRef<CSeq_loc> loc1;
    CRef<CSeq_loc> loc2;

    try {
        const CSeq_id& seq_id1 = sequence::GetId(*m_loc1, m_scope_t);
        const CSeq_id& seq_id2 = sequence::GetId(*m_loc2, m_scope_t);
        this->m_sameBioseq = sequence::IsSameBioseq(seq_id1, seq_id2, m_scope_t);
    } catch(...) { //GetId may throw if intervals refer to multiple bioseqs
        this->m_sameBioseq = false;   
    }
    
    if(false && this->m_sameBioseq) { //disabled sorting
        //this "merge" is actually just a sort
        loc1 = sequence::Seq_loc_Merge(*m_loc1, CSeq_loc::fSort, m_scope_t);
        loc2 = sequence::Seq_loc_Merge(*m_loc2, CSeq_loc::fSort, m_scope_t);
    } else {
        loc1.Reset(new CSeq_loc);
        loc2.Reset(new CSeq_loc);
        loc1->Assign(*m_loc1);
        loc2->Assign(*m_loc2);
    }

   
    //If we have a packed int, then when iterating, CSeq_loc_CI::GetSeq_loc() will
    //fetch the whole packed int instead of an individual interval which we need.
    //Hence, 
    loc1->ChangeToMix();
    loc2->ChangeToMix();
    
    
    m_sameStrand = SameOrientation(sequence::GetStrand(*loc1, m_scope_t), sequence::GetStrand(*loc2, m_scope_t));
    

    //To avoid issues if some interval points to another sequence or they are not sorted (as a result of 
    //mapping or otherwise), we compare every interval against every other interval 
    //leading to O(exon_count1*exon_count2) complexity.
    //TODO: optimize to O(exon_count1+exon_count2) complexity after self-mapping seq-loc filter is implemented.
    
    set<unsigned> loc2_reported_set;     //keep track of matched exons on loc2 so we can report the unmatched
    unsigned it1_exon_ordinal = 1;
    
    
    
    int adjust_for_strand = IsReverse(sequence::GetStrand(*loc1, m_scope_t)) ? -1 : 1;
    
    //have to use eEmpty_Allow to conserve the numbering of exons in the presence of non-mapping intervals
    for(CSeq_loc_CI it1(*loc1, CSeq_loc_CI::eEmpty_Allow, CSeq_loc_CI::eOrder_Positional); 
       it1;
       ++it1, ++it1_exon_ordinal) 
    {
        CConstRef<CSeq_loc> ci_loc1 = it1.GetRangeAsSeq_loc();
        unsigned it2_exon_ordinal = 1;
        bool loc1_found_Overlap = false;
        
        int it1_cmp_it2 = 0; //positive iff loc1 is 5' further and does not overlap loc1
        
        
        for(CSeq_loc_CI it2(*loc2, CSeq_loc_CI::eEmpty_Allow, CSeq_loc_CI::eOrder_Positional); 
            it2; 
            ++it2, ++it2_exon_ordinal) 
        {
            CConstRef<CSeq_loc> ci_loc2 = it2.GetRangeAsSeq_loc();
            FCompareLocs cmp_res = x_CompareInts(*ci_loc1, *ci_loc2);
            
            try {
                it1_cmp_it2 = adjust_for_strand * 
                              (ci_loc1->GetStart(eExtreme_Biological) > 
                               ci_loc2->GetStop(eExtreme_Biological) ? 1 : -1);
            } catch (...) {
                ; //reuse the last value
            }
                    
            
            // if no overlap and the segment on the other loc hasn't been reported and we already passed it
            if((cmp_res == fCmp_Unknown || cmp_res == fCmp_NoOverlap) 
               && loc2_reported_set.find(it2_exon_ordinal) == loc2_reported_set.end() 
               && it1_cmp_it2 > 0) 
            {
                loc2_reported_set.insert(it2_exon_ordinal);
                
                SIntervalComparisonResult sRes(
                    0                   //no corresponding exon on loc1
                  , it2_exon_ordinal    //for this exon on loc2
                  , cmp_res, it1_cmp_it2);   
                  
                m_IntComparisons.push_back(sRes);   
                
            } else if (cmp_res != fCmp_Unknown && cmp_res != fCmp_NoOverlap) {
                //also check the matching of directions; if problem - report it as strand mismatch
                if(!m_sameStrand) {
                    cmp_res = fCmp_StrandDifferent;
                }  
                
                SIntervalComparisonResult sRes(it1_exon_ordinal, it2_exon_ordinal, cmp_res, it1_cmp_it2);
                m_IntComparisons.push_back(sRes);
                
                loc2_reported_set.insert(it2_exon_ordinal);
                loc1_found_Overlap = true;
            }
        }
        
        if(!loc1_found_Overlap) {
            SIntervalComparisonResult sRes(it1_exon_ordinal, 0, fCmp_NoOverlap); 
            m_IntComparisons.push_back(sRes); 
        }
    }
    
    //add the missing / unknown intervals from the loc2
    unsigned it2_exon_ordinal = 1;
    for(CSeq_loc_CI it2(*loc2); it2; ++it2, ++it2_exon_ordinal) {
        if(loc2_reported_set.find(it2_exon_ordinal) == loc2_reported_set.end()) {
            SIntervalComparisonResult sRes(0, it2_exon_ordinal, fCmp_NoOverlap, 0);
            m_IntComparisons.push_back(sRes);  
        }
    }


    
    
    //compute the stats
    for(vector<SIntervalComparisonResult>::iterator it = m_IntComparisons.begin(); 
       it != m_IntComparisons.end(); 
       ++it) 
    {
        
        if(m_counts.missing_5p == 0 && it->missing_first()) {
            m_counts.extra_5p++;
        } else if(m_counts.extra_5p == 0 && it->missing_second()) {
            m_counts.missing_5p++;
        } else {
            break;
        }
    }
    
    for(vector<SIntervalComparisonResult>::reverse_iterator it = m_IntComparisons.rbegin(); 
        it != m_IntComparisons.rend(); 
        ++it) 
    {
        if(m_counts.missing_3p == 0 && it->missing_first()) {
            m_counts.extra_3p++;
        } else if(m_counts.extra_3p == 0 && it->missing_second()) {
            m_counts.missing_3p++;
        } else {
            break;
        }
    }
    
    for(vector<SIntervalComparisonResult>::iterator it = m_IntComparisons.begin(); it != m_IntComparisons.end(); ++it) {
        m_counts.loc1_int = std::max(m_counts.loc1_int, it->m_exon_ordinal1);
        m_counts.loc2_int = std::max(m_counts.loc2_int, it->m_exon_ordinal2);

        if(it->missing_first()) {
            (it->m_result & fCmp_Unknown) ? m_counts.unknown++ : m_counts.missing++;
        } else if(it->missing_second()) {
            (it->m_result & fCmp_Unknown) ? m_counts.unknown++ : m_counts.extra++;
        } else {
            (it->m_result & fCmp_Match) ? m_counts.matched++ : m_counts.partially_matched++;
        }
    }
}

/// Recompute m_len_seqloc_overlap, m_len_seqloc1, and m_len_seqloc2
void CCompareSeq_locs::x_ComputeOverlapValues() const 
{
    CRef<CSeq_loc> merged_loc1 = sequence::Seq_loc_Merge(*m_loc1, CSeq_loc::fMerge_All, m_scope_t);
    CRef<CSeq_loc> merged_loc2 = sequence::Seq_loc_Merge(*m_loc2, CSeq_loc::fMerge_All, m_scope_t);
    
    //fix: if have strand mismatches - the overlaps are forced to zero
    if(!m_sameStrand) {
        merged_loc1->SetEmpty();
        merged_loc2->SetEmpty();
    }   
    

    CRef<CSeq_loc> subtr_loc1 = sequence::Seq_loc_Subtract(*merged_loc1, *merged_loc2, CSeq_loc::fMerge_All, m_scope_t);
    
    TSeqPos subtr_len;
    try {
        subtr_len = sequence::GetLength(*subtr_loc1, m_scope_t);
    } catch (...) { 
        subtr_len = 0;
    }   
    
    try {
        m_len_seqloc1 = sequence::GetLength(*merged_loc1, m_scope_t);
    } catch(...) {
        m_len_seqloc1 = 0;
    }   
    
    try {
        m_len_seqloc2 = sequence::GetLength(*merged_loc2, m_scope_t);
    } catch(...) {
        m_len_seqloc2 = 0;
    }

    m_len_seqloc_overlap = m_len_seqloc1 - subtr_len;



    //Compute shared sites score
    m_shared_sites_score = 0.0f;
    m_loc1_interval_count = 0;
    m_loc2_interval_count = 0;

    try { 
        merged_loc1->ChangeToMix();
        merged_loc2->ChangeToMix();
        
        TSeqPos terminal_start = min(sequence::GetStart(*merged_loc1, NULL), sequence::GetStart(*merged_loc2, NULL));
        TSeqPos terminal_stop = max(sequence::GetStop(*merged_loc1, NULL), sequence::GetStop(*merged_loc2, NULL));

        //if splice site matches exactly, it gets the score of 1.
        //if it is not exact, it linearly drops down to zero at thr.
        const float terminal_jitter_thr = 20.0f;
        const float splice_jitter_thr = 5.0f;

        for(CSeq_loc_CI ci1(*m_loc1); ci1; ++ci1) {
            CConstRef<CSeq_loc> ci_loc1 = ci1.GetRangeAsSeq_loc();
            TSeqPos seg1_start = sequence::GetStart(*ci_loc1, NULL);
            TSeqPos seg1_stop = sequence::GetStop(*ci_loc1, NULL);
            float best_match_start = 0.0f;
            float best_match_stop = 0.0f;
            m_loc1_interval_count++;
            for(CSeq_loc_CI ci2(*merged_loc2); ci2; ++ci2) {
                CConstRef<CSeq_loc> ci_loc2 = ci2.GetRangeAsSeq_loc();
                if(m_loc1_interval_count == 1) {
                    m_loc2_interval_count++; //compute only once in this loop
                }
                ENa_strand strand1 = sequence::GetStrand(*ci_loc1, NULL);
                ENa_strand strand2 = sequence::GetStrand(*ci_loc2, NULL);
                bool same_strand = strand1 == strand2
                               || strand1 == eNa_strand_both
                               || strand2 == eNa_strand_both
                               || (strand1 == eNa_strand_unknown  && strand2 != eNa_strand_minus)
                               || (strand2 == eNa_strand_unknown  && strand1 != eNa_strand_minus);
                if(!same_strand) {
                    continue;
                }
                
                TSeqPos seg2_start = sequence::GetStart(*ci_loc2, NULL);
                TSeqPos seg2_stop = sequence::GetStop(*ci_loc2, NULL);
                
                float thr = (seg1_start == terminal_start
                          || seg1_stop == terminal_stop
                          || seg2_start == terminal_start
                          || seg2_stop == terminal_stop ) ? terminal_jitter_thr : splice_jitter_thr;
                float match_start = max(0.0f, 1.0f - abs((long)seg1_start - (long)seg2_start) / thr);
                best_match_start = max(match_start, best_match_start);
                float match_stop = max(0.0f, 1.0f - abs((long)seg1_stop - (long)seg2_stop) / thr);
                best_match_stop = max(match_stop, best_match_stop);
            }   
            m_shared_sites_score += best_match_start;
            m_shared_sites_score += best_match_stop;
        }   

        m_shared_sites_score = 0.5*m_shared_sites_score / (m_loc1_interval_count + m_loc2_interval_count - (0.5*m_shared_sites_score));
    } catch (CException&) {
        ;
    }

    m_cachedOverlapValues = true;
}

CCompareSeq_locs::TCompareLocsFlags CCompareSeq_locs::GetResult(string* str_result) const
{
    TCompareLocsFlags result_flags(0);

    CNcbiOstrstream strm;
         
    //
    //Fuzz: to be implemented. set the fCmp_Fuzz flag if necessary; do not return;
    //

    if(!m_sameStrand) {
        if(str_result) *str_result = "strand mismatch; ";
        return result_flags | fCmp_StrandDifferent;
    } else if(m_counts.loc1_int == m_counts.loc2_int && m_counts.loc1_int == m_counts.matched) {
        if(str_result) *str_result = "complete match; ";
        return result_flags | fCmp_Match;    
    }

    if(m_counts.matched > 0 ) {
        strm << m_counts.matched << " exact; ";
    }
    
    if(m_counts.partially_matched > 0) {
        strm << m_counts.partially_matched << " partial; ";
    }
    
    //if we have no mismatches internally, report mismatches on the ends if we have them
    //otherwise just report the total mismatches
    if(m_counts.missing_internal() == 0 && m_counts.extra_internal() == 0) {
        if(m_counts.extra_5p > 0) strm << m_counts.extra_5p << " novel @5'; ";
        if(m_counts.extra_3p > 0) strm << m_counts.extra_3p << " novel @3'; ";
        if(m_counts.missing_5p > 0) strm << m_counts.missing_5p << " missing @5'; ";
        if(m_counts.missing_3p > 0) strm << m_counts.missing_3p << " missing @3'; ";
    } else {
        if(m_counts.missing != 0) strm << m_counts.missing << " missing; "; 
        if(m_counts.extra != 0)   strm << m_counts.extra << " novel; ";
    }
    
    //set the corresponding flags 
    if(m_counts.extra_5p > 0)           result_flags |= fCmp_intsExtra_5p;
    if(m_counts.missing_5p > 0)         result_flags |= fCmp_intsMissing_5p;
    if(m_counts.extra_3p > 0)           result_flags |= fCmp_intsExtra_3p;
    if(m_counts.missing_3p > 0)         result_flags |= fCmp_intsMissing_3p;
    if(m_counts.missing_internal() > 0) result_flags |= fCmp_intsMissing_internal;
    if(m_counts.extra_internal() > 0)   result_flags |= fCmp_intsExtra_internal;                                                    


    //report extensions / truncatinos of the terminal exons
    SIntervalComparisonResult terminal5p_comparison = m_IntComparisons.front();
    SIntervalComparisonResult terminal3p_comparison = m_IntComparisons.back();
    
    if(terminal5p_comparison.m_result == fCmp_5pExtension)  {
        result_flags |= fCmp_5pExtension;
        strm << "5'extended; ";
    } else if(terminal5p_comparison.m_result == fCmp_5pTruncation) {
        result_flags |= fCmp_5pTruncation;
        strm << "5'truncated; ";
    }
    
    if(terminal3p_comparison.m_result == fCmp_3pExtension)  {
        result_flags |= fCmp_3pExtension;
        strm << "3'extended; ";
    } else if(terminal3p_comparison.m_result == fCmp_3pTruncation) {
        result_flags |= fCmp_3pTruncation;
        strm << "3'truncated; ";
    }
    
    if(!result_flags) {
        sequence::ECompare cmp = sequence::Compare(*m_loc1, *m_loc2, m_scope_t); 
    
        switch(cmp) {
            case sequence::eSame:       
                result_flags |= fCmp_Match;
                strm << "complete match; ";
            break;
                
            case sequence::eNoOverlap:  
                if(-1 != sequence::TestForOverlap(*this->m_loc1, *this->m_loc2, sequence::eOverlap_Simple)) {
                    result_flags |= fCmp_RegionOverlap;
                    strm << "region overlap; ";
                } else {
                    result_flags |= fCmp_NoOverlap;
                    strm << "no overlap; ";
                }
            break;
            
            case sequence::eOverlap:   
                result_flags |= fCmp_Overlap; 
                strm << "overlap; ";
            break;
            
            case sequence::eContains:
                result_flags |= fCmp_Superset;
                strm << "superset; ";
            break;
            
            case sequence::eContained:
                result_flags |= fCmp_Subset;
                strm << "subset; ";
            break;
            
            default:
            break; 
        }  
    }
    
    if(!result_flags) {
        result_flags |= fCmp_Unknown;
        strm << "unknown; ";
    }

    if(str_result) {
        *str_result = CNcbiOstrstreamToString(strm); 
    }
    
    return result_flags;
}



//return feature's gene_id/locus_id or prelocuslink gene number
int CCompareSeqRegions::s_GetGeneId(const CSeq_feat& feat) 
{
    //normally locus_id is in feat's dbxref
    if(feat.IsSetDbxref()) {
        ITERATE (CSeq_feat::TDbxref, dbxref, feat.GetDbxref()) {
            if ((*dbxref)->GetDb() == "GeneID"  || (*dbxref)->GetDb() == "LocusID") {
                return (*dbxref)->GetTag().GetId();
            }
        }
    }
    
    
    //but sometimes it is in Db
    if(feat.CanGetData() ) 
    {
        if(feat.GetData().IsGene()) {
            ITERATE (CSeq_feat::TDbxref, dbxref, feat.GetData().GetGene().GetDb()) {
                if ((*dbxref)->GetDb() == "GeneID"  || (*dbxref)->GetDb() == "LocusID") {
                    return (*dbxref)->GetTag().GetId();
                }
            }
            
            //for prelocuslink gene annots the gene number is here:
            try {
                return NStr::StringToInt(feat.GetData().GetGene().GetLocus());
            } catch (...) {};
        } else if(feat.GetData().IsRna()) {
            /*for prelocuslink merged ASN, rnas look like this:
             *
               Seq-feat ::= {
                  data rna {
                    type mRNA,
                    ext name "67485"
                  },
                  product whole local str "MmUn_53691_37.35221.67485.m",
               ...
               
             * The gene_id is the number is 35221
             */
                
            
            
            try {
                std::vector<string> tokens;
                string label = "";
                feat.GetProduct().GetWhole().GetLabel(&label, CSeq_id::eContent);
                NStr::Tokenize(label, ".", tokens);
                
                if(tokens.size() == 4 && (tokens[3] == "m" || tokens[3] == "p")) {
                    int num1 = NStr::StringToInt(tokens[1]);
                    int num2 = NStr::StringToInt(tokens[2]); //make sure this one is a number too
                    num2 = 0;
                    return num1;
                }
            } catch (...) {}
        } 
    }

    return 0;    
}

    

/// Return the next group of comparisons on the region (return true iff found any)
/// A group is a set of features on the query region where each feature overlaps at least one other feature in the group.
/// (normally related gene, introns, exons, mRNAs and CDSes).
/// Comparison for each feature contains best match from the other sequence and their relationship.
/// (may be more than one best)   
/// If cannot find matching (overlapping) feature of the same type on the target, choose the one(s) of the semantically
/// closest type.
/// TODO: need to report features on target unaccounted for in the reported comparisons
bool CCompareSeqRegions::NextComparisonGroup(vector<CRef<CCompareFeats> >& vComparisons)
{
    vComparisons.clear();
    CRef<CSeq_loc> group_loc_q(new CSeq_loc);
    CRef<CSeq_loc> group_loc_t(new CSeq_loc);
    group_loc_q->SetNull();
    group_loc_t->SetNull();
    
    _TRACE("Starting next comparison group");
    
    for(; m_loc_q_ci; ++m_loc_q_ci) {
        CConstRef<CSeq_feat> feat1(&m_loc_q_ci->GetMappedFeature()); //original feat could be on segments, so self-mapper will happily remap to nothing
       
        //get raw matches for this feat
        vector<CRef<CCompareFeats> > feat_matches;
        x_GetPutativeMatches(feat_matches, feat1);
        
        //compute the combined location of the raw matches
        CSeq_loc aggregate_match_loc_t;
        aggregate_match_loc_t.SetNull();
        ITERATE(vector<CRef<CCompareFeats> >, it, feat_matches) {
            if(!(*it)->GetFeatT().IsNull() && !(*it)->GetSelfLocT().IsNull()) {
                aggregate_match_loc_t.SetMix();
                aggregate_match_loc_t.Add(*(*it)->GetSelfLocT());
            }
        } 
        
        
        if(!group_loc_q->IsNull() 
           && sequence::Compare(*group_loc_q, feat1->GetLocation(), m_scope_q) == sequence::eNoOverlap) 
        { 
            //the feature on query does not overlap anything in the current overlap group:
            //We might think that we've reached the next group; however, we want to keep
            //multiple non-overlapping features on Q matching the same larger feat on T in the same 
            //group to avoid the ambiguity in selecting best matches. Hence we also require
            //the same on the target side:
            
            if(group_loc_t->IsNull() || sequence::Compare(*group_loc_t, aggregate_match_loc_t, m_scope_t) == sequence::eNoOverlap) {
                break;    
            }  
        }
        
        vComparisons.insert(vComparisons.end(), feat_matches.begin(), feat_matches.end());
        
        //update group locs
        CConstRef<CSeq_loc> feat1_self_range_loc = x_GetSelfLoc(feat1->GetLocation(), m_scope_q, true);
        group_loc_q = sequence::Seq_loc_Add(*group_loc_q, *feat1_self_range_loc, CSeq_loc::fMerge_SingleRange, m_scope_q);
        group_loc_t = sequence::Seq_loc_Add(*group_loc_t, aggregate_match_loc_t, CSeq_loc::fMerge_SingleRange, m_scope_t);
        
        
#if _DEBUG
        string label = "";
        feature::GetLabel(*feat1, &label, feature::fFGL_Both, m_scope_q);
        _TRACE("  " + label);
        
        label = "";
        group_loc_q->GetLabel(&label);
        _TRACE("  group_loc_q:   " + label);
        
        label = "";
        group_loc_t->GetLabel(&label);
        _TRACE("  group_loc_t:   " + label);
#endif
        
    }
    
    if(m_comp_options & CCompareSeqRegions::fSelectBest) {SelectMatches(vComparisons);}
    
    if(!vComparisons.empty()) {
        return true;
    }

    if(m_already_processed_unmatched_targets) {
        return false;
    }
    
    //process unmatched targets
    double dummy(0.0f);
    CConstRef<CSeq_loc> tgt_loc = m_mapper->Map(*this->m_loc_q, &dummy);
    
    _TRACE("Processing unmatched targets");
    SAnnotSelector sel = m_selector_t; //because original is const
    sel.SetOverlapIntervals();
    for(CFeat_CI it2(*m_scope_t, *tgt_loc, sel); it2; ++it2) {
        CConstRef<CSeq_feat> feat(&it2->GetMappedFeature());
        string loc_label;
        feat->GetLocation().GetLabel(&loc_label);
        if(m_seen_targets.find(loc_label) != m_seen_targets.end()) {
            continue;
        }
        
        CConstRef<CSeq_loc> feat_self_loc = x_GetSelfLoc(feat->GetLocation(), m_scope_t, false);
        
        //compute the remapped ratio
        CRef<CSeq_loc> subtr_loc = sequence::Seq_loc_Subtract(*feat_self_loc, *tgt_loc, CSeq_loc::fMerge_All, m_scope_t);
      
        TSeqPos len_subtr = subtr_loc->Which() == CSeq_loc::e_not_set 
                         || subtr_loc->IsNull() 
                         || subtr_loc->IsEmpty() ? 0 : sequence::GetLength(*subtr_loc, m_scope_t);
        
        TSeqPos feat_len = feat_self_loc->Which() == CSeq_loc::e_not_set 
                         || feat_self_loc->IsNull() 
                         || feat_self_loc->IsEmpty() ? 0 : sequence::GetLength(*feat_self_loc, m_scope_t);
        
        double mapped = feat_len == 0 ? 0.0f : 1.0 - (len_subtr / feat_len);
        
        
        CRef<CCompareFeats> cf(new CCompareFeats(
                *feat
              , *feat_self_loc
              , mapped
              , m_scope_t
            ));
        vComparisons.push_back(cf);
    
    }
    m_already_processed_unmatched_targets = true;
    
    _TRACE("Finished processing this group");
    return !vComparisons.empty();
}


void CCompareSeqRegions::x_GetPutativeMatches(
        vector<CRef<CCompareFeats> >& vComparisons, 
        CConstRef<CSeq_feat> feat1)
{
    double mapped_identity(0);
    CConstRef<CSeq_loc> feat1_mapped_loc = m_mapper->Map(feat1->GetLocation(), &mapped_identity);
    CConstRef<CSeq_loc> feat1_mapped_range_loc = sequence::Seq_loc_Merge(*feat1_mapped_loc, CSeq_loc::fMerge_SingleRange, m_scope_t);
    CConstRef<CSeq_loc> feat1_self_loc = x_GetSelfLoc(feat1->GetLocation(), m_scope_q, false);
    CConstRef<CSeq_loc> feat1_self_range_loc = x_GetSelfLoc(feat1->GetLocation(), m_scope_q, true);
    
    _ASSERT(!feat1_mapped_loc.IsNull());
    _ASSERT(!feat1_mapped_range_loc.IsNull());
    _ASSERT(!feat1_self_loc.IsNull());
    _ASSERT(!feat1_self_range_loc.IsNull());
    

    
    
    int feat1_gene_id = s_GetGeneId(*feat1);
    
#if 0
    if(feat1_gene_id == 0) {
        ERR_POST(Info << "Unable to determine gene_id for");
        NcbiCerr << MSerial_AsnText << *feat1;
    }
#endif
    
    bool had_some_matches = false;
    for(CFeat_CI it2(*m_scope_t, *feat1_mapped_loc, m_selector_t); it2; ++it2) {
        CConstRef<CSeq_feat> feat_t(&it2->GetMappedFeature());
        
        if((m_comp_options & CCompareSeqRegions::fDifferentGenesOnly) 
           && feat1_gene_id == s_GetGeneId(*feat_t)) 
        {
            continue;
        }
        
        string loc_label = "";
        feat_t->GetLocation().GetLabel(&loc_label);
        this->m_seen_targets.insert(loc_label);
        
        
        if(m_comp_options & CCompareSeqRegions::fSameTypeOnly
           && feat_t->GetData().Which() != feat1->GetData().Which())
        {
            continue;
        }
        

        bool usingRangeComparison = feat_t->GetData().GetSubtype() == CSeqFeatData::eSubtype_gene
                                  && feat1->GetData().GetSubtype() == CSeqFeatData::eSubtype_gene;
        
        CConstRef<CSeq_loc> feat_t_self_loc = x_GetSelfLoc(feat_t->GetLocation(), m_scope_t, usingRangeComparison);
        CRef<CCompareFeats> cf(new CCompareFeats(
            *feat1
          , usingRangeComparison ? *feat1_mapped_range_loc : *feat1_mapped_loc
          , mapped_identity
          , usingRangeComparison ? *feat1_self_range_loc : *feat1_self_loc
          , m_scope_q
          , *feat_t
          , *feat_t_self_loc
          , m_scope_t));
        
        
        vComparisons.push_back(cf);
        had_some_matches = true;

    }
    
    if(!had_some_matches) {
        string s = "";
        feature::GetLabel(*feat1, &s, feature::fFGL_Both);
        
        CRef<CCompareFeats> cf(new CCompareFeats(
                *feat1
              , *feat1_mapped_loc
              , mapped_identity
              , *feat1_self_loc
              , m_scope_q));
        
        _ASSERT(!cf->GetSelfLocQ().IsNull());
        _ASSERT(!cf->GetMappedLocQ().IsNull());
        vComparisons.push_back(cf);
    }

}

CConstRef<CSeq_loc> CCompareSeqRegions::x_GetSelfLoc(
        const CSeq_loc& loc, 
        CScope* scope,
        bool merge_single_range)
{
    CRef<CSeq_loc> new_loc;
    
    if(!sequence::IsOneBioseq(loc, scope)) {
        CSeq_loc_Mapper& mapper = (scope == m_scope_q ? *m_self_mapper_q : *m_self_mapper_t);
        new_loc = mapper.Map(loc);
    }
    
    if(merge_single_range){
        new_loc = sequence::Seq_loc_Merge(
                (new_loc.IsNull() ? loc : *new_loc), 
                CSeq_loc::fMerge_SingleRange,
                scope);
    }

    return new_loc.IsNull() ? CConstRef<CSeq_loc>(&loc) : new_loc;
}



struct SFeats_OpLess : public binary_function<CConstRef<CSeq_feat>, CConstRef<CSeq_feat>, bool>
{
public:
    bool operator()(CConstRef<CSeq_feat> f1, CConstRef<CSeq_feat> f2) const
    { 
        if(f1 == f2) {
            return false;
        }
        
        if(f1.IsNull() && !f2.IsNull()) {
            return true;
        }
        
        if(!f1.IsNull() && f2.IsNull()) {
            return false;
        }

        //compare by feat-ids, if have them
        CConstRef<CObject_id> obj_id1(f1->CanGetId() && f1->GetId().IsLocal() ? &f1->GetId().GetLocal() : NULL);
        CConstRef<CObject_id> obj_id2(f2->CanGetId() && f2->GetId().IsLocal() ? &f2->GetId().GetLocal() : NULL);
        if(obj_id1.IsNull() && !obj_id2.IsNull()) {
            return true;
        } else if(!obj_id1.IsNull() && obj_id2.IsNull()) {
            return false;
        } else if(!obj_id1.IsNull() && !obj_id2.IsNull()) {
            return *obj_id1 < *obj_id2;
        }


        //compare by locations
        try {
            int res = f1->Compare(*f2);
            
            if(res != 0) {
                return res < 0;
            }
        } catch (...) {
            //Compare fails on multi-seq-id features
        }

      
        //Compare does not always go all the way (or throws), so here we manually try to distinguish by products and labels
        //Potential problem: may not be transitive?

        string s1 = "";
        if(f1->CanGetProduct()) {
            f1->GetProduct().GetLabel(&s1);
        }

        string s2 = "";
        if(f2->CanGetProduct()) {
            f2->GetProduct().GetLabel(&s2);
        }
       
        if(s1 != s2) {
            return s1 < s2;
        } 

        s1 = "";
        feature::GetLabel(*f1, &s1, feature::fFGL_Both);
        
        s2 = "";
        feature::GetLabel(*f2, &s2, feature::fFGL_Both);
        
        return s1 < s2;
    }
};

void CCompareSeqRegions::SelectMatches(vector<CRef<CCompareFeats> >& vComparisons)
{
    typedef priority_queue<CRef<CCompareFeats>,
                           vector<CRef<CCompareFeats> >,
                           SCompareFeats_OpLess
                           > TMatchesQueue;
    
    //Note: used to use default key comparator that assumed that the same feats
    //have same addresses. However, that is not always the case, so 
    //we have to use custom content-based comparator, SFeats_OpLess
    typedef map<CConstRef<CSeq_feat>, TMatchesQueue, SFeats_OpLess > TMatchesMap;
    
    TMatchesMap q_map;
    TMatchesMap t_map;
    
    int i = 0;
    ITERATE(vector<CRef<CCompareFeats> >, it, vComparisons) {
        CRef<CCompareFeats> cf = *it;
        
        
        if(!cf->GetFeatQ().IsNull()) {
            q_map[cf->GetFeatQ()].push(cf);
        }
        
        if(!cf->GetFeatT().IsNull()) {
            t_map[cf->GetFeatT()].push(cf);
        }
        
        ++i;
    }
    
#if 0
    i = 0;
    ERR_POST(Info << "q->t");
    ITERATE(TMatchesMap, it, q_map) {
        const CConstRef<CSeq_feat> feat = it->first;
        const CConstRef<CCompareFeats> best_match = it->second.top();
        
        string s0 = "";
        feature::GetLabel(*feat, &s0, feature::fFGL_Both);
        
        string s1 = "";
        if(!best_match.IsNull() && best_match->IsMatch()) {
            feature::GetLabel(*best_match->GetFeatT(), &s1, feature::fFGL_Both);
        }
        ERR_POST(Info << "Best match for " << s0 << " : " << s1 << ", out of " << it->second.size());
    ++i;
    }
     
    i = 0;
    ERR_POST(Info << "t->q");
    ITERATE(TMatchesMap, it, t_map) {
        const CConstRef<CSeq_feat> feat = it->first;
        const CConstRef<CCompareFeats> best_match = it->second.top();
        
        string s0 = "";
        feature::GetLabel(*feat, &s0, feature::fFGL_Both);
        
        string s1 = "";
        if(!best_match.IsNull() && best_match->IsMatch()) {
            feature::GetLabel(*best_match->GetFeatQ(), &s1, feature::fFGL_Both);
        }
        ERR_POST(Info << "Best match for " << s0 << " : " << s1 << ", out of " << it->second.size());
    
    }
    ++i;
    ERR_POST(Info << "");
#endif
    
    set<CRef<CCompareFeats> > compset;

    ITERATE(vector<CRef<CCompareFeats> >, it, vComparisons) {
        CRef<CCompareFeats> cf = *it;
        if(q_map[cf->GetFeatQ()].top() == cf && t_map[cf->GetFeatT()].top() == cf) {
           cf->SetIrrelevance(0);
           compset.insert(cf);
        }
    }  
    
    ITERATE(vector<CRef<CCompareFeats> >, it, vComparisons) {
        CRef<CCompareFeats> cf = *it;
        if(compset.find(cf) == compset.end() && 
           (q_map[cf->GetFeatQ()].top() == cf || cf->GetFeatQ().IsNull())) 
        {
           cf->SetIrrelevance(1);
           compset.insert(cf);
        }
    } 
    
    ITERATE(vector<CRef<CCompareFeats> >, it, vComparisons) {
        CRef<CCompareFeats> cf = *it;
        if(compset.find(cf) == compset.end() && 
           (t_map[cf->GetFeatT()].top() == cf || cf->GetFeatT().IsNull()))
        {
           cf->SetIrrelevance(2);
           compset.insert(cf);
        }
    } 
    
    vComparisons.clear();
    ITERATE(set<CRef<CCompareFeats> >, it, compset) {
        
        //do not report non-matches if we want different genes only
        //this should really be filtered out earlier, in GetPutativeMatches but for some reason
        //that removes 99% of comparisons - need to look into it
        //cout << **it;
        //if((*it)->GetFeatT().IsNull() && (m_comp_options & CCompareSeqRegions::fDifferentGenesOnly)) {
        //    continue;
        // }
        vComparisons.push_back(*it);
    }
}
       
END_NCBI_SCOPE
