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
 *     NCBI Taxonomy information retreival library caching implementation
 *
 */
#include <objects/taxon1/taxon1.hpp>

#include "cache.hpp"

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

COrgRefCache::COrgRefCache( CTaxon1& host )
    : m_host( host ), m_nCacheCapacity( 10 )
{
}

bool
COrgRefCache::Init()
{
    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetMaxtaxid( true );

    if( m_host.SendRequest( req, resp ) ) {
	if( resp.IsMaxtaxid() ) {
	    // Correct response, return object
	    m_nMaxTaxId = resp.GetMaxtaxid();
	    m_nMaxTaxId += m_nMaxTaxId/10;
	    m_ppEntries = new CTaxon1Node*[m_nMaxTaxId];
	    memset( m_ppEntries, '\0', m_nMaxTaxId*sizeof(*m_ppEntries) );
	} else { // Internal: wrong respond type
	    m_host.SetLastError( "Response type is not Maxtaxid" );
	    return false;
	}
    } else {
	return false;
    }
    CTaxon1_name* pNode( new CTaxon1_name );
    pNode->SetTaxid( 1 );
    pNode->SetOname().assign("root");
    pNode->SetCde( 0x40000000 ); // Gene bank hidden
    m_tPartTree.SetRoot( new CTaxon1Node( pNode ) );
    return true;
}

bool
COrgRefCache::LookupAndAdd( int tax_id, CTaxon1Node** ppData )
{
    *ppData = NULL;
    if( (unsigned)tax_id < m_nMaxTaxId ) {
	CTaxon1Node* pNode( m_ppEntries[tax_id] );
	if( pNode ) {
	    *ppData = pNode;
	    return true;
	} else { // Add the entry from server
	    CTaxon1_req  req;
	    CTaxon1_resp resp;

	    req.SetTaxalineage( tax_id );

	    if( m_host.SendRequest( req, resp ) ) {
		if( resp.IsTaxalineage() ) {
		    // Correct response, return object
		    list< CRef< CTaxon1_name > >&
			lLin( resp.GetTaxalineage() );
		    CTaxon1Node* pParent( NULL );
		    CTaxon1Node* pNode;
		    list< CRef< CTaxon1_name > >::reverse_iterator i;
		    // Fill in storage
		    for( i = lLin.rbegin(); i != lLin.rend(); ++i ) {
			if( !m_ppEntries[ (*i)->GetTaxid() ] ) {
			    // Create node
			    break;
			} else {
			    pParent = m_ppEntries[ (*i)->GetTaxid() ];
			}
		    }
		    // Create tree iterator
		    CTreeIterator* pIt( m_tPartTree.GetIterator() );
		    if( !pParent ) {
			pParent = static_cast<CTaxon1Node*>(pIt->GetNode());
		    }
		    pIt->GoNode( pParent );
		    for( ; i != lLin.rend(); ++i ) {
			pNode = new CTaxon1Node(*i);
			m_ppEntries[ pNode->GetTaxId() ] = pNode;
			pIt->AddChild( pNode );
			pIt->GoNode( pNode );
		    }
		    *ppData = pNode;
		    return true;
		} else { // Internal: wrong respond type
		    m_host.SetLastError( "Unable to get node lineage:\
 Response type is not Taxalineage" );
		    return false;
		}
	    }
	}
    }
    return false;
}

bool
COrgRefCache::LookupAndInsert( int tax_id, CTaxon1_data** ppData )
{
    CTaxon1Node* pNode( NULL );
    *ppData = NULL;

    if( LookupAndAdd( tax_id, &pNode ) && pNode ) {
	SCacheEntry* pEntry( pNode->GetEntry() );
	if( !pEntry ) {
	    if( !Insert1( *pNode ) )
		return false;
	    pEntry = pNode->GetEntry();
	} else {
	    m_lCache.remove( pEntry );
	    m_lCache.push_front( pEntry );
	}
	*ppData = pEntry->GetData1();
	return true;
    }
    return false;
}

