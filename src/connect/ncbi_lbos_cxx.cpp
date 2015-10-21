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
/* C++ */
#include <ncbi_pch.hpp>
#include <sstream>
#include <corelib/ncbimisc.hpp>
#include <connect/ncbi_lbos.hpp>
/* C */
#include "ncbi_lbosp.h"


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

static void s_ProcessResult(unsigned short result, 
                            const char* lbos_answer,
                            const char* status_message)
{
    if (result == kLBOSSuccess)
        return;

    stringstream message;
    message << result;

    if (status_message == NULL)
        status_message = "";
    else {
        message << " " << status_message;
    }
    if (lbos_answer == NULL)
        lbos_answer = "";
    else {
        message << " " << lbos_answer;
    }
    throw CLBOSException(CDiagCompileInfo(__FILE__, __LINE__), NULL,
            CLBOSException::s_HTTPCodeToEnum(result),
            message.str(), result);
}


void LBOS::Announce(const string& service, const string& version,
                    unsigned short port, const string& healthcheck_url)
{
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    unsigned short result = LBOS_Announce(service.c_str(), version.c_str(),
        port, healthcheck_url.c_str(), &*body_aptr, &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
}


void LBOS::AnnounceFromRegistry(const string&  registry_section)
{
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
                                  status_message_aptr(&status_message_str);
    unsigned short result = LBOS_AnnounceFromRegistry(registry_section.c_str(),
                                                      &*body_aptr,
                                                      &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
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
    char* body_str = NULL, *status_message_str = NULL;
    AutoPtr< char*, Free<char*> > body_aptr(&body_str),
        status_message_aptr(&status_message_str);
    unsigned short result = LBOS_Deannounce(service.c_str(), version.c_str(), 
                                            host.c_str(), port, &*body_aptr, 
                                            &*status_message_aptr);
    s_ProcessResult(result, *body_aptr, *status_message_aptr);
}


CLBOSException::EErrCode 
    CLBOSException::s_HTTPCodeToEnum(unsigned short http_code) 
{
    switch (http_code) {
    case kLBOSNoLBOS:
        return e_NoLBOS;
    case kLBOSNotFound:
        return e_NotFound;
    case kLBOSBadRequest:
        return e_BadRequest;
    case kLBOSOff:
        return e_Off;
    case kLBOSInvalidArgs:
        return e_InvalidArgs;
    case kLBOSDNSResolveError:
        return e_DNSResolveError;
    case kLBOSMemAllocError:
        return e_MemAllocError;
    case kLBOSCorruptOutput:
        return e_LBOSCorruptOutput;
    default:
        return e_Unknown;
    }
}


unsigned short CLBOSException::GetStatusCode(void) const {
    return m_StatusCode;
}


const char* CLBOSException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case e_NoLBOS:
        return "LBOS was not found";
    case e_NotFound:
        return "Healthcheck URL is not working or lbos could not resolve"
               "host of healthcheck URL";
    case e_DNSResolveError:
        return "Could not get IP of current machine";
    case e_MemAllocError:
        return "Memory allocation error happened while performing request";
    case e_Off:
        return "lbos functionality is turned OFF. Check config file.";
    case e_InvalidArgs:
        return "Invalid arguments were provided. No request to lbos was sent";
    default:
        return "Unknown lbos error code";
    }
}

END_NCBI_SCOPE
