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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *  Class for getting Taxonomy data from local SQLite file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/taxon1/local_taxon.hpp>

#include <objects/seqfeat/Org_ref.hpp>

#include <objects/taxon1/taxon1.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <db/sqlite/sqlitewrapp.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CLocalTaxon::TNodes CLocalTaxon::s_DummyNodes;
CLocalTaxon::TNodeRef CLocalTaxon::s_InvalidNode = CLocalTaxon::s_DummyNodes.end();

void CLocalTaxon::AddArguments(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey("taxon-db", "TaxonDBFile",
                            "SQLite file containing taxon database, to use "
                            "instead of CTaxon1 service",
                            CArgDescriptions::eInputFile);
}

CLocalTaxon::CLocalTaxon() : m_db_supports_synonym(false)
{
    /// Initializing without command-line arguments; use Taxon server
    m_TaxonConn.reset(new CTaxon1);
    m_TaxonConn->Init();
}

CLocalTaxon::CLocalTaxon(const CArgs &args) : m_db_supports_synonym(false)
{
    if (args["taxon-db"]) {
        m_SqliteConn.reset(new CSQLITE_Connection(args["taxon-db"].AsString(),
                                  CSQLITE_Connection::fExternalMT |
                                  CSQLITE_Connection::fJournalOff |
                                  CSQLITE_Connection::fTempToMemory |
                                  CSQLITE_Connection::fVacuumOff |
                                  CSQLITE_Connection::fSyncOff));
        m_db_supports_synonym = x_SupportsSynonym();
    } else {
        m_TaxonConn.reset(new CTaxon1);
        m_TaxonConn->Init();
    }
}

CLocalTaxon::~CLocalTaxon()
{
}

CLocalTaxon::STaxidNode::STaxidNode()
    : taxid(INVALID_TAX_ID)
    , is_valid(false)
    , parent(s_InvalidNode)
    , genetic_code(-1)
{
}

CLocalTaxon::STaxidNode::~STaxidNode()
{
}

bool CLocalTaxon::IsValidTaxid(TTaxid taxid)
{
    if (m_SqliteConn.get()) {
        x_Cache(taxid);
        return m_Nodes.find(taxid)->second.is_valid;
    } else {
        return m_TaxonConn->GetTreeIterator(taxid);
    }
}

CLocalTaxon::TTaxid CLocalTaxon::GetParent(TTaxid taxid)
{
    if (m_SqliteConn.get()) {
        x_Cache(taxid);
        TNodeRef parent = m_Nodes.find(taxid)->second.parent;
        return parent == s_InvalidNode ? ZERO_TAX_ID : parent->first;
    } else {
        return m_TaxonConn->GetParent(taxid);
    }
}

string CLocalTaxon::GetRank(TTaxid taxid)
{
    if (m_SqliteConn.get()) {
        x_Cache(taxid);
        return m_Nodes.find(taxid)->second.rank;
    } else {
        TTaxRank rank_id = m_TaxonConn->GetTreeIterator(taxid)->GetNode()->GetRank();
        string rank_name;
        m_TaxonConn->GetRankName(rank_id, rank_name);
        return rank_name;
    }
}

string CLocalTaxon::GetScientificName(TTaxid taxid)
{
    if (m_SqliteConn.get()) {
        x_Cache(taxid);
        return m_Nodes.find(taxid)->second.scientific_name;
    } else {
        string scientific_name;
        m_TaxonConn->GetScientificName(taxid, scientific_name);
        return scientific_name;
    }
}

short int CLocalTaxon::GetGeneticCode(TTaxid taxid)
{
    if (m_SqliteConn.get()) {
        x_Cache(taxid);
        return m_Nodes.find(taxid)->second.genetic_code;
    } else {
        return m_TaxonConn->GetTreeIterator(taxid)->GetNode()->GetGC();
    }
}


namespace {

//        s_CopyDbTags(org, *new_org);
void s_CopyDbTags(const COrg_ref& org, COrg_ref& new_org)
{
    if( ! org.IsSetDb() ) {
        return;
    }
    new_org.SetDb().insert(
        new_org.SetDb().end(),
        const_cast<COrg_ref&>(org).SetDb().begin(),
        const_cast<COrg_ref&>(org).SetDb().end()
    );

    for (vector<CRef<CDbtag> >::iterator it1 = new_org.SetDb().begin();
         it1 != new_org.SetDb().end();  ++it1) {

        vector<CRef<CDbtag> >::iterator it2 = it1;
        for (++it2;  it2 != new_org.SetDb().end();  ) {
            if ((*it1)->Equals(**it2)) {
                it2 = new_org.SetDb().erase(it2);
            }
            else {
                ++it2;
            }
        }
    }
}

void s_RemoveTaxon(COrg_ref& org)
{
    if( ! org.IsSetDb() ) {
        return;
    }
    vector<CRef<CDbtag> >& dbs = org.SetDb();
    ERASE_ITERATE(vector<CRef<CDbtag> >, it, dbs ) {
        if ( (*it)->GetDb() == "taxon" ) {
            VECTOR_ERASE(it, dbs);
        }
    }
}

}




