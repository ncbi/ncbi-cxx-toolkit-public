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

#include <connect/services/blob_storage_netcache.hpp>

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
    auto_ptr<CNCBINode> node;
    try{

        const CNcbiRegistry& reg = ctx.GetConfig();
 
        // load in the html template file
        string baseFile = reg.Get( "filesystem", "HtmlBaseFile" );
        m_Page.reset( new CHTMLPage( NcbiEmptyString, baseFile ) );

        string incFile = reg.Get( "filesystem", "HtmlIncFile" );
        m_Page->LoadTemplateLibFile(incFile);
   
        // set up to replace <@VIEW@> in template file with html returned
        // from CreateView
        node.reset(CreateView( ctx ));

    } catch (exception& ex) {
        node.reset(CreateErrorNode());
        GetPage().AddTagMap("error", new CHTMLPlainText(ex.what()));
    } catch (...) {
        node.reset(CreateErrorNode());
        GetPage().AddTagMap("error", new CHTMLPlainText("Unknown Error"));
    }

    m_Page->AddTagMap( "VIEW", node.release() );
    m_Page->AddTagMap("SELF_URL",  new CHTMLText(ctx.GetSelfURL()));

    // actual page output
    ctx.GetResponse().WriteHeader();
    m_Page->Print(ctx.GetResponse().out(), CNCBINode::eHTML );
}

void CGridMgrCommand::Clean()
{
    m_Page.reset();
}

CNCBINode* CGridMgrCommand::CreateErrorNode()
{
    auto_ptr<CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("ERROR_VIEW"));
    return node.release();
}


//
// CStartPageCommand
// 


CStartPageCommand::CStartPageCommand(CGridMgrResource& resource)
  : CGridMgrCommand( resource )
{
}

CStartPageCommand::~CStartPageCommand()
{
}

CNcbiCommand* CStartPageCommand::Clone(void) const
{
    return new CStartPageCommand( const_cast<CGridMgrResource&>(GetGridMgrResource()) );
}

string CStartPageCommand::GetName( void ) const
{
    return string("startpage");
}

CNCBINode* CStartPageCommand::CreateView(CCgiContext& ctx)
{
    auto_ptr <CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("START_PAGE_MENU"));
	
	CHTMLPage& page = GetPage();
	
	page.AddTagMap("nc_url_label", new CHTMLText("NetCache"));
	page.AddTagMap("ns_url_label", new CHTMLText("NetShedule"));
	
    return node.release();
}



//
// CShowNSServersCommand
//

CShowNSServersCommand::CShowNSServersCommand( CGridMgrResource& resource )
  : CGridMgrCommand( resource )
{}

CShowNSServersCommand::~CShowNSServersCommand()
{}

CNcbiCommand* CShowNSServersCommand::Clone( void ) const
{
    return new CShowNSServersCommand( const_cast<CGridMgrResource&>(GetGridMgrResource()) );
}

string CShowNSServersCommand::GetName( void ) const
{
    return string("showserver"); // set the value of this string in helloapi.cpp
}



//
//  CreateView() is the function that creates the html to replace
//  the <@VIEW@> tag in the HtmlBaseFile
//


CNCBINode* CShowNSServersCommand::Render(const CQueueInfo& info, 
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

CNCBINode* CShowNSServersCommand::Render(const CHostInfo& info, 
                                       CHTMLPage& page, long row_number)
{
    auto_ptr<CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("srv_table_row_template"));

    if (info.IsActive()) {
        AutoPtr<THookCtxHost> 
            host_hook(new THookCtxHost(info,*this));

        page.AddTagMap("queue_href_hook", 
                       CreateTagMapper<CHTMLPage,THookCtxHost*>
                       (RowHook<THookCtxHost>, host_hook.get()));

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

CNCBINode* CShowNSServersCommand::Render(const CServiceInfo& info, 
                                       CHTMLPage& page, long row_number)
{
    auto_ptr<CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("srvs_table_row_template"));

    page.AddTagMap("SERVICE_NAME",  new CHTMLText(info.GetName()));

    AutoPtr<THookCtxSrv> 
        srv_hook(new THookCtxSrv(info,*this));

    page.AddTagMap("srv_table_row_hook", 
                   CreateTagMapper<CHTMLPage,THookCtxSrv*>
                   (RowHook<THookCtxSrv>, srv_hook.get()));
    
    m_SvrHooks.push_back(srv_hook);
    return node.release();

}


CNCBINode* CShowNSServersCommand::CreateView(CCgiContext& ctx)
{
    auto_ptr <CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("SERVICES_VIEW"));

    const IRegistry& reg = ctx.GetConfig();
    
    string lbsurl = reg.GetString("urls", "lbsurl", "");

    m_TableRowHook.reset(new THookCtxSrvs(new CNSServices(lbsurl),*this));

    GetPage().AddTagMap("srvs_table_row_hook", 
                        CreateTagMapper<CHTMLPage,THookCtxSrvs*>
                        (RowHook<THookCtxSrvs>, m_TableRowHook.get()));
    return node.release();
}

