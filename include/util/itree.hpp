#ifndef ITREE__HPP
#define ITREE__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Implementation of interval search tree based on McCreight's algorithm.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/01/11 15:00:37  vasilche
* Added CIntervalTree for seraching on set of intervals.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/range.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// forward declarations
class CIntervalTreeTraits;
class CIntervalTree;
struct SIntervalTreeNode;
struct SIntervalTreeNodeIntervals;

// parameter class for CIntervalTree
class CIntervalTreeTraits
{
public:
    typedef int coordinate_type;
    typedef CRange<coordinate_type> interval_type;
    typedef CObject* mapped_type;

    typedef map<interval_type, mapped_type> TIntervalMap;
    typedef TIntervalMap::iterator TIntervalMapI;
    typedef TIntervalMap::const_iterator TIntervalMapCI;

    static interval_type X2Y(interval_type interval);
    static interval_type Y2X(interval_type interval);

    static coordinate_type GetMax(void);
    static coordinate_type GetMaxCoordinate(void);
    static bool IsNormal(interval_type interval);
    static interval_type GetLimit(void);
    static bool IsLimit(interval_type interval);
    static bool IsLimit(const TIntervalMap& intervalMap);
    static void AddLimit(TIntervalMap& intervalMap);
};

// interval search tree structures
struct SIntervalTreeNode
{
    CIntervalTreeTraits::coordinate_type m_Key;

    SIntervalTreeNode* m_Left;
    SIntervalTreeNode* m_Right;

    SIntervalTreeNodeIntervals* m_NodeIntervals;
};

struct SIntervalTreeNodeIntervals
{
    typedef CIntervalTreeTraits traits;
    typedef traits::interval_type interval_type;
    typedef traits::mapped_type mapped_type;
    typedef traits::TIntervalMap TIntervalMap;

    SIntervalTreeNodeIntervals(const TIntervalMap& ref);
    
    bool Empty(void) const;
    
    void Insert(interval_type interval, const mapped_type& value);
    void Replace(interval_type interval, const mapped_type& value);
    bool Delete(interval_type interval);
    
    TIntervalMap m_ByX;
    TIntervalMap m_ByY;
};

class CIntervalTreeIterator
{
public:
    typedef CIntervalTreeTraits traits;
    typedef traits::coordinate_type coordinate_type;
    typedef traits::interval_type interval_type;
    typedef traits::mapped_type mapped_type;
    typedef traits::TIntervalMap TIntervalMap;
    typedef traits::TIntervalMapCI TIntervalMapCI;

    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;

    void Next(void);
    CIntervalTreeIterator& operator++(void);

    // get current state
    interval_type GetInterval(void) const;

protected:
    const mapped_type& GetValue(void) const;

private:
    friend class CIntervalTree;

    CIntervalTreeIterator(coordinate_type searchX,
                          const SIntervalTreeNode* nextNode);
    void SetIter(coordinate_type iterLimit, TIntervalMapCI iter);

    void Validate(void);
    bool ValidIter(void) const;
    void NextLevel(void);

    // iterator can be in four states:
    // 1. scanning auxList
    //      m_SearchX == X of search interval >= 0
    //      m_IterLimit == Y of search interval >= X
    //      m_NextNode == root node pointer
    // 2. scanning node by X
    //      m_SearchX == X of search interval >= 0
    //      m_IterLimit == X of search interval >= 0
    //      m_NextNode == next node pointer
    // 3. scanning node by Y
    //      m_SearchX == X of search interval >= 0
    //      m_IterLimit == -X of search interval <= 0
    //      m_NextNode == next node pointer
    // 4. end of scan
    //      m_SearchX < 0
    //      m_NextNode == 0
    coordinate_type m_SearchX;
    coordinate_type m_IterLimit;
    TIntervalMapCI m_Iter;
    const SIntervalTreeNode* m_NextNode;
};

