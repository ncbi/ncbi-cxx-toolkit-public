#ifndef CASS_EXCEPTION__HPP
#define CASS_EXCEPTION__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Exceptions used in the cassandra driver
 *
 */

#include <cassandra.h>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiexpt.hpp>

#include "IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;


class CCassandraException: public CException
{
 public:
    enum EErrCode {
        eUnknown = 2000,
        eRsrcFailed,
        eFailedToConn,
        eConnTimeout,
        eQueryFailedRestartable,
        eQueryFailed,
        eBindFailed,
        eQueryTimeout,
        eFetchFailed,
        eExtraFetch,
        eConvFailed,
        eMissData,
        eInconsistentData,
        eNotFound,

        // Came from class EError (IdLogUtl.hpp)
        eSeqFailed,
        eFatal,
        eGeneric,
        eMemory,
        eUserCancelled
    };

    static CCassandraException s_ProduceException(CassFuture * future, CCassandraException::EErrCode error_code) {
        const char *message;
        size_t message_length;
        CassError rc = cass_future_error_code(future);
        cass_future_error_message(future, &message, &message_length);
        string msg(message, message_length);
        msg.append(": ").append(NStr::NumericToString(static_cast<int>(rc), 0, 16));
        CCassandraException result = NCBI_EXCEPTION(CCassandraException, eUnknown, msg);
        result.SetErrorCode(error_code);
        return result;
    }

    void SetErrorCode(EErrCode error_code)
    {
        x_InitErrCode(static_cast<CException::EErrCode>(error_code));
    }

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
            case eUnknown:                return "eUnknown";
            case eRsrcFailed:             return "eRsrcFailed";
            case eFailedToConn:           return "eFailedToConn";
            case eConnTimeout:            return "eConnTimeout";
            case eQueryFailedRestartable: return "eQueryFailedRestartable";
            case eQueryFailed:            return "eQueryFailed";
            case eBindFailed:             return "eBindFailed";
            case eQueryTimeout:           return "eQueryTimeout";
            case eFetchFailed:            return "eFetchFailed";
            case eExtraFetch:             return "eExtraFetch";
            case eConvFailed:             return "eConvFailed";
            case eMissData:               return "eMissData";
            case eInconsistentData:       return "eInconsistentData";
            case eNotFound:               return "eNotFound";
            case eSeqFailed:              return "eSeqFailed";
            case eFatal:                  return "eFatal";
            case eGeneric:                return "eGeneric";
            case eMemory:                 return "eMemory";
            case eUserCancelled:          return "eUserCancelled";
            default:                      return CException::GetErrCodeString();
        }
    }

    void SetOpTime(int64_t optimeMS)
    {
        m_OpTimeMs = optimeMS;
    }

    int64_t GetOpTime(void) const
    {
        return m_OpTimeMs;
    }

    string TimeoutMsg(void) const
    {
        return "Failed to perform query in " +
               NStr::NumericToString(m_OpTimeMs) + "ms, timed out";
    }

    NCBI_EXCEPTION_DEFAULT(CCassandraException, CException);

 protected:
    virtual void x_Init(const CDiagCompileInfo &  info,
                        const string &  message,
                        const CException *  prev_exception,
                        EDiagSev  severity)
    {
        m_OpTimeMs = 0;
        ERR_POST(Info << "CCassandraException: " << message);
        CException::x_Init(info, message, prev_exception, severity);
    }

    virtual void x_Assign(const CException &  src)
    {
        const CCassandraException* _src = dynamic_cast<const CCassandraException*>(&src);
        if (_src) {
            m_OpTimeMs = _src->m_OpTimeMs;
        }
        CException::x_Assign(src);
    }

    int64_t m_OpTimeMs{0};
};



#define RAISE_CASS_ERROR(errc, dberr, comm)                                 \
    do {                                                                    \
        string __msg = comm;                                                \
        string __c = NStr::NumericToString(static_cast<int>(errc), 0, 16);  \
        __msg.append(" Cassandra error - (code: " + __c);                   \
        __msg.append(string(", description: '") +                           \
            cass_error_desc(errc) + "')");                                  \
        NCBI_THROW(CCassandraException, dberr, __msg.c_str());              \
    } while (0)

#define RAISE_CASS_FUT_ERROR(future, errc, comm)                            \
    do {                                                                    \
        const char *__message;                                              \
        size_t __msglen;                                                    \
        cass_future_error_message(future, &__message, &__msglen);           \
        CassError rc = cass_future_error_code(future);                      \
        string __msg;                                                       \
        __msg.assign(__message);                                            \
        __msg.append(string(": ") +                                         \
                     NStr::NumericToString(static_cast<int>(rc), 0, 16));   \
        if (comm.empty())                                                   \
            NCBI_THROW(CCassandraException, errc, __msg);                   \
        else                                                                \
            NCBI_THROW(CCassandraException, errc, __msg + ": " + comm);     \
    } while (0)

#define RAISE_CASS_QRY_ERROR(future, comm)                                  \
    do {                                                                    \
        CassError rc = cass_future_error_code(future);                      \
        if (rc == CASS_ERROR_SERVER_UNAVAILABLE ||                          \
            rc == CASS_ERROR_LIB_REQUEST_QUEUE_FULL ||                      \
            rc == CASS_ERROR_LIB_NO_HOSTS_AVAILABLE)                        \
            RAISE_CASS_FUT_ERROR(future, eQueryFailedRestartable, comm);    \
        else if (rc == CASS_ERROR_LIB_REQUEST_TIMED_OUT ||                  \
                rc == CASS_ERROR_SERVER_WRITE_TIMEOUT ||                    \
                rc == CASS_ERROR_SERVER_READ_TIMEOUT)                       \
            RAISE_CASS_FUT_ERROR(future, eQueryTimeout, comm);              \
        else                                                                \
            RAISE_CASS_FUT_ERROR(future, eQueryFailed, comm);               \
    } while (0)

#define RAISE_CASS_CONN_ERROR(future, comm)                                 \
    RAISE_CASS_FUT_ERROR(future, eFailedToConn, comm)

#define RAISE_DB_ERROR(errc, comm)                                          \
    do {                                                                    \
        string ___msg = comm;                                               \
        NCBI_THROW(CCassandraException, errc, comm);                        \
    } while(0)

#define RAISE_DB_QRY_TIMEOUT(tspent, tmargin, msg)                          \
    do {                                                                    \
        string ___msg = msg;                                                \
        if ((tmargin) != 0)                                                 \
            ___msg.append(string(", timeout ") +                            \
                          NStr::NumericToString(tmargin) +                  \
                          "ms (spent: " + NStr::NumericToString(tspent) +   \
                          "ms)");                                           \
        NCBI_EXCEPTION_VAR(db_exception, CCassandraException, eQueryTimeout, ___msg); \
        db_exception.SetOpTime( (tspent) );                                 \
        NCBI_EXCEPTION_THROW(db_exception);                                 \
    } while(0)




END_IDBLOB_SCOPE

#endif
