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

#include <ncbi_pch.hpp>

#include <connect/services/netschedule_api.hpp>

#include "ns_queue_parameters.hpp"
#include "ns_types.hpp"
#include "ns_db.hpp"
#include "ns_queue.hpp"
#include "ns_ini_params.hpp"



BEGIN_NCBI_SCOPE


SQueueParameters::SQueueParameters() :
    kind(CQueue::eKindStatic),
    position(-1),
    delete_request(false),
    qclass(""),
    timeout(default_timeout),
    notif_hifreq_interval(default_notif_hifreq_interval),
    notif_hifreq_period(default_notif_hifreq_period),
    notif_lofreq_mult(default_notif_lofreq_mult),
    notif_handicap(default_notif_handicap),
    dump_buffer_size(default_dump_buffer_size),
    dump_client_buffer_size(default_dump_client_buffer_size),
    dump_aff_buffer_size(default_dump_aff_buffer_size),
    dump_group_buffer_size(default_dump_group_buffer_size),
    run_timeout(default_run_timeout),
    read_timeout(default_read_timeout),
    program_name(""),
    failed_retries(default_failed_retries),
    read_failed_retries(default_failed_retries),    // See CXX-5161, the same
                                                    // default as for
                                                    // failed_retries
    blacklist_time(default_blacklist_time),
    read_blacklist_time(default_blacklist_time),    // See CXX-4993, the same
                                                    // default as for
                                                    // blacklist_time
    max_input_size(kNetScheduleMaxDBDataSize),
    max_output_size(kNetScheduleMaxDBDataSize),
    subm_hosts(""),
    wnode_hosts(""),
    reader_hosts(""),
    wnode_timeout(default_wnode_timeout),
    reader_timeout(default_reader_timeout),
    pending_timeout(default_pending_timeout),
    max_pending_wait_timeout(default_max_pending_wait_timeout),
    max_pending_read_wait_timeout(default_max_pending_read_wait_timeout),
    description(""),
    scramble_job_keys(default_scramble_job_keys),
    client_registry_timeout_worker_node(
                        default_client_registry_timeout_worker_node),
    client_registry_min_worker_nodes(default_client_registry_min_worker_nodes),
    client_registry_timeout_admin(default_client_registry_timeout_admin),
    client_registry_min_admins(default_client_registry_min_admins),
    client_registry_timeout_submitter(
                        default_client_registry_timeout_submitter),
    client_registry_min_submitters(default_client_registry_min_submitters),
    client_registry_timeout_reader(default_client_registry_timeout_reader),
    client_registry_min_readers(default_client_registry_min_readers),
    client_registry_timeout_unknown(default_client_registry_timeout_unknown),
    client_registry_min_unknowns(default_client_registry_min_unknowns)
{}



bool SQueueParameters::ReadQueueClass(const IRegistry &  reg,
                                      const string &  sname,
                                      vector<string> &  warnings)
{
    qclass = "";    // No class name for the queue class

    timeout = ReadTimeout(reg, sname, warnings);
    notif_hifreq_interval = ReadNotifHifreqInterval(reg, sname, warnings);
    notif_hifreq_period = ReadNotifHifreqPeriod(reg, sname, warnings);
    notif_lofreq_mult = ReadNotifLofreqMult(reg, sname, warnings);
    notif_handicap = ReadNotifHandicap(reg, sname, warnings);
    dump_buffer_size = ReadDumpBufferSize(reg, sname, warnings);
    dump_client_buffer_size = ReadDumpClientBufferSize(reg, sname, warnings);
    dump_aff_buffer_size = ReadDumpAffBufferSize(reg, sname, warnings);
    dump_group_buffer_size = ReadDumpGroupBufferSize(reg, sname, warnings);
    run_timeout = ReadRunTimeout(reg, sname, warnings);
    read_timeout = ReadReadTimeout(reg, sname, warnings);
    program_name = ReadProgram(reg, sname, warnings);
    failed_retries = ReadFailedRetries(reg, sname, warnings);
    read_failed_retries = ReadReadFailedRetries(reg, sname, warnings,
                                                failed_retries);
    blacklist_time = ReadBlacklistTime(reg, sname, warnings);
    read_blacklist_time = ReadReadBlacklistTime(reg, sname, warnings,
                                                blacklist_time);
    max_input_size = ReadMaxInputSize(reg, sname, warnings);
    max_output_size = ReadMaxOutputSize(reg, sname, warnings);
    subm_hosts = ReadSubmHosts(reg, sname, warnings);
    wnode_hosts = ReadWnodeHosts(reg, sname, warnings);
    reader_hosts = ReadReaderHosts(reg, sname, warnings);
    wnode_timeout = ReadWnodeTimeout(reg, sname, warnings);
    reader_timeout = ReadReaderTimeout(reg, sname, warnings);
    pending_timeout = ReadPendingTimeout(reg, sname, warnings);
    max_pending_wait_timeout = ReadMaxPendingWaitTimeout(reg, sname, warnings);
    max_pending_read_wait_timeout = ReadMaxPendingReadWaitTimeout(reg, sname,
                                                                  warnings);
    description = ReadDescription(reg, sname, warnings);
    scramble_job_keys = ReadScrambleJobKeys(reg, sname, warnings);
    client_registry_timeout_worker_node = ReadClientRegistryTimeoutWorkerNode(
                                                reg, sname, warnings);
    client_registry_min_worker_nodes = ReadClientRegistryMinWorkerNodes(
                                                reg, sname, warnings);
    client_registry_timeout_admin = ReadClientRegistryTimeoutAdmin(reg, sname,
                                                                   warnings);
    client_registry_min_admins = ReadClientRegistryMinAdmins(reg, sname,
                                                             warnings);
    client_registry_timeout_submitter = ReadClientRegistryTimeoutSubmitter(
                                                reg, sname, warnings);
    client_registry_min_submitters = ReadClientRegistryMinSubmitters(reg, sname,
                                                                     warnings);
    client_registry_timeout_reader = ReadClientRegistryTimeoutReader(reg, sname,
                                                                     warnings);
    client_registry_min_readers = ReadClientRegistryMinReaders(reg, sname,
                                                               warnings);
    client_registry_timeout_unknown = ReadClientRegistryTimeoutUnknown(
                                                reg, sname, warnings);
    client_registry_min_unknowns = ReadClientRegistryMinUnknowns(reg, sname,
                                                                 warnings);

    bool    linked_sections_ok;
    linked_sections = ReadLinkedSections(reg, sname, warnings,
                                         &linked_sections_ok);
    return linked_sections_ok;
}


