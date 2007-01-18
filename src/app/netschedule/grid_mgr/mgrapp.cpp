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
 * Author:  Maxim Didenko.
 *
 *
 */

#include <ncbi_pch.hpp>
#include "mgrapp.hpp"
#include "mgrres.hpp"
#include "mgrcmd.hpp"
#include <cgi/cgictx.hpp>



USING_NCBI_SCOPE;

static CGridMgrApp theGridMgrApp;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return theGridMgrApp.AppMain(argc, argv);
}



BEGIN_NCBI_SCOPE

CNcbiResource* CGridMgrApp::LoadResource(void)
{ 
    auto_ptr<CGridMgrResource> resource(new CGridMgrResource( GetConfig() ));

    // add commands to the resource class
    resource->AddCommand( new CShowNSServersCommand(*resource) );
    resource->AddCommand( new CShowNCServersCommand(*resource) );
    resource->AddCommand( new CShowServerStatCommand(*resource) );
    resource->AddCommand( new CShowWNStatCommand(*resource) );
    resource->AddCommand( new CTestRWNCCommand(*resource) );
    resource->AddCommand( new CShowNCServerStatCommand(*resource) );
	
    return resource.release();
}


int CGridMgrApp::ProcessRequest(CCgiContext& ctx)
{
    // execute request
    ctx.GetResource().HandleRequest(ctx);
    return 0;
}

END_NCBI_SCOPE
