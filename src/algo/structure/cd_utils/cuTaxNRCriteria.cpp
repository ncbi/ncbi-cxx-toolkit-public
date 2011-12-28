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
*   A base concrete class used for non-redundification of a cluster of sequences using
*   taxonomic criteria.
*
*/


#include <ncbi_pch.hpp>
#include <algorithm>
//#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuTaxNRCriteria.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

const CTaxNRItem::TTaxItemId CTaxNRItem::INVALID_TAX_ITEM_ID = (CTaxNRItem::TTaxItemId) CNRItem::INVALID_ITEM_ID;


int CTaxNRItem::Compare(const CNRItem& rhs) const 
{
    try {
        const CTaxNRItem& taxNRItem = dynamic_cast<const CTaxNRItem&> (rhs);
        return CTaxNRItem::CompareItems(*this, taxNRItem);
    } catch (std::bad_cast) {
        return 1;  //  rule in favor of this object if can't perform the cast
    }
}

int CTaxNRItem::CompareItems(const CTaxNRItem& lhs, const CTaxNRItem& rhs) {
    bool gotAnswer = false;
    int result;
    TTaxItemId prefNodeIdLHS, prefNodeIdRHS, modelOrgIdLHS, modelOrgIdRHS;
    string nodeName, orgName;
    TTaxItemId badId = INVALID_TAX_ITEM_ID;

    prefNodeIdLHS = lhs.prefTaxnode;
    prefNodeIdRHS = rhs.prefTaxnode;

    //  Cases where one or both items are not under a preferred tax node.  (If neither is
    //  under a preferred node, they are equivalent from point of view of taxonomy alone.)
    if (prefNodeIdLHS == badId) {
        result = (prefNodeIdRHS == badId) ? 0 : -1;
        gotAnswer = true;
    } else if (prefNodeIdRHS == badId) {
        result = 1;
        gotAnswer = true;
    }

    //  If the items are under different preferred tax nodes, in a binary comparison
    //  the items are equally favored independent of model organism status:  if there
    //  is only a non-model org. under a pref. tax node, we'll keep it.
    if (!gotAnswer && prefNodeIdLHS != prefNodeIdRHS) {
        result = 0;
        gotAnswer = true;
    }

    //  Cases where both items are under the same preferred tax node.
    if (!gotAnswer) {
        modelOrgIdLHS = lhs.modelOrg;
        modelOrgIdRHS = rhs.modelOrg;

        if (modelOrgIdLHS == badId) {
            if (modelOrgIdRHS == badId) {
                result = 0;  //  they're equally favored; we'd *keep* only one, but w/o tie-breakers it could be either
            } else {
                result = -1;
            }
        } else if (modelOrgIdRHS == badId) {
            result = 1;
        } else if (modelOrgIdLHS != modelOrgIdRHS) {
            result = 0;  //  if different model orgs, both kept
        } else {
            result = 0;  //  they're equally favored; we'd *keep* only one, but w/o tie-breakers it could be either
        }
    }

    return result;
}



////////////////////////////////////////////////
//   CTaxNRCriteria class
////////////////////////////////////////////////


TaxClient* CTaxNRCriteria::m_taxClient = NULL;

CTaxNRCriteria::CTaxNRCriteria() {
    InitializeCriteria();
    m_priorityTaxNodes = NULL;
}

CTaxNRCriteria::CTaxNRCriteria(const vector< int >& priorityTaxIds, const vector< int >& taxIdsToBeClustered) {
    InitializeCriteria();

    m_priorityTaxNodes = (m_taxClient) ? new CPriorityTaxNodes(priorityTaxIds, *m_taxClient) : NULL;

    for (CBaseClusterer::TId i = 0; i < taxIdsToBeClustered.size(); ++i) {
        m_id2Tax.insert(TId2TaxidMap::value_type(i, taxIdsToBeClustered[i]));
    }
}

CTaxNRCriteria::CTaxNRCriteria(CPriorityTaxNodes* priorityTaxNodes, const vector< int >& taxIdsToBeClustered) {
    InitializeCriteria();
    m_priorityTaxNodes = priorityTaxNodes;

    for (CBaseClusterer::TId i = 0; i < taxIdsToBeClustered.size(); ++i) {
        m_id2Tax.insert(TId2TaxidMap::value_type(i, taxIdsToBeClustered[i]));
    }
}

CTaxNRCriteria::CTaxNRCriteria(CPriorityTaxNodes* priorityTaxNodes, const TId2TaxidMap& id2TaxidMap) {
    InitializeCriteria();
    m_priorityTaxNodes = priorityTaxNodes;
    m_id2Tax = id2TaxidMap;
}

CTaxNRCriteria::~CTaxNRCriteria() {
    delete m_priorityTaxNodes;
}

