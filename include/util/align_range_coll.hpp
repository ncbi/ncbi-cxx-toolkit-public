#ifndef UTIL___ALIGN_RANGE_COLL__HPP
#define UTIL___ALIGN_RANGE_COLL__HPP

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
 * Authors:  Andrey Yazhuk
 *
 * File Description:
 *  class CAlignRangeCollection - sorted collection of CAlignRange-s
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiexpt.hpp>

#include <util/range.hpp>
#include <util/align_range_oper.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE

class CAlignRangeCollException : public CException
{
public:
    CAlignRangeCollException()
    {
    }

    virtual const char* GetErrCodeString(void) const
    {
        return "CAlignRangeCollection - operation resulted in invalid state."; 
    }
};


///////////////////////////////////////////////////////////////////////////////
/// class CAlignRangeCollection<TAlignRange> represent a sorted collection of 
/// TAlignRange. 
/// The collection has two coordinate spaces: "First" - primary, usually 
/// represents alignment or consensus space, "Second" - represents a coordinate
/// space associated with the aligned sequence.
/// policies - do not overlap on master
/// - do not overlap on aligned
/// When collection is normalized all segments are sorted by "From" position, 
/// and all policies are enforced.
/// If policy check fails
template<class TAlnRange>
    class CAlignRangeCollection
{
public:
    typedef TAlnRange TAlignRange;
    typedef typename TAlignRange::position_type position_type;
    typedef CAlignRangeCollection<TAlignRange>  TThisType;
    typedef vector<TAlignRange> TAlignRangeVector;
    typedef typename TAlignRangeVector::const_iterator const_iterator;
    typedef typename TAlignRangeVector::const_reverse_iterator const_reverse_iterator;
    typedef typename TAlignRangeVector::size_type size_type;

    enum EFlags {
        /// Policies:
        fKeepNormalized = 0x0001, /// enforce all policies after any modification
        /// if normalization is not possible exception will be thrown
        fAllowMixedDir  = 0x0002, /// allow segments with different orientation
        fAllowOverlap   = 0x0004, /// allow segments overlapping on the first sequence
        fAllowAbutting  = 0x0008, /// allows segments not separated by gaps

        fDefaultPolicy  = fKeepNormalized,

        fPolicyMask     = 0x000f,

        /// State flags:
        fNotValidated   = 0x0100, /// collection was modified and not validated
        fInvalid        = 0x0200,  /// one or more policies violated

        fUnsorted     = 0x010000,
        fDirect       = 0x020000, /// contains at least one direct range
        fReversed     = 0x040000, /// contains at least one reversed range
        fMixedDir     = fDirect | fReversed,
        fOverlap      = 0x080000,
        fAbutting     = 0x100000

        /// if fUnsorted is set then there fOverlap and fAbutting are undefined (test is expensive)
        /// if fOverlap is set fAbutting is undefined
    };
    /// adding empty ranges is considered valid, they are simply ignored

    enum ESearchDirection {
        eNone,
        eForward,
        eBackwards,
        eLeft,
        eRight
    };

    CAlignRangeCollection(int flags = fDefaultPolicy)
        :   m_Flags(flags)
    { 
    }

    CAlignRangeCollection(const TAlignRangeVector& v,
                          int flags)
        : m_Flags(flags)
    {
        m_Ranges = v;
    }

    /// @name Container Interface
    /// @{
    /// immitating vector, but providing only "const" access to elements
    void reserve(int count)
    {
        m_Ranges.reserve(count);
    }

    const_iterator begin() const
    {
        return m_Ranges.begin();
    }

    const_iterator end() const
    {
        return m_Ranges.end();
    }

    const_reverse_iterator rbegin() const
    {
        return m_Ranges.rbegin();
    }

    const_reverse_iterator rend() const
    {
        return m_Ranges.rend();
    }

    size_type size() const
    {
        return m_Ranges.size();
    }

    bool empty() const
    {
        return m_Ranges.empty();
    }

    const_iterator insert(const TAlignRange& r)
    {
        if (r.GetLength() > 0) {
            iterator it = end_nc();

            if (IsSet(fKeepNormalized)) { // find insertion point
                PAlignRangeFromLess<TAlignRange> p;
                it = std::lower_bound(begin_nc(), end_nc(), r.GetFirstFrom(), p);
            }

            return insert(it, r);
        }
        return end();
    }

