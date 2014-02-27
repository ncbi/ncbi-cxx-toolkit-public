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
#include <objects/seqfeat/seqfeat__.hpp>
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


bool
CTaxon1::Init(void)
{
    static const STimeout def_timeout = { 120, 0 };
    return CTaxon1::Init(&def_timeout);
}

bool
CTaxon1::Init(unsigned cache_capacity)
{
    static const STimeout def_timeout = { 120, 0 };
    return CTaxon1::Init(&def_timeout, 5, cache_capacity);
}

bool
CTaxon1::Init(const STimeout* timeout, unsigned reconnect_attempts,
              unsigned cache_capacity)
{
    SetLastError(NULL);
    if( TAXON1_IS_INITED ) { // Already inited
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
        if( ( (tmp=getenv("NI_TAXONOMY_SERVICE_NAME")) != NULL ) ||
            ( (tmp=getenv("NI_SERVICE_NAME_TAXONOMY")) != NULL ) ) {
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
    if( TAXON1_IS_INITED ) {
        CTaxon1_req req;
        CTaxon1_resp resp;

        req.SetFini();

        if( SendRequest( req, resp, false ) ) {
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
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return CRef<CTaxon2_data>(NULL);
	}
    }
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
                if( !isspace((unsigned char) in[i]) ) {
                    bSpace = false;
                    if( prevc )
                        out += tolower((unsigned char) prevc);
                    prevc = in[i];
                }
            } else {
                if( prevc )
                    out += tolower((unsigned char) prevc);
                if( isspace((unsigned char) in[i]) ) {
                    prevc = ' ';
                    bSpace = true;
                } else {
                    prevc = in[i];
                }
            }
        }
        if( prevc && prevc != ' ' )
            out += tolower((unsigned char) prevc);
    }

private:
    string  m_sName;
    int     m_nType;
};

class PFindConflict {
public:
    void SetTypeToMatch( int type ) {
        m_nType = type;
        switch( type ) {
        case COrgMod::eSubtype_strain:
        case COrgMod::eSubtype_variety:
            //case COrgMod::eSubtype_sub_species:
            m_bSubSpecType = true;
            break;
        default:
            m_bSubSpecType = false;
            break;
        }
    }

    bool operator()( const CRef< COrgMod >& mod ) const {
        // mod is the destination modifier
        if( m_nType == COrgMod::eSubtype_other ) {
            return true;
        }
        if( m_nType == mod->GetSubtype() ) {
            return true;
        }
#if 0
        switch( mod->GetSubtype() ) {
        case COrgMod::eSubtype_strain:
        case COrgMod::eSubtype_substrain:
        case COrgMod::eSubtype_type:
        case COrgMod::eSubtype_subtype:
        case COrgMod::eSubtype_variety:
        case COrgMod::eSubtype_serotype:
        case COrgMod::eSubtype_serogroup:
        case COrgMod::eSubtype_serovar:
        case COrgMod::eSubtype_cultivar:
        case COrgMod::eSubtype_pathovar:
        case COrgMod::eSubtype_chemovar:
        case COrgMod::eSubtype_biovar:
        case COrgMod::eSubtype_biotype:
        case COrgMod::eSubtype_group:
        case COrgMod::eSubtype_subgroup:
        case COrgMod::eSubtype_isolate:
            //  case COrgMod::eSubtype_sub_species:
            return m_bSubSpecType;

        default:
            break;
        }
#endif
        return false;
    }

private:
    int     m_nType;
    bool    m_bSubSpecType;
};

class PFindModByType {
public:
    PFindModByType( int type ) : m_nType( type ) {}

    bool operator()( const CRef< COrgMod >& mod ) const {
        return ( m_nType == mod->GetSubtype() );
    }
private:
    int     m_nType;
};

class PRemoveSynAnamorph {
public:
    PRemoveSynAnamorph( const string& sTaxname ) : m_sName( sTaxname ) {}

    bool operator()( const CRef< COrgMod >& mod ) const {
        switch( mod->GetSubtype() ) {
        case COrgMod::eSubtype_synonym:
        case COrgMod::eSubtype_anamorph:
            return (NStr::CompareNocase( m_sName, mod->GetSubname() ) == 0);
        default:
            break;
        }
        return false;
    }

private:
    const string& m_sName;
};

