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
 * Author:  Lewis Geer, Vsevolod Sandomirskiy, etc.
 *
 * File Description:  This is the main section of the program.
 *
 */

#include <ncbi_pch.hpp>
#include "helloapp.hpp"
#include "hellores.hpp"
#include "hellocmd.hpp"
#include <cgi/cgictx.hpp>

// You don't need next header in the real application,
// It is here for the test purposes only.
#include <common/test_assert.h>



/////////////////////////////////
// MAIN
//

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CHelloApp().AppMain(argc, argv);
}



/////////////////////////////////
// CHelloApp::
//   -- implementation of LoadResource() and ProcessRequest() virtual methods
//


BEGIN_NCBI_SCOPE

CNcbiResource* CHelloApp::LoadResource(void)
{ 
    auto_ptr<CHelloResource> resource(new CHelloResource( GetConfig() ));  

    // add commands to the resource class
    resource->AddCommand( new CHelloBasicCommand(*resource) );
    resource->AddCommand( new CHelloReplyCommand(*resource) );
    
    return resource.release();
}


int CHelloApp::ProcessRequest(CCgiContext& ctx)
{
    // execute request
    ctx.GetResource().HandleRequest(ctx);
    return 0;
}


END_NCBI_SCOPE
