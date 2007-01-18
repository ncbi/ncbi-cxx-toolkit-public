#ifndef NCBI_GRID_MGR_CMD__HPP
#define NCBI_GRID_MGR_CMD__HPP

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
* Author:  Maxim Didenko
*
*/

#include <cgi/ncbires.hpp>
#include <html/html.hpp>

#include <set>

#include "mgrres.hpp"
#include "mgrlogic.hpp"

BEGIN_NCBI_SCOPE
class CHTMLPage;
END_NCBI_SCOPE

USING_NCBI_SCOPE;


//
// CHookContext
//

template<typename Cont>
struct Cont_Triat {
    typedef typename Cont::value_type value_type;
};

template<typename Cont, typename Renderer>
class CHookContext 
{
public:
    typedef Cont cont_type;
    typedef Renderer  renderer_type;
    typedef typename Cont_Triat<Cont>::value_type value_type;
    typedef typename Cont::const_iterator const_iterator;

    CHookContext(Cont* cont, Renderer& renderer) 
        : m_ContGuard(cont), m_Cont(*cont), 
          m_Curr(cont->begin()), m_RowNumber(0), 
          m_Renderer(renderer)
    {}

    CHookContext(const Cont& cont, Renderer& renderer) 
        : m_Cont(cont), 
          m_Curr(cont.begin()), m_RowNumber(0), m_Renderer(renderer) 
    {}

    const value_type& GetNext()
    { const value_type& val = **m_Curr; ++m_Curr; ++m_RowNumber; return val; }
    bool HasNext() const 
    { return (m_Curr == m_Cont.end()) ?  false : true; }
    
    long GetRowNumber() const { return m_RowNumber; }
    renderer_type& GetRenderer() { return m_Renderer; }

private:
    auto_ptr<Cont> m_ContGuard;
    const Cont& m_Cont;  
    const_iterator m_Curr;
    long m_RowNumber;
    Renderer& m_Renderer;
};

template<class THookContext>
CNCBINode* RowHook(CHTMLPage* page, THookContext* ctx)
{
    typedef typename THookContext::cont_type cont_type;
    typedef typename THookContext::renderer_type renderer_type;  
    typedef typename Cont_Triat<cont_type>::value_type value_type;
    
    if (!ctx->HasNext())
        return NULL;
    long row_number = ctx->GetRowNumber();
    
    const value_type& info = ctx->GetNext();
    renderer_type& renderer = ctx->GetRenderer();
    
    auto_ptr<CNCBINode> node(renderer.Render(info, *page, row_number));
    node->RepeatTag();
    return node.release();
}




//
// CGridMgrCommand
// 

class CGridMgrCommand : public CNcbiCommand
{
public:
  
    CGridMgrCommand( CGridMgrResource& resource );
    virtual ~CGridMgrCommand();

    virtual void Execute( CCgiContext& ctx );
    virtual string GetLink(CCgiContext&) const 
        { return NcbiEmptyString; }

    virtual CNCBINode* CreateView( CCgiContext& ctx ) = 0;
    virtual void Clean();    

protected:

    CGridMgrResource& GetGridMgrResource() const
        { return dynamic_cast<CGridMgrResource&>( GetResource() ); }

    virtual string GetEntry() const;
    CHTMLPage& GetPage() { _ASSERT(m_Page.get()); return *m_Page; }
    
    static CNCBINode* CreateErrorNode();

private:
    auto_ptr<CHTMLPage> m_Page;
};

//
// CStartPageCommand
// 


class CStartPageCommand : public CGridMgrCommand
{
public:
  
    CStartPageCommand( CGridMgrResource& resource );
    virtual ~CStartPageCommand();


    virtual CNcbiCommand* Clone( void ) const;
    virtual string GetName( void ) const;
    virtual CNCBINode* CreateView( CCgiContext& ctx );

};


//
// CShowNSServersCommand
//

class CShowNSServersCommand : public CGridMgrCommand
{
public:

    CShowNSServersCommand( CGridMgrResource& resource );
    virtual ~CShowNSServersCommand();

    virtual CNcbiCommand* Clone( void ) const;

    virtual string GetName( void ) const;

    virtual CNCBINode* CreateView( CCgiContext& ctx );

    CNCBINode* Render(const CQueueInfo& info, 
                      CHTMLPage& page, long row_number);
    CNCBINode* Render(const CHostInfo& info, 
                      CHTMLPage& page, long row_number);
    CNCBINode* Render(const CServiceInfo& info, 
                      CHTMLPage& page, long row_number);

    virtual void Clean();

private:
   
    typedef CHookContext<CNSServices,CShowNSServersCommand> THookCtxSrvs;
    auto_ptr<THookCtxSrvs> m_TableRowHook;

    typedef CHookContext<CServiceInfo,CShowNSServersCommand> THookCtxSrv;
    list<AutoPtr<THookCtxSrv> > m_SvrHooks;

    typedef CHookContext<CHostInfo,CShowNSServersCommand> THookCtxHost;
    list<AutoPtr<THookCtxHost> > m_HostHooks;
};

//
// CShowNCServersCommand
//

class CShowNCServersCommand : public CGridMgrCommand
{
public:
    CShowNCServersCommand(CGridMgrResource& resource);
    virtual ~CShowNCServersCommand();

    virtual CNcbiCommand* Clone( void ) const;

    virtual string GetName( void ) const;

    virtual CNCBINode* CreateView( CCgiContext& ctx );

    CNCBINode* Render(const CServiceInfo& info, 
                      CHTMLPage& page, long row_number);

    virtual void Clean();

private:
    typedef CHookContext<CNSServices,CShowNCServersCommand> THookCtxSrvs;
    auto_ptr<THookCtxSrvs> m_TableRowHook;
};



//
// CShowServerStatCommand
//

class CShowServerStatCommand : public CGridMgrCommand
{
public:

    CShowServerStatCommand( CGridMgrResource& resource );
    virtual ~CShowServerStatCommand( void );

    virtual CNcbiCommand* Clone( void ) const;

    virtual string GetName( void ) const;

    virtual CNCBINode* CreateView( CCgiContext& ctx );

    CNCBINode* Render(const CWorkerNodeInfo& info, 
                      CHTMLPage& page, long row_number);

    virtual void Clean();

private:
    typedef CHookContext<CQueueInfo,CShowServerStatCommand> THookContext;
    auto_ptr<THookContext> m_TableRowHook;

};


//
// CShowNCServerStatCommand
//

class CShowNCServerStatCommand : public CGridMgrCommand
{
public:

    CShowNCServerStatCommand( CGridMgrResource& resource );
    virtual ~CShowNCServerStatCommand(void);

    virtual CNcbiCommand* Clone(void) const;

    virtual string GetName(void) const;

    virtual CNCBINode* CreateView(CCgiContext& ctx);
};


//
// CShowWNStatCommand
//

class CShowWNStatCommand : public CGridMgrCommand
{
public:

    CShowWNStatCommand( CGridMgrResource& resource );
    virtual ~CShowWNStatCommand( void );

    virtual CNcbiCommand* Clone( void ) const;
    virtual string GetName( void ) const;

    virtual CNCBINode* CreateView( CCgiContext& ctx );

};

//
// CTestRWNCCommand
//

class CTestRWNCCommand : public CGridMgrCommand
{
public:

    CTestRWNCCommand( CGridMgrResource& resource );
    virtual ~CTestRWNCCommand( void );

    virtual CNcbiCommand* Clone( void ) const;
    virtual string GetName( void ) const;

    virtual CNCBINode* CreateView( CCgiContext& ctx );

};

#endif /* NCBI_GRID_MGR_CMD__HPP */