void
CTaxon1::OrgRefAdjust( COrg_ref& inp_orgRef, const COrg_ref& db_orgRef,
                       int tax_id )
{
    inp_orgRef.ResetCommon();
    inp_orgRef.ResetSyn();

    // fill-up inp_orgRef based on db_orgRef
    inp_orgRef.SetTaxname( db_orgRef.GetTaxname() );
    if( db_orgRef.IsSetCommon() ) {
        inp_orgRef.SetCommon( db_orgRef.GetCommon() );
    }
    // Set tax id
    inp_orgRef.SetTaxId( tax_id );
    // copy the synonym list
    if( m_bWithSynonyms && db_orgRef.IsSetSyn() ) {
        inp_orgRef.SetSyn() = db_orgRef.GetSyn();
    }

    // copy orgname
    COrgName& on = inp_orgRef.SetOrgname();

    // Copy the orgname
    on.SetName().Assign( db_orgRef.GetOrgname().GetName() );

    bool bHasMod = on.IsSetMod();
    const COrgName::TMod& lSrcMod = db_orgRef.GetOrgname().GetMod();
    COrgName::TMod& lDstMod = on.SetMod();

    if( bHasMod ) { // Merge modifiers
        // Find and remove gb_xxx modifiers
        // tc2proc.c: CleanOrgName
        // Service stuff
        CTaxon1_req req;
        CTaxon1_resp resp;
        CRef<CTaxon1_info> pModInfo( new CTaxon1_info() );

        PushDiagPostPrefix( "Taxon1::OrgRefAdjust" );

        for( COrgName::TMod::iterator i = lDstMod.begin();
             i != lDstMod.end(); ) {
            switch( (*i)->GetSubtype() ) {
            case COrgMod::eSubtype_gb_acronym:
            case COrgMod::eSubtype_gb_anamorph:
            case COrgMod::eSubtype_gb_synonym:
                i = lDstMod.erase( i );
                break;
            default: // Check the modifier validity
                if( (*i)->CanGetSubname() && (*i)->CanGetSubtype() &&
                    !(*i)->GetSubname().empty() && (*i)->GetSubtype() != 0 ) {
                    pModInfo->SetIval1( tax_id );
                    pModInfo->SetIval2( (*i)->GetSubtype() );
                    pModInfo->SetSval( (*i)->GetSubname() );

                    req.SetGetorgmod( *pModInfo );
                    try {
                        if( SendRequest( req, resp ) ) {
                            if( !resp.IsGetorgmod() ) { // error
                                ERR_POST_X( 1, "Response type is not Getorgmod" );
                            } else {
                                if( resp.GetGetorgmod().size() > 0 ) {
                                    CRef<CTaxon1_info> pInfo
                                        = resp.GetGetorgmod().front();
                                    if( pInfo->GetIval1() == tax_id ) {
                                        if( pInfo->GetIval2() == 0 ) {
                                            // Modifier is wrong (probably, hidden)
                                            i = lDstMod.erase( i );
                                            continue;
                                        } else {
                                            (*i)->SetSubname( pInfo->GetSval() );
                                            (*i)->SetSubtype( COrgMod::TSubtype( pInfo->GetIval2() ) );
                                        }
                                    } else if( pInfo->GetIval1() != 0 ) {
                                        // Another redirection occurred
                                        // leave modifier but issue warning
                                        NCBI_NS_NCBI::CNcbiDiag(eDiag_Warning)
                                            << NCBI_NS_NCBI::ErrCode(NCBI_ERRCODE_X, 19)
                                            << "OrgMod type="
                                            << COrgMod::GetTypeInfo_enum_ESubtype()
                                            ->FindName( (*i)->GetSubtype(), true )
                                            << " name='" << (*i)->GetSubname()
                                            << "' causing illegal redirection"
                                            << NCBI_NS_NCBI::Endm;
                                    }
                                }
                            }
                        } else if( resp.IsError()
                                   && resp.GetError().GetLevel()
                                   != CTaxon1_error::eLevel_none ) {
                            string sErr;
                            resp.GetError().GetErrorText( sErr );
                            ERR_POST_X( 2, sErr );
                        }
                    } catch( exception& e ) {
                        ERR_POST_X( 3, e.what() );
                    }

                }

                ++i;
                break;
            }
        }

        PopDiagPostPrefix();

        PFindConflict predConflict;

        for( COrgName::TMod::const_iterator i = lSrcMod.begin();
             i != lSrcMod.end();
             ++i ) {
            predConflict.SetTypeToMatch( (*i)->GetSubtype() );
            if( (*i)->GetSubtype() != COrgMod::eSubtype_other ) {
                if( find_if( lDstMod.begin(), lDstMod.end(), predConflict )
                    == lDstMod.end() ) {
                    CRef<COrgMod> pMod( new COrgMod() );
                    pMod->Assign( *(*i) );
                    lDstMod.push_back( pMod );
                }
            }
        }
    } else { // Copy modifiers

        CRef<COrgMod> pMod;
        for( COrgName::TMod::const_iterator i = lSrcMod.begin();
             i != lSrcMod.end();
             ++i ) {
            switch( (*i)->GetSubtype() ) {
            case COrgMod::eSubtype_gb_acronym:
            case COrgMod::eSubtype_gb_anamorph:
            case COrgMod::eSubtype_gb_synonym:
                pMod.Reset( new COrgMod() );
                pMod->Assign( *(*i) );
                lDstMod.push_back( pMod );
            default:
                break;
            }
        }
        // Remove 'other' modifiers
        PFindModByType fmbt( COrgMod::eSubtype_other );
        remove_if( lDstMod.begin(), lDstMod.end(), fmbt );
    }
    // Remove 'synonym' or 'anamorph' it if coincides with taxname
    PRemoveSynAnamorph rsa( inp_orgRef.GetTaxname() );
    remove_if( lDstMod.begin(), lDstMod.end(), rsa );

    // Remove duplicated modifiers
    for( COrgName::TMod::iterator i = lDstMod.begin();
	 i != lDstMod.end();
	 ++i ) {
	COrgName::TMod::iterator ii = i;
	for( COrgName::TMod::iterator j = ++ii;
	 j != lDstMod.end(); ) {
	    if( (*i)->GetSubtype() == (*j)->GetSubtype() &&
		NStr::EqualNocase( (*i)->GetSubname(), (*j)->GetSubname() ) ) {
		j = lDstMod.erase( j );
	    } else {
		++j;
	    }
	}
    }

    // Reset destination modifiers if empty
    if( lDstMod.size() == 0 ) {
        on.ResetMod();
    }
    // Copy lineage
    if( db_orgRef.GetOrgname().IsSetLineage() ) {
        on.SetLineage() = db_orgRef.GetOrgname().GetLineage();
    } else {
        on.ResetLineage();
    }
    if( db_orgRef.GetOrgname().IsSetGcode() ) {
        on.SetGcode( db_orgRef.GetOrgname().GetGcode() );
    } else {
        on.ResetGcode();
    }
    if( db_orgRef.GetOrgname().IsSetMgcode() ) {
        on.SetMgcode( db_orgRef.GetOrgname().GetMgcode() );
    } else {
        on.ResetMgcode();
    }
    if( db_orgRef.GetOrgname().IsSetDiv() ) {
        on.SetDiv( db_orgRef.GetOrgname().GetDiv() );
    } else {
        on.ResetDiv();
    }
    if( db_orgRef.GetOrgname().IsSetPgcode() ) {
        on.SetPgcode( db_orgRef.GetOrgname().GetPgcode() );
    } else {
        on.ResetPgcode();
    }
}

