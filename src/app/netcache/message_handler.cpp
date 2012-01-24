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

#include <ncbi_pch.hpp>

#include <corelib/ncbi_param.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <util/md5.hpp>

#include <connect/services/netcache_key.hpp>
#include <connect/services/netcache_api_expt.hpp>

#include "message_handler.hpp"
#include "netcache_version.hpp"
#include "nc_stat.hpp"
#include "nc_memory.hpp"
#include "peer_control.hpp"
#include "distribution_conf.hpp"
#include "periodic_sync.hpp"
#include "active_handler.hpp"
#include "nc_storage.hpp"

#ifndef NCBI_OS_MSWIN
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#endif


BEGIN_NCBI_SCOPE


/// Definition of all NetCache commands
static CNCMessageHandler::SCommandDef s_CommandMap[] = {
    { "A?",      {&CNCMessageHandler::x_DoCmd_Alive,   "A?"} },
    { "VERSION", {&CNCMessageHandler::x_DoCmd_Version, "VERSION"} },
    { "HEALTH",  {&CNCMessageHandler::x_DoCmd_Health,  "HEALTH"} },
    { "HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "IC_HASB",       eWithoutBlob},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "READ",
        {&CNCMessageHandler::x_DoCmd_Get,
            "IC_READ",       eWithBlob,        eNCReadData},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "STOR",
        {&CNCMessageHandler::x_DoCmd_IC_Store,
            "IC_STOR",       eWithBlob,        eNCCreate},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "confirm", eNSPT_Int,  eNSPA_Optional },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "STRS",
        {&CNCMessageHandler::x_DoCmd_IC_Store,
            "IC_STRS",       eWithBlob,        eNCCreate},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "READLAST",
        {&CNCMessageHandler::x_DoCmd_GetLast,
            "IC_READLAST",   eWithBlob,        eNCReadData},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "start",   eNSPT_Int,  eNSPA_Optional },
          { "size",    eNSPT_Int,  eNSPA_Optional },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "SETVALID",
        {&CNCMessageHandler::x_DoCmd_SetValid,
            "IC_SETVALID",   eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "COPY_PUT",
        {&CNCMessageHandler::x_DoCmd_CopyPut,
            "COPY_PUT",      eWithBlob,        eNCCopyCreate},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "md5_pass",eNSPT_Str,  eNSPA_Required },
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "dead",    eNSPT_Int,  eNSPA_Required },
          { "exp",     eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "ver_ttl", eNSPT_Int,  eNSPA_Required },
          { "ver_dead",eNSPT_Int,  eNSPA_Required },
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          { "cr_id",   eNSPT_Int,  eNSPA_Required },
          { "log_rec", eNSPT_Int,  eNSPA_Required },
          { "cmd_ver", eNSPT_Int,  eNSPA_Optional, "0" },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "COPY_PROLONG",
        {&CNCMessageHandler::x_DoCmd_CopyProlong,
            "COPY_PROLONG",  eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          { "cr_id",   eNSPT_Int,  eNSPA_Required },
          { "dead",    eNSPT_Int,  eNSPA_Required },
          { "exp",     eNSPT_Int,  eNSPA_Required },
          { "ver_dead",eNSPT_Int,  eNSPA_Required },
          { "log_time",eNSPT_Int,  eNSPA_Optchain },
          { "log_srv", eNSPT_Int,  eNSPA_Optional },
          { "log_rec", eNSPT_Int,  eNSPA_Optional } } },
    { "PUT3",
        {&CNCMessageHandler::x_DoCmd_Put3,
            "PUT3",          eWithAutoBlobKey, eNCCreate},
        { { "ttl",     eNSPT_Int,  eNSPA_Optional },
          { "key",     eNSPT_NCID, eNSPA_Optional },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GET2",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GET2",          eWithBlob,        eNCReadData},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "NW",      eNSPT_Id,   eNSPA_Obsolete | fNSPA_Match },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "HASB",          eWithoutBlob},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "RMV2",
        {&CNCMessageHandler::x_DoCmd_Remove2,
            "RMV2",          eWithBlob,        eNCCreate},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GSIZ",
        {&CNCMessageHandler::x_DoCmd_GetSize,
            "GetSIZe",       eWithBlob,        eNCRead},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "REMO",
        {&CNCMessageHandler::x_DoCmd_Remove2,
            "IC_REMOve",     eWithBlob,        eNCCreate},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GSIZ",
        {&CNCMessageHandler::x_DoCmd_GetSize,
            "IC_GetSIZe",    eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GETPART",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GETPART",       eWithBlob,        eNCReadData},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "start",   eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "READPART",
        {&CNCMessageHandler::x_DoCmd_Get,
            "IC_READPART",   eWithBlob,        eNCReadData},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "start",   eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "PROXY_META",
        {&CNCMessageHandler::x_DoCmd_ProxyMeta,
            "PROXY_META",    eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "local",   eNSPT_Int,  eNSPA_Optional, "1" },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required } } },
    { "PROXY_PUT",
        {&CNCMessageHandler::x_DoCmd_Put3,
            "PROXY_PUT",     eWithBlob,        eNCCreate},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "PROXY_GET",
        {&CNCMessageHandler::x_DoCmd_Get,
            "PROXY_GET",     eWithBlob,        eNCReadData},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "start",   eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          { "srch",    eNSPT_Int,  eNSPA_Required },
          { "local",   eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "PROXY_HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "PROXY_HASB",    eWithoutBlob},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "PROXY_GSIZ",
        {&CNCMessageHandler::x_DoCmd_GetSize,
            "PROXY_GetSIZe", eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          { "srch",    eNSPT_Int,  eNSPA_Required },
          { "local",   eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "PROXY_READLAST",
        {&CNCMessageHandler::x_DoCmd_GetLast,
            "PROXY_READLAST",eWithBlob,        eNCReadData},
        { { "cache",   eNSPT_Str,   eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "start",   eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          { "srch",    eNSPT_Int,  eNSPA_Required },
          { "local",   eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "PROXY_SETVALID",
        {&CNCMessageHandler::x_DoCmd_SetValid,
            "PROXY_SETVALID",eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "local",   eNSPT_Int,  eNSPA_Optional, "1" },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "PROXY_RMV",
        {&CNCMessageHandler::x_DoCmd_Remove2,
            "PROXY_ReMoVe",  eWithBlob,        eNCCreate},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "PROXY_GETMETA",
        {&CNCMessageHandler::x_DoCmd_GetMeta,
            "PROXY_GETMETA", eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "qrum",    eNSPT_Int,  eNSPA_Required },
          { "local",   eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Required },
          { "sid",     eNSPT_Str,  eNSPA_Required } } },
    { "SYNC_START",
        {&CNCMessageHandler::x_DoCmd_SyncStart, "SYNC_START"},
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          { "slot",    eNSPT_Int,  eNSPA_Required },
          { "rec_my",  eNSPT_Int,  eNSPA_Required },
          { "rec_your",eNSPT_Int,  eNSPA_Required } } },
    { "SYNC_BLIST",
        {&CNCMessageHandler::x_DoCmd_SyncBlobsList, "SYNC_BLIST"},
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          { "slot",    eNSPT_Int,  eNSPA_Required } } },
    { "SYNC_PUT",
        {&CNCMessageHandler::x_DoCmd_SyncPut,
            "SYNC_PUT",      eWithBlob,        eNCCopyCreate},
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          { "slot",    eNSPT_Int,  eNSPA_Required },
          { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "md5_pass",eNSPT_Str,  eNSPA_Required },
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "dead",    eNSPT_Int,  eNSPA_Required },
          { "exp",     eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "ver_ttl", eNSPT_Int,  eNSPA_Required },
          { "ver_dead",eNSPT_Int,  eNSPA_Required },
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          { "cr_id",   eNSPT_Int,  eNSPA_Required },
          { "log_rec", eNSPT_Int,  eNSPA_Required },
          { "cmd_ver", eNSPT_Int,  eNSPA_Optional, "0" } } },
    { "SYNC_PROLONG",
        {&CNCMessageHandler::x_DoCmd_SyncProlong,
            "SYNC_PROLONG",  eWithBlob,        eNCRead},
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          { "slot",    eNSPT_Int,  eNSPA_Required },
          { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          { "cr_id",   eNSPT_Int,  eNSPA_Required },
          { "dead",    eNSPT_Int,  eNSPA_Required },
          { "exp",     eNSPT_Int,  eNSPA_Required },
          { "ver_dead",eNSPT_Int,  eNSPA_Required },
          { "log_time",eNSPT_Int,  eNSPA_Optchain },
          { "log_srv", eNSPT_Int,  eNSPA_Optional },
          { "log_rec", eNSPT_Int,  eNSPA_Optional } } },
    { "SYNC_GET",
        {&CNCMessageHandler::x_DoCmd_SyncGet,
            "SYNC_GET",      eWithBlob,        eNCReadData},
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          { "slot",    eNSPT_Int,  eNSPA_Required },
          { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "log_time",eNSPT_Int,  eNSPA_Required },
          { "cr_time", eNSPT_Int,  eNSPA_Required },
          { "cr_srv",  eNSPT_Int,  eNSPA_Required },
          { "cr_id",   eNSPT_Int,  eNSPA_Required } } },
    { "SYNC_PROINFO",
        {&CNCMessageHandler::x_DoCmd_SyncProlongInfo,
            "SYNC_PROINFO",  eWithBlob,        eNCRead},
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          { "slot",    eNSPT_Int,  eNSPA_Required },
          { "cache",   eNSPT_Str,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required } } },
    { "SYNC_COMMIT",
        {&CNCMessageHandler::x_DoCmd_SyncCommit, "SYNC_COMMIT"},
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          { "slot",    eNSPT_Int,  eNSPA_Required },
          { "rec_my",  eNSPT_Int,  eNSPA_Required },
          { "rec_your",eNSPT_Int,  eNSPA_Required } } },
    { "SYNC_CANCEL",
        {&CNCMessageHandler::x_DoCmd_SyncCancel, "SYNC_CANCEL"},
        { { "srv_id",  eNSPT_Int,  eNSPA_Required },
          { "slot",    eNSPT_Int,  eNSPA_Required } } },
    { "GETMETA",
        {&CNCMessageHandler::x_DoCmd_GetMeta,
            "GETMETA",       eWithBlob,        eNCRead},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "local",   eNSPT_Int,  eNSPA_Optional },
          { "qrum",    eNSPT_Int,  eNSPA_Optional } } },
    { "GETMETA",
        {&CNCMessageHandler::x_DoCmd_GetMeta,
            "IC_GETMETA",    eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "local",   eNSPT_Int,  eNSPA_Optional },
          { "qrum",    eNSPT_Int,  eNSPA_Optional } } },
    { "BLOBSLIST",
        {&CNCMessageHandler::x_DoCmd_GetBlobsList,
            "BLOBSLIST",     eWithoutBlob} },
    { "BLOBSLIST",
        {&CNCMessageHandler::x_DoCmd_GetBlobsList,
            "IC_BLOBSLIST",  eWithoutBlob},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "PUT2",
        {&CNCMessageHandler::x_DoCmd_Put2,
            "PUT2",          eWithAutoBlobKey, eNCCreate},
        { { "ttl",     eNSPT_Int,  eNSPA_Optional },
          { "key",     eNSPT_NCID, eNSPA_Optional } } },
    { "GET",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GET",           eWithBlob,        eNCReadData},
        { { "key",     eNSPT_NCID, eNSPA_Required } } },
    { "REMOVE",
        {&CNCMessageHandler::x_DoCmd_Remove,
            "REMOVE",        eWithBlob,        eNCCreate},
        { { "key",     eNSPT_NCID, eNSPA_Required } } },
    { "SHUTDOWN",
        {&CNCMessageHandler::x_DoCmd_Shutdown,       "SHUTDOWN"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GETSTAT",
        {&CNCMessageHandler::x_DoCmd_GetServerStats, "GETSTAT"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GETCONF",
        {&CNCMessageHandler::x_DoCmd_GetConfig,      "GETCONF"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REINIT",
        {&CNCMessageHandler::x_DoCmd_Reinit,
            "REINIT",        eWithoutBlob},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REINIT",
        {&CNCMessageHandler::x_DoCmd_Reinit,
            "IC_REINIT",     eWithoutBlob},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "RECONF",
        {&CNCMessageHandler::x_DoCmd_Reconf, "RECONF"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "LOG",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "LOG"} },
    { "STAT",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "STAT"} },
    { "MONI",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "MONITOR"} },
    { "DROPSTAT", {&CNCMessageHandler::x_DoCmd_NotImplemented, "DROPSTAT"} },
    { "GBOW",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "GBOW"} },
    { "ISLK",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "ISLK"} },
    { "SMR",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "SMR"} },
    { "SMU",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "SMU"} },
    { "OK",       {&CNCMessageHandler::x_DoCmd_NotImplemented, "OK"} },
    { "PUT",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "PUT"} },
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

static SNSProtoArgument s_AuthArgs[] = {
    { "client", eNSPT_Str, eNSPA_Optional, "Unknown client" },
    { "params", eNSPT_Str, eNSPA_Ellipsis },
    { NULL }
};




void
CBufferedSockReaderWriter::ResetSocket(CSocket* socket, unsigned int def_timeout)
{
    m_ReadDataPos  = 0;
    m_WriteDataPos = 0;
    m_CRLFMet = eNone;
    m_HasError = false;
    m_ReadBuff.resize(0);
    m_WriteBuff.resize(0);
    m_Socket = socket;
    m_DefTimeout.sec  = def_timeout;
    if (m_Socket) {
        ResetSocketTimeout();
    }
}

CBufferedSockReaderWriter::CBufferedSockReaderWriter(void)
{
    m_ReadBuff.reserve_mem(kReadNWBufSize);
    m_WriteBuff.reserve_mem(kWriteNWBufSize);
    m_DefTimeout.usec = m_ZeroTimeout.sec = m_ZeroTimeout.usec = 0;
    ResetSocket(NULL, 0);
}

void
CBufferedSockReaderWriter::x_ReadCRLF(bool force_read)
{
    if (HasDataToRead()  &&  (m_CRLFMet != eNone  ||  force_read)) {
        for (; HasDataToRead()  &&  m_CRLFMet != eCRLF; ++m_ReadDataPos) {
            char c = m_ReadBuff[m_ReadDataPos];
            if (c == '\n'  &&  (m_CRLFMet & eLF) == 0)
                m_CRLFMet = eCRLF;
            else if (c == '\r'  &&  m_CRLFMet == eNone)
                m_CRLFMet = eCR;
            else if (c == '\0'  &&  m_CRLFMet == eNone)
                m_CRLFMet = eCRLF;
            else
                break;
        }
        if (m_CRLFMet == eCRLF  ||  HasDataToRead()) {
            m_CRLFMet = eNone;
        }
    }
}

