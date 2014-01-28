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

#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>
#include <connect/ncbi_socket.hpp>

#include "nst_protocol_utils.hpp"
#include "nst_exception.hpp"



BEGIN_NCBI_SCOPE


void SetSessionAndIP(const CJsonNode &  message,
                     const CSocket &    peer)
{
    if (message.HasKey("SessionID")) {
        string  session = message.GetString("SessionID");
        if (!session.empty())
            CDiagContext::GetRequestContext().SetSessionID(
                    session);
    }


    string      client_ip;
    if (message.HasKey("ClientIP"))
        client_ip = message.GetString("ClientIP");

    if (client_ip.empty())
        CDiagContext::GetRequestContext().SetClientIP(
                                peer.GetPeerAddress(eSAF_IP));
    else
        CDiagContext::GetRequestContext().SetClientIP(
                                client_ip);
}


SCommonRequestArguments
ExtractCommonFields(const CJsonNode &  message)
{
    SCommonRequestArguments     result;
    try {
        result.m_SerialNumber = message.GetInteger("SN");
        result.m_MessageType = message.GetString("Type");
    }
    catch (const std::exception & ex) {
        // Converting to the CNetStorageServerException is done to
        // have generic request status handling.
        NCBI_THROW(CNetStorageServerException, eInvalidIncomingMessage,
                   ex.what());
    }
    return result;
}


TNetStorageFlags
ExtractStorageFlags(const CJsonNode &  message)
{
    TNetStorageFlags    result = 0;

    if (message.HasKey("StorageFlags")) {
        CJsonNode   flags = message.GetByKey("StorageFlags");
        if (flags.HasKey("Fast") && flags.GetBoolean("Fast"))
            result |= fNST_Fast;
        if (flags.HasKey("Persistent") && flags.GetBoolean("Persistent"))
            result |= fNST_Persistent;
        if (flags.HasKey("Movable") && flags.GetBoolean("Movable"))
            result |= fNST_Movable;
        if (flags.HasKey("Cacheable") && flags.GetBoolean("Cacheable"))
            result |= fNST_Cacheable;
        if (flags.HasKey("NoMetaData") && flags.GetBoolean("NoMetaData"))
            result |= fNST_NoMetaData;
    }
    return result;
}


SICacheSettings
ExtractICacheSettings(const CJsonNode &  message)
{
    SICacheSettings     result;

    if (message.HasKey("ICache")) {
        CJsonNode   settings = message.GetByKey("ICache");
        if (settings.HasKey("ServiceName"))
            result.m_ServiceName = settings.GetString("ServiceName");
        if (settings.HasKey("CacheName"))
            result.m_CacheName = settings.GetString("CacheName");
    }
    return result;
}


SUserKey
ExtractUserKey(const CJsonNode &  message)
{
    SUserKey        result;

    if (message.HasKey("UserKey")) {
        CJsonNode   user_key = message.GetByKey("UserKey");
        if (user_key.HasKey("UniqueID"))
            result.m_UniqueID = user_key.GetString("UniqueID");
        if (user_key.HasKey("AppDomain"))
            result.m_AppDomain = user_key.GetString("AppDomain");
    }
    return result;
}


CJsonNode
CreateResponseMessage(Int8  serial_number)
{
    CJsonNode       reply_message(CJsonNode::NewObjectNode());

    reply_message.SetString("Type", kMessageTypeReply);
    reply_message.SetString("Status", kStatusOK);
    reply_message.SetInteger("RE", serial_number);

    return reply_message;
}


CJsonNode
CreateErrorResponseMessage(Int8            serial_number,
                           Int8            error_code,
                           const string &  error_message)
{
    CJsonNode       reply_message(CJsonNode::NewObjectNode());
    CJsonNode       errors(CJsonNode::NewArrayNode());
    CJsonNode       error_node(CJsonNode::NewObjectNode());

    reply_message.SetString("Type", kMessageTypeReply);
    reply_message.SetString("Status", kStatusError);
    reply_message.SetInteger("RE", serial_number);


    error_node.SetInteger("Code", error_code);
    error_node.SetString("Message", error_message);
    errors.Append(error_node);

    reply_message.SetByKey("Errors", errors);

    return reply_message;
}


void
AppendWarning(CJsonNode &     message,
              Int8            code,
              const string &  warning_message)
{
    CJsonNode   warnings;
    if (!message.HasKey("Warnings"))
        message.SetByKey("Warnings", warnings = CJsonNode::NewArrayNode());
    else
        warnings = message.GetByKey("Warnings");

    CJsonNode   warning_node(CJsonNode::NewObjectNode());
    warning_node.SetInteger("Code", code);
    warning_node.SetString("Message", warning_message);

    warnings.Append(warning_node);
}


void
AppendError(CJsonNode &     message,
            Int8            code,
            const string &  error_message)
{
    message.SetString("Status", kStatusError);

    CJsonNode   errors;
    if (!message.HasKey("Errors"))
        message.SetByKey("Errors", errors = CJsonNode::NewArrayNode());
    else
        errors = message.GetByKey("Errors");

    CJsonNode   error_node(CJsonNode::NewObjectNode());
    error_node.SetInteger("Code", code);
    error_node.SetString("Message", error_message);

    errors.Append(error_node);
}

END_NCBI_SCOPE

