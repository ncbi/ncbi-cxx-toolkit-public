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
*   Basic Application class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  1998/12/03 21:24:23  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.3  1998/12/01 19:12:08  lewisg
* added CCgiApplication
*
* Revision 1.2  1998/11/05 21:45:14  sandomir
* std:: deleted
*
* Revision 1.1  1998/11/02 22:10:13  sandomir
* CNcbiApplication added; netest sample updated
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <ncbiapp.hpp>
#include <cgiapp.hpp>

#include <string.h>


BEGIN_NCBI_SCOPE

CNcbiApplication* CNcbiApplication::m_instance;

//
// class CNcbiApplication
//

CNcbiApplication* CNcbiApplication::Instance( void )
{ 
  return m_instance; 
}

CNcbiApplication::CNcbiApplication( int argc /* = 0 */, 
                                    char* argv[] /* = 0 */ ) 
  : m_argc( argc ), m_argv( argv )
{
  if( m_instance ) {
    throw logic_error( "CNcbiApplication::CNcbiApplication: "
                       "cannot create second instance" );
  }
}

CNcbiApplication::~CNcbiApplication( void )
{
  m_instance = 0;
}

void CNcbiApplication::Init( void )
{
  // exceptions not used for now
  // CNcbiOSException::SetDefHandler();
  // set_unexpected( CNcbiOSException::UnexpectedHandler ); 
  return;
}

void CNcbiApplication::Exit( void )
{
  return;
}

int CNcbiApplication::Run( void )
{
  return 0;
}
    
//
// CCgiApplication
//

CCgiApplication* CCgiApplication::Instance( void )
{ 
  return static_cast< CCgiApplication* >( CNcbiApplication::Instance() ); 
}

CCgiApplication::CCgiApplication( int argc /* = 0 */, 
                                  char* argv[] /* = 0 */ )
  : CNcbiApplication(argc, argv),
    m_CgiRequest( 0 )
{}

CCgiApplication::~CCgiApplication( void )
{}

void CCgiApplication::Init( void )
{
  CNcbiApplication::Init();
  m_CgiRequest = new CCgiRequest( m_argc, m_argv );
}

void CCgiApplication::Exit( void )
{
  CNcbiApplication::Exit();
  delete m_CgiRequest;
}


END_NCBI_SCOPE
