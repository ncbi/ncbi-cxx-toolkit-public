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
* Revision 1.1  1998/12/04 18:11:23  sandomir
* CNcbuResource - initial draft
*
* ===========================================================================
*/

#include <map>

BEGIN_NCBI_SCOPE

class CCgiApplication;
class CNcbiDatabase;
class CNcbiCommand;

class CNcbiResPresentation;
class CNcbiNode;

typedef map< string, CNcbiDatabase* > TDbList;
typedef map< string, CNcbiCommand* > TCmdList;

//
// class CNcbiResource
//

class CNcbiResource
{
public:

  CNcbiResource( void );
  virtual ~CNcbiResource( void );

  const CNcbiResPresentation* GetPresentation() const
    { return m_presentation; }

  virtual CNcbiNode* GetLogo( void ) const = 0;
  virtual string GetName( void ) const = 0;

  const TDbList& GetDbList( void ) const
    { return m_db; }

  const TCmdList& GetCmdList( void ) const
    { return m_cmd; }
  
  virtual void Init( void ); // init presentation, databases, commands
  virtual void Exit( void );

  virtual void HandleRequest( const CCgiRequest& request );

protected:

  CNcbiResPresentation* m_presentation;

  TDbList m_db;
  TCmdList m_cmd;
};

//
// class CNcbiResPresentation
//

class CNcbiResPresentation
{
public:

  CNcbiResPresentation( const CNcbiResource& resource );
  virtual ~CNcbiResPresentation( void );

protected:

  const CNcbiResource& m_resource;

};

//
// class CNcbiCommand
//

class CNcbiCommand
{
public:

  CNcbiCommand( const CNcbiResource& resource );
  virtual ~CNcbiCommand( void );

  virtual CNcbiCommand* Clone( void ) const = 0;

  virtual CNcbiNode* GetLogo( void ) const = 0;
  virtual string GetName( void ) const = 0;

  virtual void Execute( const CCgiRequest& request );

protected:

  const CNcbiResource& m_resource;

};
                     
//
// class CNcbiDatabase
//

class CNcbiDbPresentation;

typedef map< string, CNcbiDatabaseFilter* > TFilterList;

class CNcbiDatabase
{
  CNcbiDatabase( const CNcbiResource& resource );
  virtual ~CNcbiDatabase( void );

  const CNcbiDbPresentation* GetPresentation() const
    { return m_presentation; }

  virtual CNcbiNode* GetLogo( void ) const = 0;
  virtual string GetName( void ) const = 0;

  const TFilterList& GetFilterList( void ) const
    { return m_filter; }

  virtual void Init( void ); // init presentation
  virtual void Exit( void );

protected:

  const CNcbiResource& m_resource;
  CNcbiDbPresentation* m_presentation;

  TFilterList m_filter;
};

//
// class CNcbiDbPresentation
//

class CNcbiDatabaseReport;

typedef map< string, CNcbiDatabaseReport* > TReportList;

class CNcbiDbPresentation
{
public:

  CNcbiDbPresentation( const CNcbiDatabase& db  );
  virtual ~CNcbiDbPresentation( void );

  const TReportList& GetReportList( void ) const
    { return m_report; }

protected:

  const CNcbiResource& m_db;

  TReportList m_report;

};

//
// class CNcbiDatabaseReport
//

class CNcbiDatabaseReport
{
public:

  CNcbiDatabaseReport( const CNcbiDatabase& db );
  virtual ~CNcbiDatabaseReport( void );

  virtual CNcbiDatabaseReport* Clone( void ) const = 0;

  virtual CNcbiNode* GetLogo( void ) const = 0;
  virtual string GetName( void ) const = 0;

  CNcbiNode* CreateView( const CCgiRequest& request ) const = 0;

protected:

  const CNcbiDatabase& m_db;
};

END_NCBI_SCOPE

#endif /* NCBI_RES__HPP */
