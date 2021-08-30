
#define __STDC_FORMAT_MACROS

#include <ncbi_pch.hpp>

#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <ctype.h>

#include <objtools/pubseq_gateway/impl/diag/AppLogWrap.h>
#include <objtools/pubseq_gateway/impl/diag/AppLogWrap_p.h>


namespace IdLogUtil {

static pthread_key_t              sx_Tls       = 0;
static volatile int /*bool*/      sx_TlsIsInit = 0;
static volatile int /*bool*/      sx_IsInit    = 0;
static volatile int /*bool*/      sx_IsEnabled = 0;
static TAppLog_Info*              sx_Info      = NULL;
static log_queue_t                sx_Queue;
static EAppLog_Severity           sx_Severity  = eAppLog_Info;
static EAppLog_Destination        sx_Destination = eAppLog_Default;
static volatile int /*bool*/      sx_log_created = 0;
static volatile int /*bool*/      sx_log_closed = 0;
static volatile int /*bool*/      sx_log_destroyed = 0;

#define UNKNOWN_HOST         "UNK_HOST"
#define UNKNOWN_CLIENT       "UNK_CLIENT"
#define UNKNOWN_SESSION      "UNK_SESSION"
#define UNKNOWN_HITID        ""
#define UNKNOWN_APPNAME      "UNK_APP"
#define BUF_SIZE			 (16*1024)
static const char* kBaseLogDir  = "/log/";

#define MAX_MS_IN_LOG		500L
#define MAX_REQ_PER_CYCLE	100000

#ifdef QSYNC_FUTEX
static int srvutils_do_futex(void *futex, int op, int val, const struct timespec *timeout) {
    int rt;
    struct timespec ts_long = {10000, 0};
    if (!timeout) timeout = &ts_long;
    switch(op) {
		case FUTEX_WAIT:
			while ((rt = syscall(__NR_futex, futex, op, val, timeout, 0, 0))) {
				switch (errno) {
					case EWOULDBLOCK: /*** defined to be the same:   case EAGAIN: ***/
						return 0; /** someone has already grabbed this **/
					 case EINTR:
						break; /** keep waiting ***/
					 case ETIMEDOUT:
						return ETIMEDOUT;
					 default:
						return -1;
				}
			}
			break;
		 case FUTEX_WAKE:
			rt = syscall(__NR_futex, futex, op, val, timeout);
			return rt;
		 default:
			return -1;
    }
    return 0;
}
#endif

static void s_TlsInit() {
	assert(!sx_TlsIsInit);
	verify(pthread_key_create(&sx_Tls, NULL) == 0);
	verify(pthread_setspecific(sx_Tls, 0) == 0);
	sx_TlsIsInit = 1;
}

static void s_TlsDestroy(void) {
    assert(sx_TlsIsInit);
    verify(pthread_key_delete(sx_Tls) == 0);
	sx_Tls = 0;
    sx_TlsIsInit = 0 /*false*/;
}

static void* s_TlsGetValue(void) {
    void* ptr = NULL;
    assert(sx_TlsIsInit);
    ptr = pthread_getspecific(sx_Tls);
    return ptr;
}

static void s_TlsSetValue(void* ptr) {
    assert(sx_TlsIsInit);
    verify(pthread_setspecific(sx_Tls, ptr) == 0);
}

static uint64_t s_GetTID(void) {
    uint64_t tid;
    tid = (uint64_t)syscall(SYS_gettid);
    return tid;
}

static int s_GetPID(void) {
    /* For main thread always force caching of PIDs */
    static int pid = 0;
    if (!pid)
        pid = getpid();
    return pid;
}

static const char* AppLog_GetHostName(void) {
    static char host[256];
    struct utsname buf;
    if (!host[0]) {
		if (uname(&buf) == 0) {
			strncpy(host, buf.nodename, sizeof(host) - 1);
		}
	}
    return host;
}

static const char* s_ConcatPathEx(
    const char* p1, size_t p1_len,  /* [in]     non-NULL */
    const char* p2, size_t p2_len,  /* [in]     non-NULL */
    char*       dst,                /* [in/out] non-NULL */
    size_t      dst_size            /* [in]              */
)
{
    size_t n = 0;
    assert(p1);
    assert(p2);
    assert(dst);
    assert(dst_size);

    if (p1_len + p2_len + 2 >= dst_size) {
        return NULL;
    }
    memcpy(dst, p1, p1_len);
    n += p1_len;
    if (dst[n-1] != DIR_SEPARATOR) {
        dst[n++] = DIR_SEPARATOR;
    }
    memcpy(dst + n, p2, p2_len);
    n += p2_len;
    dst[n] = '\0';
    return dst;
}


/** Concatenate two parts of the path (zero-terminated strings) for the current OS.
 */
static const char* s_ConcatPath(
    const char* p1,         /* [in]     non-NULL */
    const char* p2,         /* [in]     non-NULL */
    char*       dst,        /* [in/out] non-NULL */
    size_t      dst_size    /* [in]              */
) 
{
    return s_ConcatPathEx(p1, strlen(p1), p2, strlen(p2), dst, dst_size);
}

static const char* s_ReadFileString(const char* filename) {
    FILE*  fp;
    char   buf[32];
    size_t len = sizeof(buf);
    char*  p = buf;

    fp = fopen(filename, "rt");
    if (!fp) {
        return NULL;
    }
    if (fgets(buf, (int)len, fp) == NULL) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    /* Removing trailing spaces and following EOL */
    while (*p  &&  !(*p == ' ' || *p == '\n')) p++;
    *p = '\0';
    return strdup(buf);
}    


static const char* AppLog_GetHostRole(void) {
    static const char* role = NULL;
    if (role) {
        return role;
    }
    role = s_ReadFileString("/etc/ncbi/role");
    return role;
}

static const char* AppLog_GetHostLocation(void) {
    static const char* location = NULL;
    if (location) {
        return location;
    }
    location = s_ReadFileString("/etc/ncbi/location");
    return location;
}

static uint64_t s_CreateUID(void) {
    int   pid = s_GetPID();
    time_t         t = time(0);
    const char*    host = AppLog_GetHostName();
    const char*    s;
    uint64_t  h = 212;

    /* Use host name */
    if (host) {
        for (s = host;  *s != '\0';  s++) {
            h = h*1265 + *s;
        }
    }
    h &= 0xFFFF;
    return ((uint64_t)h << 48) |
           (((uint64_t)pid & 0xFFFF) << 32) |
           (((uint64_t)t & 0xFFFFFFF) << 4) |
           3;
}

static int/*bool*/ s_GetTimeT(time_t* time_sec, unsigned long* time_ns) {
    struct timeval tp;
    if (!time_sec || ! time_ns) {
        return FALSE;
    }
    if (gettimeofday(&tp, 0) != 0 && gettimeofday(&tp, 0) != 0) {
        *time_sec = -1;
    } else {
        *time_sec = tp.tv_sec;
        *time_ns  = (unsigned long)((uint64_t)tp.tv_usec * 1000);
    }
    if (*time_sec == (time_t)(-1)) {
        return FALSE;
    }
    return TRUE;
}

static int/*bool*/ s_GetTime(STime* t) {
    if (!t) {
        return FALSE;
    }
    return s_GetTimeT(&t->sec, &t->ns);
}

static char* s_GenerateHitID_Str(TAppLog_Context ctx, char* dst, int destlen)
{
    uint64_t hi, tid, rid, us, lo;
    unsigned int b0, b1, b2, b3;
    time_t time_sec;
    unsigned long time_ns;
    int n;

    if (!sx_Info->guid) {
        sx_Info->guid = s_CreateUID();
    }
    hi  = sx_Info->guid;
    b3  = (unsigned int)((hi >> 32) & 0xFFFFFFFF);
    b2  = (unsigned int)(hi & 0xFFFFFFFF);
    tid = ((uint64_t)ctx->tid & 0xFFFFFF) << 40;
    rid = (uint64_t)(sx_Info->rid & 0xFFFFFF) << 16;
    if (!s_GetTimeT(&time_sec, &time_ns)) {
        us = 0;
    } else {
        us = (time_ns/1000/16) & 0xFFFF;
    }
    lo = tid | rid | us;
    b1 = (unsigned int)((lo >> 32) & 0xFFFFFFFF);
    b0 = (unsigned int)(lo & 0xFFFFFFFF);
    n = snprintf(dst, destlen, "%08X%08X%08X%08X", b3, b2, b1, b0);
    if (n <= 0) {
        return NULL;
    }
    dst[n] = '\0';
    return dst;
}

static void s_ReserveReqContext(TAppLog_Context ctx, int is_subreq) {
	if (ctx->requestbuf_limit == 0) {
		ctx->requestbuf = (TAppLog_ReqContext)calloc(APPLOG_INIT_SUBREQ_BUFFERS, sizeof(struct SContext_req_tag));
		ctx->requestbuf_limit = APPLOG_INIT_SUBREQ_BUFFERS;
		ctx->requestbuf_count = 0;
	}
	if (is_subreq) {
		if (ctx->requestbuf_count >= ctx->requestbuf_limit) {
			int new_limit;
			int index = ctx->request - ctx->requestbuf;
			verify(ctx->requestbuf_limit < APPLOG_MAX_SUBREQ_BUFFERS);
			new_limit = (ctx->requestbuf_limit + 1) * 3 / 2;
			ctx->requestbuf = (TAppLog_ReqContext)realloc(ctx->requestbuf, new_limit * sizeof(struct SContext_req_tag));
			verify(ctx->requestbuf != NULL);
			if (new_limit > ctx->requestbuf_limit)
				memset(ctx->requestbuf + ctx->requestbuf_limit, 0, (new_limit - ctx->requestbuf_limit) * sizeof(struct SContext_req_tag));
			ctx->requestbuf_limit = new_limit;
			ctx->request = ctx->requestbuf + index;
		}
		ctx->requestbuf_count++;
		ctx->request++;
		ctx->request->post_time.sec = ctx->request->post_time.ns = 0;
		ctx->request->req_start_time.sec = ctx->request->req_start_time.ns = 0;
		ctx->request->user_posting_time = 0;
	}
	else {
		ctx->request = ctx->requestbuf;
		ctx->requestbuf_count = 1;
	}
}

static void s_ReleaseReqContext(TAppLog_Context ctx, int is_subreq) {
	if (is_subreq) {
		verify(ctx->requestbuf_count > 1);
		ctx->requestbuf_count--;
		ctx->request--;
		if (ctx->requestbuf_count == 1) {
			ctx->request = ctx->requestbuf;
		}
	}
}


static TAppLog_Context s_CreateContext(void) {
    /* Allocate memory for context structure */
    TAppLog_Context ctx = (TAppLog_Context) malloc(sizeof(TAppLog_Context_Data));
    assert(ctx);
    memset(ctx, 0, sizeof(TAppLog_Context_Data));
    /* Initialize context values, all other values = 0 */
    ctx->tid = s_GetTID();
    ctx->tsn = 1;
	s_ReserveReqContext(ctx, 0);
	s_ReleaseReqContext(ctx, 0);
    return ctx;
}

static int /*bool*/ s_AttachContext(TAppLog_Context ctx)
{
    TAppLog_Context prev;
    if (!ctx) {
        return 0;
    }
    prev = (TAppLog_Context)s_TlsGetValue();
    /* Do not attach context if some different context is already attached */
    if (prev && prev != ctx) {
        return 0;
    }
    /* Set new context */
    s_TlsSetValue((void*)ctx);
    return 1 /*true*/;
}

static char* s_GetAppBaseName(const char* path)
{
    const char* p = path;
    const char* s;
    char* name;

    /* Find file name */
    for (s = p; *p; ++p ) {
        if ((*p == '/') || (*p == '\\') || (*p == ':')) {
            /* Found a separator, step over it and any others which immediately follow it */
            while ((*p == '/') || (*p == '\\') || (*p == ':')) {
                ++p;
            }
            /* If we didn't reach the end of the path string */
            if (*p) {
                /* We have a new candidate for the file name */
                s = p;
            } else {
                /* otherwise strip off any trailing dir separators */
                while ((p > s)  &&  ((*--p == '/') || (*p == '\\')));
                /* Now, 's' points to the start of the file name, and 'p' to the end */
                break;
            }
        }
    }
    if (s == p) {
        return NULL;
    }
    name = strdup(s);
    if (!name) {
        return NULL;
    }
    return name;
}

static const char sx_EncodeTable[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "!",   "%22", "%23", "$",   "%25", "%26", "'",
    "(",   ")",   "*",   "%2B", ",",   "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

/** URL-encode up to "src_size" symbols(bytes) from buffer "src_buf".
 *  Write the encoded data to buffer "dst_buf", but no more than
 *  "dst_size" bytes.
 *  Assign "*src_read" to the # of bytes successfully encoded from "src_buf".
 *  Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 */
static void s_URL_Encode
    (const void* src_buf,    /* [in]     non-NULL */
     size_t      src_size,   /* [in]              */
     size_t*     src_read,   /* [out]    non-NULL */
     void*       dst_buf,    /* [in/out] non-NULL */
     size_t      dst_size,   /* [in]              */
     size_t*     dst_written /* [out]    non-NULL */
     )
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size  ||  !dst  ||  !src)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
        (*src_read)++, (*dst_written)++, src++, dst++)
    {
        const char* subst = sx_EncodeTable[*src];
        if (*subst != '%') {
            *dst = *subst;
        } else if (*dst_written < dst_size - 2) {
            *dst = '%';
            *(++dst) = *(++subst);
            *(++dst) = *(++subst);
            *dst_written += 2;
        } else {
            return;
        }
    }
}

