/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   C++->C conversion tools for basic CORE connect stuff:
 *     Registry
 *     Logging
 *     Locking
 *
 */

#include <ncbi_pch.hpp>
#include "ncbi_ansi_ext.h"
#include "ncbi_socketp.h"
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <connect/error_codes.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_monkey.hpp>
#include <connect/ncbi_util.h>
#include <common/ncbi_sanitizers.h>

#ifdef NCBI_POSIX_THREADS
#include <pthread.h>
#endif // NCBI_POSIX_THREADS

#define NCBI_USE_ERRCODE_X   Connect_Core


BEGIN_NCBI_SCOPE


NCBI_PARAM_DECL  (bool, CONN, TRACE_REG);
NCBI_PARAM_DEF_EX(bool, CONN, TRACE_REG, false, eParam_Default, CONN_TRACE_REG);
using TTraceReg = NCBI_PARAM_TYPE(CONN, TRACE_REG);
static CSafeStatic<TTraceReg> s_TraceReg;

NCBI_PARAM_DECL  (bool, CONN, TRACE_LOG);
NCBI_PARAM_DEF_EX(bool, CONN, TRACE_LOG, false, eParam_Default, CONN_TRACE_LOG);
using TTraceLog = NCBI_PARAM_TYPE (CONN, TRACE_LOG);
static CSafeStatic<TTraceLog> s_TraceLog;

NCBI_PARAM_DECL  (bool, CONN, TRACE_LOCK);
NCBI_PARAM_DEF_EX(bool, CONN, TRACE_LOCK, false, eParam_Default, CONN_TRACE_LOCK);
using TTraceLock = NCBI_PARAM_TYPE (CONN, TRACE_LOCK);
static CSafeStatic<TTraceLock> s_TraceLock;


static TCORE_Set s_CORE_Set = 0;


/***********************************************************************
 *                              Registry                               *
 ***********************************************************************/


static string x_Reg(const char* section, const char* name,
                    const char* value = 0,
                    EREG_Storage storage = eREG_Transient)
{
    string x_section;
    if (section)
        x_section = '[' + string(section) + ']';
    else
        x_section = "<NULL>";
    string x_name;
    if (name)
        x_name = '"' + string(name) + '"';
    else
        x_name = "<NULL>";
    string x_value;
    if (value)
        x_value = "=\"" + string(value) + '"';
    string x_storage;
    if (value) {
        switch (int(storage)) {
        case eREG_Transient:
            x_storage = ", <Transient>";
            break;
        case eREG_Persistent:
            x_storage = ", <Persistent>";
            break;
        default:
            x_storage = ", <#" + NStr::IntToString(int(storage)) + '>';
            break;
        }
    }
    return x_section + x_name + x_value + x_storage;
}


extern "C" {
static int s_REG_Get(void* user_data,
                      const char* section, const char* name,
                      char* value, size_t value_size) THROWS_NONE
{
    if (s_TraceReg->Get()) {
        _TRACE("s_REG_Get(" + NStr::PtrToString(user_data) + ", "
               + x_Reg(section, name) + ')');
    }
    int result = 0/*assume failure, including truncation*/;
    try {
        string item
            = static_cast<const IRegistry*> (user_data)->Get(section, name);
        if (!item.empty()) {
            size_t len = item.size();
            if (len >= value_size)
                len  = value_size - 1;
            else
                result = 1/*success*/;
            strncpy0(value, item.data(), len);

            if (s_TraceReg->Get()) {
                _TRACE("s_REG_Get(" + NStr::PtrToString(user_data) + ", "
                       + x_Reg(section, name) + ") = \"" + string(value)
                       + (result ? "\"" : "\" <Truncated>"));
            }
        } else
            result = -1/*success,unmodified*/;
    }
    NCBI_CATCH_ALL_X(1, "s_REG_Get(" + NStr::PtrToString(user_data) + ", "
                     + x_Reg(section, name) + ") failed");
    return result;
}
}


