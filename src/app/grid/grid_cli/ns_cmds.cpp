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
 * File Description: NetSchedule-specific commands of the grid_cli application.
 *
 */

#include <ncbi_pch.hpp>

#include "ns_cmd_impl.hpp"
#include "util.hpp"

#include <connect/services/remote_app.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/ns_job_serializer.hpp>

#include <corelib/rwstream.hpp>

#include <ctype.h>

BEGIN_NCBI_SCOPE

void CGridCommandLineInterfaceApp::SetUp_NetScheduleCmd(
        CGridCommandLineInterfaceApp::EAPIClass api_class,
        CGridCommandLineInterfaceApp::EAdminCmdSeverity cmd_severity)
{
    if (api_class == eNetScheduleSubmitter)
        SetUp_NetCacheCmd(eNetCacheAPI);

    string queue = m_Opts.queue;

    if (!IsOptionSet(eID) && !IsOptionSet(eJobId))
        m_NetScheduleAPI = CNetScheduleAPI(m_Opts.ns_service,
            m_Opts.auth, queue);
    else {
        CNetScheduleKey key(m_Opts.id, m_CompoundIDPool);

        if (queue.empty())
            queue = key.queue;

        if (IsOptionExplicitlySet(eNetSchedule)) {
            string host, port;

            if (!NStr::SplitInTwo(m_Opts.ns_service, ":", host, port)) {
                NCBI_THROW(CArgException, eInvalidArg,
                        "When job ID is given, '--" NETSCHEDULE_OPTION "' "
                        "must be a host:port server address.");
            }

            m_NetScheduleAPI = CNetScheduleAPI(m_Opts.ns_service,
                m_Opts.auth, queue);
            m_NetScheduleAPI.GetService().GetServerPool().StickToServer(host,
                (unsigned short) NStr::StringToInt(port));
        } else {
            key.host.push_back(':');
            key.host.append(NStr::NumericToString(key.port));
            m_NetScheduleAPI = CNetScheduleAPI(key.host, m_Opts.auth, queue);
        }
    }

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    if (IsOptionSet(eAllowXSiteConn))
        m_NetScheduleAPI.GetService().AllowXSiteConnections();
#endif

    if (IsOptionSet(eCompatMode)) {
        m_NetScheduleAPI.UseOldStyleAuth();
        if (IsOptionSet(eWorkerNode))
            m_NetScheduleAPI.EnableWorkerNodeCompatMode();
    }

    // Specialize NetSchedule API.
    switch (api_class) {
    case eNetScheduleAPI:
        break;
    case eNetScheduleAdmin:
        if (cmd_severity != eReadOnlyAdminCmd) {
            if (!IsOptionExplicitlySet(eNetSchedule)) {
                NCBI_THROW(CArgException, eNoValue, "'--" NETSCHEDULE_OPTION
                        "' must be explicitly specified.");
            }
            if (IsOptionAcceptedAndSetImplicitly(eQueue)) {
                NCBI_THROW(CArgException, eNoValue, "'--" QUEUE_OPTION
                        "' must be specified explicitly (not via $"
                        LOGIN_TOKEN_ENV ").");
            }
        }
        /* FALL THROUGH */
    case eWorkerNodeAdmin:
        m_NetScheduleAdmin = m_NetScheduleAPI.GetAdmin();
        break;
    case eNetScheduleExecutor:
        m_NetScheduleExecutor = m_NetScheduleAPI.GetExecutor();

        if (!IsOptionSet(eLoginToken) &&
                !IsOptionSet(eClientNode) && !IsOptionSet(eClientSession)) {
            NCBI_THROW(CArgException, eNoArg, "client identification required "
                    "(see the '" LOGIN_COMMAND "' command).");
        }
        /* FALL THROUGH */
    case eNetScheduleSubmitter:
        m_NetScheduleSubmitter = m_NetScheduleAPI.GetSubmitter();

        m_GridClient.reset(new CGridClient(m_NetScheduleSubmitter,
                m_NetCacheAPI, CGridClient::eManualCleanup,
                        CGridClient::eProgressMsgOn));
        break;
    default:
        _ASSERT(0);
        break;
    }

    if (!IsOptionSet(eClientNode)) {
        string user, host;
        g_GetUserAndHost(&user, &host);
        DefineClientNode(user, host);
    }

    if (!m_Opts.client_node.empty())
        m_NetScheduleAPI.SetClientNode(m_Opts.client_node);

    if (!IsOptionSet(eClientSession))
        DefineClientSession((Uint8) CProcess::GetCurrentPid(),
                (Int8) GetFastLocalTime().GetTimeT(),
                GetDiagContext().GetStringUID());

    if (!m_Opts.client_session.empty())
        m_NetScheduleAPI.SetClientSession(m_Opts.client_session);
}