static void s_Init(const char* appname) {
    size_t len, r_len, w_len;
    const char* app = NULL;
    
    /* Create info structure */
    assert(!sx_Info);
    sx_Info = (TAppLog_Info*) malloc(sizeof(TAppLog_Info));
    assert(sx_Info);
    memset(sx_Info, 0, sizeof(TAppLog_Info));
        
    /* Set defaults */
    sx_Info->psn = 1;
    sx_Info->destination = eAppLog_Default;
#if defined(NDEBUG)
    sx_Info->post_level = eAppLog_Error;
#else
    sx_Info->post_level = eAppLog_Warning;
#endif
    /* Calculate GUID */
    sx_Info->guid = s_CreateUID();

    app = appname;
    /* Set application name */
    if (!(app  &&  *app)) {
        app = "UNKNOWN";
    }
    sx_Info->app_full_name = strdup(app);
    assert(sx_Info->app_full_name);
    sx_Info->app_base_name = s_GetAppBaseName(sx_Info->app_full_name);
    assert(sx_Info->app_base_name);
    /* URL-encode base name and use it as display name */
    len = strlen(sx_Info->app_base_name);
    s_URL_Encode(sx_Info->app_base_name, len, &r_len,
                sx_Info->appname, 3*APPLOG_APPNAME_MAX, &w_len);
    sx_Info->appname[w_len] = '\0';

    /* Logging is initialized now and can be used */
    sx_IsEnabled = 1 /*true*/;
}

static TAppLog_Context s_GetContext() {
    TAppLog_Context ctx = NULL;
    /* Get context for current thread, or use global value for ST mode */
    ctx = (TAppLog_Context)s_TlsGetValue();
    /* Create new context if not created/attached yet */
    if (!ctx) {
        int is_attached;
        /* Special case, context for current thread was already destroyed */
        assert(sx_IsInit != 2);
        /* Create context */
        ctx = s_CreateContext();
        is_attached = s_AttachContext(ctx);
        verify(is_attached);
    }
    assert(ctx);
    return ctx;
}

static void s_ClearContext(TAppLog_Context ctx) {
    if (ctx) {
        ctx->request = ctx->requestbuf;
        ctx->requestbuf_count = 0;
    }
}

static void s_DestroyContext(TAppLog_Context ctx) {
    s_ClearContext(ctx);
	if (ctx) {
		if (ctx->requestbuf) {
			free(ctx->requestbuf);
			ctx->requestbuf = NULL;
			ctx->request = NULL;
			ctx->requestbuf_limit = 0;
			ctx->requestbuf_count = 0;
		}
		free(ctx);
	}
}

static void s_SetSession(char* dst, int buflen, const char* session) {
    if (session  &&  *session) {
        /* URL-encode */
        size_t len, r_len, w_len;
        len = strlen(session);
        s_URL_Encode(session, len, &r_len, dst, buflen - 1, &w_len);
        dst[w_len] = '\0';
    } else {
        dst[0] = '\0';
    }
}

static void s_SetInstance(char* dst, int buflen, const char* session) {
    if (session  &&  *session) {
        /* URL-encode */
        size_t len, r_len, w_len;
        len = strlen(session);
        s_URL_Encode(session, len, &r_len, dst, buflen - 1, &w_len);
        dst[w_len] = '\0';
    } else {
        dst[0] = '\0';
    }
}

static void s_SetHitID(char* dst, int buflen, const char* hit_id)
{
    if (hit_id  &&  *hit_id) {
        /* URL-encode */
        size_t len, r_len, w_len;
        len = strlen(hit_id);
        s_URL_Encode(hit_id, len, &r_len, dst, buflen - 1, &w_len);
        dst[w_len] = '\0';
    } else {
        dst[0] = '\0';
    }       
}

static int s_IsValidHitID(const char* hit_id) 
{
    int ret_val = 1;
    int len = 0;
    if (hit_id  &&  *hit_id) {
        char ch;
        while ((ch = *hit_id) != 0) {
            if ((!isalpha(ch) && !isdigit(ch) && ch != '.' && ch != '_' && ch != '-') || len >= APPLOG_HITID_MAX) {
                ret_val = 0;
                break;
            }
            len++;
            hit_id++;
        }
    }
    return ret_val;
}

static void s_SetHost(const char* host) {
    if (host  &&  *host) {
        size_t len;
        len = strlen(host);
        if (len > sizeof(sx_Info->host) - 1) {
            len = sizeof(sx_Info->host) - 1;
        }
        memcpy(sx_Info->host, host, len);
        sx_Info->host[len] = '\0';
    } else {
        sx_Info->host[0] = '\0';
    }
}
#if NCBILOG_USE_FILE_DESCRIPTORS
static void s_CloseFile(int *file) {
    if (*file != -1 && *file != STDERR_FILENO && *file != STDOUT_FILENO) {
        close(*file);
        *file = -1;
    }
}
#else
static void s_CloseFile(FILE** file) {
    if (*file && *file != stderr && *file != stdout) {
		fflush(*file);
		fclose(*file);
        *file = NULL;
    }
}
#endif

static void s_CloseLogFiles(int/*bool*/ cleanup) {
    switch (sx_Info->destination) {
        case eAppLog_Default:
        case eAppLog_Stdlog:
        case eAppLog_Cwd:
            /* Close files, see below */
            s_CloseFile(&sx_Info->file_trace);
            s_CloseFile(&sx_Info->file_log);
            s_CloseFile(&sx_Info->file_err);
            s_CloseFile(&sx_Info->file_perf);
            break;
        case eAppLog_Stdout:
        case eAppLog_Stderr:
        case eAppLog_Disable:
            /* Nothing to do here */
            break;
        default:
            verify(0);
    }

    if (cleanup  &&  sx_Info->file_trace_name) {
        free(sx_Info->file_trace_name);
        sx_Info->file_trace_name = NULL;
    }
    if (cleanup  &&  sx_Info->file_log_name) {
        free(sx_Info->file_log_name);
        sx_Info->file_log_name = NULL;
    }
    if (cleanup  &&  sx_Info->file_err_name) {
        free(sx_Info->file_err_name);
        sx_Info->file_err_name = NULL;
    }
    if (cleanup  &&  sx_Info->file_perf_name) {
        free(sx_Info->file_perf_name);
        sx_Info->file_perf_name = NULL;
    }
    if (cleanup) {
        sx_Info->reuse_file_names = FALSE;
    }
}

#if NCBILOG_USE_FILE_DESCRIPTORS
#define OPEN(path, handle, keeppath)         \
	verify(handle == -1);                    \
	handle = open(path, flags, mode);        \
	keeppath = strdup(path);
#else
#define OPEN(path, handle, keeppath)         \
	verify(handle == NULL);                  \
	handle = fopen(path, "a");               \
	if (handle)                                  \
		setvbuf(handle, NULL, _IOLBF, BUF_SIZE); \
	keeppath = strdup(path);
#endif

static int /*bool*/ s_SetLogFiles(const char* path_with_base_name)
{
    char path[FILENAME_MAX + 1];
    size_t n;
#if NCBILOG_USE_FILE_DESCRIPTORS
    int flags = O_CREAT | O_APPEND | O_WRONLY;
    mode_t mode = 0664;
#endif

    assert(path_with_base_name);
    /* Check max possible file name (path.trace) */
    n = strlen(path_with_base_name);
    assert(n);
    assert((n + 6) < sizeof(path));
    memcpy(path, path_with_base_name, n);

    /* Trace */
    strcpy(path + n, ".trace");
	OPEN(path, sx_Info->file_trace, sx_Info->file_trace_name);

    /* Log */
    if (sx_Info->file_trace) {
        strcpy(path + n, ".log");
		OPEN(path, sx_Info->file_log, sx_Info->file_log_name);
    }
    
    /* Err */
    if (sx_Info->file_log) {
        strcpy(path + n, ".err");
		OPEN(path, sx_Info->file_err, sx_Info->file_err_name);
    }
    
    /* Perf */
    if (sx_Info->file_err) {
        strcpy(path + n, ".perf");
		OPEN(path, sx_Info->file_perf, sx_Info->file_perf_name);
    }


#if NCBILOG_USE_FILE_DESCRIPTORS
    if ((sx_Info->file_trace != -1)  &&
        (sx_Info->file_log   != -1)  &&
        (sx_Info->file_err   != -1)  &&
        (sx_Info->file_perf  != -1) ) {
        return 1 /*true*/;
    }
#else
    if ((sx_Info->file_trace != NULL)  &&
        (sx_Info->file_log   != NULL)  &&
        (sx_Info->file_err   != NULL)  &&
        (sx_Info->file_perf  != NULL) ) {
        return 1 /*true*/;
    }
#endif
    s_CloseLogFiles(1);

    return 0 /*false*/;
}

