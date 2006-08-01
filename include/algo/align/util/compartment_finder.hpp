#ifndef ALGO_ALIGN_UTIL_COMPARTMENT_FINDER__HPP
#define ALGO_ALIGN_UTIL_COMPARTMENT_FINDER__HPP

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
* File Description:  CCompartmentFinder- identification of genomic compartments
*                   
* ===========================================================================
*/


#include <corelib/ncbi_limits.hpp>

#include <algo/align/nw/align_exception.hpp>
#include <algo/align/util/hit_filter.hpp>

#include <algorithm>
#include <numeric>
#include <vector>


BEGIN_NCBI_SCOPE


// Detect all compartments over a set of hits
template<class THit>
class CCompartmentFinder {

public:

    typedef CRef<THit>            THitRef;
    typedef vector<THitRef>       THitRefs;
    typedef typename THit::TCoord TCoord;

    // hits must be in plus strand
    CCompartmentFinder(typename THitRefs::const_iterator start,
                       typename THitRefs::const_iterator finish);

    // setters and getters
    void SetMaxIntron (TCoord intr_max) {
        m_intron_max = intr_max;
    }

    static TCoord s_GetDefaultMaxIntron(void) {
        return 1048575;
    }
    
    void SetPenalty(TCoord penalty) {
        m_penalty = penalty;
    }
        
    void SetMinMatches(TCoord min_matches) {
        m_MinSingletonMatches = m_MinMatches = min_matches;
    }
        
    void SetMinSingletonMatches(TCoord min_matches) {
        m_MinSingletonMatches = min_matches;
    }

    static TCoord GetDefaultPenalty(void) {
        return 500;
    }
    
    static TCoord GetDefaultMinCoverage(void) {
        return 500;
    }


    size_t Run(void); // do the compartment search

    // order compartments by lower subj coordinate
    void OrderCompartments(void);

    // single compartment representation
    class CCompartment {

    public:
        
        CCompartment(void) {
            m_coverage = 0;
            m_box[0] = m_box[2] = numeric_limits<TCoord>::max();
            m_box[1] = m_box[3] = 0;
        }

        const THitRefs& GetMembers(void) {
            return m_members;
        }
    
        void AddMember(const THitRef& hitref) {
            m_members.push_back(hitref);
        }

        void SetMembers(const THitRefs& hits) {
            m_members = hits;
        }
    
        void UpdateMinMax(void);

        bool GetStrand(void) const;

        const THitRef GetFirst(void) const {
            m_iter = 0;
            return GetNext();
        }
        
        const THitRef GetNext(void) const {
            if(m_iter < m_members.size()) {
                return m_members[m_iter++];
            }
            return THitRef(NULL);
        }
        
        const TCoord* GetBox(void) const {
            return m_box;
        }
        
        bool operator < (const CCompartment& rhs) {
            return m_coverage < rhs.m_coverage;
        }
        
        static bool s_PLowerSubj(const CCompartment& c1,
                                 const CCompartment& c2) {

            return c1.m_box[2] < c2.m_box[2];
        }
        
    protected:
        
        TCoord              m_coverage;
        THitRefs            m_members;
        TCoord              m_box[4];
        mutable size_t      m_iter;
    };

    // iterate through compartments
    CCompartment* GetFirst(void);
    CCompartment* GetNext(void);

private:

    TCoord                m_intron_max;    // max intron size
    TCoord                m_penalty;       // penalty per compartment
    TCoord                m_MinMatches;    // min approx matches to report
    TCoord                m_MinSingletonMatches; // min matches for singleton comps

    THitRefs              m_hitrefs;         // input hits
    vector<CCompartment>  m_compartments;    // final compartments
    int                   m_iter;            // GetFirst/Next index

    // this structure describes the best target reached 
    // at a given query coordinate
    struct SQueryMark {

        TCoord       m_coord;
        double       m_score;
        int          m_hit;
        TCoord       m_LeftBound;
        
        SQueryMark(TCoord coord, double score, int hit, TCoord left_bnd):
            m_coord(coord), 
            m_score(score), 
            m_hit(hit), 
            m_LeftBound(left_bnd) 
        {}
        
        bool operator < (const SQueryMark& rhs) const {
            return m_coord < rhs.m_coord;
        }
    };
    
    struct SHitStatus {
        
        enum EType {
            eNone, 
            eExtension,
            eOpening
        };
        
        EType m_type;
        int   m_prev;
        
        SHitStatus(void): m_type(eNone), m_prev(-1) {}
        SHitStatus(EType type, int prev): m_type(type), m_prev(prev) {}
        
    };    
};


