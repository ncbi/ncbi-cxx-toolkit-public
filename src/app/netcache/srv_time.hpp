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


#ifdef NCBI_OS_LINUX
class CSrvTime : public timespec
#else
class CSrvTime
#endif
{
public:
    static int CurSecs(void);
    static CSrvTime Current(void);
    static int TZAdjustment(void);

    CSrvTime(void);
    time_t& Sec(void);
    time_t Sec(void) const;
    long& NSec(void);
    long NSec(void) const;
    Uint8 AsUSec(void) const;
    int Compare(const CSrvTime& t) const;

    CSrvTime& operator+= (const CSrvTime& t);
    CSrvTime& operator-= (const CSrvTime& t);
    bool operator> (const CSrvTime& t) const;
    bool operator>= (const CSrvTime& t) const;
    bool operator< (const CSrvTime& t) const;
    bool operator<= (const CSrvTime& t) const;

    enum EFormatType {
        eFmtLogging,
        eFmtHumanSeconds,
        eFmtHumanUSecs
    };

    Uint1 Print(char* buf, EFormatType fmt) const;
};


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_TIME__HPP */