// deal with intervals with coordinates in range [0, max], where max is
// CIntervalTree costructor argument.
class CIntervalTree
{
public:
    typedef size_t size_type;
    typedef CIntervalTreeTraits traits;
    typedef traits::coordinate_type coordinate_type;
    typedef traits::interval_type interval_type;
    typedef traits::mapped_type mapped_type;
    typedef traits::TIntervalMap TIntervalMap;
    typedef traits::TIntervalMapI TIntervalMapI;
    typedef traits::TIntervalMapCI TIntervalMapCI;

    typedef CIntervalTreeIterator const_iterator;

    CIntervalTree(void);
    ~CIntervalTree(void);

    // check state of tree
    bool Empty(void) const;
    size_type Size(void) const;

    // remove all elements
    void Clear(void);

    // set value in tree independently of old value in slot
    // return true if element was added to tree
    bool Set(interval_type interval, const mapped_type& value);
    // remove value from tree independently of old value in slot
    // return true if id element was removed from tree
    bool Reset(interval_type interval);

    // add new value to tree, throw runtime_error if this slot is not empty
    void Add(interval_type interval, const mapped_type& value);
    // replace old value in tree, throw runtime_error if this slot is empty
    void Replace(interval_type interval, const mapped_type& value);
    // delete existing value from tree, throw runtime_error if slot is empty
    void Delete(interval_type interval);

    // versions of methods without throwing runtime_error
    // add new value to tree, return false if this slot is not empty
    bool Add(interval_type interval, const mapped_type& value,
             const nothrow_t&);
    // replace old value in tree, return false if this slot is empty
    bool Replace(interval_type interval, const mapped_type& value,
                 const nothrow_t&);
    // delete existing value from tree, return false if slot is empty
    bool Delete(interval_type interval,
                const nothrow_t&);

    // list intervals containing specified point
    const_iterator AllIntervals(void) const;
    // list intervals containing specified point
    const_iterator IntervalsContaining(coordinate_type point) const;
    // list intervals overlapping with specified interval
    const_iterator IntervalsOverlapping(interval_type interval) const;

private:
    void Init(void);
    void Destroy(void);

    void DoInsert(interval_type interval, const mapped_type& value);
    void DoReplace(SIntervalTreeNode* node,
                   interval_type interval, const mapped_type& value);
    bool DoDelete(SIntervalTreeNode* node, interval_type interval);

    // root managing
    coordinate_type GetMaxRootCoordinate(void) const;
    coordinate_type GetNextRootKey(void) const;

    // node allocation
    SIntervalTreeNode* AllocNode(void);
    void DeallocNode(SIntervalTreeNode* node);

    // node creation/deletion
    SIntervalTreeNode* InitNode(SIntervalTreeNode* node,
                                coordinate_type key) const;
    void ClearNode(SIntervalTreeNode* node);
    void DeleteNode(SIntervalTreeNode* node);

    // node intervals allocation
    SIntervalTreeNodeIntervals* AllocNodeIntervals(void);
    void DeallocNodeIntervals(SIntervalTreeNodeIntervals* nodeIntervals);

    // node intervals creation/deletion
    SIntervalTreeNodeIntervals* CreateNodeIntervals(void);
    void DeleteNodeIntervals(SIntervalTreeNodeIntervals* nodeIntervals);

    SIntervalTreeNode m_Root;
    TIntervalMap m_ByX;

#if defined(_RWSTD_VER) && !defined(_RWSTD_ALLOCATOR)
    // BW_1: non statdard Sun's allocators
    typedef allocator_interface<allocator<SIntervalTreeNode>,SIntervalTreeNode> TNodeAllocator;
    typedef allocator_interface<allocator<SIntervalTreeNodeIntervals>,SIntervalTreeNodeIntervals> TNodeIntervalsAllocator;
#else
    typedef allocator<SIntervalTreeNode> TNodeAllocator;
    typedef allocator<SIntervalTreeNodeIntervals> TNodeIntervalsAllocator;
#endif

    TNodeAllocator m_NodeAllocator;
    TNodeIntervalsAllocator m_NodeIntervalsAllocator;
};

#include <util/itree.inl>

END_NCBI_SCOPE

#endif  /* ITREE__HPP */
