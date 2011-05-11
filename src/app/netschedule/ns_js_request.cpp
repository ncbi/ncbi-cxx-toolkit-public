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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: job request parameters
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/netschedule_api.hpp>


#include "ns_js_request.hpp"

USING_NCBI_SCOPE;


void SJS_Request::Init()
{
    job_id          = 0;
    job_return_code = 0;
    port            = 0;
    timeout         = 0;
    job_mask        = 0;
    count           = 0;

    input.erase();
    output.erase();
    affinity_token.erase();
    job_key.erase();
    err_msg.erase();
    param1.erase();
    param2.erase();
    param3.erase();
    return;
}


void SJS_Request::SetParamFields(TNSProtoParams& params)
{
    NON_CONST_ITERATE(TNSProtoParams, it, params) {
        const CTempString &     key = it->first;
        string                  val = it->second;

        if (key.empty())
            continue;

        if (val.size() > kNetScheduleMaxDBDataSize - 1  &&
            key != "input"   &&
            key != "output"  &&
            key != "tags"    &&  // These 3 cases are not critical but
            key != "where"   &&  // input and output sizes are controlled
            key != "fields")     // in more intelligent manner.
        {
            val.resize(kNetScheduleMaxDBDataSize - 1);
        }

        switch (key[0]) {
        case 'a':
            if (key == "aff")
                affinity_token = val;
            else if (key == "affp")
                param1 = val;
            else if (key == "action")
                param2 = val;
            break;
        case 'c':
            if (key == "count")
                count = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            else if (key == "comment")
                param3 = val;
            break;
        case 'e':
            if (key == "err_msg")
                err_msg = val;
            break;
        case 'f':
            if (key == "fields")
                param3 = val;
            break;
        case 'g':
            if (key == "guid")
                param1 = val;
            break;
        case 'i':
            if (key == "input")
                input = val;
            else if (key == "ip")
                param3 = val;
            else if (key == "info")
                param1 = val;
            break;
        case 'j':
            if (key == "job_key") {
                job_key = val;
                if (!val.empty())
                    job_id = CNetScheduleKey(val).id;
            }
            else if (key == "job_return_code")
                job_return_code = NStr::StringToInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'm':
            if (key == "msk")
                job_mask = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'o':
            if (key == "output")
                output = val;
            else if (key == "option")
                param1 = val;
            break;
        case 'p':
            if (key == "port")
                port = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            else if (key == "progress_msg")
                param1 = val;
            break;
        case 'q':
            if (key == "qname")
                param1 = val;
            else if (key == "qclass")
                param2 = val;
            break;
        case 's':
            if (key == "status")
                param1 = val;
            else if (key == "select")
                param1 = val;
            else if (key == "sid")
                param2 = val;
            break;
        case 't':
            if (key == "timeout")
                timeout = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            else if (key == "tags")
                tags = val;
            break;
        case 'w':
            if (key == "where")
                param1 = val;
            break;
        default:
            break;
        }
    }
}

