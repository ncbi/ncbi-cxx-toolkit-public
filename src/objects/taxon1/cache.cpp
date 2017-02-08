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

#include <ncbi_pch.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>

#include <objects/taxon1/taxon1.hpp>
#include "cache.hpp"

#include <vector>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


COrgRefCache::COrgRefCache( CTaxon1& host )
    : m_host( host ), m_ppEntries( 0 ), m_nCacheCapacity( 10 )
{
    return;
}

COrgRefCache::~COrgRefCache()
{
    delete[] m_ppEntries;
    for( list<SCacheEntry*>::iterator i = m_lCache.begin();
         i != m_lCache.end();
         ++i ) {
        delete *i;
    }
}

bool
COrgRefCache::Init( unsigned nCapacity )
{
    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetMaxtaxid();

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
    CTaxon1_name* pNode = ( new CTaxon1_name );
    pNode->SetTaxid( 1 );
    pNode->SetOname().assign("root");
    pNode->SetCde( 0x40000000 ); // Gene bank hidden
    CTaxon1Node* pRoot = new CTaxon1Node( CRef<CTaxon1_name>(pNode) );
    m_tPartTree.SetRoot( pRoot );
    SetIndexEntry( 1, pRoot );

    if( nCapacity != 0 ) {
        m_nCacheCapacity = nCapacity;
    }
//    InitRanks();
//    InitDivisions();
    return true;
}

bool
COrgRefCache::Lookup( TTaxId tax_id, CTaxon1Node** ppNode )
{
    if( (unsigned)tax_id < m_nMaxTaxId ) {
        *ppNode = m_ppEntries[tax_id];
    } else {
        *ppNode = NULL;
    }
    return *ppNode != NULL;
}

bool
COrgRefCache::LookupAndAdd( TTaxId tax_id, CTaxon1Node** ppData )
{
    *ppData = NULL;
    if( (unsigned)tax_id < m_nMaxTaxId ) {
        CTaxon1Node* pNode = ( m_ppEntries[tax_id] );
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
                    list< CRef<CTaxon1_name> >& lLin = resp.SetTaxalineage();
                    CTaxon1Node* pParent = 0;
                    pNode   = 0;
                    // Check if this is a secondary node
                    if( lLin.front()->GetTaxid() != tax_id ) {
                        // Secondary node, try to get primary from index
                        pNode = m_ppEntries[ lLin.front()->GetTaxid() ];
                    }
                    if( !pNode ) {
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
                        CTreeIterator* pIt = ( m_tPartTree.GetIterator() );
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
                    } else { // Store secondary in index
                        m_ppEntries[ tax_id ] = pNode;
                    }
                    _ASSERT( pNode );
                    *ppData = pNode;
                    return true;
                } else { // Internal: wrong respond type
                    m_host.SetLastError( "Unable to get node lineage: "
					 "Response type is not Taxalineage" );
                    return false;
                }
            }
        }
    }
    return false;
}

bool
COrgRefCache::LookupAndInsert( TTaxId tax_id, CTaxon2_data** ppData )
{
    CTaxon1Node* pNode = ( NULL );
    *ppData = NULL;

    if( LookupAndAdd( tax_id, &pNode ) && pNode ) {
        SCacheEntry* pEntry = ( pNode->GetEntry() );
        if( !pEntry ) {
            if( !Insert2( *pNode ) )
                return false;
            pEntry = pNode->GetEntry();
        } else {
            m_lCache.remove( pEntry );
            m_lCache.push_front( pEntry );
        }
        *ppData = pEntry->GetData();
        return true;
    }
    return false;
}

bool
COrgRefCache::Lookup( TTaxId tax_id, CTaxon2_data** ppData )
{
    if( (unsigned)tax_id < m_nMaxTaxId ) {
        CTaxon1Node* pNode = ( m_ppEntries[tax_id] );
        SCacheEntry* pEntry;
        if( pNode && (pEntry=pNode->GetEntry()) ) {
            // Move in the list
            m_lCache.remove( pEntry );
            m_lCache.push_front( pEntry );
            *ppData = pEntry->GetData();
            return true;
        }
    }
    *ppData = NULL;
    return false;
}

