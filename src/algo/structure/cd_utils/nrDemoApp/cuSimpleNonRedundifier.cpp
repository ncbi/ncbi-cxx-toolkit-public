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

#include <ncbi_pch.hpp>
//#include <corelib/ncbitime.hpp>
#include <algo/structure/cd_utils/cuTaxNRCriteria.hpp>
#include "cuSimpleClusterer.hpp"
#include "cuSimpleNonRedundifier.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils);

/*
CSimpleNonRedundifier::CSimpleNonRedundifier(CDistBasedClusterer::TDist threshold) : m_clusterer(NULL), m_threshold(threshold), m_isBusy(false) {
    m_id2ItemMapping = new CNRCriteria::TId2Item;
}
*/

CSimpleNonRedundifier::CSimpleNonRedundifier() : m_isBusy(false), m_clusterer(NULL) {
//    m_id2ItemMapping = new CNRCriteria::TId2Item;
}

CSimpleNonRedundifier::~CSimpleNonRedundifier() {
    Reset(true, true);
    for (unsigned int i = 0; i < m_id2ItemMapping.size(); ++i) {
        delete m_id2ItemMapping[i];
    }
}

bool CSimpleNonRedundifier::SetClusterer(CBaseClusterer* clusterer) {
    m_clusterer = clusterer;
    return (m_clusterer) ? m_clusterer->IsValid() : false;
}

CDistBasedClusterer::TDist CSimpleNonRedundifier::GetThreshold() const {
	CDistBasedClusterer* distBasedClusterer = NULL;
	try {
		distBasedClusterer = dynamic_cast<CDistBasedClusterer*> (m_clusterer);
        return (distBasedClusterer) ? distBasedClusterer->GetClusteringThreshold() : 0;
	}catch (...){
		return 0;
	}
}

bool CSimpleNonRedundifier::SetThreshold(CDistBasedClusterer::TDist threshold) {
    bool result = true;
	CDistBasedClusterer* distBasedClusterer = NULL;
	try {
		distBasedClusterer = dynamic_cast<CDistBasedClusterer*> (m_clusterer);
        if (distBasedClusterer) distBasedClusterer->SetClusteringThreshold(threshold);
	}catch (...){
		result = false;
	}
    return result;
}

void CSimpleNonRedundifier::AddCriteria(CNRCriteria* criteria, bool verbose) {
    if (criteria) {
        CNRCriteria::TId2Item* id2item = new CNRCriteria::TId2Item;
        m_id2ItemMapping.push_back(id2item);
        criteria->SetId2ItemMap(id2item);
        m_nrCriteria.push_back(CriteriaStruct(criteria, verbose));
    }
}

void CSimpleNonRedundifier::Reset(bool deleteClusterer, bool deleteCriteria) {
    if (deleteClusterer) {
        delete m_clusterer;
    }
    m_clusterer = NULL;

    if (deleteCriteria) {
        for (unsigned int i = 0; i < m_nrCriteria.size(); ++i) {
            delete m_nrCriteria[i].criteria;
            m_nrCriteria[i].criteria = NULL;
        }
    }
    m_nrCriteria.clear();

    for (unsigned int i = 0; i < m_id2ItemMapping.size(); ++i) {
        if (m_id2ItemMapping[i]) m_id2ItemMapping[i]->clear();
    }
}

