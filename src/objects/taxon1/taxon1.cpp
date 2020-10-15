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

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/misc/error_codes.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/serial.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <algorithm>

#include "cache.hpp"


#define NCBI_USE_ERRCODE_X   Objects_Taxonomy


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


static const char s_achInvalTaxid[] = "Invalid tax id specified";

#define TAXON1_IS_INITED (m_pServer!=NULL)


CTaxon1::CTaxon1()
    : m_pServer(NULL),
      m_pOut(NULL),
      m_pIn(NULL),
      m_plCache(NULL),
      m_bWithSynonyms(false)
{
    return;
}


CTaxon1::~CTaxon1()
{
    Fini();
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

//---------------------------------------------
// Taxon1 server init
// Returns: TRUE - OK
//          FALSE - Can't open connection to taxonomy service
///
// default:  10 sec timeout, 5 reconnect attempts,
// cache for 1000 org-refs
static const STimeout def_timeout = { 10, 0 };

bool
CTaxon1::Init(void)
{
    return CTaxon1::Init(&def_timeout, def_reconnect_attempts, def_cache_capacity);
}

bool
CTaxon1::Init(unsigned cache_capacity)
{
    return CTaxon1::Init(&def_timeout, def_reconnect_attempts, cache_capacity);
}

bool
CTaxon1::Init(const STimeout* timeout, unsigned reconnect_attempts, unsigned cache_capacity)
{
    SetLastError(NULL);
    if( TAXON1_IS_INITED ) { // Already inited
        SetLastError( "ERROR: Init(): Already initialized" );
        return false;
    }
    SConnNetInfo* pNi = NULL;

    try {
        // Open connection to Taxonomy service
        CTaxon1_req req;
        CTaxon1_resp resp;

        if ( timeout ) {
            m_timeout_value = *timeout;
            m_timeout = &m_timeout_value;
        } else {
            m_timeout = 0;
        }

        m_nReconnectAttempts = reconnect_attempts;
        m_pchService = "TaxService4";
        const char* tmp;
        if( ( (tmp=getenv("NI_TAXONOMY_SERVICE_NAME")) != NULL ) ||
            ( (tmp=getenv("NI_SERVICE_NAME_TAXONOMY")) != NULL ) ) {
            m_pchService = tmp;
        }
        auto_ptr<CConn_ServiceStream> pServer;
        auto_ptr<CObjectOStream> pOut;
        auto_ptr<CObjectIStream> pIn;
	pNi = ConnNetInfo_Create( m_pchService );
	if( pNi == NULL ) {
	    SetLastError( "ERROR: Init(): Unable to create net info" );
	    return false;
	}
	pNi->max_try = reconnect_attempts + 1;
	ConnNetInfo_SetTimeout( pNi, timeout );

   pServer.reset( new CConn_ServiceStream(m_pchService, fSERV_Any,
                                             pNi, 0, m_timeout) );
	ConnNetInfo_Destroy( pNi );
	pNi = NULL;

#ifdef USE_TEXT_ASN
        m_eDataFormat = eSerial_AsnText;
#else
        m_eDataFormat = eSerial_AsnBinary;
#endif
        pOut.reset( CObjectOStream::Open(m_eDataFormat, *pServer) );
        pIn.reset( CObjectIStream::Open(m_eDataFormat, *pServer) );
        pOut->FixNonPrint(eFNP_Allow);
        pIn->FixNonPrint(eFNP_Allow);

        req.SetInit();

        m_pServer = pServer.release();
        m_pIn = pIn.release();
        m_pOut = pOut.release();

        if( SendRequest( req, resp ) ) {
            if( resp.IsInit() ) {
                // Init is done
                m_plCache = new COrgRefCache( *this );
                if( m_plCache->Init( cache_capacity ) ) {
                    return true;
                }
                delete m_plCache;
                m_plCache = NULL;
            } else { // Set error
                SetLastError( "INTERNAL: TaxService response type is not Init" );
            }
        }
    } catch( exception& e ) {
        SetLastError( e.what() );
    }
    // Clean up streams
    delete m_pIn;
    delete m_pOut;
    delete m_pServer;
    m_pIn = NULL;
    m_pOut = NULL;
    m_pServer = NULL;
    if( pNi ) {
	ConnNetInfo_Destroy( pNi );
    }
    return false;
}

void
CTaxon1::Fini(void)
{
    SetLastError(NULL);
    if( TAXON1_IS_INITED ) {
        CTaxon1_req req;
        CTaxon1_resp resp;

        req.SetFini();

        if( SendRequest( req, resp, false ) ) {
            if( !resp.IsFini() ) {
                SetLastError( "INTERNAL: TaxService response type is not Fini" );
            }
        }
    }
    Reset();
}

//---------------------------------------------
// Get organism data (including org-ref) by tax_id
// Returns: pointer to Taxon2Data if organism exists
//          NULL - if tax_id wrong
//
// NOTE:
// Caller gets own copy of Taxon2Data structure.
///
CRef< CTaxon2_data >
CTaxon1::GetById(TTaxId tax_id)
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return CRef<CTaxon2_data>(NULL);
	}
    }
    if( tax_id > ZERO_TAX_ID ) {
        // Check if this taxon is in cache
        CTaxon2_data* pData = 0;
        if( m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {
            CTaxon2_data* pNewData = new CTaxon2_data();

            SerialAssign<CTaxon2_data>( *pNewData, *pData );

            return CRef<CTaxon2_data>(pNewData);
        }
    } else {
        SetLastError( s_achInvalTaxid );
    }
    return CRef<CTaxon2_data>(NULL);
}

static bool
s_GetBoolValue( const CObject_id& val )
{
    switch( val.Which() ) {
    case CObject_id::e_Id:
	return val.GetId() != 0;
    case CObject_id::e_Str:
	try {
	    return NStr::StringToBool( val.GetStr().c_str() );
	} catch(...) { return false; }
    default:
	return false;
    }
}


