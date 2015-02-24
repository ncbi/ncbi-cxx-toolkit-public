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
* Author:  Aaron Ucko
*
* File Description:
*   Portable system-logging API.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/syslog.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/error_codes.hpp>

#ifdef NCBI_OS_UNIX
#  include <syslog.h>
#  ifndef LOG_MAKEPRI
#    define LOG_MAKEPRI(facility, priority) ((facility) | (priority))
#  endif
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Diag


BEGIN_NCBI_SCOPE

DEFINE_CLASS_STATIC_MUTEX(CSysLog::sm_Mutex);
CSysLog* CSysLog::sm_Current = NULL;


const char* CSysLog::kLogName_Syslog = "SYSLOG";


CSysLog::CSysLog(const string& ident, TFlags flags, EFacility default_facility)
    : m_Ident(ident), m_Flags(flags),
      m_DefaultFacility((default_facility == eDefaultFacility) ? 0 
                        : x_TranslateFacility(default_facility))
{
#ifndef NCBI_OS_UNIX
    NCBI_THROW(CCoreException, eInvalidArg,
               "CSysLog not implemented for this platform");
#endif
    if (flags & fConnectNow) {
        CMutexGuard GUARD(sm_Mutex);
        x_Connect();
    }
}

CSysLog::CSysLog(const string& ident, TFlags flags, int default_facility)
    : m_Ident(ident), m_Flags(flags), m_DefaultFacility(default_facility)
{
#ifndef NCBI_OS_UNIX
    NCBI_THROW(CCoreException, eInvalidArg,
               "CSysLog not implemented for this platform");
#endif
    if (flags & fConnectNow) {
        CMutexGuard GUARD(sm_Mutex);
        x_Connect();
    }
}

CSysLog::~CSysLog()
{
    CMutexGuard GUARD(sm_Mutex);
    if (sm_Current == this) {
#ifdef NCBI_OS_UNIX
        closelog();
#endif
        sm_Current = NULL;
    }
}


void CSysLog::Post(const SDiagMessage& mess)
{
    string message_str;
    mess.Write(message_str, SDiagMessage::fNoEndl);
    EPriority priority;
    switch (mess.m_Severity) {
    case eDiag_Info:      priority = eInfo;      break;
    case eDiag_Warning:   priority = eWarning;   break;
    case eDiag_Error:     priority = eError;     break;
    case eDiag_Critical:  priority = eCritical;  break;
    case eDiag_Fatal:     priority = eAlert;     break;
    case eDiag_Trace:     priority = eDebug;     break;
    default:              priority = eNotice;    break;
    }
    Post(message_str, priority);
}

void CSysLog::Post(const string& message, EPriority priority,
                   EFacility facility)
{
    Post(message, priority, x_TranslateFacility(facility));
}

void CSysLog::Post(const string& message, EPriority priority, int facility)
{
#ifdef NCBI_OS_UNIX
    CMutexGuard GUARD(sm_Mutex);
    if (sm_Current != this  &&  !(m_Flags & fNoOverride)) {
        x_Connect();
    }
#  ifndef LOG_PID
    if (m_Flags & fIncludePID) {
        syslog(LOG_MAKEPRI(facility, priority), "[%d] %s",
               getpid(), message.c_str());
    } else
#  endif
    {
        syslog(LOG_MAKEPRI(facility, priority), "%s", message.c_str());
    }
#  ifndef LOG_PERROR
    // crudely implement it ourselves...
    if (m_Flags & fCopyToStderr) {
        clog << message << endl;
    }
#  endif
#else
    clog << message << endl;
#endif
}

