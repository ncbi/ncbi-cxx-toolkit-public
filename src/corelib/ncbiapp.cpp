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
*   CNcbiApplication -- a generic NCBI application class
*   CCgiApplication  -- a NCBI CGI-application class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  1999/04/30 19:21:03  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.15  1999/04/27 14:50:06  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.14  1999/02/22 21:12:38  sandomir
* MsgRequest -> NcbiContext
*
* Revision 1.13  1998/12/28 23:29:06  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.12  1998/12/28 15:43:12  sandomir
* minor fixed in CgiApp and Resource
*
* Revision 1.11  1998/12/14 15:30:07  sandomir
* minor fixes in CNcbiApplication; command handling fixed
*
* Revision 1.10  1998/12/09 22:59:35  lewisg
* use new cgiapp class
*
* Revision 1.9  1998/12/09 19:24:36  vakatov
* typo fixed
*
* Revision 1.8  1998/12/09 17:45:37  sandomir
* *** empty log message ***
*
* Revision 1.7  1998/12/09 16:49:56  sandomir
* CCgiApplication added
*
* Revision 1.6  1998/12/07 23:47:05  vakatov
* minor fixes
*
* Revision 1.5  1998/12/07 22:31:14  vakatov
* minor fixes
*
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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
// CNcbiApplication
//

CNcbiApplication* CNcbiApplication::m_Instance;


CNcbiApplication* CNcbiApplication::Instance( void )
{ 
    return m_Instance; 
}

CNcbiApplication::CNcbiApplication(void) 
{
    if( m_Instance ) {
        throw logic_error("CNcbiApplication::CNcbiApplication: "
                          "cannot create second instance");
    }
    m_Instance = this;
}

CNcbiApplication::~CNcbiApplication(void)
{
    m_Instance = 0;
}

void CNcbiApplication::Init(void)
{
    // exceptions not used for now
    // CNcbiOSException::SetDefHandler();
    // set_unexpected( CNcbiOSException::UnexpectedHandler ); 
    return;
}

void CNcbiApplication::Exit(void)
{
    return;
}

int CNcbiApplication::AppMain(int argc, char** argv)
{
    m_Argc = argc;
    m_Argv = argv;
    // init application
    try {
        Init();
    }
    catch (exception e) {
        ERR_POST(eDiag_Error, "CCgiApplication::Init() failed: " << e.what());
        return -1;
    }

    // run application
    int res;
    try {
        res = Run();
    }
    catch (exception e) {
        ERR_POST(eDiag_Error, "CCgiApplication::Run() failed: " << e.what());
        res = -1;
    }

    // close application
    try {
        Exit();
    }
    catch (exception e) {
        ERR_POST(eDiag_Error, "CCgiApplication::Exit() failed: " << e.what());
    }

    return res;
}

//
// Simple Cgi App
//
/*

CSimpleCgiApp::CSimpleCgiApp(int argc, char ** argv, CNcbiIstream * istr,
		       bool indexes_as_entries):
    CCgiApplication(argc, argv, istr, indexes_as_entries)
{}

void CSimpleCgiApp::Init(void)
{
  CCgiApplication::Init();
  m_CgiRequest = GetRequest();
  m_CgiEntries = m_CgiRequest->GetEntries();
}
*/

END_NCBI_SCOPE
