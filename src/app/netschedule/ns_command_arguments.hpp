#ifndef NETSCHEDULE_JS_COMMAND_ARGUMENTS__HPP
#define NETSCHEDULE_JS_COMMAND_ARGUMENTS__HPP

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
 * File Description: job request parameters
 *
 */

#include <connect/services/netservice_protocol_parser.hpp>
#include <string>

#include "ns_types.hpp"


BEGIN_NCBI_SCOPE


struct SNSCommandArguments
{
    unsigned int    job_id;
    int             job_return_code;
    unsigned int    port;
    unsigned int    timeout;
    unsigned int    job_mask;
    TJobStatus      job_status;

    string          auth_token;
    string          input;
    string          output;
    string          affinity_token;
    string          job_key;
    string          err_msg;
    string          comment;
    string          ip;                 // ?
    string          option;
    string          progress_msg;
    string          qname;
    string          qclass;
    string          sid;
    string          job_status_string;
    string          aff_to_add;
    string          aff_to_del;

    bool            any_affinity;
    bool            wnode_affinity;

    void AssignValues(const TNSProtoParams &  params);

    private:
        void x_Reset();
};

END_NCBI_SCOPE

#endif

