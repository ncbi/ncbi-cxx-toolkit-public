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
 * Author: Pavel Ivanov
 *
 */

#include "nc_pch.hpp"

#include <corelib/ncbireg.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <util/md5.hpp>

#include <connect/services/netcache_key.hpp>
#include <connect/services/netcache_api_expt.hpp>

#include "message_handler.hpp"
#include "netcache_version.hpp"
#include "nc_stat.hpp"
#include "peer_control.hpp"
#include "distribution_conf.hpp"
#include "periodic_sync.hpp"
#include "active_handler.hpp"
#include "nc_storage.hpp"
#include "netcached.hpp"
#include "nc_storage_blob.hpp"



BEGIN_NCBI_SCOPE

#if 0
#define LOG_CURRENT_FUNCTION SRV_LOG(Warning, "this: " << (void*)this);
#else
#define LOG_CURRENT_FUNCTION
#endif

/// Definition of all NetCache commands
/// 
/// General format of a "NetCache" command is as follows:
/// 
/// CMD param1 param2 ...
/// 
/// Format of "ICache" command is as follows:
/// 
/// IC(cache) CMD param1 param2 ...
/// 
/// Here "IC" is two letters that appear in command literally. "cache" is name
/// of the cache where blob is stored; it's mentioned in parameter list as
/// first parameter with type eNSPA_ICPrefix. Every command parameter can be
/// given as just value or as name=value pair. String parameter values can be
/// enclosed in double quotes. If parameter is declared with the flag
/// eNSPA_Optional then it can be skipped from the command, in this case its
/// default value will be used (if any). If parameter flag eNSPA_Optchain has
/// the same meaning except if it's not provided then all following parameters
/// marked as eNSPA_Optional will be assumed not provided too.
/// 
/// If command needs some binary data along with it then it's sent split in
/// chunks each having 4-byte integer prefix containing the length of the chunk.
/// When all data is sent special chunk length 0xFFFFFFFF should be sent
/// at the end. Successful response of each command is sent as one line
/// starting with "OK:" and then space-separated parameters that need to be
/// returned. If response should contain binary data then initial response
/// line should have "SIZE=nnn" with the size of binary data to follow.
/// Unsuccessful responses to commands always start with "ERR:" and then error
/// explanation follows.
/// 
/// Descriptions of commands have
///  - command name as it comes from client;
///  - structure containing
///    * state function processing this command;
///    * command name as it appears in statistics;
///    * flags controlling command behavior;
///    * type of access to blob if needed;
///    * type of proxy command that should be executed if command will need to
///      be proxied to other servers;
///  - set of structures explaining command parameters. Each structure has
///    * parameter name which can be used by client if it passes parameters in
///      name=value form. Also name is used to distinguish parameters in
///      x_AssignCmdParams();
///    * type of parameter - parser makes additional checks to see if given
///      value is applicable for necessary parameter type;
///    * parameter flags.
static CNCMessageHandler::SCommandDef s_CommandMap[] = {
    // "Are you alive?" command. This is old and deprecated command but it's
    // executed a lot in old ICache clients. All that they need in response is
    // "OK:".
    { "A?",
        {&CNCMessageHandler::x_FinishCommand,
            "A?",
            fConfirmOnFinish} },
    // Requests version of the server.
    { "VERSION", {&CNCMessageHandler::x_DoCmd_Version, "VERSION"} },
    // Requests some "health" information about the server.
    { "HEALTH",  {&CNCMessageHandler::x_DoCmd_Health,  "HEALTH"} },
    // Check if blob exists. Command for "ICache" clients.
    { "HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "IC_HASB",
            eClientBlobRead + fPeerFindExistsOnly,
            eNCRead,
            eProxyHasBlob},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Read blob contents. Command for "ICache" clients.
    { "READ",
        {&CNCMessageHandler::x_DoCmd_Get,
            "IC_READ",
            eClientBlobRead,
            eNCReadData,
            eProxyRead},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional },
          // Max age of blob (returned blob should be younger)
          { "age",     eNSPT_Int,  eNSPA_Optional } } },
    // Write blob contents. Command for "ICache" clients.
    { "STOR",
        {&CNCMessageHandler::x_DoCmd_IC_Store,
            "IC_STOR",
            eClientBlobWrite,
            eNCCreate,
            eProxyWrite},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Time-to-live for the blob, 0 means default from server settings.
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // 1 if client wants confirmation after blob has been written
          // (by default it just assumes that everything is written and moves
          // further).
          { "confirm", eNSPT_Int,  eNSPA_Optional },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Write blob contents. Old and deprecated command which probably is not
    // used by modern ICache clients anymore. It has the size of the blob right
    // in the command (so client should know it beforehand) and it doesn't use
    // "EOF" marker at the end of blob data.
    { "STRS",
        {&CNCMessageHandler::x_DoCmd_IC_Store,
            "IC_STRS",
            eClientBlobWrite + fReadExactBlobSize + fSkipBlobEOF,
            eNCCreate,
            eProxyWrite},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Time-to-live for the blob, 0 means default from server settings.
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          // Size of the blob to be written.
          { "size",    eNSPT_Int,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Read all or part of contents of the "last" version of the blob.
    // In response to the command NC sends blob contents, blob version it has
    // and flag showing if this version can be considered "valid", i.e. if it's
    // not expired yet.
    { "READLAST",
        {&CNCMessageHandler::x_DoCmd_GetLast,
            "IC_READLAST",
            eClientBlobRead + fNoBlobVersionCheck,
            eNCReadData,
            eProxyReadLast},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Starting position of the data that needs to be sent.
          { "start",   eNSPT_Int,  eNSPA_Optional },
          // Size of the data that needs to be sent.
          { "size",    eNSPT_Int,  eNSPA_Optional },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional },
          // Max age of blob (returned blob should be younger)
          { "age",     eNSPT_Int,  eNSPA_Optional } } },
    // Mark the given blob version as "valid" and do that only if this version
    // is still current and wasn't rewritten with another version.
    { "SETVALID",
        {&CNCMessageHandler::x_DoCmd_SetValid,
            "IC_SETVALID",
            fNeedsBlobAccess,
            eNCRead,
            eProxySetValid},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Write blob contents. Command is issued only by other servers while
    // mirroring just written blobs or processing quorum requirements,
    // i.e. writing to other servers before answering to client that blob is
    // written.
    { "COPY_PUT",
        {&CNCMessageHandler::x_DoCmd_CopyPut,
            "COPY_PUT",
            eCopyBlobFromPeer + fNeedsSpaceAsPeer + fReadExactBlobSize
                              + fCopyLogEvent,
            eNCCopyCreate},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Blob's version (for NC-generated blob keys this will be equal to 0).
          { "version", eNSPT_Int,  eNSPA_Required },
          // MD5 checksum of blob's password.
          { "md5_pass",eNSPT_Str,  eNSPA_Required },
          // Creation time of the blob (microseconds since epoch).
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          // Time-to-live for the blob.
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          // Dead-time for the blob (can be greater than expiration time).
          { "dead",    eNSPT_Int,  eNSPA_Required },
          // Expiration time for the blob
          { "exp",     eNSPT_Int,  eNSPA_Required },
          // Blob size.
          { "size",    eNSPT_Int,  eNSPA_Required },
          // Time-to-live for blob's version.
          { "ver_ttl", eNSPT_Int,  eNSPA_Required },
          // Blob's version expiration time.
          { "ver_dead",eNSPT_Int,  eNSPA_Required },
          // Server_id of the server where blob was created.
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          // Id of the blob on the server where it was created.
          { "cr_id",   eNSPT_Int,  eNSPA_Required },
          // Record number of the event of blob creation in synchronization
          // logs of the server where blob was created.
          { "log_rec", eNSPT_Int,  eNSPA_Required },
          // Version of the command. Field exists for protocol backwards
          // compatibility with previous versions of NC. In current NC this
          // version is always 1.
          { "cmd_ver", eNSPT_Int,  eNSPA_Optional, "0" },
          // Client IP for application that requested writing the blob.
          // Parameter is not empty only if command is issued as part of
          // quorum-related functionality, i.e. before client received
          // confirmation of blob writing.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application that requested writing the blob.
          // Parameter is not empty only if command is issued as part of
          // quorum-related functionality, i.e. before client received
          // confirmation of blob writing.
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    // Prolong blob lifetime. Command is issued only by other servers while
    // mirroring prolonged blobs.
    { "COPY_PROLONG",
        {&CNCMessageHandler::x_DoCmd_CopyProlong,
            "COPY_PROLONG",
            eCopyBlobFromPeer,
            eNCRead},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Creation time of the blob (microseconds since epoch).
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          // Server_id of the server where blob was created.
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          // Id of the blob on the server where it was created.
          { "cr_id",   eNSPT_Int,  eNSPA_Required },
          // Dead-time for the blob (can be greater than expiration time).
          { "dead",    eNSPT_Int,  eNSPA_Required },
          // Expiration time for the blob
          { "exp",     eNSPT_Int,  eNSPA_Required },
          // Blob's version expiration time.
          { "ver_dead",eNSPT_Int,  eNSPA_Required },
          // Time of creation of initial record in synchronization log about
          // this operation.
          { "log_time",eNSPT_Int,  eNSPA_Optchain },
          // Server that first made the blob's life prolongation.
          { "log_srv", eNSPT_Int,  eNSPA_Optional },
          // Record number of the initial record in synchronization log about
          // this operation.
          { "log_rec", eNSPT_Int,  eNSPA_Optional } } },
    // Write blob contents. Command for "NetCache" clients.
    { "PUT3",
        {&CNCMessageHandler::x_DoCmd_Put,
            "PUT3",
            eClientBlobWrite + fCanGenerateKey + fConfirmOnFinish,
            eNCCreate,
            eProxyWrite},
          // Time-to-live for the blob. If not given or 0 then default TTL
          // is used.
        { { "ttl",     eNSPT_Int,  eNSPA_Optional },
          // Key of the blob. If it's not given or empty then new key will be
          // generated.
          { "key",     eNSPT_NCID, eNSPA_Optional },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Read blob contents. Command for "NetCache" clients.
    { "GET2",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GET2",
            eClientBlobRead,
            eNCReadData,
            eProxyRead},
          // Key of the blob.
        { { "key",     eNSPT_NCID, eNSPA_Required },
          // Not used and not implemented parameter. Exists just for backwards
          // compatibility with old clients.
          { "NW",      eNSPT_Id,   eNSPA_Obsolete | fNSPA_Match },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional },
          // Max age of blob (returned blob should be younger)
          { "age",     eNSPT_Int,  eNSPA_Optional } } },
    // Check if blob exists. Command for "NetCache" clients.
    { "HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "HASB",
            eClientBlobRead + fPeerFindExistsOnly,
            eNCRead,
            eProxyHasBlob},
          // Key of the blob.
        { { "key",     eNSPT_NCID, eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Delete blob. Command for "NetCache" clients.
    // If the blob doesn't exist command is still considered successful.
    { "RMV2",
        {&CNCMessageHandler::x_DoCmd_Remove,
            "RMV2",
            fNeedsBlobAccess + fConfirmOnFinish + fNoBlobAccessStats,
            eNCCreate,
            eProxyRemove},
          // Key of the blob.
        { { "key",     eNSPT_NCID, eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Get size of the blob. Command for "NetCache" clients.
    { "GSIZ",
        {&CNCMessageHandler::x_DoCmd_GetSize,
            "GetSIZe",
            eClientBlobRead,
            eNCRead,
            eProxyGetSize},
          // Key of the blob.
        { { "key",     eNSPT_NCID, eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Delete blob. Command for "ICache" clients.
    // If the blob doesn't exist command is still considered successful.
    { "REMO",
        {&CNCMessageHandler::x_DoCmd_Remove,
            "IC_REMOve",
            fNeedsBlobAccess + fConfirmOnFinish + fNoBlobAccessStats,
            eNCCreate,
            eProxyRemove},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Get size of the blob. Command for "ICache" clients.
    { "GSIZ",
        {&CNCMessageHandler::x_DoCmd_GetSize,
            "IC_GetSIZe",
            eClientBlobRead,
            eNCRead,
            eProxyGetSize},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Read part of the blob contents. Command for "NetCache" clients.
    { "GETPART",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GETPART",
            eClientBlobRead,
            eNCReadData,
            eProxyRead},
          // Key of the blob.
        { { "key",     eNSPT_NCID, eNSPA_Required },
          // Starting position of the data that needs to be sent.
          { "start",   eNSPT_Int,  eNSPA_Required },
          // Size of the data that needs to be sent.
          { "size",    eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional },
          // Max age of blob (returned blob should be younger)
          { "age",     eNSPT_Int,  eNSPA_Optional } } },
    // Read part of the blob contents. Command for "ICache" clients.
    { "READPART",
        {&CNCMessageHandler::x_DoCmd_Get,
            "IC_READPART",
            eClientBlobRead,
            eNCReadData,
            eProxyRead},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Starting position of the data that needs to be sent.
          { "start",   eNSPT_Int,  eNSPA_Required },
          // Size of the data that needs to be sent.
          { "size",    eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional },
          // Max age of blob (returned blob should be younger)
          { "age",     eNSPT_Int,  eNSPA_Optional } } },
    // Get meta information about the blob. This command is sent only by other
    // NC servers. And it's used to determine which server has the latest
    // version of the blob. Thus response to this command is a line containing
    // enough information to compare the blob's creation time with the same
    // blob on other servers.
    { "PROXY_META",
        {&CNCMessageHandler::x_DoCmd_ProxyMeta,
            "PROXY_META",
            fNeedsBlobAccess + fNeedsStorageCache + fDoNotProxyToPeers
                             + fDoNotCheckPassword,
            eNCRead},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Client IP for application on behalf of which the info is requested.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application on behalf of which the info is requested.
          { "sid",     eNSPT_Str,  eNSPA_Required } } },
    // Write the blob contents. This command is sent only by other NC servers
    // in response to client's PUT3 (or similar) command when the server where
    // client have sent initial command cannot execute it locally for any
    // reason (slot is not processed by that server or initial database caching
    // is not completed yet).
    { "PROXY_PUT",
        {&CNCMessageHandler::x_DoCmd_Put,
            "PROXY_PUT",
            eProxyBlobWrite + fConfirmOnFinish,
            eNCCreate,
            eProxyWrite},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Blob's version (for NC-generated blob keys this will be equal to 0).
          { "version", eNSPT_Int,  eNSPA_Required },
          // Time-to-live for the blob.
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Read all or a part of the blob contents. This command is sent only
    // by other NC servers in response to client's GET2 (or similar) command
    // when the server where client have sent initial command cannot execute it
    // locally for any reason (slot is not processed by that server, or initial
    // database caching is not completed yet, or it was determined that this
    // server has latest version of the blob).
    { "PROXY_GET",
        {&CNCMessageHandler::x_DoCmd_Get,
            "PROXY_GET",
            eProxyBlobRead,
            eNCReadData,
            eProxyRead},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Blob's version (for NC-generated blob keys this will be equal to 0).
          { "version", eNSPT_Int,  eNSPA_Required },
          // Starting position of the data that needs to be sent.
          { "start",   eNSPT_Int,  eNSPA_Required },
          // Size of the data that needs to be sent.
          { "size",    eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          // Value of the flag "search_on_read" to use with this command, i.e.
          // whether this server should search the blob if it's not found
          // locally.
          { "srch",    eNSPT_Int,  eNSPA_Required },
          // Flag whether local execution of this command should be forced
          // no matter what, i.e. if this flag is set in cases when normal GET2
          // command would have been proxied to other servers an error should
          // be returned.
          { "local",   eNSPT_Int,  eNSPA_Required },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional },
          // Max age of blob (returned blob should be younger)
          { "age",     eNSPT_Int,  eNSPA_Optional } } },
    // Check if the blob exists. This command is sent only by other NC servers
    // in response to client's HASB command when the server where
    // client have sent initial command cannot execute it locally for any
    // reason (slot is not processed by that server or initial database caching
    // is not completed yet).
    { "PROXY_HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "PROXY_HASB",
            eProxyBlobRead + fPeerFindExistsOnly,
            eNCRead,
            eProxyHasBlob},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Get size of the blob. This command is sent only
    // by other NC servers in response to client's GSIZ command
    // when the server where client have sent initial command cannot execute it
    // locally for any reason (slot is not processed by that server, or initial
    // database caching is not completed yet, or it was determined that this
    // server has latest version of the blob).
    { "PROXY_GSIZ",
        {&CNCMessageHandler::x_DoCmd_GetSize,
            "PROXY_GetSIZe",
            eProxyBlobRead,
            eNCRead,
            eProxyGetSize},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Blob's version (for NC-generated blob keys this will be equal to 0).
          { "version", eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          // Value of the flag "search_on_read" to use with this command, i.e.
          // whether this server should search the blob if it's not found
          // locally.
          { "srch",    eNSPT_Int,  eNSPA_Required },
          // Flag whether local execution of this command should be forced
          // no matter what, i.e. if this flag is set in cases when normal GET2
          // command would have been proxied to other servers an error should
          // be returned.
          { "local",   eNSPT_Int,  eNSPA_Required },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Read all or a part of contents of the "last version" of the blob.
    // This command is sent only by other NC servers in response to client's
    // READLAST command when the server where client have sent initial command
    // cannot execute it locally for any reason (slot is not processed by that
    // server, or initial database caching is not completed yet, or it was
    // determined that this server has latest version of the blob).
    { "PROXY_READLAST",
        {&CNCMessageHandler::x_DoCmd_GetLast,
            "PROXY_READLAST",
            eProxyBlobRead + fNoBlobVersionCheck,
            eNCReadData,
            eProxyReadLast},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Starting position of the data that needs to be sent.
          { "start",   eNSPT_Int,  eNSPA_Required },
          // Size of the data that needs to be sent.
          { "size",    eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          // Value of the flag "search_on_read" to use with this command, i.e.
          // whether this server should search the blob if it's not found
          // locally.
          { "srch",    eNSPT_Int,  eNSPA_Required },
          // Flag whether local execution of this command should be forced
          // no matter what, i.e. if this flag is set in cases when normal
          // READLAST command would have been proxied to other servers an error
          // should be returned.
          { "local",   eNSPT_Int,  eNSPA_Required },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional },
          // Max age of blob (returned blob should be younger)
          { "age",     eNSPT_Int,  eNSPA_Optional } } },
    // Mark the "current version" of the blob as "valid".
    // This command is sent only by other NC servers in response to client's
    // SETVALID command when the server where client have sent initial command
    // cannot execute it locally for any reason (slot is not processed by that
    // server, or initial database caching is not completed yet).
    { "PROXY_SETVALID",
        {&CNCMessageHandler::x_DoCmd_SetValid,
            "PROXY_SETVALID",
            fNeedsBlobAccess + fNeedsStorageCache + fDoNotProxyToPeers,
            eNCRead,
            eProxySetValid},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Blob's version (for NC-generated blob keys this will be equal to 0).
          { "version", eNSPT_Int,  eNSPA_Required },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Remove the blob. This command is sent only by other NC servers in response
    // to client's RMV2 (or similar) command when the server where client have
    // sent initial command cannot execute it locally for any reason (slot is not
    // processed by that server, or initial database caching is not completed yet).
    { "PROXY_RMV",
        {&CNCMessageHandler::x_DoCmd_Remove,
            "PROXY_ReMoVe",
            fNeedsBlobAccess + fConfirmOnFinish + fNoBlobAccessStats,
            eNCCreate,
            eProxyRemove},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Blob's version (for NC-generated blob keys this will be equal to 0).
          { "version", eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Read meta information about the blob. This command is sent only by other
    // NC servers in response to client's GETMETA command when the server where
    // client have sent initial command cannot execute it locally for any reason
    // (slot is not processed by that server, or initial database caching
    // is not completed yet, or it was determined that this server has latest
    // version of the blob).
    { "PROXY_GETMETA",
        {&CNCMessageHandler::x_DoCmd_GetMeta,
            "PROXY_GETMETA",
            eProxyBlobRead + fDoNotCheckPassword,
            eNCRead,
            eProxyGetMeta},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          // Flag whether local execution of this command should be forced
          // no matter what, i.e. if this flag is set in cases when normal
          // GETMETA command would have been proxied to other servers an error
          // should be returned.
          { "local",   eNSPT_Int,  eNSPA_Required },
          // Client IP for application requesting the info.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application requesting the info.
          { "sid",     eNSPT_Str,  eNSPA_Required } } },
    // Start periodic synchronization session. Command is sent only by other
    // NC servers when CNCActiveSyncControl in them decides to start
    // synchronization. Response to this command contains list of events from
    // sync logs of this server which need to be synchronized. Or if this
    // server understands that synchronization using blob lists is needed then
    // first line of response will contain ALL_BLOBS word and then full list
    // of blobs in this slot will be sent.
    { "SYNC_START",
        {&CNCMessageHandler::x_DoCmd_SyncStart,
            "SYNC_START",
            fNeedsStorageCache + fNeedsLowerPriority + fNeedsAdminClient},
          // Server id of the server starting synchronization.
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          // Slot to start synchronization on.
          { "slot",    eNSPT_Int,  eNSPA_Required },
          // Last synchronized record number (in sync log) of _that_ server
          // as _that_ server thinks.
          { "rec_my",  eNSPT_Int,  eNSPA_Required },
          // Last synchronized record number (in sync log) of _this_ server
          // as _that_ server thinks.
          { "rec_your",eNSPT_Int,  eNSPA_Required } } },
    // Get full list of blobs for the slot. Command is sent only by other NC
    // servers when that server decides that synchronization using blob lists
    // is needed. Command can be sent only after successful execution of
    // SYNC_START command.
    { "SYNC_BLIST",
        {&CNCMessageHandler::x_DoCmd_SyncBlobsList,
            "SYNC_BLIST",
            eRunsInStartedSync},
          // Server id of the server managing the synchronization.
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          // Slot that synchronization is started on.
          { "slot",    eNSPT_Int,  eNSPA_Required } } },
    // Write blob contents. This command is sent only by other NC servers
    // during synchronization session if some blob was written on that server
    // and the same data didn't make it to this server yet.
    { "SYNC_PUT",
        {&CNCMessageHandler::x_DoCmd_CopyPut,
            "SYNC_PUT",
            eSyncBlobCmd + fNeedsSpaceAsPeer + fConfirmOnFinish
                         + fReadExactBlobSize + fCopyLogEvent,
            eNCCopyCreate},
          // Server id of the server managing the synchronization.
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          // Slot that synchronization is started on.
          { "slot",    eNSPT_Int,  eNSPA_Required },
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
          { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Blob's version (for NC-generated blob keys this will be equal to 0).
          { "version", eNSPT_Int,  eNSPA_Required },
          // MD5 checksum of blob's password.
          { "md5_pass",eNSPT_Str,  eNSPA_Required },
          // Creation time of the blob (microseconds since epoch).
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          // Time-to-live for the blob.
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          // Dead-time for the blob (can be greater than expiration time).
          { "dead",    eNSPT_Int,  eNSPA_Required },
          // Expiration time for the blob
          { "exp",     eNSPT_Int,  eNSPA_Required },
          // Blob size.
          { "size",    eNSPT_Int,  eNSPA_Required },
          // Time-to-live for blob's version.
          { "ver_ttl", eNSPT_Int,  eNSPA_Required },
          // Blob's version expiration time.
          { "ver_dead",eNSPT_Int,  eNSPA_Required },
          // Server_id of the server where blob was created.
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          // Id of the blob on the server where it was created.
          { "cr_id",   eNSPT_Int,  eNSPA_Required },
          // Record number of the event of blob creation in synchronization
          // logs of the server where blob was created.
          { "log_rec", eNSPT_Int,  eNSPA_Required },
          // Version of the command. Field exists for protocol backwards
          // compatibility with previous versions of NC. In current NC this
          // version is always 1.
          { "cmd_ver", eNSPT_Int,  eNSPA_Optional, "0" } } },
    // Prolong the blob's life. This command is sent only by other NC servers
    // during synchronization session if some blob was prolonged on that server
    // and the same prolongation didn't happen on this server yet.
    { "SYNC_PROLONG",
        {&CNCMessageHandler::x_DoCmd_CopyProlong,
            "SYNC_PROLONG",
            eSyncBlobCmd + fConfirmOnFinish,
            eNCRead},
          // Server id of the server managing the synchronization.
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          // Slot that synchronization is started on.
          { "slot",    eNSPT_Int,  eNSPA_Required },
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
          { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Creation time of the blob (microseconds since epoch).
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          // Server_id of the server where blob was created.
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          // Id of the blob on the server where it was created.
          { "cr_id",   eNSPT_Int,  eNSPA_Required },
          // Dead-time for the blob (can be greater than expiration time).
          { "dead",    eNSPT_Int,  eNSPA_Required },
          // Expiration time for the blob
          { "exp",     eNSPT_Int,  eNSPA_Required },
          // Blob's version expiration time.
          { "ver_dead",eNSPT_Int,  eNSPA_Required },
          // Time of creation of initial record in synchronization log about
          // this operation.
          { "log_time",eNSPT_Int,  eNSPA_Optchain },
          // Server that first made the blob's life prolongation.
          { "log_srv", eNSPT_Int,  eNSPA_Optional },
          // Record number of the initial record in synchronization log about
          // this operation.
          { "log_rec", eNSPT_Int,  eNSPA_Optional } } },
    // Read blob contents. This command is sent only by other NC servers
    // during synchronization session if some blob was written on this server
    // and the same data didn't make it to that server yet.
    { "SYNC_GET",
        {&CNCMessageHandler::x_DoCmd_SyncGet,
            "SYNC_GET",
            eSyncBlobCmd,
            eNCReadData},
          // Server id of the server managing the synchronization.
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          // Slot that synchronization is started on.
          { "slot",    eNSPT_Int,  eNSPA_Required },
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
          { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Time of creation of initial record in synchronization log about
          // this operation.
          { "log_time",eNSPT_Int,  eNSPA_Required },
          // Creation time of the blob (microseconds since epoch).
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          // Server_id of the server where blob was created.
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          // Id of the blob on the server where it was created.
          { "cr_id",   eNSPT_Int,  eNSPA_Required } } },
    // Get information necessary to prolong the blob's life. This command
    // is sent only by other NC servers during synchronization session if some
    // blob was prolonged on this server and the same prolongation didn't
    // happen on that server yet.
    { "SYNC_PROINFO",
        {&CNCMessageHandler::x_DoCmd_SyncProlongInfo,
            "SYNC_PROINFO",
            eSyncBlobCmd,
            eNCRead},
          // Server id of the server managing the synchronization.
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          // Slot that synchronization is started on.
          { "slot",    eNSPT_Int,  eNSPA_Required },
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
          { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required } } },
    // "Commit" the synchronization session. This command is sent only by other
    // NC servers at the end of synchronization session when all necessary
    // commands have been executed successfully.
    { "SYNC_COMMIT",
        {&CNCMessageHandler::x_DoCmd_SyncCommit,
            "SYNC_COMMIT",
            eRunsInStartedSync + fProhibitsSyncAbort + fConfirmOnFinish},
          // Server id of the server managing the synchronization.
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          // Slot that synchronization is started on.
          { "slot",    eNSPT_Int,  eNSPA_Required },
          // Last synchronized record number (in sync log) of _that_ server.
          { "rec_my",  eNSPT_Int,  eNSPA_Required },
          // Last synchronized record number (in sync log) of _this_ server.
          { "rec_your",eNSPT_Int,  eNSPA_Required } } },
    // "Cancel" the synchronization session. This command is sent only by other
    // NC servers at the end of synchronization session when either this server
    // requested or that server decided that synchronization should be aborted
    // despite the successful execution of all commands. Reason for
    // cancellation could be some server going to shutdown, or requirement to
    // clean sync logs (and synchronization already executes for too long).
    // The cancellation doesn't cancel any commands already executed in this
    // synchronization session. It exists only to quickly mark this
    // synchronization as no longer executing so that NC could start
    // synchronization with some other server.
    { "SYNC_CANCEL",
        {&CNCMessageHandler::x_DoCmd_SyncCancel,
            "SYNC_CANCEL",
            eRunsInStartedSync + fProhibitsSyncAbort + fConfirmOnFinish},
          // Server id of the server managing the synchronization.
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          // Slot that synchronization is started on.
          { "slot",    eNSPT_Int,  eNSPA_Required } } },
    // Get meta information about the blob. Command for "NetCache" clients.
    { "GETMETA",
        {&CNCMessageHandler::x_DoCmd_GetMeta,
            "GETMETA",
            eClientBlobRead + fDoNotCheckPassword,
            eNCRead,
            eProxyGetMeta},
          // Key of the blob
        { { "key",     eNSPT_NCID, eNSPA_Required },
          // Flag forcing local execution of the command (without forwarding
          // to other servers and without searching for blob on them).
          { "local",   eNSPT_Int,  eNSPA_Optional },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional } } },
    // Get meta information about the blob. Command for "ICache" clients.
    { "GETMETA",
        {&CNCMessageHandler::x_DoCmd_GetMeta,
            "IC_GETMETA",
            eClientBlobRead + fDoNotCheckPassword,
            eNCRead,
            eProxyGetMeta},
          // Name of cache for blob.
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's version.
          { "version", eNSPT_Int,  eNSPA_Required },
          // Blob's subkey.
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Flag forcing local execution of the command (without forwarding
          // to other servers and without searching for blob on them).
          { "local",   eNSPT_Int,  eNSPA_Optional },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional } } },
    // Prolong blob's life for a specific number of seconds from current time.
    // Command provides a minimum time this blob should be still available for.
    // If blob's expiration time was already later than that this command is
    // a no-op.
    { "PROLONG",
        {&CNCMessageHandler::x_DoCmd_Prolong,
            "PROLONG",
            eClientBlobRead,
            eNCRead,
            eProxyProlong},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Period of time for the blob to be available.
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    // Prolong blob's life for a specific number of seconds from current time.
    // This command is sent only by other NC servers in response to client's
    // PROLONG command when the server where client have sent initial command
    // cannot execute it locally for any reason (slot is not processed by that
    // server, or initial database caching is not completed yet, or it was
    // determined that this server has latest version of the blob).
    { "PROXY_PROLONG",
        {&CNCMessageHandler::x_DoCmd_Prolong,
            "PROXY_PROLONG",
            eProxyBlobRead + fConfirmOnFinish,
            eNCRead,
            eProxyProlong},
          // Name of cache for blob (for NC-generated blob keys this will be
          // empty).
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // Blob's key.
          { "key",     eNSPT_Str,  eNSPA_Required },
          // Blob's subkey (for NC-generated blob keys this will be empty).
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          // Period of time for the blob to be available.
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          // Quorum to use for this operation.
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          // Flag whether local execution of this command should be forced
          // no matter what, i.e. if this flag is set in cases when normal GET2
          // command would have been proxied to other servers an error should
          // be returned.
          { "local",   eNSPT_Int,  eNSPA_Required },
          // Value of the flag "search_on_read" to use with this command, i.e.
          // whether this server should search the blob if it's not found
          // locally.
          { "srch",    eNSPT_Int,  eNSPA_Required },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Required },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Required },
          // Password for blob access.
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    /*{ "BLOBSLIST",
        {&CNCMessageHandler::x_DoCmd_GetBlobsList,
            "BLOBSLIST",
            fNeedsStorageCache + fNeedsAdminClient} },*/
    // Write blob contents. Deprecated command used now only by old clients.
    // This command is the same as PUT3 except it uses connection closing as
    // legitimate EOF marker for blob's data.
    { "PUT2",
        {&CNCMessageHandler::x_DoCmd_Put,
            "PUT2",
            eClientBlobWrite + fCanGenerateKey + fCursedPUT2Cmd,
            eNCCreate,
            eProxyWrite},
          // Time-to-live for the blob.
        { { "ttl",     eNSPT_Int,  eNSPA_Optional },
          // Key of the blob (if skipped or empty then new one will be created).
          { "key",     eNSPT_NCID, eNSPA_Optional } } },
    // Shutdown the server
    { "SHUTDOWN",
        {&CNCMessageHandler::x_DoCmd_Shutdown,
            "SHUTDOWN",
            fNeedsAdminClient + fConfirmOnFinish},
          // Client IP for application sending the command.
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          // drain: wait until all BLOBs are expired, then shutdown
          { "drain",    eNSPT_Int,  eNSPA_Optional } } },
    // Get server statistics.
    { "GETSTAT",
        {&CNCMessageHandler::x_DoCmd_GetStat, "GETSTAT"},
          // Flag showing whether current (value is 0) or previous (value is 1)
          // statistics period should be shown.
        { { "prev",    eNSPT_Int,  eNSPA_Optchain, "0" },
          // Type of statistics period to show. See top of nc_stat.cpp for
          // list of all possible period types.
          { "type",    eNSPT_Str,  eNSPA_Optional, "life" },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    // Read full ini-file used by NetCache for configuration.
    { "GETCONF",
        {&CNCMessageHandler::x_DoCmd_GetConfig,      "GETCONF"},
        { 
          // Netcached.ini section name
          { "section", eNSPT_Str,  eNSPA_Optional },
          // when section name is "netcache", setup for this port
          { "port",    eNSPT_Str,  eNSPA_Optional },
          // when section name is "netcache", setup for this cache
          { "cache",   eNSPT_Str,  eNSPA_Optional },
          // Client IP for application sending the command.
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          // Session ID for application sending the command.
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },


    { "PURGE",
        {&CNCMessageHandler::x_DoCmd_Purge,
            "PURGE",
            fConfirmOnFinish + fNeedsAdminClient},
          // Cache name.
        { { "cache",     eNSPT_Str, eNSPA_Required } } },
    { "COPY_PURGE",
        {&CNCMessageHandler::x_DoCmd_CopyPurge,
            "COPY_PURGE",
            fConfirmOnFinish /* + fNeedsAdminClient*/},
          // Cache name.
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          // forget blobs created earlier than cr_time
          { "cr_time", eNSPT_Int,  eNSPA_Required } } },



    // All commands below are not implemented and mostly old ones not needed
    // anymore. One exception is RECONF - it would be nice to have it but it
    // needs some thinking on how to implement it.

    { "REINIT",   {&CNCMessageHandler::x_DoCmd_NotImplemented, "REINIT"} },
    { "REINIT",   {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_REINIT"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "RECONF",   {&CNCMessageHandler::x_DoCmd_NotImplemented, "RECONF"} },
    { "LOG",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "LOG"} },
    { "STAT",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "STAT"} },
    { "MONI",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "MONITOR"} },
    { "DROPSTAT", {&CNCMessageHandler::x_DoCmd_NotImplemented, "DROPSTAT"} },
    { "GBOW",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "GBOW"} },
    { "ISLK",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "ISLK"} },
    { "SMR",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "SMR"} },
    { "SMU",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "SMU"} },
    { "OK",       {&CNCMessageHandler::x_DoCmd_NotImplemented, "OK"} },
    { "GET",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "GET"} },
    { "PUT",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "PUT"} },
    { "REMOVE",   {&CNCMessageHandler::x_DoCmd_NotImplemented, "REMOVE"} },
    { "STSP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_STSP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GTSP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GTSP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "SVRP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_SVRP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GVRP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GVRP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "PRG1",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_PRG1"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "REMK",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_REMK"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GBLW",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GBLW"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "ISOP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_ISOP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GACT",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GACT"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GTOU",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GTOU"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { NULL }
};

