
#ifndef _APPLOG_WRAP_H_
#define _APPLOG_WRAP_H_

namespace IdLogUtil {
typedef enum {
	lkNone,
	lkTrace,
	lkInfo,
	lkWarn,
	lkError,
	lkClient,
	lkSession,
	lkReqStart,
	lkReqExtra,
	lkReqStop,
	lkSubReqStart,
	lkSubReqStop,
	lkThreadStop,
	lkAppStart,
	lkAppStop,
    lkInstance,
} log_kind_t;

/** Severity level for the posted diagnostics
 */
typedef enum {
    eAppLog_Trace = 0,       /**< Trace message          */
    eAppLog_Info,            /**< Informational message  */
    eAppLog_Warning,         /**< Warning message        */
    eAppLog_Error,           /**< Error message          */
    eAppLog_Critical,        /**< Critical error message */
    eAppLog_Fatal            /**< Fatal error -- guarantees exit (or abort) */
} EAppLog_Severity;

typedef enum {
    eAppLog_Default,         /**< Try /log/<*>/<appname>.log; fallback
                                   to STDERR */
    eAppLog_Stdlog,          /**< Try /log/<*>/<appname>.log;  fallback
                                   to ./<appname>.log, then to STDERR */
    eAppLog_Cwd,             /**< Try ./<appname>.log, fallback to STDERR */
    eAppLog_Stdout,          /**< To standard output stream */
    eAppLog_Stderr,          /**< To standard error stream  */
    eAppLog_Disable          /**< Don't write it anywhere   */
} EAppLog_Destination;




void LogAppStart(const char* appname);
int /*bool*/ LogQueuePeek();
void LogPush(log_kind_t akind, const char *str);
void LogSetSeverity(EAppLog_Severity sev);
void LogSetDestination(EAppLog_Destination dest);
void LogAppFinish();
void LogAppEscapeVal(char *str);
int LogAppIsStarted();

}

#endif
