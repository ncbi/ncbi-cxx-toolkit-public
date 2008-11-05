#ifndef UTIL_TIME_LINE__HPP
#define UTIL_TIME_LINE__HPP

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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Timeline object for approximate tracking of time events
 *                   
 */

#include <corelib/ncbitime.hpp>
#include <util/bitset/bmconst.h>
#include <deque>


BEGIN_NCBI_SCOPE

/// Timeline class for fast approximate time tracking.
///
/// This class discretizes time with specified precision using bit-vectors.
/// Time vector is divided so each slot represents N seconds(N-discr.factor)
/// Each slot points on bit-vector of ids (object id)
/// 
/// Typical application is when we need to setup a wake-up calls for millons
/// of int enumerated objects.
///
///  Example:
/// <pre>
///              123
///               28
///               10
///                          4
///                3
///                          2
///                ^         ^
///                |         |
///    ----+----+----+----+----+----+----+----+----+----+----+----+---->
///  slot 0   1    2    3    4   5   .....                            Time
///
///  When reaching moment 5 (slot 5) we can get the list of expired 
///  objects (all vectors from 0 to 5 OR-ed) : { 2,3,4,10,28,123 }
///
/// </pre>
///
template <class BV>
class CTimeLine 
{
public:
    typedef BV                    TBitVector;
    typedef deque<TBitVector*>    TTimeLine;

public:
    /// @param discr_factor
    ///    Time discretization factor (seconds), discretization defines
    ///    time line precision.
    /// @param tm
    ///    Initial time. (if 0 current time is taken)
    CTimeLine(unsigned discr_factor, time_t tm);

    void ReInit(time_t tm = 0);

    ~CTimeLine();

    /// Add object to the timeline
    /// @param tm
    ///    Moment in time associated with the object
    /// @param object_id
    ///    Objects id
    void AddObject(time_t tm, unsigned object_id);

    void AddObjects(time_t tm, const TBitVector& objects);

    /// Remove object from the time line, object_time defines time slot
    /// @return true if object has been removed, false if object is not 
    /// in the specified timeline
    bool RemoveObject(time_t object_time, unsigned object_id);

    /// Find and remove object
    void RemoveObject(unsigned object_id);

    /// Move object from one time slot to another
    void MoveObject(time_t old_time, time_t new_time, unsigned object_id);

    /// Extracts all objects up to 'tm' and puts them into 'objects' vector.
    /// Objects inserted into 'tm' are not extracted to provide histeresis.
    void ExtractObjects(time_t tm, TBitVector* objects);

    /// Enumerate all objects registered in the timeline
    void EnumerateObjects(TBitVector* objects) const;

    /// Return head of the timeline
    time_t GetHead() const { return m_TimeLineHead; }

    /// Time discretization factor
    unsigned GetDiscrFactor() const { return m_DiscrFactor; }

private:
    CTimeLine(const CTimeLine&);
    CTimeLine& operator=(const CTimeLine&);
    /// Add object to the timeline using time slot
    void x_AddObjectToSlot(unsigned time_slot, unsigned object_id);
    /// Compute slot position in the timeline
    unsigned x_TimeLineSlot(time_t tm) const;

private:
    unsigned    m_DiscrFactor;  //< Discretization factor
    time_t      m_TimeLineHead; //< Timeline head time
    TTimeLine   m_TimeLine;     //< timeline vector
};



#define TIMELINE_ITERATE(Var, Cont) \
    for ( typename TTimeLine::iterator Var = (Cont).begin();  Var != (Cont).end();  ++Var )

#define TIMELINE_CONST_ITERATE(Var, Cont) \
    for ( typename TTimeLine::const_iterator Var = (Cont).begin();  Var != (Cont).end();  ++Var )

template<class BV> 
CTimeLine<BV>::CTimeLine(unsigned discr_factor, time_t tm)
: m_DiscrFactor(discr_factor)
{
    ReInit(tm);
}


template<class BV> 
CTimeLine<BV>::~CTimeLine()
{
    TIMELINE_ITERATE(it, m_TimeLine) {
        TBitVector* bv = *it;
        delete bv;
    }
}


