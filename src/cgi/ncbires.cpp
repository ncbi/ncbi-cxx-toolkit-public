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
*   Basic Resource class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.31  2004/05/17 20:56:50  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.30  2002/04/10 17:04:39  vakatov
* CNcbiRelocateCommand::Execute() -- temp. un-fix:  do not set Status
*
* Revision 1.29  2002/03/19 00:41:48  vakatov
* CNcbiRelocateCommand::Execute() -- set status to 301 (Moved)
*
* Revision 1.28  2000/08/28 16:22:28  golikov
* some _TRACEs removed
*
* Revision 1.27  1999/10/01 14:21:41  golikov
* Now messages in context are html nodes
*
* Revision 1.26  1999/08/12 14:54:43  sandomir
* #include <algorithm> added
*
* Revision 1.25  1999/08/11 18:33:50  sandomir
* class CNcbiResource more logical (some functionality from CNcbiDbResource moved in CNcbiResource; CNcbiCommand get CNcbiResource in ctor
*
* Revision 1.24  1999/05/11 03:11:52  vakatov
* Moved the CGI API(along with the relevant tests) from "corelib/" to "cgi/"
*
* Revision 1.23  1999/05/06 20:33:44  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.22  1999/04/27 14:50:07  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.21  1999/04/14 17:28:59  vasilche
* Added parsing of CGI parameters from IMAGE input tag like "cmd.x=1&cmd.y=2"
* As a result special parameter is added with empty name: "=cmd"
*
* Revision 1.20  1999/04/13 14:26:01  vasilche
* Removed old _TRACE
*
* Revision 1.19  1999/03/17 18:59:51  vasilche
* Changed CNcbiQueryResult&Iterator.
*
* Revision 1.18  1999/03/15 19:58:24  vasilche
* Added CNcbiQueryResultIterator
*
* Revision 1.17  1999/03/10 21:20:23  sandomir
* Resource added to CNcbiContext
*
* Revision 1.16  1999/02/22 21:12:39  sandomir
* MsgRequest -> NcbiContext
*
* Revision 1.15  1999/01/27 16:46:23  sandomir
* minor change: PFindByName added
*
* Revision 1.14  1999/01/14 20:03:49  sandomir
* minor changes
*
* Revision 1.13  1999/01/12 17:06:37  sandomir
* GetLink changed
*
* Revision 1.12  1999/01/06 22:23:40  sandomir
* minor changes
*
* Revision 1.11  1999/01/05 21:03:01  sandomir
* GetEntry() changes
*
* Revision 1.10  1998/12/31 19:47:29  sandomir
* GetEntry() fixed
*
* Revision 1.8  1998/12/28 15:43:13  sandomir
* minor fixed in CgiApp and Resource
*
* Revision 1.7  1998/12/21 17:19:37  sandomir
* VC++ fixes in ncbistd; minor fixes in Resource
*
* Revision 1.6  1998/12/17 21:50:44  sandomir
* CNCBINode fixed in Resource; case insensitive string comparison
* predicate added
*
* Revision 1.5  1998/12/17 17:25:02  sandomir
* minor changes in Report
*
* Revision 1.4  1998/12/14 20:25:37  sandomir
* changed with Command handling
*
* Revision 1.3  1998/12/14 15:30:08  sandomir
* minor fixes in CNcbiApplication; command handling fixed
*
* Revision 1.2  1998/12/10 20:40:21  sandomir
* #include <algorithm> added in ncbires.cpp
*
* Revision 1.1  1998/12/10 17:36:55  sandomir
* ncbires.cpp added
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>

#include <cgi/ncbires.hpp>
#include <cgi/cgictx.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

//
// class CNcbiResource 
//

CNcbiResource::CNcbiResource( CNcbiRegistry& config )
    : m_config(config)
{
}

CNcbiResource::~CNcbiResource( void )
{
    DeleteElements( m_cmd );
}

const CNcbiRegistry& CNcbiResource:: GetConfig(void) const
{ 
  return m_config; 
}

CNcbiRegistry& CNcbiResource::GetConfig(void)
{ 
  return m_config; 
}

const CNcbiResPresentation* CNcbiResource::GetPresentation( void ) const
{ 
  return 0; 
}

const TCmdList& CNcbiResource::GetCmdList( void ) const
{ 
  return m_cmd; 
}

void CNcbiResource::AddCommand( CNcbiCommand* command )
{ 
  m_cmd.push_back( command ); 
}

void CNcbiResource::HandleRequest( CCgiContext& ctx )
{
    bool defCom = false;
	
	try {
	    TCmdList::iterator it = find_if( m_cmd.begin(), m_cmd.end(), 
										 PRequested<CNcbiCommand>( ctx ) );
    
		auto_ptr<CNcbiCommand> cmd( ( it == m_cmd.end() ) 
									? ( defCom = true, GetDefaultCommand() )
									: (*it)->Clone() );
		cmd->Execute( ctx );
		
    } catch( std::exception& e ) {
	    _TRACE( e.what() );
		ctx.PutMsg( string("Error handling request: ") + e.what() );
		if( !defCom ) {
		  auto_ptr<CNcbiCommand> cmd( GetDefaultCommand() );
		  cmd->Execute( ctx );
		}
    }
}

//
// class CNcbiCommand
//

CNcbiCommand::CNcbiCommand( CNcbiResource& resource )
    : m_resource( resource )
{
}

CNcbiCommand::~CNcbiCommand( void )
{
}

bool CNcbiCommand::IsRequested( const CCgiContext& ctx ) const
{ 
    const string value = GetName();
  
    TCgiEntries& entries =
        const_cast<TCgiEntries&>(ctx.GetRequest().GetEntries());

    pair<TCgiEntriesI,TCgiEntriesI> p = entries.equal_range( GetEntry() );
    for ( TCgiEntriesI itEntr = p.first; itEntr != p.second; ++itEntr ) {
        if( AStrEquiv( value, itEntr->second, PNocase() ) ) {
            return true;
        } // if
    } // for

    // if there is no 'cmd' entry
    // check the same for IMAGE value
    p = entries.equal_range( NcbiEmptyString );
    for ( TCgiEntriesI iti = p.first; iti != p.second; ++iti ) {
        if( AStrEquiv( value, iti->second, PNocase() ) ) {
            return true;
        } // if
    }
    
    return false;
}

//
// class CNcbiRelocateCommand
//

CNcbiRelocateCommand::CNcbiRelocateCommand( CNcbiResource& resource )
    : CNcbiCommand( resource )
{
    return;
}

CNcbiRelocateCommand::~CNcbiRelocateCommand( void )
{
    return;
}

void CNcbiRelocateCommand::Execute( CCgiContext& ctx )
{
    try {
        string url = GetLink(ctx);
        _TRACE("CNcbiRelocateCommand::Execute changing location to:" << url);
        // Theoretically, the status should be set, but...
        // Commented temporarily to avoid the redirection to go out of
        // NCBI and confuse some not-so-smart clients.
        // It can be restored later when (and if) the NCBI internal HTTP
        // servers are tuned to intercept the redirections and resolve these
        // internally.
        //
        //        ctx.GetResponse().SetStatus(301, "Moved");
        ctx.GetResponse().SetHeaderValue("Location", url);
        ctx.GetResponse().WriteHeader();
    }
    catch (exception&) {
        ERR_POST("CNcbiRelocateCommand::Execute error getting url");
        throw;
    }
}

END_NCBI_SCOPE