inline void
CBufferedSockReaderWriter::x_CompactBuffer(TNCBufferType& buffer, size_t& pos)
{
    if (pos != 0) {
        size_t new_size = buffer.size() - pos;
        memmove(buffer.data(), buffer.data() + pos, new_size);
        buffer.resize(new_size);
        pos = 0;
    }
}

size_t
CBufferedSockReaderWriter::x_ReadFromSocket(void* buf, size_t size)
{
    //LOG_POST("Reading from socket");
    ZeroSocketTimeout();
    size_t n_read = 0;
    EIO_Status status = m_Socket->Read(buf, size, &n_read, eIO_ReadPlain);
    ResetSocketTimeout();
    //LOG_POST("Read from socket " << n_read << " bytes: " << string((char*)buf, n_read));

    switch (status) {
    case eIO_Success:
    case eIO_Timeout:
    case eIO_Closed:
        break;
    default:
        ERR_POST("Error reading from socket: " << IO_StatusStr(status));
        m_HasError = true;
        break;
    }

    return n_read;
}

bool
CBufferedSockReaderWriter::ReadToBuf(void)
{
    x_CompactBuffer(m_ReadBuff, m_ReadDataPos);
    size_t n_read = x_ReadFromSocket(m_ReadBuff.data() + m_ReadBuff.size(),
                                     m_ReadBuff.capacity() - m_ReadBuff.size());
    m_ReadBuff.resize(m_ReadBuff.size() + n_read);
    x_ReadCRLF(false);
    return m_ReadDataPos < m_ReadBuff.size();
}

size_t
CBufferedSockReaderWriter::x_ReadFromBuf(void* dest, size_t size)
{
    size_t to_copy = m_ReadBuff.size() - m_ReadDataPos;
    if (size < to_copy)
        to_copy = size;
    memcpy(dest, m_ReadBuff.data() + m_ReadDataPos, to_copy);
    m_ReadDataPos += to_copy;
    return to_copy;
}

bool
CBufferedSockReaderWriter::ReadLine(CTempString* line)
{
    if (!ReadToBuf())
        return false;

    size_t crlf_pos;
    for (crlf_pos = m_ReadDataPos; crlf_pos < m_ReadBuff.size(); ++crlf_pos)
    {
        char c = m_ReadBuff[crlf_pos];
        if (c == '\n'  ||  c == '\r'  ||  c == '\0') {
            break;
        }
    }
    if (crlf_pos >= m_ReadBuff.size()) {
        if (m_ReadDataPos == 0  &&  m_ReadBuff.size() == m_ReadBuff.capacity())
        {
            ERR_POST(Critical << "Too long line in the protocol - at least "
                              << m_ReadBuff.size() << " bytes");
            m_HasError = true;
        }
        return false;
    }

    line->assign(m_ReadBuff.data() + m_ReadDataPos, crlf_pos - m_ReadDataPos);
    m_ReadDataPos = crlf_pos;
    x_ReadCRLF(true);
    return true;
}

size_t
CBufferedSockReaderWriter::Read(void* buf, size_t size)
{
    size_t n_read = 0;
    if (size != 0  &&  HasDataToRead()) {
        size_t copied = x_ReadFromBuf(buf, size);
        n_read += copied;
        buf = (char*)buf + copied;
        size -= copied;
    }
    if (size == 0)
        return n_read;

    if (size < kReadNWBufSize) {
        if (ReadToBuf())
            n_read += x_ReadFromBuf(buf, size);
    }
    else {
        n_read += x_ReadFromSocket(buf, size);
    }
    return n_read;
}

size_t
CBufferedSockReaderWriter::x_WriteToSocket(const void* buf, size_t size)
{
    //LOG_POST("Writing to socket");
    ZeroSocketTimeout();
    size_t n_written = 0;
    EIO_Status status = m_Socket->Write(buf, size, &n_written, eIO_WritePlain);
    ResetSocketTimeout();
    //LOG_POST("Written to socket " << n_written << " bytes: " << string((char*)buf, n_written));

    switch (status) {
    case eIO_Success:
    case eIO_Timeout:
    case eIO_Interrupt:
        break;
    default:
        ERR_POST("Error writing to socket: " << IO_StatusStr(status));
        m_HasError = true;
        break;
    }

    return n_written;
}

void
CBufferedSockReaderWriter::Flush(void)
{
    if (!m_Socket  ||  HasError()  ||  !IsWriteDataPending()) {
        return;
    }

    size_t n_written = x_WriteToSocket(m_WriteBuff.data() + m_WriteDataPos,
                                       m_WriteBuff.size() - m_WriteDataPos);
    m_WriteDataPos += n_written;
    x_CompactBuffer(m_WriteBuff, m_WriteDataPos);
}

void
CBufferedSockReaderWriter::x_CopyData(const void* buf, size_t size)
{
    m_WriteBuff.resize(m_WriteBuff.size() + size);
    void* data = m_WriteBuff.data() + m_WriteBuff.size() - size;
    memcpy(data, buf, size);
}

inline size_t
CBufferedSockReaderWriter::x_WriteNoPending(const void* buf, size_t size)
{
    if (size < kWriteNWMinSize) {
        x_CopyData(buf, size);
        return size;
    }
    else {
        return x_WriteToSocket(buf, size);
    }
}

size_t
CBufferedSockReaderWriter::Write(const void* buf, size_t size)
{
    size_t has_size = m_WriteBuff.size() - m_WriteDataPos;
    if (has_size == 0) {
        return x_WriteNoPending(buf, size);
    }
    else if (has_size + size <= kWriteNWBufSize) {
        x_CopyData(buf, size);
        Flush();
        return size;
    }
    else if (has_size < kWriteNWMinSize) {
        size_t to_copy = kWriteNWMinSize - has_size;
        x_CopyData(buf, to_copy);
        Flush();
        if (IsWriteDataPending())
            return to_copy;

        buf = (const char*)buf + to_copy;
        size -= to_copy;
        size_t n_written = x_WriteToSocket(buf, size);
        return to_copy + n_written;
    }
    else {
        Flush();
        if (IsWriteDataPending())
            return 0;
        return x_WriteNoPending(buf, size);
    }
}

void
CBufferedSockReaderWriter::WriteMessage(CTempString prefix, CTempString msg)
{
    size_t msg_size = prefix.size() + msg.size() + 1;
    m_WriteBuff.resize(m_WriteBuff.size() + msg_size);
    void* data = m_WriteBuff.data() + m_WriteBuff.size() - msg_size;
    memcpy(data, prefix.data(), prefix.size());
    data = (char*)data + prefix.size();
    memcpy(data, msg.data(), msg.size());
    data = (char*)data + msg.size();
    *(char*)data = '\n';
}

size_t
CBufferedSockReaderWriter::WriteFromBuffer(CBufferedSockReaderWriter& buf,
                                           size_t max_size)
{
    size_t n_written = 0;
    for (;;) {
        size_t n_to_write = min(buf.GetReadReadySize(), max_size);
        if (n_to_write != 0) {
            n_to_write = Write(buf.GetReadBufData(), n_to_write);
            if (n_to_write == 0)
                break;
            n_written += n_to_write;
            max_size -= n_to_write;
            buf.EraseReadBufData(n_to_write);
        }
        if (max_size == 0  ||  HasError()  ||  !buf.ReadToBuf())
            break;
    }
    return n_written;
}


/////////////////////////////////////////////////////////////////////////////
// CNCMessageHandler implementation

inline void
CNCMessageHandler::SetSocket(CSocket* socket)
{
    m_Socket = socket;
}

inline CNCMessageHandler::EStates
CNCMessageHandler::x_GetState(void) const
{
    return EStates(m_State & ~eAllFlagsMask);
}

inline void
CNCMessageHandler::x_SetState(EStates new_state)
{
    m_State = (m_State & eAllFlagsMask) + new_state;
}

inline void
CNCMessageHandler::x_SetFlag(EFlags flag)
{
    m_State |= flag;
}

inline void
CNCMessageHandler::x_UnsetFlag(EFlags flag)
{
    m_State &= ~flag;
}

inline bool
CNCMessageHandler::x_IsFlagSet(EFlags flag) const
{
    _ASSERT(flag & eAllFlagsMask);

    return (m_State & flag) != 0;
}

inline void
CNCMessageHandler::x_CloseConnection(void)
{
    g_NetcacheServer->CloseConnection(m_Socket);
}

inline void
CNCMessageHandler::InitDiagnostics(void)
{
    if (m_CmdCtx) {
        CDiagContext::SetRequestContext(m_CmdCtx);
    }
    else if (m_ConnCtx) {
        CDiagContext::SetRequestContext(m_ConnCtx);
    }
}

inline void
CNCMessageHandler::ResetDiagnostics(void)
{
    CDiagContext::SetRequestContext(NULL);
}



inline
CNCMessageHandler::CDiagnosticsGuard
    ::CDiagnosticsGuard(CNCMessageHandler* handler)
    : m_Handler(handler)
{
    m_Handler->InitDiagnostics();
}

inline
CNCMessageHandler::CDiagnosticsGuard::~CDiagnosticsGuard(void)
{
    m_Handler->ResetDiagnostics();
}


CNCMessageHandler::CNCMessageHandler(void)
    : m_Socket(NULL),
      m_State(eSocketClosed),
      m_Parser(s_CommandMap),
      m_CurCmd(NULL),
      m_CmdProcessor(NULL),
      m_BlobAccess(NULL),
      m_SrvsIndex(0),
      m_ActiveHub(NULL)
{}

CNCMessageHandler::~CNCMessageHandler(void)
{}

void
CNCMessageHandler::OnOpen(void)
{
    if (x_GetState() != eSocketClosed)
        abort();

    m_Socket->DisableOSSendDelay();
#ifdef NCBI_OS_LINUX
    int fd = 0, val = 1;
    m_Socket->GetOSHandle(&fd, sizeof(fd));
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
#endif
    m_SockBuffer.ResetSocket(m_Socket, g_NetcacheServer->GetDefConnTimeout());

    m_PrevCache.clear();
    m_ClientParams.clear();
    m_CntCmds = 0;

    m_ClientParams["peer"]  = m_Socket->GetPeerAddress(eSAF_IP);
    m_ClientParams["pport"] = m_Socket->GetPeerAddress(eSAF_Port);
    m_LocalPort             = m_Socket->GetLocalPort(eNH_HostByteOrder);
    m_ClientParams["port"]  = NStr::UIntToString(m_LocalPort);

    m_ConnCtx.Reset(new CRequestContext());
    m_ConnReqId = NStr::UInt8ToString(m_ConnCtx->SetRequestID());

    CDiagnosticsGuard guard(this);

    if (g_NetcacheServer->IsLogCmds()) {
        CDiagContext_Extra diag_extra = GetDiagContext().PrintRequestStart();
        diag_extra.Print("_type", "conn");
        ITERATE(TStringMap, it, m_ClientParams) {
            diag_extra.Print(it->first, it->second);
        }
        diag_extra.Flush();
        x_SetFlag(fConnStartPrinted);
    }
    m_ConnCtx->SetRequestStatus(eStatus_OK);

    m_ConnTime = GetFastLocalTime();
    x_SetState(ePreAuthenticated);

    CNCStat::AddOpenedConnection();

    m_InSyncCmd = m_WaitForThrottle = false;
}

void
CNCMessageHandler::OnTimeout(void)
{
    CDiagnosticsGuard guard(this);

    INFO_POST(Info << "Inactivity timeout expired, closing connection");
    if (m_CmdCtx)
        m_CmdCtx->SetRequestStatus(eStatus_CmdTimeout);
    else if (m_ConnCtx)
        m_ConnCtx->SetRequestStatus(eStatus_Inactive);
    else
        abort();
}

void
CNCMessageHandler::OnTimer(void)
{
    if (!x_IsFlagSet(fCommandStarted))
        return;

    CDiagnosticsGuard guard(this);

    INFO_POST("Command execution timed out. Closing connection.");
    CNCStat::AddTimedOutCommand();
    m_CmdCtx->SetRequestStatus(eStatus_CmdTimeout);
    x_CloseConnection();
}

void
CNCMessageHandler::OnOverflow(void)
{
    //abort();
    ERR_POST(Critical << "Max number of connections reached, closing connection");
    CNCStat::AddOverflowConnection();
}

void
CNCMessageHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    // It's possible that this method will be called before OnOpen - when
    // connection is just created and server is shutting down. In this case
    // OnOpen will never be called.
    if (m_ConnCtx.IsNull())
        return;

    CDiagnosticsGuard guard(this);

    switch (peer)
    {
    case IServer_ConnectionHandler::eOurClose:
        if (m_CmdCtx) {
            m_ConnCtx->SetRequestStatus(m_CmdCtx->GetRequestStatus());
        }
        else if (m_ConnCtx->GetRequestStatus() == eStatus_OK) {
            m_ConnCtx->SetRequestStatus(eStatus_Inactive);
        }
        break;
    case IServer_ConnectionHandler::eClientClose:
        if (m_CmdCtx) {
            m_CmdCtx->SetRequestStatus(eStatus_BadCmd);
            m_ConnCtx->SetRequestStatus(eStatus_BadCmd);
        }
        break;
    }

    // Hack-ish fix to make cursed PUT2 command to work - it uses connection
    // closing as legal end-of-blob signal.
    if (x_GetState() == eReadBlobChunkLength
        &&  NStr::strcmp(m_CurCmd, "PUT2") == 0)
    {
        _ASSERT(m_CmdCtx  &&  m_BlobAccess);
        m_CmdCtx->SetRequestStatus(eStatus_OK);
        m_ConnCtx->SetRequestStatus(eStatus_PUT2Used);
        x_FinishReadingBlob();
    }
    x_FinishCommand(false);

    if (x_GetState() == ePreAuthenticated) {
        CNCStat::SetFakeConnection();
    }
    else {
        double conn_span = (GetFastLocalTime() - m_ConnTime).GetAsDouble();
        CNCStat::AddClosedConnection(conn_span,
                                     m_ConnCtx->GetRequestStatus(),
                                     m_CntCmds);
    }

    if (x_IsFlagSet(fConnStartPrinted)) {
        CDiagContext::SetRequestContext(m_ConnCtx);
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("cmds_cnt", NStr::UInt8ToString(m_CntCmds));
        extra.Flush();
        //LOG_POST(StackTrace);
        m_ConnCtx->SetBytesRd(m_Socket->GetCount(eIO_Read));
        m_ConnCtx->SetBytesWr(m_Socket->GetCount(eIO_Write));
        GetDiagContext().PrintRequestStop();
        x_UnsetFlag(fConnStartPrinted);
    }
    m_ConnCtx.Reset();

    x_SetState(eSocketClosed);
    m_SockBuffer.ResetSocket(NULL, 0);
}

