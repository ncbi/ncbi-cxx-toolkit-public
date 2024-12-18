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
 * Author:  Adapted from CDTree-1 files by Chris Lanczycki
 *
 * File Description:
 *
 *       Various utilities and classes for obtaining taxonomy information
 *       from ASN objects and NCBI taxonomy services.
 *       Also maintain lists of preferred and model tax nodes.
 *
 * ===========================================================================
 */


#include <ncbi_pch.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/taxon1/Taxon2_data.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <math.h>

#include <algo/structure/cd_utils/cuTaxClient.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const bool   TaxClient::REFRESH_DEFAULT               = false;

TaxClient::TaxClient(bool refresh) : m_taxonomyClient(0), m_scope(0)
{
}


TaxClient::~TaxClient() {
	if(m_taxonomyClient){ 
		m_taxonomyClient->Fini();
		delete m_taxonomyClient;
		m_taxonomyClient = 0;
	}
	if (m_scope)
		delete m_scope;
}

bool TaxClient::init()
{
	return ConnectToTaxServer();
}

bool TaxClient::ConnectToTaxServer()
{
	if(!m_taxonomyClient)
		m_taxonomyClient= new CTaxon1();
	m_taxonomyClient->Init();
	
	if (m_taxonomyClient->IsAlive()) 
		return true;
    else
		return false;
}


bool TaxClient::IsAlive() {
    if(!m_taxonomyClient)return false;
    return m_taxonomyClient->IsAlive();
}

// try to get "official" tax info from seq_id's gi
TTaxId TaxClient::GetTaxIDForSeqId(CConstRef< CSeq_id > sid)
{
	TGi gi = ZERO_GI;
    if (sid.Empty()) return ZERO_TAX_ID;
    
    if (sid->IsGi()) 
	{
        gi = sid->GetGi();  
    }
	else
	{
        //  Lazy-create OM/Scope only when needed.
        if (!m_scope) {
            CRef<CObjectManager> objMgr = CObjectManager::GetInstance();
            CGBDataLoader::RegisterInObjectManager(*objMgr); //  GenBank data loader

            m_scope = new CScope(*objMgr);
            if (m_scope)
                m_scope->AddDefaults();
            else
                return ZERO_TAX_ID;
        }
        
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*sid);
        vector<CSeq_id_Handle> ids = m_scope->GetIds(idh);
        ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
            if (it->GetSeqId()->IsGi()) {
                gi = it->GetSeqId()->GetGi();
                break;
            }
        }        
	}
	return GetTaxIDForGI(gi);
}


TTaxId TaxClient::GetTaxIDForGI(TGi gi) {
    TTaxId taxid = ZERO_TAX_ID;
    if (IsAlive()) {
        return (m_taxonomyClient->GetTaxId4GI(gi, taxid)) ? taxid : ZERO_TAX_ID;
    }
    return taxid;
}

bool TaxClient::GetOrgRef(TTaxId taxId, COrg_ref& orgRef) {

    bool result = false;

    if (IsAlive() && taxId > ZERO_TAX_ID) {
        bool is_species, is_uncultured;
        string blast_name;
        CConstRef< COrg_ref > constOrgRef = m_taxonomyClient->GetOrgRef(taxId, is_species, is_uncultured, blast_name);
        orgRef.Assign(*constOrgRef);
        result = true;
    } else {
        orgRef.Reset();
    }

    return result;
}

bool TaxClient::GetOrgRef(TTaxId taxId, CRef< COrg_ref >& orgRef) {

    if (orgRef.NotEmpty()) {
        return GetOrgRef(taxId, *orgRef);
    } else {
        return false;
    }
}

bool TaxClient::SetSynonyms(bool on_off){
    return m_taxonomyClient->SetSynonyms(on_off);
}

//  Look through the bioseq for a COrg object, and use it to get taxid.
//  Use tax server by default, unless server fails and lookInBioseq is true.
TTaxId TaxClient::GetTaxIDFromBioseq(const CBioseq& bioseq, bool lookInBioseq) {

    TTaxId	taxid =	ZERO_TAX_ID;
	list< CRef<	CSeqdesc > >::const_iterator  j, jend;

	if (bioseq.IsSetDescr()) 
	{
		jend = bioseq.GetDescr().Get().end();

		// look	through	the	sequence descriptions
		for	(j=bioseq.GetDescr().Get().begin();	j!=jend; j++) 
		{
			const COrg_ref *org	= NULL;
			if ((*j)->IsOrg())
				org	= &((*j)->GetOrg());
			else if	((*j)->IsSource())
				org	= &((*j)->GetSource().GetOrg());
			if (org) 
			{
				//	Use	tax	server
				if (IsAlive()) 
				{
						if ((taxid = m_taxonomyClient->GetTaxIdByOrgRef(*org)) != ZERO_TAX_ID)
						{
							if (taxid <	ZERO_TAX_ID)
							{  //  multiple	tax	nodes; -taxid is one of	them
								taxid = -taxid;
							}
							break;
						}
				}
				//	Use	bioseq,	which may be obsolete, only	when requested and tax server failed.
				if (taxid == ZERO_TAX_ID && lookInBioseq)
				{	//	is there an	ID in the bioseq itself	if fails
					vector < CRef< CDbtag >	>::const_iterator k, kend =	org->GetDb().end();
					for	(k=org->GetDb().begin(); k != kend;	++k) {
						if ((*k)->GetDb() == "taxon") {
							if ((*k)->GetTag().IsId()) {
								taxid = TAX_ID_FROM(CObject_id::TId, (*k)->GetTag().GetId());
								break;
							}
						}
					}
				}
			} //end	if (org)
		}//end for
	}
	return taxid;
}

