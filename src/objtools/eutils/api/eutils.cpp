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
#include <serial/objistr.hpp>


BEGIN_NCBI_SCOPE


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


static const string kEUtils_Base_URL =
    "http://eutils.ncbi.nlm.nih.gov/entrez/eutils/";
/*
static const string kEInfo_URL = "einfo.fcgi?";
static const string kESearch_URL = "esearch.fcgi?";
static const string kEPost_URL = "epost.fcgi?";
static const string kESummary_URL = "esummary.fcgi?";
static const string kELink_URL = "elink.fcgi?";
static const string kEGQuery_URL = "egquery.fcgi?";
static const string kESpell_URL = "espell.fcgi?";
*/


void CEUtils_Request::Connect(void)
{
    m_Stream.reset(new CConn_HttpStream(
        kEUtils_Base_URL +
        GetScriptName() + "?" +
        GetQueryString(),
        fHCC_AutoReconnect,
        GetConnContext()->GetTimeout()));
}


string CEUtils_Request::GetQueryString(void) const
{
    string args = "db=" + m_Database;
    const string& webenv = GetConnContext()->GetWebEnv();
    if ( !webenv.empty() ) {
        args += "&WebEnv=" +
            URL_EncodeString(webenv, eUrlEncode_ProcessMarkChars);
    }
    string qk = GetQueryKey();
    if ( !qk.empty() ) {
        args += "&query_key=" + qk;
    }
    const string& tool = GetConnContext()->GetTool();
    if ( !tool.empty() ) {
        args += "&tool=" +
            URL_EncodeString(tool, eUrlEncode_ProcessMarkChars);
    }
    const string& email = GetConnContext()->GetEmail();
    if ( !email.empty() ) {
        args += "&email=" +
            URL_EncodeString(email, eUrlEncode_ProcessMarkChars);
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
    content->erase();
    char buf[4096];
    CNcbiIostream& is = GetStream();
    while ( is.good() ) {
        is.read(buf, sizeof(buf));
        streamsize len = is.gcount();
        if ( content ) {
            content->append(buf, len);
        }
    }
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
            "id=" : URL_EncodeString(",", eUrlEncode_ProcessMarkChars);
        ret += URL_EncodeString(*it, eUrlEncode_ProcessMarkChars);
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
            ret += "&";
        }
        ret += it->AsQueryString();
    }
    return ret;
}


END_NCBI_SCOPE
