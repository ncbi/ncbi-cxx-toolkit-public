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
* Revision 1.10  1998/12/28 15:43:11  sandomir
* minor fixed in CgiApp and Resource
*
* Revision 1.9  1998/12/21 17:19:36  sandomir
* VC++ fixes in ncbistd; minor fixes in Resource
*
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
// class CNcbiMsgRequest
//

class CNcbiMsgRequest : public CCgiRequest
{
public:

  typedef list<string> TMsgList;
  
  CNcbiMsgRequest(CNcbiIstream* istr=0, bool indexes_as_entries=true);
  CNcbiMsgRequest(int argc, char* argv[], CNcbiIstream* istr=0,
              bool indexes_as_entries=true);
  virtual~CNcbiMsgRequest(void);

  void PutMsg( const string& msg );
  const TMsgList& GetMsgList( void ) const;
  void ClearMsgList( void );

protected:

  TMsgList m_msg;

};

//
// class CNcbiRequest
//


//class CNcbiMsgRequest : public CCgiRequest

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

  virtual void HandleRequest( CNcbiMsgRequest& request );

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

  CNcbiCommand( CNcbiResource& resource );
  virtual ~CNcbiCommand( void );

  virtual CNcbiCommand* Clone( void ) const = 0;

  virtual CNCBINode* GetLogo( void ) const { return 0; }
  virtual string GetName( void ) const = 0;
  virtual CHTML_a* GetLink( void ) const { return 0; }

  virtual void Execute( CNcbiMsgRequest& request ) = 0;

  virtual bool IsRequested( const CNcbiMsgRequest& request ) const;

protected:

  virtual string GetEntry() const;

  CNcbiResource& m_resource;
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

  virtual bool IsRequested( const CNcbiMsgRequest& request ) const;

protected:

  virtual bool CheckName( const string& name ) const = 0;
  virtual string GetEntry() const;
  
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

  virtual CNcbiQueryResult* Execute( CNcbiMsgRequest& request ) = 0;

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

  virtual CNCBINode* CreateView( CNcbiMsgRequest& request ) const = 0;
};

//
// class CNcbiDbPresentation
//

class CNcbiDbPresentation
{
public:

  virtual ~CNcbiDbPresentation() {}

  virtual CNCBINode* GetLogo( void ) const { return 0; }
  virtual string GetName( void ) const = 0; // this is printable name
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

  virtual CNCBINode* CreateView( CNcbiMsgRequest& request ) const = 0;

  virtual bool IsRequested( const CNcbiMsgRequest& request ) const;

protected:

  virtual string GetEntry() const;
  
};

//
// PRequested
//

template<class T>
class PRequested : public unary_function<T,bool>
{  
  const CNcbiMsgRequest& m_request;
  
public:
  
  explicit PRequested( const CNcbiMsgRequest& request ) 
    : m_request( request ) {}

  bool operator() ( const T* t ) const 
    { return t->IsRequested( m_request ); }

}; // class PRequested

END_NCBI_SCOPE

#endif /* NCBI_RES__HPP */
