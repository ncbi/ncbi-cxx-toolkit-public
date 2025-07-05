#include <ncbi_pch.hpp>

#include <string.h>
#include <time.h>
#include <sstream>
#include <forward_list>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

#include "flatfile_message_reporter.hpp"
#include "ftaerr.hpp"
#include "ftacpp.hpp"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "ftaerr.cpp"

#define MESSAGE_DIR "/am/ncbiapdata/errmsg"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

struct FtaErrCode {
    const char* module = nullptr;
    const char* fname  = nullptr;
    int         line   = 0;
};

struct FtaMsgModTagCtx {
    string           strsubtag;
    int              intsubtag   = -1;
    ErrSev           intseverity = ErrSev(-1);
};

struct FtaMsgModTag {
    string           strtag;
    int              inttag = 0;
    std::forward_list<FtaMsgModTagCtx> bmctx_list;
};

struct FtaMsgModFile {
    string          modname;  /* NCBI_MODULE or THIS_MODULE value */
    string          filename; /* Name with full path of .msg file */
    std::forward_list<FtaMsgModTag> bmmt_list;
};


struct FtaMsgPost {
    FILE*           lfd;     /* Opened logfile */
    string          logfile; /* Logfile full name */
    string          appname;
    string          prefix_accession;
    string          prefix_locus;
    string          prefix_feature;
    bool            to_stderr;
    bool            show_msg_codeline;
    bool            show_log_codeline;
    bool            show_msg_codes;
    bool            show_log_codes;
    bool            hook_only;
    ErrSev          msglevel; /* Filter out messages displaying on
                                           stderr only: ignode those with
                                           severity lower than msglevel */
    ErrSev          loglevel; /* Filter out messages displaying in
                                           logfile only: ignode those with
                                           severity lower than msglevel */
    std::forward_list<FtaMsgModFile> bmmf_list;

    FtaMsgPost() :
        lfd(nullptr),
        logfile(),
        prefix_accession(),
        prefix_locus(),
        prefix_feature(),
        to_stderr(true),
        show_msg_codeline(false),
        show_log_codeline(false),
        show_msg_codes(false),
        show_log_codes(false),
        hook_only(false),
        msglevel(SEV_NONE),
        loglevel(SEV_NONE)
    {
    }

    virtual ~FtaMsgPost()
    {
        if (lfd) {
            fclose(lfd);
        }
        logfile.clear();
        prefix_locus.clear();
        prefix_accession.clear();
        prefix_feature.clear();
    };
};

const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

thread_local unique_ptr<FtaMsgPost> bmp;
FtaErrCode                          fec;

/**********************************************************/
static ErrSev FtaStrSevToIntSev(const string& strsevcode)
{
    if (strsevcode.empty())
        return ErrSev(-1);

    static const map<string, ErrSev> sStringToInt = {
        { "SEV_INFO", SEV_INFO },
        { "SEV_WARNING", SEV_WARNING },
        { "SEV_ERROR", SEV_ERROR },
        { "SEV_REJECT", SEV_REJECT },
        { "SEV_FATAL", SEV_FATAL }
    };

    if (auto it = sStringToInt.find(strsevcode);
        it != sStringToInt.end()) {
        return it->second;
    }

    return ErrSev(-1);
}

