#ifndef NCBI_CGI_CTX__HPP
#define NCBI_CGI_CTX__HPP

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
* Author: 
*	Vsevolod Sandomirskiy
*
* File Description:
*   Basic CGI Application class
*/

#include <corelib/ncbistd.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/ncbicgir.hpp>
#include <connect/ncbi_types.h>


/** @addtogroup CGIBase
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CCgiServerContext::
//

class NCBI_XCGI_EXPORT CCgiServerContext
{
public:
    virtual ~CCgiServerContext(void);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CCtxMsg::
//

class NCBI_XCGI_EXPORT CCtxMsg
{
public:
    virtual ~CCtxMsg(void);
    virtual CNcbiOstream& Write(CNcbiOstream& os) const = 0;
};


/* @} */


inline
CNcbiOstream& operator<< (CNcbiOstream& os, const CCtxMsg& ctx_msg)
{
    return ctx_msg.Write(os);
}


/** @addtogroup CGIBase
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
//
//  CCtxMsgString::
//

class NCBI_XCGI_EXPORT CCtxMsgString : public CCtxMsg
{
public:
    CCtxMsgString(const string& msg) : m_Message(msg) {}
    virtual ~CCtxMsgString(void);
    virtual CNcbiOstream& Write(CNcbiOstream& os) const;

    static string sm_nl;

private:
    string m_Message;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CCgiContext::
//
// CCgiContext is a wrapper for request, response, server context.
// In addition, it contains list of messages (as HTML nodes).
// Having non-const reference, CCgiContext's user has access to all its 
// internal data.
// Context will try to create request from given data or default request
// on any request creation error
//

class CNcbiEnvironment;
class CNcbiRegistry;
class CNcbiResource;
class CCgiApplication;


class NCBI_XCGI_EXPORT CCgiContext
{
public:
    CCgiContext(CCgiApplication&        app,
                const CNcbiArguments*   args = 0 /* D: app.GetArguments()   */,
                const CNcbiEnvironment* env  = 0 /* D: app.GetEnvironment() */,
                CNcbiIstream*           inp  = 0 /* see ::CCgiRequest(istr) */,
                CNcbiOstream*           out  = 0 /* see ::CCgiResponse(out) */,
                int                     ifd  = -1,
                int                     ofd  = -1,
                size_t                  errbuf_size = 256, /* see CCgiRequest */
                CCgiRequest::TFlags     flags = 0
                );
    virtual ~CCgiContext(void);

    const CCgiApplication& GetApp(void) const;

    const CNcbiRegistry& GetConfig(void) const;
    CNcbiRegistry& GetConfig(void);
    
    // these methods will throw exception if no server context is set
    const CNcbiResource& GetResource(void) const;
    CNcbiResource&       GetResource(void);

    const CCgiRequest& GetRequest(void) const;
    CCgiRequest&       GetRequest(void);
    
    const CCgiResponse& GetResponse(void) const;
    CCgiResponse&       GetResponse(void);
    
    // these methods will throw exception if no server context set
    const CCgiServerContext& GetServCtx(void) const;
    CCgiServerContext&       GetServCtx(void);

    // message buffer functions
    CNcbiOstream& PrintMsg(CNcbiOstream& os);

    void PutMsg(const string& msg);
    void PutMsg(CCtxMsg*      msg);

    bool EmptyMsg(void);
    void ClearMsg(void);

    // request access wrappers

    // return entry from request
    // return empty string if no such entry
    // throw runtime_error if there are several entries with the same name
    const CCgiEntry& GetRequestValue(const string& name, bool* is_found = 0)
        const;

    void AddRequestValue    (const string& name, const CCgiEntry& value);
    void RemoveRequestValues(const string& name);
    void ReplaceRequestValue(const string& name, const CCgiEntry& value);

    /// Whether to use the port number when composing the CGI's own URL
    /// @sa GetSelfURL()
    enum ESelfUrlPort {
        eSelfUrlPort_Use,     ///< Use port number in self-URL
        eSelfUrlPort_Strip,   ///< Do not use port number in self-URL
        eSelfUrlPort_Default  ///< Use port number, except for NCBI front-ends
    };

    /// Using HTTP environment variables, compose the CGI's own URL as:
    ///   http://SERVER_NAME[:SERVER_PORT]/SCRIPT_NAME
    const string& GetSelfURL(ESelfUrlPort use_port = eSelfUrlPort_Default)
        const;

    // Which streams are ready?
    enum EStreamStatus {
        fInputReady  = 0x1,
        fOutputReady = 0x2
    };
    typedef int TStreamStatus;  // binary OR of 'EStreamStatus'
    TStreamStatus GetStreamStatus(STimeout* timeout) const;
    TStreamStatus GetStreamStatus(void) const; // supplies {0,0}

