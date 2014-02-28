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
    InitRanks();
    InitDivisions();
    return true;
}

bool
COrgRefCache::Lookup( int tax_id, CTaxon1Node** ppNode )
{
    if( (unsigned)tax_id < m_nMaxTaxId ) {
        *ppNode = m_ppEntries[tax_id];
    } else {
        *ppNode = NULL;
    }
    return *ppNode != NULL;
}

bool
COrgRefCache::LookupAndAdd( int tax_id, CTaxon1Node** ppData )
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
    CTaxon1Node* pNode = ( NULL );
    *ppData = NULL;

    if( LookupAndAdd( tax_id, &pNode ) && pNode ) {
        SCacheEntry* pEntry = ( pNode->GetEntry() );
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
        *ppData = pEntry->GetData2();
        return true;
    }
    return false;
}

bool
COrgRefCache::Lookup( int tax_id, CTaxon1_data** ppData )
{
    if( (unsigned)tax_id < m_nMaxTaxId ) {
        CTaxon1Node* pNode = ( m_ppEntries[tax_id] );
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
        CTaxon1Node* pNode = ( m_ppEntries[tax_id] );
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
//         if( pNode->GetRank() > sp_rank-1 ) {
//             s_BuildLineage( str, pNode->GetParent(), 0, sp_rank );
//             return false;
//         } else {
	if( pNode->IsGenBankHidden() ) {
	    return s_BuildLineage( str, pNode->GetParent(), sz, sp_rank );
	}
	bool bCont;
	bCont=s_BuildLineage( str, pNode->GetParent(),
			      sz+pNode->GetName().size()+2, sp_rank );
	if( bCont ) {
	    str.append( pNode->GetName() );
	    if( sz != 0 ) {
		str.append( "; " );
	    }
	}
	return bCont;
//         }
    } else {
        str.reserve( sz );
    }
    return true;
}

string::size_type
s_AfterPrefix( const string& str1, const string& prefix )
{
    string::size_type pos(0), result(string::npos);
    if( NStr::StartsWith( str1, prefix ) ) {
        pos += prefix.size();
	if( pos < str1.size() ) {
	    result = str1.find_first_not_of( " \t\n\r", pos );
	    if( result == pos ) { // fail in word-for-word comparison
		result = string::npos;
	    }
	}
    }
    return result;
}

static const char s_achSubsp[] = "subsp.";
static const char s_achSsp[] = "ssp.";
static const char s_achF_Sp[] = "f. sp.";
static const char s_achFSp[] = "f.sp.";
static const char s_achStr[] = "str.";
static const char s_achSubstr[] = "substr.";
static const char s_achVar[] = "var.";
static const char s_achSv[] = "sv.";
static const char s_achCv[] = "cv.";
static const char s_achPv[] = "pv.";
static const char s_achBv[] = "bv.";
static const char s_achF[] = "f.";
static const char s_achFo[] = "fo.";
static const char s_achGrp[] = "grp.";

struct SSubtypeAbbr {
    const char*       m_pchAbbr;
    size_t            m_nAbbrLen;
    COrgMod::ESubtype m_eSubtype;
};


static SSubtypeAbbr s_aSubtypes[] = {
    { s_achSubsp, sizeof(s_achSubsp)-1, COrgMod::eSubtype_sub_species },
    { s_achSsp,   sizeof(s_achSsp)-1,   COrgMod::eSubtype_sub_species },
    { s_achF_Sp,  sizeof(s_achF_Sp)-1,  COrgMod::eSubtype_forma_specialis },
    { s_achFSp,   sizeof(s_achFSp)-1,   COrgMod::eSubtype_forma_specialis },
    { s_achStr,   sizeof(s_achStr)-1,   COrgMod::eSubtype_strain },
    { s_achSubstr,sizeof(s_achSubstr)-1,COrgMod::eSubtype_substrain },
    { s_achVar,   sizeof(s_achVar)-1,   COrgMod::eSubtype_variety },
    { s_achSv,    sizeof(s_achSv)-1,    COrgMod::eSubtype_serovar },
    { s_achCv,    sizeof(s_achCv)-1,    COrgMod::eSubtype_cultivar },
    { s_achPv,    sizeof(s_achPv)-1,    COrgMod::eSubtype_pathovar },
    { s_achBv,    sizeof(s_achBv)-1,    COrgMod::eSubtype_biovar },
    { s_achF,     sizeof(s_achF)-1,     COrgMod::eSubtype_forma },
    { s_achFo,    sizeof(s_achFo)-1,    COrgMod::eSubtype_forma },
    { s_achGrp,   sizeof(s_achGrp)-1,   COrgMod::eSubtype_group },
    { NULL,       0,                    COrgMod::eSubtype_other }
};

