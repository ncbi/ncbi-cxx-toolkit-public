#ifndef NCBITIME_HPP
#define NCBITIME_HPP

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
*           Denis    Vakatov
*           Vladimir Ivanov
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
* Revision 1.5  2001/04/30 22:01:29  lavr
* Rollback to pre-nanosecond-revision due to necessity to use
* configure to figure out names of global variables governing time zones
*
* Revision 1.4  2001/04/29 03:06:10  lavr
* #include <time.h>" moved from .cpp to ncbitime.hpp
*
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
#include <time.h>

BEGIN_NCBI_SCOPE

class CTime;


// Add (subtract if negative) to the time (see CTime::AddXXX)
extern CTime AddYear   (const CTime& tm, int years   = 1);
extern CTime AddMonth  (const CTime& tm, int months  = 1);
extern CTime AddDay    (const CTime& tm, int days    = 1);
extern CTime AddHour   (const CTime& tm, int hours   = 1);
extern CTime AddMinute (const CTime& tm, int minutes = 1);
extern CTime AddSecond (const CTime& tm, int seconds = 1);

// Add/subtract days (see CTime::operator)
extern CTime operator + (const CTime& tm,   int          days);
extern CTime operator + (int          days, const CTime& tm  );
extern CTime operator - (const CTime& tm,   int          days);

// Difference in days (see CTime::operator)
extern int operator - (const CTime& tm1, const CTime& tm2);

// Get current time
extern CTime CurrentTime(void);

// Truncate the time to days (see CTime::Truncate)
extern CTime Truncate(const CTime& tm);



//
//  CTime
//

class CTime
{
public:
    // Initialize using current time (or "empty" time)
    enum EMode {
        eCurrent,  // use current time
        eEmpty     // use "empty" time
    };
    CTime(EMode mode=eEmpty);

    // Note:  "day" and "month" here are starting from 1 (one)
    CTime(int year, int month, int day,
          int hour=0, int minute=0, int second=0);

    // Construct date as N-th day of the year
    CTime(int year, int day);

    // Read from the string "str" using format "fmt".
    // If "fmt" is empty, then use the current format -- as set by SetFormat().
    // Default format is "M/D/Y h:m:s".
    CTime(const string& str, const string& fmt = kEmptyStr);

    // Copy constructor
    CTime(const CTime& tm);

    // Assignment
    CTime& operator=(const CTime&  tm);
    CTime& operator=(const string& str);  // uses current format

    // Make the time "empty"
    CTime& Clear(void);

    // Change current format (the default one is: "M/D/Y h:m:s").
    // "Y" - 4 digit year,  "y" - 2 digit year,  "M" - month,  "D" - day
    // "h" - hour, "m" - minute,  "s" - second.
    static void SetFormat(const string& fmt);

    // Transform time to string (use format "sm_Format" by default)
    string AsString(const string& fmt = kEmptyStr) const;
    operator string(void) const { return AsString(); }

    // Get various constituents of the time
    int YearDayNumber(void) const;
    int Year   (void) const { return m_Year;   }
    int Month  (void) const { return m_Month;  }
    int Day    (void) const { return m_Day;    }
    int Hour   (void) const { return m_Hour;   }
    int Minute (void) const { return m_Minute; }
    int Second (void) const { return m_Second; }

    // Add (subtract if negative) to the time
    CTime& AddYear   (int years   = 1);
    CTime& AddMonth  (int months  = 1);
    CTime& AddDay    (int days    = 1);
    CTime& AddHour   (int hours   = 1);
    CTime& AddMinute (int minutes = 1);
    CTime& AddSecond (int seconds = 1);

    // Add/subtract days
    CTime& operator += (int days)  { return AddDay( days); }
    CTime& operator -= (int days)  { return AddDay(-days); }
    CTime& operator ++ (void)      { return AddDay( 1); }
    CTime& operator -- (void)      { return AddDay(-1); }
    CTime  operator ++ (int);
    CTime  operator -- (int);

    // Time difference
    double DiffDay    (const CTime& tm) const;
    double DiffHour   (const CTime& tm) const;
    double DiffMinute (const CTime& tm) const;
    int    DiffSecond (const CTime& tm) const;

    // Checks
    bool IsEmpty(void) const;
    bool IsLeap (void) const;
    bool IsValid(void) const;

    // Make the time current
    CTime& SetCurrent(void);

    // Truncate the time to days (strip H,M,S)
    CTime& Truncate(void);

    // Time comparison ('>' means "later",  '<' means "earlier")
    bool operator == (const CTime& tm) const;
    bool operator != (const CTime& tm) const;
    bool operator >  (const CTime& tm) const;
    bool operator <  (const CTime& tm) const;
    bool operator >= (const CTime& tm) const;
    bool operator <= (const CTime& tm) const;

private:
    static void x_VerifyFormat(const string& fmt);

    void x_Init(const string& str, const string& fmt);
    void x_AdjustDay(void);

    void x_SetYear   (int year);
    void x_SetHour   (int hour);
    void x_SetMinute (int minute);
    void x_SetSecond (int second);

    int           m_Year;
    unsigned char m_Month;
    unsigned char m_Day;
    unsigned char m_Hour;
    unsigned char m_Minute;
    unsigned char m_Second;

    static string sm_Format;
};


END_NCBI_SCOPE

#endif /* NCBITIME_HPP */