CTaxon1::TOrgRefStatus
CTaxon1::x_ConvertOrgrefProps( CTaxon2_data& data )
{
    TOrgRefStatus result = 0;
    COrg_ref& org = data.SetOrg();
    // Copy dbtags from org to inp_orgRef
    for( COrg_ref::TDb::iterator i = org.SetDb().begin(); i != org.SetDb().end(); ) {
	if( (*i)->GetDb() != "taxon" ) {
	    if( NStr::StartsWith( (*i)->GetDb(), "taxlookup" ) ) {
		// convert taxlookup to status or refresh flags
		if( NStr::EndsWith( (*i)->GetDb(), "-changed" ) ) { // refresh flag
		    if( NStr::Equal( (*i)->GetDb(), "taxlookup%status-changed" ) &&
			(*i)->IsSetTag() && (*i)->GetTag().IsId() ) {
			result = (*i)->GetTag().GetId();
		    } else {
			if( NStr::Equal( (*i)->GetDb(), "taxlookup?taxid-changed" ) &&
			    (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongTaxId;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?taxname-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongTaxname;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?common-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongCommonName;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?orgname-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongOrgname;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?division-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongDivision;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?lineage-changed" ) &&
			       (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongLineage;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?gc-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongGC;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?mgc-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongMGC;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?orgmod-changed" ) &&
			       (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongOrgmod;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?pgc-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongPGC;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?orgrefmod-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongOrgrefMod;
			} else if( NStr::Equal( (*i)->GetDb(), "taxlookup?orgnameattr-changed" ) &&
				   (*i)->IsSetTag() && s_GetBoolValue((*i)->GetTag()) ) {
			    result |= eStatus_WrongOrgnameAttr;
			}
		    }
		} else { // status flag
		    if( NStr::Equal( (*i)->GetDb(), "taxlookup?is_species_level" ) &&
			(*i)->IsSetTag() ) {
			data.SetIs_species_level( s_GetBoolValue((*i)->GetTag()) );
		    } else if( NStr::Equal( (*i)->GetDb(), "taxlookup?is_uncultured" ) &&
			(*i)->IsSetTag() ) {
			data.SetIs_uncultured( s_GetBoolValue((*i)->GetTag()) );
		    } else if( NStr::EqualNocase( (*i)->GetDb(), "taxlookup$blast_lineage" ) &&
			       (*i)->IsSetTag() && (*i)->GetTag().IsStr() ) {
			list< string > lBlastNames;
			NStr::Split( (*i)->GetTag().GetStr(), ";", lBlastNames, NStr::fSplit_Tokenize );
                        NON_CONST_ITERATE( list< string >, i, lBlastNames ) {
                            NStr::TruncateSpacesInPlace( *i );
                        }
			lBlastNames.reverse();
			data.SetBlast_name().insert( data.SetBlast_name().end(),
						     lBlastNames.begin(), lBlastNames.end() );
		    } else {
			string sPropName = (*i)->GetDb().substr( strlen( "taxlookup?" ) );
			switch( (*i)->GetTag().Which() ) {
			case CObject_id::e_Str: data.SetProperty( sPropName, (*i)->GetTag().GetStr() ); break;
			case CObject_id::e_Id: data.SetProperty( sPropName, (*i)->GetTag().GetId() ); break;
			default: break; // unknown type
			}
		    }
		}
		i = org.SetDb().erase(i);
	    } else {
		++i;
	    }
	} else {
	    ++i;
	}
    }
		    
    // Set flags to default values if corresponding property is not found
    if( !data.IsSetIs_uncultured() ) {
	data.SetIs_uncultured( false );
    }
    if( !data.IsSetIs_species_level() ) {
	data.SetIs_species_level( false );
    }
    return result;
}

//----------------------------------------------
// Get organism data by OrgRef
// Returns: pointer to Taxon2Data if organism exists
//          NULL - if no such organism in taxonomy database
//
// NOTE:
// 1. These functions uses the following data from inp_orgRef to find
//    organism in taxonomy database. It uses taxname first. If no organism
//    was found (or multiple nodes found) then it tries to find organism
//    using common name. If nothing found, then it tries to find organism
//    using synonyms. Lookup never uses tax_id to find organism.
// 2. LookupMerge function modifies given OrgRef to correspond to the
//    found one and returns constant pointer to the Taxon2Data structure
//    stored internally.
///
CRef< CTaxon2_data >
CTaxon1::Lookup(const COrg_ref& inp_orgRef, string* psLog)
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return CRef<CTaxon2_data>(NULL);
	}
    }
    // Check if this taxon is in cache
    CRef<CTaxon2_data> pData( 0 );

    SetLastError(NULL);

    CTaxon1_req  req;
    CTaxon1_resp resp;

    SerialAssign< COrg_ref >( req.SetLookup(), inp_orgRef );
    // Set version db tag
    COrgrefProp::SetOrgrefProp( req.SetLookup(), "version", 2 );
    if( m_bWithSynonyms ) {
	COrgrefProp::SetOrgrefProp( req.SetLookup(), "syn", m_bWithSynonyms );
    }
    if( psLog ) {
        COrgrefProp::SetOrgrefProp( req.SetLookup(), "log", true );
    }

    if( SendRequest( req, resp ) ) {
        if( resp.IsLookup() ) {
            // Correct response, return object
	    pData.Reset( new CTaxon2_data() );
	    
            SerialAssign< COrg_ref >( pData->SetOrg(), resp.GetLookup().GetOrg() );
	    x_ConvertOrgrefProps( *pData );
            if( psLog ) {
                pData->GetProperty( "log", *psLog );
            }
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Lookup" );
        }
    }

    return pData;
}

CConstRef< CTaxon2_data >
CTaxon1::LookupMerge(COrg_ref& inp_orgRef, string* psLog, TOrgRefStatus* pStatusOut)
{
    //CTaxon2_data* pData = 0;

    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return CConstRef<CTaxon2_data>(NULL);
	}
    }
    // Check if this taxon is in cache
    CRef<CTaxon2_data> pData( 0 );

    SetLastError(NULL);

    CTaxon1_req  req;
    CTaxon1_resp resp;

    SerialAssign< COrg_ref >( req.SetLookup(), inp_orgRef );
    // Set version db tag
    COrgrefProp::SetOrgrefProp( req.SetLookup(), "version", 2 );
    COrgrefProp::SetOrgrefProp( req.SetLookup(), "merge", true );
    COrgrefProp::SetOrgrefProp( req.SetLookup(), "syn", m_bWithSynonyms );
    if( psLog ) {
        COrgrefProp::SetOrgrefProp( req.SetLookup(), "log", true );
    }
    if( SendRequest( req, resp ) ) {
        if( resp.IsLookup() ) {
            // Correct response, return object
	    pData.Reset( new CTaxon2_data() );
	    
            SerialAssign< COrg_ref >( pData->SetOrg(), resp.GetLookup().GetOrg() );
	    TOrgRefStatus stat_out = x_ConvertOrgrefProps( *pData );
	    if( pStatusOut ) {
		*pStatusOut = stat_out;
	    }
            if( psLog ) {
                pData->GetProperty( "log", *psLog );
            }
	    SerialAssign< COrg_ref >( inp_orgRef, pData->GetOrg() );
	    
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Lookup" );
        }
    }

    return CConstRef<CTaxon2_data>(pData);
}

//-----------------------------------------------
// Get tax_id by OrgRef
// Returns: tax_id - if organism found
//               0 - no organism found
//              -1 - error during processing occured
//         -tax_id - if multiple nodes found
//                   (where tax_id > 1 is id of one of the nodes)
// NOTE:
// This function uses the same information from inp_orgRef as Lookup
///
TTaxId
CTaxon1::GetTaxIdByOrgRef(const COrg_ref& inp_orgRef)
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return INVALID_TAX_ID;
	}
    }

    CTaxon1_req  req;
    CTaxon1_resp resp;

    SerialAssign< COrg_ref >( req.SetGetidbyorg(), inp_orgRef );

    if( SendRequest( req, resp ) ) {
        if( resp.IsGetidbyorg() ) {
            // Correct response, return object
            return TAX_ID_FROM(int, resp.GetGetidbyorg());
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Getidbyorg" );
        }
    }
    return ZERO_TAX_ID;
}

//----------------------------------------------
// Get tax_id by organism name
// Returns: tax_id - if organism found
//               0 - no organism found
//              -1 - error during processing occured
//         -tax_id - if multiple nodes found
//                   (where tax_id > 1 is id of one of the nodes)
///
TTaxId
CTaxon1::GetTaxIdByName(const string& orgname)
{
    SetLastError(NULL);
    if( orgname.empty() )
        return ZERO_TAX_ID;
    list< CRef< CTaxon1_name > > lNames;
    TTaxId retc = SearchTaxIdByName(orgname, eSearch_Exact, &lNames);
    switch(TAX_ID_TO(TIntId, retc) ) {
    case -2: retc = INVALID_TAX_ID; break;
    case -1: retc = -lNames.front()->GetTaxid(); break; // Multiple nodes found, get first taxid
    default: break;
    }

    return retc;
}

//----------------------------------------------
// Get tax_id by organism "unique" name
// Returns: tax_id - if organism found
//               0 - no organism found
//              -1 - error during processing occured
//         -tax_id - if multiple nodes found
//                   (where tax_id > 1 is id of one of the nodes)
///
TTaxId
CTaxon1::FindTaxIdByName(const string& orgname)
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return INVALID_TAX_ID;
	}
    }
    if( orgname.empty() )
        return ZERO_TAX_ID;

    TTaxId id( GetTaxIdByName(orgname) );

    if(id < TAX_ID_CONST(1)) {

        TTaxId idu = ZERO_TAX_ID;

        CTaxon1_req  req;
        CTaxon1_resp resp;

        req.SetGetunique().assign( orgname );

        if( SendRequest( req, resp ) ) {
            if( resp.IsGetunique() ) {
                // Correct response, return object
                idu = resp.GetGetunique();
            } else { // Internal: wrong respond type
                SetLastError( "INTERNAL: TaxService response type is not Getunique" );
            }
        }

        if( idu > ZERO_TAX_ID )
            id= idu;
    }
    return id;
}