void CGridCommandLineInterfaceApp::JobInfo_PrintStatus(
        CNetScheduleAPI::EJobStatus status)
{
    string job_status(CNetScheduleAPI::StatusToString(status));

    if (m_Opts.output_format == eJSON)
        printf("{\"status\": \"%s\"}\n", job_status.c_str());
    else
        PrintLine(job_status);
}

int CGridCommandLineInterfaceApp::Cmd_JobInfo()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin, eReadOnlyAdminCmd);

    if (IsOptionSet(eDeferExpiration)) {
        CNetScheduleAPI::EJobStatus status =
                m_NetScheduleAPI.GetSubmitter().GetJobStatus(m_Opts.id);
        if (IsOptionSet(eStatusOnly)) {
            JobInfo_PrintStatus(status);
            return 0;
        }
    } else if (IsOptionSet(eStatusOnly)) {
        JobInfo_PrintStatus(
                m_NetScheduleAPI.GetExecutor().GetJobStatus(m_Opts.id));
        return 0;
    }

    if (IsOptionSet(eProgressMessageOnly)) {
        CNetScheduleJob job;
        job.job_id = m_Opts.id;
        m_NetScheduleAPI.GetProgressMsg(job);
        if (m_Opts.output_format == eJSON)
            printf("{\"progress_message\": \"%s\"}\n",
                    job.progress_msg.c_str());
        else
            if (!job.progress_msg.empty())
                PrintLine(job.progress_msg);
        return 0;
    }

    switch (m_Opts.output_format) {
    case eRaw:
        if (IsOptionSet(eBrief)) {
            fprintf(stderr, GRID_APP_NAME " " JOBINFO_COMMAND ": option '--"
                    BRIEF_OPTION "' cannot be used with '"
                    RAW_OUTPUT_FORMAT "' output format.\n");
            return 2;
        }
        m_NetScheduleAdmin.DumpJob(NcbiCout, m_Opts.id);
        break;

    case eJSON:
        {
            CJobInfoToJSON job_info_to_json;
            g_ProcessJobInfo(m_NetScheduleAPI, m_Opts.id,
                    &job_info_to_json, !IsOptionSet(eBrief), m_CompoundIDPool);
            g_PrintJSON(stdout, job_info_to_json.GetRootNode());
        }
        break;

    default:
        {
            CPrintJobInfo print_job_info;
            g_ProcessJobInfo(m_NetScheduleAPI, m_Opts.id,
                    &print_job_info, !IsOptionSet(eBrief), m_CompoundIDPool);
        }
    }

    return 0;
}

class CBatchSubmitAttrParser
{
public:
    CBatchSubmitAttrParser(istream* input_stream) :
        m_InputStream(input_stream),
        m_LineNumber(0)
    {
    }
    bool NextLine();
    bool NextAttribute();
    EOption GetAttributeType() const {return m_JobAttribute;}
    const string& GetAttributeValue() const {return m_JobAttributeValue;}
    size_t GetLineNumber() const {return m_LineNumber;}

private:
    istream* m_InputStream;
    size_t m_LineNumber;
    string m_Line;
    CAttrListParser m_AttrParser;
    EOption m_JobAttribute;
    string m_JobAttributeValue;
};

bool CBatchSubmitAttrParser::NextLine()
{
    if (m_InputStream == NULL)
        return false;

    ++m_LineNumber;

    getline(*m_InputStream, m_Line);

    if (m_InputStream->fail()) {
        m_InputStream = NULL;
        return false;
    }

    if (m_InputStream->eof()) {
        m_InputStream = NULL;
        if (m_Line.empty())
            return false;
    }

    m_AttrParser.Reset(m_Line);
    return true;
}

bool CBatchSubmitAttrParser::NextAttribute()
{
    m_JobAttribute = eUntypedArg;

#define ATTR_CHECK_SET(name, type) \
    if (attr_name.length() == sizeof(name) - 1 && \
            memcmp(attr_name.data(), name, sizeof(name) - 1) == 0) { \
        m_JobAttribute = type; \
        break; \
    }

    CTempString attr_name;
    size_t attr_column;

    CAttrListParser::ENextAttributeType next_attr_type =
            m_AttrParser.NextAttribute(&attr_name,
                    &m_JobAttributeValue, &attr_column);

    if (next_attr_type == CAttrListParser::eNoMoreAttributes)
        return false;

    switch (attr_name[0]) {
    case 'i':
        ATTR_CHECK_SET("input", eInput);
        break;
    case 'a':
        ATTR_CHECK_SET("args", eRemoteAppArgs);
        ATTR_CHECK_SET("affinity", eAffinity);
        break;
    case 'e':
        ATTR_CHECK_SET("exclusive", eExclusiveJob);
    }

#define ATTR_POS " at line " << m_LineNumber << ", column " << attr_column

    switch (m_JobAttribute) {
    case eUntypedArg:
        NCBI_THROW_FMT(CArgException, eInvalidArg,
            "unknown attribute " << attr_name << ATTR_POS);

    case eExclusiveJob:
        break;

    default:
        if (next_attr_type != CAttrListParser::eAttributeWithValue) {
            NCBI_THROW_FMT(CArgException, eInvalidArg,
                "attribute " << attr_name << " requires a value" ATTR_POS);
        }
    }

    return true;
}