bool
COrgRefCache::LookupAndInsert( int tax_id, CTaxon2_data** ppData )
{
    CTaxon1Node* pNode( NULL );
    *ppData = NULL;

    if( LookupAndAdd( tax_id, &pNode ) && pNode ) {
	SCacheEntry* pEntry( pNode->GetEntry() );
	if( !pEntry ) {
	    if( !Insert2( *pNode ) )
		return false;
	    pEntry = pNode->GetEntry();
	} else {
	    m_lCache.remove( pEntry );
	    m_lCache.push_front( pEntry );
	}
	*ppData = pEntry->GetData2();
	return true;
    }
    return false;
}

bool
COrgRefCache::Lookup( int tax_id, CTaxon1_data** ppData )
{
    if( (unsigned)tax_id < m_nMaxTaxId ) {
	CTaxon1Node* pNode( m_ppEntries[tax_id] );
	SCacheEntry* pEntry;
	if( pNode && (pEntry=pNode->GetEntry()) ) {
	    // Move in the list
	    m_lCache.remove( pEntry );
	    m_lCache.push_front( pEntry );
	    *ppData = pEntry->GetData1();
	    return true;
	}
    }
    *ppData = NULL;
    return false;
}

bool
COrgRefCache::Lookup( int tax_id, CTaxon2_data** ppData )
{
    if( (unsigned)tax_id < m_nMaxTaxId ) {
	CTaxon1Node* pNode( m_ppEntries[tax_id] );
	SCacheEntry* pEntry;
	if( pNode && (pEntry=pNode->GetEntry()) ) {
	    // Move in the list
	    m_lCache.remove( pEntry );
	    m_lCache.push_front( pEntry );
	    *ppData = pEntry->GetData2();
	    return true;
	}
    }
    *ppData = NULL;
    return false;
}

bool
s_BuildLineage( string& str, CTaxon1Node* pNode, unsigned sz, int sp_rank )
{
    if( !pNode->IsRoot() ) {
	if( pNode->GetRank() > sp_rank-1 ) {
	    s_BuildLineage( str, pNode->GetParent(), 0, sp_rank );
	    return false;
	} else {
	    if( pNode->IsGBHidden() ) {
		return s_BuildLineage( str, pNode->GetParent(), sz, sp_rank );
	    }
	    bool bCont;
	    bCont=s_BuildLineage( str, pNode->GetParent(),
				  sz+pNode->GetName().size()+2, sp_rank );
	    if( bCont ) {
		str.append( pNode->GetName() ).append( "; " );
	    }
	    return bCont;
	}
    } else {
	str.reserve( sz );
    }
    return true;
}

std::string::size_type
s_AfterPrefix( const string& str1, const string& prefix )
{
    std::string::size_type pos(0);
    if( NStr::StartsWith( str1, prefix ) ) {
	pos += prefix.size();
    }
    return str1.find_first_not_of( " \t\n\r", pos );
}

bool
COrgRefCache::BuildOrgModifier( CTaxon1Node* pNode,
				list< CRef<COrgMod> >& lMod,
				CTaxon1Node* pParent )
{
    CTaxon1Node* pTmp;
    CRef<COrgMod> pMod( new COrgMod );

    if( !pParent && !pNode->IsRoot() ) {
	pTmp = pNode->GetParent();
	while( !pTmp->IsRoot() ) {
	    int prank( pTmp->GetRank() );
	    if((prank == GetSubspeciesRank()) || 
	       (prank == GetSpeciesRank()) ||
	       (prank == GetGenusRank())) {
		pParent = pTmp;
		break;
	    }
	    pTmp = pTmp->GetParent();
	}
    }
    std::string::size_type pos = 0;
    if( pParent ) { // Get rid of parent prefix
	pos = s_AfterPrefix( pNode->GetName(),
			     pParent->GetName() );
    }
    pMod->SetSubname().assign( pNode->GetName(), pos,
			       pNode->GetName().size()-pos );
    int rank = pNode->GetRank();
    if( rank == GetSubspeciesRank()) {
	pMod->SetSubtype( COrgMod_Base::eSubtype_sub_species );
    } else if( rank == GetVarietyRank()) {
	pMod->SetSubtype( COrgMod_Base::eSubtype_variety );
    } else if( rank == GetFormaRank()
	       || (pParent
		   && rank == GetSubspeciesRank()) ) {
	pMod->SetSubtype( COrgMod_Base::eSubtype_strain );
    }  else {
	pMod->SetSubtype( COrgMod_Base::eSubtype_other );
    }
    // Store it into list
    lMod.push_back( pMod );

    return true;
}

