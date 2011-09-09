#ifndef ALGO_GNOMON___SET__HPP
#define ALGO_GNOMON___SET__HPP

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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 * simplified set implementation
 *
 */

#include <corelib/ncbistd.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

template <class T>
class CVectorSet {
    typedef vector<T> TElements;
    typedef typename TElements::iterator iterator;
public:
    typedef typename TElements::const_iterator const_iterator;

    CVectorSet() {};
    bool operator==(const CVectorSet& another) const
    {
        if (size() != another.size())
            return false;
        for (size_t i=0; i<size(); ++i) {
            if (m_elements[i] < another.m_elements[i] || another.m_elements[i] < m_elements[i])
                return false;
        }
        return true;
    }

    bool insert(const T& element)
    {
        if (empty() || m_elements.back()<element) {
            m_elements.push_back(element);
            return true;
        } else {
            iterator i = m_elements.begin();
            while (*i<element)
                ++i;
            if (!(*i==element)) {
                m_elements.insert(i,element);
                return true;
            }
        }
        return false;
    }
    
    template <typename iterator>
    void insert(iterator begin,iterator end)
    {
        for (iterator i = begin; i != end; ++i)
            insert(*i);
    }

    void clear() { m_elements.clear(); }
  
    size_t size() const { return m_elements.size(); }
    bool empty() const { return m_elements.empty(); }
    const_iterator begin() const { return m_elements.begin(); }
    const_iterator end() const { return m_elements.end(); }
private:
    TElements m_elements;
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___SET__HPP