static int
s_NofTokens( const string& s )
{
    int nof = 0;
    char first, last, c;
    int bracket_level, token;

    if( !s.empty() ) {
        string::size_type pos = 0;
        while( pos < s.size() ) {
            bracket_level= 0;
            token = 0;

            do { // Skip heading white space
                first= s[pos++];
            } while( (isspace((unsigned char) first) || iscntrl((unsigned char) first)) &&
                     pos < s.size() );
	    
            switch( first ) {
            case '"': last= '"'; break;
            case '(': last= ')'; break;
            case '{': last= '}'; break;
            case '[': last= ']'; break;
            default:  last= 0;   break;
            }

            for(; pos < s.size(); ++pos) {
                c = s[pos];
                if( !isalnum((unsigned char) c) ) {
                    if( last != 0 ) {
                        if( first == c ) {
                            ++bracket_level;
                        }
                        if( last == c && (!bracket_level--) ) {
                            ++pos;
                            break;
                        }
                    } else {
                        if( c == '.' || isspace((unsigned char) c) || iscntrl((unsigned char) c) ) {
                            ++pos;
                            break;
                        }
                    }
                } else {
                    token = 1;
                }
            }
            nof += token;
        }
    }            
    return nof;
}

COrgMod::ESubtype
COrgRefCache::GetSubtypeFromName( string& sName )
{
    static const char* s_sSubspCf  = " subsp. cf.";
    static const char* s_sSubspAff = " subsp. aff.";
    static const char* s_sCf       = " cf.";
    static const char* s_sAff      = " aff.";

    string::size_type pos;
    if( sName.find('.') == string::npos ) {
        return COrgMod::eSubtype_other;
    }
    /* ignore subsp. cf. and subsp. aff. */
    if( NStr::FindNoCase( sName, s_sSubspCf ) != string::npos ) {
        return COrgMod::eSubtype_other;
    }
    if( NStr::FindNoCase( sName, s_sSubspAff ) != string::npos ) {
        return COrgMod::eSubtype_other;
    }
    /* ignore cf. and aff. */
    if( NStr::FindNoCase( sName, s_sCf ) != string::npos ) {
	return COrgMod::eSubtype_other;
    }
    if( NStr::FindNoCase( sName, s_sAff ) != string::npos ) {
	return COrgMod::eSubtype_other;
    }

    /* check for subsp */
    SSubtypeAbbr* pSubtypeAbbr = &s_aSubtypes[0];
    while( pSubtypeAbbr->m_eSubtype != COrgMod::eSubtype_other ) {
	if( (pos=NStr::FindNoCase( sName,
	       string(pSubtypeAbbr->m_pchAbbr,
		      pSubtypeAbbr->m_nAbbrLen) )) != NPOS ) {
	    if( pos == 0 || sName[pos-1] == ' ' || sName[pos-1] == '\t' ) {
		sName.erase( pos, pSubtypeAbbr->m_nAbbrLen );
		sName = NStr::TruncateSpaces( sName, NStr::eTrunc_Begin );
		if( pSubtypeAbbr->m_eSubtype == COrgMod::eSubtype_sub_species
		    && s_NofTokens( sName ) != 1 ) {
		    break; // Return other
		}
		return pSubtypeAbbr->m_eSubtype;
	    }
	}
	++pSubtypeAbbr;
    }
    return COrgMod::eSubtype_other;
}

