#ifndef NETCACHE__SRV_TIME__HPP
#define NETCACHE__SRV_TIME__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 */


#ifdef NCBI_OS_LINUX
# include <sys/time.h>
#endif


BEGIN_NCBI_SCOPE


enum {
    kMSecsPerSecond = 1000,
    kUSecsPerMSec = 1000,
    kNSecsPerUSec = 1000,
    kUSecsPerSecond = kUSecsPerMSec * kMSecsPerSecond,
    kNSecsPerMSec = kNSecsPerUSec * kUSecsPerMSec,
    kNSecsPerSecond = kNSecsPerMSec * kMSecsPerSecond
};


/// Class incorporating convenient methods to work with struct timespec.
/// Besides that the class allows to hide all dependency on Linux inside
/// making outside code cleaner. Object of this class can store absolute times
/// or time intervals.
#ifdef NCBI_OS_LINUX
class CSrvTime : public timespec
#else
class CSrvTime
#endif
{
public:
    /// Current time in seconds since epoch (time_t). This is not absolutely
    /// exact current time, it's the value of variable which should be updated
    /// once in a jiffy (0.001 of a second by default) although with overloaded
    /// server it can update less often.
    static int CurSecs(void);
    /// Exact current time with precision up to nanoseconds.
    static CSrvTime Current(void);
    /// Timezone adjustment (in seconds) for times stored in CSrvTime.
    /// If you add result of this method to number of seconds from CSrvTime
    /// (or from CurSecs() function) and pass the result to gmtime_r you'll
    /// get correct time components in the current timezone.
    static int TZAdjustment(void);

    CSrvTime(void);
    /// Read/set number of seconds since epoch stored in the object.
    time_t& Sec(void);
    time_t Sec(void) const;
    /// Read/set number of nanoseconds stored in the object
    long& NSec(void);
    long NSec(void) const;
    /// Converts object's value to microseconds since epoch.
    Uint8 AsUSec(void) const;
    /// Compares object's value to another CSrvTime object's value. Returns 0
    /// if values are equal, -1 if this object's value is earlier than
    /// provided, 1 if this object's value is later than provided.
    int Compare(const CSrvTime& t) const;

    /// Add value of other CSrvTime object to this one. Result is stored in
    /// this object.
    CSrvTime& operator+= (const CSrvTime& t);
    /// Subtract value of other CSrvTime object from this one. Result is stored
    /// in this object.
    CSrvTime& operator-= (const CSrvTime& t);
    /// Compare object's value to another CSrvTime object's value.
    bool operator> (const CSrvTime& t) const;
    bool operator>= (const CSrvTime& t) const;
    bool operator< (const CSrvTime& t) const;
    bool operator<= (const CSrvTime& t) const;

    /// Type of time formatting in the Print() method.
    enum EFormatType {
        /// Format used in logs which is YYYY-MM-DDThh:mm:ss.ssssss
        eFmtLogging,
        /// Format that can be readable by humans with precision up to seconds.
        /// This format looks like MM/DD/YYYY hh:mm:ss.
        eFmtHumanSeconds,
        /// Format that can be readable by humans with precision up to
        /// microseconds. This format looks like MM/DD/YYYY hh:mm:ss.ssssss.
        eFmtHumanUSecs
    };
    /// Formats time value in the object and writes it in buf. Buffer should
    /// have sufficient size for the whole time (see above formats description
    /// for size necessary for each format).
    Uint1 Print(char* buf, EFormatType fmt) const;
};


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_TIME__HPP */