void
CNCMessageHandler::OnRead(void)
{
    CFastMutexGuard guard(m_ObjLock);
    x_ManageCmdPipeline();
}

void
CNCMessageHandler::OnWrite(void)
{
    CFastMutexGuard guard(m_ObjLock);
    x_ManageCmdPipeline();
}

EIO_Event
CNCMessageHandler::GetEventsToPollFor(const CTime** alarm_time) const
{
    if (alarm_time  &&  x_IsFlagSet(fCommandStarted)) {
        *alarm_time = &m_MaxCmdTime;
    }
    if (x_GetState() == eWriteBlobData
        ||  x_GetState() == eWriteSendBuff
        ||  m_SockBuffer.IsWriteDataPending())
    {
        return eIO_Write;
    }
    return eIO_Read;
}

bool
CNCMessageHandler::IsReadyToProcess(void) const
{
    if (m_WaitForThrottle) {
        return CNetCacheServer::GetPreciseTime() >= m_ThrottleTime;
    }
    else if (m_ActiveHub != NULL) {
        if (m_NeedFlushBuff)
            return true;
        if (x_GetState() == eReadBlobSignature)
            return m_ActiveHub->GetStatus() != eNCHubCmdInProgress
                   ||  m_GotInitialAnswer;
        if (x_IsFlagSet(fWaitForActive)) {
            return m_ActiveHub->GetStatus() != eNCHubCmdInProgress
                   ||  m_ActiveHub->GetHandler()->IsBufferFlushed();
        }
        return m_ActiveHub->GetStatus() != eNCHubCmdInProgress
               &&  m_ActiveHub->GetStatus() != eNCHubWaitForConn;
    }
    else if (x_IsFlagSet(fWaitForBlockedOp)) {
        return false;
    }

    return true;
}

void
CNCMessageHandler::OnBlockedOpFinish(void)
{
    m_ObjLock.Lock();
    x_UnsetFlag(fWaitForBlockedOp);
    m_ObjLock.Unlock();

    g_NetcacheServer->WakeUpPollCycle();
}

CBufferedSockReaderWriter&
CNCMessageHandler::GetSockBuffer(void)
{
    CFastMutexGuard guard(m_ObjLock);
    return m_SockBuffer;
}

void
CNCMessageHandler::ForceBufferFlush(void)
{
    m_NeedFlushBuff = true;
}

void
CNCMessageHandler::InitialAnswerWritten(void)
{
    m_GotInitialAnswer = true;
}

bool
CNCMessageHandler::IsBufferFlushed(void)
{
    return !m_NeedFlushBuff;
}

void
CNCMessageHandler::WriteInitWriteResponse(const string& response)
{
    m_ObjLock.Lock();
    m_SockBuffer.WriteMessage("", response);
    m_SockBuffer.Flush();
    m_GotInitialAnswer = true;
    m_ObjLock.Unlock();

    g_NetcacheServer->WakeUpPollCycle();
}

bool
CNCMessageHandler::x_ReadAuthMessage(void)
{
    CTempString auth_line;
    if (!m_SockBuffer.ReadLine(&auth_line))
        return false;

    TNSProtoParams params;
    try {
        m_Parser.ParseArguments(auth_line, s_AuthArgs, &params);
    }
    catch (CNSProtoParserException& ex) {
        ERR_POST("Error authenticating client: '" << auth_line << "': " << ex);
        params["client"] = auth_line;
    }
    ITERATE(TNSProtoParams, it, params) {
        m_ClientParams[it->first] = it->second;
    }
    if (x_IsFlagSet(fConnStartPrinted)) {
        CDiagContext_Extra diag_extra = GetDiagContext().Extra();
        ITERATE(TNSProtoParams, it, params) {
            diag_extra.Print(it->first, it->second);
        }
    }

    m_BaseAppSetup = m_AppSetup = g_NetcacheServer->GetAppSetup(m_ClientParams);
    x_SetState(eReadyForCommand);

    if (m_AppSetup->disable) {
        m_ConnCtx->SetRequestStatus(eStatus_Disabled);
        ERR_POST(Warning << "Disabled client is being disconnected ('"
                         << auth_line << "').");
        x_CloseConnection();
    }
    else {
        m_SockBuffer.SetTimeout(m_AppSetup->conn_timeout);
    }
    return true;
}

bool
CNCMessageHandler::x_CheckAdminClient(void)
{
    if (m_ClientParams["client"] != g_NetcacheServer->GetAdminClient()
        &&  m_ClientParams["client"] != kNCPeerClientName)
    {
        m_SockBuffer.WriteMessage("ERR:", "Command is not accessible");
        return false;
    }
    return true;
}

void
CNCMessageHandler::x_AssignCmdParams(TNSProtoParams& params)
{
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
    m_ConfirmPut = false;
    bool quorum_was_set = false;
    bool search_was_set = false;

    CTempString cache_name;

    ERASE_ITERATE(TNSProtoParams, it, params) {
        const CTempString& key = it->first;
        CTempString& val = it->second;

        switch (key[0]) {
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
                    m_ConfirmPut = val == "1";
                }
                break;
            case 'r':
                if (key == "cr_time") {
                    m_CopyBlobInfo.create_time = NStr::StringToUInt8(val);
                }
                else if (key == "cr_id") {
                    m_CopyBlobInfo.create_id = NStr::StringToUInt(val);
                }
                else if (key == "cr_srv") {
                    m_CopyBlobInfo.create_server = NStr::StringToUInt8(val);
                }
                break;
            }
            break;
        case 'd':
            if (key == "dead") {
                m_CopyBlobInfo.dead_time = NStr::StringToInt(val);
            }
            break;
        case 'e':
            if (key == "exp") {
                m_CopyBlobInfo.expire = NStr::StringToInt(val);
            }
            break;
        case 'i':
            if (key == "ip") {
                _ASSERT(m_CmdCtx.NotNull());
                if (!val.empty()) {
                    m_CmdCtx->SetClientIP(val);
                }
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
                params.erase(it);
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
                    _ASSERT(m_CmdCtx.NotNull());
                    if (!val.empty()) {
                        m_CmdCtx->SetSessionID(NStr::URLDecode(val));
                    }
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
            break;
        case 'v':
            if (key == "version") {
                m_BlobVersion = NStr::StringToInt(val);
            }
            else if (key == "ver_ttl") {
                m_CopyBlobInfo.ver_ttl = NStr::StringToUInt(val);
            }
            else if (key == "ver_dead") {
                m_CopyBlobInfo.ver_expire = NStr::StringToInt(val);
            }
            break;
        default:
            break;
        }
    }

    g_NCStorage->PackBlobKey(&m_BlobKey, cache_name, blob_key, blob_subkey);
    if (cache_name.empty()) {
        m_AppSetup = m_BaseAppSetup;
    }
    else if (cache_name == m_PrevCache) {
        m_AppSetup = m_PrevAppSetup;
    }
    else {
        m_PrevCache = cache_name;
        m_ClientParams["cache"] = cache_name;
        m_AppSetup = m_PrevAppSetup = g_NetcacheServer->GetAppSetup(m_ClientParams);
    }
    if (!quorum_was_set)
        m_Quorum = m_AppSetup->quorum;
    if (m_ForceLocal)
        m_Quorum = 1;
    if (!search_was_set)
        m_SearchOnRead = m_AppSetup->srch_on_read;
}

void
CNCMessageHandler::x_PrintRequestStart(const SParsedCmd& cmd,
                                       auto_ptr<CDiagContext_Extra>& diag_extra)
{
    if (!g_NetcacheServer->IsLogCmds())
        return;

    diag_extra.reset(new CDiagContext_Extra(GetDiagContext().PrintRequestStart()));
    diag_extra->Print("_type", "cmd");
    diag_extra->Print("cmd",  cmd.command->cmd);
    diag_extra->Print("client", m_ClientParams["client"]);
    diag_extra->Print("conn", m_ConnReqId);
    ITERATE(TNSProtoParams, it, cmd.params) {
        diag_extra->Print(it->first, it->second);
    }
    if (!m_BlobPass.empty()) {
        diag_extra->Print("pass", g_NCStorage->PrintablePassword(m_BlobPass));
    }

    x_SetFlag(fCommandPrinted);
}

void
CNCMessageHandler::x_WaitForWouldBlock(void)
{
    x_SetFlag(fWaitForBlockedOp);
    g_NetcacheServer->DeferConnectionProcessing(m_Socket);
}

bool
CNCMessageHandler::x_StartCommand(SParsedCmd& cmd)
{
    x_SetFlag(fCommandStarted);

    const SCommandExtra& cmd_extra = cmd.command->extra;
    m_CmdProcessor = cmd_extra.processor;
    m_CurCmd       = cmd_extra.cmd_name;

    m_MaxCmdTime = m_CmdStartTime = GetFastLocalTime();
    m_MaxCmdTime.AddSecond(m_AppSetup->cmd_timeout, CTime::eIgnoreDaylight);

    CNCStat::AddStartedCmd(m_CurCmd);
    auto_ptr<CDiagContext_Extra> diag_extra;
    x_PrintRequestStart(cmd, diag_extra);

    if (!CNetCacheServer::IsCachingComplete()
        &&  (NStr::StartsWith(m_CurCmd, "COPY_")
             ||  NStr::StartsWith(m_CurCmd, "PROXY_")
             ||  NStr::StartsWith(m_CurCmd, "SYNC_")
             ||  NStr::FindCase(m_CurCmd, "BLOBSLIST") != NPOS
             ||  (NStr::FindCase(m_CurCmd, "GETMETA") != NPOS  &&  m_ForceLocal)))
    {
        m_SockBuffer.WriteMessage("ERR:", "Caching is not completed");
        if (diag_extra.get() != NULL)
            diag_extra->Flush();
        m_CmdCtx->SetRequestStatus(eStatus_JustStarted);
        x_SetState(eReadyForCommand);
        return false;
    }

    if (m_AppSetup->disable) {
        if (diag_extra.get() != NULL)
            diag_extra->Flush();
        // We'll be here only if generally work for the client is enabled but
        // for current particular cache is disabled.
        m_CmdCtx->SetRequestStatus(eStatus_Disabled);
        m_SockBuffer.WriteMessage("ERR:", "Cache is disabled");
        x_SetState(eReadyForCommand);
        return false;
    }

    if (cmd_extra.storage_access == eWithBlob
        ||  cmd_extra.storage_access == eWithAutoBlobKey
        ||  m_CmdProcessor == &CNCMessageHandler::x_DoCmd_HasBlob)
    {
        if ((m_BlobPass.empty()  &&  m_AppSetup->pass_policy == eNCOnlyWithPass)
            ||  (!m_BlobPass.empty()  &&  m_AppSetup->pass_policy == eNCOnlyWithoutPass))
        {
            if (diag_extra.get() != NULL)
                diag_extra->Flush();
            string msg = "Password in the command doesn't match server settings.";
            m_SockBuffer.WriteMessage("ERR:", msg);
            ERR_POST(msg);
            m_CmdCtx->SetRequestStatus(eStatus_BadPassword);
            x_SetState(eReadyForCommand);
            return false;
        }
        if (cmd_extra.storage_access == eWithAutoBlobKey  &&  m_RawKey.empty()) {
            m_RawKey = CNCDistributionConf::GenerateBlobKey(m_LocalPort);
            g_NCStorage->PackBlobKey(&m_BlobKey, CTempString(), m_RawKey, CTempString());

            if (diag_extra.get() != NULL) {
                diag_extra->Print("key", m_RawKey);
                diag_extra->Print("gen_key", "1");
            }
        }
        else if (m_BlobKey[0] == '\1') {
            try {
                CNetCacheKey nc_key(m_RawKey);
                m_RawKey = nc_key.StripKeyExtensions();
                g_NCStorage->PackBlobKey(&m_BlobKey, CTempString(), m_RawKey, CTempString());
            }
            catch (CNetCacheException& ex) {
                ERR_POST(Critical << "Error parsing blob key: " << ex);
            }
        }
        m_BlobSlot = CNCDistributionConf::GetSlotByKey(m_BlobKey);
        m_BlobSize = 0;
        if (diag_extra.get() != NULL) {
            diag_extra->Print("slot", NStr::UIntToString(m_BlobSlot));
            diag_extra->Flush();
        }
        // PrintRequestStart() resets status value, so setting default status
        // should go here, though more logical is in x_ReadCommand()
        m_CmdCtx->SetRequestStatus(eStatus_OK);

        if ((CNCDistributionConf::IsServedLocally(m_BlobSlot)
                &&  CNetCacheServer::IsCachingComplete())
            ||  (NStr::FindCase(m_CurCmd, "GETMETA") != NPOS  &&  m_ForceLocal)
            ||  NStr::CompareCase(m_CurCmd, "COPY_PUT") == 0
            ||  NStr::StartsWith(m_CurCmd, "PROXY_")
            ||  NStr::StartsWith(m_CurCmd, "SYNC_"))
        {
            if (!CNetCacheServer::IsInitiallySynced()  &&  !m_ForceLocal
                &&  (m_CmdProcessor == &CNCMessageHandler::x_DoCmd_HasBlob
                     ||  cmd_extra.blob_access == eNCRead
                     ||  cmd_extra.blob_access == eNCReadData)
                &&  !NStr::StartsWith(m_CurCmd, "SYNC_"))
            {
                m_Quorum = 0;
            }
            if (cmd_extra.blob_access == eNCCreate
                &&  m_CmdProcessor != &CNCMessageHandler::x_DoCmd_Remove
                &&  m_CmdProcessor != &CNCMessageHandler::x_DoCmd_Remove2
                &&  g_NCStorage->NeedStopWrite())
            {
                m_CmdCtx->SetRequestStatus(eStatus_NoDiskSpace);
                m_SockBuffer.WriteMessage("ERR:", "Not enough disk space");
                m_SockBuffer.Flush();
                x_SetState(eReadyForCommand);
            }
            else if (m_CmdProcessor == &CNCMessageHandler::x_DoCmd_HasBlob)
            {
                x_SetState(eCommandReceived);
            }
            else {
                m_BlobAccess = g_NCStorage->GetBlobAccess(cmd_extra.blob_access,
                                            m_BlobKey, m_BlobPass, m_BlobSlot);
                x_SetState(eWaitForBlobAccess);
            }
        }
        else {
            CDiagContext_Extra extra = GetDiagContext().Extra();
            extra.Print("proxy", "1");
            extra.Flush();

            x_GetCurSlotServers();
            x_SetState(eProxyToNextPeer);
        }
    }
    else {
        if (diag_extra.get() != NULL)
            diag_extra->Flush();
        // PrintRequestStart() resets status value, so setting default status
        // should go here, though more logical is in x_ReadCommand()
        m_CmdCtx->SetRequestStatus(eStatus_OK);

        x_SetState(eCommandReceived);
    }
    return true;
}