// List of arguments that can be in client authentication line.
static SNSProtoArgument s_AuthArgs[] = {
    { "client", eNSPT_Str, eNSPA_Optional, "Unknown client" },
    { "params", eNSPT_Str, eNSPA_Ellipsis },
    { NULL }
};




/////////////////////////////////////////////////////////////////////////////
// CNCMessageHandler implementation

inline void
CNCMessageHandler::x_SetFlag(ENCCmdFlags flag)
{
    m_Flags |= flag;
}

inline void
CNCMessageHandler::x_UnsetFlag(ENCCmdFlags flag)
{
    m_Flags &= ~flag;
}

inline bool
CNCMessageHandler::x_IsFlagSet(ENCCmdFlags flag)
{
    return (m_Flags & flag) != 0;
}

//static Uint8 s_CntHndls = 0;

CNCMessageHandler::CNCMessageHandler(void)
    : m_Flags(0),
      m_Parser(s_CommandMap),
      m_CmdProcessor(NULL),
      m_BlobAccess(NULL),
      m_ChunkLen(0),
      m_SrvsIndex(0),
      m_ActiveHub(NULL)
{
    LOG_CURRENT_FUNCTION
    m_CopyBlobInfo = new SNCBlobVerData();
    m_LatestBlobSum = new SNCBlobSummary();

    SetState(&Me::x_SocketOpened);

    //Uint8 cnt = AtomicAdd(s_CntHndls, 1);
    //INFO("CNCMessageHandler, cnt=" << cnt);
}