bool
COrgRefCache::Insert2( CTaxon1Node& node )
{
    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetLookup().SetTaxId( node.GetTaxId() );
    // Set version db tag
    COrgrefProp::SetOrgrefProp( req.SetLookup(), "version", 2 );

    if( m_host.SendRequest( req, resp ) ) {
        if( resp.IsLookup() ) {
            // Correct response, return object
	    struct SCacheEntry* pEntry = ( new SCacheEntry );
	    pEntry->m_pTax2 = new CTaxon2_data();
	    pEntry->m_pTreeNode = &node;
	    
            SerialAssign< COrg_ref >( pEntry->m_pTax2->SetOrg(), resp.GetLookup().GetOrg() );
	    m_host.x_ConvertOrgrefProps( *pEntry->m_pTax2 );

	    // Remove last element from list
	    if( m_lCache.size() >= m_nCacheCapacity ) {
		CTaxon1Node* pNode = m_lCache.back()->m_pTreeNode;
		pNode->m_cacheEntry = NULL;
		delete m_lCache.back();
		m_lCache.pop_back();
	    }
    
	    node.m_cacheEntry = pEntry;
	    m_lCache.push_front( pEntry );

	    return true;

        } else { // Internal: wrong respond type
            m_host.SetLastError( "Response type is not Lookup" );
        }
    }

    return false;
}

TTaxRank
COrgRefCache::FindRankByName( const char* pchName )
{
    if( InitRanks() )
      for( TRankMapCI ci = m_rankStorage.begin();
         ci != m_rankStorage.end();
         ++ci )
        if( ci->second.compare( pchName ) == 0 )
            return ci->first;
    return -1000;
}


const char*
COrgRefCache::GetRankName( int rank )
{
    if( InitRanks() ) {
        TRankMapCI ci( m_rankStorage.find( rank ) );
        if( ci != m_rankStorage.end() ) {
            return ci->second.c_str();
        }
    }
    return NULL;
}

