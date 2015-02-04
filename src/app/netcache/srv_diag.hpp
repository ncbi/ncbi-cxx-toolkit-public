#ifndef NETCACHE__SRV_DIAG__HPP
#define NETCACHE__SRV_DIAG__HPP
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



BEGIN_NCBI_SCOPE


class CTempString;
class CRequestContext;
struct SSrvThread;
struct SLogData;


#define _VERIFY(x)  if (x) {} else SRV_FATAL("assertion failed: " << #x)

#ifdef _ASSERT
# undef _ASSERT
#endif
#ifdef _DEBUG
# define _ASSERT(x)  _VERIFY(x)
#else
# define _ASSERT(x)  do {} while (0)
#endif

#ifdef assert
# undef assert
#endif
#define assert(x)   _ASSERT(x)


/// Class used in all diagnostic logging.
/// This class effectively replaces things like CNcbiDiag and CDiagContext.
/// Any started messages should finish by call to Flush() or by destruction
/// of the object. One shouldn't start any new messages before he finishes
/// started one. After call to Flush() the same object can be reused to start
/// a new message. All messages will have request number from request context
/// assigned to currently executing task, except those methods that get
/// pointer to CRequestContext as input parameter (these methods should be
/// used very rarely).
/// CSrvDiagMsg objects cannot persist upon return to TaskServer code and
/// cannot be used in several threads simultaneously.
class CSrvDiagMsg
{
public:
    CSrvDiagMsg(void);
    ~CSrvDiagMsg(void);

    /// Severity levels for logging
    enum ESeverity {
        Trace,
        Info,
        Warning,
        Error,
        Critical,
        Fatal
    };

    /// Checks if given severity level is visible, i.e. will make its way to
    /// log file if someone will print anything on this level.
    static bool IsSeverityVisible(ESeverity sev);

    /// Starts log message which will include severity, filename, line number
    /// and function name.
    const CSrvDiagMsg& StartSrvLog(ESeverity sev,
                                   const char* file,
                                   int line,
                                   const char* func) const;
    /// Starts informational message which doesn't need to have filename, line
    /// number or function name.
    const CSrvDiagMsg& StartInfo(void) const;
    const CSrvDiagMsg& StartInfo(CRequestContext* ctx) const;
    /// Starts "request-start" message. Use PrintParam() methods to include
    /// parameters into the message.
    CSrvDiagMsg& StartRequest(void);
    CSrvDiagMsg& StartRequest(CRequestContext* ctx);
    /// Starts "extra" message. Use PrintParam() methods to include
    /// parameters into the message.
    CSrvDiagMsg& PrintExtra(void);
    CSrvDiagMsg& PrintExtra(CRequestContext* ctx);
    /// Prints "request-stop" message. This is the only method that doesn't
    /// start message, it prints the whole message which doesn't need to be
    /// manually finished.
    void StopRequest(void);
    void StopRequest(CRequestContext* ctx);
    /// Finishes current message and prepare to start new one.
    void Flush(void);
    /// Starts the "old style" log message. Method shouldn't be used by
    /// anybody except ERR_POST() macro used in legacy code.
    const CSrvDiagMsg& StartOldStyle(const char* file, int line, const char* func);

    /// Adds parameter to "request-start" or "extra" message.
    CSrvDiagMsg& PrintParam(CTempString name, CTempString value);
    CSrvDiagMsg& PrintParam(CTempString name, Int4 value);
    CSrvDiagMsg& PrintParam(CTempString name, Uint4 value);
    CSrvDiagMsg& PrintParam(CTempString name, Int8 value);
    CSrvDiagMsg& PrintParam(CTempString name, Uint8 value);
    CSrvDiagMsg& PrintParam(CTempString name, double value);

    /// Converts input value to string and adds to started log message.
    const CSrvDiagMsg& operator<< (CTempString str) const;
    const CSrvDiagMsg& operator<< (const string& str) const;
    const CSrvDiagMsg& operator<< (const char* str) const;
    const CSrvDiagMsg& operator<< (Int4 num) const;
    const CSrvDiagMsg& operator<< (Uint4 num) const;
    const CSrvDiagMsg& operator<< (Int8 num) const;
    const CSrvDiagMsg& operator<< (Uint8 num) const;
    const CSrvDiagMsg& operator<< (double num) const;
    const CSrvDiagMsg& operator<< (const void* ptr) const;
    const CSrvDiagMsg& operator<< (const exception& ex) const;

// Consider this section private as it's public for internal use only
// to minimize implementation-specific clutter in headers.
public:
    /// Current thread created this object.
    SSrvThread* m_Thr;
    /// Log data from current thread.
    SLogData* m_Data;
    /// Flag showing if "old style" message was started.
    bool m_OldStyle;
};


/// Macro to be used for printing log messages.
/// Macro usage is as follows:
/// SRV_LOG(Error, message << some_param << " more message");
#define SRV_LOG(sev, msg)                                       \
    do {                                                        \
        if (CSrvDiagMsg::IsSeverityVisible(CSrvDiagMsg::sev)) { \
            CSrvDiagMsg().StartSrvLog(CSrvDiagMsg::sev,         \
                                      __FILE__, __LINE__,       \
                                      NCBI_CURRENT_FUNCTION)    \
                << msg;                                         \
        }                                                       \
    }                                                           \
    while (0)                                                   \
/**/
#define SRV_FATAL(msg)                          \
    do {                                           \
        SRV_LOG(Fatal, "Fatal error: " << msg);    \
        abort();                                   \
    } while (0)                                    \

/// Macro to be used for printing informational messages.
/// Macro usage is as follows:
/// INFO(message << some_param << " more message");
#define INFO(msg)   CSrvDiagMsg().StartInfo() << msg


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_DIAG__HPP */