//----------------------------------------------
// Get tax_id by organism name using fancy search modes. If given a pointer
// to the list of names then it'll return all found names (one name per 
// tax id). Previous content of name_list_out will be destroyed.
// Returns: tax_id - if organism found
//               0 - no organism found
//              -1 - if multiple nodes found
//              -2 - error during processing occured
///
TTaxId
CTaxon1::SearchTaxIdByName(const string& orgname, ESearch mode,
                           list< CRef< CTaxon1_name > >* pNameList)
{
    // Use fancy searches
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return TAX_ID_CONST(-2);
	}
    }
    if( orgname.empty() ) {
        return ZERO_TAX_ID;
    }
    CRef< CTaxon1_info > pQuery( new CTaxon1_info() );
    int nMode = 0;
    switch( mode ) {
    default:
    case eSearch_Exact:    nMode = 0; break;
    case eSearch_TokenSet: nMode = 1; break;
    case eSearch_WildCard: nMode = 2; break; // shell-style wildcards, i.e. *,?,[]
    case eSearch_Phonetic: nMode = 3; break;
    }
    pQuery->SetIval1( nMode );
    pQuery->SetIval2( 0 );
    pQuery->SetSval( orgname );

    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetSearchname( *pQuery );

    if( SendRequest( req, resp ) ) {
        if( resp.IsSearchname() ) {
            // Correct response, return object
            TTaxId retc = ZERO_TAX_ID;
            const CTaxon1_resp::TSearchname& lNm = resp.GetSearchname();
            if( lNm.size() == 0 ) {
                retc = ZERO_TAX_ID;
            } else if( lNm.size() == 1 ) {
                retc = lNm.front()->GetTaxid();
            } else {
                retc = INVALID_TAX_ID;
            }
            // Fill the names list
            if( pNameList ) {
                pNameList->swap( resp.SetSearchname() );
            }
            return retc;
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Searchname" );
            return TAX_ID_CONST(-2);
        }
    } else if( GetLastError().find("Nothing found") != string::npos ) {
        return ZERO_TAX_ID;
    }
    return TAX_ID_CONST(-2);
}

//----------------------------------------------
// Get ALL tax_id by organism name
// Returns: number of organisms found (negative value on error), 
// id list appended with found tax ids
///
int
CTaxon1::GetAllTaxIdByName(const string& orgname, TTaxIdList& lIds)
{
    int count = 0;

    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -2;
	}
    }
    if( orgname.empty() )
        return 0;

    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetFindname().assign(orgname);

    if( SendRequest( req, resp ) ) {
        if( resp.IsFindname() ) {
            // Correct response, return object
            const list< CRef< CTaxon1_name > >& lNm = resp.GetFindname();
            // Fill in the list
            for( list< CRef< CTaxon1_name > >::const_iterator
                     i = lNm.begin();
                 i != lNm.end(); ++i, ++count )
                lIds.push_back( (*i)->GetTaxid() );
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Findname" );
            return 0;
        }
    }
    return count;
}

//----------------------------------------------
// Get organism by tax_id
// Returns: pointer to OrgRef structure if organism found
//          NULL - if no such organism in taxonomy database or error occured 
//                 (check GetLastError() for the latter)
// NOTE:
// This function does not make a copy of OrgRef structure but returns
// pointer to internally stored OrgRef.
///
CConstRef< COrg_ref >
CTaxon1::GetOrgRef(TTaxId tax_id,
                   bool& is_species,
                   bool& is_uncultured,
                   string& blast_name,
		   bool* is_specified)
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return null;
	}
    }
    if( tax_id > ZERO_TAX_ID ) {
        CTaxon2_data* pData = 0;
        if( m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {
            is_species = pData->GetIs_species_level();
            is_uncultured = pData->GetIs_uncultured();
            if( pData->IsSetBlast_name() && pData->GetBlast_name().size() > 0 ) {
                blast_name.assign( pData->GetBlast_name().front() );
            }
	    if( is_specified ) {
		//*is_specified = pData->GetOrg().GetOrgname().IsFormalName();
 		bool specified = false;
 		if( GetNodeProperty( tax_id, "specified_inh", specified ) ) {
 		    *is_specified = specified;
 		} else {
 		    return null;
 		}		
	    }
            return CConstRef<COrg_ref>(&pData->GetOrg());
        }
    }
    return null;
}

//---------------------------------------------
// Set mode for synonyms in OrgRef
// Returns: previous mode
// NOTE:
// Default value: false (do not copy synonyms to the new OrgRef)
///
bool
CTaxon1::SetSynonyms(bool on_off)
{
    SetLastError(NULL);
    bool old_val( m_bWithSynonyms );
    m_bWithSynonyms = on_off;
    return old_val;
}

//---------------------------------------------
// Get parent tax_id
// Returns: tax_id of parent node or 0 if error
// NOTE:
//   Root of the tree has tax_id of 1
///
TTaxId
CTaxon1::GetParent(TTaxId id_tax)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return ZERO_TAX_ID;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode && pNode->GetParent() ) {
        return pNode->GetParent()->GetTaxId();
    }
    return ZERO_TAX_ID;
}

//---------------------------------------------
// Get species tax_id (id_tax should be below species).
// There are 2 species search modes: one based solely on node rank and
// another based on the flag is_species returned in the Taxon2_data
// structure.
// Returns: tax_id of species node (> 1)
//       or 0 if no species above (maybe id_tax above species)
//       or -1 if error
// NOTE:
//   Root of the tree has tax_id of 1
///
TTaxId
CTaxon1::GetSpecies(TTaxId id_tax, ESpeciesMode mode)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return INVALID_TAX_ID;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode && m_plCache->InitRanks() ) {
	if( mode == eSpeciesMode_RankOnly ) {
	    TTaxRank species_rank(m_plCache->GetSpeciesRank());
	    while( !pNode->IsRoot() ) {
		TTaxRank rank( pNode->GetRank() );
		if( rank == species_rank )
		    return pNode->GetTaxId();
		if( (rank > 0) && (rank < species_rank))
		    return ZERO_TAX_ID;
		pNode = pNode->GetParent();
	    }
	    return ZERO_TAX_ID;
	} else { // Based on flag
	    CTaxon1Node* pResult = NULL;
	    CTaxon2_data* pData = NULL;
	    while( !pNode->IsRoot() ) {
		if( m_plCache->LookupAndInsert( pNode->GetTaxId(), &pData ) ) {
		    if( !pData )
			return INVALID_TAX_ID;
		    if( !(pData->IsSetIs_species_level() && pData->GetIs_species_level()) ) {
			if( pResult ) {
			    return pResult->GetTaxId();
			} else {
			    return ZERO_TAX_ID;
			}
		    }
		    pResult = pNode;
		    pNode = pNode->GetParent();
		} else { // Node in the lineage not found
		    return INVALID_TAX_ID;
		}
	    }
	}
    }
    return INVALID_TAX_ID;
}

//---------------------------------------------
// Get genus tax_id (id_tax should be below genus)
// Returns: tax_id of genus or
//               0 - no genus in the lineage
//              -1 - if error
///
TTaxId
CTaxon1::GetGenus(TTaxId id_tax)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return INVALID_TAX_ID;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode && m_plCache->InitRanks() ) {
        TTaxRank genus_rank(m_plCache->GetGenusRank());
        while( !pNode->IsRoot() ) {
            TTaxRank rank( pNode->GetRank() );
            if( rank == genus_rank )
                return pNode->GetTaxId();
            if( (rank > 0) && (rank < genus_rank))
                return ZERO_TAX_ID;
            pNode = pNode->GetParent();
        }
        return ZERO_TAX_ID;
    }
    return INVALID_TAX_ID;
}

//---------------------------------------------
// Get superkingdom tax_id (id_tax should be below superkingdom)
// Returns: tax_id of superkingdom or
//               0 - no superkingdom in the lineage
//              -1 - if error
///
TTaxId
CTaxon1::GetSuperkingdom(TTaxId id_tax)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return INVALID_TAX_ID;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode && m_plCache->InitRanks() ) {
        TTaxRank sk_rank(m_plCache->GetSuperkingdomRank());
        while( !pNode->IsRoot() ) {
            TTaxRank rank( pNode->GetRank() );
            if( rank == sk_rank )
                return pNode->GetTaxId();
            if( (rank > 0) && (rank < sk_rank))
                return ZERO_TAX_ID;
            pNode = pNode->GetParent();
        }
        return ZERO_TAX_ID;
   }
    return INVALID_TAX_ID;
}

