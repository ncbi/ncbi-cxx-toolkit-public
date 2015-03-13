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
 * Authors:  Pavel Ivanov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <stdio.h>
#ifdef NCBI_COMPILER_MSVC
# define snprintf _snprintf
#endif


USING_NCBI_SCOPE;


class CLogsSplitterApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int Run(void);
};


enum ERequestType
{
    ePUT3,
    eGET2,
    eRMV2
};

struct SRequestInfo
{
    SRequestInfo(ERequestType req)
        : req_type(req), finished(false), gen_key(false),
            req_id(0), req_time(0), key_id(0), size(0)
    {
    }
    ERequestType req_type;
    bool finished;
    bool gen_key;
    Uint8 req_id;
    Uint8 req_time;
    string key;
    Uint8 key_id;
    Uint8 size;
};

struct SKeyIdIndex
{
    int index;
    Uint8 id;

    SKeyIdIndex(int idx, Uint8 id_) : index(idx), id(id_) {}
    SKeyIdIndex(void) : index(0), id(0) {}
};


static CFileIO s_InFile;
static char s_InBuf[1024 * 1024];
static Uint4 s_InPos = 0;
static Uint4 s_InReadSize = 0;
static Uint8 s_TotalInPos = 0;
static vector<CFileIO*> s_OutFiles;

typedef map<string, SKeyIdIndex> TKeyIndexMap;
static TKeyIndexMap s_KeyOutIndexes;
static int s_NextKeyIndex = 0;
static Uint8 s_NextKeyId = 1;
static Uint8 s_StartTime = 0;
typedef map<Uint8, SRequestInfo*> TReqsByIndex;
static TReqsByIndex s_StartedRequests;

// input LOG is not chronological; output - must be. So, we should sort it here

// amount of milliseconds to store before saving into output file
static const Uint8 kMaxMsecToStore = 120000;
// it is possible to have several requests per millisecond;
// but, replay log does not distinguish fractions of msec
static const Uint8 kMaxRequestsPerMsec = 10;
// timestamp to request, and let map sort them
typedef map<Uint8, SRequestInfo*> TReqsQueue;


typedef vector<TReqsQueue> TOutReqsQueues;
static TOutReqsQueues s_OutReqsQueues;


static void
s_FlushRequests(bool skip_unfinished = false)
{
    for (size_t i = 0; i < s_OutReqsQueues.size(); ++i) {
        TReqsQueue& q = s_OutReqsQueues[i];
        CFileIO* io = s_OutFiles[i];
        // we can save up to this time
        Uint8 LastReqTime = (Uint8)-1;
        if (!skip_unfinished && !q.empty()) {
            LastReqTime = q.rbegin()->second->req_time;
            if (LastReqTime > kMaxMsecToStore) {
                LastReqTime -= kMaxMsecToStore;
            } else {
                continue;
            }
        }
        while (!q.empty()) {
            SRequestInfo* req = q.begin()->second;
            if (!skip_unfinished && req->req_time >= LastReqTime) {
                break;
            }
            if (!req->finished) {
                if (!skip_unfinished)
                    break;
                q.erase(q.begin());
                delete req;
                continue;
            }
                q.erase(q.begin());
            char buf[4096];
            const char* str_cmd = NULL;
            int gen_key = 0;
            Uint8 size = 0;
            switch (req->req_type) {
            case ePUT3:
                str_cmd = "PUT3";
                gen_key = int(req->gen_key);
                size = req->size;
                break;
            case eGET2:
                str_cmd = "GET2";
                break;
            case eRMV2:
                str_cmd = "RMV2";
                break;
            }
            int buf_size = snprintf(buf, sizeof(buf),
                                    "%" NCBI_UINT8_FORMAT_SPEC
                                    " %s %" NCBI_UINT8_FORMAT_SPEC
                                    " %d %" NCBI_UINT8_FORMAT_SPEC
#ifdef NCBI_COMPILER_MSVC
                                    "\r\n",
#else
                                    "\n",
#endif
                                    req->req_time, str_cmd, req->key_id,
                                    gen_key, size);
            io->Write(buf, buf_size);

            delete req;
        }
    }
}

