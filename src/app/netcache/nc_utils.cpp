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

#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE;


map<int, string> s_MsgForStatus;
map<string, int> s_StatusForMsg;


struct SStatusMsg
{
    int status;
    const char* msg;
};


static SStatusMsg s_StatusMessages[] =
    {
        {eStatus_JustStarted, "ERR:Caching is not completed"},
        {eStatus_Disabled, "ERR:Cache is disabled"},
        {eStatus_NotAllowed, "ERR:Password in the command doesn't match server settings"},
        {eStatus_NoDiskSpace, "ERR:Not enough disk space"},
        {eStatus_BadPassword, "ERR:Access denied."},
        {eStatus_NotFound, "ERR:BLOB not found."},
        {eStatus_CondFailed, "ERR:Too few data for blob"},
        {eStatus_NoImpl, "ERR:Not implemented"}
    };



void
InitClientMessages(void)
{
    Uint1 num_elems = Uint1(sizeof(s_StatusMessages) / sizeof(s_StatusMessages[0]));
    for (Uint1 i = 0; i < num_elems; ++i) {
        SStatusMsg& stat_msg = s_StatusMessages[i];
        s_MsgForStatus[stat_msg.status] = stat_msg.msg;
        s_StatusForMsg[stat_msg.msg] = stat_msg.status;
    }
}

END_NCBI_SCOPE;
