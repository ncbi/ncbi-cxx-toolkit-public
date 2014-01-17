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
* Author: Aleksey Grichenko
*
* File Description:
*   EUtils base classes
*
*/

#include <ncbi_pch.hpp>
#include <objtools/eutils/api/eutils.hpp>
#include <cgi/cgi_util.hpp>
#include <corelib/stream_utils.hpp>
#include <corelib/ncbi_param.hpp>
#include <serial/objistr.hpp>


BEGIN_NCBI_SCOPE


CEUtils_Request::CEUtils_Request(CRef<CEUtils_ConnContext>& ctx,
                                 const string& script_name)
    : m_Context(ctx),
      m_Stream(0),
      m_ScriptName(script_name),
      m_Method(eHttp_Post)
{
}


CRef<CEUtils_ConnContext>& CEUtils_Request::GetConnContext(void) const
{
    if ( !m_Context ) {
        m_Context.Reset(new CEUtils_ConnContext);
    }
    return m_Context;
}


void CEUtils_Request::SetConnContext(const CRef<CEUtils_ConnContext>& ctx)
{
    Disconnect();
    m_Context = ctx;
}


static const string kDefaultEUtils_Base_URL =
    "http://eutils.ncbi.nlm.nih.gov/entrez/eutils/";


NCBI_PARAM_DECL(string, EUtils, Base_URL);
NCBI_PARAM_DEF_EX(string, EUtils, Base_URL,
                  kDefaultEUtils_Base_URL,
                  eParam_NoThread,
                  EUTILS_BASE_URL);
typedef NCBI_PARAM_TYPE(EUtils, Base_URL) TEUtilsBaseURLParam;


string CEUtils_Request::GetBaseURL(void)
{
    return TEUtilsBaseURLParam::GetDefault();
}


void CEUtils_Request::SetBaseURL(const string& url)
{
    TEUtilsBaseURLParam::SetDefault(url);
}


void CEUtils_Request::Connect(void)
{
    string url = GetBaseURL() + GetScriptName();
    string body = GetQueryString();
    STimeout timeout_value;
    const STimeout* timeout = 
        g_CTimeoutToSTimeout(GetConnContext()->GetTimeout(), timeout_value);

    if ( m_Method == eHttp_Post ) {
        m_Stream.reset(new CConn_HttpStream(
            url,
            NULL,
            "Content-Type: application/x-www-form-urlencoded",
            NULL, NULL, NULL, NULL,
            fHTTP_AutoReconnect,
            timeout));
        *m_Stream << body;
    }
    else {
        m_Stream.reset(new CConn_HttpStream(
            url + "?" + body,
            fHTTP_AutoReconnect,
            timeout));
    }
}


void CEUtils_Request::SetArgument(const string& name, const string& value)
{
    if ( value.empty() ) {
        TRequestArgs::iterator it = m_Args.find(name);
        if (it != m_Args.end()) {
            m_Args.erase(it);
            return;
        }
    }
    m_Args[name] = value;
}


const string& CEUtils_Request::GetArgument(const string& name) const
{
    TRequestArgs::const_iterator it = m_Args.find(name);
    return (it != m_Args.end()) ? it->second : kEmptyStr;
}


string CEUtils_Request::GetQueryString(void) const
{
    string args;
    if ( !m_Database.empty() ) {
        args = "db=" + m_Database;
    }
    const string& webenv = GetConnContext()->GetWebEnv();
    if ( !webenv.empty() ) {
        if ( !args.empty() ) {
            args += '&';
        }
        args += "WebEnv=" +
            NStr::URLEncode(webenv, NStr::eUrlEnc_ProcessMarkChars);
    }
    string qk = GetQueryKey();
    if ( !qk.empty() ) {
        if ( !args.empty() ) {
            args += '&';
        }
        args += "query_key=" + qk;
    }
    const string& tool = GetConnContext()->GetTool();
    if ( !tool.empty() ) {
        if ( !args.empty() ) {
            args += '&';
        }
        args += "tool=" +
            NStr::URLEncode(tool, NStr::eUrlEnc_ProcessMarkChars);
    }
    const string& email = GetConnContext()->GetEmail();
    if ( !email.empty() ) {
        if ( !args.empty() ) {
            args += '&';
        }
        args += "email=" +
            NStr::URLEncode(email, NStr::eUrlEnc_ProcessMarkChars);
    }
    ITERATE(TRequestArgs, it, m_Args) {
        if ( !args.empty() ) {
            args += '&';
        }
        args += it->first + '=' +
            NStr::URLEncode(it->second, NStr::eUrlEnc_ProcessMarkChars);
    }
    return args;
}


CNcbiIostream& CEUtils_Request::GetStream(void)
{
    if ( !m_Stream.get() ) {
        Connect();
    }
    _ASSERT(m_Stream.get());
    return *m_Stream;
}


CObjectIStream* CEUtils_Request::GetObjectIStream(void)
{
    ESerialDataFormat fmt = GetSerialDataFormat();
    return fmt == eSerial_None ?
        NULL : CObjectIStream::Open(fmt, GetStream());
}


void CEUtils_Request::Read(string* content)
{
    NcbiStreamToString(content, GetStream());
    Disconnect();
}


void CEUtils_Request::SetDatabase(const string& database)
{
    Disconnect();
    m_Database = database;
}


const string& CEUtils_Request::GetQueryKey(void) const
{
    return m_QueryKey.empty() ? GetConnContext()->GetQueryKey() : m_QueryKey;
}


void CEUtils_Request::SetQueryKey(const string& key)
{
    Disconnect();
    m_QueryKey = key;
}


void CEUtils_Request::ResetQueryKey(void)
{
    Disconnect();
    m_QueryKey.erase();
}


string CEUtils_IdGroup::AsQueryString(void) const
{
    string ret;
    ITERATE(TIdList, it, m_Ids) {
        ret += ret.empty() ?
            "id=" : NStr::URLEncode(",", NStr::eUrlEnc_ProcessMarkChars);
        ret += NStr::URLEncode(*it, NStr::eUrlEnc_ProcessMarkChars);
    }
    return ret;
}


void CEUtils_IdGroup::SetIds(const string& ids)
{
    list<string> tmp;
    NStr::Split(ids, ",", tmp);
    ITERATE(list<string>, it, tmp) {
        AddId(*it);
    }
}


void CEUtils_IdGroupSet::SetGroups(const string& groups)
{
    list<string> tmp;
    NStr::Split(groups, "&", tmp);
    ITERATE(list<string>, it, tmp) {
        string ids = *it;
        if (ids.find("id=") == 0) {
            ids = ids.substr(3);
        }
        CEUtils_IdGroup grp;
        grp.SetIds(ids);
        m_Groups.push_back(grp);
    }
}


string CEUtils_IdGroupSet::AsQueryString(void) const
{
    string ret;
    ITERATE(TIdGroupSet, it, m_Groups) {
        if ( !ret.empty() ) {
            ret += '&';
        }
        ret += it->AsQueryString();
    }
    return ret;
}


END_NCBI_SCOPE