//---------------------------------------------
// Get ancestor tax_id by rank name
// rank name might be one of:
// no rank
// superkingdom
// kingdom
// subkingdom
// superphylum
// phylum
// subphylum
// superclass
// class
// subclass
// infraclass
// cohort
// subcohort
// superorder
// order
// suborder
// infraorder
// parvorder
// superfamily
// family
// subfamily
// tribe
// subtribe
// genus
// subgenus
// species group
// species subgroup
// species
// subspecies
// varietas
// forma
//
// Returns: tax_id of properly ranked accessor or
//               0 - no such rank in the lineage
//              -1 - tax id is not found
//              -2 - invalid rank name
//              -3 - any other error (use GetLastError for details)
///
TTaxId
CTaxon1::GetAncestorByRank(TTaxId id_tax, const char* rank_name)
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
        if( !Init() ) { 
            return TAX_ID_CONST(-3);
        }
    }
    if( rank_name ) {
        TTaxRank rank( m_plCache->FindRankByName( rank_name ) );
        if( rank != -1000 ) {
            return GetAncestorByRank(id_tax, rank);
        }
    }
    SetLastError( "rank not found" );
    ERR_POST_X( 2, GetLastError() );
    return TAX_ID_CONST(-2);
}

TTaxId
CTaxon1::GetAncestorByRank(TTaxId id_tax, TTaxRank rank_id)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
        if( !Init() ) { 
            return TAX_ID_CONST(-3);
        }
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode ) {
        while( !pNode->IsRoot() ) {
            TTaxRank rank( pNode->GetRank() );
            if( rank == rank_id )
                return pNode->GetTaxId();
            if( (rank >= 0) && (rank < rank_id))
                return ZERO_TAX_ID;
            pNode = pNode->GetParent();
        }
        return ZERO_TAX_ID;
    }
    return INVALID_TAX_ID;
}

//---------------------------------------------
// Get taxids for all children of specified node.
// Returns: number of children, id list appended with found tax ids
//          -1 - in case of error
///
int
CTaxon1::GetChildren(TTaxId id_tax, TTaxIdList& children_ids)
{
    int count(0);
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode ) {

        CTaxon1_req  req;
        CTaxon1_resp resp;

        req.SetTaxachildren(TAX_ID_TO(int, id_tax) );

        if( SendRequest( req, resp ) ) {
            if( resp.IsTaxachildren() ) {
                // Correct response, return object
                list< CRef< CTaxon1_name > >& lNm = resp.SetTaxachildren();
                // Fill in the list
                CTreeIterator* pIt = m_plCache->GetTree().GetIterator();
                pIt->GoNode( pNode );
                for( list< CRef< CTaxon1_name > >::const_iterator
                         i = lNm.begin();
                     i != lNm.end(); ++i, ++count ) {
                    children_ids.push_back( (*i)->GetTaxid() );
                    // Add node to the partial tree
                    CTaxon1Node* pNewNode = new CTaxon1Node(*i);
                    m_plCache->SetIndexEntry(TAX_ID_TO(int, pNewNode->GetTaxId()), pNewNode);
                    pIt->AddChild( pNewNode );
                }
            } else { // Internal: wrong respond type
                SetLastError( "INTERNAL: TaxService response type is not Taxachildren" );
                return 0;
            }
        }
    }
    return count;
}

bool
CTaxon1::GetGCName(TTaxGeneticCode gc_id, string& gc_name_out )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    if( m_gcStorage.empty() ) {
        CTaxon1_req  req;
        CTaxon1_resp resp;

        req.SetGetgcs();

        if( SendRequest( req, resp ) ) {
            if( resp.IsGetgcs() ) {
                // Correct response, return object
                const list< CRef< CTaxon1_info > >& lGc = resp.GetGetgcs();
                // Fill in storage
                for( list< CRef< CTaxon1_info > >::const_iterator
                         i = lGc.begin();
                     i != lGc.end(); ++i ) {
                    m_gcStorage.insert( TGCMap::value_type((*i)->GetIval1(),
                                                           (*i)->GetSval()) );
                }
            } else { // Internal: wrong respond type
                SetLastError( "INTERNAL: TaxService response type is not Getgcs" );
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

//---------------------------------------------
// Get taxonomic rank name by rank id
///
bool
CTaxon1::GetRankName(TTaxRank rank_id, string& rank_name_out )
{
    SetLastError( NULL );
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    const char* pchName = m_plCache->GetRankName( rank_id );
    if( pchName ) {
        rank_name_out.assign( pchName );
        return true;
    } else {
        SetLastError( "ERROR: GetRankName(): Rank not found" );
        return false;
    }
}

//---------------------------------------------
// Get taxonomic rank id by rank name
// Returns: rank id
//          -2 - in case of error
///
TTaxRank
CTaxon1::GetRankIdByName(const string& rank_name)
{
    SetLastError( NULL );
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    TTaxRank id = m_plCache->FindRankByName( rank_name.c_str() );
    if( id != -1000 ) {
	return id;
    } else {
	// Error already set
        return -2;
    }
}

//---------------------------------------------
// Get taxonomic division name by division id
///
bool
CTaxon1::GetDivisionName(TTaxDivision div_id, string& div_name_out, string* div_code_out )
{
    SetLastError( NULL );
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    const char* pchName = m_plCache->GetDivisionName( div_id );
    const char* pchCode = m_plCache->GetDivisionCode( div_id );
    if( pchName ) {
        div_name_out.assign( pchName );
        if( pchCode && div_code_out != NULL ) {
            div_code_out->assign( pchCode );
        }
        return true;
    } else {
        SetLastError( "ERROR: GetDivisionName(): Division not found" );
        return false;
    }
}

//---------------------------------------------
// Get taxonomic division id by division name (or code)
// Returns: rank id
//          -1 - in case of error
///
TTaxDivision
CTaxon1::GetDivisionIdByName(const string& div_name)
{
    SetLastError( NULL );
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    TTaxDivision id = m_plCache->FindDivisionByName( div_name.c_str() );
    if( id <= -1 ) {
	id = m_plCache->FindDivisionByCode( div_name.c_str() );
	if( id <= -1 ) {
	    return -1;
	}
    }
    return id;
}

//---------------------------------------------
// Get taxonomic name class (scientific name, common name, etc.) by id
///
bool
CTaxon1::GetNameClass(TTaxNameClass nameclass_id, string& name_class_out )
{
    SetLastError( NULL );
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    const char* pchName = m_plCache->GetNameClassName( nameclass_id );
    if( pchName ) {
        name_class_out.assign( pchName );
        return true;
    } else {
        SetLastError( "ERROR: GetNameClass(): Name class not found" );
        return false;
    }
}

//---------------------------------------------
// Get name class id by name class name
// Returns: value < 0 - Incorrect class name
///
TTaxNameClass
CTaxon1::GetNameClassId( const string& class_name )
{
    SetLastError( NULL );
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }
    if( m_plCache->InitNameClasses() ) {
        return m_plCache->FindNameClassByName( class_name.c_str() );
    }
    return -1;
}

//---------------------------------------------
// Get the nearest common ancestor for two nodes
// Returns: id of this ancestor (id == 1 means that root node only is
// ancestor)
//          -1 - in case of an error
///
TTaxId
CTaxon1::Join(TTaxId taxid1, TTaxId taxid2)
{
    TTaxId tax_id = ZERO_TAX_ID;
    CTaxon1Node *pNode1, *pNode2;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return INVALID_TAX_ID;
	}
    }
    if( m_plCache->LookupAndAdd( taxid1, &pNode1 ) && pNode1
        && m_plCache->LookupAndAdd( taxid2, &pNode2 ) && pNode2 ) {
        CRef< ITreeIterator > pIt( GetTreeIterator() );
        pIt->GoNode( pNode1 );
        pIt->GoAncestor( pNode2 );
        tax_id = pIt->GetNode()->GetTaxId();
    }
    return tax_id;
}