CNRItem* CSimpleNonRedundifier::GetItemForId(CBaseClusterer::TId itemId, unsigned int nrIndex) {
    CNRItem* itemFromCriteria = m_nrCriteria[nrIndex].criteria->GetItemForId(itemId);
    const CNRCriteria::TId2Item* thismap = m_nrCriteria[nrIndex].criteria->GetId2ItemMap();
    const CNRCriteria::TId2Item* thatmap = m_id2ItemMapping[nrIndex];

    if (nrIndex >= m_id2ItemMapping.size() || !m_id2ItemMapping[nrIndex]) return NULL;
    CNRCriteria::TId2ItemIt it = m_id2ItemMapping[nrIndex]->find(itemId);
    return (it == m_id2ItemMapping[nrIndex]->end()) ? NULL : it->second;

    /*
    //  Don't rely on the criteria having same map as this object:
    //  Although we initialize criteria w/ m_id2ItemMapping ptr, it
    //  may have been changed.
    CNRItem* result = NULL;
    unsigned int i = 0, nCriteria = m_nrCriteria.size();
    while (!result && i < nCriteria) {
        if (m_nrCriteria[i].criteria) {
            result = m_nrCriteria[i].criteria->GetItemForId(itemId);
        }
        ++i;
    }
    */
}

 unsigned int CSimpleNonRedundifier::ComputeRedundancies() {

    unsigned int nClusters;
    unsigned int nRedundant, nRedundantForCriteria, nRedundantTotal = 0;
    string reportFromApply;
    CBaseClusterer::TCluster* clusterPtr;
//    ncbi::CTime start, stop;

    m_isBusy=true;
    if (m_clusterer) {
//        start.SetCurrent();
//        m_clusterer->SetClusteringThreshold(m_threshold);
        nClusters = m_clusterer->MakeClusters();
//        stop.SetCurrent();
//        ERR_POST(ncbi::Info << "MakeClusters:  elapsed time (s):  %f" << stop.DiffNanoSecond(start)/::kNanoSecondsPerSecond);

        /*
        string giStr;
        CSeqClusterer::TClusterCit clusIt, clusEnd;
        for (unsigned int ij = 0; ij < nClusters; ++ij) {
            string s = "cluster " + NStr::UIntToString(ij);
            const CSeqClusterer::TCluster& cluster = m_clusterer->GetCluster(ij);
            clusIt = cluster.begin();
            clusEnd = cluster.end();
            for (; clusIt != clusEnd; ++clusIt) {
                giStr = m_clusterer->IdToString(clusIt->second);
                s.append("\n    gi " + giStr + ", row " + NStr::UIntToString(clusIt->first));
            }
            s.append("\n");
            cdLog::Printf(LOG_DEBUG, s.c_str());
        }
        */

        //  For each cluster, apply each of the non-redundification criteria in turn.
        for (unsigned int j = 0; j < m_nrCriteria.size(); ++j) {
            string s = "\n    Apply  " + m_nrCriteria[j].criteria->GetName();
            nRedundantForCriteria = 0;
//            start.SetCurrent();
            for (unsigned int i = 0; i < nClusters; ++i) {
                if (m_clusterer->GetCluster(i, clusterPtr) && clusterPtr != NULL) {
                    reportFromApply.clear();
                    nRedundant = m_nrCriteria[j].criteria->Apply(clusterPtr, m_nrCriteria[j].verbose ? &reportFromApply : NULL);
                    nRedundantForCriteria += nRedundant;
                    if (nRedundant > 0) {
                        s.append("\nCluster " + NStr::UIntToString(i) + ";  " + NStr::UIntToString(nRedundant) + " redundant of " + NStr::UIntToString(clusterPtr->size()) + " members.");
                        if (reportFromApply.size() > 0) s.append("\n" + reportFromApply + "\n");
/*
                        CBaseClusterer::TClusterIt itemIt = clusterPtr->begin(), itemItEnd = clusterPtr->end();
                        s.append("\n    Redundancies found by criteria " + NStr::UIntToString(j) + ":\n");
                        for (; itemIt != itemItEnd; ++itemIt) {
                            if (!m_nrCriteria[j].criteria->IsItemKept(*itemIt)) {
                                s.append("        id " + NStr::UIntToString(*itemIt));
                                s.append("  taxId " + NStr::IntToString(((CTaxNRCriteria*) m_nrCriteria[j].criteria)->GetTaxIdForId(*itemIt)) + "\n");
                            }
                        }
*/
                    }
                }
            }
            // nRedundant counts the number of redundancies the *last* criteria returns
            nRedundantTotal += nRedundantForCriteria;
//            if (nRedundantForCriteria && s.length() > 0) cdLog::Printf(LOG_DEBUG, s.c_str());
//            stop.SetCurrent();
            s.append("\nFound " + NStr::UIntToString(nRedundantForCriteria) + " redundant for criteria " + NStr::UIntToString(j) + ".\n");
            ERR_POST(ncbi::Info << "Non-redundifier log:\n\n" << s << ncbi::Endl());
//            ERR_POST(ncbi::Info << "\n    Time to apply criteria:  elapsed time (s):  " << stop.DiffNanoSecond(start)/::kNanoSecondsPerSecond);
        }
    }

    m_isBusy=false;
    return nRedundantTotal;
}

