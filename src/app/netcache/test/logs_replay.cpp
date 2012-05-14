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
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/services/netcache_api.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/md5.hpp>
#include <util/random_gen.hpp>

#ifdef NCBI_OS_LINUX
# include <signal.h>
# include <time.h>
#endif


USING_NCBI_SCOPE;


class CLogsReplayApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int Run(void);
};


struct SKeyInfo
{
    string key;
    Uint8 size;
    string md5;
};


class CReplayThread : public CThread
{
public:
    CReplayThread(const string& file_prefix, int file_num);
    virtual ~CReplayThread(void) {};

private:
    virtual void* Main(void);

    bool x_ReadNextLine(CTempString& line);
    void x_PutBlob(Uint8 key_id, bool gen_key, Uint8 size);
    void x_GetBlob(Uint8 key_id);
    void x_Remove(Uint8 key_id);


    CFileIO m_InFile;
    CNetCacheAPI m_NC;
    typedef map<Uint8, SKeyInfo> TIdKeyMap;
    TIdKeyMap m_IdKeys;
    Uint4 m_InPos;
    Uint4 m_InReadSize;
    Uint8 m_TotalRead;
    char m_InBuf[1024 * 1024];
    char m_GetBuf[1024 * 1024];
    char m_LastChunkBuf[1024 * 1024];

public:
    Uint8 m_ReqTime;
    vector<Uint8> m_CntWrites;
    vector<Uint8> m_CntErased;
    vector<Uint8> m_CntReads;
    vector<Uint8> m_CntErrWrites;
    vector<Uint8> m_CntErrReads;
    vector<Uint8> m_CntBadReads;
    vector<Uint8> m_CntNotFound;
    vector<Uint8> m_SumWrites;
    vector<Uint8> m_SumReads;
    vector<Uint8> m_SizeWrites;
    vector<Uint8> m_SizeErased;
    vector<Uint8> m_SizeReads;
};


static bool s_SelfGen = false;
static string s_InFile;
static string s_NCService;
static Uint8 s_StartTime = 0;
static bool s_NeedConfReload = false;
static CAtomicCounter s_ThreadsFinished;
static CFastMutex s_ReloadMutex;
static vector< CRef<CReplayThread> >* s_Threads;

static CAtomicCounter s_BlobId;
static CFastMutex s_RndLock;
static CRandom s_Rnd((CRandom::TValue)CProcess::GetCurrentPid());


static Uint4
s_GetLogBase2(Uint8 value)
{
    unsigned int result = 0;
    if (value > 0xFFFFFFFF) {
        value >>= 32;
        result += 32;
    }
    if (value > 0xFFFF) {
        value >>= 16;
        result += 16;
    }
    if (value > 0xFF) {
        value >>= 8;
        result += 8;
    }
    if (value > 0xF) {
        value >>= 4;
        result += 4;
    }
    if (value > 0x3) {
        value >>= 2;
        result += 2;
    }
    if (value > 0x1)
        ++result;
    return result;
}

static Uint4
s_GetSizeIndex(Uint8 size)
{
    if (size <= 1)
        return 0;
    --size;
    unsigned int result = s_GetLogBase2(size);
    return result;
}

static inline string
s_SafeDiv(Uint8 left, Uint8 right)
{
    return right == 0? "0": NStr::UInt8ToString(left / right, NStr::fWithCommas);
}

static string
s_ToSizeStr(Uint8 size)
{
    static const char* const posts[] = {" B", " KB", " MB", " GB", " TB", " PB"};

    string res = NStr::UInt8ToString(size);
    size_t dot_pos = res.size();
    Uint1 post_idx = 0;
    while (dot_pos > 3  &&  post_idx < sizeof(posts) / sizeof(posts[0])) {
        dot_pos -= 3;
        ++post_idx;
    }
    if (dot_pos >= 3  ||  res.size() <= 3) {
        res.resize(dot_pos);
    }
    else {
        res.resize(3);
        res.insert(dot_pos, 1, '.');
    }
    return res + posts[post_idx];
}