static const string s_NotificationTimestampFormat("Y/M/D h:m:s.l");

void CGridCommandLineInterfaceApp::PrintJobStatusNotification(
        CNetScheduleNotificationHandler& submit_job_handler,
        const string& job_key,
        const string& server_host)
{
    CNetScheduleAPI::EJobStatus job_status = CNetScheduleAPI::eJobNotFound;
    int last_event_index = -1;

    const char* format = "%s \"%s\" from %s [invalid]\n";

    if (submit_job_handler.CheckJobStatusNotification(job_key,
            &job_status, &last_event_index))
        format = "%s \"%s\" from %s [valid, "
                "job_status=%s, last_event_index=%d]\n";

    printf(format, GetFastLocalTime().
            AsString(s_NotificationTimestampFormat).c_str(),
            submit_job_handler.GetMessage().c_str(),
            server_host.c_str(),
            CNetScheduleAPI::StatusToString(job_status).c_str(),
            last_event_index);
}

void CGridCommandLineInterfaceApp::CheckJobInputStream(
        CNcbiOstream& job_input_ostream)
{
    if (job_input_ostream.fail()) {
        NCBI_THROW(CIOException, eWrite, "Error while writing job input");
    }
}

void CGridCommandLineInterfaceApp::PrepareRemoteAppJobInput(
        size_t max_embedded_input_size,
        const string& args,
        CNcbiIstream& remote_app_stdin,
        CNcbiOstream& job_input_ostream)
{
    CRemoteAppRequest request(m_GridClient->GetNetCacheAPI());

    // Roughly estimate the maximum embedded input size.
    request.SetMaxInlineSize(max_embedded_input_size == 0 ?
            numeric_limits<size_t>().max() :
            max_embedded_input_size - max_embedded_input_size / 10);

    request.SetCmdLine(args);

    remote_app_stdin.peek();
    if (!remote_app_stdin.eof())
        NcbiStreamCopy(request.GetStdIn(), remote_app_stdin);

    request.Send(job_input_ostream);
}

struct SBatchSubmitRecord {
    CBatchSubmitAttrParser attr_parser;

    string job_input;
    bool job_input_defined;
    string remote_app_args;
    bool remote_app_args_defined;
    string affinity;
    bool exclusive_job;

    SBatchSubmitRecord(istream* input_stream) : attr_parser(input_stream) {}

    bool LoadNextRecord();
};

bool SBatchSubmitRecord::LoadNextRecord()
{
    if (!attr_parser.NextLine())
        return false;

    job_input = kEmptyStr;
    remote_app_args = kEmptyStr;
    affinity = kEmptyStr;
    job_input_defined = remote_app_args_defined = exclusive_job = false;

    while (attr_parser.NextAttribute())
        switch (attr_parser.GetAttributeType()) {
        case eInput:
            if (job_input_defined) {
                NCBI_THROW_FMT(CArgException, eInvalidArg,
                        "More than one \"input\" attribute is defined "
                        "at line " << attr_parser.GetLineNumber());
            }
            job_input = attr_parser.GetAttributeValue();
            job_input_defined = true;
            break;
        case eRemoteAppArgs:
            if (remote_app_args_defined) {
                NCBI_THROW_FMT(CArgException, eInvalidArg,
                        "More than one \"args\" attribute is defined "
                        "at line " << attr_parser.GetLineNumber());
            }
            remote_app_args = attr_parser.GetAttributeValue();
            remote_app_args_defined = true;
            break;
        case eAffinity:
            affinity = attr_parser.GetAttributeValue();
            break;
        case eExclusiveJob:
            exclusive_job = true;
            break;
        default:
            _ASSERT(0);
            break;
        }

    if (!job_input_defined && !remote_app_args_defined) {
        NCBI_THROW_FMT(CArgException, eInvalidArg, "\"input\" "
                "(and/or \"args\") attribute is required "
                "at line " << attr_parser.GetLineNumber());
    }

    return true;
}

void CGridCommandLineInterfaceApp::x_LoadJobInput(
        size_t max_embedded_input_size, CNcbiOstream &job_input_ostream)
{
    if (IsOptionSet(eRemoteAppArgs)) {
        if (!IsOptionSet(eInput))
            PrepareRemoteAppJobInput(max_embedded_input_size,
                    m_Opts.remote_app_args,
                    *m_Opts.input_stream, job_input_ostream);
        else {
            CNcbiStrstream remote_app_stdin;
            remote_app_stdin.write(m_Opts.input.data(),
                    m_Opts.input.length());
            PrepareRemoteAppJobInput(max_embedded_input_size,
                    m_Opts.remote_app_args,
                    remote_app_stdin, job_input_ostream);
        }
    } else if (IsOptionSet(eInput))
        job_input_ostream.write(m_Opts.input.data(), m_Opts.input.length());
    else
        NcbiStreamCopy(job_input_ostream, *m_Opts.input_stream);

    CheckJobInputStream(job_input_ostream);
}