// Facilitate access to compartments over a hit set
template<class THit>
class CCompartmentAccessor
{
public:

    typedef CCompartmentFinder<THit> TCompartmentFinder;
    typedef typename TCompartmentFinder::THitRefs THitRefs;
    typedef typename TCompartmentFinder::TCoord   TCoord;

    // [start,finish) are assumed to share same query and subj
    CCompartmentAccessor(typename THitRefs::iterator start, 
                         typename THitRefs::iterator finish,
                         TCoord comp_penalty_bps,
                         TCoord min_matches,
                         TCoord min_singleton_matches =numeric_limits<TCoord>::max());
    
    bool GetFirst(THitRefs& compartment);
    bool GetNext(THitRefs& compartment);
    
    size_t GetCount(void) const {
        return m_pending.size();
    }
    
    void Get(size_t i, THitRefs& compartment) const {
        compartment = m_pending[i];
    }
    
    const TCoord* GetBox(size_t i) const {
        return &m_ranges.front() + i*4;
    }
    
    bool GetStrand(size_t i) const {
        return m_strands[i];
    }
        
private:
    
    vector<THitRefs>         m_pending;
    vector<TCoord>           m_ranges;
    vector<bool>             m_strands;
    size_t                   m_iter;
        
    void x_Copy2Pending(TCompartmentFinder& finder);
};



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


const double kPenaltyPerIntronBase = -1e-7; // a small penalty to prefer
                                            // more compact models
                                            // among equal
template<class THit>
void CCompartmentFinder<THit>::CCompartment::UpdateMinMax() 
{
    typedef CHitFilter<THit> THitFilter;
    THitFilter::s_GetSpan(m_members, m_box);
}


template<class THit>
bool CCompartmentFinder<THit>::CCompartment::GetStrand() const {

    if(m_members.size()) {
        return m_members.front()->GetSubjStrand();
    }
    else {
        NCBI_THROW( CAlgoAlignException,
                    eInternal,
                    "Strand requested on an empty compartment" );
    }
}


template<class THit>
CCompartmentFinder<THit>::CCompartmentFinder(
    typename THitRefs::const_iterator start,
    typename THitRefs::const_iterator finish ):

    m_intron_max(s_GetDefaultMaxIntron()),
    m_penalty(GetDefaultPenalty()),
    m_MinMatches(1),
    m_MinSingletonMatches(1),
    m_iter(-1)
{
    m_hitrefs.resize(finish - start);
    copy(start, finish, m_hitrefs.begin());
}


// accumulate matches on query
template<class THit>
class CQueryMatchAccumulator:
    public binary_function<float, CRef<THit>, float>
{
public:

    CQueryMatchAccumulator(void):
        m_Finish(-1.0f)
    {}

    float operator() (float acm, CRef<THit> ph)
    {
        typename THit::TCoord qmin = ph->GetQueryMin(), 
            qmax = ph->GetQueryMax();
        if(qmin > m_Finish)
            return acm + ph->GetIdentity() * ((m_Finish = qmax) - qmin + 1);
        else {
            if(qmax > m_Finish) {
                float finish0 = m_Finish;
                return acm + ph->GetIdentity() * ((m_Finish = qmax) - finish0);
            }
            else {
                return acm;
            }
        }
    }

private:

    float m_Finish;
};


template<class THit>
double GetTotalMatches(
    const typename CCompartmentFinder<THit>::THitRefs& hitrefs0)
{
    typename CCompartmentFinder<THit>::THitRefs hitrefs (hitrefs0);   

    typedef CHitComparator<THit> THitComparator;
    THitComparator sorter (THitComparator::eQueryMin);
    stable_sort(hitrefs.begin(), hitrefs.end(), sorter);

    const float rv = accumulate(hitrefs.begin(), hitrefs.end(), 0.0f, 
                                CQueryMatchAccumulator<THit>());
    return rv;
}


