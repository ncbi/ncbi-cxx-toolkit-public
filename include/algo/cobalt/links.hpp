#ifndef ALGO_COBALT___LINKS__HPP
#define ALGO_COBALT___LINKS__HPP

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

File name: links.hpp

Author: Greg Boratyn

Contents: Interface for CLinks class: Distance matrix represented as a graph

******************************************************************************/

/// @file links.hpp

#include <corelib/ncbiobj.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <list>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)


/// Set of edges with weights between nodes represented by zero-based positive
/// integers
class NCBI_COBALT_EXPORT CLinks : public CObject
{
public:

    /// Single link
    struct SLink {
        int first;      ///< Node
        int second;     ///< Node
        double weight;  ///< Edge weight

        /// Constructor
        SLink(int f, int s, double w) : first(f), second(s), weight(w) {}

        /// Less then operator for sorting links by weights
        bool operator<(const SLink& link) {return weight < link.weight;}
    };

    typedef list<SLink>::const_iterator SLink_CI;

public:

    /// Constructor
    /// @param num_elements Number of nodes (not necessary connected) [in]
    /// @param mark_links If true existing links will be marked in binary
    /// [in] table
    ///
    CLinks(Uint4 num_elements, bool mark_links = true)
        : m_NumElements(num_elements),
          m_IsSorted(false),
          m_MarkLinks(mark_links),
          m_MaxWeight(0.0)
    {}

    /// Destructor
    ~CLinks();

    /// Add link
    /// @param first Node number [in]
    /// @param second Node number [in]
    /// @param weight Link weight [in]
    ///
    /// The links are assumed to be undirected. All nodes are stored such that
    /// first < second, if the inputs are different the nodes will be swaped.
    /// The method does not check whether given link already exists. Adding
    /// two links between the same nodes may cause unexpected results.
    void AddLink(int first, int second, double weight);

    /// Check if link exists between given two nodes
    /// @param first Node number [in]
    /// @param second Node number [in]
    /// @return True if link exists, false otherwise
    ///
    bool IsLink(int first, int second) const;

    /// Check whether the links are sorted according to weights
    /// @return True if links are sorted, false otherwise
    ///
    bool IsSorted(void) const {return m_IsSorted;}

    /// Sort links according to weights in ascending order
    ///
    void Sort(void);

    /// Get number of nodes
    /// @return Number of nodes
    ///
    Uint4 GetNumElements(void) const {return m_NumElements;}

    /// Get number of links
    /// @return Number of links
    ///
    Uint4 GetNumLinks(void) const {return m_NumLinks;}

    /// Get maximum weight over all links
    /// @return Maxium weight
    ///
    double GetMaxWeight(void) const {return m_MaxWeight;}

    /// Check if links are marked in binary table. If they are the IsLink()
    /// function will be fast. If not the funtion will iterate through all
    /// links.
    /// @return True if links are marked, false otherwise
    ///
    bool GetMarkLinks(void) const {return m_MarkLinks;}

    /// Get iterator pointing to the first link
    /// @return Link iterator
    ///
    SLink_CI begin(void) const {return m_Links.begin();}

    /// Get iterator pointing behind the last link
    /// @return Link iterator
    ///
    SLink_CI end(void) const {return m_Links.end();}

private:
    /// Forbid copy constructor
    CLinks(const CLinks& links);

    /// Forbid assignment operator
    CLinks& operator=(const CLinks& links);

    /// Get index for a link in the binary table
    int x_GetBinIndex(int first, int second) const;


protected:

    /// Links
    list<SLink> m_Links;

    /// Binary table for marking existing links
    bm::bvector<> m_IsLink;

    /// Number of nodes
    Uint4 m_NumElements;

    /// Number of links
    Uint4 m_NumLinks;

    /// Is list of links sorted
    bool m_IsSorted;

    /// Are existing linkes marked
    bool m_MarkLinks;

    /// Maximym weight in the list
    double m_MaxWeight;
};


/// Exceptions for CLinks class
class CLinksException : public CException
{
public:

    /// Error codes
    enum EErrCode {
        eInvalidNode  ///< Invalid node index
    };

    NCBI_EXCEPTION_DEFAULT(CLinksException, CException);
};


END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // ALGO_COBALT___LINKS__HPP
