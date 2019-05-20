#ifndef ALGO_ALIGN_UTIL_HITFILTER__HPP
#define ALGO_ALIGN_UTIL_HITFILTER__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description: Multiple-sequence alignment uniquification algorithms.
*
*/

#include <algo/align/util/hit_comparator.hpp>

#include <algorithm>
#include <numeric>
#include <vector>
#include <set>


/** @addtogroup AlgoAlignRoot
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////
// CHitCoverageAccumulator

template<class THit>
class CHitCoverageAccumulator
{
public:

    typedef CRef<THit>    THitRef;
    typedef typename THit::TCoord  TCoord;

    /// Create the object; specify the accumulation sequence
    ///
    /// @param where
    ///    Accumulation source sequence (0 = query, 1 = subject)
    CHitCoverageAccumulator(Uint1 where): 
        m_Where(where),  
        m_Finish(0)
    {
        if(m_Where == 0) {
            m_i1 = 0;
            m_i2 = 1;
        }
        else {
            m_i1 = 2;
            m_i2 = 3;
        }
    }

    /// Overloaded function call operator to be used with std::accumulate()
    ///
    /// @param iVal
    ///    Accumulation target
    /// @return
    ///    Updated accumulation target
    TCoord operator() (TCoord iVal, const THitRef& ph)
    {
        const TCoord box [] = { ph->GetQueryMin(), ph->GetQueryMax(),
                                ph->GetSubjMin(), ph->GetSubjMax()   };

        if(box[m_i1] > m_Finish || m_Finish == 0) {
            return iVal + (m_Finish = box[m_i2]) - box[m_i1] + 1;
        }
        else {
            TCoord Finish0 = m_Finish;
            return (box[m_i2] > Finish0)?
                (iVal + (m_Finish = box[m_i2]) - Finish0): iVal;
        }
    }

private:

    Uint1   m_Where;
    TCoord  m_Finish;
    Uint1   m_i1, m_i2;
};

/////////////////////////////////////////////
/////////////////////////////////////////////

// The multiple-sequence alignment uniquification algorithm.
// The algorithm ensures that every residue on each sequence is either aligned 
// uniquely, or not at all (unless the residue is covered alignments overlapping 
// by longer than retain_overlap).In the process of resolving ambiguities 
// in the input alignments, the algorithm starts from the highest-scoring hit.
// This is a greedy algorithm so no claim is made as of optimality of the overall 
// alignment.

template<class THit>
class CHitFilter: public CObject
{
public:

    typedef CRef<THit>              THitRef;
    typedef vector<THitRef>         THitRefs;
    typedef typename THit::TCoord   TCoord;

    /// Collect coverage for the range of hits on the specified source sequence
    ///
    /// @param where
    ///    Accumulation source sequence (0 = query, 1 = subject)
    /// @param from
    ///    Hit range start
    /// @param to
    ///    Hit range stop
    /// @return
    ///    The number of residues on the source sequences covered by at least one
    ///    alignment
    static  TCoord s_GetCoverage(Uint1 where, 
                                 typename THitRefs::const_iterator from,
                                 typename THitRefs::const_iterator to)
    {
        // since we need to sort, create and init a local vector
        THitRefs hitrefs (to - from);
        typedef typename THitRefs::const_iterator TCI; 
        typedef typename THitRefs::iterator TI; 
        TCI ii = from;
        TI jj = hitrefs.begin();
        while(ii != to) {
            *jj++ = *ii++;
        }
        
        // prepare a sorter object and sort
        typedef CHitComparator<THit> THitComparator;
        typename THitComparator::ESortCriterion sort_type (
            where == 0? 
            THitComparator::eQueryMin:
            THitComparator::eSubjMin);

        THitComparator sorter (sort_type);
        stable_sort(hitrefs.begin(), hitrefs.end(), sorter);
        
        // compute coverage
        return accumulate(hitrefs.begin(), hitrefs.end(), 
                          TCoord(0), 
                          CHitCoverageAccumulator<THit>(where));
    }

    /// Get sequence span for a set of alignments (hits).
    ///
    /// @param hitrefs
    ///    Source hits
    /// @param span
    ///    Destination array (0=query min, 1=query max, 2=subj min, 3=subj max)
    static void s_GetSpan(const THitRefs& hitrefs, TCoord span [4]) 
    {      
        span[0] = span[2] = kMax_UInt; 
        span[1] = span[3] = 0;
          
        for(typename THitRefs::const_iterator ii = hitrefs.begin(),
                ii_end = hitrefs.end(); ii != ii_end; ++ii) {
            
            TCoord x = (*ii)->GetQueryMin();
            if(span[0] > x) {
                span[0] = x;
            }
            x = (*ii)->GetSubjMin();
            if(span[2] > x) {
                span[2] = x;
            }
            x = (*ii)->GetQueryMax();
            if(span[1] < x) {
                span[1] = x;
            }
            x = (*ii)->GetSubjMax();
            if(span[3] < x) {
                span[3] = x;
            }
        }
    }

    static TCoord s_GetOverlap(TCoord L1, TCoord R1, TCoord L2, TCoord R2)
    {
        const TCoord minmax = min(R1, R2);
        const TCoord maxmin = max(L1, L2);
        const  int n = minmax - maxmin + 1;
        return n > 0? n: 0;
    }
    
    /// Multiple-sequences greedy alignment uniquification algorithm
    ///
    /// @param hri_begin
    ///    Input hit range start
    /// @param hri_end
    ///    Input hit range stop
    /// @param phits_new
    ///    An external hit vector to keep any extra hits created during the call
    /// @param min_hit_len
    ///    Minimum alignment length to keep
    /// @param min_hit_idty
    ///    Minimum alignment identity to keep
    /// @param margin
    ///    Speed/memory trade-off (0 will use max memory; >0 will slow down a bit but 
    //     reduce memory requirements)
    /// @param retain_overlap
    ///    Minimum length of overlaps to keep (0 = no overlaps)

    enum EUnique_type {
        e_Strict,
        e_Query,
        e_Subject
    };
    
    static void s_RunGreedy(typename THitRefs::iterator hri_beg, 
                            typename THitRefs::iterator hri_end,
                            THitRefs* phits_new,
                            TCoord min_hit_len = 100,
                            double min_hit_idty = .9,
                            TCoord margin = 1,
                            TCoord retain_overlap = 0,
                            EUnique_type unique_type = e_Strict)
    {

        int max_idx = 2;
        Uint1 min_idx = 0;
        switch (unique_type) {
            case e_Strict:
                break;
            case e_Query:
                max_idx = 1;
                break;
            case e_Subject:
                min_idx = 1;
                break;
        }
                
        if(hri_beg > hri_end) {
            NCBI_THROW(CAlgoAlignUtilException, eInternal, 
                       "Invalid input iterator order");
        }

        const size_t dim0 = hri_end - hri_beg;
        THitRefs all;
        
        size_t dim_factor = 2;
        bool restart = true;
        
        vector<bool> del;

        while(restart) {
            
            restart = false;

            all.resize(dim0);
            copy(hri_beg, hri_end, all.begin());
            all.reserve(dim_factor * dim0);

            const typename THitRefs::iterator all_beg = all.begin();

            // compile ordered set of hit ends
            THitEnds hit_ends;
            {
                typename THitRefs::iterator ii = all_beg;
                for(size_t i = 0; i < dim0; ++i, ++ii) {

                    THitRef& h = *ii;
                    for(Uint1 point = 0; point < 4; ++point) {
                        THitEnd he;
                        he.m_Point = point;
                        he.m_Ptr = &h;
                        he.m_X = h->GetBox()[point];
                        hit_ends.insert(he);
                    }
                }
            }

            // Create additional markers for potential 'hot dogs'.
            // A hot dog is a higher-scoring hit within a lower scoring one.
            THitEnds hit_mids;
            typename THit::TId id(new typename THit::TId::element_type());

            typedef set<THitRef*> TOpenHits;
            TOpenHits open_hits;
            ITERATE(typename THitEnds, ii, hit_ends) {

                const Uint1 where = ii->m_Point/2;
                THitRef* hitrefptr = ii->m_Ptr;
                if((*hitrefptr)->GetId(where)->CompareOrdered(*id) != 0) {
                    // new id => reset
                    open_hits.clear();
                    open_hits.insert(hitrefptr);
                    id = (*hitrefptr)->GetId(where);
                }
                else {
                    auto iis = open_hits.find(hitrefptr);
                    if(iis == open_hits.end()) {
                        // new open
                        open_hits.insert(hitrefptr);
                    } else {
                        // hit closed => check for hot fogs
                        open_hits.erase(iis);
                        const float curscore = (*hitrefptr)->GetScore();
                        for (auto& open_hit: open_hits) {

                            sx_AddMidIfNeeded(hitrefptr, curscore, open_hit, where, margin, hit_mids);
                        }
                    }
                }
            }

            hit_ends.insert(hit_mids.begin(), hit_mids.end());

            vector<bool> skip (dim0, false);
            skip.reserve(dim_factor * dim0);
            del.assign(dim0, false);
            del.reserve(dim_factor * dim0);

            const THitRef* hitref_firstptr = &(*all_beg);
            size_t dim = dim0;
            while(restart == false) {

                // select the best hit
                double score_best = 0;
                typename THitRefs::iterator ii = all_beg, ii_best = all.end();
                for(size_t i = 0; i < dim; ++i, ++ii) {
                    if(skip[i] == false) {
                        const double score = (*ii)->GetScore();
                        if(score > score_best) {
                            ii_best = ii;
                            score_best = score;
                        }
                    }
                }

                if(ii_best == all.end()) {
                    break;
                }

                const THitRef& hc = *ii_best;

                // truncate overlaps with the current best hit

                for(Uint1 where = min_idx; where < max_idx; ++where) {

                    auto iters = sx_GetEndsInRange(hc, hc->GetBox(), where, margin, hit_ends);

                    const TCoord cmin = hc->GetMin(where);
                    const TCoord cmax = hc->GetMax(where);

                    for(auto ii_lcl=iters.first; ii_lcl != iters.second; ++ii_lcl) {
                    
                        const THitEnd& he_lcl = *ii_lcl;                   

                        const size_t hitrefidx = he_lcl.m_Ptr - hitref_firstptr;
                        const bool alive       = skip[hitrefidx] == false;
                        const bool self        = he_lcl.m_Ptr == &hc;

                        if(alive && !self) {
                            
                            TCoord saved_box[4];
                            const TCoord* box = (*he_lcl.m_Ptr)->GetBox();
                            std::copy(box, box+4, saved_box);
                            THitRef hit_new;
                            int newpos;
                            Uint1 newpoint;
                            tie(newpos, newpoint)
                                = sx_Cleave(*he_lcl.m_Ptr, he_lcl.m_Point/2, 
                                            cmin, cmax, min_hit_len, 
                                            min_hit_idty, & hit_new,
                                            retain_overlap);

                            if(newpos <= -2) { // eliminate
                                del[hitrefidx] = skip[hitrefidx] = true;
                            }

                            if(newpos >= 0) { // truncated or split


                                THitEnd he2;
                                he2.m_Point = newpoint; //he_lcl.m_Point;
                                he2.m_Ptr = he_lcl.m_Ptr;
                                he2.m_X = box[he2.m_Point];
                                hit_ends.insert(he2);

                                THitEnd he3;
                                he3.m_Point = (he2.m_Point ^ 2);
                                he3.m_Ptr = he_lcl.m_Ptr;
                                he3.m_X = box[he3.m_Point];
                                hit_ends.insert(he3);

                                sx_AddMidPoints(*he_lcl.m_Ptr, saved_box, margin, hit_ends, skip, hitref_firstptr);

                                if(hit_new.NotEmpty()) {
                                
                                    if(++dim > 2 * dim0) {
                                        dim_factor *= 2;
                                        restart = true;
                                        break;
                                    }

                                    all.push_back(hit_new);
                                    THitRef* ptr = & all.back();
                                    del.push_back(false);
                                    skip.push_back(false);

                                    box = hit_new->GetBox();

                                    Uint1 point = he2.m_Point ^ 1;

                                    // only two new ends to add
                                    THitEnd he2_lcl;
                                    he2_lcl.m_Point = point;
                                    he2_lcl.m_Ptr = ptr;
                                    he2_lcl.m_X = box[he2_lcl.m_Point];
                                    hit_ends.insert(he2_lcl);

                                    THitEnd he3_lcl;
                                    he3_lcl.m_Point = (he2_lcl.m_Point ^ 2);
                                    he3_lcl.m_Ptr = ptr;
                                    he3_lcl.m_X = box[he3_lcl.m_Point];
                                    hit_ends.insert(he3_lcl);

                                    sx_AddMidPoints(*ptr, saved_box, margin, hit_ends, skip, hitref_firstptr);

                                } // if(new_hit.NotEmpty())
                            } // if(newpos >= 0) 
                        } // if(alive && !self)
                    } // for(ii_lcl ...
                } // for(where ...

                skip[&hc - hitref_firstptr] = true;
            } // while(restart == false)
        } // while(restart)

        // execute any pending deletions
        typename THitRefs::iterator all_beg = all.begin(), ii = all_beg;
        const size_t dim = all.size();
        for(size_t i = 0; i < dim; ++i, ++ii) {
            if(del[i]) {
                ii->Reset(NULL);
            }
        }

        // copy hits to the results vector until the results are full or
        // there are no more hits to copy
        typename THitRefs::iterator idst = hri_beg, isrc = all_beg, 
            isrc_end = all.end();
        while(isrc != isrc_end && idst != hri_end) {
            if(isrc->NotEmpty()) {
                *idst++ = *isrc;
            }
            ++isrc;
        }

        // nullify any slack hits in the results
        while(idst != hri_end) {
            idst++ -> Reset(NULL);
        }

        // if there are hits remaining, write them as new
        phits_new->resize(0);
        while(isrc != isrc_end) {
            if(isrc->NotEmpty()) {
                phits_new->push_back(*isrc);
            }
            ++isrc;
        }
    }


    // merge hits abutting on query(subject) but separated on subject(query).
    // maxlenfr is the maximum allowed length of an alignment within a gap
    // as a fraction of the minimum length of two merging hits
    static void s_MergeAbutting(typename THitRefs::iterator hri_beg,
                                typename THitRefs::iterator hri_end,
                                const double& maxlenfr,
                                THitRefs* pout)
    {
        if(hri_beg > hri_end) {
            NCBI_THROW(CAlgoAlignUtilException, eInternal, 
                       "Invalid input iterator order");
        }

        // copy input hits and make all queries in plus strand
        const size_t dim0 = hri_end - hri_beg;
        THitRefs all;
        all.reserve(dim0);
        for(typename THitRefs::iterator hri = hri_beg; hri != hri_end; ++hri) {
            if((*hri)->GetQueryStrand() == false) {
                (*hri)->FlipStrands();
            }
            all.push_back(*hri);
        }

        // compile ordered set of hit ends;
        // go from the ends for better balancing
        typename THitRefs::iterator ii1 = all.begin();
        typename THitRefs::iterator ii2 = all.end(); --ii2;
        THitEnds hit_ends;
        while(ii1 != ii2) {
            
            THitRef& h1 = *ii1;
            THitRef& h2 = *ii2;
            for(Uint1 point = 0; point < 4; ++point) {

                THitEnd he1;
                he1.m_Point = point;
                he1.m_Ptr = &h1;
                he1.m_X = h1->GetBox()[point];
                hit_ends.insert(he1);

                THitEnd he2;
                he2.m_Point = point;
                he2.m_Ptr = &h2;
                he2.m_X = h2->GetBox()[point];
                hit_ends.insert(he2);
            }

            if(++ii1 == ii2) {
                --ii2;
                break;
            }
            --ii2;
        }

        if(ii1 == ii2) {
            THitRef& h = *ii2;
            for(Uint1 point = 0; point < 4; ++point) {
                THitEnd he;
                he.m_Point = point;
                he.m_Ptr = &h;
                he.m_X = h->GetBox()[point];
                hit_ends.insert(he);
            }
        }

        // collate by id pairs and strands
        typedef CHitComparator<THit> THitComparator;
        typename THitComparator::ESortCriterion sort_type
            (THitComparator::eQueryIdSubjIdSubjStrand);
        THitComparator sorter (sort_type);
        stable_sort(all.begin(), all.end(), sorter);
        
        // go chunk-by-chunk and merge whatever is suitable
        pout->clear();
        
        THitRef h0 (all.front());
        typename THitRefs::iterator ib = all.begin();
        NON_CONST_ITERATE(typename THitRefs, ii, all) {

            if ( ! (*ii)->GetQueryId()->Match(*(h0->GetQueryId())) ||
                 ! (*ii)->GetSubjId()->Match(*(h0->GetSubjId())) ||
                 (*ii)->GetSubjStrand() != h0->GetSubjStrand() )
            {
                h0 = *ii;
                sx_TestAndMerge(ib, ii, hit_ends, maxlenfr, pout);
                ib = ii;
            }
        }
        sx_TestAndMerge(ib, all.end(), hit_ends, maxlenfr, pout);
    }

    // helper predicate
    static bool s_PNullRef(const THitRef& hr) {
        return hr.IsNull();
    }

protected:

    struct SHitEnd {

        Uint1    m_Point; // 0 = query start, 1 = query stop, 2 = subj start, 3 = stop
        THitRef* m_Ptr;
        TCoord   m_X;

        bool operator < (const SHitEnd& rhs) const {
            
            const Uint1 where1 = m_Point < 2? 0: 1;
            const Uint1 where2 = rhs.m_Point < 2? 0: 1;
            const THit& h1 = **m_Ptr;
            const THit& h2 = **rhs.m_Ptr;
            TIntId c = h1.GetId(where1)->CompareOrdered(*h2.GetId(where2));
            return c < 0? true: (c > 0? false:
             (m_X == rhs.m_X? (h1.GetScore() < h2.GetScore()): m_X < rhs.m_X) );
        }

        friend ostream& operator << (ostream& ostr, const SHitEnd& he) {
            return 
                ostr << "(Point,Coord) = (" 
                     << int(he.m_Point) << ", " 
                     << he.m_X << ")\n"
                     << **he.m_Ptr;
        }
    };

    typedef SHitEnd THitEnd;
    typedef multiset<THitEnd> THitEnds;
    typedef typename THitEnds::iterator THitEndsIter;

    static void sx_AddMidIfNeeded(THitRef* hit, const float curscore,
                                  THitRef* container_candidate,
                                  const Uint1 where, TCoord margin, THitEnds& hit_mids) 
    {                           
        bool hd_score (((*(container_candidate))->GetScore() < curscore));
        // In large-scale alignments, the 'hot dogs' can be 
        // numerous and increase the size of hit_mids 
        // significantly. This is addressed by using the margin,
        // which reduces the number of mid-points stored,
        // but requires to scan through endpoints in a larger
        // vicinity of a current best hit.
        bool hd_xl (margin + (*(container_candidate))->GetMin(where)
                    < (*hit)->GetMin(where));
        bool hd_xr (margin + (*hit)->GetMax(where)
                    < (*(container_candidate))->GetMax(where));
        if(hd_score && hd_xl && hd_xr) {
                                
            THitEnd hm;
            hm.m_Point = where << 1;
            hm.m_Ptr = container_candidate;
            hm.m_X = ( (*hit)->GetStart(where) 
                       + (*hit)->GetStop(where) ) / 2;
            hit_mids.insert(hm);
        }
    }
    static pair<THitEndsIter, THitEndsIter> sx_GetEndsInRange(const THitRef& hc, const TCoord* box, Uint1 where, TCoord margin, THitEnds& hit_ends)
    {
        const bool is_constraint = hc->GetScore() > 1e20;
        THitEnd he [4];
        for(Uint1 point = 0; point < 4; ++point) {

            he[point].m_Point = point;
            he[point].m_Ptr = const_cast<THitRef*>(&hc);
            if(is_constraint == false) {
                he[point].m_X = box[point];
            } else {
                const TCoord a = 0, b = numeric_limits<TCoord>::max() / 2;
                const bool is_start = (point & 1) == 0;
                const bool is_plus  = hc->GetStrand(point / 2);
                he[point].m_X =  (is_start ^ is_plus)? b: a;
            }
        }

        THitEnd *phe_lo, *phe_hi;
        const Uint1 i1 = where << 1, i2 = i1 | 1;
        if(he[i1].m_X <= he[i2].m_X) {
            phe_lo = &he[i1];
            phe_hi = &he[i2];
        } else {
            phe_lo = &he[i2];
            phe_hi = &he[i1];
        }

        // we want all endpoints in [phe_lo->m_X - margin..phe_hi->m_X + margin]
        // but SHitEnd::"operator <" compares scores as well
        // and lower_bound(m_X - margin) will skip m_X - margin end with lower score
        // so we will seek for m_X - margin -1 and then skip all m_X - margin -1 ends

        auto prev_X = phe_lo->m_X;

        if(phe_lo->m_X >= margin +1) {
            phe_lo->m_X -= margin +1;
        } else {
            phe_lo->m_X = 0;
        }

        phe_hi->m_X += margin;

        THitEndsIter ii0 (hit_ends.lower_bound(*phe_lo));
        THitEndsIter ii1 (hit_ends.upper_bound(*phe_hi));

        while ( ii0 != ii1 && ii0->m_X+margin < prev_X ) ++ii0; // skip m_X - margin -1 ends

        // special case: if X is zero, go left as long as it's the same id
        if(phe_lo->m_X == 0) {
            THitRef hitref (*(ii0->m_Ptr));
            const typename THit::TId& id_lcl (hitref->GetId(ii0->m_Point/2));
            for(THitEndsIter ii_start(hit_ends.begin()); 
                ii0 != ii_start; --ii0) {
                THitRef hr (*(ii0->m_Ptr));
                const typename THit::TId & id2 (hr->GetId(ii0->m_Point/2));
                if(0 != id_lcl->CompareOrdered(*id2)) {
                    ++ii0;
                    break;
                }
            }
        }
        return make_pair(ii0, ii1);
    }
    static void sx_AddMidPoints(THitRef& h, const TCoord* outer_box, TCoord margin, THitEnds& hit_ends,
                                vector<bool>& skip, const THitRef* hitref_firstptr)
    {
        THitEnds hit_mids;
        
        const float score = h->GetScore();
        for (Uint1 where: {0, 1}) {
            auto iters = sx_GetEndsInRange(h, outer_box, where, margin, hit_ends);
            for (auto it = iters.first; it != iters.second; ++it) {
                const size_t hitrefidx = it->m_Ptr - hitref_firstptr;
                const bool alive       = skip[hitrefidx] == false;
                const bool self        = it->m_Ptr == &h;

                if(alive && !self) {
                    sx_AddMidIfNeeded(&h, score, it->m_Ptr, where, margin, hit_mids);
                    sx_AddMidIfNeeded(it->m_Ptr, (*it->m_Ptr)->GetScore(), &h, where, margin, hit_mids);
                }
            }
        }
        hit_ends.insert(hit_mids.begin(), hit_mids.end());
    }
    // return adjusted coordinate; -1 if unaffected; -2 to delete; -3 on exception;
    // 0 on split.
    static pair<int, Uint1> sx_Cleave(THitRef& h, Uint1 where, 
                         TCoord cmin, TCoord cmax, 
                         TCoord min_hit_len, double& min_idty,
                         THitRef* pnew_hit,
                         TCoord retain_overlap)
    {
        int rv = -1;
        Uint1 rp = 0;
        Uint1 reverse = 0;
        pnew_hit->Reset(NULL);

        try {
            TCoord lmin = h->GetMin(where), lmax = h->GetMax(where);

            if(retain_overlap > 0) {
                const TCoord overlap = s_GetOverlap(lmin, lmax, cmin, cmax);
                if(overlap >= retain_overlap) {
                    return make_pair(-1, 0);
                }
            }

            if(cmin <= lmin && lmax <= cmax) {
                return make_pair(-2, 0);
            }

            int ldif = cmin - lmin, rdif = lmax - cmax;
            reverse = h->GetStrand(where) ? 0 : 1;

            if(ldif > int(min_hit_len) && rdif > int(min_hit_len)) {
                
                // create a new hit
                pnew_hit->Reset(new THit (*h));
                (*pnew_hit)->Modify(2*where, cmax + 1);

                h->Modify(rp = 2*where + 1, lmax = cmin - 1);
                rv = lmax;

                if((*pnew_hit)->GetIdentity() < min_idty) {
                    pnew_hit->Reset(NULL);
                    return make_pair((h->GetIdentity() < min_idty)? -2: rv, rp ^ reverse);
                } 

                if (h->GetIdentity() < min_idty) {
                    *h = **pnew_hit;
                    return make_pair(cmax + 1, (2*where) ^ reverse);
                }
                return make_pair(0, rp ^ reverse);
            }
            else if(ldif > rdif) {

                if(lmin <= cmin && cmin <= lmax) {
                    if(cmin == 0) return make_pair(-2, 0);
                    h->Modify(rp = 2*where + 1, lmax = cmin - 1);
                    rv = lmax;
                }
                if(lmin <= cmax && cmax <= lmax) {
                    h->Modify(rp = 2*where, lmin = cmax + 1);
                    rv = lmin;
                }
            }
            else {

                if(lmin <= cmax && cmax <= lmax) {
                    h->Modify(rp = 2*where, lmin = cmax + 1);
                    rv = lmin;
                }
                if(lmin <= cmin && cmin <= lmax) {
                    if(cmin == 0) return make_pair(-2, 0);
                    h->Modify(rp = 2*where + 1, lmax = cmin - 1);
                    rv = lmax;
                }

            }

            if(1 + lmax - lmin < min_hit_len || h->GetIdentity() < min_idty) {
                return make_pair(-2, 0);
            }
        }
        catch(CAlgoAlignUtilException&) {
            rv = -3; // hit would become inconsistent through the adjustment
        }
        return make_pair(rv, rp ^ reverse);
    }

    
    static THitRef sx_Merge(const THitRef& lhs, const THitRef& rhs, Uint1 where)
    {
        THitRef rv (0);

        const bool sstrand (lhs->GetSubjStrand());
        if(sstrand != rhs->GetSubjStrand()) {

            NCBI_THROW(CAlgoAlignUtilException, eInternal, 
                       "Cannot merge hits: different strands");
        }

        const int qgap ((!sstrand && where == 1)? 
                        (int(lhs->GetQueryMin()) - int(rhs->GetQueryMax())):
                        (int(rhs->GetQueryMin()) - int(lhs->GetQueryMax())));

        const int sgap ((!sstrand && where == 0)?
                        (int(lhs->GetSubjMin()) - int(rhs->GetSubjMax())):
                        (int(rhs->GetSubjMin()) - int(lhs->GetSubjMax())));

        const bool bv1 (abs(qgap) == 1 && abs(sgap) > 0);
        const bool bv2 (abs(sgap) == 1 && abs(qgap) > 0);

        if(!bv1 && !bv2){
            return rv;
        }

        typename THit::TTranscript x_lhs = 
            THit::s_RunLengthDecode(lhs->GetTranscript());
        typename THit::TTranscript x_rhs = 
            THit::s_RunLengthDecode(rhs->GetTranscript());

        if(x_lhs.size() && x_rhs.size()) {

            string x_gap;
            if(where == 0) {
                x_gap.assign(abs(sgap) - 1, 'I');
            }
            else {
                x_gap.assign(abs(qgap) - 1, 'D');
            }

            string xcript;
            THitRef h;
            if(where == 0 || sstrand) {
                xcript = x_lhs + x_gap + x_rhs;
                h = lhs;
            }
            else {
                xcript = x_rhs + x_gap + x_lhs;
                h = rhs;
            }

            rv.Reset (new THit(h->GetId(0), h->GetStart(0), h->GetStrand(0), 
                               h->GetId(1), h->GetStart(1), h->GetStrand(1), 
                               xcript));
        }
        else {

            rv.Reset(new THit());
            rv->SetId(0, lhs->GetId(0));
            rv->SetId(1, lhs->GetId(1));
            TCoord box [4] = {
                lhs->GetQueryStart(), rhs->GetQueryStop(),
                lhs->GetSubjStart(), rhs->GetSubjStop()
            };
            rv->SetBox(box);
        }

        return rv;
    }

    
    static bool s_TrackHit(const THit& h)
    {
        const string xcript (THit::s_RunLengthDecode(h.GetTranscript()));
        if(xcript.size()) {

            TCoord q = h.GetQueryStart(), q0 = q, s = h.GetSubjStart(), s0 = s;

            const int qinc = h.GetQueryStrand()? 1 : -1;
            const int sinc = h.GetSubjStrand()? 1 : -1;

            ITERATE(string, ii, xcript) {
                switch(*ii) {
                case 'M' : case 'R' : q0 = q; q += qinc; s0 = s; s += sinc; break;
                case 'I' : s0 = s; s += sinc; break;
                case 'D' : q0 = q; q += qinc; break;
                default:
                    NCBI_THROW(CAlgoAlignUtilException, eInternal, 
                               "Invalid transcript symbol");
                }
            }
            
            const bool rv = (q0 == h.GetQueryStop() && s0 == h.GetSubjStop());
            return rv;
        }
        else {
            return true;
        }
    }


    static void sx_TM(Uint1 where,
                      typename THitRefs::iterator ii_beg,
                      typename THitRefs::iterator ii_end,
                      const THitEnds& hit_ends,
                      const double& maxlenfr,
                      map<typename THitRefs::iterator, THitRef>& m)
    {
        typedef CHitComparator<THit> THitComparator;
        typename THitComparator::ESortCriterion sort_type (
                 where == 0? 
                 THitComparator::eQueryMinScore:
                 THitComparator::eSubjMinScore);
        THitComparator sorter (sort_type);
        stable_sort(ii_beg, ii_end, sorter);

        typedef typename THitRefs::iterator TIter;
        typedef map<typename THitRefs::iterator, THitRef> TIterMap;
        for(TIter ii = ii_beg; ii != ii_end; ++ii)  {

            TIter jj = ii;
            const TCoord cmax = (*ii)->GetMax(where);
            TCoord cmin = 0;
            for(++jj; jj != ii_end && cmax >= cmin; ++jj) {
                cmin = (*jj)->GetMin(where);
            }
            
            if(cmax + 1 == cmin) {

                --jj;
                bool can_merge = true;
                const Uint1 where2 (where ^ 1);
                const bool subj_strand ((*ii)->GetSubjStrand());
                if(subj_strand) {
                    can_merge = (*ii)->GetMax(where2) < (*jj)->GetMin(where2);
                }
                else {
                    can_merge = (*jj)->GetMax(where2) < (*ii)->GetMin(where2);
                }
                
                if(can_merge) {

                    // also test the maxlenfr condition using hit_ends
                    
                    THitEnd he_ii, he_jj;

                    if(subj_strand) {

                        he_ii.m_Point = 2*where2 + 1;
                        he_ii.m_Ptr = &(*ii);
                        he_ii.m_X = (*ii)->GetStop(where2) + 1;

                        he_jj.m_Point = 2*where2;
                        he_jj.m_Ptr = &(*jj);
                        he_jj.m_X = (*jj)->GetStart(where2) - 1;
                    }
                    else {

                        he_jj.m_Point = 2*where2 + 1;
                        he_jj.m_Ptr = &(*ii);
                        he_jj.m_X = (*ii)->GetStop(where2) - 1;

                        he_ii.m_Point = 2*where2;
                        he_ii.m_Ptr = &(*jj);
                        he_ii.m_X = (*jj)->GetStart(where2) + 1;
                    }

                    const THitEndsIter ii0 = hit_ends.lower_bound(he_ii);
                    const THitEndsIter ii1 = hit_ends.upper_bound(he_jj);

                    const size_t max_len
                        = size_t(maxlenfr * min((*ii)->GetLength(),
                                                (*jj)->GetLength())
                                 + 0.5);

                    for(THitEndsIter ii_lcl = ii0; ii_lcl != ii1; ++ii_lcl) {

                        const THitRef& h = *(ii_lcl->m_Ptr);

                        TCoord cmin_lcl (h->GetMin(where2));
                        if(cmin_lcl < he_ii.m_X) cmin_lcl = he_ii.m_X;

                        TCoord cmax_lcl (h->GetMax(where2));
                        if(cmax_lcl < he_jj.m_X) cmax_lcl = he_jj.m_X;
                        
                        if(cmax_lcl - cmin_lcl + 1 > max_len) {
                            can_merge = false;
                            break;
                        }
                    }
                }

                if(can_merge) {
                
                    // if (*ii) already merged, merge with the result;
                    // if (*jj) already merged, do not merge.
                    typename TIterMap::iterator imi = m.find(ii);
                    THitRef lhs (imi == m.end()? *ii: imi->second);
                    if (m.find(jj) == m.end() && lhs.NotEmpty()) {

                        THitRef rv (sx_Merge(lhs, *jj, where));
                        if(rv.NotNull()) {
                            m[ii] = THitRef(0);
                            m[jj] = rv;
                        }
                    }                    
                }
            }
        }
    }


    // merging of abutting hits sharing same ids and subject strand
    // (plus query strand assumed)
    void static sx_TestAndMerge(typename THitRefs::iterator ii_beg,
                                typename THitRefs::iterator ii_end,
                                const THitEnds& hit_ends,
                                const double& maxlenfr,
                                THitRefs* pout)
    {
        typedef typename THitRefs::iterator TIter;
        typedef map<TIter, THitRef> TIterMap;
        TIterMap m;

        sx_TM(0, ii_beg, ii_end, hit_ends, maxlenfr, m);
        sx_TM(1, ii_beg, ii_end, hit_ends, maxlenfr, m);

        // copy final merge targets and unmerged hits
        for(TIter ii = ii_beg; ii != ii_end; ++ii) {
            typename TIterMap::iterator imi = m.find(ii);
            THitRef hr (imi == m.end()? *ii: imi->second);
            if(hr.NotEmpty()) {
                pout->push_back(hr);
            }
        }
    }

};


END_NCBI_SCOPE


/* @} */

#endif /* ALGO_ALIGN_UTIL_HITFILTER__HPP */
