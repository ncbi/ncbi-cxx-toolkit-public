/* $Id$
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
 * Author:  Vladimir Soussov, Michael Domrachev
 *
 * File Description:
 *     NCBI Taxonomy information retreival library implementation
 *
 */

#include <objects/taxon1/taxon1.hpp>

#include "cache.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

static const char s_achInvalTaxid[] = "Invalid tax id specified";

CTaxon1::CTaxon1()
    : m_pServer(NULL),
      m_pOut(NULL),
      m_pIn(NULL),
      m_plCache(NULL),
      m_bWithSynonyms(false)
{
}


CTaxon1::~CTaxon1()
{
    Reset();
}

void
CTaxon1::Reset()
{
    SetLastError(NULL);
    delete m_pIn;
    delete m_pOut;
    delete m_pServer;
    m_pIn = NULL;
    m_pOut = NULL;
    m_pServer = NULL;
    delete m_plCache;
    m_plCache = NULL;
}

bool
CTaxon1::Init( std::time_t timeout, unsigned reconnect_attempts )
{
    SetLastError(NULL);
    if( m_pServer ) { // Already inited
	SetLastError( "ERROR: Init(): Already initialized" );
	return false;
    }
    try {
	// Open connection to Taxonomy service
	CTaxon1_req req;
	CTaxon1_resp resp;
	
	m_timeout.sec = timeout; m_timeout.usec = 0;
	m_nReconnectAttempts = reconnect_attempts;
	m_pchService = "TaxService";
	const char* tmp;
	if( (tmp=getenv("NI_TAXONOMY_SERVICE_NAME")) != NULL ) {
	    m_pchService = tmp;
	}
	auto_ptr<CObjectOStream> pOut;
	auto_ptr<CObjectIStream> pIn;
	auto_ptr<CConn_ServiceStream>
	    pServer( new CConn_ServiceStream(m_pchService, fSERV_Any,
					     0, 0, &m_timeout) );

#ifdef USE_TEXT_ASN
	m_eDataFormat = eSerial_AsnText;
#else
	m_eDataFormat = eSerial_AsnBinary;
#endif
	pOut.reset( CObjectOStream::Open(m_eDataFormat, *pServer) );
	pIn.reset( CObjectIStream::Open(m_eDataFormat, *pServer) );

	req.SetInit( true );
	
	m_pServer = pServer.release();
	m_pIn = pIn.release();
	m_pOut = pOut.release();

	if( SendRequest( req, resp ) ) {
	    if( resp.IsInit() ) {
		// Init is done
		m_plCache = new COrgRefCache( *this );
		if( m_plCache->Init() ) {
		    return true;
		}
		delete m_plCache;
		m_plCache = NULL;
	    } else { // Set error
		SetLastError( "ERROR: Response type is not Init" );
	    }
	}
    } catch( exception& e ) {
	SetLastError( e.what() );
    }
    // Clean streams
    delete m_pIn;
    delete m_pOut;
    delete m_pServer;
    m_pIn = NULL;
    m_pOut = NULL;
    m_pServer = NULL;
    return false;
}

void
CTaxon1::Fini(void)
{
    SetLastError(NULL);
    if( m_pServer ) {
	CTaxon1_req req;
	CTaxon1_resp resp;
	
	req.SetFini( true );
	
	if( SendRequest( req, resp ) ) {
	    if( !resp.IsFini() ) {
		SetLastError( "Response type is not Fini" );
	    }
	}
    }
    Reset();
}

