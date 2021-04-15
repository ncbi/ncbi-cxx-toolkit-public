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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Monitor VDB User-Agent
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "vdb_user_agent.hpp"
#include <corelib/ncbithr.hpp>
#include <klib/log.h>
#include <klib/debug.h>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

#if !HAVE_LIBCONNEXT // external builds do not have debug version of VDB library
# define HAVE_DEBUG_VDB_CALL 0
#else
# define HAVE_DEBUG_VDB_CALL 1
#endif

bool CVDBUserAgentMonitor::SUserAgentValues::operator==(const SUserAgentValues& values) const
{
    return
        client_ip == values.client_ip &&
        session_id == values.session_id &&
        page_hit_id == values.page_hit_id;
}


void CVDBUserAgentMonitor::Append(const char* buffer, size_t size)
{
    while ( size ) {
        const char* eol = (const char*)memchr(buffer, '\n', size);
        size_t copy = eol? eol+1-buffer: size;
        m_Line.append(buffer, copy);
        buffer += copy;
        size -= copy;
        if ( eol ) {
            x_CheckLine();
            m_Line.erase();
        }
    }
}
inline ostream& operator<<(ostream& out, const CVDBUserAgentMonitor::SUserAgentValues& values)
{
    return out << "{ cip="<<values.client_ip
               <<", sid="<<values.session_id
               <<", hid="<<values.page_hit_id<<" }";
}

static map<string, CVDBUserAgentMonitor::SUserAgentValues> sm_ExpectedValues;

string s_GetKey(const string& file_name)
{
    size_t pos = file_name.rfind('/');
    if ( pos == NPOS ) {
        pos = 0;
    }
    else {
        pos += 1;
    }
    return file_name.substr(pos);
}

void CVDBUserAgentMonitor::SetExpectedUserAgentValues(const string& file_name,
                                                      const SUserAgentValues& values)
{
    _TRACE("VDBUA: set file "<<file_name<<" values: "<<values);
    string key = s_GetKey(file_name);
    sm_ExpectedValues[key] = values;
}

DEFINE_STATIC_FAST_MUTEX(s_ErrorMutex);
static bool s_HaveDebugVDBHook = false;
static size_t s_ValidUserAgentCount = 0;
static CVDBUserAgentMonitor::TErrorFlags s_ErrorFlags = CVDBUserAgentMonitor::fNoErrors;
static CVDBUserAgentMonitor::TErrorMessages s_ErrorMessages;

void CVDBUserAgentMonitor::x_AddError(const string& message, TErrorFlags flags)
{
    _TRACE("VDBUA: error("<<flags<<"): "<<message);
    CFastMutexGuard guard(s_ErrorMutex);
    s_ErrorFlags |= flags;
    s_ErrorMessages.push_back(message);
}

void CVDBUserAgentMonitor::x_AddValidUserAgent()
{
    _TRACE("VDBUA: valid Useq-agent");
    CFastMutexGuard guard(s_ErrorMutex);
    s_ValidUserAgentCount += 1;
}

void CVDBUserAgentMonitor::ReportErrors()
{
    CFastMutexGuard guard(s_ErrorMutex);
    if ( !s_ValidUserAgentCount ) {
        if ( s_HaveDebugVDBHook ) {
            ERR_POST("CVDBUserAgentMonitor: no User-Agent lines");
        }
        else {
            LOG_POST("CVDBUserAgentMonitor: no User-Agent lines because of non-debug VDB library");
        }
    }
    else {
        LOG_POST("CVDBUserAgentMonitor: "<<s_ValidUserAgentCount<<" User-Agent lines");
    }
    if ( s_ErrorFlags ) {
        ERR_POST("CVDBUserAgentMonitor: error flags="<<s_ErrorFlags);
        for ( auto& msg : s_ErrorMessages ) {
            ERR_POST("CVDBUserAgentMonitor: "<<msg);
        }
    }
}

void CVDBUserAgentMonitor::ResetErrors()
{
    CFastMutexGuard guard(s_ErrorMutex);
    s_ValidUserAgentCount = 0;
    s_ErrorFlags = fNoErrors;
    s_ErrorMessages.clear();
}

size_t CVDBUserAgentMonitor::GetValidUserAgentCount()
{
    CFastMutexGuard guard(s_ErrorMutex);
    return s_ValidUserAgentCount;
}

CVDBUserAgentMonitor::TErrorFlags CVDBUserAgentMonitor::GetErrorFlags()
{
    CFastMutexGuard guard(s_ErrorMutex);
    auto flags = s_ErrorFlags;
    if ( s_HaveDebugVDBHook && s_ValidUserAgentCount == 0 ) {
        flags |= fErrorNoUserAgentLines;
    }
    return flags;
}

CVDBUserAgentMonitor::TErrorMessages CVDBUserAgentMonitor::GetErrorMessages()
{
    CFastMutexGuard guard(s_ErrorMutex);
    return s_ErrorMessages;
}

const CVDBUserAgentMonitor::SUserAgentValues*
CVDBUserAgentMonitor::GetExpectedUserAgentValues(const string& file_name)
{
    string key = s_GetKey(file_name);
    auto iter = sm_ExpectedValues.find(key);
    if ( iter == sm_ExpectedValues.end() ) {
        ostringstream s;
        s << "unknown file: \""<<file_name<<"\"";
        x_AddError(s.str(), fErrorUnknownFile);
        return nullptr;
    }
    return &iter->second;
}