//---------------------------------------------
// Get all names for tax_id
// Returns: number of names, name list appended with ogranism's names
//          -1 - in case of an error
// NOTE:
// If unique is true then only unique names will be stored
///
int
CTaxon1::GetAllNames(TTaxId tax_id, TNameList& lNames, bool unique)
{
    int count(0);
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }
    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetGetorgnames(TAX_ID_TO(int, tax_id) );

    if( SendRequest( req, resp ) ) {
        if( resp.IsGetorgnames() ) {
            // Correct response, return object
            const list< CRef< CTaxon1_name > >& lNm = resp.GetGetorgnames();
            // Fill in the list
            for( list< CRef< CTaxon1_name > >::const_iterator
                     i = lNm.begin();
                 i != lNm.end(); ++i, ++count )
                if( !unique ) {
                    lNames.push_back( (*i)->GetOname() );
                } else {
                    lNames.push_back( ((*i)->IsSetUname() && !(*i)->GetUname().empty()) ?
                                      (*i)->GetUname() :
                                      (*i)->GetOname() );
                }
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Getorgnames" );
            return 0;
        }
    }

    return count;
}

//---------------------------------------------
// Get list of all names for tax_id.
// Clears the previous content of the list.
// Returns: TRUE - success
//          FALSE - failure
///
bool
CTaxon1::GetAllNamesEx(TTaxId tax_id, list< CRef< CTaxon1_name > >& lNames)
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    CTaxon1_req  req;
    CTaxon1_resp resp;

    lNames.clear();

    req.SetGetorgnames(TAX_ID_TO(int, tax_id) );

    if( SendRequest( req, resp ) ) {
        if( resp.IsGetorgnames() ) {
            // Correct response, return object
            const list< CRef< CTaxon1_name > >& lNm = resp.GetGetorgnames();
            // Fill in the list
            for( list< CRef< CTaxon1_name > >::const_iterator
                     i = lNm.begin(), li = lNm.end(); i != li; ++i ) {
		lNames.push_back( *i );
	    }
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Getorgnames" );
            return false;
        }
    }

    return true;
}

bool
CTaxon1::DumpNames( TTaxNameClass name_class, list< CRef< CTaxon1_name > >& lOut )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetDumpnames4class( name_class );

    if( SendRequest( req, resp ) ) {
        if( resp.IsDumpnames4class() ) {
            // Correct response, return object
            lOut.swap( resp.SetDumpnames4class() );
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Dumpnames4class" );
            return false;
        }
    }

    return true;
}

bool
CTaxon1::IsAlive(void)
{
    SetLastError(NULL);
    if( TAXON1_IS_INITED ) {
        if( !m_pOut || !m_pOut->InGoodState() )
            SetLastError( "Output stream is not in good state" );
        else if( !m_pIn || !m_pIn->InGoodState() )
            SetLastError( "Input stream is not in good state" );
        else
            return true;
    } else {
        SetLastError( "Not connected to Taxonomy service" );
    }
    return false;
}

bool
CTaxon1::GetTaxId4GI(TGi gi, TTaxId& tax_id_out )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }

    CTaxon1_req  req;
    CTaxon1_resp resp;

    req.SetId4gi( gi );

    if( SendRequest( req, resp ) ) {
        if( resp.IsId4gi() ) {
            // Correct response, return object
            tax_id_out = resp.GetId4gi();
            return true;
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Id4gi" );
        }
    }
    return false;
}

bool
CTaxon1::GetBlastName(TTaxId tax_id, string& blast_name_out )
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
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

bool
CTaxon1::SendRequest( CTaxon1_req& req, CTaxon1_resp& resp, bool bShouldReconnect )
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

        try {
        *m_pIn >> resp;

        if( m_pIn->InGoodState() ) {
            if( resp.IsError() ) { // Process error here
            string err;
            resp.GetError().GetErrorText( err );
            SetLastError( err.c_str() );
            return false;
            } else
            return true;
        }
        } catch( CEofException& /*eoe*/ ) {
	    bNeedReconnect = bShouldReconnect;
        } catch( exception& e ) {
	    SetLastError( e.what() );
	    bNeedReconnect = bShouldReconnect;
        }
        fail_flags = m_pIn->GetFailFlags();
        bNeedReconnect |= bShouldReconnect &&
	    (fail_flags & ( CObjectIStream::eReadError
			    |CObjectIStream::eFail
			    |CObjectIStream::eNotOpen )
	     ? true : false);
        } catch( exception& e ) {
            SetLastError( e.what() );
            fail_flags = m_pOut->GetFailFlags();
            bNeedReconnect = bShouldReconnect &&
		(fail_flags & ( CObjectOStream::eWriteError
				|CObjectOStream::eFail
				|CObjectOStream::eNotOpen )
		 ? true : false);
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
                                                     0, 0, m_timeout) );

                pOut.reset( CObjectOStream::Open(m_eDataFormat, *pServer) );
                pIn.reset( CObjectIStream::Open(m_eDataFormat, *pServer) );
                pOut->FixNonPrint(eFNP_Allow);
                pIn->FixNonPrint(eFNP_Allow);

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

static void s_StoreResidueTaxid( CTreeIterator* pIt, CTaxon1::TTaxIdList& lTo )
{
    CTaxon1Node* pNode =  static_cast<CTaxon1Node*>( pIt->GetNode() );
    if( !pNode->IsJoinTerminal() ) {
        lTo.push_back( pNode->GetTaxId() );
    }
    if( pIt->GoChild() ) {
        do {
            s_StoreResidueTaxid( pIt, lTo );
        } while( pIt->GoSibling() );
        pIt->GoParent();
    }
}
//--------------------------------------------------
// This function constructs minimal common tree from the gived tax id
// set (ids_in) treated as tree's leaves. It then returns a residue of
// this tree node set and the given tax id set in ids_out.
// Returns: false if some error
//          true  if Ok
///
typedef vector<CTaxon1Node*> TTaxNodeLineage;
bool
CTaxon1::GetPopsetJoin( const TTaxIdList& ids_in, TTaxIdList& ids_out )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    if( ids_in.size() > 0 ) {
        map< TTaxId, CTaxon1Node* > nodeMap;
        CTaxon1Node *pParent = 0, *pNode = 0, *pNewParent = 0;
        CTreeCont tPartTree; // Partial tree
        CTreeIterator* pIt = tPartTree.GetIterator();
        TTaxNodeLineage vLin;
        // Build the partial tree
        bool bHasSiblings;
        vLin.reserve( 256 );
        for( TTaxIdList::const_iterator ci = ids_in.begin();
             ci != ids_in.end();
             ++ci ) {
            map< TTaxId, CTaxon1Node* >::iterator nmi = nodeMap.find( *ci );
            if( nmi == nodeMap.end() ) {
                if( m_plCache->LookupAndAdd( *ci, &pNode ) ) {
                    if( !tPartTree.GetRoot() ) {
                        pNewParent = new CTaxon1Node
                            ( *static_cast<const CTaxon1Node*>
                              (m_plCache->GetTree().GetRoot()) );
                        tPartTree.SetRoot( pNewParent );
                        nodeMap.insert( map< TTaxId, CTaxon1Node* >::value_type
                                        (pNewParent->GetTaxId(), pNewParent) );
                    }
                    if( pNode ) {
                        vLin.clear();
                        pParent = pNode->GetParent();
                        pNode = new CTaxon1Node( *pNode );
                        pNode->SetJoinTerminal();
                        vLin.push_back( pNode );
                        while( pParent &&
                               ((nmi=nodeMap.find(pParent->GetTaxId()))
                                == nodeMap.end()) ) {
                            pNode = new CTaxon1Node( *pParent );
                            vLin.push_back( pNode );
                            pParent = pParent->GetParent();
                        }
                        if( !pParent ) {
                            pIt->GoRoot();
                        } else {
                            pIt->GoNode( nmi->second );
                        }
                        for( TTaxNodeLineage::reverse_iterator i =
                                 vLin.rbegin();
                             i != vLin.rend();
                             ++i ) {
                            pNode = *i;
                            nodeMap.insert( map< TTaxId, CTaxon1Node* >::value_type
                                            ( pNode->GetTaxId(), pNode ) );
                            pIt->AddChild( pNode );
                            pIt->GoNode( pNode );
                        }
                    }
                } else { // Error while adding - ignore invalid tax_ids
                    continue;
                    //return false;
                }
            } else { // Node is already here
                nmi->second->SetJoinTerminal();
            }
        }
        // Partial tree is build, make a residue
        if( tPartTree.GetRoot() ) {
            pIt->GoRoot();
            bHasSiblings = true;
            if( pIt->GoChild() ) {
                while( !pIt->GoSibling() ) {
                    pNode = static_cast<CTaxon1Node*>( pIt->GetNode() );
                    if( pNode->IsJoinTerminal() || !pIt->GoChild() ) {
                        bHasSiblings = false;
                        break;
                    }
                }
                if( bHasSiblings ) {
                    pIt->GoParent();
                }
                s_StoreResidueTaxid( pIt, ids_out );
            }
        }
    }
    return true;
}

