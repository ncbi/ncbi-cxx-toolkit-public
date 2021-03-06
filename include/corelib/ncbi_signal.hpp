#ifndef CORELIB___NCBI_SIGNAL__HPP
#define CORELIB___NCBI_SIGNAL__HPP

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

/// @file ncbi_signal.hpp
/// Setup interrupt signal handling.

#include <corelib/ncbistd.hpp>

/** @addtogroup Utility
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CSignal --
///
/// Setup interrupt signal handling.
///
/// @note
///   Windows:
///     The SIGINT cannot be cough by handler, except it was explicitly
///     generated by calling raise().
///     The SIGILL, SIGSEGV, and SIGTERM signals are not generated under
///     Windows NT. They are included for ANSI compatibility. Thus you can
///     set signal handlers for these signals, and you can also explicitly
///     generate these signals by calling raise().
/// @note
///   Signal settings are not preserved in spawned processes, they resets
///   to the default in the each child process.

class NCBI_XNCBI_EXPORT CSignal
{
public:
    enum ESignal {
        eSignal_HUP   = (1<<1),     ///< Hangup
        eSignal_INT   = (1<<2),     ///< Interrupt
        eSignal_QUIT  = (1<<3),     ///< Quit
        eSignal_ILL   = (1<<4),     ///< Illegal instruction
        eSignal_FPE   = (1<<5),     ///< Floating-point exception
        eSignal_ABRT  = (1<<6),     ///< Abort
        eSignal_SEGV  = (1<<7),     ///< Segmentation violation
        eSignal_PIPE  = (1<<8),     ///< Broken pipe
        eSignal_TERM  = (1<<9),     ///< Termination
        eSignal_USR1  = (1<<10),    ///< User defined signal 1
        eSignal_USR2  = (1<<11),    ///< User defined signal 2

        eSignal_Any   = 0xfffffff   ///< Any/all signal(s) from the list above
    };
    typedef int TSignalMask;        ///< Binary OR of "ESignal"

    /// Sets interrupt signal handling.
    ///
    /// Install default signal handler for all specified signal types.
    /// Use IsSignaled/GetSignals to check in your code to check that some
    /// signal was caught. Consecutive calls add signals to list of handled
    /// signals.
    /// @sa IsSignaled, GetSignals, Reset
    /// @param signals
    ///   The set of signals to check.
    static void TrapSignals(TSignalMask signals);

    /// Reset the list of handled signal.
    ///
    /// Default signal handlers will be used for all handled signals (SIG_DFL).
    /// @return
    ///   List of handled signals before reset.
    /// @sa TrapSignals, GetSignals
    static TSignalMask Reset(void);

    /// Check that any of specified signals is received.
    ///
    /// @param signals
    ///   The set of signals to check.
    /// @return
    ///   TRUE if any of specified signals is set, FALSE otherwise.
    /// @sa TrapSignals, GetSignals, ClearSignals
    static bool IsSignaled(TSignalMask signals = eSignal_Any);

    /// Get signals state.
    ///
    /// @return
    ///   Current signal state. Each bit represent some caught signal.
    /// @sa TrapSignals, ClearSignals, IsSignaled
    static TSignalMask GetSignals(void);

    /// Clear signals state.
    ///
    /// @return
    ///   Current signal state after clearing. Each bit represent some caught signal.
    /// @sa TrapSignals, GetSignals, IsSignaled
    static TSignalMask ClearSignals(TSignalMask signals = eSignal_Any);

    /// Sends a signal to the current process.
    ///
    /// @param signal
    ///   Signal to send to the current process.
    /// @return
    ///   TRUE on success, FALSE otherwise. Return FALSE also if the signal
    ///   is not supported on current platform.
    static bool Raise(ESignal signal);
};


END_NCBI_SCOPE


#endif  // CORELIB___NCBI_SIGNAL__HPP