template<class THit>
size_t CCompartmentFinder<THit>::Run()
{
    const TCoord kMax_TCoord = numeric_limits<TCoord>::max();
    const double kMinusInf = -1e12;

    m_compartments.clear();
    const size_t dimhits = m_hitrefs.size();
    if(dimhits == 0) {
        return 0;
    }
    
    // sort here to make sure that later at every step
    // all potential sources (hits from where to continue)
    // are visible

    typedef CHitComparator<THit> THitComparator;
    THitComparator sorter (THitComparator::eSubjMaxQueryMax);
    stable_sort(m_hitrefs.begin(), m_hitrefs.end(), sorter);
    
    // insert dummy element
    list<SQueryMark> qmarks; // ordered list of query marks
    qmarks.clear();
    qmarks.push_back(SQueryMark(0, 0, -1, kMax_TCoord));
    const typename list<SQueryMark>::iterator li_b = qmarks.begin();
    
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
    //   save the hit's status (previous hit, extension or opening)
    //
    
    vector<SHitStatus> hitstatus (dimhits);

    for(size_t i = 0; i < dimhits; ++i) {
        
        const typename list<SQueryMark>::iterator li_e = qmarks.end();
        
        const THitRef& h = m_hitrefs[i];
        const typename THit::TCoord hbox [4] = {
            h->GetQueryMin(),
            h->GetQueryMax(),
            h->GetSubjMin(),
            h->GetSubjMax()
        };

//#define CF_DBG_TRACE
#ifdef CF_DBG_TRACE
        cerr << "***" << *h << endl;
#endif
        typename list<SQueryMark>::iterator li0 
            = lower_bound(li_b, li_e, SQueryMark(hbox[1], 0, -2, kMax_TCoord));
        bool qm_new = (li0 == li_e)? true: (hbox[1] < li0->m_coord);

        double best_ext_score = kMinusInf;
        typename list<SQueryMark>::iterator li_best_ext = li_b;
        double best_open_score = kMinusInf;
        typename list<SQueryMark>::iterator li_best_open = li_b;

        double intron_penalty_be = 0.0;
        
        for(typename list<SQueryMark>::iterator li = li_b; li != li_e; ++li) {
            
            THitRef phc;
            typename THit::TCoord phcbox[4];
            if(li->m_hit != -1) {

                phc = m_hitrefs[li->m_hit];
                phcbox[0] = phc->GetQueryMin();
                phcbox[1] = phc->GetQueryMax();
                phcbox[2] = phc->GetSubjMin();
                phcbox[3] = phc->GetSubjMax();
#ifdef CF_DBG_TRACE
                cerr << '\t' << *phc << endl;
#endif
            }
#ifdef CF_DBG_TRACE
            else {
                cerr << "\t[Dummy]" << endl;
            }
#endif
            
            // check if good for extension
            {{
                typename THit::TCoord q0, s0; // possible continuation
                bool good = false;
                int subj_space;
                if(li->m_hit >= 0) {

                    if(phcbox[1] < hbox[1]) {

                        if(hbox[0] <= phcbox[1] && 
                           hbox[2] + phcbox[1] - hbox[0] >= phcbox[3]) {

                            q0 = phcbox[1] + 1;
                            s0 = hbox[2] + phcbox[1] - hbox[0];
                        }
                        else if(phcbox[3] >= hbox[2]) {

                            s0 = phcbox[3] + 1;
                            q0 = hbox[1] - (hbox[3] - s0);
                        }
                        else {
                            
                            q0 = hbox[0];
                            s0 = hbox[2];
                        }
                        subj_space = s0 - phcbox[3] - 1;

                        const TCoord max_gap = 50; // max run of spaces
                                                   // inside an exon
                        good = subj_space <= int(m_intron_max)
                            && subj_space + max_gap >= q0 - phcbox[1] - 1;
                    }
                }
                
                if(good) {
                    
                    const double identity = h->GetIdentity();
                    const double li_score = li->m_score;
                    const double intron_penalty = subj_space > 0?
                        subj_space * kPenaltyPerIntronBase: 0.0;

                    const double ext_score = li_score +
                        identity * (hbox[1] - q0 + 1);

                    if(ext_score + intron_penalty > 
                       best_ext_score + intron_penalty_be)
                    {
                        best_ext_score = ext_score;
                        li_best_ext = li;
                        intron_penalty_be = intron_penalty;
                    }
#ifdef CF_DBG_TRACE
                    cerr << "\tGood for extension with score = " 
                         << ext_score << endl;
#endif
                }
            }}
            
            // check if good for closing and opening
            if(li->m_hit == -1 || hbox[2] > hbox[0] + phcbox[3]) {

                const double identity = h->GetIdentity();
                const double li_score = li->m_score;
                double score_open = li_score - m_penalty +
                    identity * (hbox[1] - hbox[0] + 1);
                if(score_open > best_open_score) {
                    best_open_score = score_open;
                    li_best_open = li;
                }
#ifdef CF_DBG_TRACE
                    cerr << "\tGood for opening with score = " 
                         << score_open << endl;
#endif
            }
        } // li
        
        typename SHitStatus::EType hit_type;
        int prev_hit;
        double best_score;
        TCoord lft_bnd;
        if(best_ext_score > best_open_score) {

            hit_type = SHitStatus::eExtension;
            prev_hit = li_best_ext->m_hit;
            best_score = best_ext_score;
            lft_bnd = li_best_ext->m_LeftBound;
        }
        else {

            hit_type = SHitStatus::eOpening;
            prev_hit = li_best_open->m_hit;
            best_score = best_open_score;
            lft_bnd = hbox[2];
        }
        
        bool updated = false;
        if(qm_new) {
            qmarks.insert(li0, SQueryMark(hbox[1], best_score, i, lft_bnd));
            updated = true;
        }
        else {
            
            const double dif = best_score - li0->m_score;
            if(dif >= 0.0) { // kludge around optimizer bug - compare the difference
                li0->m_score = best_score;
                li0->m_hit = i;
                updated = true;
                li0->m_LeftBound = lft_bnd;
            }
        }
        
        if(updated) {
            hitstatus[i].m_type = hit_type;
            hitstatus[i].m_prev = prev_hit;
        }
    }
    
#ifdef CF_DBG_TRACE
    cerr << "\n\n--- BACKTRACE ---\n";
#endif

    // *** backtrace ***
    // - find query mark with the highest score
    // - trace it back until the dummy
    typename list<SQueryMark>::iterator li_best = li_b;
    ++li_best;
    double score_best = kMinusInf;
    for(typename list<SQueryMark>::iterator li = li_best, li_e = qmarks.end();
        li != li_e; ++li) {
        if(li->m_score > score_best) {
            score_best = li->m_score;
            li_best = li;
        }
    }
    
    const double min_matches = m_MinSingletonMatches < m_MinMatches? 
        m_MinSingletonMatches: m_MinMatches;
    if(score_best + m_penalty >= min_matches) {

        int i = li_best->m_hit;
        bool new_compartment = true;
        THitRefs hitrefs;
        while(i != -1) {

            if(new_compartment) {
                float mp (GetTotalMatches<THit>(hitrefs));
                if(mp >= m_MinMatches) {
                    m_compartments.push_back(CCompartment());
                    m_compartments.back().SetMembers(hitrefs);
                }
                hitrefs.resize(0);
                new_compartment = false;
            }
            hitrefs.push_back(m_hitrefs[i]);

#ifdef CF_DBG_TRACE
            cerr << *(m_hitrefs[i]) << endl;
#endif            
            
            if(hitstatus[i].m_type == SHitStatus::eOpening) {
                new_compartment = true;
            }
            i = hitstatus[i].m_prev;
        }

        float mp (GetTotalMatches<THit>(hitrefs));
        if(m_compartments.size() == 0 && mp >= m_MinSingletonMatches 
           || mp >= m_MinMatches) 
        {
            m_compartments.push_back(CCompartment());
            m_compartments.back().SetMembers(hitrefs);
        }
    }
    
    return m_compartments.size();
}


