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
*   Contains both the virtual base class and a concrete subclass used for non-redundification
*   of clusters of sequences according to class-specific rules.
*
*/

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuNRCriteria.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

//  Do the redundant cast in case the underlying type of CBaseClusterer::TId changes.
const CNRItem::TItemId CNRItem::INVALID_ITEM_ID = (CNRItem::TItemId) CBaseClusterer::INVALID_ID;

CNRCriteria::~CNRCriteria() {
    if (m_id2ItemMap) {
        for (TId2ItemIt i = m_id2ItemMap->begin(); i != m_id2ItemMap->end(); ++i) {
            delete i->second;
            i->second = NULL;
        }
    }
};

CNRItem* CNRCriteria::GetItemForId(CBaseClusterer::TId itemId) {
    if (!m_id2ItemMap) return NULL;
    TId2ItemIt it = m_id2ItemMap->find(itemId);
    return (it == m_id2ItemMap->end()) ? NULL : it->second;
}

bool CNRCriteria::GetItemForId(CBaseClusterer::TId itemId, CNRItem& nrItem) const {
    bool result = false;
    TId2ItemCit cit;
    if (m_id2ItemMap) {
        cit = m_id2ItemMap->find(itemId);
        if (cit != m_id2ItemMap->end() && cit->second) {
            result = true;
            nrItem = *(cit->second);
        }
    }
    if (!result) {
        nrItem.itemId = CNRItem::INVALID_ITEM_ID;
        nrItem.keep   = false;
    }
    return result;
}

bool CNRCriteria::IsItemKept(CBaseClusterer::TId itemId) const {
    bool result = false;
    CNRItem nrItem;
    if (GetItemForId(itemId, nrItem)) {
        result = nrItem.keep;
    }
    return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
