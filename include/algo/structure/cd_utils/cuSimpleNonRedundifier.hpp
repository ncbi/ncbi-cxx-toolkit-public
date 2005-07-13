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
* Author:  Chris Lanczycki
*
* File Description:
*   Simple non-redundifier:  allow for arbitrary clustering rules and criteria
*   by which to decide which elements in a cluster are redundant.
*   Requires a criteria that supports an id -> CNRItem* mapping.
*
*/

#ifndef CU_SIMPLE_NONREDUNDIFIER__HPP
#define CU_SIMPLE_NONREDUNDIFIER__HPP

#include <vector>
#include <algo/structure/cd_utils/cuBaseClusterer.hpp>
#include <algo/structure/cd_utils/cuNRCriteria.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils);

class CSimpleNonRedundifier
{
public:
//    CSimpleNonRedundifier(CDistBasedClusterer::TDist threshold = 0);
    CSimpleNonRedundifier();
    ~CSimpleNonRedundifier();

    unsigned int NumClusters(void) const;

    //  return the total number of redundant sequences across all clusters.
    unsigned int ComputeRedundancies();

    //  Use this form if the clusterer need to be pass a raw distance matrix.
    //  First creates a CSimpleClusterer, deleting any existing clusterer if creation succeeds.  
    //  If creation fails, or invalid input, return kMax_UInt and do nothing.
    unsigned int ComputeRedundancies(const CDistBasedClusterer::TDist** distances, unsigned int dim, CDistBasedClusterer::TDist clusteringThreshold);

    //  If storing TaxNRItems, the optional 'indexInCluster' argument returns it's 'prefTaxNode' element.
    bool GetItemStatus(CBaseClusterer::TId itemId, CBaseClusterer::TClusterId* clusterIndex  = NULL, unsigned int* indexInCluster = NULL);

//    const CDistBasedClusterer* GetClusterer() const;
//    bool SetClusterer(CDistBasedClusterer* clusterer);
    const CBaseClusterer* GetClusterer() const;
    bool SetClusterer(CBaseClusterer* clusterer);

    //  Return 0 if m_clusterer is not a CDistBasedClusterer.  
    CDistBasedClusterer::TDist GetThreshold() const;

    //  Return false if m_clusterer is not a CDistBasedClusterer.
    bool SetThreshold(CDistBasedClusterer::TDist threshold);
//    void SetThreshold(CDistBasedClusterer::TDist threshold);

    void AddCriteria(CNRCriteria* criteria, bool verbose = false);
    void Reset(bool deleteClusterer = false, bool deleteCriteria = false);


private : 

    bool m_isBusy;

    struct CriteriaStruct {
        CNRCriteria* criteria;
        bool verbose;
        CriteriaStruct(CNRCriteria* c, bool v = false) : criteria(c), verbose(v) {};
    };

    CBaseClusterer* m_clusterer;
//    CDistBasedClusterer* m_clusterer;        //  method by which to cluster items; holds underlying clusters
//    CDistBasedClusterer::TDist m_threshold;  //  %identity threshold:  clusters will contain seqs at this or greater sim

    vector< CriteriaStruct > m_nrCriteria;   //  rules for deciding redundancy; applied in vector's order
    CNRCriteria::TId2Item* m_id2ItemMapping; //  Common itemId->item mapping to be shared among criteria.

    CNRItem* GetItemForId(CBaseClusterer::TId itemId);

};

inline
unsigned int CSimpleNonRedundifier::NumClusters(void) const {
    return (m_clusterer) ? m_clusterer->NumClusters() : 0;
}

inline
const CBaseClusterer* CSimpleNonRedundifier::GetClusterer() const {
    return m_clusterer;
}

/*
inline
CDistBasedClusterer::TDist CSimpleNonRedundifier::GetThreshold() const {
    return m_threshold;
}

inline
void CSimpleNonRedundifier::SetThreshold(CDistBasedClusterer::TDist threshold) {
    m_threshold = threshold;
}
*/

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif    //  CU_SIMPLE_NONREDUNDIFIER__HPP

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/07/13 19:06:03  lanczyck
* classes for building a SLC non-redundifier that does not depend on CCdd-related classes
*
*
* ===========================================================================
*/