bool
CNCMessageHandler::x_ReadCommand(void)
{
    CTempString cmd_line;
    if (!m_SockBuffer.ReadLine(&cmd_line))
        return false;

    SParsedCmd cmd;
    try {
        cmd = m_Parser.ParseCommand(cmd_line);
    }
    catch (CNSProtoParserException& ex) {
        ERR_POST("Error parsing command: " << ex);
        m_ConnCtx->SetRequestStatus(eStatus_BadCmd);
        x_CloseConnection();
        return true;
    }
    m_CmdCtx.Reset(new CRequestContext());
    m_CmdCtx->SetRequestID();
    try {
        x_AssignCmdParams(cmd.params);
    }
    catch (CStringException& ex) {
        ERR_POST("Error while parsing command '" << cmd_line << "': " << ex);
        m_ConnCtx->SetRequestStatus(eStatus_BadCmd);
        m_CmdCtx.Reset();
        x_CloseConnection();
        return true;
    }
    CDiagContext::SetRequestContext(m_CmdCtx);
    if (x_StartCommand(cmd)  &&  NStr::StartsWith(m_CurCmd, "SYNC_")
        &&  CNetCacheServer::IsInitiallySynced())
    {
        m_InSyncCmd = true;
        Uint4 to_wait = CNCPeriodicSync::BeginTimeEvent(m_SrvId);
        if (to_wait != 0) {
            Uint8 now = CNetCacheServer::GetPreciseTime();
            m_ThrottleTime = now + to_wait;
            if (g_NetcacheServer->IsLogCmds()) {
                GetDiagContext().Extra().Print("throt", NStr::UIntToString(to_wait));
            }
            m_WaitForThrottle = true;
            g_NetcacheServer->DeferConnectionProcessing(m_Socket);
            return false;
        }
        else {
            m_WaitForThrottle = false;
        }
    }
    return true;
}

void
CNCMessageHandler::x_GetCurSlotServers(void)
{
    m_CheckSrvs = CNCDistributionConf::GetServersForSlot(m_BlobSlot);
    Uint8 main_srv_id = 0;
    if (m_BlobKey[0] == '\1')
        main_srv_id = CNCDistributionConf::GetMainSrvId(m_BlobKey);
    if (main_srv_id != 0  &&  main_srv_id != CNCDistributionConf::GetSelfID()) {
        TServersList::iterator it = find(m_CheckSrvs.begin(), m_CheckSrvs.end(),
                                         main_srv_id);
        if (it != m_CheckSrvs.end()) {
            Uint8 srv_id = *it;
            m_CheckSrvs.erase(it);
            m_CheckSrvs.insert(m_CheckSrvs.begin(), srv_id);
        }
    }
}

bool
CNCMessageHandler::x_WaitForBlobAccess(void)
{
    if (m_BlobAccess->ObtainMetaInfo(this) == eNCWouldBlock) {
        x_WaitForWouldBlock();
        return false;
    }
    bool is_meta = NStr::FindCase(m_CurCmd, "GETMETA") != NPOS;
    if (NStr::CompareCase(m_CurCmd, "COPY_PUT") == 0
        ||  NStr::StartsWith(m_CurCmd, "SYNC_")
        ||  (is_meta  &&  m_ForceLocal))
    {
        m_LatestExist = m_BlobAccess->IsBlobExists()
                        &&  !m_BlobAccess->IsCurBlobExpired()
                        &&  m_BlobAccess->GetCurBlobVersion() == m_BlobVersion;
        m_LatestSrvId = CNCDistributionConf::GetSelfID();
        x_SetState(eCommandReceived);
    }
    else if (!m_BlobAccess->IsAuthorized()  &&  !is_meta) {
        x_SetState(ePasswordFailed);
    }
    else if ((m_BlobAccess->GetAccessType() == eNCRead
                ||  m_BlobAccess->GetAccessType() == eNCReadData)
             &&  NStr::FindCase(m_CurCmd, "SETVALID") == NPOS
             &&  NStr::CompareCase(m_CurCmd, "PROXY_META") != 0
             &&  NStr::CompareCase(m_CurCmd, "COPY_PROLONG") != 0)
    {
        m_LatestExist = m_BlobAccess->IsBlobExists()
                        &&  !m_BlobAccess->IsCurBlobExpired()
                        &&  (m_CmdProcessor == &CNCMessageHandler::x_DoCmd_GetLast
                             ||  m_BlobAccess->GetCurBlobVersion() == m_BlobVersion);
        m_LatestSrvId = CNCDistributionConf::GetSelfID();
        if (m_Quorum != 1  ||  (!m_LatestExist  &&  m_SearchOnRead)) {
            if (m_LatestExist) {
                if (m_Quorum != 0)
                    --m_Quorum;
                m_LatestBlobSum.create_time = m_BlobAccess->GetCurBlobCreateTime();
                m_LatestBlobSum.create_server = m_BlobAccess->GetCurCreateServer();
                m_LatestBlobSum.create_id = m_BlobAccess->GetCurCreateId();
                m_LatestBlobSum.dead_time = m_BlobAccess->GetCurBlobDeadTime();
                m_LatestBlobSum.expire = m_BlobAccess->GetCurBlobExpire();
                m_LatestBlobSum.ver_expire = m_BlobAccess->GetCurVerExpire();
            }
            x_GetCurSlotServers();
            x_SetState(eReadMetaNextPeer);
        }
        else {
            x_SetState(eCommandReceived);
        }
    }
    else {
        x_SetState(eCommandReceived);
    }
    return true;
}

bool
CNCMessageHandler::x_ProcessBadPassword(void)
{
    m_SockBuffer.WriteMessage("ERR:", "Access denied.");
    m_SockBuffer.Flush();
    m_CmdCtx->SetRequestStatus(eStatus_BadPassword);
    ERR_POST(Warning << "Incorrect password is used to access the blob");
    x_SetState(eReadyForCommand);
    return true;
}

inline bool
CNCMessageHandler::x_DoCommand(void)
{
    bool do_next = (this->*m_CmdProcessor)();
    if (do_next  &&  x_GetState() == eCommandReceived) {
        x_SetState(eReadyForCommand);
    }
    return do_next;
}