bool SQueueParameters::ReadQueue(const IRegistry &  reg, const string &  sname,
                                 const TQueueParams &  queue_classes,
                                 vector<string> &  warnings)
{
    string      queue_class = ReadClass(reg, sname, warnings);
    if (queue_class.empty()) {
        // There is no class so it exactly matches a queue class
        return ReadQueueClass(reg, sname, warnings);
    }

    // This queue derives from a class
    TQueueParams::const_iterator  found = queue_classes.find(queue_class);
    if (found == queue_classes.end()) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "class") +
                           ". It refers to un unknown queue class");
        // Read the queue as if there were no queue class
        return ReadQueueClass(reg, sname, warnings);
    }

    // Here: the queue class is here and the queue needs to derive everything
    // from the queue class
    *this = found->second;
    // The class name has been reset in the assignment above
    qclass = queue_class;

    // Override the class with what found in the section
    list<string>        values;
    reg.EnumerateEntries(sname, &values);

    // Avoid to have double warnings if so
    unsigned int        failed_retries_preread = ReadFailedRetries(reg, sname,
                                                                   warnings);
    CNSPreciseTime      blacklist_time_preread = ReadBlacklistTime(reg, sname,
                                                                   warnings);

    for (list<string>::const_iterator  val = values.begin();
         val != values.end(); ++val) {
        if (*val == "timeout")
            timeout = ReadTimeout(reg, sname, warnings);
        else if (*val == "notif_hifreq_interval")
            notif_hifreq_interval =
                ReadNotifHifreqInterval(reg, sname, warnings);
        else if (*val == "notif_hifreq_period")
            notif_hifreq_period = ReadNotifHifreqPeriod(reg, sname, warnings);
        else if (*val == "notif_lofreq_mult")
            notif_lofreq_mult = ReadNotifLofreqMult(reg, sname, warnings);
        else if (*val == "notif_handicap")
            notif_handicap = ReadNotifHandicap(reg, sname, warnings);
        else if (*val == "dump_buffer_size")
            dump_buffer_size = ReadDumpBufferSize(reg, sname, warnings);
        else if (*val == "dump_client_buffer_size")
            dump_client_buffer_size =
                ReadDumpClientBufferSize(reg, sname, warnings);
        else if (*val == "dump_aff_buffer_size")
            dump_aff_buffer_size = ReadDumpAffBufferSize(reg, sname, warnings);
        else if (*val == "dump_group_buffer_size")
            dump_group_buffer_size =
                ReadDumpGroupBufferSize(reg, sname, warnings);
        else if (*val == "run_timeout")
            run_timeout = ReadRunTimeout(reg, sname, warnings);
        else if (*val == "read_timeout")
            read_timeout = ReadReadTimeout(reg, sname, warnings);
        else if (*val == "program")
            program_name = ReadProgram(reg, sname, warnings);
        else if (*val == "failed_retries")
            failed_retries = failed_retries_preread;
        else if (*val == "read_failed_retries")
            // See CXX-5161, the read_failed_retries depends on failed_retries
            read_failed_retries =
                ReadReadFailedRetries(reg, sname, warnings,
                                      failed_retries_preread);
        else if (*val == "blacklist_time")
            blacklist_time = blacklist_time_preread;
        else if (*val == "read_blacklist_time")
            // See CXX-4993, the read_blacklist_time depends on blacklist_time
            read_blacklist_time =
                ReadReadBlacklistTime(reg, sname, warnings,
                                      blacklist_time_preread);
        else if (*val == "max_input_size")
            max_input_size = ReadMaxInputSize(reg, sname, warnings);
        else if (*val == "max_output_size")
            max_output_size = ReadMaxOutputSize(reg, sname, warnings);
        else if (*val == "subm_host")
            subm_hosts = ReadSubmHosts(reg, sname, warnings);
        else if (*val == "wnode_host")
            wnode_hosts = ReadWnodeHosts(reg, sname, warnings);
        else if (*val == "reader_host")
            reader_hosts = ReadReaderHosts(reg, sname, warnings);
        else if (*val == "wnode_timeout")
            wnode_timeout = ReadWnodeTimeout(reg, sname, warnings);
        else if (*val == "reader_timeout")
            reader_timeout = ReadReaderTimeout(reg, sname, warnings);
        else if (*val == "pending_timeout")
            pending_timeout = ReadPendingTimeout(reg, sname, warnings);
        else if (*val == "max_pending_wait_timeout")
            max_pending_wait_timeout =
                ReadMaxPendingWaitTimeout(reg, sname, warnings);
        else if (*val == "max_pending_read_wait_timeout")
            max_pending_read_wait_timeout =
                ReadMaxPendingReadWaitTimeout(reg, sname, warnings);
        else if (*val == "description")
            description = ReadDescription(reg, sname, warnings);
        else if (*val == "scramble_job_keys")
            scramble_job_keys = ReadScrambleJobKeys(reg, sname, warnings);
        else if (*val == "client_registry_timeout_worker_node")
            client_registry_timeout_worker_node =
                ReadClientRegistryTimeoutWorkerNode(reg, sname, warnings);
        else if (*val == "client_registry_min_worker_nodes")
            client_registry_min_worker_nodes =
                ReadClientRegistryMinWorkerNodes(reg, sname, warnings);
        else if (*val == "client_registry_timeout_admin")
            client_registry_timeout_admin =
                ReadClientRegistryTimeoutAdmin(reg, sname, warnings);
        else if (*val == "client_registry_min_admins")
            client_registry_min_admins =
                ReadClientRegistryMinAdmins(reg, sname, warnings);
        else if (*val == "client_registry_timeout_submitter")
            client_registry_timeout_submitter =
                ReadClientRegistryTimeoutSubmitter(reg, sname, warnings);
        else if (*val == "client_registry_min_submitters")
            client_registry_min_submitters =
                ReadClientRegistryMinSubmitters(reg, sname, warnings);
        else if (*val == "client_registry_timeout_reader")
            client_registry_timeout_reader =
                ReadClientRegistryTimeoutReader(reg, sname, warnings);
        else if (*val == "client_registry_min_readers")
            client_registry_min_readers =
                ReadClientRegistryMinReaders(reg, sname, warnings);
        else if (*val == "client_registry_timeout_unknown")
            client_registry_timeout_unknown =
                ReadClientRegistryTimeoutUnknown(reg, sname, warnings);
        else if (*val == "client_registry_min_unknowns")
            client_registry_min_unknowns =
                ReadClientRegistryMinUnknowns(reg, sname, warnings);
    }

    // Apply linked sections if so
    bool                linked_sections_ok;
    map<string, string> queue_linked_sections =
                            ReadLinkedSections(reg, sname, warnings,
                                               &linked_sections_ok);
    for (map<string, string>::const_iterator  k = queue_linked_sections.begin();
         k != queue_linked_sections.end(); ++k)
        linked_sections[k->first] = k->second;

    // After deriving from a queue class there might be some restrictions
    // broken. Check them here.
    if (client_registry_timeout_worker_node <= wnode_timeout) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_timeout_worker_node")
                           + ". It must be > " +
                           NStr::NumericToString(double(wnode_timeout)));
        double  fixed_timeout =
                max(
                    max(double(default_client_registry_timeout_worker_node),
                        double(wnode_timeout + wnode_timeout)),
                    double(run_timeout + run_timeout));
        client_registry_timeout_worker_node = CNSPreciseTime(fixed_timeout);
    }

    if (client_registry_timeout_reader <= reader_timeout) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_timeout_reader")
                           + ". It must be > " +
                           NStr::NumericToString(double(reader_timeout)));
        double  fixed_timeout =
                max(
                    max(double(default_client_registry_timeout_reader),
                        double(reader_timeout + reader_timeout)),
                    double(read_timeout + read_timeout));
        client_registry_timeout_reader = CNSPreciseTime(fixed_timeout);
    }
    return linked_sections_ok;
}



