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

    typedef CConstRef<THit>   THitRef;

    // sorting criteria
    enum ESortCriterion {
        eQueryMin,
        eSubjMin
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
 * Revision 1.1  2005/07/27 18:53:16  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* ALGO_ALIGN_UTIL_HITFILTER__HPP */
