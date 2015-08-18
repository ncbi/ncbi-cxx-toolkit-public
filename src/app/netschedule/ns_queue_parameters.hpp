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
#include <connect/services/json_over_uttp.hpp>

#include "ns_precise_time.hpp"


BEGIN_NCBI_SCOPE

class IRegistry;


// Queue parameters
struct SQueueParameters
{
    SQueueParameters();

    // Returns true if evrything is OK with the linked sections
    bool ReadQueueClass(const IRegistry &  reg, const string &  sname,
                        vector<string> &  warnings);
    // Returns true if evrything is OK with the linked sections
    bool ReadQueue(const IRegistry &  reg, const string &  sname,
                   const map<string, SQueueParameters> &  queue_classes,
                   vector<string> &  warnings);

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
    CNSPreciseTime  read_timeout;
    string          program_name;
    unsigned int    failed_retries;
    unsigned int    read_failed_retries;
    CNSPreciseTime  blacklist_time;
    CNSPreciseTime  read_blacklist_time;
    unsigned int    max_input_size;
    unsigned int    max_output_size;
    string          subm_hosts;
    string          wnode_hosts;
    string          reader_hosts;
    CNSPreciseTime  wnode_timeout;
    CNSPreciseTime  reader_timeout;
    CNSPreciseTime  pending_timeout;
    CNSPreciseTime  max_pending_wait_timeout;
    CNSPreciseTime  max_pending_read_wait_timeout;
    string          description;
    bool            scramble_job_keys;
    CNSPreciseTime  client_registry_timeout_worker_node;
    unsigned int    client_registry_min_worker_nodes;
    CNSPreciseTime  client_registry_timeout_admin;
    unsigned int    client_registry_min_admins;
    CNSPreciseTime  client_registry_timeout_submitter;
    unsigned int    client_registry_min_submitters;
    CNSPreciseTime  client_registry_timeout_reader;
    unsigned int    client_registry_min_readers;
    CNSPreciseTime  client_registry_timeout_unknown;
    unsigned int    client_registry_min_unknowns;
    map<string,
        string>     linked_sections;

    // These fields are not for reading/writing/printing
    // It is used for QINF2 command which should print this status
    // together with the queue parameters. So it is handy to have it here and
    // set when parameters are retrieved.
    bool            refuse_submits;
    int             pause_status;
    size_t          max_aff_slots;
    size_t          aff_slots_used;
    size_t          clients;
    size_t          groups;
    size_t          gc_backlog;
    size_t          notif_count;

  public:
    CJsonNode Diff(const SQueueParameters &  other,
                   bool                      include_class_cmp,
                   bool                      include_description) const;
    string GetPrintableParameters(bool  include_class,
                                  bool  url_encoded) const;
    string ConfigSection(bool is_class) const;
    CNSPreciseTime  CalculateRuntimePrecision(void) const;


