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
* Author: Eugene Vasilchenko
*
* File Description:
*   Definition CGI application class and its context class.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  2000/05/24 20:57:13  vasilche
* Use new macro _DEBUG_ARG to avoid warning about unused argument.
*
* Revision 1.18  2000/01/20 17:54:15  vakatov
* CCgiContext:: constructor to get "CNcbiArguments*" instead of raw argc/argv.
* All virtual member function implementations moved away from the header.
*
* Revision 1.17  1999/12/23 17:16:18  golikov
* CtxMsgs made not HTML lib depended
*
* Revision 1.16  1999/12/15 19:19:10  golikov
* fixes
*
* Revision 1.15  1999/11/15 15:54:53  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.14  1999/10/21 16:10:53  vasilche
* Fixed memory leak in CNcbiOstrstream::str()
*
* Revision 1.13  1999/10/01 14:21:40  golikov
* Now messages in context are html nodes
*
* Revision 1.12  1999/09/03 21:32:28  vakatov
* Move #include <algorithm> after the NCBI #include's for more
* consistency and to suppress some bulky MSVC++ warnings.
*
* Revision 1.11  1999/07/15 19:05:17  sandomir
* GetSelfURL(() added in Context
*
* Revision 1.10  1999/07/07 14:23:37  pubmed
* minor changes for VC++
*
* Revision 1.9  1999/06/29 20:02:29  pubmed
* many changes due to query interface changes
*
* Revision 1.8  1999/05/14 19:21:54  pubmed
* myncbi - initial version; minor changes in CgiContext, history, query
*
* Revision 1.6  1999/05/06 20:33:43  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.5  1999/05/04 16:14:44  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* Revision 1.4  1999/05/04 00:03:11  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.3  1999/04/30 19:21:02  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.2  1999/04/28 16:54:41  vasilche
* Implemented stream input processing for FastCGI applications.
* Fixed POST request parsing
*
* Revision 1.1  1999/04/27 14:50:04  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <cgi/ncbires.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgiapp.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE


string CCtxMsgString::sm_nl = "\n";


CCgiServerContext::~CCgiServerContext(void) {
    return;
}


CCtxMsg::~CCtxMsg(void) {
    return;
}


CCtxMsgString::~CCtxMsgString(void) {
    return;
}

CNcbiOstream& CCtxMsgString::Write(CNcbiOstream& os) const {
    return os << m_s << sm_nl;
}


//
// class CCgiContext
//

CCgiContext::CCgiContext(CCgiApplication&        app,
                         const CNcbiArguments*   args,
                         const CNcbiEnvironment* env,
                         CNcbiIstream*           inp,
                         CNcbiOstream*           out)
: m_app(app),
  m_request(0),
  m_response(out)
{
    try {
        if ( !args )
            args = &app.GetArguments();
        if ( !env )
            env = &app.GetEnvironment();

        m_request.reset(new CCgiRequest(args, env, inp));
    } catch (exception& _DEBUG_ARG(e)) {
        _TRACE("CCgiContext::CCgiContext: " << e.what());
        PutMsg("Bad request");

        char buf[1];
        CNcbiIstrstream dummy(buf, 0);
        m_request.reset(new CCgiRequest
                        (args, env, &dummy, CCgiRequest::fIgnoreQueryString));
    }
}


CCgiContext::~CCgiContext( void )
{
    DeleteElements( m_lmsg );
}

const CNcbiRegistry& CCgiContext::GetConfig(void) const
{
    return m_app.GetConfig();
}

CNcbiRegistry& CCgiContext::GetConfig(void)
{
    return m_app.GetConfig();
}

CNcbiResource& CCgiContext::x_GetResource(void) const
{
    return m_app.x_GetResource();
}

CCgiServerContext& CCgiContext::x_GetServCtx( void ) const
{ 
    CCgiServerContext* context = m_srvCtx.get();
    if ( !context ) {
        context = m_app.LoadServerContext(const_cast<CCgiContext&>(*this));
        if ( !context ) {
            ERR_POST("CCgiContext::GetServCtx: no server context set");
            throw runtime_error("no server context set");
        }
        const_cast<CCgiContext&>(*this).m_srvCtx.reset(context);
    }
    return *context;
}

string CCgiContext::GetRequestValue(const string& name) const
{
    TCgiEntries& entries = const_cast<TCgiEntries&>  // (workaround WS5.0 bug)
        (GetRequest().GetEntries() );
    pair<TCgiEntriesI, TCgiEntriesI> range = entries.equal_range(name);
    if (range.second == range.first)
        return NcbiEmptyString;

    const string& value = range.first->second;
    while (++range.first != range.second) {
        if (range.first->second != value) {
            THROW1_TRACE(runtime_error,
                         "duplicated entries in request with name: " +
                         name + ": " + value + "!=" + range.first->second);
        }
    }
    return value;
}

void CCgiContext::RemoveRequestValues(const string& name)
{
    const_cast<TCgiEntries&>(GetRequest().GetEntries()).erase(name);
}

void CCgiContext::AddRequestValue(const string& name, const string& value)
{
    const_cast<TCgiEntries&>( GetRequest().GetEntries()).insert(
                                     TCgiEntries::value_type(name, value));
}

void CCgiContext::ReplaceRequestValue(const string& name, 
                                      const string& value)
{
    RemoveRequestValues(name);
    AddRequestValue(name, value);
}

const string& CCgiContext::GetSelfURL(void) const
{
    if( m_selfURL.empty() ) {
        m_selfURL = "http://";
        m_selfURL += GetRequest().GetProperty(eCgi_ServerName);
        m_selfURL += ':';
        m_selfURL += GetRequest().GetProperty(eCgi_ServerPort);
        
        // bug in www.ncbi proxy - replace adjacent '//'
        string script( GetRequest().GetProperty(eCgi_ScriptName) );
        string::iterator b = script.begin(), it;
        
        while( ( it = adjacent_find( b, script.end() ) ) != script.end() ) {
            if( *it == '/' ) {
                b = script.erase( it );
            } else {
                b = it + 2;
            }
        } // while
        
        m_selfURL += script;
    }
    return m_selfURL;
}

END_NCBI_SCOPE
