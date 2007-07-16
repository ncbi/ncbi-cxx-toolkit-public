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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Network cache daemon request parser
 *
 */

#include <ncbi_pch.hpp>

#include "netcached.hpp"

BEGIN_NCBI_SCOPE

#define NC_SKIPNUM(x)  \
    while (*x && (*x >= '0' && *x <= '9')) { ++x; }
#define NC_SKIPSPACE(x)  \
    while (*x && (*x == ' ' || *x == '\t')) { ++x; }
#define NC_RETURN_ERROR(err) \
    { req->req_type = eIC_Error; req->err_msg = err; return; }
#define NC_CHECK_END(s) \
    if (*s == 0) { \
        req->req_type = eIC_Error; req->err_msg = "Protocol error"; return; }
#define NC_GETSTRING(x, str) \
    for (;*x && !(*x == ' ' || *x == '\t'); ++x) { str.push_back(*x); }
#define NC_GETVAR(x, str) \
    if (*x != '"') NC_RETURN_ERROR("Invalid IC request"); \
    do { ++x;\
        if (*x == 0) NC_RETURN_ERROR("Invalid IC request"); \
        if (*x == '"') { ++x; break; } \
        str.push_back(*x); \
    } while (1);



inline static
bool CmpCmd4(const char* req, const char* cmd4)
{
    return req[0] == cmd4[0] && 
           req[1] == cmd4[1] &&
           req[2] == cmd4[2] &&
           req[3] == cmd4[3];
}




void CNetCacheServer::ParseRequestIC(const string& reqstr, SIC_Request* req)
{
    // request format:
    // IC(cache_name) COMMAND_CLAUSE
    //
    // COMMAND_CLAUSE:
    //   SVRP KA | DO | DA
    //   GVRP
    //   STSP policy_mask timeout max_timeout
    //   GTSP
    //   GTOU
    //   ISOP
    //   STOR ttl "key" version "subkey"
    //   STRS ttl blob_size "key" version "subkey"
    //   GSIZ "key" version "subkey"
    //   GBLW "key" version "subkey"
    //   READ "key" version "subkey"
    //   REMO "key" version "subkey"
    //   GACT "key" version "subkey"
    //   HASB "key" 0 "subkey"
    //   PRG1 "key" keep_version


    const char* s = reqstr.c_str();

    if (s[0] != 'I' || s[1] != 'C' || s[2] != '(') {
        NC_RETURN_ERROR("Unknown request");
    }

    s+=3;

    for (; *s != ')'; ++s) {
        if (*s == 0) {
            NC_RETURN_ERROR("Invalid IC request")
        }
        req->cache_name.append(1, *s);
    }

    ++s;

    NC_SKIPSPACE(s)

    // Parsing commands


    if (CmpCmd4(s, "STOR")) { // Store
        req->req_type = eIC_Store;
        s += 4;
        NC_SKIPSPACE(s)
        req->i0 = (unsigned) atoi(s);  // ttl
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
    get_kvs:
        NC_GETVAR(s, req->key)
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->version = (unsigned) atoi(s);
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        NC_GETVAR(s, req->subkey)
        return;
    } // STOR

    if (CmpCmd4(s, "STRS")) { // Store size
        req->req_type = eIC_StoreBlob;
        s += 4;
        NC_SKIPSPACE(s)
        req->i0 = (unsigned) atoi(s);  // ttl
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        req->i1 = (unsigned) atoi(s);  // blob size
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "GSIZ")) { // GetSize
        req->req_type = eIC_GetSize;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "GBLW")) { // GetBlobOwner
        req->req_type = eIC_GetBlobOwner;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "READ")) { // Read
        req->req_type = eIC_Read;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "REMO")) { // Remove
        req->req_type = eIC_Remove;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "REMK")) { // RemoveKey
        req->req_type = eIC_Remove;
        s += 4;
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        NC_GETVAR(s, req->key);
        return;
    }

    if (CmpCmd4(s, "GACT")) { // GetAccessTime
        req->req_type = eIC_GetAccessTime;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }

    if (CmpCmd4(s, "HASB")) { // HasBlobs
        req->req_type = eIC_HasBlobs;
        s += 4;
        NC_SKIPSPACE(s)
        goto get_kvs;
    }


    if (CmpCmd4(s, "STSP")) { // SetTimeStampPolicy
        req->req_type = eIC_SetTimeStampPolicy;
        s += 4;
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i0 = (unsigned)atoi(s);
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i1 = (unsigned)atoi(s);
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i2 = (unsigned)atoi(s);
        return;
    }

    if (CmpCmd4(s, "GTSP")) { // GetTimeStampPolicy
        req->req_type = eIC_GetTimeStampPolicy;
        return;
    }

    if (CmpCmd4(s, "GTOU")) { // GetTimeout
        req->req_type = eIC_GetTimeout;
        return;
    }

    if (CmpCmd4(s, "ISOP")) { // IsOpen
        req->req_type = eIC_IsOpen;
        return;
    }

    if (CmpCmd4(s, "SVRP")) { // SetVersionRetention
        req->req_type = eIC_SetVersionRetention;
        s += 4;
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        NC_GETSTRING(s, req->key)
        return;
    }
    if (CmpCmd4(s, "GVRP")) { // GetVersionRetention
        req->req_type = eIC_GetVersionRetention;
        return;
    }

    if (CmpCmd4(s, "PRG1")) { // Purge1
        req->req_type = eIC_Purge1;
        s += 4;
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i0 = (unsigned)atoi(s);
        NC_SKIPNUM(s)
        NC_SKIPSPACE(s)
        NC_CHECK_END(s)
        req->i1 = (unsigned)atoi(s);
        return;
    }


}


