#ifndef NCBI_ITAXON3_HPP
#define NCBI_ITAXON3_HPP

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
 * Author:  Colleen Bollin, based on work by Vladimir Soussov, Michael Domrachev
 *          Brad Holmes : Added ITaxon interface, to support mocking the service.
 *
 * File Description:
 *     NCBI Taxonomy information retreival library
 *
 */


#include <objects/seqfeat/Org_ref.hpp>
#include <objects/taxon3/Taxon3_request.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>
#include <serial/serialdef.hpp>
#include <connect/ncbi_types.h>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbimisc.hpp>
#include <objects/seqfeat/seqfeat__.hpp>

#include <list>
#include <vector>
#include <map>


BEGIN_NCBI_SCOPE

class CObjectOStream;
class CConn_ServiceStream;


BEGIN_objects_SCOPE

class NCBI_TAXON3_EXPORT ITaxon3 {
public:

    virtual ~ITaxon3(){};

    //---------------------------------------------
    // Taxon1 server init
    // Returns: TRUE - OK
    //          FALSE - Can't open connection to taxonomy service
    virtual void Init(void) = 0;  
                     
    virtual void Init(const STimeout* timeout, unsigned reconnect_attempts=5) = 0;

    enum ETaxon3_reply_part {
	eT3reply_nothing    = 0,
	eT3reply_org        = 0x0001, // data.org
	eT3reply_blast_lin  = 0x0002, // data.blast_name_lineage
	eT3reply_status     = 0x0004, // data.status
	eT3reply_refresh    = 0x0008, // data.refresh

	eT3reply_all        = (eT3reply_org | eT3reply_blast_lin | eT3reply_status | eT3reply_refresh),
	eT3reply_default    = eT3reply_all
    };
    typedef unsigned int fT3reply_parts;

    // submit a list of org_refs
    virtual CRef< CTaxon3_reply > SendOrgRefList(const vector< CRef< COrg_ref > >& list,
						 COrg_ref::fOrgref_parts result_parts = COrg_ref::eOrgref_default,
						 fT3reply_parts t3result_parts = eT3reply_default ) = 0;

    // Name list lookup all the names (including common names) regardless of their lookup flag
    // By default returns scientific name and taxid in T3Data.org and old_name_class property with matched class name
    virtual CRef< CTaxon3_reply > SendNameList(const vector<std::string>& list,
					       COrg_ref::fOrgref_parts parts = (COrg_ref::eOrgref_taxname |
										COrg_ref::eOrgref_db_taxid),
					       fT3reply_parts t3parts = (eT3reply_org|eT3reply_status) ) = 0;
    // By default returns only scientific name and taxid in T3Data.org
    virtual CRef< CTaxon3_reply > SendTaxidList(const vector<TTaxId>& list,
						COrg_ref::fOrgref_parts parts = (COrg_ref::eOrgref_taxname |
										 COrg_ref::eOrgref_db_taxid),
						fT3reply_parts t3parts = eT3reply_org) = 0;

    virtual CRef< CTaxon3_reply > SendRequest(const CTaxon3_request& request) = 0;
    //--------------------------------------------------
    // Get error message after latest erroneous operation
    // Returns: error message, or empty string if no error occurred
    ///
    virtual const string& GetLastError() const = 0;

};


END_objects_SCOPE
END_NCBI_SCOPE

#endif //NCBI_ITAXON1_HPP