bool
CTaxon1::LookupByOrgRef(const COrg_ref& inp_orgRef, int* pTaxid,
                        COrgName::TMod& hitMods )
{
    SetLastError(NULL);

    CTaxon1_req  req;
    CTaxon1_resp resp;

    SerialAssign< COrg_ref >( req.SetLookup(), inp_orgRef );

    if( SendRequest( req, resp ) ) {
        if( resp.IsLookup() ) {
            // Correct response, return object
            COrg_ref& result = resp.SetLookup().SetOrg();
            *pTaxid = result.GetTaxId();
            if( result.IsSetOrgname() &&
                result.GetOrgname().IsSetMod() ) {
                hitMods.swap( result.SetOrgname().SetMod() );
            }
            //      for( COrgName::TMod::const_iterator ci =
            //           result.GetOrgname().GetMod().begin();
            //           ci != result.GetOrgname().GetMod().end();
            //           ++ci ) {
            //          if( (*ci)->GetSubtype() == COrgMod::eSubtype_old_name ) {
            //          hitMod->Assign( *ci );
            //          bHitFound = true;
            //          break;
            //          }
            //      }
            //      }
            //      if( bHitFound ) {
            //      hitMod.Reset( NULL );
            //      }
            return true;
        } else { // Internal: wrong respond type
            SetLastError( "Response type is not Lookup" );
        }
    }
    return false;
}

