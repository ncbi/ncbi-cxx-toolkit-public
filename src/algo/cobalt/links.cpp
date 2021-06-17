/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: links.cpp

Author: Greg Boratyn

Contents: Implementation of CLinks

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/cobalt/links.hpp>

/// @file links.cpp
/// Implementation of CLinks

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)


// Compare links based on node indexes
static bool compare_links_by_nodes(const CLinks::SLink* link1,
                                   const CLinks::SLink* link2)
{
    if (link1->first == link2->first) {
        return link1->second < link2->second;
    }

    return link1->first < link2->first;
}

CLinks::~CLinks()
{}

void CLinks::AddLink(int first, int second, double weight)
{
    // make the link nodes ordered
    if (first > second) {
        swap(first, second);
    }
    if (second >= (int)m_NumElements) {
        NCBI_THROW(CLinksException, eInvalidNode, "Adding node with index "
                   " larger than number of elements attempted");
    }
    m_Links.push_back(SLink(first, second, weight));

    m_NumLinks++;
    if (weight > m_MaxWeight) {
        m_MaxWeight = weight;
    }

    // adding a new link makes the list unsorted
    m_IsSorted = false;
}

bool CLinks::IsLink(int first, int second) const
{
    if (!m_IsSorted) {
        NCBI_THROW(CLinksException, eUnsortedLinks, "Links must be sorted "
                   "before checks for links can be made");
    }
    if (first >= (int)m_NumElements || second >= (int)m_NumElements) {
        NCBI_THROW(CLinksException, eInvalidNode, "Adding node with index "
                   " larger than number of elements attempted");
    }

    if (first > second) {
        swap(first, second);
    }

    return x_IsLinkPtr(first, second);
}

bool CLinks::IsLink(const vector<int>& elems1, const vector<int>& elems2,
                    double& dist) const
{
    if (!m_IsSorted) {
        NCBI_THROW(CLinksException, eUnsortedLinks, "Links must be sorted "
                   "before checks for links can be made");
    }

    double sum = 0.0;
    ITERATE (vector<int>, it1, elems1) {
        ITERATE (vector<int>, it2, elems2) {
            const SLink* link = x_GetLink(*it1, *it2);
            if (!link) {
                return false;
            }
            else {
                sum += link->weight;
            }
        }
    }

    dist = sum / ((double)(elems1.size() * elems2.size()));
    return true;
}

void CLinks::Sort(void)
{
    // sort according to weights
    m_Links.sort();
    m_IsSorted = true;

    // initialize list of link pointers
    x_InitLinkPtrs();
}


void CLinks::x_InitLinkPtrs(void)
{
    m_LinkPtrs.clear();

    // create list of link pointers
    NON_CONST_ITERATE (list<SLink>, it, m_Links) {
        m_LinkPtrs.push_back(&*it);
    }

    // sort by node indexes
    sort(m_LinkPtrs.begin(), m_LinkPtrs.end(), compare_links_by_nodes);
}

bool CLinks::x_IsLinkPtr(int first, int second) const
{
    _ASSERT(!m_LinkPtrs.empty());
    _ASSERT(first <= second);

    SLink link(first, second, 0.0);
    return binary_search(m_LinkPtrs.begin(), m_LinkPtrs.end(), &link,
                         compare_links_by_nodes);
}

const CLinks::SLink* CLinks::x_GetLink(int first, int second) const
{
    _ASSERT(!m_LinkPtrs.empty());

    if (first > second) {
        swap(first, second);
    }

    SLink link(first, second, 0.0);
    vector<SLink*>::const_iterator it = lower_bound(m_LinkPtrs.begin(),
                                                    m_LinkPtrs.end(),
                                                    &link,
                                                    compare_links_by_nodes);

    if (it != m_LinkPtrs.end() && (*it)->first == first
        && (*it)->second == second) {

        return *it;
    }

    return NULL;
}


END_SCOPE(cobalt)
END_NCBI_SCOPE