//-----------------------------------
//  Tree-related functions
bool
CTaxon1::LoadSubtreeEx( TTaxId tax_id, int levels, const ITaxon1Node** ppNode )
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    if( ppNode ) {
        *ppNode = pNode;
    }
    if( m_plCache->LookupAndAdd( tax_id, &pNode )
        && pNode ) {

        if( ppNode ) {
            *ppNode = pNode;
        }

        if( pNode->IsSubtreeLoaded() ) {
            return true;
        }

        if( levels == 0 ) {
            return true;
        }

        CTaxon1_req  req;
        CTaxon1_resp resp;

        if( levels < 0 ) {
            tax_id = -tax_id;
        }
        req.SetTaxachildren(TAX_ID_TO(int, tax_id) );

        if( SendRequest( req, resp ) ) {
            if( resp.IsTaxachildren() ) {
                // Correct response, return object
                list< CRef< CTaxon1_name > >& lNm = resp.SetTaxachildren();
                // Fill in the list
                CTreeIterator* pIt = m_plCache->GetTree().GetIterator();
                pIt->GoNode( pNode );
                for( list< CRef< CTaxon1_name > >::const_iterator
                         i = lNm.begin();
                     i != lNm.end(); ++i ) {
                    if( (*i)->GetCde() == 0 ) { // Change parent node
                        if( m_plCache->LookupAndAdd( (*i)->GetTaxid(), &pNode )
                            && pNode ) {
                            pIt->GoNode( pNode );
                        } else { // Invalid parent specified
                            SetLastError( ("Invalid parent taxid "
                                           + NStr::NumericToString((*i)->GetTaxid())
                                           ).c_str() );
                            return false;
                        }
                    } else { // Add node to the partial tree
                        if( !m_plCache->Lookup((*i)->GetTaxid(), &pNode) ) {
                            pNode = new CTaxon1Node(*i);
                            m_plCache->SetIndexEntry(TAX_ID_TO(int, pNode->GetTaxId()), pNode);
                            pIt->AddChild( pNode );
                        }
                    }
                    pNode->SetSubtreeLoaded( pNode->IsSubtreeLoaded() ||
                                             (levels < 0) );
                }
                return true;
            } else { // Internal: wrong respond type
                SetLastError( "INTERNAL: TaxService response type is not Taxachildren" );
                return false;
            }
        }
    }
    return false;
}

CRef< ITreeIterator >
CTaxon1::GetTreeIterator( CTaxon1::EIteratorMode mode )
{
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return null;
	}
    }

    CRef< ITreeIterator > pIt;
    CTreeConstIterator* pIter = m_plCache->GetTree().GetConstIterator();

    switch( mode ) {
    default:
    case eIteratorMode_FullTree:
        pIt.Reset( new CFullTreeConstIterator( pIter ) );
        break;
    case eIteratorMode_LeavesBranches:
        pIt.Reset( new CTreeLeavesBranchesIterator( pIter ) );
        break;
    case eIteratorMode_Best:
        pIt.Reset( new CTreeBestIterator( pIter ) );
        break;
    case eIteratorMode_Blast:
        pIt.Reset( new CTreeBlastIterator( pIter ) );
        break;
    }
    SetLastError(NULL);
    return pIt;
}

CRef< ITreeIterator >
CTaxon1::GetTreeIterator( TTaxId tax_id, CTaxon1::EIteratorMode mode )
{
    CRef< ITreeIterator > pIt;
    CTaxon1Node* pData = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return null;
	}
    }
    if( m_plCache->LookupAndAdd( tax_id, &pData ) ) {
        pIt = GetTreeIterator( mode );
        if( !pIt->GoNode( pData ) ) {
            SetLastError( "Iterator in this mode cannot point to the node with"
                          " this tax id" );
            pIt.Reset( NULL );
        }
    }
    return pIt;
}

bool
CTaxon1::GetNodeProperty( TTaxId tax_id, const string& prop_name,
                          string& prop_val )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    CTaxon1_req req;
    CTaxon1_resp resp;
    CRef<CTaxon1_info> pProp( new CTaxon1_info() );

    CDiagAutoPrefix( "Taxon1::GetNodeProperty" );

    if( !prop_name.empty() ) {
        pProp->SetIval1(TAX_ID_TO(int, tax_id) );
        pProp->SetIval2( -1 ); // Get string property by name
        pProp->SetSval( prop_name );

        req.SetGetorgprop( *pProp );
        try {
            if( SendRequest( req, resp ) ) {
                if( !resp.IsGetorgprop() ) { // error
                    ERR_POST_X( 4, "Response type is not Getorgprop" );
		    SetLastError( "INTERNAL: TaxService response type is not Getorgprop" );
                } else {
                    if( resp.GetGetorgprop().size() > 0 ) {
                        CRef<CTaxon1_info> pInfo
                            ( resp.GetGetorgprop().front() );
                        prop_val.assign( pInfo->GetSval() );
                        return true;
                    }
                }
            } else if( resp.IsError()
                       && resp.GetError().GetLevel()
                       != CTaxon1_error::eLevel_none ) {
                string sErr;
                resp.GetError().GetErrorText( sErr );
                ERR_POST_X( 5, sErr );
            }
        } catch( exception& e ) {
            ERR_POST_X( 6, e.what() );
            SetLastError( e.what() );
        }
    } else {
        SetLastError( "Empty property name is not accepted" );
        ERR_POST_X( 7, GetLastError() );
    }
    return false;
}

bool
CTaxon1::GetNodeProperty( TTaxId tax_id, const string& prop_name,
                          bool& prop_val )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    CTaxon1_req req;
    CTaxon1_resp resp;
    CRef<CTaxon1_info> pProp( new CTaxon1_info() );

    CDiagAutoPrefix( "Taxon1::GetNodeProperty" );

    if( !prop_name.empty() ) {
        pProp->SetIval1(TAX_ID_TO(int, tax_id) );
        pProp->SetIval2( -3 ); // Get bool property by name
        pProp->SetSval( prop_name );

        req.SetGetorgprop( *pProp );
        try {
            if( SendRequest( req, resp ) ) {
                if( !resp.IsGetorgprop() ) { // error
                    ERR_POST_X( 8, "Response type is not Getorgprop" );
		    SetLastError( "INTERNAL: TaxService response type is not Getorgprop" );
                } else {
                    if( resp.GetGetorgprop().size() > 0 ) {
                        CRef<CTaxon1_info> pInfo
                            = resp.GetGetorgprop().front();
                        prop_val = pInfo->GetIval2() != 0;
                        return true;
                    }
                }
            } else if( resp.IsError()
                       && resp.GetError().GetLevel()
                       != CTaxon1_error::eLevel_none ) {
                string sErr;
                resp.GetError().GetErrorText( sErr );
                ERR_POST_X( 9, sErr );
            }
        } catch( exception& e ) {
            ERR_POST_X( 10, e.what() );
            SetLastError( e.what() );
        }
    } else {
        SetLastError( "Empty property name is not accepted" );
        ERR_POST_X( 11, GetLastError() );
    }
    return false;
}

