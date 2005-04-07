/* $Id$
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
* Author:  Yuri Kapustin, Alexander Souvorov
*
* File Description: Compartment Finder
*                   
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <corelib/ncbi_limits.hpp>
#include <algo/align/splign/splign_compartment_finder.hpp>
#include <algo/align/nw/align_exception.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

const double kPenaltyPerIntronBase = -1e-3; // a small penalty to prefer
                                            // more compact models
                                            // among equal

void CCompartmentFinder::CCompartment::UpdateMinMax() {
    
    m_box[0] = m_box[2] = kMax_UInt;
    m_box[1] = m_box[3] = 0;
    ITERATE(THitConstPtrs, ph, m_members) {
        
        const CHit* hit = *ph;
        if(size_t(hit->m_ai[0]) < m_box[0]) {
            m_box[0] = hit->m_ai[0];
        }
        if(size_t(hit->m_ai[2]) < m_box[2]) {
            m_box[2] = hit->m_ai[2];
        }
        if(size_t(hit->m_ai[1]) > m_box[1]) {
            m_box[1] = hit->m_ai[1];
        }
        if(size_t(hit->m_ai[3]) > m_box[3]) {
            m_box[3] = hit->m_ai[3];
        }
    }
}


bool CCompartmentFinder::CCompartment::GetStrand() const {

    if(m_members.size()) {
        return m_members[0]->IsStraight();
    }
    else {
        NCBI_THROW( CAlgoAlignException,
                    eInternal,
                    "Strand requested on an empty compartment" );
    }
}


CCompartmentFinder::CCompartmentFinder(THits::const_iterator start,
                                       THits::const_iterator finish):
    m_intron_max(GetDefaultMaxIntron()),
    m_penalty(GetDefaultPenalty()),
    m_min_coverage(GetDefaultMinCoverage()),
    m_iter(-1)
{
    const size_t dim = finish - start;
    if(dim >  0) {
        m_hits.resize(dim);
        const CHit* p0 = &(*start);
        for(size_t i = 0; i < dim; ++i) {
            m_hits[i] = p0++;
        }
    }
}


#ifdef _SPL_CF_V0_

size_t CCompartmentFinder::Run()
{
    m_compartments.clear();
    const size_t dimhits = m_hits.size();
    if(dimhits == 0) {
        return 0;
    }
    
    // sort here to make sure that later at every step
    // all potential sources (hits from where to continue)
    // are visible
    sort(m_hits.begin(), m_hits.end(), CHit::PSubjLow_QueryLow_ptr);
    
    // insert dummy element
    list<SQueryMark> qmarks; // ordered list of query marks
    qmarks.clear();
    qmarks.push_back(SQueryMark(0, 0, -1));
    const list<SQueryMark>::iterator li_b = qmarks.begin();
    
    // *** Generic hit iteration (not yet quite optimized) ***
    // For every hit:
    // - find out if its query mark already exists (qm_new)
    // - for query mark below the current:
    //   -- check if it could be extended
    //   -- evaluate extension potential
    // - for every other qm:
    //   -- check if its compartment could be terminated
    //   -- evaluate potential of terminating the compartment
    //      to open a new one
    // - select the best potential
    // - if qm_new, create the new query mark, otherwise
    //   update the existing one if the new score is higher
    // - if query mark was created or updated, 
    //   save the hit's status ( previous hit, extension or opening)
    //
    
    vector<SHitStatus> hitstatus (dimhits);
    
    for(size_t i = 0; i < dimhits; ++i) {
        
        const list<SQueryMark>::iterator li_e = qmarks.end();
        
        const CHit& h = *m_hits[i];

        list<SQueryMark>::iterator li0 = lower_bound(li_b, li_e,
                                             SQueryMark(h.m_ai[1], 0, -2));
        bool qm_new = (li0 == li_e)? true: (size_t(h.m_ai[1]) < li0->m_coord);

        int best_ext_score = kMin_Int;
        list<SQueryMark>::iterator li_best_ext = li_b;
        int best_open_score = kMin_Int;
        list<SQueryMark>::iterator li_best_open = li_b;
        
        for( list<SQueryMark>::iterator li = li_b; li != li_e; ++li) {
            
            const CHit* phc = (li->m_hit == -1)? 0: m_hits[li->m_hit];

            // check if good for extention
            if(li->m_hit < int(i)) {

                size_t q0 = h.m_ai[0], s0 = h.m_ai[2]; // possible continuation
                bool good;
                int intron;
                if(li->m_hit == -1) {
                    good = false;
                }
                else {
                    if(phc->m_ai[1] >= h.m_ai[1]) {
                        good = false;
                    }
                    else {
                        if(phc->m_ai[1] < h.m_ai[0]) {
                            q0 = h.m_ai[0];
                            s0 = h.m_ai[2];
                        }
                        else {
                            // find where this point would be on h
                            q0 = phc->m_ai[1] + 1;
                            s0 += q0 - h.m_ai[0];
                        }
                        intron = int(s0) - phc->m_ai[3] - 1;
                        good = intron <= int(m_intron_max);
                    }          
                }
                
                if(good) {
                    
                    int intron_penalty = intron > 0?
                        int(intron*kPenaltyPerIntronBase): 0;

                    int ext_score = li->m_score +
                        int(0.01 * h.m_Idnty * (h.m_ai[1] - q0 + 1)) +
                        intron_penalty;

                    if(ext_score > best_ext_score) {
                        best_ext_score = ext_score;
                        li_best_ext = li;
                    }
                }
            }
            
            // check if good for closing and opening
            if(li->m_hit == -1 ||  phc->m_ai[3] < h.m_ai[2]) {
                int score_open = li->m_score - m_penalty +
                    int(0.01*h.m_Idnty*(h.m_ai[1] - h.m_ai[0] + 1));
                if(score_open > best_open_score) {
                    best_open_score = score_open;
                    li_best_open = li;
                }
            }
        }

        SHitStatus::EType hit_type;
        int prev_hit;
        int best_score;
        if(best_ext_score > best_open_score) {
            hit_type = SHitStatus::eExtension;
            prev_hit = li_best_ext->m_hit;
            best_score = best_ext_score;
        }
        else {
            hit_type = SHitStatus::eOpening;
            prev_hit = li_best_open->m_hit;
            best_score = best_open_score;
        }
        
        bool updated = false;
        if(qm_new) {
            qmarks.insert(li0, SQueryMark(h.m_ai[1], best_score, i));
            updated = true;
        }
        else {
            if(best_score >= li0->m_score) {
                li0->m_score = best_score;
                li0->m_hit = i;
                updated = true;
            }
        }
        
        if(updated) {
            hitstatus[i].m_type = hit_type;
            hitstatus[i].m_prev = prev_hit;
        }
    }
    
    // *** backtrace ***
    // - find query mark with the highest score
    // - trace it back until the dummy
    list<SQueryMark>::iterator li_best = li_b;
    ++li_best;
    int score_best = kMin_Int;
    for(list<SQueryMark>::iterator li = li_best, li_e = qmarks.end();
        li != li_e; ++li) {
        if(li->m_score > score_best) {
            score_best = li->m_score;
            li_best = li;
        }
    }
    
    if( int(score_best + m_penalty) >= int(m_min_coverage) ) {
        int i = li_best->m_hit;
        CCompartment* pc = 0;
        bool new_compartment = true;
        while(i != -1) {
            if(new_compartment) {
                new_compartment = false;
                m_compartments.push_back(CCompartment());
                pc = &m_compartments.back();
            }
            pc->AddMember(m_hits[i]);
            
            if(hitstatus[i].m_type == SHitStatus::eOpening) {
                new_compartment = true;
            }
            i = hitstatus[i].m_prev;
        }
    }
    
    return m_compartments.size();
}

#else

size_t CCompartmentFinder::Run()
{
    m_compartments.clear();
    const size_t dimhits = m_hits.size();
    if(dimhits == 0) {
        return 0;
    }
    
    // sort here to make sure that later at every step
    // all potential sources (hits from where to continue)
    // are visible
    sort(m_hits.begin(), m_hits.end(), CHit::PSubjHigh_QueryHigh_ptr);
    
    // insert dummy element
    list<SQueryMark> qmarks; // ordered list of query marks
    qmarks.clear();
    qmarks.push_back(SQueryMark(0, 0, -1));
    const list<SQueryMark>::iterator li_b = qmarks.begin();
    
    // For every hit:
    // - find out if its query mark already exists (qm_new)
    // - for query mark below the current:
    //   -- check if it could be extended
    //   -- evaluate extension potential
    // - for every other qm:
    //   -- check if its compartment could be terminated
    //   -- evaluate potential of terminating the compartment
    //      to open a new one
    // - select the best potential
    // - if qm_new, create the new query mark, otherwise
    //   update the existing one if the new score is higher
    // - if query mark was created or updated, 
    //   save the hit's status ( previous hit, extension or opening)
    //
    
    vector<SHitStatus> hitstatus (dimhits);

    for(size_t i = 0; i < dimhits; ++i) {
        
        const list<SQueryMark>::iterator li_e = qmarks.end();
        
        const CHit& h = *m_hits[i];

        list<SQueryMark>::iterator li0 
            = lower_bound(li_b, li_e, SQueryMark(h.m_ai[1], 0, -2));
        bool qm_new = (li0 == li_e)? true: (size_t(h.m_ai[1]) < li0->m_coord);

        int best_ext_score = kMin_Int;
        list<SQueryMark>::iterator li_best_ext = li_b;
        int best_open_score = kMin_Int;
        list<SQueryMark>::iterator li_best_open = li_b;
        
        for( list<SQueryMark>::iterator li = li_b; li != li_e; ++li) {
            
            const CHit* phc = (li->m_hit == -1)? 0: m_hits[li->m_hit];
            
            // check if good for extention
            {{
                int q0, s0; // possible continuation
                bool good = false;
                int intron;
                if(li->m_hit >= 0) {

                    if(phc->m_ai[1] < h.m_ai[1]) {

                        if(h.m_ai[0] <= phc->m_ai[1] && 
                           h.m_ai[2] + phc->m_ai[1] -h.m_ai[0] >= phc->m_ai[3])
                        {

                            q0 = phc->m_ai[1] + 1;
                            s0 = h.m_ai[3] - (h.m_ai[1] - q0);
                            intron = phc->m_ai[3] < s0? s0 - phc->m_ai[3]: 0;
                        }
                        else if(phc->m_ai[3] >= h.m_ai[2]) {

                            s0 = phc->m_ai[3] + 1;
                            q0 = h.m_ai[1] - (h.m_ai[3] - s0);
                            intron = 0;
                        }
                        else {
                            
                            q0 = h.m_ai[0];
                            s0 = h.m_ai[2];
                            intron = s0 - phc->m_ai[3] - 1;
                        }

                        good = intron <= int(m_intron_max);
                    }
                }
                
                if(good) {
                    
                    int intron_penalty = intron > 0?
                        int(intron*kPenaltyPerIntronBase): 0;

                    int ext_score = li->m_score +
                        int(h.m_ai[1] - q0 + 1) +
                        intron_penalty;

                    if(ext_score > best_ext_score) {
                        best_ext_score = ext_score;
                        li_best_ext = li;
                    }
                }
            }}
            
            // check if good for closing and opening
            if(li->m_hit == -1 ||  phc->m_ai[3] < h.m_ai[2]) {
                int score_open = li->m_score - m_penalty +
                    int(h.m_ai[1] - h.m_ai[0] + 1);
                if(score_open > best_open_score) {
                    best_open_score = score_open;
                    li_best_open = li;
                }
            }
        }

        SHitStatus::EType hit_type;
        int prev_hit;
        int best_score;
        if(best_ext_score > best_open_score) {
            hit_type = SHitStatus::eExtension;
            prev_hit = li_best_ext->m_hit;
            best_score = best_ext_score;
        }
        else {
            hit_type = SHitStatus::eOpening;
            prev_hit = li_best_open->m_hit;
            best_score = best_open_score;
        }
        
        bool updated = false;
        if(qm_new) {
            qmarks.insert(li0, SQueryMark(h.m_ai[1], best_score, i));
            updated = true;
        }
        else {
            if(best_score >= li0->m_score) {
                li0->m_score = best_score;
                li0->m_hit = i;
                updated = true;
            }
        }
        
        if(updated) {
            hitstatus[i].m_type = hit_type;
            hitstatus[i].m_prev = prev_hit;
        }
    }
    
    // *** backtrace ***
    // - find query mark with the highest score
    // - trace it back until the dummy
    list<SQueryMark>::iterator li_best = li_b;
    ++li_best;
    int score_best = kMin_Int;
    for(list<SQueryMark>::iterator li = li_best, li_e = qmarks.end();
        li != li_e; ++li) {
        if(li->m_score > score_best) {
            score_best = li->m_score;
            li_best = li;
        }
    }
    
    if( int(score_best + m_penalty) >= int(m_min_coverage) ) {
        int i = li_best->m_hit;
        CCompartment* pc = 0;
        bool new_compartment = true;
        while(i != -1) {
            if(new_compartment) {
                new_compartment = false;
                m_compartments.push_back(CCompartment());
                pc = &m_compartments.back();
            }
            pc->AddMember(m_hits[i]);
            
            if(hitstatus[i].m_type == SHitStatus::eOpening) {
                new_compartment = true;
            }
            i = hitstatus[i].m_prev;
        }
    }
    
    return m_compartments.size();
}


#endif




CCompartmentFinder::CCompartment* CCompartmentFinder::GetFirst()
{
    if(m_compartments.size()) {
        m_iter = 0;
        return &m_compartments[m_iter++];
    }
    else {
        m_iter = -1;
        return 0;
    }
}


void CCompartmentFinder::OrderCompartments(void) {
  
    for(int i = 0, dim = m_compartments.size(); i < dim; ++i) {
        m_compartments[i].UpdateMinMax();
    }
    
    sort(m_compartments.begin(), m_compartments.end(), PLowerSubj);
}


CCompartmentFinder::CCompartment* CCompartmentFinder::GetNext()
{
    const size_t dim = m_compartments.size();
    if(m_iter == -1 || m_iter >= int(dim)) {
        m_iter = -1;
        return 0;
    }
    else {
        return &m_compartments[m_iter++];
    }
}


CCompartmentAccessor::CCompartmentAccessor(THits::iterator istart,
                                           THits::iterator ifinish,
                                           size_t comp_penalty,
                                           size_t min_coverage)
{
    // separate strands for CompartmentFinder
    THits::iterator ib = istart, ie = ifinish, ii = ib, iplus_beg = ie;
    sort(ib, ie, CHit::PPrecedeStrand);
    size_t minus_subj_min = kMax_UInt, minus_subj_max = 0;
    for(ii = ib; ii != ie; ++ii) {
        if(ii->IsStraight()) {
            iplus_beg = ii;
            break;
        }
        else {
            if(size_t(ii->m_ai[2]) < minus_subj_min) {
                minus_subj_min = ii->m_ai[2];
            }
            if(size_t(ii->m_ai[3]) > minus_subj_max) {
                minus_subj_max = ii->m_ai[3];
            }
        }
    }
    
    // minus
    {{
        // flip
        for(ii = ib; ii != iplus_beg; ++ii) {
            int s0 = minus_subj_min + minus_subj_max - ii->m_ai[3];
            int s1 = minus_subj_min + minus_subj_max - ii->m_ai[2];
            ii->m_an[2] = ii->m_ai[2] = s0;
            ii->m_an[3] = ii->m_ai[3] = s1;
        }
        
        CCompartmentFinder finder (ib, iplus_beg);
        finder.SetMinCoverage(min_coverage);
        finder.SetPenalty(comp_penalty);
        finder.Run();
        
        // un-flip
        for(ii = ib; ii != iplus_beg; ++ii) {
            int s0 = minus_subj_min + minus_subj_max - ii->m_ai[3];
            int s1 = minus_subj_min + minus_subj_max - ii->m_ai[2];
            ii->m_an[3] = ii->m_ai[2] = s0;
            ii->m_an[2] = ii->m_ai[3] = s1;
        }
        
        x_Copy2Pending(finder);
    }}

    // plus
    {{
        CCompartmentFinder finder (iplus_beg, ie);
        finder.SetMinCoverage(min_coverage);
        finder.SetPenalty(comp_penalty);
        finder.Run();
        x_Copy2Pending(finder);
    }}
}


void CCompartmentAccessor::x_Copy2Pending(CCompartmentFinder& finder)
{
    finder.OrderCompartments();

    // copy to pending
    for(CCompartmentFinder::CCompartment* compartment = finder.GetFirst();
        compartment;
        compartment = finder.GetNext()) {
        
        m_pending.push_back(THits(0));
        THits& vh = m_pending.back();
        
        for(const CHit* ph = compartment->GetFirst();
            ph; ph = compartment->GetNext()) {
            
            vh.push_back(*ph);
        }
        
        const size_t* box = compartment->GetBox();
        m_ranges.push_back(box[0] - 1);
        m_ranges.push_back(box[1] - 1);
        m_ranges.push_back(box[2] - 1);
        m_ranges.push_back(box[3] - 1);
        
        m_strands.push_back(compartment->GetStrand());
    }
}


bool CCompartmentAccessor::GetFirst(THits& compartment) {

    m_iter = 0;
    return GetNext(compartment);
}


bool CCompartmentAccessor::GetNext(THits& compartment) {

    compartment.clear();
    if(m_iter < m_pending.size()) {
        compartment = m_pending[m_iter++];
        return true;
    }
    else {
        return false;
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/04/07 18:50:06  kapustin
 * Run(): change ordering predicate, use raw query hit length when computing target
 *
 * Revision 1.10  2004/12/16 23:12:26  kapustin
 * algo/align rearrangement
 *
 * Revision 1.9  2004/12/06 22:13:36  kapustin
 * File header update
 *
 * Revision 1.8  2004/10/07 13:42:03  kapustin
 * Introduced intron length penalty and let the latest hit to own a query mark if there are several hits with exactly same score and query coordinate
 *
 * Revision 1.7  2004/09/27 17:12:38  kapustin
 * Move splign_compartment_finder.hpp to /include/algo/align/splign. SetIntronLimits() => SetMaxIntron()
 *
 * Revision 1.6  2004/06/03 19:29:07  kapustin
 * Minor fixes
 *
 * Revision 1.5  2004/05/24 16:13:57  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2004/04/23 14:37:44  kapustin
 * *** empty log message ***
 *
 * ==========================================================================
 */
