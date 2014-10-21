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
 * Authors:  Victor Joukov
 *
 * File Description: Utility functions for NetSchedule
 *
 */

#include <ncbi_pch.hpp>

#include "ns_util.hpp"
#include "ns_queue.hpp"
#include "ns_ini_params.hpp"
#include "ns_queue_parameters.hpp"
#include <util/bitset/bmalgo.h>
#include <connect/ncbi_socket.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>


BEGIN_NCBI_SCOPE


static void NS_ValidateServerSection(const IRegistry &  reg,
                                     vector<string> &   warnings,
                                     bool               throw_port_exception);
static void NS_ValidateQueuesAndClasses(const IRegistry &  reg,
                                        list<string> &  queues,
                                        vector<string> &  warnings);
static TQueueParams NS_ValidateClasses(const IRegistry &  reg,
                                       vector<string> &  warnings);
static void NS_ValidateQueues(const IRegistry &  reg,
                              const TQueueParams &  qclasses,
                              list<string> &  queues,
                              vector<string> &  warnings);
static void NS_ValidateServiceToQueueSection(const IRegistry &  reg,
                                             const list<string> &  queues,
                                             vector<string> &  warnings);
static string NS_OutOfLimitMessage(const string &  section,
                                   const string &  entry,
                                   unsigned int  low_limit,
                                   unsigned int  high_limit);

const string    g_ValidPrefix = "Validating config file: ";


void NS_ValidateConfigFile(const IRegistry &  reg, vector<string> &  warnings,
                           bool  throw_port_exception)
{
    list<string>    queues;

    NS_ValidateServerSection(reg, warnings, throw_port_exception);
    NS_ValidateQueuesAndClasses(reg, queues, warnings);
    NS_ValidateServiceToQueueSection(reg, queues, warnings);
}


