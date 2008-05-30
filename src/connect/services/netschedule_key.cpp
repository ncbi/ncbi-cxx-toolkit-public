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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko, Victor Joukov
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>

#include <connect/services/netschedule_key.hpp>
#include <connect/services/netschedule_api_expt.hpp>


BEGIN_NCBI_SCOPE

static const string kNetScheduleKeyPrefix = "JSID";
static const string kNetScheduleKeySchema = "nsid";

CNetScheduleKey::CNetScheduleKey(const string& key_str)
{

    // Parses several notations for job id:
    // version 1:
    //   JSID_01_1_MYHOST_9000
    // version 2:
    //   nsid|host/port/[queue]/num[/run] e.g.,
    //     nsid|192.168.1.1/9101/splign/1
    //     nsid|192.168.1.1/9101/splign/1/5
    //     nsid|192.168.1.1/9101//1/5
    // "version 0", or just job number or job number with run number:
    //   num[/run]

    const char* ch = key_str.c_str();
    if (key_str.compare(0, 8, "JSID_01_") == 0) {
        // Old (version 1) key
        version = 1;
        ch += 8;

        // id
        id = (unsigned) atoi(ch);
        while (*ch && *ch != '_') {
            ++ch;
        }
        if (*ch == 0 || id == 0) {
            NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
        }
        ++ch;

        // hostname
        for (;*ch && *ch != '_'; ++ch) {
            host += *ch;
        }
        if (*ch == 0) {
            NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
        }
        ++ch;

        // port
        port = atoi(ch);
        return;
    } else if (key_str.compare(0, 5, "nsid|") == 0) {
        version = 2;
        ch += 5;
        do {
            // hostname
            for (;*ch && *ch != '/'; ++ch)
                host += *ch;
            if (*ch == 0) break;
            ++ch;
            // port
            port = atoi(ch);
            while (*ch && *ch != '/') ++ch;
            if (*ch == 0) break;
            ++ch;
            // queue
            for (;*ch && *ch != '/'; ++ch)
                queue += *ch;
            if (*ch == 0) break;
            ++ch;
            // id
            id = (unsigned) atoi(ch);
            while (*ch && *ch != '/') ++ch;
            if (*ch) {
                // run
                run = atoi(ch+1);
            } else {
                run = -1;
            }
            return;
        } while (0);
    } else if (isdigit(*ch)) {
        version = 0;
        id = (unsigned) atoi(ch);
        while (*ch && *ch != '/') ++ch;
        if (*ch) {
            ch += 1;
            if (!isdigit(*ch))
                NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
            // run
            run = atoi(ch);
        } else {
            run = -1;
        }
        return;
    }
    NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
}

CNetScheduleKey::operator string() const
{
    string key;
    if (version == 1) {
        key = "JSID_01_";
        key += NStr::IntToString(id);
        key += '_';
        key += host;
        key += '_';
        key += NStr::IntToString(port);
    } else if (version == 2) {
        key = "nsid|";
        key += host + '/';
        key += NStr::IntToString(port);
        key += '/';
        key += queue + '/';
        key += NStr::IntToString(id);
        if (run >= 0) {
            key += '/';
            key += NStr::IntToString(run);
        }
    }
    return key;
}


END_NCBI_SCOPE
