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
* Revision 1.23  1999/03/15 19:57:19  vasilche
* Added CNcbiQueryResultIterator
*
* Revision 1.22  1999/03/10 21:20:23  sandomir
* Resource added to CNcbiContext
*
* Revision 1.21  1999/02/22 21:12:37  sandomir
* MsgRequest -> NcbiContext
*
* Revision 1.20  1999/02/18 19:29:14  vasilche
* Added CCgiServerContext.
*
* Revision 1.19  1999/02/05 22:00:36  sandomir
* Command::GetLink() changes
*
* Revision 1.18  1999/02/02 17:27:07  sandomir
* PFindByName fixed
*
* Revision 1.17  1999/01/29 17:52:42  sandomir
* FilterList changed
*
* Revision 1.16  1999/01/27 16:46:22  sandomir
* minor change: PFindByName added
*
* Revision 1.15  1999/01/21 16:18:03  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.14  1999/01/14 20:03:48  sandomir
* minor changes
*
* Revision 1.13  1999/01/12 17:06:29  sandomir
* GetLink changed
*
* Revision 1.12  1998/12/31 19:47:28  sandomir
* GetEntry() fixed
*
* Revision 1.11  1998/12/28 23:29:00  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
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

#include <corelib/ncbicgi.hpp>
#include <corelib/ncbicgir.hpp>

#include <functional>

BEGIN_NCBI_SCOPE

class CCgiServerContext
{
public:
    virtual ~CCgiServerContext( void ) {}
};

//
// class CNcbiContext
//

/* CNcbiContext is a wrapper for request, response, server context.
In addtion, it contains list of messages.
Having non-const reference, CNcbiContext's user has access to its all internal data
*/

class CNcbiResource;
 
class CNcbiContext
{
public:

    CNcbiContext( CNcbiResource& resource,
                  CCgiRequest& request, 
                  CCgiResponse& response,
                  CCgiServerContext& srvCtx );

    const CNcbiResource& GetResource( void ) const THROWS_NONE;

    const CCgiRequest& GetRequest( void ) const THROWS_NONE;
    CCgiRequest& GetRequest( void ) THROWS_NONE;

    const CCgiResponse& GetResponse( void ) const THROWS_NONE;
    CCgiResponse& GetResponse( void ) THROWS_NONE;

    const CCgiServerContext& GetServCtx( void ) const THROWS_NONE;
    CCgiServerContext& GetServCtx( void ) THROWS_NONE;

    typedef list<string> TMsgList;

    const TMsgList& GetMsg( void ) const THROWS_NONE;
    TMsgList& GetMsg( void ) THROWS_NONE;

    void PutMsg( const string& msg );
    void ClearMsgList( void );

private:

    CNcbiResource& m_resource;
    CCgiRequest& m_request;
    CCgiResponse& m_response;
    CCgiServerContext& m_srvCtx;
   
    TMsgList m_msg;

}; 

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

  virtual void HandleRequest( CNcbiContext& ctx );

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
  virtual string GetLink( void ) const = 0;
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
  virtual string GetLink( CNcbiContext& ctx ) const = 0;

  virtual void Execute( CNcbiContext& ctx ) = 0;

  virtual bool IsRequested( const CNcbiContext& ctx ) const;

protected:

  virtual string GetEntry() const = 0;

  CNcbiResource& GetResource() const
        { return m_resource; }

private:

  CNcbiResource& m_resource;
};

//
// class CNcbiDatabaseInfo
//

class CNcbiDbPresentation;

class CNcbiDatabaseInfo
{
public:

  virtual ~CNcbiDatabaseInfo( void ) {}
  
  virtual string GetName( void ) const = 0;

  virtual const CNcbiDbPresentation* GetPresentation() const
    { return 0; }

  virtual bool IsRequested( const CNcbiContext& ctx ) const;

protected:

  virtual bool CheckName( const string& name ) const = 0;
  virtual string GetEntry() const = 0;
  
};
                     
//
// class CNcbiDatabase
//

