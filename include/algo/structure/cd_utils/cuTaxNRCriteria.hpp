#ifndef CU_TAXNRCRITERIA__HPP
#define CU_TAXNRCRITERIA__HPP

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

#include <algo/structure/cd_utils/cuPrefTaxNodes.hpp>
#include <algo/structure/cd_utils/cuTaxClient.hpp>
#include <algo/structure/cd_utils/cuNRCriteria.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CTaxNRItem : public CNRItem {
public:

    typedef int  TTaxItemId;
    static const TTaxItemId INVALID_TAX_ITEM_ID;

    TTaxItemId  prefTaxnode; // labels a subcluster this item is placed in (INVALID_TAX_ITEM_ID = not a preferred tax node)
    TTaxItemId  modelOrg;    // labels the model org identifier for this item (INVALID_TAX_ITEM_ID = not a model org)
    TTaxId      taxId;       // NCBI taxonomy Id

    virtual ~CTaxNRItem() {};

    CTaxNRItem() : CNRItem() {
        InitTaxItem(INVALID_TAX_ITEM_ID, INVALID_TAX_ITEM_ID, INVALID_TAX_ID);
    }
    CTaxNRItem(TItemId idIn, TTaxItemId prefTaxnodeIn, TTaxItemId modelOrgIn, TTaxId taxIdIn, bool keepIn=true) : CNRItem(idIn, keepIn) {
        InitTaxItem(prefTaxnodeIn, modelOrgIn, taxIdIn);
    }
    CTaxNRItem(const CTaxNRItem& rhs) : CNRItem(rhs.itemId, rhs.keep) {
        InitTaxItem(rhs.prefTaxnode, rhs.modelOrg, rhs.taxId);
    }

    const CTaxNRItem& operator=(const CTaxNRItem& rhs) {
        if (&rhs != this) {
            Set(rhs);
        }
        return *this;
    }

    void Set(const CTaxNRItem& rhs) {
        itemId = rhs.itemId;
        keep   = rhs.keep;
        InitTaxItem(rhs.prefTaxnode, rhs.modelOrg, rhs.taxId);
    }

    bool operator==(const CTaxNRItem& rhs) const {
        return (itemId == rhs.itemId && keep == rhs.keep && prefTaxnode == rhs.prefTaxnode && modelOrg == rhs.modelOrg && taxId == rhs.taxId);
    }
    bool operator!=(const CTaxNRItem& rhs) const {
        return (itemId != rhs.itemId || keep != rhs.keep || prefTaxnode != rhs.prefTaxnode || modelOrg != rhs.modelOrg || taxId != rhs.taxId);
    }

    //  Both compare based on taxonomy state *alone* (i.e., ignore itemId from base class).
    //  Return -1, 0, 1 depending on whether lhs is dropped in favor of rhs (lhs '<' rhs),
    //  lhs and rhs are both equally favorable, or lhs is kept in favor of rhs (lhs '>' rhs), respectively.
    //  Note:  for a single pref. tax node, returns 0 if both are the same model organism or if
    //         neither is a model organism:  we must lose one, but w/o further info either
    //         is equally favored to be kept.
    virtual int Compare(const CNRItem& rhs) const;
    static int CompareItems(const CTaxNRItem& lhs, const CTaxNRItem& rhs);

private:

    void InitTaxItem(TTaxItemId prefTaxnodeIn, TTaxItemId modelOrgIn, TTaxId taxIdIn) {
        prefTaxnode = prefTaxnodeIn;
        modelOrg = modelOrgIn;
        taxId = taxIdIn;
    }
};

class NCBI_CDUTILS_EXPORT CTaxNRCriteria : public CNRCriteria {

public:

    //  Map between the underlying ID (for CDs, it's the row number) and tax id.
    typedef map< CBaseClusterer::TId, TTaxId > TId2TaxidMap;
    typedef TId2TaxidMap::iterator TId2TaxidMapIt;
    typedef TId2TaxidMap::const_iterator TId2TaxidMapCIt;

