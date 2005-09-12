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
* File Description:
*
*/

#include <algo/align/util/hit_comparator.hpp>

#include <algorithm>
#include <numeric>
#include <vector>

BEGIN_NCBI_SCOPE



/////////////////////////////////////////
// CHitCoverageAccumulator

template<class THit>
class CHitCoverageAccumulator
{
public:

    typedef CRef<THit>    THitRef;
    typedef typename THit::TCoord  TCoord;

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

template<class THit>
class CHitFilter: public CObject
{
public:

    typedef CRef<THit>              THitRef;
    typedef vector<THitRef>         THitRefs;

    typedef typename THit::TCoord   TCoord;

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

    // 0 = query min, 1 = query max, 2 = subj min, 3 = subj max
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
};




END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/09/12 16:21:34  kapustin
 * Add compartmentization algorithm
 *
 * Revision 1.4  2005/07/28 16:43:35  kapustin
 * sort => stable_sort
 *
 * Revision 1.3  2005/07/28 02:16:46  ucko
 * s_GetCoverage: remove redundant class specifier that made GCC 2.95 choke.
 *
 * Revision 1.2  2005/07/27 20:45:03  kapustin
 * Move s_GetCoverage() definition under the class declaration 
 * to calm down MSVC
 *
 * Revision 1.1  2005/07/27 18:53:16  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_HITFILTER__HPP */