static const char* s_GetToolkitRCLogLocation()
{
    static const char* log_path       = NULL;
    static const char  kSectionName[] = "[Web_dir_to_port]";
                
    FILE*  fp;
    char   buf[256];
    char   *line, *token, *p;
    int    inside_section = 0 /*false*/;
    size_t line_size;
            
    if (log_path) {
        return log_path;
    }

    /* Try to get more specific location from configuration file */
    fp = fopen("/etc/toolkitrc", "rt");
    if (!fp) {
        return log_path;
    }
    /* Reserve first byte in the buffer for the case of non-absolute path */
    buf[0] = '/';
    line = buf + 1;
    line_size = sizeof(buf) - 1;
        
    /* Read configuration file */
    while (fgets(line, (int)line_size, fp) != NULL) {
        if (inside_section) {
            if (line[0] == '#') {
                /* Comment - skip */
                continue;
            }
            if (line[0] == '[') {
                /* New section started - stop */
                break;
            }
            p = strchr(line, '=');
            if (!p) {
                /* Skip not matched lines*/
                continue;
            }
            token = p + 1;

            /* Remove all spaces */
            while ((p > line)  &&  (*--p == ' '));
            /* Compare path with file name */
            if (line[0] == '/') {
                /* absolute path */
                *++p = '\0';
                if (strncmp(line, sx_Info->app_full_name, strlen(sx_Info->app_full_name)) != 0) {
                    continue;
                }
            } else {
                char c;
                /* not an absolute path */
                if (*p != '/') {
                    *++p ='/';
                }
                /* save and restore byte used for string termination */
                c = *++p;
                *p = '\0';
                if (!strstr(sx_Info->app_full_name, buf)) {
                    continue;
                }
                *p = c;
            }
            /* Get token value, removing spaces and following EOL */
            while (*token == ' ') token++;
            p = token + 1;
            while (*p  &&  !(*p == ' ' || *p == '\n')) p++;
            *p = '\0';
    
            /* Compose full directory name */
            {{
                char path[FILENAME_MAX + 1];
                s_ConcatPath(kBaseLogDir, token, path, FILENAME_MAX + 1);
                log_path = strdup(path);
                break;
            }}
            /* continue to next line */
        }
        if (memcmp(line, kSectionName, sizeof(kSectionName)) >= 0) {
            inside_section = 1;
        }
    }
    fclose(fp);

    return log_path;
}

static int /*bool*/ s_SetLogFilesDir(const char* dir)
{
    char path[FILENAME_MAX + 1];
    size_t n, nlen;
    char* p;

    assert(dir);
    n = strlen(dir);
    assert(n);
    nlen = strlen(sx_Info->app_base_name);
    assert(nlen);

    /* Check max possible file name (dir/basename.trace) */
    assert((n + 1 + nlen + 6) < sizeof(path));
    s_ConcatPathEx(dir, n,  sx_Info->app_base_name, nlen, path, FILENAME_MAX + 1);
    if ( (p = getenv("NCBI_LOG_SUFFIX")) != NULL  &&  *p ) {
        int len = strlen(p);
        int lenpath = strlen(path);
        int maxlen = FILENAME_MAX - lenpath;
        if (len > maxlen)
            len = maxlen;
        if (len > 0)
            memcpy(&path[lenpath], p, len);
    }
    return s_SetLogFiles(path);
}

static void s_InitDestination(const char* logfile_path) {
#if NCBILOG_USE_FILE_DESCRIPTORS
    int flags = O_CREAT | O_APPEND | O_WRONLY;
    mode_t mode = 0664;
#endif
    time_t now;
    assert(sx_Info->destination != eAppLog_Disable);
            
#if NCBILOG_USE_FILE_DESCRIPTORS
    if (sx_Info->file_log == STDOUT_FILENO  ||  sx_Info->file_log == STDERR_FILENO) {
        /* We already have destination set to stdout/stderr -- don't need to reopen it */
        return;
    }       
#else
    if (sx_Info->file_log == stdout ||  sx_Info->file_log == stderr) {
        /* We already have destination set to stdout/stderr -- don't need to reopen it */
        return;
    }       
#endif
    /* Reopen files every 1 minute */
    time(&now);
    if (now - sx_Info->last_reopen_time < 60) {
        return;
    }
    sx_Info->last_reopen_time = now;
                    
    /* Try to open files */
                        
    if (sx_Info->destination == eAppLog_Default  &&  logfile_path) {
        /* special case to redirect all logging to specified files */
        s_CloseLogFiles(1);
        if (s_SetLogFiles(logfile_path)) {
            sx_Info->reuse_file_names = 1;
            return;
        }
        return;
    }
                
    /* Close */
    s_CloseLogFiles(0);
                
    if (sx_Info->destination == eAppLog_Default  ||
        sx_Info->destination == eAppLog_Stdlog   ||
        sx_Info->destination == eAppLog_Cwd) {
                    
        /* Destination and file names didn't changed, just reopen files */
        if (sx_Info->reuse_file_names) {
            
            assert(sx_Info->file_trace_name &&
                   sx_Info->file_log_name   &&
                   sx_Info->file_err_name   &&
                   sx_Info->file_perf_name);


#if NCBILOG_USE_FILE_DESCRIPTORS
            assert((sx_Info->file_trace == -1)  &&
                   (sx_Info->file_log   == -1)  &&
                   (sx_Info->file_err   == -1)  &&
                   (sx_Info->file_perf  == -1));
            sx_Info->file_trace = open(sx_Info->file_trace_name, flags, mode);
            sx_Info->file_log   = open(sx_Info->file_log_name,   flags, mode);
            sx_Info->file_err   = open(sx_Info->file_err_name,   flags, mode);
            sx_Info->file_perf  = open(sx_Info->file_perf_name,  flags, mode);
            if ((sx_Info->file_trace != -1)  &&
                (sx_Info->file_log   != -1)  &&
                (sx_Info->file_err   != -1)  &&
                (sx_Info->file_perf  != -1) ) {
                return;
            }
#else
            verify(sx_Info->file_trace == NULL);
            verify(sx_Info->file_log   == NULL);
            verify(sx_Info->file_err   == NULL);
			verify(sx_Info->file_perf  == NULL);
            sx_Info->file_trace = fopen(sx_Info->file_trace_name, "a");
            sx_Info->file_log   = fopen(sx_Info->file_log_name,   "a");
            sx_Info->file_err   = fopen(sx_Info->file_err_name,   "a");
            sx_Info->file_perf  = fopen(sx_Info->file_perf_name,  "a");
            if ((sx_Info->file_trace != NULL)  &&
                (sx_Info->file_log   != NULL)  &&
                (sx_Info->file_err   != NULL)  &&
                (sx_Info->file_perf  != NULL) ) {
				setvbuf(sx_Info->file_trace, NULL, _IOLBF, BUF_SIZE);
				setvbuf(sx_Info->file_log, NULL, _IOLBF, BUF_SIZE);
				setvbuf(sx_Info->file_err, NULL, _IOLBF, BUF_SIZE);
				setvbuf(sx_Info->file_perf, NULL, _IOLBF, BUF_SIZE);
                return;
            }
#endif
            /* Failed to reopen, try again from scratch */
            s_CloseLogFiles(1);
            sx_Info->reuse_file_names = 0;
        }
        
        /* Try default log location */
        {{
            char xdir[FILENAME_MAX + 1];
            const char* dir;

            /* /log */
            if (sx_Info->destination != eAppLog_Cwd) {
                /* toolkitrc file */
                dir = s_GetToolkitRCLogLocation();
                if (dir) {
                    if (s_SetLogFilesDir(dir)) {
                        sx_Info->reuse_file_names = 1;
                        return;
                    }
                }
                /* server port */
                if (sx_Info->server_port) {
                    snprintf(xdir, sizeof(xdir), "%s%d", kBaseLogDir, sx_Info->server_port);
                    if (s_SetLogFilesDir(xdir)) {
                        sx_Info->reuse_file_names = 1;
                        return;
                    }
                }
                
                /* /log/srv */
                dir = s_ConcatPath(kBaseLogDir, "srv", xdir, FILENAME_MAX + 1);
                if (dir) {
                    if (s_SetLogFilesDir(dir)) {
                        sx_Info->reuse_file_names = 1;
                        return;
                    }
                }
                /* /log/fallback */
                dir = s_ConcatPath(kBaseLogDir, "fallback", xdir, FILENAME_MAX + 1);
                if (dir) {
                    if (s_SetLogFilesDir(dir)) {
                        sx_Info->reuse_file_names = 1;
                        return;
                    }
                }
            }

            /* Try current directory -- eAppLog_Stdlog, eAppLog_Cwd */
            if (sx_Info->destination != eAppLog_Default) {
                char* cwd;
                cwd = getcwd(NULL, 0);
                if (cwd  &&  s_SetLogFilesDir(cwd)) {
                    free(cwd);
                    sx_Info->reuse_file_names = 1;
                    return;
                }
                free(cwd);
            }
            /* Fallback - use stderr */
            sx_Info->destination = sx_Destination = eAppLog_Stderr;
        }}
    }
    
    /* Use stdout/stderr */
    switch (sx_Info->destination) {
        case eAppLog_Stdout:
#if NCBILOG_USE_FILE_DESCRIPTORS
            sx_Info->file_trace = STDOUT_FILENO;
            sx_Info->file_log   = STDOUT_FILENO;
            sx_Info->file_err   = STDOUT_FILENO;
            sx_Info->file_perf  = STDOUT_FILENO;
#else
            sx_Info->file_trace = stdout;
            sx_Info->file_log   = stdout;
            sx_Info->file_err   = stdout;
            sx_Info->file_perf  = stdout;
#endif
            break;
        case eAppLog_Stderr:
#if NCBILOG_USE_FILE_DESCRIPTORS
            sx_Info->file_trace = STDERR_FILENO;
            sx_Info->file_log   = STDERR_FILENO;
            sx_Info->file_err   = STDERR_FILENO;
            sx_Info->file_perf  = STDERR_FILENO;
#else
            sx_Info->file_trace = stderr;
            sx_Info->file_log   = stderr;
            sx_Info->file_err   = stderr;
            sx_Info->file_perf  = stderr;
#endif
            break;
        default:
            verify(0);
    }
}

static void s_SetClient(char* dst, int buflen, const char* client) {
    if (client  &&  *client) {
        size_t len;
        len = strlen(client);
        if ((int)len > buflen - 1) {
            len = buflen - 1;
        }
        memcpy(dst, client, len);
        dst[len] = '\0';
    } else {
        dst[0] = '\0';
    }
}

static double s_DiffTime(const STime time_start, const STime time_end) {
    double ts;
    ts = (time_end.sec - time_start.sec) +
         ((double)time_end.ns - time_start.ns)/1000000000;
    return ts;
}

static int/*bool*/ s_GetTimeStr(
	char*         buf,        /* [in/out] non-NULL, must fit 27 symbols */
	int           buf_len,
    time_t        time_sec,   /* [in]              */
    unsigned long time_ns     /* [in]              */)
{
    struct tm temp;
    struct tm *t;
    int n;

    /* Get current time if necessary */
    if ( !time_sec ) {
        if (!s_GetTimeT(&time_sec, &time_ns)) {
            return FALSE;
        }
    }
    
    /* Get local time */
    if (localtime_r(&time_sec, &temp) == NULL)
		return FALSE;
    t = &temp;
    /* YYYY-MM-DDThh:mm:ss.ssssss */
    n = snprintf(buf, buf_len, "%04u-%02u-%02uT%02u:%02u:%02u.%06u",
        t->tm_year + 1900,
        t->tm_mon + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec,
        (unsigned int)(time_ns / 1000) /* truncate to 6 digits */
    );
    if (n != 26) {
        return FALSE;
    }
    return TRUE;
}

