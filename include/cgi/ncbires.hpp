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
* Revision 1.8  1998/12/17 21:50:43  sandomir
* CNCBINode fixed in Resource; case insensitive string comparison predicate added
*
* Revision 1.7  1998/12/17 17:25:02  sandomir
* minor changes in Report
*
* Revision 1.6  1998/12/14 20:25:36  sandomir
* changed with Command handling
*
* Revision 1.5  1998/12/11 15:57:31  sandomir
* CNcbiDatabase: public keyword added
*
* Revision 1.4  1998/12/10 17:36:54  sandomir
* ncbires.cpp added
*
* Revision 1.3  1998/12/10 15:14:13  sandomir
* minor updates
*
* Revision 1.2  1998/12/09 21:04:26  sandomir
* CNcbiResource - initial draft; in no means is a final header
*
* Revision 1.1  1998/12/04 18:11:23  sandomir
* CNcbuResource - initial draft
*
* ===========================================================================
*/

#include <ncbicgi.hpp>

#include <functional>

BEGIN_NCBI_SCOPE

//
// class CNcbiResource
//

class CNcbiDatabase;
class CNcbiDatabaseInfo;
class CNcbiCommand;

class CNcbiResPresentation;
class CNCBINode;

class CHTML_a;

typedef list<CNcbiDatabaseInfo*> TDbInfoList;
typedef list<CNcbiCommand*> TCmdList;

class CNcbiResource
{
public:

  CNcbiResource( void );
  virtual ~CNcbiResource( void );

  virtual void Init( void ) {} // init presentation, databases, commands
  virtual void Exit( void ) {}

  virtual const CNcbiResPresentation* GetPresentation( void ) const
    { return 0; }

  const TDbInfoList& GetDatabaseInfoList( void ) const
    { return m_dbInfo; }

  virtual CNcbiDatabase& GetDatabase( const CNcbiDatabaseInfo& info ) = 0;

  const TCmdList& GetCmdList( void ) const
    { return m_cmd; }

  virtual CNcbiCommand* GetDefaultCommand( void ) const = 0;

  virtual void HandleRequest( const CCgiRequest& request );

protected:

  TDbInfoList m_dbInfo;
  TCmdList m_cmd;
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
  virtual CHTML_a* GetLink( void ) const { return 0; }
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

  virtual CNCBINode* GetLogo( void ) const { return 0; }
  virtual string GetName( void ) const = 0;
  virtual CHTML_a* GetLink( void ) const { return 0; }

  virtual void Execute( const CCgiRequest& request ) = 0;

  virtual bool IsRequested( const CCgiRequest& request ) const;

  // inner class CFind to be used to find command(s) corresponding to the request
  // TCmdList l; 
  // TCmdList::iterator it = find_if( l.begin(), l.end(), CNcbiCommand::CFind( request ) ); 
  class CFind : public unary_function<CNcbiCommand,bool>
  {
    const CCgiRequest& m_request;

  public:

    explicit CFind( const CCgiRequest& request ) : m_request( request ) {}
    bool operator() ( const CNcbiCommand* cmd ) const
      { return cmd->IsRequested( m_request ); }
  }; // class CFind 
    
protected:

  virtual string GetEntry() const;

  const CNcbiResource& m_resource;
};

//
// class CNcbiDatabaseInfo
//

class CNcbiDbPresentation;

class CNcbiDatabaseInfo
{
public:

  virtual ~CNcbiDatabaseInfo() {}

  virtual const CNcbiDbPresentation* GetPresentation() const
    { return 0; }

  virtual bool CheckName( const string& name ) const = 0;

  // inner class CFind to be used to find db(s) corresponding to the request
  // TDbInfoList l; 
  // TDbInfoList::iterator it = find_if( l.begin(), l.end(), CNcbiDatabaseInfo::CFind( alias ) ); 
  class CFind : public unary_function<CNcbiDatabaseInfo,bool>
  {
    const string& m_name;
    
  public:
    
    explicit CFind( const string& name ) : m_name( name ) {}
    bool operator() ( const CNcbiDatabaseInfo* dbinfo ) const
      { return dbinfo->CheckName( m_name ); }
  }; // class CFind
};
                     
//
// class CNcbiDatabase
//

class CNcbiDatabaseFilter;
typedef map< string, CNcbiDatabaseFilter* > TFilterList;

class CNcbiQueryResult;

class CNcbiDatabase
{
public:

  CNcbiDatabase( const CNcbiDatabaseInfo& dbinfo );
  virtual ~CNcbiDatabase( void );

  virtual void Connect( void ) {} 
  virtual void Disconnect( void ) {}

  const TFilterList& GetFilterList( void ) const
    { return m_filter; }

  virtual CNcbiQueryResult* Execute( const CCgiRequest& request ) = 0;

protected:

  const CNcbiDatabaseInfo& m_dbinfo;

  TFilterList m_filter;
};

//
// class CNcbiDatabaseFilter
//

class CNcbiDatabaseFilter
{
public:

  virtual ~CNcbiDatabaseFilter() {}
};

class CNcbiDbFilterReport
{
public:

  virtual ~CNcbiDbFilterReport() {}

  virtual CNCBINode* CreateView( const CCgiRequest& request ) const = 0;
};

//
// class CNcbiDbPresentation
//

class CNcbiDbPresentation
{
public:

  virtual ~CNcbiDbPresentation() {}

  virtual CNCBINode* GetLogo( void ) const { return 0; }
  virtual string GetName( void ) const = 0;
  virtual CHTML_a* GetLink( void ) const { return 0; }
};

//
// CNcbiDataObject
//

class CNcbiDataObjectReport;
typedef list<CNcbiDataObjectReport*> TReportList;

class CNcbiDataObject
{
public:

  virtual ~CNcbiDataObject() {}

  virtual int GetID( void ) const = 0;
  virtual string GetType( void ) const = 0;

  virtual const TReportList& GetReportList( void ) const = 0;
};

//
// class CNcbiQueryResult
//

class CNcbiQueryResult
{
public:

  virtual ~CNcbiQueryResult() {}

};

//
// class CNcbiDatabaseReport
//

class CNcbiDataObjectReport
{
public:

  virtual ~CNcbiDataObjectReport() {}

  virtual CNcbiDataObjectReport* Clone( void ) const = 0;

  virtual CNCBINode* GetLogo( void ) const { return 0; }
  virtual string GetName( void ) const = 0;
  virtual CHTML_a* GetLink( void ) const { return 0; }

  virtual CNCBINode* CreateView( const CCgiRequest& request ) const = 0;

  virtual bool IsRequested( const CCgiRequest& request ) const;

  class CFind : public unary_function<CNcbiDataObjectReport,bool>
  {
    const CCgiRequest& m_request;

  public:

    explicit CFind( const CCgiRequest& request ) : m_request( request ) {}
    bool operator() ( const CNcbiDataObjectReport* rpt ) const
      { return rpt->IsRequested( m_request ); }
  }; // class CFind

protected:

  virtual string GetEntry() const;
  
};

END_NCBI_SCOPE

#endif /* NCBI_RES__HPP */