void CVDBUserAgentMonitor::x_CheckLine()
{
    if ( NStr::StartsWith(m_Line, "HTTP send '") ) {
        _TRACE("VDBUA: LINE: "<<m_Line);
        size_t pos = NPOS;
        if ( pos == NPOS ) {
            pos = m_Line.find("' 'HEAD ");
            if ( pos != NPOS ) {
                pos += 8;
            }
        }
        if ( pos == NPOS ) {
            pos = m_Line.find("' 'GET ");
            if ( pos != NPOS ) {
                pos += 7;
            }
        }
        if ( pos == NPOS ) {
            pos = m_Line.find("' 'POST ");
            if ( pos != NPOS ) {
                pos += 8;
            }
        }
        size_t end = m_Line.find(" HTTP/", pos);
        if ( pos == NPOS || end == NPOS ) {
            m_FileName.erase();
            ostringstream s;
            s << "bad HTTP send line: \""<<m_Line<<"\""
                " pos="<<ssize_t(pos)<<" end="<<ssize_t(end);
            x_AddError(s.str(), fErrorParseHTTPLine);
            return;
        }
        m_FileName = m_Line.substr(pos, end-pos);
        _TRACE("VDBUA: file "<<m_FileName);
        GetExpectedUserAgentValues(m_FileName); // verify values are known
        return;
    }
    if ( NStr::StartsWith(m_Line, "User-Agent: ") ) {
        _TRACE("VDBUA: LINE: "<<m_Line);
        if ( m_FileName.empty() ) {
            ostringstream s;
            s << "User-Agent without HTTP send: \""<<m_Line<<"\"";
            x_AddError(s.str(), fErrorNoHTTPLine);
            return;
        }
        size_t p_end = m_Line.rfind(')');
        size_t p_begin = m_Line.rfind(',', p_end);
        string str;
        if ( p_begin != NPOS && p_end != NPOS ) {
            p_begin += 1;
            str = NStr::Base64Decode(m_Line.substr(p_begin, p_end-p_begin));
        }
        SUserAgentValues values;
        vector<string> split_values;
        NStr::Split(str, ",", split_values);
        for ( auto v : split_values ) {
            string name, value;
            NStr::SplitInTwo(v, "=", name, value);
            if ( name == "cip" ) {
                values.client_ip = value;
            }
            else if ( name == "sid" ) {
                values.session_id = value;
            }
            else if ( name == "pagehit" ) {
                values.page_hit_id = value;
            }
        }
        if ( auto exp_values = GetExpectedUserAgentValues(m_FileName) ) {
            if ( *exp_values == SUserAgentValues::Any() || values == *exp_values ) {
                x_AddValidUserAgent();
            }
            else {
                _TRACE("VDBUA: file: "<<m_FileName);
                _TRACE("VDBUA: VALUES: "<<values);
                _TRACE("VDBUA: REQCTX: "<<*exp_values);
                ostringstream s;
                s << "wrong User-Agent values file=\""<<m_FileName<<"\""
                    " "<<values<<" != expected "<<*exp_values;
                x_AddError(s.str(), fErrorWrongValues);
            }
        }
        m_FileName.erase();
    }
}


static CStaticTls<CVDBUserAgentMonitor> s_Monitor;

CVDBUserAgentMonitor& CVDBUserAgentMonitor::GetInstance()
{
    if ( !s_Monitor.GetValue() ) {
        s_Monitor.SetValue(new CVDBUserAgentMonitor());
    }
    return *s_Monitor.GetValue();
}


#if HAVE_DEBUG_VDB_CALL
static rc_t CC s_VDBOutWriter(void* self, const char* buffer, size_t bufsize, size_t* num_writ)
{
    CVDBUserAgentMonitor::GetInstance().Append(buffer, bufsize);
    *num_writ = bufsize;
    return 0;
}
#endif


void CVDBUserAgentMonitor::Initialize()
{
    KLogLevelSet(klogDebug);
#if HAVE_DEBUG_VDB_CALL
    KWrtWriter my_writer = s_VDBOutWriter;
    if ( KDbgWriterGet() != nullptr ) {
        NCBI_THROW_FMT(CException, eUnknown,
                       "CVDBUserAgentMonitor:"
                       " KDbgHandler is already set");
    }
    if ( rc_t rc = KDbgHandlerSet(my_writer, 0) ) {
        NCBI_THROW_FMT(CException, eUnknown,
                       "CVDBUserAgentMonitor:"
                       " KDbgHandlerSet() failed: "<<rc);
    }
    s_HaveDebugVDBHook = KDbgWriterGet() != nullptr;
    if ( s_HaveDebugVDBHook && KDbgWriterGet() != my_writer ) {
        NCBI_THROW_FMT(CException, eUnknown,
                       "CVDBUserAgentMonitor:"
                       " KDbgHandler was set to something else");
    }
    if ( rc_t rc = KDbgSetString("KNS") ) {
        NCBI_THROW_FMT(CException, eUnknown,
                       "CVDBUserAgentMonitor:"
                       " KDbgSetString() failed: "<<rc);
    }
#endif
    SetExpectedUserAgentValues("availability-zone", SUserAgentValues::Any());
    SetExpectedUserAgentValues("names.fcgi", SUserAgentValues::Any());
}

END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