bool
COrgRefCache::BuildOrgModifier( CTaxon1Node* pNode,
                                COrgName& on,
                                CTaxon1Node* pParent )
{
    CTaxon1Node* pTmp;
    CRef<COrgMod> pMod( new COrgMod );

    if( !pParent && !pNode->IsRoot() ) {
        pTmp = pNode->GetParent();
        while( !pTmp->IsRoot() ) {
            int prank = pTmp->GetRank();
            if((prank == GetSubspeciesRank()) || 
               (prank == GetSpeciesRank()) ||
               (prank == GetGenusRank())) {
                pParent = pTmp;
                break;
            }
            pTmp = pTmp->GetParent();
        }
    }
    string::size_type pos = 0;
    if( pParent ) { // Get rid of parent prefix
        pos = s_AfterPrefix( pNode->GetName(),
                             pParent->GetName() );
	if( pos == string::npos ) {
	    return false;
	}
    }
    pMod->SetSubname().assign( pNode->GetName(), pos,
                               pNode->GetName().size()-pos );

    pMod->SetSubtype( GetSubtypeFromName( pMod->SetSubname() ) );

    if( pMod->GetSubtype() == COrgMod_Base::eSubtype_sub_species &&
	(pNode->GetRank() != GetSubspeciesRank() ||
	 s_NofTokens( pMod->GetSubname() ) != 1) ) {
        pMod->SetSubtype( COrgMod_Base::eSubtype_other );
    }
    if( pMod->GetSubtype() == COrgMod_Base::eSubtype_variety &&
	(pNode->GetRank() != GetVarietyRank() ||
	 s_NofTokens( pMod->GetSubname() ) != 1) ) {
	pMod->SetSubtype( COrgMod_Base::eSubtype_other );
    }
    if( pMod->GetSubtype() == COrgMod_Base::eSubtype_forma &&
	(pNode->GetRank() != GetFormaRank() ||
	 s_NofTokens( pMod->GetSubname() ) != 1) ) {
	pMod->SetSubtype( COrgMod_Base::eSubtype_other );
    } 

    if( pMod->GetSubtype() == COrgMod_Base::eSubtype_other ) {
	int rank = pNode->GetRank();
	if( rank == GetSubspeciesRank() &&
	    s_NofTokens( pNode->GetName() ) == 3 ) {
	    pMod->SetSubtype( COrgMod_Base::eSubtype_sub_species );
	} else { // Do not insert invalid modifier
	    return false;
	}
    }
    // Store it into list
    on.SetMod().push_back( pMod );

    return true;
}

bool
COrgRefCache::SetBinomialName( CTaxon1Node& node, COrgName& on )
{
    CTaxon1Node* pSpec = ( NULL );
    CTaxon1Node* pSubspec = ( NULL );
    CTaxon1Node* pGenus = ( NULL );
    CTaxon1Node* pSubgen = ( NULL );
    CTaxon1Node* pNode = ( &node );
    string::size_type pos(0);
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
    CBinomialOrgName& bon = ( on.SetName().SetBinomial() );

    bon.SetGenus( pGenus->GetName() );

    if( pSpec ) { // we have a species in lineage
	pos = s_AfterPrefix( pSpec->GetName(), pGenus->GetName() );
	if( pos != string::npos ) {
	    bon.SetSpecies().assign( pSpec->GetName(),
				     pos, pSpec->GetName().size() - pos );
	} else {
	    bon.SetSpecies().assign( pSpec->GetName() );
	}
        if( pSubspec ) { // we also have a subspecies in lineage
	    pos = s_AfterPrefix( pSubspec->GetName(), pSpec->GetName() );
	    if( pos != string::npos ) {
		bon.SetSubspecies().assign( pSubspec->GetName(),
					    pos,
					    pSubspec->GetName().size() - pos );
	    } else {
		bon.SetSubspecies().assign( pSubspec->GetName() );
	    }
        }
        if( pNode != pSpec ) {
            BuildOrgModifier( pNode, on );
        }
        return true;
    }
    // no species in lineage
    if( pSubspec ) { // we have no species but we have subspecies
	pos = s_AfterPrefix( pSubspec->GetName(), pGenus->GetName() );
	if( pos != string::npos ) {
	    bon.SetSubspecies().assign( pSubspec->GetName(),
					pos,
					pSubspec->GetName().size() - pos );
	} else {
	    bon.SetSubspecies().assign( pSubspec->GetName() );
	}
        BuildOrgModifier( pNode, on,
                          pNode==pSubspec ? pGenus : pSubspec );
        return true;
    }
  
    // we have no species, no subspecies
    // but we are under species level (varietas or forma)
    BuildOrgModifier( pNode, on, pGenus );
    return true;
}

