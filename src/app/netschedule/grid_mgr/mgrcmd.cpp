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
 * Author: Maxim Didneko
 *
 *
 */

#include <ncbi_pch.hpp>
#include "mgrres.hpp"
#include "mgrcmd.hpp"

#include <corelib/ncbistd.hpp>
#include <cgi/cgictx.hpp>
#include <corelib/ncbireg.hpp>
#include <html/page.hpp>
#include <html/html.hpp>

#include <memory>
#include <list>

//BEGIN_NCBI_SCOPE

CGridMgrCommand::CGridMgrCommand( CGridMgrResource& resource )
  : CNcbiCommand( resource )
{}

CGridMgrCommand::~CGridMgrCommand()
{
    Clean();
}

string CGridMgrCommand::GetEntry() const
{
    return string("cmd");  // set the value of this string in helloapi.cpp
}

void CGridMgrCommand::Execute( CCgiContext& ctx )
{
    const CNcbiRegistry& reg = ctx.GetConfig();
 
    // load in the html template file
    string baseFile = reg.Get( "filesystem", "HtmlBaseFile" );
    m_Page.reset( new CHTMLPage( NcbiEmptyString, baseFile ) );

    string incFile = reg.Get( "filesystem", "HtmlIncFile" );
    m_Page->LoadTemplateLibFile(incFile);
   
    // set up to replace <@VIEW@> in template file with html returned
    // from CreateView
    m_Page->AddTagMap( "VIEW", CreateView( ctx ) );

    m_Page->AddTagMap("SELF_URL",  new CHTMLText(ctx.GetSelfURL()));

    // actual page output
    ctx.GetResponse().WriteHeader();
    m_Page->Print(ctx.GetResponse().out(), CNCBINode::eHTML );
}

void CGridMgrCommand::Clean()
{
    m_Page.reset();
}

//
// CShowServersCommand
//

CShowServersCommand::CShowServersCommand( CGridMgrResource& resource )
  : CGridMgrCommand( resource )
{}

CShowServersCommand::~CShowServersCommand()
{}

CNcbiCommand* CShowServersCommand::Clone( void ) const
{
    return new CShowServersCommand( const_cast<CGridMgrResource&>(GetGridMgrResource()) );
}

string CShowServersCommand::GetName( void ) const
{
    return string("showserver"); // set the value of this string in helloapi.cpp
}





//
//  CreateView() is the function that creates the html to replace
//  the <@VIEW@> tag in the HtmlBaseFile
//


CNCBINode* CShowServersCommand::Render(const CQueueInfo& info, 
                                       CHTMLPage& page, long row_number)
{
    auto_ptr<CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("queue_href_template"));
    if (row_number != 0)
        page.AddTagMap("queue_sep",  new CHTMLPlainText(", "));
    else
        page.AddTagMap("queue_sep",  new CHTMLPlainText(""));

    page.AddTagMap("queue_name",  new CHTMLText(info.GetName()));
    page.AddTagMap("queue_host",  new CHTMLText(info.GetHost()));
    page.AddTagMap("queue_port",  new CHTMLText(GetSPort(info)));

    return node.release();
}

CNCBINode* CShowServersCommand::Render(const CHostInfo& info, 
                                       CHTMLPage& page, long row_number)
{
    auto_ptr<CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("srv_table_row_template"));

    if (info.IsActive()) {
        AutoPtr<THookCtxHost> 
            host_hook(new THookCtxHost(info,*this));

        page.AddTagMap("queue_href_hook", 
                    CreateTagMapper(RowHook<THookCtxHost>, host_hook.get()));

        m_HostHooks.push_back(host_hook);

    } else {
        page.AddTagMap("queue_href_hook",  new CHTMLPlainText(" "));
    }

    page.AddTagMap("srv_host",  new CHTMLText(info.GetHost()));
    page.AddTagMap("srv_port",  new CHTMLText(GetSPort(info)));
    page.AddTagMap("srv_version",  new CHTMLText(info.GetVersion()));
    
    string css_class = row_number % 2 ? "even" : "odd";
    page.AddTagMap("class", new CHTMLText(css_class));

    return node.release();
}

