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
	Vsevolod Sandomirskiy
*
* File Description:
*   Sample to test OS-independent C++ exceptions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1998/11/02 22:10:15  sandomir
* CNcbiApplication added; netest sample updated
*
* Revision 1.1  1998/11/02 15:31:56  sandomir
* Portable exception classes
*
* ===========================================================================
*/

#include <ncbiapp.hpp>
#include <ncbiexcp.hpp>
#include <iostream>

#define _POSIX_C_SOURCE 199506L

#ifdef UNIX
#include <signal.h>
#elif WIN32
#include <windows.h>
#endif

class CMyApplication : public CNcbiApplication
{
public:

  CMyApplication( int argc, char* argv[] ) throw()
    : CNcbiApplication( argc, argv ) {}

  virtual void Exit( void ); // cleanup

  virtual int Run();

};

void CMyApplication::Exit()
{
  int x = 0;
  x = 1 / x;
}

int CMyApplication::Run()
{
  
  try {
    
#ifdef UNIX
    raise( SIGILL );
#elif WIN32    
    RaiseException( STATUS_ACCESS_VIOLATION, 0, 0, 0 );
#endif
    
  } catch( CNcbiOSException e ) {
    std::cout << e.what() << std::endl;
  } catch( std::runtime_error e ) {
    std::cout << e.what() << std::endl;
  } catch( std::exception e ) {
    std::cout << e.what() << std::endl;
  }
  
  std::cout << "try 1 OK" << std::endl;
  
  try {
    
#ifdef UNIX   
    raise( SIGFPE );
#elif WIN32    
    RaiseException( STATUS_BREAKPOINT, 0, 0, 0 );
#endif    

  } catch( CNcbiOSException e ) {
    std::cout << e.what() << std::endl;
  } catch( std::runtime_error e ) {
    std::cout << e.what() << std::endl;
  } catch( std::exception e ) {
    std::cout << e.what() << std::endl;
  }
  
  std::cout << "try 2 OK" << std::endl;
  
  try {
    
#ifdef UNIX     
    // this signal is blocked
    raise( SIGHUP );
#elif WIN32    
    RaiseException( STATUS_INTEGER_OVERFLOW, 0, 0, 0 );
#endif     
  } catch( CNcbiOSException e ) {
    std::cout << e.what() << std::endl;
  } catch( std::runtime_error e ) {
        std::cout << e.what() << std::endl;
  } catch( std::exception e ) {
    std::cout << e.what() << std::endl;
  }
  
  std::cout << "try 3 OK" << std::endl;
  
  std::cout << "Done" << std::endl;
  
  return 0;
}

int main( int argc, char* argv[] )
{

  CMyApplication app( argc, argv );
  int res = 1;
  
  try {
    app.Init();
    res = app.Run();
    app.Exit();
  } catch( CNcbiOSException e ) {
    std::cout << "Failed " << e.what() << std::endl;
    return res;
  } catch( ... ) {
    std::cout << "Failed" << std::endl;
    return res;
  }

  return res;
}

    
