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
 * Authors:  Anton Butanayev, Denis Vakatov, Vladimir Ivanov
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/error_codes.hpp>
#include <stdlib.h>
#include <limits>

#if defined(NCBI_OS_MSWIN)
#  include <windows.h>
#elif defined(NCBI_OS_UNIX)
#  include <sys/time.h>
#endif


// The current difference in seconds between UTC and local time on this computer.
// UTC = local time + TimeZone()
#if defined(__CYGWIN__) ||  defined(NCBI_COMPILER_MSVC)
#  define TimeZone() _timezone
#  define Daylight() _daylight
#  define TZName()   _tzname
#else
#  define TimeZone()  timezone
#  define Daylight()  daylight
#  define TZName()    tzname
#endif

// Global timezone/daylight information is not available on selected platforms
#if defined(NCBI_OS_DARWIN)  ||  defined(NCBI_OS_BSD)
#  define NCBI_TIMEZONE_IS_UNDEFINED  1
#endif

// The offset in seconds of daylight saving time.
// 1 hour for most time zones.
#if defined(NCBI_COMPILER_MSVC)
#  define DSTBias()   _dstbias
#else
#  define DSTBias()   -3600
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Util


BEGIN_NCBI_SCOPE


// Protective mutex
DEFINE_STATIC_MUTEX(s_TimeMutex);
DEFINE_STATIC_MUTEX(s_TimeAdjustMutex);
DEFINE_STATIC_MUTEX(s_FastLocalTimeMutex);

// Store global time/timespan formats in TLS
static CStaticTls<CTimeFormat> s_TlsFormatTime;
static CStaticTls<CTimeFormat> s_TlsFormatSpan;
static CStaticTls<CTimeFormat> s_TlsFormatStopWatch;

// Global quick and dirty getter of local time
static CSafeStatic<CFastLocalTime> s_FastLocalTime;


//============================================================================