void
CTaxon1::PopulateReplaced( COrg_ref& org, COrgName::TMod& lMods  )
{
    if( org.IsSetOrgname() ) {
        CRef< COrgMod > pOldNameMod;
        COrgName& on = org.SetOrgname();
        for( COrgName::TMod::iterator i = lMods.begin();
             i != lMods.end();
             ++i ) {
            if( (*i)->GetSubtype() == COrgMod::eSubtype_old_name ) {
                pOldNameMod = *i;
                continue;
            }
            if( on.IsSetMod() ) {
                PFindModByType fmbt( (*i)->GetSubtype() );
                if( find_if( on.GetMod().begin(), on.GetMod().end(), fmbt )
                    != on.GetMod().end() ) {
                    /* modifier already present in target orgref */
                    continue;
                }
            }
            /* adding this modifier */
            on.SetMod().push_back( *i );
        }
        if( pOldNameMod ) {
            if( on.IsSetMod() ) {
                PFindModByType fmbt( COrgMod::eSubtype_old_name );
                COrgName::TMod::iterator i =
                    find_if( on.SetMod().begin(), on.SetMod().end(), fmbt );
                if( i != on.SetMod().end() ) {
                    // There is old-name in the target already
                    if( !(*i)->IsSetAttrib() && pOldNameMod->IsSetAttrib() &&
                        NStr::CompareNocase(pOldNameMod->GetSubname(),
                                            (*i)->GetSubname() ) == 0 ) {
                        (*i)->SetAttrib( pOldNameMod->GetAttrib() );
                    }
                    return;
                }
            }
            /* we probably don't need to populate search name */
            if( org.IsSetTaxname() &&
                NStr::CompareNocase( org.GetTaxname(),
                                     pOldNameMod->GetSubname() ) == 0 ) {
                if( pOldNameMod->IsSetAttrib() ) {
                    const string& sAttrib = pOldNameMod->GetAttrib();
                    if( !sAttrib.empty() && sAttrib[0] == '(' ) {
                        try {
                            CRef< COrgMod > srchMod( new COrgMod );
                            string::size_type pos = sAttrib.find("=");
                            if( pos == string::npos ) {
                                return;
                            }
                            if( on.IsSetMod() ) {
                                const COrgName::TMod& mods = on.GetMod();
                                srchMod->SetSubname()
                                    .assign( sAttrib.c_str()+pos+1 );
                                srchMod->SetSubtype
                                    ( COrgMod::TSubtype
                                      (NStr::StringToInt
                                       (sAttrib.substr(1, pos-1),
                                        NStr::fAllowTrailingSymbols) ) );
                                PFindMod mf;
                                mf.SetModToMatch( srchMod );
                                if( find_if( mods.begin(), mods.end(),
                                             mf ) != mods.end() ) {
                                    return;
                                }
                            }
                        } catch(...) { return; }
                    } else
                        return;
                } else
                    return;
            }
            // Add old-name to modifiers
            on.SetMod().push_back( pOldNameMod );
        }
    }
}

CRef< CTaxon2_data >
CTaxon1::Lookup(const COrg_ref& inp_orgRef )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return CRef<CTaxon2_data>(NULL);
	}
    }
    // Check if this taxon is in cache
    CTaxon2_data* pData = 0;
    COrgName::TMod hitMod;
    int tax_id = 0;

    if( LookupByOrgRef( inp_orgRef, &tax_id, hitMod )
        && tax_id > 0
        && m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {

        CTaxon2_data* pNewData = new CTaxon2_data();

        COrg_ref* pOrf = new COrg_ref;
        pOrf->Assign( inp_orgRef );
        if( pOrf->IsSetOrgname() && pOrf->GetOrgname().IsSetMod() ) {
            // Clean up modifiers
            pOrf->SetOrgname().ResetMod();
        }
        pNewData->SetOrg( *pOrf );

        const COrg_ref& db_orgRef = pData->GetOrg();

        OrgRefAdjust( pNewData->SetOrg(), db_orgRef, tax_id );
        // Copy all other fields
        if( pData->IsSetBlast_name() ) {
            pNewData->SetBlast_name() = pData->GetBlast_name();
        }
        if( pData->IsSetIs_uncultured() ) {
            pNewData->SetIs_uncultured( pData->GetIs_uncultured() );
        }
        if( pData->IsSetIs_species_level() ) {
            pNewData->SetIs_species_level( pData->GetIs_species_level() );
        }
        // Insert the hitMod if necessary
        if( hitMod.size() > 0 ) {
            PopulateReplaced( pNewData->SetOrg(), hitMod );
        }

        return CRef<CTaxon2_data>(pNewData);
    }
    return CRef<CTaxon2_data>(NULL);
}