int CTaxNRCriteria::GetTaxIdForId(const CBaseClusterer::TId& id) const {
    TId2TaxidMapCIt cit = m_id2Tax.find(id);
    int result = (cit != m_id2Tax.end()) ? cit->second : -1;
    return result;
}

void CTaxNRCriteria::InitializeCriteria() {

    m_name = "Taxonomic Non-redundification Criteria";
    m_shouldMatch = true;

    //  Lazy-initialize tax server.
    if (!m_taxClient) {
        m_taxClient = new TaxClient();
    }
    m_isTaxConnected = ConnectToServer();

    m_id2Tax.clear();
}

int CTaxNRCriteria::GetTaxIdFromClient(const CBioseq& bioseq)
{
    return (m_taxClient) ? m_taxClient->GetTaxIDFromBioseq(bioseq, false) : -1;
}

unsigned int CTaxNRCriteria::Apply(CBaseClusterer::TCluster*& clusterPtr, string* report) {

    unsigned int nSubcluster = 0, nMarkedRedundant = 0;
    int priorityNodeId, taxId;
    string nodeName;

    if (!clusterPtr || !m_id2ItemMap) return nMarkedRedundant;

    CTaxNRItem* taxNRItem = NULL;
    CTaxNRItem::TTaxItemId badId = CTaxNRItem::INVALID_TAX_ITEM_ID;

    CBaseClusterer::TId id;
    CBaseClusterer::TClusterIt itemIt = clusterPtr->begin(), itemItEnd = clusterPtr->end();

    //  Sort cluster into subclusters based on the priority tax node items are under.
    //  priorityNodeId == INVALID_TAX_ITEM_ID is the case where under no priority tax node.
    //  Apply rules to mark those items in the subclusters that are redundant.
    m_subclusters.clear();
    for (; itemIt != itemItEnd; ++itemIt) {

        id = *itemIt;
        taxId = m_id2Tax[id];
        nodeName.erase();
        priorityNodeId = (taxId > 0 && m_priorityTaxNodes) ? m_priorityTaxNodes->GetPriorityTaxnode(taxId, nodeName, m_taxClient) : badId;
        if (priorityNodeId == -1) priorityNodeId = badId;

        taxNRItem = new CTaxNRItem(id, (CTaxNRItem::TTaxItemId)(priorityNodeId), CTaxNRItem::INVALID_TAX_ITEM_ID, taxId, true);
        if (!taxNRItem) {
            continue;
        }

        if ((priorityNodeId == badId && m_shouldMatch) || (priorityNodeId != badId && !m_shouldMatch)) {
            if (report) report->append("\n    Toss ID " + NStr::UIntToString(id) + "  taxId = " + NStr::IntToString(taxId) + " nodeName = " + nodeName + ": priorityNodeId " + NStr::IntToString(priorityNodeId));
            taxNRItem->keep = false;
            ++nMarkedRedundant;
        } else {
            if (report) report->append("\n    Keep ID " + NStr::UIntToString(id) + "  taxId = " + NStr::IntToString(taxId) + " nodeName = " + nodeName + ": priorityNodeId " + NStr::IntToString(priorityNodeId));
        }
        if (m_subclusters.count(priorityNodeId) == 0) ++nSubcluster;

        _ASSERT(m_id2ItemMap->count(id) == 0);

        m_subclusters[priorityNodeId].insert(CBaseClusterer::TClusterVT(id));
        m_id2ItemMap->insert(TId2ItemVT(id, taxNRItem));
    }

    if (report && report->length() > 0) report->append("\n");

    return nMarkedRedundant;
}


int CTaxNRCriteria::CompareItems(const CTaxNRItem& lhs, const CTaxNRItem& rhs) const {

    string nodeName, orgName;
    CTaxNRItem lhsItem(lhs), rhsItem(rhs);

    //  If don't have valid field in the passed-in items, fill them in the temporaries.
    if (lhsItem.taxId == -1) {
        //  Doing it this way since operator[] returns non-const reference.
        TId2TaxidMapCIt citLHS = m_id2Tax.find(lhs.itemId);
        lhsItem.taxId = citLHS != m_id2Tax.end() ? citLHS->second : 0;
    }
    if (rhsItem.taxId == -1) {
        //  Doing it this way since operator[] returns non-const reference.
        TId2TaxidMapCIt citRHS = m_id2Tax.find(rhs.itemId);
        rhsItem.taxId = citRHS != m_id2Tax.end() ? citRHS->second : 0;
    }

    return (lhsItem.taxId == rhsItem.taxId) ? 0 : (lhsItem.taxId < rhsItem.taxId) ? -1 : 1;
}

bool CTaxNRCriteria::ConnectToServer() {
    if (m_taxClient && !m_taxClient->IsAlive()) {
        m_taxClient->ConnectToTaxServer();
    }
    m_isTaxConnected = (m_taxClient) ? m_taxClient->IsAlive() : false;
    return m_isTaxConnected;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