// Returns descriptive diff
// Empty dictionary if there is no difference
CJsonNode
SQueueParameters::Diff(const SQueueParameters &  other,
                       bool                      include_class_cmp,
                       bool                      include_description) const
{
    CJsonNode       diff = CJsonNode::NewObjectNode();

    // It does not make sense to compare the qclass field for a queue class
    if (include_class_cmp && qclass != other.qclass)
        AddParameterToDiff(diff, "class",
                           qclass, other.qclass);

    if (timeout.Sec() != other.timeout.Sec())
        AddParameterToDiff(diff, "timeout",
                           timeout.Sec(), other.timeout.Sec());

    if (notif_hifreq_interval != other.notif_hifreq_interval)
        AddParameterToDiff(
                diff, "notif_hifreq_interval",
                NS_FormatPreciseTimeAsSec(notif_hifreq_interval),
                NS_FormatPreciseTimeAsSec(other.notif_hifreq_interval));

    if (notif_hifreq_period != other.notif_hifreq_period)
        AddParameterToDiff(
                diff, "notif_hifreq_period",
                NS_FormatPreciseTimeAsSec(notif_hifreq_period),
                NS_FormatPreciseTimeAsSec(other.notif_hifreq_period));

    if (notif_lofreq_mult != other.notif_lofreq_mult)
        AddParameterToDiff(diff, "notif_lofreq_mult",
                           notif_lofreq_mult, other.notif_lofreq_mult);

    if (notif_handicap != other.notif_handicap)
        AddParameterToDiff(
                diff, "notif_handicap",
                NS_FormatPreciseTimeAsSec(notif_handicap),
                NS_FormatPreciseTimeAsSec(other.notif_handicap));

    if (dump_buffer_size != other.dump_buffer_size)
        AddParameterToDiff(diff, "dump_buffer_size",
                           dump_buffer_size, other.dump_buffer_size);

    if (dump_client_buffer_size != other.dump_client_buffer_size)
        AddParameterToDiff(diff, "dump_client_buffer_size",
                           dump_client_buffer_size,
                           other.dump_client_buffer_size);

    if (dump_aff_buffer_size != other.dump_aff_buffer_size)
        AddParameterToDiff(diff, "dump_aff_buffer_size",
                           dump_aff_buffer_size, other.dump_aff_buffer_size);

    if (dump_group_buffer_size != other.dump_group_buffer_size)
        AddParameterToDiff(diff, "dump_group_buffer_size",
                           dump_group_buffer_size,
                           other.dump_group_buffer_size);

    if (run_timeout != other.run_timeout)
        AddParameterToDiff(
                diff, "run_timeout",
                NS_FormatPreciseTimeAsSec(run_timeout),
                NS_FormatPreciseTimeAsSec(other.run_timeout));

    if (read_timeout != other.read_timeout)
        AddParameterToDiff(
                diff, "read_timeout",
                NS_FormatPreciseTimeAsSec(read_timeout),
                NS_FormatPreciseTimeAsSec(other.read_timeout));

    if (program_name != other.program_name)
        AddParameterToDiff(diff, "program",
                           program_name, other.program_name);

    if (failed_retries != other.failed_retries)
        AddParameterToDiff(diff, "failed_retries",
                           failed_retries, other.failed_retries);

    if (read_failed_retries != other.read_failed_retries)
        AddParameterToDiff(diff, "read_failed_retries",
                           read_failed_retries, other.read_failed_retries);

    if (blacklist_time != other.blacklist_time)
        AddParameterToDiff(
                diff, "blacklist_time",
                NS_FormatPreciseTimeAsSec(blacklist_time),
                NS_FormatPreciseTimeAsSec(other.blacklist_time));

    if (read_blacklist_time != other.read_blacklist_time)
        AddParameterToDiff(
                diff, "read_blacklist_time",
                NS_FormatPreciseTimeAsSec(read_blacklist_time),
                NS_FormatPreciseTimeAsSec(other.read_blacklist_time));

    if (max_input_size != other.max_input_size)
        AddParameterToDiff(diff, "max_input_size",
                           max_input_size, other.max_input_size);

    if (max_output_size != other.max_output_size)
        AddParameterToDiff(diff, "max_output_size",
                           max_output_size, other.max_output_size);

    if (subm_hosts != other.subm_hosts)
        AddParameterToDiff(diff, "subm_host",
                           subm_hosts, other.subm_hosts);

    if (wnode_hosts != other.wnode_hosts)
        AddParameterToDiff(diff, "wnode_host",
                           wnode_hosts, other.wnode_hosts);

    if (reader_hosts != other.reader_hosts)
        AddParameterToDiff(diff, "reader_host",
                           reader_hosts, other.reader_hosts);

    if (wnode_timeout != other.wnode_timeout)
        AddParameterToDiff(
                diff, "wnode_timeout",
                NS_FormatPreciseTimeAsSec(wnode_timeout),
                NS_FormatPreciseTimeAsSec(other.wnode_timeout));

    if (reader_timeout != other.reader_timeout)
        AddParameterToDiff(
                diff, "reader_timeout",
                NS_FormatPreciseTimeAsSec(reader_timeout),
                NS_FormatPreciseTimeAsSec(other.reader_timeout));

    if (pending_timeout != other.pending_timeout)
        AddParameterToDiff(
                diff, "pending_timeout",
                NS_FormatPreciseTimeAsSec(pending_timeout),
                NS_FormatPreciseTimeAsSec(other.pending_timeout));

    if (max_pending_wait_timeout != other.max_pending_wait_timeout)
        AddParameterToDiff(
                diff, "max_pending_wait_timeout",
                NS_FormatPreciseTimeAsSec(max_pending_wait_timeout),
                NS_FormatPreciseTimeAsSec(other.max_pending_wait_timeout));

    if (max_pending_read_wait_timeout != other.max_pending_read_wait_timeout)
        AddParameterToDiff(
                diff, "max_pending_read_wait_timeout",
                NS_FormatPreciseTimeAsSec(max_pending_read_wait_timeout),
                NS_FormatPreciseTimeAsSec(other.max_pending_read_wait_timeout));

    if (scramble_job_keys != other.scramble_job_keys)
        AddParameterToDiff(diff, "scramble_job_keys",
                           scramble_job_keys, other.scramble_job_keys);


    if (client_registry_timeout_worker_node !=
                                    other.client_registry_timeout_worker_node)
        AddParameterToDiff(diff, "client_registry_timeout_worker_node",
                NS_FormatPreciseTimeAsSec(
                                client_registry_timeout_worker_node),
                NS_FormatPreciseTimeAsSec(
                                other.client_registry_timeout_worker_node));

    if (client_registry_min_worker_nodes !=
                                    other.client_registry_min_worker_nodes)
        AddParameterToDiff(diff, "client_registry_min_worker_nodes",
                           client_registry_min_worker_nodes,
                           other.client_registry_min_worker_nodes);

    if (client_registry_timeout_admin !=
                                    other.client_registry_timeout_admin)
        AddParameterToDiff(diff, "client_registry_timeout_admin",
                NS_FormatPreciseTimeAsSec(
                                client_registry_timeout_admin),
                NS_FormatPreciseTimeAsSec(
                                other.client_registry_timeout_admin));

    if (client_registry_min_admins !=
                                    other.client_registry_min_admins)
        AddParameterToDiff(diff, "client_registry_min_admins",
                           client_registry_min_admins,
                           other.client_registry_min_admins);

    if (client_registry_timeout_submitter !=
                                    other.client_registry_timeout_submitter)
        AddParameterToDiff(
                diff, "client_registry_timeout_submitter",
                NS_FormatPreciseTimeAsSec(
                                client_registry_timeout_submitter),
                NS_FormatPreciseTimeAsSec(
                                other.client_registry_timeout_submitter));

    if (client_registry_min_submitters !=
                                    other.client_registry_min_submitters)
        AddParameterToDiff(diff, "client_registry_min_submitters",
                           client_registry_min_submitters,
                           other.client_registry_min_submitters);

    if (client_registry_timeout_reader !=
                                    other.client_registry_timeout_reader)
        AddParameterToDiff(
                diff, "client_registry_timeout_reader",
                NS_FormatPreciseTimeAsSec(
                                client_registry_timeout_reader),
                NS_FormatPreciseTimeAsSec(
                                other.client_registry_timeout_reader));

    if (client_registry_min_readers !=
                                    other.client_registry_min_readers)
        AddParameterToDiff(diff, "client_registry_min_readers",
                           client_registry_min_readers,
                           other.client_registry_min_readers);

    if (client_registry_timeout_unknown !=
                                    other.client_registry_timeout_unknown)
        AddParameterToDiff(
                diff, "client_registry_timeout_unknown",
                NS_FormatPreciseTimeAsSec(
                                client_registry_timeout_unknown),
                NS_FormatPreciseTimeAsSec(
                                other.client_registry_timeout_unknown));

    if (client_registry_min_unknowns !=
                                    other.client_registry_min_unknowns)
        AddParameterToDiff(diff, "client_registry_min_unknowns",
                           client_registry_min_unknowns,
                           other.client_registry_min_unknowns);

    if (include_description && description != other.description)
        AddParameterToDiff(diff, "description",
                           description, other.description);

    if (linked_sections != other.linked_sections) {
        list<string>    added;
        list<string>    deleted;
        list<string>    changed;

        for (map<string, string>::const_iterator  k = linked_sections.begin();
             k != linked_sections.end(); ++k) {
            string                                  old_section = k->first;
            map<string, string>::const_iterator     found = other.linked_sections.find(old_section);

            if (found == other.linked_sections.end()) {
                deleted.push_back(old_section);
                continue;
            }
            if (k->second != found->second)
                changed.push_back(old_section);
        }

        for (map<string, string>::const_iterator  k = other.linked_sections.begin();
             k != other.linked_sections.end(); ++k) {
            string  new_section = k->first;

            if (linked_sections.find(new_section) == linked_sections.end())
                added.push_back(new_section);
        }

        if (!added.empty()) {
            CJsonNode       added_sections = CJsonNode::NewArrayNode();
            for (list<string>::const_iterator k = added.begin();
                    k != added.end(); ++k)
                added_sections.AppendString(NStr::PrintableString(*k));
            diff.SetByKey("linked_section_added", added_sections);
        }

        if (!deleted.empty()) {
            CJsonNode       deleted_sections = CJsonNode::NewArrayNode();
            for (list<string>::const_iterator k = deleted.begin();
                    k != deleted.end(); ++k)
                deleted_sections.AppendString(NStr::PrintableString(*k));
            diff.SetByKey("linked_section_deleted", deleted_sections);
        }

        if (!changed.empty()) {
            CJsonNode       changed_sections = CJsonNode::NewArrayNode();
            for (list<string>::const_iterator k = changed.begin();
                    k != changed.end(); ++k) {
                CJsonNode       section_changes = CJsonNode::NewObjectNode();
                CJsonNode       values = CJsonNode::NewArrayNode();

                map<string,
                    string>::const_iterator     val_iter;
                string                          changed_section_name = *k;

                val_iter = linked_sections.find(changed_section_name);
                string      old_value = val_iter->second;
                val_iter = other.linked_sections.find(changed_section_name);
                string      new_value = val_iter->second;

                values.AppendString(old_value);
                values.AppendString(new_value);
                section_changes.SetByKey(changed_section_name, values);
                changed_sections.Append(section_changes);
            }

            diff.SetByKey("linked_section_changed", changed_sections);
        }
    }

    return diff;
}


