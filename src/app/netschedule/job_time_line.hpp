#ifndef NS_JOB_TIME_LINE__HPP
#define NS_JOB_TIME_LINE__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Job timeline
 *                   
 */

#include <corelib/ncbitime.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <deque>

BEGIN_NCBI_SCOPE

/// Timeline class to track objects in time with specified precision.
///
/// @internal
///
class CJobTimeLine 
{
public:
    typedef bm::bvector<>           TObjVector;
    typedef deque<TObjVector*>      TTimeLine;

public:
    /// @param discr_factor
    ///    Time discretization factor (seconds), discretization defines
    ///    time line precision.
    /// @param tm
    ///    Initial time. (if 0 current time is taken)
    CJobTimeLine(unsigned discr_factor, time_t tm);

    void ReInit(time_t tm);

    ~CJobTimeLine();

    /// Add object to the timeline
    /// @param object_time
    ///    Moment in time associated with the object
    /// @param object_id
    ///    Objects id
    void AddObject(time_t object_time, unsigned object_id);

    /// Add object to the timeline using time slot
    void AddObjectToSlot(unsigned time_slot, unsigned object_id);

    /// Remove object from the time line, object_time defines time slot
    /// @return true if object has been removed, false if object is not 
    /// in the specified timeline
    bool RemoveObject(time_t object_time, unsigned object_id);

    /// Find and remove object
    void RemoveObject(unsigned object_id);

    /// Move object from one time slot to another
    void MoveObject(time_t old_time, time_t new_time, unsigned object_id);

    /// Compute slot position in the timeline
    unsigned TimeLineSlot(time_t tm) const;

    /// Add all object ids to the vector up to the specified slot
    /// [0, slot] closed interval
    void EnumerateObjects(TObjVector* objects, unsigned slot) const;

    /// Truncate the timeline from the head up to the specified slot
    /// slot + 1 becomes the new head
    void HeadTruncate(unsigned slot);

    /// Return head of the timeline
    time_t GetHead() const { return m_TimeLineHead; }

    /// Time discretization factor
    unsigned GetDiscrFactor() const { return m_DiscrFactor; }

private:
    CJobTimeLine(const CJobTimeLine&);
    CJobTimeLine& operator=(const CJobTimeLine&);
private:
    unsigned    m_DiscrFactor;  //< Discretization factor
    time_t      m_TimeLineHead; //< Timeline head time
    TTimeLine   m_TimeLine;     //< timeline vector
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/03/10 14:18:46  kuznets
 * +MoveObject()
 *
 * Revision 1.1  2005/03/09 17:37:17  kuznets
 * Added node notification thread and execution control timeline
 *
 *
 * ===========================================================================
 */

#endif