extern "C" {
static int s_REG_Set(void* user_data,
                     const char* section, const char* name,
                     const char* value, EREG_Storage storage) THROWS_NONE
{
    if (s_TraceReg->Get()) {
        _TRACE("s_REG_" + string(value ? "Set" : "Unset") + '('
               + NStr::PtrToString(user_data) + ", "
               + x_Reg(section, name, value ? value : "", storage) + ')');
    }
    int result = 0/*assume failure*/;
    try {
        IRWRegistry* reg = static_cast<IRWRegistry*> (user_data);
        result = value
            ? reg->Set  (section, name, value,
                         (storage == eREG_Persistent
                          ? CNcbiRegistry::fPersistent
                          | CNcbiRegistry::fTruncate
                          : CNcbiRegistry::fTruncate))
            : reg->Unset(section, name,
                         (storage == eREG_Persistent
                          ? CNcbiRegistry::fPersistent
                          : CNcbiRegistry::fTransient));
    }
    NCBI_CATCH_ALL_X(2, "s_REG_" + string(value ? "Set" : "Unset") + '('
                     + NStr::PtrToString(user_data) + ", "
                     + x_Reg(section, name, value ? value : "", storage)
                     + ") failed");
    return result;
}
}


extern "C" {
static void s_REG_Cleanup(void* user_data) THROWS_NONE
{

    if (s_TraceReg->Get())
        _TRACE("s_REG_Cleanup(" + NStr::PtrToString(user_data) + ')');
    try {
        static_cast<const IRegistry*> (user_data)->RemoveReference();
    }
    NCBI_CATCH_ALL_X(3, "s_REG_Cleanup("
                     + NStr::PtrToString(user_data) + ") failed");
}
}


extern REG REG_cxx2c(IRWRegistry* reg, bool pass_ownership)
{
    if (s_TraceReg->Get())
        _TRACE("REG_cxx2c(" + NStr::PtrToString(reg) + ')');
    if (!reg)
        return 0;
    if (pass_ownership)
        reg->AddReference();
    return REG_Create(reg,
                      s_REG_Get, s_REG_Set,
                      pass_ownership ? s_REG_Cleanup : 0, 0);
}


extern REG REG_cxx2c(const IRWRegistry* reg, bool pass_ownership)
{
    if (s_TraceReg->Get())
        _TRACE("REG_cxx2c(const " + NStr::PtrToString(reg) + ')');
    if (!reg)
        return 0;
    if (pass_ownership)
        reg->AddReference();
    return REG_Create(const_cast<IRWRegistry*> (reg),
                      s_REG_Get, 0/*no setter*/,
                      pass_ownership ? s_REG_Cleanup : 0, 0);
}


/***********************************************************************
 *                                Logger                               *
 ***********************************************************************/


static string x_Log(ELOG_Level level)
{
    string x_level;
    switch (int(level)) {
        case eLOG_Trace:
            x_level = "Trace";
            break;
        case eLOG_Note:
            x_level = "Note";
            break;
        case eLOG_Warning:
            x_level = "Warning";
            break;
        case eLOG_Error:
            x_level = "Error";
            break;
        case eLOG_Critical:
            x_level = "Critical";
            break;
        case eLOG_Fatal:
            x_level = "Fatal";
            break;
        default:
            x_level = '#' + NStr::IntToString(int(level));
            break;
    }
    return x_level;
}