static bool
s_ReadNextLine(CTempString& line)
{
retry:
    Uint4 end_pos = s_InPos;
    if (end_pos < s_InReadSize  &&  s_InBuf[end_pos] == '\n')
        s_InPos = ++end_pos;
    while (end_pos < s_InReadSize
           &&  s_InBuf[end_pos] != '\r'  &&  s_InBuf[end_pos] != '\n')
    {
        ++end_pos;
    }
    if (end_pos == s_InReadSize) {
        if (s_InPos == 0  &&  s_InReadSize == sizeof(s_InBuf)) {
            ERR_FATAL("Too long line in the input file");
        }
        memmove(s_InBuf, s_InBuf + s_InPos, s_InReadSize - s_InPos);
        s_InReadSize -= s_InPos;
        s_InPos = 0;
        Uint4 n_read = Uint4(s_InFile.Read(s_InBuf + s_InReadSize,
                                           sizeof(s_InBuf) - s_InReadSize));
        if (n_read == 0)
            return false;
        s_InReadSize += n_read;

        s_FlushRequests();
        goto retry;
    }

    line.assign(s_InBuf + s_InPos, end_pos - s_InPos);
    s_TotalInPos += end_pos - s_InPos;
    s_InPos = end_pos;
    if (s_InBuf[s_InPos] == '\r') {
        ++s_InPos;
        ++s_TotalInPos;
    }

    return true;
}

static const string&
s_GetMsgParam(const SDiagMessage::TExtraArgs& args, const string& name)
{
    ITERATE(SDiagMessage::TExtraArgs, it, args) {
        if (it->first == name)
            return it->second;
    }
    return kEmptyStr;
}

static void
s_SetRequestTime(SRequestInfo* req, const SDiagMessage& msg)
{
    CTime req_ctime = msg.GetTime();
    Uint8 req_time = Uint8(req_ctime.GetTimeT()) * 1000 + req_ctime.MilliSecond();
    if (s_StartTime == 0)
        s_StartTime = req_time;
    req->req_time = req_time - s_StartTime;
}

static bool
s_ReadRequestKey(SRequestInfo* req,
                 const SDiagMessage::TExtraArgs& args,
                 bool allow_new)
{
    const string& key = s_GetMsgParam(args, "key");
    const string& gen_key = s_GetMsgParam(args, "gen_key");
    if (key.empty()  &&  gen_key.empty())
        return false;

    if (key.empty()) {
        req->key = gen_key;
        req->gen_key = true;
    }
    else {
        req->key = key;
        if (gen_key == "1")
            req->gen_key = true;
    }

    int out_idx;
    Uint8 key_id;
    TKeyIndexMap::iterator it_idx = s_KeyOutIndexes.lower_bound(req->key);
    if (it_idx == s_KeyOutIndexes.end()  ||  it_idx->first != req->key) {
        if (!allow_new)
            return false;
        req->gen_key = true;
        out_idx = s_NextKeyIndex++;
        if (s_NextKeyIndex == int(s_OutFiles.size()))
            s_NextKeyIndex = 0;
        key_id = s_NextKeyId++;
        s_KeyOutIndexes.insert(it_idx, TKeyIndexMap::value_type(
                                       req->key, SKeyIdIndex(out_idx, key_id)));
    }
    else {
        out_idx = it_idx->second.index;
        key_id = it_idx->second.id;
    }
    req->key_id = key_id;

    // add to queue
    TReqsQueue& q = s_OutReqsQueues[out_idx];
    Uint8 reqtimestamp = req->req_time * kMaxRequestsPerMsec;
    Uint8 reqtimemax = reqtimestamp + kMaxRequestsPerMsec;
    for ( ; q.find(reqtimestamp) != q.end() && reqtimestamp < reqtimemax; ++reqtimestamp)
        ;

    q[reqtimestamp] = req;

    return true;
}