static void s_SetState(TAppLog_Context ctx, EAppLog_AppState state) {
	EAppLog_AppState s = (ctx->state == eAppLog_NotSet)  ? sx_Info->state  : ctx->state;
    switch (state) {
        case eAppLog_AppBegin:
			verify(sx_Info->state == eAppLog_NotSet);
            sx_Info->state = state;
            ctx->state = state;
            break;
        case eAppLog_AppRun:
			verify(sx_Info->state == eAppLog_AppBegin || s == eAppLog_RequestEnd);
            sx_Info->state = state;
            ctx->state = state;
            break;
        case eAppLog_AppEnd:
			verify(sx_Info->state != eAppLog_AppEnd);
            sx_Info->state = state;
            ctx->state = state;
            break;
        case eAppLog_RequestBegin:
			verify(s == eAppLog_AppBegin  ||
                   s == eAppLog_AppRun    ||
                   s == eAppLog_RequestEnd);
            ctx->state = state;
            break;
		case eAppLog_SubRequestBegin:
			verify(s == eAppLog_Request ||
			       s == eAppLog_RequestBegin);
			ctx->state = state;
			break;
        case eAppLog_Request:
			verify(s == eAppLog_RequestBegin || 
			       s == eAppLog_SubRequestEnd);
            ctx->state = state;
            break;
        case eAppLog_SubRequest:
			verify(s == eAppLog_SubRequestBegin);
            ctx->state = state;
            break;
        case eAppLog_RequestEnd:
			verify(s == eAppLog_Request  || s == eAppLog_RequestBegin);
            ctx->state = state;
            break;
        case eAppLog_SubRequestEnd:
			verify(s == eAppLog_SubRequest  || s == eAppLog_SubRequestBegin);
            ctx->state = state;
            break;
        default:
            verify(0);
    }
}

static const char* sx_AppStateStr[] = {
    "NS", "PB", "P", "PE", "RB", "R", "RE", "RB", "R", "RE"
};

static size_t s_PrintCommonPrefix(TAppLog_Context ctx)
{
    uint64_t  x_pid;
    uint64_t  x_rid;
    EAppLog_AppState x_st;
    const char        *x_state, *x_host, *x_client, *x_session, *x_appname;
    char              x_time[27];
    int               x_guid_hi, x_guid_lo;
    int               n;
    int               inside_request;
    
    x_guid_hi = (int)((sx_Info->guid >> 32) & 0xFFFFFFFF);
    x_guid_lo = (int) (sx_Info->guid & 0xFFFFFFFF);

    /* Get application state (context-specific) */
    x_st = (ctx->state == eAppLog_NotSet) ? sx_Info->state : ctx->state;
    x_state = sx_AppStateStr[x_st];
    switch (x_st) {
        case eAppLog_SubRequestBegin:
        case eAppLog_SubRequest:
        case eAppLog_SubRequestEnd:
        case eAppLog_RequestBegin:
        case eAppLog_Request:
        case eAppLog_RequestEnd:
            inside_request = 1;
            break;
        default:
            inside_request = 0;
    }
    
	verify(ctx->request != NULL);
    /* Get posting time string (current or specified by user) */
    if (!ctx->request->post_time.sec) {
        if (!s_GetTime((STime*)&ctx->request->post_time)) {
            /* error */
            return 0;
        }
    }
    if (!s_GetTimeStr(x_time, sizeof(x_time), ctx->request->post_time.sec, ctx->request->post_time.ns)) {
        return 0;  /* error */
    }
    
    /* Define properties */

    if (!sx_Info->host[0]) {
        s_SetHost(AppLog_GetHostName());
    }

    x_pid     = sx_Info->pid        ? sx_Info->pid            : s_GetPID();
    x_rid     = ctx->request->rid   ? ctx->request->rid       : 0;
    x_host    = sx_Info->host[0]    ? sx_Info->host    : UNKNOWN_HOST;
    x_appname = sx_Info->appname[0] ? sx_Info->appname : UNKNOWN_APPNAME;

    /* client */
    if (inside_request  &&  ctx->is_client_set && ctx->client[0]) {
        x_client = ctx->client;
    } else {
        x_client = UNKNOWN_CLIENT;
    }
    /* session */
    if (inside_request  &&  ctx->is_session_set && ctx->session[0]) {
        x_session =  ctx->session;
    } else {
        x_session = UNKNOWN_SESSION;
    }

    n = snprintf(ctx->message, sizeof(ctx->message),
        "%05" APPLOG_UINT8_FORMAT_SPEC "/%03" APPLOG_UINT8_FORMAT_SPEC \
        "/%04" APPLOG_UINT8_FORMAT_SPEC "/%-2s %08X%08X %04" APPLOG_UINT8_FORMAT_SPEC \
        "/%04" APPLOG_UINT8_FORMAT_SPEC " %s %-15s %-15s %-24s %s ",
        x_pid,
        ctx->tid,
        x_rid,
        x_state,
        x_guid_hi, x_guid_lo,
        sx_Info->psn,
        ctx->tsn,
        x_time,
        x_host,
        x_client,
        x_session,
        x_appname
    );
    if (n <= 0) {
        return 0;  /* error */
    }
    return n;
}

static size_t s_PrintParamsPair(char* dst, size_t pos, const char* key, const char* value) {
    size_t len, r_len, w_len;

    if (!key  ||  key[0] == '\0') {
        return pos;
    }
    /* Key */
    len = strlen(key);
    s_URL_Encode(key, len, &r_len, dst + pos, APPLOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;
    if (pos >= APPLOG_ENTRY_MAX-1) {
        return 0;  /* error */
    }
    /* Value */
    dst[pos++] = '=';
    if (!value  ||  value[0] == '\0') {
        return pos;
    }       
    len = strlen(value);
    s_URL_Encode(value, len, &r_len, dst + pos, APPLOG_ENTRY_MAX - pos, &w_len);
    pos += w_len;
    dst[pos] = '\0';

    return pos;
}

static size_t s_PrintParams(char* dst, size_t pos, const SAppLog_Param* params) {
    int/*bool*/ amp = 0;

    /* Walk through the list of arguments */
    if (params) {
        while (params  &&  params->key  &&  pos < APPLOG_ENTRY_MAX) {
            if (params->key[0] != '\0') {
                if (amp) {
                    dst[pos++] = '&';
                } else {
                    amp = 1;
                }
                pos = s_PrintParamsPair(dst, pos, params->key, params->value);
                verify(pos);
            }
            params++;
        }
    }
    dst[pos] = '\0';
    return pos;
}

static size_t s_PrintReqStartExtraParams(TAppLog_Context _ctx, char* dst, size_t pos, int is_subreq) {
    SAppLog_Param ext[6];
    size_t ext_idx = 0;
	char buf[64];

    memset((char*)ext, 0, sizeof(ext));

    /* Add instance name */
    if (sx_Info->instancename[0]) {
        ext[ext_idx].key = "@@instance";
        ext[ext_idx].value = sx_Info->instancename;
        ext_idx++;
    }

    /* Add host role */
    if (!sx_Info->host_role  &&  !sx_Info->remote_logging) {
        sx_Info->host_role = AppLog_GetHostRole();
    }
    if (sx_Info->host_role  &&  sx_Info->host_role[0]) {
        ext[ext_idx].key = "ncbi_role";
        ext[ext_idx].value = sx_Info->host_role;
        ext_idx++;
    }

    /* Add host location */
    if (!sx_Info->host_location  &&  !sx_Info->remote_logging) {
        sx_Info->host_location = AppLog_GetHostLocation();
    }
    if (sx_Info->host_location  &&  sx_Info->host_location[0]) {
        ext[ext_idx].key = "ncbi_location";
        ext[ext_idx].value = sx_Info->host_location;
        ext_idx++;
    }
    
	/* Add parent rid */
	if (is_subreq) {
        ext[ext_idx].key = "@@parent";
		if (_ctx->request > _ctx->requestbuf) {
			snprintf(buf, sizeof(buf), "%" APPLOG_UINT8_FORMAT_SPEC, _ctx->request[-1].rid);
		}
		else {
			snprintf(buf, sizeof(buf), "???");
			verify(0);
		}
        ext[ext_idx].value = buf;
        ext_idx++;
	}
    
    if (ext_idx) {
        pos = s_PrintParams(dst, pos, ext);
    }
    return pos;
}

static size_t s_ReqStart(TAppLog_Context ctx, const STime* time, int is_subreq)
{
    int    n;
    size_t pos;

	verify(sx_Info != NULL);
    verify(sx_Info->state != eAppLog_NotSet);
    s_SetState(ctx, is_subreq ? eAppLog_SubRequestBegin : eAppLog_RequestBegin);

    /* Increase global request number, and save it in the context */
    sx_Info->rid++;
	s_ReserveReqContext(ctx, is_subreq);
    ctx->request->rid = sx_Info->rid;

    if (time && time->sec)
       ctx->request->post_time = *time;

    /* Prefix */
    pos = s_PrintCommonPrefix(ctx);
	verify(pos > 0);
    if (pos <= 0) {
        return 0;
    }
    /* We already have current time in sx_Info->post_time */
    /* Save it into sx_RequestStartTime. */
    ctx->request->req_start_time.sec = ctx->request->post_time.sec;
    ctx->request->req_start_time.ns  = ctx->request->post_time.ns;

    /* TODO: 
        if "params" is NULL, parse and print $ENV{"QUERY_STRING"}  
    */

    /* Event name */
    n = snprintf(ctx->message + pos, sizeof(ctx->message) - pos, "%-13s ", "request-start");
	verify(n > 0);
    if (pos <= 0) {
        return 0;
    }
    pos += n;

    /* Return position in the message buffer */
    return pos;
}

static void s_Post(TAppLog_Context ctx, EAppLog_DiagFile diag) {
#if NCBILOG_USE_FILE_DESCRIPTORS
    int f = -1;
#else
	FILE* f = NULL;
#endif
    int64_t n_write;
#if NCBILOG_USE_FILE_DESCRIPTORS
	int64_t n_written;
    ssize_t n;
#endif

    if (sx_Info->destination == eAppLog_Disable) {
        return;
    }
    s_InitDestination(NULL);
    
    switch (diag) {
        case eDiag_Trace:
            f = sx_Info->file_trace;
            break;
        case eDiag_Err:
            f = sx_Info->file_err;
            break;
        case eDiag_Log:
            f = sx_Info->file_log;
            break;
        case eDiag_Perf:
            f = sx_Info->file_perf;
            break;
        default:
            verify(0);
    }
            
    /* Write */
#if NCBILOG_USE_FILE_DESCRIPTORS
    verify(f != -1);
#else
    verify(f != NULL);
#endif
    
    n_write = strlen(ctx->message);
    verify(n_write <= APPLOG_ENTRY_MAX);
    if (n_write > APPLOG_ENTRY_MAX)
        n_write = APPLOG_ENTRY_MAX;
    ctx->message[n_write] = '\n';
    n_write++;
	ctx->message[n_write] = '\0';

#if NCBILOG_USE_FILE_DESCRIPTORS
	n_written = 0;
	const char* pmessage = ctx->message;
	do {
		n = write(f, pmessage, n_write);
		if (n > 0) {
			pmessage += n;
			n_write -= n;
			n_written += n;
		}
	} while (n <= 0 && errno == EINTR);	
    verify(n_write == 0);
#else
	int  rv = 0;
	do {
		rv = fputs(ctx->message, f);
	}
	while (rv < 0 && errno == EINTR);
	verify(rv > 0);
#endif
    
    /* Increase posting serial numbers */
    sx_Info->psn++;
    ctx->tsn++;
    
    /* Reset posting time if no user time defined */
    if (ctx->request->user_posting_time == FALSE) {
        ctx->request->post_time.sec = 0;
        ctx->request->post_time.ns  = 0;
    }
}

static void s_Extra(TAppLog_Context ctx, const SAppLog_Param* params)
{
    int    n;
    size_t pos;
    char*  buf;

    /* Prefix */
    buf = ctx->message;
    pos = s_PrintCommonPrefix(ctx);
    verify(pos);
    if (pos <= 0) {
        return;
    }
    /* Event name */
    n = snprintf(buf + pos, sizeof(ctx->message) - pos, "%-13s ", "extra");
    verify(n > 0);
    pos += n;
    /* Parameters */
    pos = s_PrintParams(buf, pos, params);
    verify(pos);
    /* Post a message */
    s_Post(ctx, eDiag_Log);
}

static void s_LogHitID(TAppLog_Context ctx, const char* hit_id, unsigned int sub_id)
{
    SAppLog_Param params[2];
	char hit[3*APPLOG_HITID_MAX+1 + 33];
    assert(hit_id  &&  hit_id[0]);

    params[0].key   = "ncbi_phid";
	if (sub_id == 0) {
    params[0].value = hit_id;
	}
	else {
		snprintf(hit, sizeof(hit), "%s.%u", hit_id, sub_id);
		params[0].value = hit;
	}
    params[1].key   = NULL;
    params[1].value = NULL;
    s_Extra(ctx, params);
}

static void s_EnsureHitIDLogged(TAppLog_Context ctx, int is_subreq) {
    if (!ctx->is_phid_logged || is_subreq) {
        if (!is_subreq)
            ctx->is_phid_logged = 1;
        /* log PHID on the {request | sub-request} stop or on the sub-request start.
           If PHID is not set on application or request level, then new PHID
           will be generated and set for the request.
        */
        if (!ctx->phid[0]) {
            char phid_str[APPLOG_HITID_MAX + 1];
            /* Usually we should always have app-wide PHID, but not 
               in the case for ncbi_applog utility */
            /* VERIFY(sx_Info->phid[0]); */
            if (sx_Info->phid[0]  &&  sx_Info->phid_inherit) {
                /* Just log app-wide PHID, do not set it as request specific
                   to allow AppLog_GetNextSubHitID() work correctly and
                   use app-wide sub hits.
                */
                s_LogHitID(ctx, sx_Info->phid, sx_Info->phid_sub_id);
                return;
            }
            /* Generate and set new request-specific PHID */
            s_SetHitID(ctx->phid, sizeof(ctx->phid), s_GenerateHitID_Str(ctx, phid_str, sizeof(phid_str) - 1));
        }
        s_LogHitID(ctx, ctx->phid, ctx->phid_sub_id);
    }
}

static const char* AppLogP_GetHitID_Env(void) {
    static const char* phid = NULL;
    const char* p = NULL;

    if ( phid ) {
        return phid;
    }
    if ( (p = getenv("HTTP_NCBI_PHID")) != NULL  &&  *p ) {
        phid = strdup(p);
        return phid;
    }       
    if ( (p = getenv("NCBI_LOG_HIT_ID")) != NULL  &&  *p ) {
        phid = strdup(p);
        return phid;
    }
    return NULL;
}

static void s_Sanitize
    (const void* src_buf,    /* [in]     non-NULL */
     size_t      src_size,   /* [in]              */
     size_t*     src_read,   /* [out]    non-NULL */
     void*       dst_buf,    /* [in/out] non-NULL */
     size_t      dst_size,   /* [in]              */
     size_t*     dst_written /* [out]    non-NULL */
     )
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size  ||  !dst  ||  !src)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
        (*src_read)++, src++, dst++)
    {
        unsigned char c = *src;
        char subst = 0;

        switch (c) {
            case '\n':
                subst = 'n'; break;
            case '\r':
                subst = 'r'; break;
            case '\t':
                subst = 't'; break;
            case '\v':
                subst = 'v'; break;
            case '\b':
                subst = 'b'; break;
            case '\f':
                subst = 'f'; break;
            case '\a':
                subst = 'a'; break;
        }
        if ( subst ) {
            if (*dst_written + 2 > dst_size) {
                break;
            }
            *dst = '\\';
            *(++dst) = subst;
            *dst_written += 2;
        } else if (isprint(c)) {
            *dst = c;
            *dst_written += 1;
        } else {
            /* convert to octal string */
            unsigned char v;
            if (*dst_written + 4 > dst_size) {
                break;
            }
            *dst = '\\';
            v = c >> 6;
            *(++dst) = '0' + v;
            v = (c >> 3) & 7;
            *(++dst) = '0' + v;
            v = c & 7;
            *(++dst) = '0' + v;
            *dst_written += 4;
        }
    }
}

