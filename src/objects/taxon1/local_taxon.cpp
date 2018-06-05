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

CLocalTaxon::CLocalTaxon()
{
    /// Initializing without command-line arguments; use Taxon server
    m_TaxonConn.reset(new CTaxon1);
    m_TaxonConn->Init();
    s_InvalidNode = m_Nodes.end();
}

CLocalTaxon::CLocalTaxon(const CArgs &args)
{
    if (args["taxon-db"]) {
        m_SqliteConn.reset(new CSQLITE_Connection(args["taxon-db"].AsString(),
                                  CSQLITE_Connection::fExternalMT |
                                  CSQLITE_Connection::fJournalOff |
                                  CSQLITE_Connection::fTempToMemory |
                                  CSQLITE_Connection::fVacuumOff |
                                  CSQLITE_Connection::fSyncOff));
    } else {
        m_TaxonConn.reset(new CTaxon1);
        m_TaxonConn->Init();
    }
    s_InvalidNode = m_Nodes.end();
}

CLocalTaxon::~CLocalTaxon()
{
}

CLocalTaxon::STaxidNode::STaxidNode()
: is_valid(false), parent(s_InvalidNode), genetic_code(-1)
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
        return parent == s_InvalidNode ? 0 : parent->first;
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
        return 0;
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
        for (TTaxid ancestor = taxid; ancestor > 0;
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

CLocalTaxon::TNodeRef CLocalTaxon::x_Cache(TTaxid taxid, bool including_org_ref)
{
    NCBI_ASSERT(m_SqliteConn.get(), "x_Cache called with server execution");

    TNodes::iterator it = m_Nodes.find(taxid);
    if (it != m_Nodes.end() && (!including_org_ref || it->second.org_ref))
    {
        return it;
    }

    if (it == m_Nodes.end()) {
        int parent = -1;
        it = m_Nodes.insert(TNodes::value_type(taxid, STaxidNode())).first;
        {{
             CSQLITE_Statement stmt
                 (m_SqliteConn.get(),
                  "SELECT scientific_name, rank, parent, genetic_code "
                  "FROM TaxidInfo "
                  "WHERE taxid = ? ");
             stmt.Bind(1, taxid);
             stmt.Execute();
             if  (stmt.Step()) {
                 it->second.is_valid = true;
                 it->second.scientific_name = stmt.GetString(0);
                 it->second.rank = stmt.GetString(1);
                 if (it->second.rank.empty()) {
                     it->second.rank = "no rank";
                 }
                 parent = stmt.GetInt(2);
                 it->second.genetic_code = stmt.GetInt(3);
             }
       }}
       if (parent > 1) {
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
        stmt.Bind(1, taxid);
        stmt.Execute();
        stmt.Step();
        string org_ref_asn = stmt.GetString(0);
        if (!org_ref_asn.empty()) {
            CNcbiIstrstream istr(org_ref_asn.c_str());
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

END_NCBI_SCOPE