bool
COrgRefCache::SetBinomialName( CTaxon1Node& node, COrgName& on )
{
    CTaxon1Node* pSpec( NULL );
    CTaxon1Node* pSubspec( NULL );
    CTaxon1Node* pGenus( NULL );
    CTaxon1Node* pSubgen( NULL );
    CTaxon1Node* pNode( &node );
    std::string::size_type pos(0);
    do {
	int rank( pNode->GetRank() );
	if( rank == GetSubspeciesRank())
	    pSubspec = pNode;
	else if( rank == GetSpeciesRank())
	    pSpec = pNode;
	else if( rank == GetSubgenusRank())
	    pSubgen = pNode;
	else if(rank == GetGenusRank()) {
	    pGenus = pNode;
	    break;
	}
	pNode = pNode->GetParent();
    } while( pNode && !pNode->IsRoot() );
    pNode = &node;

    if( !pGenus ) {
	if( !pSubgen )
	    return false;
	else
	    pGenus = pSubgen;
    }
    CBinomialOrgName& bon( on.SetName().SetBinomial() );

    bon.SetGenus( pGenus->GetName() );

    if( pSpec ) { // we have a species in lineage
	bon.SetSpecies().assign( pSpec->GetName(),
				 pos=s_AfterPrefix( pSpec->GetName(),
						    pGenus->GetName() ),
				 pSpec->GetName().size() - pos );

	if( pSubspec ) { // we also have a subspecies in lineage
	    bon.SetSubspecies().assign( pSubspec->GetName(),
					pos=s_AfterPrefix(pSubspec->GetName(),
							  pSpec->GetName()),
					pSubspec->GetName().size() - pos );
	}
	if( pNode != pSpec ) {
	    BuildOrgModifier( pNode, on.SetMod() );
	}
	return true;
    }
    // no species in lineage
    if( pSubspec ) { // we have no species but we have subspecies
	bon.SetSubspecies().assign( pSubspec->GetName(),
				    pos=s_AfterPrefix(pSubspec->GetName(),
						      pGenus->GetName()),
				    pSubspec->GetName().size() - pos );
	BuildOrgModifier( pNode, on.SetMod(),
			  pNode==pSubspec ? pGenus : pSubspec );
	return true;
    }
  
    // we have no species, no subspecies
    // but we are under species level (varietas or forma)
    BuildOrgModifier( pNode, on.SetMod(), pGenus );
    return true;
}

bool
COrgRefCache::SetPartialName( CTaxon1Node& node, COrgName& on )
{
    CTaxElement* pTaxElem( new CTaxElement );
    int rank_id= node.GetRank();
    
    CPartialOrgName& pon( on.SetName().SetPartial() );
    pon.Set().push_back( pTaxElem );

    if( rank_id == GetFamilyRank()) {
	pTaxElem->SetFixed_level( CTaxElement_Base::eFixed_level_family );
    }
    else if(rank_id == GetOrderRank()) {
	pTaxElem->SetFixed_level( CTaxElement_Base::eFixed_level_order );
    }
    else if(rank_id == GetClassRank()) {
	pTaxElem->SetFixed_level( CTaxElement_Base::eFixed_level_class );
    }
    else {
	pTaxElem->SetFixed_level( CTaxElement_Base::eFixed_level_other );
	pTaxElem->SetLevel( GetRankName( rank_id ) );
    }
    pTaxElem->SetName( node.GetName() );
    return true;
}