static void
s_PrintStats(int elapsed)
{
    Uint8 m_MinReqTime = numeric_limits<Uint8>::max();
    Uint8 m_MaxReqTime = 0;
    vector<Uint8> m_CntWrites(100);
    vector<Uint8> m_CntErased(100);
    vector<Uint8> m_CntReads(100);
    vector<Uint8> m_CntErrWrites(100);
    vector<Uint8> m_CntErrReads(100);
    vector<Uint8> m_CntBadReads(100);
    vector<Uint8> m_CntNotFound(100);
    vector<Uint8> m_SumWrites(100);
    vector<Uint8> m_SumReads(100);
    vector<Uint8> m_SizeWrites(100);
    vector<Uint8> m_SizeErased(100);
    vector<Uint8> m_SizeReads(100);

    Uint8 tot_w = 0, tot_er = 0, tot_r = 0, tot_err = 0, tot_bad = 0, tot_nf = 0;
    Uint8 tot_ws = 0, tot_ers = 0, tot_rs = 0;
    for (size_t i = 0; i < s_Threads->size(); ++i) {
        for (size_t j = 0; j < m_CntWrites.size(); ++j) {
            Uint8 req_time = s_Threads->at(i)->m_ReqTime;
            m_MinReqTime = min(m_MinReqTime, req_time);
            m_MaxReqTime = max(m_MaxReqTime, req_time);
            Uint8 cnt = s_Threads->at(i)->m_CntWrites[j];
            m_CntWrites[j] += cnt;
            tot_w += cnt;
            cnt = s_Threads->at(i)->m_CntErased[j];
            m_CntErased[j] += cnt;
            tot_er += cnt;
            cnt = s_Threads->at(i)->m_CntReads[j];
            m_CntReads[j] += cnt;
            tot_r += cnt;
            cnt = s_Threads->at(i)->m_CntErrWrites[j];
            m_CntErrWrites[j] += cnt;
            tot_err += cnt;
            cnt = s_Threads->at(i)->m_CntErrReads[j];
            m_CntErrReads[j] += cnt;
            tot_err += cnt;
            cnt = s_Threads->at(i)->m_CntBadReads[j];
            m_CntBadReads[j] += cnt;
            tot_bad += cnt;
            cnt = s_Threads->at(i)->m_CntNotFound[j];
            m_CntNotFound[j] += cnt;
            tot_nf += cnt;
            m_SumWrites[j] += s_Threads->at(i)->m_SumWrites[j];
            m_SumReads[j] += s_Threads->at(i)->m_SumReads[j];
            Uint8 size = s_Threads->at(i)->m_SizeWrites[j];
            m_SizeWrites[j] += size;
            tot_ws += size;
            size = s_Threads->at(i)->m_SizeErased[j];
            m_SizeErased[j] += size;
            tot_ers += size;
            size = s_Threads->at(i)->m_SizeReads[j];
            m_SizeReads[j] += size;
            tot_rs += size;
        }
    }

    LOG_POST("   ");
    LOG_POST("   ");
    LOG_POST("Time: " << CTime(CTime::eCurrent));
    LOG_POST("Elapsed: " << elapsed << " secs, processing " << s_InFile
                         << " starting from " << s_StartTime
                         << ", min req_time " << (m_MinReqTime / 1000)
                         << ", max req_time " << (m_MaxReqTime / 1000));
    LOG_POST("Total: " << tot_w << " (w) " << s_ToSizeStr(tot_ws) << " (ws) "
                       << tot_er << " (er) " << s_ToSizeStr(tot_ers) << " (ers) "
                       << tot_r << " (r) " << s_ToSizeStr(tot_rs) << " (rs) "
                       << tot_err << " (err), " << tot_bad << " (bad), "
                       << tot_nf << " (nf)");
    LOG_POST("   ");
    Uint8 prev_size = 0, size = 2;
    for (size_t i = 0; i < m_CntWrites.size(); ++i, prev_size = size + 1, size <<= 1)
    {
        if (m_CntWrites[i] == 0  &&  m_CntErrWrites[i] == 0)
            continue;

        LOG_POST(prev_size << "-" << size << ": "
                 << m_CntWrites[i] << " (w) "
                 << s_ToSizeStr(m_SizeWrites[i]) << " (ws) "
                 << s_SafeDiv(m_SumWrites[i], m_CntWrites[i] - m_CntErrWrites[i]) << " (wt) "
                 << m_CntErased[i] << " (er) "
                 << s_ToSizeStr(m_SizeErased[i]) << " (ers) "
                 << m_CntReads[i] << " (r) "
                 << s_ToSizeStr(m_SizeReads[i]) << " (rs) "
                 << s_SafeDiv(m_SumReads[i], m_CntReads[i] - m_CntErrReads[i]) << " (rt) "
                 << m_CntErrWrites[i] << " (we) "
                 << m_CntErrReads[i] << " (re) "
                 << m_CntBadReads[i] << " (bad) "
                 << m_CntNotFound[i] << " (nf) "
                );
    }
    LOG_POST("   ");
    LOG_POST("   ");
}


