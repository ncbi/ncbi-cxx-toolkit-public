#ifndef NCBI_CGI_APP__HPP
#define NCBI_CGI_APP__HPP

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
*   Basic CGI Application class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  1999/04/27 17:01:23  vakatov
* #include <ncbires.hpp> to provide CNcbiResource class definition
* for the "auto_ptr<CNcbiResource>" (required by "delete" under MSVC++)
*
* Revision 1.12  1999/04/27 14:49:46  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.11  1999/02/22 21:12:37  sandomir
* MsgRequest -> NcbiContext
*
* Revision 1.10  1998/12/28 23:28:59  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.9  1998/12/28 15:43:09  sandomir
* minor fixed in CgiApp and Resource
*
* Revision 1.8  1998/12/10 17:36:54  sandomir
* ncbires.cpp added
*
* Revision 1.7  1998/12/09 22:59:05  lewisg
* use new cgiapp class
*
* Revision 1.6  1998/12/09 17:27:44  sandomir
* tool should be changed to work with the new CCgiApplication
*
* Revision 1.5  1998/12/09 16:49:55  sandomir
* CCgiApplication added
*
* Revision 1.1  1998/12/03 21:24:21  sandomir
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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbicgi.hpp>
#include <corelib/ncbicgir.hpp>
#include <corelib/ncbires.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CNcbiRegistry;
class CCgiContext;
class CCgiServerContext;
class CCgiApplication;

//
// class CCgiApplication
//

class CCgiApplication : public CNcbiApplication
{
    typedef CNcbiApplication CParent;
public:

    static CCgiApplication* Instance(void); // Singleton method

    const CNcbiRegistry& GetConfig(void) const;
    CNcbiRegistry& GetConfig(void);
    
    // these methods will throw exception if no server context set
    const CNcbiResource& GetResource(void) const;
    CNcbiResource& GetResource(void);

    virtual void Init(void); // initialization
    virtual void Exit(void); // cleanup

    virtual int Run(void);
    virtual int ProcessRequest(CCgiContext& context) = 0;

    virtual CNcbiRegistry* LoadConfig(void);
    virtual CNcbiResource* LoadResource(void);
    virtual CCgiServerContext* LoadServerContext(CCgiContext& context);

private:
    CNcbiRegistry& x_GetConfig(void) const;
    CNcbiResource& x_GetResource(void) const;

    auto_ptr<CNcbiRegistry> m_config;
    auto_ptr<CNcbiResource> m_resource;

    friend class CCgiContext;
};


#include <corelib/cgiapp.inl>

END_NCBI_SCOPE

#endif // NCBI_CGI_APP__HPP
