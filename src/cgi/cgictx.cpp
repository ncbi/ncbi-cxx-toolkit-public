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
* Author: Eugene Vasilchenko
*
* File Description:
*   Definition CGI application class and its context class.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/04/27 14:50:04  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbires.hpp>
#include <corelib/cgictx.hpp>
#include <corelib/cgiapp.hpp>

BEGIN_NCBI_SCOPE

//
// class CCgiContext
//

CCgiContext::CCgiContext(CCgiApplication& app, int argc, char** argv)
    : m_app(app), m_request(argc, argv)
{
}

CNcbiRegistry& CCgiContext::x_GetConfig(void) const
{
    return m_app.x_GetConfig();
}

CNcbiResource& CCgiContext::x_GetResource(void) const
{
    return m_app.x_GetResource();
}

CCgiServerContext& CCgiContext::x_GetServCtx( void ) const
{ 
    CCgiServerContext* context = m_srvCtx.get();
    if ( !context ) {
        context = m_app.LoadServerContext(const_cast<CCgiContext&>(*this));
        if ( !context ) {
            ERR_POST("CCgiContext::GetServCtx: no server context set");
            throw runtime_error("no server context set");
        }
        const_cast<CCgiContext&>(*this).m_srvCtx.reset(context);
    }
    return *context;
}


END_NCBI_SCOPE
