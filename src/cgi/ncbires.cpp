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
* CNCBINode fixed in Resource; case insensitive string comparison predicate added
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

#include <corelib/ncbires.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

//
// class CNcbiResource 
//

CNcbiContext::CNcbiContext( CCgiRequest& request, 
                            CCgiResponse& response,
                            CCgiServerContext& srvCtx )
    : m_request( request ), m_response( response ), m_srvCtx( srvCtx )
{}

//
// class CNcbiResource 
//

CNcbiResource::CNcbiResource( void )
{}

CNcbiResource::~CNcbiResource( void )
{}

void CNcbiResource::HandleRequest( CNcbiContext& ctx )
{
  try {
    TCmdList::iterator it = find_if( m_cmd.begin(), m_cmd.end(), 
                                     PRequested<CNcbiCommand>( ctx ) );
    
    auto_ptr<CNcbiCommand> cmd( ( it == m_cmd.end() ) 
                                ? GetDefaultCommand()
                                : (*it)->Clone() );
    cmd->Execute( ctx );
    
  } catch( std::exception& e ) {
    _TRACE( e.what() );
    if( ctx.GetMsg().empty() ) {
      ctx.GetMsg().push_back( "Unknown error" );
    }
    auto_ptr<CNcbiCommand> cmd( GetDefaultCommand() );
    cmd->Execute( ctx );
  }
}

//
// class CNcbiCommand
//

CNcbiCommand::CNcbiCommand( CNcbiResource& resource )
  : m_resource( resource )
{}

CNcbiCommand::~CNcbiCommand( void )
{}

bool CNcbiCommand::IsRequested( const CNcbiContext& ctx ) const
{
  const string value = GetName();
  
  TCgiEntries& entries = const_cast<TCgiEntries&>( 
      ctx.GetRequest().GetEntries() );
  pair<TCgiEntriesI,TCgiEntriesI> p = entries.equal_range( GetEntry() );

  for( TCgiEntriesI itEntr = p.first; itEntr != p.second; itEntr++ ) {
    if( AStrEquiv( value, itEntr->second, PNocase() ) ) {
      return true;
    } // if
  } // for

  return false;
}

//
// class CNcbiDatabaseInfo
//

bool CNcbiDatabaseInfo::IsRequested( const CNcbiContext& ctx ) const
{  
  TCgiEntries& entries = const_cast<TCgiEntries&>(
      ctx.GetRequest().GetEntries() );
  pair<TCgiEntriesI,TCgiEntriesI> p = entries.equal_range( GetEntry() );
  
  for( TCgiEntriesI itEntr = p.first; itEntr != p.second; itEntr++ ) {
    if( CheckName( itEntr->second ) == true ) {
      return true;
    } // if
  } // for

  return false;
}

//
// class CNcbiDatabase
//

CNcbiDatabase::CNcbiDatabase( const CNcbiDatabaseInfo& dbinfo )
  : m_dbinfo( dbinfo )
{}

CNcbiDatabase::~CNcbiDatabase( void )
{}

//
// class CNcbiDatabaseReport
//

bool CNcbiDataObjectReport::IsRequested( const CNcbiContext& ctx ) const
{ 
  const string value = GetName();
  
  TCgiEntries& entries = const_cast<TCgiEntries&>( 
      ctx.GetRequest().GetEntries() );
  pair<TCgiEntriesI,TCgiEntriesI> p = entries.equal_range( GetEntry() );

  for( TCgiEntriesI itEntr = p.first; itEntr != p.second; itEntr++ ) {
    if( AStrEquiv( value, itEntr->second, PNocase() ) ) {
      return true;
    } // if
  } // for

  return false;
}

END_NCBI_SCOPE