/* Severity string representation  */
static const char* sx_SeverityStr[] = {
    "Trace", "Info", "Warning", "Error", "Critical", "Fatal"
};

static void s_PrintMessage(TAppLog_Context ctx, EAppLog_Severity severity, const char* msg) {
    int               n;
    size_t            pos, r_len, w_len;
    char*             buf;
    EAppLog_DiagFile diag = eDiag_Trace;

    switch (severity) {
        case eAppLog_Trace:
        case eAppLog_Info:
            diag = eDiag_Trace;
            break;
        case eAppLog_Warning:
        case eAppLog_Error:
        case eAppLog_Critical:
        case eAppLog_Fatal:
            diag = eDiag_Err;
            break;
        default:
            verify(0);
    }
    /* Check minimum allowed posting level */
    if (severity < sx_Info->post_level) {
        return;
    }
    
    verify(sx_Info->state != eAppLog_NotSet);

    /* Prefix */
    buf = ctx->message;
    pos = s_PrintCommonPrefix(ctx);
    verify(pos);
    if (pos <= 0) {
        return;
    }

    /* Severity */
    n = snprintf(buf + pos, sizeof(ctx->message) - pos, "%s: ", sx_SeverityStr[severity]);
    verify(n > 0);
    pos += n;
    /* Message */
    s_Sanitize(msg, strlen(msg), &r_len, buf + pos, sizeof(ctx->message) - pos, &w_len);
    pos += w_len;
    buf[pos] = '\0';

    /* Post a message */
    s_Post(ctx, diag);

    if (severity == eAppLog_Fatal) {
        abort(); /* no additional error reporting */
    }
}

static void s_AppStart(TAppLog_Context ctx, const char* argv[]) {
    int    i, n;
    size_t pos;
    char*  buf;

    /* Try to get app-wide hit ID from environment */
    if (!sx_Info->phid[0]) {
        const char* ev = AppLogP_GetHitID_Env();
        if (ev) {
            s_SetHitID(sx_Info->phid, sizeof(sx_Info->phid), ev);
            sx_Info->phid_inherit = 1;
        } else {
            /* Auto-generate new PHID (not-inherited by requests) */
            char phid_str[APPLOG_HITID_MAX + 1];
            s_SetHitID(sx_Info->phid, sizeof(sx_Info->phid), s_GenerateHitID_Str(ctx, phid_str,  sizeof(phid_str) - 1));
        }
    }
    
    s_SetState(ctx, eAppLog_AppBegin);
    /* Prefix */
    buf = ctx->message;
	s_ReserveReqContext(ctx, 0);

    pos = s_PrintCommonPrefix(ctx);
    verify(pos > 0);
    if (pos <= 0) {
        return;
    }
    /* We already have current time in sx_Info->post_time */
    /* Save it into sx_AppStartTime. */
    sx_Info->app_start_time.sec = ctx->request->post_time.sec;
    sx_Info->app_start_time.ns  = ctx->request->post_time.ns;

    /* Event name */
    n = snprintf(buf + pos, sizeof(ctx->message) - pos,  "%-13s", "start");
    verify(n > 0);
    pos += n;
    /* Walk through list of arguments */
    if (argv) {
        for (i = 0; argv[i] != NULL; ++i) {
            n = snprintf(buf + pos, sizeof(ctx->message) - pos, " %s", argv[i]);
            verify(n > 0);
            pos += n;
        }
    }
    /* Post a message */
    s_Post(ctx, eDiag_Log);
}

typedef enum {
    LOG_ARG_VOID = -1,
    LOG_ARG_SESSION_ID = 0,
    LOG_ARG_CLIENT_IP,
    LOG_ARG_CLIENT_NAME,
    LOG_ARG_NCBI_PHID,
    LOG_ARG_OSG_REQ_ID,
    LOG_ARG_SPID,
        LOG_ARG_TOTAL
} LOG_ARG_ENUM;

#define MATCH(what, len, needle) \
       (((len) == sizeof((needle)) - 1) && (strncmp((what), (needle), (len)) == 0))

static int s_ArgIdFromStr(const char* str, size_t len) {
    int arg_id = LOG_ARG_VOID;
    if (MATCH(str, len, "session_id"))
        arg_id = LOG_ARG_SESSION_ID;
    else if (MATCH(str, len, "client_ip"))
        arg_id = LOG_ARG_CLIENT_IP;
    else if (MATCH(str, len, "osg_client_name"))
        arg_id = LOG_ARG_CLIENT_NAME;
    else if (MATCH(str, len, "ncbi_phid"))
        arg_id = LOG_ARG_NCBI_PHID;
    else if (MATCH(str, len, "osg_request_id"))
        arg_id = LOG_ARG_OSG_REQ_ID;
    else if (MATCH(str, len, "spid"))
        arg_id = LOG_ARG_SPID;
    return arg_id;
}

/* ============== A P I ===================*/

static void AppLog_Init(const char* appname) {
    if (sx_IsInit) {
        /* AppLog_Init() is already in progress or done */
        /* This function can be called only once         */
        assert(!sx_IsInit);
        /* error */
        return;
    }
    /* Start initialization */
    /* The race condition is possible here if AppLog_Init() is simultaneously
       called from several threads. But AppLog_Init() should be called
       only once, so we can leave this to user's discretion.
    */
    sx_IsInit = 1;
    s_Init(appname);
}

void AppLog_SetSession(void* ctx, const char* session) {
	TAppLog_Context _ctx = (TAppLog_Context) ctx;
    s_SetSession(_ctx->session, sizeof(_ctx->session), session);
    _ctx->is_session_set = 1;
}

void AppLog_SetInstance(const char* instance) {
    s_SetInstance(sx_Info->instancename, sizeof(sx_Info->instancename), instance);
}

void AppLog_SetPhid(void* ctx, const char* phid) {
    TAppLog_Context _ctx = (TAppLog_Context) ctx;
    if (s_IsValidHitID(phid))
        s_SetHitID(_ctx->phid, sizeof(_ctx->phid), phid);
}

void AppLog_SetClient(void* ctx, const char* client) {
    TAppLog_Context _ctx = (TAppLog_Context) ctx;
    s_SetClient(_ctx->client, sizeof(_ctx->client), client);
    _ctx->is_client_set = 1;
}

static void AppLog_ReqExtra(void* ctx, const SAppLog_Param* params) {
	TAppLog_Context _ctx = (TAppLog_Context) ctx;
	s_Extra(_ctx, params);
}

