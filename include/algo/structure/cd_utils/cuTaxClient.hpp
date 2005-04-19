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
#include <math.h>
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class TaxClient
{

    static const bool   REFRESH_DEFAULT;

public: 

    TaxClient(bool refresh = REFRESH_DEFAULT);
    ~TaxClient();

	bool init();
    //  return true is the CTaxon1 object is alive
    bool IsAlive();

    //  return 0 if Get*TaxID*(...) fails
    int GetTaxIDForSeqId(const CRef< CSeq_id >& sid);  // was GetTaxIDForSequence
    int GetTaxIDForGI(int gi);

	short GetRankID(int taxId, string& rankName);

    //  Look through the bioseq for a COrg object, and use it to get taxid.
    //  Use tax server by default, unless lookInBioseq is true.
    int GetTaxIDFromBioseq(const CBioseq& bioseq, bool lookInBioseq);
    string GetTaxNameForTaxID(int taxid);
    string GetSuperKingdom(int taxid);

    int GetParentTaxID(int taxid);
	//is tax2 the descendant of tax1?
    bool IsTaxDescendant(int tax1, int tax2);

    bool ConnectToTaxServer();  

private:

    CTaxon1 * m_taxonomyClient;

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif 

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