CConstRef< CTaxon2_data >
CTaxon1::LookupMerge(COrg_ref& inp_orgRef )
{
    CTaxon2_data* pData = 0;

    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return CConstRef<CTaxon2_data>(NULL);
	}
    }
    COrgName::TMod hitMod;
    int tax_id = 0; //GetTaxIdByOrgRef( inp_orgRef );

    if( LookupByOrgRef( inp_orgRef, &tax_id, hitMod )
        && tax_id > 0
        && m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {

        const COrg_ref& db_orgRef = pData->GetOrg();

        OrgRefAdjust( inp_orgRef, db_orgRef, tax_id );

        if( hitMod.size() > 0 ) {
            PopulateReplaced( inp_orgRef, hitMod );
        }
    }
    return CConstRef<CTaxon2_data>(pData);
}

int
CTaxon1::GetTaxIdByOrgRef(const COrg_ref& inp_orgRef)
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }

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
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }
    if( orgname.empty() )
        return 0;

    int id( GetTaxIdByName(orgname) );

    if(id < 1) {

        int idu = 0;

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

//----------------------------------------------
// Get tax_id by organism name using fancy search modes. If given a pointer
// to the list of names then it'll return all found names (one name per 
// tax id). Previous content of name_list_out will be destroyed.
// Returns: tax_id - if organism found
//               0 - no organism found
//              -1 - if multiple nodes found
//              -2 - error during processing occured
///
int
CTaxon1::SearchTaxIdByName(const string& orgname, ESearch mode,
                           list< CRef< CTaxon1_name > >* pNameList)
{
    // Use fancy searches
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -2;
	}
    }
    if( orgname.empty() ) {
        return 0;
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
            int retc = 0;
            const CTaxon1_resp::TSearchname& lNm = resp.GetSearchname();
            if( lNm.size() == 0 ) {
                retc = 0;
            } else if( lNm.size() == 1 ) {
                retc = lNm.front()->GetTaxid();
            } else {
                retc = -1;
            }
            // Fill the names list
            if( pNameList ) {
                pNameList->swap( resp.SetSearchname() );
            }
            return retc;
        } else { // Internal: wrong respond type
            SetLastError( "Response type is not Searchname" );
            return 0;
        }
    }
    return 0;
}

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
            SetLastError( "Response type is not Findname" );
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
CTaxon1::GetOrgRef(int tax_id,
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
    if( tax_id > 0 ) {
        CTaxon2_data* pData = 0;
        if( m_plCache->LookupAndInsert( tax_id, &pData ) && pData ) {
            is_species = pData->GetIs_species_level();
            is_uncultured = pData->GetIs_uncultured();
            if( pData->GetBlast_name().size() > 0 ) {
                blast_name.assign( pData->GetBlast_name().front() );
            }
	    if( is_specified ) {
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
int
CTaxon1::GetParent(int id_tax)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return 0;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode && pNode->GetParent() ) {
        return pNode->GetParent()->GetTaxId();
    }
    return 0;
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
int
CTaxon1::GetSpecies(int id_tax, ESpeciesMode mode)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode && m_plCache->InitRanks() ) {
    if( mode == eSpeciesMode_RankOnly ) {
        int species_rank(m_plCache->GetSpeciesRank());
        while( !pNode->IsRoot() ) {
        int rank( pNode->GetRank() );
        if( rank == species_rank )
            return pNode->GetTaxId();
        if( (rank > 0) && (rank < species_rank))
            return 0;
        pNode = pNode->GetParent();
        }
        return 0;
    } else { // Based on flag
        CTaxon1Node* pResult = NULL;
        CTaxon2_data* pData = NULL;
        while( !pNode->IsRoot() ) {
        if( m_plCache->LookupAndInsert( pNode->GetTaxId(), &pData ) ) {
            if( !pData )
            return -1;
            if( !(pData->IsSetIs_species_level() &&
              pData->GetIs_species_level()) ) {
            if( pResult ) {
                return pResult->GetTaxId();
            } else {
                return 0;
            }
            }
            pResult = pNode;
            pNode = pNode->GetParent();
        } else { // Node in the lineage not found
            return -1;
        }
        }
    }
    }
    return -1;
}

//---------------------------------------------
// Get genus tax_id (id_tax should be below genus)
// Returns: tax_id of genus or
//               0 - no genus in the lineage
//              -1 - if error
///
int
CTaxon1::GetGenus(int id_tax)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode && m_plCache->InitRanks() ) {
        int genus_rank(m_plCache->GetGenusRank());
        while( !pNode->IsRoot() ) {
            int rank( pNode->GetRank() );
            if( rank == genus_rank )
                return pNode->GetTaxId();
            if( (rank > 0) && (rank < genus_rank))
                return 0;
            pNode = pNode->GetParent();
        }
    }
    return -1;
}