short TaxClient::GetRankID(TTaxId taxId, string& rankName)
{
	short rankId = -1;
	if(IsAlive())
	{
		CRef< ITreeIterator > nodeIt = m_taxonomyClient->GetTreeIterator( taxId );
		rankId = nodeIt->GetNode()->GetRank();
		m_taxonomyClient->GetRankName(rankId, rankName);
	}

	return rankId;
}
    // get info for taxid
std::string TaxClient::GetTaxNameForTaxID(TTaxId taxid)
{
	std::string taxName = kEmptyStr;
	if (taxid <= ZERO_TAX_ID)
		return taxName;
	if (taxid == TAX_ID_CONST(1))
	{
		taxName = "Root";
		return taxName;
	}
	if (IsAlive()) 
	{
		CRef < CTaxon2_data > data = m_taxonomyClient->GetById(taxid);
		if (data->IsSetOrg() && data->GetOrg().IsSetTaxname()) {
			taxName = data->GetOrg().GetTaxname();
		}
	}
	
	return taxName;
}

    // get parent for taxid
TTaxId TaxClient::GetParentTaxID(TTaxId taxid)
{
    TTaxId parent = ZERO_TAX_ID;
	if (IsAlive())
		parent = m_taxonomyClient->GetParent(taxid);
    return parent;
}

string TaxClient::GetSuperKingdom(TTaxId taxid) {

    TTaxId skId = m_taxonomyClient->GetSuperkingdom(taxid);
    return (skId == INVALID_TAX_ID) ? kEmptyStr : GetTaxNameForTaxID(skId);
}

bool TaxClient::GetDisplayCommonName(TTaxId taxid, string& displayCommonName)
{
    if (IsAlive()) {
        return m_taxonomyClient->GetDisplayCommonName(taxid, displayCommonName);
    } else {
        return false;
    }
}

//is taxid2 the descendant of taxid1?
bool TaxClient::IsTaxDescendant(TTaxId taxid1, TTaxId taxid2)
{   
    if (IsAlive()) {
        TTaxId ancestor = m_taxonomyClient->Join(taxid1, taxid2);
        return (ancestor == taxid1); 
    } else 
        return false;
}

CConstRef< COrg_ref > TaxClient::GetOrgRef(TTaxId tax_id, bool& is_species, bool& is_uncultured, string& blast_name)
{
    if (IsAlive()) {
        return m_taxonomyClient->GetOrgRef(tax_id, is_species, is_uncultured, blast_name);
    } else {
        //  following the behavior of CTaxon1::GetOrgRef on failure.
        return ncbi::null;
    }
}

TTaxId TaxClient::Join(TTaxId taxid1, TTaxId taxid2)
{
    if (IsAlive()) {
        return m_taxonomyClient->Join(taxid1, taxid2);
    } else {
        return ZERO_TAX_ID;
    }
}

bool TaxClient::GetFullLineage(TTaxId taxid, vector<TTaxId>& lineageFromRoot)
{
    TTaxId rootTaxid = TAX_ID_CONST(1);
    vector<TTaxId> v;
    v.push_back(rootTaxid);
    v.push_back(taxid);
    lineageFromRoot.clear();

    if (IsAlive() && m_taxonomyClient->GetPopsetJoin(v, lineageFromRoot) && lineageFromRoot.size() > 0) {
        lineageFromRoot.push_back(taxid);  //  GetPopsetJoin does not put taxid at the end of the lineage
        return true;
    }
    return false;
}

bool TaxClient::GetFullLineage(TTaxId taxid, vector< pair<TTaxId, string> >& lineageFromRoot, bool useCommonName)
{
    string commonName;
    string root("root");
    vector<TTaxId> lineageAsTaxids;
    pair<TTaxId, string> p;

    lineageFromRoot.clear();
    if (GetFullLineage(taxid, lineageAsTaxids)) {
        ITERATE(vector<TTaxId>, vit, lineageAsTaxids) {
            p.first = *vit;
            if (!useCommonName) {  // formal name via the COrg_ref
                commonName = GetTaxNameForTaxID(*vit);
                if (commonName.length() > 0) {
                    p.second = (p.first > TAX_ID_CONST(1)) ? commonName : root;
                } else {
                    p.second = kEmptyStr;
                }
            } else if (useCommonName && GetDisplayCommonName(*vit, commonName) && commonName.length() > 0) {
                p.second = (p.first > TAX_ID_CONST(1)) ? commonName : root;
            } else {
                p.second = kEmptyStr;
            }
            lineageFromRoot.push_back(p);
        }
        return true;
    }
    return false;
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