// Populates the warnings list if there are problems
void NS_ValidateServerSection(const IRegistry &  reg,
                              vector<string> &   warnings,
                              bool               throw_port_exception)
{
    const string    section = "server";

    // port is a unique value in this section. NS must not start
    // if there is a problem with port.
    bool    ok = NS_ValidateInt(reg, section, "port", warnings);
    if (ok) {
        int     port_val = reg.GetInt(section, "port", 0);
        if (port_val < port_low_limit || port_val > port_high_limit) {
            string  msg = "Invalid " + NS_RegValName(section, "port") +
                          " value. Allowed range: " +
                          NStr::NumericToString(port_low_limit) +
                          " to " + NStr::NumericToString(port_high_limit);
            if (throw_port_exception)
                NCBI_THROW(CNetScheduleException, eInvalidParameter, msg);
            warnings.push_back(msg);
        }
    } else {
        string  msg = "Invalid " + NS_RegValName(section, "port") +
                      " parameter.";
        if (throw_port_exception)
            NCBI_THROW(CNetScheduleException, eInvalidParameter, msg);
        warnings.push_back(msg);
    }

    ok = NS_ValidateBool(reg, section, "reinit", warnings);

    ok = NS_ValidateInt(reg, section, "max_connections", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "max_connections",
                                 default_max_connections);
        if (val < int(max_connections_low_limit) ||
            val > int(max_connections_high_limit))
            warnings.push_back(
                    NS_OutOfLimitMessage(section, "max_connections",
                                         max_connections_low_limit,
                                         max_connections_high_limit));
    }

    ok = NS_ValidateInt(reg, section, "max_threads", warnings);
    unsigned int    max_threads_val = default_max_threads;
    if (ok) {
        int     val = reg.GetInt(section, "max_threads", default_max_threads);
        if (val < int(max_threads_low_limit) ||
            val > int(max_threads_high_limit))
            warnings.push_back(NS_OutOfLimitMessage(section, "max_threads",
                                                    max_threads_low_limit,
                                                    max_threads_high_limit));
        else
            max_threads_val = val;
    }

    ok = NS_ValidateInt(reg, section, "init_threads", warnings);
    unsigned int    init_threads_val = default_init_threads;
    if (ok) {
        int     val = reg.GetInt(section, "init_threads", default_init_threads);
        if (val < int(init_threads_low_limit) ||
            val > int(init_threads_high_limit))
            warnings.push_back(NS_OutOfLimitMessage(section, "init_threads",
                                                    init_threads_low_limit,
                                                    init_threads_high_limit));
        else
            init_threads_val = val;
    }

    if (init_threads_val > max_threads_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "max_threads") + " and " +
                 NS_RegValName(section, "init_threads") +
                 " break the mandatory condition "
                 "init_threads <= max_threads");


    NS_ValidateBool(reg, section, "use_hostname", warnings);

    ok = NS_ValidateInt(reg, section, "network_timeout", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "network_timeout",
                                 default_network_timeout);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "network_timeout") +
                     " must be > 0");
    }

    NS_ValidateBool(reg, section, "log", warnings);
    NS_ValidateBool(reg, section, "log_batch_each_job", warnings);
    NS_ValidateBool(reg, section, "log_notification_thread", warnings);
    NS_ValidateBool(reg, section, "log_cleaning_thread", warnings);
    NS_ValidateBool(reg, section, "log_execution_watcher_thread", warnings);
    NS_ValidateBool(reg, section, "log_statistics_thread", warnings);


    ok = NS_ValidateInt(reg, section, "del_batch_size", warnings);
    unsigned int    del_batch_size_val = default_del_batch_size;
    if (ok) {
        int     val = reg.GetInt(section, "del_batch_size",
                                 default_del_batch_size);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "del_batch_size") +
                     " must be > 0");
        else
            del_batch_size_val = val;
    }

    ok = NS_ValidateInt(reg, section, "markdel_batch_size", warnings);
    unsigned int    markdel_batch_size_val = default_markdel_batch_size;
    if (ok) {
        int     val = reg.GetInt(section, "markdel_batch_size",
                                 default_markdel_batch_size);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "markdel_batch_size") +
                     " must be > 0");
        else
            markdel_batch_size_val = val;
    }

    ok = NS_ValidateInt(reg, section, "scan_batch_size", warnings);
    unsigned int    scan_batch_size_val = default_scan_batch_size;
    if (ok) {
        int     val = reg.GetInt(section, "scan_batch_size",
                                 default_scan_batch_size);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "scan_batch_size") +
                     " must be > 0");
        else
            scan_batch_size_val = val;
    }

    ok = NS_ValidateDouble(reg, section, "purge_timeout", warnings);
    if (ok) {
        double  val = reg.GetDouble(section, "purge_timeout",
                                    default_purge_timeout);
        if (val <= 0.0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "purge_timeout") +
                     " must be > 0");
    }

    if (scan_batch_size_val < markdel_batch_size_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "scan_batch_size") + " and " +
                 NS_RegValName(section, "markdel_batch_size") +
                 " break the mandatory condition "
                 "markdel_batch_size <= scan_batch_size");

    if (markdel_batch_size_val < del_batch_size_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "markdel_batch_size") + " and " +
                 NS_RegValName(section, "del_batch_size") +
                 " break the mandatory condition "
                 "del_batch_size <= markdel_batch_size");


    ok = NS_ValidateInt(reg, section, "stat_interval", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "stat_interval",
                                 default_stat_interval);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "stat_interval") +
                     " must be > 0");
    }


    ok = NS_ValidateInt(reg, section, "max_affinities", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "max_affinities",
                                 default_max_affinities);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "max_affinities") +
                     " must be > 0");
    }

    ok = NS_ValidateInt(reg, section, "max_client_data", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "max_client_data",
                                 default_max_client_data);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "max_client_data") +
                     " must be > 0");
    }

    ok = NS_ValidateInt(reg, section, "affinity_high_mark_percentage",
                        warnings);
    unsigned int    affinity_high_mark_percentage_val =
                                default_affinity_high_mark_percentage;
    if (ok) {
        int     val = reg.GetInt(section, "affinity_high_mark_percentage",
                                 default_affinity_high_mark_percentage);
        if (val <= 0 || val >= 100)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "affinity_high_mark_percentage") +
                     " must be between 0 and 100");
        else
            affinity_high_mark_percentage_val = val;
    }

    ok = NS_ValidateInt(reg, section, "affinity_low_mark_percentage",
                        warnings);
    unsigned int    affinity_low_mark_percentage_val =
                                default_affinity_low_mark_percentage;
    if (ok) {
        int     val = reg.GetInt(section, "affinity_low_mark_percentage",
                                 default_affinity_low_mark_percentage);
        if (val <= 0 || val >= 100)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "affinity_low_mark_percentage") +
                     " must be between 0 and 100");
        else
            affinity_low_mark_percentage_val = val;
    }

    ok = NS_ValidateInt(reg, section, "affinity_high_removal", warnings);
    unsigned int    affinity_high_removal_val =
                                default_affinity_high_removal;
    if (ok) {
        int     val = reg.GetInt(section, "affinity_high_removal",
                                 default_affinity_high_removal);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "affinity_high_removal") +
                     " must be > 0");
        else
            affinity_high_removal_val = val;
    }

    ok = NS_ValidateInt(reg, section, "affinity_low_removal", warnings);
    unsigned int    affinity_low_removal_val =
                                default_affinity_low_removal;
    if (ok) {
        int     val = reg.GetInt(section, "affinity_low_removal",
                                 default_affinity_low_removal);
        if (val <= 0)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "affinity_low_removal") +
                     " must be > 0");
        else
            affinity_low_removal_val = val;
    }

    ok = NS_ValidateInt(reg, section, "affinity_dirt_percentage", warnings);
    unsigned int    affinity_dirt_percentage_val =
                                default_affinity_dirt_percentage;
    if (ok) {
        int     val = reg.GetInt(section, "affinity_dirt_percentage",
                                 default_affinity_dirt_percentage);
        if (val <= 0 || val >= 100)
            warnings.push_back(g_ValidPrefix + "value " +
                     NS_RegValName(section, "affinity_dirt_percentage") +
                     " must be between 0 and 100");
        else
            affinity_dirt_percentage_val = val;
    }

    if (affinity_low_mark_percentage_val >= affinity_high_mark_percentage_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "affinity_low_mark_percentage") +
                 " and " +
                 NS_RegValName(section, "affinity_high_mark_percentage") +
                 " break the mandatory condition "
                 "affinity_low_mark_percentage < "
                 "affinity_high_mark_percentage");

    if (affinity_dirt_percentage_val >= affinity_low_mark_percentage_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "affinity_low_mark_percentage") +
                 " and " +
                 NS_RegValName(section, "affinity_dirt_percentage") +
                 " break the mandatory condition "
                 "affinity_dirt_percentage < affinity_low_mark_percentage");

    if (affinity_high_removal_val < affinity_low_removal_val)
        warnings.push_back(g_ValidPrefix + "values " +
                 NS_RegValName(section, "affinity_high_removal") +
                 " and " +
                 NS_RegValName(section, "affinity_low_removal") +
                 " break the mandatory condition "
                 "affinity_low_removal <= affinity_high_removal");

    NS_ValidateString(reg, section, "admin_host", warnings);
    NS_ValidateString(reg, section, "admin_client_name", warnings);
}


