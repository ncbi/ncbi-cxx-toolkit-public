#ifndef NCBITIME__HPP
#define NCBITIME__HPP

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
* Authors:  Anton  Butanayev <butanaev@ncbi.nlm.nih.gov>
*           Denis  Vakatov
*           Ivanov Vladimir
*
* File Description:
*   CTime   - the standard Date/Time class;  it also can be used
*             as a time span (to represent elapsed time).
*
*	NOTE 1: Do not use Local time with time span and dates < "1/1/1900"
*			  (use GMT time only!!!)	
*	NOTE 2: Under UNIX link applications with realtime library (rt.lib).
*			  (used for support nanoseconds)	
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/04/27 20:42:29  ivanov
* Support for Local and UTC time added.
* Support for work with nanoseconds added.
*
* Revision 1.2  2000/11/21 18:15:29  butanaev
* Fixed bug in operator ++/-- (int)
*
* Revision 1.1  2000/11/20 22:17:42  vakatov
* Added NCBI date/time class CTime ("ncbitime.[ch]pp") and
* its test suite ("test/test_ncbitime.cpp")
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


// Number nanoseconds in one second
// Interval for it is from 0 to 999,999,999 
const long kNanoSecondsPerSecond = 1000000000;

// Number milliseconds in one second
// Interval for it is from 0 to 999
const long kMilliSecondsPerSecond = 1000;