private:
    CCgiServerContext& x_GetServerContext(void) const;

    CCgiApplication&      m_App;
    auto_ptr<CCgiRequest> m_Request;  // CGI request  information
    CCgiResponse          m_Response; // CGI response information

    // message buffer
    typedef list< AutoPtr<CCtxMsg> > TMessages;
    TMessages m_Messages;

    // server context will be obtained from CCgiApp::LoadServerContext()
    auto_ptr<CCgiServerContext> m_ServerContext; // application defined context

    mutable string m_SelfURL;

    // forbidden
    CCgiContext(const CCgiContext&);
    CCgiContext& operator=(const CCgiContext&);
}; 


/* @} */


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CCgiContext::
//

inline
const CCgiApplication& CCgiContext::GetApp(void) const
{ 
    return m_App;
}
    

inline
const CCgiRequest& CCgiContext::GetRequest(void) const
{
    return *m_Request;
}


inline
CCgiRequest& CCgiContext::GetRequest(void)
{
    return *m_Request;
}

    
inline
const CCgiResponse& CCgiContext::GetResponse(void) const
{
    return m_Response;
}


inline
CCgiResponse& CCgiContext::GetResponse(void)
{
    return m_Response;
}


inline
const CCgiServerContext& CCgiContext::GetServCtx(void) const
{
    return x_GetServerContext();
}


inline
CCgiServerContext& CCgiContext::GetServCtx(void)
{
    return x_GetServerContext();
}


inline
CNcbiOstream& CCgiContext::PrintMsg(CNcbiOstream& os)
{
    ITERATE (TMessages, it, m_Messages) {
        os << **it;
    }
    return os;
}


inline
void CCgiContext::PutMsg(const string& msg)
{
    m_Messages.push_back(new CCtxMsgString(msg));
}


inline
void CCgiContext::PutMsg(CCtxMsg* msg)
{
    m_Messages.push_back(msg);
}


inline
bool CCgiContext::EmptyMsg(void)
{
    return m_Messages.empty();
}


inline
void CCgiContext::ClearMsg(void)
{
    m_Messages.clear();
}


inline
CCgiContext::TStreamStatus CCgiContext::GetStreamStatus(void) const
{
    STimeout timeout = {0, 0};
    return GetStreamStatus(&timeout);
}


END_NCBI_SCOPE



