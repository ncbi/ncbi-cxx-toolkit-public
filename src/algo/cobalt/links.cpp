static char const rcsid[] = "$Id$";

/*
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

struct compare_links
{
    bool operator() (const CLinks::SLink& link1,
                     const CLinks::SLink& link2) const
    {
        return link1.weight >= link2.weight;
    }
};

CLinks::~CLinks()
{}

void CLinks::AddLink(int first, int second, double weight)
{
    // make the link nodes ordered
    if (first > second) {
        swap(first, second);
    }
    m_Links.push_back(SLink(first, second, weight));

    if (m_MarkLinks) {
        m_IsLink.set(x_GetBinIndex(first, second));
    }
    m_NumLinks++;
    if (weight > m_MaxWeight) {
        m_MaxWeight = weight;
    }

    // adding a new link makes the list unsorted
    m_IsSorted = false;
}

bool CLinks::IsLink(int first, int second) const
{
    if (first > second) {
        swap(first, second);
    }

    if (m_MarkLinks) {
        return m_IsLink[x_GetBinIndex(first, second)];
    }
    
    ITERATE (list<SLink>, it, m_Links) {
        if (it->first == first && it->second == second) {
            return true;
        }
    }

    return false;
}

void CLinks::Sort(void)
{
    // sort according to weights
    m_Links.sort(compare_links());
    m_IsSorted = true;
}


int CLinks::x_GetBinIndex(int first, int second) const
{
    _ASSERT(first < second);
    return (second * second - second) / 2 + first;
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
