#ifndef NCBI_HELLOCMD__HPP
#define NCBI_HELLOCMD__HPP

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
* File Description:  These are the commands triggered by cgi parameters.
*   These command classes construct the html pages returned to the user.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/05/04 14:44:21  kuznets
* MOving from the root "hello" to the new location
*
* Revision 1.5  2000/03/20 22:48:18  vakatov
* Get rid of the "unused var" warning on compilation
*
* Revision 1.4  1999/11/10 20:10:57  lewisg
* clean out unnecessary code
*
* Revision 1.3  1999/11/10 01:01:48  lewisg
* get rid of namespaces
*
* Revision 1.2  1999/10/28 20:08:29  lewisg
* add commands and  comments
*
* Revision 1.1  1999/10/25 21:19:31  lewisg
* first draft of simple cgi app
*
*
* ===========================================================================
*/

#include "hellores.hpp"
#include <cgi/ncbires.hpp>

BEGIN_NCBI_SCOPE

//
// CHelloCommand
// 

// Abstract base class for hello command classes

class CHelloCommand : public CNcbiCommand
{
public:
  
    CHelloCommand( CNcbiResource& resource );
    virtual ~CHelloCommand( void );

    virtual void Execute( CCgiContext& ctx );
    virtual string GetLink(CCgiContext&) const 
        { return NcbiEmptyString; }

    virtual CNCBINode* CreateView( CCgiContext& ctx ) = 0;


protected:

    CHelloResource& GetHelloResource() const
        { return dynamic_cast<CHelloResource&>( GetResource() ); }

    // hide operator= for this abstract class
    CHelloCommand& operator=( const CHelloCommand& rhs );

    // returns the string used to match the name in a cgi request.
    // e.g. for "?cmd=search", GetEntry should return "cmd"
    virtual string GetEntry() const;
};

//
// CHelloBasicCommand
//

// welcome page

class CHelloBasicCommand : public CHelloCommand
{
public:

    CHelloBasicCommand( CNcbiResource& resource );
    virtual ~CHelloBasicCommand( void );

    virtual CNcbiCommand* Clone( void ) const;

    // GetName() returns the string used to match the name in a cgi request.
    // e.g. for "?cmd=search", GetName() should return "search"
    virtual string GetName( void ) const;

    virtual CNCBINode* CreateView( CCgiContext& ctx );
};

//
// CHelloReplyCommand
//

// The reply hello page

class CHelloReplyCommand : public CHelloBasicCommand
{
public:

    CHelloReplyCommand( CNcbiResource& resource );
    virtual ~CHelloReplyCommand( void );

    virtual CNcbiCommand* Clone( void ) const;

    // GetName() returns the string used to match the name in a cgi request.
    // e.g. for "?cmd=search", GetName() should return "search"
    virtual string GetName( void ) const;

    virtual CNCBINode* CreateView( CCgiContext& ctx );
};

END_NCBI_SCOPE

#endif /* NCBI_HELLOCMD__HPP */
