/*  $Id$
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
 *  Government have not placed any restriction on its use or requeryion.
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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *  Class for getting Taxonomy data from local SQLite file
 */

#ifndef LOCAL_TAXON_HPP
#define LOCAL_TAXON_HPP

#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
class CSQLITE_Connection;

BEGIN_SCOPE(objects)
class COrg_ref;
class CTaxon1;
END_SCOPE(objects)


class CLocalTaxon
{
public:
    typedef TTaxId TTaxid;
    typedef vector<TTaxid> TLineage;

    static void AddArguments(CArgDescriptions& arg_desc);

    CLocalTaxon();
    CLocalTaxon(const CArgs &args);

    ~CLocalTaxon();

    bool IsValidTaxid(TTaxid taxid);

    TTaxid GetParent(TTaxid taxid);
    string GetRank(TTaxid taxid);
    TTaxid GetAncestorByRank(TTaxid taxid, const string &rank);

    TTaxid GetTaxIdByOrgRef(const objects::COrg_ref &inp_orgRef);

    TTaxid GetSpecies(TTaxid taxid)
    { return GetAncestorByRank(taxid, "species"); }
    TTaxid GetGenus(TTaxid taxid)
    { return GetAncestorByRank(taxid, "genus"); }
    TTaxid GetOrder(TTaxid taxid)
    { return GetAncestorByRank(taxid, "order"); }

    TLineage GetLineage(TTaxid taxid);
    TTaxid Join(TTaxid taxid1, TTaxid taxid2);

    string GetScientificName(TTaxid taxid);
    short int GetGeneticCode(TTaxid taxid);
    CConstRef<objects::COrg_ref> GetOrgRef(TTaxid taxid);
    void LookupMerge(objects::COrg_ref& org);

public: // lookups other than taxid

    TTaxid GetTaxIdByName(const string& orgname);

private: // data types
    struct STaxidNode;
    typedef map<TTaxId, STaxidNode> TNodes;
    typedef map<string, STaxidNode> TScientificNameIndex;
    typedef TNodes::const_iterator TNodeRef;
    typedef TScientificNameIndex::const_iterator TScientificNameRef;
    typedef list<TNodeRef> TInternalLineage;

    struct STaxidNode {
        TTaxid taxid;
        bool is_valid;
        string scientific_name;
        string rank;
        TNodeRef parent;
        short int genetic_code;
        CConstRef<objects::COrg_ref> org_ref;

        STaxidNode();
        ~STaxidNode();
    };
private: // static 
    static TNodes s_DummyNodes;
    static TNodeRef s_InvalidNode;
    
private: // data model
    bool m_db_supports_synonym;
    unique_ptr<CSQLITE_Connection> m_SqliteConn;
    unique_ptr<objects::CTaxon1> m_TaxonConn;
    TNodes m_Nodes;
    TScientificNameIndex m_ScientificNameIndex;
private: // implementation 
    TNodeRef x_Cache(TTaxid taxid, bool including_org_ref = false);
    TScientificNameRef x_Cache(const string& orgname);
    void x_GetLineage(TTaxid taxid, TInternalLineage &lineage);
    bool x_SupportsSynonym();
};


END_NCBI_SCOPE


#endif /// LOCAL_TAXON_HPP
