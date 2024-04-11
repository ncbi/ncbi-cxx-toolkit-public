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
 */

/// @file tax4blastsqlite.cpp
/// @author Christiam Camacho
/// Implementation for the CTaxonomy4BlastSQLite class

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_reader/tax4blastsqlite.hpp>

BEGIN_NCBI_SCOPE

const string CTaxonomy4BlastSQLite::kDefaultName("taxonomy4blast.sqlite3");

const char* kSQLQuery = R"(
WITH RECURSIVE descendants_from(taxid, parent) AS
    (SELECT ?1, NULL
     UNION ALL
     SELECT T.taxid, T.parent
       FROM descendants_from D, TaxidInfo T
       WHERE D.taxid = T.parent)
SELECT taxid from descendants_from;
)";

CTaxonomy4BlastSQLite::CTaxonomy4BlastSQLite(const string& dbname)
{
    const string kFileName = dbname.empty() ? kDefaultName : dbname;
    m_DbName = SeqDB_ResolveDbPath(kFileName);
    if (m_DbName.empty()) {
        CNcbiOstrstream oss;
        oss << "Database '" << kFileName << "' not found";
        NCBI_THROW(CSeqDBException, eFileErr, CNcbiOstrstreamToString(oss));
    }

    CSQLITE_Connection::TOperationFlags opFlags =
            CSQLITE_Connection::fExternalMT |
            CSQLITE_Connection::fVacuumOff |
            CSQLITE_Connection::fJournalOff |
            CSQLITE_Connection::fSyncOff |
            CSQLITE_Connection::fTempToMemory |
            CSQLITE_Connection::fReadOnly;
    m_DbConn.reset(new CSQLITE_Connection(m_DbName, opFlags));
    x_SanityCheck();
}

CTaxonomy4BlastSQLite::~CTaxonomy4BlastSQLite()
{
    m_Statement.reset();
    m_DbConn.reset();
}

void CTaxonomy4BlastSQLite::x_SanityCheck()
{
    const map<string, string> kRequiredElements {
        { "table", "TaxidInfo" },
        { "index", "TaxidInfoCompositeIdx_parent" }
    };
    const string kSqlStmt =
        "SELECT COUNT(*) FROM sqlite_master WHERE type=? and name=?;";
    for (auto& [type, name] : kRequiredElements) {
        unique_ptr<CSQLITE_Statement> s(new CSQLITE_Statement(&*m_DbConn, kSqlStmt));
        s->Bind(1, type);
        s->Bind(2, name);
        s->Execute();
        if (s->Step()) {
            if (s->GetInt(0) != 1) {
                CNcbiOstrstream oss;
                oss << "Database '" << m_DbName << "' does not have " << type;
                oss << " " << name << ". Please run the following command or ";
                oss << "contact your system administrator to install it:" << endl;
                oss << "update_blastdb.pl --decompress taxdb";
                NCBI_THROW(CSeqDBException, eArgErr, CNcbiOstrstreamToString(oss));
            }
        } else {
            CNcbiOstrstream oss;
            oss << "Failed to check for " << type << " " << name << " in '";
            oss << m_DbName << "'";
            NCBI_THROW(CSeqDBException, eArgErr, CNcbiOstrstreamToString(oss));
        }
    }
}

void CTaxonomy4BlastSQLite::GetLeafNodeTaxids(const int taxid, vector<int>& descendants)
{
    descendants.clear();
    if (taxid <= 0) return;

    if (!m_Statement) {
        m_Statement.reset(new CSQLITE_Statement(&*m_DbConn, kSQLQuery));
    }
    m_Statement->Reset();
    m_Statement->ClearBindings();
    m_Statement->Bind(1, taxid);
    m_Statement->Execute();

    while (m_Statement->Step()) {
        auto desc_taxid = m_Statement->GetInt(0);
        if (desc_taxid != taxid)
            descendants.push_back(desc_taxid);
    }
}


END_NCBI_SCOPE

