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


BEGIN_NCBI_SCOPE


// Protective mutex
DEFINE_STATIC_FAST_MUTEX(s_TimeMutex);
DEFINE_STATIC_FAST_MUTEX(s_TimeAdjustMutex);

// Store global time/timespan formats in TLS
static CSafeStaticRef< CTls<string> > s_TlsFormatTime;
static CSafeStaticRef< CTls<string> > s_TlsFormatSpan;
static CSafeStaticRef< CTls<string> > s_TlsFormatStopWatch;

static void s_TlsFormatCleanup(string* fmt, void* /* data */)
{
    delete fmt;
}


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

// Error messages
static const string kMsgInvalidTime = "CTime:  invalid";


// Get number of days in "date"
static unsigned s_Date2Number(const CTime& date)
{
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
        CTime (year, month, day, t.Hour(), t.Minute(), t.Second(),
              t.NanoSecond(), t.GetTimeZoneFormat(), t.GetTimeZonePrecision());
}


// Calc <value> + <offset> on module <bound>.
// Normalized value return in <value> and other part, which above
// than <bound>, write at <major>.
static void s_Offset(long *value, long offset, long bound, int *major)
{
    *value += offset;
    *major += (int) (*value / bound);
    *value %= bound;
    if (*value < 0) {
        *major -= 1;
        *value += bound;
    }
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


CTime::CTime(int year, int yearDayNumber, ETimeZone tz, ETimeZonePrecision tzp)
{
    Clear();
    m_Tz = tz;
    m_TzPrecision = tzp;

    CTime t = CTime(year, 1, 1);
    t += yearDayNumber - 1;
    m_Year  = t.Year();
    m_Month = t.Month();
    m_Day   = t.Day();
}


void CTime::x_Init(const string& str, const string& fmt)
{
    Clear();
    if ( str.empty() ) {
        return;
    }

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
        if ( isspace(*fff) ) {
            continue;
        }
        // Skip space symbols in time string
        while ( isspace(*sss) )
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
                    m_Month = i + 1;
                    break;
                }
                name++;
            }
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
                m_Tz = eGmt;
                sss += 3;
            } else {
                m_Tz = eLocal;
            }
            continue;
        }

        // Timezone (local time in format GMT+HHMM)
        if (*fff == 'z') {
#if defined(TIMEZONE_IS_UNDEFINED)
            ERR_POST("Format symbol 'z' is unsupported on this platform");
#else
            m_Tz = eLocal;
            if (NStr::strncasecmp(sss, "GMT", 3) == 0) {
                sss += 3;
            }
            while ( isspace(*sss) ) {
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
                 len != 0  &&  *sss != '\0'  &&  isdigit(*sss);  len--) {
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
                        len != 0  &&  *sss != '\0'  &&  isdigit(*sss);
                        len--) {
                        *s++ = *sss++;
                    }
                    *s = '\0';
                    x_min = NStr::StringToLong(value_str,10,NStr::eCheck_Skip);
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
        for ( ; len != 0  &&  *sss != '\0'  &&  isdigit(*sss);  len--) {
            *s++ = *sss++;
        }
        *s = '\0';
        long value = NStr::StringToLong(value_str);

        // Set time part
        switch ( *fff ) {
        case 'Y':
            m_Year = (int)value;
            break;
        case 'y':
            if (value >= 0  &&  value < 50) {
                value += 2000;
            } else if (value >= 50  &&  value < 100) {
                value += 1900;
            }
            m_Year = (int)value;
            break;
        case 'M':
            m_Month = (unsigned char) value;
            break;
        case 'D':
        case 'd':
            m_Day = (unsigned char) value;
            break;
        case 'h':
            m_Hour = (unsigned char) value;
            break;
        case 'H':
            m_Hour = (unsigned char) value % 12;
            is_12hour = true;
            break;
        case 'm':
            m_Minute = (unsigned char) value;
            break;
        case 's':
            m_Second = (unsigned char) value;
            break;
        case 'l':
            m_NanoSecond = value * 1000000;
            break;
        case 'r':
            m_NanoSecond = value * 1000;
            break;
        case 'S':
            m_NanoSecond = value;
            break;
        default:
            NCBI_THROW(CTimeException, eFormat, "CTime:  format is incorrect");
        }
    }

    // Correct 12-hour time if needed
    if (is_12hour  &&  hourformat == ePM) {
        m_Hour += 12;
    }

    // Check on errors
    if (weekday != -1  &&  weekday != DayOfWeek()) {
        NCBI_THROW(CTimeException, eInvalid, "CTime:  invalid day of week");
    }

    while ( isspace(*sss) )
        sss++;
    if (*fff != '\0'  ||  *sss != '\0') {
        NCBI_THROW(CTimeException, eFormat, "CTime:  format is incorrect");
    }

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
    m_Year           = year;
    m_Month          = month;
    m_Day            = day;
    m_Hour           = hour;
    m_Minute         = minute;
    m_Second         = second;
    m_NanoSecond     = nanosecond;
    m_Tz             = tz;
    m_TzPrecision    = tzp;
    m_AdjustTimeDiff = 0;

    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


