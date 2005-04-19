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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const bool   TaxClient::REFRESH_DEFAULT    = false;

TaxClient::TaxClient(bool refresh) : m_taxonomyClient(0)
{
}


TaxClient::~TaxClient() {
	if(m_taxonomyClient){ 
		m_taxonomyClient->Fini();
		delete m_taxonomyClient;
		m_taxonomyClient = 0;
	}	
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
int TaxClient::GetTaxIDForSeqId(const CRef< CSeq_id >& sid)
{

	int gi = 0;
    int taxid = 0;
    std::string err = "no gi or source info";

	//  Check if have a valid, live tax server
    if (sid->IsGi()) {
        gi = sid->GetGi();
        taxid = GetTaxIDForGI(gi);
    }
    return taxid;
}


int TaxClient::GetTaxIDForGI(int gi) {
    int taxid = 0;
    if (IsAlive()) {
        return (m_taxonomyClient->GetTaxId4GI(gi, taxid)) ? taxid : 0;
    }
    return taxid;
}

//  Look through the bioseq for a COrg object, and use it to get taxid.
//  Use tax server by default, unless lookInBioseq is true.
int TaxClient::GetTaxIDFromBioseq(const CBioseq& bioseq, bool lookInBioseq) {

    int taxid = 0;
	list< CRef< CSeqdesc > >::const_iterator  j, jend;

    if (bioseq.IsSetDescr()) 
	{
        jend = bioseq.GetDescr().Get().end();

        // look through the sequence descriptions
        for (j=bioseq.GetDescr().Get().begin(); j!=jend; j++) 
		{
			const COrg_ref *org = NULL;
			if ((*j)->IsOrg())
				org = &((*j)->GetOrg());
			else if ((*j)->IsSource())
				org = &((*j)->GetSource().GetOrg());
			if (org) 
			{
                //  Use tax server
				if (IsAlive()) 
				{
						if ((taxid = m_taxonomyClient->GetTaxIdByOrgRef(*org)) != 0) 
						{
							if (taxid < 0) 
							{  //  multiple tax nodes; -taxid is one of them
								taxid *= -1;
							}
							break;
						}
				}
                //  Use bioseq, which may be obsolete, only when requested and tax server failed.
                if (taxid == 0 && lookInBioseq) 
				{   //  is there an ID in the bioseq itself if fails
                	vector < CRef< CDbtag > >::const_iterator k, kend = org->GetDb().end();
                    for (k=org->GetDb().begin(); k != kend; ++k) {
						if ((*k)->GetDb() == "taxon") {
							if ((*k)->GetTag().IsId()) {
								taxid = (*k)->GetTag().GetId();
        						break;
							}
						}
					}
                }
			} //end if (org)
		}//end for
	}
    return taxid;
}

short TaxClient::GetRankID(int taxId, string& rankName)
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
std::string TaxClient::GetTaxNameForTaxID(int taxid)
{
	std::string taxName = kEmptyStr;
	if (taxid <= 0)
		return taxName;
	if (taxid == 1)
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
int TaxClient::GetParentTaxID(int taxid)
{
	int parent;
	if (IsAlive())
		parent = m_taxonomyClient->GetParent(taxid);
    return parent;
}

string TaxClient::GetSuperKingdom(int taxid) {

    int skId = m_taxonomyClient->GetSuperkingdom(taxid);
    return (skId == -1) ? kEmptyStr : GetTaxNameForTaxID(skId);
}


//is taxid2 the descendant of taxid1?
bool TaxClient::IsTaxDescendant(int taxid1, int taxid2)
{
	int ancestor = m_taxonomyClient->Join(taxid1, taxid2);
	return ancestor == taxid1;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
