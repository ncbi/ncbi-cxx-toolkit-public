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
* Authors:  Anton Butanayev <butanaev@ncbi.nlm.nih.gov>
*           Denis Vakatov
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <time.h>
#include <stdlib.h>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CTime::
//


string CTime::sm_Format = "M/D/Y h:m:s";

static int s_DaysInMonth[] = {31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


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
        ((146097 * c) >> 2) +
        ((1461 * ya) >> 2) +
        (153 * m + 2) / 5 +
        d +
        1721119;
}


static CTime s_Number2Date(unsigned num, const CTime& tm)
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
    return CTime(year, month, day, tm.Hour(), tm.Minute(), tm.Second());
}


static void s_Offset(int *value, int offset, int bound, int *major)
{
    *value += offset;
    *major += *value / bound;
    *value %= bound;
    if (*value < 0) {
        *major -= 1;
        *value += bound;
    }
}


CTime::CTime(const CTime& tm)
{
    m_Year   = tm.m_Year;
    m_Month  = tm.m_Month;
    m_Day    = tm.m_Day;
    m_Hour   = tm.m_Hour;
    m_Minute = tm.m_Minute;
    m_Second = tm.m_Second;
}


CTime& CTime::operator = (const string& str)
{
    CTime tm(str);
    return *this = tm;
}


CTime& CTime::operator = (const CTime& tm)
{
    m_Year   = tm.m_Year;
    m_Month  = tm.m_Month;
    m_Day    = tm.m_Day;
    m_Hour   = tm.m_Hour;
    m_Minute = tm.m_Minute;
    m_Second = tm.m_Second;
    return *this;
}


CTime::CTime(int year, int yearDayNumber)
{
    Clear();

    CTime tm = CTime(year, 1, 1);

    tm += yearDayNumber - 1;
    x_SetYear(tm.Year());
    m_Month = tm.Month();
    m_Day   = tm.Day();
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
        if (strchr("YyMDhms", *j) != 0  &&  ++count[(unsigned int) *j] > 1) {
            THROW1_TRACE(runtime_error,
                         "CTime::  duplicated symbols in format:  `" + fmt
                         + "'");
        }
    }
}


void CTime::x_Init(const string& str, const string& fmt)
{
    Clear();

    const char* fff;
    const char* sss = str.c_str();
    for (fff = fmt.c_str();  *fff != '\0';  fff++) {

        // Process non-format symbols
        if (strchr("YyMDhms", *fff) == 0) {
            if (*fff == *sss) {
                sss++;
                continue;  // skip matching non-format symbols
            }
            break;  // error:  non-matching non-format symbols
        }

        // Process format symbols ("YyMDhms") -- read the next data ingredient
        char value_str[5];
        char* s = value_str;
        for (size_t len = (*fff == 'Y') ? 4 : 2;
             len != 0  &&  *sss != '\0'  &&  isdigit(*sss);  len--) {
            *s++ = *sss++; 
        }
        *s = '\0';

        int value = NStr::StringToInt(value_str);

        switch ( *fff ) {
        case 'Y':
            x_SetYear(value);
            break;
        case 'y':
            if (value >= 0  &&  value < 50) {
                value += 2000;
            } else if (value >= 50  &&  value < 100) {
                value += 1900;
            }
            x_SetYear(value);
            break;
        case 'M':
            m_Month = value;
            break;
        case 'D':
            m_Day = value;
            break;
        case 'h':
            x_SetHour(value);
            break;
        case 'm':
            x_SetMinute(value);
            break;
        case 's':
            x_SetSecond(value);
            break;
        default:
            _TROUBLE;
        }
    }

    if (*fff != '\0'  ||  *sss != '\0') {
        THROW1_TRACE(runtime_error,
                     "CTime::  format mismatch:  `" +
                     fmt + "' <-- `" + str + "'");
    }
    if ( !IsValid() ) {
        THROW1_TRACE(runtime_error, "CTime:: not valid:  `" + str + "'");
    }
}


CTime::CTime(int year, int month, int day, int hour, int minute, int second)
{
    x_SetYear(year);
    m_Month = month;
    m_Day   = day;
    x_SetHour  (hour);
    x_SetMinute(minute);
    x_SetSecond(second);

    if ( !IsValid() ) {
        THROW1_TRACE(runtime_error, "CTime::CTime() passed invalid time");
    }
}


CTime::CTime(EMode mode)
{
    if (mode == eCurrent) {
        SetCurrent();
    } else {
        Clear();
    }
}


CTime::CTime(const string& str, const string& fmt)
{
    if ( fmt.empty() ) {
        x_Init(str, sm_Format);
    } else {
        x_VerifyFormat(fmt);
        x_Init(str, fmt);
    }
}


