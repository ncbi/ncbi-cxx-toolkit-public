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
* Authors:  Anton    Butanayev <butanaev@ncbi.nlm.nih.gov>
*           Denis    Vakatov   <vakatov@ncbi.nlm.nih.gov>
*           Vladimir Ivanov    <ivanov@ncbi.nlm.nih.gov>
*
* ---------------------------------------------------------------------------
* $Log$
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


#include <corelib/ncbitime.hpp>
#include <stdlib.h>

#if defined NCBI_OS_MSWIN
#   include <sys/timeb.h>
#elif defined NCBI_OS_UNIX
#   include <sys/time.h>
#endif

#ifdef NCBI_OS_MAC
#	include <OSUtils.h>
typedef
struct MyTZDLS {
	long timezone;
	bool daylight;
} MyTZDLS;
static MyTZDLS MyReadLocation()
{
	MachineLocation loc;
	ReadLocation(&loc);
	long tz = loc.u.gmtDelta & 0x00ffffff;
	bool dls = (loc.u.dlsDelta != 0);
	MyTZDLS tzdls = {tz, dls};
	return tzdls;
}
static MyTZDLS sTZDLS = MyReadLocation();
#	define TimeZone() sTZDLS.timezone
#	define Daylight() sTZDLS.daylight
#else
#	define TimeZone() timezone
#	define Daylight() daylight
#endif

BEGIN_NCBI_SCOPE


//============================================================================
//
//  CTime
//
//============================================================================


// Day's count in months
static int s_DaysInMonth[] = {31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


// Default value for time format
string CTime::sm_Format = "M/D/Y h:m:s";


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
    
    return 
        ((146097 * c) >> 2) + ((1461 * ya) >> 2) +
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
    if ( *value < 0 ) {
        *major -= 1;
        *value += bound;
    }
}


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


void CTime::x_VerifyFormat(const string& fmt)
{
    // Check for duplicated format symbols...
    const int kSize = 256;
    int count[kSize];
    for (int i = 0;  i < kSize;  i++) {
        count[i] = 0;
    }
    for (string::const_iterator j = fmt.begin();  j != fmt.end();  ++j) {
        if (strchr("YyMDhmsSZ", *j) != 0  &&  ++count[(unsigned int) *j] > 1) {
            THROW1_TRACE(runtime_error, 
                        "CTime:: duplicated symbols in format: `" + fmt + "'");
        }
    }
}


void CTime::x_Init(const string& str, const string& fmt)
{
    Clear();

    const char* fff;
    const char* sss = str.c_str();

    for ( fff = fmt.c_str();  *fff != '\0';  fff++ ) {

        // Process non-format symbols
        if (strchr("YyMDhmsSZ", *fff) == 0) {
            if ( *fff == *sss ) {
                sss++;
                continue;  // skip matching non-format symbols
            }
            break;  // error: non-matching non-format symbols
        }

        // Process timezone format symbol
        if ( *fff == 'Z' ) {
            if ( strncmp(sss, "GMT", 3) == 0 ) {
                m_Tz = eGmt;
                sss += 3;
            } else {
                m_Tz = eLocal;
            }
            continue;
        }

        // Process format symbols ("YyMDhmsS") - read the next data ingredient
        char value_str[10];
        char* s = value_str;
        for ( size_t len = (*fff == 'Y') ? 4 : ((*fff == 'S') ? 9 : 2 );
             len != 0  &&  *sss != '\0'  &&  isdigit(*sss);  len--) {
            *s++ = *sss++; 
        }
        *s = '\0';

        long value = NStr::StringToLong(value_str);

        switch ( *fff ) {
        case 'Y':
            m_Year = (int) value;
            break;
        case 'y':
            if ( value >= 0  &&  value < 50 ) {
                value += 2000;
            } else if ( value >= 50  &&  value < 100 ) {
                value += 1900;
            }
            m_Year = (int) value;
            break;
        case 'M':
            m_Month = (unsigned char) value;
            break;
        case 'D':
            m_Day = (unsigned char) value;
            break;
        case 'h':
            m_Hour = (unsigned char) value;
            break;
        case 'm':
            m_Minute = (unsigned char) value;
            break;
        case 's':
            m_Second = (unsigned char) value;
            break;
        case 'S':
            m_NanoSecond = value;
            break;
        default:
            _TROUBLE;
        }
    }
    // Check on errors
    if ( *fff != '\0'  ||  *sss != '\0' ) {
        THROW1_TRACE(runtime_error, "CTime:: format mismatch:  `" + fmt + 
                     "' <-- `" + str + "'");
    }
    if ( !IsValid() ) {
        THROW1_TRACE(runtime_error, "CTime:: not valid:  `" + str + "'");
    }
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
        THROW1_TRACE(runtime_error, "CTime::CTime() passed invalid time");
    }
}