// Populates the warnings list if there are problems in the config file
// the queues parameter is filled with what queues will be accepted
void NS_ValidateQueuesAndClasses(const IRegistry &  reg,
                                 list<string> &     queues,
                                 vector<string> &   warnings)
{
    TQueueParams    qclasses;

    qclasses = NS_ValidateClasses(reg, warnings);
    NS_ValidateQueues(reg, qclasses, queues, warnings);
}


TQueueParams NS_ValidateClasses(const IRegistry &   reg,
                                vector<string> &    warnings)
{
    TQueueParams        queue_classes;
    list<string>        sections;

    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        string              queue_class;
        string              prefix;
        const string &      section_name = *it;

        NStr::SplitInTwo(section_name, "_", prefix, queue_class);
        if (NStr::CompareNocase(prefix, "qclass") != 0)
           continue;
        if (queue_class.empty()) {
            warnings.push_back(g_ValidPrefix + "section " + section_name +
                               " does not have a queue class name");
            continue;
        }

        SQueueParameters    params;
        params.ReadQueueClass(reg, section_name, warnings);

        // The same sections cannot appear twice
        queue_classes[queue_class] = params;
    }

    return queue_classes;
}


// Returns true if the config file is well formed
void NS_ValidateQueues(const IRegistry &     reg,
                       const TQueueParams &  qclasses,
                       list<string> &        queues,
                       vector<string> &      warnings)
{
    list<string>    sections;

    reg.EnumerateSections(&sections);
    for (list<string>::const_iterator  k = sections.begin();
         k != sections.end(); ++k) {
        string      queue_name;
        string      prefix;
        string      section_name = *k;

        NStr::SplitInTwo(section_name, "_", prefix, queue_name);
        if (NStr::CompareNocase(prefix, "queue") != 0)
           continue;

        if (queue_name.empty()) {
            warnings.push_back(g_ValidPrefix + "section " + section_name +
                               " does not have a queue name");
            continue;
        }

        queues.push_back(queue_name);

        SQueueParameters    params;
        params.ReadQueue(reg, section_name, qclasses, warnings);
    }
}


