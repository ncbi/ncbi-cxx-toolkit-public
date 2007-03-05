#ifndef CU_NRCRITERIA__HPP
#define CU_NRCRITERIA__HPP

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
*   Contains the virtual base class used for defining criteria for the
*   non-redundification of clusters of sequences.
*
*/

#include <string>
#include <map>
#include <algo/structure/cd_utils/cuBaseClusterer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CBaseNRCriteria {

public:
    CBaseNRCriteria() {};
    virtual ~CBaseNRCriteria() {};

    //  This method will examine the contents of 'cluster' and *remove* any members
    //  deemed redundant according to this filter.
    //  Details can be reported if a valid string pointer is provided.
    //  Return value is the number of redundant elements removed.
    virtual unsigned int ApplyAndRemove(CBaseClusterer::TCluster*& cluster, string* report = NULL) = 0;

    //  Return true if the criteria will work as intended in it's current state.
    //  Return false otherwise (e.g., if a network data source is unavailable, ...)
    virtual bool CanBeApplied() const {return true;}
    string GetCriteriaError() const {return m_criteriaError;}

    string GetName() const {return m_name;}

protected:
    string   m_name;
    string   m_criteriaError;
};


//  Generic non-redundifier item to be associated with a unique TId from a cluster.
//  Most simply, itemId = the TId from the cluster, but its meaning may be defined
//  differently by a concrete subclass.
//  Subclass to add data needed by specific criteria.

class NCBI_CDUTILS_EXPORT CNRItem {
public:
    virtual ~CNRItem() {};

    typedef unsigned int TItemId;

    //  Sync the definition of invalid id with that in the clusterer so can
    //  treat 'itemId' as a TId if desired.
    static const TItemId INVALID_ITEM_ID;

    TItemId  itemId;      // meaning of the id is defined by a concrete subclass
    bool     keep;        // anticipating use of items in a non-redundification algorithm

    CNRItem() {Init(INVALID_ITEM_ID, false);}
    CNRItem(TItemId itemIdIn, bool keepIn=true) {Init(itemIdIn, keepIn);}
    CNRItem(const CNRItem& rhs) {Init(rhs.itemId, rhs.keep);}

    const CNRItem& operator=(const CNRItem& rhs) {
        if (&rhs != this) {
            Init(rhs.itemId, rhs.keep);
        }
        return *this;
    }

    bool operator==(const CNRItem& rhs) const {
        return (itemId == rhs.itemId && keep == rhs.keep);
    }
    bool operator!=(const CNRItem& rhs) const {
        return (itemId != rhs.itemId || keep != rhs.keep);
    }

    //  Functions to allow sorting of specified items in a cluster using a
    //  class-specific algorithm.  Any valid itemId should be less than INVALID_ITEM_ID.
    //  Standard 'compare' semantics:  returns 1 if lhs > rhs, 0 if lhr = rhs, -1 if lhs < rhs.

    virtual int Compare(const CNRItem& rhs) const {
        return CNRItem::CompareItems(*this, rhs);
    }

    static int CompareItems(const CNRItem& lhs, const CNRItem& rhs) {
        return (lhs.itemId == rhs.itemId) ? 0 : ((lhs.itemId < rhs.itemId) ? -1 : 1);
    }

private:
    void Init(TItemId itemIdIn, bool keepIn) {
        itemId = itemIdIn;
        keep = keepIn;
    }
};


//  Add a mapping between the unique id of an item in a cluster and a generic
//  CNRItem instance associated with that id.  Class cleans up the CNRItems.

class NCBI_CDUTILS_EXPORT CNRCriteria : public CBaseNRCriteria {

public:

    typedef map< CBaseClusterer::TId, CNRItem* > TId2Item;
    typedef TId2Item::iterator TId2ItemIt;
    typedef TId2Item::const_iterator TId2ItemCit;
    typedef TId2Item::value_type TId2ItemVT;

public:
    CNRCriteria(TId2Item* id2ItemMap = NULL) : m_id2ItemMap(id2ItemMap) {
        if (m_id2ItemMap) m_id2ItemMap->clear();
    };

    virtual ~CNRCriteria();

    virtual unsigned int ApplyAndRemove(CBaseClusterer::TCluster*& cluster, string* report = NULL) = 0;

    //  This method will examine the contents of 'cluster' and *mark* any CNRItems
    //  deemed redundant according to this filter in its 'keep' data member.
    //  Details can be reported if a valid string pointer is provided.
    //  Return value is the number of redundant elements identified.
    //  No items are removed from the cluster.
    virtual unsigned int Apply(CBaseClusterer::TCluster*& cluster, string* report = NULL) = 0;

    string GetName() const {return m_name;}

    //  Get an editable version of the NRItem mapped to the itemId.
    CNRItem* GetItemForId(CBaseClusterer::TId itemId);

    //  Returns false if itemId not found in map or mapped item is NULL, in which case
    //  nrItem is rest to have INVALID_ITEM_ID and keep == false.
    virtual bool GetItemForId(CBaseClusterer::TId itemId, CNRItem& nrItem) const;
    bool IsItemKept(CBaseClusterer::TId itemId) const;

    const TId2Item* GetId2ItemMap() const {return m_id2ItemMap;}
    void SetId2ItemMap(TId2Item* id2ItemMap) {
        m_id2ItemMap = id2ItemMap;
    }


protected:

    string    m_name;
    TId2Item* m_id2ItemMap;   //  Criteria object does not have ownership of this object.
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif  /* CU_NRCRITERIA__HPP */