extern "C" {
static void s_LOG_Handler(void*             /*data*/,
                          const SLOG_Message* mess) THROWS_NONE
{
    static const char kOutOfMemory[] = "Ouch! Out of memory";

    if (s_TraceLog->Get())
        _TRACE("s_LOG_Handler(" + x_Log(mess->level) + ')');
    try {
        EDiagSev level;
        switch (int(mess->level)) {
        case eLOG_Trace:
            level = eDiag_Trace;
            break;
        case eLOG_Note:
            level = eDiag_Info;
            break;
        case eLOG_Warning:
            level = eDiag_Warning;
            break;
        case eLOG_Error:
            level = eDiag_Error;
            break;
        case eLOG_Critical:
            level = eDiag_Critical;
            break;
        case eLOG_Fatal:
            /*FALLTHRU*/
        default:
            level = eDiag_Fatal;
            break;
        }
        if (level != eDiag_Fatal  &&  !IsVisibleDiagPostLevel(level))
            return;

        CDiagCompileInfo info(mess->file,
                              mess->line,
                              mess->func,
                              mess->module);
        CNcbiDiag diag(info, level);
        diag.SetErrorCode(mess->err_code, mess->err_subcode);
        size_t message_len;
        if (mess->message  &&  *mess->message) {
            message_len = strlen(mess->message);
            do {
                if (!isspace((unsigned char) mess->message[message_len - 1]))
                    break;
            } while (--message_len > 0);
        } else
            message_len = mess->message ? 0 : sizeof(kOutOfMemory) - 1;
        if (message_len)
            diag << string(mess->message ? mess->message : kOutOfMemory, message_len);
        if (mess->raw_size) {
            typedef AutoPtr< char, ArrayDeleter<char> >  TTempCharPtr;
            TTempCharPtr buf;
            CTempString data;
            if (mess->raw_data) {
                buf.reset(new char[UTIL_PrintableStringSize
                                   ((const char*)
                                    mess->raw_data,
                                    mess->raw_size)]);
                char* end = UTIL_PrintableStringEx((const char*)
                                                   mess->raw_data,
                                                   mess->raw_size,
                                                   buf.get(), 0/*flags*/,
                                                   UTIL_PRINTABLE_WIDTH);
                data.assign(buf.get(), (size_t)(end - buf.get()));
            }
            diag << "\n#################### [BEGIN] Raw Data ("
                 << mess->raw_size
                 << " byte" << &"s"[!(mess->raw_size != 1)] << "):"
                 << CTempString(data.empty() ? " <NULL>" : "\n") << data
                 << "\n#################### [_END_] Raw Data";
        }
        diag << Endm;
        if (level == eDiag_Fatal)
            Abort();
    }
    NCBI_CATCH_ALL_X(4, "s_LOG_Handler(" + x_Log(mess->level) + ") failed");
}
}


extern LOG LOG_cxx2c(void)
{
    if (s_TraceLog->Get())
        _TRACE("LOG_cxx2c()");
    return LOG_Create(0, s_LOG_Handler, 0, 0);
}


/***********************************************************************
 *                               MT-Lock                               *
 ***********************************************************************/


static string x_Lock(EMT_Lock how)
{
    string x_how;
    switch (int(how)) {
    case eMT_Lock:
        x_how = "Lock";
        break;
    case eMT_LockRead:
        x_how = "ReadLock";
        break;
    case eMT_Unlock:
        x_how = "Unlock";
        break;
    case eMT_TryLock:
        x_how = "TryLock";
        break;
    case eMT_TryLockRead:
        x_how = "TryLockRead";
        break;
    default:
        x_how = '#' + NStr::IntToString(int(how));
        break;
    }
    return x_how;
}


extern "C" {
static int/*bool*/ s_LOCK_Handler(void* user_data, EMT_Lock how) THROWS_NONE
{
    if (s_TraceLock->Get()) {
        _TRACE("s_LOCK_Handler(" + NStr::PtrToString(user_data) + ", "
               + x_Lock(how) + ')');
    }
    try {
        CRWLock* lock = static_cast<CRWLock*> (user_data);
        switch (int(how)) {
        case eMT_Lock:
            lock->WriteLock();
            return 1/*success*/;
        case eMT_LockRead:
            lock->ReadLock();
            return 1/*success*/;
        case eMT_Unlock:
            lock->Unlock();
            return 1/*success*/;
        case eMT_TryLock:
            if (lock->TryWriteLock())
                return 1/*success*/;
            break;
        case eMT_TryLockRead:
            if (lock->TryReadLock())
                return 1/*success*/;
            break;
        default:
            NCBI_THROW(CCoreException, eCore, "Lock used with unknown op #"
                       + NStr::IntToString(int(how)));
        }
    }
    NCBI_CATCH_ALL_X(5, "s_LOCK_Handler(" + NStr::PtrToString(user_data) + ", "
                     + x_Lock(how) + ") failed");
    return 0/*failure*/;
}
}


extern "C" {
static void s_LOCK_Cleanup(void* user_data) THROWS_NONE
{
    if (s_TraceLock->Get())
        _TRACE("s_LOCK_Cleanup(" + NStr::PtrToString(user_data) + ')');
    try {
        delete static_cast<CRWLock*> (user_data);
    }
    NCBI_CATCH_ALL_X(6, "s_LOCK_Cleanup("
                     + NStr::PtrToString(user_data) + ") failed");
}
}