CNCMessageHandler::~CNCMessageHandler(void)
{
    LOG_CURRENT_FUNCTION
    delete m_LatestBlobSum;
    delete m_CopyBlobInfo;

    //Uint8 cnt = AtomicSub(s_CntHndls, 1);
    //INFO("~CNCMessageHandler, cnt=" << cnt);
}

CNCMessageHandler::State
CNCMessageHandler::x_SocketOpened(void)
{
    LOG_CURRENT_FUNCTION
    m_PrevCache.clear();
    m_ClientParams.clear();
    m_CntCmds = 0;

    string host;
    Uint2 port = 0;
    GetPeerAddress(host, port);
    m_ClientParams["peer"]  = host;
    m_ClientParams["pport"] = NStr::UIntToString(port);
    m_LocalPort             = GetLocalPort();
    m_ClientParams["port"]  = NStr::UIntToString(m_LocalPort);

    return &Me::x_ReadAuthMessage;
}

CNCMessageHandler::State
CNCMessageHandler::x_CloseCmdAndConn(void)
{
    LOG_CURRENT_FUNCTION
    if (GetDiagCtx()->GetRequestStatus() == eStatus_OK) {
        if (HasError()  ||  !CanHaveMoreRead())
            GetDiagCtx()->SetRequestStatus(eStatus_PrematureClose);
        else if (CTaskServer::IsInShutdown())
            GetDiagCtx()->SetRequestStatus(eStatus_CmdAborted);
        else
            GetDiagCtx()->SetRequestStatus(eStatus_CmdTimeout);
    }
    int status = GetDiagCtx()->GetRequestStatus();
    x_UnsetFlag(fConfirmOnFinish);
    x_CleanCmdResources();
    GetDiagCtx()->SetRequestStatus(status);
    return &Me::x_SaveStatsAndClose;
}

CNCMessageHandler::State
CNCMessageHandler::x_SaveStatsAndClose(void)
{
    LOG_CURRENT_FUNCTION
    //CNCStat::AddClosedConnection(conn_span, GetDiagCtx()->GetRequestStatus(), m_CntCmds);
    CNCStat::ConnClosing(m_CntCmds);
    return &Me::x_PrintCmdsCntAndClose;
}

CNCMessageHandler::State
CNCMessageHandler::x_PrintCmdsCntAndClose(void)
{
    LOG_CURRENT_FUNCTION
    CSrvDiagMsg().PrintExtra().PrintParam("cmds_cnt", m_CntCmds);
    CloseSocket();
    Terminate();
    return NULL;
}

