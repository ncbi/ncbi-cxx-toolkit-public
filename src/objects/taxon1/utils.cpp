#include <objects/taxon1/taxon1.hpp>

#include "CTreeCont.hpp"
#include "cache.hpp"

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

bool
CTaxon1::SendRequest( CTaxon1_req& req, CTaxon1_resp& resp )
{
    unsigned nIterCount( 0 );
    unsigned fail_flags( 0 );
    if( !m_pServer ) {
	SetLastError( "Service is not initialized" );
	return false;
    }
    SetLastError( NULL );

    do {
	bool bNeedReconnect( false );

	try {
	    *m_pOut << req;
	    m_pOut->Flush();

	    if( m_pOut->InGoodState() ) {
		try {
		    *m_pIn >> resp;
		
		    if( m_pIn->InGoodState() ) {
			if( resp.IsError() ) { // Process error here
			    std::string err;
			    resp.GetError().GetErrorText( err );
			    SetLastError( err.c_str() );
			    return false;
			} else
			    return true;
		    }
		} catch( exception& e ) {
		    SetLastError( e.what() );
		}
		fail_flags = m_pIn->GetFailFlags();
		bNeedReconnect = (fail_flags & ( CObjectIStream::eEOF
						|CObjectIStream::eReadError
						|CObjectIStream::eOverflow
						|CObjectIStream::eFail
						|CObjectIStream::eNotOpen ));
	    } else {
		m_pOut->ThrowError1((CObjectOStream::EFailFlags)
				    m_pOut->GetFailFlags(),
				    "Output stream is in bad state");
	    }
	} catch( exception& e ) {
	    SetLastError( e.what() );
	    fail_flags = m_pOut->GetFailFlags();
	    bNeedReconnect = (fail_flags & ( CObjectOStream::eEOF
					    |CObjectOStream::eWriteError
					    |CObjectOStream::eOverflow
					    |CObjectOStream::eFail
					    |CObjectOStream::eNotOpen ));
	}	    
	
	if( !bNeedReconnect )
	    break;
	// Reconnect the service
	if( nIterCount < m_nReconnectAttempts ) {
	    delete m_pOut;
	    delete m_pIn;
	    delete m_pServer;
	    m_pOut = NULL;
	    m_pIn = NULL;
	    m_pServer = NULL;
	    try {
		auto_ptr<CObjectOStream> pOut;
		auto_ptr<CObjectIStream> pIn;
		auto_ptr<CConn_ServiceStream>
		    pServer( new CConn_ServiceStream(m_pchService, fSERV_Any,
						     0, 0, &m_timeout) );
		
		pOut.reset( CObjectOStream::Open(m_eDataFormat, *pServer) );
		pIn.reset( CObjectIStream::Open(m_eDataFormat, *pServer) );
		m_pServer = pServer.release();
		m_pIn = pIn.release();
		m_pOut = pOut.release();
	    } catch( exception& e ) {
		SetLastError( e.what() );
	    }
	} else { // No more attempts left
	    break;
	}
    } while( nIterCount++ < m_nReconnectAttempts );
    return false;
}

void
CTaxon1::SetLastError( const char* pchErr )
{
    if( pchErr )
	m_sLastError.assign( pchErr );
    else
	m_sLastError.erase();
}

//////////////////////////////////
//  CTaxon1Node implementation
//
#define TXC_INH_DIV 0x4000000
#define TXC_INH_GC  0x8000000
#define TXC_INH_MGC 0x10000000
/* the following three flags are the same (it is not a bug) */
#define TXC_SUFFIX  0x20000000
#define TXC_UPDATED 0x20000000
#define TXC_UNCULTURED 0x20000000

#define TXC_GBHIDE  0x40000000
#define TXC_STHIDE  0x80000000

short
CTaxon1Node::GetRank() const
{
    return ((m_ref->GetCde())&0xff)-1;
}

short
CTaxon1Node::GetDivision() const
{
    return (m_ref->GetCde()>>8)&0x3f;
}

short
CTaxon1Node::GetGC() const
{
    return (m_ref->GetCde()>>(8+6))&0x3f;
}

short
CTaxon1Node::GetMGC() const
{
    return (m_ref->GetCde()>>(8+6+6))&0x3f;
}

bool
CTaxon1Node::IsUncultured() const
{
    return (m_ref->GetCde() & TXC_UNCULTURED);
}

bool
CTaxon1Node::IsGBHidden() const
{
    return (m_ref->GetCde() & TXC_GBHIDE);
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

