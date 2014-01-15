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

#include "ns_precise_time.hpp"


BEGIN_NCBI_SCOPE

class IRegistry;



// Queue parameters
struct SQueueParameters
{
    SQueueParameters();
    void Read(const IRegistry& reg, const string& sname);

    // Transit parameters; stored in memory and in the DB but not present
    // in the config files.
    int             kind;       // 0 - static, 1 - dynamic
    int             position;
    bool            delete_request;

    // General parameters, reconfigurable at run time
    string          qclass;
    CNSPreciseTime  timeout;
    CNSPreciseTime  notif_hifreq_interval;
    CNSPreciseTime  notif_hifreq_period;
    unsigned int    notif_lofreq_mult;
    CNSPreciseTime  notif_handicap;
    unsigned int    dump_buffer_size;
    unsigned int    dump_client_buffer_size;
    unsigned int    dump_aff_buffer_size;
    unsigned int    dump_group_buffer_size;
    CNSPreciseTime  run_timeout;
    string          program_name;
    unsigned int    failed_retries;
    CNSPreciseTime  blacklist_time;
    unsigned int    max_input_size;
    unsigned int    max_output_size;
    string          subm_hosts;
    string          wnode_hosts;
    CNSPreciseTime  wnode_timeout;
    CNSPreciseTime  pending_timeout;
    CNSPreciseTime  max_pending_wait_timeout;
    string          description;
    bool            scramble_job_keys;
    map<string,
        string>     linked_sections;

    // This parameter is not reconfigurable
    CNSPreciseTime  run_timeout_precision;

    // These fields are not for reading/writing/printing
    // It is used for QINF2 command which should print this status
    // together with the queue parameters. So it is handy to have it here and
    // set when parameters are retrieved.
    bool            refuse_submits;
    size_t          max_aff_slots;
    size_t          aff_slots_used;
    size_t          clients;
    size_t          groups;
    size_t          gc_backlog;
    size_t          notif_count;

  public:
    string Diff(const SQueueParameters &  other,
                bool                      include_class_cmp,
                bool                      include_description) const;
    string GetPrintableParameters(bool  include_class,
                                  bool  url_encoded) const;
    string ConfigSection(bool is_class) const;

    // Parameters are always: registry and section name
    CNSPreciseTime  ReadTimeout(const IRegistry &, const string &);
    CNSPreciseTime  ReadNotifHifreqInterval(const IRegistry &, const string &);
    CNSPreciseTime  ReadNotifHifreqPeriod(const IRegistry &, const string &);
    unsigned int    ReadNotifLofreqMult(const IRegistry &, const string &);
    CNSPreciseTime  ReadNotifHandicap(const IRegistry &, const string &);
    unsigned int    ReadDumpBufferSize(const IRegistry &, const string &);
    unsigned int    ReadDumpClientBufferSize(const IRegistry &, const string &);
    unsigned int    ReadDumpAffBufferSize(const IRegistry &, const string &);
    unsigned int    ReadDumpGroupBufferSize(const IRegistry &, const string &);
    CNSPreciseTime  ReadRunTimeout(const IRegistry &, const string &);
    string          ReadProgram(const IRegistry &, const string &);
    unsigned int    ReadFailedRetries(const IRegistry &, const string &);
    CNSPreciseTime  ReadBlacklistTime(const IRegistry &, const string &);
    unsigned int    ReadMaxInputSize(const IRegistry &, const string &);
    unsigned int    ReadMaxOutputSize(const IRegistry &, const string &);
    string          ReadSubmHosts(const IRegistry &, const string &);
    string          ReadWnodeHosts(const IRegistry &, const string &);
    CNSPreciseTime  ReadWnodeTimeout(const IRegistry &, const string &);
    CNSPreciseTime  ReadPendingTimeout(const IRegistry &, const string &);
    CNSPreciseTime  ReadMaxPendingWaitTimeout(const IRegistry &, const string &);
    string          ReadDescription(const IRegistry &, const string &);
    CNSPreciseTime  ReadRunTimeoutPrecision(const IRegistry &, const string &);
    bool            ReadScrambleJobKeys(const IRegistry &, const string &);
    map<string,
        string>     ReadLinkedSections(const IRegistry &, const string &);
};


template<typename TValueType>
void  AddParameterToDiffString(string &            output,
                               const string &      param_name,
                               const TValueType &  value_from,
                               const TValueType &  value_to)
{
    if (!output.empty())
        output += ", ";
    output += "\"" + param_name + "\" [" + NStr::NumericToString(value_from) +
              ", " + NStr::NumericToString(value_to) + "]";
    return;
}

template<> inline
void AddParameterToDiffString(string &            output,
                              const string &      param_name,
                              const string &      value_from,
                              const string &      value_to)
{
    if (!output.empty())
        output += ", ";
    output += "\"" + param_name + "\" [\"" + NStr::PrintableString(value_from) +
              "\", \"" + NStr::PrintableString(value_to) + "\"]";
    return;
}

template<> inline
void AddParameterToDiffString(string &          output,
                              const string &    param_name,
                              const bool &      value_from,
                              const bool &      value_to)
{
    if (!output.empty())
        output += ", ";
    output += "\"" + param_name + "\" [" + NStr::BoolToString(value_from) +
              ", " + NStr::BoolToString(value_to) + "]";
    return;
}


END_NCBI_SCOPE

#endif