bool
COrgRefCache::SetPartialName( CTaxon1Node& node, COrgName& on )
{
    CTaxElement* pTaxElem = ( new CTaxElement );
    int rank_id= node.GetRank();
    
    CPartialOrgName& pon = ( on.SetName().SetPartial() );
    pon.Set().push_back(CRef<CTaxElement>(pTaxElem));

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
                lLin = ( resp.SetGetorgnames() );
            // Save taxname
            org.SetTaxname().swap( lLin.front()->SetOname() );
            lLin.pop_front();

            list< CRef< CTaxon1_name > >::iterator i;
            // Find preferred common name
            int pref_cls = GetPreferredCommonNameClass();
            for( i = lLin.begin(); i != lLin.end(); ++i ) {
                if( (*i)->CanGetCde() && (*i)->GetCde() == pref_cls ) {
                    org.SetCommon().swap( (*i)->SetOname() );
                    lLin.erase( i );
                    break;
                }
            }
            int syn_cls(GetSynonymNameClass());
            int comm_cls(GetCommonNameClass());
            for( i = lLin.begin(); i != lLin.end(); ++i ) {
                if( (*i)->CanGetCde() ) {
                    int cls = (*i)->GetCde();
                    if( cls == syn_cls || cls == comm_cls ) {
                        org.SetSyn().push_back( (*i)->GetOname() );
                    }
                }
            }
            // Set taxid as db tag
            org.SetTaxId( node.GetTaxId() );

            COrgName& on = ( org.SetOrgname() );

            // Create name
	    bool bOrgnameRetreived = false;
	    CRef<CTaxon1_info> pProp( new CTaxon1_info() );
	    pProp->SetIval1( node.GetTaxId() );
	    pProp->SetIval2( -1 ); // Get string property by name
	    pProp->SetSval( "orgname" );
	    
	    req.SetGetorgprop( *pProp );
	    try {
		if( m_host.SendRequest( req, resp ) ) {
		    if( resp.IsGetorgprop() ) { 
			if( resp.GetGetorgprop().size() > 0 ) {
			    CRef<CTaxon1_info> pInfo
				= resp.GetGetorgprop().front();
			    if( pInfo->IsSetSval() && !pInfo->GetSval().empty() ) {
				try {
 				    CObjectIStreamAsn is( pInfo->GetSval().c_str(),
 							  pInfo->GetSval().size(), eFNP_Allow );
 				    is >> on;
				    bOrgnameRetreived = true;
				} catch( exception& e ) {
				    if( e.what() ) {
					_TRACE( "Exception while parsing orgname property: " << e.what() );
				    }
				}
			    }
			}
		    }
		}
	    } catch( exception& e ) {
	    }
	    // Fill some other orgname fields
            short div_id( node.GetDivision() );
            if( GetDivisionCode( div_id ) ) {
                on.SetDiv( GetDivisionCode( div_id ) );
            }
            on.SetGcode( node.GetGC() );
            if( node.GetMGC() > 0 ) {
                on.SetMgcode( node.GetMGC() );
            }
            // Build lineage
            CTaxon1Node* pNode;
            if( !node.IsRoot() ) {
                pNode = node.GetParent();
                on.SetLineage(kEmptyStr);
                s_BuildLineage( on.SetLineage(), pNode, 0,
                                GetSpeciesRank() );
                if( on.GetLineage().empty() ) {
                    on.ResetLineage();
                }
            }
            // Set rank
            int rank_id( node.GetRank() );

            is_species = (rank_id >= GetSpeciesRank());
            // correct level by lineage if node has no rank
            if( rank_id < 0 && !node.IsRoot() ) {
                pNode = node.GetParent();
                while( !pNode->IsRoot() ) {
                    int rank( pNode->GetRank() );
                    if(rank >= 0) {
                        is_species= (rank >= GetSpeciesRank());
                        break;
                    }
                    pNode = pNode->GetParent();
                }
            }

	    if( !bOrgnameRetreived ) { // build orgname myself
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
			    pNode = 0;
			    CTaxon1Node* pTmp = ( node.GetParent() );
			    
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
			    BuildOrgModifier( &node, on );
			} // non species rank
		    } else if( !SetBinomialName( node, on ) ) {
			// name is not binomial: set partial
			SetPartialName( node, on );
		    }
		} else { // above species
		    SetPartialName( node, on );
		}
	    }
            // Add some genbank names as organism modifiers
            if( org.IsSetOrgname() ) { // OrgName is not empty
                for( i = lLin.begin(); i != lLin.end(); ++i ) {
                    if( (*i)->CanGetCde() ) {
                        int cde = (*i)->GetCde();
                        COrgMod::ESubtype stype = (COrgMod::ESubtype)0;
                        if( cde == GetGBAcronymNameClass() ) {
                            stype = COrgMod::eSubtype_gb_acronym;
                        } else if( cde == GetGBSynonymNameClass() ) {
                            stype = COrgMod::eSubtype_gb_synonym;
                        } else if( cde == GetGBAnamorphNameClass() ) {
                            stype = COrgMod::eSubtype_gb_anamorph;
                        }
                        if( stype ) {
                            CRef<COrgMod> pMod( new COrgMod );
                            pMod->SetSubname().swap( (*i)->SetOname() );
                            pMod->SetSubtype( stype );
                            on.SetMod().push_back( pMod );
                        }
                    }
                }
            }

        } else {
            m_host.SetLastError
                ("Unable to get orgref: Response is not Getorgnames");
            return false;
        }
    } else
        return false;

    CRef<CTaxon1_info> pProp( new CTaxon1_info() );
    pProp->SetIval1( node.GetTaxId() );
    pProp->SetIval2( -2 ); // Get int property by name
    pProp->SetSval( "pgcode" );

    req.SetGetorgprop( *pProp );
    try {
	if( m_host.SendRequest( req, resp ) ) {
	    if( resp.IsGetorgprop() ) { 
		if( resp.GetGetorgprop().size() > 0 ) {
		    CRef<CTaxon1_info> pInfo
			= resp.GetGetorgprop().front();
		    org.SetOrgname().SetPgcode( pInfo->GetIval2() );
		}
	    }
	}
    } catch( exception& e ) {
    }
    return true;
}