template<class THit>
typename CCompartmentFinder<THit>::CCompartment* 
CCompartmentFinder<THit>::GetFirst()
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

template<class THit>
void CCompartmentFinder<THit>::OrderCompartments(void) {
  
    for(int i = 0, dim = m_compartments.size(); i < dim; ++i) {
        m_compartments[i].UpdateMinMax();
    }
    
    stable_sort(m_compartments.begin(), m_compartments.end(), 
                CCompartmentFinder<THit>::CCompartment::s_PLowerSubj);
}

template<class THit>
typename CCompartmentFinder<THit>::CCompartment* 
CCompartmentFinder<THit>::GetNext()
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


template<class THit>
CCompartmentAccessor<THit>::CCompartmentAccessor(
     typename THitRefs::iterator istart,
     typename THitRefs::iterator ifinish,
     TCoord comp_penalty,
     TCoord min_matches,
     TCoord min_singleton_matches)
{
    const TCoord kMax_TCoord = numeric_limits<TCoord>::max();

    const TCoord max_intron = CCompartmentFinder<THit>::s_GetDefaultMaxIntron();

    // separate strands for CompartmentFinder
    typename THitRefs::iterator ib = istart, ie = ifinish, ii = ib, 
        iplus_beg = ie;
    typedef CHitComparator<THit> THitComparator;
    THitComparator sorter (THitComparator::eSubjStrand);
    stable_sort(ib, ie, sorter);

    TCoord minus_subj_min = kMax_TCoord, minus_subj_max = 0;
    for(ii = ib; ii != ie; ++ii) {
        if((*ii)->GetSubjStrand()) {
            iplus_beg = ii;
            break;
        }
        else {
            if((*ii)->GetSubjMin() < minus_subj_min) {
                minus_subj_min = (*ii)->GetSubjMin();
            }
            if((*ii)->GetSubjMax() > minus_subj_max) {
                minus_subj_max = (*ii)->GetSubjMax();
            }
        }
    }
    
    // minus
    {{
        // flip
        for(ii = ib; ii != iplus_beg; ++ii) {

            const typename THit::TCoord s0 = minus_subj_min + minus_subj_max 
                - (*ii)->GetSubjMax();
            const typename THit::TCoord s1 = minus_subj_min + minus_subj_max 
                - (*ii)->GetSubjMin();
            (*ii)->SetSubjStart(s0);
            (*ii)->SetSubjStop(s1);
        }
        
        CCompartmentFinder<THit> finder (ib, iplus_beg);
        finder.SetPenalty(comp_penalty);
        finder.SetMinMatches(min_matches);
        finder.SetMinSingletonMatches(min_singleton_matches);
        finder.SetMaxIntron(max_intron);
        finder.Run();
        
        // un-flip
        for(ii = ib; ii != iplus_beg; ++ii) {

            const typename THit::TCoord s0 = minus_subj_min + minus_subj_max 
                - (*ii)->GetSubjMax();
            const typename THit::TCoord s1 = minus_subj_min + minus_subj_max 
                - (*ii)->GetSubjMin();
            (*ii)->SetSubjStart(s1);
            (*ii)->SetSubjStop(s0);
        }
        
        x_Copy2Pending(finder);
    }}

    // plus
    {{
        CCompartmentFinder<THit> finder (iplus_beg, ie);
        finder.SetPenalty(comp_penalty);
        finder.SetMinMatches(min_matches);
        finder.SetMinSingletonMatches(min_singleton_matches);
        finder.SetMaxIntron(max_intron);
        finder.Run();
        x_Copy2Pending(finder);
    }}
}


