#ifndef NCBI_TAXON3_HPP
#define NCBI_TAXON3_HPP

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
 *
 * File Description:
 *     NCBI Taxonomy information retreival library
 *
 */


#include <objects/taxon3/itaxon3.hpp>
#include <objects/taxon3/taxon3__.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <serial/serialdef.hpp>
#include <connect/ncbi_types.h>
#include <corelib/ncbi_limits.hpp>

#include <list>
#include <vector>
#include <map>


BEGIN_NCBI_SCOPE

class CObjectOStream;
class CConn_ServiceStream;


BEGIN_objects_SCOPE

class NCBI_TAXON3_EXPORT CTaxon3 : public ITaxon3 {
public:

    CTaxon3();
    virtual ~CTaxon3();

    //---------------------------------------------
    // Taxon1 server init
    // Returns: TRUE - OK
    //          FALSE - Can't open connection to taxonomy service
    ///
    virtual void                  Init(void);  // default:  20 sec timeout, 5 reconnect attempts,
                     
    virtual void                  Init(const STimeout* timeout, unsigned reconnect_attempts=5);

    // submit a list of org_refs
    virtual CRef< CTaxon3_reply > SendOrgRefList(const vector< CRef< COrg_ref > >& list,
						 COrg_ref::fOrgref_parts result_parts = COrg_ref::eOrgref_default,
						 fT3reply_parts t3result_parts = eT3reply_default);

    // Name list lookup all the names (including common names) regardless of their lookup flag
    virtual CRef< CTaxon3_reply > SendNameList(const vector<std::string>& list,
					       COrg_ref::fOrgref_parts parts = (COrg_ref::eOrgref_taxname |
										COrg_ref::eOrgref_db_taxid),
					       fT3reply_parts t3parts = (eT3reply_org|eT3reply_status));

    virtual CRef< CTaxon3_reply > SendTaxidList(const vector<TTaxId>& list,
						COrg_ref::fOrgref_parts parts = (COrg_ref::eOrgref_taxname |
										 COrg_ref::eOrgref_db_taxid),
						fT3reply_parts t3parts = eT3reply_org);

    virtual CRef< CTaxon3_reply > SendRequest(const CTaxon3_request& request);

    //--------------------------------------------------
    // Get error message after latest erroneous operation
    // Returns: error message, or empty string if no error occurred
    ///
    virtual const string&         GetLastError() const { return m_sLastError; }

private:

    ESerialDataFormat             m_eDataFormat;
    string                        m_sService;
    STimeout*                     m_timeout;  // NULL, or points to "m_timeout_value"
    STimeout                      m_timeout_value;

    unsigned                      m_nReconnectAttempts;

    string                        m_sLastError;

    void                          SetLastError(const char* err_msg);
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif //NCBI_TAXON1_HPP