bool
COrgRefCache::BuildOrgRef( CTaxon1Node& node, COrg_ref& org, bool& is_species )
{
    // Init ranks here
    if( !InitRanks() || !InitNameClasses() || !InitDivisions() )
	return false;

    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetGetorgnames( node.GetTaxId() );

    if( m_host.SendRequest( req, resp ) ) {
	if( resp.IsGetorgnames() ) {
	    // Correct response, return object
	    list< CRef< CTaxon1_name > >&
		lLin( resp.GetGetorgnames() );
	    // Save taxname
	    org.SetTaxname().swap( lLin.front()->SetOname() );
	    lLin.pop_front();

	    list< CRef< CTaxon1_name > >::iterator i;
	    // Find preferred common name
	    int pref_cls(GetPreferredCommonNameClass());
	    for( i = lLin.begin(); i != lLin.end(); ++i ) {
		if( (*i)->GetCde() == pref_cls ) {
		    org.SetCommon().swap( (*i)->SetOname() );
		    lLin.erase( i );
		    break;
		}
	    }
	    int syn_cls(GetSynonymNameClass());
	    int comm_cls(GetCommonNameClass());
	    for( i = lLin.begin(); i != lLin.end(); ++i ) {
		int cls( (*i)->GetCde() );
		if( cls == syn_cls || cls == comm_cls ) {
		    org.SetSyn().push_back( (*i)->GetOname() );
		}
	    }
	    // Set taxid as db tag
	    org.SetTaxId( node.GetTaxId() );

	    COrgName& on( org.SetOrgname() );

	    short div_id( node.GetDivision() );
	    if( GetDivisionCode( div_id ) ) {
		on.SetDiv( GetDivisionCode( div_id ) );
	    }
	    on.SetGcode( node.GetGC() );
	    on.SetMgcode( node.GetMGC() );
	    // Build lineage
	    CTaxon1Node* pNode;
	    if( !node.IsRoot() ) {
		pNode = node.GetParent();
		s_BuildLineage( on.SetLineage(), pNode, 0, GetSpeciesRank() );
		if( on.GetLineage().empty() )
		    on.ResetLineage();
	    }
	    // Set rank
	    int rank_id( node.GetRank() );

	    is_species = (rank_id >= GetSpeciesRank());
	    // correct level by lineage if node has no rank
	    if(rank_id < 0 && !node.IsRoot() ) {
		pNode = node.GetParent();
		while( !pNode->IsRoot() ) {
		    int rank( pNode->GetRank() );
		    if(rank >= 0) {
			is_species= (rank >= GetSpeciesRank()-1);
			break;
		    }
		    pNode = pNode->GetParent();
		}
	    }
	    // Create name
	    if(is_species) {
		/* we are on species level or below */
	     
		/* check for viruses */
		if( div_id == GetVirusesDivision()
		    || div_id == GetPhagesDivision() ) {
		    /* this is a virus */
		     /* virus */
		    if( rank_id == GetSpeciesRank() ) {
			/* we are on species level */
			on.SetName().SetVirus( node.GetName() );
		    } else {
			/* we are below species */
			/* first try to find species or min rank which
			   below species but above us */
			CTaxon1Node* pNode( NULL );
			CTaxon1Node* pTmp( node.GetParent() );
			
			while( pTmp && !pTmp->IsRoot() ) {
			    int rank(pTmp->GetRank());
			    if( rank >= GetSpeciesRank() ) {
				pNode = pTmp;
				if( rank == GetSpeciesRank() )
				    break;
			    } else if( rank >= 0 )
				break;
			    pTmp = pTmp->GetParent();
			}
			if( !pNode ) {// we have species or something above us
			    pNode = &node;
			}
			on.SetName().SetVirus( pNode->GetName() );
			// Add modifier to orgname
			BuildOrgModifier( &node, on.SetMod() );
		    } // non species rank
		} else if( !SetBinomialName( node, on ) ) {
		    // name is not binomial: set partial
		    SetPartialName( node, on );
		}
	    } else { // above species
		SetPartialName( node, on );
	    }
	} else {
	    m_host.SetLastError("Unable to get orgref: Response is not Getorgnames");
	    return false;
	}
    } else
	return false;
    return true;
}