CTime::CTime(EInitMode mode, ETimeZone tz, ETimeZonePrecision tzp)
{
    m_Tz = tz;
    m_TzPrecision = tzp;
    if (mode == eCurrent) {
        SetCurrent();
    } else {
        Clear();
    }
}

CTime::CTime(time_t t, ETimeZonePrecision tzp)
{
    m_Tz = eGmt;
    m_TzPrecision = tzp;
    SetTimeT(t);
}


CTime::CTime(const string& str, const string& fmt,
             ETimeZone tz, ETimeZonePrecision tzp)
{
    m_Tz = tz;
    m_TzPrecision = tzp;

    if (fmt.empty()) {
        x_Init(str, GetFormat());
    } else {
        x_VerifyFormat(fmt);
        x_Init(str, fmt);
    }
}


void CTime::SetYear(int year)
{
    m_Year = year;
    int n_days = DaysInMonth();
    if ( m_Day > n_days ) {
        m_Day = n_days;
    }
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetMonth(int month)
{
    m_Month = month;
    int n_days = DaysInMonth();
    if ( m_Day > n_days ) {
        m_Day = n_days;
    }
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetDay(int day)
{
    m_Day = day;
    int n_days = DaysInMonth();
    if ( m_Day > n_days ) {
        m_Day = n_days;
    }
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetHour(int hour)
{
    m_Hour = hour;
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetMinute(int minute)
{
    m_Minute = minute;
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetSecond(int second)
{
    m_Second = second;
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
}


void CTime::SetNanoSecond(long nanosecond)
{
    m_NanoSecond = nanosecond;
    if ( !IsValid() ) {
        NCBI_THROW(CTimeException, eInvalid, kMsgInvalidTime);
    }
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
    int y = Year();
    int m = Month();

    y -= int(m < 3);
    return (y + y/4 - y/100 + y/400 + "-bed=pen+mad."[m] + Day()) % 7;
}


int CTime::DaysInMonth(void) const
{
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

void CTime::x_VerifyFormat(const string& fmt)
{
    // Check for duplicated format symbols...
    const int kSize = 256;
    int count[kSize];
    for (int i = 0;  i < kSize;  i++) {
        count[i] = 0;
    }
    ITERATE(string, it, fmt) {
        if (strchr(kFormatSymbolsTime, *it) != 0  &&
            ++count[(unsigned int) *it] > 1) {
            NCBI_THROW(CTimeException, eFormat, "CTime's format is incorrect");
        }
    }
}

void CTime::SetFormat(const string& fmt)
{
    x_VerifyFormat(fmt);
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using s_TlsFormatCleanup().
    string* format = new string(fmt);
    s_TlsFormatTime->SetValue(format, s_TlsFormatCleanup);
}


string CTime::GetFormat(void)
{
    string* format = s_TlsFormatTime->GetValue();
    if ( !format ) {
        return kDefaultFormatTime;
    }
    return *format;
}

string CTime::AsString(const string& fmt, long out_tz) const
{
    if ( fmt.empty() ) {
        return AsString(GetFormat());
    }

    x_VerifyFormat(fmt);

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
	    ERR_POST("Output timezone is unsupported on this platform");
#else
        if (out_tz != TimeZone()) {
            t_out = new CTime(*this);
            t_out->AddSecond(TimeZone() - out_tz);
            t = t_out;
        }
#endif
    }
    string str;
    ITERATE(string, it, fmt) {
        switch ( *it ) {
        case 'y': s_AddZeroPadInt(str, t->Year() % 100);    break;
        case 'Y': s_AddZeroPadInt(str, t->Year(), 4);       break;
        case 'M': s_AddZeroPadInt(str, t->Month());         break;
        case 'b': str += kMonthAbbr[t->Month()-1];          break;
        case 'B': str += kMonthFull[t->Month()-1];          break;
        case 'D': s_AddZeroPadInt(str, t->Day());           break;
        case 'd': s_AddZeroPadInt(str, t->Day(),1);         break;
        case 'h': s_AddZeroPadInt(str, t->Hour());          break;
        case 'H': s_AddZeroPadInt(str, (t->Hour()+11) % 12+1);     break;
        case 'm': s_AddZeroPadInt(str, t->Minute());        break;
        case 's': s_AddZeroPadInt(str, t->Second());        break;
        case 'l': s_AddZeroPadInt(str, t->NanoSecond() / 1000000, 3);
                  break;
        case 'r': s_AddZeroPadInt(str, t->NanoSecond() / 1000, 6);
                  break;
        case 'S': s_AddZeroPadInt(str, t->NanoSecond(), 9); break;
        case 'p': str += ( t->Hour() < 12) ? "am" : "pm" ; break;
        case 'P': str += ( t->Hour() < 12) ? "AM" : "PM" ; break;
        case 'z': {
#if defined(TIMEZONE_IS_UNDEFINED)
                      ERR_POST("Format symbol 'z' is not supported on "
                               "this platform");
#else
                      str += "GMT";
                      if (IsGmtTime()) {
                          break;
                      }
                      long tz = (out_tz == eCurrentTimeZone) ?
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
    time.ToTime(GetTimeZoneFormat());

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
    time.ToTime(GetTimeZoneFormat());

    *this = time;
    return *this;
}


CTime& CTime::x_SetTime(const time_t* value)
{
    long ns = 0;
    time_t timer;

    // MT-Safe protect
    CFastMutexGuard LOCK(s_TimeMutex);

    // Get time with nanoseconds
#if defined(NCBI_OS_MSWIN)

    if ( value ) {
        timer = *value;
    } else {
        struct _timeb timebuffer;
        _ftime(&timebuffer);
        timer = timebuffer.time;
        ns = (long) timebuffer.millitm *
            (long) (kNanoSecondsPerSecond / kMilliSecondsPerSecond);
    }
#elif defined(NCBI_OS_UNIX)

    if ( value ) {
        timer = *value;
    } else {
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
    }

#else // NCBI_OS_MAC

    timer = value ? *value : time(0);
    ns = 0;
#endif

    // Bind values to internal variables
    struct tm *t;

#ifdef HAVE_LOCALTIME_R
    struct tm temp;
    if (GetTimeZoneFormat() == eLocal) {
        localtime_r(&timer, &temp);
    } else {
        gmtime_r(&timer, &temp);
    }
    t = &temp;
#else
    t = ( GetTimeZoneFormat() == eLocal ) ? localtime(&timer) : gmtime(&timer);
#endif
    m_AdjustTimeDiff = 0;
    m_Year           = t->tm_year + 1900;
    m_Month          = t->tm_mon + 1;
    m_Day            = t->tm_mday;
    m_Hour           = t->tm_hour;
    m_Minute         = t->tm_min;
    m_Second         = t->tm_sec;
    m_NanoSecond     = ns;

    return *this;
}


CTime& CTime::AddMonth(int months, EDaylight adl)
{
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
    m_Year = newYear;
    m_Month = (int)newMonth + 1;
    x_AdjustDay();
    if ( aflag ) {
        x_AdjustTime(*pt);
        delete pt;
    }
    return *this;
}


CTime& CTime::AddDay(int days, EDaylight adl)
{
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
    m_Hour = (int)newHour;
    AddDay(dayOffset, eIgnoreDaylight);
    if ( aflag ) {
        x_AdjustTime(*pt, shift_time);
        delete pt;
    }
    return *this;
}


CTime& CTime::AddMinute(int minutes, EDaylight adl)
{
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
    m_Minute = (int)newMinute;
    AddHour(hourOffset, eIgnoreDaylight);
    if ( aflag ) {
        x_AdjustTime(*pt);
        delete pt;
    }
    return *this;
}


CTime& CTime::AddSecond(long seconds)
{
    if ( !seconds ) {
        return *this;
    }
    int minuteOffset = 0;
    long newSecond = Second();
    s_Offset(&newSecond, seconds, 60, &minuteOffset);
    m_Second = (int)newSecond;
    return AddMinute(minuteOffset);
}


CTime& CTime::AddNanoSecond(long ns)
{
    if ( !ns ) {
        return *this;
    }
    int secondOffset = 0;
    long newNanoSecond = NanoSecond();
    s_Offset(&newNanoSecond, ns, kNanoSecondsPerSecond, &secondOffset);
    m_NanoSecond = newNanoSecond;
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
    if (Second() < 0  ||  Second() > 59)
        return false;
    if (NanoSecond() < 0  ||  NanoSecond() >= kNanoSecondsPerSecond)
        return false;

    return true;
}


CTime& CTime::ToTime(ETimeZone tz)
{
    if (GetTimeZoneFormat() != tz) {
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
        m_Year   = t->tm_year + 1900;
        m_Month  = t->tm_mon + 1;
        m_Day    = t->tm_mday;
        m_Hour   = t->tm_hour;
        m_Minute = t->tm_min;
        m_Second = t->tm_sec;
        m_Tz     = tz;
    }
    return *this;
}


bool CTime::operator== (const CTime& t) const
{
    CTime tmp(t);
    tmp.ToTime(GetTimeZoneFormat());
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
    tmp.ToTime(GetTimeZoneFormat());

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
    tmp.ToTime(GetTimeZoneFormat());

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
    if (NanoSecond() > tmp.NanoSecond())
        return true;

    return false;
}


bool CTime::IsLeap(void) const
{
    int year = Year();
    return year % 4 == 0  &&  year % 100 != 0  ||  year % 400 == 0;
}


CTime& CTime::Truncate(void)
{
    m_Hour           = 0;
    m_Minute         = 0;
    m_Second         = 0;
    m_NanoSecond     = 0;
    m_AdjustTimeDiff = 0;
    return *this;
}


CTime& CTime::Clear()
{
    m_Year  = 0;
    m_Month = 0;
    m_Day   = 0;
    Truncate();
    return *this;
}


int CTime::DiffSecond(const CTime& t) const
{
    int dSec  = Second() - t.Second();
    int dMin  = Minute() - t.Minute();
    int dHour = Hour()   - t.Hour();
    int dDay  = (*this)  - t;
    return dSec + 60 * dMin + 60 * 60 * dHour + 60 * 60 * 24 * dDay;
}


CTimeSpan CTime::DiffTimeSpan(const CTime& t) const
{
    CTimeSpan ts((*this)  - t,
                 Hour()   - t.Hour(),
                 Minute() - t.Minute(),
                 Second() - t.Second(),
                 NanoSecond() - t.NanoSecond());
    return ts;
}

void CTime::x_AdjustDay()
{
    int n_days = DaysInMonth();
    if (Day() > n_days) {
        m_Day = n_days;
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
    int diff = 0;
    // Primary procedure call
    if ( shift_time ) {
        sign = ( *this > from ) ? 1 : -1;
        // !!! Run TimeZoneDiff() first for old time value
        diff = -tmp.TimeZoneDiff() + TimeZoneDiff();
        // Correction need's if time already in identical timezone
        if (!diff  ||  diff == m_AdjustTimeDiff) {
            return *this;
        }
    }
    // Recursive procedure call. Inside below
    // x_AddHour(*, eAdjustDaylight, false)
    else  {
        // Correction need't if difference not found
        if (diff == m_AdjustTimeDiff) {
            return *this;
        }
    }
    // Make correction with temporary time shift
    time_t t = GetTimeT();
    CTime tn(t + diff + 3600 * kShift * sign);
    if (from.GetTimeZoneFormat() == eLocal) {
        tn.ToLocalTime();
    }
    tn.SetTimeZonePrecision(GetTimeZonePrecision());

    // Release adjust time mutex
    LOCK.Release();

    // Primary procedure call
    if ( shift_time ) {
        // Cancel temporary time shift
        tn.x_AddHour(-kShift * sign, eAdjustDaylight, false);
        tn.m_AdjustTimeDiff = diff;
    }
    *this = tn;
    return *this;
}



//=============================================================================
//
//  CTimeSpan
//
//=============================================================================

CTimeSpan::CTimeSpan(const string& str, const string& fmt)
{
    if (fmt.empty()) {
        x_Init(str, GetFormat());
    } else {
        x_VerifyFormat(fmt);
        x_Init(str, fmt);
    }
}

void CTimeSpan::x_Init(const string& str, const string& fmt)
{
    Clear();
    if ( str.empty() ) {
        return;
    }

    const char* fff;
    const char* sss = str.c_str();
    int   sign = 1;

    for (fff = fmt.c_str();  *fff != '\0';  fff++) {

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
             len != 0  &&  *sss != '\0'  &&  isdigit(*sss);  len--) {
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
    // If signs is different, than make timespan correction
    if ( (m_Sec > 0) && (m_NanoSec < 0)) {
        m_Sec--;
        m_NanoSec = kNanoSecondsPerSecond + m_NanoSec;
    } else  if ((m_Sec < 0) && (m_NanoSec > 0)) {
        m_Sec++;
        m_NanoSec = m_NanoSec - kNanoSecondsPerSecond;
    }
    m_Sec += (m_NanoSec / kNanoSecondsPerSecond);
    m_NanoSec %= kNanoSecondsPerSecond;
}


void CTimeSpan::x_VerifyFormat(const string& fmt)
{
    // Check for duplicated format symbols...
    const int kSize = 256;
    int count[kSize];
    for (int i = 0;  i < kSize;  i++) {
        count[i] = 0;
    }
    ITERATE(string, it, fmt) {
        if (strchr(kFormatSymbolsSpan, *it) != 0  &&
            ++count[(unsigned int) *it] > 1) {
            NCBI_THROW(CTimeException, eFormat,
                       "CTimeSpan's format is incorrect");
        }
    }
}


void CTimeSpan::SetFormat(const string& fmt)
{
    x_VerifyFormat(fmt);
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using s_TlsFormatCleanup().
    string* format = new string(fmt);
    s_TlsFormatSpan->SetValue(format, s_TlsFormatCleanup);
}


string CTimeSpan::GetFormat(void)
{
    string* format = s_TlsFormatSpan->GetValue();
    if ( !format ) {
        return kDefaultFormatSpan;
    }
    return *format;
}


string CTimeSpan::AsString(const string& fmt) const
{
    if ( fmt.empty() ) {
        return AsString(GetFormat());
    }
    x_VerifyFormat(fmt);

    string str;
    ITERATE(string, it, fmt) {
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
                // nanoseconds -- nothing to do
		;
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
    if ( nanoseconds % 1000000 == 0 ) {
        span[6] = SItem(nanoseconds / 1000000, "millisecond","0 milliseconds");
    } else if ( nanoseconds % 1000 == 0 ) {
        span[6] = SItem(nanoseconds / 1000, "microsecond", "0 microseconds");
    } else {
        span[6] = SItem(nanoseconds, "nanosecond", "0 nanoseconds");
    }

    // Result string
    string result;
    int current_precision = is_named_precision ? eSSP_Year  : eSSP_Precision1;

    // Compose result string

    for (int i = 0;  i < max_count  &&  current_precision <= precision;  i++) {
        long val = span[i].value;
        if ( !val ) {
            if ( result.empty() ) {
                if (current_precision == precision) {
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
//  CStopWatch
//
//=============================================================================

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


void CStopWatch::SetFormat(const string& fmt)
{
    // Here we do not need to delete a previous value stored in the TLS.
    // The TLS will destroy it using s_TlsFormatCleanup().
    string* format = new string(fmt);
    s_TlsFormatStopWatch->SetValue(format, s_TlsFormatCleanup);
}


string CStopWatch::GetFormat(void)
{
    string* format = s_TlsFormatStopWatch->GetValue();
    if ( !format ) {
        return kDefaultFormatStopWatch;
    }
    return *format;
}


string CStopWatch::AsString(const string& fmt) const
{
    if ( fmt.empty() ) {
        return AsString(GetFormat());
    }
    CTimeSpan ts(Elapsed());
    return ts.AsString(fmt);
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


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.58  2005/02/07 16:01:04  ivanov
 * Changed parameter type in the CTime::AddSecons() to long.
 * Fixed Workshop 64bits compiler warnings.
 *
 * Revision 1.57  2004/12/29 21:41:30  vasilche
 * Fixed parsing and formatting of twelfth hour in AM/PM mode.
 *
 * Revision 1.56  2004/09/27 23:16:56  vakatov
 * Fixed a typo;  +cosmetics
 *
 * Revision 1.55  2004/09/27 17:12:06  ucko
 * Tweak to avoid defining struct SItem within AsSmartString, which
 * MIPSpro can't handle.
 *
 * Revision 1.54  2004/09/27 13:53:34  ivanov
 * + CTimeSpan::AsSmartString()
 *
 * Revision 1.53  2004/09/20 16:27:26  ivanov
 * CTime:: Added milliseconds, microseconds and AM/PM to string time format.
 *
 * Revision 1.52  2004/09/07 18:47:49  ivanov
 * CTimeSpan:: renamed GetTotal*() -> GetComplete*()
 *
 * Revision 1.51  2004/09/07 16:31:51  ivanov
 * Added CTimeSpan class and its support to CTime class (addition/subtraction)
 * CTime:: added new format letter support 'd', day without leading zero.
 * CStopWatch:: added operatir << to dumps the current stopwatch time to an
 * output stream.
 * Comments and other cosmetic changes.
 *
 * Revision 1.50  2004/08/09 20:48:41  vakatov
 * Type conv warning fix
 *
 * Revision 1.49  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.48  2004/03/26 16:05:29  ivanov
 * Format symbol 'z' and output time zone for AsString are unsupported
 * on NCBI_OS_DARWIN and NCBI_OS_BSD (problems with determinition daylight
 * flag for specified time).
 *
 * Revision 1.47  2004/03/25 13:03:31  ivanov
 * Temporary fix for NCBI_OS_DARWIN and NCBI_OS_BSD
 *
 * Revision 1.46  2004/03/24 18:47:47  ivanov
 * Replaced abs() with if-clause
 *
 * Revision 1.45  2004/03/24 15:52:08  ivanov
 * Added new format symbol support 'z' (local time in format GMT{+|-}HHMM).
 * Added second parameter to AsString() method that specify an output
 * timezone.
 *
 * Revision 1.44  2003/11/25 19:55:49  ivanov
 * Added setters for various components of time -- Set*().
 * Added YearWeekNumber(), MonthWeekNumber().
 * Reimplemented AddYear() as AddMonth(years*12).
 * Changed DayOfWeek() to use other computation method.
 *
 * Revision 1.43  2003/11/21 20:04:23  ivanov
 * + DaysInMonth()
 *
 * Revision 1.42  2003/10/06 13:59:01  ivanov
 * Some cosmetics
 *
 * Revision 1.41  2003/10/06 13:30:37  ivanov
 * Fixed cut&paste bug in the MonthNumToName()
 *
 * Revision 1.40  2003/10/03 18:27:06  ivanov
 * Added month and day of week names conversion functions
 *
 * Revision 1.39  2003/09/29 21:20:17  golikov
 * x_Init: create empty CTime obj if str.empty() true, ignore format
 *
 * Revision 1.38  2003/07/15 20:09:22  ivanov
 * Fixed some memory leaks
 *
 * Revision 1.37  2003/07/15 19:37:03  vakatov
 * CTime::x_Init() -- recognize (but then just skip, ignoring the value)
 * the weekday
 *
 * Revision 1.36  2003/04/23 21:07:31  ivanov
 * CStopWatch::GetTimeMark: removed 'f' to avoid a conversion float
 * 1e6f to double.
 *
 * Revision 1.35  2003/04/16 20:28:00  ivanov
 * Added class CStopWatch
 *
 * Revision 1.34  2003/04/04 16:02:38  lavr
 * Lines wrapped at 79th column; some minor reformatting
 *
 * Revision 1.33  2003/04/03 14:15:48  rsmith
 * combine pp symbols NCBI_COMPILER_METROWERKS & _MSL_USING_MW_C_HEADERS
 * into NCBI_COMPILER_MW_MSL
 *
 * Revision 1.32  2003/04/02 16:22:34  rsmith
 * clean up metrowerks ifdefs.
 *
 * Revision 1.31  2003/04/02 13:31:18  rsmith
 * change #ifdefs to allow compilation on MacOSX w/codewarrior
 * using MSL headers.
 *
 * Revision 1.30  2003/02/10 17:17:30  lavr
 * Fix off-by-one bug in DayOfWeek() calculation
 *
 * Revision 1.29  2002/10/24 15:16:37  ivanov
 * Fixed bug with using two obtainments of the current time in x_SetTime().
 *
 * Revision 1.28  2002/10/18 20:13:01  ivanov
 * Get rid of some Sun Workshop compilation warnings
 *
 * Revision 1.27  2002/10/17 16:55:30  ivanov
 * Added new time format symbols - 'b' and 'B' (month abbreviated and full name)
 *
 * Revision 1.26  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.25  2002/08/19 14:02:24  ivanov
 * MT-safe fix for ToTime()
 *
 * Revision 1.24  2002/07/23 19:51:17  lebedev
 * NCBI_OS_MAC: GetTimeT fix
 *
 * Revision 1.23  2002/07/15 18:17:25  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.22  2002/07/11 14:18:28  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.21  2002/06/26 20:47:45  lebedev
 * Darwin specific: ncbitime changes
 *
 * Revision 1.20  2002/06/19 17:18:05  ucko
 * Fix confusing indentation introduced by R1.19.
 *
 * Revision 1.19  2002/06/19 16:18:36  lebedev
 * Added CoreServices.h for Darwin (timezone and daylight
 *
 * Revision 1.18  2002/06/18 16:08:01  ivanov
 * Fixed all clauses "#if defined *" to "#if defined(*)"
 *
 * Revision 1.17  2002/05/13 13:56:46  ivanov
 * Added MT-Safe support
 *
 * Revision 1.16  2002/04/11 21:08:04  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.15  2002/03/25 17:08:17  ucko
 * Centralize treatment of Cygwin as Unix rather than Windows in configure.
 *
 * Revision 1.14  2002/03/22 19:59:29  ucko
 * Use timegm() when available [fixes FreeBSD build].
 * Tweak to work on Cygwin.
 *
 * Revision 1.13  2001/07/23 16:05:57  ivanov
 * Fixed bug in Get/Set DB-time formats (1 day difference)
 *
 * Revision 1.12  2001/07/06 15:11:11  ivanov
 * Added support DataBase-time's -- GetTimeDBI(), GetTimeDBU()
 *                                  SetTimeDBI(), SetTimeDBU()
 *
 * Revision 1.11  2001/06/20 14:46:17  vakatov
 * Get rid of the '^M' symbols introduced in the R1.10 log
 *
 * Revision 1.10  2001/06/19 23:03:46  juran
 * Replace timezone and daylight with macros
 * Implement for Mac OS
 * Note:  This compiles, but it may not work correctly yet.
 *
 * Revision 1.9  2001/05/29 20:14:03  ivanov
 * Added #include <sys/time.h> for UNIX platforms.
 *
 * Revision 1.8  2001/05/29 16:14:01  ivanov
 * Return to nanosecond-revision. Corrected mistake of the work with local
 * time on Linux. Polish and improvement source code.
 * Renamed AsTimeT() -> GetTimerT().
 *
 * Revision 1.7  2001/05/17 15:05:00  lavr
 * Typos corrected
 *
 * Revision 1.6  2001/04/30 22:01:30  lavr
 * Rollback to pre-nanosecond-revision due to necessity to use
 * configure to figure out names of global variables governing time zones
 *
 * Revision 1.5  2001/04/29 03:06:09  lavr
 * #include <time.h>" moved from .cpp to ncbitime.hpp
 *
 * Revision 1.4  2001/04/27 20:38:14  ivanov
 * Support for Local and UTC time added.
 * Support for work with nanoseconds added.
 *
 * Revision 1.3  2001/01/03 17:53:05  butanaev
 * Fixed bug in SetCurrent()
 *
 * Revision 1.2  2000/11/21 18:14:58  butanaev
 * Fixed bug in operator ++/-- (int)
 *
 * Revision 1.1  2000/11/20 22:17:46  vakatov
 * Added NCBI date/time class CTime ("ncbitime.[ch]pp") and
 * its test suite ("test/test_ncbitime.cpp")
 *
 * ===========================================================================
 */
