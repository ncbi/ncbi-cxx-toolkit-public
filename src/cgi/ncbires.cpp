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

#include <ncbistd.hpp>
#include <ncbires.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

//
// class CNcbiResource
//

CNcbiResource::CNcbiResource( void )
{}

CNcbiResource::~CNcbiResource( void )
{}

void CNcbiResource::HandleRequest( const CCgiRequest& request )
{
  try {
    TCmdList::iterator it = find_if( m_cmd.begin(), m_cmd.end(), 
                                     CNcbiCommand::CFind( request ) );
    if( it == m_cmd.end() ) {
      // command not found
      throw runtime_error( "unknown command" );

    } else for( ; it != m_cmd.end(); it++ ) {
      auto_ptr<CNcbiCommand> cmd( (*it)->Clone() );
      cmd->Execute( request );
    } // else

  } catch( exception& e ) {
    auto_ptr<CNcbiCommand> cmd( GetDefaultCommand() );
    cmd->Execute( request );
  }
}

//
// class CNcbiCommand
//

CNcbiCommand::CNcbiCommand( const CNcbiResource& resource )
  : m_resource( resource )
{}

CNcbiCommand::~CNcbiCommand( void )
{}

bool CNcbiCommand::IsRequested( const CCgiRequest& request ) const
{
  const string value = GetName();
  
  TCgiEntries& entries = const_cast<TCgiEntries&>( request.GetEntries() );
  pair<TCgiEntriesI,TCgiEntriesI> it = entries.equal_range( 
                                          CNcbiCommand::GetEntry() );

  for( TCgiEntriesI itEntr = it.first; itEntr != it.second; itEntr++ ) {
    if( AStrEquiv( value, itEntr->second, PNocase() ) ) {
      return true;
    } // if
  } // for

  return false;
}

string CNcbiCommand::GetEntry() const
{
  return "cmd";
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

bool CNcbiDataObjectReport::IsRequested( const CCgiRequest& request ) const
{
  const string value = GetName();
  
  TCgiEntries& entries = const_cast<TCgiEntries&>( request.GetEntries() );
  pair<TCgiEntriesI,TCgiEntriesI> it = entries.equal_range( 
                                          CNcbiDataObjectReport::GetEntry() );

  for( TCgiEntriesI itEntr = it.first; itEntr != it.second; itEntr++ ) {
    if( AStrEquiv( value, itEntr->second, PNocase() ) ) {
      return true;
    } // if
  } // for

  return false;
}

string CNcbiDataObjectReport::GetEntry() const
{
  return "dopt";
}

END_NCBI_SCOPE