void CLocalTaxon::LookupMerge(objects::COrg_ref& org)
{
    if (m_SqliteConn.get()) {
        TTaxId taxid = ZERO_TAX_ID;
        if( ! org.IsSetDb() ) {
            taxid = GetTaxIdByOrgRef(org);
        } else {
            taxid = org.GetTaxId();
        }
        if ( taxid <= ZERO_TAX_ID ) {
            NCBI_THROW(CException, eUnknown, "s_UpdateOrgRef: organism does not contain tax id or has unequivocal registered taxonomy name");
        }

        CConstRef<COrg_ref> public_org = GetOrgRef(taxid);
        CRef<COrg_ref> new_org(new COrg_ref);
        new_org->Assign(*public_org);
        if (org.IsSetOrgname() && org.GetOrgname().IsSetMod()) {
            new_org->SetOrgname().SetMod() =
                org.GetOrgname().GetMod();
        }
        if ( !new_org->Equals(org) ) {
            s_RemoveTaxon(org);
            s_CopyDbTags(org, *new_org);
            org.Assign(*new_org);
        }
    }
    else {
        m_TaxonConn->LookupMerge(org);
    }
}


CConstRef<objects::COrg_ref> CLocalTaxon::GetOrgRef(TTaxid taxid)
{
    if (m_SqliteConn.get()) {
        x_Cache(taxid, true);
        return m_Nodes.find(taxid)->second.org_ref;
    } else {
        bool is_species, is_uncultured;
        string blast_name;
        return m_TaxonConn->GetOrgRef(taxid, is_species, is_uncultured, blast_name);
    }
}

CLocalTaxon::TTaxid CLocalTaxon::GetAncestorByRank(TTaxid taxid, const string &rank)
{
    if (m_SqliteConn.get()) {
        TInternalLineage lineage;
        x_GetLineage(taxid, lineage);
        for (TNodeRef ancestor : lineage) {
            if (ancestor->second.rank == rank) {
                return ancestor->first;
            }
        }
        return ZERO_TAX_ID;
    } else {
        return m_TaxonConn->GetAncestorByRank(taxid, rank.c_str());
    }
}

CLocalTaxon::TTaxid CLocalTaxon::GetTaxIdByOrgRef(const COrg_ref &inp_orgRef)
{
    if (inp_orgRef.IsSetDb()) {
        return inp_orgRef.GetTaxId();
    } else if (m_TaxonConn.get()) {
        return m_TaxonConn->GetTaxIdByOrgRef(inp_orgRef);
    } else {
        NCBI_THROW(CException, eUnknown,
                   "GetTaxIdByOrgRef not supported for local execution");
    }
}

CLocalTaxon::TLineage CLocalTaxon::GetLineage(TTaxid taxid)
{
    TLineage lineage;
    if (m_SqliteConn.get()) {
        TInternalLineage internal_lineage;
        x_GetLineage(taxid, internal_lineage);
        for (TNodeRef ancestor : internal_lineage) {
            lineage.push_back(ancestor->first);
        }
    } else {
        for (TTaxid ancestor = taxid; ancestor > ZERO_TAX_ID;
             ancestor = m_TaxonConn->GetParent(ancestor))
        {
            lineage.push_back(ancestor);
        }
        reverse(lineage.begin(), lineage.end());
    }
    return lineage;
}

CLocalTaxon::TTaxid CLocalTaxon::Join(TTaxid taxid1, TTaxid taxid2)
{
    if (m_SqliteConn.get()) {
        TLineage lineage1 = GetLineage(taxid1),
                 lineage2 = GetLineage(taxid2);
        TLineage::const_iterator it1 = lineage1.begin(),
                                 it2 = lineage2.begin();
        for (; it1 != lineage1.end() && it2 != lineage2.end() && *it1 == *it2;
               ++it1, ++it2);
        return *--it1;
    } else {
        return m_TaxonConn->Join(taxid1, taxid2);
    }
}

CLocalTaxon::TTaxid CLocalTaxon::GetTaxIdByName(const string& orgname)
{
    TTaxid taxid = INVALID_TAX_ID;
    if (m_SqliteConn.get()) {
        x_Cache(orgname);
        auto& taxnode = m_ScientificNameIndex.find(orgname)->second;
        taxid =  taxnode.is_valid ? taxnode.taxid : INVALID_TAX_ID;
    } else {
        taxid = m_TaxonConn->GetTaxIdByName(orgname);
    }
    return taxid;
    
}