// Returns true if the config file is well formed
void NS_ValidateServiceToQueueSection(const IRegistry &     reg,
                                      const list<string> &  queues,
                                      vector<string> &      warnings)
{
    const string            section = "service_to_queue";
    list<string>            entries;
    reg.EnumerateEntries(section, &entries);

    for (list<string>::const_iterator  k = entries.begin();
         k != entries.end(); ++k) {
        string      service_name = *k;
        string      qname = reg.Get(section, service_name);
        if (qname.empty()) {
            warnings.push_back(g_ValidPrefix +
                               NS_RegValName(section, service_name) +
                               " does not specify a queue name");
            continue;
        }

        // Check that the queue name has been provided
        if (find(queues.begin(), queues.end(), qname) == queues.end())
            warnings.push_back(g_ValidPrefix +
                               NS_RegValName(section, service_name) +
                               " does not point to an existing static queue");
    }
}


// Forms an out of limits message
string NS_OutOfLimitMessage(const string &  section,
                            const string &  entry,
                            unsigned int  low_limit,
                            unsigned int  high_limit)
{
    return g_ValidPrefix + NS_RegValName(section, entry) +
           " is out of limits (" + NStr::NumericToString(low_limit) + "..." +
           NStr::NumericToString(high_limit) + ")";

}


// Special values for checksums for errors
static bool             s_SpecialChecksumInitialized = false;
static unsigned char    s_CannotOpenChecksum[MD5_DIGEST_LENGTH];
static unsigned char    s_CannotGetSizeChecksum[MD5_DIGEST_LENGTH];
static unsigned char    s_CannotMapChecksum[MD5_DIGEST_LENGTH];

void NS_GetConfigFileChecksum(const string &  file_name,
                              vector<string> & warnings,
                              unsigned char *  md5)
{
    if (!s_SpecialChecksumInitialized) {
        memset(s_CannotOpenChecksum, 0, MD5_DIGEST_LENGTH);
        s_CannotOpenChecksum[MD5_DIGEST_LENGTH - 1] = 1;
        memset(s_CannotGetSizeChecksum, 0, MD5_DIGEST_LENGTH);
        s_CannotGetSizeChecksum[MD5_DIGEST_LENGTH - 1] = 2;
        memset(s_CannotMapChecksum, 0, MD5_DIGEST_LENGTH);
        s_CannotMapChecksum[MD5_DIGEST_LENGTH - 1] = 3;
        s_SpecialChecksumInitialized = true;
    }

    int     fd = open(file_name.c_str(), O_RDONLY);
    if (fd < 0) {
        warnings.push_back("Cannot open configuration file for reading: " +
                           file_name);
        memcpy(md5, s_CannotOpenChecksum, MD5_DIGEST_LENGTH);
        return;
    }

    struct stat     stat_buf;
    if (fstat(fd, &stat_buf) < 0) {
        warnings.push_back("Cannot get configuration file size: " + file_name);
        memcpy(md5, s_CannotGetSizeChecksum, MD5_DIGEST_LENGTH);
        return;
    }

    unsigned char * file_buffer = (unsigned char *)mmap(0, stat_buf.st_size,
                                                        PROT_READ, MAP_SHARED,
                                                        fd, 0);
    if (file_buffer == MAP_FAILED) {
        warnings.push_back("Cannot map configuration file into memory: " +
                           file_name);
        memcpy(md5, s_CannotMapChecksum, MD5_DIGEST_LENGTH);
        return;
    }
    MD5(file_buffer, stat_buf.st_size, md5);
    munmap(file_buffer, stat_buf.st_size);
    close(fd);
}


int NS_CompareChecksums(unsigned char *  lhs_md5,
                        unsigned char *  rhs_md5)
{
    for (size_t  k = 0; k < MD5_DIGEST_LENGTH; ++k) {
        if (lhs_md5[k] < rhs_md5[k])
            return -1;
        if (lhs_md5[k] > rhs_md5[k])
            return 1;
    }
    return 0;
}



END_NCBI_SCOPE