extern MT_LOCK MT_LOCK_cxx2c(CRWLock* lock, bool pass_ownership)
{
    if (s_TraceLock->Get())
        _TRACE("MT_LOCK_cxx2c(" + NStr::PtrToString(lock) + ')');
    return MT_LOCK_Create(static_cast<void*> (lock ? lock : new CRWLock),
                          s_LOCK_Handler,
                          !lock  ||  pass_ownership ? s_LOCK_Cleanup : 0);
}


/***********************************************************************
 *                               App Name                              *
 ***********************************************************************/


extern "C" {
static const char* s_GetAppName(void)
{
    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    return app ? app->GetProgramDisplayName().c_str() : 0;
}
}


/***********************************************************************
 *                               Referer                               *
 ***********************************************************************/


extern "C" {
static const char* s_GetReferer(void)
{
    const string& referer
        = CDiagContext::GetRequestContext().GetProperty("SELF_URL");
    return referer.empty() ? 0 : strdup(referer.c_str());
}
}


/***********************************************************************
 *                           NCBI Request ID                           *
 ***********************************************************************/


extern "C" {
static char* s_GetRequestID(ENcbiRequestID reqid)
{
    string id;
    switch (reqid) {
    case eNcbiRequestID_SID:
        if (!CDiagContext::GetRequestContext().IsSetSessionID())
            CDiagContext::GetRequestContext().SetSessionID();
        CDiagContext::GetRequestContext().GetSessionID().swap(id);
        break;
    case eNcbiRequestID_HitID:
        id = CDiagContext::GetRequestContext().GetNextSubHitID();
        break;
    case eNcbiTraceState:
        id = CDiagContext::GetRequestContext().GetTraceState();
        break;
    case eNcbiTraceParent:
        id = CDiagContext::GetRequestContext().GetTraceParent();
        break;
    default:
        return 0;
    }
    return id.empty() ? 0 : strdup(id.c_str());
}
}


/***********************************************************************
 *                          NCBI Request DTab                          *
 ***********************************************************************/


extern "C" {
static const char* s_GetRequestDTab(void)
{
    if (!CDiagContext::GetRequestContext().IsSetDtab())
        CDiagContext::GetRequestContext().SetDtab("");
    return CDiagContext::GetRequestContext().GetDtab().c_str();
}
}


/***********************************************************************
 *                         CRAZY MONKEY CALLS                          *
 ***********************************************************************/


#ifdef NCBI_MONKEY
extern "C" {
    static MONKEY_RETTYPE 
        MONKEY_STDCALL s_MonkeySend(MONKEY_SOCKTYPE        sock,
                                    const MONKEY_DATATYPE  data,
                                    MONKEY_LENTYPE         size,
                                    int                    flags,
                                    void* /* SOCK* */      sock_ptr)
    {
        return CMonkey::Instance()->Send(sock, data, size, flags, 
                                         (SOCK*)sock_ptr);
    }
    
    static MONKEY_RETTYPE 
        MONKEY_STDCALL s_MonkeyRecv(MONKEY_SOCKTYPE   sock,
                                    MONKEY_DATATYPE   buf,
                                    MONKEY_LENTYPE    size,
                                    int               flags,
                                    void* /* SOCK* */ sock_ptr)
    {
        return CMonkey::Instance()->Recv(sock, buf, size, flags, 
                                         (SOCK*) sock_ptr);
    }

    
    static int MONKEY_STDCALL s_MonkeyConnect(MONKEY_SOCKTYPE        sock,
                                              const struct sockaddr* name,
                                              MONKEY_SOCKLENTYPE     namelen)
    {
        return CMonkey::Instance()->Connect(sock, name, namelen);
    }

    
    static int /*bool*/ s_MonkeyPoll(size_t*                  n,
                                     void* /* SSOCK_Poll** */ polls,
                                     EIO_Status*              return_status)
    {
        return CMonkey::Instance()->
            Poll(n, (SSOCK_Poll**) polls, return_status) ? 1 : 0;
    }


    static void s_MonkeyClose(SOCKET  sock)
    {
        CMonkey::Instance()->Close(sock);
    }


    static void s_MonkeySockHasSocket(void* /* SOCK* */ sock, 
                                      MONKEY_SOCKTYPE socket)
    {
        CMonkey::Instance()->SockHasSocket((SOCK)sock, socket);
    }
}


static int s_MONKEY_Poll_dummy(size_t*     n,
                               void*       polls,
                               EIO_Status* return_status)
{
    return 0; /* call was not intercepted by Monkey*/
}