void CShowNSServersCommand::Clean()
{
    CGridMgrCommand::Clean();
    m_TableRowHook.reset();
    m_SvrHooks.clear();
    m_HostHooks.clear();
}


//
// CShowNCServersCommand
//

CShowNCServersCommand::CShowNCServersCommand(CGridMgrResource& resource)
  : CGridMgrCommand( resource )
{}

CShowNCServersCommand::~CShowNCServersCommand()
{}

CNcbiCommand* CShowNCServersCommand::Clone( void ) const
{
    return new CShowNCServersCommand(const_cast<CGridMgrResource&>(GetGridMgrResource()));
}

string CShowNCServersCommand::GetName( void ) const
{
    return string("showncserver");
}

CNCBINode* CShowNCServersCommand::CreateView(CCgiContext& ctx)
{
    auto_ptr <CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("NC_SERVICES_VIEW"));

    const IRegistry& reg = ctx.GetConfig();
    
    string url = reg.GetString("urls", "lb_nc_url", "");
	
	auto_ptr<CNSServices> nsserv(new CNSServices(url));
    m_TableRowHook.reset(new THookCtxSrvs(nsserv.release(),*this));

    GetPage().AddTagMap("srvs_table_row_hook", 
                        CreateTagMapper<CHTMLPage,THookCtxSrvs*>
                        (RowHook<THookCtxSrvs>, m_TableRowHook.get()));
						
    return node.release();
}

CNCBINode* CShowNCServersCommand::Render(const CServiceInfo& info, 
                                         CHTMLPage&          page, 
										 long                row_number)
{
    auto_ptr<CNCBINode> node(new CNCBINode);
	
    node->AppendChild(new CHTMLTagNode("nc_srvs_table_row_template"));
    page.AddTagMap("service",new CHTMLText(info.GetName()));
	
	CServiceInfo::const_iterator it = info.begin();
	CServiceInfo::const_iterator it_end = info.end();

	string host_port;	
	for (;it != it_end; ++it) {
		string addr = (*it)->GetHost();
		unsigned port = (*it)->GetPort();
		
		addr.append(":");
		addr.append(NStr::IntToString(port));
		
		string url = "<a href=\"<@SELF_URL@>?cmd=ncserverstat&server=";
		url.append(addr);
		url.append("\">");
		url.append(addr);
		url.append("</a>");
		
		host_port.append(url);
		host_port.append(" ");
	}
    page.AddTagMap("host_port",new CHTMLText(host_port));
	
	return node.release();
}


void CShowNCServersCommand::Clean()
{
    CGridMgrCommand::Clean();
    m_TableRowHook.reset();
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
    
    if( h_it == entries.end() ||  p_it == entries.end() 
        || q_it == entries.end())
        throw runtime_error("Server, port or queue is not specified");
        
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
                        CreateTagMapper<CHTMLPage,THookContext*>
                        (RowHook<THookContext>, m_TableRowHook.get()));
    
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

    if( h_it == entries.end() ||  p_it == entries.end())
        throw runtime_error("Server or port is not spesified");
        
    string host = (*h_it).second.GetValue();
    string sport = (*p_it).second.GetValue();
    GetPage().AddTagMap("HOST",  new CHTMLText(host));
    GetPage().AddTagMap("PORT",  new CHTMLText(sport));
    
    unsigned int port = NStr::StringToUInt(sport);
    CWorkerNodeInfo wn_info(host,port);

    GetPage().AddTagMap("INFO",  new CHTMLText(wn_info.GetStatistics()));

    return node.release();
}

//
// CTestRWNCCommand
//

CShowNCServerStatCommand::CShowNCServerStatCommand(CGridMgrResource& resource)
  : CGridMgrCommand(resource)
{}

CShowNCServerStatCommand::~CShowNCServerStatCommand()
{}