void CGridCommandLineInterfaceApp::SubmitJob_Batch()
{
    SBatchSubmitRecord job_input_record(m_Opts.input_stream);

    size_t max_embedded_input_size = m_GridClient->GetMaxServerInputSize();

    if (m_Opts.batch_size <= 1) {
        while (job_input_record.LoadNextRecord()) {
            CNcbiOstream& job_input_ostream(m_GridClient->GetOStream());

            if (job_input_record.remote_app_args_defined) {
                CNcbiStrstream remote_app_stdin;
                remote_app_stdin.write(job_input_record.job_input.data(),
                        job_input_record.job_input.length());
                PrepareRemoteAppJobInput(max_embedded_input_size,
                        job_input_record.remote_app_args,
                        remote_app_stdin, job_input_ostream);
            } else {
                job_input_ostream.write(job_input_record.job_input.data(),
                        job_input_record.job_input.length());
            }

            CheckJobInputStream(job_input_ostream);

            if (!job_input_record.affinity.empty())
                m_GridClient->SetJobAffinity(job_input_record.affinity);

            if (job_input_record.exclusive_job)
                m_GridClient->SetJobMask(CNetScheduleAPI::eExclusiveJob);

            if (IsOptionSet(eGroup))
                m_GridClient->SetJobGroup(m_Opts.job_group);

            fprintf(m_Opts.output_stream,
                "%s\n", m_GridClient->Submit(m_Opts.affinity).c_str());
        }
    } else {
        CGridJobBatchSubmitter& batch_submitter(
            m_GridClient->GetJobBatchSubmitter());
        unsigned remaining_batch_size = m_Opts.batch_size;

        while (job_input_record.LoadNextRecord()) {
            if (remaining_batch_size == 0) {
                batch_submitter.Submit(m_Opts.job_group);
                const vector<CNetScheduleJob>& jobs =
                    batch_submitter.GetBatch();
                ITERATE(vector<CNetScheduleJob>, it, jobs)
                    fprintf(m_Opts.output_stream,
                        "%s\n", it->job_id.c_str());
                batch_submitter.Reset();
                remaining_batch_size = m_Opts.batch_size;
            }
            batch_submitter.PrepareNextJob();

            CNcbiOstream& job_input_ostream(batch_submitter.GetOStream());

            if (job_input_record.remote_app_args_defined) {
                CNcbiStrstream remote_app_stdin;
                remote_app_stdin.write(job_input_record.job_input.data(),
                        job_input_record.job_input.length());
                PrepareRemoteAppJobInput(max_embedded_input_size,
                        job_input_record.remote_app_args,
                        remote_app_stdin, job_input_ostream);
            } else
                job_input_ostream.write(job_input_record.job_input.data(),
                        job_input_record.job_input.length());

            CheckJobInputStream(job_input_ostream);

            if (!job_input_record.affinity.empty())
                batch_submitter.SetJobAffinity(job_input_record.affinity);

            if (job_input_record.exclusive_job)
                batch_submitter.SetJobMask(CNetScheduleAPI::eExclusiveJob);

            --remaining_batch_size;
        }
        if (remaining_batch_size < m_Opts.batch_size) {
            batch_submitter.Submit(m_Opts.job_group);
            const vector<CNetScheduleJob>& jobs =
                batch_submitter.GetBatch();
            ITERATE(vector<CNetScheduleJob>, it, jobs)
                fprintf(m_Opts.output_stream, "%s\n", it->job_id.c_str());
            batch_submitter.Reset();
        }
    }
}