static void AppLog_ReqStart(void* ctx, const STime* time, const SAppLog_Param* params, int is_subreq) {
    TAppLog_Context _ctx = (TAppLog_Context) ctx;
    size_t prev, pos = 0;

    if (is_subreq) {
        /* Make sure HitID for the main request has logged */
        s_EnsureHitIDLogged(_ctx, 0);
    }
    /* Common request info */
    pos = s_ReqStart(_ctx, time, is_subreq);
    verify(pos);
    
    /* Print host role/location -- add it before users parameters */
    prev = pos;
    pos = s_PrintReqStartExtraParams(_ctx, _ctx->message, pos, is_subreq);
    verify(pos);
    if (prev != pos  &&  params  &&  params->key  &&  params->key[0]  &&  pos < APPLOG_ENTRY_MAX) {
        _ctx->message[pos++] = '&';
    }
    
    /* User request parameters */
    pos = s_PrintParams(_ctx->message, pos, params);
    verify(pos);

    /* Post a message */
    s_Post(_ctx, eDiag_Log);
}

static void AppLog_ReqRun(void* ctx,  int is_subreq) {
    TAppLog_Context _ctx = (TAppLog_Context) ctx;

    verify(sx_Info->state != eAppLog_NotSet);
    s_SetState(_ctx, is_subreq ? eAppLog_SubRequest : eAppLog_Request);

	if (is_subreq) {
		_ctx->phid_sub_id++;
	}
}

static void AppLog_ReqStop(void *ctx, const STime* time, int status, size_t bytes_rd, size_t bytes_wr, int is_subreq) {
    TAppLog_Context _ctx = (TAppLog_Context) ctx;
    int    n;
    size_t pos;
    double timespan;

    verify(sx_Info->state != eAppLog_NotSet);
    s_EnsureHitIDLogged(_ctx, is_subreq);
    s_SetState(_ctx, is_subreq ? eAppLog_SubRequestEnd : eAppLog_RequestEnd);

    if (time && time->sec)
       _ctx->request->post_time = *time;
    
    /* Prefix */
    pos = s_PrintCommonPrefix(_ctx);
    verify(pos);
    if (pos <= 0) {
        return;
    }
    /* We already have current time in sx_Info->post_time */
    timespan = s_DiffTime(_ctx->request->req_start_time, _ctx->request->post_time);
    n = snprintf(_ctx->message + pos, sizeof(_ctx->message) - pos, "%-13s %d %.6f %lu %lu",
                "request-stop", status, timespan,
                (unsigned long)bytes_rd, (unsigned long)bytes_wr);
    verify(n > 0);
    /* Post a message */
    s_Post(_ctx, eDiag_Log);
    /* Reset state */
    s_SetState(_ctx, is_subreq ? eAppLog_Request : eAppLog_AppRun);

    /* Reset request, client, session and hit ID */
    _ctx->request->rid = 0;
	s_ReleaseReqContext(_ctx, is_subreq);
	if (!is_subreq) {
	    _ctx->client[0]  = '\0';  _ctx->is_client_set  = 0;
	    _ctx->session[0] = '\0';  _ctx->is_session_set = 0;
	    _ctx->phid[0]    = '\0';  _ctx->phid_sub_id = 0;
        _ctx->is_phid_logged = 0;
	}
}

static EAppLog_Destination AppLog_SetDestination(EAppLog_Destination ds) {
    char* logfile = NULL;

    /* Close current destination */
    s_CloseLogFiles(1 /*force cleanup*/);

    /* Set new destination */
    sx_Info->destination = ds;
    if (sx_Info->destination != eAppLog_Disable) {
        /* and force to initialize it */
        sx_Info->last_reopen_time = 0;
        if (sx_Info->destination == eAppLog_Default) {
            /* Special case to redirect default logging output */
            logfile = getenv("NCBI_CONFIG__LOG__FILE");
            if (logfile) {
                if (!*logfile) {
                    logfile = NULL;
                } else {
                    if (strcmp(logfile, "-") == 0) {
                        sx_Info->destination = eAppLog_Stderr;
                        logfile = NULL;
                    }
                }
            }
        }
        s_InitDestination(logfile);
    }
    ds = sx_Info->destination;
    return ds;
}

static void AppLog_AppStart(void* ctx, const char* argv[]) {
    TAppLog_Context _ctx = (TAppLog_Context) ctx;
    s_AppStart(_ctx, argv);
}

static EAppLog_Severity AppLog_SetPostLevel(EAppLog_Severity sev) {
    EAppLog_Severity prev;
    prev = sx_Info->post_level;
    sx_Info->post_level = sev;
    return prev;
}

static void AppLog_AppRun(void* ctx) {
    TAppLog_Context _ctx = (TAppLog_Context) ctx;
            
    verify(sx_Info->state != eAppLog_NotSet);
    s_SetState(_ctx, eAppLog_AppRun);
    
    verify(sx_Info->phid[0]);
    s_LogHitID(_ctx, sx_Info->phid, sx_Info->phid_sub_id);
    
}

static void AppLog_AppStopSignal(void* ctx, int exit_status, int exit_signal) {
    TAppLog_Context _ctx = (TAppLog_Context) ctx;
    int    n;
    size_t pos;
    double timespan;

    verify(sx_Info->state != eAppLog_NotSet);
    s_SetState(_ctx, eAppLog_AppEnd);
    /* Prefix */
    pos = s_PrintCommonPrefix(_ctx);
    verify(pos);
    if (pos <= 0) {
        return;
    }
    /* We already have current time in _ctx->post_time */
	if (_ctx->request)
		timespan = s_DiffTime(sx_Info->app_start_time, _ctx->request->post_time);
	else
		timespan = 0;
    if ( exit_signal ) {
        n = snprintf(_ctx->message + pos, sizeof(_ctx->message) - pos, "%-13s %d %.3f SIG=%d",
                    "stop", exit_status, timespan, exit_signal);
    } else {
        n = snprintf(_ctx->message + pos, sizeof(_ctx->message) - pos, "%-13s %d %.3f",
                    "stop", exit_status, timespan);
    }
    verify(n > 0);
    /* Post a message */
    s_Post(_ctx, eDiag_Log);
}

static void AppLog_AppStop(void* ctx, int exit_status) {
    AppLog_AppStopSignal(ctx, exit_status, 0);
}

static void AppLog_Destroy(void *ctx) {
	TAppLog_Context _ctx = (TAppLog_Context) ctx;
    sx_IsEnabled = 0;
    sx_IsInit    = 2; /* still TRUE to prevent recurring initialization, but not default 1 */

    /* Close files */
    s_CloseLogFiles(1 /*force cleanup*/);

    /* Free memory */
    if (sx_Info->app_full_name) {
        free(sx_Info->app_full_name);
		sx_Info->app_full_name = NULL;
    }
    if (sx_Info->app_base_name) {
        free(sx_Info->app_base_name);
		sx_Info->app_base_name = NULL;
    }
    if (sx_Info) {
        free(sx_Info);
        sx_Info = NULL;
    }
    /* Destroy thread-specific context and TLS */
    s_DestroyContext(_ctx);
    if (sx_TlsIsInit) {
        s_TlsDestroy();
    }
}

static void AppLog_Trace(void* ctx, const char* msg) {
	TAppLog_Context _ctx = (TAppLog_Context) ctx;
    s_PrintMessage(_ctx, eAppLog_Trace, msg);
}
        
static void AppLog_Info(void* ctx, const char* msg) {
	TAppLog_Context _ctx = (TAppLog_Context) ctx;
    s_PrintMessage(_ctx, eAppLog_Info, msg);
}
            
static void AppLog_Warning(void* ctx, const char* msg) {
	TAppLog_Context _ctx = (TAppLog_Context) ctx;
    s_PrintMessage(_ctx, eAppLog_Warning, msg);
}
    
static void AppLog_Error(void* ctx, const char* msg) {
	TAppLog_Context _ctx = (TAppLog_Context) ctx;
    s_PrintMessage(_ctx, eAppLog_Error, msg);
}

static int64_t gettime(void) {
	int cnt = 100;
#ifdef __MACH__
	struct timeval tv = {0};
	while (gettimeofday(&tv, NULL) != 0 && cnt-- > 0);
	return (int64_t)tv.tv_usec + ((int64_t)tv.tv_sec) * 1000000L;
#else
	struct timespec tv = {0};
	while (clock_gettime(CLOCK_MONOTONIC, &tv) != 0 && cnt-- > 0);
	return (int64_t)tv.tv_nsec / 1000 + ((int64_t)tv.tv_sec) * 1000000L;
#endif
}

/* ======================== Q U E U E ============================ */

#if defined(QSYNC_CONDVAR)
static int qsync_object_create(qsync_object_t* obj) {
	
	pthread_cond_init(&obj->cond, NULL);
	pthread_mutex_init(&obj->mutex, NULL);
	return 0;
}
#elif defined(QSYNC_FUTEX)
static int qsync_object_create(qsync_object_t* obj) {
	return 0;
}
#elif defined(QSYNC_WIN32)
#endif

#if defined(QSYNC_CONDVAR)
void qsync_object_destroy(qsync_object_t* obj) {
	pthread_mutex_destroy(&obj->mutex);
	pthread_cond_destroy(&obj->cond);
}
#elif defined(QSYNC_FUTEX)
void qsync_object_destroy(qsync_object_t* obj) {}
#elif defined(QSYNC_WIN32)
#endif

#if defined(QSYNC_CONDVAR)
int qsync_object_wait(qsync_object_t* obj, atomic_t* counter, int saved_val, unsigned long timeout_mks) {
	if (atomic_get(counter) == saved_val) {
		int rv;
		struct timespec abstime;
		unsigned long t = gettime() + timeout_mks;
		abstime.tv_sec = t / 1000000L;
		abstime.tv_nsec = (t % 1000000L) * 1000L;
		pthread_mutex_lock(&obj->mutex);
		rv = pthread_cond_timedwait(&obj->cond, &obj->mutex, &abstime);
		pthread_mutex_unlock(&obj->mutex);
		return rv;
	}
	else
		return 0;
}
#elif defined(QSYNC_FUTEX)
int qsync_object_wait(qsync_object_t* obj, atomic_t* counter, int saved_val, unsigned long timeout_mks) {
	struct timespec timeout;
	timeout.tv_sec = timeout_mks / 1000000L;
	timeout.tv_nsec = (timeout_mks % 1000000L) * 1000L;
	return  srvutils_do_futex(counter, FUTEX_WAIT, saved_val, &timeout);
}
#elif defined(QSYNC_WIN32)
#endif

#if defined(QSYNC_CONDVAR)
int qsync_object_wake(qsync_object_t* obj, atomic_t* counter, int numberof_threads_towake) {
	int rv;
	if (numberof_threads_towake <= 1) {
		rv = pthread_cond_signal(&obj->cond);
	}
	else {
		rv = pthread_cond_broadcast(&obj->cond);
	}
	return rv;
}
#elif defined(QSYNC_FUTEX)
int qsync_object_wake(qsync_object_t* obj, atomic_t* counter, int numberof_threads_towake) {
	return  srvutils_do_futex(counter, FUTEX_WAKE, numberof_threads_towake, NULL);
}
#elif defined(QSYNC_WIN32)
#endif


