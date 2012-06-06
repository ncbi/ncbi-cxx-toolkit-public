#ifndef NETCACHE__TIMERS__HPP
#define NETCACHE__TIMERS__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 */


namespace intr = boost::intrusive;


BEGIN_NCBI_SCOPE


class CSrvTask;


void InitTimers(void);
void TimerTick(void);
void FireAllTimers(void);
void RemoveTaskFromTimer(CSrvTask* task, TSrvTaskFlags new_flags);



struct STimerList_tag;
typedef intr::list_base_hook<intr::tag<STimerList_tag>,
                             intr::link_mode<intr::auto_unlink> >   TTimerListHook;

struct STimerTicket : public TTimerListHook
{
    int timer_time;
    CSrvTask* task;
};

typedef intr::list<STimerTicket,
                   intr::base_hook<TTimerListHook>,
                   intr::constant_time_size<false> >    TTimerList;


static const Uint1  kTimerLowBits = 8;
static const Uint1  kTimerMidBits = 5;
static const Uint1  kTimerMidLevels = 2;
static const int    kTimerLowMask = (1 << kTimerLowBits) - 1;
static const int    kTimerMidMask = (1 << kTimerMidBits) - 1;


END_NCBI_SCOPE

#endif /* NETCACHE__TIMERS__HPP */
