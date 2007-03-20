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
#include <algo/sequence/compare_feats.hpp>
#include <queue>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);




CNcbiOstream& operator<<(CNcbiOstream& out, const CCompareFeats& cf)
{
    if(!cf.m_feat1.IsNull()) {
        out << CCompareFeats::s_GetFeatLabel(*cf.m_feat1) << "\t";
        out << CCompareFeats::s_GetLocLabel(cf.m_feat1->GetLocation(), false) << "\t";
        out << CCompareFeats::s_GetLocLabel(*cf.m_feat1_mapped_loc, false) << "\t";
    } else {
        out << "\t\t\t";
    }
    
    if(!cf.m_feat2.IsNull()) {
        out << CCompareFeats::s_GetFeatLabel(*cf.m_feat2) << "\t";
        out << CCompareFeats::s_GetLocLabel(cf.m_feat2->GetLocation(), false) << "\t";
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
        out << cf.m_compare->GetRelativeOverlap() << "\t";
        out << cf.m_compare->GetSymmetricalOverlap();
    } else {
        out << "\t\t\t";
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
        //TODO: not sure if we need to flip
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

    ITERATE(vector<SIntervalComparisonResult>, it, m_IntComparisons) {
        SIntervalComparisonResult comp = *it;
        if(!grp.Add(comp)) {
            if(grp.IsValid()) {
                strm << sep << grp.ToString();
                sep = "  ";
            }
            grp.Reset(comp);   
        }  
    }

    strm << sep << grp.ToString() << "]";
    return  CNcbiOstrstreamToString(strm); 
}   



/// Compare two loc primitives
CCompareSeq_locs::FCompareLocs CCompareSeq_locs::x_CompareInts(const CSeq_loc& loc1, const CSeq_loc& loc2) const
{
    bool is5p_match = loc1.GetStart(eExtreme_Biological) == loc2.GetStart(eExtreme_Biological);
    bool is3p_match = loc1.GetStop(eExtreme_Biological) == loc2.GetStop(eExtreme_Biological);

    sequence::ECompare cmp = sequence::Compare(loc1, loc2, m_scope1); //TODO: will it be a problem if loc2 lives in another scope?
    
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
    // TODO: report this  somewhere? 
    CRef<CSeq_loc> loc1;
    CRef<CSeq_loc> loc2;

    try {
        const CSeq_id& seq_id1 = sequence::GetId(*m_loc1, m_scope1);
        const CSeq_id& seq_id2 = sequence::GetId(*m_loc2, m_scope2);
        this->m_sameBioseq = sequence::IsSameBioseq(seq_id1, seq_id2, m_scope1);
    } catch(...) { //GetId may throw if intervals refer to multiple bioseqs
        this->m_sameBioseq = false;   
    }
    
    if(false && this->m_sameBioseq) { //disabled sorting
        //this "merge" is actually just a sort
        loc1 = sequence::Seq_loc_Merge(*m_loc1, CSeq_loc::fSort, m_scope1);
        loc2 = sequence::Seq_loc_Merge(*m_loc2, CSeq_loc::fSort, m_scope2);
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
    
    
    //TODO: At least for now we don't compare things on different strands.
    //Note that even the locs may be of "same orientation", in general case one or more of the intervals
    //may be on opposite strand. This will be handled in int-wise comparisons
    if(!SameOrientation(sequence::GetStrand(*loc1, m_scope1), sequence::GetStrand(*loc2, m_scope2))) {
        m_sameStrand = false;   
        return;
    } else {
        m_sameStrand = true;
    }
    
    
    //TODO: to avoid issues if some interval points to another sequence or they are not sorted (as a result of 
    //mapping or otherwise), we compare
    //every interval against every other interval leading to O(exon_count1*exon_count2) complexity. This can be optimized
    //to run in O(exon_count1+exon_count2). For now treat we treat the premature optimization as evil.
    
    set<unsigned> loc2_reported_set;     //keep track of matched exons on loc2 so we can report the unmatched
    unsigned it1_exon_ordinal = 1;
    
    int adjust_for_strand = IsReverse(sequence::GetStrand(*loc1, m_scope1)) ? -1 : 1;
    
    //have to use eEmpty_Allow to conserve the numbering of exons in the presence of non-mapping intervals
    for(CSeq_loc_CI it1(*loc1, CSeq_loc_CI::eEmpty_Allow, CSeq_loc_CI::eOrder_Positional); 
       it1;
       ++it1, ++it1_exon_ordinal) 
    {
        unsigned it2_exon_ordinal = 1;
        bool loc1_found_Overlap = false;
        int it1_cmp_it2 = 0;
        
        
        
        for(CSeq_loc_CI it2(*loc2, CSeq_loc_CI::eEmpty_Allow, CSeq_loc_CI::eOrder_Positional); 
            it2; 
            ++it2, ++it2_exon_ordinal) 
        {
            FCompareLocs cmp_res = x_CompareInts(it1.GetSeq_loc(), it2.GetSeq_loc());
            try {
                it1_cmp_it2 = adjust_for_strand * it1.GetSeq_loc().Compare(it2.GetSeq_loc());
            } catch (...) {
                ; //reuse the last value
            }
            
            
            // if no overlap and the segment on the other loc hasn't been reported and we already passed it
            if((cmp_res == fCmp_Unknown || cmp_res == fCmp_NoOverlap) 
               && loc2_reported_set.find(it2_exon_ordinal) == loc2_reported_set.end() 
               && it1_cmp_it2 < 0) 
            {
                loc2_reported_set.insert(it2_exon_ordinal);
                
                SIntervalComparisonResult sRes(
                    0                   //no corresponding exon on loc1
                  , it2_exon_ordinal    //for this exon on loc2
                  , cmp_res);   
                  
                m_IntComparisons.push_back(sRes);   
                
            } else if (cmp_res != fCmp_Unknown && cmp_res != fCmp_NoOverlap) {
                //also check the matching of directions; if problem - report it as strand mismatch
                if(!SameOrientation(sequence::GetStrand(it1.GetSeq_loc(), m_scope1), sequence::GetStrand(it2.GetSeq_loc(), m_scope2))) {
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
    
    //TODO: subset/superset may need to be reported regardles off the result_flags setting?
    if(!result_flags) {
        sequence::ECompare cmp = sequence::Compare(*m_loc1, *m_loc2, m_scope1); 
    
        switch(cmp) {
            case sequence::eSame:       
                result_flags |= fCmp_Match;
                strm << "complete match; ";
            break;
                
            case sequence::eNoOverlap:  
                if(sequence::TestForOverlap(*this->m_loc1, *this->m_loc2, sequence::eOverlap_Simple)) {
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


/// Comparison functor for pqueue storing related comparisons
/// A comparison of the same subtypes (e.g. mRNA vs. mRNA as opposed to mRNA vs. CDS)
/// is always preferable; Otherwise a better-overlapping comparison is preferable.
struct SCompareFeats_OpLess : public binary_function<CRef<CCompareFeats>&, CRef<CCompareFeats>&, bool>
{
public:
    //TODO: this needs to be imroved such that if we don't have a matching feature
    //of the same type we pick semantically closest comparison: i.e. we want to
    //report exon vs. mRNA as opposed to exon vs. gene if possible.
    //But what if comparing to a different feature has better overlap and the 
    //feature of the same type is irrelevant?
    //There's definitely some room for improvement in prioritizing matches
    
    bool operator()(const CRef<CCompareFeats>& c1, const CRef<CCompareFeats>& c2) const
    {    
        //any match is better than no match
        if(c1->m_unmatched && !c2->m_unmatched) return true;
        if(!c1->m_unmatched && c2->m_unmatched) return false;
        
        //same subtype is better than different subtypes
        bool c1_sameSubtype = c1->IsSameSubtype();
        bool c2_sameSubtype = c2->IsSameSubtype();
        if(c1_sameSubtype && !c2_sameSubtype) return false;
        if(!c1_sameSubtype && c2_sameSubtype) return true;
        
        //smaller difference in TypeSortingOrder is better 
        unsigned c1_typeSortingOrderDiff = abs(c1->m_feat1->GetTypeSortingOrder() - c1->m_feat2->GetTypeSortingOrder());
        unsigned c2_typeSortingOrderDiff = abs(c2->m_feat1->GetTypeSortingOrder() - c2->m_feat2->GetTypeSortingOrder());
        if(c1_typeSortingOrderDiff < c2_typeSortingOrderDiff) return false;
        if(c1_typeSortingOrderDiff > c2_typeSortingOrderDiff) return true;
        
        
        double overlap1 = c1->GetAutoOverlap();
        double overlap2 = c2->GetAutoOverlap();
            
        //if overlaps are equal, fall back on symmetrical overlap
        if(overlap1 == overlap2 && !c1->m_unmatched && !c2->m_unmatched) {
             return c1->m_compare->GetSymmetricalOverlap() < c2->m_compare->GetSymmetricalOverlap();
        } else {
             return overlap1 < overlap2;
        }
        
    }
private:

};
    

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
    CSeq_loc aggregate_loc;
    aggregate_loc.SetEmpty();

    for(; m_loc1_ci; ++m_loc1_ci) {
    
        const CSeq_feat& feat1 = m_loc1_ci->GetMappedFeature();
        if(m_loc1_ci->GetFeatSubtype() == CSeqFeatData::eSubtype_STS) {
            continue;
            //don't want to deal with these at the moment (they are on one annotation but not the other) 
        }


        if(!aggregate_loc.IsEmpty() 
           && sequence::Compare(aggregate_loc, feat1.GetLocation(), m_scope1) == sequence::eNoOverlap) 
        {
            return !vComparisons.empty();
        }

        
        aggregate_loc.SetMix();
        aggregate_loc.Add(feat1.GetLocation());

        SAnnotSelector sel2;
        sel2.SetResolveMethod(SAnnotSelector::eResolve_All);
        sel2.SetAdaptiveDepth(true);
        //sel2.IncludeFeatSubtype(m_loc1_ci->GetFeatSubtype());

          
        CConstRef<CSeq_loc> feat1_mapped_loc = m_mapper->Map(feat1.GetLocation());


        priority_queue<CRef<CCompareFeats>
                     , vector<CRef<CCompareFeats> > 
                     , SCompareFeats_OpLess> pq;
        
        CRef<CSeq_loc> feat1_mapped_range = sequence::Seq_loc_Merge(*feat1_mapped_loc, CSeq_loc::fMerge_SingleRange, m_scope1); //TODO: which scope to use here? 
        for(CFeat_CI it2(*m_scope2, *feat1_mapped_range, sel2); it2; ++it2) {
            CRef<CCompareFeats> cf(new CCompareFeats(
                feat1
              , *feat1_mapped_loc
              , m_scope1
              , it2->GetMappedFeature()
              , m_scope2));
            pq.push(cf);
        }
        
       
       if(pq.empty()) {
            //create a no-match entry
            CRef<CCompareFeats> cf(new CCompareFeats(feat1, *feat1_mapped_loc, m_scope1));
            vComparisons.push_back(cf);
        } else {
            
            //report all best matches
            //Note that we may have 'best' overlap with multiple features on the target sequence
            //Do we want to report all of them? Probably not because we may have
            //features on this sequence of the same type that will have best match to them, and we
            //want to avoid the combinatorial mini-explosion. Hence we report all best matches
            //not solely based on score, but as determined by SCompareFeats_OpLess
            
            
            CRef<CCompareFeats> top_comparison = pq.top();
            vComparisons.push_back(top_comparison);
            pq.pop();
            
            SCompareFeats_OpLess opLess;
            while(!pq.empty()
                  && !opLess(top_comparison, pq.top()) 
                  && !opLess(pq.top(), top_comparison) )
            {
                vComparisons.push_back(pq.top());
                pq.pop();
            }
        }

    }
    
    return !vComparisons.empty();
}
       
END_NCBI_SCOPE