/**********************************************************/
static void FtaErrGetMsgCodes(
    const char* module, int code, int subcode, string& strcode, string& strsubcode, ErrSev& sevcode)
{
    char* p;
    char* q;
    char  s[2048];
    char  ch;
    bool  got_mod;

    if (! bmp)
        FtaErrInit();

    got_mod = false;
    for (const auto& bmmf : bmp->bmmf_list) {
        if (bmmf.modname != module) {
            continue;
        }

        got_mod = true;
        for (const auto& bmmt : bmmf.bmmt_list) {
            if (bmmt.inttag != code)
                continue;

            strcode = bmmt.strtag;
            for (const auto& bmctx : bmmt.bmctx_list) {
                if (bmctx.intsubtag != subcode)
                    continue;

                strsubcode = bmctx.strsubtag;
                sevcode    = bmctx.intseverity;
                break;
            }
            break;
        }
        break;
    }

    if (got_mod)
        return;

    string curdir = CDir::GetCwd();
    string buf    = curdir + CDir::GetPathSeparator() + module + ".msg";
    FILE*  fd     = fopen(buf.c_str(), "r");
    if (! fd) {
        buf = string(MESSAGE_DIR) + CDir::GetPathSeparator() + module + ".msg";
        fd  = fopen(buf.c_str(), "r");
        if (! fd) {
            return;
        }
    }

    auto& bmmf    = bmp->bmmf_list.emplace_front();
    bmmf.modname  = module;
    bmmf.filename = buf;

    while (fgets(s, 2047, fd)) {
        if (s[0] != '$' || (s[1] != '^' && s[1] != '$'))
            continue;

        string val1;
        string val3;
        int    val2{ 0 };

        for (p = s + 2; *p == ' ' || *p == '\t'; p++) {}
        for (q = p; *p && *p != ','; p++) {}
        if (*p != ',')
            continue;

        *p   = '\0';
        val1 = q;

        for (*p++ = ','; *p == ' ' || *p == '\t'; p++) {}
        for (q = p; *p >= '0' && *p <= '9'; p++) {}

        if (q == p) {
            continue;
        }

        ch   = *p;
        *p   = '\0';
        val2 = fta_atoi(q);
        *p   = ch;

        if (val2 < 1) {
            continue;
        }

        if (s[1] == '^' && ch == ',') {
            for (p++; *p == ' ' || *p == '\t'; p++) {}
            for (q = p;
                 *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != ',';
                 p++) {}
            if (p > q) {
                ch = *p;
                *p = '\0';
                if (! strcmp(q, "SEV_INFO") || ! strcmp(q, "SEV_WARNING") ||
                    ! strcmp(q, "SEV_ERROR") || ! strcmp(q, "SEV_REJECT") ||
                    ! strcmp(q, "SEV_FATAL")) {
                    val3 = q;
                }
                *p = ch;
            }
        }

        if (s[1] == '$') {
            auto& bmmt  = bmmf.bmmt_list.emplace_front();
            bmmt.strtag = val1;
            bmmt.inttag = val2;
            if (val2 == code && strcode.empty())
                strcode = val1;

            continue;
        }

        if (bmmf.bmmt_list.empty()) {
            continue;
        }

        auto& bmmt  = bmmf.bmmt_list.front();
        auto& bmctx = bmmt.bmctx_list.emplace_front();

        bmctx.strsubtag   = val1;
        bmctx.intsubtag   = val2;
        bmctx.intseverity = FtaStrSevToIntSev(val3);

        if (val2 == subcode && strsubcode.empty() && ! strcode.empty()) {
            strsubcode = val1;
            if (sevcode < 0)
                sevcode = bmctx.intseverity;
        }
    }

    fclose(fd);
}

/**********************************************************/
void FtaErrInit()
{
    if (bmp)
        return;

    bmp.reset(new FtaMsgPost());
    bmp->appname = CNcbiApplication::GetAppName();

    fec.module     = nullptr;
    fec.fname      = nullptr;
    bmp->hook_only = false;
    fec.line       = -1;
}

/**********************************************************/
void FtaErrFini(void)
{
    if (bmp) {
        bmp.reset();
    }
}


/**********************************************************/
void FtaInstallPrefix(int prefix, string_view name, string_view location)
{
    if (name.empty())
        return;

    if ((prefix & PREFIX_ACCESSION) == PREFIX_ACCESSION) {
        bmp->prefix_accession = name;
    }
    if ((prefix & PREFIX_LOCUS) == PREFIX_LOCUS) {
        bmp->prefix_locus = name;
    }
    if ((prefix & PREFIX_FEATURE) == PREFIX_FEATURE) {
        if (name.size() > 20)
            name = name.substr(0, 20);
        if (location.size() > 127)
            location = location.substr(0, 127);
        bmp->prefix_feature = format("FEAT={}[{}]", name, location);
    }
}

/**********************************************************/
void FtaDeletePrefix(int prefix)
{
    if ((prefix & PREFIX_ACCESSION) == PREFIX_ACCESSION) {
        bmp->prefix_accession.clear();
    }
    if ((prefix & PREFIX_LOCUS) == PREFIX_LOCUS) {
        bmp->prefix_locus.clear();
    }
    if ((prefix & PREFIX_FEATURE) == PREFIX_FEATURE) {
        bmp->prefix_feature.clear();
    }
}

/**********************************************************/
bool ErrSetLog(const char* logfile)
{
    struct tm* tm;
    time_t     now;
    int        i;

    if (! logfile || ! *logfile)
        return (false);

    if (! bmp)
        FtaErrInit();

    if (bmp->logfile.empty()) {
        bmp->logfile = logfile;
    }

    if (! bmp->lfd && ! bmp->logfile.empty()) {
        time(&now);
        tm = localtime(&now);
        i  = tm->tm_hour % 12;
        if (! i)
            i = 12;

        bmp->lfd = fopen(bmp->logfile.c_str(), "a");
        fprintf(bmp->lfd,
                "\n========================[ %s %d, %d %2d:%02d %s ]========================\n",
                months[tm->tm_mon],
                tm->tm_mday,
                tm->tm_year + 1900,
                i,
                tm->tm_min,
                (tm->tm_hour >= 12) ? "PM" : "AM");
    }

    return (true);
}

/**********************************************************/
void ErrSetOptFlags(int flags)
{
    if (! bmp)
        FtaErrInit();

    if ((flags & EO_MSG_CODES) == EO_MSG_CODES)
        bmp->show_msg_codes = true;
    if ((flags & EO_LOG_CODES) == EO_LOG_CODES)
        bmp->show_log_codes = true;
    if ((flags & EO_MSG_FILELINE) == EO_MSG_FILELINE)
        bmp->show_msg_codeline = true;
    if ((flags & EO_LOG_FILELINE) == EO_LOG_FILELINE)
        bmp->show_log_codeline = true;
}