/*
* ===========================================================================
* $Log$
* Revision 1.30  2004/06/21 16:20:08  vakatov
* GetSelfURL() -- allow to skip port #;  do it for NCBI frontents by default
*
* Revision 1.29  2004/05/11 12:43:29  kuznets
* Changes to control HTTP parsing (CCgiRequest flags)
*
* Revision 1.28  2003/11/05 18:25:32  dicuccio
* Added export specifiers.  Added private, unimplemented copy ctor/assignment
* operator
*
* Revision 1.27  2003/04/16 21:48:17  vakatov
* Slightly improved logging format, and some minor coding style fixes.
*
* Revision 1.26  2003/04/10 19:01:41  siyan
* Added doxygen support
*
* Revision 1.25  2003/03/11 19:17:10  kuznets
* Improved error diagnostics in CCgiRequest
*
* Revision 1.24  2003/03/10 17:46:36  kuznets
* iterate->ITERATE
*
* Revision 1.23  2003/02/21 19:19:15  vakatov
* CCgiContext::GetRequestValue() -- added optional arg "is_found"
*
* Revision 1.22  2003/02/16 05:30:18  vakatov
* GetRequestValue() to return "const CCgiEntry&" rather than just "CCgiEntry"
* to avoid some nasty surprises for earlier user code looking as:
*    const string& s = GetRequestValue(...);
* caused by 'premature' destruction of temporary CCgiEntry object (GCC 3.0.4).
*
* Revision 1.21  2002/07/17 17:03:01  ucko
* Phase out GetRequestValueEx.
*
* Revision 1.20  2002/07/10 18:39:44  ucko
* Made CCgiEntry-based functions the only version; kept "Ex" names as
* temporary synonyms, to go away in a few days.
*
* Revision 1.19  2002/07/03 20:24:30  ucko
* Extend to support learning uploaded files' names; move CVS logs to end.
*
* Revision 1.18  2001/10/04 18:17:51  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* Revision 1.17  2001/06/13 21:04:35  vakatov
* Formal improvements and general beautifications of the CGI lib sources.
*
* Revision 1.16  2001/05/17 14:49:01  lavr
* Typos corrected
*
* Revision 1.15  2000/12/23 23:53:19  vakatov
* TLMsg container to use AutoPtr instead of regular pointer
*
* Revision 1.14  2000/01/20 17:54:13  vakatov
* CCgiContext:: constructor to get "CNcbiArguments*" instead of raw argc/argv.
* All virtual member function implementations moved away from the header.
*
* Revision 1.13  1999/12/23 17:16:11  golikov
* CtxMsgs made not HTML lib depended
*
* Revision 1.12  1999/11/15 15:53:20  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.11  1999/10/28 16:53:53  vasilche
* Fixed bug with error message node.
*
* Revision 1.10  1999/10/28 13:37:49  vasilche
* Fixed small memory leak.
*
* Revision 1.9  1999/10/01 14:22:04  golikov
* Now messages in context are html nodes
*
* Revision 1.8  1999/07/15 19:04:37  sandomir
* GetSelfURL(() added in Context
*
* Revision 1.7  1999/06/29 20:04:37  pubmed
* many changes due to query interface changes
*
* Revision 1.6  1999/05/14 19:21:49  pubmed
* myncbi - initial version; minor changes in CgiContext, history, query
*
* Revision 1.4  1999/05/06 20:32:48  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.3  1999/05/04 16:14:03  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* Revision 1.2  1999/04/28 16:54:18  vasilche
* Implemented stream input processing for FastCGI applications.
*
* Revision 1.1  1999/04/27 14:49:48  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.11  1999/02/22 21:12:37  sandomir
* MsgRequest -> NcbiContext
*
* Revision 1.10  1998/12/28 23:28:59  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.9  1998/12/28 15:43:09  sandomir
* minor fixed in CgiApp and Resource
*
* Revision 1.8  1998/12/10 17:36:54  sandomir
* ncbires.cpp added
*
* Revision 1.7  1998/12/09 22:59:05  lewisg
* use new cgiapp class
*
* Revision 1.6  1998/12/09 17:27:44  sandomir
* tool should be changed to work with the new CCgiApplication
*
* Revision 1.5  1998/12/09 16:49:55  sandomir
* CCgiApplication added
*
* Revision 1.1  1998/12/03 21:24:21  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.3  1998/12/01 19:12:36  lewisg
* added CCgiApplication
*
* Revision 1.2  1998/11/05 21:45:13  sandomir
* std:: deleted
*
* Revision 1.1  1998/11/02 22:10:12  sandomir
* CNcbiApplication added; netest sample updated
* ===========================================================================
*/

#endif // NCBI_CGI_CTX__HPP
