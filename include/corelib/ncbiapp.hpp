#ifndef NCBIAPP__HPP
#define NCBIAPP__HPP

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
* Revision 1.12  1999/11/15 18:57:01  vakatov
* Added <memory> (for "auto_ptr<>" template)
*
* Revision 1.11  1999/11/15 15:53:27  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.10  1999/04/27 14:49:50  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.9  1998/12/28 17:56:25  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.8  1998/12/09 17:30:12  sandomir
* ncbicgi.hpp deleted from ncbiapp.hpp
*
* Revision 1.7  1998/12/09 16:49:56  sandomir
* CCgiApplication added
*
* Revision 1.6  1998/12/07 23:46:52  vakatov
* Merged with "cgiapp.hpp";  minor fixes
*
* Revision 1.5  1998/12/07 22:31:12  vakatov
* minor fixes
*
* Revision 1.4  1998/12/03 21:24:21  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.3  1998/12/01 19:12:36  lewisg
* added CCgiApplication
*
* Revision 1.2  1998/11/05 21:45:13  sandomir
* std:: deleted
*
* Revision 1.1  1998/11/02 22:10:12  sandomir
* CNcbiApplication added; netest sample updated
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////
// CNcbiApplication
//

class CNcbiRegistry;

class CNcbiApplication {
public:
    static CNcbiApplication* Instance(void); // Singleton method

    // (throw exception if not-only instance)
    CNcbiApplication(void);
    virtual ~CNcbiApplication(void);

    virtual void Init(void); // initialization
    virtual void Exit(void); // cleanup

    virtual int AppMain(int argc, char** argv);

    virtual int Run(void) = 0; // main loop

    const CNcbiRegistry& GetConfig(void) const;
    CNcbiRegistry& GetConfig(void);

protected:

    virtual CNcbiRegistry* LoadConfig(void);

private:

    CNcbiRegistry& x_GetConfig(void) const;    
    
protected: 

    // data members
    
    static CNcbiApplication* m_Instance;
    int    m_Argc;
    char** m_Argv;

private:

    auto_ptr<CNcbiRegistry> m_config;    
};

inline CNcbiRegistry& CNcbiApplication::x_GetConfig(void) const
{
    return *m_config;
}

inline const CNcbiRegistry& CNcbiApplication::GetConfig(void) const
{
    return x_GetConfig();
}

inline CNcbiRegistry& CNcbiApplication::GetConfig(void)
{
    return x_GetConfig();
}

END_NCBI_SCOPE

#endif // NCBIAPP__HPP