//---------------------------------------------
// Get superkingdom tax_id (id_tax should be below superkingdom)
// Returns: tax_id of superkingdom or
//               0 - no superkingdom in the lineage
//              -1 - if error
///
int
CTaxon1::GetSuperkingdom(int id_tax)
{
    CTaxon1Node* pNode = 0;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }
    if( m_plCache->LookupAndAdd( id_tax, &pNode )
        && pNode && m_plCache->InitRanks() ) {
        int sk_rank(m_plCache->GetSuperkingdomRank());
        while( !pNode->IsRoot() ) {
            int rank( pNode->GetRank() );
            if( rank == sk_rank )
                return pNode->GetTaxId();
            if( (rank > 0) && (rank < sk_rank))
                return 0;
            pNode = pNode->GetParent();
        }
    }
    return -1;
}


//---------------------------------------------
// Get taxids for all children of specified node.
// Returns: number of children, id list appended with found tax ids
//          -1 - in case of error
///
int
CTaxon1::GetChildren(int id_tax, TTaxIdList& children_ids)
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

//---------------------------------------------
// Get taxonomic rank name by rank id
///
bool
CTaxon1::GetRankName(short rank_id, string& rank_name_out )
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
// Get taxonomic division name by division id
///
bool
CTaxon1::GetDivisionName(short div_id, string& div_name_out, string* div_code_out )
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
// Get taxonomic name class (scientific name, common name, etc.) by id
///
bool
CTaxon1::GetNameClass(short nameclass_id, string& name_class_out )
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
short
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
int
CTaxon1::Join(int taxid1, int taxid2)
{
    int tax_id = 0;
    CTaxon1Node *pNode1, *pNode2;
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
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
CTaxon1::GetAllNames(int tax_id, TNameList& lNames, bool unique)
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
                    lNames.push_back( ((*i)->IsSetUname() && !(*i)->GetUname().empty()) ?
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

//---------------------------------------------
// Get list of all names for tax_id.
// Clears the previous content of the list.
// Returns: TRUE - success
//          FALSE - failure
///
bool
CTaxon1::GetAllNamesEx(int tax_id, list< CRef< CTaxon1_name > >& lNames)
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

    req.SetGetorgnames( tax_id );

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
            SetLastError( "Response type is not Getorgnames" );
            return false;
        }
    }

    return true;
}