bool
COrgRefCache::Insert1( CTaxon1Node& node )
{
    bool is_species( false );
    struct SCacheEntry* pEntry( new SCacheEntry );
    pEntry->m_pTax1 = new CTaxon1_data;
    pEntry->m_pTax2 = NULL;
    pEntry->m_pTreeNode = &node;

    COrg_ref& org( pEntry->m_pTax1->SetOrg() );

    if( !BuildOrgRef( node, org, is_species ) ) {
	delete pEntry;
	return false;
    }
    // Set division code
    if( GetDivisionCode(node.GetDivision()) )
	pEntry->m_pTax1->SetDiv()
	    .assign( GetDivisionCode(node.GetDivision()) );
    // Set species level
    pEntry->m_pTax1->SetIs_species_level( is_species );
    // Remove last element from list
    if( m_lCache.size() >= m_nCacheCapacity ) {
	CTaxon1Node* pNode( m_lCache.back()->m_pTreeNode );
	pNode->m_cacheEntry = NULL;
	delete m_lCache.back();
	m_lCache.pop_back();
    }
    
    node.m_cacheEntry = pEntry;
    m_lCache.push_front( pEntry );

    return true;
}

bool
COrgRefCache::Insert2( CTaxon1Node& node )
{
    bool is_species( false );
    struct SCacheEntry* pEntry( new SCacheEntry );
    pEntry->m_pTax1 = NULL;
    pEntry->m_pTax2 = new CTaxon2_data;
    pEntry->m_pTreeNode = &node;

    pEntry->m_pTax2->SetIs_uncultured( node.IsUncultured() );

    COrg_ref& org( pEntry->m_pTax2->SetOrg() );

    if( !BuildOrgRef( node, org, is_species ) ) {
	delete pEntry;
	return false;
    }
    // Set blast names
    CTaxon1Node* pNode( &node );
    while( !pNode->IsRoot() ) {
	if( !pNode->GetBlastName().empty() ) {
	    pEntry->m_pTax2->SetBlast_name()
		.push_back( pNode->GetBlastName() );
	}
	pNode = pNode->GetParent();
    }
    // Set species level
    pEntry->m_pTax2->SetIs_species_level( is_species );
    // Remove last element from list
    if( m_lCache.size() >= m_nCacheCapacity ) {
	pNode = m_lCache.back()->m_pTreeNode;
	pNode->m_cacheEntry = NULL;
	delete m_lCache.back();
	m_lCache.pop_back();
    }
    
    node.m_cacheEntry = pEntry;
    m_lCache.push_front( pEntry );

    return true;
}

CTaxon1_data*
COrgRefCache::SCacheEntry::GetData1()
{
    if( ! m_pTax1 ) {
	m_pTax1 = new CTaxon1_data;
	if( m_pTax2->IsSetOrg() )
	    m_pTax1->SetOrg( m_pTax2->SetOrg() );
	if( m_pTax2->GetOrg().GetOrgname().IsSetDiv() )
	    m_pTax1->SetDiv( m_pTax2->GetOrg().GetOrgname().GetDiv() );
	else
	    m_pTax1->SetDiv();
	m_pTax1->SetIs_species_level(m_pTax2->GetIs_species_level());
    }
    return m_pTax1;
}

CTaxon2_data*
COrgRefCache::SCacheEntry::GetData2()
{
    if( ! m_pTax2 ) {
	m_pTax2 = new CTaxon2_data;
	if( m_pTax1->IsSetOrg() )
	    m_pTax2->SetOrg( m_pTax1->SetOrg() );
	CTaxon1Node* pNode( m_pTreeNode );
	while( !pNode->IsRoot() ) {
	    if( !pNode->GetBlastName().empty() ) {
		m_pTax2->SetBlast_name().push_back( pNode->GetBlastName() );
	    }
	    pNode = pNode->GetParent();
	}
	m_pTax2->SetIs_uncultured( m_pTreeNode->IsUncultured() );
	m_pTax2->SetIs_species_level(m_pTax1->GetIs_species_level());
    }
    return m_pTax2;
}