    //  Subcluster mapping:  int is the index returned for a preferred tax node,
    //  where -1 is the index for the subcluster of indices under no preferred tax node.
    typedef map< int, CBaseClusterer::TCluster > TSubclusterMap;
    typedef TSubclusterMap::iterator TSubclusterMapIt;
    typedef TSubclusterMap::const_iterator TSubclusterMapCIt;
    typedef TSubclusterMap::value_type TSubclusterMapVT;

    //  Loads the priorityTaxIds into the instance-specific priority tax nodes, type 'eRawTaxIds'.
    //  The vector index of taxIdsToBeClustered is used as the 'id' associated w/ its entries
    //  in setting up m_id2Tax:  this is assumed to be the same index used in building clusters.
    CTaxNRCriteria(const vector< TTaxId >& priorityTaxIds, const vector< TTaxId >& taxIdsToBeClustered);

    //  Use an existing set of priority tax nodes.
    //  The vector index of taxIdsToBeClustered is used as the 'id' associated w/ its entries
    //  in setting up m_id2Tax:  this is assumed to be the same index used in building clusters.
    CTaxNRCriteria(CPriorityTaxNodes* priorityTaxNodes, const vector< TTaxId >& taxIdsToBeClustered);

    //  Pass in the item id <--> tax id mapping and the set of priority tax nodes.
    //  The key in 'id2TaxidMap' is the index used when making clusters.
    //  NOTE:  Ownership is taken of 'priorityTaxNodes'.
    CTaxNRCriteria(CPriorityTaxNodes* priorityTaxNodes, const TId2TaxidMap& id2TaxidMap);

    virtual ~CTaxNRCriteria();

    virtual bool CanBeApplied() const {return m_isTaxConnected;}

    //  Don't allow this class to remove items from a cluster; always use 'Apply' instead.
    virtual unsigned int ApplyAndRemove(CBaseClusterer::TCluster*& cluster, string* report = NULL) {return 0;}

    //  Keep only those elements with taxonomy nodes under one of the priority nodes.
    //  This method will examine the contents of 'cluster' and mark any items
    //  deemed redundant by the criteria defined here by setting their 'keep'
    //  member false.
    //  Return value is the number of redundant elements identified.
    //  This class needs to be able to map from TItems in the clusters to a taxonomy id,
    //  or something we can relate to it:  here, we assume the index in the c-tor is the
    //  TItem in the cluster.
    virtual unsigned int Apply(CBaseClusterer::TCluster*& cluster, string* report = NULL);
    void ShouldMatch() { m_shouldMatch = true; }
    void ShouldNotMatch() { m_shouldMatch = false; }

    //  Simply compare values of taxId in the two items.
    virtual int CompareItems(const CTaxNRItem& lhs, const CTaxNRItem& rhs) const;

    //  If 'id' has not been seen, return an id of -1.
    TTaxId  GetTaxIdForId(const CBaseClusterer::TId& id) const;

    bool ConnectToServer();

//    static void SetPriorityTaxNodes(const CCdd_pref_nodes& prefNodes);
//    static unsigned int AddPriorityTaxNodes(const CCdd_pref_nodes& prefNodes, CPriorityTaxNodes::TaxNodeInputType nodeType = CPriorityTaxNodes::eCddPrefNodesAll);

protected:

    //  For initialization of subclasses.
    CTaxNRCriteria();

    //  Creates tax server, causing the load of pref nodes/model orgs
    void InitializeCriteria();

    //  If true, instructs to look for a 'match' among the priority tax nodes (default).
    //  If false, instructs to look for lack of a 'match' among the priority tax nodes
    //  (i.e., in the set of not-priority nodes).
    bool m_shouldMatch;

    bool m_isTaxConnected;
    static TaxClient* m_taxClient;           //  contains a CTaxon1 object
    CPriorityTaxNodes* m_priorityTaxNodes;   //  contains instance-specific priority tax nodes.

    TSubclusterMap m_subclusters;   // mapped by pref tax node identifier (incl. -1)
    TId2TaxidMap m_id2Tax;          // map row identifier to a taxon ID

    TTaxId GetTaxIdFromClient(const CBioseq& bioseq);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif  /* CU_TAXNRCRITERIA__HPP */