CNCBINode* CShowServersCommand::Render(const CServiceInfo& info, 
                                       CHTMLPage& page, long row_number)
{
    auto_ptr<CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("srvs_table_row_template"));

    page.AddTagMap("SERVICE_NAME",  new CHTMLText(info.GetName()));

    AutoPtr<THookCtxSrv> 
        srv_hook(new THookCtxSrv(info,*this));

    page.AddTagMap("srv_table_row_hook", 
                   CreateTagMapper(RowHook<THookCtxSrv>, srv_hook.get()));
    
    m_SvrHooks.push_back(srv_hook);
    return node.release();

}


CNCBINode* CShowServersCommand::CreateView(CCgiContext& ctx)
{
    auto_ptr <CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("SERVICES_VIEW"));

    const IRegistry& reg = ctx.GetConfig();
    
    string lbsurl = reg.GetString("urls", "lbsurl", "");
    CNSServices ssss(lbsurl);
    m_TableRowHook.reset(new THookCtxSrvs(new CNSServices(lbsurl),*this));


    GetPage().AddTagMap("srvs_table_row_hook", 
              CreateTagMapper(RowHook<THookCtxSrvs>, m_TableRowHook.get()));

    /*     
    auto_ptr <CHTML_form> Form(new CHTML_form(ctx.GetSelfURL()));    
                                 
    Form->AppendChild(new CHTML_text("input", 40));
    Form->AddHidden(GetEntry(), "reply");
    Form->AppendChild(new CHTML_submit("Go","submit"));
    return Form.release();
    */
    return node.release();
}

void CShowServersCommand::Clean()
{
    CGridMgrCommand::Clean();
    m_TableRowHook.reset();
    m_SvrHooks.clear();
    m_HostHooks.clear();
}

//
// CShowServerStatCommand
//

CShowServerStatCommand::CShowServerStatCommand( CGridMgrResource& resource )
  : CGridMgrCommand( resource )
{}

CShowServerStatCommand::~CShowServerStatCommand()
{}

CNcbiCommand* CShowServerStatCommand::Clone( void ) const
{
    return new CShowServerStatCommand( GetGridMgrResource() );
}

//
// GetName() returns the string used to match the value in a cgi request.
// e.g. for "?cmd=search", GetName() should return "search"
//

string CShowServerStatCommand::GetName( void ) const
{
    return string("queue_stat"); // set the value of this string in helloapi.cpp
}

//
//  CreateView() is the function that creates the html to replace
//  the <@VIEW@> tag in the HtmlBaseFile
//
CNCBINode* CShowServerStatCommand::Render(const CWorkerNodeInfo& info, 
                                          CHTMLPage& page, long row_number)
{
    auto_ptr<CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("wn_table_row_template"));

    auto_ptr<CNCBINode> wn_host_node(new CNCBINode);
    if (info.IsActive()) 
        wn_host_node->AppendChild(new CHTMLTagNode("wn_host_href"));
    else
        wn_host_node->AppendChild(new CHTMLTagNode("wn_host_text"));
    page.AddTagMap("wn_host_temp", wn_host_node.release());
    
    page.AddTagMap("name",  new CHTMLText(info.GetClientName()));
    page.AddTagMap("version",  new CHTMLText(info.GetVersion()));
    page.AddTagMap("wn_host",  new CHTMLText(info.GetHost()));
    page.AddTagMap("wn_port",  new CHTMLText(GetSPort(info)));

    page.AddTagMap("lastaccess", 
                    new CHTMLText(info.GetLastAccess().AsString("B D Y h:m:s")));
    page.AddTagMap("builddate", 
                    new CHTMLText(info.GetBuildTime().AsString("B D Y h:m:s")));

    string css_class = row_number % 2 ? "even" : "odd";
    page.AddTagMap("class", new CHTMLText(css_class));
    return node.release();
}


