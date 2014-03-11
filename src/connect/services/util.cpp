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
 *   Contains implementations of utility classes used by Grid applications.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <ncbi_pch.hpp>

#include "util.hpp"

#include <connect/services/util.hpp>
#include <connect/services/netservice_api_expt.hpp>

#include <connect/ncbi_socket.hpp>

#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_config.hpp>

BEGIN_NCBI_SCOPE

struct SCmdLineArgListImpl : public CObject
{
    SCmdLineArgListImpl(FILE* file, const string& file_name);
    SCmdLineArgListImpl(const string& file_name, bool for_output);
    SCmdLineArgListImpl(const string& args);

    virtual ~SCmdLineArgListImpl();

    FILE* m_File;
    string m_FileName;
    list<string> m_Args;
};

inline SCmdLineArgListImpl::SCmdLineArgListImpl(
        FILE* file, const string& file_name) :
    m_File(file), m_FileName(file_name)
{
}

inline SCmdLineArgListImpl::SCmdLineArgListImpl(
        const string& file_name, bool for_output) :
    m_FileName(file_name)
{
    if ((m_File = fopen(file_name.c_str(), for_output ? "wt" : "rt")) == NULL) {
        NCBI_THROW(CFileErrnoException, eFileIO,
            "Cannot open '" + file_name +
                (for_output ? "' for output" : "' for input"));
    }
}

inline SCmdLineArgListImpl::SCmdLineArgListImpl(const string& args) :
    m_File(NULL)
{
    NStr::Split(args, CCmdLineArgList::GetDelimiterString(), m_Args);
}

SCmdLineArgListImpl::~SCmdLineArgListImpl()
{
    if (m_File != NULL)
        fclose(m_File);
}

CCmdLineArgList CCmdLineArgList::OpenForOutput(
    const string& file_or_stdout)
{
    if (file_or_stdout == "-")
        return new SCmdLineArgListImpl(stdout, "stdout");
    else
        return new SCmdLineArgListImpl(file_or_stdout, true);
}

void CCmdLineArgList::WriteLine(const string& line)
{
    _ASSERT(m_Impl->m_File != NULL);

    if (fprintf(m_Impl->m_File, "%s\n", line.c_str()) < 0) {
        NCBI_THROW(CFileErrnoException, eFileIO,
            "Cannot write to '" + m_Impl->m_FileName + '\'');
    }
}

CCmdLineArgList CCmdLineArgList::CreateFrom(const string& file_or_list)
{
    if (file_or_list[0] == '@')
        return new SCmdLineArgListImpl(
            string(file_or_list.begin() + 1,
                file_or_list.end()), false);
    else
        return new SCmdLineArgListImpl(file_or_list);
}

bool CCmdLineArgList::GetNextArg(string& arg)
{
    if (m_Impl->m_File != NULL) {
        char buffer[256];
        size_t buffer_len;

        do {
            if (fgets(buffer, sizeof(buffer), m_Impl->m_File) == NULL)
                return false;

            buffer_len = strlen(buffer);
            if (buffer_len == 0)
                return false;

            if (buffer[buffer_len - 1] == '\n')
                --buffer_len;
        } while (buffer_len == 0);

        arg.assign(buffer, buffer + buffer_len);
    } else {
        if (m_Impl->m_Args.empty())
            return false;

        arg = m_Impl->m_Args.front();
        m_Impl->m_Args.pop_front();
    }

    return true;
}

const string CCmdLineArgList::GetDelimiterString()
{
    return " \t\n\r,;";
}


string g_NetService_gethostnamebyaddr(unsigned ip)
{
    string hostname = CSocketAPI::gethostbyaddr(ip, eOn);
    return !hostname.empty() ? hostname : CSocketAPI::ntoa(ip);
}

unsigned g_NetService_gethostbyname(const string& hostname)
{
    unsigned ip = CSocketAPI::gethostbyname(hostname, eOn);
    if (ip == 0) {
        NCBI_THROW_FMT(CNetServiceException, eCommunicationError,
            "gethostbyname('" << hostname << "') failed");
    }
    return ip;
}

string g_NetService_gethostname(const string& ip_or_hostname)
{
    string hostname(CSocketAPI::gethostbyaddr(
        g_NetService_gethostbyname(ip_or_hostname), eOn));
    if (hostname.empty()) {
        NCBI_THROW_FMT(CNetServiceException, eCommunicationError,
            "g_NetService_gethostname('" << ip_or_hostname << "') failed");
    }
    return hostname;
}

string g_NetService_gethostip(const string& ip_or_hostname)
{
    return CSocketAPI::ntoa(g_NetService_gethostbyname(ip_or_hostname));
}

string g_NetService_TryResolveHost(const string& ip_or_hostname)
{
    unsigned ip = CSocketAPI::gethostbyname(ip_or_hostname, eOn);
    if (ip == 0)
        return ip_or_hostname;

    string hostname(CSocketAPI::gethostbyaddr(ip, eOn));
    if (hostname.empty())
        return ip_or_hostname;

    return hostname;
}

#define NOT_ALPHA(c) ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z'))
#define NOT_DIGIT(c) (c < '0' || c > '9')

bool g_CheckAlphabet(const string& str, const CTempString& param_name,
        ECharacterClass char_class, char* bad_char)
{
    const char* ch = str.data();
    size_t len = str.length();

    switch (char_class) {
    case eCC_Alphabetic:
        for (; len > 0; ++ch, --len)
            if (NOT_ALPHA(*ch))
                break;
        break;

    case eCC_Alphanumeric:
        for (; len > 0; ++ch, --len)
            if (NOT_ALPHA(*ch) && NOT_DIGIT(*ch))
                break;
        break;

    case eCC_StrictId:
        for (; len > 0; ++ch, --len)
            if (NOT_ALPHA(*ch) && NOT_DIGIT(*ch) && *ch != '_')
                break;
        break;

    case eCC_BASE64URL:
        for (; len > 0; ++ch, --len)
            if (NOT_ALPHA(*ch) && NOT_DIGIT(*ch) && *ch != '_' && *ch != '-')
                break;
        break;

    case eCC_BASE64_PI:
        for (; len > 0; ++ch, --len)
            if (NOT_ALPHA(*ch) && NOT_DIGIT(*ch) && *ch != '_' && *ch != '.')
                break;
        break;

    case eCC_RelaxedId:
        for (; len > 0; ++ch, --len)
            if (NOT_ALPHA(*ch) && NOT_DIGIT(*ch) && *ch != '_' &&
                    *ch != '-' && *ch != '.' && *ch != ':' &&
                    *ch != '@' && *ch != '|')
                break;
        break;
    }

    if (len != 0) {
        *bad_char = *ch;
        return false;
    }

    return true;
}

void g_VerifyAlphabet(const string& str, const CTempString& param_name,
        ECharacterClass char_class)
{
    char bad_char;

    if (!g_CheckAlphabet(str, param_name, char_class, &bad_char)) {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
                "Invalid character '" <<
                NStr::PrintableString(CTempString(&bad_char, 1)) <<
                "' in the " << param_name << " \"" <<
                NStr::PrintableString(str) << "\".");
    }
}

unsigned g_NumberOfUnderscoresPlusOne(const string& str)
{
    unsigned underscore_count = 1;
    const char* underscore = strchr(str.c_str(), '_');
    while (underscore != NULL) {
        ++underscore_count;
        underscore = strchr(underscore + 1, '_');
    }
    return underscore_count;
}

END_NCBI_SCOPE
