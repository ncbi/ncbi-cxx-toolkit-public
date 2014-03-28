#ifndef CORELIB__NCBITIME__HPP
#define CORELIB__NCBITIME__HPP

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
 * DayOfWeek():  Used code has been posted on comp.lang.c on March 10th, 1993
 *               by Tomohiko Sakamoto (sakamoto@sm.sony.co.jp).
 *
 */

/// @file ncbitime.hpp
/// Defines:
///   CTimeFormat    - storage class for fime format.
///   CTime          - standard Date/Time class to represent an absolute time.
///   CTimeSpan      - class to represents a relative time span.
///   CStopWatch     - stop watch class to measure elasped time.
///   CTimeout       - timeout interval for various I/O etc activity.
///   CNanoTimeout   - timeout interval with nanoseconds precision.
///   CDeadline      - timeout that use absolute time mark (deadline time).
///   CFastLocalTime - class for quick and dirty getting a local time.
///
/// NOTE about CTime:
///
///   - The time zone database in most Windows-based computer systems usually
///     stores only a single start and end rule for each zone, regardless
///     of year. One of the problems of this approach is that using time-zone
///     information will get incorrect results if referring to a year with
///     rules that are different from those currently in the database.
///
///   - Do not use Local time and time_t and its dependent functions with
///     dates outside the range January 1, 1970 UTC to January 18, 2038 UTC.
///     Also avoid to use GMT -> Local time conversion functions.
///
///   - Do not use DataBase conversion functions for dates
///     prior to January 1, 1900.

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup Time
 *
 * @{
 */

// Forward declarations
class CTimeSpan;
class CFastLocalTime;


/// Number of nanoseconds in one second.
///
/// Interval for it is from 0 to 999,999,999.
const long kNanoSecondsPerSecond = 1000000000;

/// Number of microseconds in one second.
///
/// Interval for it is from 0 to 999,999.
const long kMicroSecondsPerSecond = 1000000;

/// Number milliseconds in one second.
///
/// Interval for it is from 0 to 999.
const long kMilliSecondsPerSecond = 1000;


// Time formats in databases (always contain local time only!)

/// Number of seconds.
typedef Int8 TSeconds;

/// Database format for time where day and time is unsigned 16 bit.
typedef struct {
    Uint2 days;   ///< Days from 1/1/1900
    Uint2 time;   ///< Minutes from the beginning of current day
} TDBTimeU, *TDBTimeUPtr;

/// Database format for time where day and time is signed 32 bit.
typedef struct {
    Int4  days;   ///< days from 1/1/1900
    Int4  time;   ///< x/300 seconds from the beginning of current day
} TDBTimeI, *TDBTimeIPtr;



/////////////////////////////////////////////////////////////////////////////
///
/// CTimeFormat --
///
/// Defines a storage class for time format.
///
/// See CTime::SetFormat and CTimeSpan::SetFormat for description
/// of format symbols for specific class.

class NCBI_XNCBI_EXPORT CTimeFormat
{
public:
    /// Flags.
    ///
    /// @sa SetFormat, AsString
    enum EFlags {
        /// Use single characters as format symbols.
        fFormat_Simple     = (1 << 0),
        /// Specify that each format symbol have a preceding symbol '$'.
        /// This can be useful if you want to include to format string
        /// a characters that are a format symbols. 
        /// To include symbol '$' use '$$'.
        fFormat_Ncbi       = (1 << 1),
        /// A time string should strictly match the format string.
        fMatch_Strict      = (1 << 5),       ///< eg "Y" and "1997"
        /// A format string can have extra leading format symbols,
        /// that do not have matching symbols in the time string.
        /// Corresponding time components will be initialized by default
        /// in the time object.
        fMatch_ShortTime   = (1 << 6),       ///< eg "Y/M/D h:m:s" and "1997"
        fMatch_ShortFormat = (1 << 7),       ///< eg "Y" and "1997/07/16"
        /// Time API tries to get what it can from time string using specified
        /// time format. It not guaranties that format or time strings will be
        /// processed fully, it allow that time string can have a garbage at the end. 
        fMatch_Weak        = fMatch_ShortFormat | fMatch_ShortTime,
        /// Default flags
        fDefault           = fFormat_Simple | fMatch_Strict,

        /// "Enums", used for backward compatibility. Please use flags instead.
        eNcbiSimple        = fFormat_Simple,
        eNcbi              = fFormat_Ncbi,
        eDefault           = fDefault
    };
    typedef unsigned int TFlags;  ///< Binary OR of "EFlags"

    /// Predefined formats.
    ///
    /// @sa GetPredefined, CTime::SetFormat
    enum EPredefined {
        // ISO 8601 formats (without time zone)
        eISO8601_Year         = 0,  ///< Y            (eg 1997)
        eISO8601_YearMonth    = 1,  ///< Y-M          (eg 1997-07)
        eISO8601_Date         = 2,  ///< Y-M-D        (eg 1997-07-16)
        eISO8601_DateTimeMin  = 3,  ///< Y-M-DTh:m    (eg 1997-07-16T19:20)
        eISO8601_DateTimeSec  = 4,  ///< Y-M-DTh:m:s  (eg 1997-07-16T19:20:30)
        eISO8601_DateTimeFrac = 5   ///< Y-M-DTh:m:s.l(eg 1997-07-16T19:20:30.123)
    };

    /// Default constructor.
    CTimeFormat(void);

    /// Copy constructor.
    CTimeFormat(const CTimeFormat& format);

    /// Constructor.
    ///
    /// @sa SetFormat
    CTimeFormat(const char* fmt, TFlags flags = fDefault);

    /// Constructor.
    ///
    /// @sa SetFormat
    CTimeFormat(const string& fmt, TFlags flags = fDefault);

    /// Assignment operator.
    CTimeFormat& operator= (const CTimeFormat& format);

    /// Set the current time format.
    ///
    /// @param fmt
    ///   String of letters describing the time format.
    /// @param flags
    ///   Flags specifying how to match a time string against format string.
    /// @sa
    ///   GetFormat, EFormat
    void SetFormat(const char* fmt, TFlags flags = fDefault);

    /// Set the current time format.
    ///
    /// @param fmt
    ///   String of letters describing the time format.
    /// @param flags
    ///   Flags specifying how to match a time string against format string.
    /// @sa
    ///   GetFormat, EFormat
    void SetFormat(const string& fmt, TFlags flags = fDefault);

    /// Get format string.
    ///
    /// @return
    ///   A string of letters describing the time format.
    /// @sa SetFormat, GetFlags
    const string& GetString(void) const;

    /// Get format flags.
    ///
    /// @return
    ///   A flags specifying how to match a time string against format string.
    /// @sa SetFormat, GetString
    TFlags GetFlags(void) const;

    /// Check that format string is empty.
    bool IsEmpty(void) const;

    /// Get predefined format.
    /// @param fmt
    ///   String of letters describing the time format.
    /// @param fmt_type
    ///   Specify type of the format string.
    /// @return
    ///   A time format object.
    /// @sa EPredefined, SetFormat
    static CTimeFormat GetPredefined(EPredefined fmt, TFlags flags = fDefault);

public:
    /// Return time format as string.
    /// Note: This method added temporarily, and will be deleted soon.
    /// @deprecated Use CTimeFormat::GetString()/GetFormat() methods instead.
    NCBI_DEPRECATED operator string(void) const;

private:
    string  m_Str;        ///< String format.
    TFlags  m_Flags;      ///< Format flags.
};


/////////////////////////////////////////////////////////////////////////////
///
/// CTime --
///
/// Defines a standard Date/Time class.
///
/// Can be used to span time (to represent elapsed time). Can operate with
/// local and GMT (aka UTC) time. The time is kept in class in the format
/// in which it was originally given.
///
/// Throw exception of type CTimeException on errors.
///
/// NOTE: Do not use local time with time span and dates < "1/1/1900"
/// (use GMT time only!!!).

class NCBI_XNCBI_EXPORT CTime
{
public:
    /// Which initial value to use for time.
    enum EInitMode {
        eCurrent,     ///< Use current time
        eEmpty        ///< Use "empty" time
    };

    /// Which initial value to use for timezone.
    enum ETimeZone {
        eLocal,       ///< Use local time
        eGmt          ///< Use GMT (Greenwich Mean Time)
    };

    /// Current timezone. Used in AsString() method.
    enum {
        eCurrentTimeZone = -1
    };

    /// Which format use to get name of month or week of day.
    enum ENameFormat {
        eFull,        ///< Use full name.
        eAbbr         ///< Use abbreviated name.
    };

    /// Month names.
    enum EMonth {
        eJanuary = 1,
        eFebruary,
        eMarch,
        eApril,
        eMay,
        eJune,
        eJuly,
        eAugust,
        eSeptember,
        eOctober,
        eNovember,
        eDecember
    };

    /// Day of week names.
    enum EDayOfWeek {
        eSunday = 0,
        eMonday,
        eTuesday,
        eWednesday,
        eThursday,
        eFriday,
        eSaturday
    };

    /// What time zone precision to use for adjusting daylight saving time.
    ///
    /// Controls when (if ever) to adjust for the daylight saving time
    /// (only if the time is represented in local timezone format).
    ///
    /// NOTE: if diff between previous time value and the time
    /// after manipulation is greater than this range, then try apply
    /// daylight saving conversion on the result time value.
    enum ETimeZonePrecision {
        eNone,    ///< Daylight saving not to affect time manipulations.
        eMinute,  ///< Check condition - new minute.
        eHour,    ///< Check condition - new hour.
        eDay,     ///< Check condition - new day.
        eMonth,   ///< Check condition - new month.
        eTZPrecisionDefault = eNone
    };

    /// Whether to adjust for daylight saving time.
    enum EDaylight {
        eIgnoreDaylight,   ///< Ignore daylight saving time.
        eAdjustDaylight,   ///< Adjust for daylight saving time.
        eDaylightDefault = eAdjustDaylight
    };

    /// Constructor.
    ///
    /// @param mode
    ///   Whether to build time object with current time or empty
    ///   time (default).
    /// @param tz
    ///   Whether to use local time (default) or GMT.
    /// @param tzp
    ///   What time zone precision to use.
    CTime(EInitMode          mode = eEmpty,
          ETimeZone          tz   = eLocal,
          ETimeZonePrecision tzp  = eTZPrecisionDefault);

    /// Conversion constructor for time_t representation of time.
    ///
    /// Construct time object from GMT time_t value.
    /// The constructed object will be in the eGMT format.
    ///
    /// @param t
    ///   Time in the GMT time_t format.
    /// @param tzp
    ///   What time zone precision to use.
    /// @sa SetTimeT, GetTimeT
    explicit CTime(time_t t, ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Conversion constructor for "struct tm" local time representation.
    ///
    /// Construct time object from "struct tm" time value.
    /// The constructed object will be in the eLocal format.
    ///
    /// @param t
    ///   Time in "struct tm" format.
    /// @param tzp
    ///   What time zone precision to use.
    /// @sa SetTimeTM, GetTimeTM
    CTime(const struct tm& t, ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Constructor.
    ///
    /// Construct time given the year, month, day, hour, minute, second,
    /// nanosecond parts of a time value.
    ///
    /// @param year
    ///   Year part of time.
    /// @param month
    ///   Month part of time. Note month starts from 1.
    /// @param day
    ///   Day part of time. Note day starts from 1.
    /// @param hour
    ///   Hour part of time.
    /// @param minute
    ///   Minute part of time.
    /// @param second
    ///   Second part of time.
    /// @param nanosecond
    ///   Nanosecond part of time.
    /// @param tz
    ///   Whether to use local time (default) or GMT.
    /// @param tzp
    ///   What time zone precision to use.
    CTime(int year, int month, int day,
          int hour = 0, int minute = 0, int second = 0, long nanosecond = 0,
          ETimeZone tz = eLocal,
          ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Constructor.
    ///
    /// Construct date as N-th day of the year.
    ///
    /// @param year
    ///   Year part of date.
    /// @param yearDayNumber
    ///   N-th day of the year.
    /// @param tz
    ///   Whether to use local time (default) or GMT.
    /// @param tzp
    ///   What time zone precision to use.
    CTime(int year, int yearDayNumber,
          ETimeZone tz = eLocal,
          ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Explicit conversion constructor for string representation of time.
    ///
    /// Construct time object from string representation of time.
    ///
    /// @param str
    ///   String representation of time in format "fmt".
    /// @param fmt
    ///   Format in which "str" is presented. Default value of kEmptyStr,
    ///   implies the format, that was previously setup using SetFormat()
    ///   method, or default "M/D/Y h:m:s".
    /// @param tz
    ///   Whether to use local time (default) or GMT.
    /// @param tzp
    ///   What time zone precision to use.
    /// @sa AsString, operator=
    explicit CTime(const string& str, const CTimeFormat& fmt = kEmptyStr,
                   ETimeZone tz = eLocal,
                   ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Copy constructor.
    CTime(const CTime& t);

    /// Assignment operator.
    CTime& operator= (const CTime& t);

    /// Assignment operator.
    ///
    /// If current format contains 'Z', then objects timezone will be set to:
    ///   - eGMT if "str" has word "GMT" in the appropriate position;
    ///   - eLocal otherwise.
    /// If current format does not contain 'Z', objects timezone
    /// will not be changed.
    /// NOTE: This operator expect a string in the format, 
    ///       that was previously setup using SetFormat() method.
    /// @sa CTime constructor from string, AsString
    CTime& operator= (const string& str);

    /// Set time using time_t time value.
    ///
    /// @param t
    ///   Time to set in time object. This is always in GMT time format, and
    ///   nanoseconds will be truncated.
    /// @return
    ///   Time object that is set.
    CTime& SetTimeT(const time_t t);

    /// Get time in time_t format.
    ///
    /// The function return the number of seconds elapsed since midnight
    /// (00:00:00), January 1, 1970 UTC. Do not use this function if the year
    /// is before 1970.
    /// @return
    ///   Time in time_t format.
    time_t GetTimeT(void) const;

    /// Get current GMT time in time_t format (with nanoseconds).
    ///
    /// @param sec
    ///   The function return the number of seconds elapsed since
    ///   midnight (00:00:00), January 1, 1970 UTC.
    /// @param nanosec
    ///   Number of nanoseconds (0, if not possible to get).
    static void GetCurrentTimeT(time_t* sec, long* nanosec = 0);

    /// Set time using "struct tm" time value.
    ///
    /// @param t
    ///   Time to set in time object. This time always represents a local
    ///   time in current time zone. Time object will be set to have eLocal
    ///   time format, and nanoseconds will be truncated. Note that all 
    ///   significant fields in the time structure should be set and have
    ///   correct vales, otherwise exception will be thrown.
    /// @return
    ///   Time object that is set.
    CTime& SetTimeTM(const struct tm& t);

    /// Get time in "struct tm" format.
    ///
    /// @return
    ///   Time in "struct tm" format (local time).
    struct tm GetTimeTM(void) const;

    /// Set time using database format time, TDBTimeU.
    ///
    /// Object's time format will always change to eLocal after call.
    ///
    /// @param t
    ///   Time to set in time object in TDBTimeU format.
    ///   This is always in local time format, and seconds, and nanoseconds
    ///   will be truncated.
    /// @return
    ///   Time object that is set.
    CTime& SetTimeDBU(const TDBTimeU& t);

    /// Set time using database format time, TDBTimeI.
    ///
    /// Object's time format will always change to eLocal after call.
    ///
    /// @param t
    ///   Time to set in time object in TDBTimeI format.
    ///   This is always in local time format, and seconds, and nanoseconds
    ///   will be truncated.
    /// @return
    ///   Time object that is set.
    CTime& SetTimeDBI(const TDBTimeI& t);

    /// Get time in database format time, TDBTimeU.
    ///
    /// @return
    ///   Time value in database format, TDBTimeU.
    TDBTimeU GetTimeDBU(void) const;

    /// Get time in database format time, TDBTimeI.
    ///
    /// @return
    ///   Time value in database format TDBTimeI.
    TDBTimeI GetTimeDBI(void) const;

    /// Make the time current in the presently active time zone.
    CTime& SetCurrent(void);

    /// Make the time "empty",
    CTime& Clear(void);

    /// Set the current time format.
    ///
    /// The default format is: "M/D/Y h:m:s".
    /// @param format
    ///   An object contains string of letters describing the time
    ///   format and its type. The format letters have
    ///   the following meanings:
    ///   - Y = year with century
    ///   - y = year without century           (00-99)
    ///   - M = month as decimal number        (01-12)
    ///   - B = full month name                (January-December)
    ///   - b = abbreviated month name         (Jan-Dec)
    ///   - D = day as decimal number          (01-31)
    ///   - d = day as decimal number (w/o 0)  (1-31)
    ///   - H = hour in 12-hour format         (00-12)
    ///   - h = hour in 24-hour format         (00-23)
    ///   - m = minute as decimal number       (00-59)
    ///   - s = second as decimal number       (00-59)
    ///   - l = milliseconds as decimal number (000-999)
    ///   - r = microseconds as decimal number (000000-999999)
    ///   - S = nanosecond as decimal number   (000000000-999999999)
    ///   - P = am/pm                          (AM/PM)
    ///   - p = am/pm                          (am/pm)
    ///   - Z = timezone format                (GMT or none)
    ///   - z = timezone shift                 ([GMT]+/-HHMM)
    ///         -- available only on POSIX platforms
    ///   - W = full day of week name          (Sunday-Saturday)
    ///   - w = abbreviated day of week name   (Sun-Sat)
    ///
    ///   Format string can represent date/time partially, in this case
    ///   current time, or default values, will be used to amplify time
    ///   object, if possible. Current date/time cannot be used
    ///   if format string contains "z" (time shift) format symbol.
    ///   Also, it cannot be used if time format is ambiguous, like "Y/D".
    ///   Note, that you still can use "Y/M", or even "Y", where month and
    ///   day will be defined to 1; or "M/D", where year will be set as
    ///   current year.
    /// @sa
    ///   CTimeFormat, GetFormat, AsString
    static void SetFormat(const CTimeFormat& format);

    /// Get the current time format.
    ///
    /// The default format is: "M/D/Y h:m:s".
    /// @return
    ///   An object describing the time format.
    /// @sa
    ///   CTimeFormat, SetFormat, AsString
    static CTimeFormat GetFormat(void);

    /// Get numerical value of the month by name.
    ///
    /// @param month
    ///   Full or abbreviated month name.
    /// @return
    ///   Numerical value of a given month (1..12).
    /// @sa
    ///   MonthNumToName, Month
    static int MonthNameToNum(const string& month);

    /// Get name of the month by numerical value.
    ///
    /// @param month
    ///   Full or abbreviated month name.
    /// @param format
    ///   Format for returned value (full or abbreviated).
    /// @return
    ///   Name of the month.
    /// @sa
    ///   MonthNameToNum, Month
    static string MonthNumToName(int month, ENameFormat format = eFull);

    /// Get numerical value of the day of week by name.
    ///
    /// @param day
    ///   Full or abbreviated day of week name.
    /// @return
    ///   Numerical value of a given day of week (0..6).
    /// @sa
    ///   DayOfWeekNumToName, DayOfWeek
    static int DayOfWeekNameToNum(const string& day);

    /// Get name of the day of week by numerical value.
    ///
    /// @param day
    ///   Full or abbreviated day of week name.
    /// @param format
    ///   Format for returned value (full or abbreviated).
    /// @return
    ///   Name of the day of week.
    /// @sa
    ///   DayOfWeekNameToNum, DayOfWeek
    static string DayOfWeekNumToName(int day, ENameFormat format = eFull);

    /// Transform time to string.
    ///
    /// @param format
    ///   Format specifier used to convert time to string.
    ///   If "format" is not defined, then GetFormat() will be used.
    /// @param out_tz
    ///   Output timezone. This is a difference in seconds between GMT time
    ///   and local time for some place (for example, for EST5 timezone
    ///   its value is 18000). This parameter works only with local time.
    ///   If the time object have GMT time that it is ignored.
    ///   Before transformation to string the time will be converted to output
    ///   timezone. Timezone can be printed as string 'GMT[+|-]HHMM' using
    ///   format symbol 'z'. By default current timezone is used.
    /// @sa
    ///   GetFormat, SetFormat
    string AsString(const CTimeFormat& format = kEmptyStr,
                    TSeconds           out_tz = eCurrentTimeZone) const;

    /// Return time as string using the format returned by GetFormat().
    operator string(void) const;

    //
    // Get various components of time.
    //

    /// Get year.
    ///
    /// Year = 1900 ..
    /// AsString() format symbols "Y", "y".
    int Year(void) const;

    /// Get month.
    ///
    /// Month number = 1..12.
    /// AsString() format symbols "M", "B", "b".
    int Month(void) const;

    /// Get day.
    ///
    /// Day of the month = 1..31
    /// AsString() format symbol "D".
    int Day(void) const;

    /// Get hour.
    ///
    /// Hours since midnight = 0..23.
    /// AsString() format symbol "h".
    int Hour(void) const;

    /// Get minute.
    ///
    /// Minutes after the hour = 0..59
    /// AsString() format symbol "m".
    int Minute(void) const;

    /// Get second.
    ///
    /// Seconds after the minute = 0..59
    /// AsString() format symbol "s".
    int Second(void) const;

    /// Get milliseconds.
    ///
    /// Milliseconds after the second = 0..999
    /// AsString() format symbol "l".
    /// @sa
    ///   NanoSecond
    long MilliSecond(void) const;

    /// Get microseconds.
    ///
    /// Microseconds after the second = 0..999999
    /// AsString() format symbol "r".
    /// @sa
    ///   NanoSecond
    long MicroSecond(void) const;

    /// Get nano-seconds.
    ///
    /// Nano-seconds after the second = 0..999999999
    /// AsString() format symbol "S".
    /// @sa
    ///   MilliSecond, MicroSecond
    long NanoSecond(void) const;

    //
    // Set various components of time.
    //

    /// Set year.
    ///
    /// Beware that this operation is inherently inconsistent.
    /// In case of different number of days in the months, the day number
    /// can change, e.g.:
    ///  - "Feb 29 2000".SetYear(2001) => "Feb 28 2001".
    /// Because 2001 is not leap year.
    /// @param year
    ///   Year to set.
    /// @sa
    ///   Year
    void SetYear(int year);

    /// Set month.
    ///
    /// Beware that this operation is inherently inconsistent.
    /// In case of different number of days in the months, the day number
    /// can change, e.g.:
    ///  - "Dec 31 2000".SetMonth(2) => "Feb 29 2000".
    /// Therefore e.g. calling SetMonth(1) again that result will be "Jan 28".
    /// @param month
    ///   Month number to set. Month number = 1..12.
    /// @sa
    ///   Month
    void SetMonth(int month);

    /// Set day.
    ///
    /// Beware that this operation is inherently inconsistent.
    /// In case of number of days in the months, the day number
    /// can change, e.g.:
    ///  - "Feb 01 2000".SetDay(31) => "Feb 29 2000".
    /// @param day
    ///   Day to set. Day of the month = 1..31.
    /// @sa
    ///   Day
    void SetDay(int day);

    /// Set hour.
    ///
    /// @param hour
    ///   Hours since midnight = 0..23.
    /// @sa
    ///   Hour
    void SetHour(int hour);

    /// Set minute.
    ///
    /// @param minute
    ///   Minutes after the hour = 0..59.
    /// @sa
    ///   Minute
    void SetMinute(int minute);

    /// Set second.
    ///
    /// @param second
    ///   Seconds after the minute = 0..59.
    /// @sa
    ///   Second
    void SetSecond(int second);

    /// Set milliseconds.
    ///
    /// @param millisecond
    ///   Milliseconds after the second = 0..999.
    /// @sa
    ///   MilliSecond, SetNanoSecond
    void SetMilliSecond(long millisecond);

    /// Set microseconds.
    ///
    /// @param microsecond
    ///   Microseconds after the second = 0..999999.
    /// @sa
    ///   MicroSecond, SetNanoSecond
    void SetMicroSecond(long microsecond);

    /// Set nanoseconds.
    ///
    /// @param nanosecond
    ///   Nanoseconds after the second = 0..999999999.
    /// @sa
    ///   NanoSecond, SetMilliSecond, SetMicroSecond
    void SetNanoSecond(long nanosecond);

    /// Get year's day number.
    ///
    /// Year day number = 1..366
    int YearDayNumber(void) const;

    /// Get this date's week number within the year.
    ///
    /// Calculate the week number in a year of a given date.
    /// The week can start on any day accordingly given parameter.
    /// First week always start with 1st January.
    /// @param week_start
    ///   What day of week is first.
    ///   Default is to use Sunday as first day of week. For Monday-based
    ///   weeks use eMonday as parameter value.
    /// @return
    ///   Week number = 1..54.
    int YearWeekNumber(EDayOfWeek first_day_of_week = eSunday) const;

    /// Get this date's week number in the month.
    ///
    /// @return
    ///   Week number in the month = 1..6.
    /// @sa
    ///   YearWeekNumber
    int MonthWeekNumber(EDayOfWeek first_day_of_week = eSunday) const;

    /// Get day of week.
    ///
    /// Days since Sunday = 0..6
    /// AsString() format symbols "W", "w".
    int DayOfWeek(void) const;

    /// Get number of days in the month.
    ///
    /// Number of days = 1..31
    int DaysInMonth(void) const;

    /// Add specified years and adjust for daylight saving time.
    ///
    /// It is an exact equivalent of calling AddMonth(years * 12).
    /// @sa
    ///   AddMonth
    CTime& AddYear(int years = 1, EDaylight adl = eDaylightDefault);

    /// Add specified months and adjust for daylight saving time.
    ///
    /// Beware that this operation is inherently inconsistent.
    /// In case of different number of days in the months, the day number
    /// can change, e.g.:
    ///  - "Dec 31 2000".AddMonth(2) => "Feb 28 2001" ("Feb 29" if leap year).
    /// Therefore e.g. calling AddMonth(1) 12 times for e.g. "Jul 31" will
    /// result in "Jul 28" (or "Jul 29") of the next year.
    /// @param months
    ///   Months to add. Default is 1 month.
    ///   If negative, it will result in a "subtraction" operation.
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight savings time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    CTime& AddMonth(int months = 1, EDaylight adl = eDaylightDefault);

    /// Add specified days and adjust for daylight saving time.
    ///
    /// @param days
    ///   Days to add. Default is 1 day.
    ///   If negative, it will result in a "subtraction" operation.
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight saving time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    CTime& AddDay(int days = 1, EDaylight adl = eDaylightDefault);

    /// Add specified hours and adjust for daylight saving time.
    ///
    /// @param hours
    ///   Hours to add. Default is 1 hour.
    ///   If negative, it will result in a "subtraction" operation.
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight saving time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    CTime& AddHour(int hours = 1, EDaylight adl = eDaylightDefault);

    /// Add specified minutes and adjust for daylight saving time.
    ///
    /// @param minutes
    ///   Minutes to add. Default is 1 minute.
    ///   If negative, it will result in a "subtraction" operation.
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight saving time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    CTime& AddMinute(int minutes = 1, EDaylight adl = eDaylightDefault);

    /// Add specified seconds.
    ///
    /// @param seconds
    ///   Seconds to add. Default is 1 second.
    ///   If negative, it will result in a "subtraction" operation.
    CTime& AddSecond(TSeconds seconds = 1, EDaylight adl = eDaylightDefault);

    /// Add specified nanoseconds.
    ///
    /// @param nanoseconds
    ///   Nanoseconds to add. Default is 1 nanosecond.
    ///   If negative, it will result in a "subtraction" operation.
    CTime& AddNanoSecond(long nanoseconds = 1);

    /// Add specified time span.
    ///
    /// @param timespan
    ///   Object of CTimeSpan class to add.
    ///   If negative, it will result in a "subtraction" operation.
    CTime& AddTimeSpan(const CTimeSpan& timespan);


    /// Precision for rounding time.
    /// @sa Round, Truncate
    enum ERoundPrecision {
        eRound_Day,         ///< Round to days
        eRound_Hour,        ///< Round to hours
        eRound_Minute,      ///< Round to minutes
        eRound_Second,      ///< Round to seconds
        eRound_Millisecond, ///< Round to milliseconds
        eRound_Microsecond  ///< Round to microseconds
    };

    /// Round time.
    ///
    /// Round stored time to specified precision. All time components with
    /// precision less that specified will be zero-filled, all other
    /// components will be adjusted accordingly to rules for rounding
    /// numbers.
    /// @param precision
    ///   Rounding precision. 
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight saving time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    /// @sa ERoundPrecision, Truncate
    CTime& Round(ERoundPrecision precision = eRound_Day, 
                 EDaylight       adl       = eDaylightDefault);

    /// Truncate time.
    ///
    /// Truncate stored time to specified precision. All time components with
    /// precision less that specified will be zero-filled. 
    /// By default method strips hours, minutes, seconds and nanoseconds.
    /// @param precision
    ///   Truncating precision. 
    /// @sa ERoundPrecision, Round
    CTime& Truncate(ERoundPrecision precision = eRound_Day);

    //
    // Add/subtract time span
    //

    // Operator to add time span.
    CTime& operator+= (const CTimeSpan& ts);

    /// Operator to subtract time span.
    CTime& operator-= (const CTimeSpan& ts);

    // Operator to add time span.
    CTime operator+ (const CTimeSpan& ts) const;

    /// Operator to subtract time span.
    CTime operator- (const CTimeSpan& ts) const;

    /// Operator to subtract times.
    CTimeSpan operator- (const CTime& t) const;

    //
    // Time comparison ('>' means "later", '<' means "earlier")
    //

    /// Operator to test equality of time.
    bool operator== (const CTime& t) const;

    /// Operator to test in-equality of time.
    bool operator!= (const CTime& t) const;

    /// Operator to test if time is later.
    bool operator>  (const CTime& t) const;

    /// Operator to test if time is earlier.
    bool operator<  (const CTime& t) const;

    /// Operator to test if time is later or equal.
    bool operator>= (const CTime& t) const;

    /// Operator to test if time is earlier or equal.
    bool operator<= (const CTime& t) const;

    //
    // Time difference
    //

    /// Difference in whole days from specified time.
    int DiffWholeDays(const CTime& t) const;

    /// Difference in days from specified time.
    double DiffDay(const CTime& t) const;

    /// Difference in hours from specified time.
    double DiffHour(const CTime& t) const;

    /// Difference in minutes from specified time.
    double DiffMinute(const CTime& t) const;

    /// Difference in seconds from specified time.
    TSeconds DiffSecond(const CTime& t) const;

    /// Difference in nanoseconds from specified time.
    double DiffNanoSecond(const CTime& t) const;

    /// Difference in nanoseconds from specified time.
    CTimeSpan DiffTimeSpan(const CTime& t) const;

    //
    // Checks
    //

    /// Is time object empty (date and time)?
    bool IsEmpty     (void) const;

    /// Is date empty?
    bool IsEmptyDate (void) const;

    /// Is time in a leap year?
    bool IsLeap      (void) const;

    /// Is time valid?
    bool IsValid     (void) const;

    /// Is time local time?
    bool IsLocalTime (void) const;

    /// Is time GMT time?
    bool IsGmtTime   (void) const;

    //
    // Timezone functions
    //

    /// Get time zone.
    ETimeZone GetTimeZone(void) const;
    NCBI_DEPRECATED ETimeZone GetTimeZoneFormat(void) const;

    /// Set time zone.
    ETimeZone SetTimeZone(ETimeZone val);
    NCBI_DEPRECATED ETimeZone SetTimeZoneFormat(ETimeZone val);

    /// Get time zone precision.
    ETimeZonePrecision GetTimeZonePrecision(void) const;

    /// Set time zone precision.
    ETimeZonePrecision SetTimeZonePrecision(ETimeZonePrecision val);

    /// Get the time as local time.
    CTime GetLocalTime(void) const;

    /// Get the time as GMT time.
    CTime GetGmtTime(void) const;

    /// Convert the time into specified time zone time.
    CTime& ToTime(ETimeZone val);

    /// Convert the time into local time.
    CTime& ToLocalTime(void);

    /// Convert the time into GMT time.
    CTime& ToGmtTime(void);

    /// Get difference between local timezone for current time object and UTC in seconds.
    /// @deprecated Use CTime::TimeZoneOffset() instead.
    NCBI_DEPRECATED TSeconds TimeZoneDiff(void) const;

    /// Get difference between local timezone for current time object and UTC in seconds.
    TSeconds TimeZoneOffset(void) const;

    /// Get time zone offset string in format [+/-]HHMM.
    /// @sa TimeZoneName, TimeZoneOffset
    string TimeZoneOffsetStr(void);

    /// Get current time zone name.
    ///
    /// @return
    ///   String with time zone name for current time, if supported by OS,
    ///   or empty string otherwise. This name can be any one of 3-letter
    ///   abbreviated code or some arbitrary string, depending on OS.
    /// @sa
    ///   TimeZoneOffset, TimeZoneOffsetStr
    string TimeZoneName(void);

private:
    /// Helper method to set time value from string "str" using "format".
    void x_Init(const string& str, const CTimeFormat& format);

    /// Helper method to set time from 'time_t' -- If "t" not specified,
    /// then set to current time.
    CTime& x_SetTime(const time_t* t = 0);

    /// Version of x_SetTime() with MT-safe locks
    CTime& x_SetTimeMTSafe(const time_t* t = 0);

    /// Helper method to adjust day number to correct value after day
    /// manipulations.
    void x_AdjustDay(void);

    /// Helper method to adjust the time to correct timezone (across the
    /// barrier of winter & summer times) using "from" as a reference point.
    ///
    /// This does the adjustment only if the time object:
    /// - contains local time (not GMT), and
    /// - has TimeZonePrecision != CTime::eNone, and
    /// - differs from "from" in the TimeZonePrecision (or larger) part.
    CTime& x_AdjustTime(const CTime& from, bool shift_time = true);

    /// Helper method to forcibly adjust timezone using "from" as a
    /// reference point.
    CTime& x_AdjustTimeImmediately(const CTime& from, bool shift_time = true);

    /// Helper method to check if there is a need adjust time in timezone.
    bool x_NeedAdjustTime(void) const;

    /// Helper method to add hour with/without shift time.
    /// Parameter "shift_time" access or denied use time shift in
    /// process adjust hours.
    CTime& x_AddHour(int hours = 1, EDaylight daylight = eDaylightDefault,
                     bool shift_time = true);

private:
#if defined(NCBI_COMPILER_WORKSHOP)  &&  defined(__x86_64)  &&  NCBI_COMPILER_VERSION < 590
// Work around some WorkShop versions' incorrect handling of bitfields
// when compiling for x86-64 (at least with optimization enabled) by
// not using them at all. :-/
#  define NCBI_TIME_BITFIELD(n)
#  define NCBI_TIME_EMPTY_BITFIELD
#else
#  define NCBI_TIME_BITFIELD(n)    : n
#  define NCBI_TIME_EMPTY_BITFIELD unsigned : 0;
#endif
    typedef struct {
        // Time
        unsigned int  year        NCBI_TIME_BITFIELD(12);  // 4 digits
        unsigned char month       NCBI_TIME_BITFIELD( 4);  // 0..12
        unsigned char day         NCBI_TIME_BITFIELD( 5);  // 0..31
        unsigned char hour        NCBI_TIME_BITFIELD( 5);  // 0..23
        unsigned char min         NCBI_TIME_BITFIELD( 6);  // 0..59
        unsigned char sec         NCBI_TIME_BITFIELD( 6);  // 0..61
        // Difference between GMT and local time in seconds,
        // as stored during the last call to x_AdjustTime***().
        Int4          adjTimeDiff NCBI_TIME_BITFIELD(18);
        // Timezone and precision
        ETimeZone     tz          NCBI_TIME_BITFIELD(2);  // local/GMT
        ETimeZonePrecision tzprec NCBI_TIME_BITFIELD(4);  // Time zone precision
        NCBI_TIME_EMPTY_BITFIELD  // Force alignment
        Int4          nanosec;
    } TData;
    TData m_Data;  ///< Packed members

    // Friend class
    friend class CFastLocalTime;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CTimeSpan
///
/// Defines a class to represents a relative time span.
/// Time span can be both positive and negative.
///
/// Throw exception of type CTimeException on errors.

class NCBI_XNCBI_EXPORT CTimeSpan
{
public:
    /// Default constructor.
    CTimeSpan(void);

    /// Constructor.
    ///
    /// Construct time span given the number of days, hours, minutes, seconds,
    /// nanoseconds parts of a time span value.
    /// @param days
    ///   Day part of time. Note day starts from 1.
    /// @param hours
    ///   Hour part of time.
    /// @param minutes
    ///   Minute part of time.
    /// @param seconds
    ///   Second part of time.
    /// @param nanoseconds
    ///   Nanosecond part of time.
    CTimeSpan(long days, long hours, long minutes, long seconds,
              long nanoseconds = 0);

    /// Constructor.
    ///
    /// Construct time span given the number of seconds and nanoseconds.
    /// @param seconds
    ///   Second part of time.
    /// @param nanoseconds
    ///   Nanosecond part of time.
    explicit CTimeSpan(long seconds, long nanoseconds = 0);

    /// Constructor.
    ///
    /// Construct time span from number of seconds.
    /// Please, use this constructor as rarely as possible, because after
    /// doing some arithmetical operations and conversion with it,
    /// the time span can differ at some nanoseconds from expected value.
    /// @param seconds
    ///   Second part of time. The fractional part is used to compute
    ///   nanoseconds.
    explicit CTimeSpan(double seconds);

    /// Explicit conversion constructor for string representation of time span.
    ///
    /// Construct time span object from string representation of time.
    ///
    /// @param str
    ///   String representation of time span in format "fmt".
    /// @param fmt
    ///   Format in which "str" is presented. Default value of kEmptyStr,
    ///   implies the "-S.n" format.
    explicit CTimeSpan(const string& str, const CTimeFormat& fmt = kEmptyStr);

    /// Copy constructor.
    CTimeSpan(const CTimeSpan& t);

    /// Assignment operator.
    CTimeSpan& operator= (const CTimeSpan& t);

    /// Assignment operator.
    CTimeSpan& operator= (const string& str);

    /// Make the time span "empty",
    CTimeSpan& Clear(void);

    /// Get sign of time span.
    ESign GetSign(void) const;

    /// Set the current time span format.
    ///
    /// The default format is: "-S.n".
    /// @param format
    ///   An object contains string of letters describing the time
    ///   format and its type. The format letters have
    ///   the following meanings:
    ///   - - = add minus for negative time spans
    ///   - d = number of whole days
    ///   - H = total whole number of hours stored in the time span
    ///   - h = hours, "H" modulo 24 (-23 - 23)
    ///   - M = total whole number of minutes stored in the time span
    ///   - m = minutes, "M" modulo 60 (-59 - 59)
    ///   - S = total whole number of seconds stored in the time span
    ///   - s = seconds, "S" modulo 60 (-59 - 59)
    ///   - N = total whole number of nanoseconds stored in the time span
    ///   - n = nanoseconds (-999999999 - 999999999)
    /// @sa
    ///   CTimeFormat, GetFormat, AsString
    static void SetFormat(const CTimeFormat& format);

    /// Get the current time span format.
    ///
    /// The default format is: "-S.n".
    /// @return
    ///   An object describing the time format.
    /// @sa
    ///   CTimeFormat, SetFormat, AsString
    static CTimeFormat GetFormat(void);

    /// Transform time span to string.
    ///
    /// @param format
    ///   Format specifier used to convert time span to string.
    ///   If "format" is not defined, then GetFormat() will be used.
    /// @return
    ///   A string representation of time span in specified format.
    /// @sa
    ///   CTimeFormat, GetFormat, SetFormat
    string AsString(const CTimeFormat& format = kEmptyStr) const;

    /// Return span time as string using the format returned by GetFormat().
    operator string(void) const;


    /// Precision for span "smart" string. Used in AsSmartString() method.
    enum ESmartStringPrecision {
        // Named precision levels
        eSSP_Year,          ///< Round to years
        eSSP_Month,         ///< Round to months
        eSSP_Day,           ///< Round to days
        eSSP_Hour,          ///< Round to hours
        eSSP_Minute,        ///< Round to minutes
        eSSP_Second,        ///< Round to seconds
        eSSP_Millisecond,   ///< Round to milliseconds
        eSSP_Microsecond,   ///< Round to microseconds
        eSSP_Nanosecond,    ///< Do not round at all (accurate time span)

        // Floating precision levels (1-7)
        eSSP_Precision1,
        eSSP_Precision2,
        eSSP_Precision3,
        eSSP_Precision4,
        eSSP_Precision5,
        eSSP_Precision6,
        eSSP_Precision7,

        /// Be as smart as possible (use most significant levels of time span)
        eSSP_Smart
    };

    /// Which format use to output zero time span parts.
    enum ESmartStringZeroMode {
        eSSZ_SkipZero,      ///< Skip zero valued parts
        eSSZ_NoSkipZero     ///< Print zero valued parts
    };

    /// Transform time span to "smart" string.
    ///
    /// @param precision
    ///   Enum value describing how many parts of time span should be
    ///   returned. Values from eSSP_Year to eSSP_Nanosecond apparently
    ///   describe part of time span which will be last in output string.
    ///   Float precision levels eSSP_PrecisionN say that maximum 'N'
    ///   parts of time span will be put to output string.
    ///   The parts counting begin from first non-zero value.
    ///   For special precision value eSSP_Smart it try to represent
    ///   time span as close and shorter as possible, usually using only
    ///   one or two most significant non-zero parts of the time span.
    /// @param rounding
    ///   Rounding mode. By default time span will be truncated at last value
    ///   specified by precision. If mode is eRound, that time span will be
    ///   arithmetically rounded by precision level. 
    /// @param zero_mode
    ///   Mode to print or skip zero parts of time span which should be
    ///   printed but have 0 value. Apply to middle and trailing zero parts
    ///   of time span only. Leading zeros will be ignored in common case
    ///   in any mode, they can be printed only if it is impossible
    ///   to represent time span otherwise.
    ///   This parameters is ignored for eSSP_Smart precision level and
    ///   always works as eSSZ_SkipZero.
    /// @return
    ///   A string representation of time span.
    /// @sa
    ///   AsString, ESmartStringPrecision, ERound, ESmartStringZeroMode
    string AsSmartString(ESmartStringPrecision precision = eSSP_Smart,
                         ERound                rounding  = eTrunc,
                         ESmartStringZeroMode  zero_mode = eSSZ_SkipZero)
                         const;

    //
    // Get various components of time span.
    //

    /// Get number of complete days.
    long GetCompleteDays(void) const;

    /// Get number of complete hours.
    long GetCompleteHours(void) const;

    /// Get number of complete minutes.
    long GetCompleteMinutes(void) const;

    /// Get number of complete seconds.
    long GetCompleteSeconds(void) const;

    /// Get number of nanoseconds.
    long GetNanoSecondsAfterSecond(void) const;

    /// Return time span as number of seconds.
    ///
    /// @return
    ///   Return representative of time span as type double.
    ///   The fractional part represents nanoseconds part of time span.
    ///   The double representation of the time span is aproximate.
    double GetAsDouble(void) const;

    /// Return TRUE is an object keep zero time span.
    bool IsEmpty(void) const;

    //
    // Set time span
    //

    /// Set time span in seconds and nanoseconds.
    void Set(long seconds, long nanoseconds = 0);

    /// Set time span from number of seconds (fractional value).
    void Set(double seconds);

    //
    // Arithmetic
    //

    // Operator to add time span.
    CTimeSpan& operator+= (const CTimeSpan& t);

    // Operator to add time span.
    CTimeSpan operator+ (const CTimeSpan& t) const;

    /// Operator to subtract time span.
    CTimeSpan& operator-= (const CTimeSpan& t);

    /// Operator to subtract time span.
    CTimeSpan operator- (const CTimeSpan& t) const;

    /// Unary operator "-" (minus) to change time span sign.
    CTimeSpan operator- (void) const;

    /// Invert time span. Changes time span sign.
    void Invert(void);

    //
    // Comparison
    //

    /// Operator to test equality of time span.
    bool operator== (const CTimeSpan& t) const;

    /// Operator to test in-equality of time span.
    bool operator!= (const CTimeSpan& t) const;

    /// Operator to test if time span is greater.
    bool operator>  (const CTimeSpan& t) const;

    /// Operator to test if time span is less.
    bool operator<  (const CTimeSpan& t) const;

    /// Operator to test if time span is greater or equal.
    bool operator>= (const CTimeSpan& t) const;

    /// Operator to test if time span is less or equal.
    bool operator<= (const CTimeSpan& t) const;

private:
    /// Get hour.
    /// Hours since midnight = -23..23
    int x_Hour(void) const;

    /// Get minute.
    /// Minutes after the hour = -59..59
    int x_Minute(void) const;

    /// Get second.
    /// Seconds after the minute = -59..59
    int x_Second(void) const;

    /// Helper method to set time value from string "str" using "format".
    void x_Init(const string& str, const CTimeFormat& format);

    /// Helper method to normalize stored time value.
    void x_Normalize(void);

    /// Helpers for AsSmartString()
    string x_AsSmartString_Smart_Big  (ERound rounding);
    string x_AsSmartString_Smart_Small(ERound rounding);
    string x_AsSmartString_Precision  (ESmartStringPrecision precision,
                                       ERound rounding, ESmartStringZeroMode zero_mode);

private:
    long  m_Sec;      ///< Seconds part of the time span
    long  m_NanoSec;  ///< Nanoseconds after the second
};



/////////////////////////////////////////////////////////////////////////////
///
/// CTimeout -- Timeout interval
///
/// @sa CNanoTimeout, STimeout, CConnTimeout, CTimeSpan
/// @note Throw exception of type CTimeException on errors.

class NCBI_XNCBI_EXPORT CTimeout
{
public:
    /// Type of timeouts.
    enum EType {
        eFinite,    ///< A finite timeout value has been set.
        eDefault,   ///< Default timeout (to be interpreted by the client code)
        eInfinite,  ///< Infinite timeout.
        eZero       ///< Zero timeout, equal to CTimeout(0,0).
    };

    /// Create default timeout.
    CTimeout(void);

    /// Create timeout of specified type.
    CTimeout(EType type);

    /// Initialize timeout from CTimeSpan.
    CTimeout(const CTimeSpan& ts);

    /// Initialize timeout in seconds and microseconds.
    /// @note
    ///  Use CNanoTimeout ctor to initialize with (seconds and) nanoseconds
    /// @sa CNanoTimeout
    CTimeout(unsigned int sec, unsigned int usec);

    /// Initialize timeout from number of seconds (fractional value).
    explicit CTimeout(double sec);

    /// Destructor.
    ~CTimeout(void) {}

    // Check on special timeout values.
    bool IsDefault()  const;
    bool IsInfinite() const;
    bool IsZero()     const;
    /// Check if timeout holds a numeric value.
    bool IsFinite()   const;

    //
    // Get timeout
    //

    /// Get as number of milliseconds.
    unsigned long GetAsMilliSeconds(void) const;

    /// Get as number of seconds (fractional value).
    double GetAsDouble(void) const;

    /// Convert to CTimeSpan.
    CTimeSpan GetAsTimeSpan(void) const;

    /// Get timeout in seconds and microseconds.
    void Get(unsigned int *sec, unsigned int *microsec) const;

    /// Get timeout in seconds and nanoseconds.
    void GetNano(unsigned int *sec, unsigned int *nanosec) const;


    //
    // Set timeout
    //

    /// Set special value.
    void Set(EType type);

    /// Set timeout in seconds and microseconds.
    void Set(unsigned int sec, unsigned int microsec);

    /// Set timeout in seconds and nanoseconds.
    void SetNano(unsigned int sec, unsigned int nanosec);

    /// Set timeout from number of seconds (fractional value).
    void Set(double sec);

    /// Set from CTimeSpan.
    void Set(const CTimeSpan& ts);

    //
    // Comparison.
    // eDefault special value cannot be compared with any value.
    //

    /// Operator to test equality of timeouts.
    bool operator== (const CTimeout& t) const;

    /// Operator to test in-equality of timeouts.
    bool operator!= (const CTimeout& t) const;

    /// Operator to test if timeout is greater.
    bool operator>  (const CTimeout& t) const;

    /// Operator to test if timeout is less.
    bool operator<  (const CTimeout& t) const;

    /// Operator to test if timeout is greater or equal.
    bool operator>= (const CTimeout& t) const;

    /// Operator to test if timeout is less or equal.
    bool operator<= (const CTimeout& t) const;

private:
    EType         m_Type;       ///< Type of timeout.
    unsigned int  m_Sec;        ///< Seconds part of the timeout.
    unsigned int  m_NanoSec;    ///< Nanoseconds part of the timeout.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CNanoTimeout -- Timeout interval, using nanoseconds 
///
/// @sa CTimeout, STimeout, CConnTimeout, CTimeSpan
/// @note Throw exception of type CTimeException on errors.


class NCBI_XNCBI_EXPORT CNanoTimeout : public CTimeout
{
public:
    CNanoTimeout(unsigned int seconds, unsigned int nanoseconds)
        : CTimeout() {
        SetNano(seconds, nanoseconds);
    }
};



/////////////////////////////////////////////////////////////////////////////
/// 
/// CDeadline
///
///  Given a relative timeout, compose the absolute time mark
///  by adding the timeout to the current time.
///
/// @sa CTimeout

class NCBI_XNCBI_EXPORT CDeadline
{
public:
    /// Initialize deadline using seconds and nanoseconds
    /// (adding to the current time)
    /// @param seconds
    ///   Number of seconds to add to the current time
    /// @param nanoseconds
    ///   Number of nanoseconds to add to the current time
    CDeadline(unsigned int rel_seconds, unsigned int rel_nanoseconds = 0);

    /// Initialize deadline by adding relative timeout to the current time.
    CDeadline(const CTimeout& timeout);

    /// Check if the deadline is infinite.
    bool IsInfinite(void) const { return m_Infinite; }

    /// Check if the deadline is expired.
    bool IsExpired(void) const
    { return !IsInfinite()  &&  GetRemainingTime().IsZero(); }

    /// Get the number of seconds and nanoseconds (since 1/1/1970).
    /// Throw an exception if the deadline is infinite.
    void GetExpirationTime(time_t* sec, unsigned int* nanosec) const;

    /// Get time left to the expiration.
    CNanoTimeout GetRemainingTime(void) const;

    /// Compare two CDeadline values.
    bool operator < (const CDeadline& right_hand_operand) const;

private:
    void x_Now(void);
    void x_Add(unsigned int seconds, unsigned int nanoseconds);

    time_t       m_Seconds;
    unsigned int m_Nanoseconds;
    bool         m_Infinite;
};


/// @deprecated  Use CDeadline instead
NCBI_DEPRECATED typedef CDeadline CAbsTimeout;



/////////////////////////////////////////////////////////////////////////////
///
/// CFastLocalTime --
///
/// Define a class for quick and dirty getting a local time.
///
/// Getting local time may trigger request to a time server daemon,
/// thus potentially causing a relatively significant delay,
/// so we 'll need a caching local timer.

class NCBI_XNCBI_EXPORT CFastLocalTime
{
public:
    /// Constructor.
    /// It should not try to get local time from OS more often than once
    /// an hour. Default:  check once, 5 seconds after each hour.
    CFastLocalTime(unsigned int sec_after_hour = 5);

    /// Get local time
    CTime GetLocalTime(void);

    /// Get difference in seconds between UTC and current local time
    /// (daylight information included)
    int GetLocalTimezone(void);

    /// Do unscheduled check
    void Tuneup(void);

private:
    /// Internal version of Tuneup()
    bool x_Tuneup(time_t timer, long nanosec);

private:
    unsigned int m_SecAfterHour;  ///< Time interval in seconds after hour
                              ///< in which we should avoid to do Tuneup().
    CTime   m_LocalTime;      ///< Current local time
    CTime   m_TunedTime;      ///< Last tuned time (changed by Tuneup())

    time_t  m_LastTuneupTime; ///< Last Tuneup() time
    time_t  m_LastSysTime;    ///< Last system time
    int     m_Timezone;       ///< Cached timezone adjustment for local time
    int     m_Daylight;       ///< Cached system daylight information
    void* volatile m_IsTuneup;///< (bool) Tuneup() in progress (MT)
};


/////////////////////////////////////////////////////////////////////////////
///
/// CStopWatch --
///
/// Define a stop watch class to measure elasped time.

class NCBI_XNCBI_EXPORT CStopWatch
{
public:
    /// Defines how to create new timer.
    enum EStart {
        eStart,   ///< Start timer immediately after creating.
        eStop     ///< Do not start timer, just create it.
    };

    /// Constructor.
    /// NB. By default ctor doesn't start timer, it merely creates it.
    CStopWatch(EStart state = eStop);

    /// Constructor.
    /// Start timer if argument is true.
    /// @deprecated Use CStopWatch(EStat) constuctor instead.
    NCBI_DEPRECATED_CTOR(CStopWatch(bool start));

    /// Start the timer.
    /// Do nothing if already started.
    void Start(void);

    /// Return time elapsed since first Start() or last Restart() call
    /// (in seconds).
    /// Result is 0.0 if Start() or Restart() wasn't previously called.
    double Elapsed(void) const;

    /// Suspend the timer.
    /// Next Start() call continue to count time accured before.
    void Stop(void);

    /// Return time elapsed since first Start() or last Restart() call
    /// (in seconds). Start new timer after that.
    /// Result is 0.0 if Start() or Restart() wasn't previously called.
    double Restart(void);

    /// Stop (if running) and reset the timer.
    void Reset(void);

    /// Check state of stopwatch.
    /// @return
    ///   TRUE if stopwatch is "running", FALSE otherwise.
    /// @sa
    ///   Start, Stop
    bool IsRunning(void);

    /// Set the current stopwatch time format.
    ///
    /// The default format is: "-S.n".
    /// @param format
    ///   Format specifier used to convert time span to string.
    ///   If "format" is not defined, then GetFormat() will be used.
    ///   Uses the same time format as CTimeSpan class.
    /// @sa
    ///   CTimeFormat, CTimeSpan::SetFormat, AsString
    static void SetFormat(const CTimeFormat& format);

    /// Get the current stopwatch time format.
    ///
    /// The default format is: "-S.n".
    /// @return
    ///   An object describing the time format.
    ///   The letters having the same means that for CTimeSpan.
    /// @sa
    ///   CTimeFormat, CTimeSpan::GetFormat, AsString
    static CTimeFormat GetFormat(void);

    /// Transform stopwatch time to string.
    ///
    /// According to used OS, the double representation can provide much
    /// finer grained time control. The string representation is limited
    /// by nanoseconds.
    /// @param format
    ///   If "format" is not defined, then GetFormat() will be used.
    ///   Format specifier used to convert value returned by Elapsed()
    ///   to string.
    /// @sa
    ///   CTimeSpan::AsString, CTimeFormat, Elapsed, GetFormat, SetFormat
    string AsString(const CTimeFormat& format = kEmptyStr) const;

    /// Return stopwatch time as string using the format returned
    /// by GetFormat().
    operator string(void) const;

    /// Transform elapsed time to "smart" string.
    ///
    /// For more details see CTimeSpan::AsSmartString().
    /// @param precision
    ///   Enum value describing how many parts of time span should be
    ///   returned.
    /// @param rounding
    ///   Rounding mode.
    /// @param zero_mode
    ///   Mode to print or skip zero parts of time span.
    /// @return
    ///   A string representation of elapsed time span.
    /// @sa
    ///   CTimeSpan::AsSmartString, AsString, Elapsed
    string AsSmartString(
        CTimeSpan::ESmartStringPrecision precision = CTimeSpan::eSSP_Nanosecond,
        ERound                           rounding  = eTrunc,
        CTimeSpan::ESmartStringZeroMode  zero_mode = CTimeSpan::eSSZ_SkipZero)
        const;

protected:
    /// Get current time mark.
    static double GetTimeMark();

private:
    double m_Start;  ///< Start time value.
    double m_Total;  ///< Accumulated elapsed time.
    EStart m_State;  ///< Stopwatch state (started/stopped)
};



/////////////////////////////////////////////////////////////////////////////
///
/// CTimeException --
///
/// Define exceptions generated by Time API.
///
/// CTimeException inherits its basic functionality from CCoreException
/// and defines additional error codes.

class NCBI_XNCBI_EXPORT CTimeException : public CCoreException
{
public:
    /// Error types that CTime can generate.
    enum EErrCode {
        eArgument,     ///< Bad function argument.
        eConvert,      ///< Error converting value from one format to another.
        eInvalid,      ///< Invalid time value.
        eFormat        ///< Incorrect format.
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CTimeException, CCoreException);
};


/* @} */



//=============================================================================
//
//  Extern
//
//=============================================================================

/// Quick and dirty getter of local time.
/// Use global object of CFastLocalTime class to obtain time.
/// See CFastLocalTime for details.
NCBI_XNCBI_EXPORT
extern CTime GetFastLocalTime(void);

NCBI_XNCBI_EXPORT
extern void TuneupFastLocalTime(void);


//=============================================================================
//
//  Inline
//
//=============================================================================

// Add (subtract if negative) to the time (see CTime::AddXXX)

inline
CTime AddYear(const CTime& t, int  years = 1)
{
    CTime tmp(t);
    return tmp.AddYear(years);
}

inline
CTime AddMonth(const CTime& t, int  months = 1)
{
    CTime  tmp(t);
    return tmp.AddMonth(months);
}

inline
CTime AddDay(const CTime& t, int  days = 1)
{
    CTime  tmp(t);
    return tmp.AddDay(days);
}

inline
CTime AddHour(const CTime& t, int  hours = 1)
{
    CTime  tmp(t);
    return tmp.AddHour(hours);
}

inline
CTime AddMinute(const CTime& t, int  minutes = 1)
{
    CTime  tmp(t);
    return tmp.AddMinute(minutes);
}

inline
CTime AddSecond(const CTime& t, long seconds = 1)
{
    CTime  tmp(t);
    return tmp.AddSecond(seconds);
}

inline
CTime AddNanoSecond (const CTime& t, long nanoseconds = 1)
{
    CTime  tmp(t);
    return tmp.AddNanoSecond(nanoseconds);
}

// Add/subtract CTimeSpan (see CTime::operator +/-)
inline
CTime operator+ (const CTimeSpan& ts, const CTime& t)
{
    CTime tmp(t);
    tmp.AddTimeSpan(ts);
    return tmp;
}

#ifdef CurrentTime // from <X11/X.h>, perhaps
#  undef CurrentTime
#endif
// Get current time (in local or GMT format)
inline
CTime CurrentTime(
    CTime::ETimeZone          tz  = CTime::eLocal,
    CTime::ETimeZonePrecision tzp = CTime::eTZPrecisionDefault
    )
{
    return CTime(CTime::eCurrent, tz, tzp);
}

// Truncate the time to days (see CTime::Truncate)
inline
CTime Truncate(const CTime& t)
{
    CTime  tmp(t);
    return tmp.Truncate();
}

/// Dumps the current stopwatch time to an output stream.
/// The time will be printed out using format specified
/// by CStopWatch::GetFormat().
inline
ostream& operator<< (ostream& os, const CStopWatch& sw)
{
    return os << sw.AsString();
}

/// Dumps the current CTime time to an output stream.
/// The time will be printed out using format
/// returned by CTime::GetFormat().
inline
ostream& operator<< (ostream& os, const CTime& t)
{
    return os << t.AsString();
}


//=============================================================================
//
//  Inline class methods
//
//=============================================================================

//
//  CTimeFormat
//

inline
void CTimeFormat::SetFormat(const string& fmt, TFlags flags)
{
    m_Str   = fmt;
    m_Flags = flags;
}

inline
void CTimeFormat::SetFormat(const char* fmt, TFlags flags)
{
    m_Str   = fmt;
    m_Flags = flags;
}

inline
const string& CTimeFormat::GetString(void) const
{
    return m_Str;
}

inline
CTimeFormat::TFlags CTimeFormat::GetFlags(void) const
{
    return m_Flags;
}

inline
bool CTimeFormat::IsEmpty(void) const
{
    return m_Str.empty();
}

inline
CTimeFormat::operator string(void) const
{
    return m_Str;
}


//
//  CTime
//

inline
int CTime::Year(void) const { return m_Data.year; }

inline
int CTime::Month(void) const { return m_Data.month; }

inline
int CTime::Day(void) const { return m_Data.day; }

inline
int CTime::Hour(void) const { return m_Data.hour; }

inline
int CTime::Minute(void) const { return m_Data.min; }

inline
int CTime::Second(void) const { return m_Data.sec; }

inline
long CTime::MilliSecond(void) const { return (long)m_Data.nanosec/1000000; }

inline
long CTime::MicroSecond(void) const { return (long)m_Data.nanosec/1000; }

inline
long CTime::NanoSecond(void) const { return (long)m_Data.nanosec; }

inline
CTime& CTime::AddYear(int years, EDaylight adl)
{
    return AddMonth(years * 12, adl);
}

inline
CTime& CTime::SetTimeT(const time_t t) { return x_SetTimeMTSafe(&t); }

inline
CTime& CTime::SetCurrent(void) { return x_SetTimeMTSafe(); }

inline
CTime& CTime::operator+= (const CTimeSpan& ts) { return AddTimeSpan(ts); }

inline
CTime& CTime::operator-= (const CTimeSpan& ts) { return AddTimeSpan(-ts); }

inline
CTime CTime::operator+ (const CTimeSpan& ts) const
{
    CTime tmp(*this);
    tmp.AddTimeSpan(ts);
    return tmp;
}

inline
CTime CTime::operator- (const CTimeSpan& ts) const
{
    CTime tmp(*this);
    tmp.AddTimeSpan(-ts);
    return tmp;
}

inline
CTimeSpan CTime::operator- (const CTime& t) const
{
    return DiffTimeSpan(t);
}

inline
CTime::operator string(void) const { return AsString(); }


inline
CTime& CTime::operator= (const string& str)
{
    x_Init(str, GetFormat());
    return *this;
}

inline
CTime& CTime::operator= (const CTime& t)
{
    if ( &t == this ) {
        return *this;
    }
    m_Data = t.m_Data;
    return *this;
}

inline
bool CTime::operator!= (const CTime& t) const
{
    return !(*this == t);
}

inline
bool CTime::operator>= (const CTime& t) const
{
    return !(*this < t);
}

inline
bool CTime::operator<= (const CTime& t) const
{
    return !(*this > t);
}

inline
CTime& CTime::AddHour(int hours, EDaylight use_daylight)
{
    return x_AddHour(hours, use_daylight, true);
}

inline
bool CTime::IsEmpty() const
{
    return
        !Day()   &&  !Month()   &&  !Year()  &&
        !Hour()  &&  !Minute()  &&  !Second()  &&  !NanoSecond();
}

inline
bool CTime::IsEmptyDate() const
{
    // We check year value only, because all time object date fields
    // can be zeros only at one time.
    return !Year();
}

inline
double CTime::DiffDay(const CTime& t) const
{
    return (double)DiffSecond(t) / 60.0 / 60.0 / 24.0;
}

inline
double CTime::DiffHour(const CTime& t) const
{
    return (double)DiffSecond(t) / 60.0 / 60.0;
}

inline
double CTime::DiffMinute(const CTime& t) const
{
    return (double)DiffSecond(t) / 60.0;
}

inline
double CTime::DiffNanoSecond(const CTime& t) const
{
    long dNanoSec = NanoSecond() - t.NanoSecond();
    return (double) DiffSecond(t) * kNanoSecondsPerSecond + dNanoSec;
}

inline
bool CTime::IsLocalTime(void) const { return m_Data.tz == eLocal; }

inline
bool CTime::IsGmtTime(void) const { return m_Data.tz == eGmt; }

inline
CTime::ETimeZone CTime::GetTimeZone(void) const
{
    return m_Data.tz;
}

inline
CTime::ETimeZone CTime::GetTimeZoneFormat(void) const
{
    return GetTimeZone();
}

inline
CTime::ETimeZonePrecision CTime::GetTimeZonePrecision(void) const
{
    return m_Data.tzprec;
}

inline
CTime::ETimeZone CTime::SetTimeZone(ETimeZone val)
{
    ETimeZone tmp = m_Data.tz;
    m_Data.tz = val;
    return tmp;
}

inline
CTime::ETimeZone CTime::SetTimeZoneFormat(ETimeZone val)
{
    return SetTimeZone(val);
}

inline
CTime::ETimeZonePrecision CTime::SetTimeZonePrecision(ETimeZonePrecision val)
{
    ETimeZonePrecision tmp = m_Data.tzprec;
    m_Data.tzprec = val;
    return tmp;
}

inline
CTime& CTime::ToLocalTime(void)
{
    ToTime(eLocal);
    return *this;
}

inline
CTime& CTime::ToGmtTime(void)
{
    ToTime(eGmt);
    return *this;
}

// @deprecated
inline
TSeconds CTime::TimeZoneDiff(void) const
{
    return TimeZoneOffset();
}

inline
bool CTime::x_NeedAdjustTime(void) const
{
    return GetTimeZone() == eLocal  &&  GetTimeZonePrecision() != eNone;
}


//
//  CTimeSpan
//

inline
CTimeSpan::CTimeSpan(void)
{
    Clear();
    return;
}

inline
CTimeSpan::CTimeSpan(long seconds, long nanoseconds)
{
    Set(seconds, nanoseconds);
}

inline
CTimeSpan::CTimeSpan(double seconds)
{
    Set(seconds);
}

inline
CTimeSpan::CTimeSpan(const CTimeSpan& t)
{
    m_Sec = t.m_Sec;
    m_NanoSec = t.m_NanoSec;
}

inline
CTimeSpan& CTimeSpan::Clear(void) {
    m_Sec = 0;
    m_NanoSec = 0;
    return *this;
}

inline
ESign CTimeSpan::GetSign(void) const
{
    if ((m_Sec < 0) || (m_NanoSec < 0)) {
        return eNegative;
    }
    if (!m_Sec  &&  !m_NanoSec) {
        return eZero;
    }
    return ePositive;
}

inline
int CTimeSpan::x_Hour(void) const { return int((m_Sec / 3600L) % 24); }

inline
int CTimeSpan::x_Minute(void) const { return int((m_Sec / 60L) % 60); }

inline
int CTimeSpan::x_Second(void) const { return int(m_Sec % 60L); }

inline
long CTimeSpan::GetCompleteDays(void) const { return m_Sec / 86400L; }

inline
long CTimeSpan::GetCompleteHours(void) const { return m_Sec / 3600L; }

inline
long CTimeSpan::GetCompleteMinutes(void) const { return m_Sec / 60L; }

inline
long CTimeSpan::GetCompleteSeconds(void) const { return m_Sec; }

inline
long CTimeSpan::GetNanoSecondsAfterSecond(void) const { return m_NanoSec; }

inline
double CTimeSpan::GetAsDouble(void) const
{
    return m_Sec + double(m_NanoSec) / kNanoSecondsPerSecond;
}

inline
void CTimeSpan::Set(long seconds, long nanoseconds)
{
    m_Sec = seconds + nanoseconds/kNanoSecondsPerSecond;
    m_NanoSec = nanoseconds % kNanoSecondsPerSecond;
    x_Normalize();
}

inline
bool CTimeSpan::IsEmpty(void) const
{ 
    return m_Sec == 0  &&  m_NanoSec == 0;
}

inline
CTimeSpan& CTimeSpan::operator= (const CTimeSpan& t)
{
    m_Sec = t.m_Sec;
    m_NanoSec = t.m_NanoSec;
    return *this;
}

inline
CTimeSpan& CTimeSpan::operator= (const string& str)
{
    x_Init(str, GetFormat());
    return *this;
}

inline
CTimeSpan::operator string(void) const { return AsString(); }

inline
CTimeSpan& CTimeSpan::operator+= (const CTimeSpan& t)
{
    m_Sec += t.m_Sec;
    m_NanoSec += t.m_NanoSec;
    x_Normalize();
    return *this;
}

inline
CTimeSpan CTimeSpan::operator+ (const CTimeSpan& t) const
{
    CTimeSpan tnew(0, 0, 0, m_Sec + t.m_Sec, m_NanoSec + t.m_NanoSec);
    return tnew;
}

inline
CTimeSpan& CTimeSpan::operator-= (const CTimeSpan& t)
{
    m_Sec -= t.m_Sec;
    m_NanoSec -= t.m_NanoSec;
    x_Normalize();
    return *this;
}

inline
CTimeSpan CTimeSpan::operator- (const CTimeSpan& t) const
{
    CTimeSpan tnew(0, 0, 0, m_Sec - t.m_Sec, m_NanoSec - t.m_NanoSec);
    return tnew;
}

inline
CTimeSpan CTimeSpan::operator- (void) const
{
    CTimeSpan t;
    t.m_Sec     = -m_Sec;
    t.m_NanoSec = -m_NanoSec;
    return t;
}

inline
void CTimeSpan::Invert(void)
{
    m_Sec     = -m_Sec;
    m_NanoSec = -m_NanoSec;
}

inline
bool CTimeSpan::operator== (const CTimeSpan& t) const
{
    return m_Sec == t.m_Sec  &&  m_NanoSec == t.m_NanoSec;
}

inline
bool CTimeSpan::operator!= (const CTimeSpan& t) const
{
    return !(*this == t);
}

inline
bool CTimeSpan::operator> (const CTimeSpan& t) const
{
    if (m_Sec == t.m_Sec) {
        return m_NanoSec > t.m_NanoSec;
    }
    return m_Sec > t.m_Sec;
}


inline
bool CTimeSpan::operator< (const CTimeSpan& t) const
{
    if (m_Sec == t.m_Sec) {
        return m_NanoSec < t.m_NanoSec;
    }
    return m_Sec < t.m_Sec;
}

inline
bool CTimeSpan::operator>= (const CTimeSpan& t) const
{
    return !(*this < t);
}

inline
bool CTimeSpan::operator<= (const CTimeSpan& t) const
{
    return !(*this > t);
}


//
// CTimeout
//

inline
CTimeout::CTimeout(void) { Set(eDefault); }

inline
CTimeout::CTimeout(EType type) { Set(type); }

inline
CTimeout::CTimeout(const CTimeSpan& ts) { Set(ts); }

inline
CTimeout::CTimeout(unsigned int sec, unsigned int usec) { Set(sec, usec); }

inline
CTimeout::CTimeout(double sec) { Set(sec); }

inline
bool CTimeout::IsDefault() const
{ 
    return m_Type == eDefault;
}

inline
bool CTimeout::IsInfinite() const
{
    return m_Type == eInfinite;
}

inline
bool CTimeout::IsFinite() const
{
    return m_Type == eFinite;
}

inline
bool CTimeout::operator!= (const CTimeout& t) const
{
    return !(*this == t);
}



//
//  CStopWatch
//

inline
CStopWatch::CStopWatch(EStart state)
{
    m_Total = 0;
    m_Start = 0;
    m_State = eStop;
    if ( state == eStart ) {
        Start();
    }
}

inline
void CStopWatch::Start(void)
{
    if ( m_State == eStart ) {
        return;
    }
    m_Start = GetTimeMark();
    m_State = eStart;
}


inline
double CStopWatch::Elapsed(void) const
{
    double total = m_Total;
    if ( m_State == eStop ) {
        return total;
    }
    // Workaround for -0 (negative zero) values,
    // that can occur at subtraction of very close doubles.
    double mark = GetTimeMark() - m_Start;
    if (mark > 0.0) {
        total += mark;
    }
    return total;
}


inline
void CStopWatch::Stop(void)
{
    if ( m_State == eStop ) {
        return;
    }
    m_State = eStop;

    double mark = GetTimeMark() - m_Start;
    if (mark > 0.0) {
        m_Total += mark;
    }
}


inline
void CStopWatch::Reset(void)
{
    m_State = eStop;
    m_Total = 0;
    m_Start = 0;
}


inline
double CStopWatch::Restart(void)
{
    double total   = m_Total;
    double current = GetTimeMark();
    if ( m_State == eStart ) {
        // Workaround for -0 (negative zero) values,
        // that can occur at subtraction of very close doubles.
        double mark = current - m_Start;
        if ( mark > 0.0 ) {
            total += mark;
        }
    }
    m_Total = 0;
    m_Start = current;
    m_State = eStart;
    return total;
}

inline
bool CStopWatch::IsRunning(void)
{
    return m_State == eStart;
}


inline
CStopWatch::operator string(void) const
{
    return AsString();
}


inline
string CStopWatch::AsSmartString(
    CTimeSpan::ESmartStringPrecision precision,
    ERound                           rounding,
    CTimeSpan::ESmartStringZeroMode  zero_mode)
    const
{
    return CTimeSpan(Elapsed()).AsSmartString(precision, rounding, zero_mode);
}


END_NCBI_SCOPE

#endif /* CORELIB__NCBITIME__HPP */