// Number of days per month
static int s_DaysInMonth[12] = {
    31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

// Month names
static const char* kMonthAbbr[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char* kMonthFull[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

// Day of week names
static const char* kWeekdayAbbr[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char* kWeekdayFull [7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

// Default value for time/timespan format
static const char* kDefaultFormatTime      = "M/D/Y h:m:s";
static const char* kDefaultFormatSpan      = "-S.n";
static const char* kDefaultFormatSpanIn    = "-G";    // default CTimeSpan init format
static const char* kDefaultFormatStopWatch = "S.n";

// Set of the checked format symbols.
// For CStopWatch class the format symbols are equal
// to kFormatSymbolsSpan also.
static const char* kFormatSymbolsTime = "yYMbBDdhHmsSlrgGzZwWpPo";
static const char* kFormatSymbolsSpan = "-dhHmMsSnNgG";

// Character used to escape formatted symbols.
const char kFormatEscapeSymbol = '$';


// Macro to check range for time components.
// See also:
//      CTime::m_Data
//      CTime::IsValid
//      CTime::Set*() methods

#define CHECK_RANGE_EXCEPTION(value, what, min, max) \
    if ( value < min  ||  value > max ) {  \
        NCBI_THROW(CTimeException, eArgument, \
                    what " value '" + \
                    NStr::Int8ToString((Int8)value) + "' is out of range"); \
    }

#define CHECK_RANGE2(value, what, min, max, err_action) \
    if ( value < min  ||  value > max ) {  \
        if ( err_action == eErr_Throw ) { \
            NCBI_THROW(CTimeException, eArgument, \
                       what " value '" + \
                       NStr::Int8ToString((Int8)value) + "' is out of range"); \
        } else { return false; } \
    }

#define CHECK_RANGE_YEAR(value)         CHECK_RANGE_EXCEPTION(value, "Year", 1583, kMax_Int)
#define CHECK_RANGE_MONTH(value)        CHECK_RANGE_EXCEPTION(value, "Month", 1, 12)
#define CHECK_RANGE_DAY(value)          CHECK_RANGE_EXCEPTION(value, "Day", 1, 31)
#define CHECK_RANGE_HOUR(value)         CHECK_RANGE_EXCEPTION(value, "Hour", 0, 23)
#define CHECK_RANGE_MIN(value)          CHECK_RANGE_EXCEPTION(value, "Minute", 0, 59)
#define CHECK_RANGE_SEC(value)          CHECK_RANGE_EXCEPTION(value, "Second", 0, 61)
#define CHECK_RANGE_NSEC(value)         CHECK_RANGE_EXCEPTION(value, "Nanosecond", 0, kNanoSecondsPerSecond - 1)

#define CHECK_RANGE2_YEAR(value, err)   CHECK_RANGE2(value, "Year", 1583, kMax_Int, err)
#define CHECK_RANGE2_MONTH(value, err)  CHECK_RANGE2(value, "Month", 1, 12, err)
#define CHECK_RANGE2_DAY(value, err)    CHECK_RANGE2(value, "Day", 1, 31, err)
#define CHECK_RANGE2_HOUR24(value, err) CHECK_RANGE2(value, "Hour", 0, 23, err)
#define CHECK_RANGE2_HOUR12(value, err) CHECK_RANGE2(value, "Hour", 0, 12, err)
#define CHECK_RANGE2_MIN(value, err)    CHECK_RANGE2(value, "Minute", 0, 59, err)
#define CHECK_RANGE2_SEC(value, err)    CHECK_RANGE2(value, "Second", 0, 61, err)
#define CHECK_RANGE2_NSEC(value, err)   CHECK_RANGE2(value, "Nanosecond", 0, kNanoSecondsPerSecond - 1, err)

// Bitfilds setters (to avoid warnings)

#define SET_YEAR(value)  m_Data.year  = static_cast<unsigned int>((value) & 0xFFF)
#define SET_MONTH(value) m_Data.month = static_cast<unsigned char>((value) & 0x0F)
#define SET_DAY(value)   m_Data.day   = static_cast<unsigned char>((value) & 0x1F)
#define SET_HOUR(value)  m_Data.hour  = static_cast<unsigned char>((value) & 0x1F)
#define SET_MIN(value)   m_Data.min   = static_cast<unsigned char>((value) & 0x3F)
#define SET_SEC(value)   m_Data.sec   = static_cast<unsigned char>((value) & 0x3F)



//============================================================================

// Get number of days in "date"
static unsigned s_Date2Number(const CTime& date)
{
    if ( date.IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    unsigned d = date.Day();
    unsigned m = date.Month();
    unsigned y = date.Year();
    unsigned c, ya;

    if (m > 2) {
        m -= 3;
    } else {
        m += 9;
        --y;
    }
    c  = y / 100;
    ya = y - 100 * c;

    return ((146097 * c) >> 2) + ((1461 * ya) >> 2) +
            (153 * m + 2) / 5 +  d + 1721119;
}


// Conversion number of days in date format
// timezone value compute on base <t>
static CTime s_Number2Date(unsigned num, const CTime& t)
{
    unsigned d;
    unsigned j = num - 1721119;
    unsigned year;
    unsigned day;
    int      month;

    year = (((j<<2) - 1) / 146097);
    j = (j<<2) - 1 - 146097 * year;
    d = (j>>2);
    j = ((d<<2) + 3) / 1461;
    d = (d<<2) + 3 - 1461 * j;
    d = (d + 4) >> 2;
    month = (5*d - 3) / 153;
    d = 5*d - 3 - 153 * month;
    day = (d + 5) / 5;
    year = 100 * year + j;

    if (month < 10) {
        month += 3;
    } else {
        month -= 9;
        ++year;
    }
    // Construct new CTime object
    return
        CTime(year, month, day, t.Hour(), t.Minute(), t.Second(),
              t.NanoSecond(), t.GetTimeZone(), t.GetTimeZonePrecision());
}


// Calc <value> + <offset> on module <bound>.
// Returns normalized value in <value>. 
// The <major> will have a remainder after dividing.
static inline
void s_Offset(long *value, Int8 offset, long bound, int *major)
{
    Int8 v = *value + offset;
    *major += (int)(v / bound);
    *value = (long)(v % bound);
    if (*value < 0) {
        *major -= 1;
        *value += bound;
    }
}


// Convert 'value' to string, append result to string 'str'.
static inline 
void s_AddInt(string& str, long value)
{
    const size_t size = CHAR_BIT * sizeof(value);
    char   buf[size];
    size_t pos = size;
    do {
        buf[--pos] = char((value % 10) + '0');
        value /= 10;
    } while (value);
    str.append(buf + pos, size - pos);
}


// Convert 'value' to string, add leading '0' to size 'len'.
// Append result to string 'str'.
static inline 
void s_AddZeroPadInt(string& str, long value, size_t len, bool ignore_trailing_zeros = false)
{
    _ASSERT(value >= 0);
    _ASSERT((len > 0)  &&  (len < 10));

    const size_t size = 9;
    char buf[size];
    memset(buf, '0', size);

    size_t pos = size;
    do {
        buf[--pos] = char((value % 10) + '0');
        value /= 10;
    } while (value);

    if (size - pos > len) {
        len = size - pos;
    }
    char* p = buf + size - len;
    if (ignore_trailing_zeros) {
        for (; len > 1  &&  p[len-1] == '0'; --len) {}
    }
    str.append(p, len);
}


// Optimized variant of s_AddZeroPadInt() for len = 2.
static inline 
void s_AddZeroPadInt2(string& str, long value)
{
    _ASSERT((value >= 0)  &&  (value <= 99));
    char buf[2];
    buf[1] = char((value % 10) + '0');
    buf[0] = char((value / 10) + '0'); 
    str.append(buf, 2);
}



//============================================================================
//
// CTimeFormat
//
//============================================================================


CTimeFormat::CTimeFormat(void)
    : m_Flags(fDefault)
{
    return;
}


CTimeFormat::CTimeFormat(const CTimeFormat& fmt)
{
    *this = fmt;
}


CTimeFormat::CTimeFormat(const char* fmt, TFlags flags)
{
    SetFormat(fmt, flags);
}


CTimeFormat::CTimeFormat(const string& fmt, TFlags flags)
{
    SetFormat(fmt, flags);
}


void CTimeFormat::SetFormat(const string& fmt, TFlags flags)
{
    // Check flags compatibility

    TFlags f = fFormat_Simple | fFormat_Ncbi;
    if ((flags & f) == f) {
        NCBI_THROW(CTimeException, eArgument,
            "Incompatible flags specified together: fFormat_Simple | fFormat_Ncbi");
    }
    if ((flags & f) == 0) {
        flags |= fFormat_Simple;  // default
    }

    if ((flags & fMatch_Strict) && (flags & fMatch_Weak)) {
        NCBI_THROW(CTimeException, eArgument,
            "Incompatible flags specified together: fMatch_Strict | fMatch_Weak");
    }
    if ((flags & (fMatch_Strict | fMatch_Weak)) == 0) {
        flags |= fMatch_Strict;  // default
    }

    m_Str   = fmt;
    m_Flags = flags;
}


CTimeFormat& CTimeFormat::operator= (const CTimeFormat& fmt)
{
    if ( &fmt == this ) {
        return *this;
    }
    m_Str   = fmt.m_Str;
    m_Flags = fmt.m_Flags;
    return *this;
}


CTimeFormat CTimeFormat::GetPredefined(EPredefined fmt, TFlags flags)
{
    // Predefined time formats
    static const char* s_Predefined[][2] =
    {
        {"Y",            "$Y"},
        {"Y-M",         "$Y-$M"},
        {"Y-M-D",       "$Y-$M-$D"},
        {"Y-M-DTh:m",   "$Y-$M-$DT$h:$m"},
        {"Y-M-DTh:m:s", "$Y-$M-$DT$h:$m:$s"},
        {"Y-M-DTh:m:G", "$Y-$M-$DT$h:$m:$G"},
    };
    int fmt_type = (flags & fFormat_Ncbi) ? 1 : 0;
    return CTimeFormat(s_Predefined[(int)fmt][(int)fmt_type], flags);
}


//============================================================================
//
// CTime
//
//============================================================================

static string s_TimeDump(const CTime& time)
{
    string out;
    out.reserve(128);
    out = string("[") + 
        "year="    + NStr::Int8ToString(time.Year()) + ", " +
        "month="   + NStr::Int8ToString(time.Month()) + ", " +
        "day="     + NStr::Int8ToString(time.Day()) + ", " +
        "hour="    + NStr::Int8ToString(time.Hour()) + ", " +
        "min="     + NStr::Int8ToString(time.Minute()) + ", " +
        "sec="     + NStr::Int8ToString(time.Second()) + ", " +
        "nanosec=" + NStr::Int8ToString(time.NanoSecond()) + ", " +
        "tz="      + (time.IsUniversalTime() ? "UTC" : "Local") +
        "]";
    return out;
}


CTime::CTime(const CTime& t)
{
    *this = t;
}


CTime::CTime(int year, int yearDayNumber,
             ETimeZone tz, ETimeZonePrecision tzp)
{
    memset(&m_Data, 0, sizeof(m_Data));
    m_Data.tz = tz;
    m_Data.tzprec = tzp;

    CTime t = CTime(year, 1, 1);
    t.AddDay(yearDayNumber - 1);
    m_Data.year  = t.m_Data.year;
    m_Data.month = t.m_Data.month;
    m_Data.day   = t.m_Data.day;
}


// Helper macro for x_Init() errors
#define X_INIT_ERROR(type, msg) \
    if (err_action == eErr_Throw) { \
        NCBI_THROW(CTimeException, type, msg); \
    } else { \
        return false; \
    }

// Skip white spaces
#define SKIP_SPACES(s) \
    while (isspace((unsigned char)(*s))) ++s;


// The helper to check string with 'o' format symbol specified
struct STzFormatCheck
{
    STzFormatCheck() : m_Active(false) {}

    bool fmt(const char*& f) {
        _ASSERT(f);
        return m_Active = *f == 'o';
    }
    bool str(const char*& s) {
        _ASSERT(s);
        if (m_Active) {
            if (*s != ':') return false;
            ++s;
            m_Active = false;
        }
        return true;
    }
private:
    bool m_Active;
};

// The helper to compose string with 'o' format symbol specified
struct STzFormatMake
{
    STzFormatMake() : m_Active(false) {}

    void activate() {
        m_Active = true;
    }
    void str(string& s) {
        if (m_Active) {
            s += ':';
            m_Active = false;
        }
    }
    bool active() const { return m_Active; }

private:
    bool m_Active;
};


bool CTime::x_Init(const string& str, const CTimeFormat& format, EErrAction err_action)
{
    Clear();

    // Special case, always create empty CTime object if str is empty, ignoring format.
    // See IsEmpty().
    if ( str.empty() ) {
        return true;
    }

    // Use guard against accidental exceptions from other parts of API or NStr::,
    // we do best to avoid exceptions if requested, but it is not alway possible
    // to control all calling code from here, so just to be safe...
    try {

    // For partially defined times use default values
    bool is_year_present  = false;
    bool is_month_present = false;
    bool is_day_present   = false;
    bool is_time_present  = false;

    const string& fmt = format.GetString();
    bool is_escaped_fmt = ((format.GetFlags() & CTimeFormat::fFormat_Simple) == 0);
    bool is_escaped_symbol = false;

    const char* fff;
    const char* sss = str.c_str();
    bool  adjust_needed = false;
    long  adjust_tz     = 0;

    enum EHourFormat{
        e24, eAM, ePM
    };
    EHourFormat hour_format = e24;
    int weekday = -1;
    bool is_12hour = false;

    // Used to check white spaces for fMatch_IgnoreSpaces
    bool is_ignore_spaces = ((format.GetFlags() & CTimeFormat::fMatch_IgnoreSpaces) != 0);
    bool is_format_space = false;
    STzFormatCheck tz_fmt_check;

    for (fff = fmt.c_str();  *fff != '\0';  ++fff) {

        // White space processing -- except directly after escape symbols

        if (!is_escaped_symbol) {
            // Skip space symbols in format string
            if (isspace((unsigned char)(*fff))) {
                is_format_space = true;
                continue;
            }
            if (is_ignore_spaces) {
                // Skip space symbols in time string also
                SKIP_SPACES(sss);
            } else {
                // match spaces
                if (isspace((unsigned char)(*sss))) {
                    if (is_format_space) {
                        // Skip space symbols in time string
                        SKIP_SPACES(sss);
                    } else {
                        break;  // error: non-matching spaces
                    }
                } else {
                    if (is_format_space) {
                        break;  // error: non-matching spaces
                    }
                }
                is_format_space = false;
            }
        }

        // Skip preceding symbols for some formats
        if (is_escaped_fmt  &&  !is_escaped_symbol) {
            if (*fff == kFormatEscapeSymbol)  {
                is_escaped_symbol = true;
                continue;
            }
        }

        // Match non-format symbols

        if (is_escaped_fmt) {
            if (!(is_escaped_symbol  &&  *fff != kFormatEscapeSymbol)) {
                // Match non-format symbols
                if (*fff == *sss) {
                    ++sss;
                    continue;
                }
                break;  // error: non-matching symbols
            }
            // format symbol found, process as usual
            is_escaped_symbol = false;
        } else {
            // Regular non-escaped format, each symbol can be a format symbol.
            // Check allowed format symbols
            if (strchr(kFormatSymbolsTime, *fff) == 0) {
                // Match non-format symbols
                if (*fff == *sss) {
                    ++sss;
                    continue;
                }
                break;  // error: non-matching symbols
            }
        }

        // Process format symbols

        // Month
        if (*fff == 'b'  ||  *fff == 'B') {
            const char** name;
            if (*fff == 'b') {
                name = kMonthAbbr;
            } else {
                name = kMonthFull;
            }
            for (unsigned char i = 0;  i < 12;  ++i) {
                size_t namelen = strlen(*name);
                if (NStr::strncasecmp(sss, *name, namelen) == 0) {
                    sss += namelen;
                    SET_MONTH(i + 1);
                    break;
                }
                ++name;
            }
            is_month_present = true;
            continue;
        }

        // Day of week
        if (*fff == 'w'  ||  *fff == 'W') {
            const char** day = (*fff == 'w') ? kWeekdayAbbr : kWeekdayFull;
            for (unsigned char i = 0;  i < 7;  ++i) {
                size_t len = strlen(*day);
                if (NStr::strncasecmp(sss, *day, len) == 0) {
                    sss += len;
                    weekday = i;
                    break;
                }
                ++day;
            }
            continue;
        }

        // Timezone (UTC time)
        if (*fff == 'Z') {
            if ((NStr::strncasecmp(sss, "UTC", 3) == 0) ||
                (NStr::strncasecmp(sss, "GMT", 3) == 0) ||
                (*sss == 'Z')) {
                m_Data.tz = eUTC;
                sss += ((*sss == 'Z') ? 1 : 3);
            } else {
                m_Data.tz = eLocal;
            }
            continue;
        }

        // Timezone (local time in format '[GMT/UTC]+/-HHMM')
        if (*fff == 'z' || tz_fmt_check.fmt(fff)) {
            m_Data.tz = eUTC;
            if ((NStr::strncasecmp(sss, "UTC", 3) == 0) ||
                (NStr::strncasecmp(sss, "GMT", 3) == 0)) {
                sss += 3;
            }
            SKIP_SPACES(sss);
            int sign = (*sss == '+') ? 1 : ((*sss == '-') ? -1 : 0);
            if ( sign ) {
                ++sss;
            } else {
                sign = 1;
            }
            long x_hour = 0;
            long x_min  = 0;

            char value_str[3];
            char* s = value_str;
            for (size_t len = 2;
                 len  &&  *sss  &&  isdigit((unsigned char)(*sss));
                 --len) {
                *s++ = *sss++;
            }
            *s = '\0';
            try {
                x_hour = NStr::StringToLong(value_str);
            }
            catch (const CStringException&) {
                x_hour = 0;
            }
            try {
                if ( *sss != '\0' ) {
                    if (!tz_fmt_check.str(sss)) {
                        break;  // error: not matched format symbol ':'
                    }
                    s = value_str;
                    for (size_t len = 2;
                         len  &&  *sss  &&  isdigit((unsigned char)(*sss));
                         --len) {
                        *s++ = *sss++;
                    }
                    *s = '\0';
                    x_min = NStr::StringToLong(value_str,
                                               NStr::fAllowTrailingSymbols);
                }
            }
            catch (const CStringException&) {
                x_min = 0;
            }
            adjust_needed = true;
            adjust_tz = sign * (x_hour * 60 + x_min) * 60;
            continue;
        }

        // AM/PM modifier
        if (*fff == 'p'  ||  *fff == 'P') {
            if (NStr::strncasecmp(sss, "AM", 2) == 0) {
                hour_format = eAM;
                sss += 2;
            } else if (NStr::strncasecmp(sss, "PM", 2) == 0) {
                hour_format = ePM;
                sss += 2;
            }
            continue;
        }

        // Other format symbols -- read the next data ingredient
        char value_str[10];
        char* s = value_str;
        size_t len = 2;
        switch (*fff) {
            case 'Y': len = 4; break;
            case 'S': len = 9; break;
            case 'l': len = 3; break;
            case 'r': len = 6; break;
        }
        for ( ; len  &&  *sss  &&  isdigit((unsigned char)(*sss));  --len) {
            *s++ = *sss++;
        }
        *s = '\0';
        long value = NStr::StringToLong(value_str);

        // Set time part
        switch ( *fff ) {
        case 'Y':
            CHECK_RANGE2_YEAR(value, err_action);
            SET_YEAR(value);
            is_year_present = true;
            break;
        case 'y':
            if (value >= 0  &&  value < 50) {
                value += 2000;
            } else if (value >= 50  &&  value < 100) {
                value += 1900;
            }
            CHECK_RANGE2_YEAR(value, err_action);
            SET_YEAR(value);
            is_year_present = true;
            break;
        case 'M':
            CHECK_RANGE2_MONTH(value, err_action);
            SET_MONTH(value);
            is_month_present = true;
            break;
        case 'D':
        case 'd':
            CHECK_RANGE2_DAY(value, err_action);
            SET_DAY(value);
            is_day_present = true;
            break;
        case 'h':
            CHECK_RANGE2_HOUR24(value, err_action);
            SET_HOUR(value);
            is_time_present = true;
            break;
        case 'H':
            CHECK_RANGE2_HOUR12(value, err_action);
            SET_HOUR(value % 12);
            is_12hour = true;
            is_time_present = true;
            break;
        case 'm':
            CHECK_RANGE2_MIN(value, err_action);
            SET_MIN(value);
            is_time_present = true;
            break;
        case 's':
            CHECK_RANGE2_SEC(value, err_action);
            SET_SEC(value);
            is_time_present = true;
            break;
        case 'l':
            CHECK_RANGE2_NSEC((Int8)value * 1000000, err_action);
            m_Data.nanosec = (Int4)value * 1000000;
            is_time_present = true;
            break;
        case 'r':
            CHECK_RANGE2_NSEC((Int8)value * 1000, err_action);
            m_Data.nanosec = (Int4)value * 1000;
            is_time_present = true;
            break;
        case 'S':
            CHECK_RANGE2_NSEC(value, err_action);
            m_Data.nanosec = (Int4)value;
            is_time_present = true;
            break;
        case 'g':
        case 'G':
            CHECK_RANGE2_SEC(value, err_action);
            SET_SEC(value);
            if ( *sss == '.' ) {
                ++sss;
                s = value_str;
                // Limit fraction of second to 9 digits max,
                // ignore all other digits in string if any.
                for (size_t n = 9;
                    n  &&  *sss  &&  isdigit((unsigned char)(*sss));  --n) {
                    *s++ = *sss++;
                }
                *s = '\0';
                value = NStr::StringToLong(value_str);
                size_t n = strlen(value_str);
                // 'n' cannot have more then 9 (max for nanoseconds) - see above.
                _ASSERT(n <= 9);
                for (;  n < 9;  ++n) {
                    value *= 10;
                }
                CHECK_RANGE2_NSEC(value, err_action);
                m_Data.nanosec = (Int4)value;
                // Ignore extra digits
                while ( isdigit((unsigned char)(*sss)) ) {
                    ++sss;
                }
            }
            is_time_present = true;
            break;

        default:
            X_INIT_ERROR(eFormat, "Format '" + fmt + "' has incorrect format symbol '" + *fff + "'");
        }
    }

    // Correct 12-hour time if needed
    if (is_12hour  &&  hour_format == ePM) {
        SET_HOUR(m_Data.hour + 12);
    }

    // Skip all remaining white spaces in the string
    if (is_ignore_spaces) {
        SKIP_SPACES(sss);
    } else {
        if (isspace((unsigned char)(*sss))  &&  is_format_space) {
            SKIP_SPACES(sss);
        }
        // else { error: non-matching spaces -- processed below }
    }

    if (*fff != '\0'  &&  
        !(format.GetFlags() & CTimeFormat::fMatch_ShortTime)) {
        X_INIT_ERROR(eFormat, "Time string '" + str + 
                              "' is too short for time format '" + fmt + "'");
    }
    if (*sss != '\0'  &&  
        !(format.GetFlags() & CTimeFormat::fMatch_ShortFormat)) {
        X_INIT_ERROR(eFormat, "Time string '" + str +
                              "' is too long for time format '" + fmt + "'");
    }
    if (*fff != '\0'  &&  *sss != '\0'  && 
        ((format.GetFlags() & CTimeFormat::fMatch_Weak) == CTimeFormat::fMatch_Weak)) {
        X_INIT_ERROR(eFormat, "Time string '" + str +
                              "' does not match time format '" + fmt + "'");
    }

    // For partially defined times use default values
    int ptcache = 0;
    ptcache += (is_year_present  ? 2000 : 1000);
    ptcache += (is_month_present ? 200 : 100);
    ptcache += (is_day_present   ? 20 : 10);
    ptcache += (is_time_present  ? 2 : 1);

    // Use empty or current time to set missed time components
    CTime current;
    if ( !adjust_needed ) {
        switch (ptcache) {
            case 1222:
            case 1221:
            case 1211:
            case 1121:
            case 1122:
            case 1112:
                current.SetCurrent();
        }
    }
    switch (ptcache) {
        case 2211:                          // Y,M      -> D = 1
            m_Data.day   = 1;
            break;
        case 2111:                          // Y        -> M,D = 1
            m_Data.month = 1;
            m_Data.day   = 1;
            break;
        case 1222:                          // M,D,time -> Y = current
        case 1221:                          // M,D      -> Y = current
            m_Data.year  = current.m_Data.year;
            break;
        case 1211:                          // M        -> Y = current, D = 1
            m_Data.year  = current.m_Data.year;
            m_Data.day   = 1;
            break;
        case 1122:                          // D, time  -> Y,M = current
        case 1121:                          // D        -> Y,M = current
            m_Data.year  = current.m_Data.year;
            m_Data.month = current.m_Data.month;
            break;
        case 1112:                          // time     -> Y,M,D = current
            m_Data.year  = current.m_Data.year;
            m_Data.month = current.m_Data.month;
            m_Data.day   = current.m_Data.day;
            break;
    }

    // Check on errors for weekday
    if (weekday != -1  &&  weekday != DayOfWeek()) {
        X_INIT_ERROR(eConvert, "Invalid day of week " + NStr::IntToString(weekday));
    }
    // Validate time value
    if ( !IsValid() ) {
        X_INIT_ERROR(eConvert, "Unable to convert string '" + str + "' to CTime");
    }
    // Adjust time to universal (see 'z' format symbol above)
    if ( adjust_needed ) {
        AddSecond(-adjust_tz, CTime::eIgnoreDaylight);
    }

    // see 'try' at the beginning of the method
    } catch (const CException&) {
        if (err_action == eErr_Throw) {
            throw;
        } else {
            return false;
        }
    }
    return true;
}


CTime::CTime(int year, int month, int day, int hour,
             int minute, int second, long nanosecond,
             ETimeZone tz, ETimeZonePrecision tzp)
{
    memset(&m_Data, 0, sizeof(m_Data));

    CHECK_RANGE_YEAR(year);
    CHECK_RANGE_MONTH(month);
    CHECK_RANGE_DAY(day);
    CHECK_RANGE_HOUR(hour);
    CHECK_RANGE_MIN(minute);
    CHECK_RANGE_SEC(second);
    CHECK_RANGE_NSEC(nanosecond);

    SET_YEAR(year);
    SET_MONTH(month);
    SET_DAY(day);
    SET_HOUR(hour);
    SET_MIN(minute);
    SET_SEC(second);
    
    m_Data.nanosec     = (Int4)nanosecond;
    m_Data.tz          = tz;
    m_Data.tzprec      = tzp;
    m_Data.adjTimeDiff = 0;

    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid,
                   string("Invalid time ") + s_TimeDump(*this));
    }
}


CTime::CTime(EInitMode mode, ETimeZone tz, ETimeZonePrecision tzp)
{
    memset(&m_Data, 0, sizeof(m_Data));
    m_Data.tz = tz;
    m_Data.tzprec = tzp;
    if (mode == eCurrent) {
        SetCurrent();
    }
}


CTime::CTime(time_t t, ETimeZonePrecision tzp)
{
    memset(&m_Data, 0, sizeof(m_Data));
    m_Data.tz = eUTC;
    m_Data.tzprec = tzp;
    SetTimeT(t);
}


CTime::CTime(const struct tm& t, ETimeZonePrecision tzp)
{
    memset(&m_Data, 0, sizeof(m_Data));
    m_Data.tz = eLocal;
    m_Data.tzprec = tzp;
    SetTimeTM(t);
}


CTime::CTime(const string& str, const CTimeFormat& fmt,
             ETimeZone tz, ETimeZonePrecision tzp)
{
    memset(&m_Data, 0, sizeof(m_Data));
    _ASSERT(eLocal > 0);
    _ASSERT(eUTC   > 0);
    _ASSERT(eGmt   > 0);
    if ( !m_Data.tz ) {
        m_Data.tz = tz;
    }
    m_Data.tzprec = tzp;
    if (fmt.IsEmpty()) {
        x_Init(str, GetFormat());
    } else {
        x_Init(str, fmt);
    }
}


bool CTime::ValidateString(const string& str, const CTimeFormat& fmt)
{
    CTime t;
    return t.x_Init(str, fmt.IsEmpty() ? GetFormat() : fmt,  eErr_NoThrow);
}


void CTime::SetYear(int year)
{
    CHECK_RANGE_YEAR(year);
    SET_YEAR(year);
    int n_days = DaysInMonth();
    if ( m_Data.day > n_days ) {
        SET_DAY(n_days);
    }
    // Additional checks
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid,
                   "Unable to set year number '" +
                   NStr::IntToString(year) + "'");
    }
}


void CTime::SetMonth(int month)
{
    CHECK_RANGE_MONTH(month);
    SET_MONTH(month);
    int n_days = DaysInMonth();
    if ( m_Data.day > n_days ) {
        SET_DAY(n_days);
    }
    // Additional checks
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid,
                   "Unable to set month number '" +
                   NStr::IntToString(month) + "'");
    }
}