/**********************************************************/
void ErrLogPrintStr(const char* str)
{
    if (! str || str[0] == '\0')
        return;

    if (! bmp)
        FtaErrInit();

    fprintf(bmp->lfd, "%s", str);
}

/**********************************************************/
ErrSev ErrSetLogLevel(ErrSev sev)
{
    ErrSev prev;

    if (! bmp)
        FtaErrInit();

    prev          = bmp->loglevel;
    bmp->loglevel = sev;
    return (prev);
}

/**********************************************************/
ErrSev ErrSetMessageLevel(ErrSev sev)
{
    ErrSev prev;

    if (! bmp)
        FtaErrInit();

    prev          = bmp->msglevel;
    bmp->msglevel = sev;
    return (prev);
}

/**********************************************************/
void Nlm_ErrSetContext(const char* module, const char* fname, int line)
{
    if (! bmp)
        FtaErrInit();

    fec.module = module;
    fec.fname  = fname;
    fec.line   = line;
}

/**********************************************************/
EDiagSev ErrCToCxxSeverity(int c_severity)
{
    EDiagSev cxx_severity;

    switch (c_severity) {
    case SEV_NONE:
        cxx_severity = eDiag_Trace;
        break;
    case SEV_INFO:
        cxx_severity = eDiag_Info;
        break;
    case SEV_WARNING:
        cxx_severity = eDiag_Warning;
        break;
    case SEV_ERROR:
        cxx_severity = eDiag_Error;
        break;
    case SEV_REJECT:
        cxx_severity = eDiag_Critical;
        break;
    case SEV_FATAL:
    default:
        cxx_severity = eDiag_Fatal;
        break;
    }
    return (cxx_severity);
}

/**********************************************************/
NCBI_UNUSED
string ErrFormat(const char* fmt, ...)
{
    va_list args;
    char    fpiBuffer[1024];
    va_start(args, fmt);
    vsnprintf(fpiBuffer, 1024, fmt, args);
    va_end(args);
    return fpiBuffer;
}

/**********************************************************/
static void s_ReportError(ErrSev sev, int lev1, int lev2, string_view msg)
{
    ErrSev fpiSevcode    = ErrSev(-1);
    int    fpiIntcode    = lev1;
    int    fpiIntsubcode = lev2;

    string fpiStrcode;
    string fpiStrsubcode;

    int         fpiLine   = fec.line;
    const char* fpiFname  = fec.fname;
    const char* fpiModule = fec.module;

    fec.module = nullptr;
    fec.fname  = nullptr;
    fec.line   = -1;

    if (fpiModule && *fpiModule)
        FtaErrGetMsgCodes(fpiModule, fpiIntcode, fpiIntsubcode, fpiStrcode, fpiStrsubcode, fpiSevcode);
    else
        fpiModule = nullptr;

    if (fpiSevcode < SEV_NONE)
        fpiSevcode = sev;

    if (bmp->appname.empty())
        bmp->appname = CNcbiApplication::GetAppName();

    stringstream textStream;
    if (! fpiStrcode.empty()) {
        textStream << "[" << fpiStrcode.c_str();
        if (! fpiStrsubcode.empty()) {
            textStream << "." << fpiStrsubcode.c_str();
        }
        textStream << "] ";
    }

    if (bmp->show_log_codeline) {
        textStream << "{" << fpiFname << ", line  " << fpiLine;
    }
    if (! bmp->prefix_locus.empty()) {
        textStream << bmp->prefix_locus << ": ";
    }
    if (! bmp->prefix_accession.empty()) {
        textStream << bmp->prefix_accession << ": ";
    }
    if (! bmp->prefix_feature.empty()) {
        textStream << bmp->prefix_feature << " ";
    }
    textStream << msg;

    static const map<ErrSev, EDiagSev> sSeverityMap = {
        { SEV_NONE, eDiag_Trace },
        { SEV_INFO, eDiag_Info },
        { SEV_WARNING, eDiag_Warning },
        { SEV_ERROR, eDiag_Error },
        { SEV_REJECT, eDiag_Critical },
        { SEV_FATAL, eDiag_Fatal }
    };

    CFlatFileMessageReporter::GetInstance()
        .Report(fpiModule ? fpiModule : "",
                sSeverityMap.at(fpiSevcode),
                lev1,
                lev2,
                textStream.str());
}


/**********************************************************/
void Nlm_ErrPostStr(ErrSev sev, int lev1, int lev2, string_view str)
{
    if (! bmp)
        FtaErrInit();

    if (! fec.fname || fec.line < 0) {
        fec.module = nullptr;
        fec.fname  = nullptr;
        fec.line   = -1;
        return;
    }

    if (str.size() > 1023)
        str = str.substr(0, 1023);
    s_ReportError(sev, lev1, lev2, str);
}


/**********************************************************/

END_NCBI_SCOPE