bool
COrgRefCache::Insert1( CTaxon1Node& node )
{
    bool is_species( false );
    struct SCacheEntry* pEntry = ( new SCacheEntry );
    pEntry->m_pTax1 = new CTaxon1_data;
    pEntry->m_pTax2 = NULL;
    pEntry->m_pTreeNode = &node;

    COrg_ref& org = ( pEntry->m_pTax1->SetOrg() );

    if( !BuildOrgRef( node, org, is_species ) ) {
        delete pEntry;
        return false;
    }
    // Set division code
    if( GetDivisionCode(node.GetDivision()) ) {
        pEntry->m_pTax1->SetDiv()
            .assign( GetDivisionCode(node.GetDivision()) );
    }
    // Set species level
    pEntry->m_pTax1->SetIs_species_level( is_species );
    // Remove last element from list
    if( m_lCache.size() >= m_nCacheCapacity ) {
        CTaxon1Node* pNode = ( m_lCache.back()->m_pTreeNode );
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
    struct SCacheEntry* pEntry = ( new SCacheEntry );
    pEntry->m_pTax1 = NULL;
    pEntry->m_pTax2 = new CTaxon2_data;
    pEntry->m_pTreeNode = &node;

    pEntry->m_pTax2->SetIs_uncultured( node.IsUncultured() );

    COrg_ref& org = pEntry->m_pTax2->SetOrg();

    if( !BuildOrgRef( node, org, is_species ) ) {
        delete pEntry;
        return false;
    }
    // Set blast names
    CTaxon1Node* pNode = ( &node );
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
        if( m_pTax2->IsSetOrg() ) {
            m_pTax1->SetOrg( m_pTax2->SetOrg() );
        }
        if( m_pTax2->GetOrg().GetOrgname().CanGetDiv() ) {
            m_pTax1->SetDiv( m_pTax2->GetOrg().GetOrgname().GetDiv() );
        } else {
            m_pTax1->SetDiv( kEmptyStr );
        }
        m_pTax1->SetIs_species_level(m_pTax2->GetIs_species_level());
    }
    return m_pTax1;
}

CTaxon2_data*
COrgRefCache::SCacheEntry::GetData2()
{
    if( ! m_pTax2 ) {
        m_pTax2 = new CTaxon2_data;
        if( m_pTax1->IsSetOrg() ) {
            m_pTax2->SetOrg( m_pTax1->SetOrg() );
        }
        CTaxon1Node* pNode = ( m_pTreeNode );
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
COrgRefCache::GetNameClassName( short nc )
{
    if( !InitNameClasses() ) return NULL;
    TNameClassMapCI ci( m_ncStorage.find( nc ) );
    if( ci != m_ncStorage.end() ) {
        return ci->second.c_str();
    }
    return NULL;
}

short
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
        m_ncSynonym = FindNameClassByName( "synonym" );
        if( m_ncSynonym < 0 ) {
            m_host.SetLastError( "Synonym name class was not found" );
            return false;
        }

        m_ncGBAcronym= FindNameClassByName("genbank acronym");
        if( m_ncGBAcronym < 0 ) {
            m_host.SetLastError( "Genbank acrony name class was not found" );
            return false;
        }
        m_ncGBSynonym= FindNameClassByName("genbank synonym");
        if( m_ncGBSynonym < 0 ) {
            m_host.SetLastError( "Genbank synonym name class was not found" );
            return false;
        }
        m_ncGBAnamorph= FindNameClassByName("genbank anamorph");
        if( m_ncGBAnamorph < 0 ) {
            m_host.SetLastError( "Genbank anamorph name class was not found" );
            return false;
        }
    }
    return true;
}

short
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