int CSysLog::x_TranslateFlags(TFlags flags)
{
    if (flags & fNoOverride) {
#ifdef _DEBUG
        if (flags != fNoOverride) {
            ERR_POST_X(15, Warning << "CSysLog::x_TranslateFlags:"
                           " fNoOverride is incompatible with other flags.");
        }
#endif
        return 0;
    }

    int result = 0;
#ifdef _DEBUG
    if (flags & ~fAllFlags) {
        ERR_POST_X(16, Warning
                       << "CSysLog::x_TranslateFlags: ignoring extra flags.");
    }
#endif
#ifdef NCBI_OS_UNIX
#  ifdef LOG_PERROR
    if (flags & fCopyToStderr) {
        result |= LOG_PERROR;
    }
#  endif
#  ifdef LOG_CONS
    if (flags & fFallBackToConsole) {
        result |= LOG_CONS;
    }
#  endif
#  ifdef LOG_PID
    if (flags & fIncludePID) {
        result |= LOG_PID;
    }
#  endif
#  ifdef LOG_NDELAY
    if (flags & fConnectNow) {
        result |= LOG_NDELAY;
    }
#  elif defined(LOG_ODELAY)
    if ( !(flags & fConnectNow) ) {
        result |= LOG_ODELAY;
    }
#  endif
#  ifdef LOG_NOWAIT
    if (flags & fNoChildWait) {
        result |= LOG_NOWAIT;
    }
#  endif
#endif
    return result;
}

int CSysLog::x_TranslateFacility(EFacility facility)
{
    switch (facility) {
    case eDefaultFacility:  return m_DefaultFacility;
#ifdef NCBI_OS_UNIX
    case eKernel:           return LOG_KERN;
    case eUser:             return LOG_USER;
    case eMail:             return LOG_MAIL;
    case eDaemon:           return LOG_DAEMON;
    case eAuth:             return LOG_AUTH;
    case eSysLog:           return LOG_SYSLOG;
    case eLPR:              return LOG_LPR;
    case eNews:             return LOG_NEWS;
    case eUUCP:             return LOG_UUCP;
    case eCron:             return LOG_CRON;
#  ifdef LOG_AUTHPRIV
    case eAuthPriv:         return LOG_AUTHPRIV;
#  else
    case eAuthPriv:         return LOG_AUTH;
#  endif
#  ifdef LOG_FTP
    case eFTP:              return LOG_FTP;
#  endif
    case eLocal0:           return LOG_LOCAL0;
    case eLocal1:           return LOG_LOCAL1;
    case eLocal2:           return LOG_LOCAL2;
    case eLocal3:           return LOG_LOCAL3;
    case eLocal4:           return LOG_LOCAL4;
    case eLocal5:           return LOG_LOCAL5;
    case eLocal6:           return LOG_LOCAL6;
    case eLocal7:           return LOG_LOCAL7;
#endif
    default:                return m_DefaultFacility;
    }
}

void CSysLog::x_Connect(void)
{
#ifdef NCBI_OS_UNIX
    if (m_Flags & fNoOverride) {
        return;
    }
    openlog(m_Ident.empty() ? NULL : m_Ident.c_str(),
            x_TranslateFlags(m_Flags),
            m_DefaultFacility);
    sm_Current = this;
#endif
}

void CSysLog::HonorRegistrySettings(IRegistry* reg)
{
    if (reg == NULL  &&  CNcbiApplication::Instance()) {
        reg = &CNcbiApplication::Instance()->GetConfig();
    }
    if (reg == NULL  ||  !(m_Flags & fNoOverride) ) {
        return;
    }

    string fac_name = reg->Get("LOG", "SysLogFacility");
    if ( !fac_name.empty() ){
        // just check for the few possibilities that make sense
        EFacility facility = eDefaultFacility;
        if (fac_name.size() == 6
            &&  NStr::StartsWith(fac_name, "local", NStr::eNocase)
            &&  fac_name[5] >= '0'  &&  fac_name[5] <= '7') {
            facility = static_cast<EFacility>(eLocal0 + fac_name[5] - '0');
        } else if (NStr::EqualNocase(fac_name, "user")) {
            facility = eUser;
        } else if (NStr::EqualNocase(fac_name, "mail")) {
            facility = eMail;
        } else if (NStr::EqualNocase(fac_name, "daemon")) {
            facility = eDaemon;
        } else if (NStr::EqualNocase(fac_name, "auth")) {
            facility = eAuth;
        } else if (NStr::EqualNocase(fac_name, "authpriv")) {
            facility = eAuthPriv;
        } else if (NStr::EqualNocase(fac_name, "ftp")) {
            facility = eFTP;
        }
        if (facility != eDefaultFacility) {
            CMutexGuard GUARD(sm_Mutex);
            m_Flags &= ~fNoOverride;
            m_DefaultFacility = facility;
            if (sm_Current == this) {
                // x_Connect();
                sm_Current = NULL;
            }
        }
    }
}

END_NCBI_SCOPE
