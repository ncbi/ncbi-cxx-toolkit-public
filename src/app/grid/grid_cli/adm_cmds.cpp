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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Miscellaneous commands of the grid_cli application
 *                   (the ones that are not specific to NetCache, NetSchedule,
 *                   or worker nodes).
 *
 */

#include <ncbi_pch.hpp>

#include "util.hpp"
#include "ns_cmd_impl.hpp"

USING_NCBI_SCOPE;

void CGridCommandLineInterfaceApp::SetUp_AdminCmd(
        CGridCommandLineInterfaceApp::EAdminCmdSeverity cmd_severity)
{
    // For commands that accept only one type of server.
    switch (IsOptionAccepted(eNetCache, OPTION_N(0)) |
            IsOptionAccepted(eNetSchedule, OPTION_N(1)) |
            IsOptionAccepted(eWorkerNode, OPTION_N(2)) |
            IsOptionAccepted(eNetStorage, OPTION_N(3))) {
    case OPTION_N(0): // eNetCache
        SetUp_NetCacheAdminCmd(cmd_severity);
        return;

    case OPTION_N(1): // eNetSchedule
        SetUp_NetScheduleCmd(eNetScheduleAdmin, cmd_severity);
        return;

    case OPTION_N(2): // eWorkerNode
        SetUp_NetScheduleCmd(eWorkerNodeAdmin, cmd_severity);
        return;

    case OPTION_N(3): // eNetStorage
        SetUp_NetStorageCmd(eNetStorageAdmin, cmd_severity);
        return;

    default:
        break;
    }

    // For commands that accept multiple types of servers.
    switch (IsOptionExplicitlySet(eNetCache, OPTION_N(0)) |
            IsOptionExplicitlySet(eNetSchedule, OPTION_N(1)) |
            IsOptionExplicitlySet(eWorkerNode, OPTION_N(2)) |
            IsOptionExplicitlySet(eNetStorage, OPTION_N(3))) {
    case OPTION_N(0): // eNetCache
        SetUp_NetCacheAdminCmd(cmd_severity);
        return;

    case OPTION_N(1): // eNetSchedule
        SetUp_NetScheduleCmd(eNetScheduleAdmin, cmd_severity);
        return;

    case OPTION_N(2): // eWorkerNode
        SetUp_NetScheduleCmd(eWorkerNodeAdmin, cmd_severity);
        return;

    case OPTION_N(3): // eNetStorage
        SetUp_NetStorageCmd(eNetStorageAdmin, cmd_severity);
        return;

    case 0: // No options specified
        NCBI_THROW(CArgException, eNoValue, "this command requires "
                "a service name or a server address");

    default: // A combination of options
        NCBI_THROW(CArgException, eNoValue, "this command works "
                "with only one type of server at a time");
    }
}

