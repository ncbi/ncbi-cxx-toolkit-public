#ifndef NETSCHEDULE_QUEUE_PARAMETERS__HPP
#define NETSCHEDULE_QUEUE_PARAMETERS__HPP

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
 * File Description: Queue parameters
 *
 */

#include <string>


BEGIN_NCBI_SCOPE

class IRegistry;

/// Queue parameters
struct SQueueParameters
{
    SQueueParameters();
    void Read(const IRegistry& reg, const string& sname);

    /// General parameters, reconfigurable at run time
    int             timeout;
    double          notif_hifreq_interval;
    unsigned int    notif_hifreq_period;
    unsigned int    notif_lofreq_mult;
    int             run_timeout;
    string          program_name;
    int             failed_retries;
    time_t          blacklist_time;
    time_t          empty_lifetime;
    unsigned        max_input_size;
    unsigned        max_output_size;
    bool            deny_access_violations;
    bool            log_access_violations;
    string          subm_hosts;
    string          wnode_hosts;

    // This parameter is not reconfigurable
    int             run_timeout_precision;
};


END_NCBI_SCOPE

#endif