template<class THit>
void CCompartmentAccessor<THit>::x_Copy2Pending(
    CCompartmentFinder<THit>& finder)
{
    finder.OrderCompartments();

    typedef typename CCompartmentFinder<THit>::THitRef THitRef;

    // copy to pending
    for(typename CCompartmentFinder<THit>::CCompartment* compartment =
            finder.GetFirst();  compartment; compartment = finder.GetNext()) {
        
        m_pending.push_back(THitRefs(0));
        THitRefs& vh = m_pending.back();
        
        for(THitRef ph = compartment->GetFirst(); ph; 
            ph = compartment->GetNext()) {
            
            vh.push_back(ph);
        }
        
        const TCoord* box = compartment->GetBox();
        m_ranges.push_back(box[0] - 1);
        m_ranges.push_back(box[1] - 1);
        m_ranges.push_back(box[2] - 1);
        m_ranges.push_back(box[3] - 1);
        
        m_strands.push_back(compartment->GetStrand());
    }
}


template<class THit>
bool CCompartmentAccessor<THit>::GetFirst(THitRefs& compartment) {

    m_iter = 0;
    return GetNext(compartment);
}


template<class THit>
bool CCompartmentAccessor<THit>::GetNext(THitRefs& compartment) {

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
 * Revision 1.10  2006/08/01 15:06:10  kapustin
 * Max intron length up to 1M
 *
 * Revision 1.9  2006/06/27 14:23:58  kapustin
 * Cosmetics
 *
 * Revision 1.8  2006/04/12 16:28:25  kapustin
 * Use intron penalities only when discirminating between extension candidates
 *
 * Revision 1.7  2006/03/21 16:16:12  kapustin
 * +max_singleton_matches parameter
 *
 *
 * Revision 1.5  2005/10/04 19:33:53  kapustin
 * Limit min distance btw compartments by unaligned term query space only.
 *
 * Revision 1.4  2005/09/21 14:14:16  kapustin
 * Fix the problem with initial (dummy) score. Replace size_t => TCoord.
 *
 * Revision 1.3  2005/09/13 15:56:15  kapustin
 * kMax* => numeric_limits<>
 *
 * Revision 1.2  2005/09/12 20:15:16  ucko
 * Use TCoord rather than size_t for m_box for consistency with THitFilter.
 *
 * Revision 1.1  2005/09/12 16:21:34  kapustin
 * Add compartmentization algorithm
 *
 * ==========================================================================
 */

#endif
