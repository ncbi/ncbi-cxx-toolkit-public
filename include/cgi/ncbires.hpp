#ifndef NCBI_RES__HPP
#define NCBI_RES__HPP

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
*   Basic Resource class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.27  1999/05/06 20:32:51  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
*
* ===========================================================================
*/

#include <corelib/ncbicgi.hpp>
#include <corelib/ncbicgir.hpp>

#include <functional>

BEGIN_NCBI_SCOPE

class CNcbiResPresentation;
class CNcbiRegistry;
class CCgiContext;

class CNCBINode;

//
// class CNcbiResource
//

class CNcbiResource
{
public:

    CNcbiResource(CNcbiRegistry& config);
    virtual ~CNcbiResource( void );

    const CNcbiRegistry& GetConfig(void) const
        { return m_config; }
    CNcbiRegistry& GetConfig(void)
        { return m_config; }

    virtual const CNcbiResPresentation* GetPresentation( void ) const
        { return 0; }

    virtual void HandleRequest( CCgiContext& ctx ) = 0;

protected:

    CNcbiRegistry& m_config;  
};

//
// class CNcbiResPresentation
//

class CNcbiResPresentation
{
public:

    virtual ~CNcbiResPresentation() {}

    virtual CNCBINode* GetLogo( void ) const { return 0; }
    virtual string GetName( void ) const = 0;
    virtual string GetLink( void ) const = 0;
};

//
// PRequested
//

template<class T>
class PRequested : public unary_function<T,bool>
{  
    const CCgiContext& m_ctx;
  
public:
  
    explicit PRequested( const CCgiContext& ctx ) 
        : m_ctx( ctx ) {}

    bool operator() ( const T* t ) const 
        { return t->IsRequested( m_ctx ); }

}; // class PRequested

//
// PFindByName
//

template<class T>
class PFindByName : public unary_function<T,bool>
{  
    const string& m_name;
  
public:
    
    explicit PFindByName( const string& name ) 
        : m_name( name ) {}
    
    bool operator() ( const T* t ) const 
        { return AStrEquiv( m_name, t->GetName(), PNocase() ); }
    
}; // class PFindByName

END_NCBI_SCOPE

#endif /* NCBI_RES__HPP */