void CTime::SetDay(int day)
{
    CHECK_RANGE_DAY(day);
    int n_days = DaysInMonth();
    if ( day > n_days ) {
        SET_DAY(n_days);
    } else {
        SET_DAY(day);
    }
    // Additional checks
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid,
                   "Unable to set day number '" +
                   NStr::IntToString(day) + "'");
    }
}


void CTime::SetHour(int hour)
{
    CHECK_RANGE_HOUR(hour);
    SET_HOUR(hour);
}


void CTime::SetMinute(int minute)
{
    CHECK_RANGE_MIN(minute);
    SET_MIN(minute);
}


void CTime::SetSecond(int second)
{
    CHECK_RANGE_SEC(second);
    SET_SEC(second);
}


void CTime::SetMilliSecond(long millisecond)
{
    CHECK_RANGE_NSEC(millisecond * 1000000);
    m_Data.nanosec = (Int4)millisecond * 1000000;
}


void CTime::SetMicroSecond(long microsecond)
{
    CHECK_RANGE_NSEC(microsecond * 1000);
    m_Data.nanosec = (Int4)microsecond * 1000;
}


void CTime::SetNanoSecond(long nanosecond)
{
    CHECK_RANGE_NSEC(nanosecond);
    m_Data.nanosec = (Int4)nanosecond;
}


int CTime::YearDayNumber(void) const
{
    unsigned first = s_Date2Number(CTime(Year(), 1, 1));
    unsigned self  = s_Date2Number(*this);
    _ASSERT(first <= self  &&  self < first + (IsLeap() ? 366 : 365));
    return int(self - first + 1);
}


int CTime::YearWeekNumber(EDayOfWeek first_day_of_week) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if (first_day_of_week > eSaturday) {
        NCBI_THROW(CTimeException, eArgument,
                   "Day of week with value " + 
                   NStr::IntToString((int)first_day_of_week) +
                   " is incorrect");
    }

    int week_num = 0;
    int wday = DayOfWeek();

    // Adjust day of week (from default Sunday)
    wday -= first_day_of_week;
    if (wday < 0) {
        wday += 7;
    }

    // Calculate week number
    int yday = YearDayNumber() - 1;  // YearDayNumber() returns 1..366
    if (yday >= wday) {
        week_num = yday / 7;
        if ( (yday % 7) >= wday ) {
            ++week_num;
        }
    }
    // Adjust range from [0..53] to [1..54]
    return week_num + 1;
}


int CTime::MonthWeekNumber(EDayOfWeek first_day_of_week) const
{
    CTime first_of_month(Year(), Month(), 1);
    int week_num_first   = first_of_month.YearWeekNumber(first_day_of_week);
    int week_num_current = YearWeekNumber(first_day_of_week);
    return week_num_current - week_num_first + 1;
}


int CTime::DayOfWeek(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    int y = Year();
    int m = Month();

    y -= int(m < 3);
    return (y + y/4 - y/100 + y/400 + "-bed=pen+mad."[m] + Day()) % 7;
}


int CTime::DaysInMonth(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    int n_days = s_DaysInMonth[Month()-1];
    if (n_days == 0) {
        n_days = IsLeap() ? 29 : 28;
    }
    return n_days;
}


int CTime::MonthNameToNum(const string& month)
{
    const char** name = month.length() == 3 ? kMonthAbbr : kMonthFull;
    for (int i = 0; i < 12; ++i) {
        if ( NStr::CompareNocase(month, name[i]) == 0 ) {
            return i+1;
        }
    }
    // Always throw exceptions here.
    // Next if statements avoid compilation warnings.
    if ( name ) {
        NCBI_THROW(CTimeException, eArgument,
                   "Invalid month name '" + month + "'");
    }
    return -1;
}


string CTime::MonthNumToName(int month, ENameFormat fmt)
{
    if (month < 1  ||  month > 12) {
        NCBI_THROW(CTimeException, eArgument,
                   "Invalid month number " + NStr::IntToString(month));
    }
    --month;
    return fmt == eFull ? kMonthFull[month] : kMonthAbbr[month];
}


int CTime::DayOfWeekNameToNum(const string& day)
{
    const char** name = day.length() == 3 ? kWeekdayAbbr : kWeekdayFull;
    for (int i = 0; i <= 6; ++i) {
        if (NStr::CompareNocase(day, name[i]) == 0) {
            return i;
        }
    }
    // Always throw exceptions here.
    // Next if statements avoid compilation warnings.
    if ( name ) {
        NCBI_THROW(CTimeException, eArgument,
                   "Invalid day of week name '" + day + "'");
    }
    return -1;
}


string CTime::DayOfWeekNumToName(int day, ENameFormat fmt)
{
    if (day < 0  ||  day > 6) {
        return kEmptyStr;
    }
    return fmt == eFull ? kWeekdayFull[day] : kWeekdayAbbr[day];
}


void CTime::SetFormat(const CTimeFormat& fmt)
{
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using cleanup function.
    CTimeFormat* ptr = new CTimeFormat(fmt);
    s_TlsFormatTime.SetValue(ptr, CTlsBase::DefaultCleanup<CTimeFormat>);
}


CTimeFormat CTime::GetFormat(void)
{
    CTimeFormat fmt;
    CTimeFormat* ptr = s_TlsFormatTime.GetValue();
    if ( !ptr ) {
        fmt.SetFormat(kDefaultFormatTime);
    } else {
        fmt = *ptr;
    }
    return fmt;
}


// Internal version without checks and locks
time_t s_GetTimeT(const CTime& ct)
{
    struct tm t;

    // Convert time to time_t value at base local time
#if defined(HAVE_TIMEGM)  ||  defined(NCBI_OS_DARWIN)
    t.tm_sec   = ct.Second();
#else
    // see below
    t.tm_sec   = ct.Second() + (int)(ct.IsUniversalTime() ? -TimeZone() : 0);
#endif
    t.tm_min   = ct.Minute();
    t.tm_hour  = ct.Hour();
    t.tm_mday  = ct.Day();
    t.tm_mon   = ct.Month()-1;
    t.tm_year  = ct.Year()-1900;
    t.tm_isdst = -1;
#if defined(NCBI_OS_DARWIN)
    time_t tt = mktime(&t);
    if ( tt == (time_t)(-1L) ) {
        return tt;
    }
    return ct.IsUniversalTime() ? tt+t.tm_gmtoff : tt;
#elif defined(HAVE_TIMEGM)
    return ct.IsUniversalTime() ? timegm(&t) : mktime(&t);
#else
    struct tm *ttemp;
    time_t timer;
    timer = mktime(&t);
    if ( timer == (time_t)(-1L) ) {
        return timer;
    }

    // Correct timezone for UTC time
    if ( ct.IsUniversalTime() ) {

       // Somewhat hackish, but seem to work.
       // Local time is ambiguous and we don't know is DST on
       // for specified date/time or not, so we call mktime()
       // second time for GMT/UTC time:
       // 1st - to get correct value of TimeZone().
       // 2nd - to get value of "timer".

        t.tm_sec   = ct.Second() - (int)TimeZone();
        t.tm_min   = ct.Minute();
        t.tm_hour  = ct.Hour();
        t.tm_mday  = ct.Day();
        t.tm_mon   = ct.Month()-1;
        t.tm_year  = ct.Year()-1900;
        t.tm_isdst = -1;
        timer = mktime(&t);
        if ( timer == (time_t)(-1L) ) {
            return timer;
        }

#  if defined(HAVE_LOCALTIME_R)
        struct tm temp;
        localtime_r(&timer, &temp);
        ttemp = &temp;
#  else
        ttemp = localtime(&timer);
#  endif
        if (ttemp == NULL)
            return (time_t)(-1L);
        if (ttemp->tm_isdst > 0  &&  Daylight())
            timer -= DSTBias();  // +1 hour in common case
    }
    return timer;
#endif
}


// Internal version without checks and locks
bool s_IsDST(const CTime& ct)
{
    time_t timer = s_GetTimeT(ct);
    if (timer == (time_t)(-1L)) {
        return false;
    }
    struct tm *t;
#  if defined(HAVE_LOCALTIME_R)
    struct tm temp;
    localtime_r(&timer, &temp);
    t = &temp;
#  else
    t = localtime(&timer);
#  endif
    if (t == NULL) {
        return false;
    }
    return (t->tm_isdst > 0);
}


bool CTime::IsDST(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    // MT-Safe protect
    CMutexGuard LOCK(s_TimeMutex);
    return s_IsDST(*this);
}


time_t CTime::GetTimeT(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    // MT-Safe protect
    CMutexGuard LOCK(s_TimeMutex);
    return s_GetTimeT(*this);
}


struct tm CTime::GetTimeTM(void) const
{
    CTime lt = GetLocalTime();
    struct tm t;
    t.tm_sec   = lt.Second();
    t.tm_min   = lt.Minute();
    t.tm_hour  = lt.Hour();
    t.tm_mday  = lt.Day();
    t.tm_mon   = lt.Month()-1;
    t.tm_year  = lt.Year()-1900;
    t.tm_wday  = lt.DayOfWeek();
    t.tm_yday  = -1;
    t.tm_isdst = -1;
    return t;
}


CTime& CTime::SetTimeTM(const struct tm& t)
{
    CHECK_RANGE_YEAR   (t.tm_year + 1900);
    CHECK_RANGE_MONTH  (t.tm_mon + 1);
    CHECK_RANGE_DAY    (t.tm_mday);
    CHECK_RANGE_HOUR   (t.tm_hour);
    CHECK_RANGE_MIN    (t.tm_min);
    CHECK_RANGE_SEC    (t.tm_sec);

    SET_YEAR  (t.tm_year + 1900);
    SET_MONTH (t.tm_mon + 1);
    SET_DAY   (t.tm_mday);
    SET_HOUR  (t.tm_hour);
    SET_MIN   (t.tm_min);
    SET_SEC   (t.tm_sec);
    
    m_Data.nanosec     = 0;
    m_Data.tz          = eLocal;
    //m_Data.tzprec    -- not changed;
    m_Data.adjTimeDiff = 0;

    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid,
                   "Invalid time " + s_TimeDump(*this));
    }
    return *this;
}



TDBTimeU CTime::GetTimeDBU(void) const
{
    CTime t = GetLocalTime();
    unsigned first = s_Date2Number(CTime(1900, 1, 1));
    unsigned curr  = s_Date2Number(t);

    TDBTimeU dbt;
    dbt.days = (Uint2)(curr - first);
    dbt.time = (Uint2)(t.Hour() * 60 + t.Minute());
    return dbt;
}


TDBTimeI CTime::GetTimeDBI(void) const
{
    CTime t = GetLocalTime();
    unsigned first = s_Date2Number(CTime(1900, 1, 1));
    unsigned curr  = s_Date2Number(t);

    TDBTimeI dbt;
    dbt.days = (Int4)(curr - first);
    dbt.time = (Int4)((t.Hour() * 3600 + t.Minute() * 60 + t.Second()) * 300 +
                      (Int8(t.NanoSecond()) * 300) / kNanoSecondsPerSecond);
    return dbt;
}


CTime& CTime::SetTimeDBU(const TDBTimeU& t)
{
    // Local time - 1/1/1900 00:00:00.0
    CTime time(1900, 1, 1, 0, 0, 0, 0, eLocal);

    time.SetTimeZonePrecision(GetTimeZonePrecision());
    time.AddDay(t.days);
    time.AddMinute(t.time);
    time.ToTime(GetTimeZone());

    *this = time;
    return *this;
}


CTime& CTime::SetTimeDBI(const TDBTimeI& t)
{
    // Local time - 1/1/1900 00:00:00.0
    CTime time(1900, 1, 1, 0, 0, 0, 0, eLocal);

    time.SetTimeZonePrecision(GetTimeZonePrecision());
    time.AddDay(t.days);
    time.AddSecond(t.time / 300);
    time.AddNanoSecond(long((Int8(t.time % 300) * kNanoSecondsPerSecond)/300));
    time.ToTime(GetTimeZone());

    *this = time;
    return *this;
}


string CTime::AsString(const CTimeFormat& format, TSeconds out_tz) const
{
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid,
                   "Invalid time " + s_TimeDump(*this));
    }
    if ( IsEmpty() ) {
        return kEmptyStr;
    }

    // Time format

    string fmt;
    CTimeFormat::TFlags fmt_flags;
    if ( format.IsEmpty() ) {
        CTimeFormat f = GetFormat();
        fmt       = f.GetString();
        fmt_flags = f.GetFlags();
    } else {
        fmt       = format.GetString();
        fmt_flags = format.GetFlags();
    }
    bool is_escaped = ((fmt_flags & CTimeFormat::fFormat_Simple) == 0);
    bool is_format_symbol = !is_escaped;
    STzFormatMake tz_fmt_make;


    // Adjust time to timezone if necessary

    const CTime* t = this;
    CTime* t_out = 0;

#if defined(NCBI_TIMEZONE_IS_UNDEFINED)
    if (out_tz != eCurrentTimeZone) {
        ERR_POST_X(4, "Output timezone is unsupported on this platform");
    }
#else
    // Cache some not MT-safe values, so we can unlock mutex as fast as possible
    TSeconds x_timezone = 0, x_dstbias = 0;
    bool x_isdst = false;

    // Speedup:: timezone information can be used if defined or for 'z' format symbols.
    if (out_tz != eCurrentTimeZone  ||  fmt.find('z') != NPOS) 
    {{
        CMutexGuard LOCK(s_TimeMutex);
        // Adjust time for output timezone (should be MT protected)
        {{
            if (out_tz != eCurrentTimeZone) {
                if (out_tz != TimeZone()) {
                    t_out = new CTime(*this);
                    t_out->AddSecond(TimeZone() - out_tz);
                    t = t_out;
                }
            }
        }}
        // Cache values - now we should avoid to use functions specified at right side.
        x_timezone = TimeZone();
        x_dstbias  = DSTBias();
        x_isdst    = s_IsDST(*this);
    }}
