#if defined(NCBI_RES__HPP)  &&  !defined(NCBI_RES__INL)
#define NCBI_RES__INL

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
* Revision 1.4  1999/03/17 18:59:47  vasilche
* Changed CNcbiQueryResult&Iterator.
*
* Revision 1.3  1999/03/15 19:57:20  vasilche
* Added CNcbiQueryResultIterator
*
* Revision 1.2  1999/03/10 21:20:23  sandomir
* Resource added to CNcbiContext
*
* Revision 1.1  1999/02/22 21:12:38  sandomir
* MsgRequest -> NcbiContext
*
* ===========================================================================
*/

inline const CNcbiResource& CNcbiContext::GetResource( void ) const THROWS_NONE
{
    return m_resource;
}

inline const CCgiRequest& CNcbiContext::GetRequest( void ) const THROWS_NONE
{ 
    return m_request;
}
   
inline CCgiRequest& CNcbiContext::GetRequest( void ) THROWS_NONE
{ 
    return m_request;
}
      
inline const CCgiResponse& CNcbiContext::GetResponse( void ) const THROWS_NONE
{ 
    return m_response;
}
     
inline CCgiResponse& CNcbiContext::GetResponse( void ) THROWS_NONE
{ 
    return m_response;
}

inline const CCgiServerContext& CNcbiContext::GetServCtx( void ) const THROWS_NONE
{ 
    return m_srvCtx;
}

inline CCgiServerContext& CNcbiContext::GetServCtx( void ) THROWS_NONE
{ 
    return m_srvCtx;
}

inline const CNcbiContext::TMsgList& CNcbiContext::GetMsg( void ) const THROWS_NONE
{ 
    return m_msg;
}
 
inline CNcbiContext::TMsgList& CNcbiContext::GetMsg( void ) THROWS_NONE
{ 
    return m_msg;
}

inline void CNcbiContext::PutMsg( const string& msg ) 
{
    m_msg.push_back( msg ); 
}

inline void CNcbiContext::ClearMsgList( void ) 
{
    m_msg.clear();
}


// CNcbiQieryResult::CIterator

inline
void CNcbiQueryResultIterator::reset(void)
{
    TObject* old = m_Object;
    if ( old ) {
        m_Result->FreeObject(old);
        m_Object = 0;
    }
}

inline
void CNcbiQueryResultIterator::reset(TObject* object)
{
    TObject* old = m_Object;
    if ( object != old ) {
        if ( old )
            m_Result->FreeObject(old);
        m_Object = object;
    }
}

inline
CNcbiDataObject* CNcbiQueryResultIterator::release(void) const
{
    TObject* object = m_Object;
    const_cast<TIterator*>(this)->m_Object = 0;
    return object;
}

inline
CNcbiQueryResultIterator::CNcbiQueryResultIterator(TResult* result)
    : m_Result(result), m_Object(0)
{
    if ( m_Result )
        m_Result->FirstObject(*this);
}

inline
CNcbiQueryResultIterator::CNcbiQueryResultIterator(TResult* result, TSize index)
    : m_Result(result), m_Object(0)
{
    if ( m_Result )
        m_Result->FirstObject(*this, index);
}

inline
CNcbiQueryResultIterator::CNcbiQueryResultIterator(bool, TResult* result)
    : m_Result(result), m_Object(0)
{
}

inline
CNcbiQueryResultIterator::CNcbiQueryResultIterator(void)
    : m_Result(0), m_Object(0)
{
}

inline
CNcbiQueryResultIterator::CNcbiQueryResultIterator(const TIterator& i)
    : m_Result(i.m_Result), m_Object(i.release())
{
}

inline
CNcbiQueryResultIterator::~CNcbiQueryResultIterator(void)
{
    reset();
}

inline
CNcbiQueryResultIterator&
CNcbiQueryResultIterator::operator =(const TIterator& i)
{
    if ( &i != this ) {
        reset(i.release());
        m_Result = i.m_Result;
    }
    return *this;
}

inline
CNcbiQueryResultIterator& CNcbiQueryResultIterator::operator ++(void)
{
    if ( m_Object )
        m_Result->NextObject(*this);
    return *this;
}

inline
CNcbiQueryResult::TIterator CNcbiQueryResult::begin(void)
{
    return TIterator(this);
}

inline
CNcbiQueryResult::TIterator CNcbiQueryResult::begin(TSize index)
{
    return TIterator(this, index);
}

inline
CNcbiQueryResult::TIterator CNcbiQueryResult::end(void)
{
    return TIterator(false, this);
}

inline
void CNcbiQueryResult::ResetObject(TIterator& obj, CNcbiDataObject* object)
{
    obj.reset(object);
}

#endif /* def NCBI_RES__HPP  &&  ndef NCBI_RES__INL */