void
CNCMessageHandler::x_ProlongBlobDeadTime(void)
{
    if (!m_AppSetup->prolong_on_read)
        return;

    Uint8 cur_time = CNetCacheServer::GetPreciseTime();
    int new_expire = int(cur_time / kNCTimeTicksInSec)
                     + m_BlobAccess->GetCurBlobTTL();
    int old_expire = m_BlobAccess->GetCurBlobExpire();
    if (!CNetCacheServer::IsDebugMode()
        &&  new_expire - old_expire < m_AppSetup->ttl_unit)
    {
        return;
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
    Uint8 cur_time = CNetCacheServer::GetPreciseTime();
    int new_expire = int(cur_time / kNCTimeTicksInSec)
                     + m_BlobAccess->GetCurBlobTTL();
    int old_expire = m_BlobAccess->GetCurVerExpire();
    if (!CNetCacheServer::IsDebugMode()
        &&  new_expire - old_expire < m_AppSetup->ttl_unit)
    {
        return;
    }

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
CNCMessageHandler::x_FinishCommand(bool do_sock_write)
{
    if (!x_IsFlagSet(fCommandStarted)) {
        m_CmdCtx.Reset();
        CDiagContext::SetRequestContext(m_ConnCtx);
        return;
    }

    bool was_blob_access = (m_BlobAccess != NULL);
    if (was_blob_access) {
        if (!m_BlobAccess->IsBlobExists()
            ||  m_CmdProcessor == &CNCMessageHandler::x_DoCmd_Remove
            ||  m_CmdProcessor == &CNCMessageHandler::x_DoCmd_Remove2
            ||  m_CmdCtx->GetRequestStatus() == eStatus_NotFound
            ||  m_CmdCtx->GetRequestStatus() == eStatus_NewerBlob
            ||  m_CmdCtx->GetRequestStatus() == eStatus_StaleSync
            ||  m_CmdCtx->GetRequestStatus() == eStatus_SyncAborted)
        {
            // Blob was deleted or didn't exist, logging of size is not necessary
            was_blob_access = false;
        }
        else {
            if (m_BlobAccess->GetAccessType() == eNCRead
                ||  m_BlobAccess->GetAccessType() == eNCReadData)
            {
                m_BlobSize = m_BlobAccess->GetCurBlobSize();
            }
            else {
                m_BlobSize = m_BlobAccess->GetNewBlobSize();
            }
            if (m_BlobAccess->GetAccessType() == eNCReadData) {
                m_CmdCtx->SetBytesWr(m_BlobAccess->GetSizeRead());
            }
            else if (m_BlobAccess->GetAccessType() == eNCCreate
                     ||  m_BlobAccess->GetAccessType() == eNCCopyCreate)
            {
                m_CmdCtx->SetBytesRd(m_BlobAccess->GetNewBlobSize());
            }
        }
        m_BlobAccess->Release();
        m_BlobAccess = NULL;
    }

    if (x_IsFlagSet(fNeedSyncFinish)) {
        if (m_CmdCtx->GetRequestStatus() == eStatus_BadCmd
            ||  m_CmdCtx->GetRequestStatus() == eStatus_ServerError)
        {
            CNCPeriodicSync::Cancel(m_SrvId, m_Slot, m_SyncId);
        }
        else {
            CNCPeriodicSync::SyncCommandFinished(m_SrvId, m_Slot, m_SyncId);
        }
        x_UnsetFlag(fNeedSyncFinish);
    }
    if (x_IsFlagSet(fConfirmBlobPut)  &&  do_sock_write) {
        m_SockBuffer.WriteMessage("OK:", "");
        m_SockBuffer.Flush();
    }

    if (m_ActiveHub) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
    }
    m_CheckSrvs.clear();
    m_SrvsIndex = 0;

    int cmd_status = m_CmdCtx->GetRequestStatus();
    if (x_IsFlagSet(fCommandPrinted)) {
        if (was_blob_access
            &&  (cmd_status == eStatus_OK
                 ||  (cmd_status == eStatus_BadCmd  &&  m_BlobSize != 0)))
        {
            CDiagContext_Extra diag_extra = GetDiagContext().Extra();
            diag_extra.Print("size", NStr::UInt8ToString(m_BlobSize));
            diag_extra.Flush();
        }
        GetDiagContext().PrintRequestStop();
        x_UnsetFlag(fCommandPrinted);
    }

    x_UnsetFlag(fCommandStarted);
    x_UnsetFlag(fConfirmBlobPut);
    x_UnsetFlag(fReadExactBlobSize);
    x_UnsetFlag(fSkipBlobEOF);
    x_UnsetFlag(fCopyLogEvent);
    x_UnsetFlag(fWaitForActive);

    double cmd_span = (GetFastLocalTime() - m_CmdStartTime).GetAsDouble();
    CNCStat::AddFinishedCmd(m_CurCmd, cmd_span, cmd_status);
    ++m_CntCmds;

    if (m_InSyncCmd) {
        CNCPeriodicSync::EndTimeEvent(m_SrvId, 0);
        m_InSyncCmd = false;
    }

    m_SendBuff.reset();
    m_CmdCtx.Reset();
    CDiagContext::SetRequestContext(m_ConnCtx);
}



void
CNCMessageHandler::x_StartReadingBlob(void)
{
    m_SockBuffer.Flush();
    if (m_SockBuffer.HasError())
        x_SetState(eReadyForCommand);
    else
        x_SetState(eReadBlobSignature);
}

void
CNCMessageHandler::x_FinishReadingBlob(void)
{
    x_SetState(eReadyForCommand);
    if (x_IsFlagSet(fReadExactBlobSize)  &&  m_BlobSize != m_Size) {
        m_CmdCtx->SetRequestStatus(eStatus_CondFailed);
        ERR_POST("Too few data for blob size " << m_Size << " (received "
                 << m_BlobSize << " bytes)");
        if (x_IsFlagSet(fConfirmBlobPut)) {
            m_SockBuffer.WriteMessage("ERR:", "Too few data for blob");
            x_UnsetFlag(fConfirmBlobPut);
        }
        else {
            x_CloseConnection();
        }
        return;
    }
    if (m_CmdCtx->GetRequestStatus() == eStatus_NewerBlob
        ||  m_CmdCtx->GetRequestStatus() == eStatus_SyncAborted)
    {
        return;
    }

    SNCSyncEvent* write_event = new SNCSyncEvent();
    write_event->event_type = eSyncWrite;
    write_event->key = m_BlobKey;
    if (x_IsFlagSet(fCopyLogEvent)) {
        write_event->orig_time = m_BlobAccess->GetNewBlobCreateTime();
        write_event->orig_server = m_BlobAccess->GetNewCreateServer();
        write_event->orig_rec_no = m_OrigRecNo;
        m_BlobAccess->SetBlobSlot(m_BlobSlot);
    }
    else {
        Uint8 cur_time = CNetCacheServer::GetPreciseTime();
        Uint4 cur_secs = int(cur_time / kNCTimeTicksInSec);
        m_BlobAccess->SetBlobCreateTime(cur_time);
        if (m_BlobAccess->GetNewBlobExpire() == 0)
            m_BlobAccess->SetNewBlobExpire(cur_secs + m_BlobAccess->GetNewBlobTTL());
        m_BlobAccess->SetNewVerExpire(cur_secs + m_BlobAccess->GetNewVersionTTL());
        m_BlobAccess->SetCreateServer(CNCDistributionConf::GetSelfID(),
                                      g_NCStorage->GetNewBlobId(), m_BlobSlot);
        write_event->orig_server = CNCDistributionConf::GetSelfID();
        write_event->orig_time = cur_time;
    }

    m_BlobAccess->Finalize();
    if (m_BlobAccess->HasError()) {
        m_CmdCtx->SetRequestStatus(eStatus_ServerError);
        x_CloseConnection();
        return;
    }
    m_BlobSize = m_BlobAccess->GetNewBlobSize();
    m_CmdCtx->SetBytesRd(m_BlobSize);

    if (!x_IsFlagSet(fCopyLogEvent)) {
        m_OrigRecNo = CNCSyncLog::AddEvent(m_BlobSlot, write_event);
        CNCPeerControl::MirrorWrite(m_BlobKey, m_BlobSlot,
                                    m_OrigRecNo, m_BlobAccess->GetNewBlobSize());
        if (m_Quorum != 1) {
            if (m_Quorum != 0)
                --m_Quorum;
            x_GetCurSlotServers();
            x_SetState(ePutToNextPeer);
        }
    }
    else if (m_OrigRecNo != 0) {
        CNCSyncLog::AddEvent(m_BlobSlot, write_event);
    }
    else
        delete write_event;
}

bool
CNCMessageHandler::x_ReadBlobSignature(void)
{
    if (m_ActiveHub) {
        if (m_ActiveHub->GetStatus() == eNCHubError) {
            ERR_POST(Warning << "Error executing command on peer: "
                             << m_ActiveHub->GetErrMsg());
            m_ActiveHub->Release();
            m_ActiveHub = NULL;
            x_SetState(eProxyToNextPeer);
            return true;
        }
        else if (m_ActiveHub->GetStatus() != eNCHubCmdInProgress)
            abort();

        if (!m_GotInitialAnswer) {
            g_NetcacheServer->DeferConnectionProcessing(m_Socket);
            return false;
        }
    }

    if (m_SockBuffer.GetReadReadySize() < 4) {
        if (!m_SockBuffer.ReadToBuf()
            ||  m_SockBuffer.GetReadReadySize() < 4)
        {
            return false;
        }
    }

    Uint4 sig = *static_cast<const Uint4*>(m_SockBuffer.GetReadBufData());
    m_SockBuffer.EraseReadBufData(4);

    if (sig == 0x04030201) {
        x_SetFlag(fSwapLengthBytes);
        x_SetState(eReadBlobChunkLength);
    }
    else if (sig == 0x01020304) {
        x_UnsetFlag(fSwapLengthBytes);
        x_SetState(eReadBlobChunkLength);
    }
    else {
        m_CmdCtx->SetRequestStatus(eStatus_BadCmd);
        ERR_POST("Cannot determine the byte order. Got: "
                 << NStr::UIntToString(sig, 0, 16));
        //abort();
        x_CloseConnection();
    }
    return true;
}

bool
CNCMessageHandler::x_ReadBlobChunkLength(void)
{
    if (m_ActiveHub) {
        if (m_ActiveHub->GetStatus() == eNCHubError) {
            ERR_POST(Critical << "Error executing command on peer: "
                              << m_ActiveHub->GetErrMsg());
            m_CmdCtx->SetRequestStatus(eStatus_PeerError);
            x_CloseConnection();
            return true;
        }
        else if (m_ActiveHub->GetStatus() != eNCHubCmdInProgress)
            abort();
    }

    if (x_IsFlagSet(fSkipBlobEOF)  &&  m_BlobSize == m_Size) {
        // Workaround for old STRS
        m_ChunkLen = 0xFFFFFFFF;
    }
    else {
        if (m_SockBuffer.GetReadReadySize() < 4) {
            if (!m_SockBuffer.ReadToBuf()
                ||  m_SockBuffer.GetReadReadySize() < 4)
            {
                return false;
            }
        }

        m_ChunkLen = *static_cast<const Uint4*>(m_SockBuffer.GetReadBufData());
        m_SockBuffer.EraseReadBufData(4);
        if (x_IsFlagSet(fSwapLengthBytes)) {
            m_ChunkLen = CByteSwap::GetInt4(
                         reinterpret_cast<const unsigned char*>(&m_ChunkLen));
        }
    }

    if (m_ChunkLen == 0xFFFFFFFF) {
        if (m_ActiveHub) {
            m_ActiveHub->GetHandler()->FinishBlobFromClient();
            x_SetState(eWaitForActiveWrite);
        }
        else {
            x_FinishReadingBlob();
        }
    }
    else if (x_IsFlagSet(fReadExactBlobSize)
             &&  m_BlobSize + m_ChunkLen > m_Size)
    {
        m_CmdCtx->SetRequestStatus(eStatus_CondFailed);
        ERR_POST("Too much data for blob size " << m_Size
                 << " (received at least "
                 << (m_BlobSize + m_ChunkLen) << " bytes)");
        //abort();
        x_CloseConnection();
    }
    else if (m_ActiveHub) {
        CNCActiveHandler* handler = m_ActiveHub->GetHandler();
        CBufferedSockReaderWriter& active_buf = handler->GetSockBuffer();
        active_buf.WriteNoFail(&m_ChunkLen, sizeof(m_ChunkLen));
        x_SetState(eReadChunkToActive);
    }
    else {
        x_SetState(eReadBlobChunk);
    }
    return true;
}

bool
CNCMessageHandler::x_ReadBlobChunk(void)
{
    while (m_ChunkLen != 0) {
        Uint4 read_len = Uint4(m_BlobAccess->GetWriteMemSize());
        if (m_BlobAccess->HasError()) {
            m_CmdCtx->SetRequestStatus(eStatus_ServerError);
            x_CloseConnection();
            return true;
        }
        if (read_len > m_ChunkLen)
            read_len = m_ChunkLen;
        Uint4 n_read = Uint4(m_SockBuffer.Read(m_BlobAccess->GetWriteMemPtr(),
                                               read_len));
        if (n_read == 0)
            return false;
        m_BlobAccess->MoveWritePos(n_read);
        m_ChunkLen -= n_read;
        m_BlobSize += n_read;
    }
    x_SetState(eReadBlobChunkLength);
    return true;
}

bool
CNCMessageHandler::x_ReadChunkToActive(void)
{
    x_UnsetFlag(fWaitForActive);

    if (m_ActiveHub->GetStatus() == eNCHubError) {
        m_CmdCtx->SetRequestStatus(eStatus_PeerError);
        x_CloseConnection();
        return true;
    }
    else if (m_ActiveHub->GetStatus() != eNCHubCmdInProgress)
        abort();

    CNCActiveHandler* handler = m_ActiveHub->GetHandler();
    CBufferedSockReaderWriter& active_buf = handler->GetSockBuffer();
    while (m_ChunkLen != 0) {
        size_t n_read = active_buf.WriteFromBuffer(m_SockBuffer, m_ChunkLen);
        m_ChunkLen -= Uint4(n_read);
        m_BlobSize += n_read;
        if (m_SockBuffer.HasError()) {
            m_CmdCtx->SetRequestStatus(eStatus_BadCmd);
            x_CloseConnection();
            return true;
        }
        if (active_buf.HasError()) {
            m_CmdCtx->SetRequestStatus(eStatus_PeerError);
            x_CloseConnection();
            return true;
        }
        if (active_buf.IsWriteDataPending()  ||  m_SockBuffer.HasDataToRead()) {
            x_SetFlag(fWaitForActive);
            handler->ForceBufferFlush();
            g_NetcacheServer->DeferConnectionProcessing(m_Socket);
            return false;
        }
        if (n_read == 0) {
            return false;
        }
    }
    x_SetState(eReadBlobChunkLength);
    return true;
}

bool
CNCMessageHandler::x_WaitForFirstData(void)
{
    if (m_BlobAccess->ObtainFirstData(this) == eNCWouldBlock) {
        x_WaitForWouldBlock();
        return false;
    }
    if (m_BlobAccess->HasError()) {
        m_CmdCtx->SetRequestStatus(eStatus_ServerError);
        x_CloseConnection();
    }
    else
        x_SetState(eWriteBlobData);
    return true;
}

bool
CNCMessageHandler::x_WriteBlobData(void)
{
    size_t n_written;
    do {
        size_t want_read = m_BlobAccess->GetDataSize();
        if (m_BlobAccess->HasError()) {
            m_CmdCtx->SetRequestStatus(eStatus_ServerError);
            x_CloseConnection();
            return true;
        }
        if (want_read == 0) {
            x_SetState(eReadyForCommand);
            return true;
        }
        if (m_Size != Uint8(-1)  &&  m_Size < want_read)
            want_read = size_t(m_Size);

        n_written = m_SockBuffer.Write(m_BlobAccess->GetDataPtr(), want_read);
        // HasError is processed in ManageCmdPipeline
        if (m_Size != Uint8(-1))
            m_Size -= n_written;
        m_BlobAccess->MoveCurPos(n_written);
    }
    while (!m_SockBuffer.HasError()  &&  n_written != 0);

    return false;
}

bool
CNCMessageHandler::x_WriteSendBuff(void)
{
    size_t n_written = m_SockBuffer.Write(m_SendBuff->data() + m_SendPos,
                                          m_SendBuff->size() - m_SendPos);
    // HasError() will be processed in ManageCmdPipeline()
    m_SendPos += n_written;
    if (m_SendPos == m_SendBuff->size()) {
        x_SetState(eReadyForCommand);
        return true;
    }
    return false;
}

bool
CNCMessageHandler::x_ProxyToNextPeer(void)
{
    if (m_SrvsIndex >= m_CheckSrvs.size()) {
        ERR_POST(Warning << "Cannot connect to peer servers");
        m_SockBuffer.WriteMessage("ERR:", "Cannot connect to peer servers");
        m_CmdCtx->SetRequestStatus(eStatus_PeerError);
        x_SetState(eReadyForCommand);
        return true;
    }

    Uint8 srv_id = m_CheckSrvs[m_SrvsIndex++];
    if (m_ActiveHub)
        abort();
    m_ActiveHub = CNCActiveClientHub::Create(srv_id, this);
    x_SetState(eSendCmdAsProxy);
    return true;
}

bool
CNCMessageHandler::x_SendCmdAsProxy(void)
{
    if (m_ActiveHub->GetStatus() == eNCHubWaitForConn) {
        g_NetcacheServer->DeferConnectionProcessing(m_Socket);
        return false;
    }
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        x_SetState(eProxyToNextPeer);
        return true;
    }
    if (m_ActiveHub->GetStatus() != eNCHubConnReady)
        abort();

    m_NeedFlushBuff = false;
    m_GotInitialAnswer = false;

    bool need_reader = false;
    bool need_writer = false;
    if (NStr::CompareCase(m_CurCmd, "GET2") == 0
        ||  NStr::CompareCase(m_CurCmd, "IC_READ") == 0
        ||  NStr::CompareCase(m_CurCmd, "GETPART") == 0
        ||  NStr::CompareCase(m_CurCmd, "IC_READPART") == 0
        ||  NStr::CompareCase(m_CurCmd, "PROXY_GET") == 0
        ||  NStr::CompareCase(m_CurCmd, "GET") == 0)
    {
        m_ActiveHub->GetHandler()->ProxyRead(m_CmdCtx, m_BlobKey, m_RawBlobPass,
                                             m_BlobVersion, m_StartPos, m_Size,
                                             m_Quorum, m_SearchOnRead, m_ForceLocal);
        need_reader = true;
    }
    else if (NStr::CompareCase(m_CurCmd, "PUT3") == 0
             ||  NStr::CompareCase(m_CurCmd, "IC_STOR") == 0
             ||  NStr::CompareCase(m_CurCmd, "IC_STRS") == 0
             ||  NStr::CompareCase(m_CurCmd, "PUT2") == 0)
    {
        m_ActiveHub->GetHandler()->ProxyWrite(m_CmdCtx, m_BlobKey, m_RawBlobPass,
                                              m_BlobVersion, m_BlobTTL, m_Quorum);
        need_writer = true;
        if (NStr::CompareCase(m_CurCmd, "PUT3") == 0  ||  m_ConfirmPut)
            x_SetFlag(fConfirmBlobPut);
    }
    else if (NStr::CompareCase(m_CurCmd, "IC_HASB") == 0
             ||  NStr::CompareCase(m_CurCmd, "HASB") == 0)
    {
        m_ActiveHub->GetHandler()->ProxyHasBlob(m_CmdCtx, m_BlobKey, m_RawBlobPass,
                                                m_Quorum);
    }
    else if (NStr::CompareCase(m_CurCmd, "GetSIZe") == 0
             ||  NStr::CompareCase(m_CurCmd, "IC_GetSIZe") == 0
             ||  NStr::CompareCase(m_CurCmd, "PROXY_GetSIZe") == 0)
    {
        m_ActiveHub->GetHandler()->ProxyGetSize(m_CmdCtx, m_BlobKey, m_RawBlobPass,
                                                m_BlobVersion, m_Quorum,
                                                m_SearchOnRead, m_ForceLocal);
    }
    else if (NStr::CompareCase(m_CurCmd, "IC_READLAST") == 0
             ||  NStr::CompareCase(m_CurCmd, "PROXY_READLAST") == 0)
    {
        m_ActiveHub->GetHandler()->ProxyReadLast(m_CmdCtx, m_BlobKey, m_RawBlobPass,
                                                 m_StartPos, m_Size, m_Quorum,
                                                 m_SearchOnRead, m_ForceLocal);
        need_reader = true;
    }
    else if (NStr::CompareCase(m_CurCmd, "IC_SETVALID") == 0) {
        m_ActiveHub->GetHandler()->ProxySetValid(m_CmdCtx, m_BlobKey, m_RawBlobPass,
                                                 m_BlobVersion);
    }
    else if (NStr::CompareCase(m_CurCmd, "RMV2") == 0
             ||  NStr::CompareCase(m_CurCmd, "REMOVE") == 0
             ||  NStr::CompareCase(m_CurCmd, "IC_REMOve") == 0)
    {
        m_ActiveHub->GetHandler()->ProxyRemove(m_CmdCtx, m_BlobKey, m_RawBlobPass,
                                               m_BlobVersion, m_Quorum);
    }
    else if (NStr::CompareCase(m_CurCmd, "GETMETA") == 0
             ||  NStr::CompareCase(m_CurCmd, "IC_GETMETA") == 0)
    {
        m_ActiveHub->GetHandler()->ProxyGetMeta(m_CmdCtx, m_BlobKey,
                                                m_Quorum, m_ForceLocal);
        need_reader = true;
    }
    else
        abort();

    if (need_reader)
        x_SetState(eWaitForActiveWrite);
    else if (need_writer)
        x_SetState(eReadBlobSignature);
    else
        x_SetState(eWaitForPeerAnswer);
    return true;
}