void CNetCacheServer::ParseRequestNC(const string& reqstr, SNC_Request* req)
{
    const char* s = reqstr.c_str();


    switch (s[0]) {

    case 'P':

        if (strncmp(s, "PUT2", 4) == 0) {
            req->req_type = ePut2;
            req->timeout = 0;
            req->req_id.erase();

            s += 4;
            goto put_args_parse;

        } // PUT2

        if (strncmp(s, "PUT", 3) == 0) {
            req->req_type = ePut;
            req->timeout = 0;
            req->req_id.erase();

            s += 3;
    put_args_parse:
            while (*s && isspace((unsigned char)(*s))) {
                ++s;
            }

            if (*s) {  // timeout value
                int time_out = atoi(s);
                if (time_out > 0) {
                    req->timeout = time_out;
                }
            }
            while (*s && isdigit((unsigned char)(*s))) {
                ++s;
            }
            while (*s && isspace((unsigned char)(*s))) {
                ++s;
            }
            req->req_id = s;

            return;
        } // PUT


        break;


    case 'G':

        if (strncmp(s, "GET2", 4) == 0) {
            req->req_type = eGet;
            s += 4;
            goto parse_get;
        }


        if (strncmp(s, "GET", 3) == 0) {
            req->req_type = eGet;
            s += 3;

        parse_get:

            if (isspace((unsigned char)(*s))) { // "GET"
                while (*s && isspace((unsigned char)(*s))) {
                    ++s;
                }

                req->req_id.erase();

                while (*s && !isspace((unsigned char)(*s))) {
                    char ch = *s;
                    req->req_id.append(1, ch);
                    ++s;
                }

                if (!*s) {
                    return;
                }

                // skip whitespace
                while (*s && isspace((unsigned char)(*s))) {
                    ++s;
                }

                if (!*s) {
                    return;
                }

                // NW modificator (no wait request)
                if (s[0] == 'N' && s[1] == 'W') {
                    req->no_lock = true;
                }

                return;

            } else { // GET...
                if (strncmp(s, "CONF", 4) == 0) {
                    req->req_type = eGetConfig;
                    s += 4;
                    return;
                } // GETCONF

                if (strncmp(s, "STAT", 4) == 0) {
                    req->req_type = eGetStat;
                    s += 4;
                    return;
                } // GETSTAT
            }
        } // GET*


        if (strncmp(s, "GBOW", 4) == 0) {  // get blob owner
            req->req_type = eGetBlobOwner;
            s += 4;
            goto parse_blob_id;
        } // GBOW

        break;

    case 'I':

        if (strncmp(s, "ISLK", 4) == 0) {
            req->req_type = eIsLock;
            s += 4;

    parse_blob_id:
            req->req_id.erase();
            while (*s && isspace((unsigned char)(*s))) {
                ++s;
            }

            req->req_id = s;
            return;
        }

        break;

    case 'R':

        if (strncmp(s, "RMV2", 4) == 0) {
            req->req_type = eRemove2;
            s += 6;
            goto parse_blob_id;
        } // REMOVE

        if (strncmp(s, "REMOVE", 3) == 0) {
            req->req_type = eRemove;
            s += 6;
            goto parse_blob_id;
        } // REMOVE

        break;

    case 'D':

        if (strncmp(s, "DROPSTAT", 8) == 0) {
            req->req_type = eDropStat;
            s += 8;
            return;
        } // DROPSTAT

        break;

    case 'S':

        if (strncmp(s, "SHUTDOWN", 7) == 0) {
            req->req_type = eShutdown;
            return;
        } // SHUTDOWN

        break;

    case 'V':

        if (strncmp(s, "VERSION", 7) == 0) {
            req->req_type = eVersion;
            return;
        } // VERSION

        break;

    case 'L':

        if (strncmp(s, "LOG", 3) == 0) {
            req->req_type = eLogging;
            s += 3;
            goto parse_blob_id;  // "ON/OFF" instead of blob_id in this case
        } // LOG

        break;

    default:
        break;
    
    } // switch


    req->req_type = eError;
    req->err_msg = "Unknown request";
}


END_NCBI_SCOPE