// Classes are included for queues only (not for queue classes)
string
SQueueParameters::GetPrintableParameters(bool  include_class,
                                         bool  url_encoded) const
{
    string      result;

    /* Initialized for multi-line output */
    string      prefix("OK:");
    string      suffix(": ");
    string      separator("\n");

    if (url_encoded) {
        prefix    = "";
        suffix    = "=";
        separator = "&";
    }

    if (include_class) {
        // These parameters make sense for queues only
        result = prefix + "kind" + suffix;
        if (kind == CQueue::eKindStatic)
            result += "static" + separator;
        else
            result += "dynamic" + separator;

        result +=
        prefix + "position" + suffix + NStr::NumericToString(position) + separator +
        prefix + "qclass" + suffix + qclass + separator +
        prefix + "refuse_submits" + suffix + NStr::BoolToString(refuse_submits) + separator +
        prefix + "max_aff_slots" + suffix + NStr::NumericToString(max_aff_slots) + separator +
        prefix + "aff_slots_used" + suffix + NStr::NumericToString(aff_slots_used) + separator +
        prefix + "clients" + suffix + NStr::NumericToString(clients) + separator +
        prefix + "groups" + suffix + NStr::NumericToString(groups) + separator +
        prefix + "gc_backlog" + suffix + NStr::NumericToString(gc_backlog) + separator +
        prefix + "notif_count" + suffix + NStr::NumericToString(notif_count) + separator;

        if (pause_status == CQueue::ePauseWithPullback)
            result += prefix + "pause" + suffix + "pullback" + separator;
        else if (pause_status == CQueue::ePauseWithoutPullback)
            result += prefix + "pause" + suffix + "nopullback" + separator;
        else
            result += prefix + "pause" + suffix + "nopause" + separator;
    }

    result +=
    prefix + "delete_request" + suffix + NStr::BoolToString(delete_request) + separator +
    prefix + "timeout" + suffix + NStr::NumericToString(timeout.Sec()) + separator +
    prefix + "notif_hifreq_interval" + suffix + NS_FormatPreciseTimeAsSec(notif_hifreq_interval) + separator +
    prefix + "notif_hifreq_period" + suffix + NS_FormatPreciseTimeAsSec(notif_hifreq_period) + separator +
    prefix + "notif_lofreq_mult" + suffix + NStr::NumericToString(notif_lofreq_mult) + separator +
    prefix + "notif_handicap" + suffix + NS_FormatPreciseTimeAsSec(notif_handicap) + separator +
    prefix + "dump_buffer_size" + suffix + NStr::NumericToString(dump_buffer_size) + separator +
    prefix + "dump_client_buffer_size" + suffix + NStr::NumericToString(dump_client_buffer_size) + separator +
    prefix + "dump_aff_buffer_size" + suffix + NStr::NumericToString(dump_aff_buffer_size) + separator +
    prefix + "dump_group_buffer_size" + suffix + NStr::NumericToString(dump_group_buffer_size) + separator +
    prefix + "run_timeout" + suffix + NS_FormatPreciseTimeAsSec(run_timeout) + separator +
    prefix + "read_timeout" + suffix + NS_FormatPreciseTimeAsSec(read_timeout) + separator +
    prefix + "failed_retries" + suffix + NStr::NumericToString(failed_retries) + separator +
    prefix + "read_failed_retries" + suffix + NStr::NumericToString(read_failed_retries) + separator +
    prefix + "blacklist_time" + suffix + NS_FormatPreciseTimeAsSec(blacklist_time) + separator +
    prefix + "read_blacklist_time" + suffix + NS_FormatPreciseTimeAsSec(read_blacklist_time) + separator +
    prefix + "max_input_size" + suffix + NStr::NumericToString(max_input_size) + separator +
    prefix + "max_output_size" + suffix + NStr::NumericToString(max_output_size) + separator +
    prefix + "wnode_timeout" + suffix + NS_FormatPreciseTimeAsSec(wnode_timeout) + separator +
    prefix + "reader_timeout" + suffix + NS_FormatPreciseTimeAsSec(reader_timeout) + separator +
    prefix + "pending_timeout" + suffix + NS_FormatPreciseTimeAsSec(pending_timeout) + separator +
    prefix + "max_pending_wait_timeout" + suffix + NS_FormatPreciseTimeAsSec(max_pending_wait_timeout) + separator +
    prefix + "max_pending_read_wait_timeout" + suffix + NS_FormatPreciseTimeAsSec(max_pending_read_wait_timeout) + separator +
    prefix + "scramble_job_keys" + suffix + NStr::BoolToString(scramble_job_keys) + separator +
    prefix + "client_registry_timeout_worker_node" + suffix + NS_FormatPreciseTimeAsSec(client_registry_timeout_worker_node) + separator +
    prefix + "client_registry_min_worker_nodes" + suffix + NStr::NumericToString(client_registry_min_worker_nodes) + separator +
    prefix + "client_registry_timeout_admin" + suffix + NS_FormatPreciseTimeAsSec(client_registry_timeout_admin) + separator +
    prefix + "client_registry_min_admins" + suffix + NStr::NumericToString(client_registry_min_admins) + separator +
    prefix + "client_registry_timeout_submitter" + suffix + NS_FormatPreciseTimeAsSec(client_registry_timeout_submitter) + separator +
    prefix + "client_registry_min_submitters" + suffix + NStr::NumericToString(client_registry_min_submitters) + separator +
    prefix + "client_registry_timeout_reader" + suffix + NS_FormatPreciseTimeAsSec(client_registry_timeout_reader) + separator +
    prefix + "client_registry_min_readers" + suffix + NStr::NumericToString(client_registry_min_readers) + separator +
    prefix + "client_registry_timeout_unknown" + suffix + NS_FormatPreciseTimeAsSec(client_registry_timeout_unknown) + separator +
    prefix + "client_registry_min_unknowns" + suffix + NStr::NumericToString(client_registry_min_unknowns) + separator;

    if (url_encoded) {
        result +=
        prefix + "program_name" + suffix + NStr::URLEncode(program_name) + separator +
        prefix + "subm_hosts" + suffix + NStr::URLEncode(subm_hosts) + separator +
        prefix + "wnode_hosts" + suffix + NStr::URLEncode(wnode_hosts) + separator +
        prefix + "reader_hosts" + suffix + NStr::URLEncode(reader_hosts) + separator +
        prefix + "description" + suffix + NStr::URLEncode(description);
        for (map<string, string>::const_iterator  k = linked_sections.begin();
             k != linked_sections.end(); ++k)
            result += separator + prefix + k->first + suffix + NStr::URLEncode(k->second);
    } else {
        result +=
        prefix + "program_name" + suffix + NStr::PrintableString(program_name) + separator +
        prefix + "subm_hosts" + suffix + NStr::PrintableString(subm_hosts) + separator +
        prefix + "wnode_hosts" + suffix + NStr::PrintableString(wnode_hosts) + separator +
        prefix + "reader_hosts" + suffix + NStr::PrintableString(reader_hosts) + separator +
        prefix + "description" + suffix + NStr::PrintableString(description);
        for (map<string, string>::const_iterator  k = linked_sections.begin();
             k != linked_sections.end(); ++k)
            result += separator + prefix + k->first + suffix + NStr::PrintableString(k->second);
    }

    return result;
}