#endif

    string str;
    str.reserve(64); // try to save on memory allocations

    ITERATE(string, it, fmt) {

        if ( !is_format_symbol ) {
            if ( *it == kFormatEscapeSymbol )  {
                is_format_symbol = true;
            } else {
                str += *it;
            }
            continue;
        }
        if ( is_escaped ) {
            is_format_symbol = false;
        }
        switch ( *it ) {
        case 'y': s_AddZeroPadInt2(str, t->Year() % 100);   break;
        case 'Y': s_AddZeroPadInt(str, t->Year(), 4);       break;
        case 'M': s_AddZeroPadInt2(str, t->Month());        break;
        case 'b': str += kMonthAbbr[t->Month()-1];          break;
        case 'B': str += kMonthFull[t->Month()-1];          break;
        case 'D': s_AddZeroPadInt2(str, t->Day());          break;
        case 'd': s_AddZeroPadInt(str, t->Day(),1);         break;
        case 'h': s_AddZeroPadInt2(str, t->Hour());         break;
        case 'H': s_AddZeroPadInt2(str, (t->Hour()+11) % 12+1);
                  break;
        case 'm': s_AddZeroPadInt2(str, t->Minute());       break;
        case 's': s_AddZeroPadInt2(str, t->Second());       break;
        case 'l': s_AddZeroPadInt(str, t->NanoSecond() / 1000000, 3);
                  break;
        case 'r': s_AddZeroPadInt(str, t->NanoSecond() / 1000, 6);
                  break;
        case 'S': s_AddZeroPadInt(str, t->NanoSecond(), 9); break;
        case 'G': s_AddZeroPadInt2(str, t->Second());
                  str += ".";
                  s_AddZeroPadInt(str, t->NanoSecond(), 9, true);
                  break;
        case 'g': s_AddInt(str, t->Second());
                  str += ".";
                  s_AddZeroPadInt(str, t->NanoSecond(), 9, true);
                  break;
        case 'p': str += ( t->Hour() < 12) ? "am" : "pm" ;  break;
        case 'P': str += ( t->Hour() < 12) ? "AM" : "PM" ;  break;
        case 'o': tz_fmt_make.activate(); /* FALL THROUGH */
        case 'z': {
#if defined(NCBI_TIMEZONE_IS_UNDEFINED)
                  ERR_POST_X(5, "Format symbol 'z' is unsupported "
                                "on this platform");
#else
                  if (!tz_fmt_make.active()) {
                      str += (fmt_flags & CTimeFormat::fConf_UTC) ? "UTC" : "GMT";
                  }
                  if (IsUniversalTime()) {
                      break;
                  }
                  TSeconds tz = out_tz;
                  if (tz == eCurrentTimeZone) {
                     tz = x_timezone;
                     if (x_isdst) {
                        tz += x_dstbias;  // DST in effect
                     }
                  }
                  str += (tz > 0) ? '-' : '+';
                  if (tz < 0) tz = -tz;
                  tz /= 60;
                  s_AddZeroPadInt2(str, (long)(tz / 60));
                  tz_fmt_make.str(str);
                  s_AddZeroPadInt2(str, (long)(tz % 60));
#endif
                  break;
                  }
        case 'Z': if (IsUniversalTime()) {
                      str += (fmt_flags & CTimeFormat::fConf_UTC) ? "UTC" : "GMT";
                  }
                  break;
        case 'w': str += kWeekdayAbbr[t->DayOfWeek()];      break;
        case 'W': str += kWeekdayFull[t->DayOfWeek()];      break;
        default : str += *it;                               break;
        }
    }
    // Free used memory
    if ( t_out ) {
        delete t_out;
    }
    return str;
}


CTime& CTime::x_SetTimeMTSafe(const time_t* value)
{
    // MT-Safe protect
    CMutexGuard LOCK(s_TimeMutex);
    x_SetTime(value);
    return *this;
}


void CTime::GetCurrentTimeT(time_t* sec, long* nanosec)
{
    _ASSERT(sec);
    long ns;
#if   defined(NCBI_OS_MSWIN)
    FILETIME systime;
    Uint8    systemp;

    GetSystemTimeAsFileTime(&systime);

    systemp   = systime.dwHighDateTime;
    systemp <<= 32;
    systemp  |= systime.dwLowDateTime;
    *sec      =  systemp / 10000000  - NCBI_CONST_UINT8(11644473600);
    ns        = (systemp % 10000000) * 100;
#elif defined(NCBI_OS_UNIX)
    struct timeval tp;
    if (gettimeofday(&tp,0) == 0) {
        *sec = tp.tv_sec;
        ns   = tp.tv_usec * (kNanoSecondsPerSecond / kMicroSecondsPerSecond);
    } else {
        *sec = (time_t)(-1L);
        ns   = 0;
    }
#else
    *sec = time(0);
    ns   = 0;
#endif
    if (*sec == (time_t)(-1L)) {
        NCBI_THROW(CTimeException, eConvert,
                   "Unable to get time value");
    }
    if (nanosec) {
        *nanosec = ns;
    }
}


CTime& CTime::x_SetTime(const time_t* value)
{
    time_t timer;
    long ns = 0;

    // Get time with nanoseconds
    if ( value ) {
        timer = *value;
    } else {
        GetCurrentTimeT(&timer, &ns);
    }

    // Bind values to internal variables
    struct tm *t;

#ifdef HAVE_LOCALTIME_R
    struct tm temp;
    if (GetTimeZone() == eLocal) {
        localtime_r(&timer, &temp);
    } else {
        gmtime_r(&timer, &temp);
    }
    t = &temp;
#else
    t = ( GetTimeZone() == eLocal ) ? localtime(&timer) : gmtime(&timer);
    if ( !t ) {
        // Error was detected: incorrect timer value or system error
        NCBI_THROW(CTimeException, eConvert, 
                   "localtime/gmtime error, possible incorrect time_t value");
    }
#endif
    m_Data.adjTimeDiff = 0;
    
    SET_YEAR  (t->tm_year + 1900);
    SET_MONTH (t->tm_mon + 1);
    SET_DAY   (t->tm_mday);
    SET_HOUR  (t->tm_hour);
    SET_MIN   (t->tm_min);
    SET_SEC   (t->tm_sec);
    
    CHECK_RANGE_NSEC(ns);
    m_Data.nanosec     = (Int4)ns;

    return *this;
}


CTime& CTime::AddMonth(int months, EDaylight adl)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if ( !months ) {
        return *this;
    }
    CTime *pt = 0;
    bool aflag = false;
    if ((adl == eAdjustDaylight)  &&  x_NeedAdjustTime()) {
        pt = new CTime(*this);
        if ( !pt ) {
            NCBI_THROW(CCoreException, eNullPtr, kEmptyStr);
        }
        aflag = true;
    }
    long newMonth = Month() - 1;
    int newYear = Year();
    s_Offset(&newMonth, months, 12, &newYear);
    SET_YEAR(newYear);
    SET_MONTH(newMonth + 1);
    x_AdjustDay();
    if ( aflag ) {
        x_AdjustTime(*pt);
        delete pt;
    }
    return *this;
}


CTime& CTime::AddDay(int days, EDaylight adl)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if ( !days ) {
        return *this;
    }
    CTime *pt = 0;
    bool aflag = false;
    if ((adl == eAdjustDaylight)  &&  x_NeedAdjustTime()) {
        pt = new CTime(*this);
        if ( !pt ) {
            NCBI_THROW(CCoreException, eNullPtr, kEmptyStr);
        }
        aflag = true;
    }

    // Make necessary object
    *this = s_Number2Date(s_Date2Number(*this) + days, *this);

    // If need, make adjustment time specially
    if ( aflag ) {
        x_AdjustTime(*pt);
        delete pt;
    }
    return *this;
}


// Parameter <shift_time> access or denied use time shift in process
// adjust hours.
CTime& CTime::x_AddHour(int hours, EDaylight adl, bool shift_time)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if ( !hours ) {
        return *this;
    }
    CTime *pt = 0;
    bool aflag = false;
    if ((adl == eAdjustDaylight)  &&  x_NeedAdjustTime()) {
        pt = new CTime(*this);
        if ( !pt ) {
            NCBI_THROW(CCoreException, eNullPtr, kEmptyStr);
        }
        aflag = true;
    }
    int dayOffset = 0;
    long newHour = Hour();
    s_Offset(&newHour, hours, 24, &dayOffset);
    SET_HOUR(newHour);
    AddDay(dayOffset, eIgnoreDaylight);
    if ( aflag ) {
        x_AdjustTime(*pt, shift_time);
        delete pt;
    }
    return *this;
}


CTime& CTime::AddMinute(int minutes, EDaylight adl)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if ( !minutes ) {
        return *this;
    }
    CTime *pt = 0;
    bool aflag = false;
    if ((adl == eAdjustDaylight) && x_NeedAdjustTime()) {
        pt = new CTime(*this);
        if ( !pt ) {
            NCBI_THROW(CCoreException, eNullPtr, kEmptyStr);
        }
        aflag = true;
    }
    int hourOffset = 0;
    long newMinute = Minute();
    s_Offset(&newMinute, minutes, 60, &hourOffset);
    SET_MIN(newMinute);
    AddHour(hourOffset, eIgnoreDaylight);
    if ( aflag ) {
        x_AdjustTime(*pt);
        delete pt;
    }
    return *this;
}


CTime& CTime::AddSecond(TSeconds seconds, EDaylight adl)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if ( !seconds ) {
        return *this;
    }
    int minuteOffset = 0;
    long newSecond = Second();
    s_Offset(&newSecond, seconds, 60, &minuteOffset);
    SET_SEC(newSecond);
    return AddMinute(minuteOffset, adl);
}


CTime& CTime::AddNanoSecond(long ns)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if ( !ns ) {
        return *this;
    }
    int secondOffset = 0;
    long newNanoSecond = NanoSecond();
    s_Offset(&newNanoSecond, ns, kNanoSecondsPerSecond, &secondOffset);
    m_Data.nanosec = (Int4)newNanoSecond;
    return AddSecond(secondOffset);
}


CTime& CTime::AddTimeSpan(const CTimeSpan& ts)
{
    if ( ts.GetSign() == eZero ) {
        return *this;
    }
    AddSecond(ts.GetCompleteSeconds());
    AddNanoSecond(ts.GetNanoSecondsAfterSecond());
    return *this;
}


CTime& CTime::Round(ERoundPrecision precision, EDaylight adl)
{
    if ( IsEmptyDate() ) {
        return *this;
    }
    switch (precision) {
        case eRound_Day:
            if ( m_Data.hour >= 12 )
                AddDay(1, adl);
            break;
        case eRound_Hour:
            if ( m_Data.min >= 30 )
                AddHour(1, adl);
            break;
        case eRound_Minute:
            if ( m_Data.sec >= 30 )
                AddMinute(1, adl);
            break;
        case eRound_Second:
            if ( m_Data.nanosec >= kNanoSecondsPerSecond/2 )
                AddSecond(1, adl);
            m_Data.nanosec = 0;
            break;
        case eRound_Millisecond:
            m_Data.nanosec = 
                (Int4)(m_Data.nanosec + kNanoSecondsPerSecond/2000) 
                / 1000000 * 1000000;
            break;
        case eRound_Microsecond:
            m_Data.nanosec = 
                (Int4)(m_Data.nanosec + kNanoSecondsPerSecond/2000000)
                / 1000 * 1000;
            break;
        default:
            NCBI_THROW(CTimeException, eArgument,
                       "Rounding precision is out of range");
    }
    if ( m_Data.nanosec == kNanoSecondsPerSecond ) {
        AddSecond(1, adl);
        m_Data.nanosec = 0;
    }
    // Clean time components with lesser precision
    Truncate(precision);
    return *this;
}


CTime& CTime::Truncate(ERoundPrecision precision)
{
    // Clean time components with lesser precision
    switch (precision) {
        case eRound_Day:
            m_Data.hour = 0;
            // fall through
        case eRound_Hour:
            m_Data.min = 0;
            // fall through
        case eRound_Minute:
            m_Data.sec = 0;
            // fall through
        case eRound_Second:
            m_Data.nanosec = 0;
            break;
        case eRound_Millisecond:
            m_Data.nanosec = m_Data.nanosec / 1000000 * 1000000;
            break;
        case eRound_Microsecond:
            m_Data.nanosec = m_Data.nanosec / 1000 * 1000;
            break;
        default:
            break;
    }
    return *this;
}


CTime& CTime::Clear()
{
    m_Data.year        = 0;
    m_Data.month       = 0;
    m_Data.day         = 0;
    m_Data.hour        = 0;
    m_Data.min         = 0;
    m_Data.sec         = 0;
    m_Data.nanosec     = 0;
    m_Data.adjTimeDiff = 0;
    return *this;
}


bool CTime::IsValid(void) const
{
    if ( IsEmpty() )
        return true;

    _ASSERT(m_Data.tz);
    if ( !m_Data.tz ) {
        return false;
    }

    if (Year() < 1583) // first Gregorian date February 24, 1582
        return false;
    if (Month()  < 1  ||  Month()  > 12)
        return false;
    if (Month() == 2) {
        if (Day() < 1 ||  Day() > (IsLeap() ? 29 : 28))
            return false;
    } else {
        if (Day() < 1 ||  Day() > s_DaysInMonth[Month() - 1])
            return false;
    }
    if (Hour()   < 0  ||  Hour()   > 23)
        return false;
    if (Minute() < 0  ||  Minute() > 59)
        return false;
    // leap seconds are supported
    if (Second() < 0  ||  Second() > 61)
        return false;
    if (NanoSecond() < 0  ||  NanoSecond() >= kNanoSecondsPerSecond)
        return false;

    return true;
}


CTime CTime::GetLocalTime(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if ( IsLocalTime() ) {
        return *this;
    }
    CTime t(*this);
    return t.ToLocalTime();
}


CTime CTime::GetUniversalTime(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if ( IsUniversalTime() ) {
        return *this;
    }
    CTime t(*this);
    return t.ToUniversalTime();
}


CTime& CTime::ToTime(ETimeZone tz)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eArgument, "The date is empty");
    }
    if (GetTimeZone() != tz) {
        struct tm* t;
        time_t timer;
        timer = GetTimeT();
        if (timer == (time_t)(-1L)) {
            return *this;
        }
        // MT-Safe protect
        CMutexGuard LOCK(s_TimeMutex);

#if defined(HAVE_LOCALTIME_R)
        struct tm temp;
        if (tz == eLocal) {
            localtime_r(&timer, &temp);
        }
        else {
            gmtime_r(&timer, &temp);
        }
        t = &temp;
#else
        t = (tz == eLocal) ? localtime(&timer) : gmtime(&timer);
        if (!t) {
            // Error was detected: incorrect timer value or system error
            NCBI_THROW(CTimeException, eConvert,
                "localtime/gmtime error, possible incorrect time_t value");
        }
#endif
        LOCK.Release();

        SET_YEAR  (t->tm_year + 1900);
        SET_MONTH (t->tm_mon + 1);
        SET_DAY   (t->tm_mday);
        SET_HOUR  (t->tm_hour);
        SET_MIN   (t->tm_min);
        SET_SEC   (t->tm_sec);
        m_Data.tz    = tz;
    }
    return *this;
}


bool CTime::operator== (const CTime& t) const
{
    CTime tmp(t);
    if ( !tmp.IsEmptyDate() ) {
        tmp.ToTime(GetTimeZone());
    }
    return
        Year()       == tmp.Year()    &&
        Month()      == tmp.Month()   &&
        Day()        == tmp.Day()     &&
        Hour()       == tmp.Hour()    &&
        Minute()     == tmp.Minute()  &&
        Second()     == tmp.Second()  &&
        NanoSecond() == tmp.NanoSecond();
}