CNCMessageHandler::State
CNCMessageHandler::x_WriteInitWriteResponse(void)
{
    LOG_CURRENT_FUNCTION
check_again:
    ENCClientHubStatus status = m_ActiveHub->GetStatus();
    if (status == eNCHubError) {
        m_LastPeerError = m_ActiveHub->GetErrMsg();
        SRV_LOG(Warning, "Error executing command on peer "
            << m_ActiveHub->GetFullPeerName() << ", peer says: "
            << m_LastPeerError);
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        return &Me::x_ProxyToNextPeer;
    }
    else if (status != eNCHubCmdInProgress)
        abort();
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    CNCActiveHandler* active = m_ActiveHub->GetHandler();
    if (!active->GotClientResponse())
        return NULL;
    // Intentionally re-reading the status because it could change since our
    // previous read at the beginning of the function.
    if (m_ActiveHub->GetStatus() != eNCHubCmdInProgress)
        goto check_again;

    WriteText(active->GetCmdResponse()).WriteText("\n");
    Flush();
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    return &Me::x_ReadBlobSignature;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadAuthMessage(void)
{
    LOG_CURRENT_FUNCTION
    if (NeedToClose()  ||  CTaskServer::IsInSoftShutdown()) {
        if (CTaskServer::IsInShutdown())
            GetDiagCtx()->SetRequestStatus(eStatus_ShuttingDown);
        else
            GetDiagCtx()->SetRequestStatus(eStatus_Inactive);
        return &Me::x_SaveStatsAndClose;
    }

    CTempString auth_line;
    if (!ReadLine(&auth_line)) {
        if (!HasError()  &&  CanHaveMoreRead())
            return NULL;
        if (IsReadDataAvailable()) {
            GetDiagCtx()->SetRequestStatus(eStatus_PrematureClose);
            return &Me::x_SaveStatsAndClose;
        }
        else {
            GetDiagCtx()->SetRequestStatus(eStatus_FakeConn);
            return &Me::x_PrintCmdsCntAndClose;
        }
    }

    TNSProtoParams params;
    try {
        m_Parser.ParseArguments(auth_line, s_AuthArgs, &params);
    }
    catch (CNSProtoParserException& ex) {
        SRV_LOG(Warning, "Error authenticating client: '"
                         << auth_line << "': " << ex);
        params["client"] = auth_line;
    }
    ITERATE(TNSProtoParams, it, params) {
        m_ClientParams[it->first] = it->second;
    }
    CSrvDiagMsg diag_msg;
    diag_msg.PrintExtra();
    ITERATE(TNSProtoParams, it, params) {
        diag_msg.PrintParam(it->first, it->second);
    }
    diag_msg.Flush();

    m_BaseAppSetup = m_AppSetup = CNCServer::GetAppSetup(m_ClientParams);
    if (m_AppSetup->disable) {
        SRV_LOG(Warning, "Disabled client is being disconnected ('"
                         << auth_line << "').");
        GetDiagCtx()->SetRequestStatus(eStatus_Disabled);
        return &Me::x_SaveStatsAndClose;
    }
    else {
        return &Me::x_ReadCommand;
    }
}

void
CNCMessageHandler::x_AssignCmdParams(void)
{
    LOG_CURRENT_FUNCTION
    CTempString blob_key, blob_subkey;
    m_BlobVersion = 0;
    m_RawKey.clear();
    m_BlobPass.clear();
    m_RawBlobPass.clear();
    m_KeyVersion = 1;
    m_BlobTTL = 0;
    m_StartPos = 0;
    m_Size = Uint8(-1);
    m_OrigRecNo = 0;
    m_OrigSrvId = 0;
    m_OrigTime = 0;
    m_Quorum = 1;
    m_CmdVersion = 0;
    m_ForceLocal = false;
    m_AgeMax = m_AgeCur = 0;
    bool quorum_was_set = false;
    bool search_was_set = false;

    CTempString cache_name;

    ERASE_ITERATE(TNSProtoParams, it, m_ParsedCmd.params) {
        const CTempString& key = it->first;
        CTempString& val = it->second;

        switch (key[0]) {
        case 'a':
            if (key == "age") {
                m_AgeMax = NStr::StringToUInt8(val);
            }
            break;
        case 'c':
            switch (key[1]) {
            case 'a':
                if (key == "cache") {
                    cache_name = val;
                }
                break;
            case 'm':
                if (key == "cmd_ver") {
                    m_CmdVersion = NStr::StringToUInt(val);
                }
                break;
            case 'o':
                if (key == "confirm") {
                    if (val == "1")
                        x_SetFlag(fConfirmOnFinish);
                    else
                        x_UnsetFlag(fConfirmOnFinish);
                }
                break;
            case 'r':
                if (key == "cr_time") {
                    m_CopyBlobInfo->create_time = NStr::StringToUInt8(val);
                }
                else if (key == "cr_id") {
                    m_CopyBlobInfo->create_id = NStr::StringToUInt(val);
                }
                else if (key == "cr_srv") {
                    m_CopyBlobInfo->create_server = NStr::StringToUInt8(val);
                }
                break;
            }
            break;
        case 'd':
            if (key == "dead") {
                m_CopyBlobInfo->dead_time = NStr::StringToInt(val);
            }
            break;
        case 'e':
            if (key == "exp") {
                m_CopyBlobInfo->expire = NStr::StringToInt(val);
            }
            break;
        case 'i':
            if (key == "ip") {
                if (!val.empty())
                    GetDiagCtx()->SetClientIP(val);
                // Erase parameter to not print it in request-start, it will be
                // printed as a part of standard log header.
                m_ParsedCmd.params.erase(it);
            }
            break;
        case 'k':
            if (key == "key") {
                m_RawKey = blob_key = val;
            }
            break;
        case 'l':
            if (key == "log_rec") {
                m_OrigRecNo = NStr::StringToUInt8(val);
            }
            else if (key == "log_srv") {
                m_OrigSrvId = NStr::StringToUInt8(val);
            }
            else if (key == "log_time") {
                m_OrigTime = NStr::StringToUInt8(val);
            }
            else if (key == "local") {
                m_ForceLocal = val == "1";
            }
            break;
        case 'm':
            if (key == "md5_pass") {
                m_BlobPass = val;
            }
            break;
        case 'p':
            if (key == "pass") {
                m_RawBlobPass = val;
                CMD5 md5;
                md5.Update(val.data(), val.size());
                unsigned char digest[16];
                md5.Finalize(digest);
                m_BlobPass.assign((char*)digest, 16);
                // Erase parameter to not expose passwords via logs.
                m_ParsedCmd.params.erase(it);
            }
            else if (key == "prev") {
                m_StatPrev = val == "1";
            }
            break;
        case 'q':
            if (key == "qrum") {
                m_Quorum = NStr::StringToUInt(val);
                quorum_was_set = true;
            }
            break;
        case 'r':
            if (key == "rec_my") {
                m_RemoteRecNo = NStr::StringToUInt8(val);
            }
            else if (key == "rec_your") {
                m_LocalRecNo = NStr::StringToUInt8(val);
            }
            break;
        case 's':
            switch (key[1]) {
            case 'i':
                if (key == "sid") {
                    if (!val.empty())
                        GetDiagCtx()->SetSessionID(NStr::URLDecode(val));
                    // Erase parameter to not print it in request-start,
                    // it will be printed as a part of standard log header.
                    m_ParsedCmd.params.erase(it);
                }
                else if (key == "size") {
                    m_Size = Uint8(NStr::StringToInt8(val));
                }
                break;
            case 'l':
                if (key == "slot") {
                    m_Slot = Uint2(NStr::StringToUInt(val));
                }
                break;
            case 'r':
                if (key == "srv_id") {
                    m_SrvId = NStr::StringToUInt8(val);
                }
                else if (key == "srch") {
                    m_SearchOnRead = val != "0";
                    search_was_set = true;
                }
                break;
            case 't':
                if (key == "start") {
                    m_StartPos = NStr::StringToUInt8(val);
                }
                break;
            case 'u':
                if (key == "subkey") {
                    blob_subkey = val;
                }
                break;
            }
            break;
        case 't':
            if (key == "ttl") {
                m_BlobTTL = NStr::StringToUInt(val);
            }
            else if (key == "type") {
                m_StatType = val;
            }
            break;
        case 'v':
            if (key == "version") {
                m_BlobVersion = NStr::StringToInt(val);
            }
            else if (key == "ver_ttl") {
                m_CopyBlobInfo->ver_ttl = NStr::StringToUInt(val);
            }
            else if (key == "ver_dead") {
                m_CopyBlobInfo->ver_expire = NStr::StringToInt(val);
            }
            break;
        default:
            break;
        }
    }

    CNCBlobStorage::PackBlobKey(&m_BlobKey, cache_name, blob_key, blob_subkey);
    if (cache_name.empty()) {
        m_AppSetup = m_BaseAppSetup;
        m_ClientParams.erase("cache");
    }
    else if (cache_name == m_PrevCache) {
        m_AppSetup = m_PrevAppSetup;
    }
    else {
        m_PrevCache = cache_name;
        m_ClientParams["cache"] = cache_name;
        m_AppSetup = m_PrevAppSetup = CNCServer::GetAppSetup(m_ClientParams);
    }
    if (!quorum_was_set)
        m_Quorum = m_AppSetup->quorum;
    if (m_ForceLocal)
        m_Quorum = 1;
    if (!search_was_set)
        m_SearchOnRead = m_AppSetup->srch_on_read;
}

void
CNCMessageHandler::x_PrintRequestStart(CSrvDiagMsg& diag_msg)
{
    LOG_CURRENT_FUNCTION
    diag_msg.StartRequest();
    diag_msg.PrintParam("_type", "cmd");
    diag_msg.PrintParam("cmd", m_ParsedCmd.command->cmd);
    diag_msg.PrintParam("client", m_ClientParams["client"]);
    diag_msg.PrintParam("conn", m_ConnReqId);
    ITERATE(TNSProtoParams, it, m_ParsedCmd.params) {
        diag_msg.PrintParam(it->first, it->second);
    }
    if (!m_BlobPass.empty())
        diag_msg.PrintParam("pass", CNCBlobStorage::PrintablePassword(m_BlobPass));
}

CNCMessageHandler::State
CNCMessageHandler::x_StartCommand(void)
{
    LOG_CURRENT_FUNCTION
    m_CmdStartTime = CSrvTime::Current();
    CNCStat::CmdStarted(m_ParsedCmd.command->cmd);
    CSrvDiagMsg diag_msg;
    x_PrintRequestStart(diag_msg);

    if (NeedToClose()) {
        diag_msg.Flush();
        m_Flags = 0;
        return &Me::x_CloseCmdAndConn;
    }
    if (HasError()  ||  !CanHaveMoreRead()) {
        diag_msg.Flush();
        GetDiagCtx()->SetRequestStatus(eStatus_PrematureClose);
        m_Flags = 0;
        return &Me::x_CloseCmdAndConn;
    }

    if (x_IsFlagSet(fNeedsAdminClient)
        &&  m_ClientParams["client"] != CNCServer::GetAdminClient()
        &&  m_ClientParams["client"] != kNCPeerClientName)
    {
        diag_msg.Flush();
        WriteText("ERR:Command requires administrative privileges\n");
        GetDiagCtx()->SetRequestStatus(eStatus_NeedAdmin);
        m_Flags = 0;
        return &Me::x_FinishCommand;
    }

    if (!CNCServer::IsCachingComplete()
        &&  (x_IsFlagSet(fNeedsStorageCache)  ||  m_ForceLocal))
    {
        diag_msg.Flush();
        GetDiagCtx()->SetRequestStatus(eStatus_JustStarted);
        WriteText(s_MsgForStatus[eStatus_JustStarted]).WriteText("\n");
        m_Flags = 0;
        return &Me::x_FinishCommand;
    }

    if (m_AppSetup->disable) {
        diag_msg.Flush();
        // We'll be here only if generally work for the client is enabled but
        // for current particular cache it is disabled.
        GetDiagCtx()->SetRequestStatus(eStatus_Disabled);
        WriteText(s_MsgForStatus[eStatus_Disabled]).WriteText("\n");
        m_Flags = 0;
        return &Me::x_FinishCommand;
    }

    if (x_IsFlagSet(fRunsInStartedSync)) {
        ESyncInitiateResult start_res = CNCPeriodicSync::CanStartSyncCommand(
                                        m_SrvId, m_Slot,
                                        !x_IsFlagSet(fProhibitsSyncAbort),
                                        m_SyncId);
        if (start_res == eNetworkError) {
            diag_msg.Flush();
            WriteText("ERR:Stale synchronization\n");
            GetDiagCtx()->SetRequestStatus(eStatus_StaleSync);
            m_Flags = 0;
            return &Me::x_FinishCommand;
        }
        else if (start_res == eServerBusy  ||  CTaskServer::IsInSoftShutdown()) {
            diag_msg.Flush();
            WriteText("OK:SIZE=0, NEED_ABORT1\n");
            GetDiagCtx()->SetRequestStatus(eStatus_SyncAborted);
            // Old NC servers (those which used CNetCacheAPI instead of
            // CNCActiveHandler) always started to write blob data in SYNC_PUT
            // even when we responded to them NEED_ABORT. To avoid breaking the protocol
            // we need to read from them those fake blob writes.
            bool needs_fake = x_IsFlagSet(fReadExactBlobSize)  &&  m_CmdVersion == 0;
            if (start_res == eServerBusy)
                m_Flags = 0;
            if (needs_fake)
                return &Me::x_ReadBlobSignature;
            return &Me::x_FinishCommand;
        }
    }

    if (!x_IsFlagSet(fNeedsBlobAccess)) {
        diag_msg.Flush();
        // if we do not need blob access
        return m_CmdProcessor;
    }

    if (((m_BlobPass.empty()  &&  m_AppSetup->pass_policy == eNCOnlyWithPass)
            ||  (!m_BlobPass.empty()  &&  m_AppSetup->pass_policy == eNCOnlyWithoutPass))
        &&  !x_IsFlagSet(fDoNotCheckPassword))
    {
        diag_msg.Flush();
        GetDiagCtx()->SetRequestStatus(eStatus_NotAllowed);
        WriteText(s_MsgForStatus[eStatus_NotAllowed].substr(4)).WriteText("\n");
        SRV_LOG(Warning, s_MsgForStatus[eStatus_NotAllowed]);
        return &Me::x_FinishCommand;
    }

    if (x_IsFlagSet(fCanGenerateKey)  &&  m_RawKey.empty()) {
        CNCDistributionConf::GenerateBlobKey(m_LocalPort, m_RawKey, m_BlobSlot, m_TimeBucket);
        CNCBlobStorage::PackBlobKey(&m_BlobKey, CTempString(), m_RawKey, CTempString());

        diag_msg.PrintParam("key", m_RawKey);
        diag_msg.PrintParam("gen_key", "1");
    }
    else if (m_BlobKey[0] == '\1') {
        CNetCacheKey nc_key;
        if (!nc_key.ParseBlobKey(m_RawKey.data(), m_RawKey.length(), &nc_key) ||
                !CNCDistributionConf::GetSlotByNetCacheKey(m_BlobKey,
                        m_BlobSlot, m_TimeBucket)) {
            diag_msg.Flush();
            GetDiagCtx()->SetRequestStatus(eStatus_NotFound);
            WriteText(s_MsgForStatus[eStatus_NotFound]).WriteText("\n");
            SRV_LOG(Critical, "Invalid blob key format: " << m_RawKey);
            return &Me::x_FinishCommand;
        }
        // If key is given and it's NetCache key (not ICache key) then we need
        // to strip service name from it. It's necessary for the case when new
        // CNetCacheAPI with enabled_mirroring=true passes key to an old CNetCacheAPI
        // which doesn't even know about mirroring.
        if (nc_key.HasExtensions())
            CNCBlobStorage::PackBlobKey(&m_BlobKey, CTempString(),
                    nc_key.StripKeyExtensions(), CTempString());
    } else
        CNCDistributionConf::GetSlotByICacheKey(m_BlobKey,
                m_BlobSlot, m_TimeBucket);

    m_BlobSize = 0;
    diag_msg.PrintParam("slot", m_BlobSlot);

    if ((!CNCDistributionConf::IsServedLocally(m_BlobSlot)
            ||  !CNCServer::IsCachingComplete())
        &&  !x_IsFlagSet(fDoNotProxyToPeers)
        &&  !m_ForceLocal)
    {
        diag_msg.PrintParam("proxy", "1");
        diag_msg.Flush();
        x_GetCurSlotServers();
        return &Me::x_ProxyToNextPeer;
    }

    diag_msg.Flush();

    if (!CNCServer::IsInitiallySynced()  &&  !m_ForceLocal
        &&  x_IsFlagSet(fUsesPeerSearch))
    {
        m_Quorum = 0;
    }
    if (x_IsFlagSet(fNeedsLowerPriority))
        SetPriority(CNCDistributionConf::GetSyncPriority());
    if ((x_IsFlagSet(fNeedsSpaceAsClient)
            &&  CNCBlobStorage::NeedStopWrite())
        ||  (x_IsFlagSet(fNeedsSpaceAsPeer)
            &&  !CNCBlobStorage::AcceptWritesFromPeers()))
    {
        GetDiagCtx()->SetRequestStatus(eStatus_NoDiskSpace);
        WriteText(s_MsgForStatus[eStatus_NoDiskSpace]).WriteText("\n");
        return &Me::x_FinishCommand;
    }

    m_BlobAccess = CNCBlobStorage::GetBlobAccess(
                                    m_ParsedCmd.command->extra.blob_access,
                                    m_BlobKey, m_BlobPass, m_TimeBucket);
    m_BlobAccess->RequestMetaInfo(this);
    return &Me::x_WaitForBlobAccess;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadCommand(void)
{
    LOG_CURRENT_FUNCTION
    if (NeedToClose()  ||  CTaskServer::IsInSoftShutdown()) {
        if (CTaskServer::IsInShutdown())
            GetDiagCtx()->SetRequestStatus(eStatus_ShuttingDown);
        else
            GetDiagCtx()->SetRequestStatus(eStatus_Inactive);
        return &Me::x_SaveStatsAndClose;
    }

    CTempString cmd_line;
    if (!ReadLine(&cmd_line)) {
        if (!HasError()  &&  CanHaveMoreRead())
            return NULL;
        if (IsReadDataAvailable())
            GetDiagCtx()->SetRequestStatus(eStatus_PrematureClose);
        return &Me::x_SaveStatsAndClose;
    }

    try {
        m_ParsedCmd = m_Parser.ParseCommand(cmd_line);
    }
    catch (CNSProtoParserException& ex) {
        SRV_LOG(Warning, "Error parsing command: " << ex);
        GetDiagCtx()->SetRequestStatus(eStatus_BadCmd);
        //abort();
        return &Me::x_SaveStatsAndClose;
    }
    const SCommandExtra& cmd_extra = m_ParsedCmd.command->extra;
    m_CmdProcessor = cmd_extra.processor;
    m_Flags        = cmd_extra.cmd_flags;
    CreateNewDiagCtx();
    try {
        x_AssignCmdParams();
    }
    catch (CStringException& ex) {
        ReleaseDiagCtx();
        SRV_LOG(Warning, "Error while parsing command '" << cmd_line
                         << "': " << ex);
        GetDiagCtx()->SetRequestStatus(eStatus_BadCmd);
        return &Me::x_SaveStatsAndClose;
    }
    return &Me::x_StartCommand;
}

void
CNCMessageHandler::x_GetCurSlotServers(void)
{
    LOG_CURRENT_FUNCTION
    m_CheckSrvs = CNCDistributionConf::GetServersForSlot(m_BlobSlot);
    Uint4 main_srv_ip = 0;
    if (m_BlobKey[0] == '\1') {
        string cache_name, key, subkey;
        CNCBlobStorage::UnpackBlobKey(m_BlobKey, cache_name, key, subkey);
        main_srv_ip = CNCDistributionConf::GetMainSrvIP(key);
    }
    if (main_srv_ip != 0) {
        // Note: this check for "main" server for blob assumes that for each
        // blob slot only one NetCache instance processing it works on each
        // server. It will give false positive results if there are several
        // NC instances on the same server processing the same slot. But there's
        // no much sense in such setup, so it's pretty safe assumption.
        if (Uint4(CNCDistributionConf::GetSelfID() >> 32) == main_srv_ip) {
            m_ThisServerIsMain = true;
        }
        else {
            m_ThisServerIsMain = false;
            for (size_t i = 0; i < m_CheckSrvs.size(); ++i) {
                if (Uint4(m_CheckSrvs[i] >> 32) == main_srv_ip) {
                    Uint8 srv_id = m_CheckSrvs[i];
                    m_CheckSrvs.erase(m_CheckSrvs.begin() + i);
                    m_CheckSrvs.insert(m_CheckSrvs.begin(), srv_id);
                    break;
                }
            }
        }
    }
}

CNCMessageHandler::State
CNCMessageHandler::x_WaitForBlobAccess(void)
{
    LOG_CURRENT_FUNCTION
    if (!m_BlobAccess->IsMetaInfoReady())
        return NULL;
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    if (!m_BlobAccess->IsAuthorized()  &&  !x_IsFlagSet(fDoNotCheckPassword)) {
        GetDiagCtx()->SetRequestStatus(eStatus_BadPassword);
        WriteText(s_MsgForStatus[eStatus_BadPassword]).WriteText("\n");
        return &Me::x_FinishCommand;
    }

    if (!x_IsFlagSet(fUsesPeerSearch))
        return m_CmdProcessor;

    // All commands that have fUsesPeerSearch will operate on m_LatestExist and
    // m_LatestBlobSum, so we need to fill it here even if in next "if" we'll go
    // almost directly to m_CmdProcessor.
    m_LatestExist = m_BlobAccess->IsBlobExists()
                    &&  (x_IsFlagSet(fNoBlobVersionCheck)
                         ||  m_BlobAccess->GetCurBlobVersion() == m_BlobVersion);
    m_LatestSrvId = CNCDistributionConf::GetSelfID();
    if (m_LatestExist) {
        m_LatestBlobSum->create_time = m_BlobAccess->GetCurBlobCreateTime();
        m_LatestBlobSum->create_server = m_BlobAccess->GetCurCreateServer();
        m_LatestBlobSum->create_id = m_BlobAccess->GetCurCreateId();
        m_LatestBlobSum->dead_time = m_BlobAccess->GetCurBlobDeadTime();
        m_LatestBlobSum->expire = m_BlobAccess->GetCurBlobExpire();
        m_LatestBlobSum->ver_expire = m_BlobAccess->GetCurVerExpire();
    }
    if (x_IsFlagSet(fDoNotProxyToPeers)
        ||  m_ForceLocal
        ||  (m_Quorum == 1  &&  (m_LatestExist  ||  !m_SearchOnRead))
        ||  (m_LatestExist  &&  x_IsFlagSet(fPeerFindExistsOnly)))
    {
        return &Me::x_ExecuteOnLatestSrvId;
    }

    x_GetCurSlotServers();
    if (m_ThisServerIsMain  &&  m_AppSetup->fast_on_main
        &&  CNCServer::IsInitiallySynced())
    {
        return &Me::x_ExecuteOnLatestSrvId;
    }

    if (m_LatestExist  &&  m_Quorum != 0)
        --m_Quorum;
    return &Me::x_ReadMetaNextPeer;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReportBlobNotFound(void)
{
    LOG_CURRENT_FUNCTION
    x_SetFlag(fNoBlobAccessStats);
    x_UnsetFlag(fConfirmOnFinish);
    GetDiagCtx()->SetRequestStatus(eStatus_NotFound);
    WriteText(s_MsgForStatus[eStatus_NotFound]);
    if (m_AgeCur != 0) {
        WriteText(", AGE=").WriteNumber(m_AgeCur);
        WriteText(", VER=").WriteNumber(m_BlobAccess->GetCurBlobVersion());
    }
    WriteText("\n");
    return &Me::x_FinishCommand;
}

void
CNCMessageHandler::x_ProlongBlobDeadTime(unsigned int add_time)
{
    LOG_CURRENT_FUNCTION
    if (!m_AppSetup->prolong_on_read)
        return;

    CSrvTime cur_srv_time = CSrvTime::Current();
    Uint8 cur_time = cur_srv_time.AsUSec();
    int now = int(cur_srv_time.Sec());
    int new_expire = now + add_time;
    if (new_expire < now) {
        return;
    }
    int old_expire = m_BlobAccess->GetCurBlobExpire();
    if (!CNCServer::IsDebugMode()  &&  new_expire - old_expire < m_AppSetup->ttl_unit)
        return;

    if (m_AppSetup->lifespan_ttl > 0) {
        int created = (int)(m_BlobAccess->GetCurBlobCreateTime()/kUSecsPerSecond);
        int retire = (int)(created + m_AppSetup->lifespan_ttl);
        if (retire > created) {
            new_expire = min(new_expire,retire);
        }
    }

    m_BlobAccess->SetCurBlobExpire(new_expire);
    SNCSyncEvent* event = new SNCSyncEvent();
    event->event_type = eSyncProlong;
    event->key = m_BlobKey;
    event->orig_server = CNCDistributionConf::GetSelfID();
    event->orig_time = cur_time;
    CNCSyncLog::AddEvent(m_BlobSlot, event);
    CNCPeerControl::MirrorProlong(m_BlobKey, m_BlobSlot,
                                  event->orig_rec_no, cur_time, m_BlobAccess);
}

void
CNCMessageHandler::x_ProlongVersionLife(void)
{
    LOG_CURRENT_FUNCTION
    CSrvTime cur_srv_time = CSrvTime::Current();
    Uint8 cur_time = cur_srv_time.AsUSec();
    int new_expire = int(cur_srv_time.Sec()) + m_BlobAccess->GetCurVersionTTL();
    int old_expire = m_BlobAccess->GetCurVerExpire();
    if (!CNCServer::IsDebugMode()  &&  new_expire - old_expire < m_AppSetup->ttl_unit)
        return;

    m_BlobAccess->SetCurVerExpire(new_expire);
    SNCSyncEvent* event = new SNCSyncEvent();
    event->event_type = eSyncProlong;
    event->key = m_BlobKey;
    event->orig_server = CNCDistributionConf::GetSelfID();
    event->orig_time = cur_time;
    CNCSyncLog::AddEvent(m_BlobSlot, event);
    CNCPeerControl::MirrorProlong(m_BlobKey, m_BlobSlot,
                                  event->orig_rec_no, cur_time, m_BlobAccess);
}

void
CNCMessageHandler::x_CleanCmdResources(void)
{
    LOG_CURRENT_FUNCTION
    int cmd_status = GetDiagCtx()->GetRequestStatus();
    bool print_size = false;
    Uint8 written_size = 0;
    ENCAccessType access_type = eNCCopyCreate;
    if (m_BlobAccess) {
        access_type = m_BlobAccess->GetAccessType();
        if (m_BlobAccess->IsBlobExists()  &&  !x_IsFlagSet(fNoBlobAccessStats)) {
            print_size = true;
            if (access_type == eNCRead  ||  access_type == eNCReadData)
                m_BlobSize = m_BlobAccess->GetCurBlobSize();
            else
                m_BlobSize = m_BlobAccess->GetNewBlobSize();
            if (access_type == eNCReadData)
                GetDiagCtx()->SetBytesWr(m_BlobAccess->GetSizeRead());
            else if (access_type == eNCCreate  ||  access_type == eNCCopyCreate)
                GetDiagCtx()->SetBytesRd(m_BlobSize);
        }
        else if (access_type == eNCCreate) {
            written_size = m_BlobAccess->GetNewBlobSize();
        }
        m_BlobAccess->Release();
        m_BlobAccess = NULL;
    }
    else if (m_ActiveHub) {
        switch (m_ParsedCmd.command->extra.proxy_cmd) {
        case eProxyRead:
        case eProxyReadLast:
            print_size = true;
            m_BlobSize = m_ActiveHub->GetHandler()->GetSizeRd();
            GetDiagCtx()->SetBytesWr(m_BlobSize);
            break;
        case eProxyWrite:
            print_size = true;
            m_BlobSize = m_ActiveHub->GetHandler()->GetSizeWr();
            GetDiagCtx()->SetBytesRd(m_BlobSize);
            break;
        default:
            break;
        }
        m_ActiveHub->GetHandler()->ResetSizeRdWr();
    }

    if (x_IsFlagSet(fRunsInStartedSync)) {
        if (cmd_status == eStatus_OK  ||  x_IsFlagSet(fSyncCmdSuccessful))
            CNCPeriodicSync::SyncCommandFinished(m_SrvId, m_Slot, m_SyncId);
        else
            CNCPeriodicSync::Cancel(m_SrvId, m_Slot, m_SyncId);
    }
    if (x_IsFlagSet(fConfirmOnFinish))
        WriteText("OK:\n");
    Flush();

    if (m_ActiveHub) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
    }
    m_CheckSrvs.clear();
    m_SrvsIndex = 0;
    m_ChunkLen = 0;
    m_LastPeerError.clear();

    if (print_size  &&  (cmd_status == eStatus_OK  ||  m_BlobSize != 0))
        CSrvDiagMsg().PrintExtra().PrintParam("blob_size", m_BlobSize);
    CSrvDiagMsg().StopRequest();

    CSrvTime cmd_len = CSrvTime::Current();
    cmd_len -= m_CmdStartTime;
    Uint8 len_usec = cmd_len.AsUSec();
    CNCStat::CmdFinished(m_ParsedCmd.command->cmd, len_usec, cmd_status);
    if (m_Flags & fComesFromClient) {
        if (access_type == eNCCreate) {
            if (print_size  &&  cmd_status == eStatus_OK)
                CNCStat::ClientBlobWrite(m_BlobSize, len_usec);
            else
                CNCStat::ClientBlobRollback(written_size);
        }
        else if (access_type == eNCReadData) {
            if (print_size)
                CNCStat::ClientBlobRead(m_BlobSize, len_usec);
        }
    }
    ++m_CntCmds;

    if (x_IsFlagSet(fNeedsLowerPriority))
        SetPriority(1);

    m_SendBuff.reset();
    ReleaseDiagCtx();
}

CNCMessageHandler::State
CNCMessageHandler::x_FinishCommand(void)
{
    LOG_CURRENT_FUNCTION
    if (GetDiagCtx()->GetRequestStatus() == eStatus_PUT2Used)
        return &Me::x_CloseCmdAndConn;

    x_CleanCmdResources();
    SetState(&Me::x_ReadCommand);
    SetRunnable();
    return NULL;
}

CNCMessageHandler::State
CNCMessageHandler::x_StartReadingBlob(void)
{
    LOG_CURRENT_FUNCTION
    // Flushing the initial response line that client should receive before it
    // will start writing blob data.
    Flush();
    if (NeedEarlyClose())
        return &Me::x_FinishCommand;
    else
        return &Me::x_ReadBlobSignature;
}

CNCMessageHandler::State
CNCMessageHandler::x_FinishReadingBlob(void)
{
    LOG_CURRENT_FUNCTION
    if (x_IsFlagSet(fReadExactBlobSize)  &&  m_BlobSize != m_Size) {
        GetDiagCtx()->SetRequestStatus(eStatus_CondFailed);
        SRV_LOG(Error, "Too few data for blob size " << m_Size
                       << " (received " << m_BlobSize << " bytes)");
        //abort();
        if (x_IsFlagSet(fConfirmOnFinish)) {
            WriteText(s_MsgForStatus[eStatus_CondFailed]).WriteText("\n");
            x_UnsetFlag(fConfirmOnFinish);
            return &Me::x_FinishCommand;
        }
        else {
            return &Me::x_CloseCmdAndConn;
        }
    }

    if (GetDiagCtx()->GetRequestStatus() != eStatus_OK  &&  !x_IsFlagSet(fCursedPUT2Cmd))
        return &Me::x_FinishCommand;
    if (x_IsFlagSet(fConfirmOnFinish)  &&  NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    // Fill all new event data but not add it to CNCSyncLog until we execute
    // m_BlobAccess->Finalize().
    SNCSyncEvent* write_event = new SNCSyncEvent();
    write_event->event_type = eSyncWrite;
    write_event->key = m_BlobKey;
    if (x_IsFlagSet(fCopyLogEvent)) {
        write_event->orig_time = m_BlobAccess->GetNewBlobCreateTime();
        write_event->orig_server = m_BlobAccess->GetNewCreateServer();
        write_event->orig_rec_no = m_OrigRecNo;
    }
    else {
        CSrvTime cur_srv_time = CSrvTime::Current();
        Uint8 cur_time = cur_srv_time.AsUSec();
        int cur_secs = int(cur_srv_time.Sec());
        m_BlobAccess->SetBlobCreateTime(cur_time);
        if (m_BlobAccess->GetNewBlobExpire() == 0)
            m_BlobAccess->SetNewBlobExpire(cur_secs + m_BlobAccess->GetNewBlobTTL());
        m_BlobAccess->SetNewVerExpire(cur_secs + m_BlobAccess->GetNewVersionTTL());
        m_BlobAccess->SetCreateServer(CNCDistributionConf::GetSelfID(),
                                      CNCBlobStorage::GetNewBlobId());
        write_event->orig_server = CNCDistributionConf::GetSelfID();
        write_event->orig_time = cur_time;
    }

    m_BlobAccess->Finalize();
    if (m_BlobAccess->HasError()) {
        delete write_event;
        GetDiagCtx()->SetRequestStatus(eStatus_ServerError);
        if (!x_IsFlagSet(fConfirmOnFinish))
            return &Me::x_CloseCmdAndConn;

        WriteText("ERR:Error while writing blob\n");
        x_UnsetFlag(fConfirmOnFinish);
        return &Me::x_FinishCommand;
    }

    if (!x_IsFlagSet(fCopyLogEvent)) {
        m_OrigRecNo = CNCSyncLog::AddEvent(m_BlobSlot, write_event);
        CNCPeerControl::MirrorWrite(m_BlobKey, m_BlobSlot,
                                    m_OrigRecNo, m_BlobAccess->GetNewBlobSize());
        // If fCopyLogEvent is not set then this blob comes from client and
        // thus we need to check quorum value before answering to client.
        // If fCopyLogEvent is set then this write comes from other server
        // and we don't care about quorum in this case.
        if (m_Quorum != 1) {
            if (m_Quorum != 0)
                --m_Quorum;
            x_GetCurSlotServers();
            if (!m_ThisServerIsMain  ||  !m_AppSetup->fast_on_main)
                return &Me::x_PutToNextPeer;
        }
    }
    else if (m_OrigRecNo != 0) {
        CNCSyncLog::AddEvent(m_BlobSlot, write_event);
    }
    else {
        // m_OrigRecNo can be 0 if blob comes from another server as a result
        // of synchronization by blob lists. In this case there's no event to
        // link to and thus we don't need to add event to our sync log.
        delete write_event;
    }

    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_CloseOnPeerError(void)
{
    LOG_CURRENT_FUNCTION
    SRV_LOG(Warning, "Error executing command on peer "
        << m_ActiveHub->GetFullPeerName() << ", peer says: "
        << m_ActiveHub->GetErrMsg());
    GetDiagCtx()->SetRequestStatus(eStatus_PeerError);
    return &Me::x_CloseCmdAndConn;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadBlobSignature(void)
{
    LOG_CURRENT_FUNCTION
    Uint4 sig = 0;
    bool has_sig = ReadNumber(&sig);
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;
    if (!has_sig)
        return NULL;

    if (sig == 0x04030201) {
        x_SetFlag(fSwapLengthBytes);
        return &Me::x_ReadBlobChunkLength;
    }
    if (sig == 0x01020304) {
        x_UnsetFlag(fSwapLengthBytes);
        return &Me::x_ReadBlobChunkLength;
    }

    GetDiagCtx()->SetRequestStatus(eStatus_BadCmd);
    SRV_LOG(Error, "Cannot determine the byte order. Got: "
                   << NStr::UIntToString(sig, 0, 16));
    //abort();
    return &Me::x_CloseCmdAndConn;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadBlobChunkLength(void)
{
    LOG_CURRENT_FUNCTION
    if (m_ActiveHub) {
        if (ProxyHadError()) {
            // If we were proxying blob data from client to another server and some
            // error occurred protocol doesn't allow us to anything else but
            // close both connections - to client and to other NC server.
            CSrvSocketTask* active_sock = m_ActiveHub->GetHandler()->GetSocket();
            if (active_sock  &&  active_sock->HasError())
                return &Me::x_CloseOnPeerError;
            else
                return &Me::x_CloseCmdAndConn;
        }
        if (m_ActiveHub->GetStatus() == eNCHubError)
            return &Me::x_CloseOnPeerError;
        else if (m_ActiveHub->GetStatus() != eNCHubCmdInProgress)
            abort();

        if (m_ChunkLen != 0) {
            CNCStat::ClientDataWrite(m_ChunkLen);
            CNCStat::PeerDataRead(m_ChunkLen);
        }
    }

    bool has_chunklen = true;
    if (x_IsFlagSet(fSkipBlobEOF)  &&  m_BlobSize == m_Size) {
        // Workaround for old STRS
        m_ChunkLen = 0xFFFFFFFF;
    }
    else {
        has_chunklen = ReadNumber(&m_ChunkLen);
        if (!has_chunklen  &&  !CanHaveMoreRead()  &&  x_IsFlagSet(fCursedPUT2Cmd)) {
            GetDiagCtx()->SetRequestStatus(eStatus_PUT2Used);
            return &Me::x_FinishReadingBlob;
        }
    }

    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;
    if (!has_chunklen)
        return NULL;

    if (x_IsFlagSet(fSwapLengthBytes))
        m_ChunkLen = CByteSwap::GetInt4((const unsigned char*)&m_ChunkLen);
    if (m_ChunkLen == 0xFFFFFFFF) {
        if (m_ActiveHub) {
            // Data transfer is finished, CNCActiveHandler will wait for response
            // from other NC server and wake us up.
            m_ActiveHub->GetHandler()->SetRunnable();
            return &Me::x_WaitForPeerAnswer;
        }
        return &Me::x_FinishReadingBlob;
    }

    if (!m_BlobAccess  &&  !m_ActiveHub) {
        // We can be here only when expecting fake start of blob writing from old
        // NC server, but for some reason we got non-EOF chunk length.
        GetDiagCtx()->SetRequestStatus(eStatus_BadCmd);
        SRV_LOG(Critical, "Received non-EOF chunk len from peer "
            << m_ActiveHub->GetFullPeerName() << ": " << m_ChunkLen);
        return &Me::x_CloseCmdAndConn;
    }

    if (x_IsFlagSet(fReadExactBlobSize)  &&  m_BlobSize + m_ChunkLen > m_Size) {
        GetDiagCtx()->SetRequestStatus(eStatus_CondFailed);
        SRV_LOG(Error, "Too much data for blob size " << m_Size
                       << " (received at least "
                       << (m_BlobSize + m_ChunkLen) << " bytes)");
        //abort();
        return &Me::x_CloseCmdAndConn;
    }

    if (m_ActiveHub) {
        CSrvSocketTask* active_sock = m_ActiveHub->GetHandler()->GetSocket();
        active_sock->WriteData(&m_ChunkLen, sizeof(m_ChunkLen));
        m_ActiveHub->GetHandler()->AddSizeWr(m_ChunkLen);
        StartProxyTo(active_sock, m_ChunkLen);
        if (IsProxyInProgress())
            return NULL;
        else
            return &Me::x_ReadBlobChunkLength;
    }

    return &Me::x_ReadBlobChunk;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadBlobChunk(void)
{
    LOG_CURRENT_FUNCTION
    while (m_ChunkLen != 0) {
        Uint4 read_len = Uint4(m_BlobAccess->GetWriteMemSize());
        if (m_BlobAccess->HasError()) {
            GetDiagCtx()->SetRequestStatus(eStatus_ServerError);
            if (x_IsFlagSet(fConfirmOnFinish)) {
                WriteText("ERR:Server error\n");
                Flush();
            }
            SetState(&Me::x_CloseCmdAndConn);
            SetRunnable();
            return NULL;
        }
        if (read_len == 0)
            return NULL;
        if (read_len > m_ChunkLen)
            read_len = m_ChunkLen;

        Uint4 n_read = Uint4(Read(m_BlobAccess->GetWriteMemPtr(), read_len));
        if (n_read != 0) {
            if (m_Flags & fComesFromClient)
                CNCStat::ClientDataWrite(n_read);
            else
                CNCStat::PeerDataWrite(n_read);
        }
        if (NeedEarlyClose())
            return &Me::x_CloseCmdAndConn;
        if (n_read == 0)
            return NULL;

        m_BlobAccess->MoveWritePos(n_read);
        m_ChunkLen -= n_read;
        m_BlobSize += n_read;
    }
    return &Me::x_ReadBlobChunkLength;
}

CNCMessageHandler::State
CNCMessageHandler::x_WriteBlobData(void)
{
    LOG_CURRENT_FUNCTION
    while (m_Size != 0) {
        if (m_BlobAccess->GetPosition() == m_BlobAccess->GetCurBlobSize())
            return &Me::x_FinishCommand;

        Uint4 want_read = m_BlobAccess->GetReadMemSize();
        if (m_BlobAccess->HasError()) {
            GetDiagCtx()->SetRequestStatus(eStatus_ServerError);
            return &Me::x_CloseCmdAndConn;
        }
        if (m_Size != Uint8(-1)  &&  m_Size < want_read)
            want_read = Uint4(m_Size);

        Uint4 n_written = Uint4(Write(m_BlobAccess->GetReadMemPtr(), want_read));
        if (n_written != 0) {
            if (m_Flags & fComesFromClient)
                CNCStat::ClientDataRead(n_written);
            else
                CNCStat::PeerDataRead(n_written);
            m_BlobAccess->MoveReadPos(n_written);
            if (m_Size != Uint8(-1))
                m_Size -= n_written;
        }
        if (NeedEarlyClose())
            return &Me::x_CloseCmdAndConn;
        if (n_written == 0)
            return NULL;
    }
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_WriteSendBuff(void)
{
    LOG_CURRENT_FUNCTION
    while (m_SendPos != m_SendBuff->size()) {
        size_t n_written = Write(m_SendBuff->data() + m_SendPos,
                                 m_SendBuff->size() - m_SendPos);

        if (NeedEarlyClose())
            return &Me::x_CloseCmdAndConn;
        if (n_written == 0)
            return NULL;

        m_SendPos += n_written;
    }
    if (strcmp(m_ParsedCmd.command->cmd, "SYNC_START") == 0) {
        return &Me::x_WriteSyncStartExtra;
    }
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_WriteSyncStartExtra(void)
{
    LOG_CURRENT_FUNCTION
    WriteText("PURGE:\n");
    WriteText(CNCBlobAccessor::GetPurgeData()). WriteText(";\n");
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_ProxyToNextPeer(void)
{
    LOG_CURRENT_FUNCTION
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;
    if (m_SrvsIndex < m_CheckSrvs.size()) {
        Uint8 srv_id = m_CheckSrvs[m_SrvsIndex++];
        if (m_ActiveHub)
            abort();
        m_ActiveHub = CNCActiveClientHub::Create(srv_id, this);
        return &Me::x_SendCmdAsProxy;
    }

    // Either there's no servers to execute this command on or all servers were
    // tried and some error was the result from all of them.
    SRV_LOG(Warning, "Got error on all peer servers");
    if (m_LastPeerError.empty())
        m_LastPeerError = "ERR:Cannot execute command on peer servers";
    if (s_StatusForMsg.find(m_LastPeerError) != s_StatusForMsg.end())
        GetDiagCtx()->SetRequestStatus(s_StatusForMsg[m_LastPeerError]);
    else
        GetDiagCtx()->SetRequestStatus(eStatus_PeerError);
    WriteText(m_LastPeerError).WriteText("\n");
    x_UnsetFlag(fConfirmOnFinish);
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_SendCmdAsProxy(void)
{
    LOG_CURRENT_FUNCTION
    if (m_ActiveHub->GetStatus() == eNCHubWaitForConn)
        return NULL;
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        return &Me::x_ProxyToNextPeer;
    }
    if (m_ActiveHub->GetStatus() != eNCHubConnReady)
        abort();
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    switch (m_ParsedCmd.command->extra.proxy_cmd) {
    case eProxyRead:
        m_ActiveHub->GetHandler()->ProxyRead(GetDiagCtx(), m_BlobKey, m_RawBlobPass,
                                             m_BlobVersion, m_StartPos, m_Size,
                                             m_Quorum, m_SearchOnRead, m_ForceLocal, m_AgeMax);
        break;
    case eProxyWrite:
        m_ActiveHub->GetHandler()->ProxyWrite(GetDiagCtx(), m_BlobKey, m_RawBlobPass,
                                              m_BlobVersion, m_BlobTTL, m_Quorum);
        // The only place that needs to go further to a different state.
        return &Me::x_WriteInitWriteResponse;
    case eProxyHasBlob:
        m_ActiveHub->GetHandler()->ProxyHasBlob(GetDiagCtx(), m_BlobKey, m_RawBlobPass,
                                                m_Quorum);
        break;
    case eProxyGetSize:
        m_ActiveHub->GetHandler()->ProxyGetSize(GetDiagCtx(), m_BlobKey, m_RawBlobPass,
                                                m_BlobVersion, m_Quorum,
                                                m_SearchOnRead, m_ForceLocal);
        break;
    case eProxyReadLast:
        m_ActiveHub->GetHandler()->ProxyReadLast(GetDiagCtx(), m_BlobKey, m_RawBlobPass,
                                                 m_StartPos, m_Size, m_Quorum,
                                                 m_SearchOnRead, m_ForceLocal, m_AgeMax);
        break;
    case eProxySetValid:
        m_ActiveHub->GetHandler()->ProxySetValid(GetDiagCtx(), m_BlobKey, m_RawBlobPass,
                                                 m_BlobVersion);
        break;
    case eProxyRemove:
        m_ActiveHub->GetHandler()->ProxyRemove(GetDiagCtx(), m_BlobKey, m_RawBlobPass,
                                               m_BlobVersion, m_Quorum);
        break;
    case eProxyGetMeta:
        m_ActiveHub->GetHandler()->ProxyGetMeta(GetDiagCtx(), m_BlobKey,
                                                m_Quorum, m_ForceLocal);
        break;
    case eProxyProlong:
        m_ActiveHub->GetHandler()->ProxyProlong(GetDiagCtx(), m_BlobKey, m_RawBlobPass,
                                                m_BlobTTL, m_Quorum,
                                                m_SearchOnRead, m_ForceLocal);
        break;
    default:
        abort();
    }

    return &Me::x_WaitForPeerAnswer;
}

CNCMessageHandler::State
CNCMessageHandler::x_WaitForPeerAnswer(void)
{
    LOG_CURRENT_FUNCTION
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;
    if (m_ActiveHub->GetStatus() == eNCHubCmdInProgress)
        return NULL;

    if (m_ActiveHub->GetStatus() == eNCHubError) {
        if (m_ActiveHub->GetHandler()->GotClientResponse())
            return &Me::x_CloseOnPeerError;

        m_LastPeerError = m_ActiveHub->GetErrMsg();
        SRV_LOG(Warning, "Error executing command on peer "
            << m_ActiveHub->GetFullPeerName() << ", peer says: "  << m_LastPeerError);
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        return &Me::x_ProxyToNextPeer;
    }
    if (m_ActiveHub->GetStatus() != eNCHubSuccess)
        abort();

    const string& err_msg = m_ActiveHub->GetErrMsg();
    if (!x_IsFlagSet(fConfirmOnFinish) || err_msg.empty())
        return &Me::x_FinishCommand;

    if (s_StatusForMsg.find(err_msg) != s_StatusForMsg.end()) {
        int status = s_StatusForMsg[err_msg];
        if (status == eStatus_NotFound)
            return &Me::x_ReportBlobNotFound;
        GetDiagCtx()->SetRequestStatus(status);
    }
    WriteText(err_msg).WriteText("\n");
    x_UnsetFlag(fConfirmOnFinish);
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadMetaNextPeer(void)
{
    LOG_CURRENT_FUNCTION
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;
    if (m_SrvsIndex >= m_CheckSrvs.size())
        return &Me::x_ExecuteOnLatestSrvId;

    Uint8 srv_id = m_CheckSrvs[m_SrvsIndex++];
    if (m_ActiveHub)
        abort();
    m_ActiveHub = CNCActiveClientHub::Create(srv_id, this);
    return &Me::x_SendGetMetaCmd;
}

CNCMessageHandler::State
CNCMessageHandler::x_SendGetMetaCmd(void)
{
    LOG_CURRENT_FUNCTION
    if (m_ActiveHub->GetStatus() == eNCHubWaitForConn)
        return NULL;
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        return &Me::x_ReadMetaNextPeer;
    }
    if (m_ActiveHub->GetStatus() != eNCHubConnReady)
        abort();
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    m_ActiveHub->GetHandler()->SearchMeta(GetDiagCtx(), m_BlobKey);
    return &Me::x_ReadMetaResults;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadMetaResults(void)
{
    LOG_CURRENT_FUNCTION
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;
    ENCClientHubStatus status = m_ActiveHub->GetStatus();
    if (status == eNCHubCmdInProgress)
        return NULL;
    if (status == eNCHubError)
        goto results_processed;
    if (status != eNCHubSuccess)
        abort();

    CNCActiveHandler* handler;
    handler = m_ActiveHub->GetHandler();
    const SNCBlobSummary* cur_blob_sum;
    cur_blob_sum = &handler->GetBlobSummary();
    bool cur_exist;
    cur_exist = handler->IsBlobExists();
    if (!cur_exist  &&  !x_IsFlagSet(fPeerFindExistsOnly))
        goto results_processed;

    if (cur_exist  &&  x_IsFlagSet(fPeerFindExistsOnly)) {
        m_LatestExist = true;
        goto meta_search_finished;
    }
    if (cur_exist  &&  (!m_LatestExist  ||  m_LatestBlobSum->isOlder(*cur_blob_sum)))
    {
        m_LatestExist = true;
        m_LatestSrvId = m_CheckSrvs[m_SrvsIndex - 1];
        *m_LatestBlobSum = *cur_blob_sum;
    }
    if (m_Quorum == 1)
        goto meta_search_finished;
    if (m_Quorum != 0)
        --m_Quorum;

results_processed:
    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    return &Me::x_ReadMetaNextPeer;

meta_search_finished:
    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    m_CheckSrvs.clear();
    m_SrvsIndex = 0;
    return &Me::x_ExecuteOnLatestSrvId;
}

CNCMessageHandler::State
CNCMessageHandler::x_ExecuteOnLatestSrvId(void)
{
    LOG_CURRENT_FUNCTION
    if (m_LatestExist) {
        // if max age specified, check age
        if (m_AgeMax != 0) {
            CSrvTime cur_srv_time = CSrvTime::Current();
            unsigned int vttl = m_BlobAccess->GetCurVersionTTL();
            Uint8 creation = vttl != 0 ?
// see x_ProlongVersionLife
                (m_BlobAccess->GetCurVerExpire() - vttl) :
                (m_LatestBlobSum->create_time / kUSecsPerSecond);
            m_AgeCur =  Uint8(cur_srv_time.Sec()) - creation;
            if (m_AgeCur > m_AgeMax) {
                return &Me::x_ReportBlobNotFound;
            }
        }
    }
    if (!m_PrevCache.empty() && m_BlobAccess->IsPurged(m_PrevCache)) {
        if (x_IsFlagSet(fPeerFindExistsOnly)) {
            m_LatestExist = false;
            return m_CmdProcessor;
        }
        return &Me::x_ReportBlobNotFound;
    }
    if (x_IsFlagSet(fPeerFindExistsOnly))
        return m_CmdProcessor;
    if (m_LatestSrvId == CNCDistributionConf::GetSelfID()) {
        if (m_LatestExist  &&  !m_BlobAccess->IsCurBlobExpired())
            return m_CmdProcessor;
        else
            return &Me::x_ReportBlobNotFound;
    }

    CSrvDiagMsg().PrintExtra().PrintParam("proxy", "1");
    // Changing parameters that will go to other server: that server have to
    // execute command locally, without quorum (quorum equal to 1), and without
    // searching on other servers.
    m_Quorum = 1;
    m_SearchOnRead = false;
    m_ForceLocal = true;
    m_CheckSrvs.push_back(m_LatestSrvId);
    return &Me::x_ProxyToNextPeer;
}

CNCMessageHandler::State
CNCMessageHandler::x_PutToNextPeer(void)
{
    LOG_CURRENT_FUNCTION
    if (m_SrvsIndex >= m_CheckSrvs.size()  ||  NeedEarlyClose())
        return &Me::x_FinishCommand;

    Uint8 srv_id = m_CheckSrvs[m_SrvsIndex++];
    if (m_ActiveHub)
        abort();
    m_ActiveHub = CNCActiveClientHub::Create(srv_id, this);
    return &Me::x_SendPutToPeerCmd;
}

CNCMessageHandler::State
CNCMessageHandler::x_SendPutToPeerCmd(void)
{
    LOG_CURRENT_FUNCTION
    if (m_ActiveHub->GetStatus() == eNCHubWaitForConn)
        return NULL;
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        return &Me::x_PutToNextPeer;
    }
    if (m_ActiveHub->GetStatus() != eNCHubConnReady)
        abort();
    if (NeedEarlyClose())
        return &Me::x_FinishCommand;

    m_ActiveHub->GetHandler()->CopyPut(GetDiagCtx(), m_BlobKey, m_BlobSlot, m_OrigRecNo);
    return &Me::x_ReadPutResults;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadPutResults(void)
{
    LOG_CURRENT_FUNCTION
    if (NeedEarlyClose())
        return &Me::x_FinishCommand;
    if (m_ActiveHub->GetStatus() == eNCHubCmdInProgress)
        return NULL;
    if (m_ActiveHub->GetStatus() == eNCHubError)
        goto results_processed;
    if (m_ActiveHub->GetStatus() != eNCHubSuccess)
        abort();

    if (m_Quorum == 1)
        return &Me::x_FinishCommand;
    if (m_Quorum != 0)
        --m_Quorum;

results_processed:
    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    return &Me::x_PutToNextPeer;
}

CNCMessageHandler::State
CNCMessageHandler::x_PurgeToNextPeer(void)
{
    LOG_CURRENT_FUNCTION
    if (m_SrvsIndex >= m_CheckSrvs.size()  ||  NeedEarlyClose())
        return &Me::x_FinishCommand;

    Uint8 srv_id = m_CheckSrvs[m_SrvsIndex++];
    if (m_ActiveHub)
        abort();
    m_ActiveHub = CNCActiveClientHub::Create(srv_id, this);
    return &Me::x_SendPurgeToPeerCmd;
}

CNCMessageHandler::State
CNCMessageHandler::x_SendPurgeToPeerCmd(void)
{
    LOG_CURRENT_FUNCTION
    if (m_ActiveHub->GetStatus() == eNCHubWaitForConn)
        return NULL;
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        return &Me::x_PurgeToNextPeer;
    }
    if (m_ActiveHub->GetStatus() != eNCHubConnReady)
        abort();
    if (NeedEarlyClose())
        return &Me::x_FinishCommand;

    m_ActiveHub->GetHandler()->CopyPurge(GetDiagCtx(), m_PrevCache, m_CmdStartTime);
    return &Me::x_ReadPurgeResults;
}

CNCMessageHandler::State
CNCMessageHandler::x_ReadPurgeResults(void)
{
    LOG_CURRENT_FUNCTION
    if (NeedEarlyClose())
        return &Me::x_FinishCommand;
    if (m_ActiveHub->GetStatus() == eNCHubCmdInProgress)
        return NULL;
    if (m_ActiveHub->GetStatus() == eNCHubError)
        goto results_processed;
    if (m_ActiveHub->GetStatus() != eNCHubSuccess)
        abort();

results_processed:
    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    return &Me::x_PurgeToNextPeer;
}


inline unsigned int
CNCMessageHandler::x_GetBlobTTL(void)
{
    LOG_CURRENT_FUNCTION
    return m_BlobTTL != 0 ? min(m_BlobTTL,m_AppSetup->max_ttl) : m_AppSetup->blob_ttl;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_Health(void)
{
    LOG_CURRENT_FUNCTION
    const char* health_coeff = "1";
    if (CNCBlobStorage::NeedStopWrite())
        health_coeff = "0";
    else if (!CNCServer::IsCachingComplete())
        health_coeff = "0.1";
    else if (!CNCServer::IsInitiallySynced())
        health_coeff = "0.5";
    WriteText("OK:HEALTH_COEFF=").WriteText(health_coeff).WriteText("\n");
    WriteText("OK:UP_TIME=").WriteNumber(CNCServer::GetUpTime()).WriteText("\n");
    WriteText("OK:CACHING_COMPLETE=").WriteText(CNCServer::IsCachingComplete()? "yes": "no").WriteText("\n");
    WriteText("OK:INITIALLY_SYNCED=").WriteText(CNCServer::IsInitiallySynced()? "yes": "no").WriteText("\n");
    Uint8 free_space = CNCBlobStorage::GetDiskFree();
    Uint8 allowed_size = CNCBlobStorage::GetAllowedDBSize(free_space);
    WriteText("OK:DISK_FREE=").WriteNumber(free_space).WriteText("\n");
    WriteText("OK:DISK_LIMIT=").WriteNumber(allowed_size).WriteText("\n");
    WriteText("OK:DISK_USED=").WriteNumber(CNCBlobStorage::GetDBSize()).WriteText("\n");
    WriteText("OK:DISK_LIMIT_ALERT=").WriteText(CNCBlobStorage::IsDBSizeAlert()? "yes": "no").WriteText("\n");
    WriteText("OK:N_DB_FILES=").WriteNumber(CNCBlobStorage::GetNDBFiles()).WriteText("\n");
    WriteText("OK:COPY_QUEUE_SIZE=").WriteNumber(CNCPeerControl::GetMirrorQueueSize()).WriteText("\n");

    typedef map<Uint8, string> TPeers;
    const TPeers& peers(CNCDistributionConf::GetPeers());
    ITERATE(TPeers, it_peer, peers) {
        WriteText("OK:QUEUE_SIZE_").WriteNumber(it_peer->first).WriteText("=").WriteNumber(CNCPeerControl::GetMirrorQueueSize(it_peer->first)).WriteText("\n");
    }
    WriteText("OK:SYNC_LOG_SIZE=").WriteNumber(CNCSyncLog::GetLogSize()).WriteText("\n");

    WriteText("OK:END\n");
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_Shutdown(void)
{
    LOG_CURRENT_FUNCTION
    TNSProtoParams& param = m_ParsedCmd.params;
    if (param.find("drain") != param.end() && param["drain"] != "0") {
        CNCBlobStorage::SetDraining(true);
    } else {
        CTaskServer::RequestShutdown(eSrvSlowShutdown);
    }
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_Version(void)
{
    LOG_CURRENT_FUNCTION
//    WriteText("OK:").WriteText(NETCACHED_HUMAN_VERSION).WriteText("\n");
    WriteText("OK:server_version=" NETCACHED_VERSION
              "&storage_version="  NETCACHED_STORAGE_VERSION
              "&protocol_version=" NETCACHED_PROTOCOL_VERSION
              "&build_date=" + NStr::URLEncode(NETCACHED_BUILD_DATE) +
              "&mirrored=" + (NStr::BoolToString( !CNCDistributionConf::GetPeers().empty())))
        .WriteText("\n");
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_GetConfig(void)
{
    LOG_CURRENT_FUNCTION
    TNSProtoParams& params = m_ParsedCmd.params;
    if (params.find("section") != params.end()) {
        string section(params["section"]);
        WriteText("{\"").WriteText(section).WriteText("\": {\n\"section\": \"");
        WriteText(section).WriteText("\"");
        if (section == "netcache") {
            TStringMap client;
            client = m_ClientParams;
            if (params.find("port") != params.end()) {
                client["port"] = params["port"];
            }
            if (params.find("cache") != params.end()) {
                client["cache"] = params["cache"];
            }
            ITERATE(TStringMap, p, client) {
                string iss("\": \""), eol(",\n\"");
                WriteText(eol).WriteText("__client__").WriteText(p->first).WriteText(iss).WriteText(p->second).WriteText("\"");
            }
            CNCServer::WriteAppSetup(*this, CNCServer::GetAppSetup(client));
//        } else if (section == "task_server") {
        } else if (section == "storage") {
            CNCBlobStorage::WriteSetup(*this);
        } else if (section == "mirror") {
            CNCDistributionConf::WriteSetup(*this);
        } else if (section == "stat") {
            CSrvRef<CNCStat> stat = CNCStat::GetStat("life", false);
            if (stat) {
                stat->PrintState(*this);
            }
        } else {
            WriteText(",\n\"error\": \"Unknown section name, valid names: netcache, storage, mirror\"");
        }
        WriteText("\n}}");
    } else {
        CNcbiOstrstream str;
        CTaskServer::GetConfRegistry().Write(str);
        string conf = CNcbiOstrstreamToString(str);
        WriteText(conf);
    }
    WriteText("\nOK:END\n");
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_GetStat(void)
{
    LOG_CURRENT_FUNCTION
    CSrvRef<CNCStat> stat = CNCStat::GetStat(m_StatType, m_StatPrev);
    if (!stat) {
        WriteText("ERR:Unknown statistics type\n");
        GetDiagCtx()->SetRequestStatus(eStatus_BadCmd);
    }
    else {
        stat->PrintToSocket(this);
        WriteText("OK:END\n");
    }
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_Put(void)
{
    LOG_CURRENT_FUNCTION
    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
    m_BlobAccess->SetVersionTTL(0);
    m_BlobAccess->SetBlobVersion(0);
    WriteText("OK:ID:").WriteText(m_RawKey).WriteText("\n");
    return &Me::x_StartReadingBlob;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_Get(void)
{
    LOG_CURRENT_FUNCTION
    if (m_AppSetup->prolong_on_read)
        x_ProlongBlobDeadTime(m_BlobAccess->GetCurBlobTTL());

    Uint8 blob_size = m_BlobAccess->GetCurBlobSize();
    if (blob_size < m_StartPos)
        blob_size = 0;
    else
        blob_size -= m_StartPos;
    if (m_Size != Uint8(-1)) {
        if (m_Size < blob_size)
            blob_size = m_Size;
        else
            m_Size = blob_size;
    }

    WriteText("OK:BLOB found. SIZE=").WriteNumber(blob_size);
    if (m_AgeCur != 0) {
        WriteText(", AGE=").WriteNumber(m_AgeCur);
    }
    WriteText("\n");

    if (blob_size == 0)
        return &Me::x_FinishCommand;
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    m_BlobAccess->SetPosition(m_StartPos);
    return &Me::x_WriteBlobData;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_GetLast(void)
{
    LOG_CURRENT_FUNCTION
    if (m_AppSetup->prolong_on_read)
        x_ProlongBlobDeadTime(m_BlobAccess->GetCurBlobTTL());

    Uint8 blob_size = m_BlobAccess->GetCurBlobSize();
    if (blob_size < m_StartPos)
        blob_size = 0;
    else
        blob_size -= m_StartPos;
    if (m_Size != Uint8(-1)  &&  m_Size < blob_size)
        blob_size = m_Size;

    WriteText("OK:BLOB found. SIZE=").WriteNumber(blob_size);
    WriteText(", VER=").WriteNumber(m_BlobAccess->GetCurBlobVersion());
    WriteText(", VALID=").WriteText(m_BlobAccess->IsCurVerExpired()? "false": "true");
    if (m_AgeCur != 0) {
        WriteText(", AGE=").WriteNumber(m_AgeCur);
    }
    WriteText("\n");

    if (blob_size == 0)
        return &Me::x_FinishCommand;
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    m_BlobAccess->SetPosition(m_StartPos);
    return &Me::x_WriteBlobData;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_SetValid(void)
{
    LOG_CURRENT_FUNCTION
    if (!m_BlobAccess->IsBlobExists()  ||  m_BlobAccess->IsCurBlobExpired())
        return &Me::x_ReportBlobNotFound;

    if (m_BlobAccess->GetCurBlobVersion() != m_BlobVersion) {
        GetDiagCtx()->SetRequestStatus(eStatus_RaceCond);
        WriteText("OK:BLOB was changed.\n");
    }
    else {
        x_ProlongVersionLife();
        WriteText("OK:\n");
    }
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_GetSize(void)
{
    LOG_CURRENT_FUNCTION
    if (m_AppSetup->prolong_on_read)
        x_ProlongBlobDeadTime(m_BlobAccess->GetCurBlobTTL());

    Uint8 size = m_BlobAccess->GetCurBlobSize();
    WriteText("OK:").WriteNumber(size).WriteText("\n");

    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_Prolong(void)
{
    LOG_CURRENT_FUNCTION
    bool ttl_overrun = false;
#if 0
    if (m_BlobTTL <= m_BlobAccess->GetCurBlobTTL())
        ttl_overrun = false;
    else {
        m_BlobTTL = m_BlobAccess->GetCurBlobTTL();
        ttl_overrun = true;
    }
#else
    if (m_BlobTTL > m_AppSetup->max_ttl) {
        m_BlobTTL = m_AppSetup->max_ttl;
        ttl_overrun = true;
    }
#endif

    x_ProlongBlobDeadTime(m_BlobTTL);
    // Distinguish "PROLONG" vs "PROXY_PROLONG".
    // The latter has the fConfirmOnFinish flag and
    // doesn't require an explicit confirmation.
    if (x_IsFlagSet(fComesFromClient)) {
        if (!ttl_overrun)
            WriteText("OK:\n");
        else
            WriteText("OK:WARNING:Capped the requested TTL for '").
                    WriteText(m_RawKey).WriteText("' at ").
                    WriteNumber(m_BlobTTL).WriteText(" seconds.\n");
    }
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_HasBlob(void)
{
    LOG_CURRENT_FUNCTION
    bool exist = m_LatestExist  &&  m_LatestBlobSum->expire > CSrvTime::CurSecs();
    if (!exist)
        GetDiagCtx()->SetRequestStatus(eStatus_NotFound);
    WriteText("OK:").WriteNumber(int(exist)).WriteText("\n");
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_Remove(void)
{
    LOG_CURRENT_FUNCTION
    // We delete blob only from client point of view. From our POV we create
    // new blob version with expiration time one second in the past. This is
    // necessary for proper synchronization with other servers (if we delete
    // blob and then will synchronize using blob lists then other server will
    // copy the blob back to us). And we can create this new blob version
    // safely even when we haven't completed yet the initial synchronization.
    if ((!m_BlobAccess->IsBlobExists()  ||  m_BlobAccess->IsCurBlobExpired())
        &&  CNCServer::IsInitiallySynced())
    {
        return &Me::x_FinishCommand;
    }

    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
    m_BlobAccess->SetBlobVersion(m_BlobVersion);
    int expire = CSrvTime::CurSecs() - 1;
    unsigned int ttl = m_BlobAccess->GetNewBlobTTL();
    if (m_BlobAccess->IsBlobExists()  &&  m_BlobAccess->GetCurBlobTTL() > ttl)
        ttl = m_BlobAccess->GetCurBlobTTL();
    m_BlobAccess->SetNewBlobExpire(expire, expire + ttl + 1);
    return &Me::x_FinishReadingBlob;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_IC_Store(void)
{
    LOG_CURRENT_FUNCTION
    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
    m_BlobAccess->SetVersionTTL(m_AppSetup->ver_ttl);
    m_BlobAccess->SetBlobVersion(m_BlobVersion);
    WriteText("OK:\n");
    if (m_Size == 0)
        return &Me::x_FinishCommand;
    return &Me::x_StartReadingBlob;
}

void
CNCMessageHandler::x_WriteFullBlobsList(void)
{
    LOG_CURRENT_FUNCTION
    TNCBlobSumList blobs_list;
    CNCBlobStorage::GetFullBlobsList(m_Slot, blobs_list);
    m_SendBuff.reset(new TNCBufferType());
    m_SendBuff->reserve_mem(blobs_list.size() * 200);
    NON_CONST_ITERATE(TNCBlobSumList, it_blob, blobs_list) {
        if (NeedEarlyClose())
            goto error_return;

        const string& key = it_blob->first;
        SNCBlobSummary* blob_sum = it_blob->second;
        Uint2 key_size = Uint2(key.size());
        m_SendBuff->append(&key_size, sizeof(key_size));
        m_SendBuff->append(key.data(), key_size);
        m_SendBuff->append(&blob_sum->create_time, sizeof(blob_sum->create_time));
        m_SendBuff->append(&blob_sum->create_server, sizeof(blob_sum->create_server));
        m_SendBuff->append(&blob_sum->create_id, sizeof(blob_sum->create_id));
        m_SendBuff->append(&blob_sum->dead_time, sizeof(blob_sum->dead_time));
        m_SendBuff->append(&blob_sum->expire, sizeof(blob_sum->expire));
        m_SendBuff->append(&blob_sum->ver_expire, sizeof(blob_sum->ver_expire));
        delete blob_sum;
        it_blob->second = NULL;
        blob_sum = NULL;
    }
    return;

error_return:
    ITERATE(TNCBlobSumList, it_blob, blobs_list) {
        if (it_blob->second)
            delete it_blob->second;
    }
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_SyncStart(void)
{
    LOG_CURRENT_FUNCTION
    TReducedSyncEvents sync_events;
    ESyncInitiateResult sync_res = CNCPeriodicSync::Initiate(m_SrvId, m_Slot,
                                                &m_LocalRecNo, &m_RemoteRecNo,
                                                &sync_events, &m_SyncId);
    if (sync_res == eCrossSynced) {
        GetDiagCtx()->SetRequestStatus(eStatus_CrossSync);
        WriteText("OK:CROSS_SYNC,SIZE=0\n");
        return &Me::x_FinishCommand;
    }
    else if (sync_res == eServerBusy) {
        GetDiagCtx()->SetRequestStatus(eStatus_SyncBusy);
        WriteText("OK:IN_PROGRESS,SIZE=0\n");
        return &Me::x_FinishCommand;
    }
    else if (sync_res == eUnknownServer) {
        GetDiagCtx()->SetRequestStatus(eStatus_NotAllowed);
        WriteText("OK:SIZE=0, NEED_ABORT2\n");
        if (CNCStat::AddUnknownServer(m_SrvId)) {
            SRV_LOG(Warning, "SYNC_START request from unknown server " << m_SrvId);
        }
        return &Me::x_FinishCommand;
    }

    // Set fRunsInStartedSync flag so that x_CleanCmdResources() could properly call
    // CNCPeriodicSync::SyncCommandFinished() or CNCPeriodicSync::Cancel().
    x_SetFlag(fRunsInStartedSync);
    string result;
    if (sync_res == eProceedWithEvents) {
        m_SendBuff.reset(new TNCBufferType());
        m_SendBuff->reserve_mem(sync_events.size() * 200);
        ITERATE(TReducedSyncEvents, it_evt, sync_events) {
            if (NeedEarlyClose())
                break;

            const SBlobEvent& blob_evt = it_evt->second;
            for (int i = 0; i < 2; ++i) {
                SNCSyncEvent* evt = (i == 0? blob_evt.wr_or_rm_event: blob_evt.prolong_event);
                if (!evt)
                    continue;
                Uint2 key_size = Uint2(evt->key.size());
                m_SendBuff->append(&key_size, sizeof(key_size));
                m_SendBuff->append(evt->key.data(), key_size);
                char c = char(evt->event_type);
                m_SendBuff->append(&c, 1);
                m_SendBuff->append(&evt->rec_no, sizeof(evt->rec_no));
                m_SendBuff->append(&evt->local_time, sizeof(evt->local_time));
                m_SendBuff->append(&evt->orig_rec_no, sizeof(evt->orig_rec_no));
                m_SendBuff->append(&evt->orig_server, sizeof(evt->orig_server));
                m_SendBuff->append(&evt->orig_time, sizeof(evt->orig_time));
            }
        }
    }
    else {
        _ASSERT(sync_res == eProceedWithBlobs);
        m_LocalRecNo = CNCSyncLog::GetCurrentRecNo(m_Slot);
        x_WriteFullBlobsList();
        GetDiagCtx()->SetRequestStatus(eStatus_SyncBList);
        x_SetFlag(fSyncCmdSuccessful);
        result += "ALL_BLOBS,";
    }

    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    WriteText("OK:").WriteText(result);
    WriteText("SIZE=").WriteNumber(m_SendBuff->size());
    WriteText(" ").WriteNumber(m_LocalRecNo);
    WriteText(" ").WriteNumber(m_RemoteRecNo);
    WriteText("\n");
    m_SendPos = 0;
    return &Me::x_WriteSendBuff;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_SyncBlobsList(void)
{
    LOG_CURRENT_FUNCTION
    CNCPeriodicSync::MarkCurSyncByBlobs(m_SrvId, m_Slot, m_SyncId);
    Uint8 rec_no = CNCSyncLog::GetCurrentRecNo(m_Slot);
    x_WriteFullBlobsList();

    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    WriteText("OK:SIZE=").WriteNumber(m_SendBuff->size());
    WriteText(" ").WriteNumber(rec_no);
    WriteText("\n");
    m_SendPos = 0;
    return &Me::x_WriteSendBuff;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_CopyPut(void)
{
    LOG_CURRENT_FUNCTION
    m_CopyBlobInfo->ttl = m_BlobTTL;
    m_CopyBlobInfo->password = m_BlobPass;
    m_CopyBlobInfo->blob_ver = m_BlobVersion;
    bool need_read_blob = m_BlobAccess->ReplaceBlobInfo(*m_CopyBlobInfo);
    if (need_read_blob) {
        WriteText("OK:\n");
    }
    else {
        GetDiagCtx()->SetRequestStatus(eStatus_NewerBlob);
        x_SetFlag(fNoBlobAccessStats);
        x_SetFlag(fSyncCmdSuccessful);
        x_UnsetFlag(fCopyLogEvent);
        WriteText("OK:HAVE_NEWER1\n");
    }
    // Old NC servers (those which used CNetCacheAPI instead of
    // CNCActiveHandler) always started to write blob data in SYNC_PUT and
    // COPY_PUT even when we responded to them HAVE_NEWER. To avoid breaking
    // the protocol we need to read from them those fake blob writes. So for
    // old NC servers when we answered HAVE_NEWER we'll go to x_StartReadingBlob,
    // for newer ones we'll go to x_FinishCommand.
    if (!need_read_blob  &&  m_CmdVersion != 0) {
        x_UnsetFlag(fConfirmOnFinish);
        x_UnsetFlag(fReadExactBlobSize);
        return &Me::x_FinishCommand;
    }

    return &Me::x_StartReadingBlob;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_CopyProlong(void)
{
    LOG_CURRENT_FUNCTION
    if (!m_BlobAccess->IsBlobExists()) {
        x_SetFlag(fSyncCmdSuccessful);
        return &Me::x_ReportBlobNotFound;
    }

    if (m_BlobAccess->GetCurBlobCreateTime() == m_CopyBlobInfo->create_time
        &&  m_BlobAccess->GetCurCreateServer() == m_CopyBlobInfo->create_server
        &&  m_BlobAccess->GetCurCreateId() == m_CopyBlobInfo->create_id)
    {
        bool need_event = false;
        if (m_BlobAccess->GetCurBlobExpire() < m_CopyBlobInfo->expire) {
            m_BlobAccess->SetCurBlobExpire(m_CopyBlobInfo->expire,
                                           m_CopyBlobInfo->dead_time);
            need_event = true;
        }
        if (m_BlobAccess->GetCurVerExpire() < m_CopyBlobInfo->ver_expire) {
            m_BlobAccess->SetCurVerExpire(m_CopyBlobInfo->ver_expire);
            need_event = true;
        }

        // m_OrigRecNo can be 0 if prolong happens as a result of synchronization
        // using blob lists. In this case there's no event to link to and thus
        // no need to create event here.
        if (need_event  &&  m_OrigRecNo != 0) {
            SNCSyncEvent* event = new SNCSyncEvent();
            event->event_type = eSyncProlong;
            event->key = m_BlobKey;
            event->orig_server = m_OrigSrvId;
            event->orig_time = m_OrigTime;
            event->orig_rec_no = m_OrigRecNo;
            CNCSyncLog::AddEvent(m_BlobSlot, event);
        }
    }
    else {
        GetDiagCtx()->SetRequestStatus(eStatus_NewerBlob);
        x_SetFlag(fSyncCmdSuccessful);
    }
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_SyncGet(void)
{
    LOG_CURRENT_FUNCTION
    if (!m_BlobAccess->IsBlobExists()) {
        x_SetFlag(fSyncCmdSuccessful);
        return &Me::x_ReportBlobNotFound;
    }

    bool need_send = true;
    if (m_OrigTime != m_BlobAccess->GetCurBlobCreateTime()) {
        need_send = false;
    }
    else if (m_BlobAccess->GetCurBlobCreateTime() < m_CopyBlobInfo->create_time) {
        need_send = false;
    }
    else if (m_BlobAccess->GetCurBlobCreateTime() == m_CopyBlobInfo->create_time) {
        if (m_BlobAccess->GetCurCreateServer() < m_CopyBlobInfo->create_server) {
            need_send = false;
        }
        else if (m_BlobAccess->GetCurCreateServer() == m_CopyBlobInfo->create_server
                 &&  m_BlobAccess->GetCurCreateId() <= m_CopyBlobInfo->create_id)
        {
            need_send = false;
        }
    }
    if (!need_send) {
        GetDiagCtx()->SetRequestStatus(eStatus_NewerBlob);
        x_SetFlag(fSyncCmdSuccessful);
        WriteText("OK:SIZE=0, HAVE_NEWER\n");
        return &Me::x_FinishCommand;
    }

    WriteText("OK:SIZE=").WriteNumber(m_BlobAccess->GetCurBlobSize());
    WriteText(" ").WriteNumber(m_BlobAccess->GetCurBlobVersion());
    WriteText(" \"").WriteText(m_BlobAccess->GetCurPassword());
    WriteText("\" ").WriteNumber(m_BlobAccess->GetCurBlobCreateTime());
    WriteText(" ").WriteNumber(Uint4(m_BlobAccess->GetCurBlobTTL()));
    WriteText(" ").WriteNumber(m_BlobAccess->GetCurBlobDeadTime());
    WriteText(" ").WriteNumber(m_BlobAccess->GetCurBlobExpire());
    WriteText(" ").WriteNumber(Uint4(m_BlobAccess->GetCurVersionTTL()));
    WriteText(" ").WriteNumber(m_BlobAccess->GetCurVerExpire());
    WriteText(" ").WriteNumber(m_BlobAccess->GetCurCreateServer());
    WriteText(" ").WriteNumber(m_BlobAccess->GetCurCreateId());
    WriteText("\n");

    if (m_BlobAccess->GetCurBlobSize() == 0)
        return &Me::x_FinishCommand;
    if (NeedEarlyClose())
        return &Me::x_CloseCmdAndConn;

    m_BlobAccess->SetPosition(0);
    return &Me::x_WriteBlobData;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_SyncProlongInfo(void)
{
    LOG_CURRENT_FUNCTION
    if (!m_BlobAccess->IsBlobExists()) {
        x_SetFlag(fSyncCmdSuccessful);
        return &Me::x_ReportBlobNotFound;
    }

    WriteText("OK:SIZE=0 ");
    WriteNumber(m_BlobAccess->GetCurBlobCreateTime()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurCreateServer()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurCreateId()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurBlobDeadTime()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurBlobExpire()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurVerExpire());
    WriteText("\n");

    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_SyncCommit(void)
{
    LOG_CURRENT_FUNCTION
    CNCPeriodicSync::Commit(m_SrvId, m_Slot, m_SyncId, m_LocalRecNo, m_RemoteRecNo);
    x_UnsetFlag(fRunsInStartedSync);
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_SyncCancel(void)
{
    LOG_CURRENT_FUNCTION
    CNCPeriodicSync::Cancel(m_SrvId, m_Slot, m_SyncId);
    x_UnsetFlag(fRunsInStartedSync);
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_GetMeta(void)
{
    LOG_CURRENT_FUNCTION
    m_SendBuff.reset(new TNCBufferType());
    m_SendBuff->reserve_mem(1024);
    string tmp;
    char time_buf[50];

    tmp = "OK:Slot: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::UIntToString(m_BlobSlot);
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Write time: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    Uint8 create_time = m_BlobAccess->GetCurBlobCreateTime();
    CSrvTime t;
    t.Sec() = time_t(create_time / kUSecsPerSecond);
    t.NSec() = (create_time % kUSecsPerSecond) * 1000;
    t.Print(time_buf, CSrvTime::eFmtHumanUSecs);
    m_SendBuff->append(time_buf, strlen(time_buf));

    tmp = "\nOK:Control server: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    Uint8 create_server = m_BlobAccess->GetCurCreateServer();
    tmp = CTaskServer::GetHostByIP(Uint4(create_server >> 32));
    m_SendBuff->append(tmp.data(), tmp.size());
    m_SendBuff->append(":", 1);
    tmp = NStr::UIntToString(Uint4(create_server));
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Control id: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::Int8ToString(m_BlobAccess->GetCurCreateId());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:TTL: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::UIntToString(m_BlobAccess->GetCurBlobTTL());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Expire: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    t.Sec() = m_BlobAccess->GetCurBlobExpire();
    t.Print(time_buf, CSrvTime::eFmtHumanSeconds);
    m_SendBuff->append(time_buf, strlen(time_buf));

    tmp = "\nOK:Size: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::UInt8ToString(m_BlobAccess->GetCurBlobSize());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Password: '";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = m_BlobAccess->GetCurPassword();
    if (!tmp.empty()) {
        tmp = "yes";
        m_SendBuff->append(tmp.data(), tmp.size());
    }
    m_SendBuff->append("'", 1);

    tmp = "\nOK:Version: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::IntToString(m_BlobAccess->GetCurBlobVersion());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Version's TTL: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::UIntToString(m_BlobAccess->GetCurVersionTTL());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Version expire: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    t.Sec() = m_BlobAccess->GetCurVerExpire();
    t.Print(time_buf, CSrvTime::eFmtHumanSeconds);
    m_SendBuff->append(time_buf, strlen(time_buf));

    tmp = "\nOK:END\n";
    m_SendBuff->append(tmp.data(), tmp.size());


    WriteText("OK:SIZE=").WriteNumber(m_SendBuff->size()).WriteText("\n");
    m_SendPos = 0;
    return &Me::x_WriteSendBuff;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_ProxyMeta(void)
{
    LOG_CURRENT_FUNCTION
    if (!m_BlobAccess->IsBlobExists())
        return &Me::x_ReportBlobNotFound;

    WriteText("OK:SIZE=0 ");
    WriteNumber(m_BlobAccess->GetCurBlobCreateTime()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurCreateServer()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurCreateId()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurBlobDeadTime()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurBlobExpire()).WriteText(" ");
    WriteNumber(m_BlobAccess->GetCurVerExpire());
    WriteText("\n");

    return &Me::x_FinishCommand;
}
/*
CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_GetBlobsList(void)
{
    const vector<Uint2>& slots = CNCDistributionConf::GetSelfSlots();
    string slot_str;
    ITERATE(vector<Uint2>, it_slot, slots) {
        slot_str += NStr::UIntToString(*it_slot);
        slot_str += ",";
    }
    slot_str.resize(slot_str.size() - 1);
    Write("OK:Serving slots: ").Write(slot_str).Write("\n");

    ITERATE(vector<Uint2>, it_slot, slots) {
        TNCBlobSumList blobs_lst;
        CNCBlobStorage::GetFullBlobsList(*it_slot, blobs_lst);
        NON_CONST_ITERATE(TNCBlobSumList, it_blob, blobs_lst) {
            if (x_NeedEarlyClose())
                goto error_return;

            const string& raw_key = it_blob->first;
            SNCCacheData*& blob_sum = it_blob->second;
            string cache_name, key, subkey;
            CNCBlobStorage::UnpackBlobKey(raw_key, cache_name, key, subkey);
            Write("OK:key: ").Write(key);
            if (!cache_name.empty())
                Write(", subkey: ").Write(subkey);
            Write(", expire: ");
            CTime tmp_time(CTime::eEmpty, CTime::eLocal);
            tmp_time.SetTimeT(time_t(blob_sum->expire));
            Write(tmp_time.AsString("M/D/Y h:m:s"));
            Write(", create_time: ");
            tmp_time.SetTimeT(time_t(blob_sum->create_time / kNCTimeTicksInSec));
            tmp_time.SetMicroSecond(blob_sum->create_time % kNCTimeTicksInSec);
            Write(tmp_time.AsString("M/D/Y h:m:s.r"));
            Write(", create_id: ").Write(blob_sum->create_id);
            Write(", create_server: ");
            Write(CTaskServer::GetHostByIP(Uint4(blob_sum->create_server >> 32)));
            Write(":").Write(Uint4(blob_sum->create_server));
            Write("\n");
            delete blob_sum;
            blob_sum = NULL;
        }
        continue;

error_return:
        ITERATE(TNCBlobSumList, it_blob, blobs_lst) {
            delete it_blob->second;
        }
        return &Me::x_CloseCmdAndConn;
    }
    Write("OK:END\n");
    return &Me::x_FinishCommand;
}
*/
CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_NotImplemented(void)
{
    LOG_CURRENT_FUNCTION
    GetDiagCtx()->SetRequestStatus(eStatus_NoImpl);
    WriteText(s_MsgForStatus[eStatus_NoImpl]).WriteText("\n");
    return &Me::x_FinishCommand;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_Purge(void)
{
    LOG_CURRENT_FUNCTION
    if (CNCBlobAccessor::Purge( m_PrevCache, m_CmdStartTime.AsUSec())) {
        CNCBlobStorage::SavePurgeData();
    }
    m_CheckSrvs = CNCDistributionConf::GetPeerServers();
    return &Me::x_PurgeToNextPeer;
}

CNCMessageHandler::State
CNCMessageHandler::x_DoCmd_CopyPurge(void)
{
    LOG_CURRENT_FUNCTION
    if (CNCBlobAccessor::Purge( m_PrevCache, m_CopyBlobInfo->create_time)) {
        CNCBlobStorage::SavePurgeData();
    }
    return &Me::x_FinishCommand;
}


CNCMsgHandler_Factory::CNCMsgHandler_Factory(void)
{}

CNCMsgHandler_Factory::~CNCMsgHandler_Factory(void)
{}

CSrvSocketTask*
CNCMsgHandler_Factory::CreateSocketTask(void)
{
    return new CNCMessageHandler();
}

END_NCBI_SCOPE