bool
CTaxon1::GetNodeProperty( TTaxId tax_id, const string& prop_name,
                          int& prop_val )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    CTaxon1_req req;
    CTaxon1_resp resp;
    CRef<CTaxon1_info> pProp( new CTaxon1_info() );

    CDiagAutoPrefix( "Taxon1::GetNodeProperty" );

    if( !prop_name.empty() ) {
        pProp->SetIval1(TAX_ID_TO(int, tax_id) );
        pProp->SetIval2( -2 ); // Get int property by name
        pProp->SetSval( prop_name );

        req.SetGetorgprop( *pProp );
        try {
            if( SendRequest( req, resp ) ) {
                if( !resp.IsGetorgprop() ) { // error
                    ERR_POST_X( 12, "Response type is not Getorgprop" );
		    SetLastError( "INTERNAL: TaxService response type is not Getorgprop" );
                } else {
                    if( resp.GetGetorgprop().size() > 0 ) {
                        CRef<CTaxon1_info> pInfo
                            = resp.GetGetorgprop().front();
                        prop_val = pInfo->GetIval2();
                        return true;
                    }
                }
            } else if( resp.IsError()
                       && resp.GetError().GetLevel()
                       != CTaxon1_error::eLevel_none ) {
                string sErr;
                resp.GetError().GetErrorText( sErr );
                ERR_POST_X( 13, sErr );
            }
        } catch( exception& e ) {
            ERR_POST_X( 14, e.what() );
            SetLastError( e.what() );
        }
    } else {
        SetLastError( "Empty property name is not accepted" );
        ERR_POST_X( 15, GetLastError() );
    }
    return false;
}

bool
CTaxon1::GetInheritedPropertyDefines( const string& prop_name,
				      CTaxon1::TInfoList& results,
				      TTaxId root )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    CTaxon1_req req;
    CTaxon1_resp resp;
    CRef<CTaxon1_info> pProp( new CTaxon1_info() );

    CDiagAutoPrefix( "Taxon1::GetInheritedPropertyDefines" );

    if( !prop_name.empty() ) {
        pProp->SetIval1(TAX_ID_TO(int, -root) );
        pProp->SetIval2( -4 ); // Get inherited property defines by name
        pProp->SetSval( prop_name );

        req.SetGetorgprop( *pProp );
        try {
            if( SendRequest( req, resp ) ) {
                if( !resp.IsGetorgprop() ) { // error
                    ERR_POST_X( 12, "Response type is not Getorgprop" );
                } else {
                    if( resp.GetGetorgprop().size() > 0 ) {
			results = resp.SetGetorgprop();
                        return true;
                    }
                }
            } else if( resp.IsError()
                       && resp.GetError().GetLevel()
                       != CTaxon1_error::eLevel_none ) {
                string sErr;
                resp.GetError().GetErrorText( sErr );
                ERR_POST_X( 13, sErr );
            }
        } catch( exception& e ) {
            ERR_POST_X( 14, e.what() );
            SetLastError( e.what() );
        }
    } else {
        SetLastError( "Empty property name is not accepted" );
        ERR_POST_X( 15, GetLastError() );
    }
    return false;
}


//-----------------------------------
//  Iterator stuff
//
// 'Downward' traverse mode (nodes that closer to root processed first)
ITreeIterator::EAction
ITreeIterator::TraverseDownward(I4Each& cb, unsigned levels)
{
    if( levels ) {
        switch( cb.Execute(GetNode()) ) {
        default:
        case eOk:
            if(!IsTerminal()) {
                switch( cb.LevelBegin(GetNode()) ) {
                case eStop: return eStop;
                default:
                case eOk:
                    if(GoChild()) {
                        do {
                            if(TraverseDownward(cb, levels-1)==eStop)
                                return eStop;
                        } while(GoSibling());
                    }
                case eSkip: // Means skip this level
                    break;
                }
                GoParent();
                if( cb.LevelEnd(GetNode()) == eStop )
                    return eStop;
            }
        case eSkip: break;
        case eStop: return eStop;
        }
    }
    return eOk;
}

// 'Upward' traverse mode (nodes that closer to leaves processed first)
ITreeIterator::EAction
ITreeIterator::TraverseUpward(I4Each& cb, unsigned levels)
{
    if( levels > 0 ) {
        if(!IsTerminal()) {
            switch( cb.LevelBegin(GetNode()) ) {
            case eStop: return eStop;
            default:
            case eOk:
                if(GoChild()) {
                    do {
                        if( TraverseUpward(cb, levels-1) == eStop )
                            return eStop;
                    } while(GoSibling());
                }
            case eSkip: // Means skip this level
                break;
            }
            GoParent();
            if( cb.LevelEnd(GetNode()) == eStop )
                return eStop;
        }
        return cb.Execute(GetNode());
    }
    return eOk;
}

// 'LevelByLevel' traverse (nodes that closer to root processed first)
ITreeIterator::EAction
ITreeIterator::TraverseLevelByLevel(I4Each& cb, unsigned levels)
{
    switch( cb.Execute( GetNode() ) ) {
    case eStop:
        return eStop;
    case eSkip:
        return eSkip;
    case eOk:
    default:
        break;
    }
    if(!IsTerminal()) {
        vector< const ITaxon1Node* > skippedNodes;
        return TraverseLevelByLevelInternal(cb, levels, skippedNodes);
    }
    return eOk;
}

ITreeIterator::EAction
ITreeIterator::TraverseLevelByLevelInternal(I4Each& cb, unsigned levels,
                                            vector< const ITaxon1Node* >& skp)
{
    size_t skp_start = skp.size();
    if( levels > 1 ) {
        if(!IsTerminal()) {
            switch( cb.LevelBegin(GetNode()) ) {
            case eStop: return eStop;
            default:
            case eOk:
                if(GoChild()) {
                    // First pass - call Execute for all children
                    do {
                        switch( cb.Execute(GetNode()) ) {
                        default:
                        case eOk:
                            break;
                        case eSkip: // Means skip this node
                            skp.push_back( GetNode() );
                            break;
                        case eStop: return eStop;
                        }
                    } while( GoSibling() );
                    GoParent();
                    // Start second pass
                    size_t skp_cur = skp_start;
                    GoChild();
                    do {
                        if( skp.size() == skp_start ||
                            skp[skp_cur] != GetNode() ) {
                            if(TraverseLevelByLevelInternal(cb, levels-1, skp)
                               == eStop ) {
                                return eStop;
                            }
                        } else {
                            ++skp_cur;
                        }
                    } while(GoSibling());
                    GoParent();
                }
                if( cb.LevelEnd( GetNode() ) == eStop )
                    return eStop;
                break;
            case eSkip:
                break;
            }
        }
    }
    skp.resize( skp_start );
    return eOk;
}

// Scans all the ancestors starting from immediate parent up to the root
// (no levelBegin, levelEnd calls performed)
ITreeIterator::EAction
ITreeIterator::TraverseAncestors(I4Each& cb)
{
    const ITaxon1Node* pNode = GetNode();
    EAction stat = eOk;
    while( GoParent() ) {
        stat = cb.Execute(GetNode());
        switch( stat ) {
        case eStop: return eStop; // Stop scan, some error occurred
        default:
        case eOk:
        case eSkip: // Means skip further scan, no error generated
            break;
        }
        if( stat == eSkip ) {
            break;
        }
    }
    GoNode( pNode );
    return stat;
}

