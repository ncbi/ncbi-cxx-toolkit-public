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
* Revision 1.1  1998/11/02 15:31:56  sandomir
* Portable exception classes
*
* ===========================================================================
*/

#include <ncbiexcp.hpp>
#include <iostream>

#define _POSIX_C_SOURCE 199506L

#ifdef UNIX
#include <signal.h>
#elif WIN32
#include <windows.h>
#endif

int main()
{
  try {
    
    CNcbiOSException::SetDefHandler();
    
#ifdef UNIX
    raise( SIGILL );
#elif WIN32    
    RaiseException( STATUS_ACCESS_VIOLATION, 0, 0, 0 );
#endif
    
  } catch( CNcbiMemException e ) {
    std::cout << e.what() << std::endl;
  } catch( CNcbiFPEException e ) {
    std::cout << e.what() << std::endl;
  } catch( CNcbiSystemException e ) {
    std::cout << e.what() << std::endl;
  } catch( CNcbiOSException e ) {
    std::cout << e.what() << std::endl;
  } catch( std::runtime_error e ) {
    std::cout << e.what() << std::endl;
  } catch( std::exception e ) {
    std::cout << e.what() << std::endl;
  }

try {
     
#ifdef UNIX   
    raise( SIGFPE );
#elif WIN32    
    RaiseException( STATUS_BREAKPOINT, 0, 0, 0 );
#endif    
  } catch( CNcbiMemException e ) {
    std::cout << e.what() << std::endl;
  } catch( CNcbiFPEException e ) {
    std::cout << e.what() << std::endl;
  } catch( CNcbiSystemException e ) {
    std::cout << e.what() << std::endl;
  } catch( CNcbiOSException e ) {
    std::cout << e.what() << std::endl;
  } catch( std::runtime_error e ) {
    std::cout << e.what() << std::endl;
  } catch( std::exception e ) {
    std::cout << e.what() << std::endl;
  }

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

  std::cout << "Done" << std::endl;

  return 0;
}

    
