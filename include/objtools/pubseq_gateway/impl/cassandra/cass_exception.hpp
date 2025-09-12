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

class CCassandraException
    : public CException
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

    void SetErrorCode(EErrCode error_code)
    {
        x_InitErrCode(static_cast<CException::EErrCode>(error_code));
    }

    const char* GetErrCodeString(void) const override
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

    NCBI_EXCEPTION_DEFAULT(CCassandraException, CException);

protected:
    void x_Init(const CDiagCompileInfo & info,
                const string & message,
                const CException * prev_exception,
                EDiagSev severity) override
    {
        m_OpTimeMs = 0;
        ERR_POST(Info << "CCassandraException: " << message);
        CException::x_Init(info, message, prev_exception, severity);
    }

    void x_Assign(const CException & src) override
    {
        const CCassandraException* src_ex = dynamic_cast<const CCassandraException*>(&src);
        if (src_ex) {
            m_OpTimeMs = src_ex->m_OpTimeMs;
        }
        CException::x_Assign(src);
    }

    int64_t m_OpTimeMs{0};
};

#define RAISE_CASS_ERROR(errc, dberr, comm)                                     \
    do {                                                                        \
        string macro_msg = comm;                                                \
        string macro_c = NStr::NumericToString(static_cast<int>(errc), 0, 16);  \
        macro_msg.append(" Cassandra error - (code: " + macro_c);               \
        macro_msg.append(string(", description: '") +                           \
            cass_error_desc(errc) + "')");                                      \
        NCBI_THROW(CCassandraException, dberr, macro_msg.c_str());              \
    } while (0)

#define RAISE_DB_ERROR(errc, comm)                                              \
    NCBI_THROW(CCassandraException, errc, comm);                                \

END_IDBLOB_SCOPE

#endif  // CASS_EXCEPTION__HPP
