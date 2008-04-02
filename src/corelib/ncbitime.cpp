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

#if defined(NCBI_OS_MSWIN)
#  include <sys/timeb.h>
#  include <windows.h>
#elif defined(NCBI_OS_UNIX)
#  include <sys/time.h>
#endif

#if defined(NCBI_OS_MAC) || defined(NCBI_COMPILER_MW_MSL)
#  include <OSUtils.h>

typedef struct MyTZDLS {
    long timezone;
    bool daylight;
} MyTZDLS;

static MyTZDLS MyReadLocation()
{
    MachineLocation loc;
    ReadLocation(&loc);
    long tz = loc.u.gmtDelta & 0x00ffffff;
    // Propogate sign bit from bit 23 to bit 31 if West of UTC.
    // (Sign-extend the GMT correction)
    if ((tz & 0x00800000) != 0) {
        tz |= 0xFF000000;
    }
    bool dls = (loc.u.dlsDelta != 0);
    MyTZDLS tzdls = {tz, dls};
    return tzdls;
}

static MyTZDLS sTZDLS = MyReadLocation();

#  define TimeZone()  sTZDLS.timezone
#  define Daylight()  sTZDLS.daylight
#elif defined(__CYGWIN__)
#  define TimeZone() _timezone
#  define Daylight() _daylight
#else
#  define TimeZone()  timezone
#  define Daylight()  daylight
#endif

#if defined(NCBI_OS_DARWIN)  ||  defined(NCBI_OS_BSD)
#  define TIMEZONE_IS_UNDEFINED  1
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Util


BEGIN_NCBI_SCOPE


// Protective mutex
DEFINE_STATIC_FAST_MUTEX(s_TimeMutex);
DEFINE_STATIC_FAST_MUTEX(s_TimeAdjustMutex);
DEFINE_STATIC_FAST_MUTEX(s_FastLocalTimeMutex);

// Store global time/timespan formats in TLS
static CSafeStaticRef< CTls<CTimeFormat> > s_TlsFormatTime;
static CSafeStaticRef< CTls<CTimeFormat> > s_TlsFormatSpan;
static CSafeStaticRef< CTls<CTimeFormat> > s_TlsFormatStopWatch;

static void s_TlsFormatCleanup(CTimeFormat* fmt, void* /* data */)
{
    delete fmt;
}

// Global quick and dirty getter of local time
static CSafeStaticPtr<CFastLocalTime> s_FastLocalTime;


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
static const char* kDefaultFormatTime = "M/D/Y h:m:s";
static const char* kDefaultFormatSpan = "-S.n";
static const char* kDefaultFormatStopWatch = "-S.n";

// Set of the checked format symbols.
// For CStopWatch class the format symbols are equal
// to kFormatSymbolsSpan also.
static const char* kFormatSymbolsTime = "yYMbBDdhHmsSzZwWlrpP";
static const char* kFormatSymbolsSpan = "-dhHmMsSnN";

// Character used to escape formatted symbols.
const char kFormatEscapeSymbol = '$';

// Error messages
static const string kMsgInvalidTime = "CTime:  invalid";