CReplayThread::CReplayThread(const string& file_prefix, int file_num)
    : m_NC(s_NCService, "logs_replay_" + NStr::IntToString(file_num)),
      m_InPos(0),
      m_InReadSize(0),
      m_TotalRead(0),
      m_CntWrites(100),
      m_CntErased(100),
      m_CntReads(100),
      m_CntErrWrites(100),
      m_CntErrReads(100),
      m_CntBadReads(100),
      m_CntNotFound(100),
      m_SumWrites(100),
      m_SumReads(100),
      m_SizeWrites(100),
      m_SizeErased(100),
      m_SizeReads(100)
{
    STimeout to;
    to.sec = 2;
    to.usec = 0;
    m_NC.SetCommunicationTimeout(to);

    string file_name = file_prefix;
    file_name += ".";
    file_name += NStr::IntToString(file_num);
    m_InFile.Open(file_name, CFileIO::eOpen, CFileIO::eRead);
}

bool
CReplayThread::x_ReadNextLine(CTempString& line)
{
retry:
    Uint4 end_pos = m_InPos;
    if (end_pos < m_InReadSize  &&  m_InBuf[end_pos] == '\n')
        m_InPos = ++end_pos;
    while (end_pos < m_InReadSize
           &&  m_InBuf[end_pos] != '\r'  &&  m_InBuf[end_pos] != '\n')
    {
        ++end_pos;
    }
    if (end_pos == m_InReadSize) {
        if (m_InPos == 0  &&  m_InReadSize == sizeof(m_InBuf)) {
            LOG_POST(Fatal << "Too long line in the input file");
        }
        memmove(m_InBuf, m_InBuf + m_InPos, m_InReadSize - m_InPos);
        m_InReadSize -= m_InPos;
        m_InPos = 0;
        Uint4 n_read = Uint4(m_InFile.Read(m_InBuf + m_InReadSize,
                                           sizeof(m_InBuf) - m_InReadSize));
        if (n_read == 0) {
            /*LOG_POST("Cannot read file " << m_InFile.GetPathname()
                     << " (attempted to read "
                     << (sizeof(m_InBuf) - m_InReadSize)
                     << " at position " << m_TotalRead
                     << " with last req_time=" << m_ReqTime << ")");*/
            return false;
        }
        m_InReadSize += n_read;
        m_TotalRead += n_read;
        goto retry;
    }

    line.assign(m_InBuf + m_InPos, end_pos - m_InPos);
    m_InPos = end_pos;
    if (m_InBuf[m_InPos] == '\r')
        ++m_InPos;

    return true;
}