static void
s_StartRequest(const SDiagMessage& msg)
{
    const string& cmd = s_GetMsgParam(msg.m_ExtraArgs, "cmd");
    if (cmd == "PUT3") {
        SRequestInfo* req = new SRequestInfo(ePUT3);
        req->req_id = msg.m_RequestId;
        s_SetRequestTime(req, msg);
        s_StartedRequests[req->req_id] = req;
        s_ReadRequestKey(req, msg.m_ExtraArgs, true);
    }
    else if (cmd == "GET2") {
        SRequestInfo* req = new SRequestInfo(eGET2);
        s_SetRequestTime(req, msg);
        req->finished = true;
        if (!s_ReadRequestKey(req, msg.m_ExtraArgs, false)) {
            delete req;
        }
    }
    else if (cmd == "RMV2") {
        SRequestInfo* req = new SRequestInfo(eRMV2);
        s_SetRequestTime(req, msg);
        req->finished = true;
        if (!s_ReadRequestKey(req, msg.m_ExtraArgs, false)) {
            delete req;
        }
    }
}

static void
s_AddRequestExtra(const SDiagMessage& msg)
{
    TReqsByIndex::iterator it = s_StartedRequests.find(msg.m_RequestId);
    if (it == s_StartedRequests.end())
        return;

    SRequestInfo* req = it->second;
    if (req->req_type == ePUT3  &&  req->key.empty())
        s_ReadRequestKey(req, msg.m_ExtraArgs, true);
    const string& str_size = s_GetMsgParam(msg.m_ExtraArgs, "blob_size");
    if (!str_size.empty())
        req->size = NStr::StringToUInt8(str_size);
}

static void
s_StopRequest(const SDiagMessage& msg)
{
    TReqsByIndex::iterator it = s_StartedRequests.find(msg.m_RequestId);
    if (it != s_StartedRequests.end()) {
        SRequestInfo* req = it->second;
        req->finished = true;
        s_StartedRequests.erase(it);
    }
}


void
CLogsSplitterApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Split NetCache logs for testing");

    arg_desc->AddPositional("log_file",
                            "File with NetCache logs to split",
                            CArgDescriptions::eInputFile);
    arg_desc->AddPositional("out_file_prefix",
                            "Prefix for output files with test commands",
                            CArgDescriptions::eString);
    arg_desc->AddPositional("n_files",
                            "Number of output files to create",
                            CArgDescriptions::eInteger);
    arg_desc->AddDefaultPositional("start_num",
                                   "Starting number of files to create",
                                   CArgDescriptions::eInteger,
                                   "0");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int
CLogsSplitterApp::Run(void)
{
    const CArgs& args = GetArgs();
    string in_file = args["log_file"].AsString();
    string out_file = args["out_file_prefix"].AsString();
    int n_files = args["n_files"].AsInteger();
    int start_num = args["start_num"].AsInteger();

    s_InFile.Open(in_file, CFileIO::eOpen, CFileIO::eRead);
    s_OutFiles.resize(n_files, NULL);
    s_OutReqsQueues.resize(n_files);
    for (int i = 0; i < n_files; ++i) {
        string file_name = out_file;
        file_name += ".";
        file_name += NStr::IntToString(start_num + i);
        CFileIO* io = new CFileIO();
        io->Open(file_name, CFileIO::eCreate, CFileIO::eReadWrite);
        s_OutFiles[i] = io;
    }

    CTempString line;
    SDiagMessage msg("");
    Uint8 cnt_lines = 0;
    while (s_ReadNextLine(line)) {
        if (!msg.ParseMessage(line)) {
            LOG_POST("Cannot parse input line: " << line);
            continue;
        }
        if (!(msg.m_Flags & eDPF_AppLog))
            continue;

        if (msg.m_Event == SDiagMessage::eEvent_RequestStart)
            s_StartRequest(msg);
        else if (msg.m_Event == SDiagMessage::eEvent_Extra)
            s_AddRequestExtra(msg);
        else if (msg.m_Event == SDiagMessage::eEvent_RequestStop)
            s_StopRequest(msg);

        if ((++cnt_lines % 100000) == 0) {
            LOG_POST("Processed " << cnt_lines << " lines, file_pos=" << s_TotalInPos);
        }
    }

    s_FlushRequests(true);

    return 0;
}


int main(int argc, const char* argv[])
{
    return CLogsSplitterApp().AppMain(argc, argv, 0, eDS_Default);
}