string SQueueParameters::ConfigSection(bool is_class) const
{
    string  result = "description=\"" + description + "\"\n";

    if (!is_class)
        result += "class=\"" + qclass + "\"\n";

    result +=
    "timeout=\"" + NStr::NumericToString(timeout.Sec()) + "\"\n"
    "notif_hifreq_interval=\"" + NS_FormatPreciseTimeAsSec(notif_hifreq_interval) + "\"\n"
    "notif_hifreq_period=\"" + NS_FormatPreciseTimeAsSec(notif_hifreq_period) + "\"\n"
    "notif_lofreq_mult=\"" + NStr::NumericToString(notif_lofreq_mult) + "\"\n"
    "notif_handicap=\"" + NS_FormatPreciseTimeAsSec(notif_handicap) + "\"\n"
    "dump_buffer_size=\"" + NStr::NumericToString(dump_buffer_size) + "\"\n"
    "dump_client_buffer_size=\"" + NStr::NumericToString(dump_client_buffer_size) + "\"\n"
    "dump_aff_buffer_size=\"" + NStr::NumericToString(dump_aff_buffer_size) + "\"\n"
    "dump_group_buffer_size=\"" + NStr::NumericToString(dump_group_buffer_size) + "\"\n"
    "run_timeout=\"" + NS_FormatPreciseTimeAsSec(run_timeout) + "\"\n"
    "read_timeout=\"" + NS_FormatPreciseTimeAsSec(read_timeout) + "\"\n"
    "program=\"" + program_name + "\"\n"
    "failed_retries=\"" + NStr::NumericToString(failed_retries) + "\"\n"
    "read_failed_retries=\"" + NStr::NumericToString(read_failed_retries) + "\"\n"
    "blacklist_time=\"" + NS_FormatPreciseTimeAsSec(blacklist_time) + "\"\n"
    "read_blacklist_time=\"" + NS_FormatPreciseTimeAsSec(read_blacklist_time) + "\"\n"
    "max_input_size=\"" + NStr::NumericToString(max_input_size) + "\"\n"
    "max_output_size=\"" + NStr::NumericToString(max_output_size) + "\"\n"
    "subm_host=\"" + subm_hosts + "\"\n"
    "wnode_host=\"" + wnode_hosts + "\"\n"
    "reader_host=\"" + reader_hosts + "\"\n"
    "wnode_timeout=\"" + NS_FormatPreciseTimeAsSec(wnode_timeout) + "\"\n"
    "reader_timeout=\"" + NS_FormatPreciseTimeAsSec(reader_timeout) + "\"\n"
    "pending_timeout=\"" + NS_FormatPreciseTimeAsSec(pending_timeout) + "\"\n"
    "max_pending_wait_timeout=\"" + NS_FormatPreciseTimeAsSec(max_pending_wait_timeout) + "\"\n"
    "max_pending_read_wait_timeout=\"" + NS_FormatPreciseTimeAsSec(max_pending_read_wait_timeout) + "\"\n"
    "scramble_job_keys=\"" + NStr::BoolToString(scramble_job_keys) + "\"\n"
    "client_registry_timeout_worker_node=\"" + NS_FormatPreciseTimeAsSec(client_registry_timeout_worker_node) + "\"\n"
    "client_registry_min_worker_nodes=\"" + NStr::NumericToString(client_registry_min_worker_nodes) + "\"\n"
    "client_registry_timeout_admin=\"" + NS_FormatPreciseTimeAsSec(client_registry_timeout_admin) + "\"\n"
    "client_registry_min_admins=\"" + NStr::NumericToString(client_registry_min_admins) + "\"\n"
    "client_registry_timeout_submitter=\"" + NS_FormatPreciseTimeAsSec(client_registry_timeout_submitter) + "\"\n"
    "client_registry_min_submitters=\"" + NStr::NumericToString(client_registry_min_submitters) + "\"\n"
    "client_registry_timeout_reader=\"" + NS_FormatPreciseTimeAsSec(client_registry_timeout_reader) + "\"\n"
    "client_registry_min_readers=\"" + NStr::NumericToString(client_registry_min_readers) + "\"\n"
    "client_registry_timeout_unknown=\"" + NS_FormatPreciseTimeAsSec(client_registry_timeout_unknown) + "\"\n"
    "client_registry_min_unknowns=\"" + NStr::NumericToString(client_registry_min_unknowns) + "\"\n";

    for (map<string, string>::const_iterator  k = linked_sections.begin();
         k != linked_sections.end(); ++k)
        result += k->first + "=\"" + NStr::PrintableString(k->second) + "\"\n";

    return result;
}


CNSPreciseTime
SQueueParameters::CalculateRuntimePrecision(void) const
{
    // Min(run_timeout, read_timeout) / 10
    // but no less that 0.2 seconds
    CNSPreciseTime      min_timeout = run_timeout;
    if (read_timeout < min_timeout)
        min_timeout = read_timeout;

    // Divide by ten
    min_timeout.tv_sec = min_timeout.tv_sec / 10;
    min_timeout.tv_nsec = min_timeout.tv_nsec / 10;
    min_timeout.tv_nsec += (min_timeout.tv_sec % 10) * (kNSecsPerSecond / 10);

    static CNSPreciseTime       limit(0.2);
    if (min_timeout < limit)
        return limit;
    return min_timeout;
}


string
SQueueParameters::ReadClass(const IRegistry &  reg,
                            const string &     sname,
                            vector<string> &   warnings)
{
    bool    ok = NS_ValidateString(reg, sname, "class", warnings);
    if (!ok)
        return kEmptyStr;

    return reg.GetString(sname, "class", kEmptyStr);
}


CNSPreciseTime
SQueueParameters::ReadTimeout(const IRegistry &  reg,
                              const string &     sname,
                              vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "timeout", warnings);
    if (!ok)
        return default_timeout;

    double  val = reg.GetDouble(sname, "timeout", double(default_timeout));
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(sname, "timeout") +
                           ". It must be > 0.0");
        return default_timeout;
    }
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadNotifHifreqInterval(const IRegistry &  reg,
                                          const string &     sname,
                                          vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "notif_hifreq_interval",
                                   warnings);
    if (!ok)
        return default_notif_hifreq_interval;

    double  val = reg.GetDouble(sname, "notif_hifreq_interval",
                                double(default_notif_hifreq_interval));
    val = (int(val * 10)) / 10.0;
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "notif_hifreq_interval") +
                           ". It must be > 0.0");
        return default_notif_hifreq_interval;
    }
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadNotifHifreqPeriod(const IRegistry &  reg,
                                        const string &     sname,
                                        vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "notif_hifreq_period",
                                   warnings);
    if (!ok)
        return default_notif_hifreq_period;

    double  val = reg.GetDouble(sname, "notif_hifreq_period",
                                double(default_notif_hifreq_period));
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "notif_hifreq_period") +
                           ". It must be > 0.0");
        return default_notif_hifreq_period;
    }
    return CNSPreciseTime(val);
}

unsigned int
SQueueParameters::ReadNotifLofreqMult(const IRegistry &  reg,
                                      const string &     sname,
                                      vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "notif_lofreq_mult", warnings);
    if (!ok)
        return default_notif_lofreq_mult;

    int     val = reg.GetInt(sname, "notif_lofreq_mult",
                             default_notif_lofreq_mult);
    if (val <= 0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "notif_lofreq_mult") +
                           ". It must be > 0");
        return default_notif_lofreq_mult;
    }
    return val;
}

CNSPreciseTime
SQueueParameters::ReadNotifHandicap(const IRegistry &  reg,
                                    const string &     sname,
                                    vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "notif_handicap", warnings);
    if (!ok)
        return default_notif_handicap;

    double  val = reg.GetDouble(sname, "notif_handicap",
                                double(default_notif_handicap));
    if (val < 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "notif_handicap") +
                           ". It must be >= 0.0");
        return default_notif_handicap;
    }
    return CNSPreciseTime(val);
}

unsigned int
SQueueParameters::ReadDumpBufferSize(const IRegistry &  reg,
                                     const string &     sname,
                                     vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "dump_buffer_size", warnings);
    if (!ok)
        return default_dump_buffer_size;

    int     val = reg.GetInt(sname, "dump_buffer_size",
                             default_dump_buffer_size);
    if (val < int(default_dump_buffer_size)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "dump_buffer_size") +
                           ". It must be not less than " +
                           NStr::NumericToString(default_dump_buffer_size));
        return default_dump_buffer_size; // Avoid too small buffer
    }

    if (val > int(max_dump_buffer_size)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "dump_buffer_size") +
                           ". It must be not larger than " +
                           NStr::NumericToString(max_dump_buffer_size));
        return max_dump_buffer_size;     // Avoid too large buffer
    }
    return val;
}

unsigned int
SQueueParameters::ReadDumpClientBufferSize(const IRegistry &  reg,
                                           const string &     sname,
                                           vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "dump_client_buffer_size",
                                warnings);
    if (!ok)
        return default_dump_client_buffer_size;

    int     val = reg.GetInt(sname, "dump_client_buffer_size",
                             default_dump_client_buffer_size);
    if (val < int(default_dump_client_buffer_size)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "dump_client_buffer_size") +
                           ". It must be not less than " +
                           NStr::NumericToString(
                                        default_dump_client_buffer_size));
        return default_dump_client_buffer_size;  // Avoid too small buffer
    }

    if (val > int(max_dump_client_buffer_size)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "dump_client_buffer_size") +
                           ". It must be not larger than " +
                           NStr::NumericToString(
                                        max_dump_client_buffer_size));
        return max_dump_client_buffer_size;      // Avoid too large buffer
    }
    return val;
}

