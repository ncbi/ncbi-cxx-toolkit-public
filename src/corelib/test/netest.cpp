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
* Revision 1.5  1998/12/03 21:24:25  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.4  1998/11/13 00:26:31  vakatov
* Incorporated into "ncbiexpt.[ch]pp"
*
* Revision 1.3  1998/11/05 21:45:15  sandomir
* std:: deleted
*
* Revision 1.2  1998/11/02 22:10:15  sandomir
* CNcbiApplication added; netest sample updated
*
* Revision 1.1  1998/11/02 15:31:56  sandomir
* Portable exception classes
*
* ===========================================================================
*/

#define __EXTENSIONS__
#define _POSIX_C_SOURCE 199506L

#ifdef UNIX
#include <signal.h>
#elif WIN32
#include <windows.h>
#endif

using namespace std;

#include <ncbiapp.hpp>
#include <ncbiexcp.hpp>

#include <vector>

class CMyApplication : public CNcbiApplication
{
public:

  CMyApplication( int argc, char* argv[] ) throw()
    : CNcbiApplication( argc, argv ) {}

  virtual void Exit( void ); // cleanup

  virtual int Run();

  void fun() throw()
    { throw "test"; }

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
    //abort();
#elif WIN32    
    RaiseException( STATUS_ACCESS_VIOLATION, 0, 0, 0 );
#endif
    
  } catch( CNcbiOSException& e ) {
    cout << e.what() << endl;
  } catch( runtime_error e ) {
    cout << e.what() << endl;
  } catch( exception& e ) {
    cout << e.what() << endl;
  }
  
  cout << "try 1 OK" << endl;
  
  try {
    
#ifdef UNIX   
    raise( SIGFPE );
#elif WIN32    
    RaiseException( STATUS_BREAKPOINT, 0, 0, 0 );
#endif    

  } catch( CNcbiOSException& e ) {
    cout << e.what() << endl;
  } catch( runtime_error e ) {
    cout << e.what() << endl;
  } catch( exception& e ) {
    cout << e.what() << endl;
  }
  
  cout << "try 2 OK" << endl;
  
  try {
    
#ifdef UNIX     
    // this signal is blocked
    raise( SIGHUP );
#elif WIN32    
    RaiseException( STATUS_INTEGER_OVERFLOW, 0, 0, 0 );
#endif     
  } catch( CNcbiOSException& e ) {
    cout << e.what() << endl;
  } catch( runtime_error e ) {
        cout << e.what() << endl;
  } catch( exception& e ) {
    cout << e.what() << endl;
  }
  
  cout << "try 3 OK" << endl;
  
  cout << "Done" << endl;
  
  return 0;
}

template<class T>
class Test
{
public:

  static T* Get()
    { cout << "Test generic\n"; return new T; }
};

template<> 
class Test<int>
{
public:

  static int* Get()
    { cout << "Test int\n"; return new int; }
};

template<class T> T* GetF(T* t)
{ return new T; }

struct B
{};


int main( int argc, char* argv[] )
{

  CMyApplication app( argc, argv );
  int res = 1;

  cout << "Start" << endl;

  vector<B> v;
  
  try {
    app.Init();

    cout << "before call\n";
    int *ii = 0;
    *ii = 7;
    cout << "after call\n";

    res = app.Run();

    //CMyApplication* a( 0 , NULL );
    //cout << "before fun\n";
    //a.fun();
    //cout << "after fun\n";

    app.Exit();
  } catch( CNcbiOSException& e ) {
    cout << "Failed " << e.what() << endl;
    return res;
  } catch( bad_exception& ) {
      cout << "Failed" << endl;
      return res;
  } catch( ... ) {
      cout << "Failed" << endl;
      return res;
  }
  
  return res;
}
  