class CNcbiDatabaseFilter;
typedef list<CNcbiDatabaseFilter*> TFilterList;

class CNcbiQueryResult;

class CNcbiDatabase
{
public:

  CNcbiDatabase( const CNcbiDatabaseInfo& dbinfo );
  virtual ~CNcbiDatabase( void );

  virtual void Connect( void ) {} 
  virtual void Disconnect( void ) {}

  const CNcbiDatabaseInfo& GetDbInfo( void ) const
    { return m_dbinfo; }

  const TFilterList& GetFilterList( void ) const
    { return m_filter; }

  virtual CNcbiQueryResult* Execute( CNcbiContext& ctx ) = 0;

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

  virtual CNCBINode* CreateView( CNcbiContext& ctx ) const = 0;
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
  virtual string GetLink( void ) const = 0;
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

class CNcbiQueryResultIterator
{
    friend class CNcbiQueryResult;
    typedef CNcbiQueryResult TResult;
    typedef CNcbiDataObject TObject;
    typedef CNcbiQueryResultIterator TIterator;

private:
    inline CNcbiQueryResultIterator(TResult* result, TObject* object);

public:
    inline CNcbiQueryResultIterator(void)
        : m_Result(0), m_Object(0)
        { }

    // pass ownership of object to this
    inline CNcbiQueryResultIterator(const TIterator& i);

    // will free object
    inline ~CNcbiQueryResultIterator(void);

    // pass ownership of object to this
    inline TIterator& operator =(const TIterator& i);

    // check whether result finished
    operator bool(void) const
        { return m_Object != 0; }

    // check whether iterator points to the same object
    bool operator ==(const TIterator& i) const
        {
            if ( m_Result != i.m_Result )
                return false;
            if ( m_Object == 0 )
                return i.m_Object == 0;
            else
                return i.m_Object != 0 &&
                    i.m_Object->GetID() == m_Object->GetID();
        }
        
    const TObject& operator *(void) const
        { return *m_Object; }
    const TObject* operator ->(void) const
        { return m_Object; }
    const TObject* get(void) const
        { return m_Object; }

    // go to next result
    inline TIterator& operator ++(void);
        
private:
    TResult* m_Result;
    TObject* m_Object;

    // free current object ( for call from destructor etc.)
    inline void reset(void);
    // disown object ( for call from assignment )
    inline TObject* release(void) const;
};

class CNcbiQueryResult
{
public:
    friend class CNcbiQueryResultIterator;
    typedef CNcbiQueryResultIterator TIterator;
    typedef unsigned long TSize;

    CNcbiQueryResult(void);
    virtual ~CNcbiQueryResult(void);

    TIterator begin(TSize index = 0)
        { return TIterator(this, FirstObject(index)); }
    TIterator end(void)
        { return TIterator(this, 0); }

    virtual TSize Size(void) const = 0;

protected:
    // should return 'index' object in query (null if none)
    virtual CNcbiDataObject* FirstObject(TSize index) = 0;
    // should free 'object' and return next object in query (null if none)
    virtual CNcbiDataObject* NextObject(CNcbiDataObject* object) = 0;
    // should free 'object' (if needed)
    virtual void FreeObject(CNcbiDataObject* object);
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

#if 0 // generic object makes no sense in practice

  virtual string GetLink( const CNcbiDataObject& obj ) const = 0;

  virtual CNCBINode* CreateView( CNcbiContext& ctx,
                                 const CNcbiDataObject& obj
                                 bool selected = false) const = 0;
#endif

  virtual bool IsRequested( const CNcbiContext& ctx ) const;

protected:

  virtual string GetEntry() const = 0;
  
};

//
// PRequested
//

template<class T>
class PRequested : public unary_function<T,bool>
{  
  const CNcbiContext& m_ctx;
  
public:
  
  explicit PRequested( const CNcbiContext& ctx ) 
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

#include <corelib/ncbires.inl>

END_NCBI_SCOPE

#endif /* NCBI_RES__HPP */