unsigned int
SQueueParameters::ReadDumpAffBufferSize(const IRegistry &  reg,
                                        const string &     sname,
                                        vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "dump_aff_buffer_size", warnings);
    if (!ok)
        return default_dump_aff_buffer_size;

    int     val = reg.GetInt(sname, "dump_aff_buffer_size",
                             default_dump_aff_buffer_size);
    if (val < int(default_dump_aff_buffer_size)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "dump_aff_buffer_size") +
                           ". It must be not less than " +
                           NStr::NumericToString(
                                        default_dump_aff_buffer_size));
        return default_dump_aff_buffer_size; // Avoid too small buffer
    }

    if (val > int(max_dump_aff_buffer_size)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "dump_aff_buffer_size") +
                           ". It must be not larger than " +
                           NStr::NumericToString(
                                        max_dump_aff_buffer_size));
        return max_dump_aff_buffer_size;     // Avoid too large buffer
    }
    return val;
}

unsigned int
SQueueParameters::ReadDumpGroupBufferSize(const IRegistry &  reg,
                                          const string &     sname,
                                          vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "dump_group_buffer_size", warnings);
    if (!ok)
        return default_dump_group_buffer_size;

    int     val = reg.GetInt(sname, "dump_group_buffer_size",
                             default_dump_group_buffer_size);
    if (val < int(default_dump_group_buffer_size)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "dump_group_buffer_size") +
                           ". It must be not less than " +
                           NStr::NumericToString(
                                        default_dump_group_buffer_size));
        return default_dump_group_buffer_size;   // Avoid too small buffer
    }
    if (val > int(max_dump_group_buffer_size)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "dump_group_buffer_size") +
                           ". It must be not larger than " +
                           NStr::NumericToString(
                                        max_dump_group_buffer_size));
        return max_dump_group_buffer_size;       // Avoid too large buffer
    }
    return val;
}

CNSPreciseTime
SQueueParameters::ReadRunTimeout(const IRegistry &  reg,
                                 const string &     sname,
                                 vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "run_timeout", warnings);
    if (!ok)
        return default_run_timeout;

    double  val = reg.GetDouble(sname, "run_timeout",
                                double(default_run_timeout));
    if (val < 0.0) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(sname, "run_timeout") +
                           ". It must be >= 0.0");
        return default_run_timeout;
    }
    return CNSPreciseTime(val);
}


CNSPreciseTime
SQueueParameters::ReadReadTimeout(const IRegistry &  reg,
                                  const string &     sname,
                                  vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "read_timeout", warnings);
    if (!ok)
        return default_read_timeout;

    double  val = reg.GetDouble(sname, "read_timeout",
                                double(default_read_timeout));
    if (val < 0.0) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(sname, "read_timeout") +
                           ". It must be >= 0.0");
        return default_read_timeout;
    }
    return CNSPreciseTime(val);
}


string
SQueueParameters::ReadProgram(const IRegistry &  reg,
                              const string &     sname,
                              vector<string> &   warnings)
{
    bool    ok = NS_ValidateString(reg, sname, "program", warnings);
    if (!ok)
        return kEmptyStr;

    string  val = reg.GetString(sname, "program", kEmptyStr);
    if (val.size() > kMaxQueueLimitsSize) {
        // See CXX-2617
        warnings.push_back(g_WarnPrefix + NS_RegValName(sname, "program") +
                           ". The value length (" +
                           NStr::NumericToString(val.size()) + " bytes) "
                           "exceeds the max allowed length (" +
                           NStr::NumericToString(kMaxQueueLimitsSize - 1) +
                           " bytes)");
        val = "";
    }
    return val;
}

unsigned int
SQueueParameters::ReadFailedRetries(const IRegistry &  reg,
                                    const string &     sname,
                                    vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "failed_retries", warnings);
    if (!ok)
        return default_failed_retries;

    int     val = reg.GetInt(sname, "failed_retries", default_failed_retries);
    if (val < 0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "failed_retries") +
                           ". It must be >= 0");
        return default_failed_retries;
    }
    return val;
}

unsigned int
SQueueParameters::ReadReadFailedRetries(const IRegistry &  reg,
                                        const string &     sname,
                                        vector<string> &   warnings,
                                        unsigned int       failed_retries)
{
    // The default for the read retries is failed_retries
    bool    ok = NS_ValidateInt(reg, sname, "read_failed_retries", warnings);
    if (!ok)
        return failed_retries;

    int     val =  reg.GetInt(sname, "read_failed_retries", failed_retries);
    if (val < 0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "read_failed_retries") +
                           ". It must be >= 0");
        return failed_retries;
    }
    return val;
}

CNSPreciseTime
SQueueParameters::ReadBlacklistTime(const IRegistry &  reg,
                                    const string &     sname,
                                    vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "blacklist_time", warnings);
    if (!ok)
        return default_blacklist_time;

    double  val = reg.GetDouble(sname, "blacklist_time",
                                double(default_blacklist_time));
    if (val < 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "blacklist_time") +
                           ". It must be >= 0.0");
        return default_blacklist_time;
    }
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadReadBlacklistTime(const IRegistry &  reg,
                                        const string &     sname,
                                        vector<string> &   warnings,
                                        const CNSPreciseTime &  blacklist_time)
{
    // The default for the read_blacklist_time is blacklist_time
    bool    ok = NS_ValidateDouble(reg, sname, "read_blacklist_time", warnings);
    if (!ok)
        return blacklist_time;

    double  val = reg.GetDouble(sname, "read_blacklist_time",
                                double(blacklist_time));
    if (val < 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "read_blacklist_time") +
                           ". It must be >= 0.0");
        return blacklist_time;
    }
    return CNSPreciseTime(val);
}

unsigned int
SQueueParameters::ReadMaxInputSize(const IRegistry &  reg,
                                   const string &     sname,
                                   vector<string> &   warnings)
{
    // default is kNetScheduleMaxDBDataSize, i.e. 2048
    bool    ok = NS_ValidateString(reg, sname, "max_input_size", warnings);
    if (!ok)
        return kNetScheduleMaxDBDataSize;

    string  s = reg.GetString(sname, "max_input_size", kEmptyStr);
    if (!s.empty()) {
        unsigned int    val = kNetScheduleMaxDBDataSize;
        try {
            val = (unsigned int) NStr::StringToUInt8_DataSize(s);
        } catch (const CStringException &  ex) {
            warnings.push_back(g_WarnPrefix +
                               NS_RegValName(sname, "max_input_size") +
                               ". It could not be converted to "
                               "unsigned integer: " + ex.what());
            return kNetScheduleMaxDBDataSize;
        } catch (...) {
            warnings.push_back(g_WarnPrefix +
                               NS_RegValName(sname, "max_input_size") +
                               ". It could not be converted to "
                               "unsigned integer");
            return kNetScheduleMaxDBDataSize;
        }

        if (val > kNetScheduleMaxOverflowSize) {
            warnings.push_back(g_WarnPrefix +
                               NS_RegValName(sname, "max_input_size") +
                               ". It must not be larger than " +
                               NStr::NumericToString(
                                                kNetScheduleMaxOverflowSize));
            return kNetScheduleMaxOverflowSize;
        }
        return val;
    }
    return kNetScheduleMaxDBDataSize;
}

unsigned int
SQueueParameters::ReadMaxOutputSize(const IRegistry &  reg,
                                    const string &     sname,
                                    vector<string> &   warnings)
{
    // default is kNetScheduleMaxDBDataSize, i.e. 2048
    bool    ok = NS_ValidateString(reg, sname, "max_output_size", warnings);
    if (!ok)
        return kNetScheduleMaxDBDataSize;

    string  s = reg.GetString(sname, "max_output_size", kEmptyStr);
    if (!s.empty()) {
        unsigned int    val = kNetScheduleMaxDBDataSize;
        try {
            val = (unsigned int) NStr::StringToUInt8_DataSize(s);
        } catch (const CStringException &  ex) {
            warnings.push_back(g_WarnPrefix +
                               NS_RegValName(sname, "max_output_size") +
                               ". It could not be converted to "
                               "unsigned integer: " + ex.what());
            return kNetScheduleMaxDBDataSize;
        } catch (...) {
            warnings.push_back(g_WarnPrefix +
                               NS_RegValName(sname, "max_output_size") +
                               ". It could not be converted to "
                               "unsigned integer");
            return kNetScheduleMaxDBDataSize;
        }

        if (val > kNetScheduleMaxOverflowSize) {
            warnings.push_back(g_WarnPrefix +
                               NS_RegValName(sname, "max_output_size") +
                               ". It must not be larger than " +
                               NStr::NumericToString(
                                                kNetScheduleMaxOverflowSize));
            return kNetScheduleMaxOverflowSize;
        }
        return val;
    }
    return kNetScheduleMaxDBDataSize;
}