bool CTime::operator> (const CTime& t) const
{
    CTime tmp(t);
    if ( !tmp.IsEmptyDate() ) {
        tmp.ToTime(GetTimeZone());
    }
    if (Year()   > tmp.Year())
        return true;
    if (Year()   < tmp.Year())
        return false;
    if (Month()  > tmp.Month())
        return true;
    if (Month()  < tmp.Month())
        return false;
    if (Day()    > tmp.Day())
        return true;
    if (Day()    < tmp.Day())
        return false;
    if (Hour()   > tmp.Hour())
        return true;
    if (Hour()   < tmp.Hour())
        return false;
    if (Minute() > tmp.Minute())
        return true;
    if (Minute() < tmp.Minute())
        return false;
    if (Second() > tmp.Second())
        return true;
    if (Second() < tmp.Second())
        return false;
    if (NanoSecond() > tmp.NanoSecond())
        return true;

    return false;
}


bool CTime::operator< (const CTime& t) const
{
    CTime tmp(t);
    if ( !tmp.IsEmptyDate() ) {
        tmp.ToTime(GetTimeZone());
    }
    if (Year()   < tmp.Year())
        return true;
    if (Year()   > tmp.Year())
        return false;
    if (Month()  < tmp.Month())
        return true;
    if (Month()  > tmp.Month())
        return false;
    if (Day()    < tmp.Day())
        return true;
    if (Day()    > tmp.Day())
        return false;
    if (Hour()   < tmp.Hour())
        return true;
    if (Hour()   > tmp.Hour())
        return false;
    if (Minute() < tmp.Minute())
        return true;
    if (Minute() > tmp.Minute())
        return false;
    if (Second() < tmp.Second())
        return true;
    if (Second() > tmp.Second())
        return false;
    if (NanoSecond() < tmp.NanoSecond())
        return true;

    return false;
}


bool CTime::IsLeap(void) const
{
    int year = Year();
    return (year % 4 == 0  &&  year % 100 != 0)  ||  year % 400 == 0;
}


TSeconds CTime::TimeZoneOffset(void) const
{
    const CTime tl(GetLocalTime());
    const CTime tg(GetUniversalTime());

    TSeconds dSecs  = tl.Second() - tg.Second();
    TSeconds dMins  = tl.Minute() - tg.Minute();
    TSeconds dHours = tl.Hour()   - tg.Hour();
    TSeconds dDays  = tl.DiffWholeDays(tg);
    return ((dDays * 24 + dHours) * 60 + dMins) * 60 + dSecs;
}


string CTime::TimeZoneOffsetStr(void)
{
    int tz = (int)(TimeZoneOffset() / 60);
    string str;
    str.reserve(5);
    if (tz > 0) {
        str = '+';
    } else {
        str = '-';
        tz = -tz;
    }
    s_AddZeroPadInt2(str, tz / 60);
    s_AddZeroPadInt2(str, tz % 60);
    return str;
}


string CTime::TimeZoneName(void)
{
    time_t timer = GetTimeT();
    if (timer == (time_t)(-1L)) {
       return kEmptyStr;
    }
    // MT-Safe protect
    CMutexGuard LOCK(s_TimeMutex);
    
    struct tm* t;
#if defined(HAVE_LOCALTIME_R)
    struct tm temp;
    localtime_r(&timer, &temp);
    t = &temp;
#else
    t = localtime(&timer);
#endif
    if ( !t ) {
        return kEmptyStr;
    }
#if defined(__USE_BSD)
    if (t->tm_zone) {
        return t->tm_zone;
    }
#endif
    return t->tm_isdst > 0 ? TZName()[1] : TZName()[0];
}


int CTime::DiffWholeDays(const CTime& t) const
{
    return int(s_Date2Number(*this) - s_Date2Number(t));
}

TSeconds CTime::DiffSecond(const CTime& from) const
{ 
    const CTime* p1, *p2;
    CTime        t1,  t2;
    if (GetTimeZone() != from.GetTimeZone()) {
        t1 = *this;
        t2 =  from;
        t1.ToUniversalTime();
        t2.ToUniversalTime();
        p1 = &t1;
        p2 = &t2;
    } else {
        p1 =  this;
        p2 = &from;
    }
    TSeconds dSecs  = p1->Second() - p2->Second();
    TSeconds dMins  = p1->Minute() - p2->Minute();
    TSeconds dHours = p1->Hour()   - p2->Hour();
    TSeconds dDays  = p1->DiffWholeDays(*p2);
    return ((dDays * 24 + dHours) * 60 + dMins) * 60 + dSecs;
}


CTimeSpan CTime::DiffTimeSpan(const CTime& t) const
{
    TSeconds sec = DiffSecond(t);
    if (sec < kMin_Long  || sec > kMax_Long) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Difference in time " +
                   NStr::Int8ToString(sec) + 
                   " is too big to convert to CTimeSpan");
    }
    return CTimeSpan((long)sec , NanoSecond() - t.NanoSecond());
}


void CTimeSpan::Set(double seconds)
{
    if (seconds < (double)kMin_Long  || seconds > (double)kMax_Long) {
        NCBI_THROW(CTimeException, eConvert, 
                  "Value " + NStr::DoubleToString(seconds) +
                  " is too big to convert to CTimeSpan");
    }
    m_Sec = long(seconds);
    m_NanoSec = long((seconds - (double)m_Sec) * kNanoSecondsPerSecond);
    x_Normalize();
}


void CTime::x_AdjustDay()
{
    int n_days = DaysInMonth();
    if (Day() > n_days) {
        SET_DAY(n_days);
    }
}


CTime& CTime::x_AdjustTime(const CTime& from, bool shift_time)
{
    if ( !x_NeedAdjustTime() )
        return *this;

    switch ( GetTimeZonePrecision() ) {
    case eMinute:
        if (Minute() != from.Minute())
            return x_AdjustTimeImmediately(from, shift_time);
    case eHour:
        if (Hour() != from.Hour())
            return x_AdjustTimeImmediately(from, shift_time);
    case eDay:
        if (Day() != from.Day())
            return x_AdjustTimeImmediately(from, shift_time);
    case eMonth:
        if (Month() != from.Month())
            return x_AdjustTimeImmediately(from, shift_time);
    default:
        break;
    }
    return *this;
}


CTime& CTime::x_AdjustTimeImmediately(const CTime& from, bool shift_time)
{
    // Time in hours for temporary time shift.
    // Shift used for obtainment correct result at changeover daytime saving.
    // Must be > 3 (Linux distinction). On other platforms may be == 3.
    const int kShiftHours = 4;

    // MT-Safe protect
    CMutexGuard LOCK(s_TimeAdjustMutex);

    // Special conversion from <const CTime> to <CTime>
    CTime tmp(from);
    int sign = 0;
    TSeconds diff = 0;
    // Primary procedure call
    if ( shift_time ) {
        sign = ( *this > from ) ? 1 : -1;
        // !!! Run TimeZoneOffset() first for old time value
        diff = -tmp.TimeZoneOffset() + TimeZoneOffset();
        // Correction needs if time already in identical timezone
        if (!diff  ||  diff == m_Data.adjTimeDiff) {
            return *this;
        }
    }
    // Recursive procedure call. Inside below
    // x_AddHour(*, eAdjustDaylight, false)
    else  {
        // Correction needn't if difference not found
        if (diff == m_Data.adjTimeDiff) {
            return *this;
        }
    }
    // Make correction with temporary time shift
    time_t t = GetTimeT();
    CTime tn(t + (time_t)diff + 3600 * kShiftHours * sign);
    if (from.GetTimeZone() == eLocal) {
        tn.ToLocalTime();
    }
    tn.SetTimeZonePrecision(GetTimeZonePrecision());

    // Release adjust time mutex
    LOCK.Release();

    // Primary procedure call
    if ( shift_time ) {
        // Cancel temporary time shift
        tn.x_AddHour(-kShiftHours * sign, eAdjustDaylight, false);
        tn.m_Data.adjTimeDiff = (Int4)diff;  /* NCBI_FAKE_WARNING */
    }
    *this = tn;
    return *this;
}



//=============================================================================
//
//  CTimeSpan
//
//=============================================================================


CTimeSpan::CTimeSpan(long days, long hours, long minutes, long seconds,
                     long nanoseconds)
{
    TSeconds sec = (((TSeconds)days*24 + hours)*60 + minutes)*60 +
                   seconds + nanoseconds/kNanoSecondsPerSecond;        
    if (sec < kMin_Long  || seconds > kMax_Long) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Value (" +
                   NStr::Int8ToString(days)    + ", " +
                   NStr::Int8ToString(hours)   + ", " +
                   NStr::Int8ToString(minutes) + ", " +
                   NStr::Int8ToString(seconds) +
                   ", nanosec) is too big to convert to CTimeSpan");
    }
    m_Sec     = (long)sec;
    m_NanoSec = nanoseconds % kNanoSecondsPerSecond;
    x_Normalize();
}


CTimeSpan::CTimeSpan(const string& str, const CTimeFormat& fmt)
{
    if (fmt.IsEmpty()) {
        // Use another format to init from string by default
        // if global format has not set.
        CTimeFormat* ptr = s_TlsFormatSpan.GetValue();
        if (!ptr) {
            CTimeFormat default_fmt(kDefaultFormatSpanIn);
            x_Init(str, default_fmt);
        } else {
            x_Init(str, *ptr);
        }
    } else {
        x_Init(str, fmt);
    }
}


CTimeSpan& CTimeSpan::operator= (const string& str)
{
    // Use another format to init from string by default
    // if global format has not set.
    CTimeFormat* ptr = s_TlsFormatSpan.GetValue();
    if (!ptr) {
        CTimeFormat fmt(kDefaultFormatSpanIn);
        x_Init(str, fmt);
    } else {
        x_Init(str, *ptr);
    }
    return *this;
}


void CTimeSpan::x_Init(const string& str, const CTimeFormat& format)
{
    Clear();
    if ( str.empty() ) {
        return;
    }
    const string& fmt = format.GetString();
    bool is_escaped_fmt = ((format.GetFlags() & CTimeFormat::fFormat_Simple) == 0);
    bool is_escaped_symbol = false;

    const char* fff;
    char  f;
    const char* sss = str.c_str();
    int   sign = 1;

    for (fff = fmt.c_str();  *fff != '\0';  ++fff) {

        // Skip preceding symbols for some formats
        if (is_escaped_fmt  &&  !is_escaped_symbol) {
            if (*fff == kFormatEscapeSymbol)  {
                is_escaped_symbol = true;
                continue;
            }
        }

        // Match non-format symbols

        if (is_escaped_fmt) {
            if (!(is_escaped_symbol  &&  *fff != kFormatEscapeSymbol)) {
                // Match non-format symbols
                if (*fff == *sss) {
                    ++sss;
                    continue;
                }
                break;  // error: non-matching symbols
            }
            // format symbol found, process as usual
            is_escaped_symbol = false;
        } else {
            // Regular non-escaped format, each symbol can be a format symbol.
            // Check allowed format symbols
            if (strchr(kFormatSymbolsSpan, *fff) == 0) {
                // Match non-format symbols
                if (*fff == *sss) {
                    ++sss;
                    continue;
                }
                break;  // error: non-matching symbols
            }
        }

        // Process format symbols

        // Sign: if specified that the time span is negative
        if (*fff == '-') {
            if (*sss == '-') {
                sign = -1;
                ++sss;
            }
            continue;
        }
        f = *fff;

    read_next_value:

        // Other format symbols -- read the next data ingredient
        char value_str[11];
        char* s = value_str;
        // Read 9 (special case) or 10 digits only, ignore all other.
        size_t len = (f == '\1') ? 9 : 10;
        for (; len  &&  *sss  &&  isdigit((unsigned char)(*sss));  --len) {
            *s++ = *sss++;
        }
        *s = '\0';
        long value = NStr::StringToLong(value_str);

        switch ( f ) {
        case 'd':
            m_Sec += value * 3600L * 24L;
            break;
        case 'h':
            m_Sec += value * 3600L;
            break;
        case 'H':
            m_Sec = value * 3600L;
            break;
        case 'm':
            m_Sec += value * 60L;
            break;
        case 'M':
            m_Sec = value * 60L;
            break;
        case 's':
            m_Sec += value;
            break;
        case 'S':
            m_Sec = value;
            break;
        case 'n':
            m_NanoSec = value;
            break;
        case 'g':
            m_Sec += value;
            if ( *sss == '.' ) {
                // Read fraction
                ++sss;
                f = '\1';
                goto read_next_value;
            }
            break;
        case 'G':
            m_Sec = value;
            if ( *sss == '.' ) {
                // Read fraction
                ++sss;
                f = '\1';
                goto read_next_value;
            }
            break;
        case '\1':
            {
                // Special format symbol.
                // Process a fraction of a second for format symbols 'g' and 'G'.
                // Convert value to nanoseconds.
                size_t n = strlen(value_str);
                // 'n' cannot have more then 9 (max for nanoseconds)
                // ignore all other digits
                for (;  n < 9;  ++n) {
                    value *= 10;
                }
                m_NanoSec = value;
                // Limit fraction of second to 9 digits max,
                // ignore all other digits in string if any.
                while ( isdigit((unsigned char)(*sss)) ) {
                    ++sss;
                }
            }
            break;
        default:
            NCBI_THROW(CTimeException, eFormat, "Format '" + fmt + "' has incorrect format symbol '" + f + "'");
        }
    }

    // Check on errors
    if (*fff != '\0'  &&  
        !(format.GetFlags() & CTimeFormat::fMatch_ShortTime)) {
        NCBI_THROW(CTimeException, eFormat, 
                   "Time string '" + str +
                   "' is too short for time format '" + fmt + "'");
    }
    if (*sss != '\0'  && 
        !(format.GetFlags() & CTimeFormat::fMatch_ShortFormat)) {
        NCBI_THROW(CTimeException, eFormat,
                   "Time string '" + str +
                   "' is too long for time format '" + fmt + "'");
    }
    // Normalize time span
    if (sign < 0) {
        Invert();
    }
    x_Normalize();
}


void CTimeSpan::x_Normalize(void)
{
    m_Sec += m_NanoSec / kNanoSecondsPerSecond;
    m_NanoSec %= kNanoSecondsPerSecond;
    // If signs are different then make timespan correction
    if (m_Sec > 0  &&  m_NanoSec < 0) {
        --m_Sec;
        m_NanoSec += kNanoSecondsPerSecond;
    } else if (m_Sec < 0  &&  m_NanoSec > 0) {
        ++m_Sec;
        m_NanoSec -= kNanoSecondsPerSecond;
    }
}


void CTimeSpan::SetFormat(const CTimeFormat& fmt)
{
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using cleanup function.
    CTimeFormat* ptr = new CTimeFormat(fmt);
    s_TlsFormatSpan.SetValue(ptr, CTlsBase::DefaultCleanup<CTimeFormat>);
}


CTimeFormat CTimeSpan::GetFormat(void)
{
    CTimeFormat fmt;
    CTimeFormat* ptr = s_TlsFormatSpan.GetValue();
    if ( !ptr ) {
        fmt.SetFormat(kDefaultFormatSpan);
    } else {
        fmt = *ptr;
    }
    return fmt;
}


