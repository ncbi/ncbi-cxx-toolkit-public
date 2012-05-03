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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_signal.hpp>

#if !defined(HAVE_SIGNAL_H)
#  error "signal.h is not found on this platform"
#else
#  include <signal.h>


BEGIN_NCBI_SCOPE

/// Use sigaction() instead of signal() where possible
#ifdef SIGSEGV
#  if !defined(NCBI_OS_MSWIN)
#    define HAS_SIGACTION 1
#  endif
#endif

// Mask of handled signals
static CSignal::TSignalMask s_SignalMask = 0;
// Global set of caught signals
static CSignal::TSignalMask s_Signals = 0;


// Define missed signals to zero
#ifndef SIGHUP
#  define SIGHUP    0
#endif
#ifndef SIGINT
#  define SIGINT    0
#endif
#ifndef SIGBREAK    // Ctrl-Break sequence on MS Windows
#  define SIGBREAK  0
#endif
#ifndef SIGILL
#  define SIGILL    0
#endif
#ifndef SIGFPE
#  define SIGFPE    0
#endif
#ifndef SIGABRT
#  define SIGABRT   0
#endif
#ifndef SIGSEGV
#  define SIGSERV   0
#endif
#ifndef SIGPIPE
#  define SIGPIPE   0
#endif
#ifndef SIGTERM
#  define SIGTERM   0
#endif
#ifndef SIGUSR1
#  define SIGUSR1   0
#endif
#ifndef SIGUSR2
#  define SIGUSR2   0
#endif


// Internal signal handler
extern "C" 
void s_CSignal_SignalHandler(int signum)
{
    if ( !signum ) {
        return;
    }
    else if (signum == SIGHUP) {
        s_Signals |= CSignal::eSignal_HUP;
    }
    else if (signum == SIGBREAK  ||  signum == SIGINT) {
        s_Signals |= CSignal::eSignal_INT;
    }
    else if (signum == SIGILL) {
        s_Signals |= CSignal::eSignal_ILL;
    } 
    else if (signum == SIGFPE) {
        s_Signals |= CSignal::eSignal_FPE;
    }
    else if (signum == SIGABRT) {
        s_Signals |= CSignal::eSignal_ABRT;
    }
    else if (signum == SIGSEGV) {
        s_Signals |= CSignal::eSignal_SEGV;
    }
    else if (signum == SIGPIPE) {
        s_Signals |= CSignal::eSignal_PIPE;
    }
    else if (signum == SIGTERM) {
        s_Signals |= CSignal::eSignal_TERM;
    }
    else if (signum == SIGUSR1) {
        s_Signals |= CSignal::eSignal_USR1;
    }
    else if (signum == SIGUSR2) {
        s_Signals |= CSignal::eSignal_USR2;
    }
    else {
        // Please sync with TrapSignal()
        _TROUBLE;
    }
}


#ifdef HAS_SIGACTION
#  define SET_SIGNAL(SIGNAL, HANDLER) \
    if (SIGNAL) { \
        struct sigaction sig; \
        memset(&sig, 0, sizeof(sig)); \
        sig.sa_handler = HANDLER; \
        sigaction(SIGNAL, &sig, NULL); \
    }
#else
#  define SET_SIGNAL(SIGNAL, HANDLER) \
    if (SIGNAL) { \
        signal(SIGNAL, HANDLER); \
    }
#endif

#define TRAP_SIGNAL(SIGMASK, SIGNAL) \
    if (SIGNAL  &&  (signals & SIGMASK)) { \
        SET_SIGNAL(SIGNAL, s_CSignal_SignalHandler); \
        s_SignalMask |= SIGMASK; \
    }

void CSignal::TrapSignals(TSignalMask signals)
{
    TRAP_SIGNAL(eSignal_HUP, SIGHUP);
    TRAP_SIGNAL(eSignal_INT, SIGINT);
    TRAP_SIGNAL(eSignal_INT, SIGBREAK); // Ctrl-Break sequence on MS Windows
    TRAP_SIGNAL(eSignal_ILL, SIGILL);
    TRAP_SIGNAL(eSignal_FPE, SIGFPE);
    TRAP_SIGNAL(eSignal_ABRT, SIGABRT);
    TRAP_SIGNAL(eSignal_SEGV, SIGSEGV);
    TRAP_SIGNAL(eSignal_PIPE, SIGPIPE);
    TRAP_SIGNAL(eSignal_TERM, SIGTERM);
    TRAP_SIGNAL(eSignal_USR1, SIGUSR1);
    TRAP_SIGNAL(eSignal_USR2, SIGUSR2);
}


CSignal::TSignalMask CSignal::Reset(void)
{
    TSignalMask old = s_SignalMask;
    s_SignalMask = 0;

    SET_SIGNAL(SIGHUP,  SIG_DFL);
    SET_SIGNAL(SIGINT,  SIG_DFL);
    SET_SIGNAL(SIGBREAK,SIG_DFL);
    SET_SIGNAL(SIGILL,  SIG_DFL);
    SET_SIGNAL(SIGFPE,  SIG_DFL);
    SET_SIGNAL(SIGABRT, SIG_DFL);
    SET_SIGNAL(SIGSEGV, SIG_DFL);
    SET_SIGNAL(SIGPIPE, SIG_DFL);
    SET_SIGNAL(SIGTERM, SIG_DFL);
    SET_SIGNAL(SIGUSR1, SIG_DFL);
    SET_SIGNAL(SIGUSR2, SIG_DFL);

    return old;
}


bool CSignal::IsSignaled(TSignalMask signals)
{
    return (s_Signals & signals) != 0;
}


CSignal::TSignalMask CSignal::GetSignals()
{
    return s_Signals;
}


CSignal::TSignalMask CSignal::ClearSignals(TSignalMask signals)
{
    s_Signals &= (~signals);
    return s_Signals;
}


bool CSignal::Raise(ESignal signal)
{
    int signum = 0;
    switch (signal) {
        case eSignal_HUP:
            signum = SIGHUP;
            break;
        case eSignal_INT:
            signum = SIGINT;
            break;
        case eSignal_ILL:
            signum = SIGILL;
            break;
        case eSignal_FPE:
            signum = SIGFPE;
            break;
        case eSignal_ABRT:
            signum = SIGABRT;
            break;
        case eSignal_SEGV:
            signum = SIGSEGV;
            break;
        case eSignal_PIPE:
            signum = SIGPIPE;
            break;
        case eSignal_TERM:
            signum = SIGTERM;
            break;
        case eSignal_USR1:
            signum = SIGUSR1;
            break;
        case eSignal_USR2:
            signum = SIGUSR2;
            break;
        default:
            // Please sync with CSignal::ESignal
            _TROUBLE;
    }
    if ( !signum ) {
        // Corresponding signal is not defined on this platform
        return false;
    }
    if (raise(signum) != 0) {
        // Error sending a signal
        return false;
    }
    return true;
}


END_NCBI_SCOPE

#endif  // HAVE_SIGNAL_H
