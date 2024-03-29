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

#ifndef CU_TAXCLIENT_HPP
#define CU_TAXCLIENT_HPP

#include <objects/taxon1/taxon1.hpp>
#include <objects/taxon1/Taxon2_data.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <math.h>
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT TaxClient
{

    static const bool   REFRESH_DEFAULT;

public: 

    TaxClient(bool refresh = REFRESH_DEFAULT);
    virtual ~TaxClient();

	virtual bool init();
    //  return true is the CTaxon1 object has been created and is alive
    virtual bool IsAlive();

    //  return 0 if Get*TaxID*(...) fails
    virtual TTaxId GetTaxIDForSeqId(CConstRef< CSeq_id > sid);  // was GetTaxIDForSequence
    virtual TTaxId GetTaxIDForGI(TGi gi);

    virtual bool GetOrgRef(TTaxId taxId, COrg_ref& orgRef);
    virtual bool GetOrgRef(TTaxId taxId, CRef< COrg_ref >& orgRef);

    // Set mode for synonyms in OrgRef
    // Returns: previous mode
    // Default value: false (do not copy synonyms to the new OrgRef)
    virtual bool SetSynonyms(bool on_off);

	virtual short GetRankID(TTaxId taxId, string& rankName);

    //  Look through the bioseq for a COrg object, and use it to get taxid.
    //  Use tax server by default, unless server fails and lookInBioseq is true.
    virtual TTaxId GetTaxIDFromBioseq(const CBioseq& bioseq, bool lookInBioseq);
    virtual string GetTaxNameForTaxID(TTaxId taxid);
    virtual string GetSuperKingdom(TTaxId taxid);

    //  Returns true on success, false in case of error.
    virtual bool GetDisplayCommonName(TTaxId taxid, string& displayCommonName);

    //  Returns zero if the client is not alive (i.e., IsAlive() returns false).
    virtual TTaxId GetParentTaxID(TTaxId taxid);
	//is tax2 the descendant of tax1?
    virtual bool IsTaxDescendant(TTaxId tax1, TTaxId tax2);

    //  On success, lineageFromRoot contains the path in the taxonomy from the root to taxid.
    //  Returns true on success (i.e., a non-empty lineage), false in case of error.
    virtual bool GetFullLineage(TTaxId taxid, vector<TTaxId>& lineageFromRoot);

    //  On success, lineageFromRoot contains (taxid, taxname) for each step along the path 
    //  in the taxonomy from the root to provided taxid.  An empty string is used should any
    //  individual taxname not be found.
    //  When 'useCommonName' is true, the taxname is the 'common name'; otherwise, it is
    //  the preferred formal name found in the corresponding COrg_ref for the taxid (and
    //  shown in lineages on the Entrez taxonomy pages).
    //  Returns true on success (i.e., a non-empty lineage), false in case of error.
    virtual bool GetFullLineage(TTaxId taxid, vector< pair<TTaxId, string> >& lineageFromRoot, bool useCommonName = false);

    virtual bool ConnectToTaxServer();  

    //  Wrapper functions to invoke CTaxon1 methods.
    CConstRef< COrg_ref > GetOrgRef(TTaxId tax_id, bool& is_species, bool& is_uncultured, string& blast_name);
    TTaxId Join(TTaxId taxid1, TTaxId taxid2);

private:

    CTaxon1 * m_taxonomyClient;
    CScope * m_scope;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