// Week day names
const string kDaysOfWeekShort[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const string kDaysOfWeekLong [7] = {"Sunday","Monday","Tuesday","Wednesday",
                                    "Thursday","Friday","Saturday"};


//=============================================================================
//
//  CTime
//
//=============================================================================

// This is standard Date/Time class. 
// It also can be used as a time span (to represent elapsed time).
// CTime can operate with local and UTC time.
// Time is kept in class in the format in which it was originally given.

// NOTE: Do not use local time with time span and dates < "1/1/1900"
// 		(use GMT time only!!!)	

class CTime
{
public:
    // Initialize using current time (or "empty" time)
    enum EInitMode {
        eCurrent,   // use current time
        eEmpty      // use "empty" time
    };
    
    // Initialize using timezone information
    enum ETimeZone {
        eLocal,     // time is in local format
        eGmt        // time is in GMT (Greenwich Mean Time)
    };

    // Controls when (if ever) to adjust for the daylight saving time
    // (only if the time is represented in local timezone format).
    // NOTE: if diff between previous time value and the time
    // after manipulation is greater than this range, then try apply
    // daylight saving conversion on the result time value.
    enum ETimeZonePrecision {
        eNone,    // daylight saving not to affect time manipulations
        eMinute,  // check condition - new minute
        eHour,    // check condition - new hour
        eDay,     // check condition - new day
        eMonth,   // check condition - new month
        eDefault = eMinute // default time quantum for adjustment
    };

    // Initialize using adjust daylight saving time
    enum EDaylight {
        eIgnoreDaylight,
        eAdjustDaylight
    };

    // Construct empty or object with current date-time
    CTime(EInitMode          mode = eEmpty,
          ETimeZone          tz   = eLocal,
          ETimeZonePrecision tzp  = eNone);

    // Construct object from UTC time_t value (assuming it is eGMT)
    CTime(time_t t, ETimeZonePrecision tzp = eNone);
	
    // NOTE: <day> and <month> here are starting from 1 (one)
    CTime(int year, int month, int day,
          int hour = 0, int minute = 0, int second = 0, long nanosecond = 0,
          ETimeZone tz = eLocal, ETimeZonePrecision tzp = eNone);

    // Construct date as N-th day of the year
    CTime(int year, int yearDayNumber,
          ETimeZone tz = eLocal, ETimeZonePrecision tzp = eNone);

    // Read from the string <str> using format <fmt>.
    // If <fmt> is empty, then use the current format - as set by SetFormat().
    // Default format is "M/D/Y h:m:s".
    CTime(const string& str, const string& fmt = kEmptyStr,
          ETimeZone tz = eLocal, ETimeZonePrecision tzp = eNone);

    // Copy constructor
    CTime(const CTime& t);

    // Assignment
    CTime& operator = (const CTime& t);

    // NOTE: Not change object's TimeZone if current format does not contain
    //       symbol 'Z'. If it contain 'Z' and "str" have word "GMT" in the 
    //       appropriate position, then TimeZone set to eGmt, else - to eLocal.
    //       TimeZonePrecision not change.
    CTime& operator = (const string& str);

    // Set time (<t> is always in GMT)
    CTime& Set(time_t& t);

    // Make the time current (in the presently active time zone)
    CTime& SetCurrent(void);
    
    // Make the time "empty"
    CTime& Clear(void);
    
    // Truncate the time to days (strip H,M,S and Nanoseconds)
    CTime& Truncate(void);

    // Change current format (the default one is: "M/D/Y h:m:s").
    //   "Y" - 4 digit year  "y" - 2 digit year
    //   "M" - month         "D" - day
    //   "h" - hour          "m" - minute
    //   "s" - second        "S" - nanosecond
    //   "Z" - timezone format (GMT or none) 
    //   "w" - day of week (short format - 3 first letters)
    //   "W" - day of week (long format  - full name)
    static void SetFormat(const string& fmt);

    // Transform time to string (use format <sm_Format> by default)
    string AsString(const string& fmt = kEmptyStr) const;

    // Return time as string (use format <sm_Format>)
    operator string(void) const;

    // Present CTime as time_t value (NOTE: nanoseconds will be truncated)
    time_t AsTimeT(void);

    // Get various constituents of the time
    int  YearDayNumber (void) const;
    int  Year          (void) const;
    int  Month         (void) const;
    int  Day           (void) const;
    int  Hour          (void) const;
    int  Minute        (void) const;
    int  Second        (void) const;
    long NanoSecond    (void) const;
    int  DayOfWeek     (void) const;

    // Add (subtract if negative) to the time.
    // Parameter <adl> specifies if to do adjustment for the "daylight" 
    // time - it is for (eLocal && !eNone) only!
    CTime& AddYear      (int years   = 1, EDaylight adl = eAdjustDaylight);
    CTime& AddMonth     (int months  = 1, EDaylight adl = eAdjustDaylight);
    CTime& AddDay       (int days    = 1, EDaylight adl = eAdjustDaylight);
    CTime& AddHour      (int hours   = 1, EDaylight adl = eAdjustDaylight);
    CTime& AddMinute    (int minutes = 1, EDaylight adl = eAdjustDaylight);
    CTime& AddSecond    (int seconds = 1);
    CTime& AddNanoSecond(long nanoseconds = 1);

    // Return object, which difference from original at <days> days
    // Like at AddDay() with adl==TRUE, but if it == FALSE then 
    // adjustment time between winter & summer times not effect.
    // NOTE: It add because in "extern operator +" can't transmit 
    // parameter <adl> from AddDay() function.
    CTime AfterAddDay(int days, EDaylight adl = eAdjustDaylight) const;

    // Add/subtract days
    CTime& operator += (int days);
    CTime& operator -= (int days);
    CTime& operator ++ (void);
    CTime& operator -- (void);
    CTime  operator ++ (int);
    CTime  operator -- (int);

    // Time comparison ('>' means "later", '<' means "earlier")
    bool operator == (const CTime& t) const;
    bool operator != (const CTime& t) const;
    bool operator >  (const CTime& t) const;
    bool operator <  (const CTime& t) const;
    bool operator >= (const CTime& t) const;
    bool operator <= (const CTime& t) const;

    // Time difference
    double DiffDay        (const CTime& t) const;
    double DiffHour       (const CTime& t) const;
    double DiffMinute     (const CTime& t) const;
    int    DiffSecond     (const CTime& t) const;
    double DiffNanoSecond (const CTime& t) const;

    // Checks
    bool IsEmpty     (void) const;
    bool IsLeap      (void) const;
    bool IsValid     (void) const;
    bool IsLocalTime (void) const;
    bool IsGmtTime   (void) const;

    // Timezone functions
    ETimeZone          GetTimeZoneFormat(void) const;
    ETimeZone          SetTimeZoneFormat(ETimeZone val);
    ETimeZonePrecision GetTimeZonePrecision(void) const;
    ETimeZonePrecision SetTimeZonePrecision(ETimeZonePrecision val);

    // Get difference between local timezone end GMT in seconds
    int TimeZoneDiff   (void);
	
    // Return current time as Local or GMT time
    CTime  GetLocalTime(void);
    CTime  GetGmtTime  (void);

    // Convert current time into Local or GMT time
    CTime& ToTime      (ETimeZone val);
    CTime& ToLocalTime (void);
    CTime& ToGmtTime   (void);

private:
  
    // Copy members valies from <t> to current object
    void x_SetMembersFrom(const CTime& t);

    // Check string time format 
    static void x_VerifyFormat(const string& fmt);

    // Set time value from string
    void x_Init(const string& str, const string& fmt);

    // Set time from value. If value not specified, then set current time
    CTime& x_SetTime(time_t* value = 0);

    // Adjust day number to correct value after day manipulations
    void x_AdjustDay(void);

    // Adjust the time to correct timezone (across the barrier of
    // winter & summer times) using <from> as a reference point.
    // It does the adjustment only if the time object:
    //  1) contains local time (not GMT), and
    //  2) has TimeZonePrecision != CTime::eNone, and
    //  3) differs from <from> in the TimeZonePrecision (or larger) part.
    CTime& x_AdjustTime(const CTime& from, bool shift_time = true);

    // Forcibly adjust timezone using <from> as a reference point.
    CTime& x_AdjustTimeImmediately(const CTime& from, bool shift_time = true);

    // Return TRUE if need adjust time in timezone
    bool x_NeedAdjustTime(void);

    // Add hour with/without shift time
    // Parameter <shift_time> access or denied use time shift in 
    // process adjust hours.
    CTime& x_AddHour(int hours = 1, EDaylight daylight = eAdjustDaylight, 
                     bool shift_time = true);

    // Time
    int           m_Year;
    unsigned char m_Month;
    unsigned char m_Day;
    unsigned char m_Hour;
    unsigned char m_Minute;
    unsigned char m_Second;
    unsigned long m_NanoSecond;
	
    // Timezone and precision
    ETimeZone          m_Tz;
    ETimeZonePrecision m_TzPrecision;

    // Difference between GMT and local time in seconds,
    // as stored during the last call to x_AdjustTime***().
    int m_AdjustTimeDiff;	

    // Global default string date format	
    static string sm_Format;
};



//=============================================================================
//
//  Extern
//
//=============================================================================

// Add (subtract if negative) to the time (see CTime::AddXXX)
extern CTime AddYear       (const CTime& t, int  years       = 1);
extern CTime AddMonth	   (const CTime& t, int  months      = 1);
extern CTime AddDay		   (const CTime& t, int  days        = 1);
extern CTime AddHour       (const CTime& t, int  hours       = 1);
extern CTime AddMinute	   (const CTime& t, int  minutes     = 1);
extern CTime AddSecond	   (const CTime& t, int  seconds     = 1);
extern CTime AddNanoSecond (const CTime& t, long nanoseconds = 1);

// Add/subtract days (see CTime::operator)
extern CTime operator + (const CTime& t,    int          days);
extern CTime operator + (int          days, const CTime& t   );
extern CTime operator - (const CTime& t,    int          days);

// Difference in days (see CTime::operator)
extern int   operator - (const CTime& t1, const CTime& t2);

// Get current time (in local or GMT format)
extern CTime CurrentTime(CTime::ETimeZone          tz  = CTime::eLocal, 
                         CTime::ETimeZonePrecision tzp = CTime::eNone);

// Truncate the time to days (see CTime::Truncate)
extern CTime Truncate(const CTime& t);




//=============================================================================
//
//  Inline
//
//=============================================================================


inline 
int CTime::Year(void) const { return m_Year; }

inline 
int CTime::Month(void) const { return m_Month; }

inline 
int CTime::Day(void) const { return m_Day; }

inline 
int CTime::Hour(void) const { return m_Hour; }

inline 
int CTime::Minute(void) const { return m_Minute; }

inline 
int CTime::Second(void) const { return m_Second; }

inline 
long CTime::NanoSecond(void) const { return m_NanoSecond; }

inline
CTime& CTime::Set(time_t& t) { return x_SetTime(&t); };

inline
CTime& CTime::SetCurrent(void) { return x_SetTime(); }

inline 
CTime& CTime::operator += (int days) { return AddDay(days); }

inline 
CTime& CTime::operator -= (int days) { return AddDay(-days); }

inline 
CTime& CTime::operator ++ (void) { return AddDay( 1); }

inline 
CTime& CTime::operator -- (void) { return AddDay(-1); }

inline 
CTime::operator string(void) const { return AsString(); }

inline 
CTime CTime::operator ++ (int)
{
    CTime t = *this;
    AddDay(1);
    return t;
}

inline 
CTime CTime::operator -- (int)
{
    CTime t = *this;
    AddDay(-1);
    return t;
}

inline 
CTime& CTime::operator = (const string& str)
{
    x_Init(str, sm_Format);
    return *this;
}

inline 
CTime& CTime::operator = (const CTime& t)
{
    x_SetMembersFrom(t);
    return *this;
}

inline 
bool CTime::operator != (const CTime& t) const
{
    return !(*this==t);
}

inline 
bool CTime::operator >= (const CTime& t) const
{
    return !(*this < t);
}

inline
bool CTime::operator <= (const CTime& t) const
{
    return !(*this > t);
}

inline
CTime& CTime::AddHour(int hours, EDaylight use_daylight)
{
    return x_AddHour(hours,use_daylight,true);
}

inline 
void CTime::SetFormat(const string& fmt)
{
    x_VerifyFormat(fmt);
    CTime::sm_Format = fmt;
}

inline 
bool CTime::IsEmpty() const
{
    return
        !Day()   &&  !Month()   &&  !Year()  &&
        !Hour()  &&  !Minute()  &&  !Second()  &&  !NanoSecond();
}

inline 
double CTime::DiffDay(const CTime& t) const
{
    return DiffSecond(t) / 60.0 / 60.0 / 24.0;
}

inline 
double CTime::DiffHour(const CTime& t) const
{
    return DiffSecond(t) / 60.0 / 60.0;
}

inline 
double CTime::DiffMinute(const CTime& t) const
{
    return DiffSecond(t) / 60.0;
}

inline 
double CTime::DiffNanoSecond(const CTime& t) const
{
    long dNanoSec = NanoSecond() - t.NanoSecond();
    return (double) DiffSecond(t) * kNanoSecondsPerSecond + dNanoSec;
}

inline 
bool CTime::IsLocalTime(void) const	{ return m_Tz == eLocal; } 

inline 
bool CTime::IsGmtTime(void) const	{ return m_Tz == eGmt; }	 

inline 
CTime::ETimeZone CTime::GetTimeZoneFormat(void) const
{	
    return m_Tz;
}

inline 
CTime::ETimeZonePrecision CTime::GetTimeZonePrecision(void) const
{
    return m_TzPrecision;
}

inline 
CTime::ETimeZone CTime::SetTimeZoneFormat(ETimeZone val)
{
    ETimeZone tmp = m_Tz;
    m_Tz = val;
    return tmp;
}

inline 
CTime::ETimeZonePrecision CTime::SetTimeZonePrecision(ETimeZonePrecision val)
{
    ETimeZonePrecision tmp = m_TzPrecision;
    m_TzPrecision = val;
    return tmp;
}

inline 
int CTime::TimeZoneDiff(void)
{
    return GetLocalTime().DiffSecond(GetGmtTime());
}

inline 
CTime CTime::GetLocalTime(void) 
{	
    if ( IsLocalTime() ) return *this;
    CTime t(*this);
    return t.ToLocalTime();
}

inline 
CTime CTime::GetGmtTime(void)
{
    if ( IsGmtTime() ) return *this;
    CTime t(*this);
    return t.ToGmtTime();
}

inline
CTime& CTime::ToLocalTime(void) { ToTime(eLocal); return *this; }

inline
CTime& CTime::ToGmtTime(void) { ToTime(eGmt); return *this; }

inline 
bool CTime::x_NeedAdjustTime(void)
{
    return GetTimeZoneFormat() == eLocal  &&  GetTimeZonePrecision() != eNone;
}


END_NCBI_SCOPE

#endif /* NCBITIME__HPP */