    // Parameters are always: registry and section name
    string          ReadClass(const IRegistry &, const string &,
                              vector<string> &);
    CNSPreciseTime  ReadTimeout(const IRegistry &, const string &,
                                vector<string> &);
    CNSPreciseTime  ReadNotifHifreqInterval(const IRegistry &, const string &,
                                            vector<string> &);
    CNSPreciseTime  ReadNotifHifreqPeriod(const IRegistry &, const string &,
                                          vector<string> &);
    unsigned int    ReadNotifLofreqMult(const IRegistry &, const string &,
                                        vector<string> &);
    CNSPreciseTime  ReadNotifHandicap(const IRegistry &, const string &,
                                      vector<string> &);
    unsigned int    ReadDumpBufferSize(const IRegistry &, const string &,
                                       vector<string> &);
    unsigned int    ReadDumpClientBufferSize(const IRegistry &, const string &,
                                             vector<string> &);
    unsigned int    ReadDumpAffBufferSize(const IRegistry &, const string &,
                                          vector<string> &);
    unsigned int    ReadDumpGroupBufferSize(const IRegistry &, const string &,
                                            vector<string> &);
    CNSPreciseTime  ReadRunTimeout(const IRegistry &, const string &,
                                   vector<string> &);
    CNSPreciseTime  ReadReadTimeout(const IRegistry &, const string &,
                                    vector<string> &);
    string          ReadProgram(const IRegistry &, const string &,
                                vector<string> &);
    unsigned int    ReadFailedRetries(const IRegistry &, const string &,
                                      vector<string> &);
    unsigned int    ReadReadFailedRetries(const IRegistry &, const string &,
                                          vector<string> &,
                                          unsigned int  failed_retries);
    CNSPreciseTime  ReadBlacklistTime(const IRegistry &, const string &,
                                      vector<string> &);
    CNSPreciseTime  ReadReadBlacklistTime(const IRegistry &, const string &,
                                          vector<string> &,
                                          const CNSPreciseTime &);
    unsigned int    ReadMaxInputSize(const IRegistry &, const string &,
                                     vector<string> &);
    unsigned int    ReadMaxOutputSize(const IRegistry &, const string &,
                                      vector<string> &);
    string          ReadSubmHosts(const IRegistry &, const string &,
                                  vector<string> &);
    string          ReadWnodeHosts(const IRegistry &, const string &,
                                   vector<string> &);
    string          ReadReaderHosts(const IRegistry &, const string &,
                                    vector<string> &);
    CNSPreciseTime  ReadWnodeTimeout(const IRegistry &, const string &,
                                     vector<string> &);
    CNSPreciseTime  ReadReaderTimeout(const IRegistry &, const string &,
                                      vector<string> &);
    CNSPreciseTime  ReadPendingTimeout(const IRegistry &, const string &,
                                       vector<string> &);
    CNSPreciseTime  ReadMaxPendingWaitTimeout(const IRegistry &,
                                              const string &,
                                              vector<string> &);
    CNSPreciseTime  ReadMaxPendingReadWaitTimeout(const IRegistry &,
                                                  const string &,
                                                  vector<string> &);
    string          ReadDescription(const IRegistry &, const string &,
                                    vector<string> &);
    bool            ReadScrambleJobKeys(const IRegistry &, const string &,
                                        vector<string> &);
    CNSPreciseTime  ReadClientRegistryTimeoutWorkerNode(const IRegistry &,
                                                        const string &,
                                                        vector<string> &);
    unsigned int    ReadClientRegistryMinWorkerNodes(const IRegistry &,
                                                     const string &,
                                                     vector<string> &);
    CNSPreciseTime  ReadClientRegistryTimeoutAdmin(const IRegistry &,
                                                   const string &,
                                                   vector<string> &);
    unsigned int    ReadClientRegistryMinAdmins(const IRegistry &,
                                                const string &,
                                                vector<string> &);
    CNSPreciseTime  ReadClientRegistryTimeoutSubmitter(const IRegistry &,
                                                       const string &,
                                                       vector<string> &);
    unsigned int    ReadClientRegistryMinSubmitters(const IRegistry &,
                                                    const string &,
                                                    vector<string> &);
    CNSPreciseTime  ReadClientRegistryTimeoutReader(const IRegistry &,
                                                    const string &,
                                                    vector<string> &);
    unsigned int    ReadClientRegistryMinReaders(const IRegistry &,
                                                 const string &,
                                                 vector<string> &);
    CNSPreciseTime  ReadClientRegistryTimeoutUnknown(const IRegistry &,
                                                     const string &,
                                                     vector<string> &);
    unsigned int    ReadClientRegistryMinUnknowns(const IRegistry &,
                                                  const string &,
                                                  vector<string> &);
    map<string,
        string>     ReadLinkedSections(const IRegistry &,
                                       const string &  sname,
                                       vector<string> &  warnings,
                                       bool *  linked_sections_ok);
};


template<typename TValueType>
void  AddParameterToDiff(CJsonNode &         output,
                         const string &      param_name,
                         const TValueType &  value_from,
                         const TValueType &  value_to)
{
    CJsonNode       values = CJsonNode::NewArrayNode();
    values.AppendString(NStr::NumericToString(value_from));
    values.AppendString(NStr::NumericToString(value_to));
    output.SetByKey(param_name, values);
}

template<> inline
void AddParameterToDiff(CJsonNode &         output,
                        const string &      param_name,
                        const string &      value_from,
                        const string &      value_to)
{
    CJsonNode       values = CJsonNode::NewArrayNode();
    values.AppendString(NStr::PrintableString(value_from));
    values.AppendString(NStr::PrintableString(value_to));
    output.SetByKey(param_name, values);
}

template<> inline
void AddParameterToDiff(CJsonNode &       output,
                        const string &    param_name,
                        const bool &      value_from,
                        const bool &      value_to)
{
    CJsonNode       values = CJsonNode::NewArrayNode();
    values.AppendBoolean(value_from);
    values.AppendBoolean(value_to);
    output.SetByKey(param_name, values);
}

template<> inline
void AddParameterToDiff(CJsonNode &       output,
                        const string &    param_name,
                        const double &    value_from,
                        const double &    value_to)
{
    CJsonNode       values = CJsonNode::NewArrayNode();
    values.AppendDouble(value_from);
    values.AppendDouble(value_to);
    output.SetByKey(param_name, values);
}

template<> inline
void AddParameterToDiff(CJsonNode &       output,
                        const string &    param_name,
                        const long &      value_from,
                        const long &      value_to)
{
    CJsonNode       values = CJsonNode::NewArrayNode();
    values.AppendInteger(value_from);
    values.AppendInteger(value_to);
    output.SetByKey(param_name, values);
}



END_NCBI_SCOPE

#endif