int CGridCommandLineInterfaceApp::Cmd_SubmitJob()
{
    SetUp_NetScheduleCmd(eNetScheduleSubmitter);

    if (IsOptionSet(eBatch)) {
        if (IsOptionSet(eJobInputDir)) {
            NCBI_THROW(CArgException, eInvalidArg, "'--" JOB_INPUT_DIR_OPTION
                "' option is not supported in batch mode");
        }
        SubmitJob_Batch();
    } else if (IsOptionSet(eJobInputDir)) {
        CNetScheduleJob job;

        job.affinity = m_Opts.affinity;
        job.group = m_Opts.job_group;

        CCompoundID job_key = m_CompoundIDPool.NewID(eCIC_GenericID);
        job_key.AppendCurrentTime();
        job_key.AppendRandom();
        job.job_id = job_key.ToString();

        {{
            CStringOrBlobStorageWriter job_input_writer(
                    numeric_limits<size_t>().max(), NULL, job.input);
            CWStream job_input_ostream(&job_input_writer, 0, NULL);

            x_LoadJobInput(0, job_input_ostream);
        }}

        CNetScheduleJobSerializer job_serializer(job, m_CompoundIDPool);
        job_serializer.SaveJobInput(m_Opts.job_input_dir, m_NetCacheAPI);

        PrintLine(job.job_id);
    } else {
        CNcbiOstream& job_input_ostream = m_GridClient->GetOStream();

        size_t max_embedded_input_size = m_GridClient->GetMaxServerInputSize();

        x_LoadJobInput(max_embedded_input_size, job_input_ostream);

        m_GridClient->SetJobGroup(m_Opts.job_group);
        m_GridClient->SetJobAffinity(m_Opts.affinity);

        if (IsOptionSet(eExclusiveJob))
            m_GridClient->SetJobMask(CNetScheduleAPI::eExclusiveJob);

        if (!IsOptionSet(eWaitTimeout))
            PrintLine(m_GridClient->Submit());
        else {
            m_GridClient->CloseStream();

            CNetScheduleJob& job = m_GridClient->GetJob();

            CDeadline deadline(m_Opts.timeout, 0);

            CNetScheduleNotificationHandler submit_job_handler;

            submit_job_handler.SubmitJob(m_NetScheduleSubmitter,
                job, m_Opts.timeout);

            PrintLine(job.job_id);

            if (!IsOptionSet(eDumpNSNotifications)) {
                CNetScheduleAPI::EJobStatus status =
                    submit_job_handler.WaitForJobCompletion
                    (job, deadline, m_NetScheduleAPI);

                PrintLine(CNetScheduleAPI::StatusToString(status));

                if (status == CNetScheduleAPI::eDone) {
                    if (IsOptionSet(eRemoteAppArgs))
                        MarkOptionAsSet(eRemoteAppStdOut);
                    DumpJobInputOutput(job.output);
                }
            } else {
                submit_job_handler.PrintPortNumber();

                string server_host;

                if (submit_job_handler.WaitForNotification(deadline,
                                                           &server_host))
                    PrintJobStatusNotification(submit_job_handler,
                            job.job_id, server_host);
            }
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_WatchJob()
{
    SetUp_NetScheduleCmd(eNetScheduleSubmitter);

    if (!IsOptionSet(eWaitTimeout)) {
        fprintf(stderr, GRID_APP_NAME " " WATCHJOB_COMMAND
            ": option '--" WAIT_TIMEOUT_OPTION "' is required.\n");
        return 2;
    }

    if (!IsOptionSet(eWaitForJobStatus) && !IsOptionSet(eWaitForJobEventAfter))
        m_Opts.job_status_mask = ~((1 << CNetScheduleAPI::ePending) |
                (1 << CNetScheduleAPI::eRunning));

    CDeadline deadline(m_Opts.timeout, 0);

    CNetScheduleNotificationHandler submit_job_handler;

    CNetScheduleAPI::EJobStatus job_status;
    int last_event_index = -1;

    if (!submit_job_handler.RequestJobWatching(m_NetScheduleAPI, m_Opts.id,
            deadline, &job_status, &last_event_index)) {
        fprintf(stderr, GRID_APP_NAME ": unexpected error while "
                "setting up a job event listener.\n");
        return 3;
    }

    if (!IsOptionSet(eDumpNSNotifications)) {
        if (last_event_index <= m_Opts.last_event_index &&
                (m_Opts.job_status_mask & (1 << job_status)) == 0)
            job_status = submit_job_handler.WaitForJobEvent(m_Opts.id,
                    deadline, m_NetScheduleAPI, m_Opts.job_status_mask,
                    m_Opts.last_event_index, &last_event_index);

        printf("%d\n%s\n", last_event_index,
                CNetScheduleAPI::StatusToString(job_status).c_str());
    } else {
        if (last_event_index > m_Opts.last_event_index) {
            fprintf(stderr, "Job event index (%d) has already "
                    "exceeded %d; won't wait.\n",
                    last_event_index, m_Opts.last_event_index);
            return 6;
        }
        if ((m_Opts.job_status_mask & (1 << job_status)) != 0) {
            fprintf(stderr, "Job is already '%s'; won't wait.\n",
                    CNetScheduleAPI::StatusToString(job_status).c_str());
            return 6;
        }

        submit_job_handler.PrintPortNumber();

        string server_host;

        while (submit_job_handler.WaitForNotification(deadline,
                                                      &server_host))
            PrintJobStatusNotification(submit_job_handler,
                    m_Opts.id, server_host);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::DumpJobInputOutput(
    const string& data_or_blob_id)
{
    char buffer[IO_BUFFER_SIZE];
    size_t bytes_read;

    if (IsOptionSet(eRemoteAppStdOut) || IsOptionSet(eRemoteAppStdErr)) {
        auto_ptr<IReader> reader(new CStringOrBlobStorageReader(data_or_blob_id,
                m_NetCacheAPI));

        auto_ptr<CNcbiIstream> input_stream(new CRStream(reader.release(), 0,
                0, CRWStreambuf::fOwnReader | CRWStreambuf::fLeakExceptions));

        CRemoteAppResult remote_app_result(m_NetCacheAPI);
        remote_app_result.Receive(*input_stream);

        CNcbiIstream& std_stream(IsOptionSet(eRemoteAppStdOut) ?
                remote_app_result.GetStdOut() : remote_app_result.GetStdErr());

        std_stream.exceptions((ios::iostate) 0);

        while (!std_stream.eof()) {
            std_stream.read(buffer, sizeof(buffer));
            if (std_stream.fail())
                goto Error;
            bytes_read = (size_t) std_stream.gcount();
            if (fwrite(buffer, 1, bytes_read,
                    m_Opts.output_stream) < bytes_read)
                goto Error;
        }

        return 0;
    }

    try {
        CStringOrBlobStorageReader reader(data_or_blob_id, m_NetCacheAPI);

        while (reader.Read(buffer, sizeof(buffer), &bytes_read) != eRW_Eof)
            if (fwrite(buffer, 1, bytes_read,
                    m_Opts.output_stream) < bytes_read)
                goto Error;
    }
    catch (CStringOrBlobStorageRWException& e) {
        if (e.GetErrCode() != CStringOrBlobStorageRWException::eInvalidFlag)
            throw;
        if (fwrite(data_or_blob_id.data(), 1, data_or_blob_id.length(),
                m_Opts.output_stream) < data_or_blob_id.length())
            goto Error;
    }

    return 0;

Error:
    fprintf(stderr, GRID_APP_NAME ": error while writing job data.\n");
    return 3;
}

int CGridCommandLineInterfaceApp::PrintJobAttrsAndDumpInput(
    const CNetScheduleJob& job)
{
    PrintLine(job.job_id);
    if (!job.auth_token.empty())
        printf("%s ", job.auth_token.c_str());
    if (!job.affinity.empty()) {
        string affinity(NStr::PrintableString(job.affinity));
        printf(job.mask & CNetScheduleAPI::eExclusiveJob ?
            "affinity=\"%s\" exclusive\n" : "affinity=\"%s\"\n",
            affinity.c_str());
    } else
        printf(job.mask & CNetScheduleAPI::eExclusiveJob ?
            "exclusive\n" : "\n");
    return DumpJobInputOutput(job.input);
}

int CGridCommandLineInterfaceApp::Cmd_GetJobInput()
{
    SetUp_NetScheduleCmd(eNetScheduleSubmitter);

    CNetScheduleJob job;
    job.job_id = m_Opts.id;

    if (m_NetScheduleAPI.GetJobDetails(job) == CNetScheduleAPI::eJobNotFound) {
        fprintf(stderr, GRID_APP_NAME ": job %s has expired.\n",
                job.job_id.c_str());
        return 3;
    }

    return DumpJobInputOutput(job.input);
}

int CGridCommandLineInterfaceApp::Cmd_GetJobOutput()
{
    SetUp_NetScheduleCmd(eNetScheduleSubmitter);

    CNetScheduleJob job;
    job.job_id = m_Opts.id;
    CNetScheduleAPI::EJobStatus status = m_NetScheduleAPI.GetJobDetails(job);

    switch (status) {
    case CNetScheduleAPI::eDone:
    case CNetScheduleAPI::eReading:
    case CNetScheduleAPI::eConfirmed:
    case CNetScheduleAPI::eReadFailed:
        break;

    default:
        fprintf(stderr, GRID_APP_NAME
            ": cannot retrieve job output for job status %s.\n",
            CNetScheduleAPI::StatusToString(status).c_str());
        return 3;
    }

    return DumpJobInputOutput(job.output);
}

int CGridCommandLineInterfaceApp::Cmd_ReadJob()
{
    SetUp_NetScheduleCmd(eNetScheduleSubmitter);

    if (!IsOptionSet(eConfirmRead) && !IsOptionSet(eFailRead) &&
            !IsOptionSet(eRollbackRead)) {
        string job_id, auth_token;
        CNetScheduleAPI::EJobStatus job_status;

        if (m_NetScheduleSubmitter.Read(&job_id, &auth_token, &job_status,
                m_Opts.timeout, m_Opts.job_group)) {
            PrintLine(job_id);
            PrintLine(CNetScheduleAPI::StatusToString(job_status));

            if (IsOptionSet(eReliableRead))
                PrintLine(auth_token);
            else {
                if (job_status == CNetScheduleAPI::eDone) {
                    CNetScheduleJob job;
                    job.job_id = job_id;
                    m_NetScheduleSubmitter.GetJobDetails(job);
                    int ret_code = DumpJobInputOutput(job.output);
                    if (ret_code != 0)
                        return ret_code;
                }
                m_NetScheduleSubmitter.ReadConfirm(job_id, auth_token);
            }
        }
    } else {
        if (!IsOptionSet(eJobId)) {
            fprintf(stderr, GRID_APP_NAME " " READJOB_COMMAND
                ": option '--" JOB_ID_OPTION "' is required.\n");
            return 2;
        }

        if (IsOptionSet(eConfirmRead))
            m_NetScheduleSubmitter.ReadConfirm(m_Opts.id, m_Opts.auth_token);
        else if (IsOptionSet(eFailRead))
            m_NetScheduleSubmitter.ReadFail(m_Opts.id, m_Opts.auth_token,
                    m_Opts.error_message);
        else
            m_NetScheduleSubmitter.ReadRollback(m_Opts.id, m_Opts.auth_token);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CancelJob()
{
    if (IsOptionSet(eJobGroup)) {
        SetUp_NetScheduleCmd(eNetScheduleAPI);

        m_NetScheduleAPI.GetSubmitter().CancelJobGroup(m_Opts.job_group);
    } else if (IsOptionSet(eAllJobs)) {
        SetUp_NetScheduleCmd(eNetScheduleAdmin, eSevereAdminCmd);

        m_NetScheduleAdmin.CancelAllJobs();
    } else {
        SetUp_NetScheduleCmd(eNetScheduleAPI);

        m_NetScheduleAPI.GetSubmitter().CancelJob(m_Opts.id);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RequestJob()
{
    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    CNetScheduleExecutor::EJobAffinityPreference affinity_preference =
            CNetScheduleExecutor::eExplicitAffinitiesOnly;

    switch (IsOptionSet(eUsePreferredAffinities, OPTION_N(0)) |
            IsOptionSet(eClaimNewAffinities, OPTION_N(1)) |
            IsOptionSet(eAnyAffinity, OPTION_N(2))) {
    case OPTION_N(2) + OPTION_N(0):
        affinity_preference = CNetScheduleExecutor::ePreferredAffsOrAnyJob;
        break;

    case OPTION_N(1):
    case OPTION_N(1) + OPTION_N(0):
        affinity_preference = CNetScheduleExecutor::eClaimNewPreferredAffs;
        break;

    case OPTION_N(0):
        affinity_preference = CNetScheduleExecutor::ePreferredAffinities;
        break;

    case 0:
        if (IsOptionSet(eAffinityList))
            break;
        /* FALL THROUGH */

    case OPTION_N(2):
        affinity_preference = CNetScheduleExecutor::eAnyJob;
        break;

    default:
        fprintf(stderr, GRID_APP_NAME ": options '--"
            CLAIM_NEW_AFFINITIES_OPTION "' and '--" ANY_AFFINITY_OPTION
            "' are mutually exclusive.\n");
        return 2;
    }

    m_NetScheduleExecutor.SetAffinityPreference(affinity_preference);

    CNetScheduleJob job;

    if (!IsOptionSet(eDumpNSNotifications)) {
        if (m_NetScheduleExecutor.GetJob(job, m_Opts.timeout, m_Opts.affinity))
            return PrintJobAttrsAndDumpInput(job);
    } else {
        CDeadline deadline(m_Opts.timeout, 0);

        CNetScheduleNotificationHandler wait_job_handler;

        wait_job_handler.PrintPortNumber();

        if (wait_job_handler.RequestJob(m_NetScheduleExecutor, job,
                wait_job_handler.CmdAppendTimeoutAndClientInfo(
                CNetScheduleNotificationHandler::MkBaseGETCmd(
                affinity_preference, m_Opts.affinity), &deadline))) {
            fprintf(stderr, "%s\nA job has been returned; won't wait.\n",
                    job.job_id.c_str());
            return 6;
        }

        string server_host;
        CNetServer server;
        string server_address;

        while (wait_job_handler.WaitForNotification(deadline,
                                                    &server_host)) {
            const char* format = "%s \"%s\" from %s [invalid]\n";

            if (wait_job_handler.CheckRequestJobNotification(
                    m_NetScheduleExecutor, &server)) {
                server_address = server.GetServerAddress();
                format = "%s \"%s\" from %s [valid, server=%s]\n";
            }

            printf(format, GetFastLocalTime().AsString(
                    s_NotificationTimestampFormat).c_str(),
                    wait_job_handler.GetMessage().c_str(),
                    server_host.c_str(),
                    server_address.c_str());
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CommitJob()
{
    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    CNetScheduleJob job;

    job.job_id = m_Opts.id;
    job.ret_code = m_Opts.return_code;
    job.auth_token = m_Opts.auth_token;

    if (IsOptionSet(eJobOutputBlob))
        job.output = "K " + m_Opts.job_output_blob;
    else {
        auto_ptr<IEmbeddedStreamWriter> writer(new CStringOrBlobStorageWriter(
                m_NetScheduleAPI.GetServerParams().max_output_size,
                m_NetCacheAPI, job.output));

        if (!IsOptionSet(eJobOutput)) {
            char buffer[IO_BUFFER_SIZE];

            do {
                m_Opts.input_stream->read(buffer, sizeof(buffer));
                if (m_Opts.input_stream->fail() &&
                        !m_Opts.input_stream->eof()) {
                    NCBI_THROW(CIOException, eRead,
                            "Error while reading job output data");
                }
                if (writer->Write(buffer,
                        (size_t) m_Opts.input_stream->gcount()) != eRW_Success)
                    goto ErrorExit;
            } while (!m_Opts.input_stream->eof());
        } else
            if (writer->Write(m_Opts.job_output.data(),
                    m_Opts.job_output.length()) != eRW_Success)
                goto ErrorExit;
    }

    if (!IsOptionSet(eFailJob))
        m_NetScheduleExecutor.PutResult(job);
    else {
        job.error_msg = m_Opts.error_message;
        m_NetScheduleExecutor.PutFailure(job);
    }

    return 0;

ErrorExit:
    fprintf(stderr, GRID_APP_NAME ": error while sending job output.\n");
    return 3;
}

int CGridCommandLineInterfaceApp::Cmd_ReturnJob()
{
    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    m_NetScheduleExecutor.ReturnJob(m_Opts.id, m_Opts.auth_token);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_ClearNode()
{
    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    m_NetScheduleExecutor.ClearNode();

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_UpdateJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAPI);

    if (IsOptionSet(eExtendLifetime) ||
            IsOptionSet(eProgressMessage)) {
        CNetScheduleExecutor executor(m_NetScheduleAPI.GetExecutor());

        if (IsOptionSet(eExtendLifetime))
            executor.JobDelayExpiration(m_Opts.id,
                    (unsigned) m_Opts.extend_lifetime_by);

        if (IsOptionSet(eProgressMessage)) {
            CNetScheduleJob job;
            job.job_id = m_Opts.id;
            job.progress_msg = m_Opts.progress_message;
            executor.PutProgressMsg(job);
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_QueueInfo()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin, eReadOnlyAdminCmd);

    if (!IsOptionSet(eQueueClasses)) {
        if ((IsOptionSet(eQueueArg) ^ IsOptionSet(eAllQueues)) == 0) {
            fprintf(stderr, GRID_APP_NAME " " QUEUEINFO_COMMAND
                    ": either the '" QUEUE_ARG "' argument or the '--"
                    ALL_QUEUES_OPTION "' option must be specified.\n");
            return 1;
        }
        if (m_Opts.output_format == eJSON)
            g_PrintJSON(stdout, g_QueueInfoToJson(m_NetScheduleAPI,
                    IsOptionSet(eQueueArg) ? m_Opts.queue : kEmptyStr,
                    m_NetScheduleAPI.GetService().GetServiceType()));
        else if (!IsOptionSet(eAllQueues))
            m_NetScheduleAdmin.PrintQueueInfo(m_Opts.queue, NcbiCout);
        else {
            m_NetScheduleAPI.GetService().PrintCmdOutput("STAT QUEUES",
                    NcbiCout, CNetService::eMultilineOutput);
        }
    } else if (m_Opts.output_format == eJSON)
        g_PrintJSON(stdout, g_QueueClassInfoToJson(m_NetScheduleAPI,
                m_NetScheduleAPI.GetService().GetServiceType()));
    else
        m_NetScheduleAPI.GetService().PrintCmdOutput("STAT QCLASSES",
                NcbiCout, CNetService::eMultilineOutput);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_DumpQueue()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin, eReadOnlyAdminCmd);

    m_NetScheduleAdmin.DumpQueue(NcbiCout, m_Opts.start_after_job,
            m_Opts.job_count, m_Opts.job_status, m_Opts.job_group);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CreateQueue()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin, eSevereAdminCmd);

    m_NetScheduleAdmin.CreateQueue(m_Opts.id,
            m_Opts.queue_class, m_Opts.queue_description);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetQueueList()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin, eReadOnlyAdminCmd);

    CNetScheduleAdmin::TQueueList queues;

    m_NetScheduleAdmin.GetQueueList(queues);

    typedef set<string> TServerSet;
    typedef map<string, TServerSet> TQueueRegister;

    TQueueRegister queue_register;

    ITERATE (CNetScheduleAdmin::TQueueList, it, queues) {
        string server_address =
                g_NetService_gethostnamebyaddr(it->server.GetHost());
        server_address += ':';
        server_address += NStr::UIntToString(it->server.GetPort());

        ITERATE(std::list<std::string>, queue, it->queues) {
            queue_register[*queue].insert(server_address);
        }
    }

    ITERATE(TQueueRegister, it, queue_register) {
        NcbiCout << it->first;
        if (it->second.size() != queues.size()) {
            const char* sep = " (limited to ";
            ITERATE(TServerSet, server, it->second) {
                NcbiCout << sep << *server;
                sep = ", ";
            }
            NcbiCout << ")";
        }
        NcbiCout << NcbiEndl;
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_DeleteQueue()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin, eSevereAdminCmd);

    m_NetScheduleAdmin.DeleteQueue(m_Opts.id);

    return 0;
}

END_NCBI_SCOPE