static void s_MONKEY_Close_dummy(SOCKET sock)
{
    return; /* call was not intercepted by Monkey*/
}


/* Chaos Monkey hooks for Connect library*/
static void s_SetMonkeyHooks(EMonkeyHookSwitch hook_switch)
{
    switch (hook_switch)
    {
    case eMonkeyHookSwitch_Disabled:
        g_MONKEY_Send          = 0;
        g_MONKEY_Recv          = 0;
        g_MONKEY_Connect       = 0;
        g_MONKEY_Poll          = 0;
        g_MONKEY_Close         = 0;
        g_MONKEY_SockHasSocket = 0;
        break;
    case eMonkeyHookSwitch_Enabled:
        g_MONKEY_Send          = s_MonkeySend;
        g_MONKEY_Recv          = s_MonkeyRecv;
        g_MONKEY_Connect       = s_MonkeyConnect;
        g_MONKEY_Poll          = s_MonkeyPoll;
        g_MONKEY_Close         = s_MonkeyClose;
        g_MONKEY_SockHasSocket = s_MonkeySockHasSocket;
        break;
    default:
        _TROUBLE;
        break;
    }
}
#endif //NCBI_MONKEY


#ifdef NCBI_POSIX_THREADS
extern "C" {
static void x_PreFork(void)
{
    CORE_LOCK_WRITE;
}

static void x_PostForkParent(void)
{
    CORE_UNLOCK;
}

static void x_PostForkChild(void)
{
    extern bool g_CorelibDaemonize;
    if (g_CorelibDaemonize)
        CORE_UNLOCK;
    else
        g_CORE_MT_Lock = 0;
}
} // extern "C"
#endif // NCBI_POSIX_THREADS


enum EConnectInit {
    eConnectInit_Weak     = -1,  ///< CConn_Initer
    eConnectInit_Intact   =  0,  ///< Not yet visited
    eConnectInit_Strong   =  1,  ///< User init detected
    eConnectInit_Explicit =  2   ///< CONNECT_Init() called
};

static atomic<EConnectInit> s_ConnectInit = eConnectInit_Intact;


/***********************************************************************
 *                                 Fini                                *
 ***********************************************************************/


extern "C" {
static void s_Fini(void) THROWS_NONE
{
    _TRACE("CONNECT::s_Fini()");
    s_CORE_Set &= ~g_CORE_Set;
    if (s_CORE_Set & eCORE_SetSSL)
        SOCK_SetupSSL(0);
    if (s_CORE_Set & eCORE_SetREG)
        CORE_SetREG(0);
    if (s_CORE_Set & eCORE_SetLOG)
        CORE_SetLOG(0);
    if (s_CORE_Set & eCORE_SetLOCK)
        CORE_SetLOCK(0);
    g_CORE_Set &= ~s_CORE_Set;
    s_CORE_Set  =  0;
}
}


/***********************************************************************
 *                                 Init                                *
 ***********************************************************************/


DEFINE_STATIC_FAST_MUTEX(s_ConnectInitMutex);