int
COrgRefCache::FindRankByName( const char* pchName ) const
{
    for( TRankMapCI ci = m_rankStorage.begin();
	 ci != m_rankStorage.end();
	 ++ci )
	if( ci->second.compare( pchName ) == 0 )
	    return ci->first;
    return -1000;
}


const char*
COrgRefCache::GetRankName( int rank ) const
{
    TRankMapCI ci( m_rankStorage.find( rank ) );
    if( ci != m_rankStorage.end() ) {
	return ci->second.c_str();
    }
    return NULL;
}

bool
COrgRefCache::InitRanks()
{
    if( m_rankStorage.size() == 0 ) {

	CTaxon1_req  req;
	CTaxon1_resp resp;

	req.SetGetranks( true );

	if( m_host.SendRequest( req, resp ) ) {
	    if( resp.IsGetranks() ) {
		// Correct response, return object
		const list< CRef< CTaxon1_info > >&
		    lRanks( resp.GetGetranks() );
		// Fill in storage
		for( list< CRef< CTaxon1_info > >::const_iterator
			 i = lRanks.begin();
		     i != lRanks.end(); ++i )
		    m_rankStorage
			.insert( TRankMap::value_type((*i)->GetIval1(),
						      (*i)->GetSval()) );
	    } else { // Internal: wrong respond type
		m_host.SetLastError( "Response type is not Getranks" );
		return false;
	    }
	}

	m_nFamilyRank = FindRankByName( "family" );
	if( m_nFamilyRank < -10 ) {
	    m_host.SetLastError( "Family rank was not found" );
	    return false;
	}
	m_nOrderRank = FindRankByName( "order" );
	if( m_nOrderRank < -10 ) {
	    m_host.SetLastError( "Order rank was not found" );
	    return false;
	}
	m_nClassRank = FindRankByName( "class" );
	if( m_nClassRank < -10 ) {
	    m_host.SetLastError( "Class rank was not found" );
	    return false;
	}
	m_nGenusRank = FindRankByName( "genus" );
	if( m_nGenusRank < -10 ) {
	    m_host.SetLastError( "Genus rank was not found" );
	    return false;
	}
	m_nSubgenusRank = FindRankByName( "subgenus" );
	if( m_nSubgenusRank < -10 ) {
	    m_host.SetLastError( "Subgenus rank was not found" );
	    return false;
	}
	m_nSpeciesRank = FindRankByName( "species" );
	if( m_nSpeciesRank < -10 ) {
	    m_host.SetLastError( "Species rank was not found" );
	    return false;
	}
	m_nSubspeciesRank = FindRankByName( "subspecies" );
	if( m_nSubspeciesRank < -10 ) {
	    m_host.SetLastError( "Subspecies rank was not found" );
	    return false;
	}
	m_nFormaRank = FindRankByName( "forma" );
	if( m_nFormaRank < -10 ) {
	    m_host.SetLastError( "Forma rank was not found" );
	    return false;
	}
	m_nVarietyRank = FindRankByName( "varietas" );
	if( m_nVarietyRank < -10 ) {
	    m_host.SetLastError( "Variety rank was not found" );
	    return false;
	}
    }
    return true;
}

const char*
COrgRefCache::GetNameClassName( short nc ) const
{
    TNameClassMapCI ci( m_ncStorage.find( nc ) );
    if( ci != m_ncStorage.end() ) {
	return ci->second.c_str();
    }
    return NULL;
}

short
COrgRefCache::FindNameClassByName( const char* pchName ) const
{
    for( TNameClassMapCI ci = m_ncStorage.begin();
	 ci != m_ncStorage.end();
	 ++ci )
	if( ci->second.compare( pchName ) == 0 )
	    return ci->first;
    return -1;
}

