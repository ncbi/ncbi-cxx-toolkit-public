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


#define _VERIFY(x)  if (x) {} else abort()

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


class CSrvDiagMsg
{
public:
    CSrvDiagMsg(void);
    ~CSrvDiagMsg(void);

    enum ESeverity {
        Trace,
        Info,
        Warning,
        Error,
        Critical,
        Fatal
    };

    static bool IsSeverityVisible(ESeverity sev);

    const CSrvDiagMsg& StartSrvLog(ESeverity sev,
                                   const char* file,
                                   int line,
                                   const char* func) const;
    const CSrvDiagMsg& StartInfo(void) const;
    const CSrvDiagMsg& StartInfo(CRequestContext* ctx) const;
    CSrvDiagMsg& StartRequest(void);
    CSrvDiagMsg& StartRequest(CRequestContext* ctx);
    CSrvDiagMsg& PrintExtra(void);
    CSrvDiagMsg& PrintExtra(CRequestContext* ctx);
    void StopRequest(void);
    void StopRequest(CRequestContext* ctx);
    void Flush(void);
    const CSrvDiagMsg& StartOldStyle(const char* file, int line, const char* func);

    CSrvDiagMsg& PrintParam(CTempString name, CTempString value);
    CSrvDiagMsg& PrintParam(CTempString name, Int4 value);
    CSrvDiagMsg& PrintParam(CTempString name, Uint4 value);
    CSrvDiagMsg& PrintParam(CTempString name, Int8 value);
    CSrvDiagMsg& PrintParam(CTempString name, Uint8 value);
    CSrvDiagMsg& PrintParam(CTempString name, double value);

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

public:
    SSrvThread* m_Thr;
    SLogData* m_Data;
    bool m_OldStyle;
};


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

#define INFO(msg)   CSrvDiagMsg().StartInfo() << msg


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_DIAG__HPP */
