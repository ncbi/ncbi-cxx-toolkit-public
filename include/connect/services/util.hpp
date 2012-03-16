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
 * File Description:
 *   Contains declarations of utility classes used by Grid applications.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */


#ifndef CONNECT_SERVICES___UTIL__HPP
#define CONNECT_SERVICES___UTIL__HPP

#include "netcomponent.hpp"

BEGIN_NCBI_SCOPE

struct SCmdLineArgListImpl;

class NCBI_XCONNECT_EXPORT CCmdLineArgList
{
    NCBI_NET_COMPONENT(CmdLineArgList);

    static CCmdLineArgList OpenForOutput(const string& file_or_stdout);

    void WriteLine(const string& line);

    static CCmdLineArgList CreateFrom(const string& file_or_list);

    bool GetNextArg(string& arg);

    static const string& GetDelimiterString();
};


extern NCBI_XCONNECT_EXPORT
unsigned g_NetService_gethostbyname(const string& hostname);


extern NCBI_XCONNECT_EXPORT
string g_NetService_gethostname(const string& ip_or_hostname);


extern NCBI_XCONNECT_EXPORT
string g_NetService_gethostip(const string& ip_or_hostname);


extern NCBI_XCONNECT_EXPORT
string g_NetService_TryResolveHost(const string& ip_or_hostname);


enum ECharacterClass {
    eCC_Alphabetic,
    eCC_Alphanumeric,
    eCC_StrictId,
    eCC_BASE64URL,
    eCC_BASE64_PI,
    eCC_RelaxedId
};

extern NCBI_XCONNECT_EXPORT
void g_VerifyAlphabet(const string& str, const CTempString& param_name,
        ECharacterClass char_class);


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___UTIL__HPP */
