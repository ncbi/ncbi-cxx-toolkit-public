#ifndef CONNECT___NCBI_MISC__HPP
#define CONNECT___NCBI_MISC__HPP

/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Miscellaneous C++ connect stuff
 *
 */

#include <connect/connect_export.h>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbitype.h>
#include <list>
#include <utility>


/** @addtogroup UtilityFunc
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/***********************************************************************
 *                             Rate Monitor                            *
 ***********************************************************************/

class NCBI_XCONNECT_EXPORT CRateMonitor {
public:
    typedef pair<Uint8, double> TMark;

    /// Monitor position progressing in time, calculate speed and
    /// estimate time to complete the job (if the final size is known).
    /// @param minspan
    ///   minimal time distance between marks (must be greater than 0)
    /// @param maxspan
    ///   maximal time span covered by measurements (older marks popped out)
    /// @param weight
    ///   for weighted rate calculations (current:remainig ratio),
    ///   must be within the interval (0, 1) (excluding both ends);
    ///   a value close to one (e.g. 0.9) makes recent marks more significant
    /// @precision
    ///   fraction of minspan to consider sufficient to add next mark,
    ///   must be within the interval (0, 1] (excluding 0 but including 1)
    CRateMonitor(double minspan = 0.5, double maxspan   = 10.0,
                 double weight  = 0.5, double precision = 0.95)
        : kMinSpan(minspan), kMaxSpan(minspan > maxspan ?
                                      minspan + maxspan : maxspan),
          kWeight(weight), kSpan(kMinSpan * precision),
          m_Rate(0.0), m_Size(0)
    { }

    /// Set size of the anticipated job, clear all prior measurements
    void   SetSize(Uint8 size);

    /// Get size previously set
    Uint8  GetSize(void) const { return m_Size; }

    /// Get current progress position (position 0 when job starts)
    Uint8  GetPos (void) const;

    /// Get current time (time 0.0 when job starts)
    double GetTime(void) const;

    /// Submit a mark of the job progress
    /// @param pos
    ///   current position (0-based)
    /// @param time
    ///   time spent from the beginning of the job (since time 0.0)
    void   Mark(Uint8 pos, double time);

    /// How fast the recent rate has been, in positions per time unit,
    /// using the weighted formula
    /// @return
    ///   zero if cannot estimate
    double GetRate(void) const;

    /// How fast the average pace has been so far, in positions per time unit
    /// @return
    ///   zero if cannot estimate
    double GetPace(void) const;

    /// How long it will take to complete, at the current rate
    /// @return
    ///   negative value if cannot estimate
    double GetETA          (void) const;

    /// How long it will take to complete, at the average pace
    /// @return
    ///   negative value if cannot estimate
    double GetTimeRemaining(void) const;

protected:
    const double kMinSpan;
    const double kMaxSpan;
    const double kWeight;
    const double kSpan;

    mutable double m_Rate;  ///< Cached rate from last calculation
    list<TMark>    m_Data;  ///< Measurements as submitted by Mark()
    Uint8          m_Size;  ///< Total size of job to be performed
};


inline void   CRateMonitor::SetSize(Uint8 size)
{
    m_Rate = 0.0;
    m_Size = size;
    m_Data.clear();
}


inline Uint8  CRateMonitor::GetPos(void) const
{
    return m_Data.empty() ?   0 : m_Data.front().first;
}


inline double CRateMonitor::GetTime(void) const
{
    return m_Data.empty() ? 0.0 : m_Data.front().second;
}


inline double CRateMonitor::GetPace(void) const
{
    return GetTime() ? m_Data.front().first / m_Data.front().second : 0.0;
}


END_NCBI_SCOPE


/* @} */

#endif  // CONNECT___NCBI_MISC__HPP