static inline Uint4
s_GetRandom(void)
{
    CFastMutexGuard guard(s_RndLock);
    return s_Rnd.GetRand() * 2;
}

void
CReplayThread::x_PutBlob(Uint8 key_id, bool gen_key, Uint8 size)
{
    string key;
    Uint8 blob_size = size;
    SKeyInfo* key_info = NULL;
    if (!gen_key) {
        TIdKeyMap::iterator it_key = m_IdKeys.find(key_id);
        if (it_key == m_IdKeys.end()) {
            gen_key = true;
        }
        else {
            key_info = &it_key->second;
            key = key_info->key;
        }
    }
    if (gen_key  &&  s_SelfGen) {
        CNetCacheKey::GenerateBlobKey(&key, s_BlobId.Add(1), "130.14.24.171", 9000, 1, s_GetRandom());
        CNetCacheKey::AddExtensions(key, s_NCService);
    }

    Uint4 size_index = s_GetSizeIndex(size);
    ++m_CntWrites[size_index];
    try {
#ifdef NCBI_OS_LINUX
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        Uint8 start_time = Uint8(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
#endif

        auto_ptr<IEmbeddedStreamWriter> writer(m_NC.PutData(&key));
        Uint8 orig_size = size;
        while (size > sizeof(m_InBuf)) {
            writer->Write(m_InBuf, sizeof(m_InBuf));
            size -= sizeof(m_InBuf);
        }
        size_t last_pos = 0;
        if (size != 0) {
            Uint4 cnt_chunks = Uint4(sizeof(m_InBuf) / size);
            last_pos = size_t((key_id % cnt_chunks) * size);
            writer->Write(m_InBuf + last_pos, size);
        }
        writer->Close();

#ifdef NCBI_OS_LINUX
        clock_gettime(CLOCK_REALTIME, &ts);
        Uint8 end_time = Uint8(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
        Uint8 pass_time = end_time - start_time;
        m_SumWrites[size_index] += pass_time;
        m_SizeWrites[size_index] += blob_size;
#endif

        size = orig_size;
        CMD5 md5;
        while (size > sizeof(m_InBuf)) {
            md5.Update(m_InBuf, sizeof(m_InBuf));
            size -= sizeof(m_InBuf);
        }
        if (size != 0)
            md5.Update(m_InBuf + last_pos, size);

        if (key_info) {
            if (!key_info->md5.empty()) {
                Uint4 old_index = s_GetSizeIndex(key_info->size);
                ++m_CntErased[old_index];
                m_SizeErased[old_index] += key_info->size;
            }
        }
        else {
            key_info = &m_IdKeys[key_id];
            key_info->key = key;
        }
        key_info->size = blob_size;
        key_info->md5 = md5.GetHexSum();
    }
    catch (CException& ex) {
        ERR_POST(CTime(CTime::eCurrent).AsString("h:m:s.r")
                 << " Error while writing blob with key '" << key << "': " << ex);
        ++m_CntErrWrites[size_index];
        if (key_info)
            key_info->size = Uint8(-1);
    }
}

void
CReplayThread::x_GetBlob(Uint8 key_id)
{
    TIdKeyMap::iterator it_key = m_IdKeys.find(key_id);
    if (it_key == m_IdKeys.end())
        return;
    SKeyInfo* key_info = &it_key->second;
    size_t blob_size = 0;
    Uint4 size_index = s_GetSizeIndex(key_info->size);
    ++m_CntReads[size_index];
    try {
#ifdef NCBI_OS_LINUX
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        Uint8 start_time = Uint8(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
#endif

        auto_ptr<IReader> reader(m_NC.GetData(key_info->key, &blob_size));
        if (!reader.get()) {
            if (!key_info->md5.empty()  &&  key_info->size != Uint8(-1)) {
                ERR_POST(Warning << "Blob " << key_info->key << " not found");
                ++m_CntNotFound[size_index];
            }
            key_info->md5.resize(0);
            key_info->size = Uint8(-1);
            return;
        }
        if (key_info->size != Uint8(-1)) {
            if (key_info->md5.empty()) {
                ERR_POST(Critical << CTime(CTime::eCurrent).AsString("h:m:s.r")
                         << "Blob " << key_info->key
                         << " found when it shouldn't exist");
                ++m_CntBadReads[size_index];
                return;
            }
            if (blob_size != key_info->size) {
                ERR_POST(Critical << CTime(CTime::eCurrent).AsString("h:m:s.r")
                         << " Blob " << key_info->key << " has incorrect size "
                         << blob_size << " (expected " << key_info->size << ")");
                ++m_CntBadReads[size_index];
                return;
            }
        }
        Uint8 orig_size = blob_size;
        size_t last_size = 0;
        while (blob_size > sizeof(m_GetBuf)) {
            while (last_size != sizeof(m_GetBuf)) {
                size_t n_read = 0;
                reader->Read(m_GetBuf + last_size,
                             sizeof(m_GetBuf) - last_size, &n_read);
                blob_size -= n_read;
                last_size += n_read;
            }
            last_size = 0;
        }
        while (blob_size != 0) {
            size_t n_read = 0;
            reader->Read(m_LastChunkBuf + last_size, blob_size, &n_read);
            last_size += n_read;
            blob_size -= n_read;
        }

#ifdef NCBI_OS_LINUX
        clock_gettime(CLOCK_REALTIME, &ts);
        Uint8 end_time = Uint8(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
        Uint8 pass_time = end_time - start_time;
#endif

        CMD5 md5;
        blob_size = orig_size;
        while (blob_size > sizeof(m_GetBuf)) {
            md5.Update(m_GetBuf, sizeof(m_GetBuf));
            blob_size -= sizeof(m_GetBuf);
        }
        md5.Update(m_LastChunkBuf, last_size);
        string hash = md5.GetHexSum();
        if (key_info->size != Uint8(-1)  &&  hash != key_info->md5) {
            ERR_POST(Critical << CTime(CTime::eCurrent).AsString("h:m:s.r")
                     << " Blob " << key_info->key << " has wrong md5 hash: "
                     << hash << ", should be " << key_info->md5);
            ++m_CntBadReads[size_index];
        }
        else {
#ifdef NCBI_OS_LINUX
            m_SumReads[size_index] += pass_time;
            m_SizeReads[size_index] += key_info->size;
#endif
        }
    }
    catch (CException& ex) {
        ERR_POST(CTime(CTime::eCurrent).AsString("h:m:s.r")
                 << " Error while reading blob with key '" << key_info->key << "': " << ex);
        ++m_CntErrReads[size_index];
    }
}

void
CReplayThread::x_Remove(Uint8 key_id)
{
    TIdKeyMap::iterator it_key = m_IdKeys.find(key_id);
    if (it_key == m_IdKeys.end())
        return;
    SKeyInfo* key_info = &it_key->second;
    try {
        m_NC.Remove(key_info->key);
        Uint4 size_index = s_GetSizeIndex(key_info->size);
        ++m_CntErased[size_index];
        m_SizeErased[size_index] += key_info->size;
        key_info->md5.resize(0);
    }
    catch (CException& ex) {
        ERR_POST("Error while deleting blob with key '" << key_info->key << "': " << ex);
    }
}

void*
CReplayThread::Main(void)
{
    CTime start_ctime = GetFastLocalTime();
    Uint8 start_time = Uint8(start_ctime.GetTimeT()) * 1000 + start_ctime.MilliSecond();

    CTempString line;
    while (x_ReadNextLine(line)) {
        char cmd[10];
        Uint8 key_id = 0;
        int gen_key = 0;
        Uint8 size = 0;
        int n_scaned = sscanf(string(line).c_str(),
                              "%" NCBI_UINT8_FORMAT_SPEC
                              " %s %" NCBI_UINT8_FORMAT_SPEC
                              " %d %" NCBI_UINT8_FORMAT_SPEC,
                              &m_ReqTime, cmd, &key_id, &gen_key, &size);
        if (n_scaned != 5) {
            LOG_POST(Fatal << "Incorrect line format: " << line);
        }
        if (m_ReqTime < s_StartTime)
            continue;

        CTime cur_ctime = GetFastLocalTime();
        Uint8 cur_time = Uint8(cur_ctime.GetTimeT()) * 1000 + cur_ctime.MilliSecond();
        Uint8 check_req_time = cur_time - start_time;
        check_req_time += s_StartTime;
        if (check_req_time < m_ReqTime) {
            SleepMilliSec((unsigned long)(m_ReqTime - check_req_time));
        }

        string str_cmd = cmd;
        //LOG_POST("File " << m_InFile.GetPathname() << " executing " << line << ", cur_time=" << cur_time << ", start_time=" << start_time);
        if (str_cmd == "PUT3")
            x_PutBlob(key_id, gen_key == 1, size);
        else if (str_cmd == "GET2")
            x_GetBlob(key_id);
        else if (str_cmd == "RMV2")
            x_Remove(key_id);

        if (s_NeedConfReload) {
            CFastMutexGuard guard(s_ReloadMutex);

            CNcbiApplication::Instance()->ReloadConfig();
            s_NeedConfReload = false;
        }
    }
    s_ThreadsFinished.Add(1);
    return NULL;
}


#ifdef NCBI_OS_LINUX
static void
s_ProcessHUP(int)
{
    s_NeedConfReload = true;
}
#endif


void
CLogsReplayApp::Init(void)
{
#ifdef NCBI_OS_LINUX
    signal(SIGHUP, s_ProcessHUP);
#endif

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Split NetCache logs for testing");

    arg_desc->AddFlag("self_gen",
                      "Generate all keys, forces NC randomly to process"
                      " commands locally and to proxy commands to other"
                      " instances");
    arg_desc->AddPositional("in_file_prefix",
                            "Prefix for input files with test commands",
                            CArgDescriptions::eString);
    arg_desc->AddPositional("n_files",
                            "Number of input files to create",
                            CArgDescriptions::eInteger);
    arg_desc->AddPositional("service",
                            "Service name to connect to NetCache",
                            CArgDescriptions::eString);
    arg_desc->AddDefaultPositional("start_time",
                                   "Time to start processing from (milliseconds)",
                                   CArgDescriptions::eInt8,
                                   "0");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int
CLogsReplayApp::Run(void)
{
    SetDiagPostLevel(eDiag_Warning);

    const CArgs& args = GetArgs();
    s_BlobId.Set(0);
    s_SelfGen = args["self_gen"].HasValue();
    s_InFile = args["in_file_prefix"].AsString();
    int n_files = args["n_files"].AsInteger();
    s_NCService = args["service"].AsString();
    s_StartTime = Uint8(args["start_time"].AsInt8());

    s_ThreadsFinished.Set(0);
    s_Threads = new vector< CRef<CReplayThread> >(n_files);
    for (int i = 0; i < n_files; ++i) {
        CRef<CReplayThread> thr(new CReplayThread(s_InFile, i));
        s_Threads->at(i) = thr;
        thr->Run();
    }
    CStopWatch watch(CStopWatch::eStart);
    while (int(s_ThreadsFinished.Get()) != n_files) {
        SleepMilliSec(20000, eInterruptOnSignal);
        s_PrintStats(int(watch.Elapsed()));
    }
    for (int i = 0; i < n_files; ++i) {
        s_Threads->at(i)->Join();
    }

    LOG_POST("All testing is finished. Final statistics.");
    s_PrintStats(int(watch.Elapsed()));

    return 0;
}


int main(int argc, const char* argv[])
{
    return CLogsReplayApp().AppMain(argc, argv, 0, eDS_Default);
}
