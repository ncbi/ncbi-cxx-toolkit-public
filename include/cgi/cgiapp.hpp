#ifndef NCBI_CGI_APP__HPP
#define NCBI_CGI_APP__HPP

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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.24  2001/10/05 14:56:20  ucko
* Minor interface tweaks for CCgiStreamDiagHandler and descendants.
*
* Revision 1.23  2001/10/04 18:17:51  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* Revision 1.22  2001/06/13 21:04:35  vakatov
* Formal improvements and general beautifications of the CGI lib sources.
*
* Revision 1.21  2001/01/12 21:58:25  golikov
* cgicontext available from cgiapp
*
* Revision 1.20  2000/01/20 17:54:55  vakatov
* CCgiApplication:: constructor to get CNcbiArguments, and not raw argc/argv.
* SetupDiag_AppSpecific() to override the one from CNcbiApplication:: -- lest
* to write the diagnostics to the standard output.
*
* Revision 1.19  1999/12/17 04:06:20  vakatov
* Added CCgiApplication::RunFastCGI()
*
* Revision 1.18  1999/11/15 15:53:19  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.17  1999/05/14 19:21:48  pubmed
* myncbi - initial version; minor changes in CgiContext, history, query
*
* Revision 1.15  1999/05/06 20:32:47  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.14  1999/04/30 16:38:08  vakatov
* #include <ncbireg.hpp> to provide CNcbiRegistry class definition(see R1.13)
*
* Revision 1.13  1999/04/27 17:01:23  vakatov
* #include <ncbires.hpp> to provide CNcbiResource class definition
* for the "auto_ptr<CNcbiResource>" (required by "delete" under MSVC++)
*
* Revision 1.12  1999/04/27 14:49:46  vasilche
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
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/ncbicgir.hpp>
#include <cgi/ncbires.hpp>


BEGIN_NCBI_SCOPE

class CCgiServerContext;
class CNCBINode;


/////////////////////////////////////////////////////////////////////////////
//  CCgiDiagHandler::
//

class CCgiDiagHandler
{
public:
    virtual ~CCgiDiagHandler() {}
    virtual void SetDiagNode(CNCBINode* node) {}
    virtual void operator <<(const SDiagMessage& mess) = 0;
    virtual void Flush(void) {}
};


class CCgiStreamDiagHandler : public CCgiDiagHandler
{
public:
    CCgiStreamDiagHandler(CNcbiOstream* os) : m_Stream(os) {}

    virtual void operator <<(const SDiagMessage& mess) { *m_Stream << mess; }
protected:
    CNcbiOstream* m_Stream;
};


// Should return a newly allocated handler, which the caller then owns
typedef CCgiDiagHandler* (*FCgiDiagHandlerFactory)(const string& s,
                                                   CCgiContext& context);

// Actually in libconnect to avoid extra dependencies
CCgiDiagHandler* EmailDiagHandlerFactory(const string& s, CCgiContext&);

// Actually in libhtml to avoid extra dependencies
CCgiDiagHandler* CommentDiagHandlerFactory(const string& s, CCgiContext&);


/////////////////////////////////////////////////////////////////////////////
//  CCgiApplication::
//

class CCgiApplication : public CNcbiApplication
{
    typedef CNcbiApplication CParent;

public:
    CCgiApplication(void);

    static CCgiApplication* Instance(void); // Singleton method    

    // These methods will throw exception if no server context set
    const CCgiContext& GetContext(void) const  { return x_GetContext(); }
    CCgiContext&       GetContext(void)        { return x_GetContext(); }
    
    // These methods will throw exception if no resource is set
    const CNcbiResource& GetResource(void) const { return x_GetResource(); }
    CNcbiResource&       GetResource(void)       { return x_GetResource(); }

    virtual void Init(void);  // initialization
    virtual void Exit(void);  // cleanup

    virtual int Run(void);
    virtual int ProcessRequest(CCgiContext& context) = 0;
    
    virtual CNcbiResource*     LoadResource(void);
    virtual CCgiServerContext* LoadServerContext(CCgiContext& context);

protected:
    // Factory method for the Context object construction
    virtual CCgiContext*   CreateContext(CNcbiArguments*   args = 0,
                                         CNcbiEnvironment* env  = 0,
                                         CNcbiIstream*     inp  = 0, 
                                         CNcbiOstream*     out  = 0,
                                         int               ifd  = -1,
                                         int               ofd  = -1);

    void                   RegisterCgiDiagHandler(const string& key,
                                                FCgiDiagHandlerFactory fact);
    FCgiDiagHandlerFactory FindCgiDiagHandler(const string& key);
    virtual void           ConfigureDiagnostics(CCgiContext& context);
    virtual void           ConfigureDiagDestination(CCgiContext& context);
    virtual void           ConfigureDiagThreshold(CCgiContext& context);
    virtual void           ConfigureDiagFormat(CCgiContext& context);

    virtual void           FlushDiagnostics(void);
    virtual void           SetDiagNode(CNCBINode* node);

private:
    // If FastCGI-capable, and run as a Fast-CGI then iterate through
    // the FastCGI loop -- doing initialization and running ProcessRequest()
    // time after time; then return TRUE.
    // Return FALSE overwise.
    // In the "result", return exit code of the last CGI iteration run.
    bool RunFastCGI(int* result, unsigned def_iter = 3);

    CCgiContext&   x_GetContext (void) const;
    CNcbiResource& x_GetResource(void) const;

    auto_ptr<CNcbiResource>   m_Resource;
    auto_ptr<CCgiContext>     m_Context;

    auto_ptr<CCgiDiagHandler> m_DiagHandler;
    typedef map<string, FCgiDiagHandlerFactory> TDiagHandlerMap;
    TDiagHandlerMap           m_DiagHandlers;
};


END_NCBI_SCOPE

#endif // NCBI_CGI_APP__HPP
