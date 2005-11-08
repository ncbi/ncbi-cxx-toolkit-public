/* $Id$
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

File name: hitlist.hpp

Author: Jason Papadopoulos

Contents: Interface for CHitList class

******************************************************************************/

/// @file hitlist.hpp
/// Interface for CHitList class

#ifndef _ALGO_COBALT_HITLIST_HPP_
#define _ALGO_COBALT_HITLIST_HPP_

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/hit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

class CHitList {

public:
    typedef pair<bool, CHit *> TListEntry;
    typedef vector<TListEntry> TList;

    CHitList() {}

    ~CHitList() 
    {
        NON_CONST_ITERATE(TList, itr, m_List) {
            delete (*itr).second;
        }
    }

    int Size() { return m_List.size(); }
    bool Empty() { return m_List.empty(); }

    void AddToHitList(CHit *hit) 
    { 
        m_List.push_back(TListEntry(true, hit)); 
    }

    CHit *GetHit(int index) 
    { 
        assert(index < Size());
        return m_List[index].second; 
    }

    bool GetKeepHit(int index)
    { 
        assert(index < Size());
        return m_List[index].first;
    }

    void SetKeepHit(int index, bool keep) 
    { 
        assert(index < Size());
        m_List[index].first = keep; 
    }

    void PurgeUnwantedHits()
    {
        int i, j;
        for (i = j = 0; i < Size(); i++) {
            if (m_List[i].first == false)
                delete m_List[i].second;
            else
                m_List[j++] = m_List[i];
        }
        m_List.resize(j);
    }

    void PurgeAllHits()
    {
        for (int i = 0; i < Size(); i++)
            delete m_List[i].second;
        m_List.clear();
    }

    void ResetList() { m_List.clear(); }

    void SortByScore();
    void SortByStartOffset();
    void MakeCanonical();
    void MatchOverlappingSubHits(CHitList& matched_list);
    CHitList& operator+=(CHitList& hitlist);

private:
    TList m_List;
};


END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // _ALGO_COBALT_HITLIST_HPP_

/*--------------------------------------------------------------------
  $Log$
  Revision 1.2  2005/11/08 17:40:26  papadopo
  ASSERT->assert

  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

--------------------------------------------------------------------*/