bool
COrgRefCache::InitRanks()
{
    if( m_rankStorage.size() == 0 ) {

        CTaxon1_req  req;
        CTaxon1_resp resp;

        req.SetGetranks();

        if( m_host.SendRequest( req, resp ) ) {
            if( resp.IsGetranks() ) {
                // Correct response, return object
                const list< CRef< CTaxon1_info > >&
                    lRanks = ( resp.GetGetranks() );
                // Fill in storage
                for( list< CRef< CTaxon1_info > >::const_iterator
                         i = lRanks.begin();
                     i != lRanks.end(); ++i ) {
                    m_rankStorage
                        .insert( TRankMap::value_type((*i)->GetIval1(),
                                                      (*i)->GetSval()) );
                }
            } else { // Internal: wrong respond type
                m_host.SetLastError( "Response type is not Getranks" );
                return false;
            }
        }

        m_nSuperkingdomRank = FindRankByName( "superkingdom" );
        if( m_nSuperkingdomRank < -10 ) {
            m_host.SetLastError( "Superkingdom rank was not found" );
            return false;
        }
        m_nGenusRank = FindRankByName( "genus" );
        if( m_nGenusRank < -10 ) {
            m_host.SetLastError( "Genus rank was not found" );
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
    }
    return true;
}

const char*
COrgRefCache::GetNameClassName( short nc )
{
    if( !InitNameClasses() ) return NULL;
    TNameClassMapCI ci( m_ncStorage.find( nc ) );
    if( ci != m_ncStorage.end() ) {
        return ci->second.c_str();
    }
    return NULL;
}

TTaxNameClass
COrgRefCache::FindNameClassByName( const char* pchName )
{
    if( !InitNameClasses() ) return -1;
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

        req.SetGetcde();

        if( m_host.SendRequest( req, resp ) ) {
            if( resp.IsGetcde() ) {
                // Correct response, return object
                const list< CRef< CTaxon1_info > >&
                    l = ( resp.GetGetcde() );
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

        m_ncPrefCommon = FindNameClassByName( "genbank common name" );
        if( m_ncPrefCommon < 0 ) {
            m_host.SetLastError( "Genbank common name class was not found" );
            return false;
        }
        m_ncCommon = FindNameClassByName( "common name" );
        if( m_ncCommon < 0 ) {
            m_host.SetLastError( "Common name class was not found" );
            return false;
        }
    }
    return true;
}

TTaxDivision
COrgRefCache::FindDivisionByCode( const char* pchCode )
{
    if( !InitDivisions() ) return -1;
    for( TDivisionMapCI ci = m_divStorage.begin();
         ci != m_divStorage.end();
         ++ci ) {
        const char* cp = ( ci->second.m_sCode.c_str() );
        if( strcmp( cp, pchCode ) == 0 )
            return ci->first;
    }
    return -1;
}


const char*
COrgRefCache::GetDivisionCode( short div_id )
{
    if( !InitDivisions() ) return NULL;
    TDivisionMapCI ci( m_divStorage.find( div_id ) );
    if( ci != m_divStorage.end() ) {
        return ci->second.m_sCode.c_str();
    }
    return NULL;
}

const char*
COrgRefCache::GetDivisionName( short div_id )
{
    if( !InitDivisions() ) return NULL;
    TDivisionMapCI ci( m_divStorage.find( div_id ) );
    if( ci != m_divStorage.end() ) {
        return ci->second.m_sName.c_str();
    }
    return NULL;
}


bool
COrgRefCache::InitDivisions()
{
    if( m_divStorage.size() == 0 ) {

        CTaxon1_req  req;
        CTaxon1_resp resp;

        req.SetGetdivs();

        if( m_host.SendRequest( req, resp ) ) {
            if( resp.IsGetdivs() ) {
                // Correct response, return object
                const list< CRef< CTaxon1_info > >&
                    l = ( resp.GetGetdivs() );
                // Fill in storage
                for( list< CRef< CTaxon1_info > >::const_iterator
                         i = l.begin();
                     i != l.end(); ++i ) {
                    SDivision& div = ( m_divStorage[(*i)->GetIval1()] );
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
    }
    return true;
}

void
COrgRefCache::SetIndexEntry( int id, CTaxon1Node* pNode )
{
    m_ppEntries[id] = pNode;
}

//=======================================================
//
//   Iterators implementation
//
bool
CTaxTreeConstIterator::IsLastChild() const
{
    const CTreeContNodeBase* pOldNode = m_it->GetNode();
    bool bResult = true;

    while( m_it->GoParent() ) {
        if( IsVisible( m_it->GetNode() ) ) {
            const CTreeContNodeBase* pParent = m_it->GetNode();
            m_it->GoNode( pOldNode );
            while( m_it->GetNode() != pParent ) {
                if( m_it->GoSibling() ) {
                    bResult = !NextVisible( pParent );
                    break;
                }
                if( !m_it->GoParent() ) {
                    break;
                }
            }
            break;
        }
    }
    m_it->GoNode( pOldNode );
    return bResult;
}

bool
CTaxTreeConstIterator::IsFirstChild() const
{
    const CTreeContNodeBase* pOldNode = m_it->GetNode();
    bool bResult = false;

    while( m_it->GoParent() ) {
        if( IsVisible( m_it->GetNode() ) ) {
            const CTreeContNodeBase* pParent = m_it->GetNode();
            if( m_it->GoChild() ) {
                bResult = NextVisible(pParent) && m_it->GetNode() == pOldNode;
            }
            break;
        }
    }
    m_it->GoNode( pOldNode );
    return bResult;
}

bool
CTaxTreeConstIterator::IsTerminal() const
{
    const CTreeContNodeBase* pOldNode = m_it->GetNode();

    if( m_it->GoChild() ) {
        bool bResult = NextVisible( pOldNode );
        m_it->GoNode( pOldNode );
        return !bResult;
    }
    return true;
}

bool
CTaxTreeConstIterator::NextVisible( const CTreeContNodeBase* pParent ) const
{
    if( m_it->GetNode() == pParent ) {
        return false;
    }
 next:
    if( IsVisible( m_it->GetNode() ) ) {
        return true;
    }
    if( m_it->GoChild() ) {
        goto next;
    } else if( m_it->GoSibling() ) {
        goto next;
    } else {
        while( m_it->GoParent() && m_it->GetNode() != pParent ) {
            if( m_it->GoSibling() ) {
                goto next;
            }
        }
    }
    return false;
}

bool
CTaxTreeConstIterator::GoParent()
{
    const CTreeContNodeBase* pOldNode = m_it->GetNode();
    bool bResult = false;
    while( m_it->GoParent() ) {
        if( IsVisible( m_it->GetNode() ) ) {
            bResult = true;
            break;
        }
    }
    if( !bResult ) {
        m_it->GoNode( pOldNode );
    }
    return bResult;
}

bool
CTaxTreeConstIterator::GoChild()
{
    const CTreeContNodeBase* pOldNode = m_it->GetNode();
    bool bResult = false;
    
    if( m_it->GoChild() ) {
        bResult = NextVisible( pOldNode );
    }
    if( !bResult ) {
        m_it->GoNode( pOldNode );
    }
    return bResult;
}

bool
CTaxTreeConstIterator::GoSibling()
{
    const CTreeContNodeBase* pOldNode = m_it->GetNode();
    bool bResult = false;

    if( GoParent() ) {
        const CTreeContNodeBase* pParent = m_it->GetNode();
        m_it->GoNode( pOldNode );
        while( m_it->GetNode() != pParent ) {
            if( m_it->GoSibling() ) {
                bResult = NextVisible( pParent );
                break;
            }
            if( !m_it->GoParent() ) {
                break;
            }
        }
        if( !bResult ) {
            m_it->GoNode( pOldNode );
        }
    }
    return bResult;
}

bool
CTaxTreeConstIterator::GoNode( const ITaxon1Node* pNode )
{
    const CTreeContNodeBase* pTaxNode = CastIC( pNode );

    if( pNode && IsVisible( pTaxNode ) ) {
        return m_it->GoNode( pTaxNode );
    }
    return false;
}

bool
CTaxTreeConstIterator::GoAncestor(const ITaxon1Node* pINode)
{
    const CTreeContNodeBase* pNode = CastIC( pINode );
    if( pNode && IsVisible( pNode ) ) {
        const CTreeContNodeBase* pOldNode = m_it->GetNode();
    
        vector< const CTreeContNodeBase* > v;
        do {
            v.push_back( m_it->GetNode() );
        } while( GoParent() );

        m_it->GoNode( pNode );
        vector< const CTreeContNodeBase* >::const_iterator vi;
        do {
            vi = find( v.begin(), v.end(), m_it->GetNode() );
            if( vi != v.end() ) {
                return true;
            }
        } while( GoParent() );
        // Restore old position
        m_it->GoNode( pOldNode );
    }
    return false;
}

bool
CTaxTreeConstIterator::BelongSubtree(const ITaxon1Node* pIRoot) const
{
    const CTreeContNodeBase* pRoot = CastIC( pIRoot );
    if( pRoot && IsVisible( pRoot ) ) {
        const CTreeContNodeBase* pOldNode = m_it->GetNode();
        do {
            if( IsVisible( m_it->GetNode() ) ) {
                if( m_it->GetNode() == pRoot ) {
                    m_it->GoNode( pOldNode );
                    return true;
                }
            }
        } while( m_it->GoParent() );
        m_it->GoNode( pOldNode );
    }
    return false;
}

// check if given node belongs to subtree pointed by cursor
bool
CTaxTreeConstIterator::AboveNode(const ITaxon1Node* pINode) const
{
    const CTreeContNodeBase* pNode = CastIC( pINode );
    if( pNode == m_it->GetNode() ) { // Node is not above itself
        return false;
    }

    if( pNode && IsVisible( pNode ) ) {
        const CTreeContNodeBase* pOldNode = m_it->GetNode();
        m_it->GoNode( pNode );
        do {
            if( IsVisible( m_it->GetNode() ) ) {
                if( m_it->GetNode() == pOldNode ) {
                    m_it->GoNode( pOldNode );
                    return true;
                }
            }
        } while( m_it->GoParent() );
        m_it->GoNode( pOldNode );
    }
    return false;
}

bool
CTreeLeavesBranchesIterator::IsVisible( const CTreeContNodeBase* pNode ) const
{
    return pNode &&
        ( pNode->IsRoot() || pNode->IsTerminal() ||
          !pNode->Child()->IsLastChild() );
}

bool
CTreeBestIterator::IsVisible( const CTreeContNodeBase* pNode ) const
{
    return pNode &&
        ( pNode->IsRoot() || pNode->IsTerminal() ||
          !pNode->Child()->IsLastChild() ||
          !(pNode->IsLastChild() && pNode->IsFirstChild()) );

}

bool
CTreeBlastIterator::IsVisible( const CTreeContNodeBase* pNode ) const
{
    return pNode && ( pNode->IsRoot() ||
                      !CastCI(pNode)->GetBlastName().empty() );
}


END_objects_SCOPE
END_NCBI_SCOPE