CTime::CTime(EInitMode mode, ETimeZone tz, ETimeZonePrecision tzp)
{
    m_Tz = tz;
    m_TzPrecision = tzp;
    if ( mode == eCurrent ) {
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
        x_Init(str, sm_Format);
    } else {
        x_VerifyFormat(fmt);
        x_Init(str, fmt);
    }
}


int CTime::YearDayNumber(void) const
{
    unsigned first = s_Date2Number(CTime(Year(), 1, 1));
    unsigned self  = s_Date2Number(*this);
    _ASSERT(first <= self  &&  self < first + (IsLeap() ? 366 : 365));
    return (int) (self - first + 1);
}


int CTime::DayOfWeek(void) const 
{
    int c = Year() / 100;
    int y = Year() % 100;
    int m = Month() - 2;
    if ( m < 0 ) {
        m += 12;
        y--;
    }
    return ((13*m-1)/5 + Day() + y + y/4 + c/4 - (2*c)%7 + 7) % 7;
}


static void s_AddZeroPadInt(string& str, long value, SIZE_TYPE len = 2)
{
    string s_value = NStr::IntToString(value);
    if ( s_value.length() < len ) {
        str.insert(str.end(), len - s_value.length(), '0');
    }
    str += s_value;
}


string CTime::AsString(const string& fmt) const
{
    if ( fmt.empty() ) {
        return AsString(sm_Format);
    }

    x_VerifyFormat(fmt);

    if ( !IsValid() ) {
        THROW1_TRACE(runtime_error, "CTime::AsString() of invalid time");
    }

    if ( IsEmpty() ) {
        return kEmptyStr;
    }
  
    string str;
    for ( string::const_iterator it = fmt.begin();  it != fmt.end();  ++it ) {
        static const char* s_DaysOfWeekShort[7] = {
            "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
        static const char* s_DaysOfWeekLong [7] = {
            "Sunday","Monday","Tuesday","Wednesday",
                "Thursday","Friday","Saturday" };

        switch ( *it ) {
        case 'Y':  s_AddZeroPadInt(str, Year(), 4);       break;
        case 'y':  s_AddZeroPadInt(str, Year() % 100);    break;
        case 'M':  s_AddZeroPadInt(str, Month());         break;
        case 'D':  s_AddZeroPadInt(str, Day());           break;
        case 'h':  s_AddZeroPadInt(str, Hour());          break;
        case 'm':  s_AddZeroPadInt(str, Minute());        break;
        case 's':  s_AddZeroPadInt(str, Second());        break;
        case 'S':  s_AddZeroPadInt(str, NanoSecond(), 9); break;
        case 'Z':  if (IsGmtTime()) str += "GMT";         break;
        case 'w':  str += s_DaysOfWeekShort[DayOfWeek()]; break;
        case 'W':  str += s_DaysOfWeekLong[DayOfWeek()];  break;
        default :
            str += *it;  break;
        }
    }
    return str;
}


time_t CTime::GetTimeT(void) const
{
    struct tm t;
    struct tm *ttemp;
    time_t timer;

    // Convert time to time_t value at base local time
    t.tm_sec   = Second() + (int) (IsGmtTime() ? -TimeZone() : 0);
    t.tm_min   = Minute();
    t.tm_hour  = Hour();
    t.tm_mday  = Day();
    t.tm_mon   = Month()-1;
    t.tm_year  = Year()-1900;
    t.tm_isdst = -1;
    timer = mktime(&t);

    // Correct timezone for GMT time
    if ( IsGmtTime() ) {
        if ((ttemp = localtime(&timer)) == NULL)
            return -1;
        if (ttemp->tm_isdst > 0  &&  Daylight())
            timer += 3600;
    }
    return timer;
}


TDBTimeU CTime::GetTimeDBU(void) const
{
    TDBTimeU dbt;
    CTime t  = GetLocalTime();
    unsigned first = s_Date2Number(CTime(1900, 1, 1));
    unsigned curr  = s_Date2Number(t);

    dbt.days = (Uint2)(curr - first + 1);
    dbt.time = (Uint2)(t.Hour() * 60 + t.Minute());
    return dbt;
}


TDBTimeI CTime::GetTimeDBI(void) const
{
    TDBTimeI dbt;
    CTime t  = GetLocalTime();
    unsigned first = s_Date2Number(CTime(1900, 1, 1));
    unsigned curr  = s_Date2Number(t);

    dbt.days = (Int4)(curr - first + 1);
    dbt.time = (Int4)((t.Hour() * 3600 + t.Minute() * 60 + t.Second()) * 300) +
        (Int4)((double)t.NanoSecond() * 300 / kNanoSecondsPerSecond);
    return dbt;
}


CTime& CTime::SetTimeDBU(const TDBTimeU& t)
{
    // Local time - 1/1/1900 00:00:00.0
    CTime time(1900, 1, 1, 0, 0, 0, 0, eLocal);

    time.SetTimeZonePrecision(GetTimeZonePrecision());
    if (t.days > 0) {
        time.AddDay(t.days-1);
    }
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
    if (t.days > 0) {
        time.AddDay(t.days-1);
    }
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

    // Get time with nanoseconds
#if defined NCBI_OS_MSWIN

    if (value) {
        timer = *value;
    } else {
        struct _timeb timebuffer;
        _ftime(&timebuffer);
        timer = timebuffer.time;
        ns = (long) timebuffer.millitm * 
            (long) (kNanoSecondsPerSecond / kMilliSecondsPerSecond);
    }
#elif defined NCBI_OS_UNIX

    timer = value ? *value : time(0);
    //    struct timespec tp;
    //    ns = !clock_gettime(CLOCK_REALTIME,&tp) ? tp.tv_nsec : 0;
    struct timeval tp;
    ns = (gettimeofday(&tp,0) == -1) ? 0 : 
        (long) tp.tv_usec * 
        (long) (kNanoSecondsPerSecond / kMicroSecondsPerSecond);

#else // NCBI_OS_MAC

    timer = value ? *value : time(0);
    ns = 0;
#endif

    // Bind values to internal variables
    struct tm *t;
    t = ( GetTimeZoneFormat() == eLocal ) ? localtime(&timer) : gmtime(&timer);
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


CTime& CTime::AddYear(int years, EDaylight adl)
{
    if ( !years )
        return *this;

    CTime *pt = 0;
    bool aflag = false; 
    if ( (adl == eAdjustDaylight)  &&  x_NeedAdjustTime() ) {
        pt = new CTime(*this);
        if ( !pt ) {
            THROW1_TRACE(runtime_error,
                         "CTime::AddYear() error allocate memory");
        }
        aflag = true;
    }
    m_Year = Year() + years;
    x_AdjustDay();
    if ( aflag )
        x_AdjustTime(*pt);
    return *this;
}


CTime& CTime::AddMonth(int months, EDaylight adl)
{
    if ( !months )
        return *this;

    CTime *pt = 0;
    bool aflag = false; 
    if ( (adl == eAdjustDaylight)  &&  x_NeedAdjustTime() ) {
        pt = new CTime(*this);
        if ( !pt ) {
            THROW1_TRACE(runtime_error,
                         "CTime::AddMonth() error allocate memory");
        }
        aflag = true;
    }
    long newMonth = Month() - 1;
    int newYear = Year();
    s_Offset(&newMonth, months, 12, &newYear);
    m_Year = newYear;
    m_Month = (int) newMonth + 1;
    x_AdjustDay();
    if ( aflag )
        x_AdjustTime(*pt);
    return *this;
}


CTime& CTime::AddDay(int days, EDaylight adl)
{
    if ( !days )
        return *this;
    CTime *pt = 0;
    bool aflag = false; 
    if ( (adl == eAdjustDaylight)  &&  x_NeedAdjustTime() ) {
        pt = new CTime(*this);
        if ( !pt ) {
            THROW1_TRACE(runtime_error,
                         "CTime::AddDay() error allocate memory");
        }
        aflag = true;
    }

    // Make nesessary object
    *this = s_Number2Date(s_Date2Number(*this) + days, *this);

    // If need, make adjustment time specially
    if (aflag) 
        x_AdjustTime(*pt);
    return *this;
}


// Parameter <shift_time> access or denied use time shift in process 
// adjust hours.
CTime& CTime::x_AddHour(int hours, EDaylight adl, bool shift_time)
{
    if ( !hours ) 
        return *this;
    CTime *pt = 0;
    bool aflag = false; 
    if ( (adl == eAdjustDaylight)  &&  x_NeedAdjustTime() ) {
        pt = new CTime(*this);
        if ( !pt ) {
            THROW1_TRACE(runtime_error,
                         "CTime::x_AddHour() error allocate memory");
        }
        aflag = true;
    }
    int dayOffset = 0;
    long newHour = Hour();
    s_Offset(&newHour, hours, 24, &dayOffset);
    m_Hour = (int) newHour;
    AddDay(dayOffset, eIgnoreDaylight);
    if (aflag) 
        x_AdjustTime(*pt, shift_time);
    return *this;
}


CTime& CTime::AddMinute(int minutes, EDaylight adl)
{
    if ( !minutes ) 
        return *this;
    CTime *pt = 0;
    bool aflag = false; 
    if ( (adl == eAdjustDaylight) && x_NeedAdjustTime() ) {
        pt = new CTime(*this);
        if ( !pt ) {
            THROW1_TRACE(runtime_error,
                         "CTime::AddMinute() error allocate memory");
        }
        aflag = true;
    }
    int hourOffset = 0;
    long newMinute = Minute();
    s_Offset(&newMinute, minutes, 60, &hourOffset);
    m_Minute = (int) newMinute;
    AddHour(hourOffset, eIgnoreDaylight);
    if (aflag) 
        x_AdjustTime(*pt);
    return *this;
}


CTime& CTime::AddSecond(int seconds)
{
    if ( !seconds )
        return *this;
    int minuteOffset = 0;
    long newSecond = Second();
    s_Offset(&newSecond, seconds, 60, &minuteOffset);
    m_Second = (int) newSecond;
    return AddMinute(minuteOffset);
}


CTime& CTime::AddNanoSecond(long ns)
{
    if ( !ns ) 
        return *this;
    int secondOffset = 0;
    long newNanoSecond = NanoSecond();
    s_Offset(&newNanoSecond, ns, kNanoSecondsPerSecond, &secondOffset);
    m_NanoSecond = newNanoSecond;
    return AddSecond(secondOffset);
}


bool CTime::IsValid(void) const
{
    if ( IsEmpty() ) 
        return true;

    s_DaysInMonth[1] = IsLeap() ? 29 : 28;
  
    if (Year() < 1755) // first Gregorian date
        return false;
    if (Month()  < 1  ||  Month()  > 12)
        return false;
    if (Day()    < 1  ||  Day()    > s_DaysInMonth[Month() - 1])
        return false;
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
    if ( GetTimeZoneFormat() != tz ) {
        struct tm* t;
        time_t timer;
        timer = GetTimeT();
        if (timer == -1) 
            return *this;
        t = ( tz == eLocal ) ? localtime(&timer) : gmtime(&timer);
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


bool CTime::operator == (const CTime& t) const
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


bool CTime::operator > (const CTime& t) const
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


bool CTime::operator < (const CTime& t) const
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


void CTime::x_AdjustDay()
{
    s_DaysInMonth[1] = IsLeap() ? 29 : 28;

    if ( Day() > s_DaysInMonth[Month() - 1] ) 
        m_Day = s_DaysInMonth[Month() - 1];

}


CTime& CTime::x_AdjustTime(const CTime& from, bool shift_time)
{
    if ( !x_NeedAdjustTime() )
        return *this; 

    switch ( GetTimeZonePrecision() ) 
        {
        case eMinute:
            if ( Minute() != from.Minute() )
                return x_AdjustTimeImmediately(from, shift_time);
        case eHour:
            if ( Hour() != from.Hour() )
                return x_AdjustTimeImmediately(from, shift_time);
        case eDay:
            if ( Day() != from.Day() )
                return x_AdjustTimeImmediately(from, shift_time);
        case eMonth:
            if ( Month() != from.Month() )
                return x_AdjustTimeImmediately(from, shift_time);
        default:
            break;
        }

    return *this;
}
  

CTime& CTime::x_AdjustTimeImmediately(const CTime& from, bool shift_time)
{
    // Time in hours for temporary time shift
    // Shift use for obtainment correct result at changeover daytime saving
    // Must be > 3 (Linux distinction). On other platforms may be == 3.
    const int kShift = 4;

    // Special conversion from <const CTime> to <CTime>
    CTime tmp(from); 
    int sign = 0;
    int diff = 0;
    // Primary procedure call
    if ( shift_time ) {
        sign = ( *this > from ) ? 1 : -1;
        // !!! Run TimeZoneDiff() first for old time value 
        diff = -tmp.TimeZoneDiff() + TimeZoneDiff();
        // Correction need't if time already in identical timezone
        if ( !diff  ||  diff == m_AdjustTimeDiff )
            return *this;
    } 
    // Recursive procedure call. Inside below 
    // x_AddHour(*, eAdjustDaylight, false)
    else  {
        // Correction need't if difference not found
        if ( diff == m_AdjustTimeDiff )
            return *this;
    }
    // Make correction with temporary time shift
    time_t t = GetTimeT();
    CTime tn(t + diff + 3600 * kShift * sign);
    if ( from.GetTimeZoneFormat() == eLocal )
        tn.ToLocalTime();
    tn.SetTimeZonePrecision(GetTimeZonePrecision());

    // Primary procedure call
    if ( shift_time ) {
        // Cancel temporary time shift
        tn.x_AddHour(-kShift * sign, eAdjustDaylight, false);
        tn.m_AdjustTimeDiff = diff;
    }
    *this = tn;
    return *this;
}


//============================================================================
//
//  Extern
//
//============================================================================

// Return difference between times <t1> and <t2> as number of days
int operator - (const CTime& t1, const CTime& t2)
{
    return (int) (s_Date2Number(t1) - s_Date2Number(t2));
}


CTime AddYear(const CTime& t, int years)
{
    return CTime(t).AddYear(years);
}

 
CTime AddMonth(const CTime& t, int months)
{
    return CTime(t).AddMonth(months);
}

 
CTime AddDay(const CTime& t, int days)
{
    return CTime(t).AddDay(days);
}

 
CTime AddHour(const CTime& t, int hours)
{
    return CTime(t).AddHour(hours);
}

 
CTime AddMinute(const CTime& t, int minutes)
{
    return CTime(t).AddMinute(minutes);
}

 
CTime AddSecond(const CTime& t, int seconds)
{
    return CTime(t).AddSecond(seconds);
}

 
CTime AddNanoSecond(const CTime& t, long nanoseconds)
{
    return CTime(t).AddNanoSecond(nanoseconds);
}


CTime operator + (const CTime& t, int days)
{
    CTime tmp = s_Number2Date(s_Date2Number(t) + days, t);
    tmp.x_AdjustTime(t);
    return tmp;
}

 
CTime operator + (int days, const CTime& t)
{
    CTime tmp = s_Number2Date(s_Date2Number(t) + days, t);
    tmp.x_AdjustTime(t);
    return tmp;
}

 
CTime operator - (const CTime& t, int days)
{
    CTime tmp = s_Number2Date(s_Date2Number(t) - days, t);
    tmp.x_AdjustTime(t);
    return tmp;
}

 
CTime CurrentTime(CTime::ETimeZone tz, CTime::ETimeZonePrecision tzp)
{
    return CTime(CTime::eCurrent,tz,tzp);
}

 
CTime Truncate(const CTime& t)
{
    return CTime(t).Truncate();
}


END_NCBI_SCOPE