static void log_queue_create(log_queue_t* queue, int size) {
	memset(queue, 0, sizeof(log_queue_t));
	int bit = 1;
	int i;
	while ((size & (size - 1)) != 0) {
		int m = (1 << bit) - 1;
		bit++;
		size = size & (~m);
	}
	queue->m_size = size;
	queue->m_buffer = (log_queue_cell_t*)malloc(size * sizeof(log_queue_cell_t));
	memset(queue->m_buffer, 0, size * sizeof(log_queue_cell_t));
	qsync_object_create(&queue->m_waiting_push_obj);
	qsync_object_create(&queue->m_waiting_pop_obj);
	atomic_set(&queue->m_waiting_push_ev, 0);
	atomic_set(&queue->m_waiting_pop_ev, 0);
	atomic_set(&queue->m_waiting_push_cnt, 0);
	atomic_set(&queue->m_waiting_pop_cnt, 0);
	for (i = 0; i < size; i++) {
		atomic_set(&(queue->m_buffer[i].m_sequence), i);
	}
}

static void log_queue_destroy(log_queue_t* queue) {
	qsync_object_destroy(&queue->m_waiting_push_obj);
	qsync_object_destroy(&queue->m_waiting_pop_obj);
	if (queue->m_buffer) {
		free(queue->m_buffer);
		queue->m_buffer = NULL;
	}
}

static int/*bool*/
log_queue_push(log_queue_t* queue, const log_queue_data_t* data) {
	log_queue_cell_t* cell;
	int pop_waiters = atomic_get(&queue->m_waiting_pop_cnt);
	int pos = atomic_get(&queue->m_push_pos);
	for (;;) {
		cell = &queue->m_buffer[pos & (queue->m_size - 1)];
		int seq = atomic_get(&cell->m_sequence);
		int diff = seq - pos;
		if (diff == 0) {
			if (cmpxchg(&queue->m_push_pos, pos, pos + 1)) 
				break;
		}
		else if (diff < 0)
			return FALSE;
		else
			pos = atomic_get(&queue->m_push_pos);
	}
	cell->m_data = *data;
	atomic_set(&cell->m_sequence, pos + 1);
	if (pop_waiters > 0 || (pop_waiters = atomic_get(&queue->m_waiting_pop_cnt)) > 0) {
		atomic_inc(&queue->m_waiting_pop_ev);
		qsync_object_wake(&queue->m_waiting_pop_obj, &queue->m_waiting_pop_ev, pop_waiters);
	}
	return TRUE;
}

#if WAITABLE_PUSH
static int/*bool*/
log_queue_push_wait(log_queue_t* queue, unsigned long timeout_mks, const log_queue_data_t* data) {
	int sav_queue_ev = atomic_get(&queue->m_waiting_push_ev); /* fetch before push */
	int rv = log_queue_push(queue, data);
	if (rv != TRUE && timeout != NULL) {
		while (1) {
			if (timeout != NULL) {
				atomic_inc(&queue->m_waiting_push_cnt);
//				int r = srvutils_do_futex(&(queue->m_waiting_push_ev), FUTEX_WAIT, sav_queue_ev, timeout);
				int r = qsync_object_wait(&queue->m_waiting_push_obj, &queue->m_waiting_push_ev, sav_queue_ev, timeout_mks);
				atomic_dec(&queue->m_waiting_push_cnt);
				if (r == ETIMEDOUT)
					timeout = NULL; /* waited => no more wait */
				else if (r < 0) /* error */
					break;
			}
			sav_queue_ev = atomic_get(&queue->m_waiting_push_ev); /* fetch before push */
			rv = log_queue_push(queue, data);
			if (rv == TRUE || timeout == NULL)
				break;
		}
	}
	return rv;
}
#endif /*WAITABLE_PUSH*/

static /*bool*/int
log_queue_pop(log_queue_t* queue, log_queue_data_t* data) {
	log_queue_cell_t* cell;
	int push_waiters = atomic_get(&queue->m_waiting_push_cnt);
	int pos = atomic_get(&queue->m_pop_pos);
	for (;;) {
		cell = &queue->m_buffer[pos & (queue->m_size - 1)];
		size_t seq = atomic_get(&cell->m_sequence);
		intptr_t diff = seq - (pos + 1);
		if (diff == 0) {
			if (cmpxchg(&queue->m_pop_pos, pos, pos + 1)) {
				break;
			}
		}
		else if (diff < 0)
			return FALSE;
		else
			pos = atomic_get(&queue->m_pop_pos);
	}
	*data = cell->m_data;
	atomic_set(&cell->m_sequence, pos + queue->m_size);
	if (push_waiters > 0 || (push_waiters = atomic_get(&queue->m_waiting_push_cnt)) > 0) {
		atomic_inc(&queue->m_waiting_push_ev);
		qsync_object_wake(&queue->m_waiting_push_obj, &queue->m_waiting_push_ev, push_waiters);
	}
	return TRUE;
}

static /*bool*/int
log_queue_pop_wait(log_queue_t* queue, unsigned long timeout_mks, log_queue_data_t* data) {
	int sav_queue_ev = atomic_get(&queue->m_waiting_pop_ev); /* fetch before pop */
	int rv = log_queue_pop(queue, data);
	if (rv != TRUE && timeout_mks != 0) {
		while (1) {
			if (timeout_mks != 0) {
				atomic_inc(&queue->m_waiting_pop_cnt);
//				int r = srvutils_do_futex(&(queue->m_waiting_pop_ev), FUTEX_WAIT, sav_queue_ev, timeout);
				int r = qsync_object_wait(&queue->m_waiting_pop_obj, &queue->m_waiting_pop_ev, sav_queue_ev, timeout_mks);
				atomic_dec(&queue->m_waiting_pop_cnt);
				if (r == ETIMEDOUT)
					timeout_mks = 0; /* waited => no more wait */
				else if (r < 0) /* error */
					break;
			}
			sav_queue_ev = atomic_get(&queue->m_waiting_pop_ev); /* fetch before pop */
			rv = log_queue_pop(queue, data);
			if (rv == TRUE || timeout_mks == 0)
				break;
		}
	}
	return rv;
}

typedef struct CLogArgs_tag {
    char m_arg_list[LOG_ARG_TOTAL][LOG_MAX_VALUE_LEN];
} CLogArgs;

void CLogArgs_CLogArgs(CLogArgs* _this, const char* log_args, int log_args_len) {
	int key_start = 0;
	int key_len   = 0;
	int ii;

	for (ii = 0; ii < LOG_ARG_TOTAL; ii++)
		_this->m_arg_list[ii][0] = 0;

	int done = FALSE;
	for (ii = 0; !done; ii++) {
		done = (ii >= log_args_len);
		if (!key_len && !done) {
			switch (log_args[ii]) {
				case '=':
					key_len = ii - key_start;
					break;
				case '&':
					key_start = ii + 1;
					key_len = 0;
					break;
				default:
					break;
			}
		}
		else if (log_args[ii] == '&' || done) {
            int arg_id;
			if (key_len <= 0) {
				key_start = ii + 1; key_len = 0;
				continue;
			}
            arg_id = s_ArgIdFromStr(log_args + key_start, key_len);

			int val_start = key_start + key_len + 1;
			int val_len = ii - val_start;
			key_start = ii + 1; key_len = 0;

			if ((arg_id == LOG_ARG_VOID) || (val_len <= 0)) continue;

			strncpy(_this->m_arg_list[arg_id], log_args + val_start, val_len);
			_this->m_arg_list[arg_id][val_len] = 0;
		}
	}
}
/*bool*/int CLogArgs_IsSet(CLogArgs* _this, LOG_ARG_ENUM arg_id) {
	if ((arg_id < 0) || (arg_id >= LOG_ARG_TOTAL)) 
		return FALSE;
	return (_this->m_arg_list[arg_id][0] != '\0');
}
char* CLogArgs_Get(CLogArgs* _this, LOG_ARG_ENUM arg_id) {
	if ((arg_id < 0) || (arg_id >= LOG_ARG_TOTAL)) 
		return NULL;
	return _this->m_arg_list[arg_id];
}

#define MAX_PARAM_NUM 256

static void log_parse_extra_request(void* ctx, char* arg_str) {
    SAppLog_Param params[ MAX_PARAM_NUM];
    int key_start = 0;
    int key_len   = 0;
    int ic, ip;
	int done = FALSE;

	memset(params, 0, sizeof(params));
    for (ic = 0, ip = 0; !done; ic++) {
        if (!arg_str[ic]) 
			done = TRUE;
        if (!key_len && !done) {
            switch (arg_str[ic])
            {
            case '=': key_len = ic - key_start;         break;
            case '&': key_start = ic + 1;  key_len = 0; break;
            default: break;
            }
            continue;
        }
        else if ((0xFF & arg_str[ic]) == ('&' | 0x80)) { 
			arg_str[ic] = '&'; 
			continue;
		}
        else if (arg_str[ic] != '&' && !done) {
			continue;
		}
        
        if (key_len <= 0) {
            key_start = ic + 1; 
			key_len = 0;
			if (!done)
				continue;
        }

		if (key_len > 0) {
            int arg_id;
			params[ip].key = arg_str + key_start;
			arg_str[key_start + key_len] = 0;

			params[ip].value = arg_str + key_start + key_len + 1;
			arg_str[ic] = 0;

            arg_id = s_ArgIdFromStr(params[ip].key, key_len);
            switch (arg_id) {
                case LOG_ARG_SESSION_ID:
                    AppLog_SetSession(ctx, params[ip].value);
                    break;
                case LOG_ARG_CLIENT_IP:
                    AppLog_SetClient(ctx, params[ip].value);
                    break;
                case LOG_ARG_NCBI_PHID:
                    AppLog_SetPhid(ctx, params[ip].value);
                    break;
                default:
                    ip++;
            }

			if (ip >= MAX_PARAM_NUM) break;
		}
        key_start = ic + 1; 
		key_len = 0;
    }
    
	if (ip >= MAX_PARAM_NUM) 
		ip = MAX_PARAM_NUM - 1;
    params[ip].key = params[ip].value = NULL;
    AppLog_ReqExtra(ctx, params);
}