bool
COrgRefCache::InitNameClasses()
{
    if( m_ncStorage.size() == 0 ) {

	CTaxon1_req  req;
	CTaxon1_resp resp;

	req.SetGetcde( true );

	if( m_host.SendRequest( req, resp ) ) {
	    if( resp.IsGetcde() ) {
		// Correct response, return object
		const list< CRef< CTaxon1_info > >&
		    l( resp.GetGetcde() );
		// Fill in storage
		for( list< CRef< CTaxon1_info > >::const_iterator
			 i = l.begin();
		     i != l.end(); ++i )
		    m_ncStorage
			.insert( TNameClassMap::value_type((*i)->GetIval1(),
							   (*i)->GetSval()) );
	    } else { // Internal: wrong respond type
		m_host.SetLastError( "Response type is not Getcde" );
		return false;
	    }
	}

	m_ncPrefCommon = FindNameClassByName( "preferred common name" );
	if( m_ncPrefCommon < 0 ) {
	    m_host.SetLastError( "Preferred common name class was not found" );
	    return false;
	}
	m_ncCommon = FindNameClassByName( "common name" );
	if( m_ncCommon < 0 ) {
	    m_host.SetLastError( "Common name class was not found" );
	    return false;
	}
	m_ncSynonym = FindNameClassByName( "synonym" );
	if( m_ncSynonym < 0 ) {
	    m_host.SetLastError( "Synonym name class was not found" );
	    return false;
	}
    }
    return true;
}

short
COrgRefCache::FindDivisionByCode( const char* pchCode ) const
{
    for( TDivisionMapCI ci = m_divStorage.begin();
	 ci != m_divStorage.end();
	 ++ci ) {
	const char* cp( ci->second.m_sCode.c_str() );
	if( strcmp( cp, pchCode ) == 0 )
	    return ci->first;
    }
    return -1;
}


const char*
COrgRefCache::GetDivisionCode( short div_id ) const
{
    TDivisionMapCI ci( m_divStorage.find( div_id ) );
    if( ci != m_divStorage.end() ) {
	return ci->second.m_sCode.c_str();
    }
    return NULL;
}

bool
COrgRefCache::InitDivisions()
{
    if( m_divStorage.size() == 0 ) {

	CTaxon1_req  req;
	CTaxon1_resp resp;

	req.SetGetdivs( true );

	if( m_host.SendRequest( req, resp ) ) {
	    if( resp.IsGetdivs() ) {
		// Correct response, return object
		const list< CRef< CTaxon1_info > >&
		    l( resp.GetGetdivs() );
		// Fill in storage
		for( list< CRef< CTaxon1_info > >::const_iterator
			 i = l.begin();
		     i != l.end(); ++i ) {
		    SDivision& div( m_divStorage[(*i)->GetIval1()] );
		    div.m_sName.assign( (*i)->GetSval() );
		    int code = (*i)->GetIval2();
		    for(int k= 0; k < 3; k++) {
			div.m_sCode.append( 1U, (code >> (8*(3-k))) & 0xFF );
		    }
		    div.m_sCode.append( 1U, code & 0xFF );
		}
	    } else { // Internal: wrong response type
		m_host.SetLastError( "Response type is not Getdivs" );
		return false;
	    }
	}

	if( (m_divViruses = FindDivisionByCode( "VRL" )) < 0 ) {
	    m_host.SetLastError( "Viruses division was not found" );
	    return false;
	}
	if( (m_divPhages = FindDivisionByCode( "PHG" )) < 0 ) {
	    m_host.SetLastError( "Phages division was not found" );
	    return false;
	}
    }
    return true;
}

void
COrgRefCache::SetIndexEntry( int id, CTaxon1Node* pNode )
{
    m_ppEntries[id] = pNode;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
/*
 * $Log$
 * Revision 6.3  2002/01/29 17:17:45  domrach
 * Confusion with list/set resolved
 *
 * Revision 6.2  2002/01/28 21:37:02  domrach
 * list changed to multiset in BuildOrgRef()
 *
 * Revision 6.1  2002/01/28 19:56:10  domrach
 * Initial checkin of the library implementation files
 *
 *
 * ===========================================================================
 */

