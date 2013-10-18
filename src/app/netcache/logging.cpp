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
 * Author: Pavel Ivanov
 *
 */

#include "task_server_pch.hpp"

// ncbimtx.hpp is needed for proper compilation of ncbifile.hpp
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbireg.hpp>

#include "logging.hpp"
#include "threads_man.hpp"
#include "memory_man.hpp"
#include "server_core.hpp"

#include <fcntl.h>


BEGIN_NCBI_SCOPE;

extern CSrvTime s_JiffyTime;

struct SLogData
{
    char* buf;
    char* end_ptr;
    char* cur_ptr;
    char* cur_msg_ptr;
    Uint8 post_num;
    string prefix;
    string tmp_str;
    bool has_params;
    CSrvDiagMsg::ESeverity severity;

    const char* msg_file;
    const char* msg_func;
    int msg_line;
    int err_code;
    int err_subcode;
    int last_flush_time;

    SLogData(void)
        : buf(0), end_ptr(0), cur_ptr(0), cur_msg_ptr(0), post_num(0),
          has_params(false), severity(CSrvDiagMsg::Trace), msg_file(0), msg_func(0),
          msg_line(0), err_code(0), err_subcode(0), last_flush_time(0)
    {
    }
};


class CLogWriter : public CSrvTask
{
public:
    CLogWriter(void);
    virtual ~CLogWriter(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


static CSrvDiagMsg::ESeverity s_VisibleSev = CSrvDiagMsg::Warning;
static bool s_LogRequests = true;
static string s_UnkClient = "UNK_CLIENT";
static string s_UnkSession = "UNK_SESSION";
static string s_SevNames[] = {"Trace", "Info", "Warning", "Error", "Critical", "Fatal"};
static const size_t kOneRecReserve = 500;
static const size_t kInitLogBufSize = 10000000;
static size_t s_LogBufSize = kInitLogBufSize;
static int s_MaxFlushPeriod = 60;
static int s_FileReopenPeriod = 60;
static string s_CmdLine;
static string s_Pid;
static string s_AppUID;
static string s_FileName;
static Uint8 s_ProcessPostNum = 0;
static SLogData* s_MainData = NULL;
static bool s_InApplog = false;
static CMiniMutex s_WriteQueueLock;
static list<CTempString> s_WriteQueue;
static CLogWriter* s_LogWriter = NULL;
static int s_LogFd = -1;
static int s_LastReopenTime = 0;
static bool s_NeedFatalHalt = false;
static bool s_FileNameInitialized = false;
static bool s_ThreadsStarted = false;
static bool s_DiskSpaceAlert = false;
static CFutex s_CntHaltedThreads;
static CFutex s_Halt;


extern string s_AppBaseName;
extern SSrvThread** s_Threads;



void Logging_DiskSpaceAlert(void)
{
    s_DiskSpaceAlert = true;
}

void
SaveAppCmdLine(const string& cmd_line)
{
    s_CmdLine = cmd_line;
}

void
SetLogFileName(CTempString name)
{
    s_FileName = name;
}

static void
s_InitConstants(void)
{
    Int8 pid = CProcess::GetCurrentPid();
    time_t t = time(0);
    const string& host = CTaskServer::GetHostName();
    Int8 h = 212;
    ITERATE(string, s, host) {
        h = h*1265 + *s;
    }
    h &= 0xFFFF;
    // The low 4 bits are reserved as GUID generator version number.
    Int8 uid = (h << 48) |
               ((pid & 0xFFFF) << 32) |
               ((Int8(t) & 0xFFFFFFF) << 4) |
               1; // version #1 - fixed type conversion bug

    s_AppUID = NStr::UInt8ToString(Uint8(uid), 0, 16);
    if (s_AppUID.size() != 16)
        s_AppUID.insert(s_AppUID.begin(), 16 - s_AppUID.size(), '0');
    s_Pid = NStr::UInt8ToString(Uint8(pid));
    if (s_Pid.size() < 5)
        s_Pid.insert(s_Pid.begin(), 5 - s_Pid.size(), '0');
}

static inline void
s_OpenLogFile(void)
{
#ifdef NCBI_OS_LINUX
    if (s_DiskSpaceAlert) {
        s_DiskSpaceAlert = false;
        unlink(s_FileName.c_str());
    }
    s_LogFd = open(s_FileName.c_str(), O_WRONLY | O_APPEND | O_CREAT,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
#endif
}

static void
s_WriteLog(const char* buf, size_t size)
{
    if (s_LogFd == -1) {
        s_OpenLogFile();
        if (s_LogFd == -1)
            return;
        s_LastReopenTime = CSrvTime::CurSecs();
    }

#ifdef NCBI_OS_LINUX
    if (write(s_LogFd, buf, size) != ssize_t(size)) {
        // There's nothing we can do about it here
    }
#endif
}

static void
s_QueueLogWrite(char* buf, size_t size)
{
    s_WriteQueueLock.Lock();
    bool need_signal = s_WriteQueue.empty();
    s_WriteQueue.push_back(CTempString(buf, size));
    s_WriteQueueLock.Unlock();

    if (need_signal)
        s_LogWriter->SetRunnable();
}

static inline void
s_AllocNewBuf(SLogData* data, size_t buf_size)
{
    data->buf = (char*)malloc(buf_size);
    data->cur_ptr = data->cur_msg_ptr = data->buf;
    data->end_ptr = data->buf + buf_size - 8;
}

static void
s_RotateLogBuf(SLogData* data)
{
    char* old_buf = data->buf;
    char* old_msg_ptr = data->cur_msg_ptr;
    size_t old_msg_size = data->cur_ptr - data->cur_msg_ptr;
    s_AllocNewBuf(data, s_LogBufSize);
    if (old_msg_size != 0) {
        memcpy(data->buf, old_msg_ptr, old_msg_size);
        data->cur_ptr += old_msg_size;
    }
    s_QueueLogWrite(old_buf, old_msg_ptr - old_buf);
    data->last_flush_time = CSrvTime::CurSecs();
}

static inline void
s_InitLogPrefix(SLogData* data, TSrvThreadNum thr_num)
{
    data->prefix.assign(s_Pid);
    data->prefix.append(1, '/');
    NStr::UInt8ToString(data->tmp_str, thr_num);
    if (data->tmp_str.size() < 3)
        data->prefix.append(3 - data->tmp_str.size(), '0');
    data->prefix.append(data->tmp_str.data(), data->tmp_str.size());
    data->prefix.append(1, '/');
}

static inline void
s_CheckBufSize(SLogData* data, size_t need_size)
{
    if (data->cur_ptr + need_size >= data->end_ptr)
        s_RotateLogBuf(data);
}

static inline void
s_AddToLog(SLogData* data, const char* str, size_t size)
{
    s_CheckBufSize(data, size);
    memcpy(data->cur_ptr, str, size);
    data->cur_ptr += size;
}

static inline void
s_AddToLogStripLF(SLogData* data, const char* str, size_t size)
{
    s_CheckBufSize(data, size);
    for (; size != 0; --size, ++str) {
        char c = *str;
        *data->cur_ptr++ = c == '\n'? ';': c;
    }
}

static inline void
s_AddToLog(SLogData* data, const char* str)
{
    s_AddToLog(data, str, strlen(str));
}

static inline void
s_AddToLogStripLF(SLogData* data, const char* str)
{
    s_AddToLogStripLF(data, str, strlen(str));
}

static inline void
s_AddToLog(SLogData* data, const string& str)
{
    s_AddToLog(data, str.data(), str.size());
}

static inline void
s_AddToLogStripLF(SLogData* data, const string& str)
{
    s_AddToLogStripLF(data, str.data(), str.size());
}

static inline void
s_AddToLog(SLogData* data, const string& str, Uint1 min_chars)
{
    s_CheckBufSize(data, max(str.size(), size_t(min_chars)));
    s_AddToLog(data, str.data(), str.size());
    for (Uint1 i = min_chars; i > str.size(); --i)
        *data->cur_ptr++ = ' ';
}

static inline void
s_AddToLog(SLogData* data, Uint8 num)
{
    NStr::UInt8ToString(data->tmp_str, num);
    s_AddToLog(data, data->tmp_str);
}

static inline void
s_AddToLog(SLogData* data, Int8 num)
{
    NStr::Int8ToString(data->tmp_str, num);
    s_AddToLog(data, data->tmp_str);
}

static inline void
s_AddToLog(SLogData* data, int num)
{
    s_AddToLog(data, Uint8(num));
}

static inline void
s_AddToLog(SLogData* data, Uint8 num, Uint1 min_digs)
{
    NStr::UInt8ToString(data->tmp_str, num);
    s_CheckBufSize(data, max(data->tmp_str.size(), size_t(min_digs)));
    for (Uint1 i = min_digs; i > data->tmp_str.size(); --i)
        *data->cur_ptr++ = '0';
    s_AddToLog(data, data->tmp_str);
}

static inline void
s_AddToLog(SLogData* data, double num)
{
    NStr::DoubleToString(data->tmp_str, num);
    s_AddToLog(data, data->tmp_str);
}

static void
s_AddLogPrefix(SSrvThread* thr, SLogData* data, CRequestContext* diag_ctx = NULL)
{
    s_AddToLog(data, data->prefix);
    if (!diag_ctx  &&  thr  &&  thr->cur_task)
        diag_ctx = thr->cur_task->m_DiagCtx;

    Uint8 req_id = diag_ctx? diag_ctx->GetRequestID()
                           : CRequestContext::GetNextRequestID();
    s_AddToLog(data, req_id, 4);
    s_AddToLog(data, "/A  ");

    s_AddToLog(data, s_AppUID);
    *data->cur_ptr++ = ' ';

    Uint8 proc_post_num = AtomicAdd(s_ProcessPostNum, 1);
    s_AddToLog(data, proc_post_num, 4);
    *data->cur_ptr++ = '/';
    s_AddToLog(data, ++data->post_num, 4);
    *data->cur_ptr++ = ' ';

    CSrvTime cur_time = CSrvTime::Current();
    s_CheckBufSize(data, 50);
    Uint1 len = cur_time.Print(data->cur_ptr, CSrvTime::eFmtLogging);
    data->cur_ptr += len;
    *data->cur_ptr++ = ' ';

    s_AddToLog(data, CTaskServer::GetHostName(), 15);
    *data->cur_ptr++ = ' ';

    const string* str = diag_ctx? &diag_ctx->GetClientIP(): &s_UnkClient;
    if (str->empty())
        str = &s_UnkClient;
    s_AddToLog(data, *str, 15);
    *data->cur_ptr++ = ' ';

    str = diag_ctx? &diag_ctx->GetSessionID(): &s_UnkSession;
    if (str->empty())
        str = &s_UnkSession;
    s_AddToLog(data, *str, 24);
    *data->cur_ptr++ = ' ';

    s_AddToLog(data, s_AppBaseName);
    *data->cur_ptr++ = ' ';
}

static void
s_AddParamName(SLogData* data, CTempString name)
{
    s_CheckBufSize(data, name.size() + 2);
    if (data->has_params)
        *data->cur_ptr++ = '&';
    else
        data->has_params = true;
    s_AddToLog(data, name.data(), name.size());
    *data->cur_ptr++ = '=';
}

static SLogData*
s_AllocNewData(TSrvThreadNum thr_num)
{
    SLogData* data = new SLogData;
    data->post_num = 0;
    data->last_flush_time = 0;
    s_AllocNewBuf(data, s_LogBufSize);
    s_InitLogPrefix(data, thr_num);
    return data;
}

static void
s_AllocMainData(void)
{
    s_InitConstants();
    s_MainData = s_AllocNewData(0);
    s_AddLogPrefix(NULL, s_MainData);
    s_AddToLog(s_MainData, "start         ");
    s_AddToLog(s_MainData, s_CmdLine);
    *s_MainData->cur_ptr++ = '\n';
    s_MainData->cur_msg_ptr = s_MainData->cur_ptr;
}

static void
s_InitFileName(void)
{
    if (s_FileName.empty()) {
        s_FileName = CDirEntry::MakePath("/log/srv", s_AppBaseName, "log");
        s_OpenLogFile();
        if (s_LogFd == -1)
            s_FileName = CDirEntry::MakePath(kEmptyStr, s_AppBaseName, "log");
        else
            s_InApplog = true;
    }
    s_FileNameInitialized = true;
}

NCBI_NORETURN static void
s_DoFatalAbort(SLogData* data)
{
    s_NeedFatalHalt = true;
#if 1
    int cnt_halted = s_CntHaltedThreads.AddValue(1);
    int cnt_need = GetCntRunningThreads() + 2;
    if (!s_ThreadsStarted)
        cnt_need = 1;
    for (int attempt=0; attempt<500 && cnt_halted != cnt_need; ++attempt) {
        s_CntHaltedThreads.WaitValueChange(cnt_halted, s_JiffyTime);
        cnt_halted = s_CntHaltedThreads.GetValue();
        // In a very rare situation CntRunningThreads can change here
        cnt_need = GetCntRunningThreads() + 2;
    }
#endif
    if (!s_FileNameInitialized)
        s_InitFileName();
    ITERATE(list<CTempString>, it, s_WriteQueue) {
        CTempString str = *it;
        s_WriteLog(str.data(), str.size());
    }
    if (data->cur_ptr != data->buf)
        s_WriteLog(data->buf, data->cur_ptr - data->buf);

#ifdef NCBI_OS_LINUX
    close(s_LogFd);
#endif
    abort();
}

static inline void
s_CheckFatalAbort(void)
{
    if (!s_NeedFatalHalt)
        return;

    SLogData* data = GetCurThread()->log_data;
    if (data->cur_ptr != data->buf)
        s_QueueLogWrite(data->buf, data->cur_ptr - data->buf);
    s_CntHaltedThreads.AddValue(1);
    s_CntHaltedThreads.WakeUpWaiters(1);
    s_Halt.WaitValueChange(0);
}

void
LogNoteThreadsStarted(void)
{
    s_ThreadsStarted = true;
    s_RotateLogBuf(s_MainData);
}

void
ConfigureLogging(CNcbiRegistry* reg, CTempString section)
{
    s_LogRequests = reg->GetBool(section, "log_requests", true);
    s_LogBufSize = NStr::StringToUInt8_DataSize(
                    reg->GetString(section, "log_thread_buf_size", "10 MB"));
    s_MaxFlushPeriod = reg->GetInt(section, "log_flush_period", 60);
    s_FileReopenPeriod = reg->GetInt(section, "log_reopen_period", 60);
}

void
InitLogging(void)
{
    if (!s_MainData)
        s_AllocMainData();

    s_InitFileName();
    s_LogWriter = new CLogWriter();
}

void
FinalizeLogging(void)
{
    if (s_MainData->cur_ptr != s_MainData->buf) {
        s_WriteLog(s_MainData->buf, s_MainData->cur_ptr - s_MainData->buf);
        s_MainData->cur_ptr = s_MainData->buf;
    }

    s_AddLogPrefix(NULL, s_MainData);
    s_AddToLog(s_MainData, "stop          0 ");
    CSrvTime cur_time = CSrvTime::Current();
    cur_time -= CTaskServer::GetStartTime();
    s_AddToLog(s_MainData, cur_time.Sec());
    *s_MainData->cur_ptr++ = '.';
    s_AddToLog(s_MainData, cur_time.NSec(), 9);
    *s_MainData->cur_ptr++ = '\n';
    s_WriteLog(s_MainData->buf, s_MainData->cur_ptr - s_MainData->buf);

#ifdef NCBI_OS_LINUX
    close(s_LogFd);
#endif
}

void
AssignThreadLogging(SSrvThread* thr)
{
    if (thr->thread_num == 0) {
        if (!s_MainData)
            s_AllocMainData();
        thr->log_data = s_MainData;
    }
    else {
        thr->log_data = s_AllocNewData(thr->thread_num);
    }
}

void
CheckLoggingFlush(SSrvThread* thr)
{
    s_CheckFatalAbort();

    SLogData* data = thr->log_data;
    int cur_time = CSrvTime::CurSecs();
    if (cur_time - data->last_flush_time < s_MaxFlushPeriod)
        return;

    if (data->cur_ptr == data->buf)
        data->last_flush_time = cur_time;
    else
        s_RotateLogBuf(data);
}

void
StartThreadLogging(SSrvThread* thr)
{
    thr->log_data = s_AllocNewData(thr->thread_num);
}

void
StopThreadLogging(SSrvThread* thr)
{
    SLogData* data = thr->log_data;
    if (data->cur_ptr != data->buf)
        s_QueueLogWrite(data->buf, data->cur_ptr - data->buf);
    else
        free(data->buf);
    delete data;
    s_CheckFatalAbort();
}

void
ReleaseThreadLogging(SSrvThread* thr)
{
    SLogData* data = thr->log_data;
    if (data->cur_ptr != data->buf) {
        s_WriteLog(data->buf, data->cur_ptr - data->buf);
        data->cur_ptr = data->buf;
    }
}

static const char*
find_match(char lsep, char rsep, const char* start, const char* stop)
{
    if (*(stop - 1) != rsep) return stop;
    int balance = 1;
    const char* pos = stop - 2;
    for (; pos > start; pos--) {
        if (*pos == rsep) {
            balance++;
        }
        else if (*pos == lsep) {
            if (--balance == 0) break;
        }
    }
    return (pos <= start) ? NULL : pos;
}

static const char* 
str_rev_str(const char* begin_str, const char* end_str, const char* str_search)
{
    if (begin_str == NULL)
        return NULL;
    if (end_str == NULL)
        return NULL;
    if (str_search == NULL)
        return NULL;

    const char* search_char = str_search + strlen(str_search);
    const char* cur_char = end_str;

    do {
        --search_char;
        do {
            --cur_char;
        } while(*cur_char != *search_char && cur_char != begin_str);
        if (*cur_char != *search_char)
            return NULL; 
    }
    while (search_char != str_search);
    
    return cur_char;
}

static void
s_ParseFuncName(const char* func, CTempString& class_name, CTempString& func_name)
{
    if (!func  ||  *func == '\0')
        return;

    // Skip function arguments
    size_t len = strlen(func);
    const char* end_str = find_match('(', ')',
                                     func,
                                     func + len);
    if (end_str == func + len) {
        // Missing '('
        return;
    }
    if (end_str) {
        // Skip template arguments
        end_str = find_match('<', '>', func, end_str);
    }
    if (!end_str)
        return;
    // Get a function/method name
    const char* start_str = NULL;

    // Get a function start position.
    const char* start_str_tmp = str_rev_str(func, end_str, "::");
    bool has_class = start_str_tmp != NULL;
    if (start_str_tmp != NULL) {
        start_str = start_str_tmp + 2;
    } else {
        start_str_tmp = str_rev_str(func, end_str, " ");
        if (start_str_tmp != NULL) {
            start_str = start_str_tmp + 1;
        } 
    }

    const char* cur_funct_name = (start_str == NULL? func: start_str);
    size_t cur_funct_name_len = end_str - cur_funct_name;
    func_name.assign(cur_funct_name, cur_funct_name_len);

    // Get a class name
    if (has_class) {
        end_str = find_match('<', '>', func, start_str - 2);
        start_str = str_rev_str(func, end_str, " ");
        const char* cur_class_name = (start_str == NULL? func: start_str + 1);
        size_t cur_class_name_len = end_str - cur_class_name;
        class_name.assign(cur_class_name, cur_class_name_len);
    }
}

static inline bool
s_CheckOldStyleStart(const CSrvDiagMsg* msg)
{
    SLogData* data = msg->m_Data;
    if (!CSrvDiagMsg::IsSeverityVisible(data->severity))
        return false;
    if (data->cur_msg_ptr == data->cur_ptr) {
        if (msg->m_OldStyle) {
            msg->StartSrvLog(data->severity, data->msg_file,
                             data->msg_line, data->msg_func);
        }
        else
            abort();
    }
    return true;
}

static inline CSrvDiagMsg::ESeverity
s_ConvertSeverity(EOldStyleSeverity sev)
{
    switch (sev) {
    case Trace:
        return CSrvDiagMsg::Trace;
    case Info:
        return CSrvDiagMsg::Info;
    case Warning:
        return CSrvDiagMsg::Warning;
    case Error:
        return CSrvDiagMsg::Error;
    case Critical:
        return CSrvDiagMsg::Critical;
    case Fatal:
        return CSrvDiagMsg::Fatal;
    default:
        abort();
    }
    return CSrvDiagMsg::Trace;
}

static inline CSrvDiagMsg::ESeverity
s_ConvertSeverity(EDiagSev sev)
{
    switch (sev) {
    case eDiag_Trace:
        return CSrvDiagMsg::Trace;
    case eDiag_Info:
        return CSrvDiagMsg::Info;
    case eDiag_Warning:
        return CSrvDiagMsg::Warning;
    case eDiag_Error:
        return CSrvDiagMsg::Error;
    case eDiag_Critical:
        return CSrvDiagMsg::Critical;
    case eDiag_Fatal:
        return CSrvDiagMsg::Fatal;
    default:
        abort();
    }
    return CSrvDiagMsg::Trace;
}

CDiagContext&
GetDiagContext(void)
{
    static CDiagContext ctx;
    return ctx;
}

void CDiagContext::UpdateOnFork(TOnForkFlags /*flags*/)
{
    CDiagContext::UpdatePID();
}

void
CDiagContext::UpdatePID(void)
{
    string old_uid = s_AppUID;
    s_InitConstants();
    if (s_Threads) {
        for (TSrvThreadNum i = 0; i <= s_MaxRunningThreads + 1; ++i) {
            SSrvThread* thr = s_Threads[i];
            if (thr)
                s_InitLogPrefix(thr->log_data, thr->thread_num);
        }
    }
    else if (s_MainData) {
        s_InitLogPrefix(s_MainData, 0);
    }
    CSrvDiagMsg().PrintExtra(NULL)
                 .PrintParam("action", "fork")
                 .PrintParam("parent_guid", old_uid);
}

void
CDiagContext::StartCtxRequest(CRequestContext* ctx)
{
    ctx->StartRequest();
}

void
CDiagContext::StopCtxRequest(CRequestContext* ctx)
{
    ctx->StopRequest();
}

bool
CDiagContext::IsCtxRunning(CRequestContext* ctx)
{
    return ctx->IsRunning();
}

const string&
CDiagContext::GetDefaultSessionID(void)
{
    return kEmptyStr;
}

const string&
CDiagContext::GetEncodedSessionID(void)
{
    return kEmptyStr;
}


CSrvDiagMsg::CSrvDiagMsg(void)
    : m_Thr(GetCurThread()),
      m_Data(NULL),
      m_OldStyle(false)
{
    if (m_Thr) {
        m_Data = m_Thr->log_data;
        s_CheckFatalAbort();
    }
    else {
        if (!s_MainData)
            s_AllocMainData();
        m_Data = s_MainData;
    }
}

CSrvDiagMsg::~CSrvDiagMsg(void)
{
    if (m_Data->cur_msg_ptr != m_Data->cur_ptr)
        Flush();
}

bool
CSrvDiagMsg::IsSeverityVisible(ESeverity sev)
{
    return int(sev) >= int(s_VisibleSev);
}

const CSrvDiagMsg&
CSrvDiagMsg::StartSrvLog(ESeverity sev,
                         const char* file,
                         int line,
                         const char* func) const
{
    if (m_Data->cur_msg_ptr != m_Data->cur_ptr
        /* && *m_Data->cur_ptr != '\n' */) {
        *m_Data->cur_ptr++ = '\n';
    }
    m_Data->severity = sev;
    s_AddLogPrefix(m_Thr, m_Data);
    s_AddToLog(m_Data, s_SevNames[int(sev)]);
    if (m_OldStyle) {
        s_AddToLog(m_Data, ": ");
        if (m_Data->err_code != 0  ||  m_Data->err_subcode != 0) {
            s_AddToLog(m_Data, "(");
            s_AddToLog(m_Data, m_Data->err_code);
            s_AddToLog(m_Data, ".");
            s_AddToLog(m_Data, m_Data->err_subcode);
            s_AddToLog(m_Data, ")");
        }
        s_AddToLog(m_Data, " \"");
    }
    else {
        s_AddToLog(m_Data, ":  \"");
    }
    const char* file_name;
    size_t name_size;
    ExtractFileName(file, file_name, name_size);
    s_AddToLog(m_Data, file_name, name_size);
    s_AddToLog(m_Data, "\", line ");
    s_AddToLog(m_Data, line);
    s_AddToLog(m_Data, ": ");
    CTempString class_name, func_name;
    s_ParseFuncName(func, class_name, func_name);
    s_AddToLog(m_Data, class_name);
    s_AddToLog(m_Data, "::");
    s_AddToLog(m_Data, func_name);
    s_AddToLog(m_Data, "() --- ");

    return *this;
}

const CSrvDiagMsg&
CSrvDiagMsg::StartInfo(void) const
{
    CRequestContext* ctx = NULL;
    if (m_Thr->cur_task)
        ctx = m_Thr->cur_task->m_DiagCtx;
    return StartInfo(ctx);
}

const CSrvDiagMsg&
CSrvDiagMsg::StartInfo(CRequestContext* ctx) const
{
    m_Data->severity = Warning;
    s_AddLogPrefix(m_Thr, m_Data, ctx);
    s_AddToLog(m_Data, "Message[W]:  \"UNK_FILE\", line 0: UNK_FUNC --- ");
    return *this;
}

const CSrvDiagMsg&
CSrvDiagMsg::StartOldStyle(const char* file, int line, const char* func)
{
    m_OldStyle = true;
    m_Data->severity = CSrvDiagMsg::Error;
    m_Data->msg_file = file;
    m_Data->msg_func = func;
    m_Data->msg_line = line;
    m_Data->err_code = m_Data->err_subcode = 0;
    return *this;
}

const CSrvDiagMsg&
operator<< (const CSrvDiagMsg& msg, EOldStyleSeverity sev)
{
    msg.m_Data->severity = s_ConvertSeverity(sev);
    return msg;
}

const CSrvDiagMsg&
operator<< (const CSrvDiagMsg& msg, ErrCode err_code)
{
    msg.m_Data->err_code = err_code.m_Code;
    msg.m_Data->err_subcode = err_code.m_SubCode;
    return msg;
}

CSrvDiagMsg&
CSrvDiagMsg::StartRequest(void)
{
    return StartRequest(m_Thr->cur_task->m_DiagCtx);
}

CSrvDiagMsg&
CSrvDiagMsg::StartRequest(CRequestContext* ctx)
{
    if (CDiagContext::IsCtxRunning(ctx))
        abort();
    CDiagContext::StartCtxRequest(ctx);
    ctx->SetRequestStatus(200);

    if (!s_LogRequests)
        return *this;

    s_AddLogPrefix(m_Thr, m_Data, ctx);
    s_AddToLog(m_Data, "request-start ");
    m_Data->has_params = false;

    return *this;
}

CSrvDiagMsg&
CSrvDiagMsg::PrintExtra(void)
{
    return PrintExtra(m_Thr->cur_task->m_DiagCtx);
}

CSrvDiagMsg&
CSrvDiagMsg::PrintExtra(CRequestContext* ctx)
{
    if (s_LogRequests) {
        s_AddLogPrefix(m_Thr, m_Data, ctx);
        s_AddToLog(m_Data, "extra         ");
        m_Data->has_params = false;
    }
    return *this;
}

CSrvDiagMsg&
CSrvDiagMsg::PrintParam(CTempString name, CTempString value)
{
    if (!s_LogRequests)
        return *this;

    s_AddParamName(m_Data, name);
    if (NStr::NeedsURLEncoding(value))
        s_AddToLog(m_Data, NStr::URLEncode(value));
    else
        s_AddToLog(m_Data, value);
    return *this;
}

CSrvDiagMsg&
CSrvDiagMsg::PrintParam(CTempString name, Int8 value)
{
    if (s_LogRequests) {
        s_AddParamName(m_Data, name);
        s_AddToLog(m_Data, value);
    }
    return *this;
}

CSrvDiagMsg&
CSrvDiagMsg::PrintParam(CTempString name, Uint8 value)
{
    if (s_LogRequests) {
        s_AddParamName(m_Data, name);
        s_AddToLog(m_Data, value);
    }
    return *this;
}

CSrvDiagMsg&
CSrvDiagMsg::PrintParam(CTempString name, double value)
{
    if (s_LogRequests) {
        s_AddParamName(m_Data, name);
        s_AddToLog(m_Data, value);
    }
    return *this;
}

void
CSrvDiagMsg::StopRequest(void)
{
    StopRequest(m_Thr->cur_task->m_DiagCtx);
}

void
CSrvDiagMsg::StopRequest(CRequestContext* ctx)
{
    if (!CDiagContext::IsCtxRunning(ctx))
        abort();

    if (s_LogRequests) {
        s_AddLogPrefix(m_Thr, m_Data, ctx);
        s_AddToLog(m_Data, "request-stop  ");
        s_AddToLog(m_Data, ctx->GetRequestStatus());
        *m_Data->cur_ptr++ = ' ';
        CTimeSpan span(ctx->GetRequestTimer().Elapsed());
        s_AddToLog(m_Data, span.GetCompleteSeconds());
        *m_Data->cur_ptr++ = '.';
        s_AddToLog(m_Data, span.GetNanoSecondsAfterSecond(), 9);
        *m_Data->cur_ptr++ = ' ';
        s_AddToLog(m_Data, ctx->GetBytesRd());
        *m_Data->cur_ptr++ = ' ';
        s_AddToLog(m_Data, ctx->GetBytesWr());
        Flush();
    }

    CDiagContext::StopCtxRequest(ctx);
}

void
CSrvDiagMsg::Flush(void)
{
    s_CheckBufSize(m_Data, 1);
    *m_Data->cur_ptr++ = '\n';
    m_Data->cur_msg_ptr = m_Data->cur_ptr;
    if (m_Data->severity == Fatal)
        s_DoFatalAbort(m_Data);
    s_CheckBufSize(m_Data, kOneRecReserve);
}

const CSrvDiagMsg&
CSrvDiagMsg::operator<< (CTempString str) const
{
    if (s_CheckOldStyleStart(this)) {
        if (s_InApplog)
            s_AddToLogStripLF(m_Data, str);
        else
            s_AddToLog(m_Data, str);
    }
    return *this;
}

const CSrvDiagMsg&
CSrvDiagMsg::operator<< (Int8 num) const
{
    if (s_CheckOldStyleStart(this))
        s_AddToLog(m_Data, num);
    return *this;
}

const CSrvDiagMsg&
CSrvDiagMsg::operator<< (Uint8 num) const
{
    if (s_CheckOldStyleStart(this))
        s_AddToLog(m_Data, num);
    return *this;
}

const CSrvDiagMsg&
CSrvDiagMsg::operator<< (double num) const
{
    if (s_CheckOldStyleStart(this))
        s_AddToLog(m_Data, num);
    return *this;
}

const CSrvDiagMsg&
CSrvDiagMsg::operator<< (const void* ptr) const
{
    if (s_CheckOldStyleStart(this)) {
        NStr::PtrToString(m_Data->tmp_str, ptr);
        s_AddToLog(m_Data, m_Data->tmp_str);
    }
    return *this;
}

const CSrvDiagMsg&
CSrvDiagMsg::operator<< (const exception& ex) const
{
    if (s_CheckOldStyleStart(this)) {
        if (s_InApplog)
            s_AddToLogStripLF(m_Data, ex.what());
        else
            s_AddToLog(m_Data, ex.what());
    }
    return *this;
}


void
CSrvTask::SetDiagCtx(CRequestContext* ctx)
{
    ctx->AddReference();
    if (!m_DiagCtx) {
        m_DiagCtx = ctx;
        if (m_DiagChain)
            m_DiagChain[0] = ctx;
        return;
    }
    if (!m_DiagChain) {
        m_DiagChain = (CRequestContext**)malloc(sizeof(m_DiagCtx) * 2);
        memset(m_DiagChain, 0, GetMemSize(m_DiagChain));
        m_DiagChain[0] = m_DiagCtx;
        m_DiagChain[1] = m_DiagCtx = ctx;
        return;
    }

    Uint4 mem_cap = Uint4(GetMemSize(m_DiagChain) / sizeof(m_DiagCtx));
    Uint4 cnt_in_chain = mem_cap;
    while (!m_DiagChain[cnt_in_chain - 1]) {
        if (--cnt_in_chain == 0)
            abort();
    }
    if (cnt_in_chain == mem_cap) {
        m_DiagChain = (CRequestContext**)
                      realloc(m_DiagChain, mem_cap * 2 * sizeof(m_DiagCtx));
        memset(&m_DiagChain[mem_cap], 0,
               GetMemSize(m_DiagChain) - mem_cap * sizeof(m_DiagCtx));
    }
    m_DiagChain[cnt_in_chain] = m_DiagCtx = ctx;
}

void
CSrvTask::CreateNewDiagCtx(void)
{
    CRequestContext* ctx = new CRequestContext();
    ctx->SetRequestID();
    SetDiagCtx(ctx);
}

void
CSrvTask::ReleaseDiagCtx(void)
{
    CRequestContext* was_ctx = m_DiagCtx;
    m_DiagCtx->RemoveReference();
    m_DiagCtx = NULL;
    if (!m_DiagChain)
        return;

    Uint4 mem_cap = Uint4(GetMemSize(m_DiagChain) / sizeof(m_DiagCtx));
    Uint4 cnt_in_chain = mem_cap;
    while (!m_DiagChain[cnt_in_chain - 1]) {
        if (--cnt_in_chain == 0)
            abort();
    }
    if (m_DiagChain[--cnt_in_chain] != was_ctx)
        abort();
    m_DiagChain[cnt_in_chain] = NULL;
    if (cnt_in_chain != 0)
        m_DiagCtx = m_DiagChain[cnt_in_chain - 1];
}


CDiagCompileInfo::CDiagCompileInfo(void)
    : m_File(""),
      m_Line(0),
      m_CurrFunctName(0),
      m_Parsed(false),
      m_StrFile(0),
      m_StrCurrFunctName(0)
{}

CDiagCompileInfo::CDiagCompileInfo(const char* file,
                                   int         line,
                                   const char* curr_funct)
    : m_File(file),
      m_Line(line),
      m_CurrFunctName(curr_funct),
      m_Parsed(false),
      m_StrFile(0),
      m_StrCurrFunctName(0)
{
    if (!file) {
        m_File = "";
        return;
    }
}

CDiagCompileInfo::CDiagCompileInfo(const string& file,
                                   int           line,
                                   const string& curr_funct,
                                   const string& module)
    : m_File(""),
      m_Line(line),
      m_CurrFunctName(""),
      m_Parsed(false),
      m_StrFile(0),
      m_StrCurrFunctName(0)
{
    if ( !file.empty() ) {
        m_StrFile = new char[file.size() + 1];
        strcpy(m_StrFile, file.c_str());
        m_File = m_StrFile;
    }
    if ( !curr_funct.empty() ) {
        m_StrCurrFunctName = new char[curr_funct.size() + 1];
        strcpy(m_StrCurrFunctName, curr_funct.c_str());
        m_CurrFunctName = m_StrCurrFunctName;
    }
}

CDiagCompileInfo::~CDiagCompileInfo(void)
{
    delete[] m_StrFile;
    delete[] m_StrCurrFunctName;
}

void
CDiagCompileInfo::ParseCurrFunctName(void) const
{
    m_Parsed = true;
    CTempString class_name, func_name;
    s_ParseFuncName(m_CurrFunctName, class_name, func_name);
    m_FunctName.assign(func_name.data(), func_name.size());
    m_ClassName.assign(class_name.data(), class_name.size());
}


CNcbiOstream&
SDiagMessage::Write(CNcbiOstream& os, int flags /* = 0 */) const
{
    string sev = s_SevNames[s_ConvertSeverity(severity)];
    os << setfill(' ') << setw(13) // add 1 for space
       << setiosflags(IOS_BASE::left) << setw(0)
       << sev << ':'
       << resetiosflags(IOS_BASE::left)
       << ' ';

    // <module>-<err_code>.<err_subcode> or <module>-<err_text>
    if (err_code  ||  err_subcode || err_text) {
        if (err_text) {
            os << '(' << err_text << ')';
        } else {
            os << '(' << err_code << '.' << err_subcode << ')';
        }
    }
    os << ' ';

    // "<file>"
    bool print_file = file  &&  *file;
    if ( print_file ) {
        const char* x_file;
        size_t x_file_len;
        ExtractFileName(file, x_file, x_file_len);
        os << '"' << string(x_file, x_file_len) << '"';
    }
    else {
        os << "\"UNK_FILE\"";
    }
    // , line <line>
    os << ", line " << line;
    os << ": ";

    // Class::Function
    bool print_loc = (nclass && *nclass ) || (func && *func);
    if (print_loc) {
        // Class:: Class::Function() ::Function()
        if (nclass  &&  *nclass) {
            os << nclass;
        }
        os << "::";
        if (func  &&  *func) {
            os << func << "() ";
        }
    }
    else {
        os << "UNK_FUNC ";
    }

    os << "--- ";

    // [<prefix1>::<prefix2>::.....]
    if (prefix  &&  *prefix)
        os << '[' << prefix << "] ";

    // <message>
    if (len)
        os.write(buf, len);

    // Endl
    if ((flags & fNoEndl) == 0)
        os << NcbiEndl;

    return os;
}


CLogWriter::CLogWriter(void)
{}

CLogWriter::~CLogWriter(void)
{}

void
CLogWriter::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
// those who add data into queue, make this runnable

    s_CheckFatalAbort();
// if log file was open for too long, close it
// so that app_log could move it 
    if (CSrvTime::CurSecs() - s_LastReopenTime >= s_FileReopenPeriod  &&  s_LogFd != -1)
    {
#ifdef NCBI_OS_LINUX
        close(s_LogFd);
#endif
        s_LogFd = -1;
    }

// get from the queue, write into log
    s_WriteQueueLock.Lock();
    if (s_WriteQueue.empty()) {
        s_WriteQueueLock.Unlock();
        return;
    }
    CTempString str = s_WriteQueue.front();
    s_WriteQueue.pop_front();
    bool have_more = !s_WriteQueue.empty();
    s_WriteQueueLock.Unlock();

    s_WriteLog(str.data(), str.size());
    free((void*)str.data());

    if (have_more)
        SetRunnable();
}

END_NCBI_SCOPE;
