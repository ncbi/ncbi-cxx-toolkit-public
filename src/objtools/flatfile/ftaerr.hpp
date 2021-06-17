#ifndef FTAERR_HPP
#define FTAERR_HPP

#include <corelib/ncbistl.hpp>
#include <stdio.h>
#include <stdarg.h>

USING_NCBI_SCOPE;

#ifndef THIS_MODULE
#    define THIS_MODULE ""
#endif

#define PREFIX_ACCESSION 01
#define PREFIX_LOCUS     02
#define PREFIX_FEATURE   04

/* Here: LOG - for logfile, MSG - for stderr
 */
#define EO_MSG_CODES     0001                   /* Show string codes gotten
                                                 * from .msg file */
#define EO_LOG_CODES     0002                   /* Show string codes gotten
                                                 * from .msg file */
#define EO_LOGTO_USRFILE 0004                   /* Has no effect */
#define EO_MSG_MSGTEXT   0010                   /* Has no effect */
#define EO_MSG_FILELINE  0020                   /* Code file name and
                                                   line number */
#define EO_LOG_FILELINE  0040                   /* Code file name and
                                                   line number */

#define EO_LOG_USRFILE   EO_LOGTO_USRFILE       /* Has no effect */
#define EO_SHOW_CODES    (EO_MSG_CODES | EO_LOG_CODES)
#define EO_SHOW_FILELINE (EO_MSG_FILELINE | EO_LOG_FILELINE)

/* From standard C Toolkit ErrPost system:
 *
#define EO_LOG_SEVERITY  0x00000001L
#define EO_LOG_CODES     0x00000002L
#define EO_LOG_FILELINE  0x00000004L
#define EO_LOG_USERSTR   0x00000008L
#define EO_LOG_ERRTEXT   0x00000010L
#define EO_LOG_MSGTEXT   0x00000020L
#define EO_MSG_SEVERITY  0x00000100L
#define EO_MSG_CODES     0x00000200L
#define EO_MSG_FILELINE  0x00000400L
#define EO_MSG_USERSTR   0x00000800L
#define EO_MSG_ERRTEXT   0x00001000L
#define EO_MSG_MSGTEXT   0x00002000L
#define EO_LOGTO_STDOUT  0x00010000L
#define EO_LOGTO_STDERR  0x00020000L
#define EO_LOGTO_TRACE   0x00040000L
#define EO_LOGTO_USRFILE 0x00080000L
#define EO_XLATE_CODES   0x01000000L
#define EO_WAIT_KEY      0x02000000L
#define EO_PROMPT_ABORT  0x04000000L
#define EO_BEEP          0x08000000L
*/
BEGIN_NCBI_SCOPE

enum ErrSev {
    SEV_NONE = 0,
    SEV_INFO,
    SEV_WARNING,
    SEV_ERROR,
    SEV_REJECT,
    SEV_FATAL,
    SEV_MAX
};

void   FtaErrInit(void);
void   FtaErrFini(void);

void   ErrSetOptFlags(int flags);
NCBI_DEPRECATED void   ErrSetFatalLevel(ErrSev sev);
bool   ErrSetLog(const char *logfile);
NCBI_DEPRECATED void   ErrClear(void);
void   ErrLogPrintStr(const char *str);
ErrSev ErrSetLogLevel(ErrSev sev);
ErrSev ErrSetMessageLevel(ErrSev sev);
void   Nlm_ErrPostEx(ErrSev sev, int lev1, int lev2, const char *fmt, ...);
void   Nlm_ErrPostStr(ErrSev sev, int lev1, int lev2, const char *str);
int    Nlm_ErrSetContext(const char *module, const char *fname, int line);

void   FtaInstallPrefix(int prefix, const char *name, const char *location);
void   FtaDeletePrefix(int prefix);

/*
#define ErrPostEx(sev, err_code, ...)                            \
    ( CNcbiDiag(DIAG_COMPILE_INFO, (EDiagSev) sev).GetRef()      \
    << ErrCode(err_code) << FtaErrMessage(__VA_ARGS__) << Endm )
*/
#define ErrPostEx (Nlm_ErrSetContext((const char *) THIS_MODULE, \
                                     (const char *) __FILE__,    \
                                     __LINE__) ? 0 :       \
                  Nlm_ErrPostEx)

#define ErrPostStr       ErrPostEx

END_NCBI_SCOPE

#endif //FTAERR_HPP
