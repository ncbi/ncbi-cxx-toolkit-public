
#ifndef _APPLOG_WRAP_P_H_
#define _APPLOG_WRAP_P_H_

#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>
#include <semaphore.h>

#if !defined(NDEBUG)  &&  !defined(_DEBUG)
#  define NDEBUG
#elif defined(_DEBUG) && defined(NDEBUG)
#  undef NDEBUG
#endif

#include <assert.h>


namespace IdLogUtil {

#define APPLOG_HOST_MAX       256
#define APPLOG_APPNAME_MAX   1024
#define APPLOG_CLIENT_MAX     256
#define APPLOG_SESSION_MAX    256
#define APPLOG_HITID_MAX      256

#define APPLOG_INIT_SUBREQ_BUFFERS  1
#define APPLOG_MAX_SUBREQ_BUFFERS  5000

#define LOG_MAX_VALUE_LEN     256
#define APPLOG_ENTRY_MAX_ALLOC 8192
#define APPLOG_ENTRY_MAX      8190 /* - 2, for ending '\n\0' */

#define NCBILOG_USE_FILE_DESCRIPTORS 0

#if defined(NDEBUG)
#  define verify(expr)  while ( expr ) break
#else
#  define verify(expr)  assert(expr)
#endif

#define APPLOG_INT8_FORMAT_SPEC  PRIi64
#define APPLOG_UINT8_FORMAT_SPEC PRIu64

#define DIR_SEPARATOR     '/'

#ifdef __clang__
#define CLANG_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#else
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

typedef struct { volatile int counter; } atomic_t;

#if GCC_VERSION >= 40800 || CLANG_VERSION >= 30400   /* Using GCC/CLANG C11 builtins */

