#ifndef ACTIVE_PROC_PER_REQUEST__HPP
#define ACTIVE_PROC_PER_REQUEST__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */


#include <connect/services/json_over_uttp.hpp>
USING_NCBI_SCOPE;

#include "psgs_request.hpp"

#define MAX_PROCESSOR_KINDS     16

// Forward declaration
struct SProcessorGroup;


// Number of processors
struct SProcPerRequest
{
    size_t      m_ProcessorCounter[MAX_PROCESSOR_KINDS];
    size_t      m_RequestCounter;

    void Reset(void)
    {
        m_RequestCounter = 0;
        for (size_t  k = 0; k < sizeof(m_ProcessorCounter) / sizeof(size_t); ++k) {
            m_ProcessorCounter[k] = 0;
        }
    }
};


// Processors per request
struct SActiveProcPerRequest
{
    SProcPerRequest     m_ProcPerRequest[CPSGS_Request::ePSGS_UnknownRequest];
    vector<string>      m_RegisteredProcessorGroups;
    size_t              m_ProcPriorityToIndex[MAX_PROCESSOR_KINDS];

    SActiveProcPerRequest()
    {
        for (size_t  k = 0; k < sizeof(m_ProcPerRequest) / sizeof(SProcPerRequest); ++k)
            m_ProcPerRequest[k].Reset();
    }

    size_t  GetProcessorIndex(TProcessorPriority  priority) const
    {
        return m_ProcPriorityToIndex[priority];
    }
};

void RegisterProcessorGroupName(const string &  group_name,
                                TProcessorPriority  priority);
void RegisterActiveProcGroup(CPSGS_Request::EPSGS_Type  request_type,
                             SProcessorGroup *  proc_group);
void UnregisterActiveProcGroup(CPSGS_Request::EPSGS_Type  request_type,
                               SProcessorGroup *  proc_group);
size_t  GetActiveProcGroupCounter(void);
SActiveProcPerRequest  GetActiveProcGroupSnapshot(void);
void PopulatePerRequestMomentousDictionary(CJsonNode &  dict);

#endif