bool
CNCMessageHandler::x_WaitForPeerAnswer(void)
{
    if (m_ActiveHub->GetStatus() == eNCHubCmdInProgress) {
        g_NetcacheServer->DeferConnectionProcessing(m_Socket);
        return false;
    }
    
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        if (NStr::StartsWith(m_ActiveHub->GetErrMsg(), "ERR:BLOB not found")) {
            m_CmdCtx->SetRequestStatus(eStatus_NotFound);
            goto successful_finish;
        }
        else {
            ERR_POST(Warning << "Error executing command on peer: "
                             << m_ActiveHub->GetErrMsg());
            x_SetState(eProxyToNextPeer);
        }
    }
    else if (m_ActiveHub->GetStatus() == eNCHubSuccess) {
successful_finish:
        m_SockBuffer.WriteMessage("", m_ActiveHub->GetErrMsg());
        x_SetState(eReadyForCommand);
    }
    else
        abort();

    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    return true;
}

bool
CNCMessageHandler::x_WaitForActiveWrite(void)
{
    if (m_ActiveHub->GetStatus() == eNCHubCmdInProgress) {
        g_NetcacheServer->DeferConnectionProcessing(m_Socket);
        return false;
    }
    
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        if (NStr::StartsWith(m_ActiveHub->GetErrMsg(), "ERR:BLOB not found")) {
            m_CmdCtx->SetRequestStatus(eStatus_NotFound);
            m_SockBuffer.WriteMessage("", m_ActiveHub->GetErrMsg());
            goto successful_finish;
        }
        else {
            ERR_POST(Warning << "Error executing command on peer: "
                             << m_ActiveHub->GetErrMsg());
            if (m_GotInitialAnswer) {
                m_CmdCtx->SetRequestStatus(eStatus_PeerError);
                x_CloseConnection();
                return true;
            }
            else {
                x_SetState(eProxyToNextPeer);
            }
        }
    }
    else if (m_ActiveHub->GetStatus() == eNCHubSuccess) {
successful_finish:
        x_SetState(eReadyForCommand);
    }
    else
        abort();

    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    return true;
}

bool
CNCMessageHandler::x_ReadMetaNextPeer(void)
{
    if (m_SrvsIndex >= m_CheckSrvs.size()) {
        x_SetState(eCommandReceived);
        return true;
    }

    Uint8 srv_id = m_CheckSrvs[m_SrvsIndex++];
    if (m_ActiveHub)
        abort();
    m_ActiveHub = CNCActiveClientHub::Create(srv_id, this);
    x_SetState(eSendGetMetaCmd);
    return true;
}

bool
CNCMessageHandler::x_SendGetMetaCmd(void)
{
    if (m_ActiveHub->GetStatus() == eNCHubWaitForConn) {
        g_NetcacheServer->DeferConnectionProcessing(m_Socket);
        return false;
    }
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        x_SetState(eReadMetaNextPeer);
        return true;
    }
    if (m_ActiveHub->GetStatus() != eNCHubConnReady)
        abort();

    m_ActiveHub->GetHandler()->SearchMeta(m_CmdCtx, m_BlobKey);
    x_SetState(eReadMetaResults);
    return true;
}

bool
CNCMessageHandler::x_ReadMetaResults(void)
{
    CNCActiveHandler* handler;
    bool cur_exist;
    const SNCBlobSummary* cur_blob_sum;
    bool for_hasb = m_CmdProcessor == &CNCMessageHandler::x_DoCmd_HasBlobImpl;

    if (m_ActiveHub->GetStatus() == eNCHubCmdInProgress) {
        g_NetcacheServer->DeferConnectionProcessing(m_Socket);
        return false;
    }
    if (m_ActiveHub->GetStatus() == eNCHubError)
        goto results_processed;
    if (m_ActiveHub->GetStatus() != eNCHubSuccess)
        abort();

    handler = m_ActiveHub->GetHandler();
    cur_blob_sum = &handler->GetBlobSummary();
    cur_exist = handler->IsBlobExists()
                &&  cur_blob_sum->expire > int(time(NULL));
    if (!cur_exist  &&  !for_hasb)
        goto results_processed;

    if (cur_exist  &&  for_hasb) {
        m_LatestExist = true;
        goto meta_search_finished;
    }
    if (cur_exist  &&  (!m_LatestExist
                        ||  (!cur_blob_sum->isSameData(m_LatestBlobSum)
                             &&  m_LatestBlobSum.isOlder(*cur_blob_sum))))
    {
        m_LatestExist = true;
        m_LatestSrvId = m_CheckSrvs[m_SrvsIndex - 1];
        m_LatestBlobSum = *cur_blob_sum;
    }
    if (m_Quorum == 1)
        goto meta_search_finished;
    if (m_Quorum != 0)
        --m_Quorum;

results_processed:
    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    x_SetState(eReadMetaNextPeer);
    return true;

meta_search_finished:
    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    m_CheckSrvs.clear();
    m_SrvsIndex = 0;
    x_SetState(eCommandReceived);
    return true;
}

bool
CNCMessageHandler::x_PutToNextPeer(void)
{
    if (m_SrvsIndex >= m_CheckSrvs.size()) {
        x_SetState(eReadyForCommand);
        return true;
    }

    Uint8 srv_id = m_CheckSrvs[m_SrvsIndex++];
    m_ActiveHub = CNCActiveClientHub::Create(srv_id, this);
    x_SetState(eSendPutToPeerCmd);
    return true;
}

bool
CNCMessageHandler::x_SendPutToPeerCmd(void)
{
    if (m_ActiveHub->GetStatus() == eNCHubWaitForConn) {
        g_NetcacheServer->DeferConnectionProcessing(m_Socket);
        return false;
    }
    if (m_ActiveHub->GetStatus() == eNCHubError) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        x_SetState(ePutToNextPeer);
        return true;
    }
    if (m_ActiveHub->GetStatus() != eNCHubSuccess)
        abort();

    m_ActiveHub->GetHandler()->CopyPut(m_CmdCtx, m_BlobKey, m_BlobSlot, m_OrigRecNo);
    x_SetState(eReadPutResults);
    return true;
}

bool
CNCMessageHandler::x_ReadPutResults(void)
{
    if (m_ActiveHub->GetStatus() == eNCHubCmdInProgress) {
        g_NetcacheServer->DeferConnectionProcessing(m_Socket);
        return false;
    }
    if (m_ActiveHub->GetStatus() == eNCHubError)
        goto results_processed;
    if (m_ActiveHub->GetStatus() != eNCHubSuccess)
        abort();

    if (m_Quorum == 1) {
        m_ActiveHub->Release();
        m_ActiveHub = NULL;
        x_SetState(eReadyForCommand);
        return true;
    }
    if (m_Quorum != 0)
        --m_Quorum;

results_processed:
    m_ActiveHub->Release();
    m_ActiveHub = NULL;
    x_SetState(ePutToNextPeer);
    return true;
}

inline unsigned int
CNCMessageHandler::x_GetBlobTTL(void)
{
    return m_BlobTTL? m_BlobTTL: m_AppSetup->blob_ttl;
}

void
CNCMessageHandler::x_ManageCmdPipeline(void)
{
    CDiagnosticsGuard guard(this);

    if (m_InSyncCmd) {
        m_WaitForThrottle = false;
        CNCPeriodicSync::BeginTimeEvent(m_SrvId);
    }

    m_SockBuffer.Flush();
    if (m_NeedFlushBuff  &&  !m_SockBuffer.IsWriteDataPending()) {
        m_NeedFlushBuff = false;
    }
    bool do_next_step = true;
    while (!m_SockBuffer.HasError()  &&  !m_SockBuffer.IsWriteDataPending()
           &&  do_next_step  &&  x_GetState() != eSocketClosed)
    {
        switch (x_GetState()) {
        case ePreAuthenticated:
            do_next_step = x_ReadAuthMessage();
            break;
        case eReadyForCommand:
            x_FinishCommand(true);
            do_next_step = x_ReadCommand();
            break;
        case eCommandReceived:
            do_next_step = x_DoCommand();
            break;
        case eWaitForBlobAccess:
            do_next_step = x_WaitForBlobAccess();
            break;
        case ePasswordFailed:
            do_next_step = x_ProcessBadPassword();
            break;
        case eReadBlobSignature:
            do_next_step = x_ReadBlobSignature();
            break;
        case eReadBlobChunkLength:
            do_next_step = x_ReadBlobChunkLength();
            break;
        case eReadBlobChunk:
            do_next_step = x_ReadBlobChunk();
            break;
        case eReadChunkToActive:
            do_next_step = x_ReadChunkToActive();
            break;
        case eWaitForFirstData:
            do_next_step = x_WaitForFirstData();
            break;
        case eWriteBlobData:
            do_next_step = x_WriteBlobData();
            break;
        case eWriteSendBuff:
            do_next_step = x_WriteSendBuff();
            break;
        case eProxyToNextPeer:
            do_next_step = x_ProxyToNextPeer();
            break;
        case eSendCmdAsProxy:
            do_next_step = x_SendCmdAsProxy();
            break;
        case eWaitForPeerAnswer:
            do_next_step = x_WaitForPeerAnswer();
            break;
        case eWaitForActiveWrite:
            do_next_step = x_WaitForActiveWrite();
            break;
        case eReadMetaNextPeer:
            do_next_step = x_ReadMetaNextPeer();
            break;
        case eSendGetMetaCmd:
            do_next_step = x_SendGetMetaCmd();
            break;
        case eReadMetaResults:
            do_next_step = x_ReadMetaResults();
            break;
        case ePutToNextPeer:
            do_next_step = x_PutToNextPeer();
            break;
        case eSendPutToPeerCmd:
            do_next_step = x_SendPutToPeerCmd();
            break;
        case eReadPutResults:
            do_next_step = x_ReadPutResults();
            break;
        default:
            abort();
        }

        if (!m_SockBuffer.HasError()  &&  m_SockBuffer.IsWriteDataPending()) {
            m_SockBuffer.Flush();
            do_next_step = !m_SockBuffer.IsWriteDataPending();
        }
    }

    if (m_SockBuffer.HasError()  &&  x_GetState() != eSocketClosed) {
        if (m_CmdCtx) {
            m_CmdCtx->SetRequestStatus(eStatus_BadCmd);
        }
        else {
            m_ConnCtx->SetRequestStatus(eStatus_ServerError);
        }
        x_CloseConnection();
    }
#ifdef NCBI_OS_LINUX
    else if (x_GetState() != eSocketClosed) {
        int fd = 0, val = 1;
        m_Socket->GetOSHandle(&fd, sizeof(fd));
        setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
    }
#endif

    if (m_InSyncCmd) {
        CNCPeriodicSync::EndTimeEvent(m_SrvId, 0);
    }
}