    // insert(where, r)() will add a specified range into a specified position
    // if insertion violates policies the segment will be inserted and 
    // appropriate flags set (exception thrown)
    const_iterator insert(const_iterator where, const TAlignRange& r)
    {
        const_iterator it_ins = end(); // insertion point (return value)
        if (r.GetLength() > 0) {
            iterator it = begin_nc() + (where - begin());

            int r_dir = r.IsDirect() ? fDirect : fReversed;
            m_Flags |= r_dir;

            if (IsSet(fKeepNormalized)) {
                if (it != begin_nc()) {     // check left
                    TAlignRange& r_left = *(it - 1);
                    if (r_left.IsAbutting(r)) {
                        if (IsSet(fAllowAbutting)) {
                            m_Flags |= fAbutting;
                        }
                        else {
                            r_left.CombineWithAbutting(r);
                            it_ins = it - 1;
                        }
                    }
                    else { // can overlap or be mixed
                        int flags = ValidateRanges(r_left, r);
                        m_Flags |= flags; 
                    }
                }
                if (it != end_nc()) {    // check right
                    TAlignRange& r_right = *it;
                    if (r_right.IsAbutting(r)) {
                        if (IsSet(fAllowAbutting)) {
                            m_Flags |= fAbutting;
                        }
                        else {    // merge
                            if (it_ins != end()) {
                                TAlignRange& r_left = *(it - 1);
                                r_left.CombineWithAbutting(r_right);
                                m_Ranges.erase(it);
                            }
                            else {
                                r_right.CombineWithAbutting(r);
                                it_ins = it;
                            }
                        }
                    }
                    else { // can overlap or be mixed
                        int flags = ValidateRanges(r, r_right);
                        m_Flags |= flags; 
                    }
                }

                if (it_ins == end()) {
                    it_ins = m_Ranges.insert(it, r);
                }
                x_ValidateFlags(); // validate and throw exception if needed
            }
            else {
                m_Flags |= fNotValidated;
                it_ins = m_Ranges.insert(it, r);
            }
        }
        return it_ins;
    }

    const_iterator erase(const_iterator it)
    {
        iterator it_del = begin_nc() + (it - begin());
        const_iterator it_res = end();

        if (it_del >= begin_nc()  &&  it_del < end_nc()) {
            it_res = m_Ranges.erase(it_del);

            if (empty()) {
                x_ResetFlags(fInvalid | fNotValidated | fUnsorted | 
                             fMixedDir | fAbutting | fOverlap);
            }
            else {
                if (IsSet(fInvalid)) {
                    x_SetFlags(fNotValidated); // erasing probably fixed the problem
                }
            }
            return it_res;
        }
        return it_del;
    }

    void push_back(const TAlignRange& r)
    {
        insert(end(), r);
    }

    void pop_back()
    {
        const_iterator it = end();
        erase(--it);
    }

    const TAlignRange& operator[](size_type pos) const
    {
        return m_Ranges[pos];
    }

    TAlignRange& operator[](size_type pos)
    {
        return m_Ranges[pos];
    }

    void clear()
    {
        m_Ranges.clear();

        x_ResetFlags(fInvalid | fNotValidated | fUnsorted |
                     fMixedDir | fAbutting | fOverlap);
    }

    // returns an iterator pointing to a range containing "pos" or end() if 
    // such a range does not exist
    const_iterator find(position_type pos) const
    {
        pair<const_iterator, bool> res = find_2(pos);
        return res.second ? res.first : m_Ranges.end();
    }

    /// returns an iterator pointing to a range containing "pos"; if such a 
    /// range does not exists an iterator points to a first range that has 
    /// ToOpen > pos; the bool element of pair specifies whether the range 
    /// contains the position.
    pair<const_iterator, bool> find_2(position_type pos) const
    {
        PAlignRangeToLess<TAlignRange> p;
        const_iterator it = std::lower_bound(begin(), end(), pos, p); /* NCBI_FAKE_WARNING: WorkShop */
        bool b_contains = (it != end()  &&  it->GetFirstFrom() <= pos);
        return make_pair(it, b_contains);
    }

    const_iterator lower_bound(position_type pos) const
    {
        PAlignRangeToLess<TAlignRange> p;
        return std::lower_bound(begin(), end(), pos, p);
    }

    const_iterator upper_bound(position_type pos) const
    {
        PAlignRangeToLess<TAlignRange> p;
        return std::upper_bound(begin(), end(), pos, p);
    }

    /// @}

