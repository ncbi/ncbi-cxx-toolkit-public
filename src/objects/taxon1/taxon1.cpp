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
#include <algorithm>
#include <objects/taxon1/taxon1.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

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
    return;
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
CTaxon1::Init(void)
{
    static const STimeout def_timeout = { 120, 0 };
    return CTaxon1::Init(&def_timeout);
}


bool
CTaxon1::Init(const STimeout* timeout, unsigned reconnect_attempts)
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

        if ( timeout ) {
            m_timeout_value = *timeout;
            m_timeout = &m_timeout_value;
        } else {
            m_timeout = 0;
        }

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
                                             0, 0, m_timeout) );

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

CRef< CTaxon2_data >
CTaxon1::Lookup(const COrg_ref& inp_orgRef )
{
    SetLastError(NULL);
    // Check if this taxon is in cache
    CTaxon2_data* pData = 0;

    int tax_id = GetTaxIdByOrgRef( inp_orgRef );

    if( tax_id > 0
        && m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {

        CTaxon2_data* pNewData = new CTaxon2_data();

        SerialAssign<CTaxon2_data>( *pNewData, *pData  );

        return CRef<CTaxon2_data>(pNewData);
    }
    return CRef<CTaxon2_data>(NULL);
}

class PFindMod {
public:
    void SetModToMatch( const CRef< COrgMod >& mod ) {
	CanonizeName( mod->GetSubname(), m_sName );
	m_nType = mod->GetSubtype();
    }

    bool operator()( const CRef< COrgMod >& mod ) const {
	if( m_nType == mod->GetSubtype() ) {
	    string sCanoName;
	    CanonizeName( mod->GetSubname(), sCanoName );
	    return ( sCanoName == m_sName );
	}
	return false;
    }

    void CanonizeName( const string& in, string& out ) const {
	bool bSpace = true;
	char prevc = '\0';
	for( size_t i = 0; i < in.size(); ++i ) {
	    if( bSpace ) {
		if( !isspace(in[i]) ) {
		    bSpace = false;
		    if( prevc )
			out += tolower(prevc);
		    prevc = in[i];
		}
	    } else {
		if( prevc )
		    out += tolower(prevc);
		if( isspace(in[i]) ) {
		    prevc = ' ';
		    bSpace = true;
		} else {
		    prevc = in[i];
		}
	    }
	}
	if( prevc && prevc != ' ' )
	    out += tolower(prevc);
    }

private:
    string  m_sName;
    int     m_nType;
};

CConstRef< CTaxon2_data >
CTaxon1::LookupMerge(COrg_ref& inp_orgRef )
{
    CTaxon2_data* pData = 0;

    SetLastError(NULL);
    int tax_id = GetTaxIdByOrgRef( inp_orgRef );

    if( tax_id > 0
        && m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {

        const COrg_ref& src = pData->GetOrg();

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
	
	// Remember old modifiers
	COrgName::TMod modd;
	modd.swap( inp_orgRef.SetOrgname().SetMod() );
	//        if( !inp_orgRef.GetOrgname().IsSetMod() ) {
	const COrgName::TMod& mods = src.GetOrgname().GetMod();
	PFindMod predMod;

	// Service stuff
	CTaxon1_req req;
	CTaxon1_resp resp;
	CRef<CTaxon1_info> pModInfo(new CTaxon1_info());
	// Copy with object copy as well
	for( COrgName::TMod::const_iterator ci = mods.begin();
	     ci != mods.end();
	     ++ci ) {
	    // Check if there is no such modifier in the destination
	    predMod.SetModToMatch( *ci );
	    COrgName::TMod::iterator di =
		find_if( modd.begin(), modd.end(), predMod );
	    if( di != modd.end() ) { // Modifier found
		modd.erase( di );
	    }
	}
	// Check the remaining modifiers for validity
	for( COrgName::TMod::iterator di = modd.begin();
	     di != modd.end(); ) { // Check modifier that was not found
	    pModInfo->SetIval1( tax_id );
	    pModInfo->SetIval2( (*di)->GetSubtype() );
	    pModInfo->SetSval( (*di)->GetSubname() );
	    
	    req.SetGetorgmod( *pModInfo );
	    try {
		if( SendRequest( req, resp ) ) {
		    if( !resp.IsGetorgmod() ) { // error
			SetLastError( 
				     "ERROR: Response type is not Getorgmod" );
		    } else {
			++di;
			continue;
		    }
		} else if( resp.IsError()
			   && resp.GetError().GetLevel() 
			   == CTaxon1_error::eLevel_none ) {
		    // Just delete modifier
		}
	    } catch( exception& e ) {
		SetLastError( e.what() );
	    }
	    modd.erase( di );
	}
// 	// Copy all modifiers from source
// 	for( COrgName::TMod::const_iterator ci = mods.begin();
// 	     ci != mods.end();
// 	     ++ci ) {
// 	    COrgMod* pMod = new COrgMod;
// 	    SerialAssign<COrgMod>( *pMod, ci->GetObject() );
// 	    modd.push_back( pMod );
// 	}
        /* copy orgname */
        inp_orgRef.SetOrgname().Assign( src.GetOrgname() );
	// Set modifiers
	COrgName::TMod& inp_mods = inp_orgRef.SetOrgname().SetMod();
	inp_mods.splice( inp_mods.end(), modd );

        if( src.GetOrgname().IsSetLineage() )
            inp_orgRef.SetOrgname()
                .SetLineage( src.GetOrgname().GetLineage() );
        else
            inp_orgRef.SetOrgname().ResetLineage();
        inp_orgRef.SetOrgname().SetGcode( src.GetOrgname().GetGcode() );
        inp_orgRef.SetOrgname().SetMgcode( src.GetOrgname().GetMgcode() );
        inp_orgRef.SetOrgname().SetDiv( src.GetOrgname().GetDiv() );
    }
    return CConstRef<CTaxon2_data>(pData);
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
            const list< CRef< CTaxon1_name > >& lNm = resp.GetFindname();
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
                   string& blast_name)
{
    SetLastError(NULL);
    if( tax_id > 1 ) {
        CTaxon2_data* pData = 0;
        if( m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {
            is_species = pData->GetIs_species_level();
            is_uncultured = pData->GetIs_uncultured();
            if( pData->GetBlast_name().size() > 0 ) {
                blast_name.assign( pData->GetBlast_name().front() );
            }
            return CConstRef<COrg_ref>(&pData->GetOrg());
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
    CTaxon1Node* pNode = 0;
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
    CTaxon1Node* pNode = 0;
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
CTaxon1::GetSuperkingdom(int id_tax)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode ) {
        int sk_rank(m_plCache->GetSuperkingdomRank());
        while( !pNode->IsRoot() ) {
            int rank( pNode->GetRank() );
            if( rank == sk_rank )
                return pNode->GetTaxId();
            if( (rank > 0) && (rank < sk_rank))
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
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode ) {

        CTaxon1_req  req;
        CTaxon1_resp resp;
	
        req.SetTaxachildren( id_tax );
	
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
CTaxon1::GetGCName(short gc_id, string& gc_name_out )
{
    SetLastError(NULL);
    if( m_gcStorage.empty() ) {
        CTaxon1_req  req;
        CTaxon1_resp resp;

        req.SetGetgcs( true );

        if( SendRequest( req, resp ) ) {
            if( resp.IsGetgcs() ) {
                // Correct response, return object
                const list< CRef< CTaxon1_info > >& lGc = resp.GetGetgcs();
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
        CTreeConstIterator* pIt = m_plCache->GetTree().GetConstIterator();
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
            const list< CRef< CTaxon1_name > >& lNm = resp.GetGetorgnames();
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
 * 1. orgname is substring of search_str which matches organism name
 *    (return parameter).
 * 2. Ids consists of tax_ids. Caller is responsible to free this memory
 */
 // int getTaxId4Str(const char* search_str, char** orgname, intPtr *Ids_out);

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
    } else {
        SetLastError( "Not connected to Taxonomy service" );
    }
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
CTaxon1::GetBlastName(int tax_id, string& blast_name_out )
{
    CTaxon1Node* pNode = 0;
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
                            string err;
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
                                                     0, 0, m_timeout) );
		
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

static void s_StoreResidueTaxid( CTreeIterator* pIt, CTaxon1::TTaxIdList& lTo )
{
    CTaxon1Node* pNode =  static_cast<CTaxon1Node*>( pIt->GetNode() );
    if( !pNode->IsTerminal() ) {
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
    if( ids_in.size() > 0 ) {
	map< int, CTaxon1Node* > nodeMap;
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
	    map< int, CTaxon1Node* >::iterator nmi = nodeMap.find( *ci );
	    if( nmi == nodeMap.end() ) {
		if( m_plCache->LookupAndAdd( *ci, &pNode ) ) {
		    if( !tPartTree.GetRoot() ) {
			pNewParent = new CTaxon1Node
			    ( *static_cast<const CTaxon1Node*>
			      (m_plCache->GetTree().GetRoot()) );
			tPartTree.SetRoot( pNewParent );
			nodeMap.insert( map< int,CTaxon1Node* >::value_type
					(pNewParent->GetTaxId(), pNewParent) );
		    }
		    if( pNode ) {
			vLin.clear();
			pParent = pNode->GetParent();
			pNode = new CTaxon1Node( *pNode );
			pNode->SetTerminal();
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
			    nodeMap.insert( map< int,CTaxon1Node* >::value_type
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
		nmi->second->SetTerminal();
	    }
	}
	// Partial tree is build, make a residue
	pIt->GoRoot();
	bHasSiblings = true;
	if( pIt->GoChild() ) {
	    while( !pIt->GoSibling() ) {
		pNode = static_cast<CTaxon1Node*>( pIt->GetNode() );
		if( pNode->IsTerminal() || !pIt->GoChild() ) {
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
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE



/*
 * $Log$
 * Revision 6.11  2003/01/21 19:38:59  domrach
 * GetPopsetJoin() member function optimized to copy partial tree from cached tree
 *
 * Revision 6.10  2003/01/10 19:58:46  domrach
 * Function GetPopsetJoin() added to CTaxon1 class
 *
 * Revision 6.9  2002/11/08 20:55:58  ucko
 * CConstRef<> now requires an explicit constructor.
 *
 * Revision 6.8  2002/11/08 14:39:52  domrach
 * Member function GetSuperkingdom() added
 *
 * Revision 6.7  2002/11/04 21:29:18  grichenk
 * Fixed usage of const CRef<> and CRef<> constructor
 *
 * Revision 6.6  2002/08/06 16:12:34  domrach
 * Merging of modifiers is now performed in LookupMerge
 *
 * Revision 6.5  2002/07/25 15:01:56  grichenk
 * Replaced non-const GetXXX() with SetXXX()
 *
 * Revision 6.4  2002/02/14 22:44:50  vakatov
 * Use STimeout instead of time_t.
 * Get rid of warnings and extraneous #include's, shuffled code a little.
 *
 * Revision 6.3  2002/01/31 00:31:26  vakatov
 * Follow the renaming of "CTreeCont.hpp" to "ctreecont.hpp".
 * Get rid of "std::" which is unnecessary and sometimes un-compilable.
 * Also done some source identation/beautification.
 *
 * Revision 6.2  2002/01/30 16:13:38  domrach
 * Changes made to pass through MSVC compiler. Some src files renamed
 *
 * Revision 6.1  2002/01/28 19:56:10  domrach
 * Initial checkin of the library implementation files
 *
 * ===========================================================================
 */