CNCBINode* CShowServerStatCommand::CreateView(CCgiContext& ctx)
{
    auto_ptr <CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("QUEUE_VIEW"));

    // get the list of cgi request name, value pairs
    const TCgiEntries& entries = ctx.GetRequest().GetEntries();

    // look for ones where the name is "input" 
    
    TCgiEntriesCI h_it = entries.find("server");
    TCgiEntriesCI p_it = entries.find("port");
    TCgiEntriesCI q_it = entries.find("queue");
    try {
        if( h_it == entries.end() ||  p_it == entries.end() || q_it == entries.end())
            throw runtime_error("");
        
        string host = (*h_it).second.GetValue();
        string sport = (*p_it).second.GetValue();
        string queue = (*q_it).second.GetValue();

        GetPage().AddTagMap("HOST",  new CHTMLText(host));
        GetPage().AddTagMap("PORT",  new CHTMLText(sport));
        GetPage().AddTagMap("QUEUE_NAME",  new CHTMLText(queue));

        unsigned int port = NStr::StringToUInt(sport);
        auto_ptr<CQueueInfo> queue_info(new CQueueInfo(queue,host,port));
        queue_info->CollectInfo();

        GetPage().AddTagMap("INFO",  new CHTMLText(queue_info->GetInfo()));

        m_TableRowHook.reset(new THookContext(queue_info.release(),*this));

        GetPage().AddTagMap("wn_table_row_hook", 
                            CreateTagMapper(RowHook<THookContext>,
                                            m_TableRowHook.get()));

    } catch (...) {

        node->AppendChild(new CHTMLText("Error"));
        //        Reply->AppendChild(CShowServersCommand::CreateView(ctx));
    }
    
    /*    pair<TCgiEntriesI,TCgiEntriesI> p = entries.equal_range("input");
    
    // print the values associated with input to the html page
    for(TCgiEntriesI i = p.first; i != p.second; ++i) {
        Reply->AppendChild(new CHTMLText(", "));
        Reply->AppendChild(new CHTMLPlainText(i->second));
    }
    Reply->AppendChild(new CHTML_br);*/
    //    Reply->AppendChild(CHelloBasicCommand::CreateView(ctx));
  
    return node.release();
}
void CShowServerStatCommand::Clean()
{
    CGridMgrCommand::Clean();
    m_TableRowHook.reset();
}


//
// CShowWNStatCommand
//

CShowWNStatCommand::CShowWNStatCommand( CGridMgrResource& resource )
  : CGridMgrCommand( resource )
{}

CShowWNStatCommand::~CShowWNStatCommand()
{}

CNcbiCommand* CShowWNStatCommand::Clone( void ) const
{
    return new CShowWNStatCommand( GetGridMgrResource() );
}

//
// GetName() returns the string used to match the value in a cgi request.
// e.g. for "?cmd=search", GetName() should return "search"
//

string CShowWNStatCommand::GetName( void ) const
{
    return string("wn_stat"); // set the value of this string in helloapi.cpp
}

//
//  CreateView() is the function that creates the html to replace
//  the <@VIEW@> tag in the HtmlBaseFile
//

CNCBINode* CShowWNStatCommand::CreateView(CCgiContext& ctx)
{
    auto_ptr <CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("WORKERNODE_QUEUE_VIEW"));

    // get the list of cgi request name, value pairs
    const TCgiEntries& entries = ctx.GetRequest().GetEntries();

    // look for ones where the name is "input" 
    
    TCgiEntriesCI h_it = entries.find("server");
    TCgiEntriesCI p_it = entries.find("port");
    try {
        if( h_it == entries.end() ||  p_it == entries.end())
            throw runtime_error("");
        
        string host = (*h_it).second.GetValue();
        string sport = (*p_it).second.GetValue();
        GetPage().AddTagMap("HOST",  new CHTMLText(host));
        GetPage().AddTagMap("PORT",  new CHTMLText(sport));

        unsigned int port = NStr::StringToUInt(sport);
        CWorkerNodeInfo wn_info(host,port);

        GetPage().AddTagMap("INFO",  new CHTMLText(wn_info.GetStatistics()));

    } catch (...) {

        node->AppendChild(new CHTMLText("Error"));
        //        Reply->AppendChild(CShowServersCommand::CreateView(ctx));
    }

    return node.release();
}

//END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/06/27 13:08:55  didenko
 * Made it compiling on Windows
 *
 * Revision 1.1  2005/06/27 12:52:40  didenko
 * Added grid manager cgi
 *
 * ===========================================================================
 */