    position_type GetFirstFrom() const
    {
        if (!m_Ranges.empty()) {
            return begin()->GetFirstFrom();
        }
        else {
            return TAlignRange::GetEmptyFrom();
        }
    }

    position_type GetFirstToOpen() const
    {
        if (!m_Ranges.empty()) {
            return rbegin()->GetFirstToOpen();
        }
        else {
            return TAlignRange::GetEmptyToOpen();
        }
    }

    position_type GetFirstTo() const
    {
        if (!m_Ranges.empty()) {
            return rbegin()->GetFirstTo();
        }
        else {
            return TAlignRange::GetEmptyTo();
        }
    }

    position_type GetFirstLength (void) const
    {
        if (!m_Ranges.empty()) {
            position_type from = begin()->GetFirstFrom();
            return rbegin()->GetFirstToOpen() - from;
        }
        else {
            return 0;
        }
    }

    CRange<position_type> GetFirstRange() const
    {
        return CRange<position_type>(GetFirstFrom(), GetFirstTo());
    }

    int GetFlags() const {
        return m_Flags;
    }

    bool IsSet(int flags) const {
        return (m_Flags & flags) == flags;
    }

    int GetPolicyFlags() const {
        return m_Flags & fPolicyMask;
    }

    int GetStateFlags() const {
        return m_Flags & ~fPolicyMask;
    }

    TSignedSeqPos GetSecondPosByFirstPos(position_type pos,
                                         ESearchDirection dir = eNone) const
    {
        pair<const_iterator, bool> res = find_2(pos);

        // when translating from first to second eForward = eRight, eBackwards = eLeft
        if (!res.second) {   // pos does not belong to a segment - adjust
            if (dir == eRight  ||  dir == eForward) {
                if (res.first != end()) {
                    res.second = true;
                    pos = res.first->GetFirstFrom();
                }
            }
            else if (dir == eLeft  ||  dir == eBackwards) {
                if (res.first != begin()) {
                    --res.first;
                    res.second = true;
                    pos = res.first->GetFirstTo();
                }
            }
        } 
        return res.second ? res.first->GetSecondPosByFirstPos(pos)
            : -1;
    }

    TSignedSeqPos GetFirstPosBySecondPos(position_type pos,
                                         ESearchDirection dir = eNone) const
    {
        ESearchDirection dir_direct = eNone, dir_reversed = eNone;
        if (dir == eLeft) {
            dir_direct = eBackwards;
            dir_reversed = eForward;
        }
        else if (dir == eRight) {
            dir_direct = eForward;
            dir_reversed = eBackwards;
        }

        const_iterator it_closest = end();
        TSignedSeqPos min_dist = -1, min_pos = -1;

        for (const_iterator it = begin(); it != end(); it++) {
            const TAlignRange& r = *it;
            TSignedSeqPos res = r.GetFirstPosBySecondPos(pos);
            if(res != -1) {
                return res;
            }
            else if(dir != eNone) {
                // it does not contain pos - check the distance between "r" and "pos"
                int dist = -1;
                int res_pos = -1;
                ESearchDirection dir_2 = r.IsDirect() ? dir_direct : dir_reversed;
                if (dir == eForward  ||  dir_2 == eForward) {
                    res_pos = r.GetSecondFrom();
                    dist = res_pos - pos;
                }
                else if (dir == eBackwards  ||  dir_2 == eBackwards) {
                    res_pos = it->GetSecondTo();
                    dist = pos - res_pos;
                }
                if (dist > 0  &&  (min_dist == -1  || min_dist > dist)) {
                    it_closest = it;
                    min_dist = dist;
                    min_pos = res_pos;
                }
            }
        }

        return (it_closest == end()) ? -1 : it_closest->GetFirstPosBySecondPos(min_pos);
    }

    void Sort()
    {
        std::sort(m_Ranges.begin(), m_Ranges.end(),
                  PAlignRangeFromLess<TAlignRange>());
        SortInsertions();

        x_ResetFlags(fUnsorted);
        x_SetFlags(fNotValidated);
    }

    void SortInsertions(void)
    {
        std::sort(m_Insertions.begin(), m_Insertions.end(),
            PAlignRangeFromLess<TAlignRange>());
    }

