#ifndef NETSCHEDULE_GC_REGISTRY__HPP
#define NETSCHEDULE_GC_REGISTRY__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   Net schedule garbage collection registry
 *
 */

/// @file ns_gc_registry.hpp
/// NetSchedule garbage collection registry
///
/// @internal

#include <corelib/ncbimtx.hpp>

#include <map>

#include "ns_precise_time.hpp"
#include "ns_types.hpp"

BEGIN_NCBI_SCOPE


// The job information required for garbage collector
// to mark a job for deletion without reading the database
struct SJobGCInfo
{
    unsigned int    m_AffinityID;   // The job affinity ID
    unsigned int    m_GroupID;      // The job group ID
    CNSPreciseTime  m_LifeTime;     // The last second the job considered alive
    CNSPreciseTime  m_SubmitTime;   // Precise submit time
    CNSPreciseTime  m_ReadVacantTime;

    SJobGCInfo() :
        m_AffinityID(0), m_GroupID(0), m_LifeTime()
    {}

    SJobGCInfo(unsigned int  aff, unsigned int  group,
               const CNSPreciseTime &  life_time) :
        m_AffinityID(aff), m_GroupID(group), m_LifeTime(life_time)
    {
        m_ReadVacantTime = kTimeNever;
    }
};



// The garbage collector registry. It holds all the information required for
// garbage collecting jobs without reading from the DB.
class CJobGCRegistry
{
    public:
        CJobGCRegistry();
        ~CJobGCRegistry();

        void RegisterJob(unsigned int            job_id,
                         const CNSPreciseTime &  submit_time,
                         unsigned int            aff_id,
                         unsigned int            group_id,
                         const CNSPreciseTime &  life_time);
        void ChangeAffinityAndGroup(unsigned int    job_id,
                                    unsigned int    aff_id,
                                    unsigned int    group_id);
        bool DeleteIfTimedOut(unsigned int            job_id,       // in
                              const CNSPreciseTime &  current_time, // in
                              unsigned int *          aff_id,       // out: if deleted
                              unsigned int *          group_id);    // out: if deleted
        void UpdateLifetime(unsigned int            job_id,
                            const CNSPreciseTime &  life_time);
        void UpdateReadVacantTime(unsigned int            job_id,
                                  const CNSPreciseTime &  read_vacant_time);
        CNSPreciseTime  GetLifetime(unsigned int  job_id) const;
        unsigned int    GetAffinityID(unsigned int  job_id) const;
        unsigned int    GetGroupID(unsigned int  job_id) const;
        CNSPreciseTime  GetPreciseSubmitTime(unsigned int  job_id) const;
        CNSPreciseTime  GetPreciseReadVacantTime(unsigned int  job_id) const;
        bool  IsOutdatedJob(unsigned int            job_id,
                            ECommandGroup           cmd_group,
                            const CNSPreciseTime &  timeout) const;
        void Clear(void);

    private:
        mutable CFastMutex              m_Lock;         // Lock for the operations
        map<unsigned int, SJobGCInfo>   m_JobsAttrs;    // Jobs with attributes

    private:
        CJobGCRegistry(const CJobGCRegistry &);
        CJobGCRegistry & operator=(const CJobGCRegistry &);
};



END_NCBI_SCOPE

#endif /* NETSCHEDULE_GC_REGISTRY__HPP */

