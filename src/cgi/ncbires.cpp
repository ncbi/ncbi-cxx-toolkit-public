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
      CNcbiCommand* cmd = (*it)->Clone();
      cmd->Execute( request );
      delete cmd;
    } // else

  } catch( exception& e ) {
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

CNcbiDataObjectReport::CNcbiDataObjectReport( const CNcbiDataObject& obj )
  : m_obj( obj )
{}

CNcbiDataObjectReport::~CNcbiDataObjectReport( void )
{}

END_NCBI_SCOPE