    /// merge adjacent segments together, merging changes collection size and invalidates
    /// iterators
    void CombineAbutting()
    {
        if(IsSet(fUnsorted)  ||  IsSet(fOverlap)) {  // operation cannot be performed
            throw CAlignRangeCollException();
        }

        vector<bool>* dead = NULL; // vector of indexes for elements that need to be removed
        size_t size = m_Ranges.size();

        for (size_t i = size - 1; i > 0; --i) {
            if (m_Ranges[i - 1].IsAbutting(m_Ranges[i])) {
                if (dead == NULL) {   // allocate temp vector
                    dead = new vector<bool>(size, false);
                    dead->reserve(size);
                }
                m_Ranges[i - 1].CombineWithAbutting(m_Ranges[i]); // merge i into i-1
                (*dead)[i] = true;
            }
        }
        if (dead) {   // there are deal elements - need to compress
            size_t shift = 0;
            for (size_t i = 0; i < size; i++ ) {
                if ((*dead)[i]) {
                    ++shift;
                }
                else if(shift > 0) {
                    m_Ranges[i - shift] = m_Ranges[i];
                }
            }
            delete dead;
            m_Ranges.resize(size - shift);
        }
        x_ResetFlags(fAbutting);
        x_SetFlags(fNotValidated);
    }

    /// analyses segements and updates flags
    void Validate()
    {
        if (IsSet(fNotValidated)  ||  IsSet(fInvalid)) {
            int complete = fUnsorted | fMixedDir | fAbutting | fOverlap;
            int flags = 0;
            for (size_t i = 0; i < m_Ranges.size()-1; i++) {
                flags |= ValidateRanges(m_Ranges[i], m_Ranges[i + 1]);
                if(flags == complete) {
                    break;
                }
            }
            if (flags == 0) {
                x_ResetFlags(fInvalid);
            }
            else {
                x_SetFlags(flags | fInvalid);
            }
            x_ResetFlags(fNotValidated);
        }
    }

    /// ensures that segments are sorted, if fAllowAdjust is not set - merges adjacent segments    
    void Normalize()
    {
        Sort();
        CombineAbutting();
        Validate();
    }

    /// determine conflicts between two ranges
    static int ValidateRanges(const TAlignRange& r_1, const TAlignRange& r_2)
    {
        _ASSERT(r_1.NotEmpty()  &&  r_2.NotEmpty());

        const TAlignRange* r_left = &r_1;
        const TAlignRange* r_right = &r_2;
        int flags = 0;

        if (r_1.IsDirect() != r_2.IsDirect()) {
            flags = fMixedDir;
        }
        if (r_2.GetFirstFrom() < r_1.GetFirstFrom()) {
            flags |= fUnsorted;
            swap(r_left, r_right);
        }
        if (r_left->GetFirstToOpen() > r_right->GetFirstFrom()) {
            flags |= fOverlap;
        }
        else if (r_1.IsAbutting(r_2)) {
            flags |= fAbutting;
        }
        return flags;
    }

    void AddInsertion(const TAlignRange& r)
    {
        m_Insertions.push_back(r);
    }

    void AddInsertions(const TAlignRangeVector& insertions)
    {
        ITERATE(typename TAlignRangeVector, it, insertions) {
            m_Insertions.push_back(*it);
        }
        SortInsertions();
    }

    void AddInsertions(const TThisType& collection)
    {
        ITERATE(typename TAlignRangeVector, it, collection) {
            m_Insertions.push_back(*it);
        }
        SortInsertions();
    }

    /// Each insertion shows where the 'first' sequence has a gap
    /// while the 'second' sequence has the insertion of the specified
    /// length. Direction of the insertion is always 'direct'.
    const TAlignRangeVector& GetInsertions() const
    {
        return m_Insertions;
    }

protected:
    void x_SetFlags(int flags)
    {
        m_Flags |= flags;
    }
    void x_ResetFlags(int flags)
    {
        m_Flags &= ~flags;
    }
    void x_ValidateFlags()
    {
        if (IsSet(fKeepNormalized)) {
            int invalid_flags = m_Flags & (fMixedDir | fOverlap | fAbutting);
            if (IsSet(fAllowMixedDir)) {
                invalid_flags &= ~fMixedDir;
            }
            if (IsSet(fAllowOverlap)) {
                invalid_flags &= ~fOverlap;
            }
            if (IsSet(fAllowAbutting)) {
                invalid_flags &= ~fAbutting;
            }
            if ((invalid_flags & fMixedDir) == fMixedDir  ||
               (invalid_flags & (fOverlap | fAbutting))) {
                x_SetFlags(fInvalid);
                throw CAlignRangeCollException(); // copy collection to exception?
            }
        }
    }

