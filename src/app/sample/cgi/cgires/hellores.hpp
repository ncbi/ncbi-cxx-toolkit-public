#ifndef NCBI_HELLORES__HPP
#define NCBI_HELLORES__HPP

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
* File Description:  Defines the resource class, whose main purpose is to 
*   aggregate (or hold) classes used to execute the program.  Classes
*   aggregated include:
*
*   CNcbiRegistry: opens the *.ini file and reads the entries contained
*       within.
*
*   CNcbiCommand: classes executed when particular CGI parameters are
*       received.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/05/04 14:44:21  kuznets
* MOving from the root "hello" to the new location
*
* Revision 1.2  1999/11/10 01:01:48  lewisg
* get rid of namespaces
*
* Revision 1.1  1999/10/25 21:19:31  lewisg
* first draft of simple cgi app
*
*
* ===========================================================================
*/

#include <cgi/ncbires.hpp>

BEGIN_NCBI_SCOPE 

//
// class CHelloResource
//

class CHelloResource : public CNcbiResource    // see ncbires.hpp
{
public:

    CHelloResource( CNcbiRegistry& config );
    virtual ~CHelloResource();   
   
    // define the command to be executed when no other command matches
   
    virtual CNcbiCommand* GetDefaultCommand( void ) const;
    
};

END_NCBI_SCOPE

#endif /* NCBI_HELLORES__HPP */
