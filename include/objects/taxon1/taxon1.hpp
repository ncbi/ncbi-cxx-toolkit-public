#ifndef NCBI_TAXON1_HPP
#define NCBI_TAXON1_HPP

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
 *     NCBI Taxonomy information retreival library
 *
 */


#include <objects/taxon1/taxon1__.hpp>
#include <serial/serialdef.hpp>
#include <connect/ncbi_types.h>

#include <list>
#include <vector>
#include <map>


BEGIN_NCBI_SCOPE

class CObjectOStream;
class CConn_ServiceStream;


BEGIN_objects_SCOPE

class COrgRefCache;


class CTaxon1 {
public:
    typedef list< string > TNameList;
    typedef vector< int > TTaxIdList;

    CTaxon1();
    ~CTaxon1();

    //---------------------------------------------
    // Taxon1 server init
    // Returns: TRUE - OK
    //          FALSE - Can't open connection to taxonomy service
    ///
    bool Init(void);  // default:  120 sec timeout, 5 reconnect attempts
    bool Init(const STimeout* timeout=0, unsigned reconnect_attempts=5);

    //---------------------------------------------
    // Taxon1 server fini (closes connection, frees memory)
    ///
    void Fini(void);

    //---------------------------------------------
    // Get organism by tax_id
    // Returns: pointer to Taxon2Data if organism exists
    //          NULL - if tax_id wrong
    //
    // NOTE:
    // Caller gets his own copy of Taxon2Data structure.
    ///
    CRef< CTaxon2_data > GetById(int tax_id);

    //----------------------------------------------
    // Get organism by OrgRef
    // Returns: pointer to Taxon2Data if organism exists
    //          NULL - if no such organism in taxonomy database
    //
    // NOTE:
    // 1. These functions uses the following data from inp_orgRef to find
    //    organism in taxonomy database. It uses taxname first. If no organism
    //    was found (or multiple nodes found) then it tryes to find organism
    //    using common name. If nothing found, then it tryes to find organism
    //    using synonyms. Lookup never uses tax_id to find organism.
    // 2. LookupMerge function modifies given OrgRef to correspond to the
    //    found one and returns constant pointer to the Taxon2Data structure
    //    stored internally.
    ///
    CRef< CTaxon2_data > Lookup(const COrg_ref& inp_orgRef);
    CConstRef< CTaxon2_data > LookupMerge(COrg_ref& inp_orgRef);

    //-----------------------------------------------
    // Get tax_id by OrgRef
    // Returns: tax_id - if organism found
    //               0 - no organism found
    //         -tax_id - if multiple nodes found
    //                   (where -tax_id is id of one of the nodes)
    // NOTE:
    // This function uses the same information from inp_orgRef as Lookup
    ///
    int GetTaxIdByOrgRef(const COrg_ref& inp_orgRef);

    //----------------------------------------------
    // Get tax_id by organism name
    // Returns: tax_id - if organism found
    //               0 - no organism found
    //         -tax_id - if multiple nodes found
    //                   (where -tax_id is id of one of the nodes)
    ///
    int GetTaxIdByName(const string& orgname);
    int FindTaxIdByName(const string& orgname);


    //----------------------------------------------
    // Get ALL tax_id by organism name
    // Returns: number of organisms found, id list appended with found tax ids
    ///
    int GetAllTaxIdByName(const string& orgname, TTaxIdList& lIds);

    //----------------------------------------------
    // Get organism by tax_id
    // Returns: pointer to OrgRef structure if organism found
    //          NULL - if no such organism in taxonomy database
    // NOTE:
    // This function does not make a copy of OrgRef structure but returns
    // pointer to internally stored OrgRef.
    ///
    CConstRef< COrg_ref > GetOrgRef(int tax_id,
				    bool& is_species,
				    bool& is_uncultured,
				    string& blast_name);

    //---------------------------------------------
    // Set mode for synonyms in OrgRef
    // Returns: previous mode
    // NOTE:
    // Default value: false (do not copy synonyms to the new OrgRef)
    ///
    bool SetSynonyms(bool on_off);

    //---------------------------------------------
    // Get parent tax_id
    // Returns: tax_id of parent node or 0 if error
    // NOTE:
    //   Root of the tree has tax_id of 1
    ///
    int GetParent(int id_tax);

    //---------------------------------------------
    // Get genus tax_id (id_tax should be below genus)
    // Returns: tax_id of genus or -1 if error or no genus in the lineage
    ///
    int GetGenus(int id_tax);

    //---------------------------------------------
    // Get taxids for all children of specified node.
    // Returns: number of children, id list appended with found tax ids
    ///
    int GetChildren(int id_tax, TTaxIdList& children_ids);

    //---------------------------------------------
    // Get genetic code name by genetic code id
    ///
    bool GetGCName(short gc_id, string& gc_name_out );

    //---------------------------------------------
    // Get the nearest common ancestor for two nodes
    // Returns: id of this ancestor (id == 1 means that root node only is
    // ancestor)
    int Join(int taxid1, int taxid2);

    //---------------------------------------------
    // Get all names for tax_id
    // Returns: number of names, name list appended with ogranism's names
    // NOTE:
    // If unique is true then only unique names will be stored
    ///
    int GetAllNames(int tax_id, TNameList& lNames, bool unique);

    //---------------------------------------------
    // Find out is taxonomy lookup system alive or not
    // Returns: TRUE - alive
    //          FALSE - dead
    ///

    bool IsAlive(void);

    //--------------------------------------------------
    // Get tax_id for given gi
    // Returns:
    //       true   if ok
    //       false  if error
    // tax_id_out contains:
    //       tax_id if found
    //       0      if not found
    ///
    bool GetTaxId4GI(int gi, int& tax_id_out);

    //--------------------------------------------------
    // Get "blast" name for id
    // Returns: false if some error (blast_name_out not changed)
    //          true  if Ok
    //                blast_name_out contains first blast name at or above
    //                this node in the lineage or empty if there is no blast
    //                name above
    ///
    bool GetBlastName(int tax_id, string& blast_name_out);

    //--------------------------------------------------
    // Get error message after latest erroneous operation
    // Returns: error message, or empty string if no error occured
    ///
    const string& GetLastError() const { return m_sLastError; }

private:
    friend class COrgRefCache;

    ESerialDataFormat        m_eDataFormat;
    const char*              m_pchService;
    STimeout*                m_timeout;  // NULL, or points to "m_timeout_value"
    STimeout                 m_timeout_value;

    CConn_ServiceStream*     m_pServer;

    CObjectOStream*          m_pOut;
    CObjectIStream*          m_pIn;

    unsigned                 m_nReconnectAttempts;

    COrgRefCache*            m_plCache;

    bool                     m_bWithSynonyms;
    string                   m_sLastError;

    typedef map<short, string> TGCMap;
    TGCMap                   m_gcStorage;

    void             Reset(void);
    bool             SendRequest(CTaxon1_req& req, CTaxon1_resp& resp);
    void             SetLastError(const char* err_msg);
};


END_objects_SCOPE
END_NCBI_SCOPE



//
// $Log$
// Revision 1.3  2002/02/14 22:44:48  vakatov
// Use STimeout instead of time_t.
// Get rid of warnings and extraneous #include's, shuffled code a little.
//
// Revision 1.2  2002/01/31 00:30:11  vakatov
// Get rid of "std::" which is unnecessary and sometimes un-compilable.
// Also done some source identation/beautification.
//
// Revision 1.1  2002/01/28 19:52:20  domrach
// Initial checkin
//
//
// ===========================================================================
//

#endif //NCBI_TAXON1_HPP