string CTimeSpan::AsString(const CTimeFormat& format) const
{
    string str;
    str.reserve(64); // try to save on memory allocations
    string fmt;
    CTimeFormat::TFlags fmt_flags;

    if ( format.IsEmpty() ) {
        CTimeFormat f = GetFormat();
        fmt       = f.GetString();
        fmt_flags = f.GetFlags();
    } else {
        fmt       = format.GetString();
        fmt_flags = format.GetFlags();
    }
    bool is_escaped = ((fmt_flags & CTimeFormat::fFormat_Simple) == 0);
    bool is_format_symbol = !is_escaped;

    ITERATE(string, it, fmt) {

        if ( !is_format_symbol ) {
            if ( *it == kFormatEscapeSymbol )  {
                is_format_symbol = true;
            } else {
                str += *it;
            }
            continue;
        }
        if ( is_escaped ) {
            is_format_symbol = false;
        }
        switch ( *it ) {
        case '-': if (GetSign() == eNegative) {
                      str += "-";
                  }
                  break;
        case 'd': s_AddInt(str, abs(GetCompleteDays()));
                  break;
        case 'h': s_AddZeroPadInt2(str, abs(x_Hour()));
                  break;
        case 'H': s_AddInt(str, abs(GetCompleteHours()));
                  break;
        case 'm': s_AddZeroPadInt2(str, abs(x_Minute()));
                  break;
        case 'M': s_AddInt(str, abs(GetCompleteMinutes()));
                  break;
        case 's': s_AddZeroPadInt2(str, abs(x_Second()));
                  break;
        case 'S': s_AddInt(str, abs(GetCompleteSeconds()));
                  break;
        case 'n': s_AddZeroPadInt(str, abs(GetNanoSecondsAfterSecond()), 9);
                  break;
        case 'g': s_AddInt(str, abs(x_Second()));
                  str += ".";
                  s_AddZeroPadInt(str, abs(GetNanoSecondsAfterSecond()), 9, true);
                  break;
        case 'G': s_AddInt(str, abs(GetCompleteSeconds()));
                  str += ".";
                  s_AddZeroPadInt(str, abs(GetNanoSecondsAfterSecond()), 9, true);
                  break;
        default : str += *it;
                  break;
        }
    }
    return str;
}


// Number of time components
const unsigned int kUnitCount = 9;

// Time components
enum EUnit {
    eYear        = 0,
    eMonth       = 1,
    eDay         = 2,
    eHour        = 3,
    eMinute      = 4,
    eSecond      = 5,
    eMillisecond = 6,
    eMicrosecond = 7,
    eNanosecond  = 8,

    // Special value, some precision based (see fSS_Precision*).
    // Must be last and go after eNanosecond.
    ePrecision   = 9
};

// Structure to store time component names/abbreviations
struct SUnitName {
    const char* name_full;
    const char* name_short;
};
static const SUnitName kUnitNames[kUnitCount] = {
    { "year",         "y" },
    { "month",        "mo"},
    { "day",          "d" },
    { "hour",         "h" },
    { "minute",       "m" },
    { "second",       "s" },
    { "millisecond",  "ms" },
    { "microsecond",  "us" },
    { "nanosecond",   "ns" }
};

// Multipliers for parts of time in seconds/nanoseconds
static const TSeconds kTimeSpanUnitMultipliers[kUnitCount] = {
    // second based parts
    kAverageSecondsPerYear,  // year
    kAverageSecondsPerMonth, // month
    3600L * 24L,             // day
    3600L,                   // hour
    60L,                     // minute
    1,                       // seconds
    // nanosecond based parts
    1000000L,                // milliseconds
    1000L,                   // microseconds
    1                        // nanoseconds
};


// Helper functions to convert to smart string for "smart" mode
// and timespans >= 1 min. Do not use with smaller timespans!

string CTimeSpan::x_AsSmartString_Smart_Big(TSmartStringFlags flags) const
{
    _ASSERT(*this >= CTimeSpan(60,0) );
    _ASSERT(GetSign() != eNegative);
    _ASSERT((flags & fSS_Smart) == fSS_Smart);

    // Make a copy (for rounding)
    CTimeSpan ts(*this);

    // Round timespan
    if ( flags & fSS_Round ) {

        // Find first non-zero value for proper rounding (2 level max)
        long sec = GetCompleteSeconds();
        int  adjust_level;
        for (adjust_level = eYear; adjust_level < eSecond; adjust_level++) {
            if (sec >= kTimeSpanUnitMultipliers[adjust_level]) {
                break;
            }
        }
        _ASSERT(adjust_level < eMicrosecond);
        ++adjust_level;  // one level down

        // Add adjustment time span
        switch ( EUnit(adjust_level) ) {
            case eMonth:
                ts += CTimeSpan(15, 0, 0, 0);
                break;
            case eDay:
                ts += CTimeSpan(0, 12, 0, 0);
                break;
            case eHour:
                ts += CTimeSpan(0, 0, 30, 0);
                break;
            case eMinute:
                ts += CTimeSpan(0, 0, 0, 30);
                break;
            case eSecond:
                ts += CTimeSpan(0, 0, 0, 0, kNanoSecondsPerSecond/2);
                break;
            default:
                ; // years, seconds and below -- nothing to do
        }
    }

    // Prepare data
    const int max_count = 6;
    long span[max_count];
    long sec = ts.GetCompleteSeconds();

    span[eYear]   = sec/kAverageSecondsPerYear;   sec %= kAverageSecondsPerYear;
    span[eMonth]  = sec/kAverageSecondsPerMonth;  sec %= kAverageSecondsPerMonth;
    span[eDay]    = sec/(3600L*24);               sec %= (3600L*24);
    span[eHour]   = sec/3600L;                    sec %= 3600L;
    span[eMinute] = sec/60L;                      sec %= 60L;
    span[eSecond] = sec;

    string result;   // result string
    int start = 0;   // precision level to start from (first non-zero)
    int prec;        // end precision

    // Get start precision after rounding (it can changes there)
    for (; start < 5;  ++start) {
        if ( span[start] ) {
            break;
        }
    }
    // Allow 2 levels maximum for smart mode
    if (start == eSecond) {
        prec = start;
    } else {
        prec = start + 1;
    }

    // Compose result string
    while (start <= prec) {
        long val = span[start];
        if ( !val ) {
            ++start;
            continue;
        }
        if ( !result.empty() ) {
            result += " ";
        }
        result += NStr::LongToString(val);
        if (flags & fSS_Full) {
            result += string(" ") + kUnitNames[start].name_full;
            if (val != 1) {
                result += "s";
            }
        } else {
            result += kUnitNames[start].name_short;
        }
        ++start;
    }
    return result;
}


// Helper functions to convert to smart string for "smart" mode
// and timespans < 1 min. Do not use with sbigger timespans!

string CTimeSpan::x_AsSmartString_Smart_Small(TSmartStringFlags flags) const
{
    _ASSERT(*this < CTimeSpan(60, 0));
    _ASSERT(GetSign() != eNegative);
    _ASSERT((flags & fSS_Smart) == fSS_Smart);

    // Get timespan components
    int  sec = x_Second();
    long nanoseconds  = GetNanoSecondsAfterSecond();
    // Split nanoseconds to 3 digit parts AAABBBCCC
    // AAA - milli, BBB - micro, CCC - nano
    int  milli = int(nanoseconds / 1000000);
    int  micro = int(nanoseconds / 1000 % 1000);
    int  nano  = int(nanoseconds % 1000);

    // We would like to have 3 digits in result,
    // so we need 2 components max.
    EUnit unit = eSecond;
    long  v1 = 0;
    long  v2 = 0;

    if ( sec ) {
        v1 = sec;
        v2 = milli;
    } else if ( milli ) {
        unit = eMillisecond;
        v1 = milli;
        v2 = micro;
    } else if ( micro ) {
        unit = eMicrosecond;
        v1 = micro;
        v2 = nano;
    } else if ( nano ) {
        unit = eNanosecond;
        v1 = nano;
        v2 = 0;
    } else {
        // Empty timespan
        return (flags & fSS_Full) ? "0 seconds" : "0s";
    }

    string result = NStr::LongToString(v1);
    bool plural = (v1 != 1);
    size_t len = result.length();

    // Rounding
    if ( flags & fSS_Round ) {
        // Rounding depends on number of digits we already have in v1
        if (len == 1) {
            v2 += 5;
        } else if (len == 2) {
            v2 += 50;
        } else { // len == 3
            v2 += 500;
        }
        // Check on overflow
        if ( v2 > 999 ) {
            ++v1;
            v2 = 0;
            if (unit == eSecond) {
                if ( v1 > 59 ) {
                    // Special case, because we work with timespans < 1 min
                    return (flags & fSS_Full) ? "1 minute" : "1m";
                }
            } else {
                if ( v1 > 999 ) {
                    // Shift to bigger units
                    v1 = 1;
                    unit = EUnit(unit-1);
                }
            }
            // Reconvert v1
            result = NStr::LongToString(v1);
            plural = (v1 != 1);
            len = result.length();
        }
    }

    // Add (3-n) digits from v2 after "."
    if (v2  &&  len < 3) {
        int n = int(v2 / 10);
        if (len == 2) {
            n = n / 10;
        }
        if ( n ) {
            result += "." + NStr::LongToString(n);
            plural = true;
        }
    }
    if ( flags & fSS_Full ) {
        result += string(" ") + kUnitNames[unit].name_full;
        if (plural) {
            result += "s";
        }
    } else {
        result += kUnitNames[unit].name_short;
    }
    return result;
}


// Helper functions for all modes except "smart".
string CTimeSpan::x_AsSmartString_Precision(TSmartStringFlags flags) const
{
    _ASSERT(GetSign() != eNegative);
    _ASSERT((flags & fSS_Smart) != fSS_Smart);

    // Convert precision flags to continuous numeric value (bit position - 1)
    int precision = -1;
    TSmartStringFlags pf = flags & fSS_PrecisionMask;
    while (pf)  {
        pf >>= 1;  ++precision;
    }
    _ASSERT(precision >= 0);

    // Named or float precision level
    bool is_named_precision = (precision < ePrecision);

    // Make a copy (for rounding)
    CTimeSpan ts(*this);

    // Round time span if necessary
    if ( flags & fSS_Round ) {
        int adjust_level = precision;

        // Calculate adjustment for floating precision level
        if ( !is_named_precision ) {

            // Find first non-zero value
            long sec = GetCompleteSeconds();
            for (adjust_level = eYear; adjust_level <= eSecond; adjust_level++) {
                if (sec >= kTimeSpanUnitMultipliers[adjust_level]) {
                    break;
                }
            }
            // Calculate adjustment level
            if (adjust_level <= eSecond) {
                adjust_level += (precision - eNanosecond - 1);
            } else {
                // ms, us, ns
                if (GetNanoSecondsAfterSecond() % 1000 == 0) {
                    adjust_level = eMillisecond;
                }
                else if (GetNanoSecondsAfterSecond() % 1000000 == 0) {
                    adjust_level = eMicrosecond;
                } else {
                    // no adjustment otherwise
                    adjust_level = eNanosecond;
                }
            }
        }
        // Add adjustment time span
        switch ( EUnit(adjust_level) ) {
            case eYear:
                ts += CTimeSpan((long)kAverageSecondsPerYear/2);
                break;
            case eMonth:
                ts += CTimeSpan((long)kAverageSecondsPerMonth/2);
                break;
            case eDay:
                ts += CTimeSpan(0, 12, 0, 0);
                break;
            case eHour:
                ts += CTimeSpan(0, 0, 30, 0);
                break;
            case eMinute:
                ts += CTimeSpan(0, 0, 0, 30);
                break;
            case eSecond:
                ts += CTimeSpan(0, 0, 0, 0, kNanoSecondsPerSecond/2);
                break;
            case eMillisecond:
                ts += CTimeSpan(0, 0, 0, 0, kNanoSecondsPerSecond/2000);
                break;
            case eMicrosecond:
                ts += CTimeSpan(0, 0, 0, 0, kNanoSecondsPerSecond/2000000);
                break;
            default:
                ; // nanoseconds -- nothing to do
        }
    }

    // Prepare data
   
    long sec = ts.GetCompleteSeconds();
    long nanoseconds = ts.GetNanoSecondsAfterSecond();

    struct SItem {
        SItem(void) : value(0), unit(eYear) {};
        SItem(long v, EUnit u) : value(v), unit(u) {};
        long  value;
        EUnit unit;
    };
    const int max_count = 7;
    SItem span[max_count];

    span[0] = SItem(sec/kAverageSecondsPerYear,  eYear  );  sec %= kAverageSecondsPerYear;
    span[1] = SItem(sec/kAverageSecondsPerMonth, eMonth );  sec %= kAverageSecondsPerMonth;
    span[2] = SItem(sec/(3600L*24),              eDay   );  sec %= (3600L*24);
    span[3] = SItem(sec/3600L,                   eHour  );  sec %= 3600L;
    span[4] = SItem(sec/60L,                     eMinute);  sec %= 60L;
    span[5] = SItem(sec,                         eSecond);

    switch (precision) {
        case eMillisecond:
            span[6] = SItem(nanoseconds / 1000000, eMillisecond);
            break;
        case eMicrosecond:
            span[6] = SItem(nanoseconds / 1000, eMicrosecond);
            break;
        case eNanosecond:
            span[6] = SItem(nanoseconds, eNanosecond);
            break;
        default:
            ; // other not-nanoseconds based precisions
    }

    // Result string
    string result;  
    // Precision level to start from (first non-zero)
    int start = is_named_precision ? eYear  : ePrecision;

    // Compose resulting string

    for (int i = 0; i < max_count  &&  start <= precision; ++i) {
        long val = span[i].value;

        // Adjust precision to skip zero values
        if ( !val ) {
            if ( result.empty() ) {
                if (start == precision  &&  start != ePrecision) {
                    break;
                }
                if ( is_named_precision ) {
                    ++start;
                }
                continue;
            }
            if ( flags & fSS_SkipZero ) {
                ++start;
                continue;
            } else {
                long sum = 0;
                int  cp = start + 1;
                for (int j = i + 1;
                     j < max_count  &&  (cp <= precision);  ++j, ++cp) {
                    sum += span[j].value;
                }
                if ( !sum ) {
                    // all trailing parts are zeros -- skip all
                    break;
                }
            }
        }
        ++start;
        if ( !result.empty() ) {
            result += " ";
        }
        result += NStr::LongToString(val);
        if (flags & fSS_Full) {
            result += string(" ") + kUnitNames[span[i].unit].name_full;
            if (val != 1) {
                result += "s";
            }
        } else {
            result += kUnitNames[span[i].unit].name_short;
        }
    }
    if ( result.empty() ) {
        if ( precision >= eSecond ) {
            return (flags & fSS_Full) ? "0 seconds" : "0s";
        } else {
            if (flags & fSS_Full) {
                return string("0 ") + kUnitNames[span[precision].unit].name_full + "s";
            }
            return string("0") + kUnitNames[span[precision].unit].name_short;
        }
    }
    return result;
}