static void log_parse_start_request(void* ctx, const STime* time, char* arg_str, int is_subreq) {
    SAppLog_Param params[ MAX_PARAM_NUM];
    int key_start = 0;
    int key_len   = 0;
    int ic, ip;
    const char client_ip_str[]       = "client_ip";
    const char client_name_str[]     = "client_name";
    const char port_str[]            = "port";
    const char osg_client_name_str[] = "osg_client_name";
//    const char ncbi_phid_str[]       = "ncbi_phid";
    const char osg_request_id_str[]  = "osg_request_id";
    const char spid_str[]            = "spid";
	int session_set = 0;
    int done = FALSE;
    
	memset(params, 0, sizeof(params));
    for (ic = 0, ip = 0; !done; ic++) {
        if (!arg_str[ic]) 
			done = TRUE;
        if (!key_len && !done) {
            switch (arg_str[ic])
            {
            case '=': key_len = ic - key_start;         break;
            case '&': key_start = ic + 1;  key_len = 0; break;
            default: break;
            }
            continue;
        }
        else if ((0xFF & arg_str[ic]) == ('&' | 0x80)) { 
			arg_str[ic] = '&'; 
			continue;
		}
        else if (arg_str[ic] != '&' && !done) {
			continue;
		}
        
        if (key_len <= 0) {
            key_start = ic + 1; 
			key_len = 0;
			if (!done)
				continue;
        }

		if (key_len > 0) {
			params[ip].key = arg_str + key_start;
			arg_str[key_start + key_len] = 0;

			params[ip].value = arg_str + key_start + key_len + 1;
			arg_str[ic] = 0;

			if (!strcmp(params[ip].key, "@log_arg")) {
			  /* ..., @log_arg='session_id=bla-bla&client_ip=127.0.0.1') */
				const char* log_arg = params[ip].value;
				ip--;
				CLogArgs LogArgs1;
				CLogArgs_CLogArgs(&LogArgs1, log_arg, strlen(log_arg));
				if (CLogArgs_IsSet(&LogArgs1, LOG_ARG_SESSION_ID))
				{
					AppLog_SetSession(ctx, CLogArgs_Get(&LogArgs1, LOG_ARG_SESSION_ID));
					session_set = 1;
				}
				if (CLogArgs_IsSet(&LogArgs1, LOG_ARG_CLIENT_IP))
				{
					ip++;
					params[ip].key = client_ip_str;
					params[ip].value = CLogArgs_Get(&LogArgs1, LOG_ARG_CLIENT_IP);
				}
				if (CLogArgs_IsSet(&LogArgs1, LOG_ARG_CLIENT_NAME))
				{
					ip++;
					params[ip].key = osg_client_name_str;
					params[ip].value = CLogArgs_Get(&LogArgs1, LOG_ARG_CLIENT_NAME);
				}
				if (CLogArgs_IsSet(&LogArgs1, LOG_ARG_NCBI_PHID))
				{
/*
                    ip++;
					params[ip].key = ncbi_phid_str;
					params[ip].value = CLogArgs_Get(&LogArgs1, LOG_ARG_NCBI_PHID);
*/
                    AppLog_SetPhid(ctx, CLogArgs_Get(&LogArgs1, LOG_ARG_NCBI_PHID));
				}
				if (CLogArgs_IsSet(&LogArgs1, LOG_ARG_OSG_REQ_ID))
				{
					ip++;
					params[ip].key = osg_request_id_str;
					params[ip].value = CLogArgs_Get(&LogArgs1, LOG_ARG_OSG_REQ_ID);
				}
				if (CLogArgs_IsSet(&LogArgs1, LOG_ARG_SPID))
				{
					ip++;
					params[ip].key = spid_str;
					params[ip].value = CLogArgs_Get(&LogArgs1, LOG_ARG_SPID);
				}
			}
			else if (!strcmp(params[ip].key, "@@client_ip")) {
				AppLog_SetClient(ctx, params[ip].value);
				ip--;
			}
			else if (!strcmp(params[ip].key, "@@client_name")) {
				params[ip].key = client_name_str;
			}
			else if (!strcmp(params[ip].key, "@@port")) {
				params[ip].key = port_str;
			}
			
			ip++;
			
			if (ip >= MAX_PARAM_NUM) break;
		}
        key_start = ic + 1; 
		key_len = 0;
    }
    
	if (ip >= MAX_PARAM_NUM) 
		ip = MAX_PARAM_NUM - 1;
    params[ip].key = params[ip].value = NULL;
      
	if (!session_set && !is_subreq)
		AppLog_SetSession(ctx, "");
    AppLog_ReqStart(ctx, time, params, is_subreq);
	AppLog_ReqRun(ctx, is_subreq);
}

static int ExtractNumber(int *Num, const char* Str, int From, int *Next)
{
    int Sign = 1;
    while (1) {
        if (Str[From] == '\0') { 
			*Next = From;
			return FALSE;
		}
        if (Str[From] == '-') {
			Sign = -Sign;
			From++;
			if ((Str[From] < '0') || (Str[From] > '9')) {
				*Next = From;
				return FALSE;
			}
			break;
		}
        if ((Str[From] >= '0') && (Str[From] <= '9')) 
			break;
		From++;
    }

    *Num = 0;

    for (; Str[From] != '\0'; ++From) {
		int d;
        if ((Str[From] < '0') || (Str[From] > '9')) break;
		if ((*Num) > (INT_MAX / 10))
			break;
        (*Num) = (*Num) * 10;
		d = (Str[From] - '0');
		if ((*Num) > INT_MAX - d)
			break;
		*Num += d;
    }

    *Next = From;
    *Num *= Sign;
    return TRUE;
}

static int ExtractLongNumber(long *Num, const char* Str, int From, int *Next) {
    int Sign = 1;
    while (1) {
        if (Str[From] == '\0') { 
			*Next = From;
			return FALSE;
		}
        if (Str[From] == '-') {
			Sign = -Sign;
			From++;
			if ((Str[From] < '0') || (Str[From] > '9')) {
				*Next = From;
				return FALSE;
			}
		}
        if ((Str[From] >= '0') && (Str[From] <= '9')) 
			break;
		From++;
    }

    *Num = 0;

    for (; Str[From] != '\0'; ++From) {
		int d;
        if ((Str[From] < '0') || (Str[From] > '9')) break;
		if ((*Num) > (LONG_MAX / 10))
			break;
        (*Num) = (*Num) * 10;
		d = (Str[From] - '0');
		if ((*Num) > LONG_MAX - d)
			break;
		*Num += d;
    }

    *Next = From;
    *Num *= Sign;
    return TRUE;
}


static void log_parse_end_request(void* ctx, const STime* time, char* arg_str, int is_subreq) {
    int status = 500;
    long bytes_rd = 0;
    long bytes_wr = 0;
    int next = 0;
    if (ExtractNumber(&status, arg_str, next, &next) &&
		ExtractLongNumber(&bytes_rd, arg_str, next, &next) &&
		ExtractLongNumber(&bytes_wr, arg_str, next, &next))
	{
		AppLog_ReqStop(ctx, time, status, bytes_rd, bytes_wr, is_subreq);
	}
	else {
		AppLog_ReqStop(ctx, time, 599, 0, 0, is_subreq);
	}
}

int/*bool*/ LogQueuePeek() {
	log_queue_data_t data;
	unsigned long timeout_mks = 500000; // 500ms
	int rv = 0;
	int needbreak = 0;
	if (!sx_log_destroyed) {
		if (!sx_log_created) {
			sleep(1);
		}
		if (!sx_log_created)
			return 0;
		int64_t startt = gettime();
		int cnt_sincelastcheck = 0;
		while (!needbreak) {
			if (!log_queue_pop_wait(&sx_Queue, timeout_mks, &data)) {
				sched_yield();
				if ((gettime() - startt) > MAX_MS_IN_LOG * 1000L) {
					s_InitDestination(NULL);
					break;
				}
				else 
					continue;
			}
			char* msg = data.msg ? data.msg : (data.lmsg[0] ? data.lmsg : NULL);
			rv++;
			switch (data.kind) {
				case lkNone:
					AppLog_SetPostLevel(sx_Severity);
					AppLog_SetDestination(sx_Destination);
					break;
				case lkTrace:
					if (msg)
						AppLog_Trace(data.ctx, msg);
					break;
				case lkInfo:
					if (msg)
						AppLog_Info(data.ctx, msg);
					break;
				case lkWarn:
					if (msg)
						AppLog_Warning(data.ctx, msg);
					break;
				case lkError:
					if (msg)
						AppLog_Error(data.ctx, msg);
					break;
				case lkSession:
					if (msg)
						AppLog_SetSession(data.ctx, msg);
					else
						AppLog_SetSession(data.ctx, "");
					break;
                case lkInstance:
                    AppLog_SetInstance(msg);
                    break;
				case lkClient:
					if (msg)
						AppLog_SetClient(data.ctx, msg);
					else
						AppLog_SetClient(data.ctx, "");
					break;				
				case lkReqStart:
				case lkSubReqStart: {
                    char empty[1] = {0};
                    log_parse_start_request(data.ctx, data.time.sec ? &data.time : NULL, msg ? msg : empty, data.kind == lkSubReqStart);
					break;
                }
				case lkReqExtra:
					if (msg)
						log_parse_extra_request(data.ctx, msg);
					break;
				case lkReqStop:
				case lkSubReqStop:
					if (msg)
						log_parse_end_request(data.ctx, data.time.sec ? &data.time : NULL, msg, data.kind == lkSubReqStop);
					break;
				case lkThreadStop:
					s_ClearContext((TAppLog_Context)data.ctx);
					break;
				case lkAppStart:
					AppLog_Init(msg);
					AppLog_SetDestination(sx_Destination);
					AppLog_AppStart(data.ctx, NULL);
					AppLog_AppRun(data.ctx);
					AppLog_SetPostLevel(sx_Severity);
					break;
				case lkAppStop:
                    if (!sx_log_destroyed) {
                        sx_log_destroyed = 1;
                        AppLog_AppStop(data.ctx, 0);
                        AppLog_Destroy(data.ctx);
                        log_queue_destroy(&sx_Queue);
                    }
					rv = -1;
					needbreak = 1;
					break;
				default:;
			}
			if (data.msg)
				free(data.msg);
			if (rv >= MAX_REQ_PER_CYCLE)
				break;
			cnt_sincelastcheck++;
			if (cnt_sincelastcheck > 100) {
				if ((gettime() - startt) > MAX_MS_IN_LOG * 1000L) {
					s_InitDestination(NULL);
					break;
				}
				cnt_sincelastcheck = 0;
			}
		}
	}
	else
		rv = 0;
	return rv;
}

void LogPush(log_kind_t akind, const char *str) {
	if (sx_Queue.m_buffer && !sx_log_closed) {
		switch (akind) {
			case lkTrace:
				if (sx_Severity > eAppLog_Trace)
					return;
				break;
			case lkInfo:
				if (sx_Severity > eAppLog_Info)
					return;
				break;
			case lkWarn:
				if (sx_Severity > eAppLog_Warning)
					return;
				break;
			case lkError:
				if (sx_Severity > eAppLog_Error)
					return;
				break;
			case lkAppStop:
				sx_log_closed = 1;
				break;
			default:;
		}
		
		log_queue_data_t data;
		if (str) {
			size_t len = strlen(str);
			if (len < sizeof(data.lmsg)) {
				memcpy(data.lmsg, str, len + 1);
				data.msg = NULL;
			}
			else {
				data.msg = strdup(str);
				data.lmsg[0] = 0;
			}
		}
		else {
			data.msg = NULL;
			data.lmsg[0] = 0;
		}
        switch (akind) {
           case lkReqStart:
           case lkReqStop:
           case lkSubReqStart:
           case lkSubReqStop:
              s_GetTime(&data.time);
              break;
           default:
              data.time.sec = data.time.ns = 0;
        }
		data.ctx = s_GetContext();
		data.kind = akind;
		if (akind == lkThreadStop) {
			s_TlsSetValue(NULL);
		}
		while (!log_queue_push(&sx_Queue, &data)) sched_yield();
	}
}

void LogSetSeverity(EAppLog_Severity sev) {
	sx_Severity = sev;
	LogPush(lkNone, NULL);
}

void LogSetDestination(EAppLog_Destination dest) {
	sx_Destination = dest;
	LogPush(lkNone, NULL);
}

void LogAppStart(const char* appname) {
	if (!sx_Queue.m_buffer) {
		log_queue_create(&sx_Queue, 4*1024);
		s_TlsInit();
		LogPush(lkAppStart, appname);
		sx_log_created = 1;
	}
}

void LogAppFinish() {
	LogPush(lkAppStop, NULL);
}

int LogAppIsStarted() {
	return (sx_Queue.m_buffer && sx_log_created && !sx_log_closed && !sx_log_destroyed);
}

void LogAppEscapeVal(char *str) {
	if (str) {
		char r;
		while ((r = *str) != '\0') {
			if (r == '&')
				*str |= 0x80;
			str++;
		}
	}
}

};