//unsigned int CSimpleNonRedundifier::ComputeRedundancies(const CDistBasedClusterer::TDist** distances, unsigned int dim) {
unsigned int CSimpleNonRedundifier::ComputeRedundancies(const CDistBasedClusterer::TDist** distances, unsigned int dim, CDistBasedClusterer::TDist clusteringThreshold) {

    unsigned int nClusters, nRedundant, nRedundantTotal = 0;
    CBaseClusterer::TCluster* clusterPtr;

	CSimpleClusterer* clusterer = new CSimpleClusterer(distances, dim, clusteringThreshold);
    if (!clusterer || !distances || dim == 0) {
        return kMax_UInt;
    }
    delete m_clusterer;

    m_isBusy=true;
    m_clusterer = clusterer;
    nClusters = m_clusterer->MakeClusters();

//    m_clusterer->SetClusteringThreshold(m_threshold);
//    m_clusterer->MakeClusters(distances, dim);

    //  For each cluster, apply each of the non-redundification criteria in turn.
    for (unsigned int i = 0; i < m_clusterer->NumClusters(); ++i) {
        nRedundant = 0;
        if (m_clusterer->GetCluster(i, clusterPtr) && clusterPtr != NULL) {
            for (unsigned int j = 0; j < m_nrCriteria.size(); ++j) {
                nRedundant += m_nrCriteria[j].criteria->Apply(clusterPtr, NULL);
            }
        }
        nRedundantTotal += nRedundant;
    }

    m_isBusy=false;
    return nRedundantTotal;
}

bool CSimpleNonRedundifier::GetItemStatus(unsigned int nrIndex, CBaseClusterer::TId itemId, CBaseClusterer::TClusterId* clusterIndex, unsigned int* indexInCluster)
{

    bool keepItem = false;
    CBaseClusterer::TCluster* clusterPtr;
    CBaseClusterer::TClusterIt clusterIt, clusterEnd;
    CBaseClusterer::TClusterId clusterId = CBaseClusterer::INVALID_CLUSTER_ID;
    CBaseClusterer::TId subcluster = CBaseClusterer::INVALID_ID;

    //  Gets the item in NR's map.
    //  Note:  This is initialized to be the same as in the criteria, but it could change
    //         if CNRCriteria::SetId2ItemMap(...) gets call afterwards.
    //
    CNRItem* nrItem = GetItemForId(itemId, nrIndex);

    if(m_clusterer && nrItem){
//        clusterId = m_clusterer->GetClusterForItemId(itemId, clusterPtr);
        clusterId = m_clusterer->GetClusterForId(itemId, clusterPtr);
        if (clusterPtr) {
            clusterEnd = clusterPtr->end();
            for (clusterIt = clusterPtr->begin(); clusterIt != clusterEnd; ++clusterIt) {
                if (itemId == *clusterIt) {
                    keepItem = nrItem->keep;
                    try {
                        subcluster = (dynamic_cast<CTaxNRItem*> (nrItem))->prefTaxnode;
                    } catch (...) {
                        subcluster = CTaxNRItem::INVALID_TAX_ITEM_ID;
                    }
                    break;
                }
//                if (itemId == clusterIt->second.id) {
//                    keepItem = clusterIt->second.keep;
//                    subcluster = clusterIt->second.prefTaxnode;
//                    break;
//                }
            }
        }
	}

    if (clusterIndex) *clusterIndex = clusterId;
    if (indexInCluster) *indexInCluster = subcluster;

    return keepItem;
}

bool CSimpleNonRedundifier::GetItemStatus(CBaseClusterer::TId itemId, CBaseClusterer::TClusterId* clusterIndex)
{
    bool keepItem = true;
    unsigned int nCriteria = m_nrCriteria.size();
    for (unsigned int nrIndex = 0; keepItem && nrIndex < nCriteria; nrIndex++) {
        keepItem = GetItemStatus(nrIndex, itemId, clusterIndex);
    }
    return keepItem;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