    typedef typename TAlignRangeVector::iterator iterator;
    typedef typename TAlignRangeVector::reverse_iterator reverse_iterator;

    iterator  begin_nc()
    {
        return m_Ranges.begin();
    }
    iterator  end_nc()
    {
        return m_Ranges.end(); 
    }
    bool x_Equals(const TThisType &c) const
    {
        if (&c == this) {
            return true;
        }
        else {
            return m_Ranges == c.m_Ranges;
        }
    }

    TAlignRangeVector    m_Ranges;
    TAlignRangeVector    m_Insertions;
    int m_Flags;    /// combination of EFlags
};


///////////////////////////////////////////////////////////////////////////////
/// CAlignRangeCollExtender
/// this is an adapter for a collection that perfroms some of the operations
/// faster due to an additional index built on top of the collection.

template <class TColl>
    class CAlignRangeCollExtender
{
public:
    typedef typename TColl::TAlignRange TAlignRange;
    typedef typename TColl::position_type position_type;
    typedef typename TColl::ESearchDirection TSearchDir;

    typedef multimap<position_type, const TAlignRange*> TFrom2Range;
    typedef typename TFrom2Range::const_iterator const_iterator;

    CAlignRangeCollExtender(const TColl& coll)
    {
        Init(coll);
    }

    void Init(const TColl& coll)
    {
        m_Coll = &coll;
        m_Ranges.clear(); // we are lazy and do not rebuild the index right now
        m_MapDirty = true;
    }

    void Clear()
    {
        m_Ranges.clear();
        m_MapDirty = true;
    }

    void UpdateIndex() const
    {
        if (m_MapDirty) {
            _ASSERT(m_Coll);

            m_Ranges.clear();
            ITERATE(typename TColl, it, *m_Coll) {
                const TAlignRange* r = &*it;

                if (m_Ranges.empty()) {
                    m_From = r->GetSecondFrom();
                    m_ToOpen = r->GetSecondToOpen();
                }
                else {
                    m_From = min(m_From, r->GetSecondFrom());
                    m_ToOpen = max(m_ToOpen, r->GetSecondToOpen());
                }

                m_Ranges.insert(typename TFrom2Range::value_type
                                (r->GetSecondFrom(), r));
            }

            m_MapDirty = false;
        }
    }

    const_iterator begin() const
    {
        return m_Ranges.begin();
    }

    const_iterator end() const
    {
        return m_Ranges.end();
    }

    position_type   GetSecondFrom() const
    {
        UpdateIndex();
        return m_From; 
    }

    position_type GetSecondToOpen() const
    {
        UpdateIndex();
        return m_ToOpen;
    }

    position_type GetSecondTo() const
    {
        return GetSecondToOpen() - 1;
    }

    position_type GetSecondLength (void) const
    {
        return m_ToOpen - m_From;
    }

    CRange<position_type> GetSecondRange() const
    {
        return CRange<position_type>(GetSecondFrom(), GetSecondTo());
    }

    /// finds a segment at logarithmic time (linear time on first call)
    /// returns an iterator to the fisrt segment containing "pos"
    const_iterator FindOnSecond(position_type pos) const
    {
        typename TFrom2Range::const_iterator it =
            std::lower_bound(m_Ranges.begin(), m_Ranges.end(), pos, PItLess());
        if (it != m_Ranges.end()) {
            const TAlignRange& r = *it->second;
            _ASSERT(r.GetSecondTo() >= pos);
            return r.SecondContains(pos) ? it : m_Ranges.end();
        }
        return it;
    }

protected:
    struct PItLess
    {
        typedef typename TAlignRange::position_type   position_type;
        bool operator()(const typename TFrom2Range::value_type& p,
                        position_type pos)  
        {
            return p.second->GetSecondTo() < pos;
        }
        bool operator()(position_type pos,
                        const typename TFrom2Range::value_type& p)  
        {
            return pos < p.second->GetSecondTo();
        }
        bool operator()(const typename TFrom2Range::value_type& p1,
                        const typename TFrom2Range::value_type& p2)
        {
            return p1.second->GetSecondTo() < p2.second->GetSecondTo();
        }
    };

    const TColl* m_Coll;

    mutable bool            m_MapDirty; // need to update map
    mutable TFrom2Range     m_Ranges;
    mutable position_type   m_From;
    mutable position_type   m_ToOpen;
};


END_NCBI_SCOPE

#endif  // UTIL___ALIGN_RANGE_COLL__HPP
