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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/remote_app.hpp>

BEGIN_NCBI_SCOPE

IRemoteAppRequest::~IRemoteAppRequest()
{
}

IRemoteAppRequest_Executer::~IRemoteAppRequest_Executer()
{
}

IRemoteAppResult::~IRemoteAppResult()
{
}

IRemoteAppResult_Executer::~IRemoteAppResult_Executer()
{
}

void TokenizeCmdLine(const string& cmdline, vector<string>& args)
{
    if (!cmdline.empty()) {
        string arg;
        for(size_t i = 0; i < cmdline.size(); ) {
            if (cmdline[i] == ' ') {
                if( !arg.empty() ) {
                    args.push_back(arg);
                    arg.erase();
                    }
                i++;
                continue;
            }
            if (cmdline[i] == '\'' || cmdline[i] == '"') {
                /*                    if( !arg.empty() ) {
                                      args.push_back(arg);
                                      arg.erase();
                                      }*/
                char quote = cmdline[i];
                while( ++i < cmdline.size() && cmdline[i] != quote )
                    arg += cmdline[i];
                args.push_back(arg);
                arg.erase();
                ++i;
                continue;
            }
            arg += cmdline[i++];
        }
        if( !arg.empty() )
            args.push_back(arg);
    }
}


string JoinCmdLine(const vector<string>& args)
{
    string cmd_line = "";
    for(vector<string>::const_iterator it = args.begin();
        it != args.end(); ++it) {
        if (it != args.begin())
            cmd_line += ' ';
        if (it->find(" ") != string::npos)
            cmd_line += '\"' + *it + '\"';
        else
            cmd_line += *it;
    }
    return cmd_line;
}

END_NCBI_SCOPE