// Get number of days in "date"
static unsigned s_Date2Number(const CTime& date)
{
    if ( date.IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    unsigned d = date.Day();
    unsigned m = date.Month();
    unsigned y = date.Year();
    unsigned c, ya;

    if (m > 2) {
        m -= 3;
    } else {
        m += 9;
        y--;
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
    unsigned month;

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
        year++;
    }
    // Construct new CTime object
    return
        CTime(year, month, day, t.Hour(), t.Minute(), t.Second(),
              t.NanoSecond(), t.GetTimeZone(), t.GetTimeZonePrecision());
}


// Calc <value> + <offset> on module <bound>.
// Returns normalized value in <value>. 
// The <major> will have a remainder after dividing.
static void s_Offset(long *value, Int8 offset, long bound, int *major)
{
    Int8 v = *value + offset;
    *major += (int)(v / bound);
    *value = (long)(v % bound);
    if (*value < 0) {
        *major -= 1;
        *value += bound;
    }
}

// Macro to check range for time components.
// See also:
//      CTime::m_Data
//      CTime::IsValid
//      CTime::Set*() methods

#define CHECK_RANGE(value, what, min, max) \
    if ( value < min  ||  value > max ) {  \
        NCBI_THROW(CTimeException, eArgument, "CTime: " what " value is out of range"); \
    }

#define CHECK_RANGE_YEAR(value)  CHECK_RANGE(value, "year", 1583, kMax_Int)
#define CHECK_RANGE_MONTH(value) CHECK_RANGE(value, "month", 1, 12)
#define CHECK_RANGE_DAY(value)   CHECK_RANGE(value, "day", 1, 31)
#define CHECK_RANGE_HOUR(value)  CHECK_RANGE(value, "hour", 0, 23)
#define CHECK_RANGE_MIN(value)   CHECK_RANGE(value, "minute", 0, 59)
#define CHECK_RANGE_SEC(value)   CHECK_RANGE(value, "second", 0, 61)
#define CHECK_RANGE_NSEC(value)  CHECK_RANGE(value, "nanosecond", 0, kNanoSecondsPerSecond - 1)



//============================================================================
//
// CTimeFormat
//
//============================================================================


CTimeFormat::CTimeFormat(void)
    : m_Type(eDefault)
{
    return;
}


CTimeFormat::CTimeFormat(const CTimeFormat& format)
{
    *this = format;
}


CTimeFormat::CTimeFormat(const char* fmt, EType fmt_type)
{
    SetFormat(fmt, fmt_type);
}


CTimeFormat::CTimeFormat(const string& fmt, EType fmt_type)
{
    SetFormat(fmt, fmt_type);
}


CTimeFormat& CTimeFormat::operator= (const CTimeFormat& format)
{
    if ( &format == this ) {
        return *this;
    }
    m_Str  = format.m_Str;
    m_Type = format.m_Type;
    return *this;
}


CTimeFormat CTimeFormat::GetPredefined(EPredefined fmt, EType fmt_type)
{
    // Predefined time formats
    static const char* s_Predefined[][2] =
    {
        {"Y",              "$Y"},
        {"Y-M",            "$Y-$M"},
        {"Y-M-D",          "$Y-$M-$D"},
        {"Y-M-DTh:m",      "$Y-$M-$DT$h:$m"},
        {"Y-M-DTh:m:s",    "$Y-$M-$DT$h:$m:$s"},
        {"Y-M-DTh:m:s.l",  "$Y-$M-$DT$h:$m:$s.$l"},
    };
    return CTimeFormat(s_Predefined[(int)fmt][(int)fmt_type], fmt_type);
}


//============================================================================
//
// CTime
//
//============================================================================

CTime::CTime(const CTime& t)
{
    *this = t;
}


CTime::CTime(int year, int yearDayNumber,
             ETimeZone tz, ETimeZonePrecision tzp)
{
    Clear();
    m_Data.tz = tz;
    m_Data.tzprec = tzp;

    CTime t = CTime(year, 1, 1);
    t += yearDayNumber - 1;
    m_Data.year  = t.Year();
    m_Data.month = t.Month();
    m_Data.day   = t.Day();
}


void CTime::x_Init(const string& str, const CTimeFormat& format)
{
    Clear();
    if ( str.empty() ) {
        return;
    }
    // For partialy defined times use default values
    bool is_year_present  = false;
    bool is_month_present = false;
    bool is_day_present   = false;
    bool is_time_present  = false;

    const string& fmt = format.GetString();
    CTimeFormat::EType fmt_type = format.GetType();
    bool is_escaped = (fmt_type != CTimeFormat::eNcbiSimple);
    bool is_format_symbol = !is_escaped;

    const char* fff;
    const char* sss = str.c_str();
#if ! defined(TIMEZONE_IS_UNDEFINED)
    bool  adjust_needed = false;
    long  adjust_tz     = 0;
#endif

    enum EHourFormat{
        e24, eAM, ePM
    };
    EHourFormat hourformat = e24;
    bool is_12hour = false;

    int weekday = -1;
    for (fff = fmt.c_str();  *fff != '\0';  fff++) {
        // Skip space symbols in format string
        if ( isspace((unsigned char)(*fff)) ) {
            continue;
        }
        // Skip preceding symbols for some formats
        if ( !is_format_symbol ) {
            if ( *fff == kFormatEscapeSymbol )  {
                is_format_symbol = true;
                continue;
            }
        }
        if ( is_escaped ) {
            is_format_symbol = false;
        }
        // Skip space symbols in time string
        while ( isspace((unsigned char)(*sss)) )
            sss++;

        // Non-format symbols
        if (strchr(kFormatSymbolsTime, *fff) == 0) {
            if (*fff == *sss) {
                sss++;
                continue;  // skip matching non-format symbols
            }
            break;  // error: non-matching non-format symbols
        }

        // Month
        if (*fff == 'b'  ||  *fff == 'B') {
            const char** name;
            if (*fff == 'b') {
                name = kMonthAbbr;
            } else {
                name = kMonthFull;
            }
            for (unsigned char i = 0;  i < 12;  i++) {
                size_t namelen = strlen(*name);
                if (NStr::strncasecmp(sss, *name, namelen) == 0) {
                    sss += namelen;
                    m_Data.month = i + 1;
                    break;
                }
                name++;
            }
            is_month_present = true;
            continue;
        }

        // Day of week
        if (*fff == 'w'  ||  *fff == 'W') {
            const char** day = (*fff == 'w') ? kWeekdayAbbr : kWeekdayFull;
            for (unsigned char i = 0;  i < 7;  i++) {
                size_t len = strlen(*day);
                if (NStr::strncasecmp(sss, *day, len) == 0) {
                    sss += len;
                    weekday = i;
                    break;
                }
                day++;
            }
            continue;
        }

        // Timezone (GMT time)
        if (*fff == 'Z') {
            if (NStr::strncasecmp(sss, "GMT", 3) == 0) {
                m_Data.tz = eGmt;
                sss += 3;
            } else {
                m_Data.tz = eLocal;
            }
            continue;
        }

        // Timezone (local time in format GMT+HHMM)
        if (*fff == 'z') {
#if defined(TIMEZONE_IS_UNDEFINED)
            ERR_POST_X(3, "Format symbol 'z' is unsupported on this platform");
#else
            m_Data.tz = eLocal;
            if (NStr::strncasecmp(sss, "GMT", 3) == 0) {
                sss += 3;
            }
            while ( isspace((unsigned char)(*sss)) ) {
                sss++;
            }
            int sign = (*sss == '+') ? 1 : ((*sss == '-') ? -1 : 0);
            if ( sign ) {
                sss++;
            } else {
                sign = 1;
            }
            long x_hour = 0;
            long x_min  = 0;

            char value_str[3];
            char* s = value_str;
            for (size_t len = 2;
                 len  &&  *sss  &&  isdigit((unsigned char)(*sss));
                 len--) {
                *s++ = *sss++;
            }
            *s = '\0';
            try {
                x_hour = NStr::StringToLong(value_str);
            }
            catch (CStringException) {
                x_hour = 0;
            }
            try {
                if ( *sss != '\0' ) {
                    s = value_str;
                    for (size_t len = 2;
                         len  &&  *sss  &&  isdigit((unsigned char)(*sss));
                         len--) {
                        *s++ = *sss++;
                    }
                    *s = '\0';
                    x_min = NStr::StringToLong(value_str,
                                               NStr::fAllowTrailingSymbols);
                }
            }
            catch (CStringException) {
                x_min = 0;
            }
            adjust_needed = true;
            adjust_tz = sign * (x_hour * 60 + x_min) * 60;
#endif
            continue;
        }

        // Timezone (local time in format GMT+HHMM)
        if (*fff == 'p'  ||  *fff == 'P') {
            if (NStr::strncasecmp(sss, "AM", 2) == 0) {
                hourformat = eAM;
                sss += 2;
            } else if (NStr::strncasecmp(sss, "PM", 2) == 0) {
                hourformat = ePM;
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
        for ( ; len  &&  *sss  &&  isdigit((unsigned char)(*sss));  len--) {
            *s++ = *sss++;
        }
        *s = '\0';
        long value = NStr::StringToLong(value_str);

        // Set time part
        switch ( *fff ) {
        case 'Y':
            CHECK_RANGE_YEAR(value);
            m_Data.year = (unsigned int)value;
            is_year_present = true;
            break;
        case 'y':
            if (value >= 0  &&  value < 50) {
                value += 2000;
            } else if (value >= 50  &&  value < 100) {
                value += 1900;
            }
            CHECK_RANGE_YEAR(value);
            m_Data.year = (unsigned int)value;
            is_year_present = true;
            break;
        case 'M':
            CHECK_RANGE_MONTH(value);
            m_Data.month = (unsigned char)value;
            is_month_present = true;
            break;
        case 'D':
        case 'd':
            CHECK_RANGE_DAY(value);
            m_Data.day = (unsigned char)value;
            is_day_present = true;
            break;
        case 'h':
            CHECK_RANGE_HOUR(value);
            m_Data.hour = (unsigned char)value;
            is_time_present = true;
            break;
        case 'H':
            CHECK_RANGE_HOUR(value);
            m_Data.hour = (unsigned char)value % 12;
            is_12hour = true;
            is_time_present = true;
            break;
        case 'm':
            CHECK_RANGE_MIN(value);
            m_Data.min = (unsigned char)value;
            is_time_present = true;
            break;
        case 's':
            CHECK_RANGE_SEC(value);
            m_Data.sec = (unsigned char)value;
            is_time_present = true;
            break;
        case 'l':
            CHECK_RANGE_NSEC((Int8)value * 1000000);
            m_Data.nanosec = (Int4)value * 1000000;
            is_time_present = true;
            break;
        case 'r':
            CHECK_RANGE_NSEC((Int8)value * 1000);
            m_Data.nanosec = (Int4)value * 1000;
            is_time_present = true;
            break;
        case 'S':
            CHECK_RANGE_NSEC(value);
            m_Data.nanosec = (Int4)value;
            is_time_present = true;
            break;
        default:
            NCBI_THROW(CTimeException, eFormat, "CTime: format is incorrect");
        }
    }

    // Correct 12-hour time if needed
    if (is_12hour  &&  hourformat == ePM) {
        m_Data.hour += 12;
    }

    while ( isspace((unsigned char)(*sss)) )
        sss++;
    if (*fff != '\0'  ||  *sss != '\0') {
        NCBI_THROW(CTimeException, eFormat, "CTime: format is incorrect");
    }

    // For partialy defined times use default values
    int ptcache = 0;
    ptcache += (is_year_present  ? 2000 : 1000);
    ptcache += (is_month_present ? 200 : 100);
    ptcache += (is_day_present   ? 20 : 10);
    ptcache += (is_time_present  ? 2 : 1);

    // Use current time to set some fields
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
            m_Data.year  = current.Year();
            break;
        case 1211:                          // M        -> Y = current, D = 1
            m_Data.year  = current.Year();
            m_Data.day   = 1;
            break;
        case 1122:                          // D, time  -> Y,M = current
        case 1121:                          // D        -> Y,M = current
            m_Data.year  = current.Year();
            m_Data.month = current.Month();
            break;
        case 1112:                          // time     -> Y,M,D = current
            m_Data.year  = current.Year();
            m_Data.month = current.Month();
            m_Data.day   = current.Day();
            break;
    }

    // Check on errors for weekday
    if (weekday != -1  &&  weekday != DayOfWeek()) {
        NCBI_THROW(CTimeException, eInvalid, "CTime: invalid day of week");
    }
    // Validate time value
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
#if !defined(TIMEZONE_IS_UNDEFINED)
    // Adjust time for current timezone
    if ( adjust_needed  &&  adjust_tz != -TimeZone() ) {
        AddSecond(-TimeZone() - adjust_tz);
    }
#endif
}


CTime::CTime(int year, int month, int day, int hour,
             int minute, int second, long nanosecond,
             ETimeZone tz, ETimeZonePrecision tzp)
{
    CHECK_RANGE_YEAR(year);
    CHECK_RANGE_MONTH(month);
    CHECK_RANGE_DAY(day);
    CHECK_RANGE_HOUR(hour);
    CHECK_RANGE_MIN(minute);
    CHECK_RANGE_SEC(second);
    CHECK_RANGE_NSEC(nanosecond);

    m_Data.year        = year;
    m_Data.month       = month;
    m_Data.day         = day;
    m_Data.hour        = hour;
    m_Data.min         = minute;
    m_Data.sec         = second;
    m_Data.nanosec     = (Int4)nanosecond;
    m_Data.tz          = tz;
    m_Data.tzprec      = tzp;
    m_Data.adjTimeDiff = 0;

    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


CTime::CTime(EInitMode mode, ETimeZone tz, ETimeZonePrecision tzp)
{
    m_Data.tz = tz;
    m_Data.tzprec = tzp;

    if (mode == eCurrent) {
        SetCurrent();
    } else {
        Clear();
    }
}

CTime::CTime(time_t t, ETimeZonePrecision tzp)
{
    m_Data.tz = eGmt;
    m_Data.tzprec = tzp;
    SetTimeT(t);
}


CTime::CTime(const string& str, const CTimeFormat& format,
             ETimeZone tz, ETimeZonePrecision tzp)
{
    m_Data.tz = tz;
    m_Data.tzprec = tzp;

    if (format.IsEmpty()) {
        x_Init(str, GetFormat());
    } else {
        x_Init(str, format);
    }
}


void CTime::SetYear(int year)
{
    CHECK_RANGE_YEAR(year);
    m_Data.year = year;
    int n_days = DaysInMonth();
    if ( m_Data.day > n_days ) {
        m_Data.day = n_days;
    }
    // Additional checks
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetMonth(int month)
{
    CHECK_RANGE_MONTH(month);
    m_Data.month = month;
    int n_days = DaysInMonth();
    if ( m_Data.day > n_days ) {
        m_Data.day = n_days;
    }
    // Additional checks
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetDay(int day)
{
    CHECK_RANGE_DAY(day);
    int n_days = DaysInMonth();
    if ( day > n_days ) {
        m_Data.day = n_days;
    } else {
        m_Data.day = day;
    }
    // Additional checks
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetHour(int hour)
{
    CHECK_RANGE_HOUR(hour);
    m_Data.hour = hour;
}


void CTime::SetMinute(int minute)
{
    CHECK_RANGE_MIN(minute);
    m_Data.min = minute;
}


void CTime::SetSecond(int second)
{
    CHECK_RANGE_SEC(second);
    m_Data.sec = second;
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
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    if (first_day_of_week > eSaturday) {
        NCBI_THROW(CTimeException, eArgument,
                   "CTime:  first day of week parameter is incorrect");
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
            week_num++;
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
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    int y = Year();
    int m = Month();

    y -= int(m < 3);
    return (y + y/4 - y/100 + y/400 + "-bed=pen+mad."[m] + Day()) % 7;
}


int CTime::DaysInMonth(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
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
    for (int i = 0; i < 12; i++) {
        if (month == name[i]) {
            return i+1;
        }
    }
    // Always throw exceptions here.
    // Next if statements avoid compilation warnings.
    if ( name ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  invalid month name");
    }
    return -1;
}


string CTime::MonthNumToName(int month, ENameFormat format)
{
    if (month < 1  ||  month > 12) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  invalid month number");
    }
    month--;
    return format == eFull ? kMonthFull[month] : kMonthAbbr[month];
}


int CTime::DayOfWeekNameToNum(const string& day)
{
    const char** name = day.length() == 3 ? kWeekdayAbbr : kWeekdayFull;
    for (int i = 0; i <= 6; i++) {
        if (day == name[i]) {
            return i;
        }
    }
    // Always throw exceptions here.
    // Next if statements avoid compilation warnings.
    if ( name ) {
        NCBI_THROW(CTimeException,eInvalid,"CTime:  invalid day of week name");
    }
    return -1;
}


string CTime::DayOfWeekNumToName(int day, ENameFormat format)
{
    if (day < 0  ||  day > 6) {
        return kEmptyStr;
    }
    return format == eFull ? kWeekdayFull[day] : kWeekdayAbbr[day];
}


static void s_AddZeroPadInt(string& str, long value, SIZE_TYPE len = 2)
{
    string s_value = NStr::IntToString(value);
    if (s_value.length() < len) {
        str.insert(str.end(), len - s_value.length(), '0');
    }
    str += s_value;
}


void CTime::SetFormat(const CTimeFormat& format)
{
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using s_TlsFormatCleanup().
    CTimeFormat* ptr = new CTimeFormat(format);
    s_TlsFormatTime->SetValue(ptr, s_TlsFormatCleanup);
}


CTimeFormat CTime::GetFormat(void)
{
    CTimeFormat format;
    CTimeFormat* ptr = s_TlsFormatTime->GetValue();
    if ( !ptr ) {
        format.SetFormat(kDefaultFormatTime);
    } else {
        format = *ptr;
    }
    return format;
}


string CTime::AsString(const CTimeFormat& format, TSeconds out_tz) const
{
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
    if ( IsEmpty() ) {
        return kEmptyStr;
    }

    const CTime* t = this;
    CTime* t_out = 0;
    // Adjust time for output timezone
    if (out_tz != eCurrentTimeZone) {
#if defined(TIMEZONE_IS_UNDEFINED)
        ERR_POST_X(4, "Output timezone is unsupported on this platform");
#else
        if (out_tz != TimeZone()) {
            t_out = new CTime(*this);
            t_out->AddSecond(TimeZone() - out_tz);
            t = t_out;
        }
#endif
    }
    string str;
    string fmt;
    CTimeFormat::EType fmt_type;
    if ( format.IsEmpty() ) {
        CTimeFormat f = GetFormat();
        fmt      = f.GetString();
        fmt_type = f.GetType();
    } else {
        fmt      = format.GetString();
        fmt_type = format.GetType();
    }
    bool is_escaped = (fmt_type != CTimeFormat::eNcbiSimple);
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
        case 'y': s_AddZeroPadInt(str, t->Year() % 100);    break;
        case 'Y': s_AddZeroPadInt(str, t->Year(), 4);       break;
        case 'M': s_AddZeroPadInt(str, t->Month());         break;
        case 'b': str += kMonthAbbr[t->Month()-1];          break;
        case 'B': str += kMonthFull[t->Month()-1];          break;
        case 'D': s_AddZeroPadInt(str, t->Day());           break;
        case 'd': s_AddZeroPadInt(str, t->Day(),1);         break;
        case 'h': s_AddZeroPadInt(str, t->Hour());          break;
        case 'H': s_AddZeroPadInt(str, (t->Hour()+11) % 12+1);
                  break;
        case 'm': s_AddZeroPadInt(str, t->Minute());        break;
        case 's': s_AddZeroPadInt(str, t->Second());        break;
        case 'l': s_AddZeroPadInt(str, t->NanoSecond() / 1000000, 3);
                  break;
        case 'r': s_AddZeroPadInt(str, t->NanoSecond() / 1000, 6);
                  break;
        case 'S': s_AddZeroPadInt(str, t->NanoSecond(), 9); break;
        case 'p': str += ( t->Hour() < 12) ? "am" : "pm" ;  break;
        case 'P': str += ( t->Hour() < 12) ? "AM" : "PM" ;  break;
        case 'z': {
#if defined(TIMEZONE_IS_UNDEFINED)
                  ERR_POST_X(5, "Format symbol 'z' is not supported on "
                                "this platform");
#else
                  str += "GMT";
                    if (IsGmtTime()) {
                        break;
                    }
                    TSeconds tz = (out_tz == eCurrentTimeZone) ?
                        TimeZone() : out_tz;
                    str += (tz > 0) ? '-' : '+';
                    if (tz < 0) tz = -tz;
                    int tzh = int(tz / 3600);
                    s_AddZeroPadInt(str, tzh);
                    s_AddZeroPadInt(str, (int)(tz - tzh * 3600) / 60);
#endif
                  break;
                  }
        case 'Z': if (IsGmtTime()) str += "GMT";            break;
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


#if defined (NCBI_OS_MAC)
// Mac OS 9 does not correctly support daylight savings flag.
time_t CTime::GetTimeT(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    struct tm t;
    t.tm_sec   = Second() + (int)(IsGmtTime() ? +TimeZone() : 0);
    t.tm_min   = Minute();
    t.tm_hour  = Hour();
    t.tm_mday  = Day();
    t.tm_mon   = Month()-1;
    t.tm_year  = Year()-1900;
    t.tm_isdst = -1;
    return mktime(&t);
}
#else
time_t CTime::GetTimeT(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    // MT-Safe protect
    CFastMutexGuard LOCK(s_TimeMutex);

    struct tm t;

    // Convert time to time_t value at base local time
#if defined(HAVE_TIMEGM)  ||  (defined(NCBI_OS_DARWIN)  &&  ! defined(NCBI_COMPILER_MW_MSL))
    t.tm_sec   = Second();
#else
    t.tm_sec   = Second() + (int)(IsGmtTime() ? -TimeZone() : 0);
#endif
    t.tm_min   = Minute();
    t.tm_hour  = Hour();
    t.tm_mday  = Day();
    t.tm_mon   = Month()-1;
    t.tm_year  = Year()-1900;
    t.tm_isdst = -1;
#if defined(NCBI_OS_DARWIN) && ! defined(NCBI_COMPILER_MW_MSL)
    time_t tt = mktime(&t);
    return IsGmtTime() ? tt+t.tm_gmtoff : tt;
#elif defined(HAVE_TIMEGM)
    return IsGmtTime() ? timegm(&t) : mktime(&t);
#else
    struct tm *ttemp;
    time_t timer;
    timer = mktime(&t);

    // Correct timezone for GMT time
    if ( IsGmtTime() ) {

       // Call mktime() second time for GMT time !!!
       // 1st - to get correct value of TimeZone().
       // 2nd - to get value "timer".

        t.tm_sec   = Second() - (int)TimeZone();
        t.tm_min   = Minute();
        t.tm_hour  = Hour();
        t.tm_mday  = Day();
        t.tm_mon   = Month()-1;
        t.tm_year  = Year()-1900;
        t.tm_isdst = -1;
        timer = mktime(&t);

#  if defined(HAVE_LOCALTIME_R)
        struct tm temp;
        localtime_r(&timer, &temp);
        ttemp = &temp;
#  else
        ttemp = localtime(&timer);
#  endif
        if (ttemp == NULL)
            return -1;
        if (ttemp->tm_isdst > 0  &&  Daylight())
            timer += 3600;
    }
    return timer;
#endif
}
#endif

TDBTimeU CTime::GetTimeDBU(void) const
{
    TDBTimeU dbt;
    CTime t  = GetLocalTime();
    unsigned first = s_Date2Number(CTime(1900, 1, 1));
    unsigned curr  = s_Date2Number(t);

    dbt.days = (Uint2)(curr - first);
    dbt.time = (Uint2)(t.Hour() * 60 + t.Minute());
    return dbt;
}


TDBTimeI CTime::GetTimeDBI(void) const
{
    TDBTimeI dbt;
    CTime t  = GetLocalTime();
    unsigned first = s_Date2Number(CTime(1900, 1, 1));
    unsigned curr  = s_Date2Number(t);

    dbt.days = (Int4)(curr - first);
    dbt.time = (Int4)((t.Hour() * 3600 + t.Minute() * 60 + t.Second()) * 300) +
               (Int4)((double)t.NanoSecond() * 300 / kNanoSecondsPerSecond);
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
    time.AddNanoSecond((long)((t.time % 300) *
                              (double)kNanoSecondsPerSecond / 300));
    time.ToTime(GetTimeZone());

    *this = time;
    return *this;
}


CTime& CTime::x_SetTimeMTSafe(const time_t* value)
{
    // MT-Safe protect
    CFastMutexGuard LOCK(s_TimeMutex);
    x_SetTime(value);
    return *this;
}

// Get current GMT time with nanoseconds
static void s_GetTimeT(time_t& timer, long& ns)
{
#if defined(NCBI_OS_MSWIN)
    struct _timeb timebuffer;
    _ftime(&timebuffer);
    timer = timebuffer.time;
    ns = (long) timebuffer.millitm *
         (long) (kNanoSecondsPerSecond / kMilliSecondsPerSecond);

#elif defined(NCBI_OS_UNIX)
    struct timeval tp;
    if (gettimeofday(&tp,0) == -1) {
        timer = 0;
        ns = 0;
    } else {
        timer = tp.tv_sec;
        ns = long((double)tp.tv_usec *
                    (double)kNanoSecondsPerSecond /
                    (double)kMicroSecondsPerSecond);
    }
#else // NCBI_OS_MAC
    timer = time(0);
    ns = 0;
#endif
}


CTime& CTime::x_SetTime(const time_t* value)
{
    time_t timer;
    long ns = 0;

	// Get time with nanoseconds
    if ( value ) {
        timer = *value;
    } else {
        s_GetTimeT(timer, ns);
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
#endif
    m_Data.adjTimeDiff = 0;
    m_Data.year        = t->tm_year + 1900;
    m_Data.month       = t->tm_mon + 1;
    m_Data.day         = t->tm_mday;
    m_Data.hour        = t->tm_hour;
    m_Data.min         = t->tm_min;
    m_Data.sec         = t->tm_sec;
    CHECK_RANGE_NSEC(ns);
    m_Data.nanosec     = (Int4)ns;
    return *this;
}


CTime& CTime::AddMonth(int months, EDaylight adl)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
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
    m_Data.year = newYear;
    m_Data.month = (int)newMonth + 1;
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
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
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

    // Make nesessary object
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
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
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
    m_Data.hour = (int)newHour;
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
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
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
    m_Data.min = (int)newMinute;
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
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    if ( !seconds ) {
        return *this;
    }
    int minuteOffset = 0;
    long newSecond = Second();
    s_Offset(&newSecond, seconds, 60, &minuteOffset);
    m_Data.sec = (int)newSecond;
    return AddMinute(minuteOffset, adl);
}


CTime& CTime::AddNanoSecond(long ns)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
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
            m_Data.nanosec = (Int4)(m_Data.nanosec + kNanoSecondsPerSecond/2000) 
                             / 1000000 * 1000000;
            break;
        case eRound_Microsecond:
            m_Data.nanosec = (Int4)(m_Data.nanosec + kNanoSecondsPerSecond/2000000) 
                             / 1000 * 1000;
            break;
        default:
            NCBI_THROW(CTimeException, eArgument,
                       "CTime: rounding precision is out of range");
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
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    if ( IsLocalTime() ) {
        return *this;
    }
    CTime t(*this);
    return t.ToLocalTime();
}


CTime CTime::GetGmtTime(void) const
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    if ( IsGmtTime() ) {
        return *this;
    }
    CTime t(*this);
    return t.ToGmtTime();
}


CTime& CTime::ToTime(ETimeZone tz)
{
    if ( IsEmptyDate() ) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  the date is empty");
    }
    if (GetTimeZone() != tz) {
        struct tm* t;
        time_t timer;
        timer = GetTimeT();
        if (timer == -1)
            return *this;

        // MT-Safe protect
        CFastMutexGuard LOCK(s_TimeMutex);

#if defined(HAVE_LOCALTIME_R)
        struct tm temp;
        if (tz == eLocal) {
            localtime_r(&timer, &temp);
        } else {
            gmtime_r(&timer, &temp);
        }
        t = &temp;
#else
        t = ( tz == eLocal ) ? localtime(&timer) : gmtime(&timer);
#endif
        m_Data.year  = t->tm_year + 1900;
        m_Data.month = t->tm_mon + 1;
        m_Data.day   = t->tm_mday;
        m_Data.hour  = t->tm_hour;
        m_Data.min   = t->tm_min;
        m_Data.sec   = t->tm_sec;
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
    return year % 4 == 0  &&  year % 100 != 0  ||  year % 400 == 0;
}


TSeconds CTime::TimeZoneDiff(void) const
{
    const CTime tl(GetLocalTime());
    const CTime tg(GetGmtTime());

    TSeconds dSecs  = tl.Second() - tg.Second();
    int      dMins  = tl.Minute() - tg.Minute();
    int      dHours = tl.Hour()   - tg.Hour();
    int      dDays  = tl.DiffWholeDays(tg);
    return ((dDays * 24 + dHours) * 60 + dMins) * 60 + dSecs;
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
        t1.ToGmtTime();
        t2.ToGmtTime();
        p1 = &t1;
        p2 = &t2;
    } else {
        p1 =  this;
        p2 = &from;
    }
    TSeconds dSecs  = p1->Second() - p2->Second();
    int      dMins  = p1->Minute() - p2->Minute();
    int      dHours = p1->Hour()   - p2->Hour();
    int      dDays  = p1->DiffWholeDays(*p2);
    return ((dDays * 24 + dHours) * 60 + dMins) * 60 + dSecs;
}


void CTime::x_AdjustDay()
{
    int n_days = DaysInMonth();
    if (Day() > n_days) {
        m_Data.day = n_days;
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
    const int kShift = 4;

    // MT-Safe protect
    CFastMutexGuard LOCK(s_TimeAdjustMutex);

    // Special conversion from <const CTime> to <CTime>
    CTime tmp(from);
    int sign = 0;
    TSeconds diff = 0;
    // Primary procedure call
    if ( shift_time ) {
        sign = ( *this > from ) ? 1 : -1;
        // !!! Run TimeZoneDiff() first for old time value
        diff = -tmp.TimeZoneDiff() + TimeZoneDiff();
        // Correction need's if time already in identical timezone
        if (!diff  ||  diff == m_Data.adjTimeDiff) {
            return *this;
        }
    }
    // Recursive procedure call. Inside below
    // x_AddHour(*, eAdjustDaylight, false)
    else  {
        // Correction need't if difference not found
        if (diff == m_Data.adjTimeDiff) {
            return *this;
        }
    }
    // Make correction with temporary time shift
    time_t t = GetTimeT();
    CTime tn(t + (time_t)diff + 3600 * kShift * sign);
    if (from.GetTimeZone() == eLocal) {
        tn.ToLocalTime();
    }
    tn.SetTimeZonePrecision(GetTimeZonePrecision());

    // Release adjust time mutex
    LOCK.Release();

    // Primary procedure call
    if ( shift_time ) {
        // Cancel temporary time shift
        tn.x_AddHour(-kShift * sign, eAdjustDaylight, false);
        tn.m_Data.adjTimeDiff = (Int4)diff;
    }
    *this = tn;
    return *this;
}



//=============================================================================
//
//  CTimeSpan
//
//=============================================================================

CTimeSpan::CTimeSpan(const string& str, const CTimeFormat& format)
{
    if (format.IsEmpty()) {
        x_Init(str, GetFormat());
    } else {
        x_Init(str, format);
    }
}

void CTimeSpan::x_Init(const string& str, const CTimeFormat& format)
{
    Clear();
    if ( str.empty() ) {
        return;
    }
    const string& fmt = format.GetString();
    CTimeFormat::EType fmt_type = format.GetType();
    bool is_escaped = (fmt_type != CTimeFormat::eNcbiSimple);
    bool is_format_symbol = !is_escaped;

    const char* fff;
    const char* sss = str.c_str();
    int   sign = 1;

    for (fff = fmt.c_str();  *fff != '\0';  fff++) {

        // Skip preceding symbols for some formats
        if ( !is_format_symbol ) {
            if ( *fff == kFormatEscapeSymbol )  {
                is_format_symbol = true;
                continue;
            }
        }
        if ( is_escaped ) {
            is_format_symbol = false;
        }
        // Non-format symbols
        if (strchr(kFormatSymbolsSpan, *fff) == 0) {
            if (*fff == *sss) {
                sss++;
                continue;  // skip matching non-format symbols
            }
            break;  // error: non-matching non-format symbols
        }

        // Sign: if specified that the time span is negative
        if (*fff == '-') {
            if (*sss == '-') {
                sign = -1;
                sss++;
            }
            continue;
        }
        // Other format symbols -- read the next data ingredient
        char value_str[21];
        char* s = value_str;
        for (size_t len = 20;
             len  &&  *sss  &&  isdigit((unsigned char)(*sss));  len--) {
            *s++ = *sss++;
        }
        *s = '\0';
        long value = NStr::StringToLong(value_str);

        switch ( *fff ) {
        case 'd':
            m_Sec += value * 86400L;
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
        default:
            NCBI_THROW(CTimeException, eFormat,
                       "CTimeSpan:  format is incorrect");
        }
    }
    // Normalize time span
    if (sign < 0) {
        Invert();
    }
    x_Normalize();

    // Check on errors
    if (*fff != '\0'  ||  *sss != '\0') {
        NCBI_THROW(CTimeException, eFormat, "CTimeSpan:  format is incorrect");
    }
}


void CTimeSpan::x_Normalize(void)
{
    m_Sec += m_NanoSec / kNanoSecondsPerSecond;
    m_NanoSec %= kNanoSecondsPerSecond;
    // If signs are different then make timespan correction
    if (m_Sec > 0  &&  m_NanoSec < 0) {
        m_Sec--;
        m_NanoSec += kNanoSecondsPerSecond;
    } else if (m_Sec < 0  &&  m_NanoSec > 0) {
        m_Sec++;
        m_NanoSec -= kNanoSecondsPerSecond;
    }
}


void CTimeSpan::SetFormat(const CTimeFormat& format)
{
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using s_TlsFormatCleanup().
    CTimeFormat* ptr = new CTimeFormat(format);
    s_TlsFormatSpan->SetValue(ptr, s_TlsFormatCleanup);
}


CTimeFormat CTimeSpan::GetFormat(void)
{
    CTimeFormat format;
    CTimeFormat* ptr = s_TlsFormatSpan->GetValue();
    if ( !ptr ) {
        format.SetFormat(kDefaultFormatSpan);
    } else {
        format = *ptr;
    }
    return format;
}


string CTimeSpan::AsString(const CTimeFormat& format) const
{
    string str;
    string fmt;
    CTimeFormat::EType fmt_type;
    if ( format.IsEmpty() ) {
        CTimeFormat f = GetFormat();
        fmt      = f.GetString();
        fmt_type = f.GetType();
    } else {
        fmt      = format.GetString();
        fmt_type = format.GetType();
    }
    bool is_escaped = (fmt_type != CTimeFormat::eNcbiSimple);
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
        case 'd': s_AddZeroPadInt(str, abs(GetCompleteDays()));
                  break;
        case 'h': s_AddZeroPadInt(str, abs(x_Hour()));
                  break;
        case 'H': s_AddZeroPadInt(str, abs(GetCompleteHours()), 0);
                  break;
        case 'm': s_AddZeroPadInt(str, abs(x_Minute()));
                  break;
        case 'M': s_AddZeroPadInt(str, abs(GetCompleteMinutes()), 0);
                  break;
        case 's': s_AddZeroPadInt(str, abs(x_Second()));
                  break;
        case 'S': s_AddZeroPadInt(str, abs(GetCompleteSeconds()), 0);
                  break;
        case 'n': s_AddZeroPadInt(str, abs(GetNanoSecondsAfterSecond()), 9);
                  break;
        default : str += *it;
                  break;
        }
    }
    return str;
}


struct SSmartStringItem {
    SSmartStringItem(void) : value(0), str(kEmptyStr), str0(kEmptyStr) {};
    SSmartStringItem(long v, const string& s, const string& s0)
        : value(v), str(s), str0(s0) {};
    long    value;
    string  str;
    string  str0;
};

string CTimeSpan::AsSmartString(ESmartStringPrecision precision,
                                ERound                rounding,
                                ESmartStringZeroMode  zero_mode) const
{
    // Make positive copy
    CTimeSpan diff(*this);
    if ( diff.GetSign() == eNegative ) {
        diff.Invert();
    }

    // Get nanoseconds before rounding
    long nanoseconds = diff.GetNanoSecondsAfterSecond();

    // Named or float precision level
    bool is_named_precision = (precision <= eSSP_Nanosecond);


    // Round time span
    if ( rounding == eRound  ) {

        int adjust_level;

        // Named precision level
        if ( is_named_precision ) {
            adjust_level = precision;
        } else {
            adjust_level = eSSP_Nanosecond;
            // Float precision level
            long  days         = diff.GetCompleteDays();
            int   hours        = diff.x_Hour();
            int   minutes      = diff.x_Minute();
            int   seconds      = diff.x_Second();
            int   adjust_shift = precision - eSSP_Nanosecond - 1;

            if ( days >=365 ) {
                adjust_level = eSSP_Year + adjust_shift;
            } else if (days >= 30) {
                adjust_level = eSSP_Month + adjust_shift;
            } else if (days > 0) {
                adjust_level = eSSP_Day + adjust_shift;
            } else if (hours > 0) {
                adjust_level = eSSP_Hour + adjust_shift;
            } else if (minutes > 0) {
                adjust_level = eSSP_Minute + adjust_shift;
            } else if (seconds > 0) {
                adjust_level = eSSP_Second + adjust_shift;
            }
            if (adjust_level > eSSP_Second) {
                if ( nanoseconds % 1000 == 0 ) {
                    adjust_level = eSSP_Millisecond;
                } else if ( nanoseconds % 1000000 == 0 ) {
                    adjust_level = eSSP_Microsecond;
                }
            }
       }
        // Add adjustment time span
        switch (ESmartStringPrecision(adjust_level)) {
            case eSSP_Year:
                diff += CTimeSpan(365/2, 0, 0, 0);
                break;
            case eSSP_Month:
                diff += CTimeSpan(15, 0, 0, 0);
                break;
            case eSSP_Day:
                diff += CTimeSpan(0, 12, 0, 0);
                break;
            case eSSP_Hour:
                diff += CTimeSpan(0, 0, 30, 0);
                break;
            case eSSP_Minute:
                diff += CTimeSpan(0, 0, 0, 30);
                break;
            case eSSP_Second:
                diff += CTimeSpan(0, 0, 0, 0, kNanoSecondsPerSecond/2);
                break;
            case eSSP_Millisecond:
                diff += CTimeSpan(0, 0, 0, 0, kNanoSecondsPerSecond/2000);
                break;
            case eSSP_Microsecond:
                diff += CTimeSpan(0, 0, 0, 0, kMicroSecondsPerSecond/2000000);
                break;
            default:
                ; // nanoseconds -- nothing to do
        }
    }


    // Prepare data
    typedef SSmartStringItem SItem;
    const int max_count = 7;
    SItem span[max_count];
    long days = diff.GetCompleteDays();

    span[0] = SItem(days/365       , "year",   "this year");    days %= 365;
    span[1] = SItem(days/30        , "month",  "this month");   days %= 30;
    span[2] = SItem(days           , "day",    "today");
    span[3] = SItem(diff.x_Hour()  , "hour",   "0 hours");
    span[4] = SItem(diff.x_Minute(), "minute", "0 minutes");
    span[5] = SItem(diff.x_Second(), "second", "0 seconds");
    switch (precision) {
        case eSSP_Millisecond:
            span[6] = SItem(nanoseconds / 1000000, "millisecond","0 milliseconds");
            break;
        case eSSP_Microsecond:
            span[6] = SItem(nanoseconds / 1000, "microsecond", "0 microseconds");
            break;
        case eSSP_Nanosecond:
            span[6] = SItem(nanoseconds, "nanosecond", "0 nanoseconds");
            break;
        default:
            ; // other not nanoseconds based precisions
    }

    // Result string
    string result;
    int current_precision = is_named_precision ? eSSP_Year  : eSSP_Precision1;

    // Compose result string

    for (int i = 0;  i < max_count  &&  current_precision <= precision;  i++) {
        long val = span[i].value;
        if ( !val ) {
            if ( result.empty() ) {
                if (current_precision == precision  &&
                    current_precision != eSSP_Precision1) {
                    break;
                }
                if ( is_named_precision ) {
                    current_precision++;
                }
                continue;
            }
            if (zero_mode == eSSZ_SkipZero) {
                current_precision++;
                continue;
            } else {
                long sum = 0;
                int  cp = current_precision + 1;
                for (int j = i + 1;
                     j < max_count  &&  (cp <= precision);  j++, cp++) {
                    sum += span[j].value;
                }
                if ( !sum ) {
                    // all trailing parts are zeros -- skip all
                    current_precision = precision;
                    break;
                }
            }
        }
        current_precision++;
        if ( !result.empty() ) {
            result += " ";
        }
        result += NStr::IntToString(val) + " " + span[i].str;
        if (val > 1  ||  val == 0) {
            result += "s";
        }
    }
    if ( result.empty() ) {
        if ( precision > eSSP_Second ) {
            return span[eSSP_Second].str0;
        } else {
            return span[precision].str0;
        }
    }
    return result;
}



//=============================================================================
//
//  CFastLocalTime
//
//=============================================================================

CFastLocalTime::CFastLocalTime(unsigned int sec_after_hour)
    : m_SecAfterHour(sec_after_hour),
      m_LastTuneupTime(0), m_LastSysTime(0),
      m_Timezone(0), m_Daylight(-1), m_IsTuneup(false)
{
#if !defined(TIMEZONE_IS_UNDEFINED)
    m_Timezone = (int)TimeZone();
    m_Daylight = Daylight();
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
    s_GetTimeT(timer, ns);
    x_Tuneup(timer, ns);

    // MT-Safe protect: copy current local time to cached time
    CFastMutexGuard LOCK(s_FastLocalTimeMutex);
    m_LocalTime   = m_TunedTime;
    m_LastSysTime = m_LastTuneupTime;
}


void CFastLocalTime::x_Tuneup(time_t timer, long nanosec)
{
    // Tuneup in progress
    m_IsTuneup = true;

    // MT-Safe protect: use CTime locking mutex
    CFastMutexGuard LOCK(s_TimeMutex);
    m_TunedTime.x_SetTime(&timer);
    m_TunedTime.SetNanoSecond(nanosec);

#if !defined(TIMEZONE_IS_UNDEFINED)
    m_Timezone = (int)TimeZone();
    m_Daylight = Daylight();
#endif
    m_LastTuneupTime = timer;

    // Clear flag
    m_IsTuneup = false;
}


CTime CFastLocalTime::GetLocalTime(void)
{
    // Get system time
    time_t timer;
    long ns;
    s_GetTimeT(timer, ns);

    // Avoid to make time tune up in first m_SecAfterHour for each hour
    // Otherwise do this at each hours/timezone change.
    if ( !m_IsTuneup ) {
        if ( !m_LastTuneupTime  ||
            ((timer / 3600 != m_LastTuneupTime / 3600)  &&
             (timer % 3600 >  (time_t)m_SecAfterHour))
#if !defined(TIMEZONE_IS_UNDEFINED)
            ||  (TimeZone() != m_Timezone  ||  Daylight() != m_Daylight)
#endif
        ) {
            x_Tuneup(timer, ns);
            // MT-Safe protect: copy tuned time to cached local time
            CFastMutexGuard LOCK(s_FastLocalTimeMutex);
            m_LocalTime   = m_TunedTime;
            m_LastSysTime = m_LastTuneupTime;
            return m_LocalTime;
        }
    }
    // MT-Safe protect
    CFastMutexGuard LOCK(s_FastLocalTimeMutex);

    if ( !m_LastTuneupTime ) {
        // MT: other attempt to do first time Tuneup().
        // So, local time is undefined. We cannot use timezone information,
        // because it can be undefined also.
        // Lets make its dirty initialization using UTC time... 
        m_LocalTime = CTime(1970, 1);
        m_LocalTime.AddSecond(timer, CTime::eIgnoreDaylight);
        m_LocalTime.SetNanoSecond(ns);
        // Temporary, setup a tuneup time to current.
        // This variable will be changed soon to correct value,
        // after finishing current Tuneup() process in another thread.
        m_LastTuneupTime = timer;
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
#if !defined(TIMEZONE_IS_UNDEFINED)
    // Get system timer
    time_t timer;
    long ns;
    s_GetTimeT(timer, ns);

    // Avoid to make time tune up in first m_SecAfterHour for each hour
    // Otherwise do this at each hours/timezone change.
    if ( !m_IsTuneup ) {
        if ( !m_LastTuneupTime  ||
            ((timer / 3600 != m_LastTuneupTime / 3600)  &&
             (timer % 3600 >  (time_t)m_SecAfterHour))
#if !defined(TIMEZONE_IS_UNDEFINED)
            ||  (TimeZone() != m_Timezone  ||  Daylight() != m_Daylight)
#endif
        ) {
            x_Tuneup(timer, ns);
            // MT-Safe protect: copy tuned time to cached local time
            CFastMutexGuard LOCK(s_FastLocalTimeMutex);
            m_LocalTime   = m_TunedTime;
            m_LastSysTime = m_LastTuneupTime;
            return m_Timezone;
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

// deprecated
CStopWatch::CStopWatch(bool start)
{
    m_Total = 0;
    m_State = eStop;
    if ( start ) {
        Start();
    }
}

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


void CStopWatch::SetFormat(const CTimeFormat& format)
{
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using s_TlsFormatCleanup().
    CTimeFormat* ptr = new CTimeFormat(format);
    s_TlsFormatStopWatch->SetValue(ptr, s_TlsFormatCleanup);
}


CTimeFormat CStopWatch::GetFormat(void)
{
    CTimeFormat format;
    CTimeFormat* ptr = s_TlsFormatStopWatch->GetValue();
    if ( !ptr ) {
        format.SetFormat(kDefaultFormatStopWatch);
    } else {
        format = *ptr;
    }
    return format;
}


string CStopWatch::AsString(const CTimeFormat& format) const
{
    CTimeSpan ts(Elapsed());
    if ( format.IsEmpty() ) {
        CTimeFormat fmt = GetFormat();
        return ts.AsString(fmt);
    }
    return ts.AsString(format);
}


//============================================================================
//
//  Extern
//
//============================================================================


CTime operator+ (int days, const CTime& t)
{
    CTime tmp = s_Number2Date(s_Date2Number(t) + days, t);
    tmp.x_AdjustTime(t);
    return tmp;
}

int operator- (const CTime& t1, const CTime& t2)
{
    return (int)(s_Date2Number(t1) - s_Date2Number(t2));
}


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
    case eInvalid:   return "eInvalid";
    case eFormat:    return "eFormat";
    default:         return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