template<class BV> 
void CTimeLine<BV>::ReInit(time_t tm)
{
    TIMELINE_ITERATE(it, m_TimeLine) {
        TBitVector* bv = *it;
        delete bv;
    }
    if (tm == 0) {
        tm = time(0);
    }
    m_TimeLine.resize(0);
    m_TimeLineHead = (tm / m_DiscrFactor) * m_DiscrFactor;
    m_TimeLine.push_back(0);
}


template<class BV> 
void CTimeLine<BV>::AddObject(time_t object_time, unsigned object_id)
{
    if (object_time < m_TimeLineHead) {
        object_time = m_TimeLineHead;
    }

    unsigned slot = x_TimeLineSlot(object_time);
    x_AddObjectToSlot(slot, object_id);
}


template<class BV> 
void CTimeLine<BV>::AddObjects(time_t tm, const TBitVector& objects)
{
}


template<class BV> 
void CTimeLine<BV>::x_AddObjectToSlot(unsigned slot, unsigned object_id)
{
    while (slot >= m_TimeLine.size()) {
        m_TimeLine.push_back(0); 
    }
    TBitVector* bv = m_TimeLine[slot];
    if (bv == 0) {
        // TODO: Add class factory class to create TBitVector
        // meanwhile use hardcoded bm::BM_GAP (faster and economical)
        //    (bm::BM_GAP is NOT defined for template bv)
        bv = new TBitVector(bm::BM_GAP);
        m_TimeLine[slot] = bv;
    }
    bv->set(object_id);
}


template<class BV> 
bool CTimeLine<BV>::RemoveObject(time_t object_time, unsigned object_id)
{
    if (object_time < m_TimeLineHead) {
        return false;
    }
    unsigned slot = x_TimeLineSlot(object_time);
    if (slot < m_TimeLine.size()) {
        TBitVector* bv = m_TimeLine[slot];
        if (!bv) {
            return false;
        }
        bool changed = bv->set_bit(object_id, false);
        if (changed) {
            return true;
        }
    }
    return false;
}


template<class BV> 
void CTimeLine<BV>::RemoveObject(unsigned object_id)
{
    TIMELINE_ITERATE(it, m_TimeLine) {
        TBitVector* bv = *it;
        if (bv) {
            bool changed = bv->set_bit(object_id, false);
            if (changed) {
                return;
            }
        }
    } // NON_CONST_ITERATE
}

template<class BV> 
void CTimeLine<BV>::EnumerateObjects(TBitVector* objects) const
{
    TIMELINE_CONST_ITERATE(it, m_TimeLine) {
        TBitVector* bv = *it;
        if (bv) {
            *objects |= *bv;
        }
    } // CONST_ITERATE
}

template<class BV> 
void CTimeLine<BV>::MoveObject(time_t old_time, 
                               time_t new_time, 
                               unsigned object_id)
{
    bool removed = RemoveObject(old_time, object_id);
    if (!removed) {
        RemoveObject(object_id);
    }
    AddObject(new_time, object_id);
}


template<class BV> 
unsigned CTimeLine<BV>::x_TimeLineSlot(time_t tm) const
{
    //_ASSERT(tm >= m_TimeLineHead);

    if (tm <= m_TimeLineHead)
        return 0;

    unsigned interval_head = (unsigned)((tm / m_DiscrFactor) * m_DiscrFactor);
    unsigned diff = (unsigned)(interval_head - m_TimeLineHead);
    return diff / m_DiscrFactor;
}


template<class BV> 
void CTimeLine<BV>::ExtractObjects(time_t tm, TBitVector* objects)
{
    _ASSERT(objects);

    unsigned slot = x_TimeLineSlot(tm);
    for (unsigned i = 0; i < slot; ++i) {
        if (m_TimeLine.size() == 0) {
            ReInit(tm);
            return;
        }
        const TBitVector* bv = m_TimeLine[0];
        if (bv) {
            *objects |= *bv;
            delete bv;
        }
        m_TimeLine.pop_front();
    }
    m_TimeLineHead = m_TimeLineHead + slot * m_DiscrFactor;
}



END_NCBI_SCOPE

#endif // UTIL_TIME_LINE__HPP