CRef< CTaxon2_data >
CTaxon1::GetById(int tax_id)
{
    SetLastError(NULL);
    if( tax_id > 0 ) {
	// Check if this taxon is in cache
	CTaxon2_data* pData( NULL );
	if( m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {
	    CTaxon2_data* pNewData( new CTaxon2_data() );

	    SerialAssign<CTaxon2_data>( *pNewData, *pData );

	    return pNewData;
	}
    } else {
	SetLastError( s_achInvalTaxid );
    }
    return NULL;
}

CRef< CTaxon2_data >
CTaxon1::Lookup(const COrg_ref& inp_orgRef )
{
    SetLastError(NULL);
    // Check if this taxon is in cache
    CTaxon2_data* pData( NULL );

    int tax_id = GetTaxIdByOrgRef( inp_orgRef );

    if( tax_id > 0
	&& m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {

	CTaxon2_data* pNewData( new CTaxon2_data() );

	SerialAssign<CTaxon2_data>( *pNewData, *pData  );

	return pNewData;
    }
    return NULL;
}

CConstRef< CTaxon2_data >
CTaxon1::LookupMerge(COrg_ref& inp_orgRef )
{
    CTaxon2_data* pData( NULL );

    SetLastError(NULL);
    int tax_id = GetTaxIdByOrgRef( inp_orgRef );

    if( tax_id > 0
	&& m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {

	const COrg_ref& src( pData->GetOrg() );

	inp_orgRef.SetTaxname( src.GetTaxname() );
	if( src.IsSetCommon() ) {
	    inp_orgRef.SetCommon( src.GetCommon() );
	} else {
	    inp_orgRef.ResetCommon();
	}

	// populate tax_id
	inp_orgRef.SetTaxId( tax_id );
    
	// copy the synonym list
	if( m_bWithSynonyms ) {
	    inp_orgRef.SetSyn() = src.GetSyn();
	}
	
	/* copy orgname */
	SerialAssign<COrgName>( inp_orgRef.SetOrgname(), src.GetOrgname() );
  
	if( !inp_orgRef.GetOrgname().IsSetMod() ) {
	    inp_orgRef.SetOrgname().ResetMod();
	    const COrgName::TMod& mods( src.GetOrgname().GetMod() );
	    COrgName::TMod& modd( inp_orgRef.SetOrgname().SetMod() );
	    // Copy with object copy as well
	    for( COrgName::TMod::const_iterator ci = mods.begin();
		 ci != mods.end();
		 ++ci ) {
		COrgMod* pMod( new COrgMod );
		SerialAssign<COrgMod>( *pMod, ci->GetObject() );
		modd.push_back( pMod );
	    }
	}

	if( src.GetOrgname().IsSetLineage() )
	    inp_orgRef.SetOrgname()
		.SetLineage( src.GetOrgname().GetLineage() );
	else
	    inp_orgRef.SetOrgname().ResetLineage();
	inp_orgRef.SetOrgname().SetGcode( src.GetOrgname().GetGcode() );
	inp_orgRef.SetOrgname().SetMgcode( src.GetOrgname().GetMgcode() );
	inp_orgRef.SetOrgname().SetDiv( src.GetOrgname().GetDiv() );
    }
    return pData;
}

int
CTaxon1::GetTaxIdByOrgRef(const COrg_ref& inp_orgRef)
{
    SetLastError(NULL);

    CTaxon1_req  req;
    CTaxon1_resp resp;

    SerialAssign< COrg_ref >( req.SetGetidbyorg(), inp_orgRef );

    if( SendRequest( req, resp ) ) {
	if( resp.IsGetidbyorg() ) {
	    // Correct response, return object
	    return resp.GetGetidbyorg();
	} else { // Internal: wrong respond type
	    SetLastError( "Response type is not Getidbyorg" );
	}
    }
    return 0;
}

int
CTaxon1::GetTaxIdByName(const string& orgname)
{
    SetLastError(NULL);
    if( orgname.empty() )
	return 0;
    COrg_ref orgRef;
    
    orgRef.SetTaxname().assign( orgname );
    
    return GetTaxIdByOrgRef(orgRef);
}

int
CTaxon1::FindTaxIdByName(const string& orgname)
{
    SetLastError(NULL);
    if( orgname.empty() )
	return 0;

    int id( GetTaxIdByName(orgname) );

    if(id < 1) {
	
	int idu(0);

	CTaxon1_req  req;
	CTaxon1_resp resp;

	req.SetGetunique().assign( orgname );

	if( SendRequest( req, resp ) ) {
	    if( resp.IsGetunique() ) {
		// Correct response, return object
		idu = resp.GetGetunique();
	    } else { // Internal: wrong respond type
		SetLastError( "Response type is not Getunique" );
	    }
	}

	if( idu > 0 )
	    id= idu;
    }
    return id;
}

int
CTaxon1::GetAllTaxIdByName(const string& orgname, TTaxIdList& lIds)
{
    int count(0);

    SetLastError(NULL);
    if( orgname.empty() )
	return 0;

    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetFindname().assign(orgname);

    if( SendRequest( req, resp ) ) {
	if( resp.IsFindname() ) {
	    // Correct response, return object
	    const list< CRef< CTaxon1_name > >& lNm( resp.GetFindname() );
	    // Fill in the list
	    for( list< CRef< CTaxon1_name > >::const_iterator
		     i = lNm.begin();
		 i != lNm.end(); ++i, ++count )
		lIds.push_back( (*i)->GetTaxid() );
	} else { // Internal: wrong respond type
	    SetLastError( "Response type is not Findname" );
	    return 0;
	}
    }
    return count;
}

CConstRef< COrg_ref >
CTaxon1::GetOrgRef(int tax_id,
		   bool& is_species,
		   bool& is_uncultured,
		   std::string& blast_name)
{
    SetLastError(NULL);
    if( tax_id > 1 ) {
	CTaxon2_data* pData(NULL);
	if( m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {
	    is_species = pData->GetIs_species_level();
	    is_uncultured = pData->GetIs_uncultured();
	    if( pData->GetBlast_name().size() > 0 ) {
		blast_name.assign( pData->GetBlast_name().front() );
	    }
	    return &pData->GetOrg();
	}
    }
    return null;
}

bool
CTaxon1::SetSynonyms(bool on_off)
{
    SetLastError(NULL);
    bool old_val( m_bWithSynonyms );
    m_bWithSynonyms = on_off;
    return old_val;
}

int
CTaxon1::GetParent(int id_tax)
{
    CTaxon1Node* pNode(NULL);
    SetLastError(NULL);
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
	&& pNode && pNode->GetParent() ) {
	return pNode->GetParent()->GetTaxId();
    }
    return 0;
}

int
CTaxon1::GetGenus(int id_tax)
{
    CTaxon1Node* pNode(NULL);
    SetLastError(NULL);
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
	&& pNode ) {
	int genus_rank(m_plCache->GetGenusRank());
	while( !pNode->IsRoot() ) {
	    int rank( pNode->GetRank() );
	    if( rank == genus_rank )
		return pNode->GetTaxId();
	    if( (rank > 0) && (rank < genus_rank))
		return -1;
	    pNode = pNode->GetParent();
	}
    }
    return -1;
}

int
CTaxon1::GetChildren(int id_tax, TTaxIdList& children_ids)
{
    int count(0);
    CTaxon1Node* pNode(NULL);
    SetLastError(NULL);
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
	&& pNode ) {

	CTaxon1_req  req;
	CTaxon1_resp resp;
	
	req.SetTaxachildren( id_tax );
	
	if( SendRequest( req, resp ) ) {
	    if( resp.IsTaxachildren() ) {
		// Correct response, return object
		list< CRef< CTaxon1_name > >&
		    lNm( resp.GetTaxachildren() );
		// Fill in the list
		CTreeIterator* pIt( m_plCache->GetTree().GetIterator() );
		pIt->GoNode( pNode );
		for( list< CRef< CTaxon1_name > >::const_iterator
			 i = lNm.begin();
		     i != lNm.end(); ++i, ++count ) {
		    children_ids.push_back( (*i)->GetTaxid() );
		    // Add node to the partial tree
		    CTaxon1Node* pNewNode( new CTaxon1Node(*i) );
		    m_plCache->SetIndexEntry(pNewNode->GetTaxId(), pNewNode);
		    pIt->AddChild( pNewNode );
		}
	    } else { // Internal: wrong respond type
		SetLastError( "Response type is not Taxachildren" );
		return 0;
	    }
	}
    }
    return count;
}

bool
CTaxon1::GetGCName(short gc_id, std::string& gc_name_out )
{
    SetLastError(NULL);
    if( m_gcStorage.empty() ) {
	CTaxon1_req  req;
	CTaxon1_resp resp;

	req.SetGetgcs( true );

	if( SendRequest( req, resp ) ) {
	    if( resp.IsGetgcs() ) {
		// Correct response, return object
		const list< CRef< CTaxon1_info > >& lGc( resp.GetGetgcs() );
		// Fill in storage
		for( list< CRef< CTaxon1_info > >::const_iterator
			 i = lGc.begin();
		     i != lGc.end(); ++i )
		    m_gcStorage.insert( TGCMap::value_type((*i)->GetIval1(),
							   (*i)->GetSval()) );
	    } else { // Internal: wrong respond type
		SetLastError( "Response type is not Getgcs" );
		return false;
	    }
	}
    }
    TGCMap::const_iterator gci( m_gcStorage.find( gc_id ) );
    if( gci != m_gcStorage.end() ) {
	gc_name_out.assign( gci->second );
	return true;
    } else {
	SetLastError( "ERROR: GetGCName(): Unknown genetic code" );
	return false;
    }
}

int
CTaxon1::Join(int taxid1, int taxid2)
{
    int tax_id( 0 );
    CTaxon1Node *pNode1, *pNode2;
    SetLastError(NULL);
    if( m_plCache->LookupAndAdd( taxid1, &pNode1 ) && pNode1
	&& m_plCache->LookupAndAdd( taxid2, &pNode2 ) && pNode2 ) {
	CTreeConstIterator* pIt( m_plCache->GetTree().GetConstIterator() );
	pIt->GoNode( pNode1 );
	pIt->GoAncestor( pNode2 );
	tax_id = static_cast<const CTaxon1Node*>(pIt->GetNode())->GetTaxId();
    }
    return tax_id;
}

int
CTaxon1::GetAllNames(int tax_id, TNameList& lNames, bool unique)
{
    int count(0);
    SetLastError(NULL);
    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetGetorgnames( tax_id );

    if( SendRequest( req, resp ) ) {
	if( resp.IsGetorgnames() ) {
	    // Correct response, return object
	    const list< CRef< CTaxon1_name > >& lNm( resp.GetGetorgnames() );
	    // Fill in the list
	    for( list< CRef< CTaxon1_name > >::const_iterator
		     i = lNm.begin();
		 i != lNm.end(); ++i, ++count )
		if( !unique ) {
		    lNames.push_back( (*i)->GetOname() );
		} else {
		    lNames.push_back( (*i)->IsSetUname() ?
				      (*i)->GetUname() :
				      (*i)->GetOname() );
		}
	} else { // Internal: wrong respond type
	    SetLastError( "Response type is not Getorgnames" );
	    return 0;
	}
    }

    return count;
}

/*---------------------------------------------
 * Find organism name in the string (for PDB mostly)
 * Returns: nimber of tax_ids found
 * NOTE:
 * 1. orgname is substring of search_str which matches organism name (return parameter).
 * 2. Ids consists of tax_ids. Caller is responsible to free this memory
 */
    //    int getTaxId4Str(const char* search_str, char** orgname, intPtr *Ids_out);

bool
CTaxon1::IsAlive(void)
{
    SetLastError(NULL);
    if( m_pServer ) {
	if( !m_pOut || !m_pOut->InGoodState() )
	    SetLastError( "Output stream is not in good state" );
	else if( !m_pIn || !m_pIn->InGoodState() )
	    SetLastError( "Input stream is not in good state" );
	else
	    return true;
    } else
	SetLastError( "Not connected to Taxonomy service" );
    return false;
}

bool
CTaxon1::GetTaxId4GI(int gi, int& tax_id_out )
{
    SetLastError(NULL);
    CTaxon1_req  req;
    CTaxon1_resp resp;
    
    req.SetId4gi( gi );

    if( SendRequest( req, resp ) ) {
	if( resp.IsId4gi() ) {
	    // Correct response, return object
	    tax_id_out = resp.GetId4gi();
	    return true;
	} else { // Internal: wrong respond type
	    SetLastError( "Response type is not Id4gi" );
	}
    }
    return false;
}

bool
CTaxon1::GetBlastName(int tax_id, std::string& blast_name_out )
{
    CTaxon1Node* pNode(NULL);
    SetLastError(NULL);
    if( m_plCache->LookupAndAdd( tax_id, &pNode ) && pNode ) {
	while( !pNode->IsRoot() ) {
	    if( !pNode->GetBlastName().empty() ) {
		blast_name_out.assign( pNode->GetBlastName() );
		return true;
	    }
	    pNode = pNode->GetParent();
	}
	blast_name_out.erase();
	return true;
    }
    return false;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * $Log$
 * Revision 6.1  2002/01/28 19:56:10  domrach
 * Initial checkin of the library implementation files
 *
 *
 * ===========================================================================
 */