string CTimeSpan::AsSmartString(TSmartStringFlags flags) const
{
    // Check on negative value
    if (GetSign() == eNegative) {
        NCBI_THROW(CTimeException, eArgument,
            "Negative CTimeSpan cannot be converted to smart string");
    }

    // Check flags compatibility

    _ASSERT(fSS_PrecisionMask == 
           (fSS_Year        | fSS_Month       | fSS_Day         |
            fSS_Hour        | fSS_Minute      | fSS_Second      |
            fSS_Millisecond | fSS_Microsecond | fSS_Nanosecond  |
            fSS_Precision1  | fSS_Precision2  | fSS_Precision3  |
            fSS_Precision4  | fSS_Precision5  | fSS_Precision6  |
            fSS_Precision7  | fSS_Smart)
    );

    const string kMsg = "Incompatible flags specified together: ";

    TSmartStringFlags f = flags & fSS_PrecisionMask;
    if (f == 0) {
        flags |= fSS_Smart;  // default
    } else {
        if ( !(f && !(f & (f-1))) ) {
            NCBI_THROW(CTimeException, eArgument, "Only one precision flag can be specified");
        }
    }

    // Default flags

    // Truncation
    f = fSS_Trunc | fSS_Round;
    if ((flags & f) == f) {
        NCBI_THROW(CTimeException, eArgument, kMsg + "fSS_Trunc | fSS_Round");
    }
    if ((flags & f) == 0) {
        flags |= fSS_Trunc;
    }
    // SkipZero
    f = fSS_SkipZero | fSS_NoSkipZero;
    if ((flags & f) == f) {
        NCBI_THROW(CTimeException, eArgument, kMsg + "fSS_SkipZero | fSS_NoSkipZero");
    }
    f = fSS_Smart | fSS_NoSkipZero;
    if ((flags & f) == f) {
        NCBI_THROW(CTimeException, eArgument, kMsg + "fSS_Smart | fSS_NoSkipZero");
    }
    if ((flags & f) == 0) {
        flags |= fSS_SkipZero;
    }
    // Naming
    f = fSS_Short | fSS_Full;
    if ((flags & f) == f) {
        NCBI_THROW(CTimeException, eArgument, kMsg + "fSS_Short | fSS_Full");
    }
    if ((flags & f) == 0) {
        flags |= fSS_Full;
    }

    // Use specific code depends on precision

    if ((flags & fSS_Smart) == fSS_Smart) {
        if (*this < CTimeSpan(60, 0)) {
            return x_AsSmartString_Smart_Small(flags);
        }
        return x_AsSmartString_Smart_Big(flags);
    } else {
        return x_AsSmartString_Precision(flags);
    }
}


CTimeSpan& CTimeSpan::AssignFromSmartString(const string& str)
{
    Clear();
    if ( str.empty() ) {
        return *this;
    }

    const  char* sss = str.c_str();
    bool   numeric_expected = true;
    long   value = 0;
    long   frac  = 0;
    size_t frac_len = 0;
    bool   repetitions[kUnitCount];

    memset(repetitions, 0, kUnitCount * sizeof(repetitions[0]));

    for (; *sss != '\0'; ++sss) {

        // Skip all white spaces
        if (isspace((unsigned char)(*sss))) {
            continue;
        }

        // Numeric data expected
        if (numeric_expected) {

            value = 0; frac = 0;

            if (isdigit((unsigned char)(*sss))) {
                // Get numeric value
                const char* start = sss++;
                while (*sss && isdigit((unsigned char)(*sss))) ++sss;
                size_t n = sss - start;
                if (n > 10) {
                    NCBI_THROW(CTimeException, eConvert,
                        "Too long numeric value '" + string(start, n) + 
                        "': string '" + str + "' (pos = " +
                        NStr::NumericToString(start - str.c_str()) + ")" );
                }
                value = NStr::StringToULong(CTempString(start, n));
            } else
            if (*sss != '.') {
                // no digits and no fraction part (".n" case)
                break;  // error
            }

            // Check on fraction
            if (*sss == '.') {
                ++sss;
                if (*sss && isdigit((unsigned char)(*sss))) {
                    const char* start = sss++;
                    while (*sss && isdigit((unsigned char)(*sss))) ++sss;
                    frac_len = sss - start;
                    if (frac_len > 9) {
                        NCBI_THROW(CTimeException, eConvert,
                            "Too long fractional part '" + string(start, frac_len) + 
                            "': string '" + str + "' (pos = " +
                            NStr::NumericToString(start - str.c_str()) + ")" );
                    }
                    frac = NStr::StringToULong(CTempString(start, frac_len));
                }
                // else { "n." case --= nothing to do }
            }
            --sss;
            numeric_expected = false;
            continue;
        } 

        // Unit specifier expected after numeric value
        else  {
            if (!isalpha((unsigned char)(*sss))) {
                break;  // error
            }
            const char* start = sss++;
            while (*sss && isalpha((unsigned char)(*sss))) ++sss;
            size_t n = sss - start;
            --sss;
            string spec(start, n);
            NStr::ToLower(spec);

            unsigned int i = 0;
            for (; i < kUnitCount; ++i) {
                if (spec == kUnitNames[i].name_full ||
                    spec == kUnitNames[i].name_full + string("s") ||
                    spec == kUnitNames[i].name_short) {
                    break;
                }
            }
            if (i >= kUnitCount) {
                NCBI_THROW(CTimeException, eConvert,
                    "Unknown time specifier '" + spec + "': string '" + str +
                    "' (pos = " +  NStr::NumericToString(start - str.c_str()) + ")" );
            }

            // Use bigger values for calculations to check on possible overflow
            TSeconds sec = m_Sec;
            TSeconds ns  = m_NanoSec;
            
            // Add component to timespan
            switch (EUnit(i)) {
            case eYear:
            case eMonth:
            case eDay:
            case eHour:
            case eMinute:
            case eSecond:
                sec += value * kTimeSpanUnitMultipliers[i];
                break;
            case eMillisecond:
            case eMicrosecond:
                ns += value * kTimeSpanUnitMultipliers[i];
                break;
            case eNanosecond:
                ns += value;
                break;
            default:
                _TROUBLE;
            }

            // Add fractional part
            if (frac) {
                // div = 10 ^ frac_len
                unsigned int div = 10;
                _ASSERT(frac_len > 0);
                while (--frac_len) div *= 10;

                if (i < eSecond) {
                    // fraction of year, month, day, hour, minute
                    sec += ((TSeconds)frac * kTimeSpanUnitMultipliers[i] / div);
                } else
                if (i == eSecond) {
                    // sec -- increase nanoseconds
                    ns += ((TSeconds)frac * (kNanoSecondsPerSecond / div));
                } else {
                if (i == eNanosecond) {
                    // Round fractional nanoseconds to nearest value
                    if (((TSeconds)frac * 10 / div) >=5 ) {
                        ++ns;
                    }
                } else 
                    // us, ms -- increase nanoseconds
                    ns += ((TSeconds)frac * (kTimeSpanUnitMultipliers[i] / div));
                }
            }

            // Check repetition of time components -- not allowed
            if (repetitions[i]) {
                NCBI_THROW(CTimeException, eConvert,
                    "Time component for '" + string(kUnitNames[i].name_full) + 
                    "s' already exists: string '" + str +
                    "' (pos = " + NStr::NumericToString(start - str.c_str()) + ")");
            }
            repetitions[i] = true;
            if (frac  &&  (i >= eSecond)) {
                // disable all subsequent ms, us, ns and fraction of second
                repetitions[eMillisecond] = true;
                repetitions[eMicrosecond] = true;
                repetitions[eNanosecond]  = true;
            }

            // Check on overflow
            if ( sec <= kMin_Long || sec >= kMax_Long || 
                 ns  <= kMin_Long || ns  >= kMax_Long) {
                NCBI_THROW(CTimeException, eConvert,
                    "Value is too big to convert to CTimeSpan: string '" + str +
                    "' (pos = " + NStr::NumericToString(start - str.c_str()) + ")");
            }
            m_Sec = (long)sec;
            m_NanoSec = (long)ns;
            numeric_expected = true;
        }
    }

    // Check on errors
    if (!numeric_expected) {
        NCBI_THROW(CTimeException, eConvert,
            "Time specifier expected: string '" + str +
            "' (pos = " + NStr::NumericToString(sss - str.c_str()) + ")");
    }
    if (*sss != '\0') {
        NCBI_THROW(CTimeException, eConvert,
            "Unexpected symbol: string '" + str +
            "' (pos = " + NStr::NumericToString(sss - str.c_str()) + ")");
    }
    // Normalize time span
    x_Normalize();

    return *this;
}



//=============================================================================
//
//  CTimeout
//
//=============================================================================


static string s_SpecialValueName(CTimeout::EType type)
{ 
    switch(type) {
    case CTimeout::eDefault:
        return "eDefault";
    case CTimeout::eInfinite:
        return "eInfinity";
    default:
        break;
    }
    return kEmptyStr;
}


bool CTimeout::IsZero(void) const
{
    if ( !IsFinite() ) {
        if (m_Type == eDefault) {
            NCBI_THROW(CTimeException, eInvalid, 
                       "IsZero() cannot be used with " +
                       s_SpecialValueName(m_Type) + " timeout");
        }
        return false;
    }
    return !(m_Sec | m_NanoSec);
}


unsigned long CTimeout::GetAsMilliSeconds(void) const
{ 
    if ( !IsFinite() ) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Cannot convert from " +
                   s_SpecialValueName(m_Type) + " timeout value");
    }
#if (SIZEOF_INT == SIZEOF_LONG)
    // Roughly calculate maximum number of seconds that can be safely converted
    // to milliseconds without an overflow.
    if (m_Sec > kMax_ULong / kMilliSecondsPerSecond - 1) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Timeout value " +
                   NStr::UIntToString(m_Sec) + 
                   " too big to convert to unsigned long");
    }
#endif
    return m_Sec * kMilliSecondsPerSecond +
        m_NanoSec / (kNanoSecondsPerSecond / kMilliSecondsPerSecond);
}


double CTimeout::GetAsDouble(void) const
{
    if ( !IsFinite() ) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Cannot convert from " +
                   s_SpecialValueName(m_Type) + " timeout value");
    }
    return m_Sec + double(m_NanoSec) / kNanoSecondsPerSecond;
}


CTimeSpan CTimeout::GetAsTimeSpan(void) const
{
    if ( !IsFinite() ) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Cannot convert from " +
                   s_SpecialValueName(m_Type) + " timeout value");
    }
#if (SIZEOF_INT == SIZEOF_LONG)
    if ( m_Sec > (unsigned int) kMax_Long ) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Timeout value " +
                   NStr::UIntToString(m_Sec) +
                   " too big to convert to CTimeSpan");
        // We don't need to check microseconds here, because it always keeps a
        // normalized value and can always be safely converted to nanoseconds.
    }
#endif
    CTimeSpan ts(m_Sec, m_NanoSec);
    return ts;
}


void CTimeout::Get(unsigned int* sec, unsigned int* usec) const
{
    if ( !IsFinite() ) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Cannot convert from " +
                   s_SpecialValueName(m_Type) + " timeout value");
    }
    if ( sec ) {
        *sec  = m_Sec;
    }
    if ( usec ) {
        *usec = m_NanoSec / (kNanoSecondsPerSecond / kMicroSecondsPerSecond);
    }
}


void CTimeout::GetNano(unsigned int *sec, unsigned int *nanosec) const
{
    if ( !IsFinite() ) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Cannot convert from " +
                   s_SpecialValueName(m_Type) + " timeout value");
    }
    if ( sec ) {
        *sec = m_Sec;
    }
    if ( nanosec ) {
        *nanosec = m_NanoSec;
    }
}


void CTimeout::Set(EType type)
{
    switch(type) {
    case eDefault:
    case eInfinite:
        m_Type = type;
        break;
    case eZero:
        m_Type = eFinite;
        Set(0,0);
        break;
    default:
        NCBI_THROW(CTimeException, eArgument, 
                   "Incorrect type value " + NStr::IntToString(type));
    }
}


void CTimeout::Set(unsigned int sec, unsigned int usec)
{
    m_Type    = eFinite;
    m_Sec     =  usec / (unsigned int)kMicroSecondsPerSecond + sec;
    m_NanoSec = (usec % (unsigned int)kMicroSecondsPerSecond) *
        (unsigned int)(kNanoSecondsPerSecond / kMicroSecondsPerSecond);
}


void CTimeout::SetNano(unsigned int sec, unsigned int nanosec)
{
    m_Type    = eFinite;
    m_Sec     = nanosec / (unsigned int)kNanoSecondsPerSecond + sec;
    m_NanoSec = nanosec % (unsigned int)kNanoSecondsPerSecond;
}


void CTimeout::Set(double sec)
{
    if (sec < 0) {
        NCBI_THROW(CTimeException, eArgument, 
                   "Cannot set negative value " +
                   NStr::DoubleToString(sec));
    }
    if (sec > kMax_UInt) {
        NCBI_THROW(CTimeException, eArgument, 
                   "Timeout value " +
                   NStr::DoubleToString(sec) + " too big");
    }
    m_Type    = eFinite;
    m_Sec     = (unsigned int)  sec;
    m_NanoSec = (unsigned int)((sec - m_Sec) * kNanoSecondsPerSecond);
}


void CTimeout::Set(const CTimeSpan& ts)
{
    if (ts.GetSign() == eNegative) {
        NCBI_THROW(CTimeException, eArgument, 
                   "Cannot convert from negative CTimeSpan(" +
                   ts.AsString() + ")");
    }
    if ((Uint8) ts.GetCompleteSeconds() > kMax_UInt) {
        NCBI_THROW(CTimeException, eArgument, 
                   "CTimeSpan value (" +
                   ts.AsString() + ") too big");
        // We don't need to check nanoseconds, because CTimeSpan always has it
        // normalized value, and its value can always be safely converted
        // to microseconds.
    }
    m_Type    = eFinite;
    m_Sec     = (unsigned int) ts.GetCompleteSeconds();
    m_NanoSec = (unsigned int) ts.GetNanoSecondsAfterSecond();
}


#define COMPARE_TIMEOUT_TYPES(t1, t2) ((int(t1) << 2) | int(t2))


bool CTimeout::operator== (const CTimeout& t) const
{
    switch (COMPARE_TIMEOUT_TYPES(m_Type, t.m_Type)) {
    case COMPARE_TIMEOUT_TYPES(eFinite, eFinite):
        return m_Sec == t.m_Sec  &&  m_NanoSec == t.m_NanoSec;
    case COMPARE_TIMEOUT_TYPES(eInfinite, eInfinite):
        return true;   // infinite == infinite
    case COMPARE_TIMEOUT_TYPES(eFinite, eInfinite):
    case COMPARE_TIMEOUT_TYPES(eInfinite, eFinite):
        return false;  // infinite != value
    default:
        NCBI_THROW(CTimeException, eArgument,
                   "Unable to compare with " +
                   s_SpecialValueName(eDefault) + " timeout");
    }
}


bool CTimeout::operator< (const CTimeout& t) const
{
    switch (COMPARE_TIMEOUT_TYPES(m_Type, t.m_Type)) {
    case COMPARE_TIMEOUT_TYPES(eFinite, eFinite):
        return m_Sec == t.m_Sec ? m_NanoSec < t.m_NanoSec : m_Sec < t.m_Sec;
    case COMPARE_TIMEOUT_TYPES(eFinite, eInfinite):
        return true;  // value < infinite
    case COMPARE_TIMEOUT_TYPES(eInfinite, eFinite):
    case COMPARE_TIMEOUT_TYPES(eInfinite, eInfinite):
        return false;
    default:
        NCBI_THROW(CTimeException, eArgument,
                   "Unable to compare with " +
                   s_SpecialValueName(eDefault) + " timeout");
    }
}