CNcbiCommand* CShowNCServerStatCommand::Clone(void) const
{
	return new CShowNCServerStatCommand(const_cast<CGridMgrResource&>(GetGridMgrResource()));
}

string CShowNCServerStatCommand::GetName( void ) const
{
	return "ncserverstat";
}

CNCBINode* CShowNCServerStatCommand::CreateView(CCgiContext& ctx)
{
    // get the list of cgi request name, value pairs
    const TCgiEntries& entries = ctx.GetRequest().GetEntries();

    // look for ones where the name is "input" 
    TCgiEntriesCI it = entries.find("server");
    if (it == entries.end()) {
        throw runtime_error("Server or port is not specified");
	}
	
	string server = (*it).second.GetValue();
	string host, port_str;
	NStr::SplitInTwo(server, ":", host, port_str);
	unsigned port = NStr::StringToUInt(port_str);
	
	if (host.empty() || port == 0) {
        throw runtime_error("Server or port is not specified");
	}
	

    auto_ptr <CNCBINode> node(new CNCBINode);
    node->AppendChild(new CHTMLTagNode("NC_STAT_VIEW"));


    GetPage().AddTagMap("HOST",  new CHTMLText(host));
    GetPage().AddTagMap("PORT",  new CHTMLText(port_str));

    CNetCacheStatInfo nc_info(host,port);

    GetPage().AddTagMap("VERSION", new CHTMLText(nc_info.GetVersion()));
    GetPage().AddTagMap("INFO", new CHTMLText(nc_info.GetStatistics()));
	
    return node.release();
}

//
// CTestRWNCCommand
//

CTestRWNCCommand::CTestRWNCCommand( CGridMgrResource& resource )
  : CGridMgrCommand( resource )
{}

CTestRWNCCommand::~CTestRWNCCommand()
{}

CNcbiCommand* CTestRWNCCommand::Clone( void ) const
{
    return new CTestRWNCCommand( GetGridMgrResource() );
}

//
// GetName() returns the string used to match the value in a cgi request.
// e.g. for "?cmd=search", GetName() should return "search"
//

string CTestRWNCCommand::GetName( void ) const
{
    return string("nc_rw_test"); // set the value of this string in helloapi.cpp
}

//
//  CreateView() is the function that creates the html to replace
//  the <@VIEW@> tag in the HtmlBaseFile
//

CNCBINode* CTestRWNCCommand::CreateView(CCgiContext& ctx)
{
    auto_ptr <CNCBINode> node(CreateErrorNode());

    const TCgiEntries& entries = ctx.GetRequest().GetEntries();

    TCgiEntriesCI it = entries.find("cache");
    string cache_val = "false";
    CBlobStorage_NetCache::TCacheFlags flags = 0;
    if( it != entries.end() ) {
        cache_val = (*it).second.GetValue();
        if (cache_val == "true")
            flags = CBlobStorage_NetCache::eCacheBoth;
    }

    it = entries.find("type");
    string type = "write";
    if( it != entries.end() )
        type = (*it).second.GetValue();
        
    string blob_id, msg;
    string temp_dir = "/tmp";
    it = entries.find("temp_dir");
    if( it != entries.end() )
        temp_dir = (*it).second.GetValue();
    
    
    CBlobStorage_NetCache storage( new CNetCacheAPI("NC_test", "TestDmaxCl"), 
                                   flags,
                                   temp_dir);

    if (type == "write") {
        CNcbiOstream& os = storage.CreateOStream(blob_id);
        os << "Test blob";
        storage.Reset();
        
        msg = "<br/><a href=\"<@SELF_UTL@>?cmd=nc_rw_test&type=read&cache=" + cache_val + 
            "&blob_id=" + blob_id +"\">" 
            + blob_id + "</a><br/>cache is ";
        msg +=  flags ? "on" : "off";
    }
    else {
        TCgiEntriesCI it = entries.find("blob_id");
        if( it == entries.end() ) {
            msg = "blob_id is empty";
        }
        else {
            blob_id = (*it).second.GetValue();
        
            msg = "<br/>blob_id: " + blob_id + "<br/>cache is ";
            msg +=  flags ? "on<br/>" : "off<br/>";
            CNcbiIstream& is = storage.GetIStream(blob_id);
            char buf[1024];
            while ( !is.eof() ) {
                    is.read(buf,sizeof(buf));
                    msg.append(buf, is.gcount());
            }
        }
    }
    GetPage().AddTagMap("error", new CHTMLText(msg));
 
    return node.release();
}

//END_NCBI_SCOPE