//-----------------------------------------------
// Checks whether OrgRef is current
// Returns: false on any error, stat_out filled with status flags
// (see above)
///
bool
CTaxon1::CheckOrgRef( const COrg_ref& orgRef, TOrgRefStatus& stat_out, string* psLog )
{
    CDiagAutoPrefix( "Taxon1::CheckOrgRef" );
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    stat_out = eStatus_Ok;

    CTaxon1_req  req;
    CTaxon1_resp resp;

    SerialAssign< COrg_ref >( req.SetLookup(), orgRef );
    // Set version db tag
    COrgrefProp::SetOrgrefProp( req.SetLookup(), "version", 2 );
    COrgrefProp::SetOrgrefProp( req.SetLookup(), "merge", true );
    if( psLog ) {
        COrgrefProp::SetOrgrefProp( req.SetLookup(), "log", true );
    }


    if( SendRequest( req, resp ) ) {
        if( resp.IsLookup() ) {
            // Correct response, return object
	    CRef<CTaxon2_data> pData( new CTaxon2_data() );
	    
            SerialAssign< COrg_ref >( pData->SetOrg(), resp.GetLookup().GetOrg() );
	    stat_out = x_ConvertOrgrefProps( *pData );
            if( psLog ) {
                pData->GetProperty( "log", *psLog );
            }

	    return true;
        } else { // Internal: wrong respond type
            SetLastError( "INTERNAL: TaxService response type is not Lookup" );
        }
    }

    return false;
}

//---------------------------------------------------
// This function returns the list of "type materials" for the node with taxid given.
// The list consists of names with class type material found at the species
// or subspecies ancestor of the node. 
// Returns: true  when success and last parameter is filled with type material list,
//          false when call failed
///
bool
CTaxon1::GetTypeMaterial( TTaxId tax_id, TNameList& type_material_list_out )
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    type_material_list_out.clear();
    if( m_plCache->LookupAndAdd( tax_id, &pNode )
        && pNode &&
	m_plCache->InitRanks() ) {
	list< CRef< CTaxon1_name > > lNames;

        int species_rank(m_plCache->GetSpeciesRank());
        int subspecies_rank(m_plCache->GetSubspeciesRank());
        while( !pNode->IsRoot() ) {
	    int rank( pNode->GetRank() );
	    if( rank == subspecies_rank ) {
		bool a,b;
		string blast_name;
		CConstRef< COrg_ref > subsporg( GetOrgRef( pNode->GetTaxId(), a, b, blast_name ) );
		if( subsporg->IsSetOrgname() && subsporg->GetOrgname().IsSetName() ) {
		    const COrgName::C_Name& on( subsporg->GetOrgname().GetName() );
		    if( on.IsBinomial() && on.GetBinomial().IsSetSpecies()
			&& on.GetBinomial().IsSetSubspecies()
			&& (NStr::EqualNocase( on.GetBinomial().GetSpecies(), on.GetBinomial().GetSubspecies() )
			    || NStr::EqualNocase( "ssp. "+on.GetBinomial().GetSpecies(), on.GetBinomial().GetSubspecies() )
			    || NStr::EqualNocase( "subsp. "+on.GetBinomial().GetSpecies(), on.GetBinomial().GetSubspecies() )) ) {
			// nominal subspecies
			pNode = pNode->GetParent();
			continue;
		    }
		}
		if( !GetAllNamesEx( pNode->GetTaxId(), lNames ) ) {
		    return false;
		}
		break;
	    } else if( rank == species_rank ) {
		if( !GetAllNamesEx( pNode->GetTaxId(), lNames ) ) {
		    return false;
		}
		break;
	    } else if( (rank > 0) && (rank < species_rank)) {
		SetLastError( "No species or subspecies found in lineage" );
		ERR_POST_X( 19, GetLastError() );
		return false;
	    }
	    pNode = pNode->GetParent();
        }
	// Filter out excessive name classes, leave type material only
	TTaxNameClass tm_cde = GetNameClassId( "type material" );
	if ( tm_cde < 0 ) {
	    SetLastError( "Name class for type material not found" );
	    ERR_POST_X( 19, GetLastError() );
	    return false;
	}
	// copy only type material
	ITERATE( list< CRef< CTaxon1_name > >, i, lNames ) {
	    if( (*i)->GetCde() == tm_cde ) {
		type_material_list_out.push_back( (*i)->GetOname() );
	    }
	}
        return true;
    } else {
	SetLastError( "No organisms found for tax id" );
	ERR_POST_X( 18, GetLastError() );
	return false;
    }
}

//---------------------------------------------------
// This function returns the maximal value for taxid
// or -1 in case of error
///
TTaxId 
CTaxon1::GetMaxTaxId( void )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return INVALID_TAX_ID;
	}
    }
    return TAX_ID_FROM(unsigned, m_plCache->m_nMaxTaxId);
}

//---------------------------------------------------
// This function constructs the "display common name" for the taxid following this algorithm:
//  Return first non-empty value from the following sequence:
//  1) the GenBank common name
//  2) the common name if there is only one
//  3) if taxid is subspecies
//    a) the species GenBank common name
//    b) the species common name if there is only one
//  4) the Blast inherited blast name
// Returns: true on success, false in case of error
///
bool
CTaxon1::GetDisplayCommonName( TTaxId tax_id, string& disp_name_out )
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    if( m_plCache->LookupAndAdd( tax_id, &pNode ) && pNode &&
        m_plCache->InitNameClasses() ) {
	tax_id = pNode->GetTaxId(); // get rid of secondary taxid
	TTaxNameClass cn = m_plCache->GetPreferredCommonNameClass();
	list< CRef< CTaxon1_name > > lNames;
	if( GetAllNamesEx(tax_id, lNames) ) {
	    // 1)
	    ITERATE( list< CRef< CTaxon1_name > >, ci, lNames ) {
		if( (*ci)->GetCde() == cn ) { // we have a winner
		    disp_name_out = (*ci)->GetOname();
		    return true;
		}
	    }
	    // 2)
	    cn = m_plCache->GetCommonNameClass();
	    list< CRef< CTaxon1_name > >::const_iterator lend = lNames.end();
	    ITERATE( list< CRef< CTaxon1_name > >, ci, lNames ) {
		if( (*ci)->GetCde() == cn ) { // we might have a winner
		    if( lend == lNames.end() ) {
			lend = ci;
		    } else { // another common name found
			lend = lNames.end();
			break;
		    }
		}
	    }
	    if( lend != lNames.end() ) {
		disp_name_out = (*lend)->GetOname();
		return true;
	    }
	}
	// 3)
// 	if( pNode->GetRank() == m_plCache->GetSubspeciesRank() ) {
	    // Get corresponding species
	    TTaxId species_id = GetSpecies(tax_id);
	    if( species_id < ZERO_TAX_ID ) {
		return false;
	    } else if( species_id > ZERO_TAX_ID && species_id != tax_id ) {
		lNames.clear();
		cn = m_plCache->GetPreferredCommonNameClass();
		if( GetAllNamesEx(species_id, lNames) ) {
		    // 3a)
		    ITERATE( list< CRef< CTaxon1_name > >, ci, lNames ) {
			if( (*ci)->GetCde() == cn ) { // we have a winner
			    disp_name_out = (*ci)->GetOname();
			    return true;
			}
		    }
		    // 3b)
		    cn = m_plCache->GetCommonNameClass();
		    list< CRef< CTaxon1_name > >::const_iterator lend = lNames.end();
		    ITERATE( list< CRef< CTaxon1_name > >, ci, lNames ) {
			if( (*ci)->GetCde() == cn ) { // we might have a winner
			    if( lend == lNames.end() ) {
				lend = ci;
			    } else { // another common name found
				lend = lNames.end();
				break;
			    }
			}
		    }
		    if( lend != lNames.end() ) {
			disp_name_out = (*lend)->GetOname();
			return true;
		    }
		}
	    }
// 	}
	// 4)
	if( !GetBlastName(tax_id, disp_name_out ) ) {
	    return false;
	}
    } else {
	return false;
    }
    return true;
}

//--------------------------------------------------
// Get scientific name for taxonomy id
// Returns: false if some error occurred (name_out not changed)
//          true  if Ok
//                name_out contains scientific name of this node
///
bool 
CTaxon1::GetScientificName(TTaxId tax_id, string& name_out)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return false;
	}
    }
    if( m_plCache->LookupAndAdd( tax_id, &pNode ) && pNode ) {
	if( !pNode->GetName().empty() ) {
	    name_out.assign( pNode->GetName() );
	    return true;
	} else {
	    SetLastError( "ERROR: No scientific name at the node" );
	}
    }
    return false;
}

END_objects_SCOPE
END_NCBI_SCOPE
