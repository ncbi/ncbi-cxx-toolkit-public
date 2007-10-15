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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>

#include <cgi/ncbires.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Cgi_Resourse

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
#if !defined(NCBI_COMPILER_GCC)  ||  NCBI_COMPILER_VERSION >= 300  ||  defined(_IO_THROW)
    } catch( IOS_BASE::failure& /* e */  ) {
        throw;
#endif
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
        ERR_POST_X(1, "CNcbiRelocateCommand::Execute error getting url");
        throw;
    }
}

END_NCBI_SCOPE