bool
CTaxon1::DumpNames( short name_class, list< CRef< CTaxon1_name > >& lOut )
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
            SetLastError( "Response type is not Dumpnames4class" );
            return false;
        }
    }

    return true;
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
CTaxon1::GetTaxId4GI(TGi gi, int& tax_id_out )
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
CTaxon1::LoadSubtreeEx( int tax_id, int levels, const ITaxon1Node** ppNode )
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
        req.SetTaxachildren( tax_id );

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
                                           + NStr::IntToString((*i)->GetTaxid())
                                           ).c_str() );
                            return false;
                        }
                    } else { // Add node to the partial tree
                        if( !m_plCache->Lookup((*i)->GetTaxid(), &pNode) ) {
                            pNode = new CTaxon1Node(*i);
                            m_plCache->SetIndexEntry(pNode->GetTaxId(), pNode);
                            pIt->AddChild( pNode );
                        }
                    }
                    pNode->SetSubtreeLoaded( pNode->IsSubtreeLoaded() ||
                                             (levels < 0) );
                }
                return true;
            } else { // Internal: wrong respond type
                SetLastError( "Response type is not Taxachildren" );
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
CTaxon1::GetTreeIterator( int tax_id, CTaxon1::EIteratorMode mode )
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
CTaxon1::GetNodeProperty( int tax_id, const string& prop_name,
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
        pProp->SetIval1( tax_id );
        pProp->SetIval2( -1 ); // Get string property by name
        pProp->SetSval( prop_name );

        req.SetGetorgprop( *pProp );
        try {
            if( SendRequest( req, resp ) ) {
                if( !resp.IsGetorgprop() ) { // error
                    ERR_POST_X( 4, "Response type is not Getorgprop" );
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
CTaxon1::GetNodeProperty( int tax_id, const string& prop_name,
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
        pProp->SetIval1( tax_id );
        pProp->SetIval2( -3 ); // Get bool property by name
        pProp->SetSval( prop_name );

        req.SetGetorgprop( *pProp );
        try {
            if( SendRequest( req, resp ) ) {
                if( !resp.IsGetorgprop() ) { // error
                    ERR_POST_X( 8, "Response type is not Getorgprop" );
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
CTaxon1::GetNodeProperty( int tax_id, const string& prop_name,
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
        pProp->SetIval1( tax_id );
        pProp->SetIval2( -2 ); // Get int property by name
        pProp->SetSval( prop_name );

        req.SetGetorgprop( *pProp );
        try {
            if( SendRequest( req, resp ) ) {
                if( !resp.IsGetorgprop() ) { // error
                    ERR_POST_X( 12, "Response type is not Getorgprop" );
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

bool
CTaxon1::CheckOrgRef( const COrg_ref& orgRef, TOrgRefStatus& stat_out )
{
    CDiagAutoPrefix( "Taxon1::CheckOrgRef" );
    SetLastError(NULL);
    int tax_id;

    tax_id = GetTaxIdByOrgRef( orgRef );
    stat_out = eStatus_Ok;

    if( tax_id == 0 ) {
        SetLastError( "No organism found for specified org_ref" );
        ERR_POST_X( 16, GetLastError() );
        return false;
    } else if( tax_id < 0 ) {
        SetLastError( "Multiple organisms found for specified org_ref" );
        ERR_POST_X( 17, GetLastError() );
        return false;
    } else {
        CRef< CTaxon2_data > pData( GetById( tax_id ) );
        if( pData ) {
            // Compare orgrefs
            const COrg_ref& goodOr = pData->GetOrg();

            if( !orgRef.IsSetOrgname() ) {
                stat_out |= eStatus_NoOrgname;
            } else {
                const COrgName& goodOn = goodOr.GetOrgname();
                const COrgName& inpOn = orgRef.GetOrgname();

                if( !inpOn.IsSetGcode() || !goodOn.IsSetGcode() ||
                    inpOn.GetGcode() != goodOn.GetGcode() ) {
                    stat_out |= eStatus_WrongGC;
                }
                if( !inpOn.IsSetMgcode() ) { // mgc not set in input
                    if( goodOn.IsSetMgcode() &&
                        goodOn.GetMgcode() != 0 ) {
                        stat_out |= eStatus_WrongMGC;
                    }
                } else { // mgc set
                    if( !goodOn.IsSetMgcode() ) {
                        if( inpOn.GetMgcode() != 0 ) { // not unassigned
                            stat_out |= eStatus_WrongMGC;
                        }
                    } else if( inpOn.GetMgcode() != goodOn.GetMgcode() ) {
                        stat_out |= eStatus_WrongMGC;
                    }
                }
                if( !inpOn.IsSetPgcode() ) { // pgc not set in input
                    if( goodOn.IsSetPgcode() &&
                        goodOn.GetPgcode() != 0 ) {
                        stat_out |= eStatus_WrongPGC;
                    }
                } else { // pgc set
                    if( !goodOn.IsSetPgcode() ) {
                        if( inpOn.GetPgcode() != 0 ) { // not unassigned
                            stat_out |= eStatus_WrongPGC;
                        }
                    } else if( inpOn.GetPgcode() != goodOn.GetPgcode() ) {
                        stat_out |= eStatus_WrongPGC;
                    }
                }
                if( !inpOn.IsSetLineage() || !goodOn.IsSetLineage() ||
                    inpOn.GetLineage().compare( goodOn.GetLineage() ) != 0 ) {
                    stat_out |= eStatus_WrongLineage;
                }
                if( !inpOn.IsSetName() || !goodOn.IsSetName() ||
                    inpOn.GetName().Which() != goodOn.GetName().Which() ) {
                    stat_out |= eStatus_WrongOrgname;
                }
                if( !inpOn.IsSetDiv() ) {
                    if( goodOn.IsSetDiv() &&
                        goodOn.GetDiv().compare( "UNA" ) != 0 ) {
                        stat_out |= eStatus_WrongDivision;
                    }
                } else {
                    if( !goodOn.IsSetDiv() ) {
                        if( inpOn.GetDiv().compare( "UNA" ) != 0 ) {
                            stat_out |= eStatus_WrongDivision;
                        }
                    } else if( inpOn.GetDiv().compare( goodOn.GetDiv() )
                               != 0 ) {
                        stat_out |= eStatus_WrongDivision;
                    }
                }
                if( goodOn.IsSetMod() ) {
                    if( inpOn.IsSetMod() ) {
                        const COrgName::TMod& inpMods = inpOn.GetMod();
                        const COrgName::TMod& goodMods = goodOn.GetMod();
                        for( COrgName::TMod::const_iterator gi =
                                 goodMods.begin();
                             gi != goodMods.end();
                             ++gi ) {
                            bool bFound = false;
                            for( COrgName::TMod::const_iterator ii =
                                     inpMods.begin();
                                 ii != inpMods.end();
                                 ++ii ) {
                                if( (*gi)->GetSubtype() == (*ii)->GetSubtype()
                                    && ((*gi)->GetSubname() ==
                                        (*ii)->GetSubname()) ) {
                                    bFound = true;
                                    break;
                                }
                            }
                            if( !bFound ) {
                                stat_out |= eStatus_WrongOrgmod;
                                break;
                            }
                        }
                    } else {
                        stat_out |= eStatus_WrongOrgmod;
                    }
                }
            }
            // Check taxname
            if( orgRef.IsSetTaxname() ) {
                if( !goodOr.IsSetTaxname() ||
                    orgRef.GetTaxname().compare( goodOr.GetTaxname() ) != 0 ) {
                    stat_out |= eStatus_WrongTaxname;
                }
            } else if( goodOr.IsSetTaxname() ) {
                stat_out |= eStatus_WrongTaxname;
            }
            // Check common name
            if( orgRef.IsSetCommon() ) {
                if( !goodOr.IsSetCommon() ||
                    orgRef.GetCommon().compare( goodOr.GetCommon() ) != 0 ) {
                    stat_out |= eStatus_WrongCommonName;
                }
            } else if( goodOr.IsSetCommon() ) {
                stat_out |= eStatus_WrongCommonName;
            }
        } else { // Internal error: Cannot find orgref by tax_id
            SetLastError( "No organisms found for tax id" );
            ERR_POST_X( 18, GetLastError() );
            return false;
        }
    }
    return true;
}

//---------------------------------------------------
// This function returns the list of "type materials" for the node with taxid given.
// The list consists of names with class type material found at the species
// or subspecies ancestor of the node. 
// Returns: true  when success and last parameter is filled with type material list,
//          false when call failed
///
bool
CTaxon1::GetTypeMaterial( int tax_id, TNameList& type_material_list_out )
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
        && pNode ) {
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
	short tm_cde = GetNameClassId( "type material" );
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
int 
CTaxon1::GetMaxTaxId( void )
{
    SetLastError(NULL);
    if( !TAXON1_IS_INITED ) {
	if( !Init() ) { 
	    return -1;
	}
    }
    return m_plCache->m_nMaxTaxId;
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
CTaxon1::GetDisplayCommonName( int tax_id, string& disp_name_out )
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
	short cn = m_plCache->GetPreferredCommonNameClass();
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
	    int species_id = GetSpecies(tax_id);
	    if( species_id < 0 ) {
		return false;
	    } else if( species_id > 0 && species_id != tax_id ) {
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

END_objects_SCOPE
END_NCBI_SCOPE
