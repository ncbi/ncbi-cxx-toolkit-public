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
        eSubjMin,
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

    switch(m_SortType) {

    case eQueryMin:

        rv = lhs->GetQueryMin() <= rhs->GetQueryMin();
        break;

    case eSubjMin:

        rv = lhs->GetSubjMin() <= rhs->GetSubjMin();
        break;
        
    case eQueryMinScore:
        {
            const typename THit::TCoord qmin_lhs = lhs->GetQueryMin();
            const typename THit::TCoord qmin_rhs = rhs->GetQueryMin();
            if(qmin_lhs == qmin_rhs) {
                return lhs->GetScore() > rhs->GetScore();
            }
            else {
                return qmin_lhs < qmin_rhs;
            }
        }
        break;
        
    case eSubjMinScore:
        {
            const typename THit::TCoord smin_lhs = lhs->GetSubjMin();
            const typename THit::TCoord smin_rhs = rhs->GetSubjMin();
            if(smin_lhs == smin_rhs) {
                return lhs->GetScore() > rhs->GetScore();
            }
            else {
                return smin_lhs < smin_rhs;
            }
        }
        break;


    case eSubjMaxQueryMax: 
        {
            const typename THit::TCoord lhs_subj_max = lhs->GetSubjMax();
            const typename THit::TCoord rhs_subj_max = rhs->GetSubjMax();
            
            if(lhs_subj_max < rhs_subj_max) {
                rv = true;
            }
            else if(lhs_subj_max > rhs_subj_max) {
                rv = false;
            }
            else {
                rv = lhs->GetQueryMax() < rhs->GetQueryMax();
            }
        }
        break;
        
    case eQueryId:
        
        rv = *(lhs->GetQueryId()) < *(rhs->GetQueryId());
        break;

    case eSubjId:
        
        rv = *(lhs->GetSubjId()) < *(rhs->GetSubjId());
        break;
        
    case eSubjIdQueryId: 
        {
            const int co = lhs->GetSubjId()->CompareOrdered( *(rhs->GetSubjId()) );
            if(co == 0) {
                rv = *(lhs->GetQueryId()) < *(rhs->GetQueryId());
            }
            else {
                rv = co < 0;
            }
        }
        break;
        
    case eSubjStrand:
        
        rv = lhs->GetSubjStrand() < rhs->GetSubjStrand();
        break;
        
        
    case eQueryIdSubjIdSubjStrand:
        {
            const int qid = lhs->GetQueryId()->CompareOrdered(*(rhs->GetQueryId()));
            const int sid = lhs->GetSubjId()->CompareOrdered(*(rhs->GetSubjId()));

            if(qid == 0) {
                if(sid == 0) {
                    const bool subj_strand_lhs  = lhs->GetSubjStrand();
                    const bool subj_strand_rhs = rhs->GetSubjStrand();
                    return subj_strand_lhs > subj_strand_rhs;
                }
                else {
                    rv = sid < 0;
                }
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



/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2006/11/27 14:46:53  kapustin
 * +eQueryIdSubjIdSubjStrand
 *
 * Revision 1.6  2005/10/24 17:31:59  kapustin
 * Add eSubjIdQueryId sort criterion
 *
 * Revision 1.5  2005/09/27 14:38:19  rsmith
 * Sorting CRefs not CConstRefs
 *
 * Revision 1.4  2005/09/12 16:21:34  kapustin
 * Add compartmentization algorithm
 *
 * Revision 1.3  2005/07/28 14:55:25  kapustin
 * Use std::pair instead of array to fix gcc304 complains
 *
 * Revision 1.2  2005/07/28 12:29:26  kapustin
 * Convert to non-templatized classes where causing compilation incompatibility
 *
 * Revision 1.1  2005/07/27 18:53:16  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_HITFILTER__HPP */