/* NB: gets called under a lock (s_ConnectInitMutex) */
static void s_Init(const IRWRegistry* reg  = 0,
                   FSSLSetup          ssl  = 0,
                   CRWLock*           lock = 0,
                   TConnectInitFlags  flag = 0,
                   EConnectInit       how  = eConnectInit_Weak)
{
    _TRACE("CONNECT::s_Init(reg="
           + NStr::PtrToString(reg)                         + ", ssl="
           + (ssl == NcbiSetupTls
              ? string("NcbiSetupTls")
              : NStr::PtrToString((void*) ssl))             + "(), lock="
           + NStr::PtrToString(lock)                        + ", flag=0x"
           + NStr::UIntToString((unsigned int) flag, 0, 16) + ", how="
           + NStr::IntToString(int(how))                    + ')');
    _ASSERT(how != eConnectInit_Intact);

    auto connect_init = s_ConnectInit.load(memory_order_acquire);
    if (connect_init == how  &&  how == eConnectInit_Explicit)
        ERR_POST_X(11, "CONNECT_Init() called more than once");

    TCORE_Set x_set = 0;
    if (!(g_CORE_Set & eCORE_SetLOCK)) {
        NCBI_LSAN_DISABLE_GUARD;  
        CORE_SetLOCK(MT_LOCK_cxx2c(lock, !!(flag & eConnectInit_OwnLock)));
        x_set |= eCORE_SetLOCK;
    }
    if (!(g_CORE_Set & eCORE_SetLOG)) {
        CORE_SetLOG(LOG_cxx2c());
        x_set |= eCORE_SetLOG;
    }
    if (!(g_CORE_Set & eCORE_SetREG)) {
        CORE_SetREG(REG_cxx2c(reg, !!(flag & eConnectInit_OwnRegistry)));
        x_set |= eCORE_SetREG;
    }
    if (!(g_CORE_Set & eCORE_SetSSL)) {
        EIO_Status status = SOCK_SetupSSLInternalEx(ssl, 1/*init*/);
        if (status != eIO_Success) {
            ERR_POST_X(10, Critical << "Failed to initialize SSL: "
                       << IO_StatusStr(status));
        }
        if (ssl)
            x_set |= eCORE_SetSSL;
    }
    g_CORE_Set &= ~x_set;
    s_CORE_Set |=  x_set;

    if (connect_init == eConnectInit_Intact) {
        int err = 0;
        g_NCBI_ConnectRandomSeed
            = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
        srand(g_NCBI_ConnectRandomSeed);
        if (x_set  &&  atexit(s_Fini) != 0)
            err |= 1;
#ifdef NCBI_POSIX_THREADS
        if (pthread_atfork(x_PreFork, x_PostForkParent, x_PostForkChild) != 0)
            err |= 2;
#endif // NCBI_POSIX_THREADS
        if (err) {
            static const char* what[] = {0, "exit", "fork", "exit/fork"};
            ERR_POST_X(9, Critical
                       << "Failed to register "
                       << what[err] << " handler" << &"s"[!(err == 3)]);
        }
    }

    g_CORE_GetAppName     = s_GetAppName;
    g_CORE_GetReferer     = s_GetReferer;
    g_CORE_GetRequestID   = s_GetRequestID;
    g_CORE_GetRequestDtab = s_GetRequestDTab;

#ifdef NCBI_MONKEY
    /* Allow CMonkey to switch hooks to Connect library */
    CMonkey::MonkeyHookSwitchSet(s_SetMonkeyHooks);
    /* Create CMonkey instance. It loads config and sets hooks */
    CMonkey::Instance();
#endif //NCBI_MONKEY

    if (how < eConnectInit_Strong  &&  g_CORE_Set)
        how = eConnectInit_Strong;
    if (connect_init < how  ||  connect_init == eConnectInit_Intact)
        s_ConnectInit.store(how, memory_order_release);
}


/* PUBLIC */
extern void CONNECT_Init(const IRWRegistry* reg,
                         CRWLock*           lock,
                         TConnectInitFlags  flag,
                         FSSLSetup          ssl)
{
    CFastMutexGuard guard(s_ConnectInitMutex);
    _TRACE("CONNECT_Init(reg="
           + NStr::PtrToString(reg)                         + ", lock="
           + NStr::PtrToString(lock)                        + ", flag=0x"
           + NStr::UIntToString((unsigned int) flag, 0, 16) + ", ssl="
           + NStr::PtrToString((void*) ssl)                 + "())");
    try {
        g_CORE_Set = 0;
        s_Init(reg, flag & eConnectInit_NoSSL ? 0 :
               ssl ? ssl : NcbiSetupTls,
               lock, flag, eConnectInit_Explicit);
    }
    NCBI_CATCH_ALL_X(8, "CONNECT_Init() failed");
}


CConnIniter::CConnIniter(void)
{
    auto connect_init = s_ConnectInit.load(memory_order_relaxed);
    if (connect_init != eConnectInit_Intact)
        return;
    CFastMutexGuard guard(s_ConnectInitMutex);
    try {
        connect_init = s_ConnectInit.load(memory_order_acquire);
        if (connect_init == eConnectInit_Intact) {
            CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
            s_Init(app ? &app->GetConfig() : 0, NcbiSetupTls,
                   0, eConnectInit_OwnRegistry);
        }
    }
    NCBI_CATCH_ALL_X(7, "CConn_Initer::CConn_Initer() failed");
}


END_NCBI_SCOPE