bool CTimeout::operator> (const CTimeout& t) const
{
    switch (COMPARE_TIMEOUT_TYPES(m_Type, t.m_Type)) {
    case COMPARE_TIMEOUT_TYPES(eFinite, eFinite):
        return m_Sec == t.m_Sec ? m_NanoSec > t.m_NanoSec : m_Sec > t.m_Sec;
    case COMPARE_TIMEOUT_TYPES(eInfinite, eFinite):
        return true;  // infinite > value
    case COMPARE_TIMEOUT_TYPES(eFinite, eInfinite):
    case COMPARE_TIMEOUT_TYPES(eInfinite, eInfinite):
        return false;
    default:
        NCBI_THROW(CTimeException, eArgument,
                   "Unable to compare with " +
                   s_SpecialValueName(eDefault) + " timeout");
    }
}


bool CTimeout::operator>= (const CTimeout& t) const
{
    switch (COMPARE_TIMEOUT_TYPES(m_Type, t.m_Type)) {
    case COMPARE_TIMEOUT_TYPES(eFinite, eFinite):
        return m_Sec == t.m_Sec ? m_NanoSec >= t.m_NanoSec : m_Sec >= t.m_Sec;
    case COMPARE_TIMEOUT_TYPES(eFinite, eInfinite):
        return false;     // value < infinity
    case COMPARE_TIMEOUT_TYPES(eInfinite, eFinite):
    case COMPARE_TIMEOUT_TYPES(eInfinite, eInfinite):
    case COMPARE_TIMEOUT_TYPES(eInfinite, eDefault):
        return true;      // infinity >= anything
    case COMPARE_TIMEOUT_TYPES(eDefault, eFinite):
        if ( t.IsZero() )
            return true;  // default >= zero
        // fall through
    default:
        NCBI_THROW(CTimeException, eArgument,
                   "Unable to compare with " +
                   s_SpecialValueName(eDefault) + " timeout");
    }
}


bool CTimeout::operator<= (const CTimeout& t) const
{
    switch (COMPARE_TIMEOUT_TYPES(m_Type, t.m_Type)) {
    case COMPARE_TIMEOUT_TYPES(eFinite, eFinite):
        return m_Sec == t.m_Sec ? m_NanoSec <= t.m_NanoSec : m_Sec <= t.m_Sec;
    case COMPARE_TIMEOUT_TYPES(eInfinite, eFinite):
        return false;     // infinity > value
    case COMPARE_TIMEOUT_TYPES(eFinite, eInfinite):
    case COMPARE_TIMEOUT_TYPES(eInfinite, eInfinite):
    case COMPARE_TIMEOUT_TYPES(eDefault, eInfinite):
        return true;      // anything <= infinity
    case COMPARE_TIMEOUT_TYPES(eFinite, eDefault):
        if ( IsZero() ) 
            return true;  // zero <= default
        // fall through
    default:
        NCBI_THROW(CTimeException, eArgument,
                   "Unable to compare with " +
                   s_SpecialValueName(eDefault) + " timeout");
    }
}



//=============================================================================
//
//  CDeadline
//
//=============================================================================

CDeadline::CDeadline(EType type)
    : m_Seconds(0), m_Nanoseconds(0), m_Infinite(type == eInfinite)
{
}


CDeadline::CDeadline(unsigned int seconds, unsigned int nanoseconds)
    : m_Seconds(0), m_Nanoseconds(0), m_Infinite(false)
{
    // Unless expires immediately
    if (seconds || nanoseconds) {
        x_SetNowPlus(seconds, nanoseconds);
    }
}


CDeadline::CDeadline(const CTimeout& timeout)
    : m_Seconds(0), m_Nanoseconds(0), m_Infinite(false)
{
    if (timeout.IsInfinite()) {
        m_Infinite = true;
    }
    else if (timeout.IsZero()) {
        return;
    }
    else if (timeout.IsFinite()) {
        unsigned int sec, usec;
        timeout.Get(&sec, &usec);
        x_SetNowPlus(sec, usec * (unsigned int)(kNanoSecondsPerSecond / kMicroSecondsPerSecond));
    }
    else if (timeout.IsDefault()) {
        NCBI_THROW(CTimeException, eArgument, "Cannot convert from default CTimeout");
    }
}


void CDeadline::x_SetNowPlus(unsigned int seconds, unsigned int nanoseconds)
{
#if defined(NCBI_OS_MSWIN)
    FILETIME systime;
    Uint8    systemp;

    GetSystemTimeAsFileTime(&systime);

    systemp   = systime.dwHighDateTime;
    systemp <<= 32;
    systemp  |= systime.dwLowDateTime;
    m_Seconds     =  systemp / 10000000  - NCBI_CONST_UINT8(11644473600);
    m_Nanoseconds = (systemp % 10000000) * 100;
#else
#  if 0
    struct timespec timebuffer;
    if (clock_gettime(CLOCK_REALTIME, &timebuffer) == 0) {
        m_Seconds     = timebuffer.tv_sec;
        m_Nanoseconds = timebuffer.tv_nsec;
    } else {
        NCBI_THROW(CTimeException, eInvalid,
                   "Cannot get current deadline time value");
    }
#  else
    struct timeval tp;
    if (gettimeofday(&tp, 0) == 0) {
        m_Seconds     = tp.tv_sec;
        m_Nanoseconds = (unsigned int)
            (tp.tv_usec * (kNanoSecondsPerSecond / kMicroSecondsPerSecond));
    } else {
        NCBI_THROW(CTimeException, eInvalid,
                   "Cannot get current deadline time value");
    }
#  endif
#endif

    if (seconds || nanoseconds) {
        nanoseconds  += m_Nanoseconds;
        seconds      += nanoseconds / (unsigned int)kNanoSecondsPerSecond;
        m_Nanoseconds = nanoseconds % (unsigned int)kNanoSecondsPerSecond;
        m_Seconds    += seconds;
    }
}


void CDeadline::GetExpirationTime(time_t* sec, unsigned int* nanosec) const
{
    if ( IsInfinite() ) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Cannot convert from " +
                   s_SpecialValueName(CTimeout::eInfinite) +
                   " deadline value");
    }
    if ( sec ) {
        *sec     = m_Seconds;
    }
    if ( nanosec ) {
        *nanosec = m_Nanoseconds;
    }
}


CNanoTimeout CDeadline::GetRemainingTime(void) const
{
    if ( IsInfinite() ) {
        NCBI_THROW(CTimeException, eConvert, 
                   "Cannot convert from " +
                   s_SpecialValueName(CTimeout::eInfinite) +
                   " deadline value");
    }

    // CDeadline::eNoWait / CDeadline(0, 0) / CDeadline(CTimeout::eZero)
    if (!m_Seconds) {
        return CNanoTimeout(0, 0);
    }

    CDeadline now(0,0);

    // Immediately expiring deadlines do not get actual seconds/nanoseconds anymore,
    // so the latter are explicitly set here for 'now' using x_SetNowPlus()
    now.x_SetNowPlus(0, 0);

    time_t       thenS  = m_Seconds;
    unsigned int thenNS = m_Nanoseconds;
    time_t       nowS   = now.m_Seconds;
    unsigned int nowNS  = now.m_Nanoseconds;

    if (thenS < nowS  ||  (thenS == nowS  &&  thenNS <= nowNS)) {
        return CNanoTimeout(0,0);
    }
    if (thenNS >= nowNS) {
        thenNS -= nowNS;
    } else {
        --thenS;
        thenNS = (unsigned int)kNanoSecondsPerSecond - (nowNS - thenNS);
    }
    _ASSERT(thenS >= nowS);
    thenS -= nowS;

    return CNanoTimeout((unsigned int)thenS, thenNS);
}


bool CDeadline::operator< (const CDeadline& right_hand_operand) const
{
    if (!IsInfinite()) {
        return right_hand_operand.IsInfinite()
            ||   m_Seconds <  right_hand_operand.m_Seconds
            ||  (m_Seconds == right_hand_operand.m_Seconds  &&
                 m_Nanoseconds < right_hand_operand.m_Nanoseconds);
    }
    if (right_hand_operand.IsInfinite()) {
        NCBI_THROW(CTimeException, eInvalid,
                   "Cannot compare two " +
                   s_SpecialValueName(CTimeout::eInfinite) +
                   " deadline values");
    }
    return false;
}



//=============================================================================
//
//  CFastLocalTime
//
//=============================================================================

CFastLocalTime::CFastLocalTime(unsigned int sec_after_hour)
    : m_SecAfterHour(sec_after_hour),
      m_LastTuneupTime(0), m_LastSysTime(0),
      m_Timezone(0), m_IsTuneup(NULL)
{
#if !defined(NCBI_TIMEZONE_IS_UNDEFINED)
    // MT-Safe protect: use CTime locking mutex
    CMutexGuard LOCK(s_TimeMutex);
    m_Timezone = (int)TimeZone();
    m_Daylight = Daylight();
    LOCK.Release();
#else
    m_Daylight = -1;
#endif
    m_LocalTime.SetTimeZonePrecision(CTime::eHour);
    m_TunedTime.SetTimeZonePrecision(CTime::eHour);
}


void CFastLocalTime::Tuneup(void)
{
    if ( m_IsTuneup ) {
        return;
    }
    // Get system time
    time_t timer;
    long ns;
    CTime::GetCurrentTimeT(&timer, &ns);
    x_Tuneup(timer, ns);
}


bool CFastLocalTime::x_Tuneup(time_t timer, long nanosec)
{
    // Tuneup in progress
    if (SwapPointers(&m_IsTuneup, (void*)1))
        return false;

    // MT-Safe protect: use CTime locking mutex
    CMutexGuard LOCK(s_TimeMutex);
    m_TunedTime.x_SetTime(&timer);
    m_TunedTime.SetNanoSecond(nanosec);

#if !defined(NCBI_TIMEZONE_IS_UNDEFINED)
    m_Timezone = (int)TimeZone();
    m_Daylight = Daylight();
#endif

    LOCK.Release();

    // Copy tuned time to cached local time
    CMutexGuard FLT_LOCK(s_FastLocalTimeMutex);
    m_LastTuneupTime = timer;
    m_LocalTime   = m_TunedTime;
    m_LastSysTime = m_LastTuneupTime;

    // Clear flag
    m_IsTuneup = NULL;

    return true;
}


CTime CFastLocalTime::GetLocalTime(void)
{
    CMutexGuard LOCK(eEmptyGuard);

retry:
    // Get system time
    time_t timer;
    long ns;
    CTime::GetCurrentTimeT(&timer, &ns);

    // Avoid to make time tune up in first m_SecAfterHour for each hour
    // Otherwise do this at each hours/timezone change.
    if ( !m_IsTuneup ) {
#if !defined(NCBI_TIMEZONE_IS_UNDEFINED)
        // Get current timezone
        TSeconds x_timezone;
        int x_daylight;
        {{
            // MT-Safe protect: use CTime locking mutex
            CMutexGuard LOCK_TM(s_TimeMutex);
            x_timezone = TimeZone();
            x_daylight = Daylight();
        }}
#endif
        if ( !m_LastTuneupTime  ||
            ((timer / 3600 != m_LastTuneupTime / 3600)  &&
             (timer % 3600 >  (time_t)m_SecAfterHour))
#if !defined(NCBI_TIMEZONE_IS_UNDEFINED)
            ||  (x_timezone != m_Timezone  ||  x_daylight != m_Daylight)
#endif
        ) {
            if (x_Tuneup(timer, ns)) {
                return m_LocalTime;
            }
        }
    }
    // MT-Safe protect
    LOCK.Guard(s_FastLocalTimeMutex);

    if ( !m_LastTuneupTime ) {
        LOCK.Release();
        NCBI_SCHED_YIELD();
        goto retry;
    } else {
        // Adjust local time on base of system time without any system calls
        m_LocalTime.AddSecond(timer - m_LastSysTime, CTime::eIgnoreDaylight);
        m_LocalTime.SetNanoSecond(ns);
    }
    m_LastSysTime = timer;

    // Return computed local time
    return m_LocalTime;
}


int CFastLocalTime::GetLocalTimezone(void)
{
#if !defined(NCBI_TIMEZONE_IS_UNDEFINED)
    // Get system timer
    time_t timer;
    long ns;
    CTime::GetCurrentTimeT(&timer, &ns);

    // Avoid to make time tune up in first m_SecAfterHour for each hour
    // Otherwise do this at each hours/timezone change.
    if ( !m_IsTuneup ) {
        // Get current timezone
        TSeconds x_timezone;
        int x_daylight;
        {{
            // MT-Safe protect: use CTime locking mutex
            CMutexGuard LOCK(s_TimeMutex);
            x_timezone = TimeZone();
            x_daylight = Daylight();
        }}
        if ( !m_LastTuneupTime  ||
            ((timer / 3600 != m_LastTuneupTime / 3600)  &&
             (timer % 3600 >  (time_t)m_SecAfterHour))
            ||  (x_timezone != m_Timezone  ||  x_daylight != m_Daylight)
        ) {
            x_Tuneup(timer, ns);
        }
    }
#endif
    // Return local timezone
    return m_Timezone;
}


//=============================================================================
//
//  CStopWatch
//
//=============================================================================

/// @deprecated
CStopWatch::CStopWatch(bool start)
{
    m_Total = 0;
    m_Start = 0;
    m_State = eStop;
    if ( start ) {
        Start();
    }
} // NCBI_FAKE_WARNING


double CStopWatch::GetTimeMark()
{
#if defined(NCBI_OS_MSWIN)
    // For Win32, we use QueryPerformanceCounter()

    LARGE_INTEGER bigint;
    static double freq;
    static bool first = true;

    if ( first ) {
        LARGE_INTEGER nfreq;
        QueryPerformanceFrequency(&nfreq);
        freq  = double(nfreq.QuadPart);
        first = false;
    }

    if ( !QueryPerformanceCounter(&bigint) ) {
        return 0.0;
    }
    return double(bigint.QuadPart) / freq;
#else
    // For Unixes, we use gettimeofday()

    struct timeval time;
    if ( gettimeofday (&time, 0) ) {
        return 0.0;
    }
    return double(time.tv_sec) + double(time.tv_usec) / 1e6;
#endif
}


void CStopWatch::SetFormat(const CTimeFormat& fmt)
{
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using cleanup function.
    CTimeFormat* ptr = new CTimeFormat(fmt);
    s_TlsFormatStopWatch.SetValue(ptr, CTlsBase::DefaultCleanup<CTimeFormat>);
}


CTimeFormat CStopWatch::GetFormat(void)
{
    CTimeFormat fmt;
    CTimeFormat* ptr = s_TlsFormatStopWatch.GetValue();
    if ( !ptr ) {
        fmt.SetFormat(kDefaultFormatStopWatch);
    } else {
        fmt = *ptr;
    }
    return fmt;
}


string CStopWatch::AsString(const CTimeFormat& fmt) const
{
    double e = Elapsed();
    CTimeSpan ts(e < 0.0 ? 0.0 : e);
    if ( fmt.IsEmpty() ) {
        return ts.AsString(GetFormat());
    }
    return ts.AsString(fmt);
}


//============================================================================
//
//  Extern
//
//============================================================================


CTime GetFastLocalTime(void)
{
    return s_FastLocalTime->GetLocalTime();
}


void TuneupFastLocalTime(void)
{
    s_FastLocalTime->Tuneup();
}

const char* CTimeException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eArgument:    return "eArgument";
    case eConvert:     return "eConvert";
    case eInvalid:     return "eInvalid";
    case eFormat:      return "eFormat";
    default:           return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