string
SQueueParameters::ReadSubmHosts(const IRegistry &  reg,
                                const string &     sname,
                                vector<string> &   warnings)
{
    bool    ok = NS_ValidateString(reg, sname, "subm_host", warnings);
    if (!ok)
        return kEmptyStr;

    string  val = reg.GetString(sname, "subm_host", kEmptyStr);
    if (val.size() > kMaxQueueLimitsSize) {
        // See CXX-2617
        warnings.push_back(g_WarnPrefix + NS_RegValName(sname, "subm_host") +
                           ". The value length (" +
                           NStr::NumericToString(val.size()) + " bytes) "
                           "exceeds the max allowed length (" +
                           NStr::NumericToString(kMaxQueueLimitsSize - 1) +
                           " bytes)");
        val = "";
    }
    return val;
}

string
SQueueParameters::ReadWnodeHosts(const IRegistry &  reg,
                                 const string &     sname,
                                 vector<string> &   warnings)
{
    bool    ok = NS_ValidateString(reg, sname, "wnode_host", warnings);
    if (!ok)
        return kEmptyStr;

    string  val = reg.GetString(sname, "wnode_host", kEmptyStr);
    if (val.size() > kMaxQueueLimitsSize) {
        // See CXX-2617
        warnings.push_back(g_WarnPrefix + NS_RegValName(sname, "wnode_host") +
                           ". The value length (" +
                           NStr::NumericToString(val.size()) + " bytes) "
                           "exceeds the max allowed length (" +
                           NStr::NumericToString(kMaxQueueLimitsSize - 1) +
                           " bytes)");
        val = "";
    }
    return val;
}

string
SQueueParameters::ReadReaderHosts(const IRegistry &  reg,
                                  const string &     sname,
                                  vector<string> &   warnings)
{
    bool    ok = NS_ValidateString(reg, sname, "reader_host", warnings);
    if (!ok)
        return kEmptyStr;

    string  val = reg.GetString(sname, "reader_host", kEmptyStr);
    if (val.size() > kMaxQueueLimitsSize) {
        // See CXX-2617
        warnings.push_back(g_WarnPrefix + NS_RegValName(sname, "reader_host") +
                           ". The value length (" +
                           NStr::NumericToString(val.size()) + " bytes) "
                           "exceeds the max allowed length (" +
                           NStr::NumericToString(kMaxQueueLimitsSize - 1) +
                           " bytes)");
        val = "";
    }
    return val;
}

CNSPreciseTime
SQueueParameters::ReadWnodeTimeout(const IRegistry &  reg,
                                   const string &     sname,
                                   vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "wnode_timeout", warnings);
    if (!ok)
        return default_wnode_timeout;

    double  val = reg.GetDouble(sname, "wnode_timeout",
                                double(default_wnode_timeout));
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "wnode_timeout") +
                           ". It must be > 0.0");
        return default_wnode_timeout;
    }
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadReaderTimeout(const IRegistry &  reg,
                                    const string &     sname,
                                    vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "reader_timeout", warnings);
    if (!ok)
        return default_reader_timeout;

    double  val = reg.GetDouble(sname, "reader_timeout",
                                double(default_reader_timeout));
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "reader_timeout") +
                           ". It must be > 0.0");
        return default_reader_timeout;
    }
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadPendingTimeout(const IRegistry &  reg,
                                     const string &     sname,
                                     vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "pending_timeout", warnings);
    if (!ok)
        return default_pending_timeout;

    double  val = reg.GetDouble(sname, "pending_timeout",
                                double(default_pending_timeout));
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "pending_timeout") +
                           ". It must be > 0.0");
        return default_pending_timeout;
    }
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadMaxPendingWaitTimeout(const IRegistry &  reg,
                                            const string &     sname,
                                            vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "max_pending_wait_timeout",
                                   warnings);
    if (!ok)
        return default_max_pending_wait_timeout;

    double  val = reg.GetDouble(sname, "max_pending_wait_timeout",
                                double(default_max_pending_wait_timeout));
    if (val < 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "max_pending_wait_timeout") +
                           ". It must be >= 0.0");
        return default_max_pending_wait_timeout;
    }
    return CNSPreciseTime(val);
}

CNSPreciseTime
SQueueParameters::ReadMaxPendingReadWaitTimeout(const IRegistry &  reg,
                                                const string &     sname,
                                                vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "max_pending_read_wait_timeout",
                                   warnings);
    if (!ok)
        return default_max_pending_read_wait_timeout;

    double  val = reg.GetDouble(sname, "max_pending_read_wait_timeout",
                                double(default_max_pending_read_wait_timeout));
    if (val < 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname, "max_pending_read_wait_timeout")
                           + ". It must be >= 0.0");
        return default_max_pending_read_wait_timeout;
    }
    return CNSPreciseTime(val);
}

string
SQueueParameters::ReadDescription(const IRegistry &  reg,
                                  const string &     sname,
                                  vector<string> &   warnings)
{
    bool    ok = NS_ValidateString(reg, sname, "description", warnings);
    if (!ok)
        return kEmptyStr;

    string      descr = reg.GetString(sname, "description", kEmptyStr);
    if (descr.size() > kMaxDescriptionSize - 1) {
        warnings.push_back(g_WarnPrefix + NS_RegValName(sname, "description") +
                           ". The value length (" +
                           NStr::NumericToString(descr.size()) + " bytes) "
                           "exceeds the max allowed length (" +
                           NStr::NumericToString(kMaxDescriptionSize - 1) +
                           " bytes)");
        descr = descr.substr(0, kMaxDescriptionSize - 1);
    }
    return descr;
}

bool
SQueueParameters::ReadScrambleJobKeys(const IRegistry &  reg,
                                      const string &     sname,
                                      vector<string> &   warnings)
{
    bool    ok = NS_ValidateBool(reg, sname, "scramble_job_keys", warnings);
    if (!ok)
        return default_scramble_job_keys;

    return reg.GetBool(sname, "scramble_job_keys", default_scramble_job_keys);
}

CNSPreciseTime
SQueueParameters::ReadClientRegistryTimeoutWorkerNode(
                                            const IRegistry &  reg,
                                            const string &     sname,
                                            vector<string> &   warnings)
{
    double  calc_default =
                max(
                    max(double(default_client_registry_timeout_worker_node),
                        double(wnode_timeout + wnode_timeout)),
                    double(run_timeout + run_timeout));
    bool    ok = NS_ValidateDouble(reg, sname,
                                   "client_registry_timeout_worker_node",
                                   warnings);
    if (!ok)
        return CNSPreciseTime(calc_default);

    double  val = reg.GetDouble(sname, "client_registry_timeout_worker_node",
                         double(default_client_registry_timeout_worker_node));
    if (val <= 0.0 || val <= double(wnode_timeout)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_timeout_worker_node")
                           + ". It must be > " +
                           NStr::NumericToString(double(wnode_timeout)));
        return CNSPreciseTime(calc_default);
    }
    return CNSPreciseTime(val);
}


unsigned int
SQueueParameters::ReadClientRegistryMinWorkerNodes(const IRegistry &  reg,
                                                   const string &     sname,
                                                   vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "client_registry_min_worker_nodes",
                                warnings);
    if (!ok)
        return default_client_registry_min_worker_nodes;

    int     val = reg.GetInt(sname, "client_registry_min_worker_nodes",
                             default_client_registry_min_worker_nodes);
    if (val <= 0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_min_worker_nodes") +
                           ". It must be > 0");
        return default_client_registry_min_worker_nodes;
    }
    return val;
}


CNSPreciseTime
SQueueParameters::ReadClientRegistryTimeoutAdmin(const IRegistry &  reg,
                                                 const string &     sname,
                                                 vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname, "client_registry_timeout_admin",
                                   warnings);
    if (!ok)
        return default_client_registry_timeout_admin;

    double  val = reg.GetDouble(sname, "client_registry_timeout_admin",
                                double(default_client_registry_timeout_admin));
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_timeout_admin") +
                           ". It must be > 0.0");
        return default_client_registry_timeout_admin;
    }
    return CNSPreciseTime(val);
}


