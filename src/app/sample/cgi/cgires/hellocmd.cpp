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
 * Author:  Lewis Geer, Vsevolod Sandomirskiy, etc.
 *
 * File Description:  
 *   These are the commands triggered by cgi parameters.
 *   These command classes construct the html pages returned to the user.
 *
 */

#include <ncbi_pch.hpp>
#include "hellores.hpp"
#include "hellocmd.hpp"

#include <corelib/ncbistd.hpp>
#include <cgi/cgictx.hpp>
#include <corelib/ncbireg.hpp>
#include <html/page.hpp>

#include <memory>


BEGIN_NCBI_SCOPE

CHelloCommand::CHelloCommand( CNcbiResource& resource )
  : CNcbiCommand( resource )
{}

CHelloCommand::~CHelloCommand()
{}

//
// GetEntry() returns the string used to match the name in a cgi request.
// e.g. for "?cmd=search", GetEntry should return "cmd"
//

string CHelloCommand::GetEntry() const
{
    return string("cmd");  // set the value of this string in helloapi.cpp
}

void CHelloCommand::Execute( CCgiContext& ctx )
{
    const CNcbiRegistry& reg = ctx.GetConfig();
    
    // load in the html template file
    string baseFile = reg.Get( "filesystem", "HtmlBaseFile" );
	auto_ptr<CHTMLPage> page( new CHTMLPage( NcbiEmptyString, baseFile ) );
    
    // set up to replace <@VIEW@> in template file with html returned
    // from CreateView
	page->AddTagMap( "VIEW", CreateView( ctx ) );

	// actual page output
    ctx.GetResponse().WriteHeader();
    page->Print(ctx.GetResponse().out(), CNCBINode::eHTML );
}


//
// CHelloBasicCommand
//

CHelloBasicCommand::CHelloBasicCommand( CNcbiResource& resource )
  : CHelloCommand( resource )
{}

CHelloBasicCommand::~CHelloBasicCommand( void )
{}

CNcbiCommand* CHelloBasicCommand::Clone( void ) const
{
    return new CHelloBasicCommand( GetHelloResource() );
}

//
// GetName() returns the string used to match the value in a cgi request.
// e.g. for "?cmd=search", GetName() should return "search"
//

string CHelloBasicCommand::GetName( void ) const
{
    return string("init"); // set the value of this string in helloapi.cpp
}


//
//  CreateView() is the function that creates the html to replace
//  the <@VIEW@> tag in the HtmlBaseFile
//

CNCBINode* CHelloBasicCommand::CreateView( CCgiContext& ctx)
{
    const CNcbiRegistry& reg = ctx.GetConfig();
     
    auto_ptr <CHTML_form> Form(new CHTML_form(reg.Get("html", "URL")));    
                                 
    Form->AppendChild(new CHTML_text("input", 40));
    Form->AddHidden(GetEntry(), "reply");
    Form->AppendChild(new CHTML_submit("Go","submit"));

    return Form.release();
}

//
// CHelloReplyCommand
//

CHelloReplyCommand::CHelloReplyCommand( CNcbiResource& resource )
  : CHelloBasicCommand( resource )
{}

CHelloReplyCommand::~CHelloReplyCommand( void )
{}

CNcbiCommand* CHelloReplyCommand::Clone( void ) const
{
    return new CHelloReplyCommand( GetHelloResource() );
}

//
// GetName() returns the string used to match the value in a cgi request.
// e.g. for "?cmd=search", GetName() should return "search"
//

string CHelloReplyCommand::GetName( void ) const
{
    return string("reply"); // set the value of this string in helloapi.cpp
}


//
//  CreateView() is the function that creates the html to replace
//  the <@VIEW@> tag in the HtmlBaseFile
//

CNCBINode* CHelloReplyCommand::CreateView( CCgiContext& ctx)
{
    auto_ptr <CHTMLText> Reply(new CHTMLText("Hello"));
  
    // get the list of cgi request name, value pairs
    TCgiEntries& entries = const_cast<TCgiEntries&>
        (ctx.GetRequest().GetEntries());

    // look for ones where the name is "input" 
    pair<TCgiEntriesI,TCgiEntriesI> p = entries.equal_range("input");
    
    // print the values associated with input to the html page
    for(TCgiEntriesI i = p.first; i != p.second; ++i) {
        Reply->AppendChild(new CHTMLText(", "));
        Reply->AppendChild(new CHTMLPlainText(i->second));
    }
    Reply->AppendChild(new CHTML_br);
    Reply->AppendChild(CHelloBasicCommand::CreateView(ctx));
  
    return Reply.release();
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/05/04 14:44:21  kuznets
 * MOving from the root "hello" to the new location
 *
 * Revision 1.5  2002/04/16 18:50:30  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.4  1999/11/10 20:11:00  lewisg
 * clean out unnecessary code
 *
 * Revision 1.3  1999/11/10 01:01:06  lewisg
 * get rid of namespace
 *
 * Revision 1.2  1999/10/28 20:08:24  lewisg
 * add commands and  comments
 *
 * Revision 1.1  1999/10/25 21:15:54  lewisg
 * first draft of simple cgi app
 *
 * ===========================================================================
 */
