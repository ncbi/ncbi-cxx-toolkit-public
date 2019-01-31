#ifndef ALGO_ALIGN_UTIL_HITCOMPARATOR__HPP
#define ALGO_ALIGN_UTIL_HITCOMPARATOR__HPP

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
* File Description:
*
*/

#include <corelib/ncbiobj.hpp>
#include <algo/align/util/algo_align_util_exceptions.hpp>

BEGIN_NCBI_SCOPE


template<class THit>
class CHitComparator: public CObject
{
public:

    typedef CRef<THit>   THitRef;

    // sorting criteria
    enum ESortCriterion {
        eQueryMin,
        eQueryMinQueryMax,
        eSubjMin,
        eSubjMinSubjMax,
        eQueryMinScore,
        eSubjMinScore,
        eSubjMaxQueryMax,
        eQueryId,
        eSubjId,
        eSubjIdQueryId,
        eSubjStrand,
        eQueryIdSubjIdSubjStrand
    };

    CHitComparator(ESortCriterion sort_type): m_SortType (sort_type) {}

    bool operator() (const THitRef& lhs, const THitRef& rhs) const;

private:

    ESortCriterion m_SortType;
};


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


template<class THit>
bool CHitComparator<THit>::operator() (const THitRef& lhs, 
                                       const THitRef& rhs) const 
{
    bool rv;

    auto& ah = *lhs;
    auto& bh = *rhs;
    auto a_box = ah.GetBox();
    auto b_box = bh.GetBox();


    // add same extra fields to compare for stability
#define CMP2(lhs1, rhs1, lhs2, rhs2)\
                     make_tuple(lhs1, lhs2, a_box[0], a_box[1], a_box[2], a_box[3], \
                            ah.GetIdentity(), ah.GetScore() \
                            ) < \
                     make_tuple(rhs1, rhs2, b_box[0], b_box[1], b_box[2], b_box[3], \
                            bh.GetIdentity(), bh.GetScore() \
                            )
#define CMP1(lhs, rhs) \
                     CMP2( lhs, rhs, 0, 0)


    switch(m_SortType) {

    case eQueryMin:

        rv = CMP1(lhs->GetQueryMin(), rhs->GetQueryMin());

        break;

    case eQueryMinQueryMax:
        {
            const typename THit::TCoord qmin_lhs (lhs->GetQueryMin());
            const typename THit::TCoord qmin_rhs (rhs->GetQueryMin());

            rv = CMP2(qmin_lhs, qmin_rhs, lhs->GetQueryMax(), rhs->GetQueryMax());
        }
        break;

    case eSubjMin:

        rv = CMP1(lhs->GetSubjMin(), rhs->GetSubjMin());
        break;

    case eSubjMinSubjMax:
        {
            const typename THit::TCoord smin_lhs = lhs->GetSubjMin();
            const typename THit::TCoord smin_rhs = rhs->GetSubjMin();

            rv = CMP2(smin_lhs, smin_rhs, lhs->GetSubjMax(), rhs->GetSubjMax());
        }
        break;
        
    case eQueryMinScore:
        {
            const typename THit::TCoord qmin_lhs = lhs->GetQueryMin();
            const typename THit::TCoord qmin_rhs = rhs->GetQueryMin();

            rv = CMP2(qmin_lhs, qmin_rhs, -lhs->GetScore(), -rhs->GetScore());
        }
        break;
        
    case eSubjMinScore:
        {
            const typename THit::TCoord smin_lhs = lhs->GetSubjMin();
            const typename THit::TCoord smin_rhs = rhs->GetSubjMin();
            rv = CMP2(smin_lhs, smin_rhs, -lhs->GetScore(), -rhs->GetScore());
        }
        break;


    case eSubjMaxQueryMax: 
        {
            const typename THit::TCoord lhs_subj_max = lhs->GetSubjMax();
            const typename THit::TCoord rhs_subj_max = rhs->GetSubjMax();
            
            rv = CMP2(lhs_subj_max, rhs_subj_max, lhs->GetQueryMax(), rhs->GetQueryMax());
        }
        break;
        
    case eQueryId:
        {
            const TIntId co = lhs->GetQueryId()->CompareOrdered( *(rhs->GetQueryId()) );
            rv =  CMP1(co, 0);
        }
        break;

    case eSubjId:
        {
            const TIntId co = lhs->GetSubjId()->CompareOrdered( *(rhs->GetSubjId()) );
            rv = CMP1(co, 0);
        }
        break;
        
    case eSubjIdQueryId: 
        {
            const TIntId co  = lhs->GetSubjId ()->CompareOrdered( *(rhs->GetSubjId ()) );
            const TIntId co2 = lhs->GetQueryId()->CompareOrdered( *(rhs->GetQueryId()) );
            rv = CMP2(co, 0, co2, 0);
        }
        break;
        
    case eSubjStrand:
        
        rv = CMP1(lhs->GetSubjStrand(), rhs->GetSubjStrand());
        break;
        
        
    case eQueryIdSubjIdSubjStrand:
        {
            const TIntId qid = lhs->GetQueryId()->CompareOrdered(*(rhs->GetQueryId()));

            if (qid == 0) {
                const TIntId sid = lhs->GetSubjId()->CompareOrdered(*(rhs->GetSubjId()));
                const int subj_strand_lhs = lhs->GetSubjStrand();
                const int subj_strand_rhs = rhs->GetSubjStrand();
                rv = CMP2(sid, 0, -subj_strand_lhs, -subj_strand_rhs);
            }
            else {
                rv = qid < 0;
            }
        }
        break;

    default:

        NCBI_THROW(CAlgoAlignUtilException, eInternal, 
                   "CHitComparator: Sorting criterion not supported.");
    };

    return rv;
}

END_NCBI_SCOPE

#endif /* ALGO_ALIGN_UTIL_HITFILTER__HPP */
