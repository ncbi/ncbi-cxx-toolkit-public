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
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/range.hpp>
#include <util/linkedset.hpp>


/** @addtogroup IntervalTree
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// forward declarations
class CIntervalTreeTraits;
template<class Traits> class CIntervalTreeConstIteratorTraits;
template<class Traits> class CIntervalTreeIteratorTraits;

class CIntervalTree;

template<class Traits> struct SIntervalTreeNode;
template<class Traits> struct SIntervalTreeNodeIntervals;
template<class Traits> class CIntervalTreeIterator;

// parameter class for CIntervalTree
class NCBI_XUTIL_EXPORT CIntervalTreeTraits
{
public:
    typedef CIntervalTreeTraits TTraits;
    typedef CIntervalTreeIteratorTraits<TTraits> TIteratorTraits;
    typedef CIntervalTreeConstIteratorTraits<TTraits> TConstIteratorTraits;

    typedef CIntervalTree TMap;
    typedef CIntervalTreeIterator<TIteratorTraits> TNCIterator;

    typedef int coordinate_type;
    typedef CRange<coordinate_type> interval_type;
    typedef CConstRef<CObject> mapped_type;

    typedef SLinkedSetValue<coordinate_type> TMapValue;
    struct STreeMapValue : public TMapValue
    {
        STreeMapValue(coordinate_type key, coordinate_type y,
                      const mapped_type& value)
            : TMapValue(key), m_Y(y), m_Value(value)
        {
        }

        coordinate_type m_Y;
        mapped_type m_Value;

        interval_type GetInterval(void) const
        {
            return interval_type(GetKey(), m_Y);
        }
    };
    typedef STreeMapValue TTreeMapValue;
    typedef CLinkedMultiset<TTreeMapValue> TTreeMap;
    typedef TTreeMap::iterator TTreeMapI;
    typedef TTreeMap::const_iterator TTreeMapCI;

    struct SNodeMapValue : public TMapValue
    {
        SNodeMapValue(coordinate_type key,
                      TTreeMapI value)
            : TMapValue(key), m_Value(value)
        {
        }

        TTreeMapI m_Value;
    };
    typedef SNodeMapValue TNodeMapValue;
    typedef CLinkedMultiset<TNodeMapValue> TNodeMap;
    typedef TNodeMap::iterator TNodeMapI;
    typedef TNodeMap::const_iterator TNodeMapCI;

    typedef SIntervalTreeNode<TTraits> TTreeNode;
    typedef SIntervalTreeNodeIntervals<TTraits> TTreeNodeInts;

    static coordinate_type GetMax(void);
    static coordinate_type GetMaxCoordinate(void);
    static bool IsNormal(const interval_type& interval);
};

// parameter class for CIntervalTree
template<class Traits>
class CIntervalTreeIteratorTraits : public Traits
{
    typedef Traits TParent;
public:
    typedef TParent TTreeTraits;

    typedef typename TParent::mapped_type& reference;

    typedef typename TParent::TTreeNode* TTreeNodeP;
    typedef typename TParent::TTreeNodeInts* TTreeNodeIntsP;
    typedef typename TParent::TMapValue* TMapValueP;
    typedef typename TParent::TTreeMapValue* TTreeMapValueP;
    typedef typename TParent::TNodeMapValue* TNodeMapValueP;
};

template<class Traits>
class CIntervalTreeConstIteratorTraits : public Traits
{
    typedef Traits TParent;
public:
    typedef TParent TTreeTraits;

    typedef const typename TParent::mapped_type& reference;

    typedef const typename TParent::TTreeNode* TTreeNodeP;
    typedef const typename TParent::TTreeNodeInts* TTreeNodeIntsP;
    typedef const typename TParent::TMapValue* TMapValueP;
    typedef const typename TParent::TTreeMapValue* TTreeMapValueP;
    typedef const typename TParent::TNodeMapValue* TNodeMapValueP;
};

// interval search tree structures
template<typename Traits>
struct SIntervalTreeNode
{
    typedef Traits TTraits;
    typedef typename TTraits::coordinate_type coordinate_type;
    typedef typename TTraits::interval_type interval_type;
    typedef typename TTraits::mapped_type mapped_type;

    typedef typename TTraits::TTreeNode TTreeNode;
    typedef typename TTraits::TTreeNodeInts TTreeNodeInts;

    coordinate_type m_Key;

    TTreeNode* m_Left;
    TTreeNode* m_Right;

    TTreeNodeInts* m_NodeIntervals;
};

template<typename Traits>
struct SIntervalTreeNodeIntervals
{
    typedef Traits TTraits;
    typedef typename TTraits::coordinate_type coordinate_type;
    typedef typename TTraits::interval_type interval_type;
    typedef typename TTraits::mapped_type mapped_type;

    typedef typename TTraits::TTreeNode TTreeNode;
    typedef typename TTraits::TTreeNodeInts TTreeNodeInts;

    typedef typename TTraits::TNodeMap TNodeMap;
    typedef typename TTraits::TNodeMapValue TNodeMapValue;
    typedef typename TTraits::TNodeMapI TNodeMapI;
    typedef typename TTraits::TTreeMapI TTreeMapI;

    bool Empty(void) const;
    
    void Insert(const interval_type& interval, TTreeMapI value);
    bool Delete(const interval_type& interval, TTreeMapI value);

    static void Delete(TNodeMap& m, const TNodeMapValue& v);
    
    TNodeMap m_ByX;
    TNodeMap m_ByY;
};

template<class Traits>
class CIntervalTreeIterator
{
public:
    typedef Traits TTraits;
    typedef CIntervalTreeIterator<TTraits> TThis;

    typedef typename TTraits::coordinate_type coordinate_type;
    typedef typename TTraits::interval_type interval_type;
    typedef typename TTraits::reference reference;

    typedef typename TTraits::TMapValueP TMapValueP;
    typedef typename TTraits::TTreeMapValueP TTreeMapValueP;
    typedef typename TTraits::TNodeMapValueP TNodeMapValueP;

    typedef typename TTraits::TTreeNodeP TTreeNodeP;
    typedef typename TTraits::TTreeNodeIntsP TTreeNodeIntsP;

    typedef typename TTraits::TMap TMap;
    typedef typename TTraits::TNCIterator TNCIterator;

    CIntervalTreeIterator(coordinate_type search_x = 0,
                          coordinate_type searchLimit = 0,
                          TMapValueP currentMapValue = 0,
                          TTreeNodeP nextNode = 0);
    CIntervalTreeIterator(const TNCIterator& iter);

    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;

    void Next(void);
    TThis& operator++(void);

    // get current state
    interval_type GetInterval(void) const;

    reference GetValue(void) const;

protected:
    bool InAuxMap(void) const;

    friend class CIntervalTree;

    TTreeMapValueP GetTreeMapValue(void) const;

    void NextLevel(void);

private:
    // iterator can be in four states:
    // 1. scanning auxList
    //      m_SearchX == X of search interval (>= 0)
    //      m_SearchLimit == Y of search interval (> X)
    //      m_CurrentMapValue = current node in AuxMap (!= 0)
    //      m_NextNode == root node pointer (!= 0)
    // 2. scanning node by X
    //      m_SearchX == X of search interval (>= 0)
    //      m_SearchLimit == X of search interval (>= 0)
    //      m_CurrentMapValue = current node in NodeMap (!= 0)
    //      m_NextNode == next node pointer (may be 0)
    // 3. scanning node by Y
    //      m_SearchX == X of search interval (>= 0)
    //      m_SearchLimit = -X of search interval (<= 0)
    //      m_CurrentMapValue = current node in NodeMap (!= 0)
    //      m_NextNode == next node pointer (may be 0)
    // 4. end of scan
    //      m_CurrentMapValue == 0

    // So state determination will be:
    // if ( m_CurrentMapValue == 0 ) state = END
    // else if ( m_SearchLimit > m_SearchX ) state = AUX
    // else if ( m_SearchLimit >= 0 ) state = NODE_BY_X
    // else state = NODE_BY_Y

    coordinate_type m_SearchX;
    coordinate_type m_SearchLimit;
    TMapValueP m_CurrentMapValue;
    TTreeNodeP m_NextNode;
};

// deal with intervals with coordinates in range [0, max], where max is
// CIntervalTree constructor argument.
class NCBI_XUTIL_EXPORT CIntervalTree
{
public:
    typedef CIntervalTreeTraits TTraits;
    typedef TTraits::TIteratorTraits TIteratorTraits;
    typedef TTraits::TConstIteratorTraits TConstIteratorTraits;

    typedef size_t size_type;
    typedef TTraits::coordinate_type coordinate_type;
    typedef TTraits::interval_type interval_type;
    typedef TTraits::mapped_type mapped_type;

    typedef TTraits::TTreeMap TTreeMap;
    typedef TTraits::TTreeMapI TTreeMapI;
    typedef TTraits::TTreeMapCI TTreeMapCI;
    typedef TTraits::TTreeMapValue TTreeMapValue;

    typedef TTraits::TTreeNode TTreeNode;
    typedef TTraits::TTreeNodeInts TTreeNodeInts;

    typedef CIntervalTreeIterator<TIteratorTraits> iterator;
    typedef CIntervalTreeIterator<TConstIteratorTraits> const_iterator;

    CIntervalTree(void);
    ~CIntervalTree(void);

    // check state of tree
    bool Empty(void) const;
    size_type Size(void) const;
    pair<double, size_type> Stat(void) const;

    // remove all elements
    void Clear(void);

    // insert
    iterator Insert(const interval_type& interval, const mapped_type& value);

    // set value in tree independently of old value in slot
    // return true if element was added to tree
    bool Set(const interval_type& interval, const mapped_type& value);
    // remove value from tree independently of old value in slot
    // return true if id element was removed from tree
    bool Reset(const interval_type& interval);

    // add new value to tree, throw runtime_error if this slot is not empty
    void Add(const interval_type& interval, const mapped_type& value);
    // replace old value in tree, throw runtime_error if this slot is empty
    void Replace(const interval_type& interval, const mapped_type& value);
    // delete existing value from tree, throw runtime_error if slot is empty
    void Delete(const interval_type& interval);

    // versions of methods without throwing runtime_error
    // add new value to tree, return false if this slot is not empty
    bool Add(const interval_type& interval, const mapped_type& value,
             const nothrow_t&);
    // replace old value in tree, return false if this slot is empty
    bool Replace(const interval_type& interval, const mapped_type& value,
                 const nothrow_t&);
    // delete existing value from tree, return false if slot is empty
    bool Delete(const interval_type& interval,
                const nothrow_t&);

    // end
    const_iterator End(void) const;
    iterator End(void);

    // find
    const_iterator Find(void) const;
    iterator Find(void);

    // list intervals containing specified point
    const_iterator AllIntervals(void) const;
    iterator AllIntervals(void);
    // list intervals containing specified point
    const_iterator IntervalsContaining(coordinate_type point) const;
    iterator IntervalsContaining(coordinate_type point);
    // list intervals overlapping with specified interval
    const_iterator IntervalsOverlapping(const interval_type& interval) const;
    iterator IntervalsOverlapping(const interval_type& interval);

    static void Assign(const_iterator& dst, const iterator& src);
    static void Assign(iterator& dst, const iterator& src);

private:
    void Init(void);
    void Destroy(void);

    struct SStat {
        size_type count;
        size_type total;
        size_type max;
    };
    void Stat(const TTreeNode* node, SStat& stat) const;

    void DoInsert(const interval_type& interval, TTreeMapI value);
    bool DoDelete(TTreeNode* node, const interval_type& interval, TTreeMapI value);

    // root managing
    coordinate_type GetMaxRootCoordinate(void) const;
    coordinate_type GetNextRootKey(void) const;

    // node allocation
    TTreeNode* AllocNode(void);
    void DeallocNode(TTreeNode* node);

    // node creation/deletion
    TTreeNode* InitNode(TTreeNode* node, coordinate_type key) const;
    void ClearNode(TTreeNode* node);
    void DeleteNode(TTreeNode* node);

    // node intervals allocation
    TTreeNodeInts* AllocNodeIntervals(void);
    void DeallocNodeIntervals(TTreeNodeInts* nodeIntervals);

    // node intervals creation/deletion
    TTreeNodeInts* CreateNodeIntervals(void);
    void DeleteNodeIntervals(TTreeNodeInts* nodeIntervals);

    TTreeNode m_Root;
    TTreeMap m_ByX;

#if defined(_RWSTD_VER) && !defined(_RWSTD_ALLOCATOR)
    // BW_1: non standard Sun's allocators
    typedef allocator_interface<allocator<TTreeNode>,TTreeNode> TNodeAllocator;
    typedef allocator_interface<allocator<TTreeNodeInts>,TTreeNodeInts> TNodeIntervalsAllocator;
#else
    typedef allocator<TTreeNode> TNodeAllocator;
    typedef allocator<TTreeNodeInts> TNodeIntervalsAllocator;
#endif

    TNodeAllocator m_NodeAllocator;
    TNodeIntervalsAllocator m_NodeIntervalsAllocator;
};


/* @} */


#include <util/itree.inl>

END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/06/30 20:21:41  jcherry
* Removed qualifiers for the sake of MSVC6.
*
* Revision 1.7  2004/06/30 16:42:28  jcherry
* Qualified some type names in nested structures for SWIG.
*
* Revision 1.6  2003/04/17 17:50:16  siyan
* Added doxygen support
*
* Revision 1.5  2003/02/07 16:54:01  vasilche
* Pass all structures with size > sizeof int by reference.
* Move cvs log to the end of files.
*
* Revision 1.4  2002/12/19 14:51:00  dicuccio
* Added export specifier for Win32 DLL builds.
*
* Revision 1.3  2001/05/17 15:01:19  lavr
* Typos corrected
*
* Revision 1.2  2001/01/29 15:18:38  vasilche
* Cleaned CRangeMap and CIntervalTree classes.
*
* Revision 1.1  2001/01/11 15:00:37  vasilche
* Added CIntervalTree for seraching on set of intervals.
*
* ===========================================================================
*/

#endif  /* ITREE__HPP */