int CTime::YearDayNumber() const
{
    unsigned first = s_Date2Number(CTime(Year(), 1, 1));
    unsigned self  = s_Date2Number(*this);
    _ASSERT(first <= self  &&  self < first + (IsLeap() ? 366 : 365));
    return (int)(self - first + 1);
}


static void s_AddZeroPadInt(string& str, int value, SIZE_TYPE len = 2)
{
    string s_value = NStr::IntToString(value);
    if (s_value.length() < len) {
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
    for (string::const_iterator it = fmt.begin();  it != fmt.end();  ++it) {
        switch ( *it ) {
        case 'Y':  s_AddZeroPadInt(str, Year(), 4);     break;
        case 'y':  s_AddZeroPadInt(str, Year() % 100);  break;
        case 'M':  s_AddZeroPadInt(str, Month());       break;
        case 'D':  s_AddZeroPadInt(str, Day());         break;
        case 'h':  s_AddZeroPadInt(str, Hour());        break;
        case 'm':  s_AddZeroPadInt(str, Minute());      break;
        case 's':  s_AddZeroPadInt(str, Second());      break;
        default :
            str += *it;  break;
        }
    }
    return str;
}


CTime& CTime::SetCurrent()
{
    time_t timer = time(0);
    struct tm* t = localtime(&timer);

    m_Day   = t->tm_mday;
    m_Month = t->tm_mon + 1;
    x_SetYear  (t->tm_year + 1900);
    x_SetHour  (t->tm_hour);
    x_SetMinute(t->tm_min);
    x_SetSecond(t->tm_sec);
    return *this;
}


CTime& CTime::AddDay(int days)
{
    return *this = *this + days;
}


CTime& CTime::AddYear(int years)
{
    x_SetYear(Year() + years);
    x_AdjustDay();
    return *this;
}


CTime& CTime::AddHour(int hours)
{
    int dayOffset = 0;
    int newHour   = Hour();
    s_Offset(&newHour, hours, 24, &dayOffset);
    x_SetHour(newHour);
    return *this += dayOffset;
}


CTime& CTime::AddMinute(int minutes)
{
    int hourOffset = 0;
    int newMinute  = Minute();
    s_Offset(&newMinute, minutes, 60, &hourOffset);
    x_SetMinute(newMinute);
    return AddHour(hourOffset);
}


CTime& CTime::AddSecond(int seconds)
{
    int minuteOffset = 0;
    int newSecond    = Second();
    s_Offset(&newSecond, seconds, 60, &minuteOffset);
    x_SetSecond(newSecond);
    return AddMinute(minuteOffset);
}


CTime& CTime::AddMonth(int months)
{
    int newMonth = Month() - 1;
    int newYear  = Year();
    s_Offset(&newMonth, months, 12, &newYear);
    m_Month = newMonth + 1;
    x_SetYear(newYear);
    x_AdjustDay();
    return *this;
}


bool CTime::IsValid() const
{
    if ( IsEmpty() )
        return true;

    s_DaysInMonth[1] = IsLeap() ? 29 : 28;
  
    if (Year() < 1755) // first georgian date
        return false;
    if (Month()  < 1  ||  Month()  > 12)
        return false;
    if (Hour()   < 0  ||  Hour()   > 23)
        return false;
    if (Minute() < 0  ||  Minute() > 59)
        return false;
    if (Second() < 0  ||  Second() > 59)
        return false;
    if (Day()    < 1  ||  Day()    > s_DaysInMonth[Month() - 1])
        return false;

    return true;
}


bool CTime::operator == (const CTime& tm) const
{
    return
        Year()   == tm.Year()    &&
        Month()  == tm.Month()   &&
        Day()    == tm.Day()     &&
        Hour()   == tm.Hour()    &&
        Minute() == tm.Minute()  &&
        Second() == tm.Second();
}


bool CTime::operator != (const CTime& tm) const
{
    return ! (*this == tm);
}


bool CTime::operator > (const CTime& tm) const
{
    if (Year() > tm.Year())
        return true;
    if (Year() < tm.Year())
        return false;

    if (Month() > tm.Month())
        return true;
    if (Month() < tm.Month())
        return false;

    if (Day() > tm.Day())
        return true;
    if (Day() < tm.Day())
        return false;

    if (Hour() > tm.Hour())
        return true;
    if (Hour() < tm.Hour())
        return false;

    if (Minute() > tm.Minute())
        return true;
    if (Minute() < tm.Minute())
        return false;

    if (Second() > tm.Second())
        return true;

    return false;
}


bool CTime::operator < (const CTime& tm) const
{
    if (Year() < tm.Year())
        return true;
    if (Year() > tm.Year())
        return false;

    if (Month() < tm.Month())
        return true;
    if (Month() > tm.Month())
        return false;

    if (Day() < tm.Day())
        return true;
    if (Day() > tm.Day())
        return false;

    if (Hour() < tm.Hour())
        return true;
    if (Hour() > tm.Hour())
        return false;

    if (Minute() < tm.Minute())
        return true;
    if (Minute() > tm.Minute())
        return false;

    if (Second() < tm.Second())
        return true;

    return false;
}


bool CTime::operator >= (const CTime& tm) const
{
    return ! (*this < tm);
}


bool CTime::operator <= (const CTime& tm) const
{
    return ! (*this > tm);
}


bool CTime::IsLeap(void) const
{
    int year = Year();
    return year % 4 == 0  &&  year % 100 != 0  ||  year % 400 == 0;
}


CTime& CTime::Clear()
{
    m_Day    = 0;
    m_Month  = 0;
    m_Year   = 0;
    m_Hour   = 0;
    m_Minute = 0;
    m_Second = 0;

    return *this;
}



void CTime::SetFormat(const string& fmt)
{
    x_VerifyFormat(fmt);
    CTime::sm_Format = fmt;
}


bool CTime::IsEmpty() const
{
    return
        !m_Day   &&  !m_Month   &&  !m_Year  &&
        !m_Hour  &&  !m_Minute  &&  !m_Second;
}


void CTime::x_SetYear(int year)
{
    m_Year = year;
}


void CTime::x_SetHour(int hour)
{
    m_Hour = hour;
}


void CTime::x_SetMinute(int minute)
{
    m_Minute = minute;
}


void CTime::x_SetSecond(int second)
{
    m_Second = second;
}


CTime& CTime::Truncate(void)
{
    x_SetHour(0);
    x_SetMinute(0);
    x_SetSecond(0);
    return *this;
}


double CTime::DiffDay(const CTime& tm) const
{
    return DiffSecond(tm) / 60.0 / 60.0 / 24.0;
}


double CTime::DiffHour(const CTime& tm) const
{
    return DiffSecond(tm) / 60.0 / 60.0;
}


double CTime::DiffMinute(const CTime& tm) const
{
    return DiffSecond(tm) / 60.0;
}


int CTime::DiffSecond(const CTime& tm) const
{
    int dSec  = Second() - tm.Second();
    int dMin  = Minute() - tm.Minute();
    int dHour = Hour()   - tm.Hour();
    int dDay  = (*this)  - tm;
    return dSec + 60 * dMin + 60 * 60 * dHour + 60 * 60 * 24 * dDay;
}


void CTime::x_AdjustDay()
{
    s_DaysInMonth[1] = IsLeap() ? 29 : 28;

    if (m_Day > s_DaysInMonth[m_Month - 1]) {
        m_Day = s_DaysInMonth[m_Month - 1];
    }
}

CTime CTime::operator ++ (int)
{
  CTime tmp = *this;
  AddDay(1);
  return tmp;
}

CTime CTime::operator -- (int)
{
  CTime tmp = *this;
  AddDay(-1);
  return tmp;
}


/////////////////////////////////////////////////////////////////////////////
//  EXTERN
//


CTime AddYear(const CTime& tm, int years)
{
    return CTime(tm).AddYear(years);
}


CTime AddMonth(const CTime& tm, int months)
{
    return CTime(tm).AddMonth(months);
}


CTime AddDay(const CTime& tm, int days)
{
    return CTime(tm).AddDay(days);
}


CTime AddHour(const CTime& tm, int hours)
{
    return CTime(tm).AddHour(hours);
}


CTime AddMinute(const CTime& tm, int minutes)
{
    return CTime(tm).AddMinute(minutes);
}


CTime AddSecond(const CTime& tm, int seconds)
{
    return CTime(tm).AddSecond(seconds);
}


CTime operator + (const CTime& tm, int days)
{
    return s_Number2Date(s_Date2Number(tm) + days, tm);
}


CTime operator + (int days, const CTime& tm)
{
    return tm + days;
}


CTime operator - (const CTime& tm, int days)
{
    return tm + (-days);
}


extern int operator - (const CTime& tm1, const CTime& tm2)
{
    return (int) s_Date2Number(tm1) - s_Date2Number(tm2);
}


extern CTime CurrentTime(void)
{
    return CTime(CTime::eCurrent);
}


extern CTime Truncate(const CTime& tm)
{
    return CTime(tm).Truncate();
}


END_NCBI_SCOPE