//
//  Implementation
//

CLocalTaxon::TScientificNameRef CLocalTaxon::x_Cache(const string& orgname)
{
    NCBI_ASSERT(m_SqliteConn.get(), "x_Cache called with server execution");

    TScientificNameIndex::iterator it = m_ScientificNameIndex.find(orgname);
    if (it == m_ScientificNameIndex.end()  ) {
        //
        //  do the case-insensitive comparison
        //
        string sql = "SELECT taxid FROM TaxidInfo WHERE scientific_name = ?1 COLLATE NOCASE ";
        if(m_db_supports_synonym) {
            sql +=             "   UNION "
            "SELECT taxid FROM Synonym WHERE scientific_name = ?1 COLLATE NOCASE ";
        }
        
        CSQLITE_Statement stmt(m_SqliteConn.get(),sql);
        stmt.Bind(1, orgname);
        stmt.Execute();
        if  (stmt.Step()) {
            TTaxId taxid = TAX_ID_FROM(int, stmt.GetInt(0));
            auto it2 = x_Cache(taxid);
            it = m_ScientificNameIndex.insert(TScientificNameIndex::value_type(orgname,  it2->second  )).first;
        }
        else {
            //
            //  return invalid node.
            //
            it = m_ScientificNameIndex.insert(TScientificNameIndex::value_type(orgname, STaxidNode())).first;
        }
    }
    return it;
}


CLocalTaxon::TNodeRef CLocalTaxon::x_Cache(TTaxid taxid, bool including_org_ref)
{
    NCBI_ASSERT(m_SqliteConn.get(), "x_Cache called with server execution");

    TNodes::iterator it = m_Nodes.find(taxid);
    if (it != m_Nodes.end() && (!including_org_ref || it->second.org_ref))
    {
        return it;
    }

    if (it == m_Nodes.end()) {
        TTaxId parent = INVALID_TAX_ID;
        //
        //  Note that we are unconditionally recording (so far) unknown input taxid here
        //  thereby caching all successful and unsuccessful queries
        //
        it = m_Nodes.insert(TNodes::value_type(taxid, STaxidNode())).first;
        {{
             CSQLITE_Statement stmt
                 (m_SqliteConn.get(),
                  "SELECT scientific_name, rank, parent, genetic_code "
                  "FROM TaxidInfo "
                  "WHERE taxid = ? ");
             stmt.Bind(1, TAX_ID_TO(TIntId, taxid));
             stmt.Execute();
             if  (stmt.Step()) {
                 it->second.taxid  = taxid;
                 it->second.is_valid = true;
                 it->second.scientific_name = stmt.GetString(0);
                 it->second.rank = stmt.GetString(1);
                 if (it->second.rank.empty()) {
                     it->second.rank = "no rank";
                 }
                 parent = TAX_ID_FROM(int, stmt.GetInt(2));
                 it->second.genetic_code = stmt.GetInt(3);
             }
       }}
       if (parent > TAX_ID_CONST(1)) {
           // Recursively get information for parent; no need for Org_ref, even
           // / if it was requested for child node
           it->second.parent = x_Cache(parent);
       }
    }

    if (it->second.is_valid && including_org_ref) {
        CSQLITE_Statement stmt
            (m_SqliteConn.get(),
             "SELECT org_ref_asn "
             "FROM TaxidInfo "
             "WHERE taxid = ? ");
        stmt.Bind(1, TAX_ID_TO(TIntId, taxid));
        stmt.Execute();
        stmt.Step();
        string org_ref_asn = stmt.GetString(0);
        if (!org_ref_asn.empty()) {
            CNcbiIstrstream istr(org_ref_asn);
            CRef<COrg_ref> org_ref(new COrg_ref);
            istr >> MSerial_AsnText >> *org_ref;
            it->second.org_ref = org_ref;
        }
    }

    return it;
}

void CLocalTaxon::x_GetLineage(TTaxid taxid, TInternalLineage &lineage)
{
    TNodeRef it = x_Cache(taxid);
    if (!it->second.is_valid) {
        return;
    }
    lineage.push_front(it);
    while(lineage.front()->second.parent != s_InvalidNode) {
        lineage.push_front(lineage.front()->second.parent);
    }
}

bool CLocalTaxon::x_SupportsSynonym()
{
    CSQLITE_Statement stmt(m_SqliteConn.get(),
         "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='Synonym'");
    stmt.Execute();
    stmt.Step();
    return stmt.GetInt(0) > 0;
}

END_NCBI_SCOPE