bool
CNCMessageHandler::x_DoCmd_Alive(void)
{
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Health(void)
{
    string health_coeff = "1";
    if (g_NCStorage->NeedStopWrite())
        health_coeff = "0";
    else if (!CNetCacheServer::IsCachingComplete())
        health_coeff = "0.1";
    else if (!CNetCacheServer::IsInitiallySynced())
        health_coeff = "0.5";
    m_SockBuffer.WriteMessage("OK:", "HEALTH_COEFF=" + health_coeff);
    m_SockBuffer.WriteMessage("OK:", "UP_TIME=" + NStr::IntToString(g_NetcacheServer->GetUpTime()));
    m_SockBuffer.WriteMessage("OK:", string("CACHING_COMPLETE=") + (CNetCacheServer::IsCachingComplete()? "yes": "no"));
    m_SockBuffer.WriteMessage("OK:", string("INITALLY_SYNCED=") + (CNetCacheServer::IsInitiallySynced()? "yes": "no"));
    m_SockBuffer.WriteMessage("OK:", "MEM_LIMIT=" + NStr::UInt8ToString(CNCMemManager::GetMemoryLimit()));
    m_SockBuffer.WriteMessage("OK:", "MEM_USED=" + NStr::UInt8ToString(CNCMemManager::GetMemoryUsed()));
    //m_SockBuffer.WriteMessage("OK:", "DISK_CACHE=" + NStr::UInt8ToString(CNCMemManager::GetMemoryLimit()));
    m_SockBuffer.WriteMessage("OK:", "DISK_FREE=" + NStr::UInt8ToString(g_NetcacheServer->GetDiskFree()));
    m_SockBuffer.WriteMessage("OK:", "DISK_USED=" + NStr::UInt8ToString(g_NCStorage->GetDBSize()));
    m_SockBuffer.WriteMessage("OK:", "N_DB_FILES=" + NStr::IntToString(g_NCStorage->GetNDBFiles()));
    m_SockBuffer.WriteMessage("OK:", "COPY_QUEUE_SIZE=" + NStr::UInt8ToString(CNCPeerControl::GetMirrorQueueSize()));

    typedef map<Uint8, string> TPeers;
    const TPeers& peers(CNCDistributionConf::GetPeers());
    ITERATE(TPeers, it_peer, peers) {
        m_SockBuffer.WriteMessage("OK:", "QUEUE_SIZE_" + NStr::UInt8ToString(it_peer->first) + "=" + NStr::UInt8ToString(CNCPeerControl::GetMirrorQueueSize(it_peer->first)));
    }
    m_SockBuffer.WriteMessage("OK:", "SYNC_LOG_SIZE=" + NStr::UInt8ToString(CNCSyncLog::GetLogSize()));
    m_SockBuffer.WriteMessage("OK:", "CONN_LIMIT=" + NStr::IntToString(g_NetcacheServer->GetMaxConnections()));

    CNCStat stat;
    CNCStat::CollectAllStats(stat);
    m_SockBuffer.WriteMessage("OK:", "CONN_USED=" + NStr::UIntToString(stat.GetConnsUsed()));
    m_SockBuffer.WriteMessage("OK:", "CMD_IN_PROGRESS=" + NStr::UIntToString(stat.GetProgressCmds()));
    m_SockBuffer.WriteMessage("OK:", "CONN_USED_1_SEC=" + NStr::UIntToString(stat.GetConnsUsed1Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_OPENED_1_SEC=" + NStr::UIntToString(stat.GetConnsOpened1Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_CLOSED_1_SEC=" + NStr::UIntToString(stat.GetConnsClosed1Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_OVERFLOW_1_SEC=" + NStr::UIntToString(stat.GetConnsOverflow1Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_USER_ERR_1_SEC=" + NStr::UIntToString(stat.GetUserErrs1Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_SERV_ERR_1_SEC=" + NStr::UIntToString(stat.GetServErrs1Sec()));
    m_SockBuffer.WriteMessage("OK:", "DATA_READ_1_SEC=" + NStr::UInt8ToString(stat.GetReadSize1Sec()));
    m_SockBuffer.WriteMessage("OK:", "DATA_WRITTEN_1_SEC=" + NStr::UInt8ToString(stat.GetWrittenSize1Sec()));
    m_SockBuffer.WriteMessage("OK:", "CMD_IN_PROGRESS_1_SEC=" + NStr::UIntToString(stat.GetProgressCmds1Sec()));
    const CNCStat::TCmdsCountsMap& count_map = stat.GetStartedCmds1Sec();
    ITERATE(CNCStat::TCmdsCountsMap, it_count, count_map) {
        m_SockBuffer.WriteMessage("OK:", string("CMD_STARTED_") + it_count->first + "_1_SEC=" + NStr::UInt8ToString(it_count->second));
    }
    const CNCStat::TStatCmdsSpansMap& stat_map = stat.GetCmdSpans1Sec();
    ITERATE(CNCStat::TStatCmdsSpansMap, it_stat, stat_map) {
        string status = NStr::IntToString(it_stat->first);
        const CNCStat::TCmdsSpansMap& span_map = it_stat->second;
        ITERATE(CNCStat::TCmdsSpansMap, it_span, span_map) {
            m_SockBuffer.WriteMessage("OK:", string("CMD_FINISHED_") + it_span->first + "_" + status + "_1_SEC=" + NStr::UInt8ToString(it_span->second.GetCount()));
            m_SockBuffer.WriteMessage("OK:", string("CMD_TIME_") + it_span->first + "_" + status + "_1_SEC=" + NStr::DoubleToString(it_span->second.GetAverage()));
        }
    }

    m_SockBuffer.WriteMessage("OK:", "CONN_USED_5_SEC=" + NStr::UIntToString(stat.GetConnsUsed5Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_OPENED_5_SEC=" + NStr::UIntToString(stat.GetConnsOpened5Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_CLOSED_5_SEC=" + NStr::UIntToString(stat.GetConnsClosed5Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_OVERFLOW_5_SEC=" + NStr::UIntToString(stat.GetConnsOverflow5Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_USER_ERR_5_SEC=" + NStr::UIntToString(stat.GetUserErrs5Sec()));
    m_SockBuffer.WriteMessage("OK:", "CONN_SERV_ERR_5_SEC=" + NStr::UIntToString(stat.GetServErrs5Sec()));
    m_SockBuffer.WriteMessage("OK:", "DATA_READ_5_SEC=" + NStr::UInt8ToString(stat.GetReadSize5Sec()));
    m_SockBuffer.WriteMessage("OK:", "DATA_WRITTEN_5_SEC=" + NStr::UInt8ToString(stat.GetWrittenSize5Sec()));
    m_SockBuffer.WriteMessage("OK:", "CMD_IN_PROGRESS_5_SEC=" + NStr::UIntToString(stat.GetProgressCmds5Sec()));
    CNCStat::TCmdsCountsMap count_map5;
    stat.GetStartedCmds5Sec(&count_map5);
    ITERATE(CNCStat::TCmdsCountsMap, it_count, count_map5) {
        m_SockBuffer.WriteMessage("OK:", string("CMD_STARTED_") + it_count->first + "_5_SEC=" + NStr::UInt8ToString(it_count->second));
    }
    CNCStat::TStatCmdsSpansMap stat_map5;
    stat.GetCmdSpans5Sec(&stat_map5);
    ITERATE(CNCStat::TStatCmdsSpansMap, it_stat, stat_map5) {
        string status = NStr::IntToString(it_stat->first);
        const CNCStat::TCmdsSpansMap& span_map = it_stat->second;
        ITERATE(CNCStat::TCmdsSpansMap, it_span, span_map) {
            m_SockBuffer.WriteMessage("OK:", string("CMD_FINISHED_") + it_span->first + "_" + status + "_5_SEC=" + NStr::UInt8ToString(it_span->second.GetCount()));
            m_SockBuffer.WriteMessage("OK:", string("CMD_TIME_") + it_span->first + "_" + status + "_5_SEC=" + NStr::DoubleToString(it_span->second.GetAverage()));
        }
    }

    m_SockBuffer.WriteMessage("OK:", "END");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Shutdown(void)
{
    if (x_CheckAdminClient()) {
        g_NetcacheServer->RequestShutdown();
        m_SockBuffer.WriteMessage("OK:", "");
    }
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Version(void)
{
    m_SockBuffer.WriteMessage("OK:", NETCACHED_HUMAN_VERSION);
    return true;
}

AutoPtr<CConn_SocketStream>
CNCMessageHandler::x_PrepareSockStream(void)
{
    SOCK sk = m_Socket->GetSOCK();
    m_Socket->SetOwnership(eNoOwnership);
    m_Socket->Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);
    x_CloseConnection();

    return new CConn_SocketStream(sk, eTakeOwnership);
}

bool
CNCMessageHandler::x_DoCmd_GetConfig(void)
{
    CNcbiApplication::Instance()->GetConfig().Write(*x_PrepareSockStream());
    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetServerStats(void)
{
    g_NetcacheServer->PrintServerStats(x_PrepareSockStream().get());
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Reinit(void)
{
    m_SockBuffer.WriteMessage("ERR:", "Not implemented");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Reconf(void)
{
    m_SockBuffer.WriteMessage("ERR:", "Not implemented");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Put2(void)
{
    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
    m_BlobAccess->SetVersionTTL(0);
    m_BlobAccess->SetBlobVersion(0);
    m_SockBuffer.WriteMessage("OK:ID:", m_RawKey);
    x_StartReadingBlob();
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Put3(void)
{
    x_SetFlag(fConfirmBlobPut);
    return x_DoCmd_Put2();
}

bool
CNCMessageHandler::x_DoCmd_Get(void)
{
    if (m_LatestSrvId != CNCDistributionConf::GetSelfID()) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("proxy", "1");
        extra.Flush();

        m_Quorum = 1;
        m_SearchOnRead = false;
        m_ForceLocal = true;
        m_CheckSrvs.push_back(m_LatestSrvId);
        x_SetState(eProxyToNextPeer);
        return true;
    }
    if (!m_LatestExist) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return true;
    }

    x_ProlongBlobDeadTime();
    Uint8 blob_size = m_BlobAccess->GetCurBlobSize();
    if (blob_size < m_StartPos)
        blob_size = 0;
    else
        blob_size -= m_StartPos;
    if (m_Size != Uint8(-1)  &&  m_Size < blob_size) {
        blob_size = m_Size;
    }
    m_SockBuffer.WriteMessage("OK:", "BLOB found. SIZE="
                                     + NStr::UInt8ToString(blob_size));

    if (blob_size != 0) {
        m_BlobAccess->SetPosition(m_StartPos);
        x_SetState(eWaitForFirstData);
    }
    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetLast(void)
{
    if (m_LatestSrvId != CNCDistributionConf::GetSelfID()) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("proxy", "1");
        extra.Flush();

        m_Quorum = 1;
        m_SearchOnRead = false;
        m_ForceLocal = true;
        m_CheckSrvs.push_back(m_LatestSrvId);
        x_SetState(eProxyToNextPeer);
        return true;
    }
    if (!m_LatestExist) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return true;
    }

    x_ProlongBlobDeadTime();
    Uint8 blob_size = m_BlobAccess->GetCurBlobSize();
    if (blob_size < m_StartPos)
        blob_size = 0;
    else
        blob_size -= m_StartPos;
    if (m_Size != Uint8(-1)  &&  m_Size < blob_size) {
        blob_size = m_Size;
    }

    string result("BLOB found. SIZE=");
    result += NStr::Int8ToString(blob_size);
    result += ", VER=";
    result += NStr::UIntToString(m_BlobAccess->GetCurBlobVersion());
    result += ", VALID=";
    result += (m_BlobAccess->IsCurVerExpired()? "false": "true");
    m_SockBuffer.WriteMessage("OK:", result);

    if (blob_size != 0) {
        m_BlobAccess->SetPosition(0);
        x_SetState(eWaitForFirstData);
    }
    return true;
}

bool
CNCMessageHandler::x_DoCmd_SetValid(void)
{
    if (!m_BlobAccess->IsBlobExists()  ||  m_BlobAccess->IsCurBlobExpired()) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
    }
    else if (m_BlobAccess->GetCurBlobVersion() != m_BlobVersion) {
        m_CmdCtx->SetRequestStatus(eStatus_RaceCond);
        m_SockBuffer.WriteMessage("OK:", "BLOB was changed.");
    }
    else {
        x_ProlongVersionLife();
        m_SockBuffer.WriteMessage("OK:", "");
    }
    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetSize(void)
{
    if (m_LatestSrvId != CNCDistributionConf::GetSelfID()) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("proxy", "1");
        extra.Flush();

        m_Quorum = 1;
        m_SearchOnRead = false;
        m_ForceLocal = true;
        m_CheckSrvs.push_back(m_LatestSrvId);
        x_SetState(eProxyToNextPeer);
        return true;
    }
    if (!m_LatestExist) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
    }
    else {
        x_ProlongBlobDeadTime();
        Uint8 size = m_BlobAccess->GetCurBlobSize();
        m_SockBuffer.WriteMessage("OK:", NStr::UInt8ToString(size));
    }

    return true;
}

bool
CNCMessageHandler::x_DoCmd_HasBlob(void)
{
    m_LatestExist = g_NCStorage->IsBlobExists(m_BlobSlot, m_BlobKey);
    if (!m_LatestExist  &&  m_Quorum != 1) {
        if (m_Quorum != 0)
            --m_Quorum;
        x_GetCurSlotServers();
        m_CmdProcessor = &CNCMessageHandler::x_DoCmd_HasBlobImpl;
        x_SetState(eReadMetaNextPeer);
        return true;
    }
    else
        return x_DoCmd_HasBlobImpl();
}

bool
CNCMessageHandler::x_DoCmd_HasBlobImpl(void)
{
    if (!m_LatestExist)
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
    m_SockBuffer.WriteMessage("OK:", NStr::IntToString(int(m_LatestExist)));
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Remove(void)
{
    if ((!m_BlobAccess->IsBlobExists()  ||  m_BlobAccess->IsCurBlobExpired())
        &&  CNetCacheServer::IsInitiallySynced())
    {
        return true;
    }

    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
    m_BlobAccess->SetBlobVersion(m_BlobVersion);
    int expire = int(time(NULL)) - 1;
    int ttl = m_BlobAccess->GetNewBlobTTL();
    if (m_BlobAccess->IsBlobExists()  &&  m_BlobAccess->GetCurBlobTTL() > ttl)
        ttl = m_BlobAccess->GetCurBlobTTL();
    m_BlobAccess->SetNewBlobExpire(expire, expire + ttl + 1);
    x_FinishReadingBlob();
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Remove2(void)
{
    x_DoCmd_Remove();
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_Store(void)
{
    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
    m_BlobAccess->SetVersionTTL(m_AppSetup->ver_ttl);
    m_BlobAccess->SetBlobVersion(m_BlobVersion);
    m_SockBuffer.WriteMessage("OK:", "");
    if (m_Size != 0) {
        if (m_Size != Uint8(-1)) {
            x_SetFlag(fReadExactBlobSize);
            x_SetFlag(fSkipBlobEOF);
        }
        if (m_ConfirmPut)
            x_SetFlag(fConfirmBlobPut);
        x_StartReadingBlob();
    }
    return true;
}

void
CNCMessageHandler::x_ReadFullBlobsList(void)
{
    TNCBlobSumList blobs_list;
    g_NCStorage->GetFullBlobsList(m_Slot, blobs_list);
    m_SendBuff.reset(new TNCBufferType());
    m_SendBuff->reserve_mem(blobs_list.size() * 200);
    ITERATE(TNCBlobSumList, it_blob, blobs_list) {
        const string& key = it_blob->first;
        SNCCacheData* blob_sum = it_blob->second;
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
    }
}

bool
CNCMessageHandler::x_DoCmd_SyncStart(void)
{
    if (!x_CheckAdminClient())
        return true;

    TReducedSyncEvents sync_events;
    ESyncInitiateResult sync_res = CNCPeriodicSync::Initiate(m_SrvId, m_Slot,
                                                &m_LocalRecNo, &m_RemoteRecNo,
                                                &sync_events, &m_SyncId);
    if (sync_res == eCrossSynced) {
        m_CmdCtx->SetRequestStatus(eStatus_CrossSync);
        m_SockBuffer.WriteMessage("OK:", "CROSS_SYNC,SIZE=0");
    }
    else if (sync_res == eServerBusy) {
        m_CmdCtx->SetRequestStatus(eStatus_SyncBusy);
        m_SockBuffer.WriteMessage("OK:", "IN_PROGRESS,SIZE=0");
    }
    else {
        x_SetFlag(fNeedSyncFinish);
        string result;
        if (sync_res == eProceedWithEvents) {
            m_SendBuff.reset(new TNCBufferType());
            m_SendBuff->reserve_mem(sync_events.size() * 200);
            ITERATE(TReducedSyncEvents, it_evt, sync_events) {
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
            x_ReadFullBlobsList();
            m_CmdCtx->SetRequestStatus(eStatus_SyncBList);
            result += "ALL_BLOBS,";
        }
        result += "SIZE=";
        result += NStr::UInt8ToString(m_SendBuff->size());
        result.append(1, ' ');
        result += NStr::UInt8ToString(m_LocalRecNo);
        result.append(1, ' ');
        result += NStr::UInt8ToString(m_RemoteRecNo);
        m_SockBuffer.WriteMessage("OK:", result);
        m_SendPos = 0;
        x_SetState(eWriteSendBuff);
    }
    return true;
}

bool
CNCMessageHandler::x_CanStartSyncCommand(bool can_abort /* = true */)
{
    ESyncInitiateResult start_res = CNCPeriodicSync::CanStartSyncCommand(
                                        m_SrvId, m_Slot, can_abort, m_SyncId);
    if (start_res == eNetworkError) {
        m_SockBuffer.WriteMessage("ERR:", "Stale synchronization");
        m_CmdCtx->SetRequestStatus(eStatus_StaleSync);
    }
    else if (start_res == eServerBusy) {
        m_SockBuffer.WriteMessage("OK:", "SIZE=0, NEED_ABORT1");
        m_CmdCtx->SetRequestStatus(eStatus_SyncAborted);
        if (NStr::CompareCase(m_CurCmd, "SYNC_PUT") == 0  &&  m_CmdVersion == 0)
        {
            x_SetFlag(fConfirmBlobPut);
            x_SetFlag(fCopyLogEvent);
            x_SetState(eReadBlobSignature);
        }
    }
    else
        return true;

    return false;
}

bool
CNCMessageHandler::x_DoCmd_SyncBlobsList(void)
{
    if (!x_CanStartSyncCommand())
        return true;

    x_SetFlag(fNeedSyncFinish);
    CNCPeriodicSync::MarkCurSyncByBlobs(m_SrvId, m_Slot, m_SyncId);
    Uint8 rec_no = CNCSyncLog::GetCurrentRecNo(m_Slot);
    x_ReadFullBlobsList();
    string result("SIZE=");
    result += NStr::UInt8ToString(m_SendBuff->size());
    result.append(1, ' ');
    result += NStr::UInt8ToString(rec_no);
    m_SockBuffer.WriteMessage("OK:", result);
    m_SendPos = 0;
    x_SetState(eWriteSendBuff);
    return true;
}

bool
CNCMessageHandler::x_DoCmd_CopyPut(void)
{
    if (!x_CheckAdminClient())
        return true;

    m_CopyBlobInfo.ttl = m_BlobTTL;
    m_CopyBlobInfo.password = m_BlobPass;
    m_CopyBlobInfo.blob_ver = m_BlobVersion;
    bool need_read_blob = m_BlobAccess->ReplaceBlobInfo(m_CopyBlobInfo);
    if (need_read_blob) {
        x_SetFlag(fReadExactBlobSize);
        m_SockBuffer.WriteMessage("OK:", kEmptyStr);
    }
    else {
        m_CmdCtx->SetRequestStatus(eStatus_NewerBlob);
        m_SockBuffer.WriteMessage("OK:", "HAVE_NEWER1");
    }
    if (need_read_blob  ||  m_CmdVersion == 0) {
        x_SetFlag(fConfirmBlobPut);
        x_SetFlag(fCopyLogEvent);
        x_StartReadingBlob();
    }
    return true;
}

bool
CNCMessageHandler::x_DoCmd_SyncPut(void)
{
    if (!x_CanStartSyncCommand())
        return true;

    x_SetFlag(fNeedSyncFinish);
    return x_DoCmd_CopyPut();
}

bool
CNCMessageHandler::x_DoCmd_CopyProlong(void)
{
    if (!m_BlobAccess->IsBlobExists()) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return true;
    }

    if (m_BlobAccess->GetCurBlobCreateTime() == m_CopyBlobInfo.create_time
        &&  m_BlobAccess->GetCurCreateServer() == m_CopyBlobInfo.create_server
        &&  m_BlobAccess->GetCurCreateId() == m_CopyBlobInfo.create_id)
    {
        bool need_event = false;
        if (m_BlobAccess->GetCurBlobExpire() < m_CopyBlobInfo.expire) {
            m_BlobAccess->SetCurBlobExpire(m_CopyBlobInfo.expire,
                                           m_CopyBlobInfo.dead_time);
            need_event = true;
        }
        if (m_BlobAccess->GetCurVerExpire() < m_CopyBlobInfo.ver_expire) {
            m_BlobAccess->SetCurVerExpire(m_CopyBlobInfo.ver_expire);
            need_event = true;
        }

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
        m_CmdCtx->SetRequestStatus(eStatus_NewerBlob);
    }
    m_SockBuffer.WriteMessage("OK:", "SIZE=0");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_SyncProlong(void)
{
    if (!x_CanStartSyncCommand())
        return true;

    x_SetFlag(fNeedSyncFinish);
    return x_DoCmd_CopyProlong();
}

bool
CNCMessageHandler::x_DoCmd_SyncGet(void)
{
    if (!x_CanStartSyncCommand())
        return true;

    x_SetFlag(fNeedSyncFinish);
    if (!m_BlobAccess->IsBlobExists()) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return true;
    }

    bool need_send = true;
    if (m_OrigTime != m_BlobAccess->GetCurBlobCreateTime()) {
        need_send = false;
    }
    else if (m_BlobAccess->GetCurBlobCreateTime() < m_CopyBlobInfo.create_time) {
        need_send = false;
    }
    else if (m_BlobAccess->GetCurBlobCreateTime() == m_CopyBlobInfo.create_time) {
        if (m_BlobAccess->GetCurCreateServer() < m_CopyBlobInfo.create_server) {
            need_send = false;
        }
        else if (m_BlobAccess->GetCurCreateServer() == m_CopyBlobInfo.create_server
                 &&  m_BlobAccess->GetCurCreateId() <= m_CopyBlobInfo.create_id)
        {
            need_send = false;
        }
    }
    if (!need_send) {
        m_CmdCtx->SetRequestStatus(eStatus_NewerBlob);
        m_SockBuffer.WriteMessage("OK:", "SIZE=0, HAVE_NEWER");
        return true;
    }

    string result("SIZE=");
    result += NStr::UInt8ToString(m_BlobAccess->GetCurBlobSize());
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurBlobVersion());
    result += " \"";
    result += m_BlobAccess->GetCurPassword();
    result += "\" ";
    result += NStr::UInt8ToString(m_BlobAccess->GetCurBlobCreateTime());
    result.append(1, ' ');
    result += NStr::UIntToString(Uint4(m_BlobAccess->GetCurBlobTTL()));
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurBlobDeadTime());
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurBlobExpire());
    result.append(1, ' ');
    result += NStr::UIntToString(Uint4(m_BlobAccess->GetCurVersionTTL()));
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurVerExpire());
    result.append(1, ' ');
    result += NStr::UInt8ToString(m_BlobAccess->GetCurCreateServer());
    result.append(1, ' ');
    result += NStr::UInt8ToString(m_BlobAccess->GetCurCreateId());

    m_SockBuffer.WriteMessage("OK:", result);
    if (m_BlobAccess->GetCurBlobSize() != 0) {
        m_BlobAccess->SetPosition(0);
        x_SetState(eWaitForFirstData);
    }
    return true;
}

bool
CNCMessageHandler::x_DoCmd_SyncProlongInfo(void)
{
    if (!x_CanStartSyncCommand())
        return true;

    x_SetFlag(fNeedSyncFinish);
    if (!m_BlobAccess->IsBlobExists()) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return true;
    }

    string result("SIZE=0 ");
    result += NStr::UInt8ToString(m_BlobAccess->GetCurBlobCreateTime());
    result.append(1, ' ');
    result += NStr::UInt8ToString(m_BlobAccess->GetCurCreateServer());
    result.append(1, ' ');
    result += NStr::UInt8ToString(m_BlobAccess->GetCurCreateId());
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurBlobDeadTime());
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurBlobExpire());
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurVerExpire());

    m_SockBuffer.WriteMessage("OK:", result);
    return true;
}

bool
CNCMessageHandler::x_DoCmd_SyncCommit(void)
{
    if (!x_CanStartSyncCommand(false))
        return true;

    CNCPeriodicSync::Commit(m_SrvId, m_Slot, m_SyncId, m_LocalRecNo, m_RemoteRecNo);
    m_SockBuffer.WriteMessage("OK:", "SIZE=0");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_SyncCancel(void)
{
    if (!x_CanStartSyncCommand(false))
        return true;

    CNCPeriodicSync::Cancel(m_SrvId, m_Slot, m_SyncId);
    m_SockBuffer.WriteMessage("OK:", "SIZE=0");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetMeta(void)
{
    if (m_LatestSrvId != CNCDistributionConf::GetSelfID()) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("proxy", "1");
        extra.Flush();

        m_Quorum = 1;
        m_ForceLocal = true;
        m_CheckSrvs.push_back(m_LatestSrvId);
        x_SetState(eProxyToNextPeer);
        return true;
    }
    if (!m_LatestExist) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return true;
    }

    m_SendBuff.reset(new TNCBufferType());
    m_SendBuff->reserve_mem(1024);
    string tmp;


    tmp = "OK:Slot: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::UIntToString(m_BlobSlot);
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Write time: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    Uint8 create_time = m_BlobAccess->GetCurBlobCreateTime();
    CTime tmp_time(CTime::eEmpty, CTime::eLocal);
    tmp_time.SetTimeT(time_t(create_time / kNCTimeTicksInSec));
    tmp_time.SetMicroSecond(create_time % kNCTimeTicksInSec);
    tmp = tmp_time.AsString("M/D/Y h:m:s.r");
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Control server: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    Uint8 create_server = m_BlobAccess->GetCurCreateServer();
    tmp = CSocketAPI::gethostbyaddr(Uint4(create_server >> 32));
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
    tmp = NStr::IntToString(m_BlobAccess->GetCurBlobTTL());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Expire: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp_time.SetTimeT(m_BlobAccess->GetCurBlobExpire());
    tmp = tmp_time.AsString();
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Size: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::UInt8ToString(m_BlobAccess->GetCurBlobSize());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Password: '";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = m_BlobAccess->GetCurPassword();
    m_SendBuff->append(tmp.data(), tmp.size());
    m_SendBuff->append("'", 1);

    tmp = "\nOK:Version: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::IntToString(m_BlobAccess->GetCurBlobVersion());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Version's TTL: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp = NStr::IntToString(m_BlobAccess->GetCurVersionTTL());
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:Version expire: ";
    m_SendBuff->append(tmp.data(), tmp.size());
    tmp_time.SetTimeT(m_BlobAccess->GetCurVerExpire());
    tmp = tmp_time.AsString();
    m_SendBuff->append(tmp.data(), tmp.size());

    tmp = "\nOK:END\n";
    m_SendBuff->append(tmp.data(), tmp.size());


    tmp = "SIZE=";
    tmp += NStr::UInt8ToString(m_SendBuff->size());
    m_SockBuffer.WriteMessage("OK:", tmp);
    m_SendPos = 0;
    x_SetState(eWriteSendBuff);
    return true;
}

bool
CNCMessageHandler::x_DoCmd_ProxyMeta(void)
{
    if (!m_BlobAccess->IsBlobExists()  ||  m_BlobAccess->IsCurBlobExpired()) {
        m_CmdCtx->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return true;
    }

    string result("SIZE=0 ");
    result += NStr::UInt8ToString(m_BlobAccess->GetCurBlobCreateTime());
    result.append(1, ' ');
    result += NStr::UInt8ToString(m_BlobAccess->GetCurCreateServer());
    result.append(1, ' ');
    result += NStr::UInt8ToString(m_BlobAccess->GetCurCreateId());
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurBlobDeadTime());
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurBlobExpire());
    result.append(1, ' ');
    result += NStr::IntToString(m_BlobAccess->GetCurVerExpire());

    m_SockBuffer.WriteMessage("OK:", result);

    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetBlobsList(void)
{
    if (!x_CheckAdminClient())
        return true;

    const vector<Uint2>& slots = CNCDistributionConf::GetSelfSlots();
    string slot_str;
    ITERATE(vector<Uint2>, it_slot, slots) {
        slot_str += NStr::UIntToString(*it_slot);
        slot_str += ",";
    }
    slot_str.resize(slot_str.size() - 1);
    m_SockBuffer.WriteMessage("OK:", "Serving slots: " + slot_str);

    ITERATE(vector<Uint2>, it_slot, slots) {
        TNCBlobSumList blobs_lst;
        g_NCStorage->GetFullBlobsList(*it_slot, blobs_lst);
        ITERATE(TNCBlobSumList, it_blob, blobs_lst) {
            const string& raw_key = it_blob->first;
            SNCCacheData* blob_sum = it_blob->second;
            string cache_name, key, subkey;
            g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);
            string msg("key: ");
            msg += key;
            if (!cache_name.empty()) {
                msg += ", subkey: ";
                msg += subkey;
            }
            msg += ", expire: ";
            CTime tmp_time(CTime::eEmpty, CTime::eLocal);
            tmp_time.SetTimeT(time_t(blob_sum->expire));
            msg += tmp_time.AsString("M/D/Y h:m:s");
            msg += ", create_time: ";
            tmp_time.SetTimeT(time_t(blob_sum->create_time / kNCTimeTicksInSec));
            tmp_time.SetMicroSecond(blob_sum->create_time % kNCTimeTicksInSec);
            msg += tmp_time.AsString("M/D/Y h:m:s.r");
            msg += ", create_id: ";
            msg += NStr::Int8ToString(blob_sum->create_id);
            msg += ", create_server: ";
            msg += CSocketAPI::gethostbyaddr(Uint4(blob_sum->create_server >> 32));
            msg += ":";
            msg += NStr::UIntToString(Uint4(blob_sum->create_server));
            m_SockBuffer.WriteMessage("OK:", msg);
            delete blob_sum;
        }
    }
    m_SockBuffer.WriteMessage("OK:", "END");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_NotImplemented(void)
{
    m_CmdCtx->SetRequestStatus(eStatus_NoImpl);
    m_SockBuffer.WriteMessage("ERR:", "Not implemented");
    return true;
}




CNCMsgHandler_Factory::~CNCMsgHandler_Factory(void)
{}

inline CNCMsgHandler_Factory::THandlerPool&
CNCMsgHandler_Factory::GetHandlerPool(void)
{
    return m_Pool;
}


inline
CNCMsgHandler_Proxy::CNCMsgHandler_Proxy(CNCMsgHandler_Factory* factory)
    : m_Factory(factory),
      m_Handler(factory->GetHandlerPool())
{}

CNCMsgHandler_Proxy::~CNCMsgHandler_Proxy(void)
{}

EIO_Event
CNCMsgHandler_Proxy::GetEventsToPollFor(const CTime** alarm_time) const
{
    return m_Handler->GetEventsToPollFor(alarm_time);
}

bool
CNCMsgHandler_Proxy::IsReadyToProcess(void) const
{
    return m_Handler->IsReadyToProcess();
}

void
CNCMsgHandler_Proxy::OnOpen(void)
{
    m_Handler->SetSocket(&GetSocket());
    m_Handler->OnOpen();
}

void
CNCMsgHandler_Proxy::OnRead(void)
{
    m_Handler->OnRead();
}

void
CNCMsgHandler_Proxy::OnWrite(void)
{
    m_Handler->OnWrite();
}

void
CNCMsgHandler_Proxy::OnClose(EClosePeer peer)
{
    m_Handler->OnClose(peer);
}

void
CNCMsgHandler_Proxy::OnTimeout(void)
{
    m_Handler->OnTimeout();
}

void
CNCMsgHandler_Proxy::OnTimer(void)
{
    m_Handler->OnTimer();
}

void
CNCMsgHandler_Proxy::OnOverflow(EOverflowReason)
{
    m_Handler->OnOverflow();
}


inline IServer_ConnectionHandler*
CNCMsgHandler_Factory::Create(void)
{
    return new CNCMsgHandler_Proxy(this);
}


CNCMsgHndlFactory_Proxy::~CNCMsgHndlFactory_Proxy(void)
{}

IServer_ConnectionHandler*
CNCMsgHndlFactory_Proxy::Create(void)
{
    return m_Factory->Create();
}

END_NCBI_SCOPE
