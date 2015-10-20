#ifndef NETSCHEDULE_SERVER_PARAMS__HPP
#define NETSCHEDULE_SERVER_PARAMS__HPP

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
 * File Description: [server] section of the configuration
 *
 */

#include <corelib/ncbireg.hpp>
#include <connect/server.hpp>

#include <string>
#include <limits>

BEGIN_NCBI_SCOPE



#if defined(_DEBUG) && !defined(NDEBUG)
// error emulator parameters use the following format:
// F:Ff  Fb-Fe
// F is a value (int, bool, double, etc)
// Ff is a frequency (int)
// Fb-Fe is a range of request number when it fails (integers or infinitum)
struct SErrorEmulatorParameter
{
    string      value;      // Could be int, bool or double, so here it is a
                            // generic string as read from the config file
    int         as_int;     // 'value' as int
    double      as_double;  // 'value' as double
    bool        as_bool;    // 'value' as bool

    unsigned int    frequency;  // frequency with which the corresponding event
                                // is emulated
    Uint4           range_begin;
    Uint4           range_end;

    void ReadInt(const IRegistry &  reg,
                 const string &     section,
                 const string &     param_name);
    void ReadDouble(const IRegistry &  reg,
                    const string &     section,
                    const string &     param_name);
    void ReadBool(const IRegistry &  reg,
                  const string &     section,
                  const string &     param_name);

    SErrorEmulatorParameter() :
        value(""), as_int(-1), as_double(0.0), as_bool(false), frequency(1),
        range_begin(0), range_end(numeric_limits<Uint4>::max())
    {}

    bool IsActive(void) const;

    private:
        void x_ReadCommon(const IRegistry &  reg,
                          const string &     section,
                          const string &     param_name);
};
#endif



// Parameters for server
struct SNS_Parameters : SServer_Parameters
{
    unsigned short  port;

    bool            use_hostname;
    unsigned        network_timeout;

    bool            is_log;
    bool            log_batch_each_job;
    bool            log_notification_thread;
    bool            log_cleaning_thread;
    bool            log_execution_watcher_thread;
    bool            log_statistics_thread;

    unsigned int    del_batch_size;     // Max number of jobs to be deleted
                                        // from BDB during one Purge() call
    unsigned int    markdel_batch_size; // Max number of jobs marked for deletion
    unsigned int    scan_batch_size;    // Max number of jobs expiration checks
                                        // during one Purge() call
    double          purge_timeout;      // Timeout in seconds between
                                        // Purge() calls
    unsigned int    stat_interval;      // Interval between statistics output
    unsigned int    max_affinities;     // Max number of affinities a client
                                        // can report as preferred.
    unsigned int    max_client_data;    // Max (transient) client data size

    string          admin_hosts;
    string          admin_client_names;

    // Affinity GC settings
    unsigned int    affinity_high_mark_percentage;
    unsigned int    affinity_low_mark_percentage;
    unsigned int    affinity_high_removal;
    unsigned int    affinity_low_removal;
    unsigned int    affinity_dirt_percentage;

    unsigned int    reserve_dump_space;

    void Read(const IRegistry &  reg);

    #if defined(_DEBUG) && !defined(NDEBUG)
    SErrorEmulatorParameter     debug_fd_count;     // # of used FDs
                                                    //  instead the real FDs
    SErrorEmulatorParameter     debug_mem_count;    // # of used memory bytes
                                                    //  instead the real value
    SErrorEmulatorParameter     debug_write_delay;  // delay before write
                                                    //  into socket

    // connection drops before writing into the client socket
    SErrorEmulatorParameter     debug_conn_drop_before_write;

    // connection drops before writing into the client socket
    SErrorEmulatorParameter     debug_conn_drop_after_write;

    // to send a specified garbage instead of the real response
    SErrorEmulatorParameter     debug_reply_with_garbage;
    string                      debug_garbage;

    void ReadErrorEmulatorSection(const IRegistry &  reg);
    #endif


    private:
        void x_CheckAffinityGarbageCollectorSettings(void);
        void x_CheckGarbageCollectorSettings(void);
};

END_NCBI_SCOPE

#endif

