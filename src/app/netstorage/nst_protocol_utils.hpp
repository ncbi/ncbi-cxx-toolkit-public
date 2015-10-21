#ifndef NETSTORAGE_PROTOCOL_UTILS__HPP
#define NETSTORAGE_PROTOCOL_UTILS__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description: NetStorage communication protocol utils
 *
 */


#include <string>
#include <connect/services/json_over_uttp.hpp>
#include <misc/netstorage/impl.hpp>


BEGIN_NCBI_SCOPE

class CSocket;


const string    kStatusOK = "OK";
const string    kStatusError = "ERROR";
const string    kMessageTypeReply = "REPLY";



// Stores the parsed common fields of the incoming messages
struct SCommonRequestArguments
{
    SCommonRequestArguments() :
        m_SerialNumber(-1)
    {}

    string      m_MessageType;
    Int8        m_SerialNumber;
};


// ICache settings which may appear in various requests
struct SICacheSettings
{
    string      m_ServiceName;
    string      m_CacheName;
};


// User key which may appear in various requests
struct SUserKey
{
    string      m_UniqueID;
    string      m_AppDomain;
};



void SetSessionAndIP(const CJsonNode &  message,
                     const CSocket &    peer);

SCommonRequestArguments
ExtractCommonFields(const CJsonNode &  message);

TNetStorageFlags
ExtractStorageFlags(const CJsonNode &  message);

SICacheSettings
ExtractICacheSettings(const CJsonNode &  message);

SUserKey
ExtractUserKey(const CJsonNode &  message);


CJsonNode
CreateResponseMessage(Int8  serial_number);

CJsonNode
CreateErrorResponseMessage(Int8  serial_number,
                           Int8  error_code,
                           const string &  error_message);

void
AppendWarning(CJsonNode &     message,
              Int8            code,
              const string &  warning_message);

void
AppendError(CJsonNode &     message,
            Int8            code,
            const string &  error_message);



END_NCBI_SCOPE

#endif