unsigned int
SQueueParameters::ReadClientRegistryMinAdmins(const IRegistry &  reg,
                                              const string &     sname,
                                              vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "client_registry_min_admins",
                                warnings);
    if (!ok)
        return default_client_registry_min_admins;

    int     val = reg.GetInt(sname, "client_registry_min_admins",
                             default_client_registry_min_admins);
    if (val <= 0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_min_admins") +
                           ". It must be > 0");
        return default_client_registry_min_admins;
    }
    return val;
}


CNSPreciseTime
SQueueParameters::ReadClientRegistryTimeoutSubmitter(
                                            const IRegistry &  reg,
                                            const string &     sname,
                                            vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname,
                                   "client_registry_timeout_submitter",
                                   warnings);
    if (!ok)
        return default_client_registry_timeout_submitter;

    double  val = reg.GetDouble(sname, "client_registry_timeout_submitter",
                        double(default_client_registry_timeout_submitter));
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_timeout_submitter") +
                           ". It must be > 0.0");
        return default_client_registry_timeout_submitter;
    }
    return CNSPreciseTime(val);
}


unsigned int
SQueueParameters::ReadClientRegistryMinSubmitters(const IRegistry &  reg,
                                                  const string &     sname,
                                                  vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname,
                                "client_registry_min_submitters", warnings);
    if (!ok)
        return default_client_registry_min_submitters;

    int     val = reg.GetInt(sname, "client_registry_min_submitters",
                             default_client_registry_min_submitters);
    if (val <= 0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_min_submitters") +
                           ". It must be > 0");
        return default_client_registry_min_submitters;
    }
    return val;
}


CNSPreciseTime
SQueueParameters::ReadClientRegistryTimeoutReader(const IRegistry &  reg,
                                                  const string &     sname,
                                                  vector<string> &   warnings)
{
    double  calc_default =
                max(
                    max(double(default_client_registry_timeout_reader),
                        double(reader_timeout + reader_timeout)),
                    double(read_timeout + read_timeout));
    bool    ok = NS_ValidateDouble(reg, sname,
                                   "client_registry_timeout_reader",
                                   warnings);
    if (!ok)
        return CNSPreciseTime(calc_default);

    double  val = reg.GetDouble(sname, "client_registry_timeout_reader",
                         double(default_client_registry_timeout_reader));
    if (val <= 0.0 || val <= double(reader_timeout)) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_timeout_reader")
                           + ". It must be > " +
                           NStr::NumericToString(double(reader_timeout)));
        CNSPreciseTime(calc_default);
    }
    return CNSPreciseTime(val);
}


unsigned int
SQueueParameters::ReadClientRegistryMinReaders(const IRegistry &  reg,
                                               const string &     sname,
                                               vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname, "client_registry_min_readers",
                                warnings);
    if (!ok)
        return default_client_registry_min_readers;

    int     val = reg.GetInt(sname, "client_registry_min_readers",
                             default_client_registry_min_readers);
    if (val <= 0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_min_readers") +
                           ". It must be > 0");
        return default_client_registry_min_readers;
    }
    return val;
}


CNSPreciseTime
SQueueParameters::ReadClientRegistryTimeoutUnknown(const IRegistry &  reg,
                                                   const string &     sname,
                                                   vector<string> &   warnings)
{
    bool    ok = NS_ValidateDouble(reg, sname,
                                   "client_registry_timeout_unknown", warnings);
    if (!ok)
        return default_client_registry_timeout_unknown;

    double  val = reg.GetDouble(sname, "client_registry_timeout_unknown",
                         double(default_client_registry_timeout_unknown));
    if (val <= 0.0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_timeout_unknown") +
                           ". It must be > 0.0");
        return default_client_registry_timeout_unknown;
    }
    return CNSPreciseTime(val);
}


unsigned int
SQueueParameters::ReadClientRegistryMinUnknowns(const IRegistry &  reg,
                                                const string &     sname,
                                                vector<string> &   warnings)
{
    bool    ok = NS_ValidateInt(reg, sname,
                                "client_registry_min_unknowns", warnings);
    if (!ok)
        return default_client_registry_min_unknowns;

    int     val = reg.GetInt(sname, "client_registry_min_unknowns",
                             default_client_registry_min_unknowns);
    if (val <= 0) {
        warnings.push_back(g_WarnPrefix +
                           NS_RegValName(sname,
                                         "client_registry_min_unknowns") +
                           ". It must be > 0");
        return default_client_registry_min_unknowns;
    }
    return val;
}


map<string, string>
SQueueParameters::ReadLinkedSections(const IRegistry &  reg,
                                     const string &     sname,
                                     vector<string> &   warnings,
                                     bool *  linked_sections_ok)
{
    map<string, string>     conf_linked_sections;
    list<string>            entries;
    list<string>            available_sections;

    *linked_sections_ok = true;

    reg.EnumerateEntries(sname, &entries);
    reg.EnumerateSections(&available_sections);

    for (list<string>::const_iterator  k = entries.begin();
         k != entries.end(); ++k) {
        const string &      entry = *k;

        if (!NStr::StartsWith(entry, "linked_section_", NStr::eCase))
            continue;
        if (entry == "linked_section_") {
            warnings.push_back("Validating config file: unexpected entry name "
                               + NS_RegValName(sname, "linked_section_") +
                               ". Referring name is missed");
            continue;
        }

        bool    ok = NS_ValidateString(reg, sname, entry, warnings);
        if (!ok)
            continue;

        string  ref_section = reg.GetString(sname, entry, kEmptyStr);
        if (ref_section.empty()) {
            warnings.push_back(g_WarnPrefix + NS_RegValName(sname, entry) +
                               ". Referred section name is missed");
            continue;
        }
        if (find(available_sections.begin(),
                 available_sections.end(),
                 ref_section) == available_sections.end()) {
            warnings.push_back(g_WarnPrefix + NS_RegValName(sname, entry) +
                               ". It refers to an unknown section");
            continue;
        }

        // Here: linked section exists and the prefix is fine
        conf_linked_sections[entry] = ref_section;
    }

    // Check the limit of the serialized sections/prefixes lengths
    // See how the serialization is done
    string      prefixes;
    string      sections;
    for (map<string, string>::const_iterator
            k = conf_linked_sections.begin(); k != conf_linked_sections.end();
            ++k) {
        if (!prefixes.empty())
            prefixes += ",";
        prefixes += k->first;
        if (!sections.empty())
            sections += ",";
        sections += k->second;
    }
    if (prefixes.size() > kLinkedSectionsList - 1) {
        warnings.push_back("Validating config file: total length of the "
                           "serialized linked section prefixes (" +
                           NStr::NumericToString(prefixes.size()) +
                           " bytes) exceeds the limit (" +
                           NStr::NumericToString(kLinkedSectionsList - 1) +
                           "bytes) in the section " + sname);
        *linked_sections_ok = false;
    }
    if (sections.size() > kLinkedSectionsList - 1) {
        warnings.push_back("Validating config file: total length of the "
                           "serialized linked section names (" +
                           NStr::NumericToString(prefixes.size()) +
                           " bytes) exceeds the limit (" +
                           NStr::NumericToString(kLinkedSectionsList - 1) +
                           "bytes) in the section " + sname);
        *linked_sections_ok = false;
    }

    // Check each individual value in the linked section
    for (map<string, string>::const_iterator
            k = conf_linked_sections.begin(); k != conf_linked_sections.end();
            ++k) {
        entries.clear();
        reg.EnumerateEntries(k->second, &entries);
        for (list<string>::const_iterator  j = entries.begin();
                j != entries.end(); ++j) {
            if (j->size() > kLinkedSectionValueNameSize - 1) {
                string  limit_as_str =
                    NStr::NumericToString(kLinkedSectionValueNameSize - 1);
                warnings.push_back("Validating config file: linked section [" +
                                   k->second + "]/" + *j + " name length (" +
                                   NStr::NumericToString(j->size()) +
                                   " bytes) exceeds the limit (" +
                                   limit_as_str + " bytes)");
                *linked_sections_ok = false;
            }
            string  value = reg.GetString(k->second, *j, kEmptyStr);
            if (value.size() > kLinkedSectionValueSize - 1) {
                string  limit_as_str =
                    NStr::NumericToString(kLinkedSectionValueSize - 1);
                warnings.push_back("Validating config file: linked section [" +
                                   k->second + "]/" + *j + " value length (" +
                                   NStr::NumericToString(value.size()) +
                                   " bytes) exceeds the limit (" +
                                   limit_as_str + " bytes)");
                *linked_sections_ok = false;
            }
        }
    }

    return conf_linked_sections;
}

END_NCBI_SCOPE