	static __inline__ __attribute__((always_inline)) int  cmpxchg(atomic_t * const a, int o, const int n) {
		return __atomic_compare_exchange_n(&a->counter, &o, n, 1,  __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
	}
	#define atomic_set(v, i)         __atomic_store_n(&(v)->counter, (i),  __ATOMIC_RELEASE)
	#define atomic_get(v)            __atomic_load_n(&(v)->counter, __ATOMIC_ACQUIRE)
	#define atomic_inc(v)            __atomic_add_fetch(&(v)->counter, 1, __ATOMIC_ACQUIRE)
	#define atomic_dec(v)            __atomic_add_fetch(&(v)->counter, -1, __ATOMIC_ACQUIRE)

#else

	#error "gcc 4.8 or higher (supporing C11 memory model) is needed"
	
#endif

#define FUTEX_WAIT              0
#define FUTEX_WAKE              1

#define FALSE					0
#define TRUE					1

typedef enum {
    eAppLog_NotSet = 0,      /**< Reserved value, never used in messages */
    eAppLog_AppBegin,        /**< PB  */
    eAppLog_AppRun,          /**< P   */
    eAppLog_AppEnd,          /**< PE  */
    eAppLog_RequestBegin,    /**< RB  */
    eAppLog_Request,         /**< R   */
    eAppLog_RequestEnd,      /**< RE  */
    eAppLog_SubRequestBegin, /**< RB  */
    eAppLog_SubRequest,      /**< R   */
    eAppLog_SubRequestEnd    /**< RE  */
} EAppLog_AppState;

typedef struct STime_tag {
    time_t         sec;      /**< GMT time                    */
    unsigned long  ns;       /**< Nanosecond part of the time */
} STime;


struct SInfo_tag {
    /* Posting data */
    int               pid;             			/**< Process ID                                          */
    uint64_t          rid;                      /**< Request ID (e.g. iteration number in case of a CGI) */
    EAppLog_AppState  state;                    /**< Application state                                   */
    uint64_t          guid;                     /**< Globally unique process ID                          */
    uint64_t          psn;                      /**< Serial number of the posting within the process     */
    char              host[APPLOG_HOST_MAX+1]; /**< Name of the host where the process runs 
                                                     (UNK_HOST if unknown)                               */
    char              appname[3*APPLOG_APPNAME_MAX+1];
    char              instancename[APPLOG_APPNAME_MAX+1];
    /* Extras */

    char              phid[3*APPLOG_HITID_MAX+1];    
                                                /**< App-wide hit ID (empty string if unknown)           */
    unsigned int      phid_sub_id;              /**< App-wide sub-hit ID counter                         */
    int/*bool*/       phid_inherit;             /**< 1 if PHID set explicitly and should be inherited (by requests) */
    const char*       host_role;                /**< Host role (NULL if unknown or not set)              */
    const char*       host_location;            /**< Host location (NULL if unknown or not set)          */

    /* Control parameters */
    
    int/*bool*/       remote_logging;           /**< 1 if logging request is going from remote host */
    EAppLog_Severity  post_level;               /**< Posting level                                  */
    STime             app_start_time;           /**< Application start time                         */
    char*             app_full_name;            /**< Pointer to a full application name (argv[0])   */
    char*             app_base_name;            /**< Pointer to application base name               */
                                                    
    /* Log file names and handles */

    EAppLog_Destination destination;            /**< Current logging destination            */
    unsigned int      server_port;              /**< $SERVER_PORT on calling/current host   */
    time_t            last_reopen_time;         /**< Last reopen time for log files         */
#if NCBILOG_USE_FILE_DESCRIPTORS
    int               file_trace;               /**< Saved files for log files              */
    int               file_err;
    int               file_log;
    int               file_perf;
#else
    FILE*             file_trace;               /**< Saved files for log files              */
    FILE*             file_err;
    FILE*             file_log;
    FILE*             file_perf;
#endif
    char*             file_trace_name;          /**< Saved file names for log files         */
    char*             file_err_name;
    char*             file_log_name;
    char*             file_perf_name;
    int/*bool*/       reuse_file_names;         /**< File names where not changed, reuse it */
};

typedef struct SInfo_tag TAppLog_Info;

struct SContext_req_tag {
    uint64_t          rid;                      /**< Request ID (e.g. iteration number in case of a CGI) */
                                                /**< Local value, use global value if equal to 0         */
    STime             req_start_time;           /**< Request start time                                  */
    STime             post_time;                /**< GMT time at which the message was posted, 
                                                     use current time if it is not specified (equal to 0)*/
    int/*bool*/       user_posting_time;        /**< If 1 use post_time as is, and never change it       */
};

typedef struct SContext_req_tag* TAppLog_ReqContext;

struct SContext_tag {
    uint64_t          tid;                      /**< Thread  ID                                          */
    uint64_t          tsn;                      /**< Serial number of the posting within the thread      */
    EAppLog_AppState  state;                    /**< Application state                                   */
                                                /**< Local value, use global value if eAppLog_NotSet     */
    char  client [APPLOG_CLIENT_MAX+1];         /**< Request specific client IP address                  */
    int/*bool*/  is_client_set;                 /**< 1 if request 'client' has set, even to empty        */
    char  session[3*APPLOG_SESSION_MAX+1];      /**< Request specific session ID                         */
    int/*bool*/  is_session_set;                /**< 1 if request 'session' has set, even to empty       */
    char  phid   [3*APPLOG_HITID_MAX+1];        /**< Request specific hit ID                             */
    unsigned int phid_sub_id;                   /**< Request specific sub-hit ID counter                 */
    int/*bool*/  is_phid_logged;

	int               subrequest_cnt;
	int               requestbuf_limit;
	int               requestbuf_count;
	struct SContext_req_tag *requestbuf;
	struct SContext_req_tag *request;
    char message[APPLOG_ENTRY_MAX_ALLOC];       /**< Buffer used to collect a message and log it         */
};

typedef struct SContext_tag TAppLog_Context_Data;
typedef struct SContext_tag* TAppLog_Context;

typedef struct {
    const char* key;
    const char* value;
} SAppLog_Param;

typedef enum {
    eDiag_Trace,             /**< .trace */
    eDiag_Err,               /**< .err   */
    eDiag_Log,               /**< .log   */
    eDiag_Perf               /**< .perf  */
} EAppLog_DiagFile;

/* =========== QUEUE STUFF ============ */
#ifdef __MACH__
#define QSYNC_CONDVAR
#elif defined(__linux__)
#define QSYNC_FUTEX
#elif defined(WIN32)
#define QSYNC_WIN32
#else
#error "Platform is not supported"
#endif

#if defined(QSYNC_CONDVAR)
typedef struct {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
} qsync_object_t;
#elif defined(QSYNC_FUTEX)
typedef struct { int dummy; } qsync_object_t;
#elif defined(QSYNC_WIN32)
typedef HANDLE qsync_object_t;
#else
#error "QSYNC type is undefined/unsupported"
#endif

typedef struct {
	char* msg;
	void* ctx;
	log_kind_t kind;
    STime time;
	char lmsg[128];
} log_queue_data_t;


#define CPUCLS 64

typedef struct {
    atomic_t m_sequence;
	char _pad0[CPUCLS - sizeof(atomic_t)];
    log_queue_data_t m_data;
} log_queue_cell_t;

typedef struct {
	char _pad0[CPUCLS];
    log_queue_cell_t* m_buffer;
    int m_size;
	char _pad1[CPUCLS - sizeof(atomic_t)];
    atomic_t m_push_pos;
	char _pad2[CPUCLS - sizeof(atomic_t)];
    atomic_t m_pop_pos;
	char _pad3[CPUCLS - sizeof(atomic_t)];
    atomic_t m_waiting_push_cnt;
	char _pad4[CPUCLS - sizeof(atomic_t)];
    atomic_t m_waiting_push_ev;
	char _pad5[CPUCLS - sizeof(atomic_t)];
	qsync_object_t m_waiting_push_obj;
	char _pad6[CPUCLS - sizeof(atomic_t)];
	atomic_t m_waiting_pop_cnt;
	char _pad7[CPUCLS - sizeof(atomic_t)];
	atomic_t m_waiting_pop_ev;
	char _pad8[CPUCLS - sizeof(atomic_t)];
	qsync_object_t m_waiting_pop_obj;
	char _pad9[CPUCLS - sizeof(atomic_t)];
} log_queue_t;


};

#endif
