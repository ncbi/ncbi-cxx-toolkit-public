/*
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
 * Authors:  Dmitriy Elisov
 * @file
 * File Description:
 *   C++ Wrapper for the LBOS mapper written in C
 */
#include <ncbi_pch.hpp>
#include "ncbi_lbosp.h"
#include <corelib/ncbimisc.hpp>
#include <connect/ncbi_lbos.hpp>

BEGIN_NCBI_SCOPE


/// Functor template for deleting object.
template<class X>
struct Free
{
    /// Default delete function.
    static void Delete(X* object)
    {
        free(*object);
    }
};

static void s_ProcessResult(ELBOS_Result result, const char* lbos_answer) 
{
    switch (result) {
    case eLBOS_Success:
        return;
    case eLBOS_ServerError:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::e_ServerError, string("LBOS returned error: ")
            + string(lbos_answer));
    case eLBOS_NoLBOS:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::e_NoLBOS, string("No LBOS was found"));
    case eLBOS_NotFound:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::e_NotFound, string("No LBOS was found"));
    case eLBOS_Off:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::e_Off, string("LBOS mapper is turned OFF. "
            "It is either because it could not resolve even itself, or "
            "because config does not have [CONN]LBOS_ENABLE=1"));
    case eLBOS_InvalidArgs:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::e_InvalidArgs, string("Some arguments provided "
            "are invalid"));
    case eLBOS_DNSResolveError:
        throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::e_DNSResolveError, string("LBOS mapper did not "
            "manage to resolve IP of current machine to replace \"0.0.0.0\""));
    }
}


void LBOS::Announce(const string& service, const string& version, 
                    unsigned short port, const string& healthcheck_url)
{
    char* str = NULL;
    AutoPtr<char*, Free<char*>> lbos_answer(&str);
    ELBOS_Result result = LBOS_Announce(service.c_str(), version.c_str(),
        port, healthcheck_url.c_str(), &*lbos_answer);
    s_ProcessResult(result, *lbos_answer);
}


void LBOS::AnnounceFromRegistry(const string&  registry_section)
{
    char* str = NULL;
    AutoPtr<char*, Free<char*>> lbos_answer(&str);
    ELBOS_Result result = LBOS_AnnounceFromRegistry(registry_section.c_str(),
                                                    &*lbos_answer);
    s_ProcessResult(result, *lbos_answer);
}


void LBOS::DeannounceAll(void)
{
    LBOS_DeannounceAll();
}


void LBOS::Deannounce(const string&         service,
                      const string&         version,
                      const string&         host,
                      unsigned short        port)
{
    ELBOS_Result result =
        LBOS_Deannounce(service.c_str(), version.c_str(), host.c_str(), port);
    s_ProcessResult(result, "");
}


const char* CLBOSException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case e_NoLBOS:
        return "LBOS was not found";
    case e_DNSResolveError:
        return "Could not get IP of current machine";
    case e_InvalidArgs:
        return "Some provided arguments are invalid";
    case e_ServerError:
        return "LBOS returned error message";
    default:
        return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
