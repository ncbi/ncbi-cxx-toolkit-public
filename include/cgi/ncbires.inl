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
* Revision 1.1  1999/02/22 21:12:38  sandomir
* MsgRequest -> NcbiContext
*
* ===========================================================================
*/

inline const CCgiRequest& CNcbiContext::GetRequest( void ) const
{ 
    return m_request;
}
   
inline CCgiRequest& CNcbiContext::GetRequest( void )
{ 
    return m_request;
}
      
inline const CCgiResponse& CNcbiContext::GetResponse( void ) const
{ 
    return m_response;
}
     
inline CCgiResponse& CNcbiContext::GetResponse( void )
{ 
    return m_response;
}

inline const CCgiServerContext& CNcbiContext::GetServCtx( void ) const
{ 
    return m_srvCtx;
}

inline CCgiServerContext& CNcbiContext::GetServCtx( void )
{ 
    return m_srvCtx;
}

inline const CNcbiContext::TMsgList& CNcbiContext::GetMsg( void ) const
{ 
    return m_msg;
}
 
inline CNcbiContext::TMsgList& CNcbiContext::GetMsg( void )
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

#endif /* def NCBI_RES__HPP  &&  ndef NCBI_RES__INL */