int CGridCommandLineInterfaceApp::Cmd_ServerInfo()
{
    CNetService service;

    SetUp_AdminCmd(eReadOnlyAdminCmd);

    bool server_version_key = true;

    switch (m_APIClass) {
    case eNetCacheAdmin:
        service = m_NetCacheAPI.GetService();
        break;

    case eNetScheduleAdmin:
    case eWorkerNodeAdmin:
        server_version_key = false;
        service = m_NetScheduleAPI.GetService();
        break;

    case eNetStorageAdmin:
        return PrintNetStorageServerInfo();

    default:
        return 2;
    }

    if (m_Opts.output_format == eJSON)
        g_PrintJSON(stdout, g_ServerInfoToJson(service,
                service.GetServiceType(), server_version_key));
    else if (m_Opts.output_format == eRaw)
        service.PrintCmdOutput("VERSION", NcbiCout,
                CNetService::eSingleLineOutput,
                CNetService::eIncludePenalized);
    else {
        bool print_server_address = service.IsLoadBalanced();

        for (CNetServiceIterator it =
                service.Iterate(CNetService::eIncludePenalized); it; ++it) {
            if (print_server_address)
                printf("[%s]\n", (*it).GetServerAddress().c_str());

            CNetServerInfo server_info((*it).GetServerInfo());

            string attr_name, attr_value;

            while (server_info.GetNextAttribute(attr_name, attr_value))
                printf("%s: %s\n", attr_name.c_str(), attr_value.c_str());

            if (print_server_address)
                printf("\n");
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Stats()
{
    SetUp_AdminCmd(eReadOnlyAdminCmd);

    switch (m_APIClass) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.PrintStat(NcbiCout, m_Opts.aggregation_interval,
                !IsOptionSet(ePreviousInterval) ?
                        CNetCacheAdmin::eReturnCurrentPeriod :
                        CNetCacheAdmin::eReturnCompletePeriod);
        return 0;

    case eNetScheduleAdmin:
        return PrintNetScheduleStats();

    case eWorkerNodeAdmin:
        if (m_Opts.output_format == eJSON)
            g_PrintJSON(stdout, g_WorkerNodeInfoToJson(
                    m_NetScheduleAPI.GetService().Iterate().GetServer()));
        else
            m_NetScheduleAdmin.PrintServerStatistics(NcbiCout);
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Health()
{
    SetUp_AdminCmd(eReadOnlyAdminCmd);

    switch (m_APIClass) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.PrintHealth(NcbiCout);
        return 0;

    case eNetScheduleAdmin:
        m_NetScheduleAdmin.PrintHealth(NcbiCout);
        return 0;

    default:
        return 2;
    }
}

#define CHECK_FAILED_RETVAL 10

int CGridCommandLineInterfaceApp::NetCacheSanityCheck()
{
    // functionality test

    const char test_data[] = "A quick brown fox, jumps over lazy dog.";
    const char test_data2[] = "Test 2.";
    string key = m_NetCacheAPI.PutData(test_data, sizeof(test_data));

    if (key.empty()) {
        NcbiCerr << "Failed to put data. " << NcbiEndl;
        return CHECK_FAILED_RETVAL;
    }
    NcbiCout << key << NcbiEndl;

    char data_buf[1024];

    size_t blob_size = m_NetCacheAPI.GetBlobSize(key);

    if (blob_size != sizeof(test_data)) {
        NcbiCerr << "Failed to retrieve data size." << NcbiEndl;
        return CHECK_FAILED_RETVAL;
    }

    auto_ptr<IReader> reader(m_NetCacheAPI.GetData(key, &blob_size,
            nc_caching_mode = CNetCacheAPI::eCaching_Disable));

    if (reader.get() == 0) {
        NcbiCerr << "Failed to read data." << NcbiEndl;
        return CHECK_FAILED_RETVAL;
    }

    reader->Read(data_buf, 1024);
    int res = strcmp(data_buf, test_data);
    if (res != 0) {
        NcbiCerr << "Could not read data." << NcbiEndl <<
            "Server returned:" << NcbiEndl << data_buf << NcbiEndl <<
            "Expected:" << NcbiEndl << test_data << NcbiEndl;

        return CHECK_FAILED_RETVAL;
    }
    reader.reset(0);

    {{
        auto_ptr<IEmbeddedStreamWriter> wrt(m_NetCacheAPI.PutData(&key));
        size_t bytes_written;
        wrt->Write(test_data2, sizeof(test_data2), &bytes_written);
        wrt->Close();
    }}

    memset(data_buf, 0xff, sizeof(data_buf));
    reader.reset(m_NetCacheAPI.GetReader(key, &blob_size,
            nc_caching_mode = CNetCacheAPI::eCaching_Disable));
    reader->Read(data_buf, 1024);
    res = strcmp(data_buf, test_data2);
    if (res != 0) {
        NcbiCerr << "Could not read updated data." << NcbiEndl <<
            "Server returned:" << NcbiEndl << data_buf << NcbiEndl <<
            "Expected:" << NcbiEndl << test_data2 << NcbiEndl;

        return CHECK_FAILED_RETVAL;
    }

    return 0;
}

int CGridCommandLineInterfaceApp::NetScheduleSanityCheck()
{
    if (!IsOptionSet(eQueue)) {
        if (!IsOptionSet(eQueueClass)) {
            fprintf(stderr, GRID_APP_NAME ": '--" QUEUE_OPTION
                    "' or '--" QUEUE_CLASS_OPTION
                    "' (or both) must be specified.\n");
            return 2;
        }
        m_Opts.queue = NETSCHEDULE_CHECK_QUEUE;
    }
    if (IsOptionSet(eQueueClass))
        m_NetScheduleAdmin.CreateQueue(m_Opts.queue, m_Opts.queue_class);

    CNetScheduleAdmin::TQueueList server_queues;

    m_NetScheduleAdmin.GetQueueList(server_queues);

    ITERATE(CNetScheduleAdmin::TQueueList, it, server_queues) {
        list<string>::const_iterator queue(it->queues.begin());
        for (;;) {
            if (queue == it->queues.end()) {
                fprintf(stderr, "The queue '%s' is not available on '%s'.\n",
                        m_Opts.queue.c_str(),
                        it->server.GetServerAddress().c_str());
                return 4;
            }
            if (*queue == m_Opts.queue)
                break;
            ++queue;
        }
    }

    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    const string input = "Hello ";
    const string output = "DONE ";
    CNetScheduleJob job(input);
    m_NetScheduleSubmitter.SubmitJob(job);

    for (;;) {
        CNetScheduleJob job1;
        bool job_exists = m_NetScheduleExecutor.GetJob(job1, 5);
        if (job_exists) {
            if (job1.job_id != job.job_id)
                m_NetScheduleExecutor.ReturnJob(job1);
            else {
                if (job1.input != job.input) {
                    job1.error_msg = "Job's (" + job1.job_id +
                        ") input does not match.(" + job.input +
                        ") ["+ job1.input +"]";
                    m_NetScheduleExecutor.PutFailure(job1);
                } else {
                    job1.output = output;
                    job1.ret_code = 0;
                    m_NetScheduleExecutor.PutResult(job1);
                }
                break;
            }
        }
    }

    bool check_again = true;
    int ret = 0;
    string err;
    while (check_again) {
        check_again = false;

        CNetScheduleAPI::EJobStatus status = m_NetScheduleSubmitter.GetJobDetails(job);
        switch(status) {

        case CNetScheduleAPI::eJobNotFound:
            ret = 10;
            err = "Job (" + job.job_id +") is lost.";
            break;
        case CNetScheduleAPI::eCanceled:
            ret = 12;
            err = "Job (" + job.job_id +") is canceled.";
            break;
        case CNetScheduleAPI::eFailed:
            ret = 13;
            break;
        case CNetScheduleAPI::eDone:
            if (job.ret_code != 0) {
                ret = job.ret_code;
                err = "Job (" + job.job_id +") is done, but retcode is not zero.";
            } else if (job.output != output) {
                err = "Job (" + job.job_id + ") is done, output does not match.("
                    + output + ") ["+ job.output +"]";
                ret = 14;
            } else if (job.input != input) {
                err = "Job (" + job.job_id +") is done, input does not match.("
                    + input + ") ["+ job.input +"]";
                ret = 15;
            }
            break;
        case CNetScheduleAPI::ePending:
        case CNetScheduleAPI::eRunning:
        default:
            check_again = true;
        }
    }

    if (ret != 0)
        ERR_POST(err);

    if (IsOptionSet(eQueueClass))
        m_NetScheduleAdmin.DeleteQueue(m_Opts.queue);

    return ret;
}

int CGridCommandLineInterfaceApp::Cmd_SanityCheck()
{
    SetUp_AdminCmd(eAdminCmdWithSideEffects);

    switch (m_APIClass) {
    case eNetCacheAdmin:
        return NetCacheSanityCheck();

    case eNetScheduleAdmin:
        return NetScheduleSanityCheck();

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_GetConf()
{
    SetUp_AdminCmd(eReadOnlyAdminCmd);

    switch (m_APIClass) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.PrintConfig(NcbiCout);
        return 0;

    case eNetScheduleAdmin:
        m_NetScheduleAdmin.PrintConf(NcbiCout);
        return 0;

    case eNetStorageAdmin:
        return PrintNetStorageServerConfig();

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Reconf()
{
    SetUp_AdminCmd(eAdminCmdWithSideEffects);

    switch (m_APIClass) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.ReloadServerConfig(IsOptionExplicitlySet(eMirror) ?
                CNetCacheAdmin::eMirrorReload : CNetCacheAdmin::eCompleteReload);
        return 0;

    case eNetScheduleAdmin:
        g_PrintJSON(stdout, g_ReconfAndReturnJson(m_NetScheduleAPI,
                m_NetScheduleAPI.GetService().GetServiceType()));
        return 0;

    case eNetStorageAdmin:
        return ReconfigureNetStorageServer();

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Drain()
{
    SetUp_AdminCmd(eAdminCmdWithSideEffects);

    switch (m_APIClass) {
    case eNetScheduleAdmin:
        m_NetScheduleAdmin.SwitchToDrainMode(m_Opts.on_off_switch);
        return 0;

    default:
        return 2;
    }
    return 0;
}

void CGridCommandLineInterfaceApp::NetSchedule_SuspendResume(bool suspend)
{
    if (IsOptionAcceptedAndSetImplicitly(eQueue)) {
        NCBI_THROW(CArgException, eNoValue,
                "missing option '--" QUEUE_OPTION "'");
    }

    if (suspend)
        g_SuspendNetSchedule(m_NetScheduleAPI, IsOptionSet(ePullback));
    else
        g_ResumeNetSchedule(m_NetScheduleAPI);
}

int CGridCommandLineInterfaceApp::Cmd_Suspend()
{
    SetUp_AdminCmd(eAdminCmdWithSideEffects);

    switch (m_APIClass) {
    case eNetScheduleAdmin:
        NetSchedule_SuspendResume(true);
        return 0;

    case eWorkerNodeAdmin:
        {
            CNetServer worker_node =
                    m_NetScheduleAPI.GetService().Iterate().GetServer();
            g_SuspendWorkerNode(worker_node,
                    IsOptionSet(ePullback), m_Opts.timeout);
            if (IsOptionSet(eWaitForJobCompletion)) {
                string output_line;
                for (;;) {
                    unsigned jobs_running = 0;
                    CNetServerMultilineCmdOutput stat_output(
                            worker_node.ExecWithRetry("STAT", true));
                    while (stat_output.ReadLine(output_line))
                        if (NStr::StartsWith(output_line, "Jobs Running: "))
                            jobs_running = NStr::StringToUInt(
                                    output_line.c_str() +
                                    sizeof("Jobs Running: ") - 1);
                    if (jobs_running == 0)
                        break;
                    SleepMilliSec(500);
                }
            }
        }
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Resume()
{
    SetUp_AdminCmd(eAdminCmdWithSideEffects);

    switch (m_APIClass) {
    case eNetScheduleAdmin:
        NetSchedule_SuspendResume(false);
        return 0;

    case eWorkerNodeAdmin:
        g_ResumeWorkerNode(m_NetScheduleAPI.GetService().Iterate().GetServer());
        return 0;

    default:
        return 2;
    }
}

int CGridCommandLineInterfaceApp::Cmd_Shutdown()
{
    SetUp_AdminCmd(eAdminCmdWithSideEffects);

    switch (m_APIClass) {
    case eNetCacheAdmin:
        m_NetCacheAdmin.ShutdownServer(IsOptionSet(eDrain) ?
                CNetCacheAdmin::eDrain : CNetCacheAdmin::eNormalShutdown);
        return 0;

    case eNetScheduleAdmin:
    case eWorkerNodeAdmin:
        switch (IsOptionSet(eNow, OPTION_N(0)) |
                IsOptionSet(eDie, OPTION_N(1)) |
                IsOptionSet(eDrain, OPTION_N(2))) {
        case 0: // No additional options.
            m_NetScheduleAdmin.ShutdownServer();
            return 0;

        case OPTION_N(0): // eNow is set.
            m_NetScheduleAdmin.ShutdownServer(
                CNetScheduleAdmin::eShutdownImmediate);
            return 0;

        case OPTION_N(1): // eDie is set.
            m_NetScheduleAdmin.ShutdownServer(CNetScheduleAdmin::eDie);
            return 0;

        case OPTION_N(2): // eDrain is set.
            m_NetScheduleAdmin.ShutdownServer(CNetScheduleAdmin::eDrain);
            return 0;

        default: // A combination of the above options.
            fprintf(stderr, GRID_APP_NAME ": options '--" NOW_OPTION "', '--"
                    DIE_OPTION "', and '--" DRAIN_OPTION
                    "' are mutually exclusive.\n");
        }
        /* FALL THROUGH */

    case eNetStorageAdmin:
        return ShutdownNetStorageServer();

    default:
        return 2;
    }
}

namespace {
    class CAdjustableTableColumn {
    public:
        enum {
            fAlignRight = 1,
            fLastColumn = 2
        };
        CAdjustableTableColumn(int min_width = 0,
                int spacing = 1,
                unsigned mode = 0) :
            m_Width(min_width),
            m_Spacing(spacing),
            m_Mode(mode)
        {
        }
        void AddCell(const string& text)
        {
            int length = (int)text.length();
            if (m_Width < length)
                m_Width = length;
            m_Cells.push_back(text);
        }
        void PrintCell(size_t row)
        {
            switch (m_Mode & (fAlignRight | fLastColumn)) {
            default:
                printf("%-*s", m_Width + m_Spacing, m_Cells[row].c_str());
                break;
            case fAlignRight:
                printf("%*s%*s", m_Width, m_Cells[row].c_str(), m_Spacing, "");
                break;
            case fLastColumn:
                printf("%s\n", m_Cells[row].c_str());
                break;
            case fAlignRight | fLastColumn:
                printf("%*s\n", m_Width, m_Cells[row].c_str());
            }
        }
        void PrintRule()
        {
            const static char eight_dashes[] = "--------";

            int width = m_Width;
            while (width > 8) {
                printf("%s", eight_dashes);
                width -= 8;
            }

            if ((m_Mode & fLastColumn) == 0)
                printf("%-*.*s", width + m_Spacing, width, eight_dashes);
            else
                printf("%.*s\n", width, eight_dashes);
        }
        size_t GetHeight() const {return m_Cells.size();}

    private:
        int m_Width;
        int m_Spacing;
        unsigned m_Mode;
        vector<string> m_Cells;
    };
}

#define NCBI_DOMAIN ".ncbi.nlm.nih.gov"

int CGridCommandLineInterfaceApp::Cmd_Discover()
{
    CNetService service = g_DiscoverService(m_Opts.service_name, m_Opts.auth);

    CAdjustableTableColumn ipv4_column(0, 2);
    ipv4_column.AddCell("IPv4 Address");

    CAdjustableTableColumn hostname_column(0, 2);
    if (!IsOptionSet(eNoDNSLookup))
        hostname_column.AddCell("Hostname");

    CAdjustableTableColumn port_column(0, 2,
            CAdjustableTableColumn::fAlignRight);
    port_column.AddCell("Port");

    CAdjustableTableColumn rating_column(7, 0,
            CAdjustableTableColumn::fAlignRight |
            CAdjustableTableColumn::fLastColumn);
    rating_column.AddCell("Rating");

    for (CNetServiceIterator it =
            service.Iterate(CNetService::eIncludePenalized); it; ++it) {
        CNetServer server(it.GetServer());
        unsigned ipv4 = server.GetHost();
        string ipv4_str = CSocketAPI::ntoa(ipv4);
        ipv4_column.AddCell(ipv4_str);
        if (!IsOptionSet(eNoDNSLookup)) {
            string hostname = CSocketAPI::gethostbyaddr(ipv4, eOff);
            if (hostname.empty() || hostname == ipv4_str)
                hostname = "***DNS lookup failed***";
            else if (NStr::EndsWith(hostname, NCBI_DOMAIN))
                hostname.erase(hostname.length() - (sizeof(NCBI_DOMAIN) - 1));
            hostname_column.AddCell(hostname);
        }
        port_column.AddCell(NStr::UIntToString(server.GetPort()));
        rating_column.AddCell(NStr::DoubleToString(it.GetRate(), 3));
    }

    for (unsigned row = 0; row < ipv4_column.GetHeight(); ++row) {
        ipv4_column.PrintCell(row);
        if (!IsOptionSet(eNoDNSLookup))
            hostname_column.PrintCell(row);
        port_column.PrintCell(row);
        rating_column.PrintCell(row);
        if (row == 0) {
            ipv4_column.PrintRule();
            if (!IsOptionSet(eNoDNSLookup))
                hostname_column.PrintRule();
            port_column.PrintRule();
            rating_column.PrintRule();
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Exec()
{
    SetUp_AdminCmd(eAdminCmdWithSideEffects);

    CNetService service;

    switch (m_APIClass) {
    case eNetCacheAdmin:
        service = m_NetCacheAPI.GetService();
        break;

    case eNetScheduleAdmin:
        service = m_NetScheduleAPI.GetService();
        break;

    default:
        return 2;
    }

    if (m_Opts.output_format == eRaw)
        service.PrintCmdOutput(m_Opts.command, NcbiCout,
                IsOptionSet(eMultiline) ? CNetService::eMultilineOutput :
                CNetService::eSingleLineOutput);
    else // Output format is eJSON.
        g_PrintJSON(stdout, g_ExecAnyCmdToJson(service,
                service.GetServiceType(),
                m_Opts.command, IsOptionSet(eMultiline)));

    return 0;
}
