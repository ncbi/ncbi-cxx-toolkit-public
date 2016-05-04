#ifndef NETSCHEDULE_SCOPE__HPP
#define NETSCHEDULE_SCOPE__HPP

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
 *   Net schedule scopes
 *
 */


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include "ns_types.hpp"

#include <map>
#include <vector>


BEGIN_NCBI_SCOPE


// forward declaration
class CQueue;


// Fixed identifier of a scope to tell that the jobs scope includes
// only the jobs without any scope
const string    kNoScopeOnly = "no-scope-only";



class CNSScopeRegistry
{
    public:
        CNSScopeRegistry();
        ~CNSScopeRegistry();

        size_t        size(void) const;
        bool          CanAccept(const string &  scope,
                                size_t  max_records) const;
        TNSBitVector  GetJobs(const string &  scope) const;
        TNSBitVector  GetAllJobsInScopes(void) const;
        void          AddJob(const string &  scope,
                             unsigned int  job_id);
        void          AddJobs(const string &  scope,
                              unsigned int  first_job_id,
                              unsigned int  count);
        void          RemoveJob(unsigned int  job_id);
        deque<string> GetScopeNames(void) const;
        string        GetJobScope(unsigned int  job_id) const;

        string  Print(const CQueue *  queue,
                      size_t  batch_size,
                      bool  verbose) const;
        void  Clear(void);

        unsigned int  CollectGarbage(unsigned int  max_to_del);
        unsigned int  CheckRemoveCandidates(void);
    private:
        typedef map< string,
                     TNSBitVector >     TScopeToJobsMap;

    private:
        TScopeToJobsMap     m_ScopeToJobs;

        mutable CMutex      m_Lock;             // Lock for the operations
        TNSBitVector        m_AllScopedJobs;

    private:
        string  x_PrintOne(const string &  scope_name,
                           const TNSBitVector &  jobs,
                           const CQueue *  queue,
                           bool  verbose) const;
        string  x_PrintSelected(const deque<string> &  batch,
                                const CQueue *  queue,
                                bool  verbose) const;

};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_SCOPE__HPP */

